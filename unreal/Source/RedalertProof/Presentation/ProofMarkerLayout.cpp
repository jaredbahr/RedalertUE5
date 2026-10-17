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

// Slab clipping includes touching edges. Padding accounts for line width/gap.
bool SegmentHitsBox(const FVector2D& A, const FVector2D& B, const FBox2D& Box, double Padding)
{
    double Near = 0.0, Far = 1.0;
    for (int32 Axis = 0; Axis < 2; ++Axis)
    {
        const double Start = Axis == 0 ? A.X : A.Y;
        const double Delta = Axis == 0 ? B.X - A.X : B.Y - A.Y;
        const double Min = (Axis == 0 ? Box.Min.X : Box.Min.Y) - Padding;
        const double Max = (Axis == 0 ? Box.Max.X : Box.Max.Y) + Padding;
        if (FMath::Abs(Delta) < 1.e-9)
        {
            if (Start < Min || Start > Max) return false;
        }
        else
        {
            const double T1 = (Min - Start) / Delta, T2 = (Max - Start) / Delta;
            Near = FMath::Max(Near, FMath::Min(T1, T2));
            Far = FMath::Min(Far, FMath::Max(T1, T2));
            if (Near > Far) return false;
        }
    }
    return true;
}

double PointSegmentDistance(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
{
    const FVector2D Delta = B - A;
    const double T = Delta.SizeSquared() > 1.e-9
        ? FMath::Clamp(FVector2D::DotProduct(Point - A, Delta) / Delta.SizeSquared(), 0.0, 1.0) : 0.0;
    return FVector2D::Distance(Point, A + Delta * T);
}

bool LeadersConflict(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, double Gap)
{
    const auto Cross = [](const FVector2D& P, const FVector2D& Q) { return P.X * Q.Y - P.Y * Q.X; };
    const double Denominator = Cross(B - A, D - C);
    if (FMath::Abs(Denominator) > 1.e-9)
    {
        const double T = Cross(C - A, D - C) / Denominator;
        const double U = Cross(C - A, B - A) / Denominator;
        if (T >= 0.0 && T <= 1.0 && U >= 0.0 && U <= 1.0) return true;
    }
    return PointSegmentDistance(A, C, D) <= Gap || PointSegmentDistance(B, C, D) <= Gap
        || PointSegmentDistance(C, A, B) <= Gap || PointSegmentDistance(D, A, B) <= Gap;
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

int32 FProofMarkerLayout::Reserve(const FBox2D& Bounds)
{
    if (bValid && IsUsableBox(Bounds))
    {
        return Occupied.Add(Bounds);
    }
    return INDEX_NONE;
}

bool FProofMarkerLayout::AcceptCandidate(const FVector2D& TopLeft, const FVector2D& Preferred, const FVector2D& Size,
    FBox2D& OutBounds, int32 SourceReservation, FVector2D* OutStart, FVector2D* OutEnd)
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
    const FBox2D Candidate(TopLeft, BottomRight);
    for (const FLeader& Leader : Leaders)
        if (SegmentHitsBox(Leader.Start, Leader.End, Candidate, Separation)) return false;
    FVector2D Start = FVector2D::ZeroVector, End = FVector2D::ZeroVector;
    if (SourceReservation != INDEX_NONE)
    {
        const FBox2D& Source = Occupied[SourceReservation];
        const FVector2D Center = Source.GetCenter();
        End = FVector2D(FMath::Clamp(Center.X, TopLeft.X, BottomRight.X),
            FMath::Clamp(Center.Y, TopLeft.Y, BottomRight.Y));
        const FVector2D Delta = End - Center;
        const FVector2D Half = Source.GetSize() * 0.5;
        if (!IsFinitePoint(Center) || !IsFinitePoint(Delta) || !IsFinitePoint(Half)
            || Half.X <= 0.0 || Half.Y <= 0.0) return false;
        const double Exit = FMath::Max(FMath::Abs(Delta.X) / Half.X, FMath::Abs(Delta.Y) / Half.Y);
        if (!FMath::IsFinite(Exit) || Exit <= 1.0) return false;
        Start = Center + Delta / Exit;
        if (!IsFinitePoint(Start) || !IsFinitePoint(End)) return false;
        for (int32 Index = 0; Index < Occupied.Num(); ++Index)
        {
            if (Index == SourceReservation) continue;
            if (SegmentHitsBox(Start, End, Occupied[Index], Separation)) return false;
        }
        for (const FLeader& Leader : Leaders)
            if (LeadersConflict(Start, End, Leader.Start, Leader.End, Separation)) return false;
    }
    OutBounds = Candidate;
    Occupied.Add(OutBounds);
    if (SourceReservation != INDEX_NONE)
    {
        Leaders.Add({Start, End});
        *OutStart = Start;
        *OutEnd = End;
    }
    return true;
}

bool FProofMarkerLayout::TryPlace(const FVector2D& PreferredTopLeft, const FVector2D& Size, FBox2D& OutBounds)
{
    return Place(PreferredTopLeft, Size, OutBounds);
}

bool FProofMarkerLayout::TryPlaceLinked(const FVector2D& PreferredTopLeft, const FVector2D& Size,
    int32 SourceReservation, FBox2D& OutBounds, FVector2D& OutStart, FVector2D& OutEnd)
{
    OutStart = OutEnd = FVector2D::ZeroVector;
    OutBounds = FBox2D(ForceInit);
    if (!Occupied.IsValidIndex(SourceReservation)) return false;
    if (!IsAnchorVisible(Occupied[SourceReservation].Min) || !IsAnchorVisible(Occupied[SourceReservation].Max)) return false;
    return Place(PreferredTopLeft, Size, OutBounds, SourceReservation, &OutStart, &OutEnd);
}

bool FProofMarkerLayout::Place(const FVector2D& PreferredTopLeft, const FVector2D& Size, FBox2D& OutBounds,
    int32 SourceReservation, FVector2D* OutStart, FVector2D* OutEnd)
{
    OutBounds = FBox2D(ForceInit);
    if (!bValid || !IsFinitePoint(PreferredTopLeft) || !IsFinitePoint(Size)
        || Size.X <= 0.0 || Size.Y <= 0.0
        || Size.X > ViewportSize.X - 2.0 * Inset || Size.Y > ViewportSize.Y - 2.0 * Inset)
    {
        return false;
    }
    if (AcceptCandidate(PreferredTopLeft, PreferredTopLeft, Size, OutBounds, SourceReservation, OutStart, OutEnd))
    {
        return true;
    }
    const FVector2D EdgeFit(
        FMath::Clamp(PreferredTopLeft.X, Inset, ViewportSize.X - Inset - Size.X),
        FMath::Clamp(PreferredTopLeft.Y, Inset, ViewportSize.Y - Inset - Size.Y));
    if (AcceptCandidate(EdgeFit, PreferredTopLeft, Size, OutBounds, SourceReservation, OutStart, OutEnd))
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
            if (AcceptCandidate(PreferredTopLeft + Direction * Radius, PreferredTopLeft, Size, OutBounds, SourceReservation, OutStart, OutEnd))
            {
                return true;
            }
        }
    }
    return false;
}
