#pragma once

#include "CoreMinimal.h"

// Opt-in observations only. No model writes, orders, prices or mesh edits.
struct FProofVisualGeometryRuntimeState
{
    bool bEnabled = false;
    bool bCapturing = false;
    bool bRequested = false;
    bool bPriorCursor = false;
    int32 Stage = 0;
    int32 HarvesterId = INDEX_NONE;
    int32 CombatId = INDEX_NONE;
    int32 TargetId = INDEX_NONE;
    double CaptureStarted = 0.0;
    FTransform PriorCamera;
    FVector PriorPawn = FVector::ZeroVector;
    float PriorOrthoWidth = 0.f;
    TMap<int32, bool> PriorVisibility;
    FVector HarvesterContact = FVector::ZeroVector;
    FVector HarvesterMotionA = FVector::ZeroVector;
    FVector CombatMotionA = FVector::ZeroVector;
    FString CaptureName;
};
