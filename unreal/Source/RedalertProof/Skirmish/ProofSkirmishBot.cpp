#include "Skirmish/ProofSkirmishBot.h"

namespace
{
constexpr int32 MaxCommandsPerDecision = 4;
constexpr int32 MaxLiveUnitsPerOwner = FProofCommandModel::SkirmishRecordCap;
constexpr int32 RefineryCost = FProofCommandModel::RefineryCost;
constexpr int32 FactoryCost = FProofCommandModel::FactoryCost;
constexpr int32 CombatCost = FProofCommandModel::ProductionCost;
constexpr double DecisionIntervalSeconds = 1.0;
constexpr double OrderRetrySeconds = 5.0;
constexpr double AttackGraceSeconds = 20.0;
constexpr double StructureSeparationSquared = 250.0 * 250.0;
// Local planning reservation, matching the shared spawn helper's clearance.
constexpr double PlannedMinerSeparationSquared = 260.0 * 260.0;
constexpr double WorldExtent = FProofCommandModel::WorldExtent;

bool IsStructure(EProofUnitKind Kind)
{
    return Kind == EProofUnitKind::HQ || Kind == EProofUnitKind::Refinery || Kind == EProofUnitKind::Factory;
}

bool IsValidBuildPosition(const FProofSnapshot& Snapshot, const FVector& Position, const FVector* PlannedMiner)
{
    if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Position.Z)
        || FMath::Abs(Position.X) > WorldExtent || FMath::Abs(Position.Y) > WorldExtent
        || FMath::Abs(Position.Z) > WorldExtent)
    {
        return false;
    }
    if (PlannedMiner && FVector::DistSquared(Position, *PlannedMiner) < PlannedMinerSeparationSquared)
    {
        return false;
    }
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.HitPoints > 0.f && IsStructure(Unit.Kind)
            && FVector::DistSquared(Position, Unit.Position) < StructureSeparationSquared)
        {
            return false;
        }
    }
    return true;
}

bool FindBuildPosition(const FProofSnapshot& Snapshot, const FVector& HQPosition, FVector& OutPosition,
    FVector* OutMinerPosition = nullptr, const FVector* PlannedMiner = nullptr)
{
    // These points are all within 800 cm of HQ, with the nearer cardinals
    // preferred. The fixed order makes placement independent of array order.
    const FVector Offsets[] = {
        FVector(350, 0, 0), FVector(-350, 0, 0), FVector(0, 350, 0), FVector(0, -350, 0),
        FVector(350, 350, 0), FVector(-350, 350, 0), FVector(350, -350, 0), FVector(-350, -350, 0),
        FVector(600, 0, 0), FVector(-600, 0, 0), FVector(0, 600, 0), FVector(0, -600, 0)
    };
    for (const FVector& Offset : Offsets)
    {
        const FVector Candidate = HQPosition + Offset;
        if (IsValidBuildPosition(Snapshot, Candidate, PlannedMiner))
        {
            if (OutMinerPosition && !FProofCommandModel::FindSkirmishSpawn(Snapshot, Candidate, *OutMinerPosition))
            {
                continue;
            }
            OutPosition = Candidate;
            return true;
        }
    }
    return false;
}
}

TArray<FProofCommand> FProofSkirmishBot::Decide(const FProofSnapshot& Snapshot, int32 OwnerId, double NowSeconds)
{
    TArray<FProofCommand> Commands;
    if (!FMath::IsFinite(NowSeconds) || NowSeconds < 0.0 || OwnerId < 0
        || !Snapshot.bSkirmish || Snapshot.MatchPhase != EProofMatchPhase::Playing)
    {
        return Commands;
    }
    if (bStarted && (NowSeconds <= LastObservedAt || NowSeconds < NextDecisionAt))
    {
        return Commands;
    }

    const FProofPlayerRecord* Player = Snapshot.Players.FindByPredicate(
        [OwnerId](const FProofPlayerRecord& Candidate) { return Candidate.OwnerId == OwnerId; });
    if (!Player || NextSequence <= 0)
    {
        return Commands;
    }
    if (!bStarted)
    {
        bStarted = true;
        FirstDecisionAt = NowSeconds;
    }
    LastObservedAt = NowSeconds;
    NextDecisionAt = NowSeconds + DecisionIntervalSeconds;

    TArray<const FProofUnitRecord*> OwnUnits;
    const FProofUnitRecord* HQ = nullptr;
    const FProofUnitRecord* Refinery = nullptr;
    const FProofUnitRecord* Factory = nullptr;
    bool bHasHarvester = false;
    bool bHasLoadedHarvester = false;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.OwnerId != OwnerId || Unit.HitPoints <= 0.f) { continue; }
        OwnUnits.Add(&Unit);
        if (Unit.Kind == EProofUnitKind::HQ && (!HQ || Unit.UnitId < HQ->UnitId)) { HQ = &Unit; }
        if (Unit.Kind == EProofUnitKind::Refinery && (!Refinery || Unit.UnitId < Refinery->UnitId)) { Refinery = &Unit; }
        if (Unit.Kind == EProofUnitKind::Factory && (!Factory || Unit.UnitId < Factory->UnitId)) { Factory = &Unit; }
        bHasHarvester |= Unit.Kind == EProofUnitKind::Harvester;
        bHasLoadedHarvester |= Unit.Kind == EProofUnitKind::Harvester && Unit.Cargo > 0;
    }
    OwnUnits.Sort([](const FProofUnitRecord& A, const FProofUnitRecord& B) { return A.UnitId < B.UnitId; });
    for (auto It = IssuedOrders.CreateIterator(); It; ++It)
    {
        if (!OwnUnits.ContainsByPredicate([&It](const FProofUnitRecord* Unit) { return Unit->UnitId == It.Key(); }))
        {
            It.RemoveCurrent();
        }
    }

    auto AddCommand = [this, &Commands](EProofCommandType Type, int32 UnitId, int32 TargetId, const FVector& Destination)
    {
        if (Commands.Num() >= MaxCommandsPerDecision || NextSequence <= 0) { return false; }
        FProofCommand Command;
        Command.Sequence = NextSequence;
        Command.Type = Type;
        Command.UnitId = UnitId;
        Command.TargetUnitId = TargetId;
        Command.Destination = Destination;
        Commands.Add(Command);
        NextSequence = NextSequence == MAX_int32 ? 0 : NextSequence + 1;
        return true;
    };

    // Reserve a site in this decision's local view so that two builds cannot
    // choose the same empty position before the caller submits either command.
    FProofSnapshot PlacementView = Snapshot;
    int32 SpendableCredits = Player->Credits;
    int32 PlannedLiveCount = OwnUnits.Num();
    FVector PlannedMinerPosition;
    bool bPlannedMiner = false;
    // A surviving refinery cannot restore lost miners by itself. Reuse the
    // authoritative refinery-plus-miner grant only when every own miner is
    // gone and ore remains. A surviving miner suppresses extra construction;
    // the usual funds, two-record capacity and spawn checks still apply.
    const bool bHasUsableOre = Snapshot.Resources.ContainsByPredicate(
        [](const FProofResourceRecord& Resource) { return Resource.ResourceId > 0 && Resource.Remaining > 0; });
    const bool bNeedsReplacementMiner = !bHasHarvester && bHasUsableOre;
    // Rebuilding after depletion cannot restore income unless a surviving own
    // miner still has cargo to unload. Leave those credits for other commands.
    const bool bNeedsRefinery = !Refinery && (bHasUsableOre || bHasLoadedHarvester);
    if (HQ && (bNeedsRefinery || bNeedsReplacementMiner) && SpendableCredits >= RefineryCost
        && PlannedLiveCount <= MaxLiveUnitsPerOwner - 2)
    {
        FVector Site, MinerPosition;
        if (FindBuildPosition(PlacementView, HQ->Position, Site, &MinerPosition)
            && AddCommand(EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, Site))
        {
            SpendableCredits -= RefineryCost;
            PlannedLiveCount += 2;
            FProofUnitRecord Reserved;
            Reserved.Kind = EProofUnitKind::Refinery;
            Reserved.HitPoints = 1.f;
            Reserved.Position = Site;
            PlacementView.Units.Add(Reserved);
            Reserved.Kind = EProofUnitKind::Harvester;
            Reserved.Position = MinerPosition;
            PlacementView.Units.Add(Reserved);
            PlannedMinerPosition = MinerPosition;
            bPlannedMiner = true;
        }
    }
    if (HQ && !Factory && SpendableCredits >= FactoryCost && PlannedLiveCount < MaxLiveUnitsPerOwner)
    {
        FVector Site;
        // Only this decision's grant gets an extra footprint reservation. The
        // authoritative factory/live-mobile placement policy remains unchanged.
        if (FindBuildPosition(PlacementView, HQ->Position, Site, nullptr, bPlannedMiner ? &PlannedMinerPosition : nullptr)
            && AddCommand(EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, Site))
        {
            SpendableCredits -= FactoryCost;
            ++PlannedLiveCount;
        }
    }

    if (Refinery)
    {
        // One miner intent per decision leaves production/attacks room in the
        // existing four-command budget. Rotate by stable ID so granted miners
        // join the same command path without starving behind the starter miner.
        int32 FirstMinerIndex = 0;
        for (int32 Index = 0; Index < OwnUnits.Num(); ++Index)
        {
            if (OwnUnits[Index]->UnitId > LastGatherUnitId)
            {
                FirstMinerIndex = Index;
                break;
            }
        }
        for (int32 Offset = 0; Offset < OwnUnits.Num(); ++Offset)
        {
            const FProofUnitRecord* Harvester = OwnUnits[(FirstMinerIndex + Offset) % OwnUnits.Num()];
            if (Harvester->Kind != EProofUnitKind::Harvester) { continue; }
            const FProofResourceRecord* Resource = nullptr;
            double BestDistance = 0.0;
            for (const FProofResourceRecord& Candidate : Snapshot.Resources)
            {
                if (Candidate.ResourceId <= 0 || Candidate.Remaining <= 0) { continue; }
                const double Distance = FVector::DistSquared(Harvester->Position, Candidate.Position);
                if (!Resource || Distance < BestDistance || (Distance == BestDistance && Candidate.ResourceId < Resource->ResourceId))
                {
                    Resource = &Candidate;
                    BestDistance = Distance;
                }
            }
            if (Resource)
            {
                const FIssuedOrder* Last = IssuedOrders.Find(Harvester->UnitId);
                if (!Last || Last->Type != EProofCommandType::Gather || Last->TargetId != Resource->ResourceId
                    || NowSeconds - Last->IssuedAt >= OrderRetrySeconds)
                {
                    if (AddCommand(EProofCommandType::Gather, Harvester->UnitId, Resource->ResourceId, FVector::ZeroVector))
                    {
                        FIssuedOrder& Issued = IssuedOrders.FindOrAdd(Harvester->UnitId);
                        Issued.Type = EProofCommandType::Gather;
                        Issued.TargetId = Resource->ResourceId;
                        Issued.IssuedAt = NowSeconds;
                        LastGatherUnitId = Harvester->UnitId;
                        break;
                    }
                }
            }
        }
    }

    if (Factory && SpendableCredits >= CombatCost && PlannedLiveCount < MaxLiveUnitsPerOwner)
    {
        if (AddCommand(EProofCommandType::Produce, Factory->UnitId, INDEX_NONE, FVector::ZeroVector))
        {
            SpendableCredits -= CombatCost;
            ++PlannedLiveCount;
        }
    }

    if (NowSeconds - FirstDecisionAt < AttackGraceSeconds)
    {
        // Defend each combat unit's current post during the existing grace.
        // Economy keeps priority, and the same four-command budget applies.
        // Never retry Guard while it is pursuing/returning: that would silently
        // reanchor its post. The offensive scheduler replaces it after grace.
        for (const FProofUnitRecord* Defender : OwnUnits)
        {
            if (Commands.Num() >= MaxCommandsPerDecision) { break; }
            if (Defender->Kind != EProofUnitKind::Combat) { continue; }
            const FIssuedOrder* Last = IssuedOrders.Find(Defender->UnitId);
            if (Last && Last->Type == EProofCommandType::AreaGuard) { continue; }
            if (AddCommand(EProofCommandType::AreaGuard, Defender->UnitId, INDEX_NONE, FVector::ZeroVector))
            {
                FIssuedOrder& Issued = IssuedOrders.FindOrAdd(Defender->UnitId);
                Issued.Type = EProofCommandType::AreaGuard;
                Issued.TargetId = INDEX_NONE;
                Issued.IssuedAt = NowSeconds;
            }
        }
    }
    else
    {
        // Resume after the last issued attack so changing nearest targets
        // cannot consume every decision's budget on the lowest IDs forever.
        int32 FirstAttackerIndex = 0;
        for (int32 Index = 0; Index < OwnUnits.Num(); ++Index)
        {
            if (OwnUnits[Index]->UnitId > LastAttackUnitId)
            {
                FirstAttackerIndex = Index;
                break;
            }
        }
        for (int32 Offset = 0; Offset < OwnUnits.Num(); ++Offset)
        {
            if (Commands.Num() >= MaxCommandsPerDecision) { break; }
            const FProofUnitRecord* Attacker = OwnUnits[(FirstAttackerIndex + Offset) % OwnUnits.Num()];
            if (Attacker->Kind != EProofUnitKind::Combat) { continue; }
            const FProofUnitRecord* Target = nullptr;
            double BestDistance = 0.0;
            for (const FProofUnitRecord& Candidate : Snapshot.Units)
            {
                if (Candidate.HitPoints <= 0.f || Candidate.OwnerId == OwnerId) { continue; }
                const FProofPlayerRecord* Enemy = Snapshot.Players.FindByPredicate(
                    [&Candidate](const FProofPlayerRecord& P) { return P.OwnerId == Candidate.OwnerId; });
                if (!Enemy || Enemy->TeamId == Player->TeamId) { continue; }
                const double Distance = FVector::DistSquared(Attacker->Position, Candidate.Position);
                if (!Target || Distance < BestDistance || (Distance == BestDistance && Candidate.UnitId < Target->UnitId))
                {
                    Target = &Candidate;
                    BestDistance = Distance;
                }
            }
            if (!Target) { continue; }
            const FIssuedOrder* Last = IssuedOrders.Find(Attacker->UnitId);
            if (Last && Last->Type == EProofCommandType::Attack && Last->TargetId == Target->UnitId
                && NowSeconds - Last->IssuedAt < OrderRetrySeconds)
            {
                continue;
            }
            if (AddCommand(EProofCommandType::Attack, Attacker->UnitId, Target->UnitId, FVector::ZeroVector))
            {
                FIssuedOrder& Issued = IssuedOrders.FindOrAdd(Attacker->UnitId);
                Issued.Type = EProofCommandType::Attack;
                Issued.TargetId = Target->UnitId;
                Issued.IssuedAt = NowSeconds;
                LastAttackUnitId = Attacker->UnitId;
            }
        }
    }
    return Commands;
}

void FProofSkirmishBot::Reset()
{
    IssuedOrders.Reset();
    NextSequence = 1;
    LastAttackUnitId = INDEX_NONE;
    LastGatherUnitId = INDEX_NONE;
    FirstDecisionAt = 0.0;
    NextDecisionAt = 0.0;
    LastObservedAt = 0.0;
    bStarted = false;
}
