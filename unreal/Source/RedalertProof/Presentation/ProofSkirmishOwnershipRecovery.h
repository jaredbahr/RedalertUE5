#pragma once

#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

enum class EProofSkirmishOwnershipRecovery : uint8
{
    NotHandled,
    Wait,
    Recovered
};

// Fixture-only recovery policy, shared with deterministic model tests. The
// caller retains receipt lookup, epoch synchronization and terminal handling.
class REDALERTPROOF_API FProofSkirmishOwnershipRecovery
{
public:
    // Only an exact owned-dead record can clear its issued target and receipt
    // latch. This never changes the snapshot, allocator, other intents or clocks.
    static EProofSkirmishOwnershipRecovery Apply(const FProofSnapshot& Snapshot, int32 OwnerId,
        const FString& RequestedType, const FProofCommandResult& Receipt, int32 RequestedUnitId,
        bool bSourceWasOwnedLive, TMap<int32, int32>& IssuedTargets, int32& PendingSequence);
};
