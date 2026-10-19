#pragma once
#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Read-only opt-in observations. Produce starts and Advance completions retain
// the same accepted sequence; no fixture clocks or instant-spawn overrides.
class FProofFactoryProductionEvidence
{
public:
    bool WantsCommand(int32 Owner, EProofCommandType Type) const;
    void ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
        const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After);
    void ObserveAdvance(int32 Epoch, const FProofSnapshot& Before, const FProofSnapshot& After);
private:
    int32 Builds = 0;
    int32 Rallies = 0;
    int32 Produces = 0;
    int32 PendingSequence = 0;
    int32 PendingFactory = INDEX_NONE;
    int32 PendingEpoch = 0;
    int32 AdvanceRows = 0;
};
