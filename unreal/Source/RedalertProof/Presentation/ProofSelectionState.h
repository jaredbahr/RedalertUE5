#pragma once

#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

// Local presentation state only. Every operation rechecks the replicated
// snapshot; neither selection nor planned orders modify authoritative state.
class REDALERTPROOF_API FProofSelectionState
{
public:
    static constexpr int32 MaxSelectedUnits = FProofCommandModel::SkirmishRecordCap;
    static constexpr int32 MaxControlGroups = 9;

    const TArray<int32>& GetSelectedUnitIds() const { return SelectedUnitIds; }
    int32 GetPrimaryUnitId() const { return PrimaryUnitId; }

    void Synchronize(const FProofSnapshot& Snapshot, int32 OwnerId);
    void SelectSingle(const FProofSnapshot& Snapshot, int32 OwnerId, int32 UnitId);
    void Toggle(const FProofSnapshot& Snapshot, int32 OwnerId, int32 UnitId);
    void AssignGroup(int32 Slot, const FProofSnapshot& Snapshot, int32 OwnerId);
    void RecallGroup(int32 Slot, const FProofSnapshot& Snapshot, int32 OwnerId);

    // Intent templates. The pawn stamps each command with its sequence and
    // match epoch immediately before sending it through the existing route.
    TArray<FProofCommand> PlanMove(const FProofSnapshot& Snapshot, int32 OwnerId, const FVector& Destination) const;
    TArray<FProofCommand> PlanAttack(const FProofSnapshot& Snapshot, int32 OwnerId, int32 TargetUnitId) const;
    TArray<FProofCommand> PlanStop(const FProofSnapshot& Snapshot, int32 OwnerId) const;

private:
    struct FGroup
    {
        TArray<int32> UnitIds;
        int32 PrimaryUnitId = INDEX_NONE;
    };

    int32 MatchEpoch = INDEX_NONE;
    int32 SelectionOwnerId = INDEX_NONE;
    int32 PrimaryUnitId = INDEX_NONE;
    TArray<int32> SelectedUnitIds;
    FGroup Groups[MaxControlGroups];

    static bool IsOwnedLiveUnit(const FProofSnapshot& Snapshot, int32 OwnerId, int32 UnitId);
    static void PruneIds(TArray<int32>& Ids, const FProofSnapshot& Snapshot, int32 OwnerId);
    bool IsCurrentContext(const FProofSnapshot& Snapshot, int32 OwnerId) const
    {
        return MatchEpoch == Snapshot.MatchEpoch && SelectionOwnerId == OwnerId;
    }
};
