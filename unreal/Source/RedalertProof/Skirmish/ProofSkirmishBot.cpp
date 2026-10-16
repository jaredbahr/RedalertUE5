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
constexpr double WorldExtent = FProofCommandModel::WorldExtent;

bool IsStructure(EProofUnitKind Kind)
{
    return Kind == EProofUnitKind::HQ || Kind == EProofUnitKind::Refinery || Kind == EProofUnitKind::Factory;
}

bool IsValidBuildPosition(const FProofSnapshot& Snapshot, const FVector& Position)
{
    if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Position.Z)
        || FMath::Abs(Position.X) > WorldExtent || FMath::Abs(Position.Y) > WorldExtent
        || FMath::Abs(Position.Z) > WorldExtent)
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

bool FindBuildPosition(const FProofSnapshot& Snapshot, const FVector& HQPosition, FVector& OutPosition)
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
        if (IsValidBuildPosition(Snapshot, Candidate))
        {
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
    const FProofUnitRecord* Harvester = nullptr;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.OwnerId != OwnerId || Unit.HitPoints <= 0.f) { continue; }
        OwnUnits.Add(&Unit);
        if (Unit.Kind == EProofUnitKind::HQ && (!HQ || Unit.UnitId < HQ->UnitId)) { HQ = &Unit; }
        if (Unit.Kind == EProofUnitKind::Refinery && (!Refinery || Unit.UnitId < Refinery->UnitId)) { Refinery = &Unit; }
        if (Unit.Kind == EProofUnitKind::Factory && (!Factory || Unit.UnitId < Factory->UnitId)) { Factory = &Unit; }
        if (Unit.Kind == EProofUnitKind::Harvester && (!Harvester || Unit.UnitId < Harvester->UnitId)) { Harvester = &Unit; }
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
    if (HQ && !Refinery && SpendableCredits >= RefineryCost && PlannedLiveCount < MaxLiveUnitsPerOwner)
    {
        FVector Site;
        if (FindBuildPosition(PlacementView, HQ->Position, Site)
            && AddCommand(EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, Site))
        {
            SpendableCredits -= RefineryCost;
            ++PlannedLiveCount;
            FProofUnitRecord Reserved;
            Reserved.Kind = EProofUnitKind::Refinery;
            Reserved.HitPoints = 1.f;
            Reserved.Position = Site;
            PlacementView.Units.Add(Reserved);
        }
    }
    if (HQ && !Factory && SpendableCredits >= FactoryCost && PlannedLiveCount < MaxLiveUnitsPerOwner)
    {
        FVector Site;
        if (FindBuildPosition(PlacementView, HQ->Position, Site)
            && AddCommand(EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, Site))
        {
            SpendableCredits -= FactoryCost;
            ++PlannedLiveCount;
        }
    }

    if (Harvester && Refinery)
    {
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

    if (NowSeconds - FirstDecisionAt >= AttackGraceSeconds)
    {
        for (const FProofUnitRecord* Attacker : OwnUnits)
        {
            if (Commands.Num() >= MaxCommandsPerDecision) { break; }
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
            }
        }
    }
    return Commands;
}

void FProofSkirmishBot::Reset()
{
    IssuedOrders.Reset();
    NextSequence = 1;
    FirstDecisionAt = 0.0;
    NextDecisionAt = 0.0;
    LastObservedAt = 0.0;
    bStarted = false;
}
