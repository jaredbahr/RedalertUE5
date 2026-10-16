#include "Runtime/ProofPlayerController.h"
#include "Runtime/ProofGameMode.h"
#include "Runtime/ProofGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

AProofPlayerController::AProofPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bAutomation = FParse::Param(FCommandLine::Get(), TEXT("ProofAutomation"));
}

void AProofPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AProofPlayerController, OwnerId);
}

void AProofPlayerController::ServerSubmitCommand_Implementation(FProofCommand Command)
{
    if (AProofGameMode* Mode = GetWorld()->GetAuthGameMode<AProofGameMode>())
    {
        // OwnerId is assigned by PostLogin on the server, never supplied by the RPC.
        ClientCommandResult(Mode->SubmitForOwner(OwnerId, Command));
    }
}

void AProofPlayerController::ClientCommandResult_Implementation(FProofCommandResult Result)
{
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
            bBot |= PlayerRecord.OwnerId == 100;
        }
        if (!bOwner1 || !bOwner2 || !bBot) return;
        for (const FProofUnitRecord& Unit : Snapshot.Units)
        {
            if (Unit.OwnerId == OwnerId && OwnUnit == INDEX_NONE)
            { OwnUnit = Unit.UnitId; InitialPosition = Unit.Position; }
            if (Unit.OwnerId != OwnerId && Unit.OwnerId != 100 && EnemyUnit == INDEX_NONE)
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
