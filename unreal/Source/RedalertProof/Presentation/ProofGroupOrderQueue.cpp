#include "Presentation/ProofGroupOrderQueue.h"
#include "Presentation/ProofSelectionState.h"
#include "Presentation/ProofReturnCargoSelection.h"

namespace
{
const FProofUnitRecord* FindQueueUnit(const FProofSnapshot& Snapshot, int32 UnitId)
{
    return Snapshot.Units.FindByPredicate([UnitId](const FProofUnitRecord& Unit) { return Unit.UnitId == UnitId; });
}

const FProofPlayerRecord* FindQueuePlayer(const FProofSnapshot& Snapshot, int32 OwnerId)
{
    return Snapshot.Players.FindByPredicate([OwnerId](const FProofPlayerRecord& Player) { return Player.OwnerId == OwnerId; });
}

bool IsFiniteQueuePosition(const FVector& Position)
{
    const double Extent = FProofCommandModel::WorldExtent;
    return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z)
        && FMath::Abs(Position.X) <= Extent && FMath::Abs(Position.Y) <= Extent && FMath::Abs(Position.Z) <= Extent;
}
}

void FProofGroupOrderQueue::Clear()
{
    Pending.Reset();
    MatchEpoch = INDEX_NONE;
    OwnerId = INDEX_NONE;
    LastObservedWallSeconds = 0.0;
    LastDispatchWallSeconds = 0.0;
    bHasObservedWallTime = false;
    bHasDispatched = false;
}

void FProofGroupOrderQueue::CancelUnit(int32 UnitId)
{
    Pending.RemoveAll([UnitId](const FProofCommand& Command) { return Command.UnitId == UnitId; });
}

void FProofGroupOrderQueue::Synchronize(const FProofSnapshot& Snapshot, int32 CurrentOwnerId)
{
    if (MatchEpoch != Snapshot.MatchEpoch || OwnerId != CurrentOwnerId)
    {
        Clear();
        MatchEpoch = Snapshot.MatchEpoch;
        OwnerId = CurrentOwnerId;
    }
}

bool FProofGroupOrderQueue::IsValidIntent(const FProofSnapshot& Snapshot, int32 CurrentOwnerId,
    const FProofCommand& Command)
{
    if (CurrentOwnerId < 0 || !FindQueuePlayer(Snapshot, CurrentOwnerId)
        || (Snapshot.bSkirmish && Snapshot.MatchPhase != EProofMatchPhase::Playing))
    {
        return false;
    }
    const FProofUnitRecord* Unit = FindQueueUnit(Snapshot, Command.UnitId);
    if (!Unit || Unit->OwnerId != CurrentOwnerId || !(Unit->HitPoints > 0.f)) { return false; }
    if (Command.Type == EProofCommandType::ReturnCargo)
    {
        return FProofReturnCargoSelection::CanReturn(Snapshot, CurrentOwnerId, Command.UnitId, Command.TargetUnitId);
    }
    if (Command.Type == EProofCommandType::Gather)
    {
        return FProofSelectionState::CanGather(Snapshot, CurrentOwnerId, Command.UnitId, Command.TargetUnitId);
    }
    if (Command.Type == EProofCommandType::Stop)
    {
        return Unit->Kind == EProofUnitKind::Combat || Unit->Kind == EProofUnitKind::Harvester;
    }
    if (Command.Type == EProofCommandType::AttackMove)
    {
        return Snapshot.bSkirmish && Unit->Kind == EProofUnitKind::Combat && IsFiniteQueuePosition(Command.Destination);
    }
    if (Command.Type == EProofCommandType::Move)
    {
        return (Unit->Kind == EProofUnitKind::Combat || Unit->Kind == EProofUnitKind::Harvester)
            && IsFiniteQueuePosition(Command.Destination);
    }
    if (Command.Type != EProofCommandType::Attack || Unit->Kind != EProofUnitKind::Combat) { return false; }
    const FProofUnitRecord* Target = FindQueueUnit(Snapshot, Command.TargetUnitId);
    if (!Target || !(Target->HitPoints > 0.f) || Target->OwnerId == CurrentOwnerId) { return false; }
    if (!Snapshot.bSkirmish) { return true; }
    const FProofPlayerRecord* TargetPlayer = FindQueuePlayer(Snapshot, Target->OwnerId);
    const FProofPlayerRecord* OwnPlayer = FindQueuePlayer(Snapshot, CurrentOwnerId);
    return TargetPlayer && OwnPlayer && OwnPlayer->TeamId != TargetPlayer->TeamId;
}

void FProofGroupOrderQueue::Prune(const FProofSnapshot& Snapshot, int32 CurrentOwnerId)
{
    Pending.RemoveAll([&Snapshot, CurrentOwnerId](const FProofCommand& Command)
        { return !IsValidIntent(Snapshot, CurrentOwnerId, Command); });
}

bool FProofGroupOrderQueue::Enqueue(const FProofSnapshot& Snapshot, int32 CurrentOwnerId,
    const TArray<FProofCommand>& Commands)
{
    Synchronize(Snapshot, CurrentOwnerId);
    Prune(Snapshot, CurrentOwnerId);

    // Stage the whole batch before changing the live queue. Replacing an
    // existing unit keeps its original place among other pending units.
    TArray<FProofCommand> Proposed = Pending;
    for (const FProofCommand& Command : Commands)
    {
        if ((Command.Type != EProofCommandType::Move && Command.Type != EProofCommandType::Attack
            && Command.Type != EProofCommandType::Stop && Command.Type != EProofCommandType::AttackMove
            && Command.Type != EProofCommandType::Gather && Command.Type != EProofCommandType::ReturnCargo)
            || Command.UnitId <= 0 || ((Command.Type == EProofCommandType::Attack || Command.Type == EProofCommandType::Gather
                || Command.Type == EProofCommandType::ReturnCargo) && Command.TargetUnitId <= 0))
        {
            return false;
        }
        FProofCommand Intent = Command;
        Intent.Sequence = 0;
        Intent.MatchEpoch = 0;
        if (Intent.Type == EProofCommandType::Gather || Intent.Type == EProofCommandType::ReturnCargo)
        {
            if (!IsValidIntent(Snapshot, CurrentOwnerId, Intent)) { return false; }
            Intent.Destination = FVector::ZeroVector;
        }
        if (Intent.Type == EProofCommandType::Stop)
        {
            if (!IsValidIntent(Snapshot, CurrentOwnerId, Intent)) { return false; }
            Intent.TargetUnitId = INDEX_NONE;
            Intent.Destination = FVector::ZeroVector;
        }
        if (Intent.Type == EProofCommandType::Move || Intent.Type == EProofCommandType::AttackMove)
        {
            if (!FMath::IsFinite(Intent.Destination.X) || !FMath::IsFinite(Intent.Destination.Y)
                || !FMath::IsFinite(Intent.Destination.Z)) { return false; }
            const double Extent = FProofCommandModel::WorldExtent;
            Intent.Destination.X = FMath::Clamp(Intent.Destination.X, -Extent, Extent);
            Intent.Destination.Y = FMath::Clamp(Intent.Destination.Y, -Extent, Extent);
            Intent.Destination.Z = FMath::Clamp(Intent.Destination.Z, -Extent, Extent);
        }
        if (Intent.Type == EProofCommandType::AttackMove)
        {
            Intent.TargetUnitId = INDEX_NONE;
            if (!IsValidIntent(Snapshot, CurrentOwnerId, Intent)) { return false; }
        }
        // Reject an invalid normalized intent before it can replace a valid
        // pending order or partially commit another member of this batch.
        if (!IsValidIntent(Snapshot, CurrentOwnerId, Intent)) { return false; }
        const int32 ExistingIndex = Proposed.IndexOfByPredicate([&Intent](const FProofCommand& PendingCommand)
            { return PendingCommand.UnitId == Intent.UnitId; });
        if (ExistingIndex != INDEX_NONE)
        {
            Proposed[ExistingIndex] = Intent;
        }
        else
        {
            if (Proposed.Num() == MaxPendingCommands) { return false; }
            Proposed.Add(Intent);
        }
    }
    Pending = MoveTemp(Proposed);
    return true;
}

TArray<FProofCommand> FProofGroupOrderQueue::PopReady(const FProofSnapshot& Snapshot, int32 CurrentOwnerId,
    double WallSeconds)
{
    Synchronize(Snapshot, CurrentOwnerId);
    Prune(Snapshot, CurrentOwnerId);
    TArray<FProofCommand> Ready;
    if (Pending.IsEmpty() || !FMath::IsFinite(WallSeconds) || WallSeconds < 0.0
        || (bHasObservedWallTime && WallSeconds < LastObservedWallSeconds))
    {
        return Ready;
    }
    LastObservedWallSeconds = WallSeconds;
    bHasObservedWallTime = true;
    // A tiny tolerance keeps an exact decimal boundary (for example 100.1)
    // from being delayed by binary floating-point representation.
    if (bHasDispatched && WallSeconds - LastDispatchWallSeconds + 1.e-9 < DispatchIntervalSeconds)
    {
        return Ready;
    }
    const int32 Count = FMath::Min(MaxCommandsPerInterval, Pending.Num());
    Ready.Reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index) { Ready.Add(Pending[Index]); }
    Pending.RemoveAt(0, Count, EAllowShrinking::No);
    LastDispatchWallSeconds = WallSeconds;
    bHasDispatched = true;
    return Ready;
}
