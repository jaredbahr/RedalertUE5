#include "Runtime/ProofPlayerController.h"
#include "Runtime/ProofGameMode.h"
#include "Runtime/ProofGameState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Engine/NetConnection.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"

AProofPlayerController::AProofPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    const bool bSkirmishMode = FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmish"));
    bAutomation = !bSkirmishMode && FParse::Param(FCommandLine::Get(), TEXT("ProofAutomation"));
    bControlsAutomation = !bSkirmishMode && FParse::Param(FCommandLine::Get(), TEXT("ProofControlsAutomation"));
    bControlsObserver = FParse::Param(FCommandLine::Get(), TEXT("ProofControlsObserver"));
    bSkirmishObserver = bSkirmishMode && FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmishObserver"));
    bIngressFixture = FParse::Param(FCommandLine::Get(), TEXT("ProofIngressFixture"));
    bIngressFlood = FParse::Param(FCommandLine::Get(), TEXT("ProofIngressFlood"));
    bIngressHealthy = FParse::Param(FCommandLine::Get(), TEXT("ProofIngressHealthy"));
    FParse::Value(FCommandLine::Get(), TEXT("ProofIngressRelease="), IngressRelease);
    FParse::Value(FCommandLine::Get(), TEXT("ProofLifecycleRole="), LifecycleRole);
    FParse::Value(FCommandLine::Get(), TEXT("ProofLifecycleControl="), LifecycleControl);
    bLifecycleFixture = bSkirmishMode && !LifecycleRole.IsEmpty();
}

void AProofPlayerController::SynchronizeMatchEpoch()
{
    const AProofGameState* State = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    if (!State || State->Snapshot.MatchEpoch <= LocalMatchEpoch) return;
    LocalMatchEpoch = State->Snapshot.MatchEpoch;
    NextLocalSequence = 1;
    Receipts.Reset();
    LastReceiptSequence = 0;
    ReceiptStatus = TEXT("No host receipt yet.");
    Acknowledged.Reset();
    LastLoggedRevision = -1;
    SkirmishObserverStep = 0;
    SkirmishObserverPendingSequence = INDEX_NONE;
    SkirmishObserverTerminalAt = -1.f;
    bSkirmishObserverReady = bSkirmishObserverRefinery = bSkirmishObserverFactory = false;
    bSkirmishObserverGathered = bSkirmishObserverProduced = false;
    LifecycleStep = 0;
    LifecyclePendingSequence = INDEX_NONE;
    bLifecycleRematchSent = bLifecycleStaleSent = false;
    UE_LOG(LogTemp, Display, TEXT("PROOF_CLIENT_EPOCH owner=%d epoch=%d"), OwnerId, LocalMatchEpoch);
}

int32 AProofPlayerController::NextLocalCommandSequence()
{
    SynchronizeMatchEpoch();
    return NextLocalSequence < MAX_int32 ? NextLocalSequence++ : 0;
}

bool AProofPlayerController::FindReceipt(int32 Sequence, FProofCommandResult& OutResult) const
{
    if (const FProofCommandResult* Result = Receipts.Find(Sequence)) { OutResult = *Result; return true; }
    return false;
}

void AProofPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AProofPlayerController, OwnerId);
}

bool AProofPlayerController::AdmitIngress()
{
    const bool bWasTripped = IngressBudget.IsTripped();
    const double Now = FPlatformTime::Seconds();
    if (!IngressBudget.TryAdmit(Now))
    {
        if (!bWasTripped)
        {
            UNetConnection* Connection = GetNetConnection();
            UE_LOG(LogTemp, Warning, TEXT("PROOF_INGRESS_LIMIT owner=%d action=%s"),
                OwnerId, Connection ? TEXT("close-connection") : TEXT("latch-local-controller"));
            // A receipt queued immediately before Close is not guaranteed to
            // arrive. Disconnect is the explicit boundary for sequence reset.
            if (Connection)
            {
                Connection->Close();
                if (bIngressFixture)
                    UE_LOG(LogTemp, Display, TEXT("PROOF_INGRESS_CLOSED owner=%d closed=%d admitted=%d elapsed=%.6f"),
                        OwnerId, Connection->GetConnectionState() == USOCK_Closed, IngressAdmitted, Now - IngressFirstSeconds);
            }
        }
        return false;
    }
    if (bIngressFixture)
    {
        if (IngressAdmitted == 0) IngressFirstSeconds = Now;
        if (IngressAdmitted < MAX_int32) ++IngressAdmitted;
    }
    return true;
}

void AProofPlayerController::ServerSubmitCommand_Implementation(FProofCommand Command)
{
    if (!AdmitIngress()) return;
    if (AProofGameMode* Mode = GetWorld()->GetAuthGameMode<AProofGameMode>())
    {
        // OwnerId is assigned by PostLogin on the server, never supplied by the RPC.
        ClientCommandResult(Mode->SubmitForController(this, Command));
    }
}

void AProofPlayerController::ServerRequestRematch_Implementation(int32 ExpectedEpoch)
{
    if (!AdmitIngress()) return;
    AProofGameMode* Mode = GetWorld()->GetAuthGameMode<AProofGameMode>();
    const bool bAccepted = Mode && Mode->RequestRematch(this, ExpectedEpoch);
    UE_LOG(LogTemp, Display, TEXT("PROOF_REMATCH_REQUEST owner=%d epoch=%d accepted=%d"), OwnerId, ExpectedEpoch, bAccepted ? 1 : 0);
}

void AProofPlayerController::ClientLifecycleRejected_Implementation()
{
    if (LifecycleRole == TEXT("third"))
    {
        UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_CLIENT_REJECT reason=full"));
        UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_CLIENT_COMPLETE role=third status=pass"));
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
}

void AProofPlayerController::ClientCommandResult_Implementation(FProofCommandResult Result)
{
    SynchronizeMatchEpoch();
    if (LocalMatchEpoch > 0 && Result.MatchEpoch != LocalMatchEpoch)
    {
        UE_LOG(LogTemp, Display, TEXT("PROOF_STALE_RECEIPT_IGNORED owner=%d epoch=%d current=%d sequence=%d"), OwnerId, Result.MatchEpoch, LocalMatchEpoch, Result.Sequence);
        return;
    }
    LastReceiptSequence = Result.Sequence;
    ReceiptStatus = FString::Printf(TEXT("Host #%d: %s (%s)"), Result.Sequence,
        Result.bAccepted ? TEXT("ACCEPTED") : TEXT("REJECTED"), *Result.Reason);
    // Bounded diagnostic history; no command or snapshot changes follow a receipt.
    Receipts.Add(Result.Sequence, Result);
    if (Receipts.Num() > 64)
    {
        int32 Oldest = MAX_int32;
        for (const auto& Entry : Receipts) Oldest = FMath::Min(Oldest, Entry.Key);
        Receipts.Remove(Oldest);
    }
    UE_LOG(LogTemp, Display, TEXT("PROOF_RESULT owner=%d sequence=%lld accepted=%d reason=%s"),
        OwnerId, static_cast<long long>(Result.Sequence), Result.bAccepted ? 1 : 0, *Result.Reason);
    if (bLifecycleFixture) UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_CLIENT_RESULT owner=%d epoch=%d sequence=%d accepted=%d reason=%s"),
        OwnerId, Result.MatchEpoch, Result.Sequence, Result.bAccepted ? 1 : 0, *Result.Reason);
    if (bSkirmishObserver) return;
    if (!bAutomation) return;
    const int32 Sequence = static_cast<int32>(Result.Sequence);
    if (Sequence < 1 || Sequence > 4 || Acknowledged.Contains(Sequence) || Result.bAccepted != (Sequence != 4)
        || (Sequence == 4 && Result.Reason != TEXT("ownership")))
        bAckFailure = true;
    Acknowledged.Add(Sequence);
}

void AProofPlayerController::FinishAutomation(bool bPass)
{
    if (bFinished) return;
    bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("PROOF_AUTOMATION_COMPLETE role=client owner=%d status=%s ack=%d moved=%d damaged=%d produced=%d"),
        OwnerId, bPass ? TEXT("pass") : TEXT("fail"), Acknowledged.Num() == 4 && !bAckFailure,
        bMoved, bDamaged, bProduced);
    FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
}

void AProofPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsLocalController()) return;
    Lifetime += DeltaSeconds;
    AProofGameState* State = GetWorld()->GetGameState<AProofGameState>();
    SynchronizeMatchEpoch();
    if (State && OwnerId != INDEX_NONE) RefineryEvidence.LogSnapshot(State->Snapshot, TEXT("client"), OwnerId);
    if (State && OwnerId != INDEX_NONE && LastLoggedRevision != State->Snapshot.Revision)
    {
        State->LogSnapshot(TEXT("client"), OwnerId);
        LastLoggedRevision = State->Snapshot.Revision;
    }
    if (bLifecycleFixture) { TickLifecycleFixture(State); return; }
    if (bSkirmishObserver) { TickSkirmishObserver(State); return; }
    if (bControlsObserver && !bFinished)
    {
        if (State && State->Snapshot.Players.Num() >= 3 && OwnerId != INDEX_NONE && ControlsObserverReady < 0.f)
        {
            ControlsObserverReady = Lifetime;
        }
        if (State && ControlsObserverReady >= 0.f && Lifetime - ControlsObserverReady > 23.f)
        {
            bFinished = true;
            UE_LOG(LogTemp, Display, TEXT("PROOF_CONTROLS_COMPLETE role=observer owner=%d status=pass revision=%d"), OwnerId, State->Snapshot.Revision);
            FPlatformMisc::RequestExitWithStatus(false, 0);
        }
        if (Lifetime > 60.f && !bFinished)
        {
            bFinished = true;
            UE_LOG(LogTemp, Error, TEXT("PROOF_CONTROLS_COMPLETE role=observer owner=%d status=fail reason=timeout"), OwnerId);
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
    }
    if (bIngressFlood || bIngressHealthy) { TickIngressFixture(State); return; }
    if (!bAutomation || bFinished) return;
    if (Lifetime > 40.f) { FinishAutomation(false); return; }
    if (!State || OwnerId == INDEX_NONE) return;
    const FProofSnapshot& Snapshot = State->Snapshot;
    if (ReadyTime < 0.f)
    {
        bool bOwner1 = false, bOwner2 = false, bBot = false;
        for (const FProofPlayerRecord& PlayerRecord : Snapshot.Players)
        {
            bOwner1 |= PlayerRecord.OwnerId == 1;
            bOwner2 |= PlayerRecord.OwnerId == 2;
            bBot |= ProofOwnerIds::IsReserved(PlayerRecord.OwnerId);
        }
        if (!bOwner1 || !bOwner2 || !bBot) return;
        for (const FProofUnitRecord& Unit : Snapshot.Units)
        {
            if (Unit.OwnerId == OwnerId && OwnUnit == INDEX_NONE)
            { OwnUnit = Unit.UnitId; InitialPosition = Unit.Position; }
            if (Unit.OwnerId != OwnerId && !ProofOwnerIds::IsReserved(Unit.OwnerId) && EnemyUnit == INDEX_NONE)
                EnemyUnit = Unit.UnitId;
        }
        if (OwnUnit == INDEX_NONE || EnemyUnit == INDEX_NONE) return;
        ReadyTime = Lifetime;
    }
    int32 OwnCount = 0;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.OwnerId == OwnerId) ++OwnCount;
        if (Unit.UnitId == OwnUnit && FVector::Dist(Unit.Position, InitialPosition) > 20.f) bMoved = true;
        if (Unit.HitPoints < 100.f) bDamaged = true;
    }
    bProduced |= OwnCount >= 2;
    const float Elapsed = Lifetime - ReadyTime;
    const float SendTimes[] = { 0.2f, 4.f, 4.5f, 5.f };
    if (NextStep < 4 && Elapsed >= SendTimes[NextStep])
    {
        FProofCommand Command;
        Command.Sequence = NextLocalCommandSequence();
        Command.UnitId = OwnUnit;
        Command.Type = EProofCommandType::Move;
        Command.Destination = FVector(OwnerId == 1 ? -100.f : 100.f, 0.f, 0.f);
        if (NextStep == 1) { Command.Type = EProofCommandType::Attack; Command.TargetUnitId = EnemyUnit; }
        if (NextStep == 2) Command.Type = EProofCommandType::Produce;
        if (NextStep == 3) Command.UnitId = EnemyUnit;
        ServerSubmitCommand(Command);
        ++NextStep;
    }
    if (Elapsed >= 12.f)
        FinishAutomation(!bAckFailure && Acknowledged.Num() == 4 && bMoved && bDamaged && bProduced);
}

void AProofPlayerController::FinishSkirmishObserver(bool bPass, const TCHAR* Reason, int32 WinnerTeam)
{
    if (bFinished) return;
    bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_COMPLETE role=observer owner=%d status=%s scenario=Coop winnerTeam=%d reason=%s refinery=%d factory=%d gathered=%d produced=%d step=%d"),
        OwnerId, bPass ? TEXT("pass") : TEXT("fail"), WinnerTeam, Reason,
        bSkirmishObserverRefinery, bSkirmishObserverFactory, bSkirmishObserverGathered,
        bSkirmishObserverProduced, SkirmishObserverStep);
    FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
}

void AProofPlayerController::TickSkirmishObserver(AProofGameState* State)
{
    if (bFinished) return;
    if (Lifetime >= 230.f)
    {
        FinishSkirmishObserver(false, TEXT("timeout"), State ? State->Snapshot.WinnerTeam : INDEX_NONE);
        return;
    }
    if (OwnerId != INDEX_NONE && OwnerId != 2)
    {
        FinishSkirmishObserver(false, TEXT("wrong-owner"), State ? State->Snapshot.WinnerTeam : INDEX_NONE);
        return;
    }
    if (!State || OwnerId != 2) return;
    const FProofSnapshot& Snapshot = State->Snapshot;
    if (!Snapshot.bSkirmish) return;
    if (Snapshot.MatchPhase == EProofMatchPhase::Finished && SkirmishObserverTerminalAt < 0.f)
        SkirmishObserverTerminalAt = Lifetime;

    const FProofPlayerRecord* PlayerRecord = Snapshot.Players.FindByPredicate(
        [this](const FProofPlayerRecord& P) { return P.OwnerId == OwnerId && P.TeamId == 0; });
    const FProofUnitRecord* HQ = nullptr;
    const FProofUnitRecord* Harvester = nullptr;
    const FProofUnitRecord* Factory = nullptr;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.OwnerId != OwnerId) continue;
        if (Unit.Kind == EProofUnitKind::HQ && Unit.HitPoints > 0.f) HQ = &Unit;
        if (Unit.Kind == EProofUnitKind::Harvester && Unit.HitPoints > 0.f) Harvester = &Unit;
        if (Unit.Kind == EProofUnitKind::Refinery) bSkirmishObserverRefinery = true;
        if (Unit.Kind == EProofUnitKind::Factory)
        {
            bSkirmishObserverFactory = true;
            if (Unit.HitPoints > 0.f) Factory = &Unit;
        }
        if (Unit.Kind == EProofUnitKind::Combat) bSkirmishObserverProduced = true;
    }
    if (PlayerRecord && PlayerRecord->TotalGathered > 0) bSkirmishObserverGathered = true;
    if (!bSkirmishObserverReady)
    {
        const bool bOwner1 = Snapshot.Players.ContainsByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 1 && P.TeamId == 0; });
        const bool bBot = Snapshot.Players.ContainsByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == ProofOwnerIds::Bot && P.TeamId == 1; });
        bSkirmishObserverReady = PlayerRecord && HQ && Harvester && bOwner1 && bBot;
        if (!bSkirmishObserverReady)
        {
            if (SkirmishObserverTerminalAt >= 0.f && Lifetime - SkirmishObserverTerminalAt >= 2.f)
                FinishSkirmishObserver(false, TEXT("missing-start-state"), Snapshot.WinnerTeam);
            return;
        }
        UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_OBSERVER_READY owner=2 revision=%d"), Snapshot.Revision);
    }

    if (SkirmishObserverPendingSequence != INDEX_NONE)
    {
        FProofCommandResult Receipt;
        if (!FindReceipt(SkirmishObserverPendingSequence, Receipt))
        {
            if (SkirmishObserverTerminalAt >= 0.f && Lifetime - SkirmishObserverTerminalAt >= 2.f)
                FinishSkirmishObserver(false, TEXT("missing-terminal-receipt"), Snapshot.WinnerTeam);
            return;
        }
        if (!Receipt.bAccepted)
        {
            FinishSkirmishObserver(false, TEXT("rejected-command"), Snapshot.WinnerTeam);
            return;
        }
        const bool bStateObserved = (SkirmishObserverStep == 0 && bSkirmishObserverRefinery)
            || (SkirmishObserverStep == 1 && bSkirmishObserverFactory)
            || (SkirmishObserverStep == 2 && bSkirmishObserverGathered)
            || (SkirmishObserverStep == 3 && bSkirmishObserverProduced);
        if (!bStateObserved)
        {
            if (SkirmishObserverTerminalAt >= 0.f && Lifetime - SkirmishObserverTerminalAt >= 2.f)
                FinishSkirmishObserver(false, TEXT("missing-terminal-state"), Snapshot.WinnerTeam);
            return;
        }
        UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_OBSERVER_STEP owner=2 step=%d sequence=%d revision=%d"),
            SkirmishObserverStep, SkirmishObserverPendingSequence, Snapshot.Revision);
        SkirmishObserverPendingSequence = INDEX_NONE;
        ++SkirmishObserverStep;
    }

    if (Snapshot.MatchPhase == EProofMatchPhase::Finished)
    {
        const bool bPass = Snapshot.WinnerTeam == 0 && SkirmishObserverStep == 4
            && bSkirmishObserverRefinery && bSkirmishObserverFactory
            && bSkirmishObserverGathered && bSkirmishObserverProduced;
        FinishSkirmishObserver(bPass, bPass ? TEXT("complete") : TEXT("terminal-evidence"), Snapshot.WinnerTeam);
        return;
    }
    if (SkirmishObserverStep >= 4) return;

    FProofCommand Command;
    if (SkirmishObserverStep == 0)
    {
        if (!HQ) return;
        Command.Type = EProofCommandType::BuildRefinery;
        Command.Destination = HQ->Position + FVector(350.f, 0.f, 0.f);
    }
    else if (SkirmishObserverStep == 1)
    {
        if (!HQ) return;
        Command.Type = EProofCommandType::BuildFactory;
        Command.Destination = HQ->Position + FVector(0.f, -350.f, 0.f);
    }
    else if (SkirmishObserverStep == 2)
    {
        if (!Harvester) return;
        const FProofResourceRecord* NearestOre = nullptr;
        for (const FProofResourceRecord& Ore : Snapshot.Resources)
        {
            if (Ore.Remaining <= 0) continue;
            if (!NearestOre || FVector::DistSquared(Harvester->Position, Ore.Position) < FVector::DistSquared(Harvester->Position, NearestOre->Position))
                NearestOre = &Ore;
        }
        if (!NearestOre) return;
        Command.Type = EProofCommandType::Gather;
        Command.UnitId = Harvester->UnitId;
        Command.TargetUnitId = NearestOre->ResourceId;
    }
    else
    {
        if (!Factory) return;
        Command.Type = EProofCommandType::Produce;
        Command.UnitId = Factory->UnitId;
    }
    Command.Sequence = NextLocalCommandSequence();
    Command.MatchEpoch = Snapshot.MatchEpoch;
    SkirmishObserverPendingSequence = Command.Sequence;
    ServerSubmitCommand(Command);
    UE_LOG(LogTemp, Display, TEXT("PROOF_SKIRMISH_OBSERVER_SEND owner=2 step=%d sequence=%d revision=%d"),
        SkirmishObserverStep, Command.Sequence, Snapshot.Revision);
}

// The lifecycle runner only creates gate files. Every action here still travels
// through the same controller RPC and command model as an interactive player.
void AProofPlayerController::TickLifecycleFixture(AProofGameState* State)
{
    if (bFinished || LifecycleControl.IsEmpty()) return;
    const auto HasGate = [this](const FString& Name)
    {
        return IFileManager::Get().FileExists(*FPaths::Combine(LifecycleControl, Name));
    };
    if (HasGate(TEXT("finish")))
    {
        bFinished = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_CLIENT_COMPLETE role=%s status=pass"), *LifecycleRole);
        FPlatformMisc::RequestExitWithStatus(false, 0);
        return;
    }
    if (!State || OwnerId == INDEX_NONE || State->Snapshot.MatchEpoch <= 0) return;
    const FProofSnapshot& Snapshot = State->Snapshot;
    const int32 Epoch = Snapshot.MatchEpoch;
    if (LifecycleRole == TEXT("lead") && Epoch > 1 && !bLifecycleStaleSent &&
        HasGate(FString::Printf(TEXT("stale-epoch-%d"), Epoch - 1)))
    {
        FProofCommand Stale;
        Stale.Sequence = 1;
        Stale.MatchEpoch = Epoch - 1;
        ServerSubmitCommand(Stale);
        bLifecycleStaleSent = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_STALE_SENT owner=%d epoch=%d"), OwnerId, Stale.MatchEpoch);
    }
    if (LifecycleRole == TEXT("lead") && Snapshot.MatchPhase == EProofMatchPhase::Finished &&
        !bLifecycleRematchSent && HasGate(FString::Printf(TEXT("rematch-epoch-%d"), Epoch)))
    {
        bLifecycleRematchSent = true;
        ServerRequestRematch(Epoch);
        UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_REMATCH_SENT owner=%d epoch=%d"), OwnerId, Epoch);
    }
    if (Snapshot.MatchPhase != EProofMatchPhase::Playing ||
        !HasGate(FString::Printf(TEXT("play-epoch-%d"), Epoch))) return;
    const FProofPlayerRecord* PlayerRecord = Snapshot.Players.FindByPredicate(
        [this](const FProofPlayerRecord& P) { return P.OwnerId == OwnerId; });
    if (!PlayerRecord) return; // a rejoin waits for the next match; no old assets are restored.
    const FProofUnitRecord *HQ = nullptr, *Harvester = nullptr, *Factory = nullptr, *Combat = nullptr, *BotHQ = nullptr;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.OwnerId == OwnerId && Unit.HitPoints > 0.f)
        {
            if (Unit.Kind == EProofUnitKind::HQ) HQ = &Unit;
            if (Unit.Kind == EProofUnitKind::Harvester) Harvester = &Unit;
            if (Unit.Kind == EProofUnitKind::Factory) Factory = &Unit;
            if (Unit.Kind == EProofUnitKind::Combat) Combat = &Unit;
        }
        if (Unit.OwnerId == ProofOwnerIds::Bot && Unit.Kind == EProofUnitKind::HQ && Unit.HitPoints > 0.f) BotHQ = &Unit;
    }
    if (LifecyclePendingSequence != INDEX_NONE)
    {
        FProofCommandResult Result;
        if (!FindReceipt(LifecyclePendingSequence, Result)) return;
        if (!Result.bAccepted)
        {
            bFinished = true;
            UE_LOG(LogTemp, Error, TEXT("PROOF_LIFECYCLE_CLIENT_COMPLETE role=%s status=fail reason=%s step=%d"),
                *LifecycleRole, *Result.Reason, LifecycleStep);
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        LifecyclePendingSequence = INDEX_NONE;
        ++LifecycleStep;
    }
    if (LifecycleStep >= 5 || !HQ) return;
    FProofCommand Command;
    Command.MatchEpoch = Epoch;
    if (LifecycleStep == 0)
    {
        Command.Type = EProofCommandType::BuildRefinery;
        Command.Destination = HQ->Position + FVector(350.f, 0.f, 0.f);
    }
    else if (LifecycleStep == 1)
    {
        Command.Type = EProofCommandType::BuildFactory;
        Command.Destination = HQ->Position + FVector(0.f, -350.f, 0.f);
    }
    else if (LifecycleStep == 2)
    {
        if (!Harvester) return;
        const FProofResourceRecord* Ore = Snapshot.Resources.FindByPredicate(
            [this](const FProofResourceRecord& R) { return R.OwnerId == OwnerId && R.Remaining > 0; });
        if (!Ore) return;
        Command.Type = EProofCommandType::Gather;
        Command.UnitId = Harvester->UnitId;
        Command.TargetUnitId = Ore->ResourceId;
    }
    else if (LifecycleStep == 3)
    {
        if (!Factory) return;
        Command.Type = EProofCommandType::Produce;
        Command.UnitId = Factory->UnitId;
    }
    else
    {
        if (!Combat || !BotHQ) return;
        Command.Type = EProofCommandType::Attack;
        Command.UnitId = Combat->UnitId;
        Command.TargetUnitId = BotHQ->UnitId;
    }
    Command.Sequence = NextLocalCommandSequence();
    LifecyclePendingSequence = Command.Sequence;
    ServerSubmitCommand(Command);
    UE_LOG(LogTemp, Display, TEXT("PROOF_LIFECYCLE_CLIENT_SEND owner=%d epoch=%d sequence=%d step=%d"),
        OwnerId, Epoch, Command.Sequence, LifecycleStep);
}

// Explicit loopback fixture: these flags are never enabled by normal play.
void AProofPlayerController::TickIngressFixture(AProofGameState* State)
{
    if (bFinished) return;
    if (!bIngressFailureBound && GEngine)
    {
        GEngine->OnNetworkFailure().AddWeakLambda(this,
            [this](UWorld*, UNetDriver*, ENetworkFailure::Type Type, const FString&)
            {
                const bool bExpected = bIngressFlood && Type == ENetworkFailure::ConnectionLost;
                UE_LOG(LogTemp, Display, TEXT("PROOF_INGRESS_CLIENT_CLOSED expected=%d type=%d"), bExpected, static_cast<int32>(Type));
                bFinished = true;
                FPlatformMisc::RequestExitWithStatus(false, bExpected ? 0 : 1);
            });
        bIngressFailureBound = true;
    }
    if (!State || OwnerId == INDEX_NONE) return;
    if (!bIngressSent)
    {
        if (bIngressFlood)
        {
            // Under the reliable bunch buffer; every request still uses the real RPC.
            for (int32 Index = 0; Index < 128; ++Index)
            {
                FProofCommand Bad;
                Bad.Sequence = Index % 2 == 0 ? 0 : 99;
                ServerSubmitCommand(Bad);
            }
            UE_LOG(LogTemp, Display, TEXT("PROOF_INGRESS_FLOOD_SENT owner=%d requests=128"), OwnerId);
        }
        else
        {
            for (const FProofUnitRecord& Unit : State->Snapshot.Units)
                if (Unit.OwnerId == OwnerId) { OwnUnit = Unit.UnitId; InitialPosition = Unit.Position; break; }
            if (OwnUnit == INDEX_NONE) return;
            FProofCommand Move;
            Move.Sequence = NextLocalCommandSequence();
            Move.UnitId = OwnUnit;
            Move.Destination = InitialPosition + FVector(100.f, 0.f, 0.f);
            ServerSubmitCommand(Move);
        }
        bIngressSent = true;
    }
    if (bIngressHealthy && !bIngressSecondSent && !IngressRelease.IsEmpty() && IFileManager::Get().FileExists(*IngressRelease))
    {
        FProofCommand Produce;
        Produce.Sequence = NextLocalCommandSequence();
        Produce.Type = EProofCommandType::Produce;
        ServerSubmitCommand(Produce);
        bIngressSecondSent = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_INGRESS_HEALTHY_RELEASED owner=%d"), OwnerId);
    }
    if (bIngressHealthy && bIngressSecondSent)
    {
        int32 Count = 0;
        bool bMovedUnit = false;
        for (const FProofUnitRecord& Unit : State->Snapshot.Units)
        {
            if (Unit.OwnerId == OwnerId) ++Count;
            if (Unit.UnitId == OwnUnit && FVector::Dist(Unit.Position, InitialPosition) >= 90.f) bMovedUnit = true;
        }
        const FProofPlayerRecord* PlayerRecord = State->Snapshot.Players.FindByPredicate(
            [this](const FProofPlayerRecord& Record) { return Record.OwnerId == OwnerId; });
        if (Count == 2 && bMovedUnit && PlayerRecord && PlayerRecord->Credits == 150)
        {
            bFinished = true;
            UE_LOG(LogTemp, Display, TEXT("PROOF_INGRESS_HEALTHY_COMPLETE owner=%d status=pass moved=1 produced=1 credits=150"), OwnerId);
            FPlatformMisc::RequestExitWithStatus(false, 0);
        }
    }
}
