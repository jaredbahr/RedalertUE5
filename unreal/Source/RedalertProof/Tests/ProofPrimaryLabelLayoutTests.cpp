#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofPrimaryLabelLayout.h"

#include <limits>

namespace
{
using FPrimaryPlacement = FProofPrimaryLabelLayout::FPlacement;

bool PrimaryHasGap(const FBox2D& A, const FBox2D& B, double Gap)
{
    return A.Max.X + Gap <= B.Min.X || B.Max.X + Gap <= A.Min.X
        || A.Max.Y + Gap <= B.Min.Y || B.Max.Y + Gap <= A.Min.Y;
}

bool PrimaryInside(const FBox2D& Box, const FVector2D& Viewport, double Margin)
{
    return Box.bIsValid && Box.Min.X >= Margin && Box.Min.Y >= Margin
        && Box.Max.X <= Viewport.X - Margin && Box.Max.Y <= Viewport.Y - Margin;
}

bool PrimaryFinite(const FVector2D& Point)
{
    return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
}

// Independent segment/rectangle oracle: intersect the segment with each padded
// rectangle edge, rather than reproducing the production candidate search.
double PrimaryCross(const FVector2D& A, const FVector2D& B, const FVector2D& C)
{
    return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
}

bool PrimaryOnSegment(const FVector2D& A, const FVector2D& B, const FVector2D& P)
{
    return P.X >= FMath::Min(A.X, B.X) && P.X <= FMath::Max(A.X, B.X)
        && P.Y >= FMath::Min(A.Y, B.Y) && P.Y <= FMath::Max(A.Y, B.Y);
}

bool PrimarySegmentsTouch(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
{
    const double ABC = PrimaryCross(A, B, C), ABD = PrimaryCross(A, B, D);
    const double CDA = PrimaryCross(C, D, A), CDB = PrimaryCross(C, D, B);
    if (((ABC < 0.0 && ABD > 0.0) || (ABC > 0.0 && ABD < 0.0))
        && ((CDA < 0.0 && CDB > 0.0) || (CDA > 0.0 && CDB < 0.0))) return true;
    return (ABC == 0.0 && PrimaryOnSegment(A, B, C))
        || (ABD == 0.0 && PrimaryOnSegment(A, B, D))
        || (CDA == 0.0 && PrimaryOnSegment(C, D, A))
        || (CDB == 0.0 && PrimaryOnSegment(C, D, B));
}

bool PrimaryLeaderHits(const FVector2D& Start, const FVector2D& End, const FBox2D& Box, double Gap)
{
    const FVector2D Min = Box.Min - FVector2D(Gap, Gap), Max = Box.Max + FVector2D(Gap, Gap);
    const auto Contains = [&Min, &Max](const FVector2D& P)
    {
        return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y;
    };
    const FVector2D TopRight(Max.X, Min.Y), BottomLeft(Min.X, Max.Y);
    return Contains(Start) || Contains(End)
        || PrimarySegmentsTouch(Start, End, Min, TopRight)
        || PrimarySegmentsTouch(Start, End, TopRight, Max)
        || PrimarySegmentsTouch(Start, End, Max, BottomLeft)
        || PrimarySegmentsTouch(Start, End, BottomLeft, Min);
}

FPrimaryPlacement StalePrimaryPlacement()
{
    FPrimaryPlacement Out;
    Out.Bounds = FBox2D(FVector2D(1.0, 1.0), FVector2D(2.0, 2.0));
    Out.Start = FVector2D(3.0, 3.0);
    Out.End = FVector2D(4.0, 4.0);
    Out.Preferred = FVector2D(5.0, 5.0);
    Out.Passes = 99;
    return Out;
}

bool PrimaryGeometryCleared(const FPrimaryPlacement& Out)
{
    return !Out.Bounds.bIsValid && Out.Start.IsZero() && Out.End.IsZero();
}

struct FCapturedPrimaryFixture
{
    const TCHAR* Name;
    TArray<FBox2D> Reservations;
    int32 Source;
    FVector2D Screen;
    FVector2D OriginalPreferred;
    FBox2D ExpectedBounds;
    FVector2D ExpectedStart;
    FVector2D ExpectedEnd;
};

TArray<FCapturedPrimaryFixture> CapturedPrimaryFixtures()
{
    // Captured at source 1c057572bbba05dca6beec69a3944c2332fefb1b,
    // viewport 1280x720, revision 885, epoch 1. Preserve exact doubles and
    // source ordering, including coincident glyphs. Full provenance stays private.
    // C#32 is drawOrdinal 1: these failures need no earlier label/leader history.
    // The 960x540 clean run selected C#30; it is not this failure's reproduction.
    return {
        {TEXT("03-left-edge"), {
            FBox2D(FVector2D(12.0, 12.0), FVector2D(732.0, 209.0)),
            FBox2D(FVector2D(566.0, 221.0), FVector2D(714.0, 275.0)),
            FBox2D(FVector2D(566.0, 283.0), FVector2D(696.0, 308.0)),
            FBox2D(FVector2D(75.73501586914062, 442.1335144042969), FVector2D(97.73501586914062, 464.1335144042969)),
            FBox2D(FVector2D(61.33502197265625, 442.1335144042969), FVector2D(83.33502197265625, 464.1335144042969)),
            FBox2D(FVector2D(25.33502197265625, 442.133544921875), FVector2D(47.33502197265625, 464.133544921875)),
            FBox2D(FVector2D(25.33502197265625, 442.133544921875), FVector2D(47.33502197265625, 464.133544921875)),
            FBox2D(FVector2D(75.73504638671875, 477.7716979980469), FVector2D(97.73504638671875, 499.7716979980469)),
            FBox2D(FVector2D(4.999946594238281, 448.9999694824219), FVector2D(42.99994659423828, 486.9999694824219))
        }, 8, FVector2D(23.99994659423828, 467.9999694824219),
            FVector2D(7.999946594238281, 420.9999694824219),
            FBox2D(FVector2D(7.999946594238281, 489.9999694824219), FVector2D(39.99994659423828, 514.9999694824219)),
            FVector2D(23.99994659423828, 486.9999694824219), FVector2D(23.99994659423828, 489.9999694824219)},
        // 05-panel-edge and 06-camera-before contain identical first-call inputs.
        {TEXT("05-panel-edge / 06-camera-before"), {
            FBox2D(FVector2D(12.0, 12.0), FVector2D(732.0, 209.0)),
            FBox2D(FVector2D(566.0, 221.0), FVector2D(714.0, 275.0)),
            FBox2D(FVector2D(566.0, 283.0), FVector2D(696.0, 308.0)),
            FBox2D(FVector2D(435.735107421875, 242.771728515625), FVector2D(457.735107421875, 264.771728515625)),
            FBox2D(FVector2D(365.0, 214.00001525878906), FVector2D(403.0, 252.00001525878906)),
            FBox2D(FVector2D(359.5350646972656, 210.133544921875), FVector2D(375.5350646972656, 226.133544921875))
        }, 4, FVector2D(384.0, 233.00001525878906), FVector2D(368.0, 186.00001525878906),
            FBox2D(FVector2D(368.0, 255.00001525878906), FVector2D(400.0, 280.00001525878906)),
            FVector2D(384.0, 252.00001525878906), FVector2D(384.0, 255.00001525878906)}
    };
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofPrimaryLabelsCapturedFallbackTest,
    "RedalertProof.HUD.MarkerLayout.PrimaryLabelsCapturedFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofPrimaryLabelsCapturedFallbackTest::RunTest(const FString&)
{
    const FVector2D Viewport(1280.0, 720.0), Size(32.0, 25.0);
    for (const FCapturedPrimaryFixture& Fixture : CapturedPrimaryFixtures())
    {
        const FString Context(Fixture.Name);
        FProofMarkerLayout Original(Viewport, 4.f, 2.f, 64.f);
        FProofMarkerLayout Layout(Viewport, 4.f, 2.f, 64.f);
        FProofMarkerLayout Repeat(Viewport, 4.f, 2.f, 64.f);
        for (int32 Index = 0; Index < Fixture.Reservations.Num(); ++Index)
        {
            TestEqual(*(Context + TEXT(" original reservation token")), Original.Reserve(Fixture.Reservations[Index]), Index);
            TestEqual(*(Context + TEXT(" production reservation token")), Layout.Reserve(Fixture.Reservations[Index]), Index);
            TestEqual(*(Context + TEXT(" repeat reservation token")), Repeat.Reserve(Fixture.Reservations[Index]), Index);
        }
        FBox2D OriginalBounds(FVector2D(1.0, 1.0), FVector2D(2.0, 2.0));
        FVector2D OriginalStart(1.0, 1.0), OriginalEnd(2.0, 2.0);
        TestFalse(*(Context + TEXT(" reproduces captured top-only failure")), Original.TryPlaceLinked(
            Fixture.OriginalPreferred, Size, Fixture.Source, OriginalBounds, OriginalStart, OriginalEnd));
        TestTrue(*(Context + TEXT(" top-only failure clears stale geometry")),
            !OriginalBounds.bIsValid && OriginalStart.IsZero() && OriginalEnd.IsZero());

        FPrimaryPlacement Out = StalePrimaryPlacement(), Repeated = StalePrimaryPlacement();
        const bool bPlaced = FProofPrimaryLabelLayout::TryPlace(Layout, Fixture.Screen, 18.f, Size, Fixture.Source, Out);
        const bool bRepeated = FProofPrimaryLabelLayout::TryPlace(Repeat, Fixture.Screen, 18.f, Size, Fixture.Source, Repeated);
        TestTrue(*(Context + TEXT(" production helper retains selected ID and HP group")), bPlaced);
        TestTrue(*(Context + TEXT(" repeat helper retains selected group")), bRepeated);
        TestEqual(*(Context + TEXT(" uses bounded second anchor")), Out.Passes, 2);
        if (!bPlaced || !bRepeated) continue;

        TestTrue(*(Context + TEXT(" returns the expected intact bottom rectangle")),
            Out.Bounds.Min.Equals(Fixture.ExpectedBounds.Min, 0.0) && Out.Bounds.Max.Equals(Fixture.ExpectedBounds.Max, 0.0));
        TestTrue(*(Context + TEXT(" uses the expected clear bottom leader")),
            Out.Start.Equals(Fixture.ExpectedStart, 0.0) && Out.End.Equals(Fixture.ExpectedEnd, 0.0));
        TestTrue(*(Context + TEXT(" records the chosen bottom preference")), Out.Preferred.Equals(Fixture.ExpectedBounds.Min, 0.0));
        TestTrue(*(Context + TEXT(" retains full requested dimensions")), Out.Bounds.GetSize().Equals(Size, 0.0));
        TestTrue(*(Context + TEXT(" group stays inset")), PrimaryInside(Out.Bounds, Viewport, 4.0));
        TestTrue(*(Context + TEXT(" successful geometry is finite")), PrimaryFinite(Out.Bounds.Min)
            && PrimaryFinite(Out.Bounds.Max) && PrimaryFinite(Out.Start) && PrimaryFinite(Out.End));
        TestTrue(*(Context + TEXT(" displacement is bounded from the chosen anchor")),
            FVector2D::Distance(Out.Bounds.Min, Out.Preferred) <= 64.0);
        TestTrue(*(Context + TEXT(" opposite anchor is explicitly outside original top budget")),
            FVector2D::Distance(Out.Bounds.Min, Fixture.OriginalPreferred) > 64.0);
        for (int32 Index = 0; Index < Fixture.Reservations.Num(); ++Index)
        {
            TestTrue(*(Context + TEXT(" group avoids every reserved UI and glyph, including its source")),
                PrimaryHasGap(Out.Bounds, Fixture.Reservations[Index], 2.0));
            if (Index != Fixture.Source)
                TestFalse(*(Context + TEXT(" leader avoids each foreign reservation including padding")),
                    PrimaryLeaderHits(Out.Start, Out.End, Fixture.Reservations[Index], 2.0));
        }
        TestTrue(*(Context + TEXT(" repeated history returns identical geometry and anchor")),
            Out.Bounds.Min.Equals(Repeated.Bounds.Min, 0.0) && Out.Bounds.Max.Equals(Repeated.Bounds.Max, 0.0)
            && Out.Start.Equals(Repeated.Start, 0.0) && Out.End.Equals(Repeated.End, 0.0)
            && Out.Preferred.Equals(Repeated.Preferred, 0.0) && Out.Passes == Repeated.Passes);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofPrimaryLabelsSafetyAndPriorityTest,
    "RedalertProof.HUD.MarkerLayout.PrimaryLabelsSafetyAndPriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofPrimaryLabelsSafetyAndPriorityTest::RunTest(const FString&)
{
    static_assert(FProofPrimaryLabelLayout::MaxPasses == 2, "Primary anchor attempts must stay bounded");
    static_assert(FProofPrimaryLabelLayout::MaxCandidateCount == 132, "Primary candidate bound must stay explicit");
    static_assert(FProofPrimaryLabelLayout::MaxCandidateCount == 2 * FProofMarkerLayout::MaxCandidateCount,
        "Each primary anchor retains the primitive candidate bound");
    const FVector2D Viewport(240.0, 220.0), Center(100.0, 100.0), Size(30.0, 10.0);
    const FBox2D Glyph(FVector2D(89.0, 89.0), FVector2D(111.0, 111.0));
    FProofMarkerLayout Priority(Viewport, 4.f, 2.f, 0.f);
    const int32 Source = Priority.Reserve(Glyph);
    const int32 OrdinarySource = Priority.Reserve(FBox2D(FVector2D(169.0, 89.0), FVector2D(191.0, 111.0)));
    FPrimaryPlacement Selected;
    const bool bSelected = FProofPrimaryLabelLayout::TryPlace(Priority, Center, 10.f, Size, Source, Selected);
    TestTrue(TEXT("Clear top anchor succeeds before ordinary allocation"), bSelected);
    TestEqual(TEXT("Successful top anchor does not try bottom"), Selected.Passes, 1);
    if (bSelected)
    {
        TestTrue(TEXT("Existing top preference is unchanged"), Selected.Bounds.Min.Equals(FVector2D(85.0, 76.0), 0.0)
            && Selected.Preferred.Equals(FVector2D(85.0, 76.0), 0.0));
        TestTrue(TEXT("Top success retains complete label and source-edge leader"),
            Selected.Bounds.GetSize().Equals(Size, 0.0) && Selected.Start.Equals(FVector2D(100.0, 89.0), 0.0)
            && Selected.End.Equals(FVector2D(100.0, 86.0), 0.0));
        FBox2D Ordinary;
        FVector2D Start, End;
        TestFalse(TEXT("Later rectangle cannot steal the primary first claim"), Priority.TryPlace(Selected.Preferred, Size, Ordinary));
        TestFalse(TEXT("Later linked group cannot steal the primary first claim"),
            Priority.TryPlaceLinked(Selected.Preferred, Size, OrdinarySource, Ordinary, Start, End));
        TestTrue(TEXT("Later failure leaves primary geometry unchanged"), Selected.Bounds.Min.Equals(FVector2D(85.0, 76.0), 0.0));
    }

    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    const float FloatNaN = std::numeric_limits<float>::quiet_NaN();
    const float FloatInfinity = std::numeric_limits<float>::infinity();
    const auto RejectInput = [this, &Viewport, &Glyph](const FVector2D& Screen, float Radius, const FVector2D& RequestedSize)
    {
        FProofMarkerLayout Layout(Viewport);
        const int32 Token = Layout.Reserve(Glyph);
        FPrimaryPlacement Out = StalePrimaryPlacement();
        TestFalse(TEXT("Invalid primary input fails closed"), FProofPrimaryLabelLayout::TryPlace(Layout, Screen, Radius, RequestedSize, Token, Out));
        TestTrue(TEXT("Invalid input clears all stale geometry before any pass"), PrimaryGeometryCleared(Out)
            && Out.Preferred.IsZero() && Out.Passes == 0);
    };
    for (const FVector2D& InvalidSize : {FVector2D(NaN, 10.0), FVector2D(30.0, Infinity),
        FVector2D(0.0, 10.0), FVector2D(-1.0, 10.0), FVector2D(30.0, 0.0), FVector2D(30.0, -1.0)})
        RejectInput(Center, 10.f, InvalidSize);
    for (float InvalidRadius : {FloatNaN, FloatInfinity, 0.f, -1.f}) RejectInput(Center, InvalidRadius, Size);
    RejectInput(FVector2D(NaN, 100.0), 10.f, Size);
    RejectInput(FVector2D(100.0, Infinity), 10.f, Size);
    for (int32 InvalidSource : {INDEX_NONE, 99})
    {
        FProofMarkerLayout Layout(Viewport);
        Layout.Reserve(Glyph);
        FPrimaryPlacement Out = StalePrimaryPlacement();
        TestFalse(TEXT("Missing or out-of-range source token cannot bypass identity checks"),
            FProofPrimaryLabelLayout::TryPlace(Layout, Center, 10.f, Size, InvalidSource, Out));
        TestTrue(TEXT("Invalid token clears geometry and remains bounded"), PrimaryGeometryCleared(Out) && Out.Passes == 2);
    }
    {
        FProofMarkerLayout Layout(Viewport);
        const int32 Token = Layout.Reserve(FBox2D(FVector2D(-1.0, 40.0), FVector2D(10.0, 62.0)));
        FPrimaryPlacement Out = StalePrimaryPlacement();
        TestFalse(TEXT("Partly offscreen source cannot produce either border label"),
            FProofPrimaryLabelLayout::TryPlace(Layout, FVector2D(4.5, 51.0), 10.f, Size, Token, Out));
        TestTrue(TEXT("Offscreen failure clears geometry and stays bounded"), PrimaryGeometryCleared(Out) && Out.Passes == 2);
    }
    {
        FProofMarkerLayout Layout(Viewport);
        const int32 Token = Layout.Reserve(Glyph);
        Layout.Reserve(Glyph);
        FPrimaryPlacement Out = StalePrimaryPlacement();
        TestFalse(TEXT("Coincident foreign glyph is never exempted as the source"),
            FProofPrimaryLabelLayout::TryPlace(Layout, Center, 10.f, Size, Token, Out));
        TestTrue(TEXT("Duplicate-source failure clears geometry after both bounded passes"), PrimaryGeometryCleared(Out) && Out.Passes == 2);
    }
    {
        FProofMarkerLayout Layout(Viewport, 4.f, 2.f, 0.f);
        const int32 Token = Layout.Reserve(Glyph);
        FPrimaryPlacement Out = StalePrimaryPlacement();
        TestFalse(TEXT("Source token does not exempt a group rectangle covering its own glyph"),
            FProofPrimaryLabelLayout::TryPlace(Layout, Center, 1.f, Size, Token, Out));
        TestTrue(TEXT("Own-source collision fails without stale geometry"), PrimaryGeometryCleared(Out) && Out.Passes == 2);
        TestFalse(TEXT("Oversized group cannot fit at either anchor"),
            FProofPrimaryLabelLayout::TryPlace(Layout, Center, 10.f, FVector2D(500.0, 10.0), Token, Out));
        TestTrue(TEXT("Oversized failure remains bounded and clears geometry"), PrimaryGeometryCleared(Out) && Out.Passes == 2);
    }
    {
        FProofMarkerLayout Layout(Viewport, 4.f, 0.f, 0.f);
        const int32 Token = Layout.Reserve(Glyph);
        Layout.Reserve(FBox2D(FVector2D(80.0, 70.0), FVector2D(120.0, 88.0)));
        const int32 PriorSource = Layout.Reserve(FBox2D(FVector2D(150.0, 112.75), FVector2D(170.0, 114.75)));
        FBox2D PriorBounds;
        FVector2D PriorStart, PriorEnd;
        TestTrue(TEXT("Existing linked history provides a clear horizontal leader"), Layout.TryPlaceLinked(
            FVector2D(40.0, 113.25), FVector2D(20.0, 1.0), PriorSource, PriorBounds, PriorStart, PriorEnd));
        FPrimaryPlacement Out = StalePrimaryPlacement();
        TestFalse(TEXT("Fallback cannot cross an earlier leader when its own rectangle is free"),
            FProofPrimaryLabelLayout::TryPlace(Layout, Center, 10.f, Size, Token, Out));
        TestTrue(TEXT("Earlier leader rejection clears primary geometry within two passes"), PrimaryGeometryCleared(Out) && Out.Passes == 2);
        TestTrue(TEXT("Leader-crossing failure leaves the clear bottom rectangle available"),
            Layout.TryPlace(FVector2D(85.0, 114.0), Size, PriorBounds));
    }

    // Both anchor rectangles are free, but a thin foreign box blocks each
    // leader. Zero gap/distance makes the probe geometry exact and local.
    FProofMarkerLayout Failed(Viewport, 4.f, 0.f, 0.f), Untouched(Viewport, 4.f, 0.f, 0.f);
    const TArray<FBox2D> AtomicReservations = {
        Glyph,
        FBox2D(FVector2D(98.0, 87.0), FVector2D(102.0, 88.0)),
        FBox2D(FVector2D(98.0, 112.0), FVector2D(102.0, 113.0)),
        FBox2D(FVector2D(150.0, 85.25), FVector2D(170.0, 87.25)),
        FBox2D(FVector2D(150.0, 112.75), FVector2D(170.0, 114.75))
    };
    for (const FBox2D& Box : AtomicReservations) { Failed.Reserve(Box); Untouched.Reserve(Box); }
    FPrimaryPlacement Rejected = StalePrimaryPlacement();
    TestFalse(TEXT("Foreign leader blockers make both primary passes impossible"),
        FProofPrimaryLabelLayout::TryPlace(Failed, Center, 10.f, Size, 0, Rejected));
    TestTrue(TEXT("Both-pass failure invalidates bounds and zeros endpoints"), PrimaryGeometryCleared(Rejected));
    TestEqual(TEXT("Both-pass failure stops at the explicit anchor bound"), Rejected.Passes, 2);
    for (const FVector2D& Preferred : {FVector2D(85.0, 76.0), FVector2D(85.0, 114.0)})
    {
        FBox2D A, B;
        const bool bA = Failed.TryPlace(Preferred, Size, A), bB = Untouched.TryPlace(Preferred, Size, B);
        TestTrue(TEXT("Failed primary attempts reserve neither candidate rectangle"), bA && bB);
        TestEqual(TEXT("Post-failure rectangle probe matches untouched layout"), bA, bB);
        if (bA && bB) TestTrue(TEXT("Post-failure rectangle geometry matches untouched layout"),
            A.Min.Equals(B.Min, 0.0) && A.Max.Equals(B.Max, 0.0));
    }
    const FVector2D ProbePreferences[] = {{40.0, 85.75}, {40.0, 113.25}};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FBox2D A, B;
        FVector2D StartA, EndA, StartB, EndB;
        const bool bA = Failed.TryPlaceLinked(ProbePreferences[Index], FVector2D(20.0, 1.0), 3 + Index, A, StartA, EndA);
        const bool bB = Untouched.TryPlaceLinked(ProbePreferences[Index], FVector2D(20.0, 1.0), 3 + Index, B, StartB, EndB);
        TestTrue(TEXT("Crossing probe shows neither failed primary leader was reserved"), bA && bB);
        TestEqual(TEXT("Post-failure linked probe matches untouched layout"), bA, bB);
        if (bA && bB) TestTrue(TEXT("Post-failure linked geometry matches untouched layout"),
            A.Min.Equals(B.Min, 0.0) && A.Max.Equals(B.Max, 0.0)
            && StartA.Equals(StartB, 0.0) && EndA.Equals(EndB, 0.0));
    }
    return true;
}
#endif
