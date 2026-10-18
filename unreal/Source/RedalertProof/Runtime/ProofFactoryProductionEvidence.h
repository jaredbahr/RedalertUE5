#pragma once
#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Read-only opt-in observations of two paid builds, two rallies and the first
// three Produce submissions. Later ordinary production remains uninstrumented.
class FProofFactoryProductionEvidence
{
public:
    bool WantsCommand(int32 Owner, EProofCommandType Type) const;
    void ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
        const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After);
private:
    int32 Builds = 0;
    int32 Rallies = 0;
    int32 Produces = 0;
};