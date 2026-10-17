#pragma once

#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Explicit controls-fixture diagnostics. No commands, RPCs or model mutation.
class FProofStopEvidence
{
public:
    FProofStopEvidence();
    bool Enabled() const { return bEnabled && !bFailed; }
    bool WantsCommand(EProofCommandType Type) const;
    void ObserveCommand(int32 NewEpoch, int32 Owner, const FProofCommand& Command,
        const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After);
    // Call only after the corresponding positive Model.Advance has executed.
    void ObserveAdvance(int32 NewEpoch, float DeltaSeconds);
    void LogSnapshot(const FProofSnapshot& Snapshot, const TCHAR* Role, int32 Viewer);

private:
    bool bEnabled = false;
    bool bFailed = false;
    int32 Epoch = INDEX_NONE;
    int32 LastLogEpoch = INDEX_NONE;
    int32 LastLogRevision = INDEX_NONE;
    int32 StateRows = 0;
    int32 CommandRows = 0;
    int64 PayloadBytes = 0;
    double SimulatedSeconds = 0.0;
    TMap<int32, int32> OwnerCommandRows;

    void EnterEpoch(int32 NewEpoch);
    void Fail(const TCHAR* Reason);
    bool Emit(const TCHAR* Prefix, const FString& Json);
};
