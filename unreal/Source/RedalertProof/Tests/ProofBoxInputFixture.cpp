#include "Tests/ProofBoxInputFixture.h"
#include "Presentation/ProofRTSPawn.h"
#include "Presentation/ProofHUD.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"

namespace
{
constexpr int32 CaseCount = 31;
const TCHAR* Names[CaseCount] = {TEXT("click"), TEXT("release-threshold"), TEXT("sticky-return"),
    TEXT("box-down-right"), TEXT("box-down-left"), TEXT("box-up-right"), TEXT("box-up-left"),
    TEXT("shift-latched-cap"), TEXT("late-shift-replaces"), TEXT("empty-clears"), TEXT("shift-empty"),
    TEXT("hud-start"), TEXT("hud-end-return-held"), TEXT("outside-pointer"), TEXT("real-resize"),
    TEXT("owner-change"), TEXT("epoch-change"), TEXT("phase-change"), TEXT("duplicate-press-release"),
    TEXT("group-1"), TEXT("group-2"), TEXT("group-3"), TEXT("group-4"), TEXT("group-5"),
    TEXT("group-6"), TEXT("group-7"), TEXT("group-8"), TEXT("group-9"), TEXT("type-visible"),
    TEXT("type-paired"), TEXT("context-move-route")};

FString IdText(const TArray<int32>& Ids)
{
    TArray<FString> Parts;
    for (int32 Id : Ids) Parts.Add(FString::FromInt(Id));
    return FString::Join(Parts, TEXT(","));
}
}

AProofBoxInputGameMode::AProofBoxInputGameMode()
{
    GameStateClass = AProofGameState::StaticClass();
    PlayerControllerClass = AProofPlayerController::StaticClass();
    DefaultPawnClass = AProofRTSPawn::StaticClass();
    HUDClass = AProofHUD::StaticClass();
}
void AProofBoxInputGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (AProofPlayerController* Controller = Cast<AProofPlayerController>(NewPlayer)) Controller->OwnerId = 1;
}
void AProofBoxInputGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (!FParse::Param(FCommandLine::Get(), TEXT("ProofBoxInputFixture")) || GetNetMode() != NM_Standalone)
    { FPlatformMisc::RequestExitWithStatus(false, 1); return; }
    GetWorld()->SpawnActor<AProofBoxInputFixture>();
}
AProofBoxInputFixture::AProofBoxInputFixture()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}
bool AProofBoxInputFixture::Ready() const
{
    const UGameViewportClient* Client = GetWorld()->GetGameViewport();
    if (!PC || !Pawn || !PC->IsLocalController() || !PC->GetLocalPlayer() || !PC->PlayerInput || PC->GetPawn() != Pawn
        || PC->AcknowledgedPawn != Pawn || !Pawn->InputComponent || !Pawn->InputComponent->IsRegistered()
        || !Pawn->InputEnabled() || !Client || !Client->Viewport || Client->IgnoreInput()
        || !Client->Viewport->HasFocus() || !Client->Viewport->IsForegroundWindow()) return false;
    const uint64 Frame = PC->GetStopInputProcessedFrame(Pawn->InputComponent.Get());
    if (!Frame || Frame > GFrameCounter || GFrameCounter - Frame > 1) return false;
    for (const EInputEvent Event : {IE_Pressed, IE_Released})
        if (!Pawn->InputComponent->KeyBindings.ContainsByPredicate([Event](const FInputKeyBinding& B)
            { return B.Chord.Key == EKeys::LeftMouseButton && B.KeyEvent == Event && B.KeyDelegate.IsBound(); })) return false;
    return true;
}
bool AProofBoxInputFixture::Populate()
{
    int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
    if (Width != 1280 || Height != 720) return false;
    FProofSnapshot S; S.MatchEpoch = 7; S.bSkirmish = true; S.MatchPhase = EProofMatchPhase::Playing;
    for (int32 Owner : {1, 2}) { FProofPlayerRecord P; P.OwnerId = Owner; S.Players.Add(P); }
    for (int32 Id = 1; Id <= 19; ++Id)
    {
        const FVector2D Center = Id <= 16 ? FVector2D(800 + ((Id - 1) % 4) * 70, 350 + ((Id - 1) / 4) * 80)
            : Id == 17 ? FVector2D(1130, 400) : FVector2D(1100 + (Id - 18) * 70, 600);
        FVector Origin, Direction;
        if (!PC->DeprojectScreenPositionToWorld(Center.X, Center.Y, Origin, Direction) || FMath::IsNearlyZero(Direction.Z)) return false;
        FProofUnitRecord U; U.UnitId = Id; U.OwnerId = Id == 18 ? 2 : 1;
        U.Position = Origin + Direction * (-Origin.Z / Direction.Z);
        U.Kind = Id == 17 ? EProofUnitKind::HQ : Id == 2 ? EProofUnitKind::Harvester : EProofUnitKind::Combat;
        U.HitPoints = Id == 19 ? 0.f : 100.f; S.Units.Add(U);
    }
    GetWorld()->GetGameState<AProofGameState>()->Publish(S); // One explicit diagnostic seed, never an authoritative gameplay claim.
    for (const FProofUnitRecord& U : S.Units)
    {
        FVector2D Center;
        if (!PC->ProjectWorldLocationToScreen(U.Position, Center)) return false;
        UE_LOG(LogTemp, Display, TEXT("PROOF_BOX_SEED id=%d owner=%d kind=%d hp=%.3f x=%.9f y=%.9f digest=%s population=diagnostic-over-cap"),
            U.UnitId, U.OwnerId, static_cast<int32>(U.Kind), U.HitPoints, Center.X, Center.Y, *S.ComputeDigest());
        if (U.UnitId == 1) Anchor = Center;
    }
    return true;
}
bool AProofBoxInputFixture::Edge(FKey Key, bool Pressed)
{
    if (!Ready()) return false;
    auto& Mapper = IPlatformInputDeviceMapper::Get();
    const FPlatformUserId User = PC->GetLocalPlayer()->GetPlatformUserId();
    const FInputDeviceId Device = Mapper.GetPrimaryInputDeviceForUser(User);
    if (!Device.IsValid() || Mapper.GetUserForInputDevice(Device) != User) return false;
    UGameViewportClient* Client = GetWorld()->GetGameViewport();
    const bool Handled = Client->InputKey(FInputKeyEventArgs(Client->Viewport, Device, Key,
        Pressed ? IE_Pressed : IE_Released, FPlatformTime::Cycles64()));
    if (Key == EKeys::LeftMouseButton) { if (Pressed) ++InjectedPresses; else ++InjectedReleases; }
    LastEdgeFrame = GFrameCounter;
    const FViewport* Viewport = Client->Viewport;
    UE_LOG(LogTemp, Display, TEXT("PROOF_BOX_EDGE case=%d key=%s pressed=%d frame=%llu processedFrame=%llu device=%d user=%d handled=%d ready=1 focus=%d foreground=%d injectedPress=%d injectedRelease=%d inputSource=viewport-engine-injection pointerSource=explicit-controls-cursor physicalInput=false"),
        Case, *Key.ToString(), Pressed ? 1 : 0, GFrameCounter, PC->GetStopInputProcessedFrame(Pawn->InputComponent.Get()),
        Device.GetId(), User.GetInternalId(), Handled ? 1 : 0, Viewport->HasFocus(), Viewport->IsForegroundWindow(), InjectedPresses, InjectedReleases);
    return true;
}
void AProofBoxInputFixture::Pointer(const FVector2D& Point)
{
    // Existing explicitly synthetic pointer route. Never moves the OS cursor,
    // forces focus/capture or pretends to test the real GetMousePosition route.
    Pawn->bControlsCursor = true; Pawn->ControlsCursor = Point;
}
void AProofBoxInputFixture::Baseline()
{
    const auto& S = GetWorld()->GetGameState<AProofGameState>()->Snapshot;
    Pawn->Selection.SelectSingle(S, 1, Case == 30 ? 1 : 17);
    if (Case == 7) Pawn->Selection.Toggle(S, 1, 16);
    Pawn->SelectedUnitId = Pawn->Selection.GetPrimaryUnitId();
    PressBefore = Pawn->ManualLeftPresses; ReleaseBefore = Pawn->ManualLeftReleases;
    RevisionBefore = Pawn->Selection.GetSelectionRevision(); SequenceBefore = Pawn->LastRequestedSequence;
    GroupBefore = Pawn->ManualGroupPresses; TypeBefore = Pawn->ManualTypeSelectPresses; RightBefore = Pawn->ManualRightPresses;
    Start = FVector2D(760, 310); End = FVector2D(1050, 630);
    if (Case <= 2 || Case >= 14) { Start = Anchor; End = Anchor + FVector2D(4, 0); }
    if (Case == 0) End = Anchor + FVector2D(2, 0);
    if (Case == 4 || Case == 6) { Start.X = 1050; End.X = 760; }
    if (Case == 5 || Case == 6) { Start.Y = 630; End.Y = 310; }
    if (Case == 9 || Case == 10) { Start = FVector2D(760, 650); End = FVector2D(780, 670); }
    if (Case == 11) Start = FVector2D(20, 20);
    Pointer(Start); Observe(TEXT("baseline"));
}
void AProofBoxInputFixture::Observe(const TCHAR* Phase) const
{
    const auto& S = GetWorld()->GetGameState<AProofGameState>()->Snapshot;
    int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
    const FViewport* V = GetWorld()->GetGameViewport()->Viewport;
    UE_LOG(LogTemp, Display, TEXT("PROOF_BOX_OBSERVE case=%d name=%s phase=%s frame=%llu processedFrame=%llu owner=%d epoch=%d matchPhase=%d width=%d height=%d x=%.9f y=%.9f ids=[%s] primary=%d revision=%llu nativePress=%d nativeRelease=%d nativeGroup=%d nativeType=%d nativeRight=%d leftDown=%d shiftDown=%d armed=%d dragging=%d focus=%d foreground=%d sequence=%d queue=%d digest=%s capture=%s physicalInput=false osFocusVerified=false"),
        Case, Names[Case], Phase, GFrameCounter, PC->GetStopInputProcessedFrame(Pawn->InputComponent.Get()), PC->OwnerId, S.MatchEpoch, static_cast<int32>(S.MatchPhase),
        Width, Height, Pawn->ControlsCursor.X, Pawn->ControlsCursor.Y, *IdText(Pawn->GetSelectedUnitIds()), Pawn->GetSelectedUnitId(), Pawn->Selection.GetSelectionRevision(),
        Pawn->ManualLeftPresses, Pawn->ManualLeftReleases, Pawn->ManualGroupPresses, Pawn->ManualTypeSelectPresses, Pawn->ManualRightPresses,
        PC->IsInputKeyDown(EKeys::LeftMouseButton), PC->IsInputKeyDown(EKeys::LeftShift),
        Pawn->BoxGesture.IsArmed(), Pawn->BoxGesture.IsDragging(), V->HasFocus(), V->IsForegroundWindow(), Pawn->LastRequestedSequence, Pawn->GroupOrders.NumPending(), *S.ComputeDigest(), CaptureRequested ? *CapturePath : TEXT("none"));
}
bool AProofBoxInputFixture::Verify()
{
    TArray<int32> Expected;
    int32 Primary = 17; uint64 Delta = 1;
    if (Case == 30) { Expected.Add(1); Primary = 1; Delta = 0; }
    else if (Case >= 19) { Expected.Add(17); Delta = Case <= 27 || Case == 29 ? 2 : 1; }
    else if (Case <= 2 || Case == 18) { Expected.Add(1); Primary = 1; }
    else if (Case >= 3 && Case <= 8)
    {
        for (int32 Id = 1; Id <= (Case == 7 ? 10 : 12); ++Id) Expected.Add(Id);
        if (Case == 7) { Expected.Add(16); Expected.Add(17); Primary = 16; } else Primary = 1;
    }
    else if (Case == 9) Primary = INDEX_NONE;
    else if (Case == 15 || Case == 16) { Primary = INDEX_NONE; Delta = 2; }
    else { Expected.Add(17); Delta = Case == 10 ? 1 : 0; }
    const int32 PressDelta = Pawn->ManualLeftPresses - PressBefore;
    const int32 ReleaseDelta = Pawn->ManualLeftReleases - ReleaseBefore;
    const int32 ExpectedClicks = Case >= 28 ? 0 : 1;
    const bool Counters = Case == 18 ? PressDelta >= 1 && PressDelta <= 2 && ReleaseDelta >= 1 && ReleaseDelta <= 2
        : PressDelta == ExpectedClicks && ReleaseDelta == ExpectedClicks;
    const bool KeyCounters = Pawn->ManualGroupPresses == GroupBefore + (Case >= 19 && Case <= 27 ? 2 : 0)
        && Pawn->ManualTypeSelectPresses == TypeBefore + (Case == 28 ? 1 : Case == 29 ? 2 : 0)
        && Pawn->ManualRightPresses == RightBefore + (Case == 30 ? 1 : 0);
    const bool Order = Pawn->LastRequestedSequence == SequenceBefore + (Case == 30 ? 1 : 0)
        && (Case != 30 || Pawn->LastRequestedType == TEXT("Move"));
    const bool Passed = Pawn->GetSelectedUnitIds() == Expected && Pawn->GetSelectedUnitId() == Primary
        && Pawn->Selection.GetSelectionRevision() == RevisionBefore + Delta && Counters && KeyCounters && Order
        && (Case != 29 || Pawn->RequestStatus.Contains(TEXT("battlefield")))
        && Pawn->GroupOrders.NumPending() == 0 && !Pawn->BoxGesture.IsArmed();
    UE_LOG(LogTemp, Display, TEXT("PROOF_BOX_RESULT case=%d name=%s passed=%d expected=[%s] expectedPrimary=%d expectedRevisionDelta=%llu nativePressDelta=%d nativeReleaseDelta=%d pointerSource=explicit-controls-cursor population=diagnostic-over-cap physicalInput=false"),
        Case, Names[Case], Passed ? 1 : 0, *IdText(Expected), Primary, Delta, PressDelta, ReleaseDelta);
    return Passed;
}
void AProofBoxInputFixture::TickKeyRegression()
{
    bool Sent = true;
    if (Case <= 27)
    {
        const FKey Keys[9] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
            EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
        const FKey Key = Keys[Case - 19];
        if (Step == 1) Sent = Edge(EKeys::LeftControl, true);
        if (Step == 2) Sent = Edge(Key, true);
        if (Step == 3) Sent = Edge(Key, false) && Edge(EKeys::LeftControl, false);
        if (Step == 4) { Pointer(Anchor); Sent = Edge(EKeys::LeftMouseButton, true); }
        if (Step == 5) Sent = Edge(EKeys::LeftMouseButton, false);
        if (Step == 6)
        {
            if (Pawn->GetSelectedUnitIds() != TArray<int32>({1}) || Pawn->GetSelectedUnitId() != 1)
            { Finish(false, TEXT("group-intermediate-native-click")); return; }
            Sent = Edge(Key, true);
        }
        if (Step == 7) Sent = Edge(Key, false);
        if (Step == 8)
        {
            Observe(TEXT("key-regression"));
            if (!Verify()) { Finish(false, TEXT("nine-group-bound-route")); return; }
            ++Case; Step = 0; return;
        }
    }
    else if (Case == 28 || Case == 29)
    {
        if (Step == 1 || (Step == 3 && Case == 29)) Sent = Edge(EKeys::T, true);
        if (Step == 2 || (Step == 4 && Case == 29)) Sent = Edge(EKeys::T, false);
        if ((Step == 3 && Case == 28) || Step == 5)
        {
            Observe(TEXT("key-regression"));
            if (!Verify()) { Finish(false, TEXT("type-bound-route")); return; }
            ++Case; Step = 0; return;
        }
    }
    else
    {
        if (Step == 1) { Pointer(FVector2D(760, 650)); Sent = Edge(EKeys::RightMouseButton, true); }
        if (Step == 2) Sent = Edge(EKeys::RightMouseButton, false);
        if (Step == 3)
        {
            Observe(TEXT("key-regression"));
            if (!Verify()) { Finish(false, TEXT("context-move-template-route-no-host-receipt")); return; }
            ++Case; Step = 0; Finish(true, TEXT("bounded-diagnostic-cases-complete")); return;
        }
    }
    if (!Sent) { Finish(false, TEXT("key-regression-edge")); return; }
    ++Step;
}
void AProofBoxInputFixture::Finish(bool Passed, const TCHAR* Reason)
{
    Done = true;
    UE_LOG(LogTemp, Display, TEXT("PROOF_BOX_FINISH passed=%d cases=%d elapsed=%.6f reason=%s unverified=real-pointer,physical-focus,capture,lost-release-occurrence,gameplay population=diagnostic-over-cap"),
        Passed ? 1 : 0, Case, FPlatformTime::Seconds() - Started, Reason);
    if (Pawn) Pawn->bControlsCursor = false;
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
}
void AProofBoxInputFixture::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Done) return;
    if (Started == 0.0) { Started = FPlatformTime::Seconds(); FParse::Value(FCommandLine::Get(), TEXT("ProofBoxInputOutput="), Output); }
    if (FPlatformTime::Seconds() - Started >= 220.0) { Finish(false, TEXT("scenario-deadline-or-focus-unavailable")); return; }
    if (!Pawn)
    {
        PC = Cast<AProofPlayerController>(GetWorld()->GetFirstPlayerController());
        Pawn = PC ? Cast<AProofRTSPawn>(PC->GetPawn()) : nullptr;
        if (Pawn) AddTickPrerequisiteActor(Pawn);
        return;
    }
    if (!Ready())
    {
        // Initial focus may arrive naturally. After interactions start, stop on
        // an unexpected loss. Never refocus the OS or retry the scenario.
        if (Step != 0 || Case != 0) Finish(false, TEXT("input-stack-or-actual-focus-lost"));
        return;
    }
    if (Step == 0)
    {
        CaptureRequested = false;
        if (Case == 0 && !Populate()) { Finish(false, TEXT("diagnostic-population")); return; }
        Baseline();
        if ((Case == 7 || Case == 10) && !Edge(EKeys::LeftShift, true)) { Finish(false, TEXT("shift-press")); return; }
        Step = 1; return;
    }
    if (LastEdgeFrame && PC->GetStopInputProcessedFrame(Pawn->InputComponent.Get()) <= LastEdgeFrame) return;
    if (Case >= 19) { TickKeyRegression(); return; }
    if (Step == 1)
    {
        if (!Edge(EKeys::LeftMouseButton, true)) { Finish(false, TEXT("press")); return; }
        Step = 2; return;
    }
    if (Step == 2)
    {
        Observe(TEXT("pressed"));
        if (Pawn->ManualLeftPresses != PressBefore + 1 || Pawn->Selection.GetSelectionRevision() != RevisionBefore || !PC->IsInputKeyDown(EKeys::LeftMouseButton))
        { Finish(false, TEXT("press-must-not-select")); return; }
        if (Case == 7 && !Edge(EKeys::LeftShift, false)) { Finish(false, TEXT("early-shift-release")); return; }
        if (Case == 8 && !Edge(EKeys::LeftShift, true)) { Finish(false, TEXT("late-shift-press")); return; }
        Pointer(Case == 2 ? Anchor + FVector2D(12, 0) : Case == 12 ? FVector2D(20, 20) : Case == 13 ? FVector2D(-1, 350) : End);
        if (Case == 0 || Case == 1)
        {
            if (!Edge(EKeys::LeftMouseButton, false)) { Finish(false, TEXT("release-only")); return; }
            Step = 5; return;
        }
        if (Case == 14) { FSystemResolution::RequestResolutionChange(960, 540, EWindowMode::Windowed); Step = 3; return; }
        if (Case == 15) PC->OwnerId = 2;
        if (Case == 16) ++GetWorld()->GetGameState<AProofGameState>()->Snapshot.MatchEpoch;
        if (Case == 17) GetWorld()->GetGameState<AProofGameState>()->Snapshot.MatchPhase = EProofMatchPhase::Finished;
        Step = 3; return;
    }
    if (Step == 3)
    {
        if (Case == 2 || Case == 3)
        {
            if (!CaptureRequested)
            {
                if (Output.IsEmpty()) { Finish(false, TEXT("capture-output-required")); return; }
                FBox2D Bounds;
                if (!Pawn->GetSelectionDragBounds(Bounds)) { Finish(false, TEXT("held-outline-required")); return; }
                CapturePath = FPaths::Combine(Output, FString::Printf(TEXT("box-held-%d.png"), Case));
                CaptureStarted = FPlatformTime::Seconds(); CaptureRequested = true;
                Observe(TEXT("capture-request"));
                FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
                return;
            }
            if (FScreenshotRequest::IsScreenshotRequested() || IFileManager::Get().FileSize(*CapturePath) <= 0)
            {
                if (FPlatformTime::Seconds() - CaptureStarted >= 20.0) Finish(false, TEXT("capture-deadline"));
                return;
            }
            Observe(TEXT("capture-complete"));
        }
        if (Case == 14)
        {
            int32 W = 0, H = 0; PC->GetViewportSize(W, H);
            if (W != 960 || H != 540) return;
            Observe(TEXT("resized"));
            FSystemResolution::RequestResolutionChange(1280, 720, EWindowMode::Windowed);
        }
        else Observe(TEXT("sampled"));
        if (Case == 2 && !Pawn->BoxGesture.IsDragging()) { Finish(false, TEXT("sticky-outward-sample")); return; }
        if (Case == 2) Pointer(Anchor);
        if (Case == 12 || Case == 13) Pointer(End); // Returning while held must not rearm.
        if (Case == 15) PC->OwnerId = 1;
        if (Case == 16) --GetWorld()->GetGameState<AProofGameState>()->Snapshot.MatchEpoch;
        if (Case == 17) GetWorld()->GetGameState<AProofGameState>()->Snapshot.MatchPhase = EProofMatchPhase::Playing;
        if (Case == 18 && !Edge(EKeys::LeftMouseButton, true)) { Finish(false, TEXT("duplicate-press")); return; }
        Step = 4; return;
    }
    if (Step == 4)
    {
        if (Case == 14) { int32 W = 0, H = 0; PC->GetViewportSize(W, H); if (W != 1280 || H != 720) return; }
        if (!Edge(EKeys::LeftMouseButton, false)) { Finish(false, TEXT("release")); return; }
        Step = 5; return;
    }
    if (Step == 5)
    {
        Observe(TEXT("released"));
        if (!Verify()) { Finish(false, TEXT("independent-fixed-ID-oracle")); return; }
        if ((Case == 8 || Case == 10) && !Edge(EKeys::LeftShift, false)) { Finish(false, TEXT("shift-cleanup")); return; }
        if (Case == 18) { if (!Edge(EKeys::LeftMouseButton, false)) { Finish(false, TEXT("duplicate-release")); return; } Step = 6; return; }
        ++Case; Step = 0;
    }
    else if (Step == 6)
    {
        Observe(TEXT("duplicate-released"));
        if (!Verify()) { Finish(false, TEXT("duplicate-release-inert")); return; }
        ++Case; Step = 0;
    }
    if (Case == CaseCount) Finish(true, TEXT("bounded-diagnostic-cases-complete"));
}
