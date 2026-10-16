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

AProofPlayerController::AProofPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bAutomation = FParse::Param(FCommandLine::Get(), TEXT("ProofAutomation"));
    bControlsAutomation = FParse::Param(FCommandLine::Get(), TEXT("ProofControlsAutomation"));
    bControlsObserver = FParse::Param(FCommandLine::Get(), TEXT("ProofControlsObserver"));
    bIngressFixture = FParse::Param(FCommandLine::Get(), TEXT("ProofIngressFixture"));
    bIngressFlood = FParse::Param(FCommandLine::Get(), TEXT("ProofIngressFlood"));
    bIngressHealthy = FParse::Param(FCommandLine::Get(), TEXT("ProofIngressHealthy"));
    FParse::Value(FCommandLine::Get(), TEXT("ProofIngressRelease="), IngressRelease);
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

void AProofPlayerController::ServerSubmitCommand_Implementation(FProofCommand Command)
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
        return;
    }
    if (bIngressFixture)
    {
        if (IngressAdmitted == 0) IngressFirstSeconds = Now;
        if (IngressAdmitted < MAX_int32) ++IngressAdmitted;
    }
    if (AProofGameMode* Mode = GetWorld()->GetAuthGameMode<AProofGameMode>())
    {
        // OwnerId is assigned by PostLogin on the server, never supplied by the RPC.
        ClientCommandResult(Mode->SubmitForOwner(OwnerId, Command));
    }
}

void AProofPlayerController::ClientCommandResult_Implementation(FProofCommandResult Result)
{
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
    if (State && OwnerId != INDEX_NONE && LastLoggedRevision != State->Snapshot.Revision)
    {
        State->LogSnapshot(TEXT("client"), OwnerId);
        LastLoggedRevision = State->Snapshot.Revision;
    }
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
        const FProofPlayerRecord* Player = State->Snapshot.Players.FindByPredicate(
            [this](const FProofPlayerRecord& Record) { return Record.OwnerId == OwnerId; });
        if (Count == 2 && bMovedUnit && Player && Player->Credits == 150)
        {
            bFinished = true;
            UE_LOG(LogTemp, Display, TEXT("PROOF_INGRESS_HEALTHY_COMPLETE owner=%d status=pass moved=1 produced=1 credits=150"), OwnerId);
            FPlatformMisc::RequestExitWithStatus(false, 0);
        }
    }
}