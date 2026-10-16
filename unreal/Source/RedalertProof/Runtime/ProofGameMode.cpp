#include "Runtime/ProofGameMode.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Presentation/ProofRTSPawn.h"
#include "Presentation/ProofHUD.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

AProofGameMode::AProofGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    GameStateClass = AProofGameState::StaticClass();
    PlayerControllerClass = AProofPlayerController::StaticClass();
    DefaultPawnClass = AProofRTSPawn::StaticClass();
    HUDClass = AProofHUD::StaticClass();
    bAutomation = FParse::Param(FCommandLine::Get(), TEXT("ProofAutomation"));
}

void AProofGameMode::BeginPlay()
{
    Super::BeginPlay();
    Model.AddPlayer(ProofOwnerIds::Bot);
    BotMover = Model.SeedUnit(ProofOwnerIds::Bot, FVector(0.f, 1000.f, 0.f));
    BotAttacker = Model.SeedUnit(ProofOwnerIds::Bot, FVector(0.f, 150.f, 0.f));
    InitialPositions.Add(BotMover, FVector(0.f, 1000.f, 0.f));
    InitialPositions.Add(BotAttacker, FVector(0.f, 150.f, 0.f));
    Publish();
    UE_LOG(LogTemp, Display, TEXT("PROOF_READY automation=%d"), bAutomation);
}

void AProofGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (AProofPlayerController* Controller = Cast<AProofPlayerController>(NewPlayer))
    {
        // Connection order is fixture identity only; there is no login/authentication implementation.
        int32 OwnerId = INDEX_NONE;
        if (!HumanOwnerIds.TryAllocate(OwnerId))
        {
            // No model player, resources or unit is created on exhaustion.
            UE_LOG(LogTemp, Warning, TEXT("PROOF_PLAYER_REJECT reason=owner-id-exhausted"));
            Controller->Destroy();
            return;
        }
        Controller->OwnerId = OwnerId;
        const FVector Position(Controller->OwnerId == 1 ? -600.f : 600.f,
            (Controller->OwnerId - 1) / 2 * 600.f, 0.f);
        Model.AddPlayer(Controller->OwnerId);
        const int32 UnitId = Model.SeedUnit(Controller->OwnerId, Position);
        InitialPositions.Add(UnitId, Position);
        UE_LOG(LogTemp, Display, TEXT("PROOF_PLAYER owner=%d unit=%d"), Controller->OwnerId, UnitId);
        Controller->ForceNetUpdate();
        Publish();
    }
}

FProofCommandResult AProofGameMode::SubmitForOwner(int32 TrustedOwnerId, const FProofCommand& Command)
{
    FProofCommandResult Result = Model.Submit(TrustedOwnerId, Command);
    if (TrustedOwnerId == ProofOwnerIds::Bot && Result.bAccepted) ++BotAccepted;
    if (TrustedOwnerId != ProofOwnerIds::Bot && Result.bAccepted) ++HumanAccepted;
    if (TrustedOwnerId != ProofOwnerIds::Bot && Command.Sequence == 4 && !Result.bAccepted && Result.Reason == TEXT("ownership")) ++RejectedOwnership;
    UE_LOG(LogTemp, Display, TEXT("PROOF_HOST_RESULT owner=%d sequence=%lld accepted=%d reason=%s"),
        TrustedOwnerId, static_cast<long long>(Result.Sequence), Result.bAccepted, *Result.Reason);
    return Result;
}

void AProofGameMode::StartBotIfReady()
{
    if (bBotStarted || !bAutomation) return;
    const FProofSnapshot Snapshot = Model.GetSnapshot();
    int32 HumanCount = 0, Target = INDEX_NONE;
    for (const FProofPlayerRecord& Player : Snapshot.Players)
        if (!ProofOwnerIds::IsReserved(Player.OwnerId)) ++HumanCount;
    if (HumanCount < 2) return;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
        if (Unit.OwnerId == 1) { Target = Unit.UnitId; break; }
    if (Target == INDEX_NONE) return;
    bBotStarted = true;
    BotStartedAt = Lifetime;
    FProofCommand Move;
    Move.Sequence = ++BotSequence;
    Move.Type = EProofCommandType::Move;
    Move.UnitId = BotMover;
    Move.Destination = FVector(1000.f, 1000.f, 0.f);
    SubmitForOwner(ProofOwnerIds::Bot, Move);
    FProofCommand Attack;
    Attack.Sequence = ++BotSequence;
    Attack.Type = EProofCommandType::Attack;
    Attack.UnitId = BotAttacker;
    Attack.TargetUnitId = Target;
    SubmitForOwner(ProofOwnerIds::Bot, Attack);
}

void AProofGameMode::Publish()
{
    if (AProofGameState* State = GetGameState<AProofGameState>()) State->Publish(Model.GetSnapshot());
}

void AProofGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Lifetime += DeltaSeconds;
    Accumulator += DeltaSeconds;
    StartBotIfReady();
    // Bounded catch-up avoids a long stalled frame monopolizing the game thread.
    int32 CatchupSteps = 0;
    while (Accumulator >= 0.1f && CatchupSteps++ < 10)
    {
        Model.Advance(0.1f);
        Accumulator -= 0.1f;
        if (++StepsSincePublish >= 2) { Publish(); StepsSincePublish = 0; }
    }
    if (!bAutomation) return;
    const FProofSnapshot Snapshot = Model.GetSnapshot();
    TMap<int32, int32> Counts;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        ++Counts.FindOrAdd(Unit.OwnerId);
        if (const FVector* Initial = InitialPositions.Find(Unit.UnitId))
            bMoved |= FVector::Dist(*Initial, Unit.Position) > 20.f;
        bDamaged |= Unit.HitPoints < 100.f;
    }
    bProduced |= Counts.FindRef(1) >= 2 && Counts.FindRef(2) >= 2;
    const bool bActiveWindowComplete = bBotStarted && Lifetime - BotStartedAt >= 18.f;
    const bool bStartupTimedOut = !bBotStarted && Lifetime >= 60.f;
    if (bActiveWindowComplete || bStartupTimedOut)
    {
        const bool bFixtureOwnersPresent = Snapshot.Players.ContainsByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == 1; })
            && Snapshot.Players.ContainsByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == 2; });
        const bool bPass = bFixtureOwnersPresent && HumanAccepted >= 6 && RejectedOwnership >= 2 &&
            BotAccepted >= 2 && bMoved && bDamaged && bProduced;
        UE_LOG(LogTemp, Display, TEXT("PROOF_AUTOMATION_COMPLETE role=host owner=0 status=%s humanAccepted=%d rejectedOwnership=%d botAccepted=%d moved=%d damaged=%d produced=%d"),
            bPass ? TEXT("pass") : TEXT("fail"), HumanAccepted, RejectedOwnership, BotAccepted, bMoved, bDamaged, bProduced);
        FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
    }
}
