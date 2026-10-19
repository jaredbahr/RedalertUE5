#pragma once
#include "CoreMinimal.h"

// Opt-in local three-process diagnostic state; never replicated gameplay state.
struct FProofAreaGuardRuntimeState
{
    bool bEnabled = false, bConfigured = false, bStarted = false, bTargetMoveSent = false;
    bool bRetreatSent = false, bReady = false, bSawAcquisition = false, bSawChase = false;
    bool bSawReturn = false, bMarkerWritten = false;
    int32 Stage = 0, PendingSequence = INDEX_NONE, BotSequence = 0;
    int32 TargetId = INDEX_NONE, GuardId = INDEX_NONE, PublishSteps = 0;
    float Elapsed = 0.f, Accumulator = 0.f;
    double SimulatedSeconds = 0.0;
    FVector Post = FVector::ZeroVector, TargetPost = FVector::ZeroVector;
    FString RunDirectory, RunToken;
};
