#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Commands/ProofCommandModel.h"
#include "Commands/ProofOwnerIds.h"
#include "Skirmish/ProofSkirmishBot.h"
#include "Runtime/ProofRefineryMinerEvidence.h"
#include "Runtime/ProofStopEvidence.h"
#include "Runtime/ProofFactoryRallyEvidence.h"
#include "Runtime/ProofAttackMoveEvidence.h"
#include "Runtime/ProofAttackMoveRuntimeState.h"
#include "ProofGameMode.generated.h"

class AProofPlayerController;

UCLASS()
class REDALERTPROOF_API AProofGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AProofGameMode();
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void Tick(float DeltaSeconds) override;
    FProofCommandResult SubmitForOwner(int32 TrustedOwnerId, const FProofCommand& Command);
    FProofCommandResult SubmitForController(const AProofPlayerController* Controller, const FProofCommand& Command);
    bool RequestRematch(const AProofPlayerController* Controller, int32 ExpectedEpoch);
    int32 GetMatchEpoch() const { return MatchEpoch; }
    bool IsCurrentController(const AProofPlayerController* Controller) const;
private:
    FProofCommandModel Model;
    FProofAttackMoveRuntimeState AttackMoveRuntime;
    FProofAttackMoveEvidence AttackMoveEvidence;
    void TickAttackMoveRuntime(float DeltaSeconds);
    FProofRefineryMinerEvidence RefineryEvidence;
    FProofStopEvidence StopEvidence;
    FProofFactoryRallyEvidence FactoryRallyEvidence;
    FProofSkirmishBot SkirmishBot;
    FProofHumanOwnerAllocator HumanOwnerIds;
    TWeakObjectPtr<AProofPlayerController> SkirmishOwner1;
    TWeakObjectPtr<AProofPlayerController> SkirmishOwner2;
    TSet<int32> SkirmishSeededOwners;
    int32 MatchEpoch = 0;
    int32 BotMover = INDEX_NONE;
    int32 BotAttacker = INDEX_NONE;
    int32 BotSequence = 0;
    int32 BotTarget = INDEX_NONE;
    int32 CoopContextCommandRows = 0;
    int32 HumanAccepted = 0;
    int32 RejectedOwnership = 0;
    int32 SkirmishOwnershipDiagnosticRows = 0;
    int32 BotAccepted = 0;
    bool bAutomation = false;
    bool bControlsAutomation = false;
    bool bTypeSelectionControls = false;
    double TypeSelectionSimulationSeconds = 0.0;
    int32 TypeSelectionOwnerSubmissions = 0;
    int32 TypeSelectionClockRows = 0;
    bool bSkirmish = false;
    bool bSkirmishFixtureHost = false;
    bool bLifecycleFixtureHost = false;
    FString LifecycleControl;
    bool bSkirmishFixtureStarted = false;
    bool bSkirmishTerminalPublished = false;
    FProofSnapshot SkirmishTerminalSnapshot;
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
    // Accepted-command provenance only; no access to the model's private orders.
    struct FReturnCargoEvidenceOrder
    {
        int32 Sequence = 0;
        int32 Depot = INDEX_NONE;
        int32 ResumeResource = INDEX_NONE;
        bool bTravelLogged = false;
    };
    bool bReturnCargoEvidence = false;
    int32 ReturnCargoEvidenceRows = 0;
    TMap<int32, int32> ReturnCargoGatherSources;
    TMap<int32, FReturnCargoEvidenceOrder> ReturnCargoActive;
    TMap<int32, FReturnCargoEvidenceOrder> ReturnCargoAwaitingResume;
    bool ReturnCargoEvidenceRow();
    void ObserveReturnCargoCommand(int32 Owner, const FProofCommand& Command, const FProofCommandResult& Result,
        const FProofSnapshot& Before, const FProofSnapshot& After);
    void ObserveReturnCargoAdvance(const FProofSnapshot& Before, const FProofSnapshot& After);
    TSet<int32> GroupGatherDepositMiners;
    void ObserveGroupGatherDeposits(const FProofSnapshot& Before, const FProofSnapshot& After);
    void Publish();
    void ResetSkirmishFixtureState();
    void StartBotIfReady();
    void ObserveSkirmish(const FProofSnapshot& Snapshot);
    void ObserveControlsStep(const FProofSnapshot& Before, const FProofSnapshot& After);
    bool LogControlsCounts(const TCHAR* Phase, int32 ExpectedHumanUnits) const;
};
