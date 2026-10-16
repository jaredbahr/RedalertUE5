#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Commands/ProofCommandModel.h"
#include "Commands/ProofOwnerIds.h"
#include "Skirmish/ProofSkirmishBot.h"
#include "ProofGameMode.generated.h"

UCLASS()
class REDALERTPROOF_API AProofGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AProofGameMode();
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Tick(float DeltaSeconds) override;
    FProofCommandResult SubmitForOwner(int32 TrustedOwnerId, const FProofCommand& Command);
private:
    FProofCommandModel Model;
    FProofSkirmishBot SkirmishBot;
    FProofHumanOwnerAllocator HumanOwnerIds;
    int32 BotMover = INDEX_NONE;
    int32 BotAttacker = INDEX_NONE;
    int32 BotSequence = 0;
    int32 BotTarget = INDEX_NONE;
    int32 HumanAccepted = 0;
    int32 RejectedOwnership = 0;
    int32 BotAccepted = 0;
    bool bAutomation = false;
    bool bControlsAutomation = false;
    bool bSkirmish = false;
    bool bSkirmishFixtureHost = false;
    bool bSkirmishFixtureStarted = false;
    bool bSkirmishTerminalPublished = false;
    bool bSkirmishFixtureFinished = false;
    bool bSkirmishHumanRefinery = false;
    bool bSkirmishHumanFactory = false;
    bool bSkirmishHumanGathered = false;
    bool bSkirmishHumanProduced = false;
    bool bSkirmishBotRefinery = false;
    bool bSkirmishBotFactory = false;
    bool bSkirmishBotProduced = false;
    bool bSkirmishBotAttackAccepted = false;
    float SkirmishTerminalAt = -1.f;
    double SkirmishSeconds = 0.0;
    bool bBotMoved = false;
    bool bBotDamaged = false;
    bool bBotTargetExclusive = true;
    bool bControlsInitialCountsValid = false;
    bool bControlsComplete = false;
    bool bControlsPassed = false;
    bool bBotStarted = false;
    bool bMoved = false;
    bool bDamaged = false;
    bool bProduced = false;
    float Lifetime = 0.f;
    float BotStartedAt = -1.f;
    float Accumulator = 0.f;
    int32 StepsSincePublish = 0;
    TMap<int32, FVector> InitialPositions;
    TSet<int32> HumanAttackTargets;
    void Publish();
    void StartBotIfReady();
    void ObserveSkirmish(const FProofSnapshot& Snapshot);
    void ObserveControlsStep(const FProofSnapshot& Before, const FProofSnapshot& After);
    bool LogControlsCounts(const TCHAR* Phase, int32 ExpectedHumanUnits) const;
};
