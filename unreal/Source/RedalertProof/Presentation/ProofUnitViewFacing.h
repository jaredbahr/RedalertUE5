#pragma once

#include <algorithm>
#include <cmath>

// Display-only world-Z alignment. The caller supplies the verified mesh's
// scaled/rotated forward axis and the authoritative last-travel direction.
// No client position history, movement epsilon, turn rate or simulation input.
struct FProofUnitViewFacing
{
    struct Vector { double X, Y, Z; };

    static bool TryYawDelta(const Vector& WorldForward, const Vector& LastTravelDirection,
        double& OutRadians)
    {
        OutRadians = 0.0;
        if (!std::isfinite(WorldForward.Z) || !std::isfinite(LastTravelDirection.Z)
            || LastTravelDirection.Z != 0.0) return false;

        double FX, FY, DX, DY;
        if (!NormalizeXY(WorldForward.X, WorldForward.Y, FX, FY)
            || !NormalizeXY(LastTravelDirection.X, LastTravelDirection.Y, DX, DY)) return false;
        // A left-multiplied world-Z quaternion applies this planar rotation
        // without replacing the component's existing tilt or signed scale.
        OutRadians = std::atan2(FX * DY - FY * DX, FX * DX + FY * DY);
        if (OutRadians == 0.0) OutRadians = 0.0; // canonical positive zero
        return std::isfinite(OutRadians);
    }

private:
    static bool NormalizeXY(double X, double Y, double& OutX, double& OutY)
    {
        if (!std::isfinite(X) || !std::isfinite(Y)) return false;
        const double Scale = std::max(std::abs(X), std::abs(Y));
        if (Scale == 0.0) return false;
        // Divide each component directly: reciprocal scaling would overflow
        // for subnormal input, and squaring the original values can overflow.
        const double SX = X / Scale, SY = Y / Scale;
        const double Length = std::sqrt(SX * SX + SY * SY);
        OutX = SX / Length;
        OutY = SY / Length;
        return std::isfinite(OutX) && std::isfinite(OutY);
    }
};
