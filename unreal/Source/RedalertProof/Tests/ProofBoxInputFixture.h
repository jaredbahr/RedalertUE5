#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "InputCoreTypes.h"
#include "ProofBoxInputFixture.generated.h"

class AProofRTSPawn;
class AProofPlayerController;

// Diagnostic presentation population only. No command model, bot, economy or
// replication proof. Existing gameplay regressions run in the production build.
UCLASS()
class AProofBoxInputGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AProofBoxInputGameMode();
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
};

UCLASS()
class AProofBoxInputFixture : public AActor
{
    GENERATED_BODY()
public:
    AProofBoxInputFixture();
    virtual void Tick(float DeltaSeconds) override;
private:
    AProofRTSPawn* Pawn = nullptr;
    AProofPlayerController* PC = nullptr;
    double Started = 0.0;
    uint64 LastEdgeFrame = 0;
    int32 Case = 0;
    int32 Step = 0;
    int32 PressBefore = 0;
    int32 ReleaseBefore = 0;
    uint64 RevisionBefore = 0;
    int32 InjectedPresses = 0;
    int32 InjectedReleases = 0;
    int32 SequenceBefore = 0;
    int32 GroupBefore = 0;
    int32 TypeBefore = 0;
    int32 RightBefore = 0;
    bool Done = false;
    bool CaptureRequested = false;
    double CaptureStarted = 0.0;
    FString CapturePath;
    FVector2D Anchor = FVector2D::ZeroVector;
    FVector2D Start = FVector2D::ZeroVector;
    FVector2D End = FVector2D::ZeroVector;
    FString Output;
    bool Ready() const;
    bool Populate();
    bool Edge(FKey Key, bool Pressed);
    void Pointer(const FVector2D& Point);
    void Baseline();
    void Observe(const TCHAR* Phase) const;
    void Finish(bool Passed, const TCHAR* Reason);
    bool Verify();
    void TickKeyRegression();
};
