#include "Commands/ProofCommandModel.h"
#include "Skirmish/ProofSkirmishRules.h"

#include "Misc/SecureHash.h"
#include <cmath>

namespace
{
bool IsWithinProofAttackRange(const FVector& A, const FVector& B)
{
    return FVector::DistSquared(A, B) <= FMath::Square(static_cast<double>(FProofCommandModel::AttackRange));
}
}

FString FProofSnapshot::ComputeDigest() const
{
    TArray<FProofPlayerRecord> SortedPlayers = Players;
    TArray<FProofUnitRecord> SortedUnits = Units;
    TArray<FProofResourceRecord> SortedResources = Resources;
    SortedPlayers.Sort([](const FProofPlayerRecord& A, const FProofPlayerRecord& B) { return A.OwnerId < B.OwnerId; });
    SortedUnits.Sort([](const FProofUnitRecord& A, const FProofUnitRecord& B) { return A.UnitId < B.UnitId; });
    SortedResources.Sort([](const FProofResourceRecord& A, const FProofResourceRecord& B) { return A.ResourceId < B.ResourceId; });
    FString Canonical = FString::Printf(TEXT("revision:%d;epoch:%d;skirmish:%d;phase:%d;winner:%d;"),
        Revision, MatchEpoch, bSkirmish ? 1 : 0, static_cast<int32>(MatchPhase), WinnerTeam);
    for (const FProofPlayerRecord& Player : SortedPlayers)
    {
        Canonical += FString::Printf(TEXT("p:%d,%d,%d,%d;"), Player.OwnerId, Player.Credits, Player.TeamId, Player.TotalGathered);
    }
    for (const FProofUnitRecord& Unit : SortedUnits)
    {
        Canonical += FString::Printf(TEXT("u:%d,%d,%.17g,%.17g,%.17g,%.17g,%d,%d;"),
            Unit.UnitId, Unit.OwnerId, Unit.Position.X, Unit.Position.Y, Unit.Position.Z,
            static_cast<double>(Unit.HitPoints), static_cast<int32>(Unit.Kind), Unit.Cargo);
    }
    for (const FProofResourceRecord& Resource : SortedResources)
    {
        Canonical += FString::Printf(TEXT("r:%d,%d,%.17g,%.17g,%.17g,%d;"), Resource.ResourceId,
            Resource.OwnerId, Resource.Position.X, Resource.Position.Y, Resource.Position.Z, Resource.Remaining);
    }
    return FMD5::HashAnsiString(*Canonical);
}

FProofCommandModel::FProofCommandModel(int32 InCommandsPerAdvance)
    : CommandsPerAdvance(FMath::Max(1, InCommandsPerAdvance))
{
}

bool FProofCommandModel::IsFinitePosition(const FVector& Position)
{
    return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z);
}

bool FProofCommandModel::IsInWorld(const FVector& Position)
{
    return IsFinitePosition(Position) && FMath::Abs(Position.X) <= WorldExtent
        && FMath::Abs(Position.Y) <= WorldExtent && FMath::Abs(Position.Z) <= WorldExtent;
}

void FProofCommandModel::BumpRevision()
{
    if (State.Revision < MAX_int32) { ++State.Revision; }
}

void FProofCommandModel::AddPlayer(int32 OwnerId, int32 StartingCredits)
{
    if (State.bSkirmish || OwnerId < 0 || LastSequences.Contains(OwnerId)) { return; }
    FProofPlayerRecord Player;
    Player.OwnerId = OwnerId;
    Player.Credits = FMath::Max(0, StartingCredits);
    State.Players.Add(Player);
    LastSequences.Add(OwnerId, 0);
    CommandCounts.Add(OwnerId, 0);
    BumpRevision();
}

int32 FProofCommandModel::SeedUnit(int32 OwnerId, FVector Position)
{
    if (State.bSkirmish || !LastSequences.Contains(OwnerId) || !IsInWorld(Position)) { return INDEX_NONE; }
    return AddUnit(OwnerId, Position, EProofUnitKind::Combat, 100.f);
}

int32 FProofCommandModel::AddUnit(int32 OwnerId, const FVector& Position, EProofUnitKind Kind, float HitPoints)
{
    if (NextUnitId == MAX_int32) { return INDEX_NONE; }
    FProofUnitRecord Unit;
    Unit.UnitId = NextUnitId++;
    Unit.OwnerId = OwnerId;
    Unit.Position = Position;
    Unit.Kind = Kind;
    Unit.HitPoints = HitPoints;
    State.Units.Add(Unit);
    BumpRevision();
    return Unit.UnitId;
}

void FProofCommandModel::StartSkirmish()
{
    Reset();
    State.bSkirmish = true;
    State.MatchPhase = EProofMatchPhase::Playing;
    BumpRevision();
}

bool FProofCommandModel::SeedSkirmishPlayer(int32 OwnerId, int32 TeamId, const FVector& HQPosition)
{
    const FVector OrePosition = HQPosition + FVector(550, 0, 0);
    const FVector HarvesterPosition = HQPosition + FVector(100, 0, 0);
    if (!State.bSkirmish || State.MatchPhase != EProofMatchPhase::Playing || OwnerId < 0 || (TeamId != 0 && TeamId != 1)
        || LastSequences.Contains(OwnerId) || !IsInWorld(HQPosition) || !IsInWorld(OrePosition)
        || !IsInWorld(HarvesterPosition) || NextUnitId > MAX_int32 - 2 || NextResourceId == MAX_int32)
    {
        return false;
    }
    FProofPlayerRecord Player;
    Player.OwnerId = OwnerId;
    Player.TeamId = TeamId;
    Player.Credits = 350;
    State.Players.Add(Player);
    LastSequences.Add(OwnerId, 0);
    CommandCounts.Add(OwnerId, 0);
    AddUnit(OwnerId, HQPosition, EProofUnitKind::HQ, 300.f);
    AddUnit(OwnerId, HarvesterPosition, EProofUnitKind::Harvester, 100.f);
    FProofResourceRecord Ore;
    Ore.ResourceId = NextResourceId++;
    Ore.OwnerId = OwnerId;
    Ore.Position = OrePosition;
    Ore.Remaining = 500;
    State.Resources.Add(Ore);
    BumpRevision();
    return true;
}

bool FProofCommandModel::RemoveSkirmishPlayer(int32 OwnerId)
{
    if (!State.bSkirmish || OwnerId < 0 || !LastSequences.Contains(OwnerId)) { return false; }

    const bool bBothTeamsRepresented = State.Players.ContainsByPredicate([](const FProofPlayerRecord& Player)
        { return Player.TeamId == 0; }) && State.Players.ContainsByPredicate([](const FProofPlayerRecord& Player)
        { return Player.TeamId == 1; });

    // Capture targets before erasing records. Other players may have issued
    // attacks against this player's units or gathered from their starter ore.
    TSet<int32> RemovedUnits;
    TSet<int32> RemovedResources;
    for (const FProofUnitRecord& Unit : State.Units)
    {
        if (Unit.OwnerId == OwnerId) { RemovedUnits.Add(Unit.UnitId); }
    }
    for (const FProofResourceRecord& Resource : State.Resources)
    {
        if (Resource.OwnerId == OwnerId) { RemovedResources.Add(Resource.ResourceId); }
    }

    State.Players.RemoveAll([OwnerId](const FProofPlayerRecord& Player) { return Player.OwnerId == OwnerId; });
    State.Units.RemoveAll([OwnerId](const FProofUnitRecord& Unit) { return Unit.OwnerId == OwnerId; });
    State.Resources.RemoveAll([OwnerId](const FProofResourceRecord& Resource) { return Resource.OwnerId == OwnerId; });
    LastSequences.Remove(OwnerId);
    CommandCounts.Remove(OwnerId);
    for (auto It = Orders.CreateIterator(); It; ++It)
    {
        const FOrder& Order = It.Value();
        const int32 OrderedUnitId = It.Key();
        const FProofUnitRecord* OrderedUnit = State.Units.FindByPredicate([OrderedUnitId](const FProofUnitRecord& Unit)
            { return Unit.UnitId == OrderedUnitId; });
        const bool bEmptyGatherFromRemovedOre = Order.Type == EProofCommandType::Gather
            && RemovedResources.Contains(Order.TargetUnitId) && (!OrderedUnit || OrderedUnit->Cargo <= 0);
        if (RemovedUnits.Contains(It.Key())
            || (Order.Type == EProofCommandType::Attack && RemovedUnits.Contains(Order.TargetUnitId))
            || bEmptyGatherFromRemovedOre)
        {
            It.RemoveCurrent();
        }
    }
    if (bBothTeamsRepresented && State.MatchPhase == EProofMatchPhase::Playing)
    {
        bool bTeam0HasHQ = false;
        bool bTeam1HasHQ = false;
        for (const FProofUnitRecord& Unit : State.Units)
        {
            if (Unit.Kind != EProofUnitKind::HQ || Unit.HitPoints <= 0.f) { continue; }
            const int32 Team = TeamForOwner(Unit.OwnerId);
            bTeam0HasHQ |= Team == 0;
            bTeam1HasHQ |= Team == 1;
        }
        if (!bTeam0HasHQ || !bTeam1HasHQ)
        {
            State.MatchPhase = EProofMatchPhase::Finished;
            State.WinnerTeam = bTeam0HasHQ ? 0 : (bTeam1HasHQ ? 1 : INDEX_NONE);
            Orders.Reset();
        }
    }
    BumpRevision();
    return true;
}

int32 FProofCommandModel::CountLiveOwnerUnits(int32 OwnerId) const
{
    int32 Count = 0;
    for (const FProofUnitRecord& Unit : State.Units)
    {
        if (Unit.OwnerId == OwnerId && Unit.HitPoints > 0.f) { ++Count; }
    }
    return Count;
}

int32 FProofCommandModel::TeamForOwner(int32 OwnerId) const
{
    const FProofPlayerRecord* Player = State.Players.FindByPredicate([OwnerId](const FProofPlayerRecord& P) { return P.OwnerId == OwnerId; });
    return Player ? Player->TeamId : INDEX_NONE;
}

const FProofUnitRecord* FProofCommandModel::FindLiveRefinery(int32 OwnerId, const FVector& From) const
{
    const FProofUnitRecord* Best = nullptr;
    for (const FProofUnitRecord& Unit : State.Units)
    {
        if (Unit.OwnerId != OwnerId || Unit.Kind != EProofUnitKind::Refinery || Unit.HitPoints <= 0.f) { continue; }
        if (!Best || FVector::DistSquared(From, Unit.Position) < FVector::DistSquared(From, Best->Position)
            || (FVector::DistSquared(From, Unit.Position) == FVector::DistSquared(From, Best->Position) && Unit.UnitId < Best->UnitId))
        {
            Best = &Unit;
        }
    }
    return Best;
}

FProofCommandResult FProofCommandModel::Submit(int32 TrustedOwnerId, const FProofCommand& Command)
{
    FProofCommandResult Result;
    Result.Sequence = Command.Sequence;
    const auto Reject = [&Result](const TCHAR* Reason)
    {
        Result.Reason = Reason;
        return Result;
    };
    int32* LastSequence = LastSequences.Find(TrustedOwnerId);
    if (!LastSequence) { return Reject(TEXT("unknown-player")); }
    if (State.bSkirmish && State.MatchPhase != EProofMatchPhase::Playing) { return Reject(TEXT("finished")); }
    const bool bMove = Command.Type == EProofCommandType::Move;
    const bool bAttack = Command.Type == EProofCommandType::Attack;
    const bool bProduce = Command.Type == EProofCommandType::Produce;
    const bool bBuildRefinery = Command.Type == EProofCommandType::BuildRefinery;
    const bool bBuildFactory = Command.Type == EProofCommandType::BuildFactory;
    const bool bGather = Command.Type == EProofCommandType::Gather;
    if ((!bMove && !bAttack && !bProduce && !bBuildRefinery && !bBuildFactory && !bGather)
        || ((!State.bSkirmish) && (bBuildRefinery || bBuildFactory || bGather)) || Command.Sequence <= 0
        || !IsFinitePosition(Command.Destination)
        || ((bMove || bAttack || bGather || (State.bSkirmish && bProduce)) && Command.UnitId <= 0)
        || ((bAttack || bGather) && Command.TargetUnitId <= 0))
    {
        return Reject(TEXT("malformed"));
    }
    if (static_cast<int64>(Command.Sequence) != static_cast<int64>(*LastSequence) + 1)
    {
        return Reject(TEXT("sequence"));
    }
    // Once a correctly shaped ordered envelope is admitted, gameplay failures
    // consume it too. This rule is identical for humans and bots.
    *LastSequence = Command.Sequence;
    int32& Count = CommandCounts.FindChecked(TrustedOwnerId);
    if (Count >= CommandsPerAdvance) { return Reject(TEXT("rate")); }
    ++Count;
    if (bProduce)
    {
        FProofPlayerRecord* Player = State.Players.FindByPredicate([TrustedOwnerId](const FProofPlayerRecord& P) { return P.OwnerId == TrustedOwnerId; });
        if (!Player || Player->Credits < ProductionCost) { return Reject(TEXT("resources")); }
        if (State.bSkirmish)
        {
            const FProofUnitRecord* Factory = State.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; });
            if (!Factory || Factory->OwnerId != TrustedOwnerId || Factory->Kind != EProofUnitKind::Factory || Factory->HitPoints <= 0.f)
            {
                return Reject(TEXT("factory"));
            }
            if (CountLiveOwnerUnits(TrustedOwnerId) >= SkirmishRecordCap) { return Reject(TEXT("capacity")); }
        }
        const FProofUnitRecord* Anchor = State.bSkirmish
            ? State.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; })
            : State.Units.FindByPredicate([TrustedOwnerId](const FProofUnitRecord& U) { return U.OwnerId == TrustedOwnerId && U.HitPoints > 0.f; });
        FVector Spawn = Anchor ? Anchor->Position : FVector::ZeroVector;
        if (State.bSkirmish)
        {
            // The cursor picks the first marker at a shared screen position.
            // Search a fixed, bounded grid so each new combat unit has its own
            // selectable center, including when several units are produced at
            // one factory. Legacy proof production stays at its anchor.
            constexpr double SpawnStep = 300.0;
            constexpr double MinimumCenterDistanceSquared = 260.0 * 260.0;
            const FVector Directions[] = {
                FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0),
                FVector(1, 1, 0), FVector(-1, 1, 0), FVector(1, -1, 0), FVector(-1, -1, 0)
            };
            bool bFoundSpawn = false;
            for (int32 Ring = 1; Ring <= 3 && !bFoundSpawn; ++Ring)
            {
                for (const FVector& Direction : Directions)
                {
                    const FVector Candidate = Anchor->Position + Direction * (SpawnStep * Ring);
                    if (!IsInWorld(Candidate)) { continue; }
                    bool bClear = true;
                    for (const FProofUnitRecord& Other : State.Units)
                    {
                        if (Other.HitPoints > 0.f && FVector::DistSquared(Candidate, Other.Position) < MinimumCenterDistanceSquared)
                        {
                            bClear = false;
                            break;
                        }
                    }
                    if (bClear)
                    {
                        Spawn = Candidate;
                        bFoundSpawn = true;
                        break;
                    }
                }
            }
            if (!bFoundSpawn) { return Reject(TEXT("spawn")); }
        }
        if (AddUnit(TrustedOwnerId, Spawn, EProofUnitKind::Combat, 100.f) == INDEX_NONE) { return Reject(TEXT("capacity")); }
        Player->Credits -= ProductionCost;
    }
    else if (bBuildRefinery || bBuildFactory)
    {
        if (!IsInWorld(Command.Destination)) { return Reject(TEXT("position")); }
        FProofPlayerRecord* Player = State.Players.FindByPredicate([TrustedOwnerId](const FProofPlayerRecord& P) { return P.OwnerId == TrustedOwnerId; });
        const int32 Cost = bBuildRefinery ? RefineryCost : FactoryCost;
        if (!Player || Player->Credits < Cost) { return Reject(TEXT("resources")); }
        if (CountLiveOwnerUnits(TrustedOwnerId) >= SkirmishRecordCap) { return Reject(TEXT("capacity")); }
        bool bNearHQ = false;
        for (const FProofUnitRecord& Unit : State.Units)
        {
            if (Unit.HitPoints <= 0.f) { continue; }
            if (Unit.OwnerId == TrustedOwnerId && Unit.Kind == EProofUnitKind::HQ
                && FVector::DistSquared(Unit.Position, Command.Destination) <= FMath::Square(static_cast<double>(BuildRadius)))
            {
                bNearHQ = true;
            }
            if ((Unit.Kind == EProofUnitKind::HQ || Unit.Kind == EProofUnitKind::Refinery || Unit.Kind == EProofUnitKind::Factory)
                && FVector::DistSquared(Unit.Position, Command.Destination) < FMath::Square(static_cast<double>(StructureSeparation)))
            {
                return Reject(TEXT("overlap"));
            }
        }
        if (!bNearHQ) { return Reject(TEXT("placement")); }
        if (AddUnit(TrustedOwnerId, Command.Destination, bBuildRefinery ? EProofUnitKind::Refinery : EProofUnitKind::Factory, 200.f) == INDEX_NONE)
        {
            return Reject(TEXT("capacity"));
        }
        Player->Credits -= Cost;
    }
    else
    {
        const FProofUnitRecord* Unit = State.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; });
        if (!Unit || Unit->OwnerId != TrustedOwnerId || Unit->HitPoints <= 0.f) { return Reject(TEXT("ownership")); }
        if (State.bSkirmish && ((bMove && (Unit->Kind == EProofUnitKind::HQ || Unit->Kind == EProofUnitKind::Refinery || Unit->Kind == EProofUnitKind::Factory))
            || (bAttack && Unit->Kind != EProofUnitKind::Combat) || (bGather && Unit->Kind != EProofUnitKind::Harvester)))
        {
            return Reject(TEXT("role"));
        }
        FOrder Order;
        Order.Type = Command.Type;
        if (bMove)
        {
            if (!IsInWorld(Command.Destination)) { return Reject(TEXT("position")); }
            Order.Destination = Command.Destination;
        }
        else if (bAttack)
        {
            const FProofUnitRecord* Target = State.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.TargetUnitId; });
            if (!Target || Target->HitPoints <= 0.f || Target->OwnerId == TrustedOwnerId) { return Reject(TEXT("target")); }
            if (State.bSkirmish && TeamForOwner(Target->OwnerId) == TeamForOwner(TrustedOwnerId)) { return Reject(TEXT("team")); }
            Order.TargetUnitId = Command.TargetUnitId;
        }
        else
        {
            const FProofResourceRecord* Resource = State.Resources.FindByPredicate([&Command](const FProofResourceRecord& R) { return R.ResourceId == Command.TargetUnitId; });
            // A loaded harvester may resume its return after a Move interrupts
            // Gather, including after its final extraction depleted the patch.
            if (!Resource || (Resource->Remaining <= 0 && Unit->Cargo <= 0)) { return Reject(TEXT("resource")); }
            if (!FindLiveRefinery(TrustedOwnerId, Unit->Position)) { return Reject(TEXT("refinery")); }
            Order.TargetUnitId = Command.TargetUnitId;
            Order.bReturning = Unit->Cargo > 0;
        }
        Orders.Add(Command.UnitId, Order);
    }
    BumpRevision();
    Result.bAccepted = true;
    Result.Reason = TEXT("accepted");
    return Result;
}

void FProofCommandModel::Advance(float DeltaSeconds)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f) { return; }
    if (State.bSkirmish && State.MatchPhase != EProofMatchPhase::Playing) { return; }
    for (TPair<int32, int32>& Entry : CommandCounts) { Entry.Value = 0; }
    if (DeltaSeconds == 0.f) { return; }

    // Every mover/chaser reads exactly the same initial state. Otherwise array
    // order would affect pursuit destinations and the first attack opportunity.
    const TArray<FProofUnitRecord> BeforeMove = State.Units;
    for (int32 Index = 0; Index < BeforeMove.Num(); ++Index)
    {
        const FProofUnitRecord& Unit = BeforeMove[Index];
        const FOrder* Order = Orders.Find(Unit.UnitId);
        if (Unit.HitPoints <= 0.f || !Order) { continue; }
        FVector Destination = Order->Destination;
        double StopDistance = 0.0;
        if (Order->Type == EProofCommandType::Attack)
        {
            const FProofUnitRecord* Target = BeforeMove.FindByPredicate([Order](const FProofUnitRecord& U) { return U.UnitId == Order->TargetUnitId && U.HitPoints > 0.f; });
            if (!Target) { Orders.Remove(Unit.UnitId); continue; }
            Destination = Target->Position;
            StopDistance = AttackRange;
        }
        else if (Order->Type == EProofCommandType::Gather)
        {
            if (Order->bReturning || Unit.Cargo > 0)
            {
                const FProofUnitRecord* Refinery = FindLiveRefinery(Unit.OwnerId, Unit.Position);
                if (!Refinery) { continue; }
                Destination = Refinery->Position;
            }
            else
            {
                const FProofResourceRecord* Resource = State.Resources.FindByPredicate([Order](const FProofResourceRecord& R) { return R.ResourceId == Order->TargetUnitId; });
                if (!Resource || Resource->Remaining <= 0) { Orders.Remove(Unit.UnitId); continue; }
                Destination = Resource->Position;
            }
        }
        else if (Order->Type != EProofCommandType::Move)
        {
            continue;
        }
        const FVector Offset = Destination - Unit.Position;
        const double Distance = Offset.Size();
        const bool bAttackOrder = Order->Type == EProofCommandType::Attack;
        const bool bNeedsMovement = bAttackOrder ? !IsWithinProofAttackRange(Unit.Position, Destination) : Distance > StopDistance;
        if (bNeedsMovement && Distance > UE_SMALL_NUMBER)
        {
            const double Remaining = FMath::Max(0.0, Distance - StopDistance);
            const double Step = FMath::Min(static_cast<double>(MovementSpeed) * DeltaSeconds, Remaining);
            FVector Landing = Unit.Position + Offset * (Step / Distance);
            if (bAttackOrder && Step == Remaining)
            {
                // Projection can round just outside the closed attack sphere,
                // including when sqrt rounds Distance to exactly StopDistance.
                // Correct only the reached endpoint by representable steps
                // inward; the strict range predicate itself never gets slack.
                // Bound work even if a future numeric/platform change prevents
                // correction; a later tick can retry rather than spin forever.
                for (int32 Correction = 0; Correction < 8 && !IsWithinProofAttackRange(Landing, Destination); ++Correction)
                {
                    for (int32 Axis = 0; Axis < 3; ++Axis)
                    {
                        if (Landing[Axis] == Destination[Axis]) { continue; }
                        const double Relative = Landing[Axis] - Destination[Axis];
                        const double Inward = Destination[Axis] + std::nextafter(Relative, 0.0);
                        // Relative reconstruction avoids subnormal steps near
                        // zero; absolute fallback handles a large target origin
                        // rounding that reconstruction back to the same point.
                        Landing[Axis] = Inward != Landing[Axis] ? Inward : std::nextafter(Landing[Axis], Destination[Axis]);
                    }
                }
            }
            State.Units[Index].Position = Landing;
        }
    }

    // All combat observes post-move positions and pre-damage HP. Damage is
    // accumulated before applying, so mutually lethal attackers both fire.
    TMap<int32, double> Damage;
    for (const FProofUnitRecord& Unit : State.Units)
    {
        const FOrder* Order = Orders.Find(Unit.UnitId);
        if (Unit.HitPoints <= 0.f || !Order || Order->Type != EProofCommandType::Attack) { continue; }
        const FProofUnitRecord* Target = State.Units.FindByPredicate([Order](const FProofUnitRecord& U) { return U.UnitId == Order->TargetUnitId && U.HitPoints > 0.f; });
        if (Target && IsWithinProofAttackRange(Unit.Position, Target->Position))
        {
            Damage.FindOrAdd(Target->UnitId) += static_cast<double>(DamagePerSecond) * DeltaSeconds;
        }
    }
    for (FProofUnitRecord& Unit : State.Units)
    {
        if (const double* Amount = Damage.Find(Unit.UnitId))
        {
            Unit.HitPoints = static_cast<float>(FMath::Max(0.0, static_cast<double>(Unit.HitPoints) - *Amount));
        }
        if (Unit.HitPoints <= 0.f) { Orders.Remove(Unit.UnitId); }
    }

    if (State.bSkirmish)
    {
        const FProofMatchOutcome Outcome = FProofSkirmishRules::Evaluate(State);
        if (Outcome.bFinished)
        {
            State.MatchPhase = EProofMatchPhase::Finished;
            State.WinnerTeam = Outcome.WinnerTeam;
            Orders.Reset();
            BumpRevision();
            return;
        }
        // Extraction and deposit happen only after travel and combat. Unit IDs
        // are assigned in insertion order, giving contested ore a stable order.
        for (FProofUnitRecord& Unit : State.Units)
        {
            FOrder* Order = Orders.Find(Unit.UnitId);
            if (Unit.HitPoints <= 0.f || Unit.Kind != EProofUnitKind::Harvester || !Order || Order->Type != EProofCommandType::Gather) { continue; }
            if (Order->bReturning || Unit.Cargo > 0)
            {
                const FProofUnitRecord* Refinery = FindLiveRefinery(Unit.OwnerId, Unit.Position);
                if (Refinery && FVector::DistSquared(Unit.Position, Refinery->Position) <= 1.0)
                {
                    FProofPlayerRecord* Player = State.Players.FindByPredicate([&Unit](const FProofPlayerRecord& P) { return P.OwnerId == Unit.OwnerId; });
                    if (Player)
                    {
                        const int32 Deposit = FMath::Min(Unit.Cargo, MAX_int32 - Player->Credits);
                        Player->Credits += Deposit;
                        Player->TotalGathered += FMath::Min(Deposit, MAX_int32 - Player->TotalGathered);
                        Unit.Cargo -= Deposit;
                    }
                    if (Unit.Cargo == 0)
                    {
                        Order->bReturning = false;
                        const FProofResourceRecord* Resource = State.Resources.FindByPredicate([Order](const FProofResourceRecord& R) { return R.ResourceId == Order->TargetUnitId; });
                        if (!Resource || Resource->Remaining <= 0) { Orders.Remove(Unit.UnitId); }
                    }
                }
            }
            else
            {
                FProofResourceRecord* Resource = State.Resources.FindByPredicate([Order](const FProofResourceRecord& R) { return R.ResourceId == Order->TargetUnitId; });
                if (!Resource || Resource->Remaining <= 0) { Orders.Remove(Unit.UnitId); continue; }
                if (FVector::DistSquared(Unit.Position, Resource->Position) <= 1.0)
                {
                    const int32 Taken = FMath::Min(CargoCapacity - Unit.Cargo, Resource->Remaining);
                    Unit.Cargo += Taken;
                    Resource->Remaining -= Taken;
                    Order->bReturning = Unit.Cargo > 0;
                }
            }
        }
    }
    BumpRevision();
}

FProofSnapshot FProofCommandModel::GetSnapshot() const
{
    return State;
}

void FProofCommandModel::Reset()
{
    State = FProofSnapshot();
    Orders.Reset();
    LastSequences.Reset();
    CommandCounts.Reset();
    NextUnitId = 1;
    NextResourceId = 1;
}
