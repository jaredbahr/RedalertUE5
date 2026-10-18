#pragma once

#include "CoreMinimal.h"
#include "Presentation/ProofMarkerLayout.h"

/** Full measured legacy labels use the existing bounded linked placement. */
struct FProofLegacyLabelLayout
{
    static bool TryPlace(FProofMarkerLayout& Layout, const FVector2D& Screen, float Radius,
        const FVector2D& TextSize, float UiScale, int32 SourceReservation,
        FBox2D& OutBounds, FVector2D& OutStart, FVector2D& OutEnd)
    {
        OutBounds = FBox2D(ForceInit);
        OutStart = OutEnd = FVector2D::ZeroVector;
        if (!FMath::IsFinite(Screen.X) || !FMath::IsFinite(Screen.Y)
            || !FMath::IsFinite(Radius) || Radius <= 0.f
            || !FMath::IsFinite(TextSize.X) || !FMath::IsFinite(TextSize.Y)
            || TextSize.X <= 0.0 || TextSize.Y <= 0.0
            || !FMath::IsFinite(UiScale) || UiScale <= 0.f) return false;
        const double Padding = 3.0 * UiScale;
        const FVector2D Size = TextSize + FVector2D(Padding * 2.0, Padding * 2.0);
        const FVector2D Preferred(Screen.X + Radius + 4.0 * UiScale,
            Screen.Y - TextSize.Y * 0.5 - Padding);
        return Layout.TryPlaceLinked(Preferred, Size, SourceReservation, OutBounds, OutStart, OutEnd);
    }
};
