#pragma once

#include "CoreMinimal.h"
#include "Runtime/ProofAttackMoveRuntimeState.h"
#include "Presentation/ProofFactoryProductionRuntimeState.h"
#include "Presentation/ProofCoopContextRuntimeState.h"
#include "GameFramework/Pawn.h"
#include "Presentation/ProofSelectionState.h"
#include "Presentation/ProofTypeSelectGesture.h"
#include "Presentation/ProofGroupOrderQueue.h"
#include "ProofRTSPawn.generated.h"

class UCameraComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FProofCommand;
enum class EProofCommandType : uint8;

/** Camera and snapshot-only views. This pawn never simulates commands or damage. */
UCLASS()
class REDALERTPROOF_API AProofRTSPawn : public APawn
{
    GENERATED_BODY()

public:
    AProofRTSPawn();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    int32 GetSelectedUnitId() const { return SelectedUnitId; }
    const TArray<int32>& GetSelectedUnitIds() const { return Selection.GetSelectedUnitIds(); }
    bool IsCameraPanEnabled() const { return bCameraPanEnabled; }
    const FString& GetRequestStatus() const { return RequestStatus; }
    FString GetManualInputStatus() const;
    bool IsInteractiveSession() const { return bInteractiveExitEnabled; }
    bool IsUsingBlockoutMesh() const { return bUsingBlockoutMesh; }

protected:
    virtual void BeginPlay() override;

private:
    FProofFactoryProductionRuntimeState FactoryProduction;
    bool TickFactoryProductionFixture();
    void LogFactoryProductionPhase(const TCHAR* Phase) const;
    FProofCoopContextRuntimeState CoopContext;
    bool CoopContextCheck(bool bCondition, const TCHAR* Reason);
    bool ProjectCoopContextTarget(int32 TargetId, FVector& Ground);
    void LogCoopContextPhase(const TCHAR* Phase, int32 BeforeSequence, int32 Unit,
        int32 Secondary, int32 Target, const FVector& Destination) const;
    bool PrepareCoopContextFactoryClick();
    bool TickCoopContextFixture();
    FProofAttackMoveRuntimeState AttackMoveRuntime;
    void TickAttackMoveRuntime(float DeltaSeconds);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> ProxyMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> StructureMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> HarvesterMesh;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UStaticMeshComponent>> UnitViews;
    int32 SelectedUnitId = INDEX_NONE;
    FProofSelectionState Selection;
    ProofTypeSelectGesture::State TypeGesture;
    FProofGroupOrderQueue GroupOrders;
    FVector CameraGroundCenter = FVector::ZeroVector;
    bool bCameraPanEnabled = false;
    bool bCameraEdgePanEnabled = false;
    int32 ManualGroupPresses = 0;
    int32 ManualTypeSelectPresses = 0;
    int32 LastSeenMatchEpoch = INDEX_NONE;
    int32 ManualRematchPresses = 0;
    bool bUsingBlockoutMesh = false;
    bool bConfiguredLocalInput = false;
    bool bInteractiveExitEnabled = false;
    bool bInteractiveReadyLogged = false;
    bool bFocusObserved = false;
    bool bViewportFocused = false;
    bool bViewportForeground = false;
    int32 FocusTransitions = 0;
    int32 ManualLeftPresses = 0;
    int32 ManualRightPresses = 0;
    int32 ManualProducePresses = 0;
    int32 ManualStopPresses = 0;
    int32 ManualWheelUpPresses = 0;
    int32 ManualWheelDownPresses = 0;
    int32 ManualInputLogs = 0;
    FString LastManualAction = TEXT("none");
    bool bGraphicsWarmup = false;
    double WarmupIdleStart = -1.0;
    double WarmupLastLog = -1.0;
    int32 WarmupIdleFrames = 0;
    bool bControlsCursor = false;
    FVector2D ControlsCursor = FVector2D::ZeroVector;
    int32 LastRequestedSequence = 0;
    int32 LastRequestedUnitId = INDEX_NONE;
    bool bLastRequestedSourceWasOwnedLive = false;
    FString LastRequestedType;
    int32 LastRequestedTarget = INDEX_NONE;
    TSet<FString> ControlsPassedEvidence;
    bool bTypeSelectionControls = false;
    bool bMapTypeSelectionControls = false;
    bool bMapTypeCameraPrepared = false;
    void TickMapTypeSelectionControls();
    int32 TypeSelectionStage = 0;
    int32 TypeSelectionLastRevision = INDEX_NONE;
    int32 TypeSelectionRows = 0;
    float TypeSelectionStarted = 0.f;
    TArray<int32> TypeSelectionExpected;
    TMap<int32, FVector> TypeSelectionPositions;
    void TickTypeSelectionControls();
    bool LogTypeSelectionFrame(const TCHAR* Phase, const FString& ActionBefore = FString());
    int32 ControlsStep = 0;
    int32 ControlsOwnUnit = INDEX_NONE;
    int32 ControlsTargetUnit = INDEX_NONE;
    int32 ControlsCredits = 0;
    int32 ControlsOwnCount = 0;
    float ControlsElapsed = 0.f;
    float ControlsReady = -1.f;
    float ControlsScenarioReady = -1.f;
    float ControlsStageTime = 0.f;
    bool bControlsFinished = false;
    bool bStopControls = false;
    bool bStopInputBinding = false;
    int32 StopInputStage = 0;
    int32 StopInputEdges = 0;
    int32 StopInputDevice = INDEX_NONE;
    int32 StopInputUser = INDEX_NONE;
    uint64 StopInputPressFrame = 0;
    uint64 StopInputReleaseFrame = 0;
    bool bStopInputLastHandled = false;
    bool IsStopInputReady() const;
    bool SendStopInputEdge(bool bPressed);
    bool BeginStopInputBinding();
    bool TickStopInputBinding();
    void LogStopInputBinding(const TCHAR* Phase) const;
    int32 ControlsStopNativeBaseline = 0;
    int32 ControlsStopReceiptRevision = INDEX_NONE;
    TSet<int32> ControlsStopObservedRevisions;
    float ControlsStopStationaryAt = -1.f;
    FVector ControlsStopMoveStart = FVector::ZeroVector;
    FVector ControlsStopPosition = FVector::ZeroVector;
    FVector ControlsStopResumeStart = FVector::ZeroVector;
    FVector ControlsDestination = FVector(-300.f, -300.f, 0.f);
    FVector ControlsInitialPosition = FVector::ZeroVector;
    FString ControlsOutput;
    FString RequestStatus = TEXT("Select one of your live units.");

    bool ApplyCameraPanInput(const FVector2D& Direction, float DeltaSeconds);
    bool bCameraEdgeCaptures = false;
    int32 CameraEdgeCaptureStage = 0;
    float CameraEdgeStageStarted = -1.f;
    FVector2D CameraEdgeProjectionBefore = FVector2D::ZeroVector;
    FVector2D CameraEdgeProjectionExpected = FVector2D::ZeroVector;
    FVector CameraEdgeRigBefore = FVector::ZeroVector;
    FVector CameraEdgeCameraBefore = FVector::ZeroVector;
    FQuat CameraEdgeRotationBefore = FQuat::Identity;
    float CameraEdgeWidthBefore = 0.f;
    FString CameraEdgeDigestBefore;
    TMap<int32, FTransform> CameraEdgeViewTransforms;
    FDateTime CameraEdgeCaptureRequestedAt;
    bool TickCameraEdgeCaptures();
    bool bHUDReadabilityCaptures = false;
    int32 HUDEdgeAnchorUnit = INDEX_NONE;
    int32 HUDEdgeIndex = 0;
    int32 HUDEdgePhase = 0;
    float HUDEdgeStageStarted = -1.f;
    FVector2D HUDEdgeTarget = FVector2D::ZeroVector;
    FString HUDEdgeSnapshotDigest;
    FDateTime HUDEdgeCaptureRequestedAt;
    bool TickHUDReadabilityCaptures();
    FString SkirmishScenario;
    FString SkirmishOutput;
    int32 SkirmishViewportWidth = 1280;
    int32 SkirmishViewportHeight = 720;
    bool bSkirmishViewportValid = true;
    float SkirmishElapsed = 0.f;
    float SkirmishNextDecision = 0.f;
    float SkirmishTerminalSeen = -1.f;
    int32 SkirmishStage = 0;
    int32 ManualBuildPresses = 0;
    bool bSkirmishReady = false;
    bool bSkirmishFinished = false;
    bool bSkirmishBuilt = false;
    bool bSkirmishGathered = false;
    bool bSkirmishProduced = false;
    bool bSkirmishBotRefinery = false;
    bool bSkirmishBotFactory = false;
    bool bSkirmishBotProduced = false;
    bool bSkirmishBotAttacked = false;
    bool bSkirmishBaseCaptured = false;
    bool bSkirmishBaseCaptureRequested = false;
    bool bSkirmishResultRequested = false;
    TMap<int32, int32> SkirmishIssuedTargets;
    int32 SkirmishFocusTarget = INDEX_NONE;
    int32 SkirmishProduceRecordsBefore = -1;
    bool bSkirmishCounterattack = false;
    bool bGroupGatherEvidence = false;
    int32 GroupGatherStage = 0;
    int32 GroupGatherMiners[2] = { INDEX_NONE, INDEX_NONE };
    int32 GroupGatherResource = INDEX_NONE;
    int32 GroupGatherEpoch = INDEX_NONE;
    int32 GroupGatherNextBefore = 0;
    int32 GroupGatherBeforeSequence = 0;
    int32 GroupGatherReceiptSnapshotRevision = INDEX_NONE;
    FVector GroupGatherInitial[2];
    int32 GroupGatherPriorCargo[2] = { 0, 0 };
    int32 GroupGatherPriorDelivered = 0;
    bool GroupGatherMoved[2] = { false, false };
    bool GroupGatherLoaded[2] = { false, false };
    bool GroupGatherDeposited[2] = { false, false };
    bool bGroupGatherCaptureRequested = false;
    FDateTime GroupGatherCaptureRequestedAt;
    bool TickGroupGatherFixture();
    void LogGroupGatherPhase(const TCHAR* Phase) const;
    bool bFactoryRallyEvidence = false;
    int32 FactoryRallyStage = 0;
    int32 FactoryRallyFactory = INDEX_NONE;
    int32 FactoryRallyUnit = INDEX_NONE;
    int32 FactoryRallySetSequence = 0;
    int32 FactoryRallyProduceSequence = 0;
    int32 FactoryRallySpawnRevision = INDEX_NONE;
    int32 FactoryRallyNativeRight = 0;
    float FactoryRallySeenAt = -1.f;
    float FactoryRallySpawnSeenAt = -1.f;
    float FactoryRallyCaptureAt = -1.f;
    FVector FactoryRallyDestination = FVector::ZeroVector;
    FVector FactoryRallyObservedSpawn = FVector::ZeroVector;
    TSet<int32> FactoryRallyBeforeIds;
    TSet<int32> FactoryRallyProgressRevisions;
    bool TickFactoryRallyFixture();
    void BuildUnderCursor(EProofCommandType Type);
    void OnManualRefinery();
    void OnManualFactory();
    void TickSkirmishScenario(float DeltaSeconds);
    void FinishSkirmishScenario(bool bPass, const TCHAR* Reason);
    void SelectUnderCursor();
    void OrderUnderCursor();
    bool SetFactoryRally(const FVector& GroundPoint);
    void StopSelected();
    void AttackMoveUnderCursor();
    void Produce();
    void ZoomIn();
    void ZoomOut();
    void OnManualSelect();
    void OnManualTypeSelect();
    void OnManualTypeRelease();
    void SelectTypeByGesture(double WallSeconds, bool bFocused);
    bool SelectVisibleType();
    void OnManualGroupOne();
    void OnManualGroupTwo();
    void OnManualGroupThree();
    void OnManualGroupFour();
    void OnManualGroupFive();
    void OnManualGroupSix();
    void OnManualGroupSeven();
    void OnManualGroupEight();
    void OnManualGroupNine();
    void HandleManualGroup(int32 Slot);
    void OnManualOrder();
    void OnManualProduce();
    void OnManualStop();
    void OnManualZoomIn();
    void OnManualZoomOut();
    void OnInteractiveEscape();
    void OnManualRematch();
    void RecordManualInput(const TCHAR* Key, int32& Count, const FString& Outcome);
    void ObserveViewportFocus();
    int32 FindUnitUnderCursor(bool bOwnOnly) const;
    int32 FindResourceUnderCursor(bool bAllowDepleted) const;
    bool CursorGroundPoint(FVector& OutPoint) const;
    bool PointerWorldRay(FVector& OutOrigin, FVector& OutDirection) const;
    void Request(FProofCommand Command);
    bool GetPointerScreenPosition(FVector2D& OutPoint) const;
    bool SetControlsCursorFromWorld(const FVector& Position);
    void TickControlsAutomation(float DeltaSeconds);
    bool ControlsCheck(bool bCondition, const TCHAR* Evidence);
    void FinishControls(bool bPass, const TCHAR* Reason);
    void CaptureControls(const TCHAR* Name);
    void LogStopFixture(const TCHAR* Phase, const FProofSnapshot& Snapshot, const FProofUnitRecord& Unit, const FVector& Destination);
    void TickGraphicsWarmup();
};
