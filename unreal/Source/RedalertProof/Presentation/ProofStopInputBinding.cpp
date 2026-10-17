#include "Presentation/ProofRTSPawn.h"
#include "Runtime/ProofPlayerController.h"
#include "Runtime/ProofGameState.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"

bool AProofRTSPawn::IsStopInputReady() const
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!bStopInputBinding || !PC || !PC->IsLocalController() || !PC->GetLocalPlayer() || !PC->PlayerInput
        || PC->GetPawnOrSpectator() != this || PC->AcknowledgedPawn != this || !InputComponent
        || !InputComponent->IsRegistered() || !InputEnabled() || !Viewport || !Viewport->Viewport || Viewport->IgnoreInput()) return false;
    const uint64 ProcessedFrame = PC->GetStopInputProcessedFrame(InputComponent.Get());
    if (ProcessedFrame == 0 || ProcessedFrame > GFrameCounter || GFrameCounter - ProcessedFrame > 1) return false;
    return InputComponent->KeyBindings.ContainsByPredicate([](const FInputKeyBinding& Binding)
    {
        return Binding.Chord.Key == EKeys::S && Binding.KeyEvent == IE_Pressed && Binding.KeyDelegate.IsBound();
    });
}

bool AProofRTSPawn::SendStopInputEdge(bool bPressed)
{
    if (!ControlsCheck(IsStopInputReady(), TEXT("stop_input_current_stack_ready"))) return false;
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
    const FPlatformUserId User = PC->GetLocalPlayer()->GetPlatformUserId();
    const FInputDeviceId Device = Mapper.GetPrimaryInputDeviceForUser(User);
    if (!ControlsCheck(Device.IsValid() && Mapper.GetUserForInputDevice(Device) == User,
        TEXT("stop_input_device_maps_to_local_user"))) return false;
    if (StopInputEdges == 0)
    {
        StopInputDevice = Device.GetId();
        StopInputUser = User.GetInternalId();
    }
    if (!ControlsCheck(StopInputDevice == Device.GetId() && StopInputUser == User.GetInternalId()
        && StopInputEdges == (bPressed ? 0 : 1), TEXT("stop_input_exact_edge_and_device"))) return false;
    // Only the viewport route supplies the edge. No delegate/shared handler call,
    // cursor movement, focus override, delay replay or local simulation edit.
    bStopInputLastHandled = Viewport->InputKey(FInputKeyEventArgs(Viewport->Viewport, Device,
        EKeys::S, bPressed ? IE_Pressed : IE_Released, FPlatformTime::Cycles64()));
    ++StopInputEdges;
    return true;
}

void AProofRTSPawn::LogStopInputBinding(const TCHAR* Phase) const
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    UE_LOG(LogTemp, Display, TEXT("PROOF_STOP_INPUT {\"phase\":\"%s\",\"owner\":%d,\"unit\":%d,\"epoch\":%d,")
        TEXT("\"frame\":%llu,\"processedFrame\":%llu,\"device\":%d,\"user\":%d,\"edges\":%d,\"nativeStop\":%d,")
        TEXT("\"selected\":%d,\"selectionCount\":%d,\"sequence\":%d,\"keyDown\":%d,\"handled\":%d,\"ready\":%d,")
        TEXT("\"key\":\"S\",\"inputSource\":\"engineInjection\",\"pointerSource\":\"projectedSynthetic\",")
        TEXT("\"physicalInputVerified\":false,\"osWindowFocusVerified\":false}"),
        Phase, PC->OwnerId, ControlsOwnUnit, GS->Snapshot.MatchEpoch, GFrameCounter,
        PC->GetStopInputProcessedFrame(InputComponent.Get()), StopInputDevice, StopInputUser,
        StopInputEdges, ManualStopPresses, SelectedUnitId, Selection.GetSelectedUnitIds().Num(),
        LastRequestedSequence, PC->IsInputKeyDown(EKeys::S) ? 1 : 0, bStopInputLastHandled ? 1 : 0, IsStopInputReady() ? 1 : 0);
}

bool AProofRTSPawn::BeginStopInputBinding()
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!ControlsCheck(StopInputStage == 0 && ControlsStopNativeBaseline == 0 && ManualStopPresses == 0
        && !PC->IsInputKeyDown(EKeys::S) && GroupOrders.NumPending() == 0
        && SelectedUnitId == ControlsOwnUnit && Selection.GetSelectedUnitIds().Num() == 1
        && Selection.GetSelectedUnitIds().Contains(ControlsOwnUnit), TEXT("stop_input_clean_baseline"))) return false;
    if (!SendStopInputEdge(true)) return false;
    StopInputPressFrame = GFrameCounter;
    StopInputStage = 1;
    LogStopInputBinding(TEXT("press"));
    return true;
}

bool AProofRTSPawn::TickStopInputBinding()
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!ControlsCheck(IsStopInputReady(), TEXT("stop_input_stack_stays_ready"))) return false;
    if (StopInputStage == 1)
    {
        if (ManualStopPresses == ControlsStopNativeBaseline) return false;
        if (!ControlsCheck(ManualStopPresses == ControlsStopNativeBaseline + 1
            && GFrameCounter > StopInputPressFrame && PC->GetStopInputProcessedFrame(InputComponent.Get()) >= StopInputPressFrame
            && PC->IsInputKeyDown(EKeys::S), TEXT("stop_input_one_normal_frame_callback_held"))) return false;
        if (LastRequestedSequence < 6) return false;
        if (!ControlsCheck(LastRequestedSequence == 6 && LastRequestedType == TEXT("Stop")
            && GroupOrders.NumPending() == 0, TEXT("stop_input_callback_dispatched_single_stop"))) return false;
        LogStopInputBinding(TEXT("callback"));
        if (!SendStopInputEdge(false)) return false;
        StopInputReleaseFrame = GFrameCounter;
        StopInputStage = 2;
        LogStopInputBinding(TEXT("release"));
        return false;
    }
    if (StopInputStage == 2)
    {
        if (GFrameCounter <= StopInputReleaseFrame
            || PC->GetStopInputProcessedFrame(InputComponent.Get()) <= StopInputReleaseFrame) return false;
        if (!ControlsCheck(!PC->IsInputKeyDown(EKeys::S) && StopInputEdges == 2
            && ManualStopPresses == ControlsStopNativeBaseline + 1, TEXT("stop_input_release_processed_without_repeat"))) return false;
        LogStopInputBinding(TEXT("released"));
        StopInputStage = 3;
    }
    return StopInputStage == 3;
}
