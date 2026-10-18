#include "../unreal/Source/RedalertProof/Presentation/ProofGatherEligibility.h"
#include <cstdint>
#include <iostream>
#include <limits>

int main()
{
    using ProofGatherEligibility::CanGather;
    static_assert(noexcept(CanGather(true, true, true, true, 1, 0, true)), "noexcept contract");
    static_assert(CanGather(true, true, true, true, 1, 0, true), "constexpr contract");
    int checks = 0;
    int failures = 0;
    const auto Check = [&](bool actual, bool expected) {
        ++checks;
        if (actual != expected) { ++failures; std::cerr << "Check " << checks << " failed\n"; }
    };
    // Exhaust every prerequisite combination for empty/negative/positive ore
    // and cargo; loaded miners may return even when the patch is depleted.
    for (unsigned mask = 0; mask < 32; ++mask)
    {
        const bool playing = (mask & 1u) != 0;
        const bool owner = (mask & 2u) != 0;
        const bool miner = (mask & 4u) != 0;
        const bool resource = (mask & 8u) != 0;
        const bool refinery = (mask & 16u) != 0;
        for (std::int32_t remaining : { -1, 0, 1 })
            for (std::int32_t cargo : { -1, 0, 1 })
                Check(CanGather(playing, owner, miner, resource, remaining, cargo, refinery),
                    mask == 31u && (remaining == 1 || cargo == 1));
    }
    const auto maximum = (std::numeric_limits<std::int32_t>::max)();
    const auto minimum = (std::numeric_limits<std::int32_t>::min)();
    Check(CanGather(true, true, true, true, maximum, minimum, true), true);
    Check(CanGather(true, true, true, true, minimum, maximum, true), true);
    Check(CanGather(true, true, true, true, minimum, minimum, true), false);
    Check(CanGather(true, true, true, false, maximum, maximum, true), false);
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
