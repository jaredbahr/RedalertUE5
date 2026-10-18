#include "Presentation/ProofTypeSelectionAdapter.h"

namespace
{
const FProofUnitRecord* AnchorRecord(const FProofSnapshot& Snapshot, int32 Id, int32 Owner)
{
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& Record) { return Record.UnitId == Id; });
    return Id > 0 && Owner >= 0 && Unit && Unit->OwnerId == Owner && Unit->HitPoints > 0.f
        && FMath::IsFinite(Unit->HitPoints) && !Unit->Position.ContainsNaN()
        && FMath::Abs(Unit->Position.X) <= FProofCommandModel::WorldExtent
        && FMath::Abs(Unit->Position.Y) <= FProofCommandModel::WorldExtent
        && FMath::Abs(Unit->Position.Z) <= FProofCommandModel::WorldExtent ? Unit : nullptr;
}
}

ProofTypeSelectGesture::Scope ProofTypeSelectionAdapter::Apply(FProofSelectionState& Selection,
    ProofTypeSelectGesture::State& Gesture, const FProofSnapshot& Snapshot, int32 OwnerId,
    double WallSeconds, bool Focused, TFunctionRef<bool()> VisibleAction)
{
    using ProofTypeSelectGesture::Scope;
    if (!FMath::IsFinite(WallSeconds) || WallSeconds < 0.0) { Gesture.Cancel(); return Scope::Ignored; }
    // Capture before pruning can promote a secondary unit of a different kind.
    const int32 VisibleAnchorId = Selection.GetPrimaryUnitId();
    const bool WasArmed = Gesture.IsArmed();
    const int32 PendingAnchorId = WasArmed ? Gesture.Expected().Anchor : VisibleAnchorId;
    const FProofUnitRecord* VisibleAnchor = AnchorRecord(Snapshot, VisibleAnchorId, OwnerId);
    const bool CurrentContext = Selection.HasCurrentContext(Snapshot, OwnerId);
    Selection.Synchronize(Snapshot, OwnerId);
    // An external selection/context change retires the old pair before its
    // original anchor can constrain a fresh visible action.
    if (Gesture.IsArmed() && (Gesture.Expected().Owner != OwnerId || Gesture.Expected().Epoch != Snapshot.MatchEpoch
        || Gesture.Expected().SelectionRevision != Selection.GetSelectionRevision() || Gesture.Expected().Mode != (Snapshot.bSkirmish ? 1 : 0))) Gesture.Cancel();
    const bool CurrentPair = Gesture.IsArmed();
    const int32 AnchorId = CurrentPair ? PendingAnchorId : VisibleAnchorId;
    const FProofUnitRecord* Anchor = AnchorRecord(Snapshot, AnchorId, OwnerId);
    // Empty is allowed only for the successful internal visible pass of the
    // still-current pair. A stale pre-sync primary never promotes a new kind.
    if (!CurrentContext || !Anchor || (!VisibleAnchor && !(CurrentPair && VisibleAnchorId == INDEX_NONE)))
    { Gesture.Cancel(); return Scope::Ignored; }
    const bool MapContext = Anchor && Snapshot.MatchEpoch >= 0 && Selection.GetSelectionRevision() < MAX_uint64
        && Snapshot.Players.ContainsByPredicate([OwnerId](const FProofPlayerRecord& Player) { return Player.OwnerId == OwnerId; })
        && (!Snapshot.bSkirmish || (Snapshot.MatchEpoch > 0 && Snapshot.MatchPhase == EProofMatchPhase::Playing));
    if (!MapContext && Focused)
    {
        Gesture.Cancel();
        return VisibleAction() ? Scope::Visible : Scope::Ignored; // Existing first-pass behavior, never arm expansion.
    }
    ProofTypeSelectGesture::Context Context;
    Context.Owner = OwnerId; Context.Epoch = Snapshot.MatchEpoch; Context.Anchor = AnchorId;
    Context.Kind = Anchor ? static_cast<int32>(Anchor->Kind) : INDEX_NONE;
    Context.Mode = Snapshot.bSkirmish ? 1 : 0; Context.SelectionRevision = Selection.GetSelectionRevision();
    const Scope Action = Gesture.Press(WallSeconds, Context, MapContext, Focused);
    if (Action == Scope::Battlefield)
    {
        if (!Selection.SelectBattlefieldType(Snapshot, OwnerId, AnchorId)) { Gesture.Cancel(); return Scope::Ignored; }
    }
    else if (Action == Scope::Visible)
    {
        if (!VisibleAnchor || !VisibleAction()) { Gesture.Cancel(); return Scope::Ignored; }
        Context.Anchor = VisibleAnchorId; Context.Kind = static_cast<int32>(VisibleAnchor->Kind);
        Context.SelectionRevision = Selection.GetSelectionRevision();
        if (!Gesture.CommitVisible(WallSeconds, Context)) { Gesture.Cancel(); return Scope::Ignored; }
    }
    return Action;
}
