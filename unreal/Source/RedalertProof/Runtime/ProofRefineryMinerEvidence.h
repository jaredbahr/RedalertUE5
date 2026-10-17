#pragma once
#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Opt-in diagnostics only; never submits commands or mutates a snapshot.
class FProofRefineryMinerEvidence
{
public:
    FProofRefineryMinerEvidence();
    bool Enabled() const { return bEnabled; }
    void ObserveCommand(int32 NewEpoch, int32 Owner, const FProofCommand& Command,
        const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After);
    void ObserveAdvance(int32 NewEpoch, const FProofSnapshot& Before, const FProofSnapshot& After);
    void LogSnapshot(const FProofSnapshot& Snapshot, const TCHAR* Role, int32 Viewer);
private:
    struct FGrant
    {
        int32 Owner = INDEX_NONE, Refinery = INDEX_NONE, Miner = INDEX_NONE, Resource = INDEX_NONE;
        int64 Sequence = 0;
        bool bGather = false, bLoaded = false, bDeposited = false;
    };
    bool bEnabled = false, bFailed = false;
    int32 Epoch = INDEX_NONE, Rejections = 0, SnapshotRows = 0;
    int32 LastLogEpoch = INDEX_NONE, LastLogRevision = INDEX_NONE;
    int64 SnapshotBytes = 0;
    TArray<FGrant> Grants;
    void EnterEpoch(int32 NewEpoch);
    void Fail(const TCHAR* Reason);
};
