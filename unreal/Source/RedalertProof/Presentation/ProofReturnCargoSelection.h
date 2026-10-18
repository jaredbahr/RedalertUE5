#pragma once

#include "Commands/ProofCommandModel.h"
#include "Presentation/ProofSelectionState.h"

// Snapshot-only refinery context action. Empty miners are skipped in an
// otherwise all-Harvester selection; the host still validates every command.
struct REDALERTPROOF_API FProofReturnCargoSelection
{
    static bool IsOwnedLiveRefinery(const FProofSnapshot& Snapshot, int32 OwnerId, int32 RefineryId);
    static bool CanReturn(const FProofSnapshot& Snapshot, int32 OwnerId, int32 UnitId, int32 RefineryId);
    static TArray<FProofCommand> Plan(const FProofSnapshot& Snapshot, int32 OwnerId,
        const FProofSelectionState& Selection, int32 RefineryId);
};
