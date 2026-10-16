#pragma once

#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Client-side intent queue. It never assigns a command sequence or epoch and
// never changes the snapshot; the pawn sends each released intent via Request.
class REDALERTPROOF_API FProofGroupOrderQueue
{
public:
    static constexpr int32 MaxPendingCommands = 64;
    static constexpr int32 MaxCommandsPerInterval = 2;
    static constexpr double DispatchIntervalSeconds = 0.1;

    bool Enqueue(const FProofSnapshot& Snapshot, int32 OwnerId, const TArray<FProofCommand>& Commands);
    TArray<FProofCommand> PopReady(const FProofSnapshot& Snapshot, int32 OwnerId, double WallSeconds);
    int32 NumPending() const { return Pending.Num(); }
    void CancelUnit(int32 UnitId);
    void Clear();

private:
    TArray<FProofCommand> Pending;
    int32 MatchEpoch = INDEX_NONE;
    int32 OwnerId = INDEX_NONE;
    double LastObservedWallSeconds = 0.0;
    double LastDispatchWallSeconds = 0.0;
    bool bHasObservedWallTime = false;
    bool bHasDispatched = false;

    void Synchronize(const FProofSnapshot& Snapshot, int32 CurrentOwnerId);
    void Prune(const FProofSnapshot& Snapshot, int32 CurrentOwnerId);
    static bool IsValidIntent(const FProofSnapshot& Snapshot, int32 CurrentOwnerId, const FProofCommand& Command);
};
