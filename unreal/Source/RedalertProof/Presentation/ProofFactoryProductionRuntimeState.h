#pragma once
#include "CoreMinimal.h"

// Opt-in synthetic shared-handler fixture. No authoritative state setters.
struct FProofFactoryProductionRuntimeState
{
    bool bEnabled = false;
    int32 Stage = 0;
    int32 Case = 0;
    int32 Factories[2] = { INDEX_NONE, INDEX_NONE };
    int32 SetSequences[2] = { 0, 0 };
    int32 Sequence = 0;
    int32 ExpectedFactory = INDEX_NONE;
    int32 Unit = INDEX_NONE;
    int32 SpawnRevision = INDEX_NONE;
    int32 NativeProduce = 0;
    int32 NativeRight = 0;
    int32 SelectionAtRequest = INDEX_NONE;
    int32 SelectionCountAtRequest = 0;
    float SeenAt = -1.f;
    FVector Rallies[2];
    FVector ObservedSpawn = FVector::ZeroVector;
    TSet<int32> BeforeIds;
    TSet<int32> ProgressRevisions;
    // Ordinary defense may act on every other combat unit while observing this one.
    bool Protects(int32 Id) const { return bEnabled && Stage == 7 && Unit == Id; }
    bool Complete() const { return Stage == 9; }
};