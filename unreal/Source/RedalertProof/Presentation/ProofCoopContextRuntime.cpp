#include "Presentation/ProofRTSPawn.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

bool AProofRTSPawn::CoopContextCheck(bool bCondition, const TCHAR* Reason)
{
    if (!bCondition)
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_COOP_CONTEXT_ERROR reason=%s"), Reason);
        FinishSkirmishScenario(false, Reason);
    }
    return bCondition;
}

bool AProofRTSPawn::ProjectCoopContextTarget(int32 TargetId, FVector& Ground)
{
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    const FProofUnitRecord* Target = GS ? GS->Snapshot.Units.FindByPredicate(
        [TargetId](const FProofUnitRecord& U) { return U.UnitId == TargetId && U.HitPoints > 0.f; }) : nullptr;
    return CoopContextCheck(Target && SetControlsCursorFromWorld(Target->Position)
        && FindUnitUnderCursor(false) == TargetId && CursorGroundPoint(Ground)
        && FVector::Distance(Ground, Target->Position) < 2.f, TEXT("context_target_pick_or_ground"));
}

void AProofRTSPawn::LogCoopContextPhase(const TCHAR* Phase, int32 BeforeSequence,
    int32 Unit, int32 Secondary, int32 Target, const FVector& Destination) const
{
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    UE_LOG(LogTemp, Display, TEXT("PROOF_COOP_CONTEXT_PHASE {\"phase\":\"%s\",\"epoch\":%d,\"revision\":%d,\"beforeSequence\":%d,\"sequence\":%d,\"unit\":%d,\"secondary\":%d,\"target\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"nativeRight\":%d,\"elapsed\":%.6f}"),
        Phase, GS->Snapshot.MatchEpoch, GS->Snapshot.Revision, BeforeSequence, LastRequestedSequence,
        Unit, Secondary, Target, Destination.X, Destination.Y, Destination.Z, ManualRightPresses, SkirmishElapsed);
}

bool AProofRTSPawn::PrepareCoopContextFactoryClick()
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    const FProofSnapshot& S = GS->Snapshot;
    const FProofUnitRecord* Ally = S.Units.FindByPredicate([](const FProofUnitRecord& U)
        { return U.OwnerId == 2 && U.Kind == EProofUnitKind::HQ && U.HitPoints > 0.f; });
    const FProofUnitRecord* Enemy = S.Units.FindByPredicate([&S, PC](const FProofUnitRecord& U)
        { return U.Kind == EProofUnitKind::HQ && FProofSelectionState::IsHostileTarget(S, PC->OwnerId, U.UnitId); });
    if (!CoopContextCheck(PC->OwnerId == 1 && Ally && Enemy && ManualRightPresses == 0
        && GroupOrders.NumPending() == 0, TEXT("context_factory_setup"))) return false;
    CoopContext.Ally = Ally->UnitId;
    CoopContext.Enemy = Enemy->UnitId;
    FVector Ground;
    if (!ProjectCoopContextTarget(Enemy->UnitId, Ground)) return false;
    const int32 Before = LastRequestedSequence;
    const FString BeforeDigest = S.ComputeDigest();
    OrderUnderCursor();
    if (!CoopContextCheck(LastRequestedSequence == Before && GroupOrders.NumPending() == 0
        && S.ComputeDigest() == BeforeDigest && SelectedUnitId == FactoryRallyFactory
        && Selection.GetSelectedUnitIds().Num() == 1
        && RequestStatus == TEXT("Factory rally requires ground; right-click away from enemy units."),
        TEXT("context_hostile_factory_submitted"))) return false;
    LogCoopContextPhase(TEXT("factory_hostile_rejected"), Before, FactoryRallyFactory, INDEX_NONE, Enemy->UnitId, Ground);
    if (!ProjectCoopContextTarget(Ally->UnitId, Ground)) return false;
    FactoryRallyDestination = Ground;
    OrderUnderCursor();
    if (!CoopContextCheck(LastRequestedSequence == Before + 1 && LastRequestedType == TEXT("SetRally")
        && LastRequestedUnitId == FactoryRallyFactory, TEXT("context_allied_factory_not_rallied"))) return false;
    LogCoopContextPhase(TEXT("factory_allied_rally"), Before, FactoryRallyFactory, INDEX_NONE, Ally->UnitId, Ground);
    return true;
}

bool AProofRTSPawn::TickCoopContextFixture()
{
    if (CoopContext.Stage == 8) return true;
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!CoopContextCheck(PC && GS && PC->OwnerId == 1 && SkirmishScenario == TEXT("Coop")
        && GS->Snapshot.MatchPhase == EProofMatchPhase::Playing && ManualRightPresses == 0,
        TEXT("context_fixture_context"))) return false;
    const FProofSnapshot& S = GS->Snapshot;
    const auto Find = [&S](int32 Id) { return S.Units.FindByPredicate(
        [Id](const FProofUnitRecord& U) { return U.UnitId == Id && U.HitPoints > 0.f; }); };
    const auto Accepted = [this, PC](int32 First, int32 Last)
    {
        for (int32 Sequence = First; Sequence <= Last; ++Sequence)
        {
            FProofCommandResult R;
            if (!PC->FindReceipt(Sequence, R)) return false;
            if (!CoopContextCheck(R.bAccepted, TEXT("context_command_rejected"))) return false;
        }
        return true;
    };
    const auto SelectCombat = [this, PC, &Find]()
    {
        const FProofUnitRecord* Combat = Find(FactoryRallyUnit);
        if (!CoopContextCheck(Combat && Combat->OwnerId == PC->OwnerId
            && Combat->Kind == EProofUnitKind::Combat && SetControlsCursorFromWorld(Combat->Position),
            TEXT("context_combat_missing"))) return false;
        SelectUnderCursor();
        return CoopContextCheck(SelectedUnitId == Combat->UnitId && Selection.GetSelectedUnitIds().Num() == 1,
            TEXT("context_combat_pick"));
    };
    const auto BeginMovement = [this, &S]()
    {
        CoopContext.ActionAt = SkirmishElapsed;
        CoopContext.ActionRevision = S.Revision;
        CoopContext.ObservedRevisions.Reset();
        CoopContext.Starts.Reset();
        for (const auto& Pair : CoopContext.Destinations)
        {
            const FProofUnitRecord* U = S.Units.FindByPredicate([&Pair](const FProofUnitRecord& V) { return V.UnitId == Pair.Key; });
            CoopContext.Starts.Add(Pair.Key, U->Position);
        }
    };
    const auto Moved = [this, &S, &Find]()
    {
        if (S.Revision > CoopContext.ActionRevision) CoopContext.ObservedRevisions.Add(S.Revision);
        if (SkirmishElapsed - CoopContext.ActionAt < 1.f || CoopContext.ObservedRevisions.Num() < 2) return false;
        for (const auto& Pair : CoopContext.Destinations)
        {
            const FProofUnitRecord* U = Find(Pair.Key);
            if (!CoopContextCheck(U != nullptr, TEXT("context_mover_died"))) return false;
            const FVector Start = CoopContext.Starts[Pair.Key];
            if (FVector::Distance(U->Position, Start) <= 75.f
                || FVector::Distance(U->Position, Pair.Value) >= FVector::Distance(Start, Pair.Value)) return false;
        }
        return true;
    };
    if (CoopContext.Stage == 0)
    {
        if (!SelectCombat()) return false;
        CoopContext.Combat = FactoryRallyUnit;
        const FProofUnitRecord* Harvester = S.Units.FindByPredicate([PC](const FProofUnitRecord& U)
            { return U.OwnerId == PC->OwnerId && U.Kind == EProofUnitKind::Harvester && U.HitPoints > 0.f; });
        if (!CoopContextCheck(Harvester != nullptr, TEXT("context_harvester_missing"))) return false;
        CoopContext.Harvester = Harvester->UnitId;
        if (!ProjectCoopContextTarget(CoopContext.Ally, CoopContext.Destination)) return false;
        CoopContext.BeforeSequence = LastRequestedSequence;
        OrderUnderCursor();
        if (!CoopContextCheck(LastRequestedSequence == CoopContext.BeforeSequence + 1
            && LastRequestedType == TEXT("Move") && LastRequestedUnitId == CoopContext.Combat,
            TEXT("context_single_ally_route"))) return false;
        CoopContext.Destinations.Add(CoopContext.Combat, CoopContext.Destination);
        BeginMovement();
        LogCoopContextPhase(TEXT("single_ally_issued"), CoopContext.BeforeSequence, CoopContext.Combat,
            INDEX_NONE, CoopContext.Ally, CoopContext.Destination);
        CoopContext.Stage = 1;
        return false;
    }
    if (CoopContext.Stage == 1)
    {
        if (!Accepted(CoopContext.BeforeSequence + 1, CoopContext.BeforeSequence + 1) || !Moved()) return false;
        LogCoopContextPhase(TEXT("single_ally_moved"), CoopContext.BeforeSequence, CoopContext.Combat,
            INDEX_NONE, CoopContext.Ally, CoopContext.Destination);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(SkirmishOutput, TEXT("04-context-single.png")), true, false);
        CoopContext.Stage = 2;
        return false;
    }
    if (CoopContext.Stage == 2)
    {
        if (!SelectCombat() || !CoopContextCheck(Find(CoopContext.Harvester) != nullptr, TEXT("context_group_harvester"))) return false;
        Selection.Toggle(S, PC->OwnerId, CoopContext.Harvester);
        SelectedUnitId = Selection.GetPrimaryUnitId();
        if (!CoopContextCheck(Selection.GetSelectedUnitIds().Num() == 2 && GroupOrders.NumPending() == 0,
            TEXT("context_group_selection"))) return false;
        if (!ProjectCoopContextTarget(CoopContext.Ally, CoopContext.Destination)) return false;
        const TArray<FProofCommand> Planned = Selection.PlanMove(S, PC->OwnerId, CoopContext.Destination);
        if (!CoopContextCheck(Planned.Num() == 2, TEXT("context_formation_plan"))) return false;
        CoopContext.Destinations.Reset();
        for (const FProofCommand& C : Planned) CoopContext.Destinations.Add(C.UnitId, C.Destination);
        CoopContext.BeforeSequence = LastRequestedSequence;
        OrderUnderCursor();
        if (!CoopContextCheck(LastRequestedSequence == CoopContext.BeforeSequence && GroupOrders.NumPending() == 2,
            TEXT("context_group_ally_route"))) return false;
        BeginMovement();
        LogCoopContextPhase(TEXT("group_ally_issued"), CoopContext.BeforeSequence, CoopContext.Combat,
            CoopContext.Harvester, CoopContext.Ally, CoopContext.Destination);
        CoopContext.Stage = 3;
        return false;
    }
    if (CoopContext.Stage == 3)
    {
        if (!CoopContextCheck(LastRequestedSequence <= CoopContext.BeforeSequence + 2, TEXT("context_group_extra_command"))) return false;
        if (GroupOrders.NumPending() != 0 || LastRequestedSequence != CoopContext.BeforeSequence + 2
            || !Accepted(CoopContext.BeforeSequence + 1, CoopContext.BeforeSequence + 2) || !Moved()) return false;
        LogCoopContextPhase(TEXT("group_ally_moved"), CoopContext.BeforeSequence, CoopContext.Combat,
            CoopContext.Harvester, CoopContext.Ally, CoopContext.Destination);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(SkirmishOutput, TEXT("05-context-group.png")), true, false);
        CoopContext.Stage = 4;
        return false;
    }
    if (CoopContext.Stage == 4)
    {
        if (!SelectCombat() || !ProjectCoopContextTarget(CoopContext.Enemy, CoopContext.Destination)) return false;
        CoopContext.BeforeSequence = LastRequestedSequence;
        OrderUnderCursor();
        if (!CoopContextCheck(LastRequestedSequence == CoopContext.BeforeSequence + 1 && LastRequestedType == TEXT("Attack")
            && LastRequestedTarget == CoopContext.Enemy, TEXT("context_single_enemy_route"))) return false;
        LogCoopContextPhase(TEXT("single_enemy_issued"), CoopContext.BeforeSequence, CoopContext.Combat,
            INDEX_NONE, CoopContext.Enemy, CoopContext.Destination);
        CoopContext.Stage = 5;
        return false;
    }
    if (CoopContext.Stage == 5)
    {
        if (!Accepted(CoopContext.BeforeSequence + 1, CoopContext.BeforeSequence + 1)) return false;
        if (!SelectCombat() || !CoopContextCheck(Find(CoopContext.Harvester) != nullptr, TEXT("context_enemy_group_harvester"))) return false;
        Selection.Toggle(S, PC->OwnerId, CoopContext.Harvester);
        SelectedUnitId = Selection.GetPrimaryUnitId();
        if (!ProjectCoopContextTarget(CoopContext.Enemy, CoopContext.Destination)) return false;
        CoopContext.BeforeSequence = LastRequestedSequence;
        OrderUnderCursor();
        if (!CoopContextCheck(Selection.GetSelectedUnitIds().Num() == 2
            && LastRequestedSequence == CoopContext.BeforeSequence && GroupOrders.NumPending() == 1,
            TEXT("context_group_enemy_route"))) return false;
        LogCoopContextPhase(TEXT("group_enemy_issued"), CoopContext.BeforeSequence, CoopContext.Combat,
            CoopContext.Harvester, CoopContext.Enemy, CoopContext.Destination);
        CoopContext.Stage = 6;
        return false;
    }
    if (CoopContext.Stage == 6)
    {
        if (!CoopContextCheck(LastRequestedSequence <= CoopContext.BeforeSequence + 1, TEXT("context_enemy_extra_command"))) return false;
        if (GroupOrders.NumPending() != 0 || LastRequestedSequence != CoopContext.BeforeSequence + 1
            || !Accepted(CoopContext.BeforeSequence + 1, CoopContext.BeforeSequence + 1)) return false;
        const FProofUnitRecord* H = Find(CoopContext.Harvester);
        if (!CoopContextCheck(H && SetControlsCursorFromWorld(H->Position), TEXT("context_resource_harvester"))) return false;
        SelectUnderCursor();
        if (!CoopContextCheck(SelectedUnitId == H->UnitId && Selection.GetSelectedUnitIds().Num() == 1, TEXT("context_resource_selection"))) return false;
        const FProofResourceRecord* Resource = S.Resources.FindByPredicate([PC](const FProofResourceRecord& R)
            { return R.OwnerId == PC->OwnerId && R.Remaining > 0; });
        if (!CoopContextCheck(Resource && SetControlsCursorFromWorld(Resource->Position)
            && CursorGroundPoint(CoopContext.Destination), TEXT("context_resource_projection"))) return false;
        CoopContext.Resource = Resource->ResourceId;
        CoopContext.BeforeSequence = LastRequestedSequence;
        OrderUnderCursor();
        if (!CoopContextCheck(LastRequestedSequence == CoopContext.BeforeSequence + 1 && LastRequestedType == TEXT("Gather")
            && LastRequestedTarget == CoopContext.Resource && LastRequestedUnitId == H->UnitId,
            TEXT("context_resource_route"))) return false;
        LogCoopContextPhase(TEXT("resource_issued"), CoopContext.BeforeSequence, CoopContext.Harvester,
            INDEX_NONE, CoopContext.Resource, CoopContext.Destination);
        CoopContext.ActionAt = SkirmishElapsed;
        CoopContext.Stage = 7;
        return false;
    }
    if (!Accepted(CoopContext.BeforeSequence + 1, CoopContext.BeforeSequence + 1)
        || SkirmishElapsed - CoopContext.ActionAt < 0.5f) return false;
    LogCoopContextPhase(TEXT("context_complete"), CoopContext.BeforeSequence, CoopContext.Harvester,
        INDEX_NONE, CoopContext.Resource, CoopContext.Destination);
    CoopContext.Stage = 8;
    return true;
}
