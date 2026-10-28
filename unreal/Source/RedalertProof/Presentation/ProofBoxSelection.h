#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// Screen-space candidate planning only. The caller owns gesture/context checks,
// computes UI eligibility and revalidates the current snapshot before applying.
namespace ProofBoxSelection
{
struct Point { double X, Y; };
struct Candidate { std::int32_t UnitId; Point Center; bool Eligible; };
struct Result
{
    bool Valid = false;
    std::vector<std::int32_t> UnitIds;
};

// Candidates must point to Count readable entries (or may be null for zero).
// Limit is supplied by the existing selection policy, not an army-size budget.
inline Result Build(const Candidate* Candidates, std::size_t Count,
    Point Start, Point End, double ViewportWidth, double ViewportHeight,
    std::size_t Limit)
{
    Result Planned;
    if (!std::isfinite(ViewportWidth) || !std::isfinite(ViewportHeight)
        || ViewportWidth <= 0.0 || ViewportHeight <= 0.0 || Limit == 0
        || (Count > 0 && Candidates == nullptr)) return Planned;

    const auto InViewport = [ViewportWidth, ViewportHeight](Point P)
    {
        return std::isfinite(P.X) && std::isfinite(P.Y)
            && P.X >= 0.0 && P.Y >= 0.0
            && P.X < ViewportWidth && P.Y < ViewportHeight;
    };
    if (!InViewport(Start) || !InViewport(End)) return Planned;

    Planned.Valid = true;
    const double Left = std::min(Start.X, End.X);
    const double Right = std::max(Start.X, End.X);
    const double Top = std::min(Start.Y, End.Y);
    const double Bottom = std::max(Start.Y, End.Y);
    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        const Candidate& C = Candidates[Index];
        if (C.Eligible && C.UnitId > 0 && InViewport(C.Center)
            && C.Center.X >= Left && C.Center.X <= Right
            && C.Center.Y >= Top && C.Center.Y <= Bottom)
        {
            Planned.UnitIds.push_back(C.UnitId);
        }
    }
    // Cap after sorting and deduplication so input order cannot choose winners.
    std::sort(Planned.UnitIds.begin(), Planned.UnitIds.end());
    Planned.UnitIds.erase(std::unique(Planned.UnitIds.begin(), Planned.UnitIds.end()), Planned.UnitIds.end());
    if (Planned.UnitIds.size() > Limit) Planned.UnitIds.resize(Limit);
    return Planned;
}
}
