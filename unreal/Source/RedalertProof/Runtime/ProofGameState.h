#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Commands/ProofCommandModel.h"
#include "ProofGameState.generated.h"

UCLASS()
class REDALERTPROOF_API AProofGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    AProofGameState();
    UPROPERTY(ReplicatedUsing=OnRepSnapshot)
    FProofSnapshot Snapshot;
    void Publish(const FProofSnapshot& NewSnapshot);
    void LogSnapshot(const TCHAR* ProofRole, int32 OwnerId) const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    UFUNCTION() void OnRepSnapshot();
};
