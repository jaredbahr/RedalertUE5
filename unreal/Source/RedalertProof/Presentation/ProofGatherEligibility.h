#pragma once

#include <cstdint>

// Shared presentation preflight only; authoritative Submit remains the arbiter.
namespace ProofGatherEligibility
{
inline constexpr bool CanGather(bool SkirmishPlaying, bool OwnerExists,
    bool OwnedLiveHarvester, bool ResourceExists, std::int32_t Remaining,
    std::int32_t Cargo, bool OwnLiveRefinery) noexcept
{
    return SkirmishPlaying && OwnerExists && OwnedLiveHarvester && ResourceExists
        && (Remaining > 0 || Cargo > 0) && OwnLiveRefinery;
}
}
