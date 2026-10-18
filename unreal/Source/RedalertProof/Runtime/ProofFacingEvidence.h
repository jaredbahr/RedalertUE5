#pragma once
#include "Commands/ProofCommandModel.h"

// Diagnostic branch only. Records the field actually present in each snapshot;
// never reconstructs heading from client movement or modifies visible state.
void LogProofFacingSnapshot(const FProofSnapshot& Snapshot, const TCHAR* Role, int32 Viewer);
bool ProofFacingDisplacementEnabled();
void ObserveProofFacingCommand(int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, const FProofSnapshot& After);
void ObserveProofFacingAdvance(const FProofSnapshot& Before, const FProofSnapshot& After);
