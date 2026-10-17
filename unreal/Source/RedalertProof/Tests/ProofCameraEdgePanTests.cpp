#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofCameraPan.h"
#include "Presentation/ProofHUDPanelLayout.h"
#include <limits>

namespace
{
FVector2D Edge(const FVector2D& Point, const FVector2D& Size = FVector2D(1280.0, 720.0))
{
    return FProofCameraPan::EdgeInput(Point, Size, FProofHUDPanelLayout(static_cast<float>(Size.X), true).Bounds(), true, true, true, false);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofCameraEdgeGeometryTest, "RedalertProof.CameraEdge.Geometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofCameraEdgeGeometryTest::RunTest(const FString&)
{
    TestEqual(TEXT("Center idle"), Edge(FVector2D(640, 400)), FVector2D::ZeroVector);
    TestEqual(TEXT("Left edge"), Edge(FVector2D(0, 400)), FVector2D(-1, 0));
    TestEqual(TEXT("Right edge"), Edge(FVector2D(1279, 400)), FVector2D(1, 0));
    TestEqual(TEXT("Top edge"), Edge(FVector2D(900, 0)), FVector2D(0, 1));
    TestEqual(TEXT("Bottom edge"), Edge(FVector2D(900, 719)), FVector2D(0, -1));
    TestEqual(TEXT("Top-left corner"), Edge(FVector2D(0, 0)), FVector2D(-1, 1));
    TestEqual(TEXT("Bottom-right corner"), Edge(FVector2D(1279, 719)), FVector2D(1, -1));
    TestEqual(TEXT("Strip ends at sixteen pixels"), Edge(FVector2D(16, 400)), FVector2D::ZeroVector);
    TestEqual(TEXT("Strip includes last pixel below sixteen"), Edge(FVector2D(15, 400)), FVector2D(-1, 0));
    TestEqual(TEXT("Actual small viewport right edge"), Edge(FVector2D(959, 300), FVector2D(960, 540)), FVector2D(1, 0));
    TestEqual(TEXT("Actual large viewport right edge"), Edge(FVector2D(1919, 700), FVector2D(1920, 1080)), FVector2D(1, 0));
    const FVector Corner = FProofCameraPan::Apply(FVector::ZeroVector, Edge(FVector2D(0, 0)), 0.1);
    TestTrue(TEXT("Corner movement normalized to existing speed"), FMath::IsNearlyEqual(Corner.Size(), 60.0, 1.e-6));
    TestTrue(TEXT("Corner follows fixed camera screen axes"), Corner.X > 0.0 && Corner.Y > 0.0 && Corner.Z == 0.0);
    FVector ThirtyHz = FVector::ZeroVector, SixtyHz = FVector::ZeroVector;
    for (int32 Frame = 0; Frame < 30; ++Frame)
        ThirtyHz = FProofCameraPan::Apply(ThirtyHz, Edge(FVector2D(0, 0)), 1.0 / 30.0);
    for (int32 Frame = 0; Frame < 60; ++Frame)
        SixtyHz = FProofCameraPan::Apply(SixtyHz, Edge(FVector2D(0, 0)), 1.0 / 60.0);
    TestTrue(TEXT("Equal elapsed edge input at 30/60 Hz produces equal displacement"), ThirtyHz.Equals(SixtyHz, 1.e-6));
    TestTrue(TEXT("One second edge movement uses existing 600 cm/s speed"), FMath::IsNearlyEqual(ThirtyHz.Size(), 600.0, 1.e-6));
    const FVector Both = FProofCameraPan::Apply(FVector::ZeroVector, FVector2D(1, 0) + Edge(FVector2D(1279, 400)), 0.1);
    TestTrue(TEXT("Arrows plus edge never double speed"), Both.Equals(FVector(-60, 0, 0), 1.e-6));
    TestTrue(TEXT("Edge movement respects existing center bound"), FProofCameraPan::Apply(FVector(9499, 0, 0), Edge(FVector2D(0, 400)), 0.1).Equals(FVector(9500, 0, 0), 1.e-6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofCameraEdgeOwnershipTest, "RedalertProof.CameraEdge.Ownership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofCameraEdgeOwnershipTest::RunTest(const FString&)
{
    const FVector2D Pointer(0, 400), Size(1280, 720);
    const FBox2D Panel = FProofHUDPanelLayout(1280.f, true).Bounds();
    TestEqual(TEXT("Opt-in required"), FProofCameraPan::EdgeInput(Pointer, Size, Panel, false, true, true, false), FVector2D::ZeroVector);
    TestEqual(TEXT("Focus required"), FProofCameraPan::EdgeInput(Pointer, Size, Panel, true, false, true, false), FVector2D::ZeroVector);
    TestEqual(TEXT("Foreground required"), FProofCameraPan::EdgeInput(Pointer, Size, Panel, true, true, false, false), FVector2D::ZeroVector);
    TestEqual(TEXT("Held mouse gesture owns pointer"), FProofCameraPan::EdgeInput(Pointer, Size, Panel, true, true, true, true), FVector2D::ZeroVector);
    TestEqual(TEXT("Top panel overlaps strip and suppresses scroll"), Edge(FVector2D(100, 13)), FVector2D::ZeroVector);
    TestEqual(TEXT("HUD exact border excluded"), Edge(FVector2D(12, 12)), FVector2D::ZeroVector);
    TestEqual(TEXT("Scaled HUD excluded"), Edge(FVector2D(10, 13), FVector2D(960, 540)), FVector2D::ZeroVector);
    const FProofHUDPanelLayout Legacy(1280.f, false), Small(960.f, true);
    TestTrue(TEXT("Legacy panel footprint unchanged"), Legacy.Left == 12.f && Legacy.Top == 12.f && Legacy.Width == 720.f && Legacy.Height == 183.f);
    TestTrue(TEXT("Small skirmish panel footprint unchanged"), Small.Scale == .75f && Small.Left == 9.f && Small.Width == 540.f && Small.Height == 147.75f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofCameraEdgeInvalidTest, "RedalertProof.CameraEdge.InvalidInputs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofCameraEdgeInvalidTest::RunTest(const FString&)
{
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Inf = std::numeric_limits<double>::infinity();
    for (const FVector2D& Point : {FVector2D(-1, 400), FVector2D(1280, 400), FVector2D(0, -1), FVector2D(0, 720), FVector2D(NaN, 0), FVector2D(0, Inf)})
        TestEqual(TEXT("Outside or nonfinite pointer rejected"), Edge(Point), FVector2D::ZeroVector);
    for (const FVector2D& Size : {FVector2D(0, 720), FVector2D(-1, 720), FVector2D(1280, 0), FVector2D(32, 32), FVector2D(NaN, 720), FVector2D(1280, Inf)})
        TestEqual(TEXT("Degenerate or nonfinite viewport rejected"), Edge(FVector2D(0, 0), Size), FVector2D::ZeroVector);
    TestEqual(TEXT("Reversed HUD geometry rejected"), FProofCameraPan::EdgeInput(FVector2D(0, 0), FVector2D(1280, 720), FBox2D(FVector2D(10, 10), FVector2D(1, 1)), true, true, true, false), FVector2D::ZeroVector);
    TestEqual(TEXT("Unavailable HUD geometry rejected"), FProofCameraPan::EdgeInput(FVector2D(0, 0), FVector2D(1280, 720), FBox2D(ForceInit), true, true, true, false), FVector2D::ZeroVector);
    return true;
}
#endif
