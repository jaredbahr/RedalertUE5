#pragma once
#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Opt-in authoritative command observations. Published states use the existing
// refinery semantic logger; these immediate Submit revisions may be unpublished.
class FProofFactoryRallyEvidence
{
public:
    bool WantsCommand(int32 Owner, EProofCommandType Type) const;
    void ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
        const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After);
    void ObserveAdvance(int32 Epoch, const FProofSnapshot& Before, const FProofSnapshot& After);
private:
    int32 Factory = INDEX_NONE;
    bool bProduced = false;
    int32 Rows = 0;
    int32 PendingSequence = 0;
    int32 PendingEpoch = 0;
    int32 AdvanceRows = 0;
};
