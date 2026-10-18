#pragma once

#include <array>

// Display-only placement for a valid finite local mesh box. ScaledAxisZ holds
// the world Z components of the actual transform's scaled local X/Y/Z axes.
// Translation is deliberately absent: the returned offset is relative to the
// authoritative ground position, never the view's previous position.
struct FProofUnitViewGrounding
{
    static double VerticalOffset(const std::array<double, 3>& LocalMin,
        const std::array<double, 3>& LocalMax,
        const std::array<double, 3>& ScaledAxisZ)
    {
        double MinimumZ = 0.0;
        for (std::size_t Axis = 0; Axis < 3; ++Axis)
        {
            const double First = LocalMin[Axis] * ScaledAxisZ[Axis];
            const double Second = LocalMax[Axis] * ScaledAxisZ[Axis];
            MinimumZ += First < Second ? First : Second;
        }
        return -MinimumZ;
    }
};
