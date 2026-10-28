#pragma once

#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

class AProofGameMode;
class AProofGameState;
class AProofPlayerController;

// Private diagnostic composition only. No production model/rules changes.
// Run/source tokens bind evidence; they are not authentication credentials.
struct FProofJobControlClientState
{
    bool bEnabled = false;
    bool bConfigured = false;
    bool bBegan = false;
    bool bHasCue = false;
    bool bObserved = false;
    bool bSend = false;
    bool bSent = false;
    bool bReceiptSeen = true;
    bool bFinished = false;
    bool bFailed = false;
    int32 Seat = 0;
    int32 Phase = -1;
    int32 Epoch = 0;
    int32 Revision = -1;
    int32 Rows = 0;
    int64 Bytes = 0;
    double Started = 0;
    double StageStarted = 0;
    FString Run;
    FString Source;
    FString Digest;
    FProofCommand Command;
    FProofCommand LastSent;
};

struct FProofJobControlDiagnostic
{
    bool bEnabled = false;
    bool bInitialized = false;
    bool bPrepared = false;
    bool bWaitingRpc = false;
    bool bFinished = false;
    bool bFailed = false;
    int32 Phase = -1;
    int32 Sequence = 0;
    int32 Factory = INDEX_NONE;
    int32 Refinery = INDEX_NONE;
    int32 Miner = INDEX_NONE;
    int32 Resource = INDEX_NONE;
    int32 StartA = 0;
    int32 StartB = 0;
    int32 Rows = 0;
    int32 Advances = 0;
    int64 Bytes = 0;
    double Started = 0;
    double StageStarted = 0;
    double FinishedAt = 0;
    FString Run;
    FString Source;
    FProofSnapshot Published;
    FProofCommand ExpectedCommand;
    TSet<int32> AcknowledgedOwners;

    void Initialize(AProofGameMode& Mode);
    void Tick(AProofGameMode& Mode);
    void SubmitRpc(AProofGameMode& Mode, AProofPlayerController& Controller, const FProofCommand& Command);
    void Ack(AProofGameMode& Mode, AProofPlayerController& Controller, const FString& AckRun,
        int32 AckPhase, int32 Epoch, int32 Revision, const FString& Digest);
    static void TickClient(AProofPlayerController& Controller);
    static void Cue(AProofPlayerController& Controller, const FString& CueRun, int32 CuePhase,
        int32 Seat, int32 Epoch, int32 Revision, const FString& Digest, bool bSend, const FProofCommand& Command);
    static void Receipt(AProofPlayerController& Controller, const FProofCommandResult& Result);
    static void FinishClient(AProofPlayerController& Controller, const FString& FinishRun);
    static bool TrySubmitRpc(AProofGameMode& Mode, AProofPlayerController& Controller, const FProofCommand& Command);
    static void AckForController(AProofGameMode& Mode, AProofPlayerController& Controller, const FString& AckRun,
        int32 AckPhase, int32 Epoch, int32 Revision, const FString& Digest);

private:
    FProofSnapshot Snapshot(const AProofGameMode& Mode) const;
    bool Prepare(AProofGameMode& Mode);
    void PublishPhase(AProofGameMode& Mode, int32 NextPhase);
    void QueueRpc(AProofGameMode& Mode, EProofCommandType Type, int32 Target = INDEX_NONE,
        const FVector& Destination = FVector::ZeroVector);
    bool ModelCommand(AProofGameMode& Mode, EProofCommandType Type, int32 Unit, int32 Target,
        const FVector& Destination, const TCHAR* Scope);
    bool Advance(AProofGameMode& Mode, float Delta, const TCHAR* Scope);
    bool Fund(AProofGameMode& Mode);
};
