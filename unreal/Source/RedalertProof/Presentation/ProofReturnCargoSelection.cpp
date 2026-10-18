#include "Presentation/ProofReturnCargoSelection.h"

bool FProofReturnCargoSelection::IsOwnedLiveRefinery(const FProofSnapshot& Snapshot, int32 OwnerId, int32 RefineryId)
{
    if (!Snapshot.bSkirmish || Snapshot.MatchPhase != EProofMatchPhase::Playing || OwnerId < 0 || RefineryId <= 0
        || !Snapshot.Players.ContainsByPredicate([OwnerId](const FProofPlayerRecord& P) { return P.OwnerId == OwnerId; })) return false;
    return Snapshot.Units.ContainsByPredicate([OwnerId, RefineryId](const FProofUnitRecord& Unit)
        { return Unit.UnitId == RefineryId && Unit.OwnerId == OwnerId && Unit.Kind == EProofUnitKind::Refinery
            && FMath::IsFinite(Unit.HitPoints) && Unit.HitPoints > 0.f; });
}

bool FProofReturnCargoSelection::CanReturn(const FProofSnapshot& Snapshot, int32 OwnerId, int32 UnitId, int32 RefineryId)
{
    if (UnitId <= 0 || !IsOwnedLiveRefinery(Snapshot, OwnerId, RefineryId)) return false;
    return Snapshot.Units.ContainsByPredicate([OwnerId, UnitId](const FProofUnitRecord& Unit)
        { return Unit.UnitId == UnitId && Unit.OwnerId == OwnerId && Unit.Kind == EProofUnitKind::Harvester
            && FMath::IsFinite(Unit.HitPoints) && Unit.HitPoints > 0.f && Unit.Cargo > 0; });
}

TArray<FProofCommand> FProofReturnCargoSelection::Plan(const FProofSnapshot& Snapshot, int32 OwnerId,
    const FProofSelectionState& Selection, int32 RefineryId)
{
    TArray<FProofCommand> Commands;
    // IsAllHarvesters also checks the selection's saved owner/epoch context.
    if (!Selection.IsAllHarvesters(Snapshot, OwnerId) || !IsOwnedLiveRefinery(Snapshot, OwnerId, RefineryId)) return Commands;
    for (int32 UnitId : Selection.GetSelectedUnitIds())
    {
        if (!CanReturn(Snapshot, OwnerId, UnitId, RefineryId)) continue;
        FProofCommand Command;
        Command.Type = EProofCommandType::ReturnCargo;
        Command.UnitId = UnitId;
        Command.TargetUnitId = RefineryId;
        Commands.Add(Command);
    }
    return Commands;
}
