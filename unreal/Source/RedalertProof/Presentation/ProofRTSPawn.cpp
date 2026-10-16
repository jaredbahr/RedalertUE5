#include "Presentation/ProofRTSPawn.h"
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
    bGraphicsWarmup = FParse::Param(FCommandLine::Get(), TEXT("ProofGraphicsWarmup"));
    // Optional generated content stays outside tracked source. Every record is
    // deliberately a generic proxy; there is no unit-kind field in this proof.
    if (GetNetMode() != NM_DedicatedServer)
    {
        ProxyMesh = LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/BlockoutGenerated/Meshes/SM_RhinoTank_Blockout.SM_RhinoTank_Blockout"));
        bUsingBlockoutMesh = ProxyMesh != nullptr;
        if (!ProxyMesh)
        {
            ProxyMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        }
    }
}

void AProofRTSPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AProofRTSPawn::SelectUnderCursor);
    Input->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AProofRTSPawn::OrderUnderCursor);
    Input->BindKey(EKeys::P, IE_Pressed, this, &AProofRTSPawn::Produce);
    Input->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AProofRTSPawn::ZoomIn);
    Input->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AProofRTSPawn::ZoomOut);
}

void AProofRTSPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!PC || !PC->IsLocalController() || GetNetMode() == NM_DedicatedServer)
    {
        return;
    }
    if (!bConfiguredLocalInput)
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
        // Keep the review view at the fixture origin regardless of PlayerStart.
        Camera->SetWorldLocation(FVector(0.f, -3200.f, 3200.f));
        Camera->SetWorldRotation(FRotator(-45.f, 90.f, 0.f));
        bConfiguredLocalInput = true;
    }
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!GS)
    {
        return;
    }
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
            View->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            View->SetupAttachment(RootComponent);
            View->RegisterComponent();
            // Display-only size keeps the tiny correctness fixture readable.
            // This is not approved tank/infantry scale or gameplay collision.
            View->SetWorldScale3D(bUsingBlockoutMesh ? FVector(0.3f) : FVector(1.f, 1.f, 0.6f));
        }
        if (View)
        {
            View->SetWorldLocation(Unit.Position + (bUsingBlockoutMesh ? FVector::ZeroVector : FVector(0.f, 0.f, 30.f)));
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
    if (PC->IsControlsAutomation()) TickControlsAutomation(DeltaSeconds);
}

bool AProofRTSPawn::GetPointerScreenPosition(FVector2D& OutPoint) const
{
    if (bControlsCursor) { OutPoint = ControlsCursor; return true; }
    const APlayerController* PC = Cast<APlayerController>(GetController());
    float X = 0.f, Y = 0.f;
    if (!PC || !PC->GetMousePosition(X, Y)) return false;
    OutPoint = FVector2D(X, Y);
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
    if (Rect.Width() <= 0 || Rect.Height() <= 0) return false;
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
    SelectedUnitId = FindUnitUnderCursor(true);
    RequestStatus = SelectedUnitId == INDEX_NONE ? TEXT("No owned live unit selected.") : TEXT("Owned unit selected. Right-click ground or an enemy.");
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
    FProofCommand Command;
    Command.UnitId = SelectedUnitId;
    const int32 TargetId = FindUnitUnderCursor(false);
    const FProofUnitRecord* Target = GS->Snapshot.Units.FindByPredicate(
        [TargetId](const FProofUnitRecord& Unit) { return Unit.UnitId == TargetId; });
    if (Target && Target->OwnerId != PC->OwnerId)
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

void AProofRTSPawn::Produce()
{
    FProofCommand Command;
    Command.Type = EProofCommandType::Produce;
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
    Command.Sequence = PC->NextLocalCommandSequence();
    if (Command.Sequence <= 0)
    {
        RequestStatus = TEXT("Command sequence exhausted; reconnect for a new fixture session.");
        return;
    }
    PC->ServerSubmitCommand(Command);
    LastRequestedSequence = Command.Sequence;
    LastRequestedType = Command.Type == EProofCommandType::Move ? TEXT("Move")
        : Command.Type == EProofCommandType::Attack ? TEXT("Attack") : TEXT("Produce");
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
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_RENDER_CONFIG shadersOptimize=%d maxFPS=%.3f"),
            OptimizeShaders ? OptimizeShaders->GetInt() : -1, MaxFPS ? MaxFPS->GetFloat() : -1.f);
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
    else if (ControlsStep == 6 && ControlsElapsed - ControlsStageTime > 1.f)
    {
        const bool bScreenshots = IFileManager::Get().FileSize(*FPaths::Combine(ControlsOutput, TEXT("01-selected.png"))) > 1024
            && IFileManager::Get().FileSize(*FPaths::Combine(ControlsOutput, TEXT("02-moved-combat-produced.png"))) > 1024;
        if (!ControlsCheck(bScreenshots, TEXT("engine_rendered_screenshots_written"))) return;
        ControlsStep = 7;
    }
    else if (ControlsStep == 7 && ControlsElapsed - ControlsScenarioReady > 23.f)
    {
        FinishControls(true, TEXT("select_clear_zoom_project_deproject_move_attack_produce_receipts_snapshots_capture"));
    }
}
