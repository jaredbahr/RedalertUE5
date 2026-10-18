#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Math/Box.h"
#include "Math/Transform.h"
#include "Presentation/ProofUnitViewGrounding.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofUnitViewGroundingTest, "RedalertProof.Controls.UnitViewGrounding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofUnitViewGroundingTest::RunTest(const FString&)
{
    // Synthetic local mesh bounds exercise the placement contract. This test
    // does not load art or replace the runtime mesh/component acceptance check.
    const FBox Cube(FVector(-50.0, -50.0, -50.0), FVector(50.0, 50.0, 50.0));
    const FBox GroundOrigin(FVector(-130.0, -70.0, 0.0), FVector(170.0, 80.0, 100.0));
    const FBox AboveOrigin(FVector(10.0, -45.0, 20.0), FVector(80.0, 15.0, 110.0));
    const FBox ArbitraryPivot(FVector(-10.0, 20.0, -30.0), FVector(40.0, 60.0, 70.0));
    constexpr double Tolerance = 1.e-6;

    const auto OffsetFor = [](const FBox& LocalBox, const FTransform& Source)
    {
        return FProofUnitViewGrounding::VerticalOffset(
            {LocalBox.Min.X, LocalBox.Min.Y, LocalBox.Min.Z},
            {LocalBox.Max.X, LocalBox.Max.Y, LocalBox.Max.Z},
            {Source.GetScaledAxis(EAxis::X).Z,
             Source.GetScaledAxis(EAxis::Y).Z,
             Source.GetScaledAxis(EAxis::Z).Z});
    };
    const auto CheckPlacement = [&](const FString& Label, const FBox& LocalBox,
        const FQuat& Rotation, const FVector& Scale, const FVector& ModelPosition)
    {
        const FVector SavedModelPosition = ModelPosition;
        FTransform Source(Rotation, FVector(917.0, -431.0, 2850.0), Scale);
        const FTransform SavedSource = Source;
        const double Offset = OffsetFor(LocalBox, Source);
        FTransform Placed = Source;
        Placed.SetTranslation(ModelPosition + FVector(0.0, 0.0, Offset));

        // Independent oracle: UE transforms the whole box, rather than calling
        // the helper again to calculate the expected minimum.
        const FBox WorldBox = LocalBox.TransformBy(Placed.ToMatrixWithScale());
        TestTrue(Label + TEXT(": transformed box bottom meets model ground"),
            FMath::Abs(WorldBox.Min.Z - ModelPosition.Z) <= Tolerance);

        // Also check all eight real FTransform corner positions. Reflection or
        // a tilted asymmetric pivot cannot hide behind a centered-box example.
        double CornerMinimumZ = TNumericLimits<double>::Max();
        for (int32 Corner = 0; Corner < 8; ++Corner)
        {
            const FVector LocalCorner(
                (Corner & 1) != 0 ? LocalBox.Max.X : LocalBox.Min.X,
                (Corner & 2) != 0 ? LocalBox.Max.Y : LocalBox.Min.Y,
                (Corner & 4) != 0 ? LocalBox.Max.Z : LocalBox.Min.Z);
            const FVector WorldCorner = Placed.TransformPosition(LocalCorner);
            CornerMinimumZ = FMath::Min(CornerMinimumZ, WorldCorner.Z);
        }
        TestTrue(Label + TEXT(": transformed corners meet model ground"),
            FMath::Abs(CornerMinimumZ - ModelPosition.Z) <= Tolerance);
        TestTrue(Label + TEXT(": component XY uses model XY"),
            Placed.GetTranslation().X == ModelPosition.X && Placed.GetTranslation().Y == ModelPosition.Y);
        TestTrue(Label + TEXT(": placement preserves rotation and signed scale"),
            Placed.GetRotation().Equals(Rotation, 0.0) && Placed.GetScale3D().Equals(Scale, 0.0));
        TestTrue(Label + TEXT(": source transform and model position are unchanged"),
            Source.Equals(SavedSource, 0.0) && ModelPosition.Equals(SavedModelPosition, 0.0));

        // A camera-rig translation or an earlier placement must not enter the
        // next offset. Each tick starts from the authoritative position again.
        Source.SetTranslation(FVector(-6000.0, 4100.0, -1200.0));
        TestTrue(Label + TEXT(": changed source translation cannot affect lift"),
            FMath::Abs(OffsetFor(LocalBox, Source) - Offset) <= Tolerance);
        for (int32 Tick = 0; Tick < 4; ++Tick)
        {
            Placed.SetTranslation(ModelPosition + FVector(0.0, 0.0, OffsetFor(LocalBox, Placed)));
            TestTrue(Label + TEXT(": repeated placement does not accumulate lift"),
                Placed.GetTranslation().Equals(ModelPosition + FVector(0.0, 0.0, Offset), Tolerance));
        }
        return Offset;
    };
    const auto CheckKnown = [&](const TCHAR* Label, const FBox& LocalBox,
        const FVector& Scale, double Expected)
    {
        const double Offset = CheckPlacement(Label, LocalBox, FQuat::Identity, Scale,
            FVector(211.0, -107.0, 123.0));
        TestTrue(FString(Label) + TEXT(": explicit expected offset"), FMath::Abs(Offset - Expected) <= Tolerance);
    };

    CheckKnown(TEXT("Measured centered Harvester cube"), Cube, FVector(1.0, 0.7, 0.7), 35.0);
    CheckKnown(TEXT("Non-skirmish fallback cube"), Cube, FVector(0.8, 0.8, 0.6), 30.0);
    CheckKnown(TEXT("Unscaled fallback cube"), Cube, FVector::OneVector, 50.0);
    CheckKnown(TEXT("Ground-origin proxy"), GroundOrigin, FVector(1.2, 0.8, 1.7), 0.0);
    CheckKnown(TEXT("Above-origin mesh needs a downward offset"), AboveOrigin, FVector(1.0, 2.0, 0.5), -10.0);
    CheckKnown(TEXT("Asymmetric pivot with negative Z scale"), ArbitraryPivot, FVector(2.0, 3.0, -0.5), 35.0);
    CheckKnown(TEXT("Ground-origin proxy reflected below its pivot"), GroundOrigin, FVector(1.0, 1.0, -0.5), 50.0);
    CheckKnown(TEXT("Collapsed vertical scale"), ArbitraryPivot, FVector(2.0, 0.7, 0.0), 0.0);

    constexpr double HalfPi = 1.57079632679489661923;
    TestTrue(TEXT("Quarter turn about Y gives a known local-X contribution"),
        FMath::Abs(CheckPlacement(TEXT("Rotated X supplies world Z"), ArbitraryPivot,
            FQuat(FVector(0.0, 1.0, 0.0), HalfPi), FVector(2.0, 3.0, 0.5), FVector::ZeroVector) - 80.0) <= Tolerance);
    TestTrue(TEXT("Quarter turn about X gives a known local-Y contribution"),
        FMath::Abs(CheckPlacement(TEXT("Rotated Y supplies world Z"), ArbitraryPivot,
            FQuat(FVector(1.0, 0.0, 0.0), HalfPi), FVector(2.0, 3.0, 0.5), FVector(0.0, 0.0, -23.5)) + 60.0) <= Tolerance);

    const FBox Boxes[] = {Cube, GroundOrigin, AboveOrigin, ArbitraryPivot};
    const FRotator Rotations[] = {
        FRotator::ZeroRotator, FRotator(0.0, 37.0, 0.0), FRotator(90.0, 0.0, 0.0),
        FRotator(0.0, 0.0, -90.0), FRotator(27.0, -68.0, 39.0), FRotator(-119.0, 151.0, -73.0)};
    const FVector Scales[] = {
        FVector::OneVector, FVector(2.0, 0.3, 1.7), FVector(-2.0, 0.3, 1.7),
        FVector(2.0, -0.3, -1.7), FVector(-2.0, -0.3, -1.7)};
    int32 Scenario = 0;
    for (const FBox& Box : Boxes)
        for (const FRotator& Rotation : Rotations)
            for (const FVector& Scale : Scales)
            {
                CheckPlacement(FString::Printf(TEXT("Bounds/rotation/signed-scale case %d"), ++Scenario),
                    Box, Rotation.Quaternion(), Scale, FVector(211.0, -107.0, -23.5));
            }
    AddInfo(TEXT("Checked 10 explicit offsets and 120 bounds/rotation/signed-scale combinations using real UE transforms."));
    return true;
}
#endif
