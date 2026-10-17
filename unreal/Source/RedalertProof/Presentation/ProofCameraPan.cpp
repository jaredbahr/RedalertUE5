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

FVector2D FProofCameraPan::EdgeInput(const FVector2D& Pointer, const FVector2D& ViewportSize,
    const FBox2D& HUDPanel, bool bEnabled, bool bFocused, bool bForeground, bool bMouseButtonHeld)
{
    if (!bEnabled || !bFocused || !bForeground || bMouseButtonHeld
        || !IsFiniteVector2D(Pointer) || !IsFiniteVector2D(ViewportSize)
        || ViewportSize.X <= EdgeWidthPixels * 2.0 || ViewportSize.Y <= EdgeWidthPixels * 2.0
        || Pointer.X < 0.0 || Pointer.Y < 0.0 || Pointer.X >= ViewportSize.X || Pointer.Y >= ViewportSize.Y
        || !HUDPanel.bIsValid || !IsFiniteVector2D(HUDPanel.Min) || !IsFiniteVector2D(HUDPanel.Max)
        || HUDPanel.Min.X > HUDPanel.Max.X || HUDPanel.Min.Y > HUDPanel.Max.Y
        || (Pointer.X >= HUDPanel.Min.X && Pointer.X <= HUDPanel.Max.X
            && Pointer.Y >= HUDPanel.Min.Y && Pointer.Y <= HUDPanel.Max.Y))
    {
        return FVector2D::ZeroVector;
    }
    return FVector2D(Pointer.X < EdgeWidthPixels ? -1.0 : Pointer.X >= ViewportSize.X - EdgeWidthPixels ? 1.0 : 0.0,
        Pointer.Y < EdgeWidthPixels ? 1.0 : Pointer.Y >= ViewportSize.Y - EdgeWidthPixels ? -1.0 : 0.0);
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
