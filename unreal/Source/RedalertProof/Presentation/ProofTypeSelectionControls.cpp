#include "Presentation/ProofRTSPawn.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Opt-in observation only: real projected records, shared local handler, no RPC.
bool AProofRTSPawn::LogTypeSelectionFrame(const TCHAR* Phase, const FString& ActionBefore)
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS || ++TypeSelectionRows > 1024) return ControlsCheck(false, TEXT("type_frame_context_or_cap"));
    const FProofSnapshot& Snapshot = GS->Snapshot;
    int32 W = 0, H = 0; PC->GetViewportSize(W, H);
    TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("phase"), Phase);
    Row->SetNumberField(TEXT("epoch"), Snapshot.MatchEpoch);
    Row->SetNumberField(TEXT("revision"), Snapshot.Revision);
    Row->SetStringField(TEXT("digest"), Snapshot.ComputeDigest());
    Row->SetStringField(TEXT("actionBeforeDigest"), ActionBefore);
    Row->SetNumberField(TEXT("owner"), PC->OwnerId);
    Row->SetNumberField(TEXT("width"), W); Row->SetNumberField(TEXT("height"), H);
    Row->SetNumberField(TEXT("primary"), Selection.GetPrimaryUnitId());
    Row->SetNumberField(TEXT("nativeT"), ManualTypeSelectPresses);
    Row->SetNumberField(TEXT("sequence"), LastRequestedSequence);
    Row->SetNumberField(TEXT("queue"), GroupOrders.NumPending());
    TArray<TSharedPtr<FJsonValue>> Selected, Expected, Units, Group;
    for (int32 Id : Selection.GetSelectedUnitIds()) Selected.Add(MakeShared<FJsonValueNumber>(Id));
    for (int32 Id : TypeSelectionExpected) Expected.Add(MakeShared<FJsonValueNumber>(Id));
    FProofSelectionState Probe = Selection;
    Probe.RecallGroup(9, Snapshot, PC->OwnerId);
    for (int32 Id : Probe.GetSelectedUnitIds()) Group.Add(MakeShared<FJsonValueNumber>(Id));
    Row->SetArrayField(TEXT("selected"), Selected); Row->SetArrayField(TEXT("expected"), Expected);
    Row->SetArrayField(TEXT("group9"), Group); Row->SetNumberField(TEXT("groupPrimary"), Probe.GetPrimaryUnitId());
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (!TypeSelectionExpected.Contains(Unit.UnitId)) continue;
        FVector2D Center;
        if (!PC->ProjectWorldLocationToScreen(Unit.Position, Center) || (!bMapTypeSelectionControls && !FProofSelectionState::IsViewportCenterVisible(Center, FIntPoint(W,H))))
            return ControlsCheck(false, TEXT("type_expected_projection_visible"));
        if (Unit.OwnerId != PC->OwnerId || Unit.Kind != EProofUnitKind::Combat || Unit.HitPoints <= 0.f || !FMath::IsFinite(Unit.HitPoints)
            || !Unit.Position.Equals(TypeSelectionPositions.FindChecked(Unit.UnitId), 0.0001))
            return ControlsCheck(false, TEXT("type_expected_owned_live_stationary"));
        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetNumberField(TEXT("id"), Unit.UnitId); Item->SetNumberField(TEXT("owner"), Unit.OwnerId);
        Item->SetNumberField(TEXT("kind"), static_cast<int32>(Unit.Kind)); Item->SetNumberField(TEXT("hp"), Unit.HitPoints);
        Item->SetNumberField(TEXT("cargo"), Unit.Cargo);
        Item->SetNumberField(TEXT("x"), Unit.Position.X); Item->SetNumberField(TEXT("y"), Unit.Position.Y); Item->SetNumberField(TEXT("z"), Unit.Position.Z);
        Item->SetNumberField(TEXT("screenX"), Center.X); Item->SetNumberField(TEXT("screenY"), Center.Y);
        Units.Add(MakeShared<FJsonValueObject>(Item));
    }
    Row->SetArrayField(TEXT("units"), Units);
    FString Json; const TSharedRef<TJsonWriter<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    FJsonSerializer::Serialize(Row, Writer);
    UE_LOG(LogTemp, Display, TEXT("PROOF_TYPE_SELECTION_FRAME %s"), *Json);
    return true;
}

void AProofRTSPawn::TickTypeSelectionControls()
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS) { ControlsCheck(false, TEXT("type_context")); return; }
    const FProofSnapshot& Snapshot = GS->Snapshot;
    if (TypeSelectionStage == 0)
    {
        int32 W=0,H=0; PC->GetViewportSize(W,H);
        const FProofUnitRecord* Anchor = Snapshot.Units.FindByPredicate([this](const FProofUnitRecord& U){return U.UnitId==ControlsOwnUnit;});
        if (!ControlsCheck(Anchor && Anchor->OwnerId==PC->OwnerId && Anchor->HitPoints>0.f && Anchor->Kind==EProofUnitKind::Combat, TEXT("type_anchor_live_owned"))) return;
        for (const FProofUnitRecord& Unit : Snapshot.Units)
        {
            FVector2D Center;
            if (Unit.OwnerId==PC->OwnerId && Unit.HitPoints>0.f && Unit.Kind==Anchor->Kind && PC->ProjectWorldLocationToScreen(Unit.Position,Center)
                && FProofSelectionState::IsViewportCenterVisible(Center,FIntPoint(W,H)))
            { TypeSelectionExpected.Add(Unit.UnitId); TypeSelectionPositions.Add(Unit.UnitId,Unit.Position); }
        }
        TypeSelectionExpected.Sort();
        if (!ControlsCheck(TypeSelectionExpected.Num()==2 && TypeSelectionExpected.Contains(ControlsOwnUnit), TEXT("type_two_real_visible_candidates"))) return;
        if (!ControlsCheck(SetControlsCursorFromWorld(Anchor->Position) && FindUnitUnderCursor(true)==ControlsOwnUnit, TEXT("type_anchor_actual_pick"))) return;
        SelectUnderCursor(); Selection.AssignGroup(9,Snapshot,PC->OwnerId);
        if (!LogTypeSelectionFrame(TEXT("before"))) return;
        CaptureControls(TEXT("03-type-before")); TypeSelectionStarted=ControlsElapsed; TypeSelectionStage=1;
        return;
    }
    if (TypeSelectionStage==1)
    {
        if (ControlsElapsed-TypeSelectionStarted<0.25f || IFileManager::Get().FileSize(*FPaths::Combine(ControlsOutput,TEXT("03-type-before.png")))<=1024) return;
        const FString Before=Snapshot.ComputeDigest();
        const int32 NativeBefore=ManualTypeSelectPresses, SequenceBefore=LastRequestedSequence, QueueBefore=GroupOrders.NumPending();
        SelectVisibleType();
        if (!ControlsCheck(Selection.GetSelectedUnitIds()==TypeSelectionExpected && Selection.GetPrimaryUnitId()==ControlsOwnUnit
            && Before==Snapshot.ComputeDigest() && NativeBefore==ManualTypeSelectPresses && SequenceBefore==LastRequestedSequence && QueueBefore==GroupOrders.NumPending(), TEXT("type_shared_handler_exact_selection_no_command"))) return;
        if (!LogTypeSelectionFrame(TEXT("after"),Before)) return;
        CaptureControls(TEXT("04-type-after")); TypeSelectionStarted=ControlsElapsed; TypeSelectionStage=2; return;
    }
    if (Selection.GetSelectedUnitIds()!=TypeSelectionExpected || Selection.GetPrimaryUnitId()!=ControlsOwnUnit)
    { ControlsCheck(false,TEXT("type_selection_stable")); return; }
    if (Snapshot.Revision!=TypeSelectionLastRevision)
    {
        TypeSelectionLastRevision=Snapshot.Revision;
        if (!LogTypeSelectionFrame(TEXT("observed"))) return;
    }
    if (ControlsElapsed-TypeSelectionStarted<1.6f || IFileManager::Get().FileSize(*FPaths::Combine(ControlsOutput,TEXT("04-type-after.png")))<=1024) return;
    UE_LOG(LogTemp,Display,TEXT("PROOF_TYPE_SELECTION_COMPLETE status=pass inputSource=sharedHandler nativeCallbackInvoked=0 physicalInputVerified=0"));
    FinishControls(true,TEXT("baseline_and_type_shared_handler_observed"));
}

// Opt-in synthetic shared-handler proof, not an injected/native or physical key.
void AProofRTSPawn::TickMapTypeSelectionControls()
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const FProofSnapshot& Snapshot = GetWorld()->GetGameState<AProofGameState>()->Snapshot;
    if (TypeSelectionStage == 0)
    {
        const FProofUnitRecord* Anchor = Snapshot.Units.FindByPredicate([this](const FProofUnitRecord& Unit){return Unit.UnitId==ControlsOwnUnit;});
        if (!ControlsCheck(Anchor && Anchor->OwnerId==PC->OwnerId && Anchor->HitPoints>0.f, TEXT("map_type_live_anchor"))) return;
        TypeSelectionExpected.Reset(); TypeSelectionPositions.Reset();
        for (const FProofUnitRecord& Unit : Snapshot.Units)
            if (Unit.OwnerId==PC->OwnerId && Unit.Kind==Anchor->Kind && Unit.HitPoints>0.f)
            { TypeSelectionExpected.Add(Unit.UnitId); TypeSelectionPositions.Add(Unit.UnitId,Unit.Position); }
        TypeSelectionExpected.Sort();
        if (!ControlsCheck(TypeSelectionExpected.Num()==2, TEXT("map_type_two_real_owned_candidates"))) return;
        if (!bMapTypeCameraPrepared)
        {
            Camera->SetOrthoWidth(200.f);
            // Automatic ortho planes use the owning pawn as their view target.
            // Move the rig together, as in the existing readability fixture,
            // while keeping snapshot-backed rendered units at their world positions.
            TMap<int32,FTransform> ViewTransforms;
            for(const auto& Entry:UnitViews)
                if(Entry.Value) ViewTransforms.Add(Entry.Key,Entry.Value->GetComponentTransform());
            if(!ControlsCheck(SetActorLocation(Anchor->Position),TEXT("map_type_camera_rig_move"))) return;
            for(const auto& Entry:ViewTransforms)
                if(UnitViews.Contains(Entry.Key) && UnitViews[Entry.Key]) UnitViews[Entry.Key]->SetWorldTransform(Entry.Value);
            bMapTypeCameraPrepared=true;TypeSelectionStarted=ControlsElapsed;return;
        }
        if (ControlsElapsed-TypeSelectionStarted<0.25f) return;
        int32 W=0,H=0;PC->GetViewportSize(W,H);int32 Visible=0;
        for (int32 Id : TypeSelectionExpected)
        {
            FVector2D Screen;
            const FVector& Position=TypeSelectionPositions.FindChecked(Id);
            const TObjectPtr<UStaticMeshComponent>* View=UnitViews.Find(Id);
            const FVector ExpectedWorld=Position+(bUsingBlockoutMesh?FVector::ZeroVector:FVector(0.f,0.f,30.f));
            if(!ControlsCheck(View && *View && (*View)->GetComponentLocation().Equals(ExpectedWorld,0.01),TEXT("map_type_rendered_units_stationary"))) return;
            if (PC->ProjectWorldLocationToScreen(Position,Screen) && FProofSelectionState::IsViewportCenterVisible(Screen,FIntPoint(W,H))) ++Visible;
        }
        UE_LOG(LogTemp,Display,TEXT("PROOF_MAP_TYPE_CAMERA pawn=%s camera=%s ortho=%.1f viewport=%dx%d visible=%d expected=%d"),
            *GetActorLocation().ToString(),*Camera->GetComponentLocation().ToString(),Camera->OrthoWidth,W,H,Visible,TypeSelectionExpected.Num());
        if (!ControlsCheck(Visible==1,TEXT("map_type_actual_visible_and_offscreen"))) return;
        if (!ControlsCheck(SetControlsCursorFromWorld(Anchor->Position),TEXT("map_type_anchor_cursor_in_viewport"))) return;
        if (!ControlsCheck(FindUnitUnderCursor(true)==ControlsOwnUnit,TEXT("map_type_anchor_cursor_pick"))) return;
        SelectUnderCursor();Selection.AssignGroup(9,Snapshot,PC->OwnerId);TypeGesture.Cancel();
        if (!LogTypeSelectionFrame(TEXT("before"))) return;
        CaptureControls(TEXT("03-type-before"));TypeSelectionStarted=ControlsElapsed;TypeSelectionStage=1;return;
    }
    if (TypeSelectionStage==1)
    {
        if (ControlsElapsed-TypeSelectionStarted<0.25f || IFileManager::Get().FileSize(*FPaths::Combine(ControlsOutput,TEXT("03-type-before.png")))<=1024) return;
        const FString Before=Snapshot.ComputeDigest();
        const int32 Native=ManualTypeSelectPresses,Sequence=LastRequestedSequence,Queue=GroupOrders.NumPending();
        SelectTypeByGesture(FPlatformTime::Seconds(),true);OnManualTypeRelease();
        if (!ControlsCheck(Selection.GetSelectedUnitIds()==TArray<int32>({ControlsOwnUnit}), TEXT("map_type_first_visible_only"))) return;
        if (!LogTypeSelectionFrame(TEXT("visible"),Before)) return;
        SelectTypeByGesture(FPlatformTime::Seconds(),true);OnManualTypeRelease();
        if (!ControlsCheck(Selection.GetSelectedUnitIds()==TypeSelectionExpected && Selection.GetPrimaryUnitId()==ControlsOwnUnit
            && Before==Snapshot.ComputeDigest() && Native==ManualTypeSelectPresses && Sequence==LastRequestedSequence && Queue==GroupOrders.NumPending(), TEXT("map_type_pair_exact_no_command"))) return;
        if (!LogTypeSelectionFrame(TEXT("after"),Before)) return;
        CaptureControls(TEXT("04-type-after"));TypeSelectionStarted=ControlsElapsed;TypeSelectionStage=2;return;
    }
    TickTypeSelectionControls(); // Existing stationary multi-revision observation and completion.
}
