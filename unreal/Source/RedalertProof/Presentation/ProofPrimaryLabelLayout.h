#pragma once

#include "CoreMinimal.h"
#include "Presentation/ProofMarkerLayout.h"

/** Primary ID/HP labels prefer above the glyph, then try its opposite side. */
struct FProofPrimaryLabelLayout
{
    static constexpr int32 MaxPasses = 2;
    static constexpr int32 MaxCandidateCount = MaxPasses * FProofMarkerLayout::MaxCandidateCount;

    struct FPlacement
    {
        FBox2D Bounds = FBox2D(ForceInit);
        FVector2D Start = FVector2D::ZeroVector;
        FVector2D End = FVector2D::ZeroVector;
        FVector2D Preferred = FVector2D::ZeroVector;
        int32 Passes = 0;
    };

    static bool TryPlace(FProofMarkerLayout& Layout, const FVector2D& Screen, float Radius,
        const FVector2D& Size, int32 SourceReservation, FPlacement& Out)
    {
        Out = FPlacement{};
        if (!FMath::IsFinite(Screen.X) || !FMath::IsFinite(Screen.Y)
            || !FMath::IsFinite(Radius) || Radius <= 0.f
            || !FMath::IsFinite(Size.X) || !FMath::IsFinite(Size.Y)
            || Size.X <= 0.0 || Size.Y <= 0.0) return false;

        const FVector2D Preferences[] = {
            {Screen.X - Size.X * 0.5, Screen.Y - Radius - 4.0 - Size.Y},
            {Screen.X - Size.X * 0.5, Screen.Y + Radius + 4.0}
        };
        for (const FVector2D& Preferred : Preferences)
        {
            Out.Preferred = Preferred;
            ++Out.Passes;
            // The primitive retains its own viewport, footprint, source-token,
            // leader and 66-candidate checks. Failed passes reserve nothing.
            // Its displacement bound is relative to EACH preference, not the
            // original above-label position; at most 132 candidates are tried.
            if (Layout.TryPlaceLinked(Preferred, Size, SourceReservation, Out.Bounds, Out.Start, Out.End)) return true;
        }
        return false;
    }
};
