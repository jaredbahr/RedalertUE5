#pragma once

#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Read-only, opt-in host/client evidence. The fixture that supplies commands is
// separate; this class never calls Submit, Advance, or private order access.
class FProofAreaGuardObservation
{
public:
    FProofAreaGuardObservation();
    bool Enabled() const { return bEnabled; }
    void ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
        const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After);
    void ObserveAdvance(int32 Epoch, double SimulatedSeconds, float DeltaSeconds,
        const FProofSnapshot& Before, const FProofSnapshot& After);
    void LogSnapshot(const FProofSnapshot& Snapshot, const TCHAR* Role, int32 Viewer);

private:
    bool bEnabled = false;
    bool bFailed = false;
    int32 EpochSeen = 0;
    int32 Rows = 0;
    int32 LastRevision = INDEX_NONE;
    int64 Bytes = 0;
    FString Run;
    FString Source;
    void Fail(const TCHAR* Reason);
    void Emit(const TCHAR* Role, int32 Viewer, const TCHAR* Event,
        const TSharedRef<class FJsonObject>& Row);
};
