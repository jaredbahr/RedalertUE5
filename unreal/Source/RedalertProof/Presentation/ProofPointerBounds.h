#pragma once

#include <cmath>

// Half-open screen rectangles match viewport pixels and constrained view bounds.
// Keep this independent of engine state so invalid pointer edges are testable.
struct FProofPointerBounds
{
    static bool Contains(double X, double Y, double Left, double Top, double Right, double Bottom)
    {
        return std::isfinite(X) && std::isfinite(Y) && std::isfinite(Left) && std::isfinite(Top)
            && std::isfinite(Right) && std::isfinite(Bottom) && Right > Left && Bottom > Top
            && X >= Left && Y >= Top && X < Right && Y < Bottom;
    }
};
