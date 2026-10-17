#pragma once
#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Bounded diagnostics for the explicit runtime fixture. Never submits commands.
class FProofAttackMoveEvidence
{
public:
    FProofAttackMoveEvidence();
    bool Enabled() const { return bEnabled; }
    void ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
        const FProofCommandResult& Result, FProofSnapshot Before, FProofSnapshot After);
    void LogClock(int32 Epoch, const FProofSnapshot& Snapshot, double SimulatedSeconds);
private:
    bool bEnabled = false;
    int32 Commands = 0, ClockRows = 0, LastRevision = INDEX_NONE, LastEpoch = INDEX_NONE;
};
