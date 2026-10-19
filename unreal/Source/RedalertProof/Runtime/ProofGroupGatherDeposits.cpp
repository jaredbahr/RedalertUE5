#include "Runtime/ProofGameMode.h"
#include "Runtime/ProofProductionAccounting.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Opt-in, read-only observation immediately around each real Model.Advance.
// Published snapshots can skip this exact frame, so never infer its location
// by widening the authoritative refinery distance.
void AProofGameMode::ObserveGroupGatherDeposits(const FProofSnapshot& Before, const FProofSnapshot& After)
{
    const auto FindUnit = [](const FProofSnapshot& State, int32 Id)
        { return State.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; }); };
    const auto DepotFor = [](const FProofSnapshot& State, const FProofUnitRecord& Miner)
        { return State.Units.FindByPredicate([&Miner](const FProofUnitRecord& Unit)
            { return Unit.OwnerId == Miner.OwnerId && Unit.Kind == EProofUnitKind::Refinery && Unit.HitPoints > 0.f
                && FVector::DistSquared(Miner.Position, Unit.Position) <= 1.0; }); };
    const FProofPlayerRecord* Prior = Before.Players.FindByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == 1; });
    const FProofPlayerRecord* Current = After.Players.FindByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == 1; });
    if (!Prior || !Current) return;
    int32 Sum = 0;
    FString Contributors;
    for (const FProofUnitRecord& A : Before.Units)
    {
        const FProofUnitRecord* B = FindUnit(After, A.UnitId);
        const FProofUnitRecord* Depot = B ? DepotFor(After, *B) : nullptr;
        if (A.OwnerId != 1 || A.Kind != EProofUnitKind::Harvester || A.HitPoints <= 0.f || A.Cargo <= 0
            || !B || B->OwnerId != 1 || B->Kind != EProofUnitKind::Harvester || B->HitPoints <= 0.f
            || B->Cargo >= A.Cargo || !Depot) continue;
        Sum += A.Cargo - B->Cargo;
        if (!Contributors.IsEmpty()) Contributors += TEXT(",");
        Contributors += FString::Printf(TEXT("{\"miner\":%d,\"before\":%d,\"after\":%d,\"depot\":%d}"),
            A.UnitId, A.Cargo, B->Cargo, Depot->UnitId);
    }
    if (Sum == 0) return;
    const auto Accounted = [](const FProofSnapshot& State)
    {
        int64 Value = 0;
        for (const FProofResourceRecord& Resource : State.Resources) Value += Resource.Remaining;
        for (const FProofUnitRecord& Unit : State.Units) Value += Unit.Cargo;
        for (const FProofPlayerRecord& Player : State.Players) Value += Player.TotalGathered;
        return Value;
    };
    int32 Payment = 0, Refund = 0;
    if (!ProofProductionAccounting::ValidateAdvance(Before, After, 1, Payment, Refund)
        || static_cast<int64>(Current->Credits) - Prior->Credits != Sum - Payment + Refund || Current->TotalGathered - Prior->TotalGathered != Sum
        || Accounted(Before) != Accounted(After))
    { UE_LOG(LogTemp, Error, TEXT("PROOF_GROUP_GATHER_ERROR deposit_attribution")); return; }
    for (const FProofUnitRecord& A : Before.Units)
    {
        const FProofUnitRecord* B = FindUnit(After, A.UnitId);
        const FProofUnitRecord* Depot = B ? DepotFor(After, *B) : nullptr;
        if (GroupGatherDepositMiners.Contains(A.UnitId) || A.OwnerId != 1 || A.Kind != EProofUnitKind::Harvester
            || A.HitPoints <= 0.f || A.Cargo <= 0 || !B || B->OwnerId != 1 || B->Kind != EProofUnitKind::Harvester
            || B->HitPoints <= 0.f || B->Cargo != 0 || !Depot) continue;
        if (GroupGatherDepositMiners.Num() >= FProofCommandModel::SkirmishRecordCap)
        { UE_LOG(LogTemp, Error, TEXT("PROOF_GROUP_GATHER_ERROR deposit_row_limit")); return; }
        GroupGatherDepositMiners.Add(A.UnitId);
        UE_LOG(LogTemp, Display, TEXT("PROOF_GROUP_GATHER_DEPOSIT {\"owner\":1,\"epoch\":%d,\"beforeRevision\":%d,\"revision\":%d,\"miner\":%d,\"depot\":%d,\"before\":%d,\"after\":%d,\"sum\":%d,\"creditsBefore\":%d,\"creditsAfter\":%d,\"gatheredBefore\":%d,\"gatheredAfter\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"depotX\":%.17g,\"depotY\":%.17g,\"depotZ\":%.17g,\"conservedBefore\":%lld,\"conservedAfter\":%lld,\"contributors\":[%s],\"productionPayment\":%d,\"productionRefund\":%d,\"productionBefore\":%s,\"productionAfter\":%s}"),
            MatchEpoch, Before.Revision, After.Revision, A.UnitId, Depot->UnitId, A.Cargo, B->Cargo, Sum,
            Prior->Credits, Current->Credits, Prior->TotalGathered, Current->TotalGathered,
            B->Position.X, B->Position.Y, B->Position.Z, Depot->Position.X, Depot->Position.Y, Depot->Position.Z,
            static_cast<long long>(Accounted(Before)), static_cast<long long>(Accounted(After)), *Contributors, Payment, Refund, *ProofProductionAccounting::ContextJson(Before), *ProofProductionAccounting::ContextJson(After));
    }
}
