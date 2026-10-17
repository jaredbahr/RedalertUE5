#include "Runtime/ProofGameMode.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Presentation/ProofRTSPawn.h"
#include "Presentation/ProofHUD.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

AProofGameMode::AProofGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    GameStateClass = AProofGameState::StaticClass();
    PlayerControllerClass = AProofPlayerController::StaticClass();
    DefaultPawnClass = AProofRTSPawn::StaticClass();
    HUDClass = AProofHUD::StaticClass();
    bAutomation = FParse::Param(FCommandLine::Get(), TEXT("ProofAutomation"));
    bControlsAutomation = FParse::Param(FCommandLine::Get(), TEXT("ProofControlsAutomation"));
    bTypeSelectionControls = FParse::Param(FCommandLine::Get(), TEXT("ProofTypeSelectionControls"));
    bSkirmish = FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmish"));
    bLifecycleFixtureHost = bSkirmish && FParse::Param(FCommandLine::Get(), TEXT("ProofLifecycleFixtureHost"));
    FParse::Value(FCommandLine::Get(), TEXT("ProofLifecycleControl="), LifecycleControl);
    bSkirmishFixtureHost = bSkirmish && !bLifecycleFixtureHost && FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmishFixtureHost"));
    MatchEpoch = bSkirmish ? 1 : 0;
    // PostLogin may precede BeginPlay for the first local player.
    if (bSkirmish) Model.StartSkirmish();
}

void AProofGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (bTypeSelectionControls && (!bControlsAutomation || bSkirmish || FParse::Param(FCommandLine::Get(), TEXT("ProofStopControls"))))
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_TYPE_SELECTION_ERROR incompatible_fixture_flags"));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    if (bSkirmish)
    {
        if (!Model.SeedSkirmishPlayer(ProofOwnerIds::Bot, 1, FVector(1400.f, 0.f, 0.f)))
            UE_LOG(LogTemp, Error, TEXT("PROOF_SKIRMISH_SEED_FAIL owner=100"));
        SkirmishBot.Reset();
        Publish();
        UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_READY fixture=%d"), bSkirmishFixtureHost ? 1 : 0);
        if (bLifecycleFixtureHost) UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_READY epoch=%d"), MatchEpoch);
        return;
    }
    Model.AddPlayer(ProofOwnerIds::Bot);
    BotMover = Model.SeedUnit(ProofOwnerIds::Bot, FVector(0.f, 1000.f, 0.f));
    const FVector AttackerStart = bControlsAutomation ? FVector(600.f, 600.f, 0.f) : FVector(0.f, 150.f, 0.f);
    BotAttacker = Model.SeedUnit(ProofOwnerIds::Bot, AttackerStart);
    InitialPositions.Add(BotMover, FVector(0.f, 1000.f, 0.f));
    InitialPositions.Add(BotAttacker, AttackerStart);
    if (bControlsAutomation)
    {
        // A separate stationary record gives the graphical human command test
        // an isolated combat target. It has no scripted bot order.
        const int32 ControlsTarget = Model.SeedUnit(ProofOwnerIds::Bot, FVector::ZeroVector);
        InitialPositions.Add(ControlsTarget, FVector::ZeroVector);
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_TARGET unit=%d owner=100"), ControlsTarget);
    }
    Publish();
    UE_LOG(LogTemp, Display, TEXT("PROOF_READY automation=%d"), bAutomation);
}

void AProofGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (AProofPlayerController* Controller = Cast<AProofPlayerController>(NewPlayer))
    {
        if (bSkirmish)
        {
            const int32 OwnerId = !SkirmishOwner1.IsValid() ? 1 : !SkirmishOwner2.IsValid() ? 2 : INDEX_NONE;
            if (OwnerId == INDEX_NONE)
            {
                UE_LOG(LogTemp, Warning, TEXT("PROOF_PLAYER_REJECT reason=skirmish-full"));
                if (bLifecycleFixtureHost) UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_REJECT reason=full"));
                Controller->ClientLifecycleRejected();
                // Give the reliable rejection notice one short replication window.
                Controller->SetLifeSpan(1.f);
                return;
            }
            const FVector HQPosition = OwnerId == 1 ? FVector(-1400.f, -400.f, 0.f) : FVector(-1400.f, 600.f, 0.f);
            // A weak binding can expire before Logout. Retire any orphaned owner
            // assets before this fresh connection is allowed to use the slot.
            if (SkirmishSeededOwners.Contains(OwnerId) &&
                Model.GetSnapshot().Players.ContainsByPredicate([OwnerId](const FProofPlayerRecord& P) { return P.OwnerId == OwnerId; }))
                Model.RemoveSkirmishPlayer(OwnerId);
            // Each seat is seeded once per match. Rejoins wait for rematch; otherwise
            // disconnect/reconnect could mint unlimited starting credits and ore.
            const bool bSeedNow = Model.GetSnapshot().MatchPhase == EProofMatchPhase::Playing &&
                !SkirmishSeededOwners.Contains(OwnerId);
            if (bSeedNow && !Model.SeedSkirmishPlayer(OwnerId, 0, HQPosition))
            {
                UE_LOG(LogTemp, Error, TEXT("PROOF_SKIRMISH_SEED_FAIL owner=%d"), OwnerId);
                Controller->Destroy();
                return;
            }
            if (bSeedNow) SkirmishSeededOwners.Add(OwnerId);
            (OwnerId == 1 ? SkirmishOwner1 : SkirmishOwner2) = Controller;
            Controller->OwnerId = OwnerId;
            Controller->ForceNetUpdate();
            Publish();
            UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_PLAYER owner=%d team=0 hqX=%.0f hqY=%.0f"), OwnerId, HQPosition.X, HQPosition.Y);
            if (bLifecycleFixtureHost) UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_JOIN owner=%d epoch=%d seeded=%d"), OwnerId, MatchEpoch, bSeedNow ? 1 : 0);
            if (bLifecycleFixtureHost && !bSkirmishFixtureStarted &&
                SkirmishSeededOwners.Contains(1) && SkirmishSeededOwners.Contains(2))
            {
                bSkirmishFixtureStarted = true;
                Accumulator = 0.f;
                UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_FIXTURE_START owners=1,2 simulationSeconds=0"));
            }
            return;
        }
        // Connection order is fixture identity only; there is no login/authentication implementation.
        int32 OwnerId = INDEX_NONE;
        if (!HumanOwnerIds.TryAllocate(OwnerId))
        {
            UE_LOG(LogTemp, Warning, TEXT("PROOF_PLAYER_REJECT reason=owner-id-exhausted"));
            Controller->Destroy();
            return;
        }
        Controller->OwnerId = OwnerId;
        if (bSkirmish)
        {
            if (OwnerId > 2)
            {
                UE_LOG(LogTemp, Warning, TEXT("PROOF_PLAYER_REJECT reason=skirmish-full owner=%d"), OwnerId);
                Controller->Destroy();
                return;
            }
            const FVector HQPosition = OwnerId == 1 ? FVector(-1400.f, -400.f, 0.f) : FVector(-1400.f, 600.f, 0.f);
            if (!Model.SeedSkirmishPlayer(OwnerId, 0, HQPosition))
            {
                UE_LOG(LogTemp, Error, TEXT("PROOF_SKIRMISH_SEED_FAIL owner=%d"), OwnerId);
                Controller->Destroy();
                return;
            }
            UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_PLAYER owner=%d team=0 hqX=%.0f hqY=%.0f"),
                OwnerId, HQPosition.X, HQPosition.Y);
            Controller->ForceNetUpdate();
            Publish();
            return;
        }
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

void AProofGameMode::Logout(AController* Exiting)
{
    AProofPlayerController* Controller = Cast<AProofPlayerController>(Exiting);
    if (bSkirmish && Controller && IsCurrentController(Controller))
    {
        const int32 OwnerId = Controller->OwnerId;
        (OwnerId == 1 ? SkirmishOwner1 : SkirmishOwner2).Reset();
        Model.RemoveSkirmishPlayer(OwnerId);
        if (!HasActorBegunPlay()) SkirmishSeededOwners.Remove(OwnerId);
        Publish();
        const FProofSnapshot AfterLogout = Model.GetSnapshot();
        if (AfterLogout.MatchPhase == EProofMatchPhase::Finished && !bSkirmishTerminalPublished)
        {
            SkirmishTerminalSnapshot = AfterLogout;
            bSkirmishTerminalPublished = true;
            SkirmishTerminalAt = Lifetime;
            UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_RESULT winnerTeam=%d revision=%d"), AfterLogout.WinnerTeam, AfterLogout.Revision);
            if (bLifecycleFixtureHost) UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_TERMINAL epoch=%d winnerTeam=%d"), MatchEpoch, AfterLogout.WinnerTeam);
        }
        if (bLifecycleFixtureHost) UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_LOGOUT owner=%d epoch=%d"), OwnerId, MatchEpoch);
    }
    Super::Logout(Exiting);
}

bool AProofGameMode::IsCurrentController(const AProofPlayerController* Controller) const
{
    if (!Controller || !bSkirmish) return false;
    return Controller->OwnerId == 1 ? SkirmishOwner1.Get() == Controller
        : Controller->OwnerId == 2 && SkirmishOwner2.Get() == Controller;
}

FProofCommandResult AProofGameMode::SubmitForController(const AProofPlayerController* Controller, const FProofCommand& Command)
{
    if (bSkirmish && !IsCurrentController(Controller))
    {
        FProofCommandResult Result;
        Result.Sequence = Command.Sequence;
        Result.MatchEpoch = Command.MatchEpoch;
        Result.Reason = TEXT("inactive-controller");
        return Result;
    }
    return SubmitForOwner(Controller ? Controller->OwnerId : INDEX_NONE, Command);
}

bool AProofGameMode::RequestRematch(const AProofPlayerController* Controller, int32 ExpectedEpoch)
{
    if (!bSkirmish || !IsCurrentController(Controller) || Controller->OwnerId != 1 ||
        ExpectedEpoch != MatchEpoch || MatchEpoch == MAX_int32 ||
        Model.GetSnapshot().MatchPhase != EProofMatchPhase::Finished)
        return false;
    ++MatchEpoch;
    Model.StartSkirmish();
    if (!Model.SeedSkirmishPlayer(ProofOwnerIds::Bot, 1, FVector(1400.f, 0.f, 0.f))) return false;
    SkirmishSeededOwners.Reset();
    if (SkirmishOwner1.IsValid() && !Model.SeedSkirmishPlayer(1, 0, FVector(-1400.f, -400.f, 0.f))) return false;
    if (SkirmishOwner1.IsValid()) SkirmishSeededOwners.Add(1);
    if (SkirmishOwner2.IsValid() && !Model.SeedSkirmishPlayer(2, 0, FVector(-1400.f, 600.f, 0.f))) return false;
    if (SkirmishOwner2.IsValid()) SkirmishSeededOwners.Add(2);
    if (bLifecycleFixtureHost && SkirmishOwner2.IsValid())
        UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_RESEED owner=2 epoch=%d credits=350"), MatchEpoch);
    ResetSkirmishFixtureState();
    Publish();
    UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_REMATCH epoch=%d"), MatchEpoch);
    return true;
}

void AProofGameMode::ResetSkirmishFixtureState()
{
    SkirmishBot.Reset();
    SkirmishSeconds = 0.0;
    SkirmishTerminalAt = -1.f;
    bSkirmishTerminalPublished = false;
    SkirmishTerminalSnapshot = FProofSnapshot();
    bSkirmishFixtureStarted = false;
    bSkirmishFixtureFinished = false;
    bSkirmishHumanRefinery = bSkirmishHumanFactory = bSkirmishHumanGathered = bSkirmishHumanProduced = false;
    bSkirmishBotRefinery = bSkirmishBotFactory = bSkirmishBotProduced = bSkirmishBotAttackAccepted = false;
    Accumulator = 0.f;
    StepsSincePublish = 0;
}

FProofCommandResult AProofGameMode::SubmitForOwner(int32 TrustedOwnerId, const FProofCommand& Command)
{
    if (bTypeSelectionControls && TrustedOwnerId == 1) ++TypeSelectionOwnerSubmissions;
    if (bSkirmish && Command.MatchEpoch != MatchEpoch)
    {
        FProofCommandResult Result;
        Result.Sequence = Command.Sequence;
        Result.MatchEpoch = Command.MatchEpoch;
        Result.Reason = TEXT("stale-epoch");
        if (StopEvidence.WantsCommand(Command.Type))
        {
            const FProofSnapshot RejectedSnapshot = Model.GetSnapshot();
            StopEvidence.ObserveCommand(MatchEpoch, TrustedOwnerId, Command, Result, RejectedSnapshot, RejectedSnapshot);
        }
        if (bLifecycleFixtureHost) UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_STALE_REJECT epoch=%d current=%d owner=%d sequence=%d"), Command.MatchEpoch, MatchEpoch, TrustedOwnerId, Command.Sequence);
        return Result;
    }
    const bool bRefineryEvidence = RefineryEvidence.Enabled() && bSkirmish
        && (Command.Type == EProofCommandType::BuildRefinery || Command.Type == EProofCommandType::Gather);
    const FProofSnapshot EvidenceBefore = bRefineryEvidence ? Model.GetSnapshot() : FProofSnapshot();
    TOptional<FProofSnapshot> StopBefore;
    if (StopEvidence.WantsCommand(Command.Type)) { StopBefore.Emplace(Model.GetSnapshot()); }
    FProofCommandResult Result = Model.Submit(TrustedOwnerId, Command);
    if (bSkirmish && !bLifecycleFixtureHost && Command.Type == EProofCommandType::Attack
        && !Result.bAccepted && Result.Reason == TEXT("ownership"))
    {
        FString ScriptedScenario;
        const bool bScriptedFixture = bSkirmishFixtureHost
            || (FParse::Value(FCommandLine::Get(), TEXT("ProofSkirmishScenario="), ScriptedScenario)
                && (ScriptedScenario == TEXT("Victory") || ScriptedScenario == TEXT("Defeat") || ScriptedScenario == TEXT("Coop")));
        if (bScriptedFixture)
        {
            if (SkirmishOwnershipDiagnosticRows >= 64)
            {
                if (SkirmishOwnershipDiagnosticRows == 64)
                {
                    ++SkirmishOwnershipDiagnosticRows; // Latch the single explicit overflow failure.
                    UE_LOG(LogTemp, Error, TEXT("PROOF_SKIRMISH_OWNERSHIP_REJECTION_ERROR reason=diagnostic-limit"));
                    FPlatformMisc::RequestExitWithStatus(false, 1);
                }
            }
            else
            {
                ++SkirmishOwnershipDiagnosticRows;
                // Submit has returned without an intervening Advance. Report the
                // exact validation state; do not infer why ownership rejected.
                const FProofSnapshot RejectedState = Model.GetSnapshot();
                const FProofUnitRecord* RejectedUnit = RejectedState.Units.FindByPredicate(
                    [&Command](const FProofUnitRecord& Unit) { return Unit.UnitId == Command.UnitId; });
                UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_OWNERSHIP_REJECTION owner=%d sequence=%lld unit=%d exists=%d unitOwner=%d hp=%.9g revision=%d epoch=%d type=Attack reason=ownership"),
                    TrustedOwnerId, static_cast<long long>(Command.Sequence), Command.UnitId, RejectedUnit ? 1 : 0,
                    RejectedUnit ? RejectedUnit->OwnerId : INDEX_NONE, RejectedUnit ? static_cast<double>(RejectedUnit->HitPoints) : 0.0,
                    RejectedState.Revision, MatchEpoch);
            }
        }
    }
    if (StopBefore.IsSet()) { StopEvidence.ObserveCommand(MatchEpoch, TrustedOwnerId, Command, Result, StopBefore.GetValue(), Model.GetSnapshot()); }
    if (bRefineryEvidence) RefineryEvidence.ObserveCommand(MatchEpoch, TrustedOwnerId, Command, Result, EvidenceBefore, Model.GetSnapshot());
    Result.MatchEpoch = Command.MatchEpoch;
    if (bSkirmish && Result.bAccepted && TrustedOwnerId == ProofOwnerIds::Bot && Command.Type == EProofCommandType::Attack)
        bSkirmishBotAttackAccepted = true;
    if (bControlsAutomation && Result.bAccepted && TrustedOwnerId != 100 && Command.Type == EProofCommandType::Attack)
    {
        // Conservatively discard attribution if another source could damage
        // this scenario's passive target at any point after bot start.
        HumanAttackTargets.Add(Command.TargetUnitId);
        if (Command.TargetUnitId == BotTarget) bBotTargetExclusive = false;
    }
    if (TrustedOwnerId == 100 && Result.bAccepted) ++BotAccepted;
    if (TrustedOwnerId != 100 && Result.bAccepted) ++HumanAccepted;
    if (TrustedOwnerId != 100 && Command.Sequence == 4 && !Result.bAccepted && Result.Reason == TEXT("ownership")) ++RejectedOwnership;
    UE_LOG(LogTemp, Display, TEXT("PROOF_HOST_RESULT owner=%d sequence=%lld accepted=%d reason=%s"),
        TrustedOwnerId, static_cast<long long>(Result.Sequence), Result.bAccepted, *Result.Reason);
    return Result;
}

void AProofGameMode::StartBotIfReady()
{
    if (bBotStarted || (!bAutomation && !bControlsAutomation)) return;
    const FProofSnapshot Snapshot = Model.GetSnapshot();
    int32 HumanCount = 0, Target = INDEX_NONE;
    for (const FProofPlayerRecord& Player : Snapshot.Players)
        if (Player.OwnerId != 100) ++HumanCount;
    if (HumanCount < 2) return;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
        if (Unit.OwnerId == (bControlsAutomation ? 2 : 1)) { Target = Unit.UnitId; break; }
    if (Target == INDEX_NONE) return;
    if (bControlsAutomation) bControlsInitialCountsValid = LogControlsCounts(TEXT("initial"), 2);
    bBotStarted = true;
    BotTarget = Target;
    bBotTargetExclusive = !HumanAttackTargets.Contains(Target);
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
    if (bControlsAutomation)
    {
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_BOT_START mover=%d attacker=%d target=%d targetOwner=2"),
            BotMover, BotAttacker, BotTarget);
    }
}

bool AProofGameMode::LogControlsCounts(const TCHAR* Phase, int32 ExpectedHumanUnits) const
{
    const FProofSnapshot Snapshot = Model.GetSnapshot();
    int32 HumanOwners = 0, BotOwners = 0, HumanUnits = 0, BotUnits = 0, LiveUnits = 0;
    for (const FProofPlayerRecord& PlayerRecord : Snapshot.Players)
    {
        if (PlayerRecord.OwnerId == 100) ++BotOwners;
        else ++HumanOwners;
    }
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.OwnerId == 100) ++BotUnits;
        else ++HumanUnits;
        if (Unit.HitPoints > 0.f) ++LiveUnits;
    }
    // Unit records survive death in this fixture; live counts are observed,
    // while the initial/final record totals prove exactly one production.
    const bool bValid = HumanOwners == 2 && BotOwners == 1 && HumanUnits == ExpectedHumanUnits &&
        BotUnits == 3 && Snapshot.Units.Num() == ExpectedHumanUnits + 3;
    UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_COUNTS phase=%s humanOwners=%d botOwners=%d humanUnits=%d botUnits=%d liveUnits=%d totalUnits=%d status=%s"),
        Phase, HumanOwners, BotOwners, HumanUnits, BotUnits, LiveUnits, Snapshot.Units.Num(), bValid ? TEXT("pass") : TEXT("fail"));
    return bValid;
}

void AProofGameMode::ObserveControlsStep(const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (!bBotStarted) return;
    const FProofUnitRecord* Mover = After.Units.FindByPredicate([this](const FProofUnitRecord& Unit) { return Unit.UnitId == BotMover; });
    const FVector* MoverStart = InitialPositions.Find(BotMover);
    const FVector MoveDestination(1000.f, 1000.f, 0.f);
    if (!bBotMoved && Mover && MoverStart && Mover->OwnerId == 100 && FVector::Dist(Mover->Position, MoveDestination) <= 1.f)
    {
        bBotMoved = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_BOT_MOVE unit=%d owner=100 distance=%.3f destinationX=1000 destinationY=1000 destinationZ=0 remaining=%.3f tolerance=1 revision=%d"),
            BotMover, FVector::Dist(Mover->Position, *MoverStart), FVector::Dist(Mover->Position, MoveDestination), After.Revision);
    }
    const FProofUnitRecord* TargetBefore = Before.Units.FindByPredicate([this](const FProofUnitRecord& Unit) { return Unit.UnitId == BotTarget; });
    const FProofUnitRecord* TargetAfter = After.Units.FindByPredicate([this](const FProofUnitRecord& Unit) { return Unit.UnitId == BotTarget; });
    const FProofUnitRecord* AttackerBefore = Before.Units.FindByPredicate([this](const FProofUnitRecord& Unit) { return Unit.UnitId == BotAttacker; });
    const FProofUnitRecord* AttackerAfter = After.Units.FindByPredicate([this](const FProofUnitRecord& Unit) { return Unit.UnitId == BotAttacker; });
    if (!bBotDamaged && bBotTargetExclusive && TargetBefore && TargetAfter && AttackerBefore && AttackerAfter &&
        TargetAfter->OwnerId == 2 && AttackerBefore->OwnerId == 100 && AttackerBefore->HitPoints > 0.f &&
        TargetBefore->HitPoints > TargetAfter->HitPoints &&
        FVector::DistSquared(AttackerAfter->Position, TargetAfter->Position) <= FMath::Square(static_cast<double>(FProofCommandModel::AttackRange)))
    {
        bBotDamaged = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_BOT_DAMAGE attacker=%d attackerOwner=100 target=%d targetOwner=2 beforeHP=%.3f afterHP=%.3f revision=%d"),
            BotAttacker, BotTarget, TargetBefore->HitPoints, TargetAfter->HitPoints, After.Revision);
    }
}

void AProofGameMode::Publish()
{
    if (AProofGameState* State = GetGameState<AProofGameState>())
    {
        FProofSnapshot Snapshot = Model.GetSnapshot();
        Snapshot.MatchEpoch = MatchEpoch;
        State->Publish(Snapshot);
        RefineryEvidence.LogSnapshot(Snapshot, TEXT("host"), 0);
        if (StopEvidence.Enabled()) { StopEvidence.LogSnapshot(Snapshot, TEXT("host"), 0); }
        if (bTypeSelectionControls)
        {
            if (++TypeSelectionClockRows > 1024)
            {
                UE_LOG(LogTemp, Error, TEXT("PROOF_TYPE_SELECTION_ERROR clock_row_cap"));
                FPlatformMisc::RequestExitWithStatus(false, 1);
            }
            else
            {
                TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
                Row->SetNumberField(TEXT("epoch"), Snapshot.MatchEpoch); Row->SetNumberField(TEXT("revision"), Snapshot.Revision);
                Row->SetStringField(TEXT("digest"), Snapshot.ComputeDigest());
                Row->SetNumberField(TEXT("simulatedSeconds"), TypeSelectionSimulationSeconds);
                Row->SetNumberField(TEXT("owner1Submissions"), TypeSelectionOwnerSubmissions);
                TArray<TSharedPtr<FJsonValue>> Units;
                for (const FProofUnitRecord& Unit : Snapshot.Units)
                {
                    TSharedRef<FJsonObject> U = MakeShared<FJsonObject>();
                    U->SetNumberField(TEXT("id"),Unit.UnitId); U->SetNumberField(TEXT("owner"),Unit.OwnerId);
                    U->SetNumberField(TEXT("kind"),static_cast<int32>(Unit.Kind)); U->SetNumberField(TEXT("hp"),Unit.HitPoints); U->SetNumberField(TEXT("cargo"),Unit.Cargo);
                    U->SetNumberField(TEXT("x"),Unit.Position.X); U->SetNumberField(TEXT("y"),Unit.Position.Y); U->SetNumberField(TEXT("z"),Unit.Position.Z);
                    Units.Add(MakeShared<FJsonValueObject>(U));
                }
                Row->SetArrayField(TEXT("units"),Units);
                FString Json; const TSharedRef<TJsonWriter<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>> Writer=TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
                FJsonSerializer::Serialize(Row,Writer);
                UE_LOG(LogTemp,Display,TEXT("PROOF_TYPE_SELECTION_CLOCK %s"),*Json);
            }
        }
    }
}

void AProofGameMode::ObserveSkirmish(const FProofSnapshot& Snapshot)
{
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.OwnerId != ProofOwnerIds::Bot && Unit.OwnerId != 1 && Unit.OwnerId != 2) continue;
        const bool bBot = Unit.OwnerId == ProofOwnerIds::Bot;
        if (Unit.Kind == EProofUnitKind::Refinery) (bBot ? bSkirmishBotRefinery : bSkirmishHumanRefinery) = true;
        if (Unit.Kind == EProofUnitKind::Factory) (bBot ? bSkirmishBotFactory : bSkirmishHumanFactory) = true;
        if (Unit.Kind == EProofUnitKind::Combat) (bBot ? bSkirmishBotProduced : bSkirmishHumanProduced) = true;
    }
    for (const FProofPlayerRecord& Player : Snapshot.Players)
        if ((Player.OwnerId == 1 || Player.OwnerId == 2) && Player.TotalGathered > 0) bSkirmishHumanGathered = true;
}

void AProofGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bLifecycleFixtureHost && !LifecycleControl.IsEmpty() &&
        IFileManager::Get().FileExists(*FPaths::Combine(LifecycleControl, TEXT("finish"))))
    {
        const bool bPass = MatchEpoch >= 3;
        UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_COMPLETE role=host status=%s epoch=%d"), bPass ? TEXT("pass") : TEXT("fail"), MatchEpoch);
        FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
        return;
    }
    Lifetime += DeltaSeconds;
    Accumulator += DeltaSeconds;
    if ((bSkirmishFixtureHost || bLifecycleFixtureHost) && !bSkirmishFixtureStarted)
    {
        // The harness launches clients serially. Do not give the bot a loading-
        // time head start; this is a fixture roster barrier, not a manual lobby.
        const FProofSnapshot Waiting = Model.GetSnapshot();
        const bool bOwner1 = Waiting.Players.ContainsByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 1 && P.TeamId == 0; });
        const bool bOwner2 = Waiting.Players.ContainsByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 2 && P.TeamId == 0; });
        Accumulator = 0.f;
        if (bOwner1 && bOwner2)
        {
            bSkirmishFixtureStarted = true;
            UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_FIXTURE_START owners=1,2 simulationSeconds=0"));
        }
        // Lifetime and the fixture timeout still advance while waiting.
    }
    if (!bSkirmish) StartBotIfReady();
    // Bounded catch-up avoids a long stalled frame monopolizing the game thread.
    int32 CatchupSteps = 0;
    const bool bLifecyclePlay = !bLifecycleFixtureHost ||
        (!LifecycleControl.IsEmpty() && IFileManager::Get().FileExists(*FPaths::Combine(LifecycleControl, FString::Printf(TEXT("play-epoch-%d"), MatchEpoch))));
    if (!bLifecyclePlay) Accumulator = 0.f;
    while (bLifecyclePlay && Accumulator >= 0.1f && CatchupSteps++ < 10)
    {
        if (bSkirmish)
        {
            const FProofSnapshot Before = Model.GetSnapshot();
            if (Before.MatchPhase == EProofMatchPhase::Playing)
            {
                for (FProofCommand Command : SkirmishBot.Decide(Before, ProofOwnerIds::Bot, SkirmishSeconds))
                {
                    Command.MatchEpoch = MatchEpoch;
                    SubmitForOwner(ProofOwnerIds::Bot, Command);
                }
                const FProofSnapshot EvidenceBeforeAdvance = RefineryEvidence.Enabled() ? Model.GetSnapshot() : FProofSnapshot();
                Model.Advance(0.1f);
                if (StopEvidence.Enabled()) { StopEvidence.ObserveAdvance(MatchEpoch, 0.1f); }
                SkirmishSeconds += 0.1;
                const FProofSnapshot After = Model.GetSnapshot();
                if (RefineryEvidence.Enabled()) RefineryEvidence.ObserveAdvance(MatchEpoch, EvidenceBeforeAdvance, After);
                ObserveSkirmish(After);
                if (After.MatchPhase == EProofMatchPhase::Finished && !bSkirmishTerminalPublished)
                {
                    SkirmishTerminalSnapshot = After;
                    Publish();
                    bSkirmishTerminalPublished = true;
                    SkirmishTerminalAt = Lifetime;
                    UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_RESULT winnerTeam=%d revision=%d"), After.WinnerTeam, After.Revision);
                    if (bLifecycleFixtureHost) UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_TERMINAL epoch=%d winnerTeam=%d"), MatchEpoch, After.WinnerTeam);
                }
                else if (++StepsSincePublish >= 2) { Publish(); StepsSincePublish = 0; }
            }
            Accumulator -= 0.1f;
            continue;
        }
        const FProofSnapshot Before = bControlsAutomation ? Model.GetSnapshot() : FProofSnapshot();
        Model.Advance(0.1f);
        if (StopEvidence.Enabled()) { StopEvidence.ObserveAdvance(MatchEpoch, 0.1f); }
        if (bTypeSelectionControls) TypeSelectionSimulationSeconds += static_cast<double>(0.1f);
        if (bControlsAutomation) ObserveControlsStep(Before, Model.GetSnapshot());
        Accumulator -= 0.1f;
        if (++StepsSincePublish >= 2) { Publish(); StepsSincePublish = 0; }
    }
    if (bSkirmish)
    {
        if (bSkirmishFixtureHost && !bSkirmishFixtureFinished &&
            (SkirmishTerminalAt >= 0.f && Lifetime - SkirmishTerminalAt >= 5.f || Lifetime >= 240.f))
        {
            bSkirmishFixtureFinished = true;
            const FProofSnapshot Snapshot = bSkirmishTerminalPublished ? SkirmishTerminalSnapshot : Model.GetSnapshot();
            const bool bOwner1 = Snapshot.Players.ContainsByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 1 && P.TeamId == 0; });
            const bool bOwner2 = Snapshot.Players.ContainsByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 2 && P.TeamId == 0; });
            const bool bPass = bOwner1 && bOwner2 && Snapshot.MatchPhase == EProofMatchPhase::Finished && Snapshot.WinnerTeam == 0
                && bSkirmishHumanRefinery && bSkirmishHumanFactory && bSkirmishHumanGathered && bSkirmishHumanProduced
                && bSkirmishBotRefinery && bSkirmishBotFactory && bSkirmishBotProduced && bSkirmishBotAttackAccepted;
            UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_COMPLETE role=host status=%s scenario=Coop winnerTeam=%d humanBuilt=%d humanGathered=%d humanProduced=%d botBuilt=%d botProduced=%d botAttack=%d"),
                bPass ? TEXT("pass") : TEXT("fail"), Snapshot.WinnerTeam,
                bSkirmishHumanRefinery && bSkirmishHumanFactory, bSkirmishHumanGathered, bSkirmishHumanProduced,
                bSkirmishBotRefinery && bSkirmishBotFactory, bSkirmishBotProduced, bSkirmishBotAttackAccepted);
            FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
        }
        return;
    }
    if (!bAutomation && !bControlsAutomation) return;
    if (bControlsAutomation)
    {
        const bool bActiveComplete = bBotStarted && Lifetime - BotStartedAt >= 20.f;
        const bool bStartupTimedOut = !bBotStarted && Lifetime >= 60.f;
        if (!bControlsComplete && (bActiveComplete || bStartupTimedOut))
        {
            const bool bFinalCountsValid = LogControlsCounts(TEXT("final"), 3);
            bControlsComplete = true;
            bControlsPassed = bBotStarted && BotAccepted >= 2 && bBotMoved && bBotDamaged && bBotTargetExclusive &&
                bControlsInitialCountsValid && bFinalCountsValid;
            UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_COMPLETE role=host owner=0 status=%s botAccepted=%d botMoved=%d botDamaged=%d exclusive=%d"),
                bControlsPassed ? TEXT("pass") : TEXT("fail"), BotAccepted, bBotMoved, bBotDamaged, bBotTargetExclusive);
        }
        // Hold the server until clients finish at 23s, avoiding a disconnect
        // before their evidence is written. Counts above reflect completion.
        if (bControlsComplete && (bStartupTimedOut || Lifetime - BotStartedAt >= 24.f))
            FPlatformMisc::RequestExitWithStatus(false, bControlsPassed ? 0 : 1);
        return;
    }
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
        const bool bPass = Snapshot.Players.ContainsByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 1; })
            && Snapshot.Players.ContainsByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 2; })
            && HumanAccepted >= 6 && RejectedOwnership >= 2 &&
            BotAccepted >= 2 && bMoved && bDamaged && bProduced;
        UE_LOG(LogTemp, Display, TEXT("PROOF_AUTOMATION_COMPLETE role=host owner=0 status=%s humanAccepted=%d rejectedOwnership=%d botAccepted=%d moved=%d damaged=%d produced=%d"),
            bPass ? TEXT("pass") : TEXT("fail"), HumanAccepted, RejectedOwnership, BotAccepted, bMoved, bDamaged, bProduced);
        FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
    }
}
