#include "Runtime/ProofAreaGuardRuntimeState.h"
#include "Runtime/ProofGameMode.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
void AGFail(const TCHAR* Role, const TCHAR* Reason)
{
    UE_LOG(LogTemp, Error, TEXT("PROOF_AREA_GUARD_FIXTURE_ERROR role=%s reason=%s"), Role, Reason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
const FProofUnitRecord* AGUnit(const FProofSnapshot& State, int32 Id)
{ return State.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; }); }
const FProofUnitRecord* AGKind(const FProofSnapshot& State, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* Best = nullptr;
    for (const auto& Unit : State.Units)
        if (Unit.OwnerId == Owner && Unit.Kind == Kind && Unit.HitPoints > 0.f && (!Best || Unit.UnitId < Best->UnitId)) Best = &Unit;
    return Best;
}
bool AGRoster(const FProofSnapshot& State)
{
    const auto* A = State.Players.FindByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 1 && P.TeamId == 0; });
    const auto* B = State.Players.FindByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 2 && P.TeamId == 0; });
    const auto* Bot = State.Players.FindByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 100 && P.TeamId == 1; });
    return State.Players.Num() == 3 && A && B && Bot && State.MatchPhase == EProofMatchPhase::Playing;
}
bool AGConfig(FProofAreaGuardRuntimeState& R, const TCHAR* Role)
{
    if (R.bConfigured) return true;
    const TCHAR* Cmd = FCommandLine::Get();
    if (!FParse::Param(Cmd, TEXT("ProofSkirmish")) || !FParse::Param(Cmd, TEXT("ProofAreaGuardObserve"))
        || !FParse::Value(Cmd, TEXT("ProofAreaGuardRunDirectory="), R.RunDirectory)
        || !FParse::Value(Cmd, TEXT("ProofAreaGuardRun="), R.RunToken)
        || R.RunToken.Len() != 32 || FPaths::IsRelative(R.RunDirectory))
    { AGFail(Role, TEXT("configuration")); return false; }
    for (TCHAR C : R.RunToken) if (!FChar::IsHexDigit(C)) { AGFail(Role, TEXT("run-token")); return false; }
    FString Normalized = R.RunDirectory.Replace(TEXT("/"), TEXT("\\"));
    if (!Normalized.StartsWith(TEXT("D:\\"), ESearchCase::IgnoreCase)
        || !IFileManager::Get().DirectoryExists(*R.RunDirectory))
    { AGFail(Role, TEXT("run-directory")); return false; }
    R.bConfigured = true;
    UE_LOG(LogTemp, Display, TEXT("PROOF_AREA_GUARD_FIXTURE_CONFIG role=%s activeSeconds=55 scan=600 leash=300 speed=150 input=synthetic physicalInput=unverified"), Role);
    return true;
}
FString AGMarkerPath(const FProofAreaGuardRuntimeState& R, int32 Owner)
{ return FPaths::Combine(R.RunDirectory, Owner == 1 ? TEXT("rendered.ready") : TEXT("observer.ready")); }
bool AGMarkerMatches(const FProofAreaGuardRuntimeState& R, int32 Owner)
{
    const FString Path = AGMarkerPath(R, Owner);
    const int64 Size = IFileManager::Get().FileSize(*Path);
    FString Payload;
    return Size > 0 && Size < 64 && FFileHelper::LoadFileToString(Payload, *Path)
        && Payload == FString::Printf(TEXT("owner%d:%s"), Owner, *R.RunToken);
}
bool AGWriteMarker(FProofAreaGuardRuntimeState& R, int32 Owner)
{
    const FString Path = AGMarkerPath(R, Owner);
    if (IFileManager::Get().FileExists(*Path)) return false;
    const FString Payload = FString::Printf(TEXT("owner%d:%s"), Owner, *R.RunToken);
    return FFileHelper::SaveStringToFile(Payload, *Path) && AGMarkerMatches(R, Owner);
}
FProofCommand AGCommand(EProofCommandType Type, int32 Unit = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand Command;
    Command.Type = Type;
    Command.UnitId = Unit;
    Command.Destination = Destination;
    return Command;
}
}

void AProofGameMode::TickAreaGuardRuntime(float DeltaSeconds)
{
    auto& R = AreaGuardRuntime;
    if (!AGConfig(R, TEXT("host"))) return;
    R.Elapsed += DeltaSeconds;
    if (R.Elapsed > 55.f) { AGFail(TEXT("host"), TEXT("timeout")); return; }
    if (R.bReady)
    {
        if (AGMarkerMatches(R, 1) && AGMarkerMatches(R, 2))
        {
            UE_LOG(LogTemp, Display, TEXT("PROOF_AREA_GUARD_FIXTURE_COMPLETE role=host status=pass unit=%d target=%d input=synthetic physicalInput=unverified"), R.GuardId, R.TargetId);
            FPlatformMisc::RequestExitWithStatus(false, 0);
        }
        return;
    }
    FProofSnapshot Snapshot = Model.GetSnapshot();
    if (!AGRoster(Snapshot))
    {
        R.Accumulator = 0.f;
        if (R.bStarted) AGFail(TEXT("host"), TEXT("roster-lost"));
        return;
    }
    if (!R.bStarted)
    {
        const auto* Target = AGKind(Snapshot, 100, EProofUnitKind::Harvester);
        if (!Target) { AGFail(TEXT("host"), TEXT("missing-target")); return; }
        R.TargetId = Target->UnitId;
        R.bStarted = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_AREA_GUARD_FIXTURE_START owners=1,2,100 target=%d"), R.TargetId);
    }
    const auto SubmitBot = [this, &R](FProofCommand Command)
    {
        Command.Sequence = ++R.BotSequence;
        Command.MatchEpoch = MatchEpoch;
        const auto Result = SubmitForOwner(100, Command);
        if (!Result.bAccepted) { AGFail(TEXT("host"), TEXT("bot-move-rejected")); return false; }
        return true;
    };
    if (!R.bTargetMoveSent)
    {
        const auto* Own = AGKind(Snapshot, 1, EProofUnitKind::Combat);
        if (Own)
        {
            R.GuardId = Own->UnitId;
            R.Post = Own->Position;
            R.TargetPost = R.Post + FVector(2.f * FProofCommandModel::AttackRange, 0.f, 0.f);
            if (FMath::Abs(R.TargetPost.X) > FProofCommandModel::WorldExtent
                || !SubmitBot(AGCommand(EProofCommandType::Move, R.TargetId, R.TargetPost))) return;
            R.bTargetMoveSent = true;
            UE_LOG(LogTemp, Display, TEXT("PROOF_AREA_GUARD_FIXTURE_TARGET_MOVE target=%d postX=%.17g postY=%.17g targetX=%.17g targetY=%.17g"),
                R.TargetId, R.Post.X, R.Post.Y, R.TargetPost.X, R.TargetPost.Y);
        }
    }
    R.Accumulator += DeltaSeconds;
    int32 Steps = 0;
    while (R.Accumulator >= 0.1f && Steps++ < 10)
    {
        const FProofSnapshot Before = Model.GetSnapshot();
        if (Before.MatchPhase != EProofMatchPhase::Playing) { AGFail(TEXT("host"), TEXT("terminal")); return; }
        Model.Advance(0.1f);
        R.SimulatedSeconds += static_cast<double>(0.1f);
        const FProofSnapshot After = Model.GetSnapshot();
        AreaGuardObservation.ObserveAdvance(MatchEpoch, R.SimulatedSeconds, 0.1f, Before, After);
        R.Accumulator -= 0.1f;
        if (++R.PublishSteps >= 2) { Publish(); R.PublishSteps = 0; }
        const auto* Guard = AGUnit(After, R.GuardId);
        const auto* Target = AGUnit(After, R.TargetId);
        if (R.bTargetMoveSent && Guard && Target && Guard->bAreaGuard && Guard->HitPoints > 0.f)
        {
            if (Guard->AreaGuardPost != R.Post) { AGFail(TEXT("host"), TEXT("post-changed")); return; }
            const double Outbound = FVector::Dist(Guard->Position, R.Post);
            if (Guard->AreaGuardTargetId == R.TargetId) R.bSawAcquisition = true;
            if (R.bSawAcquisition && Guard->AreaGuardTargetId == R.TargetId && Outbound > 20.0) R.bSawChase = true;
            if (!R.bRetreatSent && R.bSawChase && Outbound >= FProofCommandModel::AttackRange - 0.001
                && Target->HitPoints < 100.f && Target->HitPoints > 0.f)
            {
                if (!SubmitBot(AGCommand(EProofCommandType::Move, R.TargetId,
                    R.Post + FVector(3.f * FProofCommandModel::AttackRange, 0.f, 0.f)))) return;
                R.bRetreatSent = true;
                UE_LOG(LogTemp, Display, TEXT("PROOF_AREA_GUARD_FIXTURE_RETREAT target=%d guard=%d outbound=%.17g targetHP=%.9g revision=%d"),
                    R.TargetId, R.GuardId, Outbound, static_cast<double>(Target->HitPoints), After.Revision);
            }
            if (R.bRetreatSent && Guard->bAreaGuardReturning && Guard->AreaGuardTargetId == INDEX_NONE
                && FVector::Dist(Target->Position, R.Post) > 2.f * FProofCommandModel::AttackRange)
                R.bSawReturn = true;
            if (R.bSawReturn && Guard->bAreaGuardReturning && Guard->Position == R.Post)
            {
                R.bReady = true;
                Publish(); // Freeze the exact returning-at-post state for both receivers.
                UE_LOG(LogTemp, Display, TEXT("PROOF_AREA_GUARD_FIXTURE_READY role=host status=pass unit=%d target=%d epoch=%d revision=%d input=synthetic physicalInput=unverified"),
                    R.GuardId, R.TargetId, MatchEpoch, Model.GetSnapshot().Revision);
                return;
            }
        }
    }
}

void AProofPlayerController::TickAreaGuardRuntime(float DeltaSeconds)
{
    auto& R = AreaGuardRuntime;
    const TCHAR* Role = OwnerId == 1 ? TEXT("rendered") : TEXT("observer");
    if (!AGConfig(R, Role)) return;
    R.Elapsed += DeltaSeconds;
    if (R.Elapsed > 55.f) { AGFail(Role, TEXT("timeout")); return; }
    if (OwnerId == INDEX_NONE) return;
    if (OwnerId != 1 && OwnerId != 2) { AGFail(Role, TEXT("owner")); return; }
    const auto* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!GS || !AGRoster(GS->Snapshot)) return;
    const FProofSnapshot& Snapshot = GS->Snapshot;
    if (OwnerId == 1)
    {
        if (R.PendingSequence != INDEX_NONE)
        {
            FProofCommandResult Receipt;
            if (!FindReceipt(R.PendingSequence, Receipt)) return;
            if (!Receipt.bAccepted || Receipt.MatchEpoch != Snapshot.MatchEpoch)
            { AGFail(Role, TEXT("command-rejected")); return; }
            R.PendingSequence = INDEX_NONE;
        }
        auto Send = [this, &R, &Snapshot](FProofCommand Command)
        {
            Command.Sequence = NextLocalCommandSequence();
            Command.MatchEpoch = Snapshot.MatchEpoch;
            R.PendingSequence = Command.Sequence;
            UE_LOG(LogTemp, Display, TEXT("PROOF_AREA_GUARD_FIXTURE_SEND owner=1 epoch=%d sequence=%d type=%d unit=%d target=%d"),
                Command.MatchEpoch, Command.Sequence, static_cast<int32>(Command.Type), Command.UnitId, Command.TargetUnitId);
            ServerSubmitCommand(Command);
        };
        if (R.Stage == 0)
        {
            const auto* HQ = AGKind(Snapshot, 1, EProofUnitKind::HQ);
            if (!HQ) return;
            Send(AGCommand(EProofCommandType::BuildRefinery, INDEX_NONE, HQ->Position + FVector(350.f, 0.f, 0.f)));
            R.Stage = 1;
        }
        else if (R.Stage == 1)
        {
            const auto* HQ = AGKind(Snapshot, 1, EProofUnitKind::HQ);
            const auto* Refinery = AGKind(Snapshot, 1, EProofUnitKind::Refinery);
            if (!HQ || !Refinery) return;
            Send(AGCommand(EProofCommandType::BuildFactory, INDEX_NONE, HQ->Position + FVector(500.f, -300.f, 0.f)));
            R.Stage = 2;
        }
        else if (R.Stage == 2)
        {
            const auto* Factory = AGKind(Snapshot, 1, EProofUnitKind::Factory);
            if (!Factory) return;
            Send(AGCommand(EProofCommandType::Produce, Factory->UnitId));
            R.Stage = 3;
        }
        else if (R.Stage == 3)
        {
            const auto* Own = AGKind(Snapshot, 1, EProofUnitKind::Combat);
            const auto* Target = AGKind(Snapshot, 100, EProofUnitKind::Harvester);
            if (!Own || !Target || Target->HitPoints != 100.f
                || !Target->Position.Equals(Own->Position + FVector(2.f * FProofCommandModel::AttackRange, 0.f, 0.f), 0.0001)) return;
            R.GuardId = Own->UnitId;
            R.TargetId = Target->UnitId;
            R.Post = Own->Position;
            Send(AGCommand(EProofCommandType::AreaGuard, Own->UnitId));
            R.Stage = 4;
        }
    }
    const FProofUnitRecord* Guard = R.GuardId != INDEX_NONE ? AGUnit(Snapshot, R.GuardId) : nullptr;
    if (OwnerId == 2)
    {
        for (const auto& Unit : Snapshot.Units)
            if (Unit.OwnerId == 1 && Unit.Kind == EProofUnitKind::Combat && Unit.bAreaGuard)
            { Guard = &Unit; R.GuardId = Unit.UnitId; R.Post = Unit.AreaGuardPost; break; }
    }
    if (!Guard || !Guard->bAreaGuard || R.bReady || (OwnerId == 1 && R.Stage < 4)) return;
    if (Guard->AreaGuardPost != R.Post || Guard->HitPoints <= 0.f)
    { AGFail(Role, TEXT("guard-identity")); return; }
    const double Outbound = FVector::Dist(Guard->Position, R.Post);
    if (Guard->AreaGuardTargetId > 0 && !Guard->bAreaGuardReturning) R.bSawAcquisition = true;
    if (R.bSawAcquisition && Guard->AreaGuardTargetId > 0 && Outbound > 20.0) R.bSawChase = true;
    if (R.bSawChase && Guard->bAreaGuardReturning && Guard->AreaGuardTargetId == INDEX_NONE && Outbound > 0.0)
        R.bSawReturn = true;
    if (R.bSawReturn && Guard->bAreaGuardReturning && Guard->AreaGuardTargetId == INDEX_NONE && Guard->Position == R.Post)
    {
        if (!AGWriteMarker(R, OwnerId)) { AGFail(Role, TEXT("ready-marker")); return; }
        R.bReady = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_AREA_GUARD_FIXTURE_READY role=%s status=pass unit=%d epoch=%d revision=%d input=synthetic physicalInput=unverified"),
            Role, R.GuardId, Snapshot.MatchEpoch, Snapshot.Revision);
        UE_LOG(LogTemp, Display, TEXT("PROOF_AREA_GUARD_FIXTURE_COMPLETE role=%s status=pass input=synthetic physicalInput=unverified"), Role);
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
}
