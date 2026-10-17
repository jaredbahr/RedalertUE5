#pragma once

#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Snapshot-only planner. The caller submits its commands through the same
// authoritative model path used by players, in the order returned here.
class REDALERTPROOF_API FProofSkirmishBot
{
public:
    TArray<FProofCommand> Decide(const FProofSnapshot& Snapshot, int32 OwnerId, double NowSeconds);
    void Reset();

private:
    struct FIssuedOrder
    {
        EProofCommandType Type = EProofCommandType::Move;
        int32 TargetId = INDEX_NONE;
        double IssuedAt = 0.0;
    };

    TMap<int32, FIssuedOrder> IssuedOrders;
    int32 NextSequence = 1;
    int32 LastAttackUnitId = INDEX_NONE;
    int32 LastGatherUnitId = INDEX_NONE;
    double FirstDecisionAt = 0.0;
    double NextDecisionAt = 0.0;
    double LastObservedAt = 0.0;
    bool bStarted = false;
};
