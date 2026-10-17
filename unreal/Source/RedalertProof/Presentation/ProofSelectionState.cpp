#include "Presentation/ProofSelectionState.h"

namespace
{
const FProofUnitRecord* FindUnit(const FProofSnapshot& Snapshot, int32 UnitId)
{
    return Snapshot.Units.FindByPredicate([UnitId](const FProofUnitRecord& Unit) { return Unit.UnitId == UnitId; });
}

const FProofPlayerRecord* FindPlayer(const FProofSnapshot& Snapshot, int32 OwnerId)
{
    return Snapshot.Players.FindByPredicate([OwnerId](const FProofPlayerRecord& Player) { return Player.OwnerId == OwnerId; });
}

bool IsFiniteWorldPosition(const FVector& Position)
{
    const double Extent = FProofCommandModel::WorldExtent;
    return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z)
        && FMath::Abs(Position.X) <= Extent && FMath::Abs(Position.Y) <= Extent && FMath::Abs(Position.Z) <= Extent;
}

bool CanMove(const FProofUnitRecord& Unit)
{
    return Unit.Kind == EProofUnitKind::Combat || Unit.Kind == EProofUnitKind::Harvester;
}

double BoundFormationCenter(double Requested, double MinimumOffset, double MaximumOffset)
{
    const double Extent = FProofCommandModel::WorldExtent;
    return FMath::Clamp(Requested, -Extent - MinimumOffset, Extent - MaximumOffset);
}
}

bool FProofSelectionState::IsOwnedLiveUnit(const FProofSnapshot& Snapshot, int32 OwnerId, int32 UnitId)
{
    if (OwnerId < 0 || UnitId <= 0) { return false; }
    const FProofUnitRecord* Unit = FindUnit(Snapshot, UnitId);
    return Unit && Unit->OwnerId == OwnerId && Unit->HitPoints > 0.f;
}

void FProofSelectionState::PruneIds(TArray<int32>& Ids, const FProofSnapshot& Snapshot, int32 OwnerId)
{
    Ids.RemoveAll([&Snapshot, OwnerId](int32 UnitId) { return !IsOwnedLiveUnit(Snapshot, OwnerId, UnitId); });
    Ids.Sort();
    for (int32 Index = Ids.Num() - 1; Index > 0; --Index)
    {
        if (Ids[Index] == Ids[Index - 1]) { Ids.RemoveAt(Index, 1, EAllowShrinking::No); }
    }
    if (Ids.Num() > MaxSelectedUnits) { Ids.SetNum(MaxSelectedUnits, EAllowShrinking::No); }
}

void FProofSelectionState::Synchronize(const FProofSnapshot& Snapshot, int32 OwnerId)
{
    if (MatchEpoch != Snapshot.MatchEpoch || SelectionOwnerId != OwnerId)
    {
        MatchEpoch = Snapshot.MatchEpoch;
        SelectionOwnerId = OwnerId;
        SelectedUnitIds.Reset();
        PrimaryUnitId = INDEX_NONE;
        for (FGroup& Group : Groups)
        {
            Group.UnitIds.Reset();
            Group.PrimaryUnitId = INDEX_NONE;
        }
        return;
    }
    PruneIds(SelectedUnitIds, Snapshot, OwnerId);
    if (!SelectedUnitIds.Contains(PrimaryUnitId))
    {
        PrimaryUnitId = SelectedUnitIds.IsEmpty() ? INDEX_NONE : SelectedUnitIds[0];
    }
}

void FProofSelectionState::SelectSingle(const FProofSnapshot& Snapshot, int32 OwnerId, int32 UnitId)
{
    Synchronize(Snapshot, OwnerId);
    SelectedUnitIds.Reset();
    PrimaryUnitId = INDEX_NONE;
    if (IsOwnedLiveUnit(Snapshot, OwnerId, UnitId))
    {
        SelectedUnitIds.Add(UnitId);
        PrimaryUnitId = UnitId;
    }
}

void FProofSelectionState::Toggle(const FProofSnapshot& Snapshot, int32 OwnerId, int32 UnitId)
{
    Synchronize(Snapshot, OwnerId);
    if (!IsOwnedLiveUnit(Snapshot, OwnerId, UnitId)) { return; }
    const int32 ExistingIndex = SelectedUnitIds.IndexOfByKey(UnitId);
    if (ExistingIndex != INDEX_NONE)
    {
        SelectedUnitIds.RemoveAt(ExistingIndex, 1, EAllowShrinking::No);
        if (PrimaryUnitId == UnitId)
        {
            PrimaryUnitId = SelectedUnitIds.IsEmpty() ? INDEX_NONE : SelectedUnitIds[0];
        }
    }
    else if (SelectedUnitIds.Num() < MaxSelectedUnits)
    {
        SelectedUnitIds.Add(UnitId);
        SelectedUnitIds.Sort();
        PrimaryUnitId = UnitId;
    }
}

bool FProofSelectionState::IsViewportCenterVisible(const FVector2D& Center, const FIntPoint& Viewport)
{
    return Viewport.X > 0 && Viewport.Y > 0 && FMath::IsFinite(Center.X) && FMath::IsFinite(Center.Y)
        && Center.X >= 0.0 && Center.Y >= 0.0 && Center.X < Viewport.X && Center.Y < Viewport.Y;
}

bool FProofSelectionState::SelectVisibleType(const FProofSnapshot& Snapshot, int32 OwnerId,
    const TArray<int32>& VisibleUnitIds)
{
    const bool bCurrentContext = IsCurrentContext(Snapshot, OwnerId);
    const int32 SourcePrimary = PrimaryUnitId;
    Synchronize(Snapshot, OwnerId);
    const FProofUnitRecord* Primary = FindUnit(Snapshot, SourcePrimary);
    // A stale/empty anchor cannot implicitly choose a new kind after pruning or
    // carry a selection across an epoch/owner change. Normal pruning still runs.
    if (!bCurrentContext || !IsOwnedLiveUnit(Snapshot, OwnerId, SourcePrimary) || !Primary
        || !FMath::IsFinite(Primary->HitPoints) || !IsFiniteWorldPosition(Primary->Position)) return false;
    TArray<int32> Matches;
    for (int32 UnitId : VisibleUnitIds)
    {
        const FProofUnitRecord* Unit = FindUnit(Snapshot, UnitId);
        if (Unit && IsOwnedLiveUnit(Snapshot, OwnerId, UnitId) && Unit->Kind == Primary->Kind
            && FMath::IsFinite(Unit->HitPoints) && IsFiniteWorldPosition(Unit->Position)) Matches.AddUnique(UnitId);
    }
    Matches.Sort();
    const bool bKeepPrimary = Matches.Contains(SourcePrimary);
    if (Matches.Num() > MaxSelectedUnits)
    {
        // Reserve one slot for the visible anchor, then take the lowest IDs.
        if (bKeepPrimary) Matches.Remove(SourcePrimary);
        Matches.SetNum(bKeepPrimary ? MaxSelectedUnits - 1 : MaxSelectedUnits, EAllowShrinking::No);
        if (bKeepPrimary) { Matches.Add(SourcePrimary); Matches.Sort(); }
    }
    SelectedUnitIds = MoveTemp(Matches);
    PrimaryUnitId = bKeepPrimary ? SourcePrimary : (SelectedUnitIds.IsEmpty() ? INDEX_NONE : SelectedUnitIds[0]);
    return true;
}
void FProofSelectionState::AssignGroup(int32 Slot, const FProofSnapshot& Snapshot, int32 OwnerId)
{
    if (Slot < 1 || Slot > MaxControlGroups) { return; }
    Synchronize(Snapshot, OwnerId);
    FGroup& Group = Groups[Slot - 1];
    Group.UnitIds = SelectedUnitIds;
    Group.PrimaryUnitId = PrimaryUnitId;
}

void FProofSelectionState::RecallGroup(int32 Slot, const FProofSnapshot& Snapshot, int32 OwnerId)
{
    if (Slot < 1 || Slot > MaxControlGroups) { return; }
    Synchronize(Snapshot, OwnerId);
    FGroup& Group = Groups[Slot - 1];
    PruneIds(Group.UnitIds, Snapshot, OwnerId);
    SelectedUnitIds = Group.UnitIds;
    PrimaryUnitId = SelectedUnitIds.Contains(Group.PrimaryUnitId) ? Group.PrimaryUnitId
        : (SelectedUnitIds.IsEmpty() ? INDEX_NONE : SelectedUnitIds[0]);
    Group.PrimaryUnitId = PrimaryUnitId;
}

TArray<FProofCommand> FProofSelectionState::PlanMove(const FProofSnapshot& Snapshot, int32 OwnerId,
    const FVector& Destination) const
{
    return PlanTravel(Snapshot, OwnerId, Destination, EProofCommandType::Move);
}

TArray<FProofCommand> FProofSelectionState::PlanAttackMove(const FProofSnapshot& Snapshot, int32 OwnerId,
    const FVector& Destination) const
{
    if (!Snapshot.bSkirmish || !FindPlayer(Snapshot, OwnerId)) { return {}; }
    return PlanTravel(Snapshot, OwnerId, Destination, EProofCommandType::AttackMove);
}

TArray<FProofCommand> FProofSelectionState::PlanTravel(const FProofSnapshot& Snapshot, int32 OwnerId,
    const FVector& Destination, EProofCommandType Type) const
{
    TArray<FProofCommand> Commands;
    if (!IsCurrentContext(Snapshot, OwnerId) || (Snapshot.bSkirmish && Snapshot.MatchPhase != EProofMatchPhase::Playing)
        || !FMath::IsFinite(Destination.X) || !FMath::IsFinite(Destination.Y) || !FMath::IsFinite(Destination.Z))
    {
        return Commands;
    }

    TArray<const FProofUnitRecord*, TInlineAllocator<MaxSelectedUnits>> Movers;
    FVector Center = FVector::ZeroVector;
    for (int32 UnitId : SelectedUnitIds)
    {
        const FProofUnitRecord* Unit = FindUnit(Snapshot, UnitId);
        if (!Unit || !IsOwnedLiveUnit(Snapshot, OwnerId, UnitId) || !CanMove(*Unit)
            || (Type == EProofCommandType::AttackMove && Unit->Kind != EProofUnitKind::Combat)
            || !IsFiniteWorldPosition(Unit->Position)) { continue; }
        Movers.Add(Unit);
        Center += Unit->Position;
        if (Movers.Num() == MaxSelectedUnits) { break; }
    }
    if (Movers.IsEmpty()) { return Commands; }
    Center /= Movers.Num();

    FVector MinimumOffset = Movers[0]->Position - Center;
    FVector MaximumOffset = MinimumOffset;
    for (const FProofUnitRecord* Unit : Movers)
    {
        const FVector Offset = Unit->Position - Center;
        MinimumOffset.X = FMath::Min(MinimumOffset.X, Offset.X);
        MinimumOffset.Y = FMath::Min(MinimumOffset.Y, Offset.Y);
        MinimumOffset.Z = FMath::Min(MinimumOffset.Z, Offset.Z);
        MaximumOffset.X = FMath::Max(MaximumOffset.X, Offset.X);
        MaximumOffset.Y = FMath::Max(MaximumOffset.Y, Offset.Y);
        MaximumOffset.Z = FMath::Max(MaximumOffset.Z, Offset.Z);
    }
    const FVector BoundedCenter(
        BoundFormationCenter(Destination.X, MinimumOffset.X, MaximumOffset.X),
        BoundFormationCenter(Destination.Y, MinimumOffset.Y, MaximumOffset.Y),
        BoundFormationCenter(Destination.Z, MinimumOffset.Z, MaximumOffset.Z));
    for (const FProofUnitRecord* Unit : Movers)
    {
        FProofCommand Command;
        Command.Type = Type;
        Command.UnitId = Unit->UnitId;
        Command.Destination = BoundedCenter + (Unit->Position - Center);
        const double Extent = FProofCommandModel::WorldExtent;
        Command.Destination.X = FMath::Clamp(Command.Destination.X, -Extent, Extent);
        Command.Destination.Y = FMath::Clamp(Command.Destination.Y, -Extent, Extent);
        Command.Destination.Z = FMath::Clamp(Command.Destination.Z, -Extent, Extent);
        Commands.Add(Command);
    }
    return Commands;
}

TArray<FProofCommand> FProofSelectionState::PlanAttack(const FProofSnapshot& Snapshot, int32 OwnerId,
    int32 TargetUnitId) const
{
    TArray<FProofCommand> Commands;
    if (!IsCurrentContext(Snapshot, OwnerId) || (Snapshot.bSkirmish && Snapshot.MatchPhase != EProofMatchPhase::Playing))
    {
        return Commands;
    }
    const FProofUnitRecord* Target = FindUnit(Snapshot, TargetUnitId);
    const FProofPlayerRecord* OwnPlayer = FindPlayer(Snapshot, OwnerId);
    const FProofPlayerRecord* TargetPlayer = Target ? FindPlayer(Snapshot, Target->OwnerId) : nullptr;
    if (!Target || !(Target->HitPoints > 0.f) || Target->OwnerId == OwnerId || !OwnPlayer || !TargetPlayer
        || (Snapshot.bSkirmish && OwnPlayer->TeamId == TargetPlayer->TeamId)) { return Commands; }
    for (int32 UnitId : SelectedUnitIds)
    {
        const FProofUnitRecord* Unit = FindUnit(Snapshot, UnitId);
        if (!Unit || !IsOwnedLiveUnit(Snapshot, OwnerId, UnitId) || Unit->Kind != EProofUnitKind::Combat) { continue; }
        FProofCommand Command;
        Command.Type = EProofCommandType::Attack;
        Command.UnitId = UnitId;
        Command.TargetUnitId = TargetUnitId;
        Commands.Add(Command);
        if (Commands.Num() == MaxSelectedUnits) { break; }
    }
    return Commands;
}

TArray<FProofCommand> FProofSelectionState::PlanStop(const FProofSnapshot& Snapshot, int32 OwnerId) const
{
    TArray<FProofCommand> Commands;
    if (!IsCurrentContext(Snapshot, OwnerId) || !FindPlayer(Snapshot, OwnerId)
        || (Snapshot.bSkirmish && Snapshot.MatchPhase != EProofMatchPhase::Playing)) { return Commands; }
    for (int32 UnitId : SelectedUnitIds)
    {
        const FProofUnitRecord* Unit = FindUnit(Snapshot, UnitId);
        if (!Unit || !IsOwnedLiveUnit(Snapshot, OwnerId, UnitId) || !CanMove(*Unit)) { continue; }
        FProofCommand Command;
        Command.Type = EProofCommandType::Stop;
        Command.UnitId = UnitId;
        Commands.Add(Command);
        if (Commands.Num() == MaxSelectedUnits) { break; }
    }
    return Commands;
}
