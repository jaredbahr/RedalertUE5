#pragma once

#include "CoreMinimal.h"

/** Per-frame presentation-only rectangle placement. Call higher-priority markers first. */
class REDALERTPROOF_API FProofMarkerLayout
{
public:
    // Preferred, edge-fit, then eight rings of eight directions. This is a local
    // search, not a packing solver: failure can occur even if other space exists.
    static constexpr int32 MaxCandidateCount = 66;

    FProofMarkerLayout(FVector2D Viewport, float Margin = 4.f, float Gap = 2.f, float MaxDisplacement = 64.f);

    /** Anchor visibility ignores reserved rectangles and the layout margin. */
    bool IsAnchorVisible(const FVector2D& Anchor) const;
    /** Invalid/nonfinite/empty reservations are ignored. Reservations may cross viewport edges. */
    void Reserve(const FBox2D& Bounds);
    /** Success reserves a fully contained rectangle; failure invalidates OutBounds. */
    bool TryPlace(const FVector2D& PreferredTopLeft, const FVector2D& Size, FBox2D& OutBounds);

private:
    bool AcceptCandidate(const FVector2D& TopLeft, const FVector2D& Preferred, const FVector2D& Size, FBox2D& OutBounds);

    FVector2D ViewportSize;
    double Inset;
    double Separation;
    double DisplacementLimit;
    bool bValid;
    TArray<FBox2D> Occupied;
};
