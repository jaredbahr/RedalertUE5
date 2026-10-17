#pragma once

#include "CoreMinimal.h"

namespace ProofOwnerIds
{
inline constexpr int32 Bot = 100;

inline constexpr bool IsReserved(int32 OwnerId)
{
    return OwnerId == Bot;
}
}

// Server-owned, monotonic identities for one GameMode lifetime. Disconnected
// identities are not reused. This is allocation, not authentication/reconnect.
class FProofHumanOwnerAllocator
{
public:
    explicit FProofHumanOwnerAllocator(int32 FirstCandidate = 1)
        : NextCandidate(FirstCandidate) {}

    bool TryAllocate(int32& OutOwnerId)
    {
        OutOwnerId = INDEX_NONE;
        if (NextCandidate <= 0 || NextCandidate > MAX_int32) { return false; }
        if (ProofOwnerIds::IsReserved(static_cast<int32>(NextCandidate))) { ++NextCandidate; }
        if (NextCandidate > MAX_int32) { return false; }
        OutOwnerId = static_cast<int32>(NextCandidate++);
        return true;
    }

private:
    // A wider cursor can represent MAX_int32 + 1 without signed overflow.
    int64 NextCandidate;
};
