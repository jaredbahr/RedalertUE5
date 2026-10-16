#pragma once

#include "CoreMinimal.h"

/**
 * Pure ground-plane camera pan math for the proof view.
 * ArrowInput.X is screen-right and ArrowInput.Y is screen-up. With the proof
 * camera's fixed default yaw (90 degrees), those axes map to world +X and +Y.
 */
struct REDALERTPROOF_API FProofCameraPan
{
    static constexpr double Speed = 600.0;
    static constexpr double MaxDeltaSeconds = 0.1;
    static constexpr double WorldExtent = 10000.0;
    static constexpr double BoundaryMargin = 500.0;

    /** Return the updated ground center, always with Z set to zero. */
    static FVector Apply(const FVector& CurrentGroundCenter, const FVector2D& ArrowInput, double DeltaSeconds);
};
