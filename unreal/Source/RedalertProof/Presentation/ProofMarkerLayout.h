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
    /** Invalid/nonfinite/empty reservations return INDEX_NONE. Reserve all static
     * UI/glyph footprints before placement; reservations may cross viewport edges. */
    int32 Reserve(const FBox2D& Bounds);
    /** Success reserves a fully contained rectangle; failure invalidates OutBounds. */
    bool TryPlace(const FVector2D& PreferredTopLeft, const FVector2D& Size, FBox2D& OutBounds);
    /** Place with a clear leader from the source reservation edge. Only that
     * reservation index is exempt from leader collision; all others count.
     * Source must be fully viewport-visible. Success reserves both group and
     * leader; failure reserves neither. Later groups also avoid earlier leaders. */
    bool TryPlaceLinked(const FVector2D& PreferredTopLeft, const FVector2D& Size,
        int32 SourceReservation, FBox2D& OutBounds, FVector2D& OutStart, FVector2D& OutEnd);

private:
    bool AcceptCandidate(const FVector2D& TopLeft, const FVector2D& Preferred, const FVector2D& Size, FBox2D& OutBounds,
        int32 SourceReservation = INDEX_NONE, FVector2D* OutStart = nullptr, FVector2D* OutEnd = nullptr);
    bool Place(const FVector2D& PreferredTopLeft, const FVector2D& Size, FBox2D& OutBounds,
        int32 SourceReservation = INDEX_NONE, FVector2D* OutStart = nullptr, FVector2D* OutEnd = nullptr);

    FVector2D ViewportSize;
    double Inset;
    double Separation;
    double DisplacementLimit;
    bool bValid;
    TArray<FBox2D> Occupied;
    struct FLeader { FVector2D Start; FVector2D End; };
    TArray<FLeader> Leaders;
};
