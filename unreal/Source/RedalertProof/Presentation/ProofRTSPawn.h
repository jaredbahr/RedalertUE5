#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/Pawn.h"
#include "Presentation/ProofSelectionState.h"
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
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> ProxyMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> StructureMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> HarvesterMesh;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UStaticMeshComponent>> UnitViews;
    int32 SelectedUnitId = INDEX_NONE;
    FProofSelectionState Selection;
    FProofGroupOrderQueue GroupOrders;
    FVector CameraGroundCenter = FVector::ZeroVector;
    bool bCameraPanEnabled = false;
    int32 ManualGroupPresses = 0;
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
    FString LastRequestedType;
    int32 LastRequestedTarget = INDEX_NONE;
    TSet<FString> ControlsPassedEvidence;
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
    FVector ControlsDestination = FVector(-300.f, -300.f, 0.f);
    FVector ControlsInitialPosition = FVector::ZeroVector;
    FString ControlsOutput;
    FString RequestStatus = TEXT("Select one of your live units.");

    FString SkirmishScenario;
    FString SkirmishOutput;
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
    void BuildUnderCursor(EProofCommandType Type);
    void OnManualRefinery();
    void OnManualFactory();
    void TickSkirmishScenario(float DeltaSeconds);
    void FinishSkirmishScenario(bool bPass, const TCHAR* Reason);
    void SelectUnderCursor();
    void OrderUnderCursor();
    void Produce();
    void ZoomIn();
    void ZoomOut();
    void OnManualSelect();
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
    void OnManualZoomIn();
    void OnManualZoomOut();
    void OnInteractiveEscape();
    void OnManualRematch();
    void RecordManualInput(const TCHAR* Key, int32& Count, const FString& Outcome);
    void ObserveViewportFocus();
    int32 FindUnitUnderCursor(bool bOwnOnly) const;
    bool CursorGroundPoint(FVector& OutPoint) const;
    bool PointerWorldRay(FVector& OutOrigin, FVector& OutDirection) const;
    void Request(FProofCommand Command);
    bool GetPointerScreenPosition(FVector2D& OutPoint) const;
    bool SetControlsCursorFromWorld(const FVector& Position);
    void TickControlsAutomation(float DeltaSeconds);
    bool ControlsCheck(bool bCondition, const TCHAR* Evidence);
    void FinishControls(bool bPass, const TCHAR* Reason);
    void CaptureControls(const TCHAR* Name);
    void TickGraphicsWarmup();
    // Opt-in engine injection. These fields never affect normal sessions.
    bool bInputBindingFixture = false;
    bool bInputFixtureFinished = false;
    bool bInputFixtureVirtualPan = false;
    bool bInputFixtureRealFocusSuppressed = false;
    bool bInputFixtureVirtualFocusUsed = false;
    bool bInputFixtureSimulatedFocusLoss = false;
    FString InputFixtureOutput;
    FString InputFixtureReadinessJson = TEXT("null");
    FString InputFixtureReadinessSignature;
    double InputFixtureLastReadinessSample = -1.0;
    double InputFixtureLastReadinessChange = 0.0;
    int32 InputFixtureReadinessSamples = 0;
    int32 InputFixtureStage = 0;
    int32 InputFixtureGroup = 1;
    int32 InputFixtureRepeat = 0;
    int32 InputFixtureEpoch = INDEX_NONE;
    int32 InputFixtureHQ = INDEX_NONE;
    int32 InputFixtureHarvester = INDEX_NONE;
    int32 InputFixtureEnemyHQ = INDEX_NONE;
    int32 InputFixtureSequenceStart = 0;
    int32 InputFixtureCallbackBefore = 0;
    int32 InputFixtureEdgesSent = 0;
    int32 InputFixtureDeviceId = INDEX_NONE;
    int32 InputFixtureUserId = INDEX_NONE;
    double InputFixtureStart = 0.0;
    double InputFixtureStageStart = 0.0;
    double InputFixtureNextEdge = 0.0;
    float InputFixtureZoom = 0.f;
    FVector InputFixtureHQPosition = FVector::ZeroVector;
    FVector InputFixtureHarvesterPosition = FVector::ZeroVector;
    FVector InputFixturePanBefore = FVector::ZeroVector;
    TArray<int32> InputFixtureCombatIds;
    TMap<int32, FVector> InputFixtureMoveStart;
    TArray<TPair<FKey, bool>> InputFixtureEdges;
    TArray<FString> InputFixtureChecks;
    void TickInputBindingFixture(float DeltaSeconds);
    void ObserveInputFixtureReadiness(double Now, bool bForce);
    FString GetInputFixtureBootstrapJson() const;
    bool IsInputFixtureReadyForInjection() const;
    void ApplyCameraPan(float DeltaSeconds, bool bFocused, bool bForeground);
    void QueueInputFixtureClick(FKey Key, FKey Modifier = FKey());
    bool InputFixtureCheck(bool bCondition, const TCHAR* Evidence);
    void AdvanceInputFixture(int32 Stage);
    void FinishInputBindingFixture(bool bPass, const TCHAR* Reason);
    void CaptureInputFixture(const TCHAR* Name);
};
