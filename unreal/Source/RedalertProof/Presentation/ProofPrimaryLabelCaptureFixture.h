#pragma once

#include "CoreMinimal.h"

/** Explicit rendered-obstruction fixture; never enabled by ordinary HUD code. */
struct FProofPrimaryLabelCaptureFixture
{
    static bool Build(const FVector2D& Screen, const FVector2D& Size,
        const FVector2D& Viewport, TArray<FBox2D>& Out)
    {
        Out.Reset();
        if (!FMath::IsFinite(Screen.X) || !FMath::IsFinite(Screen.Y)
            || !Viewport.Equals(FVector2D(1280.0, 720.0), 0.0)
            || !Size.Equals(FVector2D(32.0, 25.0), 1.e-6)) return false;
        // Selected radius18 + line thickness1. The U has a two-pixel gap
        // from this footprint. Every above-search candidate either meets the
        // U or the source; the below preference has a three-pixel clear gap.
        const FBox2D Masks[] = {
            FBox2D(Screen - FVector2D(160.0, 160.0), Screen + FVector2D(160.0, -21.0)),
            FBox2D(Screen + FVector2D(-160.0, -21.0), Screen + FVector2D(-21.0, 19.0)),
            FBox2D(Screen + FVector2D(21.0, -21.0), Screen + FVector2D(160.0, 19.0))
        };
        for (const FBox2D& Mask : Masks)
            if (Mask.Min.X < 4.0 || Mask.Min.Y < 4.0
                || Mask.Max.X > Viewport.X - 4.0 || Mask.Max.Y > Viewport.Y - 4.0) return false;
        for (const FBox2D& Mask : Masks) Out.Add(Mask);
        return true;
    }
};
