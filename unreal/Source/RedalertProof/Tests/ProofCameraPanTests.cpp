#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofCameraPan.h"
#include "Math/RotationMatrix.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofCameraPanInputTest, "RedalertProof.Controls.CameraPanInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofCameraPanInputTest::RunTest(const FString&)
{
    const FVector Start(0.0, 0.0, 0.0);
    TestTrue(TEXT("No input preserves the default world center"), FProofCameraPan::Apply(Start, FVector2D::ZeroVector, 1.0).Equals(Start, 0.0));
    TestTrue(TEXT("Zero delta preserves the center"), FProofCameraPan::Apply(Start, FVector2D(1.0, 0.0), 0.0).Equals(Start, 0.0));
    TestTrue(TEXT("Negative delta preserves the center"), FProofCameraPan::Apply(Start, FVector2D(1.0, 0.0), -0.1).Equals(Start, 0.0));

    const double Infinity = std::numeric_limits<double>::infinity();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    TestTrue(TEXT("Infinite X input is rejected"), FProofCameraPan::Apply(Start, FVector2D(Infinity, 0.0), 0.1).Equals(Start, 0.0));
    TestTrue(TEXT("NaN Y input is rejected"), FProofCameraPan::Apply(Start, FVector2D(0.0, NaN), 0.1).Equals(Start, 0.0));
    TestTrue(TEXT("Infinite delta is rejected"), FProofCameraPan::Apply(Start, FVector2D(1.0, 0.0), Infinity).Equals(Start, 0.0));
    TestTrue(TEXT("NaN delta is rejected"), FProofCameraPan::Apply(Start, FVector2D(1.0, 0.0), NaN).Equals(Start, 0.0));

    const FVector NonzeroHeight(100.0, -200.0, 3200.0);
    TestTrue(TEXT("No-input result stays on the ground plane"), FProofCameraPan::Apply(NonzeroHeight, FVector2D::ZeroVector, 0.1).Equals(FVector(100.0, -200.0, 0.0), 0.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofCameraPanMovementTest, "RedalertProof.Controls.CameraPanMovement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofCameraPanMovementTest::RunTest(const FString&)
{
    const FRotator CameraRotation(-45.0, 90.0, 0.0);
    const FRotationMatrix CameraMatrix(CameraRotation);
    FVector GroundRight = CameraMatrix.GetUnitAxis(EAxis::Y);
    GroundRight.Z = 0.0;
    GroundRight.Normalize();
    FVector GroundUp = CameraMatrix.GetUnitAxis(EAxis::Z);
    GroundUp.Z = 0.0;
    GroundUp.Normalize();
    AddInfo(FString::Printf(TEXT("Camera ground basis right=%s up=%s"), *GroundRight.ToString(), *GroundUp.ToString()));
    // UE rotation-matrix trig is approximate; 0.0001 cm still resolves the pan direction and 60 cm step.
    TestTrue(TEXT("Default camera screen-right ground axis is world -X"), GroundRight.Equals(FVector(-1.0, 0.0, 0.0), 1.e-6));
    TestTrue(TEXT("Default camera screen-up ground axis is world +Y"), GroundUp.Equals(FVector(0.0, 1.0, 0.0), 1.e-6));

    const FVector Start = FVector::ZeroVector;
    const FVector Right = FProofCameraPan::Apply(Start, FVector2D(1.0, 0.0), 0.1);
    TestTrue(TEXT("Screen-right follows the camera's ground-projected local right axis"), Right.Equals(GroundRight * 60.0, 1.e-4));
    const FVector Up = FProofCameraPan::Apply(Start, FVector2D(0.0, 1.0), 0.1);
    TestTrue(TEXT("Screen-up follows the camera's ground-projected local up axis"), Up.Equals(GroundUp * 60.0, 1.e-4));

    const FVector ExpectedDiagonal = (GroundRight + GroundUp).GetSafeNormal() * 60.0;
    const FVector Diagonal = FProofCameraPan::Apply(Start, FVector2D(1.0, 1.0), 0.1);
    TestTrue(TEXT("Diagonal input is normalized"), Diagonal.Equals(ExpectedDiagonal, 1.e-4));

    const FVector LongFrame = FProofCameraPan::Apply(Start, FVector2D(-1.0, 0.0), 2.0);
    TestTrue(TEXT("Long delta is capped to one tenth of a second"), LongFrame.Equals(FVector(60.0, 0.0, 0.0), 1.e-9));

    const double Limit = FProofCameraPan::WorldExtent - FProofCameraPan::BoundaryMargin;
    const FVector ClampedPositive = FProofCameraPan::Apply(FVector(Limit - 5.0, 0.0, 0.0), FVector2D(-1.0, 0.0), 0.1);
    TestTrue(TEXT("Positive world boundary is clamped with margin"), ClampedPositive.Equals(FVector(Limit, 0.0, 0.0), 1.e-9));
    const FVector ClampedNegative = FProofCameraPan::Apply(FVector(-Limit + 5.0, 0.0, 0.0), FVector2D(1.0, 0.0), 0.1);
    TestTrue(TEXT("Negative world boundary is clamped with margin"), ClampedNegative.Equals(FVector(-Limit, 0.0, 0.0), 1.e-9));
    return true;
}
#endif

