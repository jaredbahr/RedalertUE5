#include "Commands/ProofCommandModel.h"

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
    SortedPlayers.Sort([](const FProofPlayerRecord& A, const FProofPlayerRecord& B) { return A.OwnerId < B.OwnerId; });
    SortedUnits.Sort([](const FProofUnitRecord& A, const FProofUnitRecord& B) { return A.UnitId < B.UnitId; });
    FString Canonical = FString::Printf(TEXT("revision:%d;"), Revision);
    for (const FProofPlayerRecord& Player : SortedPlayers)
    {
        Canonical += FString::Printf(TEXT("p:%d,%d;"), Player.OwnerId, Player.Credits);
    }
    for (const FProofUnitRecord& Unit : SortedUnits)
    {
        Canonical += FString::Printf(TEXT("u:%d,%d,%.17g,%.17g,%.17g,%.17g;"),
            Unit.UnitId, Unit.OwnerId, Unit.Position.X, Unit.Position.Y, Unit.Position.Z,
            static_cast<double>(Unit.HitPoints));
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
    if (OwnerId < 0 || LastSequences.Contains(OwnerId)) { return; }
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
    if (!LastSequences.Contains(OwnerId) || !IsInWorld(Position) || NextUnitId == MAX_int32) { return INDEX_NONE; }
    FProofUnitRecord Unit;
    Unit.UnitId = NextUnitId++;
    Unit.OwnerId = OwnerId;
    Unit.Position = Position;
    State.Units.Add(Unit);
    BumpRevision();
    return Unit.UnitId;
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
    const bool bMove = Command.Type == EProofCommandType::Move;
    const bool bAttack = Command.Type == EProofCommandType::Attack;
    const bool bProduce = Command.Type == EProofCommandType::Produce;
    if ((!bMove && !bAttack && !bProduce) || Command.Sequence <= 0
        || !IsFinitePosition(Command.Destination)
        || ((bMove || bAttack) && Command.UnitId <= 0)
        || (bAttack && Command.TargetUnitId <= 0))
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
        const FProofUnitRecord* Anchor = State.Units.FindByPredicate([TrustedOwnerId](const FProofUnitRecord& U) { return U.OwnerId == TrustedOwnerId && U.HitPoints > 0.f; });
        const FVector Spawn = Anchor ? Anchor->Position : FVector::ZeroVector;
        if (SeedUnit(TrustedOwnerId, Spawn) == INDEX_NONE) { return Reject(TEXT("capacity")); }
        Player->Credits -= ProductionCost;
    }
    else
    {
        const FProofUnitRecord* Unit = State.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; });
        if (!Unit || Unit->OwnerId != TrustedOwnerId || Unit->HitPoints <= 0.f) { return Reject(TEXT("ownership")); }
        FOrder Order;
        Order.Type = Command.Type;
        if (bMove)
        {
            if (!IsInWorld(Command.Destination)) { return Reject(TEXT("position")); }
            Order.Destination = Command.Destination;
        }
        else
        {
            const FProofUnitRecord* Target = State.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.TargetUnitId; });
            if (!Target || Target->HitPoints <= 0.f || Target->OwnerId == TrustedOwnerId) { return Reject(TEXT("target")); }
            Order.TargetUnitId = Command.TargetUnitId;
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
}
