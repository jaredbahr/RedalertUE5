#pragma once

#include <cstdint>

// Relationship of known participants. Negative teams mean unknown, not hostile.
// The legacy proof has distinct-owner opponents with identical default teams.
struct FProofContextOrderPolicy
{
    static bool IsHostile(bool Skirmish, std::int32_t Owner, std::int32_t TargetOwner,
        std::int32_t OwnTeam, std::int32_t TargetTeam)
    {
        return Owner >= 0 && TargetOwner >= 0 && Owner != TargetOwner
            && OwnTeam >= 0 && TargetTeam >= 0 && (!Skirmish || OwnTeam != TargetTeam);
    }
};
