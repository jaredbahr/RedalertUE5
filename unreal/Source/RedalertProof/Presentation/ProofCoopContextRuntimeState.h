#pragma once
#include "CoreMinimal.h"

// Opt-in synthetic-cursor acceptance state; never changes authoritative state.
struct FProofCoopContextRuntimeState
{
    bool bEnabled = false;
    int32 Stage = 0;
    int32 BeforeSequence = 0;
    int32 Combat = INDEX_NONE;
    int32 Harvester = INDEX_NONE;
    int32 Ally = INDEX_NONE;
    int32 Enemy = INDEX_NONE;
    int32 Resource = INDEX_NONE;
    int32 ActionRevision = INDEX_NONE;
    float ActionAt = -1.f;
    FVector Destination = FVector::ZeroVector;
    TMap<int32, FVector> Starts;
    TMap<int32, FVector> Destinations;
    TSet<int32> ObservedRevisions;
};
