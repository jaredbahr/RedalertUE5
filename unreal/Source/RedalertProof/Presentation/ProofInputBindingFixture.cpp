#include "Presentation/ProofRTSPawn.h"
#include "Presentation/ProofCameraPan.h"
#include "Camera/CameraComponent.h"
#include "Runtime/ProofPlayerController.h"
#include "Runtime/ProofGameState.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "AssetCompilingManager.h"

void AProofRTSPawn::ApplyCameraPan(float DeltaSeconds, bool bFocused, bool bForeground)
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!PC || !bCameraPanEnabled || !bConfiguredLocalInput || !bFocused || !bForeground
        || PC->IsProofAutomation() || PC->IsControlsAutomation() || !SkirmishScenario.IsEmpty()) return;
    const FVector2D Direction(
        (PC->IsInputKeyDown(EKeys::Right) ? 1.0 : 0.0) - (PC->IsInputKeyDown(EKeys::Left) ? 1.0 : 0.0),
        (PC->IsInputKeyDown(EKeys::Up) ? 1.0 : 0.0) - (PC->IsInputKeyDown(EKeys::Down) ? 1.0 : 0.0));
    CameraGroundCenter = FProofCameraPan::Apply(CameraGroundCenter, Direction, DeltaSeconds);
    Camera->SetWorldLocation(CameraGroundCenter + FVector(0.f, -3200.f, 3200.f));
}

void AProofRTSPawn::QueueInputFixtureClick(FKey Key, FKey Modifier)
{
    if (Modifier.IsValid()) InputFixtureEdges.Emplace(Modifier, true);
    InputFixtureEdges.Emplace(Key, true);
    InputFixtureEdges.Emplace(Key, false);
    if (Modifier.IsValid()) InputFixtureEdges.Emplace(Modifier, false);
}

void AProofRTSPawn::AdvanceInputFixture(int32 Stage)
{
    InputFixtureStage = Stage;
    InputFixtureStageStart = FPlatformTime::Seconds();
    UE_LOG(LogTemp, Display, TEXT("PROOF_INPUT_BINDING_STAGE stage=%d inputSource=engineInjection"), Stage);
}

bool AProofRTSPawn::InputFixtureCheck(bool bCondition, const TCHAR* Evidence)
{
    if (bCondition && InputFixtureChecks.Contains(Evidence)) return true;
    UE_LOG(LogTemp, Display, TEXT("PROOF_INPUT_BINDING_CHECK stage=%d status=%s evidence=%s inputSource=engineInjection physicalInputVerified=false"),
        InputFixtureStage, bCondition ? TEXT("pass") : TEXT("fail"), Evidence);
    if (bCondition) InputFixtureChecks.AddUnique(Evidence);
    else FinishInputBindingFixture(false, Evidence);
    return bCondition;
}

void AProofRTSPawn::CaptureInputFixture(const TCHAR* Name)
{
    IFileManager::Get().MakeDirectory(*InputFixtureOutput, true);
    FScreenshotRequest::RequestScreenshot(FPaths::Combine(InputFixtureOutput, FString(Name) + TEXT(".png")), true, false);
    UE_LOG(LogTemp, Display, TEXT("PROOF_INPUT_BINDING_CAPTURE name=%s inputSource=engineInjection pointerSource=projectedSynthetic"), Name);
}

void AProofRTSPawn::FinishInputBindingFixture(bool bPass, const TCHAR* Reason)
{
    if (bInputFixtureFinished) return;
    bInputFixtureFinished = true;
    bInputFixtureVirtualPan = false;
    // Release through the same engine route; never leave held synthetic modifiers.
    UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
    if (ViewportClient && ViewportClient->Viewport) ViewportClient->LostFocus(ViewportClient->Viewport);
    FString Checks;
    for (const FString& Check : InputFixtureChecks)
    {
        if (!Checks.IsEmpty()) Checks += TEXT(",");
        Checks += FString::Printf(TEXT("\"%s\""), *Check);
    }
    const FString Result = FString::Printf(TEXT("{\n\"status\":\"%s\",\"reason\":\"%s\",\"stage\":%d,\n")
        TEXT("\"inputSource\":\"engineInjection\",\"pointerSource\":\"projectedSynthetic\",\n")
        TEXT("\"physicalInputVerified\":false,\"osWindowFocusVerified\":false,\"positiveRealFocusPanVerified\":false,\n")
        TEXT("\"positivePanFocusSource\":\"%s\",\"simulatedLostFocus\":%s,\"observedRealFocusSuppression\":%s,\n")
        TEXT("\"injectedEdges\":%d,\"inputDeviceId\":%d,\"platformUserId\":%d,\n")
        TEXT("\"nativeCallbacks\":{\"left\":%d,\"right\":%d,\"produce\":%d,\"build\":%d,\"groups\":%d,\"wheelUp\":%d,\"wheelDown\":%d,\"rematch\":%d},\n\"checks\":[%s]\n}\n"),
        bPass ? TEXT("pass") : TEXT("fail"), Reason, InputFixtureStage,
        bInputFixtureVirtualFocusUsed ? TEXT("virtualFixture") : TEXT("notExercised"),
        bInputFixtureSimulatedFocusLoss ? TEXT("true") : TEXT("false"), bInputFixtureRealFocusSuppressed ? TEXT("true") : TEXT("false"), InputFixtureEdgesSent, InputFixtureDeviceId, InputFixtureUserId,
        ManualLeftPresses, ManualRightPresses, ManualProducePresses, ManualBuildPresses, ManualGroupPresses,
        ManualWheelUpPresses, ManualWheelDownPresses, ManualRematchPresses, *Checks);
    IFileManager::Get().MakeDirectory(*InputFixtureOutput, true);
    const bool bWritten = FFileHelper::SaveStringToFile(Result, *FPaths::Combine(InputFixtureOutput, TEXT("client-evidence.json")));
    UE_LOG(LogTemp, Display, TEXT("PROOF_INPUT_BINDING_COMPLETE role=client status=%s reason=%s stage=%d written=%d inputSource=engineInjection pointerSource=projectedSynthetic physicalInputVerified=false osWindowFocusVerified=false positiveRealFocusPanVerified=false"),
        bPass && bWritten ? TEXT("pass") : TEXT("fail"), Reason, InputFixtureStage, bWritten);
    FPlatformMisc::RequestExitWithStatus(false, bPass && bWritten ? 0 : 1);
}

void AProofRTSPawn::TickInputBindingFixture(float DeltaSeconds)
{
    if (bInputFixtureFinished) return;
    const double Now = FPlatformTime::Seconds();
    if (InputFixtureStart == 0.0) { InputFixtureStart = Now; InputFixtureStageStart = Now; }
    if (Now - InputFixtureStart > 175.0) { FinishInputBindingFixture(false, TEXT("global_wall_timeout")); return; }
    if (Now - InputFixtureStageStart > 60.0) { FinishInputBindingFixture(false, TEXT("stage_wall_timeout")); return; }
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
    if (!PC || !GS || !ViewportClient || !ViewportClient->Viewport || !PC->GetLocalPlayer()) return;
    if (Now < InputFixtureNextEdge) return;
    if (!InputFixtureEdges.IsEmpty())
    {
        IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
        const FPlatformUserId User = PC->GetLocalPlayer()->GetPlatformUserId();
        const FInputDeviceId Device = Mapper.GetPrimaryInputDeviceForUser(User);
        if (!InputFixtureCheck(Device.IsValid() && Mapper.GetUserForInputDevice(Device) == User, TEXT("device_maps_to_local_user"))) return;
        InputFixtureDeviceId = Device.GetId(); InputFixtureUserId = User.GetInternalId();
        const TPair<FKey, bool> Edge = InputFixtureEdges[0];
        InputFixtureEdges.RemoveAt(0);
        const bool bHandled = ViewportClient->InputKey(FInputKeyEventArgs(ViewportClient->Viewport, Device, Edge.Key,
            Edge.Value ? IE_Pressed : IE_Released, FPlatformTime::Cycles64()));
        ++InputFixtureEdgesSent;
        UE_LOG(LogTemp, Display, TEXT("PROOF_INPUT_BINDING_EDGE index=%d key=%s pressed=%d handled=%d inputSource=engineInjection"),
            InputFixtureEdgesSent, *Edge.Key.ToString(), Edge.Value, bHandled);
        InputFixtureNextEdge = Now + 0.12; // Allow normal input-stack processing between edges.
        return;
    }
    const FProofSnapshot& Snapshot = GS->Snapshot;
    auto UnitById = [&Snapshot](int32 Id) -> const FProofUnitRecord*
    { return Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; }); };
    const FProofUnitRecord* HQ = nullptr; const FProofUnitRecord* Harvester = nullptr;
    const FProofUnitRecord* Refinery = nullptr; const FProofUnitRecord* Factory = nullptr; const FProofUnitRecord* EnemyHQ = nullptr;
    TArray<int32> Combat;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.HitPoints <= 0.f) continue;
        if (Unit.OwnerId == 100 && Unit.Kind == EProofUnitKind::HQ) EnemyHQ = &Unit;
        if (Unit.OwnerId != PC->OwnerId) continue;
        if (Unit.Kind == EProofUnitKind::HQ) HQ = &Unit;
        if (Unit.Kind == EProofUnitKind::Harvester) Harvester = &Unit;
        if (Unit.Kind == EProofUnitKind::Refinery) Refinery = &Unit;
        if (Unit.Kind == EProofUnitKind::Factory) Factory = &Unit;
        if (Unit.Kind == EProofUnitKind::Combat) Combat.Add(Unit.UnitId);
    }
    Combat.Sort();
    auto Point = [this](const FVector& Position) { return InputFixtureCheck(SetControlsCursorFromWorld(Position), TEXT("synthetic_pointer_projected")); };
    auto Selected = [this](TArray<int32> Wanted) { Wanted.Sort(); return Selection.GetSelectedUnitIds() == Wanted; };
    auto Receipts = [this, PC](int32 Count)
    {
        if (LastRequestedSequence < InputFixtureSequenceStart + Count || GroupOrders.NumPending() != 0) return false;
        for (int32 Sequence = InputFixtureSequenceStart + 1; Sequence <= InputFixtureSequenceStart + Count; ++Sequence)
        {
            FProofCommandResult Receipt;
            if (!PC->FindReceipt(Sequence, Receipt)) return false;
            if (!InputFixtureCheck(Receipt.bAccepted, TEXT("group_host_receipt_accepted"))) return false;
        }
        return true;
    };
    const double StageAge = Now - InputFixtureStageStart;
    switch (InputFixtureStage)
    {
    case 0:
    {
        int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
        if (!HQ || !Harvester || !EnemyHQ || !bConfiguredLocalInput || PC->OwnerId != 1 || !Snapshot.bSkirmish
            || Snapshot.MatchEpoch < 1 || Width != 1280 || Height != 720 || FAssetCompilingManager::Get().GetNumRemainingAssets() != 0) return;
        if (!InputFixtureCheck(bCameraPanEnabled && !PC->IsControlsAutomation() && !PC->IsProofAutomation()
            && SkirmishScenario.IsEmpty() && !InputFixtureOutput.IsEmpty(), TEXT("isolated_fixture_configuration"))) return;
        UE_LOG(LogTemp, Display, TEXT("PROOF_INPUT_BINDING_READY owner=1 inputSource=engineInjection physicalInputVerified=false"));
        InputFixtureEpoch = Snapshot.MatchEpoch; InputFixtureHQ = HQ->UnitId; InputFixtureHarvester = Harvester->UnitId;
        InputFixtureEnemyHQ = EnemyHQ->UnitId; InputFixtureHQPosition = HQ->Position;
        if (!Point(HQ->Position + FVector(350.f, 0.f, 0.f))) return;
        QueueInputFixtureClick(EKeys::R); AdvanceInputFixture(1); return;
    }
    case 1:
        if (!Refinery) return;
        if (!InputFixtureCheck(ManualBuildPresses == 1, TEXT("native_refinery_callback"))) return;
        if (!Point(HQ->Position + FVector(0.f, -350.f, 0.f))) return;
        QueueInputFixtureClick(EKeys::F); AdvanceInputFixture(2); return;
    case 2:
        if (!Factory) return;
        if (!InputFixtureCheck(ManualBuildPresses == 2, TEXT("native_factory_callback"))) return;
        if (!Point(Harvester->Position)) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton); AdvanceInputFixture(3); return;
    case 3:
    {
        if (!InputFixtureCheck(Selected({InputFixtureHarvester}) && ManualLeftPresses == 1 && !PC->IsInputKeyDown(EKeys::LeftMouseButton), TEXT("native_select_and_release"))) return;
        const FProofResourceRecord* Ore = Snapshot.Resources.FindByPredicate([this](const FProofResourceRecord& Resource) { return Resource.OwnerId == 1 && Resource.Remaining > 0; });
        if (!Ore || !Point(Ore->Position)) return;
        QueueInputFixtureClick(EKeys::RightMouseButton); AdvanceInputFixture(4); return;
    }
    case 4:
    {
        FProofCommandResult Receipt; if (!PC->FindReceipt(LastRequestedSequence, Receipt)) return;
        if (!InputFixtureCheck(ManualRightPresses == 1 && LastRequestedType == TEXT("Gather") && Receipt.bAccepted, TEXT("native_gather_accepted"))) return;
        QueueInputFixtureClick(EKeys::P); AdvanceInputFixture(5); return;
    }
    case 5:
        if (Combat.Num() < 1) return;
        QueueInputFixtureClick(EKeys::P); AdvanceInputFixture(6); return;
    case 6:
        if (Combat.Num() < 2) return;
        if (!InputFixtureCheck(ManualProducePresses == 2, TEXT("native_produce_two_replicated_units"))) return;
        InputFixtureCombatIds = {Combat[0], Combat[1]};
        if (!Point(UnitById(Combat[0])->Position)) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton); AdvanceInputFixture(7); return;
    case 7:
        if (!InputFixtureCheck(Selected({InputFixtureCombatIds[0]}), TEXT("combat_single_selected"))) return;
        if (!Point(UnitById(InputFixtureCombatIds[1])->Position)) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton, EKeys::LeftShift); AdvanceInputFixture(8); return;
    case 8:
        if (!InputFixtureCheck(Selected(InputFixtureCombatIds) && !PC->IsInputKeyDown(EKeys::LeftShift), TEXT("native_shift_add_and_modifier_release"))) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton, EKeys::LeftShift); AdvanceInputFixture(9); return;
    case 9:
        if (!InputFixtureCheck(Selected({InputFixtureCombatIds[0]}), TEXT("native_shift_toggle_off"))) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton, EKeys::LeftShift); AdvanceInputFixture(10); return;
    case 10:
        if (!InputFixtureCheck(Selected(InputFixtureCombatIds), TEXT("native_shift_toggle_on"))) return;
        QueueInputFixtureClick(InputFixtureGroup == 1 ? EKeys::One : InputFixtureGroup == 2 ? EKeys::Two : EKeys::Three, EKeys::LeftControl);
        AdvanceInputFixture(11); return;
    case 11:
        if (!InputFixtureCheck(!PC->IsInputKeyDown(EKeys::LeftControl), TEXT("control_modifier_released"))) return;
        if (!Point(FVector(-2100.f, -1100.f, 0.f))) return;
        if (!InputFixtureCheck(FindUnitUnderCursor(true) == INDEX_NONE, TEXT("selection_clear_pointer_has_no_owned_unit"))) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton); AdvanceInputFixture(12); return;
    case 12:
        if (!InputFixtureCheck(Selected({}), TEXT("native_empty_ground_clears_selection"))) return;
        QueueInputFixtureClick(InputFixtureGroup == 1 ? EKeys::One : InputFixtureGroup == 2 ? EKeys::Two : EKeys::Three);
        AdvanceInputFixture(13); return;
    case 13:
    {
        const TArray<int32> Wanted = InputFixtureGroup == 1 ? InputFixtureCombatIds
            : TArray<int32>{InputFixtureCombatIds[InputFixtureGroup - 2]};
        if (!InputFixtureCheck(Selected(Wanted), TEXT("native_group_exact_recall"))) return;
        if (++InputFixtureRepeat < 3) { AdvanceInputFixture(11); return; }
        InputFixtureRepeat = 0;
        if (++InputFixtureGroup <= 3)
        {
            // Give each slot different contents using the native click path.
            if (!Point(UnitById(InputFixtureCombatIds[InputFixtureGroup - 2])->Position)) return;
            QueueInputFixtureClick(EKeys::LeftMouseButton); AdvanceInputFixture(37); return;
        }
        if (!InputFixtureCheck(ManualGroupPresses == 12, TEXT("three_groups_repeated_binding_callbacks"))) return;
        QueueInputFixtureClick(EKeys::One); AdvanceInputFixture(38); return;
    }
    case 37:
        if (!InputFixtureCheck(Selected({InputFixtureCombatIds[InputFixtureGroup - 2]}), TEXT("group_single_member_selected_before_assignment"))) return;
        QueueInputFixtureClick(InputFixtureGroup == 2 ? EKeys::Two : EKeys::Three, EKeys::LeftControl);
        AdvanceInputFixture(11); return;
    case 38:
        if (!InputFixtureCheck(Selected(InputFixtureCombatIds) && ManualGroupPresses == 13,
            TEXT("three_groups_independent_contents_recalled"))) return;
        CaptureInputFixture(TEXT("01-selected-group"));
        if (!Point(HQ->Position)) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton, EKeys::LeftShift); AdvanceInputFixture(14); return;
    case 14:
        if (!InputFixtureCheck(Selection.GetSelectedUnitIds().Num() == 3, TEXT("mixed_group_add_structure"))) return;
        if (!Point(Harvester->Position)) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton, EKeys::LeftShift); AdvanceInputFixture(15); return;
    case 15:
        if (!InputFixtureCheck(Selection.GetSelectedUnitIds().Num() == 4, TEXT("mixed_group_add_harvester"))) return;
        for (int32 Id : Selection.GetSelectedUnitIds()) InputFixtureMoveStart.Add(Id, UnitById(Id)->Position);
        InputFixtureSequenceStart = LastRequestedSequence;
        if (!Point(FVector(-400.f, -400.f, 0.f))) return;
        if (!InputFixtureCheck(FindUnitUnderCursor(false) == INDEX_NONE, TEXT("group_move_ground_empty"))) return;
        QueueInputFixtureClick(EKeys::RightMouseButton); AdvanceInputFixture(16); return;
    case 16:
    {
        if (!Receipts(3)) return;
        bool bAllMoved = true;
        for (const auto& Pair : InputFixtureMoveStart)
            if (Pair.Key != InputFixtureHQ) bAllMoved &= FVector::Dist(UnitById(Pair.Key)->Position, Pair.Value) > 120.f;
        if (!bAllMoved) return;
        if (!InputFixtureCheck(HQ->Position.Equals(InputFixtureHQPosition, 0.01f), TEXT("mixed_group_move_mobile_only_structure_fixed"))) return;
        // Wait for mobile arrivals before measuring excluded harvester during attack.
        if (StageAge < 18.0) return;
        CaptureInputFixture(TEXT("02-group-moved"));
        InputFixtureHarvesterPosition = Harvester->Position;
        InputFixtureSequenceStart = LastRequestedSequence;
        if (!Point(EnemyHQ->Position)) return;
        QueueInputFixtureClick(EKeys::RightMouseButton); AdvanceInputFixture(17); return;
    }
    case 17:
        if (!Receipts(2)) return;
        if (!EnemyHQ || EnemyHQ->HitPoints >= 299.f) return;
        if (!InputFixtureCheck(LastRequestedSequence == InputFixtureSequenceStart + 2
            && HQ->Position.Equals(InputFixtureHQPosition, 0.01f)
            && Harvester->Position.Equals(InputFixtureHarvesterPosition, 2.f), TEXT("mixed_group_attack_combat_only_replicated_damage"))) return;
        CaptureInputFixture(TEXT("03-group-attacked"));
        // Cancel the attack using real group movement, then isolate a duel.
        InputFixtureSequenceStart = LastRequestedSequence;
        if (!Point(FVector(-400.f, -400.f, 0.f))) return;
        QueueInputFixtureClick(EKeys::RightMouseButton); AdvanceInputFixture(29); return;
    case 29:
        if (!Receipts(3) || StageAge < 18.0) return;
        if (!Point(UnitById(InputFixtureCombatIds[1])->Position)) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton); AdvanceInputFixture(30); return;
    case 30:
        if (!InputFixtureCheck(Selected({InputFixtureCombatIds[1]}), TEXT("duel_survivor_selected_for_separation"))) return;
        if (!Point(FVector(-1600.f, -700.f, 0.f))) return;
        QueueInputFixtureClick(EKeys::RightMouseButton); AdvanceInputFixture(31); return;
    case 31:
        if (StageAge < 9.0) return;
        if (!Point(UnitById(InputFixtureCombatIds[0])->Position)) return;
        QueueInputFixtureClick(EKeys::LeftMouseButton); AdvanceInputFixture(32); return;
    case 32:
        if (!InputFixtureCheck(Selected({InputFixtureCombatIds[0]}), TEXT("duel_victim_selected"))) return;
        if (!Point(FVector(400.f, 400.f, 0.f))) return;
        QueueInputFixtureClick(EKeys::RightMouseButton); AdvanceInputFixture(33); return;
    case 33:
    {
        if (!UnitById(InputFixtureCombatIds[0]) || !UnitById(InputFixtureCombatIds[0])->Position.Equals(FVector(400.f, 400.f, 0.f), 2.f)) return;
        FString ControlPath;
        FParse::Value(FCommandLine::Get(), TEXT("ProofInputBindingControl="), ControlPath);
        if (!InputFixtureCheck(!ControlPath.IsEmpty() && FFileHelper::SaveStringToFile(
            FString::Printf(TEXT("duel:%d"), InputFixtureCombatIds[0]), *ControlPath), TEXT("ordinary_bot_duel_requested"))) return;
        AdvanceInputFixture(34); return;
    }
    case 34:
        if (!UnitById(InputFixtureCombatIds[0]) || UnitById(InputFixtureCombatIds[0])->HitPoints > 0.f) return;
        if (!InputFixtureCheck(UnitById(InputFixtureCombatIds[1]) && UnitById(InputFixtureCombatIds[1])->HitPoints > 0.f,
            TEXT("ordinary_duel_selected_unit_died_survivor_live"))) return;
        QueueInputFixtureClick(EKeys::One); AdvanceInputFixture(35); return;
    case 35:
        if (!InputFixtureCheck(Selected({InputFixtureCombatIds[1]}), TEXT("native_group_recall_prunes_dead_member"))) return;
        if (!EnemyHQ || !Point(EnemyHQ->Position)) return;
        QueueInputFixtureClick(EKeys::RightMouseButton); AdvanceInputFixture(36); return;
    case 36:
    {
        FProofCommandResult Receipt;
        if (!PC->FindReceipt(LastRequestedSequence, Receipt)) return;
        if (!InputFixtureCheck(Receipt.bAccepted && LastRequestedType == TEXT("Attack"), TEXT("survivor_attack_accepted"))) return;
        InputFixtureZoom = Camera->OrthoWidth;
        QueueInputFixtureClick(EKeys::MouseScrollUp); AdvanceInputFixture(18); return;
    }
    case 18:
        if (!InputFixtureCheck(ManualWheelUpPresses == 1 && Camera->OrthoWidth < InputFixtureZoom && !PC->IsInputKeyDown(EKeys::MouseScrollUp), TEXT("native_wheel_up_and_release"))) return;
        QueueInputFixtureClick(EKeys::MouseScrollDown); AdvanceInputFixture(19); return;
    case 19:
        if (!InputFixtureCheck(ManualWheelDownPresses == 1 && FMath::IsNearlyEqual(Camera->OrthoWidth, InputFixtureZoom), TEXT("native_wheel_down_restores_width"))) return;
        InputFixturePanBefore = CameraGroundCenter;
        InputFixtureEdges.Emplace(EKeys::Right, true); AdvanceInputFixture(20); return;
    case 20:
        if (!InputFixtureCheck(PC->IsInputKeyDown(EKeys::Right), TEXT("injected_arrow_down_polled"))) return;
        if (!bViewportFocused || !bViewportForeground)
        {
            if (!InputFixtureCheck(CameraGroundCenter.Equals(InputFixturePanBefore, 0.01f), TEXT("observed_real_focus_pan_suppressed"))) return;
            bInputFixtureRealFocusSuppressed = true;
        }
        if (StageAge < 1.0) return;
        InputFixtureEdges.Emplace(EKeys::Right, false); AdvanceInputFixture(21); return;
    case 21:
        if (!InputFixtureCheck(!PC->IsInputKeyDown(EKeys::Right), TEXT("arrow_release_clears_state"))) return;
        // Explicit boundary setup, not a claim that input traversed the world.
        CameraGroundCenter = FVector(-9490.f, 0.f, 0.f);
        Camera->SetWorldLocation(CameraGroundCenter + FVector(0.f, -3200.f, 3200.f));
        bInputFixtureVirtualPan = true;
        bInputFixtureVirtualFocusUsed = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_INPUT_BINDING_PAN focusSource=virtualFixture fixtureSetupX=-9490 osWindowFocusVerified=false"));
        InputFixtureEdges.Emplace(EKeys::Right, true); AdvanceInputFixture(22); return;
    case 22:
        ApplyCameraPan(DeltaSeconds, true, true);
        if (StageAge < 0.8) return;
        if (!InputFixtureCheck(PC->IsInputKeyDown(EKeys::Right) && FMath::IsNearlyEqual(CameraGroundCenter.X, -9500.0, 0.001)
            && CameraGroundCenter.Y == 0.0 && CameraGroundCenter.Z == 0.0, TEXT("virtual_focus_injected_pan_hits_world_limit"))) return;
        InputFixtureEdges.Emplace(EKeys::Right, false); AdvanceInputFixture(23); return;
    case 23:
        if (!InputFixtureCheck(!PC->IsInputKeyDown(EKeys::Right), TEXT("virtual_pan_arrow_released"))) return;
        bInputFixtureVirtualPan = false;
        CameraGroundCenter = FVector::ZeroVector;
        Camera->SetWorldLocation(FVector(0.f, -3200.f, 3200.f));
        InputFixtureEdges.Emplace(EKeys::Up, true); InputFixtureEdges.Emplace(EKeys::LeftShift, true); InputFixtureEdges.Emplace(EKeys::LeftControl, true);
        AdvanceInputFixture(24); return;
    case 24:
        if (!InputFixtureCheck(PC->IsInputKeyDown(EKeys::Up) && PC->IsInputKeyDown(EKeys::LeftShift)
            && PC->IsInputKeyDown(EKeys::LeftControl), TEXT("pre_simulated_focus_loss_keys_down"))) return;
        bInputFixtureSimulatedFocusLoss = true;
        ViewportClient->LostFocus(ViewportClient->Viewport);
        UE_LOG(LogTemp, Display, TEXT("PROOF_INPUT_BINDING_FOCUS notification=simulatedLostFocus osWindowFocusVerified=false"));
        AdvanceInputFixture(25); return;
    case 25:
        if (!InputFixtureCheck(!PC->IsInputKeyDown(EKeys::Up) && !PC->IsInputKeyDown(EKeys::LeftShift)
            && !PC->IsInputKeyDown(EKeys::LeftControl), TEXT("simulated_lost_focus_flushes_arrow_and_modifiers"))) return;
        if (Snapshot.MatchPhase != EProofMatchPhase::Finished) return;
        if (!InputFixtureCheck(Snapshot.WinnerTeam == 0, TEXT("ordinary_group_attack_reaches_victory"))) return;
        CaptureInputFixture(TEXT("04-terminal")); AdvanceInputFixture(26); return;
    case 26:
        if (StageAge < 0.7 || IFileManager::Get().FileSize(*FPaths::Combine(InputFixtureOutput, TEXT("04-terminal.png"))) <= 1024) return;
        QueueInputFixtureClick(EKeys::N); AdvanceInputFixture(27); return;
    case 27:
        if (Snapshot.MatchEpoch <= InputFixtureEpoch) return;
        if (!InputFixtureCheck(ManualRematchPresses == 1 && Selection.GetSelectedUnitIds().IsEmpty() && GroupOrders.NumPending() == 0
            && HQ && HQ->UnitId == InputFixtureHQ, TEXT("native_rematch_epoch_reset_reuses_ids_clears_selection_queue"))) return;
        InputFixtureGroup = 1;
        QueueInputFixtureClick(EKeys::One); AdvanceInputFixture(28); return;
    case 28:
        if (!InputFixtureCheck(Selection.GetSelectedUnitIds().IsEmpty(), TEXT("old_group_empty_after_reused_ids_epoch_reset"))) return;
        if (++InputFixtureGroup <= 3)
        { QueueInputFixtureClick(InputFixtureGroup == 2 ? EKeys::Two : EKeys::Three); return; }
        for (const TCHAR* Name : {TEXT("01-selected-group.png"), TEXT("02-group-moved.png"), TEXT("03-group-attacked.png"), TEXT("04-terminal.png")})
            if (!InputFixtureCheck(IFileManager::Get().FileSize(*FPaths::Combine(InputFixtureOutput, Name)) > 1024, TEXT("rendered_captures_written"))) return;
        if (StageAge < 0.75) return;
        FinishInputBindingFixture(true, TEXT("binding_groups_orders_virtual_pan_focus_flush_rematch")); return;
    }
}
