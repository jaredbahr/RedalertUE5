#pragma once
#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Isolated opt-in diagnostic composition. Never submits a command or mutates the model.
class FProofRefineryAutoGatherEvidence
{
public:
    FProofRefineryAutoGatherEvidence();
    bool Enabled() const { return bEnabled; }
    bool Failed() const { return bFailed; }
    void ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
        const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofCommandModel& Model);
    void ObserveAdvance(int32 Epoch, const FProofSnapshot& Before, const FProofSnapshot& After);
    void LogSnapshot(const FProofSnapshot& Snapshot, const TCHAR* Role, int32 Viewer);
    void Finish(int32 Epoch, const TCHAR* Role, int32 Viewer);
    void Fail(const TCHAR* Reason);
private:
    struct FGrant
    {
        int32 Owner = 0, Miner = 0, Refinery = 0, Resource = 0;
        bool bTravel = false, bLoaded = false, bReturn = false, bDeposited = false;
    };
    bool bEnabled = false, bFailed = false, bFinished = false;
    int32 TrustedEpoch = 0, Rows = 0, SnapshotRows = 0, LastRevision = INDEX_NONE;
    int64 Bytes = 0;
    FString Run, Source;
    TArray<FGrant> Grants;
    void EnterEpoch(int32 Epoch);
    void Row(const TCHAR* Event, const TCHAR* Role, int32 Viewer, const FString& Fields);
    static FString SnapshotJson(FProofSnapshot Snapshot, int32 Epoch);
};
