#include "Presentation/ProofRTSPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"

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
}

int32 AProofRTSPawn::FindUnitUnderCursor(bool bOwnOnly) const
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    float MouseX = 0.f, MouseY = 0.f;
    if (!PC || !GS || !PC->GetMousePosition(MouseX, MouseY))
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
            const float DistanceSquared = FVector2D::DistSquared(Screen, FVector2D(MouseX, MouseY));
            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                BestId = Unit.UnitId;
            }
        }
    }
    return BestId;
}

bool AProofRTSPawn::CursorGroundPoint(FVector& OutPoint) const
{
    const APlayerController* PC = Cast<APlayerController>(GetController());
    FVector Origin, Direction;
    if (!PC || !PC->DeprojectMousePositionToWorld(Origin, Direction) || FMath::IsNearlyZero(Direction.Z))
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
    RequestStatus = FString::Printf(TEXT("Request #%d sent; only host snapshots change units/credits."), Command.Sequence);
}

void AProofRTSPawn::ZoomIn() { Camera->SetOrthoWidth(FMath::Max(2000.f, Camera->OrthoWidth - 500.f)); }
void AProofRTSPawn::ZoomOut() { Camera->SetOrthoWidth(FMath::Min(18000.f, Camera->OrthoWidth + 500.f)); }
