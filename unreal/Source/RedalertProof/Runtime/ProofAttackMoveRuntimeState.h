#pragma once
#include "CoreMinimal.h"

// State for the opt-in validation harness only. Not replicated gameplay state.
struct FProofAttackMoveRuntimeState
{
    bool bEnabled = false, bReady = false, bViewportReady = false, bStarted = false, bBotStopped = false;
    int32 Stage = 0, Sequence = 0, Starter = INDEX_NONE, Miner = INDEX_NONE, Unit = INDEX_NONE, Target = INDEX_NONE;
    int32 ReceiptRevision = INDEX_NONE, NativeRight = 0, NativeStop = 0, PublishSteps = 0;
    float Elapsed = 0.f, PhaseStarted = -1.f, Accumulator = 0.f, PauseHP = 0.f;
    double SimulatedSeconds = 0.0, StopAt = -1.0;
    FVector Origin = FVector::ZeroVector, PausePosition = FVector::ZeroVector, StopPosition = FVector::ZeroVector;
    TSet<int32> Revisions;
    FString Output, Release;
    TMap<FString, FDateTime> CaptureRequests;
};
