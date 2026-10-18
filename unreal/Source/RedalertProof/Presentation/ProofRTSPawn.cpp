#include "Presentation/ProofRTSPawn.h"
#include "Presentation/ProofCameraPan.h"
#include "Presentation/ProofTypeSelectionAdapter.h"
#include "Presentation/ProofPointerBounds.h"
#include "Presentation/ProofReturnCargoSelection.h"
#include "Presentation/ProofSkirmishOwnershipRecovery.h"
#include "Presentation/ProofHUDPanelLayout.h"
#include "Presentation/ProofUnitViewGrounding.h"
#include "Presentation/ProofUnitViewFacing.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "AssetCompilingManager.h"
#include "InputCoreTypes.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

AProofRTSPawn::AProofRTSPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bInteractiveExitEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofInteractive"));
    bCameraPanEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofCameraPan"));
    bCameraEdgePanEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofCameraEdgePan"));
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ViewRoot"));
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("RTSCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->SetRelativeLocation(FVector(0.f, -3200.f, 3200.f));
    Camera->SetRelativeRotation(FRotator(-45.f, 90.f, 0.f));
    Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
    Camera->OrthoWidth = 5000.f;
    Camera->bConstrainAspectRatio = false;
}

void AProofRTSPawn::BeginPlay()
{
    Super::BeginPlay();
    bTypeSelectionControls = FParse::Param(FCommandLine::Get(), TEXT("ProofTypeSelectionControls"));
    bMapTypeSelectionControls = FParse::Param(FCommandLine::Get(), TEXT("ProofMapTypeSelectionControls"));
    if (bMapTypeSelectionControls && !bTypeSelectionControls) { FPlatformMisc::RequestExitWithStatus(false, 1); return; }
    if (bTypeSelectionControls && (FParse::Param(FCommandLine::Get(), TEXT("ProofStopControls")) || FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmish")) || !FParse::Param(FCommandLine::Get(), TEXT("ProofControlsAutomation"))))
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_TYPE_SELECTION_ERROR incompatible_fixture_flags"));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    AttackMoveRuntime.bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofAttackMoveFixture"));
    bGraphicsWarmup = FParse::Param(FCommandLine::Get(), TEXT("ProofGraphicsWarmup"));
    bStopControls = FParse::Param(FCommandLine::Get(), TEXT("ProofStopControls"));
    bStopInputBinding = FParse::Param(FCommandLine::Get(), TEXT("ProofStopInputBinding"));
    if (bStopInputBinding && (!bStopControls || bTypeSelectionControls
        || !FParse::Param(FCommandLine::Get(), TEXT("ProofControlsAutomation"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofControlsObserver"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofAutomation"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmish")) || bGraphicsWarmup))
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_STOP_INPUT_ERROR incompatible_fixture_flags"));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    ReturnCargoRuntime.bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofReturnCargoEvidence"));
    bGroupGatherEvidence = FParse::Param(FCommandLine::Get(), TEXT("ProofGroupGatherEvidence"));
    bFactoryRallyEvidence = FParse::Param(FCommandLine::Get(), TEXT("ProofFactoryRallyEvidence"));
    FactoryProduction.bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofFactoryProductionEvidence"));
    CoopContext.bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofCoopContextEvidence"));
    FParse::Value(FCommandLine::Get(), TEXT("ProofSkirmishScenario="), SkirmishScenario);
    FParse::Value(FCommandLine::Get(), TEXT("ProofSkirmishOutput="), SkirmishOutput);
    bHUDReadabilityCaptures = FParse::Param(FCommandLine::Get(), TEXT("ProofHUDReadabilityCaptures"));
    bCameraEdgeCaptures = FParse::Param(FCommandLine::Get(), TEXT("ProofCameraEdgeCaptures"));
    if (ReturnCargoRuntime.bEnabled && (SkirmishScenario != TEXT("Victory") || GetNetMode() != NM_Standalone
        || !FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmish"))
        || !FParse::Param(FCommandLine::Get(), TEXT("ProofRefineryMinerEvidence"))
        || bGroupGatherEvidence || bFactoryRallyEvidence || CoopContext.bEnabled
        || bHUDReadabilityCaptures || bCameraEdgeCaptures || bGraphicsWarmup
        || bTypeSelectionControls || bMapTypeSelectionControls || bStopControls || bStopInputBinding
        || AttackMoveRuntime.bEnabled || FParse::Param(FCommandLine::Get(), TEXT("ProofControlsAutomation"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofControlsObserver"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmishObserver"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofAutomation"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofLifecycleFixtureHost"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmishFixtureHost"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofAttackMoveFixtureHost"))))
    { UE_LOG(LogTemp, Error, TEXT("PROOF_RETURN_CARGO_ERROR incompatible_fixture_flags")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
    // Fixture-only viewport contract; defaults preserve the existing trace.
    FString FixtureViewport = TEXT("1280x720");
    FParse::Value(FCommandLine::Get(), TEXT("ProofSkirmishViewport="), FixtureViewport);
    if (FixtureViewport == TEXT("960x540"))
    {
        SkirmishViewportWidth = 960;
        SkirmishViewportHeight = 540;
    }
    else if (FixtureViewport != TEXT("1280x720")) bSkirmishViewportValid = false;
    // Optional generated content stays outside tracked source. Every record is
    // deliberately a generic proxy; there is no unit-kind field in this proof.
    if (GetNetMode() != NM_DedicatedServer)
    {
        ProxyMesh = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/BlockoutGenerated/Meshes/SM_RhinoTank_Blockout.SM_RhinoTank_Blockout"));
        bUsingBlockoutMesh = ProxyMesh != nullptr;
        if (FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmish")) || !SkirmishScenario.IsEmpty())
        {
            StructureMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/BlockoutGenerated/Meshes/SM_TeslaReactor_Blockout.SM_TeslaReactor_Blockout"));
            HarvesterMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        }
        if (!ProxyMesh)
        {
            ProxyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        }
    }
}

void AProofRTSPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    // Bound callbacks are the native input path. Default controls automation calls the
    // shared handlers directly; the opt-in Stop input fixture labels its engine edges.
    Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AProofRTSPawn::OnManualSelect);
    Input->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AProofRTSPawn::OnManualSelectRelease);
    Input->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AProofRTSPawn::OnManualOrder);
    Input->BindKey(EKeys::P, IE_Pressed, this, &AProofRTSPawn::OnManualProduce);
    Input->BindKey(EKeys::S, IE_Pressed, this, &AProofRTSPawn::OnManualStop);
    Input->BindKey(EKeys::R, IE_Pressed, this, &AProofRTSPawn::OnManualRefinery);
    Input->BindKey(EKeys::F, IE_Pressed, this, &AProofRTSPawn::OnManualFactory);
    Input->BindKey(EKeys::N, IE_Pressed, this, &AProofRTSPawn::OnManualRematch);
    Input->BindKey(EKeys::T, IE_Pressed, this, &AProofRTSPawn::OnManualTypeSelect);
    Input->BindKey(EKeys::T, IE_Released, this, &AProofRTSPawn::OnManualTypeRelease);
    Input->BindKey(EKeys::One, IE_Pressed, this, &AProofRTSPawn::OnManualGroupOne);
    Input->BindKey(EKeys::Two, IE_Pressed, this, &AProofRTSPawn::OnManualGroupTwo);
    Input->BindKey(EKeys::Three, IE_Pressed, this, &AProofRTSPawn::OnManualGroupThree);
    Input->BindKey(EKeys::Four, IE_Pressed, this, &AProofRTSPawn::OnManualGroupFour);
    Input->BindKey(EKeys::Five, IE_Pressed, this, &AProofRTSPawn::OnManualGroupFive);
    Input->BindKey(EKeys::Six, IE_Pressed, this, &AProofRTSPawn::OnManualGroupSix);
    Input->BindKey(EKeys::Seven, IE_Pressed, this, &AProofRTSPawn::OnManualGroupSeven);
    Input->BindKey(EKeys::Eight, IE_Pressed, this, &AProofRTSPawn::OnManualGroupEight);
    Input->BindKey(EKeys::Nine, IE_Pressed, this, &AProofRTSPawn::OnManualGroupNine);
    Input->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AProofRTSPawn::OnManualZoomIn);
    Input->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AProofRTSPawn::OnManualZoomOut);
    if (bInteractiveExitEnabled)
    {
        Input->BindKey(EKeys::Escape, IE_Pressed, this, &AProofRTSPawn::OnInteractiveEscape);
    }
}

void AProofRTSPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!PC || !PC->IsLocalController() || GetNetMode() == NM_DedicatedServer)
    {
        BoxGesture.Cancel();
        return;
    }
    if (!bConfiguredLocalInput && PC->GetLocalPlayer() && GetWorld()->GetGameViewport()
        && GetWorld()->GetGameViewport()->GetGameViewportWidget().IsValid())
    {
        PC->bShowMouseCursor = true;
        // UE 5.7 GameAndUI uses CaptureDuringMouseDown and forwards the first
        // mouse press. The viewport widget must exist before SetInputMode; do
        // not reapply it after focus changes or force the OS window forward.
        PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
        // Keep the review view at the fixture origin regardless of PlayerStart.
        Camera->SetWorldLocation(FVector(0.f, -3200.f, 3200.f));
        Camera->SetWorldRotation(FRotator(-45.f, 90.f, 0.f));
        bConfiguredLocalInput = true;
    }
    ObserveViewportFocus();
    if ((bCameraPanEnabled || (bCameraEdgePanEnabled && bInteractiveExitEnabled)) && bConfiguredLocalInput && bViewportFocused && bViewportForeground
        && !PC->IsProofAutomation() && !PC->IsControlsAutomation() && SkirmishScenario.IsEmpty())
    {
        FVector2D Direction = FVector2D::ZeroVector;
        if (bCameraPanEnabled)
        {
            Direction = FVector2D(
                (PC->IsInputKeyDown(EKeys::Right) ? 1.0 : 0.0) - (PC->IsInputKeyDown(EKeys::Left) ? 1.0 : 0.0),
                (PC->IsInputKeyDown(EKeys::Up) ? 1.0 : 0.0) - (PC->IsInputKeyDown(EKeys::Down) ? 1.0 : 0.0));
        }
        // Edge scrolling is opt-in manual practice only. Read the real viewport
        // pointer, never the fixture cursor, and leave mouse-button gestures alone.
        const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
        if (bCameraEdgePanEnabled && bInteractiveExitEnabled && !bGraphicsWarmup && !bControlsCursor
            && Viewport && Viewport->Viewport && PC->GetHUD())
        {
            int32 Width = 0, Height = 0;
            PC->GetViewportSize(Width, Height);
            float MouseX = 0.f, MouseY = 0.f;
            if (PC->GetMousePosition(MouseX, MouseY))
            {
                const AProofGameState* CameraState = GetWorld()->GetGameState<AProofGameState>();
                const FProofHUDPanelLayout Panel(static_cast<float>(Width), CameraState && CameraState->Snapshot.bSkirmish);
                const bool bMouseHeld = PC->IsInputKeyDown(EKeys::LeftMouseButton)
                    || PC->IsInputKeyDown(EKeys::RightMouseButton) || PC->IsInputKeyDown(EKeys::MiddleMouseButton);
                Direction += FProofCameraPan::EdgeInput(FVector2D(MouseX, MouseY), FVector2D(Width, Height),
                    Panel.Bounds(), true, Viewport->Viewport->HasFocus(), bViewportForeground, bMouseHeld);
            }
        }
        ApplyCameraPanInput(Direction, DeltaSeconds);
    }
    if (bInteractiveExitEnabled && bConfiguredLocalInput && !bInteractiveReadyLogged && PC->OwnerId != INDEX_NONE)
    {
        int32 Width = 0, Height = 0;
        PC->GetViewportSize(Width, Height);
        if (Width > 0 && Height > 0)
        {
            const IConsoleVariable* MaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
            UE_LOG(LogTemp, Display, TEXT("PROOF_INTERACTIVE_READY owner=%d width=%d height=%d maxFPS=%.3f physicalInput=unverified"),
                PC->OwnerId, Width, Height, MaxFPS ? MaxFPS->GetFloat() : -1.f);
            bInteractiveReadyLogged = true;
        }
    }
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!GS)
    {
        BoxGesture.Cancel();
        return;
    }
    if (LastSeenMatchEpoch != GS->Snapshot.MatchEpoch)
    {
        for (auto& Entry : UnitViews)
        {
            if (Entry.Value) Entry.Value->DestroyComponent();
        }
        UnitViews.Reset();
        SelectedUnitId = INDEX_NONE;
        LastRequestedSequence = 0;
        LastRequestedUnitId = INDEX_NONE;
        bLastRequestedSourceWasOwnedLive = false;
        LastRequestedTarget = INDEX_NONE;
        LastRequestedType.Reset();
        FactoryRallyStage = 0;
        FactoryRallyFactory = FactoryRallyUnit = INDEX_NONE;
        FactoryRallySetSequence = FactoryRallyProduceSequence = 0;
        FactoryRallySeenAt = FactoryRallySpawnSeenAt = FactoryRallyCaptureAt = -1.f;
        FactoryRallyBeforeIds.Reset();
        FactoryRallyProgressRevisions.Reset();
        const bool bCoopContextEnabled = CoopContext.bEnabled;
        CoopContext = FProofCoopContextRuntimeState();
        CoopContext.bEnabled = bCoopContextEnabled;
        RequestStatus = TEXT("Match state refreshed. Select an owned live unit.");
        LastSeenMatchEpoch = GS->Snapshot.MatchEpoch;
    }
    Selection.Synchronize(GS->Snapshot, PC->OwnerId);
    SelectedUnitId = Selection.GetPrimaryUnitId();
    TickBoxSelection();
    TSet<int32> LiveIds;
    bool bSelectionAliveAndOwned = false;
    for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
    {
        if (Unit.HitPoints <= 0.f)
        {
            continue;
        }
        LiveIds.Add(Unit.UnitId);
        bSelectionAliveAndOwned |= Unit.UnitId == SelectedUnitId && Unit.OwnerId == PC->OwnerId;
        TObjectPtr<UStaticMeshComponent>& View = UnitViews.FindOrAdd(Unit.UnitId);
        if (!View && ProxyMesh)
        {
            View = NewObject<UStaticMeshComponent>(this);
            View->SetMobility(EComponentMobility::Movable);
            View->SetStaticMesh(ProxyMesh);
            if (GS->Snapshot.bSkirmish)
            {
                const bool bStructure = Unit.Kind == EProofUnitKind::HQ || Unit.Kind == EProofUnitKind::Refinery || Unit.Kind == EProofUnitKind::Factory;
                if (bStructure && StructureMesh) View->SetStaticMesh(StructureMesh);
                else if (Unit.Kind == EProofUnitKind::Harvester && HarvesterMesh) View->SetStaticMesh(HarvesterMesh);
            }
            View->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            View->SetupAttachment(RootComponent);
            View->RegisterComponent();
            // Display-only size keeps the tiny correctness fixture readable.
            // This is not approved tank/infantry scale or gameplay collision.
            if (GS->Snapshot.bSkirmish)
            {
                const bool bStructure = Unit.Kind == EProofUnitKind::HQ || Unit.Kind == EProofUnitKind::Refinery || Unit.Kind == EProofUnitKind::Factory;
                View->SetWorldScale3D(bStructure && StructureMesh ? FVector(Unit.Kind == EProofUnitKind::HQ ? 0.45f : 0.3f)
                    : Unit.Kind == EProofUnitKind::Harvester ? FVector(1.0f, 0.7f, 0.7f) : FVector(0.6f));
            }
            else View->SetWorldScale3D(bUsingBlockoutMesh ? FVector(0.3f) : FVector(1.f, 1.f, 0.6f));
        }
        if (View && View->GetStaticMesh())
        {
            // This current imported Rhino's visual front is local +Y. Bind the
            // contract to the actual mesh and live Combat kind, including when
            // a structure or miner falls back to the same proxy mesh.
            if (Unit.Kind == EProofUnitKind::Combat && bUsingBlockoutMesh
                && View->GetStaticMesh() == ProxyMesh)
            {
                const FVector WorldForward = View->GetComponentTransform().GetScaledAxis(EAxis::Y);
                double DeltaYaw = 0.0;
                if (FProofUnitViewFacing::TryYawDelta(
                    {WorldForward.X, WorldForward.Y, WorldForward.Z},
                    {Unit.LastTravelDirection.X, Unit.LastTravelDirection.Y, Unit.LastTravelDirection.Z}, DeltaYaw)
                    && DeltaYaw != 0.0)
                {
                    View->SetWorldRotation(FQuat(FVector(0.0, 0.0, 1.0), DeltaYaw) * View->GetComponentQuat());
                }
            }
            // Each selected mesh has its own pivot and scaled/rotated lower bound.
            // Unit.Position remains the authoritative ground-plane position.
            const FBox LocalBounds = View->GetStaticMesh()->GetBoundingBox();
            const FTransform ViewTransform = View->GetComponentTransform();
            const double Lift = FProofUnitViewGrounding::VerticalOffset(
                {LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Min.Z},
                {LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Max.Z},
                {ViewTransform.GetScaledAxis(EAxis::X).Z, ViewTransform.GetScaledAxis(EAxis::Y).Z,
                    ViewTransform.GetScaledAxis(EAxis::Z).Z});
            View->SetWorldLocation(Unit.Position + FVector(0.0, 0.0, Lift));
        }
    }
    for (auto It = UnitViews.CreateIterator(); It; ++It)
    {
        if (!LiveIds.Contains(It.Key()))
        {
            if (It.Value())
            {
                It.Value()->DestroyComponent();
            }
            It.RemoveCurrent();
        }
    }
    if (!bSelectionAliveAndOwned)
    {
        SelectedUnitId = INDEX_NONE;
    }
    if (bGraphicsWarmup)
    {
        TickGraphicsWarmup();
        return;
    }
    if (ReturnCargoRuntime.bEnabled)
    {
        // A full intervening Pawn frame lets the local controller's ordinary
        // semantic snapshot logger see this revision, whatever actor tick order.
        ReturnCargoRuntime.bPublishedSnapshotPreviouslySeen = ReturnCargoRuntime.PublishedRevisionSeen == GS->Snapshot.Revision;
        ReturnCargoRuntime.PublishedRevisionSeen = GS->Snapshot.Revision;
    }
    for (const FProofCommand& Command : GroupOrders.PopReady(GS->Snapshot, PC->OwnerId, FPlatformTime::Seconds()))
    {
        Request(Command);
    }
    if (AttackMoveRuntime.bEnabled && PC->OwnerId == 1) TickAttackMoveRuntime(DeltaSeconds);
    else if (GS->Snapshot.bSkirmish && !SkirmishScenario.IsEmpty()) TickSkirmishScenario(DeltaSeconds);
    else if (PC->IsControlsAutomation()) TickControlsAutomation(DeltaSeconds);
}

FString AProofRTSPawn::GetManualInputStatus() const
{
    const TCHAR* Focus = !bFocusObserved ? TEXT("pending")
        : !bViewportForeground ? TEXT("background")
        : bViewportFocused ? TEXT("focused") : TEXT("unfocused");
    return FString::Printf(TEXT("Native L%d R%d S%d P%d W+%d/-%d LU%d | focus %s | last %s | human unverified"),
        ManualLeftPresses, ManualRightPresses, ManualStopPresses, ManualProducePresses,
        ManualWheelUpPresses, ManualWheelDownPresses, ManualLeftReleases, Focus, *LastManualAction);
}

void AProofRTSPawn::ObserveViewportFocus()
{
    const UGameViewportClient* GameViewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    const FViewport* Viewport = GameViewport ? GameViewport->Viewport : nullptr;
    if (!Viewport) { TypeGesture.Cancel(); BoxGesture.Cancel(); bViewportFocused = bViewportForeground = false; bFocusObserved = false; return; }
    const bool bFocused = Viewport->HasFocus();
    const bool bForeground = Viewport->IsForegroundWindow();
    if (!bFocused || !bForeground) { TypeGesture.Cancel(); BoxGesture.Cancel(); }
    if (!bFocusObserved || bFocused != bViewportFocused || bForeground != bViewportForeground)
    {
        bFocusObserved = true;
        bViewportFocused = bFocused;
        bViewportForeground = bForeground;
        if (FocusTransitions < MAX_int32) ++FocusTransitions;
        if (FocusTransitions <= 8)
        {
            UE_LOG(LogTemp, Display, TEXT("PROOF_MANUAL_FOCUS transition=%d viewportFocus=%d foreground=%d physicalInput=unverified"),
                FocusTransitions, bFocused ? 1 : 0, bForeground ? 1 : 0);
        }
        else if (FocusTransitions == 9)
        {
            UE_LOG(LogTemp, Display, TEXT("PROOF_MANUAL_FOCUS furtherTransitions=suppressed physicalInput=unverified"));
        }
    }
}

void AProofRTSPawn::RecordManualInput(const TCHAR* Key, int32& Count, const FString& Outcome)
{
    ObserveViewportFocus();
    if (Count < MAX_int32) ++Count;
    LastManualAction = FString::Printf(TEXT("%s %s"), Key, *Outcome.Left(56));
    if (ManualInputLogs < 24)
    {
        ++ManualInputLogs;
        UE_LOG(LogTemp, Display, TEXT("PROOF_MANUAL_INPUT key=%s count=%d outcome=%s viewportFocus=%d foreground=%d physicalInput=unverified"),
            Key, Count, *Outcome.Left(80), bViewportFocused ? 1 : 0, bViewportForeground ? 1 : 0);
        if (ManualInputLogs == 24)
        {
            UE_LOG(LogTemp, Display, TEXT("PROOF_MANUAL_INPUT furtherEvents=suppressed countersRemainVisible=1 physicalInput=unverified"));
        }
    }
}

void AProofRTSPawn::OnManualSelect()
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (GS && GS->Snapshot.bSkirmish)
    {
        const bool bArmed = BeginBoxSelection();
        RecordManualInput(TEXT("LMB"), ManualLeftPresses, bArmed ? TEXT("selection pending release") : TEXT("selection gesture ignored"));
        return;
    }
    BoxGesture.Cancel();
    if (PC && GS && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift)))
    {
        Selection.Toggle(GS->Snapshot, PC->OwnerId, FindUnitUnderCursor(true));
        SelectedUnitId = Selection.GetPrimaryUnitId();
        RequestStatus = FString::Printf(TEXT("%d owned live units selected."), Selection.GetSelectedUnitIds().Num());
    }
    else SelectUnderCursor();
    RecordManualInput(TEXT("LMB"), ManualLeftPresses,
        SelectedUnitId == INDEX_NONE ? TEXT("selection cleared") : FString::Printf(TEXT("selected unit %d"), SelectedUnitId));
}

void AProofRTSPawn::OnManualTypeSelect()
{
    ObserveViewportFocus();
    SelectTypeByGesture(FPlatformTime::Seconds(), bViewportFocused && bViewportForeground);
    // Only the bound callback records a native input edge. Shared actions and
    // model tests cannot produce a physical-input verification claim.
    RecordManualInput(TEXT("T"), ManualTypeSelectPresses, RequestStatus);
}

void AProofRTSPawn::OnManualTypeRelease() { TypeGesture.Release(); }

void AProofRTSPawn::SelectTypeByGesture(double WallSeconds, bool bFocused)
{
    BoxGesture.Cancel();
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    if (!PC || !GS || !PC->IsLocalController()) { TypeGesture.Cancel(); return; }
    const auto Scope = ProofTypeSelectionAdapter::Apply(Selection, TypeGesture, GS->Snapshot, PC->OwnerId,
        WallSeconds, bFocused, [this]() { return SelectVisibleType(); });
    SelectedUnitId = Selection.GetPrimaryUnitId();
    if (Scope == ProofTypeSelectGesture::Scope::Battlefield)
        RequestStatus = FString::Printf(TEXT("%d battlefield owned units of the same generic kind selected."), Selection.GetSelectedUnitIds().Num());
    else if (Scope == ProofTypeSelectGesture::Scope::Ignored)
        RequestStatus = TEXT("Type selection context or gesture changed; select a live owned unit and press T again.");
}

bool AProofRTSPawn::SelectVisibleType()
{
    BoxGesture.Cancel();
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    if (!PC || !GS || !PC->IsLocalController()) return false;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) { RequestStatus = TEXT("Type selection needs an active viewport."); return false; }
    TArray<int32> VisibleIds;
    for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
    {
        if (Unit.OwnerId != PC->OwnerId || !(Unit.HitPoints > 0.f)) continue;
        FVector2D Screen;
        if (PC->ProjectWorldLocationToScreen(Unit.Position, Screen)
            && FProofSelectionState::IsViewportCenterVisible(Screen, FIntPoint(Width, Height))) VisibleIds.Add(Unit.UnitId);
    }
    const bool bSelected = Selection.SelectVisibleType(GS->Snapshot, PC->OwnerId, VisibleIds);
    SelectedUnitId = Selection.GetPrimaryUnitId();
    RequestStatus = bSelected ? FString::Printf(TEXT("%d visible owned units of the same generic kind selected."), Selection.GetSelectedUnitIds().Num())
        : TEXT("Select an owned live unit before type selection.");
    return bSelected;
}
void AProofRTSPawn::OnManualGroupOne() { HandleManualGroup(1); }
void AProofRTSPawn::OnManualGroupTwo() { HandleManualGroup(2); }
void AProofRTSPawn::OnManualGroupThree() { HandleManualGroup(3); }
void AProofRTSPawn::OnManualGroupFour() { HandleManualGroup(4); }
void AProofRTSPawn::OnManualGroupFive() { HandleManualGroup(5); }
void AProofRTSPawn::OnManualGroupSix() { HandleManualGroup(6); }
void AProofRTSPawn::OnManualGroupSeven() { HandleManualGroup(7); }
void AProofRTSPawn::OnManualGroupEight() { HandleManualGroup(8); }
void AProofRTSPawn::OnManualGroupNine() { HandleManualGroup(9); }

void AProofRTSPawn::HandleManualGroup(int32 Slot)
{
    BoxGesture.Cancel();
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS) return;
    const bool bAssign = PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl);
    if (bAssign) Selection.AssignGroup(Slot, GS->Snapshot, PC->OwnerId);
    else Selection.RecallGroup(Slot, GS->Snapshot, PC->OwnerId);
    SelectedUnitId = Selection.GetPrimaryUnitId();
    RequestStatus = FString::Printf(TEXT("Group %d %s: %d owned live units."), Slot,
        bAssign ? TEXT("assigned") : TEXT("recalled"), Selection.GetSelectedUnitIds().Num());
    RecordManualInput(TEXT("Group"), ManualGroupPresses, RequestStatus);
}

void AProofRTSPawn::OnManualOrder()
{
    BoxGesture.Cancel();
    const int32 BeforeSequence = LastRequestedSequence;
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const bool bControl = PC && (PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl));
    const bool bShift = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
    if (bControl && bShift) AttackMoveUnderCursor();
    else OrderUnderCursor();
    RecordManualInput(TEXT("RMB"), ManualRightPresses,
        LastRequestedSequence != BeforeSequence
            ? FString::Printf(TEXT("%s request #%d"), *LastRequestedType, LastRequestedSequence)
            : RequestStatus);
}

void AProofRTSPawn::OnManualProduce()
{
    BoxGesture.Cancel();
    const int32 BeforeSequence = LastRequestedSequence;
    Produce();
    RecordManualInput(TEXT("P"), ManualProducePresses,
        LastRequestedSequence != BeforeSequence
            ? FString::Printf(TEXT("Produce request #%d"), LastRequestedSequence)
            : RequestStatus);
}

void AProofRTSPawn::OnManualStop()
{
    BoxGesture.Cancel();
    StopSelected();
    RecordManualInput(TEXT("S"), ManualStopPresses, RequestStatus);
}

void AProofRTSPawn::OnManualZoomIn()
{
    BoxGesture.Cancel();
    ZoomIn();
    RecordManualInput(TEXT("WheelUp"), ManualWheelUpPresses,
        FString::Printf(TEXT("ortho %.0f"), Camera->OrthoWidth));
}

void AProofRTSPawn::OnManualZoomOut()
{
    BoxGesture.Cancel();
    ZoomOut();
    RecordManualInput(TEXT("WheelDown"), ManualWheelDownPresses,
        FString::Printf(TEXT("ortho %.0f"), Camera->OrthoWidth));
}

void AProofRTSPawn::OnInteractiveEscape()
{
    BoxGesture.Cancel();
    if (!bInteractiveExitEnabled) return;
    UE_LOG(LogTemp, Display, TEXT("PROOF_MANUAL_EXIT key=Escape physicalInput=unverified"));
    FPlatformMisc::RequestExit(false);
}

void AProofRTSPawn::OnManualRematch()
{
    BoxGesture.Cancel();
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS || !GS->Snapshot.bSkirmish || PC->OwnerId != 1
        || GS->Snapshot.MatchPhase != EProofMatchPhase::Finished)
    {
        RequestStatus = TEXT("Owner 1 can request a rematch after the match ends.");
    }
    else
    {
        PC->ServerRequestRematch(GS->Snapshot.MatchEpoch);
        RequestStatus = TEXT("Rematch requested; waiting for the host's new match state.");
    }
    RecordManualInput(TEXT("N"), ManualRematchPresses, RequestStatus);
}

bool AProofRTSPawn::GetPointerScreenPosition(FVector2D& OutPoint) const
{
    const APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return false;
    FVector2D Point = ControlsCursor;
    if (!bControlsCursor)
    {
        float X = 0.f, Y = 0.f;
        if (!PC->GetMousePosition(X, Y)) return false;
        Point = FVector2D(X, Y);
    }
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    if (!FProofPointerBounds::Contains(Point.X, Point.Y, 0.0, 0.0, Width, Height)) return false;
    OutPoint = Point;
    return true;
}

int32 AProofRTSPawn::FindUnitUnderCursor(bool bOwnOnly) const
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    FVector2D Pointer;
    if (!PC || !GS || !GetPointerScreenPosition(Pointer))
    {
        return INDEX_NONE;
    }
    float BestDistanceSquared = FMath::Square(40.f);
    int32 BestId = INDEX_NONE;
    for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
    {
        if (Unit.HitPoints <= 0.f || (bOwnOnly && Unit.OwnerId != PC->OwnerId))
        {
            continue;
        }
        FVector2D Screen;
        if (PC->ProjectWorldLocationToScreen(Unit.Position, Screen))
        {
            const float DistanceSquared = FVector2D::DistSquared(Screen, Pointer);
            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                BestId = Unit.UnitId;
            }
        }
    }
    return BestId;
}

bool AProofRTSPawn::PointerWorldRay(FVector& OutOrigin, FVector& OutDirection) const
{
    const APlayerController* PC = Cast<APlayerController>(GetController());
    FVector2D Pointer;
    const ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
    if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
        || !GetPointerScreenPosition(Pointer) || !FMath::IsFinite(Pointer.X) || !FMath::IsFinite(Pointer.Y)) return false;

    FSceneViewProjectionData Projection;
    if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection)) return false;
    const FIntRect Rect = Projection.GetConstrainedViewRect();
    if (!FProofPointerBounds::Contains(Pointer.X, Pointer.Y, Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y)) return false;
    const FMatrix ViewProjection = Projection.ComputeViewProjectionMatrix();
    const double Determinant = ViewProjection.Determinant();
    if (!FMath::IsFinite(Determinant) || Determinant == 0.0) return false;
    const FMatrix Inverse = ViewProjection.Inverse();
    // Preserve fractional viewport coordinates. The native convenience helper
    // truncates them to pixels, which loses centimeters in this orthographic view.
    const double ClipX = 2.0 * (Pointer.X - Rect.Min.X) / Rect.Width() - 1.0;
    const double ClipY = 1.0 - 2.0 * (Pointer.Y - Rect.Min.Y) / Rect.Height();
    const FVector4 Near = Inverse.TransformFVector4(FVector4(ClipX, ClipY, 1.0, 1.0));
    const FVector4 Far = Inverse.TransformFVector4(FVector4(ClipX, ClipY, 0.01, 1.0));
    const auto IsFinite = [](const FVector4& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z) && FMath::IsFinite(Value.W);
    };
    if (!IsFinite(Near) || !IsFinite(Far) || FMath::IsNearlyZero(Near.W) || FMath::IsNearlyZero(Far.W)) return false;
    OutOrigin = FVector(Near.X / Near.W, Near.Y / Near.W, Near.Z / Near.W);
    const FVector OtherPoint(Far.X / Far.W, Far.Y / Far.W, Far.Z / Far.W);
    OutDirection = (OtherPoint - OutOrigin).GetSafeNormal();
    return !OutOrigin.ContainsNaN() && !OutDirection.ContainsNaN() && !OutDirection.IsNearlyZero();
}

bool AProofRTSPawn::CursorGroundPoint(FVector& OutPoint) const
{
    FVector Origin, Direction;
    if (!PointerWorldRay(Origin, Direction) || FMath::IsNearlyZero(Direction.Z))
    {
        return false;
    }
    const double Distance = -Origin.Z / Direction.Z;
    if (Distance < 0.0)
    {
        return false;
    }
    OutPoint = Origin + Direction * Distance;
    OutPoint.Z = 0.f;
    return !OutPoint.ContainsNaN();
}

void AProofRTSPawn::SelectUnderCursor()
{
    BoxGesture.Cancel();
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS) return;
    FVector2D Pointer;
    if (!GetPointerScreenPosition(Pointer)) return;
    Selection.SelectSingle(GS->Snapshot, PC->OwnerId, FindUnitUnderCursor(true));
    SelectedUnitId = Selection.GetPrimaryUnitId();
    RequestStatus = SelectedUnitId == INDEX_NONE ? TEXT("No owned live unit selected.") : TEXT("Owned unit selected. Right-click ground or an enemy.");
    const FProofUnitRecord* Selected = GS->Snapshot.Units.FindByPredicate(
        [this](const FProofUnitRecord& Unit) { return Unit.UnitId == SelectedUnitId; });
    if (GS->Snapshot.bSkirmish && Selected && Selected->Kind == EProofUnitKind::Harvester)
        RequestStatus = TEXT("Harvester: right-click ore to mine, your refinery to unload.");
}

int32 AProofRTSPawn::FindResourceUnderCursor(bool bAllowDepleted) const
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    FVector2D Pointer;
    if (!PC || !GS || !GetPointerScreenPosition(Pointer)) return INDEX_NONE;
    double BestDistance = FMath::Square(32.0);
    int32 ResourceId = INDEX_NONE;
    for (const FProofResourceRecord& Resource : GS->Snapshot.Resources)
    {
        FVector2D Screen;
        if ((Resource.Remaining > 0 || bAllowDepleted) && PC->ProjectWorldLocationToScreen(Resource.Position, Screen))
        {
            const double Distance = FVector2D::DistSquared(Pointer, Screen);
            if (Distance < BestDistance) { BestDistance = Distance; ResourceId = Resource.ResourceId; }
        }
    }
    return ResourceId;
}

void AProofRTSPawn::OrderUnderCursor()
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS || SelectedUnitId == INDEX_NONE)
    {
        RequestStatus = TEXT("Select an owned live unit first.");
        return;
    }
    if (Selection.GetSelectedUnitIds().Num() > 1)
    {
        if (PC->IsProofAutomation()) { RequestStatus = TEXT("Manual orders disabled during automation."); return; }
        const int32 TargetId = FindUnitUnderCursor(false);
        TArray<FProofCommand> Commands;
        if (FProofSelectionState::IsHostileTarget(GS->Snapshot, PC->OwnerId, TargetId))
            Commands = Selection.PlanAttack(GS->Snapshot, PC->OwnerId, TargetId);
        else if (Selection.IsAllHarvesters(GS->Snapshot, PC->OwnerId)
            && FProofReturnCargoSelection::IsOwnedLiveRefinery(GS->Snapshot, PC->OwnerId, TargetId))
            Commands = FProofReturnCargoSelection::Plan(GS->Snapshot, PC->OwnerId, Selection, TargetId);
        else
        {
            int32 ResourceId = INDEX_NONE;
            if (GS->Snapshot.bSkirmish && Selection.IsAllHarvesters(GS->Snapshot, PC->OwnerId))
            {
                const bool bLoaded = GS->Snapshot.Units.ContainsByPredicate([this](const FProofUnitRecord& Unit)
                    { return Selection.GetSelectedUnitIds().Contains(Unit.UnitId) && Unit.Cargo > 0; });
                ResourceId = FindResourceUnderCursor(bLoaded);
            }
            if (ResourceId != INDEX_NONE) Commands = Selection.PlanGather(GS->Snapshot, PC->OwnerId, ResourceId);
            else
            {
                FVector Destination;
                if (!CursorGroundPoint(Destination)) { RequestStatus = TEXT("Cursor did not project onto the ground."); return; }
                Commands = Selection.PlanMove(GS->Snapshot, PC->OwnerId, Destination);
            }
        }
        if (Commands.IsEmpty()) RequestStatus = TEXT("No eligible selected units for that order.");
        else RequestStatus = GroupOrders.Enqueue(GS->Snapshot, PC->OwnerId, Commands)
            ? FString::Printf(TEXT("Queued %d unit orders; waiting for host snapshots."), Commands.Num())
            : TEXT("Order queue full; retry after pending orders send.");
        return;
    }
    FProofCommand Command;
    Command.UnitId = SelectedUnitId;
    const FProofUnitRecord* Selected = GS->Snapshot.Units.FindByPredicate(
        [this](const FProofUnitRecord& Unit) { return Unit.UnitId == SelectedUnitId; });
    if (GS->Snapshot.bSkirmish && Selected && Selected->Kind == EProofUnitKind::Factory)
    {
        const int32 PointedId = FindUnitUnderCursor(false);
        if (FProofSelectionState::IsHostileTarget(GS->Snapshot, PC->OwnerId, PointedId))
        {
            RequestStatus = TEXT("Factory rally requires ground; right-click away from enemy units.");
            return;
        }
        FVector GroundPoint;
        if (!CursorGroundPoint(GroundPoint)) { RequestStatus = TEXT("Cursor did not project onto the ground."); return; }
        SetFactoryRally(GroundPoint);
        return;
    }
    if (GS->Snapshot.bSkirmish && Selected && Selected->Kind == EProofUnitKind::Harvester)
    {
        const int32 RefineryId = FindUnitUnderCursor(false);
        if (FProofReturnCargoSelection::IsOwnedLiveRefinery(GS->Snapshot, PC->OwnerId, RefineryId))
        {
            const TArray<FProofCommand> Returns = FProofReturnCargoSelection::Plan(GS->Snapshot, PC->OwnerId, Selection, RefineryId);
            if (Returns.Num() == 1) Request(Returns[0]);
            else RequestStatus = TEXT("Select a loaded owned Harvester to unload at this refinery.");
            return;
        }
        const int32 ResourceId = FindResourceUnderCursor(Selected->Cargo > 0);
        if (ResourceId != INDEX_NONE)
        {
            Command.Type = EProofCommandType::Gather;
            Command.TargetUnitId = ResourceId;
            Request(Command);
            return;
        }
    }
    const int32 TargetId = FindUnitUnderCursor(false);
    if (FProofSelectionState::IsHostileTarget(GS->Snapshot, PC->OwnerId, TargetId))
    {
        Command.Type = EProofCommandType::Attack;
        Command.TargetUnitId = TargetId;
    }
    else
    {
        Command.Type = EProofCommandType::Move;
        if (!CursorGroundPoint(Command.Destination))
        {
            RequestStatus = TEXT("Cursor did not project onto the ground.");
            return;
        }
    }
    Request(Command);
}

bool AProofRTSPawn::SetFactoryRally(const FVector& GroundPoint)
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    if (!PC || !PC->IsLocalController() || !GS || PC->IsProofAutomation() || !GS->Snapshot.bSkirmish
        || GS->Snapshot.MatchPhase != EProofMatchPhase::Playing || Selection.GetSelectedUnitIds().Num() != 1)
    {
        RequestStatus = TEXT("Select one owned live factory to set a rally point.");
        return false;
    }
    const FProofUnitRecord* Factory = GS->Snapshot.Units.FindByPredicate([this, PC](const FProofUnitRecord& Unit)
        { return Unit.UnitId == SelectedUnitId && Unit.OwnerId == PC->OwnerId
            && Unit.Kind == EProofUnitKind::Factory && Unit.HitPoints > 0.f; });
    if (!Factory) { RequestStatus = TEXT("Select one owned live factory to set a rally point."); return false; }
    FProofCommand Command;
    Command.Type = EProofCommandType::SetRally;
    Command.UnitId = Factory->UnitId;
    Command.Destination = GroundPoint;
    const int32 BeforeSequence = LastRequestedSequence;
    Request(Command);
    return LastRequestedSequence > BeforeSequence && LastRequestedType == TEXT("SetRally");
}

void AProofRTSPawn::AttackMoveUnderCursor()
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    if (!PC || !PC->IsLocalController() || PC->OwnerId == INDEX_NONE || !GS || PC->IsProofAutomation())
    { RequestStatus = TEXT("Attack Move requires a current manual player and match state."); return; }
    FVector Destination;
    if (!CursorGroundPoint(Destination))
    { RequestStatus = TEXT("Cursor did not project onto the ground."); return; }
    const TArray<FProofCommand> Commands = Selection.PlanAttackMove(GS->Snapshot, PC->OwnerId, Destination);
    if (Commands.IsEmpty())
    { RequestStatus = TEXT("Select owned live Combat units in a playing skirmish for Attack Move."); return; }
    RequestStatus = GroupOrders.Enqueue(GS->Snapshot, PC->OwnerId, Commands)
        ? FString::Printf(TEXT("Queued Attack Move for %d units; waiting for host snapshots."), Commands.Num())
        : TEXT("Attack Move queue full; previous pending orders retained.");
}

void AProofRTSPawn::StopSelected()
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    if (!PC || !PC->IsLocalController() || PC->OwnerId == INDEX_NONE || !GS
        || !GS->Snapshot.Players.ContainsByPredicate([PC](const FProofPlayerRecord& Player) { return Player.OwnerId == PC->OwnerId; }))
    {
        RequestStatus = TEXT("Waiting for the host's current player and match state.");
        return;
    }
    if (PC->IsProofAutomation())
    {
        RequestStatus = TEXT("Manual orders are disabled during the scripted automation fixture.");
        return;
    }
    const TArray<FProofCommand> Commands = Selection.PlanStop(GS->Snapshot, PC->OwnerId);
    if (Commands.IsEmpty())
    {
        RequestStatus = TEXT("No eligible owned mobile units selected for Stop.");
        return;
    }
    RequestStatus = GroupOrders.Enqueue(GS->Snapshot, PC->OwnerId, Commands)
        ? FString::Printf(TEXT("Queued Stop for %d units; waiting for host snapshots."), Commands.Num())
        : TEXT("Stop queue full; previous pending orders retained. Retry after orders send.");
}

void AProofRTSPawn::Produce()
{
    FProofCommand Command;
    Command.Type = EProofCommandType::Produce;
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (GS && GS->Snapshot.bSkirmish && PC)
    {
        for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
        {
            if (Unit.OwnerId == PC->OwnerId && Unit.HitPoints > 0.f && Unit.Kind == EProofUnitKind::Factory)
            {
                if (Command.UnitId == INDEX_NONE || Unit.UnitId == SelectedUnitId)
                    Command.UnitId = Unit.UnitId;
                if (Unit.UnitId == SelectedUnitId) break;
            }
        }
        if (Command.UnitId == INDEX_NONE) { RequestStatus = TEXT("Build a live factory first (F at cursor, 150 credits)."); return; }
        SkirmishProduceRecordsBefore = 0;
        for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
            if (Unit.OwnerId == PC->OwnerId && Unit.Kind == EProofUnitKind::Combat) ++SkirmishProduceRecordsBefore;
    }
    Request(Command);
}

void AProofRTSPawn::Request(FProofCommand Command)
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!PC || !PC->IsLocalController() || PC->OwnerId == INDEX_NONE)
    {
        RequestStatus = TEXT("Waiting for host-assigned owner.");
        return;
    }
    // No prediction, spawning, health or credit edits occur here.
    if (PC->IsProofAutomation())
    {
        RequestStatus = TEXT("Manual orders are disabled during the scripted automation fixture.");
        return;
    }
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!GS || !GS->Snapshot.Players.ContainsByPredicate([PC](const FProofPlayerRecord& Player) { return Player.OwnerId == PC->OwnerId; })
        || (GS->Snapshot.bSkirmish && GS->Snapshot.MatchEpoch <= 0))
    {
        RequestStatus = TEXT("Waiting for the host's current player and match state.");
        return;
    }
    Command.MatchEpoch = GS->Snapshot.MatchEpoch;
    Command.Sequence = PC->NextLocalCommandSequence();
    if (Command.Sequence <= 0)
    {
        RequestStatus = TEXT("Command sequence exhausted; reconnect for a new fixture session.");
        return;
    }
    if (Command.Type == EProofCommandType::Move || Command.Type == EProofCommandType::Attack
        || Command.Type == EProofCommandType::Gather || Command.Type == EProofCommandType::Stop
        || Command.Type == EProofCommandType::AttackMove || Command.Type == EProofCommandType::ReturnCargo) GroupOrders.CancelUnit(Command.UnitId);
    // Capture the exact replicated source before submission; a local authority
    // may publish a newer snapshot synchronously while handling the request.
    const FProofUnitRecord* Source = GS->Snapshot.Units.FindByPredicate(
        [&Command](const FProofUnitRecord& Unit) { return Unit.UnitId == Command.UnitId; });
    const bool bSourceWasOwnedLive = Source && Source->OwnerId == PC->OwnerId && Source->HitPoints > 0.f;
    PC->ServerSubmitCommand(Command);
    LastRequestedSequence = Command.Sequence;
    LastRequestedUnitId = Command.UnitId;
    bLastRequestedSourceWasOwnedLive = bSourceWasOwnedLive;
    LastRequestedTarget = Command.TargetUnitId;
    LastRequestedType = Command.Type == EProofCommandType::Move ? TEXT("Move")
        : Command.Type == EProofCommandType::Attack ? TEXT("Attack")
        : Command.Type == EProofCommandType::BuildRefinery ? TEXT("BuildRefinery")
        : Command.Type == EProofCommandType::BuildFactory ? TEXT("BuildFactory")
        : Command.Type == EProofCommandType::Gather ? TEXT("Gather")
        : Command.Type == EProofCommandType::Stop ? TEXT("Stop")
        : Command.Type == EProofCommandType::SetRally ? TEXT("SetRally")
        : Command.Type == EProofCommandType::AttackMove ? TEXT("AttackMove")
        : Command.Type == EProofCommandType::ReturnCargo ? TEXT("ReturnCargo") : TEXT("Produce");
    if (GS && GS->Snapshot.bSkirmish)
        UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_REQUEST owner=%d sequence=%d type=%s unit=%d target=%d"),
            PC->OwnerId, Command.Sequence, *LastRequestedType, Command.UnitId, Command.TargetUnitId);
    if (PC->IsControlsAutomation())
    {
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_REQUEST owner=%d sequence=%d type=%s unit=%d target=%d destinationX=%.3f destinationY=%.3f destinationZ=%.3f"),
            PC->OwnerId, Command.Sequence, *LastRequestedType, Command.UnitId, Command.TargetUnitId,
            Command.Destination.X, Command.Destination.Y, Command.Destination.Z);
    }
    RequestStatus = FString::Printf(TEXT("Request #%d sent; only host snapshots change units/credits."), Command.Sequence);
}

void AProofRTSPawn::ZoomIn() { Camera->SetOrthoWidth(FMath::Max(2000.f, Camera->OrthoWidth - 500.f)); }
void AProofRTSPawn::ZoomOut() { Camera->SetOrthoWidth(FMath::Min(18000.f, Camera->OrthoWidth + 500.f)); }

void AProofRTSPawn::TickGraphicsWarmup()
{
    const APlayerController* PC = Cast<APlayerController>(GetController());
    int32 Width = 0, Height = 0;
    if (PC) PC->GetViewportSize(Width, Height);
    const int32 Remaining = FAssetCompilingManager::Get().GetNumRemainingAssets();
    const double Now = FPlatformTime::Seconds();
    const bool bReady = Width == 1280 && Height == 720 && Remaining == 0;
    if (!bReady)
    {
        WarmupIdleStart = -1.0;
        WarmupIdleFrames = 0;
    }
    else
    {
        if (WarmupIdleStart < 0.0) WarmupIdleStart = Now;
        ++WarmupIdleFrames;
    }
    const double IdleSeconds = WarmupIdleStart < 0.0 ? 0.0 : Now - WarmupIdleStart;
    if (Now - WarmupLastLog >= 1.0)
    {
        UE_LOG(LogTemp, Display, TEXT("PROOF_GRAPHICS_WARMUP_PENDING remaining=%d idleSeconds=%.3f frames=%d width=%d height=%d"),
            Remaining, IdleSeconds, WarmupIdleFrames, Width, Height);
        WarmupLastLog = Now;
    }
    if (bReady && IdleSeconds >= 2.0 && WarmupIdleFrames >= 3)
    {
        const IConsoleVariable* MaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
        UE_LOG(LogTemp, Display, TEXT("PROOF_GRAPHICS_WARMUP_COMPLETE status=pass remaining=0 idleSeconds=%.3f frames=%d width=%d height=%d maxFPS=%.3f"),
            IdleSeconds, WarmupIdleFrames, Width, Height, MaxFPS ? MaxFPS->GetFloat() : -1.f);
        bGraphicsWarmup = false;
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
}

bool AProofRTSPawn::SetControlsCursorFromWorld(const FVector& Position)
{
    const APlayerController* PC = Cast<APlayerController>(GetController());
    int32 Width = 0, Height = 0;
    if (!PC) return false;
    PC->GetViewportSize(Width, Height);
    if (!PC->ProjectWorldLocationToScreen(Position, ControlsCursor)) return false;
    bControlsCursor = true;
    return ControlsCursor.X >= 0.f && ControlsCursor.X < Width && ControlsCursor.Y >= 140.f && ControlsCursor.Y < Height;
}

bool AProofRTSPawn::ControlsCheck(bool bCondition, const TCHAR* Evidence)
{
    if (bCondition && ControlsPassedEvidence.Contains(Evidence)) return true;
    if (bCondition) ControlsPassedEvidence.Add(Evidence);
    UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_CHECK step=%d status=%s evidence=%s elapsed=%.3f"),
        ControlsStep, bCondition ? TEXT("pass") : TEXT("fail"), Evidence, ControlsElapsed);
    if (!bCondition) FinishControls(false, Evidence);
    return bCondition;
}

void AProofRTSPawn::FinishControls(bool bPass, const TCHAR* Reason)
{
    if (bControlsFinished) return;
    bControlsFinished = true;
    UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_COMPLETE role=client owner=%d status=%s reason=%s elapsed=%.3f physicalInput=unverified"),
        Cast<AProofPlayerController>(GetController())->OwnerId, bPass ? TEXT("pass") : TEXT("fail"), Reason, ControlsElapsed);
    FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
}

void AProofRTSPawn::CaptureControls(const TCHAR* Name)
{
    const FString Path = FPaths::Combine(ControlsOutput, FString(Name) + TEXT(".png"));
    FScreenshotRequest::RequestScreenshot(Path, true, false);
    UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_CAPTURE requested=%s elapsed=%.3f"), *Path, ControlsElapsed);
}

void AProofRTSPawn::TickControlsAutomation(float DeltaSeconds)
{
    if (bControlsFinished) return;
    ControlsElapsed += DeltaSeconds;
    if (ControlsElapsed > 55.f) { FinishControls(false, TEXT("timeout")); return; }
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS || PC->OwnerId == INDEX_NONE) return;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    if (Width < 1000 || Height < 600 || !PC->GetHUD()) return;
    const FProofSnapshot& Snapshot = GS->Snapshot;
    const auto* Own = Snapshot.Units.FindByPredicate([this, PC](const FProofUnitRecord& Unit)
        { return ControlsOwnUnit == INDEX_NONE ? Unit.OwnerId == PC->OwnerId : Unit.UnitId == ControlsOwnUnit; });
    const auto* Target = Snapshot.Units.FindByPredicate([this](const FProofUnitRecord& Unit)
        { return ControlsTargetUnit == INDEX_NONE ? Unit.OwnerId == 100 && Unit.Position.IsNearlyZero(1.f) : Unit.UnitId == ControlsTargetUnit; });
    if (!Own || !Target) return;
    const auto* PlayerRecord = Snapshot.Players.FindByPredicate([PC](const FProofPlayerRecord& Record) { return Record.OwnerId == PC->OwnerId; });
    if (!PlayerRecord) return;
    if (ControlsReady < 0.f)
    {
        ControlsReady = ControlsElapsed;
        ControlsOwnUnit = Own->UnitId;
        ControlsTargetUnit = Target->UnitId;
        ControlsInitialPosition = Own->Position;
        ControlsCredits = PlayerRecord->Credits;
        for (const auto& Unit : Snapshot.Units) if (Unit.OwnerId == PC->OwnerId) ++ControlsOwnCount;
        if (!FParse::Value(FCommandLine::Get(), TEXT("ProofControlsOutput="), ControlsOutput))
            ControlsOutput = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../generated/controls")));
        IFileManager::Get().MakeDirectory(*ControlsOutput, true);
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_VIEW_READY owner=%d width=%d height=%d own=%d target=%d elapsed=%.3f"),
            PC->OwnerId, Width, Height, ControlsOwnUnit, ControlsTargetUnit, ControlsElapsed);
        const IConsoleVariable* OptimizeShaders = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));
        const IConsoleVariable* MaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
        const IConsoleVariable* ResolverDisabled = IConsoleManager::Get().FindConsoleVariable(TEXT("net.IpConnectionDisableResolution"));
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_RENDER_CONFIG shadersOptimize=%d maxFPS=%.3f resolverDisabled=%d"),
            OptimizeShaders ? OptimizeShaders->GetInt() : -1, MaxFPS ? MaxFPS->GetFloat() : -1.f,
            ResolverDisabled ? ResolverDisabled->GetInt() : -1);
        return;
    }
    FProofCommandResult Receipt;
    const bool bHasReceipt = PC->FindReceipt(LastRequestedSequence, Receipt);
    if (ControlsScenarioReady < 0.f)
    {
        bool bOwner1 = false, bOwner2 = false, bBot100 = false;
        for (const FProofPlayerRecord& Record : Snapshot.Players)
        {
            bOwner1 |= Record.OwnerId == 1;
            bOwner2 |= Record.OwnerId == 2;
            bBot100 |= Record.OwnerId == 100;
        }
        if (!bOwner1 || !bOwner2 || !bBot100) return;
        ControlsScenarioReady = ControlsElapsed;
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_SCENARIO_READY owner=%d elapsed=%.3f viewportReadyElapsed=%.3f revision=%d"),
            PC->OwnerId, ControlsScenarioReady, ControlsReady, Snapshot.Revision);
    }
    if (ControlsStep == 0 && ControlsElapsed - ControlsScenarioReady > 1.f)
    {
        if (!ControlsCheck(SetControlsCursorFromWorld(Target->Position), TEXT("enemy_projected_in_viewport"))) return;
        SelectUnderCursor();
        if (!ControlsCheck(SelectedUnitId == INDEX_NONE, TEXT("enemy_not_selected"))) return;
        if (!ControlsCheck(SetControlsCursorFromWorld(Own->Position), TEXT("own_projected_in_viewport"))) return;
        SelectUnderCursor();
        if (!ControlsCheck(SelectedUnitId == ControlsOwnUnit, TEXT("owned_unit_selected"))) return;
        if (!ControlsCheck(SetControlsCursorFromWorld(FVector(-1400.f, -800.f, 0.f)), TEXT("empty_projected_in_viewport"))) return;
        SelectUnderCursor();
        if (!ControlsCheck(SelectedUnitId == INDEX_NONE, TEXT("empty_clears_selection"))) return;
        const float BeforeWidth = Camera->OrthoWidth;
        ZoomIn();
        if (!ControlsCheck(Camera->OrthoWidth < BeforeWidth, TEXT("zoom_in_changes_camera"))) return;
        ZoomOut();
        if (!ControlsCheck(FMath::IsNearlyEqual(Camera->OrthoWidth, BeforeWidth), TEXT("zoom_out_restores_camera"))) return;
        SetControlsCursorFromWorld(Own->Position);
        SelectUnderCursor();
        CaptureControls(TEXT("01-selected"));
        ControlsStageTime = ControlsElapsed;
        ControlsStep = 1;
    }
    else if (ControlsStep == 1 && ControlsElapsed - ControlsStageTime > 1.f)
    {
        if (!ControlsCheck(SetControlsCursorFromWorld(ControlsDestination), TEXT("move_destination_projected"))) return;
        if (!ControlsCheck(FindUnitUnderCursor(false) == INDEX_NONE, TEXT("move_destination_empty_pick"))) return;
        FVector RoundTrip = FVector::ZeroVector;
        const bool bGroundProjected = CursorGroundPoint(RoundTrip);
        FVector RayOrigin = FVector::ZeroVector, RayDirection = FVector::ZeroVector;
        const bool bRayDeprojected = PC->DeprojectScreenPositionToWorld(ControlsCursor.X, ControlsCursor.Y, RayOrigin, RayDirection);
        const double RayPlaneDistance = bRayDeprojected && !FMath::IsNearlyZero(RayDirection.Z) ? -RayOrigin.Z / RayDirection.Z : 0.0;
        const FVector RayIntersection = RayOrigin + RayDirection * RayPlaneDistance;
        FVector FractionalOrigin = FVector::ZeroVector, FractionalDirection = FVector::ZeroVector;
        const bool bFractionalRayValid = PointerWorldRay(FractionalOrigin, FractionalDirection);
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_PROJECTION expected=%s actual=%s errorCm=%.6f groundValid=%d screenX=%.6f screenY=%.6f truncatedX=%.0f truncatedY=%.0f rayValid=%d rayOrigin=%s rayDirection=%s planeDistance=%.6f engineRayIntersection=%s nativeErrorCm=%.6f fractionalRayValid=%d fractionalRayOrigin=%s fractionalRayDirection=%s cameraLocation=%s cameraRotation=%s orthoWidth=%.3f viewportWidth=%d viewportHeight=%d"),
            *ControlsDestination.ToString(), *RoundTrip.ToString(), FVector::Dist(RoundTrip, ControlsDestination), bGroundProjected,
            ControlsCursor.X, ControlsCursor.Y, static_cast<double>(FMath::TruncToInt(ControlsCursor.X)), static_cast<double>(FMath::TruncToInt(ControlsCursor.Y)),
            bRayDeprojected, *RayOrigin.ToString(), *RayDirection.ToString(), RayPlaneDistance, *RayIntersection.ToString(), FVector::Dist(RayIntersection, ControlsDestination),
            bFractionalRayValid, *FractionalOrigin.ToString(), *FractionalDirection.ToString(),
            *Camera->GetComponentLocation().ToString(), *Camera->GetComponentRotation().ToString(), Camera->OrthoWidth, Width, Height);
        if (!ControlsCheck(bGroundProjected && FVector::Dist(RoundTrip, ControlsDestination) < 2.f,
            TEXT("actual_viewport_projection_deprojection_roundtrip_lt2cm"))) return;
        OrderUnderCursor();
        if (!ControlsCheck(LastRequestedSequence == 1 && LastRequestedType == TEXT("Move"), TEXT("move_request_type"))) return;
        ControlsStep = 2;
    }
    else if (ControlsStep == 2 && bHasReceipt)
    {
        if (!ControlsCheck(Receipt.bAccepted, TEXT("move_host_receipt_accepted"))) return;
        if (FVector::Dist(Own->Position, ControlsDestination) > 2.f) return;
        if (!ControlsCheck(FVector::Dist(Own->Position, ControlsInitialPosition) > 100.f, TEXT("move_replicated_destination_reached"))) return;
        if (!ControlsCheck(SetControlsCursorFromWorld(Target->Position), TEXT("attack_target_projected"))) return;
        if (!ControlsCheck(FindUnitUnderCursor(false) == ControlsTargetUnit, TEXT("attack_target_pick_matches_snapshot"))) return;
        OrderUnderCursor();
        if (!ControlsCheck(LastRequestedSequence == 2 && LastRequestedType == TEXT("Attack"), TEXT("attack_request_type"))) return;
        ControlsStep = 3;
    }
    else if (ControlsStep == 3 && bHasReceipt)
    {
        if (!ControlsCheck(Receipt.bAccepted, TEXT("attack_host_receipt_accepted"))) return;
        if (Target->HitPoints >= 99.f) return;
        if (!ControlsCheck(Target->HitPoints > 0.f, TEXT("attack_replicated_target_damage"))) return;
        Produce();
        if (!ControlsCheck(LastRequestedSequence == 3 && LastRequestedType == TEXT("Produce"), TEXT("produce_request_type"))) return;
        ControlsStep = 4;
    }
    else if (ControlsStep == 4 && bHasReceipt)
    {
        if (!ControlsCheck(Receipt.bAccepted, TEXT("produce_host_receipt_accepted"))) return;
        int32 Count = 0;
        for (const auto& Unit : Snapshot.Units) if (Unit.OwnerId == PC->OwnerId) ++Count;
        if (Count != ControlsOwnCount + 1 || PlayerRecord->Credits != ControlsCredits - FProofCommandModel::ProductionCost) return;
        if (!ControlsCheck(true, TEXT("produce_replicated_unit_and_credit_delta"))) return;
        // Move the original unit away from the spawn through the same handler,
        // keeping both proxy markers readable without a presentation-only offset.
        if (!ControlsCheck(SetControlsCursorFromWorld(FVector(-700.f, -450.f, 0.f)), TEXT("separation_destination_projected"))) return;
        if (!ControlsCheck(FindUnitUnderCursor(false) == INDEX_NONE, TEXT("separation_destination_empty_pick"))) return;
        OrderUnderCursor();
        if (!ControlsCheck(LastRequestedSequence == 4 && LastRequestedType == TEXT("Move"), TEXT("separation_request_type"))) return;
        ControlsStep = 5;
    }
    else if (ControlsStep == 5 && bHasReceipt)
    {
        if (!ControlsCheck(Receipt.bAccepted, TEXT("separation_move_host_receipt_accepted"))) return;
        if (FVector::Dist(Own->Position, FVector(-700.f, -450.f, 0.f)) > 2.f) return;
        CaptureControls(TEXT("02-moved-combat-produced"));
        ControlsStageTime = ControlsElapsed;
        ControlsStep = 6;
    }
    else if (ControlsStep == 7 && bTypeSelectionControls) { if (bMapTypeSelectionControls) TickMapTypeSelectionControls(); else TickTypeSelectionControls(); }
    else if (ControlsStep == 6 && ControlsElapsed - ControlsStageTime > 1.f)
    {
        const bool bScreenshots = IFileManager::Get().FileSize(*FPaths::Combine(ControlsOutput, TEXT("01-selected.png"))) > 1024
            && IFileManager::Get().FileSize(*FPaths::Combine(ControlsOutput, TEXT("02-moved-combat-produced.png"))) > 1024;
        if (!ControlsCheck(bScreenshots, TEXT("engine_rendered_screenshots_written"))) return;
        if (!bStopControls)
        {
            ControlsStep = 7;
            // Type selection owns step seven; the ordinary path keeps its exit.
            if (!bTypeSelectionControls) FinishControls(true, TEXT("select_clear_zoom_project_deproject_move_attack_produce_receipts_snapshots_capture"));
            return;
        }
        ControlsStopNativeBaseline = ManualStopPresses;
        ControlsStopMoveStart = Own->Position;
        if (!ControlsCheck(SetControlsCursorFromWorld(FVector(-1400.f, -700.f, 0.f)), TEXT("stop_move_destination_projected"))) return;
        OrderUnderCursor();
        if (!ControlsCheck(LastRequestedSequence == 5 && LastRequestedType == TEXT("Move"), TEXT("stop_move_request_sequence5"))) return;
        ControlsStep = 7;
    }
    else if (bStopControls && ControlsStep == 7 && bHasReceipt)
    {
        if (!ControlsCheck(Receipt.bAccepted, TEXT("stop_move_host_receipt_accepted"))) return;
        if (FVector::Dist(Own->Position, ControlsStopMoveStart) <= 50.f) return;
        if (!ControlsCheck(FVector::Dist(Own->Position, FVector(-1400.f, -700.f, 0.f)) > 100.f,
            TEXT("stop_move_observed_before_destination"))) return;
        // Native mode waits for an actually processed, bound input stack, not a delay.
        if (bStopInputBinding && !IsStopInputReady()) return;
        LogStopFixture(TEXT("moving"), Snapshot, *Own, FVector(-1400.f, -700.f, 0.f));
        if (bStopInputBinding)
        {
            if (!BeginStopInputBinding()) return;
        }
        else
        {
            StopSelected();
            if (!ControlsCheck(GroupOrders.NumPending() == 1, TEXT("stop_shared_handler_queued_one_intent"))) return;
        }
        ControlsStep = 8;
    }
    else if (bStopControls && ControlsStep == 8)
    {
        if (bStopInputBinding && !TickStopInputBinding()) return;
        if (LastRequestedSequence < 6) return;
        if (!ControlsCheck(LastRequestedSequence == 6 && LastRequestedType == TEXT("Stop"), TEXT("stop_queue_dispatch_sequence6"))) return;
        if (!bHasReceipt) return;
        if (!ControlsCheck(Receipt.bAccepted, TEXT("stop_host_receipt_accepted"))) return;
        ControlsStopReceiptRevision = Snapshot.Revision;
        ControlsStep = 9;
    }
    else if (bStopControls && ControlsStep == 9)
    {
        // Start with a later replicated state, not the possibly older state
        // present when the controller receipt arrived on its separate channel.
        if (Snapshot.Revision <= ControlsStopReceiptRevision) return;
        if (ControlsStopStationaryAt < 0.f)
        {
            ControlsStopPosition = Own->Position;
            ControlsStopStationaryAt = ControlsElapsed;
        }
        if (!ControlsCheck(FVector::Dist(Own->Position, ControlsStopPosition) <= 2.f,
            TEXT("stop_replicated_position_stationary"))) return;
        if (!ControlsCheck(FVector::Dist(Own->Position, FVector(-1400.f, -700.f, 0.f)) > 100.f,
            TEXT("stop_stationary_before_move_destination"))) return;
        if (!ControlsCheck(SelectedUnitId == ControlsOwnUnit && Selection.GetSelectedUnitIds().Num() == 1
            && Selection.GetSelectedUnitIds().Contains(ControlsOwnUnit), TEXT("stop_preserves_selection"))) return;
        if (!ControlsCheck(ManualStopPresses == ControlsStopNativeBaseline + (bStopInputBinding ? 1 : 0), TEXT("stop_expected_native_callback_count"))) return;
        ControlsStopObservedRevisions.Add(Snapshot.Revision);
        if (ControlsStopObservedRevisions.Num() < 3 || ControlsElapsed - ControlsStopStationaryAt < 1.5f) return;
        if (!ControlsCheck(true, TEXT("stop_three_later_revisions_and_1_5s_local_window"))) return;
        LogStopFixture(TEXT("stopped"), Snapshot, *Own, FVector(-1400.f, -700.f, 0.f));
        CaptureControls(TEXT("03-stop"));
        ControlsStep = 10;
    }
    else if (bStopControls && ControlsStep == 10)
    {
        if (IFileManager::Get().FileSize(*FPaths::Combine(ControlsOutput, TEXT("03-stop.png"))) <= 1024) return;
        ControlsStopResumeStart = Own->Position;
        if (!ControlsCheck(SetControlsCursorFromWorld(FVector(-700.f, -450.f, 0.f)), TEXT("stop_resume_destination_projected"))) return;
        OrderUnderCursor();
        if (!ControlsCheck(LastRequestedSequence == 7 && LastRequestedType == TEXT("Move"), TEXT("stop_resume_request_sequence7"))) return;
        ControlsStep = 11;
    }
    else if (bStopControls && ControlsStep == 11 && bHasReceipt)
    {
        if (!ControlsCheck(Receipt.bAccepted, TEXT("stop_resume_host_receipt_accepted"))) return;
        if (FVector::Dist(Own->Position, ControlsStopResumeStart) <= 50.f) return;
        if (!ControlsCheck(SelectedUnitId == ControlsOwnUnit && Selection.GetSelectedUnitIds().Num() == 1
            && ManualStopPresses == ControlsStopNativeBaseline + (bStopInputBinding ? 1 : 0), TEXT("stop_resume_selection_and_expected_native_count"))) return;
        LogStopFixture(TEXT("resumed"), Snapshot, *Own, FVector(-700.f, -450.f, 0.f));
        CaptureControls(TEXT("04-stop-resumed"));
        ControlsStep = 12;
    }
    else if (bStopControls && ControlsStep == 12)
    {
        for (const TCHAR* Name : { TEXT("01-selected.png"), TEXT("02-moved-combat-produced.png"), TEXT("03-stop.png"), TEXT("04-stop-resumed.png") })
            if (IFileManager::Get().FileSize(*FPaths::Combine(ControlsOutput, Name)) <= 1024) return;
        if (!ControlsCheck(true, TEXT("stop_all_four_engine_captures_written"))) return;
        FinishControls(true, bStopInputBinding ? TEXT("existing_controls_plus_engine_injected_stop_stationary_resume")
            : TEXT("existing_controls_plus_shared_stop_stationary_resume_receipts_snapshots_capture"));
    }
}

void AProofRTSPawn::LogStopFixture(const TCHAR* Phase, const FProofSnapshot& Snapshot,
    const FProofUnitRecord& Unit, const FVector& Destination)
{
    // Three milestones at most. The opt-in native mode labels injected S edges separately.
    UE_LOG(LogTemp, Display, TEXT("PROOF_STOP_FIXTURE {\"phase\":\"%s\",\"owner\":%d,\"unit\":%d,\"epoch\":%d,\"revision\":%d,\"sequence\":%d,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"elapsed\":%.6f,\"destinationX\":%.6f,\"destinationY\":%.6f,\"destinationZ\":%.6f,\"nativeStopCount\":%d}"),
        Phase, Unit.OwnerId, Unit.UnitId, Snapshot.MatchEpoch, Snapshot.Revision, LastRequestedSequence,
        Unit.Position.X, Unit.Position.Y, Unit.Position.Z, ControlsElapsed,
        Destination.X, Destination.Y, Destination.Z, ManualStopPresses);
}

void AProofRTSPawn::BuildUnderCursor(EProofCommandType Type)
{
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!GS || !GS->Snapshot.bSkirmish) { RequestStatus = TEXT("Base building is available in the small-match mode."); return; }
    FProofCommand Command;
    Command.Type = Type;
    if (!CursorGroundPoint(Command.Destination)) { RequestStatus = TEXT("Point at ground near your HQ."); return; }
    Request(Command);
}

void AProofRTSPawn::OnManualRefinery()
{
    BoxGesture.Cancel();
    BuildUnderCursor(EProofCommandType::BuildRefinery);
    RecordManualInput(TEXT("R"), ManualBuildPresses, RequestStatus);
}

void AProofRTSPawn::OnManualFactory()
{
    BoxGesture.Cancel();
    BuildUnderCursor(EProofCommandType::BuildFactory);
    RecordManualInput(TEXT("F"), ManualBuildPresses, RequestStatus);
}

void AProofRTSPawn::FinishSkirmishScenario(bool bPass, const TCHAR* Reason)
{
    if (bSkirmishFinished) return;
    bSkirmishFinished = true;
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_COMPLETE role=client owner=%d status=%s scenario=%s winnerTeam=%d built=%d gathered=%d produced=%d botBuilt=%d botProduced=%d botAttacked=%d reason=%s physicalInput=unverified"),
        PC ? PC->OwnerId : INDEX_NONE, bPass ? TEXT("pass") : TEXT("fail"), *SkirmishScenario,
        GS ? GS->Snapshot.WinnerTeam : INDEX_NONE, bSkirmishBuilt, bSkirmishGathered, bSkirmishProduced,
        bSkirmishBotRefinery && bSkirmishBotFactory, bSkirmishBotProduced, bSkirmishBotAttacked, Reason);
    FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
}

bool AProofRTSPawn::TickHUDReadabilityCaptures()
{
    // Camera/local-selection fixture only. The finished authoritative scene stays unchanged.
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS || GS->Snapshot.MatchPhase != EProofMatchPhase::Finished)
    { FinishSkirmishScenario(false, TEXT("readability_terminal_state")); return false; }
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    if (Width != SkirmishViewportWidth || Height != SkirmishViewportHeight || Width <= 0 || Height <= 0
        || Camera->ProjectionMode != ECameraProjectionMode::Orthographic || Camera->bConstrainAspectRatio
        || !FMath::IsNearlyEqual(Camera->OrthoWidth, 5000.f))
    { FinishSkirmishScenario(false, TEXT("readability_viewport_or_camera")); return false; }
    const FProofSnapshot& Snapshot = GS->Snapshot;
    if (HUDEdgeStageStarted < 0.f)
    {
        const FProofUnitRecord* Anchor = Snapshot.Units.FindByPredicate([this, PC](const FProofUnitRecord& Unit)
        { return Unit.UnitId == SelectedUnitId && Unit.OwnerId == PC->OwnerId && Unit.HitPoints > 0.f; });
        if (!Anchor)
        {
            for (const FProofUnitRecord& Unit : Snapshot.Units)
                if (Unit.OwnerId == PC->OwnerId && Unit.HitPoints > 0.f && (!Anchor || Unit.UnitId < Anchor->UnitId)) Anchor = &Unit;
            if (!Anchor || !SetControlsCursorFromWorld(Anchor->Position))
            { FinishSkirmishScenario(false, TEXT("readability_missing_anchor")); return false; }
            SelectUnderCursor();
        }
        if (!Anchor || SelectedUnitId != Anchor->UnitId || !Selection.GetSelectedUnitIds().Contains(Anchor->UnitId))
        { FinishSkirmishScenario(false, TEXT("readability_anchor_selection")); return false; }
        HUDEdgeAnchorUnit = Anchor->UnitId;
        HUDEdgeSnapshotDigest = Snapshot.ComputeDigest();
        HUDEdgeStageStarted = SkirmishElapsed;
    }
    if (Snapshot.ComputeDigest() != HUDEdgeSnapshotDigest)
    { FinishSkirmishScenario(false, TEXT("readability_scene_changed")); return false; }
    const FProofUnitRecord* Anchor = Snapshot.Units.FindByPredicate([this, PC](const FProofUnitRecord& Unit)
    { return Unit.UnitId == HUDEdgeAnchorUnit && Unit.OwnerId == PC->OwnerId && Unit.HitPoints > 0.f; });
    if (!Anchor || SelectedUnitId != HUDEdgeAnchorUnit)
    { FinishSkirmishScenario(false, TEXT("readability_anchor_lost")); return false; }
    if (HUDEdgeIndex >= 3) return !bCameraEdgeCaptures || TickCameraEdgeCaptures();
    const TCHAR* Names[] = { TEXT("03-left-edge.png"), TEXT("04-right-edge.png"), TEXT("05-panel-edge.png") };
    const FString Path = FPaths::Combine(SkirmishOutput, Names[HUDEdgeIndex]);
    FVector2D Projected;
    if (!PC->ProjectWorldLocationToScreen(Anchor->Position, Projected)
        || !FMath::IsFinite(Projected.X) || !FMath::IsFinite(Projected.Y))
    { FinishSkirmishScenario(false, TEXT("readability_projection_unavailable")); return false; }
    if (HUDEdgePhase == 0)
    {
        const float UiScale = FMath::Clamp(static_cast<float>(Width) / 1280.f, 0.72f, 1.f);
        // Same 12px top and 197px skirmish panel as ProofHUD; 24px keeps the bracket visible.
        HUDEdgeTarget = HUDEdgeIndex == 0 ? FVector2D(24.f, 0.65f * Height)
            : HUDEdgeIndex == 1 ? FVector2D(Width - 24.f, 0.65f * Height)
            : FVector2D(0.30f * Width, (12.f + 197.f) * UiScale + 24.f);
        if (IFileManager::Get().FileExists(*Path))
        { FinishSkirmishScenario(false, TEXT("readability_capture_not_fresh")); return false; }
        // Unconstrained orthographic scale depends on the actual aspect-axis policy.
        const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
        FSceneViewProjectionData Projection;
        if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
            || !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection))
        { FinishSkirmishScenario(false, TEXT("readability_projection_data")); return false; }
        const FIntRect Rect = Projection.GetConstrainedViewRect();
        const FMatrix ViewProjection = Projection.ComputeViewProjectionMatrix();
        const double Determinant = ViewProjection.Determinant();
        if (Rect.Min != FIntPoint::ZeroValue || Rect.Width() != Width || Rect.Height() != Height
            || !FMath::IsFinite(Determinant) || Determinant == 0.0)
        { FinishSkirmishScenario(false, TEXT("readability_projection_matrix")); return false; }
        const FMatrix Inverse = ViewProjection.Inverse();
        const auto ScreenOrigin = [&Inverse, &Rect](const FVector2D& Point, FVector& Origin)
        {
            const double ClipX = 2.0 * (Point.X - Rect.Min.X) / Rect.Width() - 1.0;
            const double ClipY = 1.0 - 2.0 * (Point.Y - Rect.Min.Y) / Rect.Height();
            const FVector4 Near = Inverse.TransformFVector4(FVector4(ClipX, ClipY, 1.0, 1.0));
            if (!FMath::IsFinite(Near.X) || !FMath::IsFinite(Near.Y) || !FMath::IsFinite(Near.Z)
                || !FMath::IsFinite(Near.W) || FMath::IsNearlyZero(Near.W)) return false;
            Origin = FVector(Near.X / Near.W, Near.Y / Near.W, Near.Z / Near.W);
            return !Origin.ContainsNaN();
        };
        FVector AnchorOrigin, TargetOrigin;
        if (!ScreenOrigin(Projected, AnchorOrigin) || !ScreenOrigin(HUDEdgeTarget, TargetOrigin))
        { FinishSkirmishScenario(false, TEXT("readability_projection_inverse")); return false; }
        const FVector Delta = AnchorOrigin - TargetOrigin;
        if (Delta.ContainsNaN())
        { FinishSkirmishScenario(false, TEXT("readability_camera_delta")); return false; }
        // UE's automatic ortho planes derive their view target from the owning
        // pawn. Translate that camera rig together, retaining every attached
        // unit view's world transform so this remains a camera-only fixture.
        TMap<int32, FTransform> ViewTransforms;
        for (const auto& Entry : UnitViews)
            if (Entry.Value) ViewTransforms.Add(Entry.Key, Entry.Value->GetComponentTransform());
        const FVector CameraBefore = Camera->GetComponentLocation();
        const FVector PawnBefore = GetActorLocation();
        if (!SetActorLocation(PawnBefore + Delta))
        { FinishSkirmishScenario(false, TEXT("readability_camera_rig_move")); return false; }
        for (const auto& Entry : ViewTransforms)
            if (UnitViews.Contains(Entry.Key) && UnitViews[Entry.Key]) UnitViews[Entry.Key]->SetWorldTransform(Entry.Value);
        UE_LOG(LogTemp, Display, TEXT("PROOF_HUD_CAMERA_MOVE name=%s cameraBefore=%s pawnBefore=%s projectionOrigin=%s delta=%s cameraAfter=%s pawnAfter=%s autoPlanes=%d updatePlanes=%d"),
            Names[HUDEdgeIndex], *CameraBefore.ToString(), *PawnBefore.ToString(), *Projection.ViewOrigin.ToString(),
            *Delta.ToString(), *Camera->GetComponentLocation().ToString(), *GetActorLocation().ToString(),
            Camera->bAutoCalculateOrthoPlanes ? 1 : 0, Camera->bUpdateOrthoPlanes ? 1 : 0);
        // Exactly one camera adjustment per view; cached projection is checked on a later frame.
        HUDEdgePhase = 1;
        HUDEdgeStageStarted = SkirmishElapsed;
        return false;
    }
    if (SkirmishElapsed - HUDEdgeStageStarted < 0.25f) return false;
    // Independently check actual world bounds after the rig move and later tick.
    // Do not repeat the placement helper: a wrong lift must fail this oracle.
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.HitPoints <= 0.f) continue;
        const TObjectPtr<UStaticMeshComponent>* View = UnitViews.Find(Unit.UnitId);
        if (!View || !*View || !(*View)->GetStaticMesh())
        { FinishSkirmishScenario(false, TEXT("readability_unit_view_moved")); return false; }
        const FVector Actual = (*View)->GetComponentLocation();
        const FBox WorldBounds = (*View)->GetStaticMesh()->GetBoundingBox().TransformBy((*View)->GetComponentTransform());
        if (!WorldBounds.IsValid || !FMath::IsNearlyEqual(Actual.X, Unit.Position.X, 0.01)
            || !FMath::IsNearlyEqual(Actual.Y, Unit.Position.Y, 0.01)
            || !FMath::IsNearlyEqual(WorldBounds.Min.Z, Unit.Position.Z, 0.01))
        { FinishSkirmishScenario(false, TEXT("readability_unit_view_moved")); return false; }
    }
    if (FVector2D::Distance(Projected, HUDEdgeTarget) > 2.0)
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_HUD_EDGE_PROJECTION_FAILURE name=%s projectedX=%.3f projectedY=%.3f targetX=%.3f targetY=%.3f width=%d height=%d"),
            Names[HUDEdgeIndex], Projected.X, Projected.Y, HUDEdgeTarget.X, HUDEdgeTarget.Y, Width, Height);
        FinishSkirmishScenario(false, TEXT("readability_projection_target")); return false;
    }
    if (HUDEdgePhase == 1)
    {
        HUDEdgeCaptureRequestedAt = FDateTime::UtcNow();
        FScreenshotRequest::RequestScreenshot(Path, true, false);
        HUDEdgePhase = 2;
        HUDEdgeStageStarted = SkirmishElapsed;
        return false;
    }
    if (IFileManager::Get().FileSize(*Path) <= 1024) return false;
    const FDateTime FileTimestamp = IFileManager::Get().GetTimeStamp(*Path);
    // UE's Windows file API intentionally truncates FILETIME to whole seconds.
    // Match that precision; the path must also be absent before the request,
    // and the runner independently checks native filesystem freshness and PNGs.
    if (FileTimestamp.ToUnixTimestamp() < HUDEdgeCaptureRequestedAt.ToUnixTimestamp())
    { FinishSkirmishScenario(false, TEXT("readability_capture_timestamp")); return false; }
    UE_LOG(LogTemp, Display, TEXT("PROOF_HUD_EDGE_FILE name=%s requestedUtc=%s fileUtc=%s comparisonPrecision=seconds"),
        Names[HUDEdgeIndex], *HUDEdgeCaptureRequestedAt.ToIso8601(), *FileTimestamp.ToIso8601());
    UE_LOG(LogTemp, Display, TEXT("PROOF_HUD_EDGE_CAPTURE name=%s unit=%d projectedX=%.3f projectedY=%.3f targetX=%.3f targetY=%.3f width=%d height=%d written=1 physicalInput=unverified"),
        Names[HUDEdgeIndex], HUDEdgeAnchorUnit, Projected.X, Projected.Y, HUDEdgeTarget.X, HUDEdgeTarget.Y, Width, Height);
    ++HUDEdgeIndex;
    HUDEdgePhase = 0;
    HUDEdgeStageStarted = SkirmishElapsed;
    return HUDEdgeIndex == 3 && (!bCameraEdgeCaptures || TickCameraEdgeCaptures());
}
void AProofRTSPawn::TickSkirmishScenario(float DeltaSeconds)
{
    if (!bSkirmishViewportValid) { FinishSkirmishScenario(false, TEXT("fixture_viewport")); return; }
    if (bSkirmishFinished) return;
    if (bCameraEdgeCaptures && (!bHUDReadabilityCaptures || SkirmishScenario != TEXT("Victory")))
    { FinishSkirmishScenario(false, TEXT("camera_edge_requires_victory_readability")); return; }
    if (bHUDReadabilityCaptures && SkirmishScenario != TEXT("Victory"))
    { FinishSkirmishScenario(false, TEXT("readability_requires_victory")); return; }
    if (bGroupGatherEvidence && (SkirmishScenario != TEXT("Victory") || bHUDReadabilityCaptures || bFactoryRallyEvidence || CoopContext.bEnabled))
    { FinishSkirmishScenario(false, TEXT("group_gather_requires_plain_victory")); return; }
    if (CoopContext.bEnabled && (!bFactoryRallyEvidence || SkirmishScenario != TEXT("Coop")))
    { FinishSkirmishScenario(false, TEXT("context_requires_cooperative_rally")); return; }
    if (FactoryProduction.bEnabled && (SkirmishScenario != TEXT("Coop") || bFactoryRallyEvidence || CoopContext.bEnabled || bGroupGatherEvidence || bHUDReadabilityCaptures))
    { FinishSkirmishScenario(false, TEXT("factory_production_requires_plain_coop")); return; }
    SkirmishElapsed += DeltaSeconds;
    if (SkirmishElapsed > 220.f) { FinishSkirmishScenario(false, TEXT("scenario_timeout")); return; }
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS || PC->OwnerId == INDEX_NONE) return;
    if (SkirmishOutput.IsEmpty()) { FinishSkirmishScenario(false, TEXT("missing_capture_directory")); return; }
    if (SkirmishScenario != TEXT("Victory") && SkirmishScenario != TEXT("Defeat") && SkirmishScenario != TEXT("Coop"))
    { FinishSkirmishScenario(false, TEXT("invalid_scenario")); return; }
    const FProofSnapshot& Snapshot = GS->Snapshot;
    const FProofPlayerRecord* LocalRecord = Snapshot.Players.FindByPredicate(
        [PC](const FProofPlayerRecord& Record) { return Record.OwnerId == PC->OwnerId; });
    if (!LocalRecord) return;
    const FProofUnitRecord* HQ = nullptr;
    const FProofUnitRecord* Harvester = nullptr;
    const FProofUnitRecord* Refinery = nullptr;
    const FProofUnitRecord* Factory = nullptr;
    const FProofUnitRecord* EnemyHQ = nullptr;
    int32 CombatCount = 0, LiveCount = 0;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.OwnerId == 100)
        {
            bSkirmishBotRefinery |= Unit.Kind == EProofUnitKind::Refinery;
            bSkirmishBotFactory |= Unit.Kind == EProofUnitKind::Factory;
            bSkirmishBotProduced |= Unit.Kind == EProofUnitKind::Combat;
            if (Unit.Kind == EProofUnitKind::HQ && Unit.HitPoints > 0.f) EnemyHQ = &Unit;
        }
        const float InitialHP = Unit.Kind == EProofUnitKind::HQ ? 300.f
            : Unit.Kind == EProofUnitKind::Refinery || Unit.Kind == EProofUnitKind::Factory ? 200.f : 100.f;
        const FProofPlayerRecord* UnitOwnerRecord = Snapshot.Players.FindByPredicate(
            [&Unit](const FProofPlayerRecord& Record) { return Record.OwnerId == Unit.OwnerId; });
        if (UnitOwnerRecord && UnitOwnerRecord->TeamId == LocalRecord->TeamId)
            bSkirmishBotAttacked |= Unit.HitPoints < InitialHP;
        if (Unit.OwnerId != PC->OwnerId) continue;
        if (Unit.Kind == EProofUnitKind::Combat) bSkirmishProduced = true;
        if (Unit.HitPoints <= 0.f) continue;
        ++LiveCount;
        if (Unit.Kind == EProofUnitKind::HQ) HQ = &Unit;
        if (Unit.Kind == EProofUnitKind::Harvester) Harvester = &Unit;
        if (Unit.Kind == EProofUnitKind::Refinery) Refinery = &Unit;
        if (Unit.Kind == EProofUnitKind::Factory) Factory = &Unit;
        if (Unit.Kind == EProofUnitKind::Combat) ++CombatCount;
    }
    if (bSkirmishBaseCaptureRequested && IFileManager::Get().FileSize(*FPaths::Combine(SkirmishOutput, TEXT("01-base.png"))) > 1024)
        bSkirmishBaseCaptured = true;
    bSkirmishBuilt |= Refinery && Factory;
    bSkirmishGathered |= LocalRecord->TotalGathered > 0;
    if (!bSkirmishReady)
    {
        int32 Width = 0, Height = 0;
        PC->GetViewportSize(Width, Height);
        if (!HQ || !Harvester || !EnemyHQ || !bConfiguredLocalInput || Width != SkirmishViewportWidth || Height != SkirmishViewportHeight
            || FAssetCompilingManager::Get().GetNumRemainingAssets() != 0) return;
        const IConsoleVariable* FPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
        const IConsoleVariable* Optimize = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));
        if (!FPS || FPS->GetFloat() != 30.f || !Optimize || Optimize->GetInt() != 1)
        { FinishSkirmishScenario(false, TEXT("render_configuration")); return; }
        bControlsCursor = true;
        bSkirmishReady = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_READY owner=%d width=%d height=%d maxFPS=%.3f shadersOptimize=%d"),
            PC->OwnerId, Width, Height, FPS->GetFloat(), Optimize->GetInt());
    }
    if (Snapshot.MatchPhase == EProofMatchPhase::Finished)
    {
        if (SkirmishTerminalSeen < 0.f) SkirmishTerminalSeen = SkirmishElapsed;
        if (!bSkirmishResultRequested && SkirmishElapsed - SkirmishTerminalSeen >= 0.5f)
        {
            IFileManager::Get().MakeDirectory(*SkirmishOutput, true);
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(SkirmishOutput, TEXT("02-result.png")), true, false);
            bSkirmishResultRequested = true;
        }
        const FString ResultPath = FPaths::Combine(SkirmishOutput, TEXT("02-result.png"));
        if (bSkirmishResultRequested && IFileManager::Get().FileSize(*ResultPath) > 1024 && SkirmishElapsed - SkirmishTerminalSeen >= 2.f)
        {
            if (!bHUDReadabilityCaptures || HUDEdgeStageStarted < 0.f)
                UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_CAPTURE name=02-result.png winnerTeam=%d phase=Finished written=1"), Snapshot.WinnerTeam);
            const int32 ExpectedWinner = SkirmishScenario == TEXT("Defeat") ? 1 : 0;
            const bool bPass = Snapshot.WinnerTeam == ExpectedWinner && bSkirmishBuilt && bSkirmishGathered
                && bSkirmishProduced && bSkirmishBotRefinery && bSkirmishBotFactory && bSkirmishBotProduced
                && bSkirmishBotAttacked && bSkirmishBaseCaptured && (!bFactoryRallyEvidence || FactoryRallyStage == 4)
                && (!CoopContext.bEnabled || CoopContext.Stage == 8)
                && (!bGroupGatherEvidence || GroupGatherStage == 3)
                && (!ReturnCargoRuntime.bEnabled || ReturnCargoRuntime.Stage == 15)
                && (!FactoryProduction.bEnabled || FactoryProduction.Complete());
            if (bPass && bHUDReadabilityCaptures && !TickHUDReadabilityCaptures()) return;
            FinishSkirmishScenario(bPass, bPass ? TEXT("complete_loop") : TEXT("outcome_or_evidence"));
        }
        return;
    }
    if (LastRequestedSequence > 0)
    {
        FProofCommandResult Receipt;
        if (!PC->FindReceipt(LastRequestedSequence, Receipt)) return;
        if (!Receipt.bAccepted)
        {
            const int32 RejectedSequence = LastRequestedSequence;
            const EProofSkirmishOwnershipRecovery OwnershipRecovery = FProofSkirmishOwnershipRecovery::Apply(
                Snapshot, PC->OwnerId, LastRequestedType, Receipt, LastRequestedUnitId,
                bLastRequestedSourceWasOwnedLive, SkirmishIssuedTargets, LastRequestedSequence);
            if (OwnershipRecovery == EProofSkirmishOwnershipRecovery::Wait) return;
            if (OwnershipRecovery == EProofSkirmishOwnershipRecovery::Recovered)
            {
                UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_RETRY reason=ownership unit=%d sequence=%d snapshotRevision=%d replicatedUnit=owned-dead"),
                    LastRequestedUnitId, RejectedSequence, Snapshot.Revision);
                return;
            }
            if (LastRequestedType == TEXT("Attack") && Receipt.Reason == TEXT("target"))
            {
                // Another real attacker can kill the chosen target between
                // our replicated snapshot and authoritative validation. Wait
                // for that death to replicate before retrying the fixture's
                // decision, within the unchanged overall scenario deadline.
                const FProofUnitRecord* RejectedTarget = Snapshot.Units.FindByPredicate(
                    [this](const FProofUnitRecord& Unit) { return Unit.UnitId == LastRequestedTarget; });
                if (RejectedTarget && RejectedTarget->HitPoints > 0.f) return;
                UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_RETRY reason=target target=%d sequence=%d snapshotRevision=%d replicatedTarget=%s"),
                    LastRequestedTarget, LastRequestedSequence, Snapshot.Revision, RejectedTarget ? TEXT("dead") : TEXT("missing"));
                for (auto It = SkirmishIssuedTargets.CreateIterator(); It; ++It)
                {
                    if (It.Value() == LastRequestedTarget) It.RemoveCurrent();
                }
                if (SkirmishFocusTarget == LastRequestedTarget) SkirmishFocusTarget = INDEX_NONE;
                // The rejected command already consumed its normal sequence.
                // Clear only this fixture receipt latch, never the allocator.
                LastRequestedSequence = 0;
                return;
            }
            FinishSkirmishScenario(false, *Receipt.Reason);
            return;
        }
    }
    if (SkirmishProduceRecordsBefore >= 0)
    {
        int32 ReplicatedRecords = 0;
        for (const FProofUnitRecord& Unit : Snapshot.Units)
            if (Unit.OwnerId == PC->OwnerId && Unit.Kind == EProofUnitKind::Combat) ++ReplicatedRecords;
        // Keep the production barrier until the accepted spawn replicates,
        // independently of later combat receipts. Existing units must still
        // receive defense orders while a paid production job is running.
        if (ReplicatedRecords > SkirmishProduceRecordsBefore) SkirmishProduceRecordsBefore = -1;
    }
    if (SkirmishElapsed < SkirmishNextDecision) return;
    SkirmishNextDecision = SkirmishElapsed + 0.1f;
    if (SkirmishStage == 0 && HQ)
    {
        if (!SetControlsCursorFromWorld(HQ->Position + FVector(350.f, 0.f, 0.f))) return;
        const int32 BeforeSequence = LastRequestedSequence;
        BuildUnderCursor(EProofCommandType::BuildRefinery);
        if (LastRequestedSequence > BeforeSequence && LastRequestedType == TEXT("BuildRefinery")) ++SkirmishStage;
        else FinishSkirmishScenario(false, TEXT("build_refinery_not_sent"));
        return;
    }
    if (SkirmishStage == 1 && HQ && Refinery)
    {
        if (!SetControlsCursorFromWorld(HQ->Position + FVector(0.f, -350.f, 0.f))) return;
        const int32 BeforeSequence = LastRequestedSequence;
        BuildUnderCursor(EProofCommandType::BuildFactory);
        if (LastRequestedSequence > BeforeSequence && LastRequestedType == TEXT("BuildFactory")) ++SkirmishStage;
        else FinishSkirmishScenario(false, TEXT("build_factory_not_sent"));
        return;
    }
    if (ReturnCargoRuntime.bEnabled && SkirmishStage >= 2 && Harvester && Refinery && Factory)
    {
        const bool bAllowOrdinaryActions = TickReturnCargoFixture();
        if (ReturnCargoRuntime.Stage > 0 && SkirmishStage == 2) ++SkirmishStage;
        // Receipt and capture barriers keep command identities/selection exact.
        // Travel/reload observation lets the ordinary Victory defense continue.
        if (bSkirmishFinished || !bAllowOrdinaryActions) return;
    }
    if (bGroupGatherEvidence && SkirmishStage >= 3 && GroupGatherStage == 2)
    {
        TickGroupGatherFixture();
        // Progress observation must not starve ordinary production/defense.
        // Hold selection only while the requested group PNG is being written.
        if (bSkirmishFinished || (bGroupGatherCaptureRequested && GroupGatherStage != 3)) return;
    }
    if (SkirmishStage == 2 && bGroupGatherEvidence && Harvester && Refinery && Factory)
    {
        TickGroupGatherFixture();
        if (GroupGatherStage >= 2) ++SkirmishStage;
        return;
    }
    if (SkirmishStage == 2 && Harvester && Refinery && Factory)
    {
        const FProofResourceRecord* Resource = nullptr;
        for (const FProofResourceRecord& Candidate : Snapshot.Resources)
            if (Candidate.Remaining > 0 && (!Resource || FVector::DistSquared(Candidate.Position, Harvester->Position)
                < FVector::DistSquared(Resource->Position, Harvester->Position))) Resource = &Candidate;
        if (!Resource || !SetControlsCursorFromWorld(Harvester->Position)) return;
        SelectUnderCursor();
        if (SelectedUnitId != Harvester->UnitId) { FinishSkirmishScenario(false, TEXT("select_harvester")); return; }
        if (!SetControlsCursorFromWorld(Resource->Position)) return;
        const int32 BeforeSequence = LastRequestedSequence;
        OrderUnderCursor();
        if (LastRequestedSequence <= BeforeSequence || LastRequestedTarget != Resource->ResourceId || LastRequestedType != TEXT("Gather")) { FinishSkirmishScenario(false, TEXT("gather_pointer")); return; }
        ++SkirmishStage;
        return;
    }
    if (SkirmishStage < 3) return;
    if (FactoryProduction.bEnabled && !FactoryProduction.Complete() && TickFactoryProductionFixture()) return;
    if (bSkirmishFinished) return;
    if (bFactoryRallyEvidence && !TickFactoryRallyFixture()) return;
    if (CoopContext.bEnabled && !TickCoopContextFixture()) return;
    if (bSkirmishBuilt && bSkirmishGathered && !bSkirmishBaseCaptureRequested)
    {
        IFileManager::Get().MakeDirectory(*SkirmishOutput, true);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(SkirmishOutput, TEXT("01-base.png")), true, false);
        bSkirmishBaseCaptureRequested = true;
    }
    const bool bDefeat = SkirmishScenario == TEXT("Defeat");
    const bool bShouldProduce = (!FactoryProduction.bEnabled || FactoryProduction.Complete())
        && SkirmishProduceRecordsBefore < 0
        && !Snapshot.ProductionJobs.ContainsByPredicate([PC](const FProofProductionJob& Job) { return Job.OwnerId == PC->OwnerId; })
        && Factory && LocalRecord->Credits >= FProofCommandModel::ProductionCost
        && LiveCount < FProofCommandModel::SkirmishRecordCap && (!bDefeat || !bSkirmishProduced);
    if (bDefeat || !bSkirmishGathered)
    {
        if (bShouldProduce) Produce();
        return;
    }
    // The victory trace defends the ore route, concentrates on one threat and
    // counterattacks after winning an actual battle. It earns/spends exactly
    // the same resources and uses exactly the same handlers as local play.
    int32 EnemyCombatCount = 0;
    const FProofUnitRecord* Focus = nullptr;
    for (const FProofUnitRecord& Candidate : Snapshot.Units)
    {
        const FProofPlayerRecord* EnemyOwner = Snapshot.Players.FindByPredicate(
            [&Candidate](const FProofPlayerRecord& Record) { return Record.OwnerId == Candidate.OwnerId; });
        if (!EnemyOwner || EnemyOwner->TeamId == LocalRecord->TeamId || Candidate.Kind != EProofUnitKind::Combat || Candidate.HitPoints <= 0.f) continue;
        ++EnemyCombatCount;
        if (Candidate.UnitId == SkirmishFocusTarget) Focus = &Candidate;
    }
    if (!Focus)
    {
        for (const FProofUnitRecord& Candidate : Snapshot.Units)
        {
            const FProofPlayerRecord* EnemyOwner = Snapshot.Players.FindByPredicate(
                [&Candidate](const FProofPlayerRecord& Record) { return Record.OwnerId == Candidate.OwnerId; });
            if (!EnemyOwner || EnemyOwner->TeamId == LocalRecord->TeamId || Candidate.Kind != EProofUnitKind::Combat || Candidate.HitPoints <= 0.f) continue;
            bool bNearbyThreat = HQ && FVector::DistSquared(Candidate.Position, HQ->Position) <= FMath::Square(1100.f);
            if (bSkirmishCounterattack)
                for (const FProofUnitRecord& Friendly : Snapshot.Units)
                    if (Friendly.OwnerId == PC->OwnerId && Friendly.Kind == EProofUnitKind::Combat && Friendly.HitPoints > 0.f
                        && FVector::DistSquared(Candidate.Position, Friendly.Position) <= FMath::Square(800.f)) bNearbyThreat = true;
            if (!bNearbyThreat) continue;
            if (!Focus || Candidate.HitPoints < Focus->HitPoints || (Candidate.HitPoints == Focus->HitPoints && Candidate.UnitId < Focus->UnitId)) Focus = &Candidate;
        }
        SkirmishFocusTarget = Focus ? Focus->UnitId : INDEX_NONE;
    }
    if (!Focus && bSkirmishBotAttacked && CombatCount >= 2 && CombatCount >= EnemyCombatCount + 2)
        bSkirmishCounterattack = true;
    const FProofUnitRecord* Target = Focus ? Focus : bSkirmishCounterattack ? EnemyHQ : nullptr;
    int32 CombatOrdinal = 0;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.OwnerId != PC->OwnerId || Unit.Kind != EProofUnitKind::Combat) continue;
        const int32 Ordinal = CombatOrdinal++;
        if (FactoryProduction.Protects(Unit.UnitId)) continue;
        if (Unit.HitPoints <= 0.f) continue;
        const int32 WantedTarget = Target ? Target->UnitId : 0;
        const int32* Issued = SkirmishIssuedTargets.Find(Unit.UnitId);
        if (Issued && *Issued == WantedTarget) continue;
        if (!SetControlsCursorFromWorld(Unit.Position)) return;
        SelectUnderCursor();
        if (SelectedUnitId != Unit.UnitId) continue;
        if (Target)
        {
            if (!SetControlsCursorFromWorld(Target->Position)) return;
            const int32 BeforeSequence = LastRequestedSequence;
            OrderUnderCursor();
            if (LastRequestedSequence <= BeforeSequence || LastRequestedTarget != Target->UnitId || LastRequestedType != TEXT("Attack"))
            { FinishSkirmishScenario(false, TEXT("attack_pointer")); return; }
        }
        else
        {
            if (!HQ) continue;
            const FVector Rally = HQ->Position + FVector(450.f + ((Ordinal % 4) - 1.5f) * 140.f,
                ((Ordinal / 4) - 1.f) * 140.f, 0.f);
            if (!SetControlsCursorFromWorld(Rally)) return;
            const int32 BeforeSequence = LastRequestedSequence;
            OrderUnderCursor();
            if (LastRequestedSequence <= BeforeSequence || LastRequestedType != TEXT("Move"))
            { FinishSkirmishScenario(false, TEXT("rally_pointer")); return; }
        }
        SkirmishIssuedTargets.Add(Unit.UnitId, WantedTarget);
        return;
    }
    if (bShouldProduce) Produce();
}

bool AProofRTSPawn::TickFactoryRallyFixture()
{
    if (FactoryRallyStage == 4) return true;
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    if (!PC || !GS || SkirmishScenario != TEXT("Coop"))
    { FinishSkirmishScenario(false, TEXT("factory_rally_fixture_context")); return false; }
    const FProofSnapshot& Snapshot = GS->Snapshot;
    const FProofUnitRecord* Factory = Snapshot.Units.FindByPredicate([this, PC](const FProofUnitRecord& U)
        { return U.OwnerId == PC->OwnerId && U.Kind == EProofUnitKind::Factory && U.HitPoints > 0.f
            && (FactoryRallyFactory == INDEX_NONE || U.UnitId == FactoryRallyFactory); });
    if (!Factory) return false;
    const auto LogPhase = [this, &Snapshot, PC](const TCHAR* Phase, const FProofUnitRecord* Unit)
    {
        const FVector Position = Unit ? Unit->Position : FVector::ZeroVector;
        UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_RALLY_FIXTURE {\"phase\":\"%s\",\"owner\":%d,\"epoch\":%d,\"revision\":%d,\"factory\":%d,\"unit\":%d,\"setSequence\":%d,\"produceSequence\":%d,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"rallyX\":%.17g,\"rallyY\":%.17g,\"rallyZ\":%.17g,\"selected\":%d,\"selectionCount\":%d,\"nativeRight\":%d,\"elapsed\":%.6f}"),
            Phase, PC->OwnerId, Snapshot.MatchEpoch, Snapshot.Revision, FactoryRallyFactory, Unit ? Unit->UnitId : INDEX_NONE,
            FactoryRallySetSequence, FactoryRallyProduceSequence, Position.X, Position.Y, Position.Z,
            FactoryRallyDestination.X, FactoryRallyDestination.Y, FactoryRallyDestination.Z,
            SelectedUnitId, Selection.GetSelectedUnitIds().Num(), ManualRightPresses, SkirmishElapsed);
    };
    if (FactoryRallyStage == 0)
    {
        if (!SetControlsCursorFromWorld(Factory->Position)) return false;
        SelectUnderCursor();
        if (SelectedUnitId != Factory->UnitId) { FinishSkirmishScenario(false, TEXT("factory_rally_selection")); return false; }
        FactoryRallyFactory = Factory->UnitId;
        FactoryRallyNativeRight = ManualRightPresses;
        FactoryRallyDestination = Factory->Position + FVector(-600, -350, 0);
        // Synthetic shared-handler invocation, not native physical input.
        if (CoopContext.bEnabled)
        {
            if (!PrepareCoopContextFactoryClick()) return false;
        }
        else if (!SetFactoryRally(FactoryRallyDestination)) { FinishSkirmishScenario(false, TEXT("factory_rally_not_sent")); return false; }
        FactoryRallySetSequence = LastRequestedSequence;
        FactoryRallyStage = 1;
        return false;
    }
    if (SelectedUnitId != FactoryRallyFactory || Selection.GetSelectedUnitIds().Num() != 1 || ManualRightPresses != FactoryRallyNativeRight)
    { FinishSkirmishScenario(false, TEXT("factory_rally_selection_or_native_counter")); return false; }
    if (!Factory->bHasRallyPoint || Factory->RallyPoint != FactoryRallyDestination) return false;
    if (FactoryRallyStage == 1)
    {
        FProofCommandResult Receipt;
        if (!PC->FindReceipt(FactoryRallySetSequence, Receipt) || !Receipt.bAccepted) return false;
        if (FactoryRallySeenAt < 0.f) { FactoryRallySeenAt = SkirmishElapsed; LogPhase(TEXT("rallied"), nullptr); }
        // Give each role a chance to observe the same published rally state.
        if (SkirmishElapsed - FactoryRallySeenAt < 1.f) return false;
        for (const FProofUnitRecord& U : Snapshot.Units) FactoryRallyBeforeIds.Add(U.UnitId);
        Produce();
        if (LastRequestedType != TEXT("Produce") || LastRequestedSequence <= FactoryRallySetSequence)
        { FinishSkirmishScenario(false, TEXT("factory_rally_produce_not_sent")); return false; }
        FactoryRallyProduceSequence = LastRequestedSequence;
        FactoryRallyStage = 2;
        return false;
    }
    const FProofUnitRecord* Produced = Snapshot.Units.FindByPredicate([this, PC](const FProofUnitRecord& U)
        { return U.OwnerId == PC->OwnerId && U.Kind == EProofUnitKind::Combat && U.HitPoints > 0.f
            && (FactoryRallyUnit == INDEX_NONE ? !FactoryRallyBeforeIds.Contains(U.UnitId) : U.UnitId == FactoryRallyUnit); });
    if (!Produced) return false;
    if (FactoryRallyStage == 2)
    {
        FProofCommandResult Receipt;
        if (!PC->FindReceipt(FactoryRallyProduceSequence, Receipt) || !Receipt.bAccepted) return false;
        FactoryRallyUnit = Produced->UnitId;
        FactoryRallyObservedSpawn = Produced->Position;
        FactoryRallySpawnRevision = Snapshot.Revision;
        FactoryRallySpawnSeenAt = SkirmishElapsed;
        LogPhase(TEXT("spawned"), Produced);
        FactoryRallyStage = 3;
    }
    if (Snapshot.Revision > FactoryRallySpawnRevision) FactoryRallyProgressRevisions.Add(Snapshot.Revision);
    if (FactoryRallyStage == 3 && FactoryRallyCaptureAt < 0.f
        && FactoryRallyProgressRevisions.Num() >= 2 && SkirmishElapsed - FactoryRallySpawnSeenAt >= 1.f
        && FVector::Distance(Produced->Position, FactoryRallyObservedSpawn) > 50.f
        && FVector::Distance(Produced->Position, FactoryRallyDestination) < FVector::Distance(FactoryRallyObservedSpawn, FactoryRallyDestination))
    {
        LogPhase(TEXT("moved"), Produced);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(SkirmishOutput, TEXT("03-factory-rally.png")), true, false);
        FactoryRallyCaptureAt = SkirmishElapsed;
    }
    if (FactoryRallyCaptureAt >= 0.f && SkirmishElapsed - FactoryRallyCaptureAt >= 0.5f
        && IFileManager::Get().FileSize(*FPaths::Combine(SkirmishOutput, TEXT("03-factory-rally.png"))) > 1024)
    {
        UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_RALLY_CAPTURE name=03-factory-rally.png selected=%d written=1 physicalInputVerified=0"), SelectedUnitId);
        FactoryRallyStage = 4;
        return true;
    }
    return false;
}
