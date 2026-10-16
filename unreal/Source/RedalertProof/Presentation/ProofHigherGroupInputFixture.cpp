#include "Presentation/ProofRTSPawn.h"
#include "Runtime/ProofPlayerController.h"
#include "Runtime/ProofGameState.h"
#include "Engine/World.h"
#include "AssetCompilingManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
FKey HigherGroupDigitKey(int32 Index)
{
    const FKey Keys[] = {EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
    return Index >= 0 && Index < UE_ARRAY_COUNT(Keys) ? Keys[Index] : FKey();
}
}

bool AProofRTSPawn::IsHigherGroupInputFixture() const
{
    return FParse::Param(FCommandLine::Get(), TEXT("ProofHigherGroupBindings"));
}

bool AProofRTSPawn::AreHigherGroupKeysReleased() const
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!PC || PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl)) return false;
    for (int32 Index = 0; Index < 6; ++Index) if (PC->IsInputKeyDown(HigherGroupDigitKey(Index))) return false;
    return true;
}

void AProofRTSPawn::QueueHigherGroupSelection(bool bOverwrite)
{
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    const bool bInitiallyHQ = HigherGroupAssignIndex % 2 == 0;
    HigherGroupExpectedSelection = (bInitiallyHQ != bOverwrite) ? InputFixtureHQ : InputFixtureHarvester;
    const FProofUnitRecord* Unit = GS ? GS->Snapshot.Units.FindByPredicate([this](const FProofUnitRecord& Candidate)
        { return Candidate.UnitId == HigherGroupExpectedSelection && Candidate.OwnerId == 1 && Candidate.HitPoints > 0.f; }) : nullptr;
    if (!InputFixtureCheck(Unit && SetControlsCursorFromWorld(Unit->Position)
        && FindUnitUnderCursor(true) == Unit->UnitId, TEXT("higher_group_select_pointer_exact"))) return;
    HigherGroupLeftBefore = ManualLeftPresses;
    QueueInputFixtureClick(EKeys::LeftMouseButton);
    AdvanceInputFixture(1);
}

void AProofRTSPawn::QueueHigherGroupClear()
{
    if (!InputFixtureCheck(SetControlsCursorFromWorld(FVector(-2100.f, -1100.f, 0.f))
        && FindUnitUnderCursor(true) == INDEX_NONE, TEXT("higher_group_clear_pointer_empty"))) return;
    HigherGroupLeftBefore = ManualLeftPresses;
    QueueInputFixtureClick(EKeys::LeftMouseButton);
    AdvanceInputFixture(3);
}

void AProofRTSPawn::TickHigherGroupInputFixture()
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS) return;
    const FProofSnapshot& Snapshot = GS->Snapshot;
    auto SelectedExactly = [this](int32 Id)
    {
        return Selection.GetSelectedUnitIds().Num() == 1 && Selection.GetSelectedUnitIds()[0] == Id;
    };
    switch (InputFixtureStage)
    {
    case 0:
    {
        int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
        const FProofUnitRecord* HQ = nullptr; const FProofUnitRecord* Harvester = nullptr;
        for (const FProofUnitRecord& Unit : Snapshot.Units)
        {
            if (Unit.OwnerId != 1 || Unit.HitPoints <= 0.f) continue;
            if (Unit.Kind == EProofUnitKind::HQ) HQ = &Unit;
            if (Unit.Kind == EProofUnitKind::Harvester) Harvester = &Unit;
        }
        if (!HQ || !Harvester || !bConfiguredLocalInput || PC->OwnerId != 1 || !Snapshot.bSkirmish
            || Snapshot.MatchEpoch < 1 || Width != 1280 || Height != 720
            || FAssetCompilingManager::Get().GetNumRemainingAssets() != 0 || !IsInputFixtureReadyForInjection()) return;
        ObserveInputFixtureReadiness(FPlatformTime::Seconds(), true);
        if (!InputFixtureCheck(!PC->IsControlsAutomation() && !PC->IsProofAutomation() && SkirmishScenario.IsEmpty()
            && !InputFixtureOutput.IsEmpty() && AreHigherGroupKeysReleased(), TEXT("higher_group_fixture_ready"))) return;
        InputFixtureHQ = HQ->UnitId; InputFixtureHarvester = Harvester->UnitId; InputFixtureEpoch = Snapshot.MatchEpoch;
        HigherGroupExpectedIds.Init(INDEX_NONE, 6);
        HigherGroupAssignIndex = 0; HigherGroupRound = 0;
        UE_LOG(LogTemp, Display, TEXT("PROOF_INPUT_BINDING_READY owner=1 fixtureMode=higherGroups inputSource=engineInjection physicalInputVerified=false"));
        QueueHigherGroupSelection(false); return;
    }
    case 1:
        if (!InputFixtureCheck(SelectedExactly(HigherGroupExpectedSelection) && ManualLeftPresses == HigherGroupLeftBefore + 1
            && !PC->IsInputKeyDown(EKeys::LeftMouseButton), TEXT("higher_group_native_selection_callback"))) return;
        HigherGroupCallbackBefore = ManualGroupPresses;
        QueueInputFixtureClick(HigherGroupDigitKey(HigherGroupAssignIndex), HigherGroupRound == 0 ? EKeys::LeftControl : EKeys::RightControl);
        AdvanceInputFixture(2); return;
    case 2:
    {
        const FString Check = FString::Printf(TEXT("higher_groups_round_%d_slot_%d_saved"), HigherGroupRound, HigherGroupAssignIndex + 4);
        if (!InputFixtureCheck(ManualGroupPresses == HigherGroupCallbackBefore + 1 && AreHigherGroupKeysReleased()
            && SelectedExactly(HigherGroupExpectedSelection), *Check)) return;
        // Change the expected contents of only the slot actually overwritten.
        HigherGroupExpectedIds[HigherGroupAssignIndex] = HigherGroupExpectedSelection;
        if (HigherGroupRound == 0 && ++HigherGroupAssignIndex < 6) { QueueHigherGroupSelection(false); return; }
        if (HigherGroupRound == 0 && !InputFixtureCheck(ManualGroupPresses == 6 && ManualLeftPresses == 6,
            TEXT("higher_groups_initial_saved"))) return;
        HigherGroupRecallIndex = 0;
        QueueHigherGroupClear(); return;
    }
    case 3:
        if (!InputFixtureCheck(Selection.GetSelectedUnitIds().IsEmpty() && ManualLeftPresses == HigherGroupLeftBefore + 1
            && !PC->IsInputKeyDown(EKeys::LeftMouseButton), TEXT("higher_group_native_clear_callback"))) return;
        HigherGroupCallbackBefore = ManualGroupPresses;
        QueueInputFixtureClick(HigherGroupDigitKey(HigherGroupRecallIndex));
        AdvanceInputFixture(4); return;
    case 4:
    {
        const FString Check = FString::Printf(TEXT("higher_groups_round_%d_slot_%d_exact_recall"), HigherGroupRound, HigherGroupRecallIndex + 4);
        if (!InputFixtureCheck(SelectedExactly(HigherGroupExpectedIds[HigherGroupRecallIndex])
            && ManualGroupPresses == HigherGroupCallbackBefore + 1 && AreHigherGroupKeysReleased(), *Check)) return;
        if (++HigherGroupRecallIndex < 6) { QueueHigherGroupClear(); return; }
        const FString RoundCheck = FString::Printf(TEXT("higher_groups_round_%d_complete"), HigherGroupRound);
        if (!InputFixtureCheck(true, *RoundCheck)) return;
        if (HigherGroupRound == 0 || HigherGroupRound == 6)
        {
            const TCHAR* Name = HigherGroupRound == 0 ? TEXT("higher-groups-initial") : TEXT("higher-groups-overwritten");
            const FString CapturePath = FPaths::Combine(InputFixtureOutput, FString(Name) + TEXT(".png"));
            if (!InputFixtureCheck(!IFileManager::Get().FileExists(*CapturePath), TEXT("higher_group_capture_path_fresh"))) return;
            if (HigherGroupRound == 0) HigherGroupInitialCaptureTime = FDateTime::UtcNow();
            else HigherGroupFinalCaptureTime = FDateTime::UtcNow();
            CaptureInputFixture(Name);
            AdvanceInputFixture(5); return;
        }
        ++HigherGroupRound; HigherGroupAssignIndex = HigherGroupRound - 1;
        QueueHigherGroupSelection(true); return;
    }
    case 5:
    {
        const TCHAR* Name = HigherGroupRound == 0 ? TEXT("higher-groups-initial.png") : TEXT("higher-groups-overwritten.png");
        const FString Path = FPaths::Combine(InputFixtureOutput, Name);
        if (IFileManager::Get().FileSize(*Path) <= 1024) return;
        const FDateTime RequestedAt = HigherGroupRound == 0 ? HigherGroupInitialCaptureTime : HigherGroupFinalCaptureTime;
        if (!InputFixtureCheck(IFileManager::Get().GetTimeStamp(*Path) >= RequestedAt, TEXT("higher_group_capture_written_after_request"))) return;
        if (HigherGroupRound == 0)
        {
            HigherGroupRound = 1; HigherGroupAssignIndex = 0;
            QueueHigherGroupSelection(true); return;
        }
        if (!InputFixtureCheck(AreHigherGroupKeysReleased() && !PC->IsInputKeyDown(EKeys::LeftMouseButton),
            TEXT("higher_groups_all_modifiers_and_digits_released"))) return;
        if (!InputFixtureCheck(ManualGroupPresses == 54 && ManualLeftPresses == 54 && InputFixtureEdgesSent == 240
            && ManualRightPresses == 0 && ManualProducePresses == 0 && ManualBuildPresses == 0
            && ManualWheelUpPresses == 0 && ManualWheelDownPresses == 0 && ManualRematchPresses == 0
            && LastRequestedSequence == 0 && GroupOrders.NumPending() == 0 && Snapshot.MatchEpoch == InputFixtureEpoch,
            TEXT("higher_groups_exact_callback_counts"))) return;
        if (!InputFixtureCheck(IFileManager::Get().FileSize(*FPaths::Combine(InputFixtureOutput, TEXT("higher-groups-initial.png"))) > 1024
            && IFileManager::Get().FileSize(*FPaths::Combine(InputFixtureOutput, TEXT("higher-groups-overwritten.png"))) > 1024,
            TEXT("higher_groups_captures_written"))) return;
        FinishInputBindingFixture(true, TEXT("higher_groups_independent_save_recall_overwrite")); return;
    }
    }
}
