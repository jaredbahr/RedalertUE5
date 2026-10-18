#pragma once
#include "Commands/ProofCommandModel.h"

// Diagnostic branch only. Records the field actually present in each snapshot;
// never reconstructs heading from client movement or modifies visible state.
void LogProofFacingSnapshot(const FProofSnapshot& Snapshot, const TCHAR* Role, int32 Viewer);
bool ProofFacingDisplacementEnabled();
// Epoch is owned by GameMode (Publish stamps it); raw model snapshots use zero.
void ObserveProofFacingCommand(int32 TrustedMatchEpoch, int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, const FProofSnapshot& After);
void ObserveProofFacingAdvance(int32 TrustedMatchEpoch, const FProofSnapshot& Before, const FProofSnapshot& After);
