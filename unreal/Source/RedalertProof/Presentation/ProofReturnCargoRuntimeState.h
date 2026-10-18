#pragma once

#include "CoreMinimal.h"

// Opt-in presentation evidence only. Never read by command validation/simulation.
struct FProofReturnCargoRuntimeState
{
    bool bEnabled = false;
    int32 Stage = 0;
    int32 PublishedRevisionSeen = INDEX_NONE;
    bool bPublishedSnapshotPreviouslySeen = false;
    int32 Epoch = INDEX_NONE;
    int32 Miners[2] = { INDEX_NONE, INDEX_NONE };
    int32 Refinery = INDEX_NONE;
    int32 Resource = INDEX_NONE;
    int32 BeforeSequence = 0;
    int32 NextBefore = 0;
    int32 ReceiptRevision = INDEX_NONE;
    bool bReceiptsConfirmed = false;
    int32 NativeLeft = 0;
    int32 NativeRight = 0;
    int32 NativeStop = 0;
    int32 PriorGathered = 0;
    int32 PriorCargo[2] = { 0, 0 };
    bool Deposited[2] = { false, false };
    int32 HeldCargo = 0;
    int32 HeldRevision = INDEX_NONE;
    FVector HeldPosition = FVector::ZeroVector;
    FVector WaitingPosition = FVector::ZeroVector;
    bool bCaptureRequested = false;
    FDateTime CaptureRequestedAt;
};
