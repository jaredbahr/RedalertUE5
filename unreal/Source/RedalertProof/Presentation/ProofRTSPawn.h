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
    FString RequestStatus = TEXT("Select one of your live units.");

    void SelectUnderCursor();
    void OrderUnderCursor();
    void Produce();
    void ZoomIn();
    void ZoomOut();
    int32 FindUnitUnderCursor(bool bOwnOnly) const;
    bool CursorGroundPoint(FVector& OutPoint) const;
    void Request(FProofCommand Command);
};
