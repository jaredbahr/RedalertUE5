#include "Presentation/ProofMarkerLayout.h"

namespace
{
bool IsFinitePoint(const FVector2D& Point)
{
    return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
}

bool IsUsableBox(const FBox2D& Box)
{
    return Box.bIsValid && IsFinitePoint(Box.Min) && IsFinitePoint(Box.Max)
        && Box.Min.X < Box.Max.X && Box.Min.Y < Box.Max.Y;
}
}

FProofMarkerLayout::FProofMarkerLayout(FVector2D Viewport, float Margin, float Gap, float MaxDisplacement)
    : ViewportSize(Viewport), Inset(Margin), Separation(Gap), DisplacementLimit(MaxDisplacement)
    , bValid(IsFinitePoint(Viewport) && Viewport.X > 0.0 && Viewport.Y > 0.0
        && FMath::IsFinite(Margin) && Margin >= 0.f
        && FMath::IsFinite(Gap) && Gap >= 0.f
        && FMath::IsFinite(MaxDisplacement) && MaxDisplacement >= 0.f
        && Viewport.X > 2.0 * Margin && Viewport.Y > 2.0 * Margin)
{
}

bool FProofMarkerLayout::IsAnchorVisible(const FVector2D& Anchor) const
{
    return bValid && IsFinitePoint(Anchor) && Anchor.X >= 0.0 && Anchor.Y >= 0.0
        && Anchor.X < ViewportSize.X && Anchor.Y < ViewportSize.Y;
}

void FProofMarkerLayout::Reserve(const FBox2D& Bounds)
{
    if (bValid && IsUsableBox(Bounds))
    {
        Occupied.Add(Bounds);
    }
}

bool FProofMarkerLayout::AcceptCandidate(const FVector2D& TopLeft, const FVector2D& Preferred, const FVector2D& Size, FBox2D& OutBounds)
{
    const FVector2D BottomRight = TopLeft + Size;
    if (!IsFinitePoint(TopLeft) || !IsFinitePoint(BottomRight)
        || BottomRight.X <= TopLeft.X || BottomRight.Y <= TopLeft.Y
        || FVector2D::Distance(TopLeft, Preferred) > DisplacementLimit
        || TopLeft.X < Inset || TopLeft.Y < Inset
        || BottomRight.X > ViewportSize.X - Inset || BottomRight.Y > ViewportSize.Y - Inset)
    {
        return false;
    }
    for (const FBox2D& Other : Occupied)
    {
        if (TopLeft.X < Other.Max.X + Separation && BottomRight.X + Separation > Other.Min.X
            && TopLeft.Y < Other.Max.Y + Separation && BottomRight.Y + Separation > Other.Min.Y)
        {
            return false;
        }
    }
    OutBounds = FBox2D(TopLeft, BottomRight);
    Occupied.Add(OutBounds);
    return true;
}

bool FProofMarkerLayout::TryPlace(const FVector2D& PreferredTopLeft, const FVector2D& Size, FBox2D& OutBounds)
{
    OutBounds = FBox2D(ForceInit);
    if (!bValid || !IsFinitePoint(PreferredTopLeft) || !IsFinitePoint(Size)
        || Size.X <= 0.0 || Size.Y <= 0.0
        || Size.X > ViewportSize.X - 2.0 * Inset || Size.Y > ViewportSize.Y - 2.0 * Inset)
    {
        return false;
    }
    if (AcceptCandidate(PreferredTopLeft, PreferredTopLeft, Size, OutBounds))
    {
        return true;
    }
    const FVector2D EdgeFit(
        FMath::Clamp(PreferredTopLeft.X, Inset, ViewportSize.X - Inset - Size.X),
        FMath::Clamp(PreferredTopLeft.Y, Inset, ViewportSize.Y - Inset - Size.Y));
    if (AcceptCandidate(EdgeFit, PreferredTopLeft, Size, OutBounds))
    {
        return true;
    }
    // Diagonals use unit vectors so every candidate obeys the same radial bound.
    constexpr double Diagonal = 0.7071067811865475;
    const FVector2D Directions[] = {
        {0.0, -1.0}, {1.0, 0.0}, {0.0, 1.0}, {-1.0, 0.0},
        {Diagonal, -Diagonal}, {Diagonal, Diagonal}, {-Diagonal, Diagonal}, {-Diagonal, -Diagonal}
    };
    static_assert(2 + 8 * UE_ARRAY_COUNT(Directions) == MaxCandidateCount, "Keep the candidate bound explicit");
    for (int32 Ring = 1; Ring <= 8; ++Ring)
    {
        const double Radius = DisplacementLimit * (static_cast<double>(Ring) / 8.0);
        for (const FVector2D& Direction : Directions)
        {
            if (AcceptCandidate(PreferredTopLeft + Direction * Radius, PreferredTopLeft, Size, OutBounds))
            {
                return true;
            }
        }
    }
    return false;
}
