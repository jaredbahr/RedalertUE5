#include "Presentation/ProofCameraPan.h"

namespace
{
bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

bool IsFiniteVector2D(const FVector2D& Value)
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
}

FVector2D NormalizePanInput(const FVector2D& Input)
{
    const double LargestAxis = FMath::Max(FMath::Abs(Input.X), FMath::Abs(Input.Y));
    if (LargestAxis <= 1.0)
    {
        const double LengthSquared = Input.SizeSquared();
        return LengthSquared > 1.0 ? Input / FMath::Sqrt(LengthSquared) : Input;
    }

    // Scale before measuring to keep normalization well-defined for every
    // finite input, including values whose squared magnitude would overflow.
    const FVector2D Scaled = Input / LargestAxis;
    return Scaled / FMath::Sqrt(Scaled.SizeSquared());
}
}

FVector FProofCameraPan::Apply(const FVector& CurrentGroundCenter, const FVector2D& ArrowInput, double DeltaSeconds)
{
    if (!IsFiniteVector(CurrentGroundCenter))
    {
        return FVector::ZeroVector;
    }

    FVector Result(CurrentGroundCenter.X, CurrentGroundCenter.Y, 0.0);
    if (!IsFiniteVector2D(ArrowInput) || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0)
    {
        return Result;
    }

    const FVector2D NormalizedInput = NormalizePanInput(ArrowInput);
    // Camera local +Y (screen-right) is world -X at yaw 90; screen-up projects to +Y.
    const FVector2D Direction(-NormalizedInput.X, NormalizedInput.Y);
    if (Direction.IsNearlyZero())
    {
        return Result;
    }

    const double Step = FMath::Min(DeltaSeconds, MaxDeltaSeconds) * Speed;
    const double Limit = WorldExtent - BoundaryMargin;
    Result.X = FMath::Clamp(Result.X + Direction.X * Step, -Limit, Limit);
    Result.Y = FMath::Clamp(Result.Y + Direction.Y * Step, -Limit, Limit);
    return Result;
}

