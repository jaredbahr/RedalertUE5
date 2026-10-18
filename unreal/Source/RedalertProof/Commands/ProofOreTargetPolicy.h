#pragma once
#include <cstdint>

// Existing mining-order policy only. The model supplies stable records and
// finite, nonnegative squared distances from its bounded world geometry.
namespace ProofOreTargetPolicy
{
inline constexpr std::int32_t InvalidIndex = -1;
struct ResourceView
{
    std::int32_t Id;
    std::int32_t Remaining;
};

// Metadata and distance callbacks borrow the caller's unchanged resource array.
// Initial refinery-granted miners use the same nearest/tie policy as depleted
// assignments. Callers supply the existing positive-ID resource records.
template <class ReadResource, class DistanceSquared>
std::int32_t SelectNearestIndex(std::int32_t Count,
    ReadResource Read, DistanceSquared Distance)
{
    std::int32_t Best = InvalidIndex;
    std::int32_t BestId = 0;
    double BestDistance = 0.0;
    for (std::int32_t Index = 0; Index < Count; ++Index)
    {
        const ResourceView Resource = Read(Index);
        if (Resource.Remaining <= 0) continue;
        const double CandidateDistance = Distance(Index);
        if (Best == InvalidIndex || CandidateDistance < BestDistance
            || (CandidateDistance == BestDistance && Resource.Id < BestId))
        {
            Best = Index;
            BestId = Resource.Id;
            BestDistance = CandidateDistance;
        }
    }
    return Best;
}

// Distance is evaluated only for nonempty replacement candidates after depletion.
template <class ReadResource, class DistanceSquared>
std::int32_t SelectIndex(std::int32_t AssignedId, std::int32_t Count,
    ReadResource Read, DistanceSquared Distance)
{
    std::int32_t Current = InvalidIndex;
    for (std::int32_t Index = 0; Index < Count; ++Index)
    {
        if (Read(Index).Id == AssignedId) { Current = Index; break; }
    }
    // Removing a resource retains the existing disconnect-cleanup behavior.
    // Only depletion of a still-existing assigned patch permits reselection.
    if (Current == InvalidIndex) return InvalidIndex;
    if (Read(Current).Remaining > 0) return Current;
    return SelectNearestIndex(Count, Read, Distance);
}
}
