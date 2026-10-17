#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofMarkerLayout.h"

#include <limits>

namespace
{
bool IsInside(const FBox2D& Box, const FVector2D& Viewport, double Margin)
{
    return Box.bIsValid && Box.Min.X >= Margin && Box.Min.Y >= Margin
        && Box.Max.X <= Viewport.X - Margin && Box.Max.Y <= Viewport.Y - Margin;
}

bool HasGap(const FBox2D& A, const FBox2D& B, double Gap)
{
    return A.Max.X + Gap <= B.Min.X || B.Max.X + Gap <= A.Min.X
        || A.Max.Y + Gap <= B.Min.Y || B.Max.Y + Gap <= A.Min.Y;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMarkerDenseLayoutTest, "RedalertProof.HUD.MarkerLayout.DenseAndReserved", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofMarkerDenseLayoutTest::RunTest(const FString&)
{
    const FVector2D Viewport(320.0, 240.0);
    const FBox2D Panel(FVector2D(0.0, 0.0), FVector2D(320.0, 45.0));
    const FBox2D Banner(FVector2D(90.0, 100.0), FVector2D(230.0, 130.0));
    FProofMarkerLayout Layout(Viewport);
    Layout.Reserve(Panel);
    Layout.Reserve(Banner);
    TestTrue(TEXT("Occupied panel does not change viewport anchor visibility"), Layout.IsAnchorVisible(FVector2D(20.0, 20.0)));
    TArray<FBox2D> Accepted;
    int32 Rejected = 0;
    // Overlapping mixed-size labels exercise an actual accepted history, not an
    // independently reproduced search algorithm. Exhaustion must be explicit.
    for (int32 Index = 0; Index < 160; ++Index)
    {
        const FVector2D Preferred(148.0 + Index % 3, 88.0 + Index % 5);
        const FVector2D Size(22.0 + Index % 4, 10.0 + Index % 2);
        FBox2D Bounds;
        if (Layout.TryPlace(Preferred, Size, Bounds))
        {
            TestTrue(TEXT("Accepted dense label is fully inset"), IsInside(Bounds, Viewport, 4.0));
            TestTrue(TEXT("Panel is excluded including gap"), HasGap(Bounds, Panel, 2.0));
            TestTrue(TEXT("Terminal banner is excluded including gap"), HasGap(Bounds, Banner, 2.0));
            TestTrue(TEXT("Dense label stays within displacement bound"), FVector2D::Distance(Bounds.Min, Preferred) <= 64.0);
            TestTrue(TEXT("Requested dimensions are retained"), Bounds.GetSize().Equals(Size, 1.e-9));
            for (const FBox2D& Other : Accepted)
            {
                TestTrue(TEXT("Accepted labels have the configured gap"), HasGap(Bounds, Other, 2.0));
            }
            Accepted.Add(Bounds);
        }
        else
        {
            ++Rejected;
            TestFalse(TEXT("Dense failure invalidates bounds"), !!Bounds.bIsValid);
        }
    }
    TestTrue(TEXT("Local search places multiple dense markers"), Accepted.Num() > 1);
    TestTrue(TEXT("Saturated local search reports failure"), Rejected > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMarkerEdgeLayoutTest, "RedalertProof.HUD.MarkerLayout.EdgesAndDistance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofMarkerEdgeLayoutTest::RunTest(const FString&)
{
    const FVector2D Viewport(200.0, 120.0);
    const FVector2D Size(20.0, 10.0);
    const FVector2D Corners[] = {{-2.0, -2.0}, {195.0, -2.0}, {-2.0, 115.0}, {195.0, 115.0}};
    for (const FVector2D& Corner : Corners)
    {
        FProofMarkerLayout Layout(Viewport);
        FBox2D Bounds;
        TestTrue(TEXT("Near-edge rectangle fits completely"), Layout.TryPlace(Corner, Size, Bounds));
        TestTrue(TEXT("Corner fit never clips"), IsInside(Bounds, Viewport, 4.0));
        TestTrue(TEXT("Corner fit stays local"), FVector2D::Distance(Bounds.Min, Corner) <= 64.0);
    }
    FProofMarkerLayout Layout(Viewport, 4.f, 2.f, 10.f);
    FBox2D Bounds(FVector2D(1.0, 1.0), FVector2D(2.0, 2.0));
    TestFalse(TEXT("Far-offscreen rectangle cannot be dragged to an edge"), Layout.TryPlace(FVector2D(-100.0, 40.0), Size, Bounds));
    TestFalse(TEXT("Offscreen failure clears stale output"), !!Bounds.bIsValid);
    TestFalse(TEXT("Oversized label fails explicitly"), Layout.TryPlace(FVector2D(10.0, 10.0), FVector2D(193.0, 10.0), Bounds));
    TestFalse(TEXT("Anchor at right edge is outside"), Layout.IsAnchorVisible(FVector2D(200.0, 20.0)));
    TestTrue(TEXT("Anchor at origin is visible despite inset"), Layout.IsAnchorVisible(FVector2D::ZeroVector));
    TestFalse(TEXT("Negative anchor is outside"), Layout.IsAnchorVisible(FVector2D(-0.1, 20.0)));
    TestTrue(TEXT("Failures do not reserve usable space"), Layout.TryPlace(FVector2D(4.0, 4.0), FVector2D(192.0, 112.0), Bounds));
    TestTrue(TEXT("Exact usable viewport retains full size"), Bounds.GetSize().Equals(FVector2D(192.0, 112.0), 0.0));
    FProofMarkerLayout ExactLimit(Viewport, 4.f, 2.f, 10.f);
    TestFalse(TEXT("Edge-fit just beyond displacement limit fails"), ExactLimit.TryPlace(FVector2D(-6.01, 4.0), Size, Bounds));
    TestTrue(TEXT("Edge-fit exactly at displacement limit succeeds"), ExactLimit.TryPlace(FVector2D(-6.0, 4.0), Size, Bounds));
    TestTrue(TEXT("Boundary placement has the exact bounded displacement"), Bounds.Min.Equals(FVector2D(4.0, 4.0), 0.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMarkerInvalidLayoutTest, "RedalertProof.HUD.MarkerLayout.InvalidInputs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofMarkerInvalidLayoutTest::RunTest(const FString&)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    const float FloatInfinity = std::numeric_limits<float>::infinity();
    const FVector2D InvalidVectors[] = {{NaN, 1.0}, {1.0, Infinity}, {0.0, 1.0}, {-1.0, 1.0}};
    FBox2D Bounds;
    for (const FVector2D& Invalid : InvalidVectors)
    {
        FProofMarkerLayout InvalidViewport(Invalid);
        TestFalse(TEXT("Invalid viewport rejects placement"), InvalidViewport.TryPlace(FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), Bounds));
        FProofMarkerLayout Layout(FVector2D(100.0, 100.0));
        TestFalse(TEXT("Invalid size rejects placement"), Layout.TryPlace(FVector2D(10.0, 10.0), Invalid, Bounds));
        TestFalse(TEXT("Invalid size leaves no successful bounds"), !!Bounds.bIsValid);
    }
    FProofMarkerLayout Layout(FVector2D(100.0, 100.0));
    Layout.Reserve(FBox2D(ForceInit));
    Layout.Reserve(FBox2D(FVector2D(NaN, 0.0), FVector2D(100.0, 100.0)));
    Layout.Reserve(FBox2D(FVector2D(5.0, 5.0), FVector2D(5.0, 10.0)));
    TestFalse(TEXT("Nonfinite preferred X fails"), Layout.TryPlace(FVector2D(Infinity, 10.0), FVector2D(10.0, 10.0), Bounds));
    TestFalse(TEXT("Nonfinite preferred Y fails"), Layout.TryPlace(FVector2D(10.0, NaN), FVector2D(10.0, 10.0), Bounds));
    TestFalse(TEXT("NaN anchor is invisible"), Layout.IsAnchorVisible(FVector2D(NaN, 10.0)));
    TestTrue(TEXT("Invalid reservations and failed requests do not poison occupancy"), Layout.TryPlace(FVector2D(4.0, 4.0), FVector2D(92.0, 92.0), Bounds));
    FProofMarkerLayout InvalidMargin(FVector2D(100.0, 100.0), -1.f);
    FProofMarkerLayout InvalidGap(FVector2D(100.0, 100.0), 4.f, FloatInfinity);
    FProofMarkerLayout InvalidDistance(FVector2D(100.0, 100.0), 4.f, 2.f, -1.f);
    FProofMarkerLayout InfiniteDistance(FVector2D(100.0, 100.0), 4.f, 2.f, FloatInfinity);
    FProofMarkerLayout NaNMargin(FVector2D(100.0, 100.0), std::numeric_limits<float>::quiet_NaN());
    FProofMarkerLayout EmptyInset(FVector2D(8.0, 8.0));
    TestFalse(TEXT("Negative margin rejected"), InvalidMargin.TryPlace(FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), Bounds));
    TestFalse(TEXT("Infinite gap rejected"), InvalidGap.TryPlace(FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), Bounds));
    TestFalse(TEXT("Negative distance rejected"), InvalidDistance.TryPlace(FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), Bounds));
    TestFalse(TEXT("Infinite distance rejected"), InfiniteDistance.TryPlace(FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), Bounds));
    TestFalse(TEXT("NaN margin rejected"), NaNMargin.TryPlace(FVector2D(10.0, 10.0), FVector2D(10.0, 10.0), Bounds));
    TestFalse(TEXT("Empty inset rejected"), EmptyInset.TryPlace(FVector2D(0.0, 0.0), FVector2D(1.0, 1.0), Bounds));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMarkerHistoryLayoutTest, "RedalertProof.HUD.MarkerLayout.DeterministicHistory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofMarkerHistoryLayoutTest::RunTest(const FString&)
{
    FProofMarkerLayout First(FVector2D(300.0, 200.0));
    FProofMarkerLayout Repeat(FVector2D(300.0, 200.0));
    const FBox2D Panel(FVector2D(0.0, 0.0), FVector2D(300.0, 40.0));
    First.Reserve(Panel);
    Repeat.Reserve(Panel);
    for (int32 Index = 0; Index < 100; ++Index)
    {
        const FVector2D Preferred(125.0 + Index % 7, 60.0 + Index % 3);
        const FVector2D Size(28.0 + Index % 2, 12.0);
        FBox2D A;
        FBox2D B;
        const bool bPlacedA = First.TryPlace(Preferred, Size, A);
        const bool bPlacedB = Repeat.TryPlace(Preferred, Size, B);
        TestEqual(TEXT("Repeated history has identical success/failure"), bPlacedA, bPlacedB);
        if (bPlacedA && bPlacedB)
        {
            TestTrue(TEXT("Repeated history has identical positions"), A.Min.Equals(B.Min, 0.0) && A.Max.Equals(B.Max, 0.0));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMarkerPriorityLayoutTest, "RedalertProof.HUD.MarkerLayout.PriorityAndFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofMarkerPriorityLayoutTest::RunTest(const FString&)
{
    FProofMarkerLayout Layout(FVector2D(200.0, 120.0), 4.f, 2.f, 0.f);
    FBox2D Selected;
    FBox2D Ordinary;
    // The caller orders selected/priority markers before stable-ID ordinary
    // markers. With no displacement budget, only the first claimant can fit.
    TestTrue(TEXT("Selected-first caller retains selected marker"), Layout.TryPlace(FVector2D(40.0, 40.0), FVector2D(30.0, 10.0), Selected));
    Ordinary = Selected;
    TestFalse(TEXT("Later collision fails with zero displacement"), Layout.TryPlace(FVector2D(40.0, 40.0), FVector2D(30.0, 10.0), Ordinary));
    TestFalse(TEXT("Failed collision invalidates stale output"), !!Ordinary.bIsValid);
    TestFalse(TEXT("Configured gap is enforced"), Layout.TryPlace(FVector2D(71.0, 40.0), FVector2D(10.0, 10.0), Ordinary));
    TestTrue(TEXT("A failed near-gap request leaves subsequent exact gap usable"), Layout.TryPlace(FVector2D(72.0, 40.0), FVector2D(10.0, 10.0), Ordinary));
    TestTrue(TEXT("Selected rectangle stays at preferred position"), Selected.Min.Equals(FVector2D(40.0, 40.0), 0.0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMarkerLinkedObstaclesTest, "RedalertProof.HUD.MarkerLayout.LinkedObstacles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofMarkerLinkedObstaclesTest::RunTest(const FString&)
{
    const FVector2D Viewport(240.0, 220.0);
    const FBox2D Glyph(FVector2D(89.0, 89.0), FVector2D(111.0, 111.0));
    const FVector2D Preferred(85.0, 140.0), Size(30.0, 9.0);
    FProofMarkerLayout Layout(Viewport);
    FProofMarkerLayout Repeat(Viewport);
    const int32 Source = Layout.Reserve(Glyph);
    const int32 RepeatedSource = Repeat.Reserve(Glyph);
    const FBox2D Obstacle(FVector2D(90.0, 125.0), FVector2D(110.0, 130.0));
    Layout.Reserve(Obstacle);
    Repeat.Reserve(Obstacle);
    FBox2D Bounds, RepeatedBounds;
    FVector2D Start, End, RepeatedStart, RepeatedEnd;
    TestTrue(TEXT("Blocked preferred leader searches for a clear bounded alternative"),
        Layout.TryPlaceLinked(Preferred, Size, Source, Bounds, Start, End));
    TestTrue(TEXT("Same linked history succeeds deterministically"),
        Repeat.TryPlaceLinked(Preferred, Size, RepeatedSource, RepeatedBounds, RepeatedStart, RepeatedEnd));
    TestTrue(TEXT("Obstacle requires displacement"), Bounds.bIsValid && !Bounds.Min.Equals(Preferred, 0.0));
    TestTrue(TEXT("Linked result stays in displacement budget"), Bounds.bIsValid && FVector2D::Distance(Bounds.Min, Preferred) <= 64.0);
    TestTrue(TEXT("Rectangle and leader repeat exactly"), Bounds.Min.Equals(RepeatedBounds.Min, 0.0)
        && Start.Equals(RepeatedStart, 0.0) && End.Equals(RepeatedEnd, 0.0));

    FProofMarkerLayout Blocked(Viewport, 4.f, 2.f, 0.f);
    const int32 BlockedSource = Blocked.Reserve(Glyph);
    Blocked.Reserve(Obstacle);
    TestFalse(TEXT("Foreign reservation blocks leader even when group rectangle fits"),
        Blocked.TryPlaceLinked(Preferred, Size, BlockedSource, Bounds, Start, End));
    TestFalse(TEXT("Failed linked placement clears old bounds"), !!Bounds.bIsValid);
    TestTrue(TEXT("Failed linked placement clears endpoints"), Start.IsZero() && End.IsZero());
    TestTrue(TEXT("Failed leader reserves no candidate rectangle"), Blocked.TryPlace(Preferred, Size, Bounds));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMarkerLinkedPriorityTest, "RedalertProof.HUD.MarkerLayout.LinkedPriorityAndCrossings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofMarkerLinkedPriorityTest::RunTest(const FString&)
{
    FProofMarkerLayout Layout(FVector2D(240.0, 200.0), 4.f, 2.f, 0.f);
    const int32 SelectedSource = Layout.Reserve(FBox2D(FVector2D(50.0, 50.0), FVector2D(70.0, 70.0)));
    const int32 OrdinarySource = Layout.Reserve(FBox2D(FVector2D(100.0, 80.0), FVector2D(120.0, 100.0)));
    FBox2D Selected, Ordinary;
    FVector2D Start, End;
    TestTrue(TEXT("Selected priority keeps valid group before ordinary allocation"),
        Layout.TryPlaceLinked(FVector2D(50.0, 120.0), FVector2D(20.0, 10.0), SelectedSource, Selected, Start, End));
    TestTrue(TEXT("Leader begins at source edge, not unit center"), Start.Equals(FVector2D(60.0, 70.0), 0.0));
    TestFalse(TEXT("Later group cannot paint over selected leader"),
        Layout.TryPlace(FVector2D(55.0, 90.0), FVector2D(10.0, 10.0), Ordinary));
    TestFalse(TEXT("Ordinary leader cannot cross selected leader"),
        Layout.TryPlaceLinked(FVector2D(20.0, 80.0), FVector2D(20.0, 10.0), OrdinarySource, Ordinary, Start, End));
    TestTrue(TEXT("Failed ordinary placements leave free space usable"),
        Layout.TryPlace(FVector2D(80.0, 90.0), FVector2D(10.0, 10.0), Ordinary));
    TestTrue(TEXT("Selected rectangle is unchanged by later failure"), Selected.Min.Equals(FVector2D(50.0, 120.0), 0.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMarkerLinkedIdentityTest, "RedalertProof.HUD.MarkerLayout.LinkedSourceIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofMarkerLinkedIdentityTest::RunTest(const FString&)
{
    FProofMarkerLayout Layout(FVector2D(200.0, 200.0));
    const FBox2D Glyph(FVector2D(89.0, 89.0), FVector2D(111.0, 111.0));
    const int32 Source = Layout.Reserve(Glyph);
    Layout.Reserve(Glyph);
    FBox2D Bounds;
    FVector2D Start, End;
    TestFalse(TEXT("Coincident foreign glyph is not exempted with source"),
        Layout.TryPlaceLinked(FVector2D(85.0, 120.0), FVector2D(30.0, 9.0), Source, Bounds, Start, End));
    TestFalse(TEXT("Missing reservation token fails"),
        Layout.TryPlaceLinked(FVector2D(85.0, 120.0), FVector2D(30.0, 9.0), INDEX_NONE, Bounds, Start, End));
    TestFalse(TEXT("Out-of-range token fails"),
        Layout.TryPlaceLinked(FVector2D(85.0, 120.0), FVector2D(30.0, 9.0), 99, Bounds, Start, End));
    const int32 Offscreen = Layout.Reserve(FBox2D(FVector2D(-1.0, 20.0), FVector2D(10.0, 30.0)));
    TestFalse(TEXT("Offscreen source cannot produce a border ghost"),
        Layout.TryPlaceLinked(FVector2D(15.0, 20.0), FVector2D(20.0, 9.0), Offscreen, Bounds, Start, End));
    return true;
}
#endif
