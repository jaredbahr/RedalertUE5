#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Commands/ProofCommandModel.h"
#include "ProofPlayerController.generated.h"

UCLASS()
class REDALERTPROOF_API AProofPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AProofPlayerController();
    UPROPERTY(Replicated) int32 OwnerId = INDEX_NONE;
    int32 NextLocalCommandSequence() { return NextLocalSequence < MAX_int32 ? NextLocalSequence++ : 0; }
    bool IsProofAutomation() const { return bAutomation; }
    bool IsControlsAutomation() const { return bControlsAutomation; }
    bool IsControlsObserver() const { return bControlsObserver; }
    bool FindReceipt(int32 Sequence, FProofCommandResult& OutResult) const;
    const FString& GetReceiptStatus() const { return ReceiptStatus; }
    int32 GetLastReceiptSequence() const { return LastReceiptSequence; }
    UFUNCTION(Server, Reliable) void ServerSubmitCommand(FProofCommand Command);
    UFUNCTION(Client, Reliable) void ClientCommandResult(FProofCommandResult Result);
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    bool bAutomation = false;
    bool bControlsAutomation = false;
    bool bControlsObserver = false;
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
