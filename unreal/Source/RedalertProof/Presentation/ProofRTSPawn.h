#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ProofRTSPawn.generated.h"

class UCameraComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FProofCommand;

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
    const FString& GetRequestStatus() const { return RequestStatus; }
    FString GetManualInputStatus() const;
    bool IsInteractiveSession() const { return bInteractiveExitEnabled; }
    bool IsUsingBlockoutMesh() const { return bUsingBlockoutMesh; }

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> ProxyMesh;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UStaticMeshComponent>> UnitViews;
    int32 SelectedUnitId = INDEX_NONE;
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

    void SelectUnderCursor();
    void OrderUnderCursor();
    void Produce();
    void ZoomIn();
    void ZoomOut();
    void OnManualSelect();
    void OnManualOrder();
    void OnManualProduce();
    void OnManualZoomIn();
    void OnManualZoomOut();
    void OnInteractiveEscape();
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
};
