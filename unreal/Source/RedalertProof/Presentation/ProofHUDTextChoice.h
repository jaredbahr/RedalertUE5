#pragma once

#include <cmath>
#include <cstddef>
#include <limits>

namespace ProofHUDTextChoice
{
inline constexpr std::size_t NoChoice = (std::numeric_limits<std::size_t>::max)();

inline std::size_t ChooseFirstFitting(
    const double* CandidateWidths,
    std::size_t CandidateCount,
    double AvailableWidth) noexcept
{
    if (CandidateWidths == nullptr || CandidateCount == 0 ||
        !std::isfinite(AvailableWidth) || AvailableWidth < 0.0)
    {
        return NoChoice;
    }

    for (std::size_t Index = 0; Index < CandidateCount; ++Index)
    {
        const double Width = CandidateWidths[Index];
        if (std::isfinite(Width) && Width >= 0.0 && Width <= AvailableWidth)
        {
            return Index;
        }
    }

    return NoChoice;
}
} // namespace ProofHUDTextChoice
