#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Commands/ProofCommandModel.h"
#include "Commands/ProofIngressBudget.h"
#include "Runtime/ProofRefineryMinerEvidence.h"
#include "Runtime/ProofStopEvidence.h"
#include "ProofPlayerController.generated.h"

class AProofGameState;

UCLASS()
class REDALERTPROOF_API AProofPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AProofPlayerController();
    UPROPERTY(Replicated) int32 OwnerId = INDEX_NONE;
    int32 NextLocalCommandSequence();
    void SynchronizeMatchEpoch();
    bool IsProofAutomation() const { return bAutomation; }
    bool IsControlsAutomation() const { return bControlsAutomation; }
    bool IsControlsObserver() const { return bControlsObserver; }
    bool FindReceipt(int32 Sequence, FProofCommandResult& OutResult) const;
    const FString& GetReceiptStatus() const { return ReceiptStatus; }
    int32 GetLastReceiptSequence() const { return LastReceiptSequence; }
    UFUNCTION(Server, Reliable) void ServerSubmitCommand(FProofCommand Command);
    UFUNCTION(Server, Reliable) void ServerRequestRematch(int32 ExpectedEpoch);
    UFUNCTION(Client, Reliable) void ClientCommandResult(FProofCommandResult Result);
    UFUNCTION(Client, Reliable) void ClientLifecycleRejected();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    FProofIngressBudget IngressBudget;
    FProofRefineryMinerEvidence RefineryEvidence;
    FProofStopEvidence StopEvidence;
    bool AdmitIngress();
    bool bIngressFixture = false;
    bool bLifecycleFixture = false;
    FString LifecycleRole;
    FString LifecycleControl;
    int32 LocalMatchEpoch = 0;
    int32 LifecycleStep = 0;
    int32 LifecyclePendingSequence = INDEX_NONE;
    bool bLifecycleRematchSent = false;
    bool bLifecycleStaleSent = false;
    void TickLifecycleFixture(AProofGameState* State);
    bool bIngressFlood = false;
    bool bIngressHealthy = false;
    bool bIngressSent = false;
    bool bIngressSecondSent = false;
    bool bIngressFailureBound = false;
    int32 IngressAdmitted = 0;
    double IngressFirstSeconds = 0.0;
    FString IngressRelease;
    void TickIngressFixture(AProofGameState* State);
    void TickSkirmishObserver(AProofGameState* State);
    void FinishSkirmishObserver(bool bPass, const TCHAR* Reason, int32 WinnerTeam);
    bool bAutomation = false;
    bool bControlsAutomation = false;
    bool bControlsObserver = false;
    bool bSkirmishObserver = false;
    bool bSkirmishObserverReady = false;
    bool bSkirmishObserverRefinery = false;
    bool bSkirmishObserverFactory = false;
    bool bSkirmishObserverGathered = false;
    bool bSkirmishObserverProduced = false;
    int32 SkirmishObserverStep = 0;
    int32 SkirmishObserverPendingSequence = INDEX_NONE;
    float SkirmishObserverTerminalAt = -1.f;
    TMap<int32, FProofCommandResult> Receipts;
    int32 LastReceiptSequence = 0;
    FString ReceiptStatus = TEXT("No host receipt yet.");
    float ControlsObserverReady = -1.f;
    bool bFinished = false;
    bool bAckFailure = false;
    bool bMoved = false;
    bool bDamaged = false;
    bool bProduced = false;
    float Lifetime = 0.f;
    float ReadyTime = -1.f;
    int32 NextStep = 0;
    int32 NextLocalSequence = 1;
    int32 OwnUnit = INDEX_NONE;
    int32 EnemyUnit = INDEX_NONE;
    FVector InitialPosition = FVector::ZeroVector;
    int64 LastLoggedRevision = -1;
    TSet<int32> Acknowledged;
    void FinishAutomation(bool bPass);
};
