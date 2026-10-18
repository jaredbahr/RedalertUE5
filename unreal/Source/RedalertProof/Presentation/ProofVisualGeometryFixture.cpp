#include "Presentation/ProofRTSPawn.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
void GeometryVector(const TSharedRef<FJsonObject>& Object, const TCHAR* Key, const FVector& Value)
{
    Object->SetArrayField(Key, {MakeShared<FJsonValueNumber>(Value.X),
        MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)});
}

TSharedRef<FJsonObject> GeometryComponent(const UStaticMeshComponent* Component)
{
    const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    const FTransform Transform = Component->GetComponentTransform();
    const FBox Local = Component->GetStaticMesh()->GetBoundingBox();
    const FBox World = Local.TransformBy(Transform);
    Object->SetStringField(TEXT("asset"), Component->GetStaticMesh()->GetPathName());
    GeometryVector(Object, TEXT("localMin"), Local.Min);
    GeometryVector(Object, TEXT("localMax"), Local.Max);
    GeometryVector(Object, TEXT("worldMin"), World.Min);
    GeometryVector(Object, TEXT("worldMax"), World.Max);
    GeometryVector(Object, TEXT("location"), Transform.GetLocation());
    GeometryVector(Object, TEXT("scale"), Transform.GetScale3D());
    GeometryVector(Object, TEXT("rotationPYR"), FVector(Transform.Rotator().Pitch, Transform.Rotator().Yaw, Transform.Rotator().Roll));
    GeometryVector(Object, TEXT("axisX"), Transform.GetUnitAxis(EAxis::X));
    GeometryVector(Object, TEXT("axisY"), Transform.GetUnitAxis(EAxis::Y));
    GeometryVector(Object, TEXT("axisZ"), Transform.GetUnitAxis(EAxis::Z));
    return Object;
}

bool GeometryMoveRig(AActor* Rig, const FVector& Location,
    const TMap<int32, TObjectPtr<UStaticMeshComponent>>& Views)
{
    TMap<int32, FTransform> Before;
    for (const auto& Entry : Views)
        if (Entry.Value) Before.Add(Entry.Key, Entry.Value->GetComponentTransform());
    if (!Rig->SetActorLocation(Location)) return false;
    for (const auto& Entry : Before)
        if (Views.Contains(Entry.Key) && Views[Entry.Key]) Views[Entry.Key]->SetWorldTransform(Entry.Value);
    for (const auto& Entry : Before)
        if (!Views.Contains(Entry.Key) || !Views[Entry.Key]
            || !Views[Entry.Key]->GetComponentTransform().Equals(Entry.Value, 0.001f)) return false;
    return Rig->GetActorLocation().Equals(Location, 0.001f);
}
}

bool AProofRTSPawn::BeginVisualGeometryCapture(int32 UnitId, const TCHAR* Name)
{
    const TObjectPtr<UStaticMeshComponent>* Target = UnitViews.Find(UnitId);
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!Target || !*Target || !(*Target)->GetStaticMesh() || !PC || !Camera
        || !(*Target)->IsVisible() || FScreenshotRequest::IsScreenshotRequested()) return false;
    VisualGeometry.TargetId = UnitId;
    VisualGeometry.CaptureName = Name;
    VisualGeometry.PriorCamera = Camera->GetComponentTransform();
    VisualGeometry.PriorPawn = GetActorLocation();
    VisualGeometry.PriorOrthoWidth = Camera->OrthoWidth;
    VisualGeometry.bPriorCursor = PC->bShowMouseCursor;
    VisualGeometry.PriorVisibility.Reset();
    for (const auto& Entry : UnitViews)
    {
        if (!Entry.Value) continue;
        VisualGeometry.PriorVisibility.Add(Entry.Key, Entry.Value->IsVisible());
        Entry.Value->SetVisibility(Entry.Key == UnitId);
    }
    const FVector Center = (*Target)->GetStaticMesh()->GetBoundingBox()
        .TransformBy((*Target)->GetComponentTransform()).GetCenter();
    // Recenter the existing camera rig for automatic ortho planes, preserving
    // all actual unit transforms. No snapshot, unit geometry or floor is moved.
    if (!GeometryMoveRig(this, Center, UnitViews))
    { RestoreVisualGeometryView(); FinishSkirmishScenario(false, TEXT("geometry_rig_move")); return true; }
    const FVector Offset(-650.f, 0.f, 220.f);
    Camera->SetWorldLocation(Center + Offset);
    Camera->SetWorldRotation((-Offset).Rotation());
    Camera->SetOrthoWidth(VisualGeometry.Stage < 3 ? 400.f : 750.f);
    PC->bShowMouseCursor = false;
    VisualGeometry.CaptureStarted = FPlatformTime::Seconds();
    VisualGeometry.bRequested = false;
    VisualGeometry.bCapturing = true;
    return true;
}

void AProofRTSPawn::RestoreVisualGeometryView()
{
    const bool bRigRestored = GeometryMoveRig(this, VisualGeometry.PriorPawn, UnitViews);
    Camera->SetWorldTransform(VisualGeometry.PriorCamera);
    Camera->SetOrthoWidth(VisualGeometry.PriorOrthoWidth);
    bool bVisibilityRestored = true;
    for (const auto& Entry : VisualGeometry.PriorVisibility)
        if (UnitViews.Contains(Entry.Key) && UnitViews[Entry.Key])
        {
            UnitViews[Entry.Key]->SetVisibility(Entry.Value);
            bVisibilityRestored &= UnitViews[Entry.Key]->IsVisible() == Entry.Value;
        }
    bool bCursorRestored = false;
    if (AProofPlayerController* PC = Cast<AProofPlayerController>(GetController()))
    {
        PC->bShowMouseCursor = VisualGeometry.bPriorCursor;
        bCursorRestored = PC->bShowMouseCursor == VisualGeometry.bPriorCursor;
    }
    VisualGeometry.PriorVisibility.Reset();
    VisualGeometry.bCapturing = false;
    VisualGeometry.bRequested = false;
    if (!bRigRestored || !bVisibilityRestored || !bCursorRestored
        || !Camera->GetComponentTransform().Equals(VisualGeometry.PriorCamera, 0.001f)
        || !FMath::IsNearlyEqual(Camera->OrthoWidth, VisualGeometry.PriorOrthoWidth))
    { FinishSkirmishScenario(false, TEXT("geometry_view_restore")); return; }
    if (VisualGeometry.Stage == 5)
        UE_LOG(LogTemp, Display, TEXT("PROOF_RHINO_DIRECTION_RESTORED stage=%d verified=1"), VisualGeometry.DirectionStage);
    else
        UE_LOG(LogTemp, Display, TEXT("PROOF_VISUAL_GEOMETRY_RESTORED stage=%d verified=1"), VisualGeometry.Stage);
}

bool AProofRTSPawn::LogVisualGeometryObservation() const
{
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!GS || !PC) return false;
    const FProofUnitRecord* Unit = GS->Snapshot.Units.FindByPredicate([this](const FProofUnitRecord& Candidate)
        { return Candidate.UnitId == VisualGeometry.TargetId && Candidate.HitPoints > 0.f; });
    const TObjectPtr<UStaticMeshComponent>* Target = UnitViews.Find(VisualGeometry.TargetId);
    if (!Unit || !Target || !*Target || !(*Target)->GetStaticMesh() || !PC) return false;
    const UStaticMeshComponent* Floor = nullptr;
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        const UStaticMeshComponent* Candidate = It->GetStaticMeshComponent();
        if (!It->ActorHasTag(TEXT("RedalertNativeProofGenerated")) || !Candidate || !Candidate->GetStaticMesh()
            || Candidate->GetStaticMesh()->GetPathName() != TEXT("/Engine/BasicShapes/Cube.Cube")) continue;
        const FBox Box = Candidate->GetStaticMesh()->GetBoundingBox().TransformBy(Candidate->GetComponentTransform());
        if (Box.GetSize().X < 5000.f || Box.GetSize().Y < 5000.f) continue;
        if (Floor) return false;
        Floor = Candidate;
    }
    if (!Floor) return false;
    const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("capture"), VisualGeometry.CaptureName);
    Row->SetNumberField(TEXT("stage"), VisualGeometry.Stage == 5 ? VisualGeometry.DirectionStage : VisualGeometry.Stage);
    if (VisualGeometry.Stage == 5)
    {
        FProofCommandResult Receipt;
        if (!PC->FindReceipt(VisualGeometry.DirectionSequence, Receipt) || !Receipt.bAccepted) return false;
        Row->SetNumberField(TEXT("moveSequence"), VisualGeometry.DirectionSequence);
        Row->SetBoolField(TEXT("moveReceiptAccepted"), true);
        GeometryVector(Row, TEXT("moveOrigin"), VisualGeometry.DirectionOrigin);
        GeometryVector(Row, TEXT("moveGoal"), VisualGeometry.DirectionGoal);
    }
    Row->SetNumberField(TEXT("unit"), Unit->UnitId);
    Row->SetNumberField(TEXT("owner"), Unit->OwnerId);
    Row->SetNumberField(TEXT("kind"), static_cast<int32>(Unit->Kind));
    Row->SetNumberField(TEXT("epoch"), GS->Snapshot.MatchEpoch);
    Row->SetNumberField(TEXT("revision"), GS->Snapshot.Revision);
    Row->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Row->SetNumberField(TEXT("elapsed"), SkirmishElapsed);
    GeometryVector(Row, TEXT("modelPosition"), Unit->Position);
    Row->SetObjectField(TEXT("component"), GeometryComponent(*Target));
    Row->SetObjectField(TEXT("floor"), GeometryComponent(Floor));
    GeometryVector(Row, TEXT("floorActorLocation"), Floor->GetOwner()->GetActorLocation());
    GeometryVector(Row, TEXT("floorActorScale"), Floor->GetOwner()->GetActorScale3D());
    const FRotator FloorRotation = Floor->GetOwner()->GetActorRotation();
    GeometryVector(Row, TEXT("floorActorRotationPYR"), FVector(FloorRotation.Pitch, FloorRotation.Yaw, FloorRotation.Roll));
    GeometryVector(Row, TEXT("cameraLocation"), Camera->GetComponentLocation());
    const FRotator Rotation = Camera->GetComponentRotation();
    GeometryVector(Row, TEXT("cameraRotationPYR"), FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll));
    Row->SetNumberField(TEXT("orthoWidth"), Camera->OrthoWidth);
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    Row->SetNumberField(TEXT("width"), Width);
    Row->SetNumberField(TEXT("height"), Height);
    const FBox UnitBox = (*Target)->GetStaticMesh()->GetBoundingBox().TransformBy((*Target)->GetComponentTransform());
    FVector2D ProjectedMin(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
    FVector2D ProjectedMax(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());
    for (int32 Corner = 0; Corner < 8; ++Corner)
    {
        const FVector Point((Corner & 1) ? UnitBox.Max.X : UnitBox.Min.X,
            (Corner & 2) ? UnitBox.Max.Y : UnitBox.Min.Y, (Corner & 4) ? UnitBox.Max.Z : UnitBox.Min.Z);
        FVector2D Projected;
        if (!PC->ProjectWorldLocationToScreen(Point, Projected, true)) return false;
        ProjectedMin.X = FMath::Min(ProjectedMin.X, Projected.X);
        ProjectedMin.Y = FMath::Min(ProjectedMin.Y, Projected.Y);
        ProjectedMax.X = FMath::Max(ProjectedMax.X, Projected.X);
        ProjectedMax.Y = FMath::Max(ProjectedMax.Y, Projected.Y);
    }
    Row->SetArrayField(TEXT("projectedMin"), {MakeShared<FJsonValueNumber>(ProjectedMin.X), MakeShared<FJsonValueNumber>(ProjectedMin.Y)});
    Row->SetArrayField(TEXT("projectedMax"), {MakeShared<FJsonValueNumber>(ProjectedMax.X), MakeShared<FJsonValueNumber>(ProjectedMax.Y)});
    Row->SetBoolField(TEXT("rhinoLoaded"), bUsingBlockoutMesh);
    int32 VisibleViews = 0;
    for (const auto& Entry : UnitViews) if (Entry.Value && Entry.Value->IsVisible()) ++VisibleViews;
    Row->SetNumberField(TEXT("visibleUnitViews"), VisibleViews);
    Row->SetBoolField(TEXT("otherUnitViewsHidden"), VisibleViews == 1 && (*Target)->IsVisible());
    Row->SetBoolField(TEXT("hudHidden"), true);
    Row->SetBoolField(TEXT("modelModified"), false);
    Row->SetBoolField(TEXT("physicalInputVerified"), false);
    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Row, Writer)) return false;
    // Condense to one row, so retained raw evidence is independently parseable.
    Json.ReplaceInline(TEXT("\r"), TEXT(""));
    Json.ReplaceInline(TEXT("\n"), TEXT(""));
    if (VisualGeometry.Stage == 5)
        UE_LOG(LogTemp, Display, TEXT("PROOF_RHINO_DIRECTION %s"), *Json);
    else
        UE_LOG(LogTemp, Display, TEXT("PROOF_VISUAL_GEOMETRY %s"), *Json);
    return true;
}

bool AProofRTSPawn::TickVisualGeometryFixture(int32 FixtureHarvesterId)
{
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!GS || !PC) return false;
    if (VisualGeometry.Stage == 5 && !VisualGeometry.bCapturing) return TickRhinoDirectionObservation();
    auto Find = [GS](int32 Id) { return GS->Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& U)
        { return U.UnitId == Id && U.HitPoints > 0.f; }); };
    if (VisualGeometry.bCapturing)
    {
        // Units produced during this short capture barrier also need isolation.
        // Their original visibility is retained and restored, never their model.
        for (const auto& Entry : UnitViews)
        {
            if (!Entry.Value) continue;
            if (!VisualGeometry.PriorVisibility.Contains(Entry.Key))
                VisualGeometry.PriorVisibility.Add(Entry.Key, Entry.Value->IsVisible());
            Entry.Value->SetVisibility(Entry.Key == VisualGeometry.TargetId);
        }
        const FProofUnitRecord* Target = Find(VisualGeometry.TargetId);
        if (!Target) { RestoreVisualGeometryView(); FinishSkirmishScenario(false, TEXT("geometry_target_lost")); return true; }
        if (!VisualGeometry.bRequested && FPlatformTime::Seconds() - VisualGeometry.CaptureStarted >= 0.35)
        {
            if (!LogVisualGeometryObservation())
            { RestoreVisualGeometryView(); FinishSkirmishScenario(false, TEXT("geometry_measurement")); return true; }
            if (VisualGeometry.Stage == 0) VisualGeometry.HarvesterContact = Target->Position;
            if (VisualGeometry.Stage == 1) VisualGeometry.HarvesterMotionA = Target->Position;
            if (VisualGeometry.Stage == 3) VisualGeometry.CombatMotionA = Target->Position;
            if (VisualGeometry.Stage == 5 && VisualGeometry.DirectionStage == 4)
            {
                VisualGeometry.StationaryPosition = Target->Position;
                VisualGeometry.StationaryElapsed = SkirmishElapsed;
            }
            IFileManager::Get().MakeDirectory(*SkirmishOutput, true);
            FScreenshotRequest::RequestScreenshot(FPaths::Combine(SkirmishOutput, VisualGeometry.CaptureName), true, false);
            VisualGeometry.bRequested = true;
        }
        if (VisualGeometry.bRequested && IFileManager::Get().FileSize(*FPaths::Combine(SkirmishOutput, VisualGeometry.CaptureName)) > 1024)
        {
            RestoreVisualGeometryView();
            if (VisualGeometry.Stage == 5)
            {
                ++VisualGeometry.DirectionStage;
                UE_LOG(LogTemp, Display, TEXT("PROOF_RHINO_DIRECTION_WRITTEN stage=%d"), VisualGeometry.DirectionStage);
            }
            else
            {
                ++VisualGeometry.Stage;
                UE_LOG(LogTemp, Display, TEXT("PROOF_VISUAL_GEOMETRY_WRITTEN stage=%d"), VisualGeometry.Stage);
            }
        }
        return true;
    }
    if (GS->Snapshot.MatchPhase == EProofMatchPhase::Finished) return false;
    if (VisualGeometry.Stage == 0)
    {
        // Observe the exact miner the existing Victory fixture is about to
        // Gather with, after paid construction. Do not order a different miner.
        if (SkirmishStage != 2) return false;
        const FProofUnitRecord* Unit = Find(FixtureHarvesterId);
        if (Unit && Unit->OwnerId == PC->OwnerId && Unit->Kind == EProofUnitKind::Harvester)
        { VisualGeometry.HarvesterId = Unit->UnitId; return BeginVisualGeometryCapture(Unit->UnitId, TEXT("03-geometry-harvester-contact.png")); }
    }
    if (VisualGeometry.Stage == 1 || VisualGeometry.Stage == 2)
    {
        const FProofUnitRecord* Unit = Find(VisualGeometry.HarvesterId);
        if (!Unit) { FinishSkirmishScenario(false, TEXT("geometry_harvester_lost")); return true; }
        const FVector Before = VisualGeometry.Stage == 1 ? VisualGeometry.HarvesterContact : VisualGeometry.HarvesterMotionA;
        // Capture A near the first ordinary Gather movement. Waiting 150 cm
        // placed A midway along the short ore/refinery loop, so another
        // 150 cm displacement could never occur. B still requires 150 cm.
        const float RequiredDistance = VisualGeometry.Stage == 1 ? 1.f : 150.f;
        if (FVector::DistSquared(Unit->Position, Before) >= FMath::Square(RequiredDistance))
            return BeginVisualGeometryCapture(Unit->UnitId, VisualGeometry.Stage == 1
                ? TEXT("04-geometry-harvester-motion-a.png") : TEXT("05-geometry-harvester-motion-b.png"));
    }
    if (VisualGeometry.Stage == 3)
    {
        for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
            if (Unit.OwnerId == PC->OwnerId && Unit.Kind == EProofUnitKind::Combat && Unit.HitPoints > 0.f)
            { VisualGeometry.CombatId = Unit.UnitId; return BeginVisualGeometryCapture(Unit.UnitId, TEXT("06-geometry-rhino-motion-a.png")); }
    }
    if (VisualGeometry.Stage == 4)
    {
        const FProofUnitRecord* Unit = Find(VisualGeometry.CombatId);
        if (!Unit) { FinishSkirmishScenario(false, TEXT("geometry_combat_lost")); return true; }
        if (FVector::DistSquared(Unit->Position, VisualGeometry.CombatMotionA) >= FMath::Square(150.f))
            return BeginVisualGeometryCapture(Unit->UnitId, TEXT("07-geometry-rhino-motion-b.png"));
    }
    return false;
}

bool AProofRTSPawn::TickRhinoDirectionObservation()
{
    if (VisualGeometry.DirectionStage == 6) return false;
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!GS || !PC || GS->Snapshot.MatchPhase == EProofMatchPhase::Finished)
    { FinishSkirmishScenario(false, TEXT("direction_context")); return true; }
    const FProofUnitRecord* Unit = GS->Snapshot.Units.FindByPredicate([this](const FProofUnitRecord& U)
        { return U.UnitId == VisualGeometry.CombatId && U.HitPoints > 0.f; });
    if (!Unit || Unit->OwnerId != PC->OwnerId || Unit->Kind != EProofUnitKind::Combat)
    { FinishSkirmishScenario(false, TEXT("direction_unit_lost")); return true; }
    if (VisualGeometry.DirectionStage == 0 || VisualGeometry.DirectionStage == 2)
    {
        // The same local selection and Move handler used by ordinary play.
        if (LastRequestedSequence > 0)
        {
            FProofCommandResult Prior;
            if (!PC->FindReceipt(LastRequestedSequence, Prior)) return true;
            if (!Prior.bAccepted) { FinishSkirmishScenario(false, TEXT("direction_prior_receipt")); return true; }
        }
        if (!SetControlsCursorFromWorld(Unit->Position)) return true;
        SelectUnderCursor();
        if (SelectedUnitId != Unit->UnitId) { FinishSkirmishScenario(false, TEXT("direction_selection")); return true; }
        const FVector Offset = VisualGeometry.DirectionStage == 0 ? FVector(300.f, 0.f, 0.f) : FVector(0.f, -300.f, 0.f);
        VisualGeometry.DirectionOrigin = Unit->Position;
        VisualGeometry.DirectionGoal = Unit->Position + Offset;
        if (!SetControlsCursorFromWorld(VisualGeometry.DirectionGoal)) return true;
        const int32 Before = LastRequestedSequence;
        OrderUnderCursor();
        if (LastRequestedSequence <= Before || LastRequestedType != TEXT("Move") || LastRequestedUnitId != Unit->UnitId)
        { FinishSkirmishScenario(false, TEXT("direction_move_handler")); return true; }
        VisualGeometry.DirectionSequence = LastRequestedSequence;
        UE_LOG(LogTemp, Display, TEXT("PROOF_RHINO_DIRECTION_MOVE stage=%d unit=%d sequence=%d destinationX=%.9f destinationY=%.9f"),
            VisualGeometry.DirectionStage, Unit->UnitId, LastRequestedSequence,
            VisualGeometry.DirectionGoal.X, VisualGeometry.DirectionGoal.Y);
        ++VisualGeometry.DirectionStage;
        return true;
    }
    FProofCommandResult Receipt;
    if (!PC->FindReceipt(VisualGeometry.DirectionSequence, Receipt)) return true;
    if (!Receipt.bAccepted) { FinishSkirmishScenario(false, TEXT("direction_move_receipt")); return true; }
    if (VisualGeometry.DirectionStage == 1 || VisualGeometry.DirectionStage == 3)
    {
        if (FVector::DistSquared(Unit->Position, VisualGeometry.DirectionOrigin) >= FMath::Square(100.f))
            BeginVisualGeometryCapture(Unit->UnitId, VisualGeometry.DirectionStage == 1
                ? TEXT("08-rhino-direction-first.png") : TEXT("09-rhino-direction-turn.png"));
        return true;
    }
    if (VisualGeometry.DirectionStage == 4)
    {
        if (Unit->Position.Equals(VisualGeometry.DirectionGoal, 0.01f))
            BeginVisualGeometryCapture(Unit->UnitId, TEXT("10-rhino-stationary-a.png"));
        return true;
    }
    if (!Unit->Position.Equals(VisualGeometry.StationaryPosition, 0.01f))
    { FinishSkirmishScenario(false, TEXT("direction_stationary_drift")); return true; }
    if (SkirmishElapsed - VisualGeometry.StationaryElapsed >= 1.2f)
        BeginVisualGeometryCapture(Unit->UnitId, TEXT("11-rhino-stationary-b.png"));
    return true;
}
