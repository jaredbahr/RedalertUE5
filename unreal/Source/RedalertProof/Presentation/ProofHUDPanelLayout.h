#pragma once

#include "CoreMinimal.h"

/** Exact existing HUD panel footprint, shared with pointer camera exclusion. */
struct FProofHUDPanelLayout
{
    float Scale;
    float Left;
    float Top;
    float Width;
    float Height;

    FProofHUDPanelLayout(float ViewportWidth, bool bSkirmish)
        : Scale(FMath::Clamp(ViewportWidth / 1280.f, 0.72f, 1.f))
        , Left(12.f * Scale)
        , Top(12.f * Scale)
        , Width(FMath::Min(720.f * Scale, FMath::Max(0.f, ViewportWidth - Left * 2.f)))
        , Height((bSkirmish ? 197.f : 183.f) * Scale)
    {}

    FBox2D Bounds() const
    {
        return FBox2D(FVector2D(Left, Top), FVector2D(Left + Width, Top + Height));
    }
};
