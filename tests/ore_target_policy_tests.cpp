#include "../unreal/Source/RedalertProof/Commands/ProofOreTargetPolicy.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <tuple>
#include <vector>

namespace
{
struct Patch
{
    std::int32_t Id, Remaining;
    double Distance;
    bool operator==(const Patch& Other) const
    { return Id == Other.Id && Remaining == Other.Remaining && Distance == Other.Distance; }
};

std::int32_t Select(const std::vector<Patch>& Patches, std::int32_t Assigned, int& DistanceCalls)
{
    return ProofOreTargetPolicy::SelectIndex(Assigned, static_cast<std::int32_t>(Patches.size()),
        [&Patches](std::int32_t Index) {
            const Patch& Value = Patches[static_cast<std::size_t>(Index)];
            return ProofOreTargetPolicy::ResourceView{Value.Id, Value.Remaining};
        },
        [&Patches, &DistanceCalls](std::int32_t Index) {
            ++DistanceCalls;
            return Patches[static_cast<std::size_t>(Index)].Distance;
        });
}

// Independent oracle: find the first assignment, then stable-sort eligible
// indices by (distance, ID). Stable sorting retains equal-ID/equal-distance order.
std::int32_t Reference(const std::vector<Patch>& Patches, std::int32_t Assigned)
{
    const auto Current = std::find_if(Patches.begin(), Patches.end(),
        [Assigned](const Patch& Value) { return Value.Id == Assigned; });
    if (Current == Patches.end()) return ProofOreTargetPolicy::InvalidIndex;
    if (Current->Remaining > 0) return static_cast<std::int32_t>(Current - Patches.begin());
    std::vector<std::size_t> Eligible;
    for (std::size_t Index = 0; Index < Patches.size(); ++Index)
        if (Patches[Index].Remaining > 0) Eligible.push_back(Index);
    std::stable_sort(Eligible.begin(), Eligible.end(), [&Patches](std::size_t A, std::size_t B) {
        return std::tie(Patches[A].Distance, Patches[A].Id) < std::tie(Patches[B].Distance, Patches[B].Id);
    });
    return Eligible.empty() ? ProofOreTargetPolicy::InvalidIndex : static_cast<std::int32_t>(Eligible.front());
}
}

int main()
{
    static_assert(ProofOreTargetPolicy::InvalidIndex < 0, "signed absent index");
    int Checks = 0, Failures = 0;
    const auto Check = [&](bool Passed) {
        ++Checks;
        if (!Passed) { ++Failures; std::cerr << "Failed check " << Checks << '\n'; }
    };
    const auto Case = [&](const std::vector<Patch>& Patches, std::int32_t Assigned,
                          std::int32_t Expected, int ExpectedDistanceCalls) {
        const auto Before = Patches;
        int Calls = 0;
        Check(Select(Patches, Assigned, Calls) == Expected);
        Check(Calls == ExpectedDistanceCalls);
        Check(Patches == Before);
    };
    Case({}, 1, -1, 0);
    Case({{1, 0, 0}, {2, 50, 0}}, 999, -1, 0); // Missing is not depleted.
    Case({{1, 1, 100}, {2, 50, 0}}, 1, 0, 0); // Keep assigned ore, even if farther.
    Case({{1, 0, 0}, {2, 6, 25}, {3, 1, 4}, {4, -5, 0}}, 1, 2, 2);
    Case({{1, 0, 0}, {8, 4, 9}, {3, 9, 9}, {6, 4, 9}}, 1, 2, 3);
    Case({{1, 0, 0}, {2, -1, 0}}, 1, -1, 0);
    Case({{1, (std::numeric_limits<std::int32_t>::max)(), 100}, {2, 1, 0}}, 1, 0, 0);
    Case({{1, (std::numeric_limits<std::int32_t>::min)(), 0}, {2, 1, 5}}, 1, 1, 1);
    Case({{1, 0, 0}, {1, 5, 4}, {3, 5, 1}}, 1, 2, 2); // First duplicate decides depletion.
    Case({{1, 5, 100}, {1, 0, 0}, {3, 5, 0}}, 1, 0, 0); // First duplicate stays assigned.
    Case({{5, 0, 0}, {2, 5, 1}, {2, 5, 1}}, 5, 1, 2); // Exact duplicate tie keeps first.
    const double Largest = (std::numeric_limits<double>::max)();
    Case({{1, 0, 0}, {2, 1, Largest}, {3, 1, Largest / 2}}, 1, 2, 2);
    Case({{1, 0, 0}, {2, 1, std::nextafter(1.0, 2.0)}, {3, 1, 1}}, 1, 2, 2);
    Case({{1, 0, 0}, {2, 1, std::nextafter(0.0, 1.0)}, {3, 1, 0}}, 1, 2, 2);

    for (std::int32_t Count : {0, -1})
    {
        int Reads = 0, Distances = 0;
        const auto Result = ProofOreTargetPolicy::SelectIndex(1, Count,
            [&Reads](std::int32_t) { ++Reads; return ProofOreTargetPolicy::ResourceView{1, 1}; },
            [&Distances](std::int32_t) { ++Distances; return 0.0; });
        Check(Result == ProofOreTargetPolicy::InvalidIndex);
        Check(Reads == 0);
        Check(Distances == 0);
    }

    // 1,024 generated fixtures, including sticky, missing, depleted and ties.
    for (std::int32_t Count = 1; Count <= 32; ++Count)
    {
        for (std::int32_t Seed = 0; Seed < 32; ++Seed)
        {
            std::vector<Patch> Patches;
            for (std::int32_t Index = 0; Index < Count; ++Index)
            {
                const std::int32_t Pattern = (Index + Seed) % 4;
                const std::int32_t Remaining = Pattern == 0 ? 0 : Pattern == 1 ? -1 : (Index + Seed) % 17 + 1;
                Patches.push_back({(Index + 1) * 3, Remaining, static_cast<double>((Index * 13 + Seed * 7) % 19)});
            }
            const std::int32_t Assigned = Seed % 8 == 0 ? 9999 : Patches[static_cast<std::size_t>(Seed % Count)].Id;
            const auto Before = Patches;
            int Calls = 0;
            Check(Select(Patches, Assigned, Calls) == Reference(Patches, Assigned));
            Check(Patches == Before);
            const auto Current = std::find_if(Patches.begin(), Patches.end(),
                [Assigned](const Patch& Value) { return Value.Id == Assigned; });
            const auto EligibleCount = std::count_if(Patches.begin(), Patches.end(),
                [](const Patch& Value) { return Value.Remaining > 0; });
            const int ExpectedCalls = Current != Patches.end() && Current->Remaining <= 0
                ? static_cast<int>(EligibleCount) : 0;
            Check(Calls == ExpectedCalls);
        }
    }

    // Permutation invariance is asserted only for unique resource IDs.
    const std::vector<Patch> Unique{{1, 0, 0}, {2, 1, 2}, {3, 1, 0}, {4, 1, 1},
                                  {5, 1, 2}, {6, 1, 0}, {7, 1, 1}, {8, 1, 2}, {9, 1, 0}};
    for (int Direction = 0; Direction < 2; ++Direction)
    {
        for (std::size_t Shift = 0; Shift < Unique.size(); ++Shift)
        {
            auto Patches = Unique;
            if (Direction != 0) std::reverse(Patches.begin(), Patches.end());
            std::rotate(Patches.begin(), Patches.begin() + static_cast<std::ptrdiff_t>(Shift), Patches.end());
            int Calls = 0;
            const auto Index = Select(Patches, 1, Calls);
            const bool Valid = Index >= 0 && static_cast<std::size_t>(Index) < Patches.size();
            Check(Valid);
            Check(Valid && Patches[static_cast<std::size_t>(Index)].Id == 3);
            Check(Calls == 8);
        }
    }
    if (Checks != 3174) { ++Failures; std::cerr << "Unexpected check count: " << Checks << '\n'; }
    std::cout << Checks << " checks, " << Failures << " failures\n";
    return Failures == 0 ? 0 : 1;
}
