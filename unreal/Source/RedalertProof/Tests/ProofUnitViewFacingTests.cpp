#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Math/Box.h"
#include "Math/Transform.h"
#include "Presentation/ProofUnitViewFacing.h"
#include "Presentation/ProofUnitViewGrounding.h"
#include <limits>

// Keep the numerical oracle meaningful under the module's /fp:fast setting.
#if defined(_MSC_VER)
#pragma float_control(precise, on, push)
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofUnitViewFacingTest, "RedalertProof.Controls.UnitViewFacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofUnitViewFacingTest::RunTest(const FString&)
{
    constexpr double Pi = 3.14159265358979323846;
    constexpr double Tolerance = 1.e-9;
    constexpr double BoundsTolerance = 1.e-6; // Match the accepted native grounding oracle (cm).
    const auto Delta = [](const FVector& Forward, const FVector& Direction, double& Radians)
    {
        return FProofUnitViewFacing::TryYawDelta({Forward.X, Forward.Y, Forward.Z},
            {Direction.X, Direction.Y, Direction.Z}, Radians);
    };
    double Radians = 0.0;
    TestTrue(TEXT("Verified +Y forward to +X requires clockwise quarter turn"),
        Delta(FVector(0.0, 1.0, 0.0), FVector(1.0, 0.0, 0.0), Radians)
        && FMath::Abs(Radians + Pi / 2.0) < Tolerance);
    TestTrue(TEXT("Verified +Y forward to -X requires counterclockwise quarter turn"),
        Delta(FVector(0.0, 1.0, 0.0), FVector(-1.0, 0.0, 0.0), Radians)
        && FMath::Abs(Radians - Pi / 2.0) < Tolerance);
    TestTrue(TEXT("Reverse requires a half turn"),
        Delta(FVector(0.0, 1.0, 0.0), FVector(0.0, -1.0, 0.0), Radians)
        && FMath::Abs(FMath::Abs(Radians) - Pi) < Tolerance);

    // This uses actual UE quaternion composition and transformed box/corner
    // semantics. The portable helper test is not a substitute for running it.
    const FQuat Rotations[] = {FQuat::Identity, FRotator(0.0, 143.0, 0.0).Quaternion(),
        FRotator(27.0, -68.0, 39.0).Quaternion(), FRotator(-119.0, 151.0, -73.0).Quaternion()};
    const FVector Scales[] = {FVector::OneVector, FVector(2.0, 0.3, 1.7),
        FVector(-2.0, 0.3, 1.7), FVector(2.0, -0.3, -1.7), FVector(-2.0, -0.3, -1.7)};
    const FVector Directions[] = {FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0),
        FVector(-1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0), FVector(0.6, 0.8, 0.0),
        FVector(-0.6, 0.8, 0.0), FVector(-0.6, -0.8, 0.0), FVector(0.6, -0.8, 0.0)};
    const FBox Bounds(FVector(-10.0, 20.0, -30.0), FVector(40.0, 60.0, 70.0));
    const FVector ModelPosition(211.0, -107.0, 23.5);
    for (const FQuat& Rotation : Rotations)
    for (const FVector& Scale : Scales)
    for (const FVector& Direction : Directions)
    {
        const FTransform Before(Rotation, FVector(917.0, -431.0, 2850.0), Scale);
        FTransform View = Before;
        const FVector SavedDirection = Direction;
        TestTrue(TEXT("A new stationary view can use stored direction immediately"),
            Delta(View.GetScaledAxis(EAxis::Y), Direction, Radians));
        View.SetRotation(FQuat(FVector(0.0, 0.0, 1.0), Radians) * View.GetRotation());
        const FVector Forward = View.GetScaledAxis(EAxis::Y);
        // Independent oracle: actual UE transformed local +Y, not helper angle.
        const double Length = std::hypot(Forward.X, Forward.Y);
        TestTrue(TEXT("Actual UE transformed front has requested positive alignment"),
            Length > 0.0 && FMath::Abs(Forward.X / Length - Direction.X) < Tolerance
            && FMath::Abs(Forward.Y / Length - Direction.Y) < Tolerance);
        TestTrue(TEXT("World yaw preserves all tilted scaled axes' Z components"),
            FMath::Abs(View.GetScaledAxis(EAxis::X).Z - Before.GetScaledAxis(EAxis::X).Z) < Tolerance
            && FMath::Abs(View.GetScaledAxis(EAxis::Y).Z - Before.GetScaledAxis(EAxis::Y).Z) < Tolerance
            && FMath::Abs(View.GetScaledAxis(EAxis::Z).Z - Before.GetScaledAxis(EAxis::Z).Z) < Tolerance);
        TestTrue(TEXT("Facing preserves signed nonuniform scale and authoritative direction"),
            View.GetScale3D().Equals(Scale, 0.0) && Direction.Equals(SavedDirection, 0.0));

        const double Lift = FProofUnitViewGrounding::VerticalOffset(
            {Bounds.Min.X, Bounds.Min.Y, Bounds.Min.Z}, {Bounds.Max.X, Bounds.Max.Y, Bounds.Max.Z},
            {View.GetScaledAxis(EAxis::X).Z, View.GetScaledAxis(EAxis::Y).Z, View.GetScaledAxis(EAxis::Z).Z});
        View.SetTranslation(ModelPosition + FVector(0.0, 0.0, Lift));
        const FBox WorldBox = Bounds.TransformBy(View.ToMatrixWithScale());
        TestTrue(TEXT("Rotated view remains grounded with authoritative X/Y"),
            FMath::Abs(WorldBox.Min.Z - ModelPosition.Z) < BoundsTolerance
            && View.GetTranslation().X == ModelPosition.X && View.GetTranslation().Y == ModelPosition.Y);
        const FQuat Aligned = View.GetRotation();
        for (int32 Repeat = 0; Repeat < 16; ++Repeat)
        {
            TestTrue(TEXT("Repeated stationary direction remains valid"),
                Delta(View.GetScaledAxis(EAxis::Y), Direction, Radians));
            View.SetRotation(FQuat(FVector(0.0, 0.0, 1.0), Radians) * View.GetRotation());
        }
        TestTrue(TEXT("Stationary repeated snapshots do not cause visible orientation drift"),
            View.GetRotation().Equals(Aligned, Tolerance));
    }

    const double Tiny = std::numeric_limits<double>::denorm_min();
    const double Huge = std::numeric_limits<double>::max();
    volatile double RuntimeTiny = Tiny;
    if (RuntimeTiny != 0.0)
    {
        TestTrue(TEXT("Usable subnormal vectors retain cardinal direction without reciprocal overflow"),
            Delta(FVector(0.0, Tiny, 0.0), FVector(Tiny, 0.0, 0.0), Radians)
            && FMath::Abs(Radians + Pi / 2.0) < Tolerance);
        TestTrue(TEXT("Usable subnormal vectors retain diagonal direction without reciprocal overflow"),
            Delta(FVector(Tiny, Tiny, 0.0), FVector(-Tiny, Tiny, 0.0), Radians)
            && FMath::Abs(Radians - Pi / 2.0) < Tolerance);
    }
    else
    {
        // DAZ may make the input compare as zero. Do not change native FP mode.
        Radians = 123.0;
        TestTrue(TEXT("Effective-zero subnormal input rejects and clears the angle"),
            !Delta(FVector(0.0, Tiny, 0.0), FVector(Tiny, 0.0, 0.0), Radians)
            && Radians == 0.0 && !std::signbit(Radians));
    }
    const double MinimumNormal = std::numeric_limits<double>::min();
    for (const double Small : {MinimumNormal, 1.e-200})
    {
        TestTrue(TEXT("Nonzero small normal vectors retain direction when original squaring underflows"),
            Delta(FVector(Small, 2.0 * Small, 0.0), FVector(-2.0 * Small, Small, 0.0), Radians)
            && FMath::Abs(Radians - Pi / 2.0) < Tolerance);
    }
    TestTrue(TEXT("Extreme finite vectors do not overflow normalization"),
        Delta(FVector(Huge, Huge, Huge), FVector(-Huge, Huge, 0.0), Radians)
        && FMath::Abs(Radians - Pi / 2.0) < Tolerance);
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    const FVector InvalidDirections[] = {FVector::ZeroVector, FVector(0.0, -0.0, 0.0),
        FVector(1.0, 0.0, 1.0), FVector(1.0, 0.0, NaN), FVector(NaN, 1.0, 0.0), FVector(1.0, Infinity, 0.0)};
    for (const FVector& Direction : InvalidDirections)
    {
        Radians = 123.0;
        TestTrue(TEXT("Unset or invalid authoritative direction preserves orientation"),
            !Delta(FVector(0.0, 1.0, 0.0), Direction, Radians) && Radians == 0.0);
    }
    const FVector InvalidForward[] = {FVector::ZeroVector, FVector(0.0, 0.0, 1.0),
        FVector(NaN, 1.0, 0.0), FVector(0.0, Infinity, 0.0), FVector(0.0, 1.0, NaN)};
    for (const FVector& Forward : InvalidForward)
    {
        Radians = 123.0;
        TestTrue(TEXT("Degenerate or invalid forward projection preserves orientation"),
            !Delta(Forward, FVector(1.0, 0.0, 0.0), Radians) && Radians == 0.0);
    }
    return true;
}
#if defined(_MSC_VER)
#pragma float_control(pop)
#endif
#endif
