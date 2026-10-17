#include "Presentation/ProofRTSPawn.h"
#include "Presentation/ProofCameraPan.h"
#include "Presentation/ProofHUDPanelLayout.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "UnrealClient.h"

bool AProofRTSPawn::ApplyCameraPanInput(const FVector2D& Direction, float DeltaSeconds)
{
    const FVector Next = FProofCameraPan::Apply(CameraGroundCenter, Direction, DeltaSeconds);
    const FVector Delta = Next - CameraGroundCenter;
    if (Delta.IsNearlyZero()) return true;
    // Automatic ortho planes use the owning pawn as view target. Move the rig
    // together, retaining snapshot-only meshes in world space. Use a delta so
    // earlier presentation-only camera placement cannot snap the view back.
    TMap<int32, FTransform> WorldTransforms;
    for (const auto& Entry : UnitViews)
        if (Entry.Value) WorldTransforms.Add(Entry.Key, Entry.Value->GetComponentTransform());
    if (!SetActorLocation(GetActorLocation() + Delta)) return false;
    for (const auto& Entry : WorldTransforms)
        if (UnitViews.Contains(Entry.Key) && UnitViews[Entry.Key]) UnitViews[Entry.Key]->SetWorldTransform(Entry.Value);
    CameraGroundCenter = Next;
    return true;
}

bool AProofRTSPawn::TickCameraEdgeCaptures()
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!bCameraEdgeCaptures || !bHUDReadabilityCaptures || SkirmishScenario != TEXT("Victory")
        || !PC || !GS || GS->Snapshot.MatchPhase != EProofMatchPhase::Finished)
    { FinishSkirmishScenario(false, TEXT("camera_edge_fixture_gate")); return false; }
    if (CameraEdgeCaptureStage == 4) return true;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    const FProofUnitRecord* Anchor = GS->Snapshot.Units.FindByPredicate([this](const FProofUnitRecord& Unit)
        { return Unit.UnitId == HUDEdgeAnchorUnit && Unit.HitPoints > 0.f; });
    FVector2D Projected;
    if (!Anchor || !PC->ProjectWorldLocationToScreen(Anchor->Position, Projected)
        || !FMath::IsFinite(Projected.X) || !FMath::IsFinite(Projected.Y))
    { FinishSkirmishScenario(false, TEXT("camera_edge_projection_unavailable")); return false; }
    const FString BeforePath = FPaths::Combine(SkirmishOutput, TEXT("06-camera-before.png"));
    const FString AfterPath = FPaths::Combine(SkirmishOutput, TEXT("07-camera-after.png"));
    if (CameraEdgeCaptureStage == 0)
    {
        if (IFileManager::Get().FileExists(*BeforePath) || IFileManager::Get().FileExists(*AfterPath))
        { FinishSkirmishScenario(false, TEXT("camera_edge_files_not_fresh")); return false; }
        CameraEdgeDigestBefore = GS->Snapshot.ComputeDigest();
        CameraEdgeRigBefore = GetActorLocation();
        CameraEdgeCameraBefore = Camera->GetComponentLocation();
        CameraEdgeRotationBefore = Camera->GetComponentQuat();
        CameraEdgeWidthBefore = Camera->OrthoWidth;
        CameraEdgeProjectionBefore = Projected;
        for (const auto& Entry : UnitViews)
            if (Entry.Value) CameraEdgeViewTransforms.Add(Entry.Key, Entry.Value->GetComponentTransform());
        CameraEdgeCaptureRequestedAt = FDateTime::UtcNow();
        FScreenshotRequest::RequestScreenshot(BeforePath, true, false);
        CameraEdgeCaptureStage = 1;
        CameraEdgeStageStarted = SkirmishElapsed;
        return false;
    }
    if (GS->Snapshot.ComputeDigest() != CameraEdgeDigestBefore
        || Camera->OrthoWidth != CameraEdgeWidthBefore || !Camera->GetComponentQuat().Equals(CameraEdgeRotationBefore, 1.e-6)
        || CameraEdgeViewTransforms.Num() != UnitViews.Num())
    { FinishSkirmishScenario(false, TEXT("camera_edge_scene_or_camera_changed")); return false; }
    for (const auto& Entry : CameraEdgeViewTransforms)
    {
        const auto* View = UnitViews.Find(Entry.Key);
        if (!View || !*View || !(*View)->GetComponentTransform().Equals(Entry.Value, 0.01))
        { FinishSkirmishScenario(false, TEXT("camera_edge_unit_world_changed")); return false; }
    }
    if (SkirmishElapsed - CameraEdgeStageStarted < 0.25f) return false;
    const FString& PendingPath = CameraEdgeCaptureStage == 1 ? BeforePath : AfterPath;
    if (CameraEdgeCaptureStage == 1 || CameraEdgeCaptureStage == 3)
    {
        if (IFileManager::Get().FileSize(*PendingPath) <= 1024) return false;
        if (IFileManager::Get().GetTimeStamp(*PendingPath).ToUnixTimestamp() < CameraEdgeCaptureRequestedAt.ToUnixTimestamp())
        { FinishSkirmishScenario(false, TEXT("camera_edge_capture_timestamp")); return false; }
    }
    if (CameraEdgeCaptureStage == 1)
    {
        if (!CameraGroundCenter.IsNearlyZero())
        { FinishSkirmishScenario(false, TEXT("camera_edge_initial_center")); return false; }
        const FProofHUDPanelLayout Panel(static_cast<float>(Width), true);
        const FVector2D Input = FProofCameraPan::EdgeInput(FVector2D(Width - 1, Height * 0.65), FVector2D(Width, Height),
            Panel.Bounds(), true, true, true, false);
        const FVector ExpectedDelta(-600.0, 0.0, 0.0);
        if (Input != FVector2D(1.0, 0.0) || CameraEdgeViewTransforms.IsEmpty()
            || !PC->ProjectWorldLocationToScreen(Anchor->Position - ExpectedDelta, CameraEdgeProjectionExpected))
        { FinishSkirmishScenario(false, TEXT("camera_edge_policy_or_expected_projection")); return false; }
        // Fixed synthetic steps exercise the production method; no native input,
        // OS focus, wall-clock speed or subsequent command acceptance is claimed.
        for (int32 Step = 0; Step < 10; ++Step)
            if (!ApplyCameraPanInput(Input, 0.1f))
            { FinishSkirmishScenario(false, TEXT("camera_edge_rig_move")); return false; }
        if (!(GetActorLocation() - CameraEdgeRigBefore).Equals(ExpectedDelta, 0.001)
            || !(Camera->GetComponentLocation() - CameraEdgeCameraBefore).Equals(ExpectedDelta, 0.001))
        { FinishSkirmishScenario(false, TEXT("camera_edge_rig_delta")); return false; }
        CameraEdgeCaptureStage = 2;
        CameraEdgeStageStarted = SkirmishElapsed;
        return false;
    }
    if (CameraEdgeCaptureStage == 2)
    {
        const double Error = FVector2D::Distance(Projected, CameraEdgeProjectionExpected);
        if (Error > 2.0 || Projected.X >= CameraEdgeProjectionBefore.X - 10.0
            || FMath::Abs(Projected.Y - CameraEdgeProjectionBefore.Y) > 2.0)
        { FinishSkirmishScenario(false, TEXT("camera_edge_actual_projection")); return false; }
        UE_LOG(LogTemp, Display, TEXT("PROOF_CAMERA_EDGE_CAPTURE unit=%d beforeX=%.6f beforeY=%.6f afterX=%.6f afterY=%.6f expectedX=%.6f expectedY=%.6f errorPixels=%.6f deltaX=%.6f deltaY=%.6f deltaZ=%.6f views=%d digestBefore=%s digestAfter=%s rotationUnchanged=1 zoomUnchanged=1 viewsUnchanged=1 input=synthetic focus=synthetic physicalInput=unverified"),
            Anchor->UnitId, CameraEdgeProjectionBefore.X, CameraEdgeProjectionBefore.Y, Projected.X, Projected.Y,
            CameraEdgeProjectionExpected.X, CameraEdgeProjectionExpected.Y, Error,
            (GetActorLocation() - CameraEdgeRigBefore).X, (GetActorLocation() - CameraEdgeRigBefore).Y,
            (GetActorLocation() - CameraEdgeRigBefore).Z, CameraEdgeViewTransforms.Num(),
            *CameraEdgeDigestBefore, *GS->Snapshot.ComputeDigest());
        CameraEdgeCaptureRequestedAt = FDateTime::UtcNow();
        FScreenshotRequest::RequestScreenshot(AfterPath, true, false);
        CameraEdgeCaptureStage = 3;
        CameraEdgeStageStarted = SkirmishElapsed;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("PROOF_CAMERA_EDGE_COMPLETE status=pass captures=2 input=synthetic focus=synthetic physicalInput=unverified"));
    CameraEdgeCaptureStage = 4;
    return true;
}
