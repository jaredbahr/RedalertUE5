#include "Runtime/ProofAttackMoveRuntimeState.h"
#include "Runtime/ProofAttackMoveReleasePolicy.h"
#include "Runtime/ProofGameMode.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Presentation/ProofRTSPawn.h"
#include "Engine/World.h"
#include "AssetCompilingManager.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"

namespace
{
void AMRuntimeFail(const TCHAR* Role, const TCHAR* Reason)
{
    UE_LOG(LogTemp, Error, TEXT("PROOF_ATTACK_MOVE_ERROR role=%s reason=%s"), Role, Reason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
const FProofUnitRecord* AMRuntimeUnit(const FProofSnapshot& S, int32 Id)
{
    return S.Units.FindByPredicate([Id](const FProofUnitRecord& U) { return U.UnitId == Id; });
}
const FProofUnitRecord* AMRuntimeKind(const FProofSnapshot& S, int32 Owner, EProofUnitKind Kind, int32 Except = INDEX_NONE)
{
    const FProofUnitRecord* Best = nullptr;
    for (const auto& U : S.Units)
        if (U.OwnerId == Owner && U.Kind == Kind && U.UnitId != Except && U.HitPoints > 0.f && (!Best || U.UnitId < Best->UnitId)) Best = &U;
    return Best;
}
const FProofPlayerRecord* AMRuntimePlayer(const FProofSnapshot& S, int32 Owner)
{
    return S.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; });
}
bool AMRuntimeRoster(const FProofSnapshot& S)
{
    const auto* A = AMRuntimePlayer(S, 1); const auto* B = AMRuntimePlayer(S, 2); const auto* Bot = AMRuntimePlayer(S, 100);
    return S.Players.Num() == 3 && A && B && Bot && A->TeamId == 0 && B->TeamId == 0 && Bot->TeamId == 1;
}
bool AMRuntimeConfig(FProofAttackMoveRuntimeState& R, const TCHAR* Role)
{
    if (R.bReleaseConfigured) return true;
    const TCHAR* Cmd = FCommandLine::Get();
    FString OtherScenario;
    if (!FParse::Param(Cmd, TEXT("ProofSkirmish")) || !FParse::Param(Cmd, TEXT("ProofRefineryMinerEvidence"))
        || !FParse::Value(Cmd, TEXT("ProofAttackMoveRelease="), R.Release) || FPaths::IsRelative(R.Release)
        || !FParse::Value(Cmd, TEXT("ProofAttackMoveReleaseToken="), R.ReleaseToken)
        || FParse::Value(Cmd, TEXT("ProofSkirmishScenario="), OtherScenario))
    { AMRuntimeFail(Role, TEXT("fixture_configuration")); return false; }
    for (const TCHAR* Flag : {TEXT("ProofAutomation"), TEXT("ProofControlsAutomation"), TEXT("ProofControlsObserver"),
        TEXT("ProofSkirmishObserver"), TEXT("ProofSkirmishFixtureHost"), TEXT("ProofLifecycleFixtureHost"),
        TEXT("ProofFactoryRallyEvidence"), TEXT("ProofStopControls"), TEXT("ProofTypeSelectionControls"), TEXT("ProofCameraPan"), TEXT("ProofCameraEdgePan")})
        if (FParse::Param(Cmd, Flag)) { AMRuntimeFail(Role, TEXT("incompatible_fixture_flags")); return false; }
    const auto Startup = FProofAttackMoveReleasePolicy::Evaluate(true, FCString::Strcmp(Role, TEXT("host")) == 0, false,
        IFileManager::Get().FileExists(*R.Release), FString(),
        IFileManager::Get().FileExists(*(R.Release + TEXT(".host"))), FString(), R.ReleaseToken);
    if (Startup.Error) { AMRuntimeFail(Role, Startup.Error); return false; }
    R.bReleaseConfigured = true;
    UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_CONFIG role=%s activeSeconds=55 range=300 speed=150 dps=25 commandCap=32 clockCap=1024 input=synthetic physicalInput=unverified"), Role);
    return true;
}
bool AMRuntimeRelease(FProofAttackMoveRuntimeState& R, const TCHAR* Role)
{
    if (R.bReleaseComplete) return true;
    const FString HostPath = R.Release + TEXT(".host");
    const bool bClientsExist = IFileManager::Get().FileExists(*R.Release);
    const bool bHostExists = IFileManager::Get().FileExists(*HostPath);
    FString Clients, Host;
    const auto ReadMarker = [](const FString& Path, FString& Text)
    {
        const int64 Size = IFileManager::Get().FileSize(*Path);
        return Size > 0 && Size <= 64 && FFileHelper::LoadFileToString(Text, *Path);
    };
    if ((bClientsExist && !ReadMarker(R.Release, Clients)) || (bHostExists && !ReadMarker(HostPath, Host)))
    { AMRuntimeFail(Role, TEXT("release_read")); return true; }
    const auto Decision = FProofAttackMoveReleasePolicy::Evaluate(false, FCString::Strcmp(Role, TEXT("host")) == 0,
        R.bReady, bClientsExist, Clients, bHostExists, Host, R.ReleaseToken);
    if (Decision.Error) { AMRuntimeFail(Role, Decision.Error); return true; }
    if (bClientsExist && !R.bReleaseObserved)
    {
        R.bReleaseObserved = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_RELEASE role=%s phase=clients token=%s"), Role, *R.ReleaseToken);
    }
    if (Decision.bComplete)
    {
        R.bReleaseComplete = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_COMPLETE role=%s status=pass input=synthetic physicalInput=unverified"), Role);
        FPlatformMisc::RequestExitWithStatus(false, 0);
    }
    return Decision.bStopTick;
}
int32 AMRuntimeReceipt(AProofPlayerController* PC, int32 Sequence, const TCHAR* Role)
{
    FProofCommandResult Receipt;
    if (!PC->FindReceipt(Sequence, Receipt)) return 0;
    if (!Receipt.bAccepted) { AMRuntimeFail(Role, TEXT("rejected_command")); return -1; }
    return 1;
}
FProofCommand AMRuntimeCommand(EProofCommandType Type, int32 Unit = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand C; C.Type = Type; C.UnitId = Unit; C.Destination = Destination; return C;
}
}

void AProofGameMode::TickAttackMoveRuntime(float DeltaSeconds)
{
    auto& R = AttackMoveRuntime;
    if (!AMRuntimeConfig(R, TEXT("host"))) return;
    R.Elapsed += DeltaSeconds;
    if (R.Elapsed > 55.f) { AMRuntimeFail(TEXT("host"), TEXT("timeout")); return; }
    if (AMRuntimeRelease(R, TEXT("host"))) return;
    FProofSnapshot S = Model.GetSnapshot();
    if (!AMRuntimeRoster(S)) { R.Accumulator = 0.f; if (R.bStarted) AMRuntimeFail(TEXT("host"), TEXT("roster_lost")); return; }
    const auto SubmitBot = [this, &R](FProofCommand C)
    {
        C.Sequence = ++R.Sequence; C.MatchEpoch = MatchEpoch;
        const auto Result = SubmitForOwner(100, C);
        if (!Result.bAccepted) AMRuntimeFail(TEXT("host"), TEXT("bot_setup_rejected"));
        return Result.bAccepted;
    };
    if (!R.bStarted)
    {
        const auto* Starter = AMRuntimeKind(S, 100, EProofUnitKind::Harvester);
        if (!Starter) { AMRuntimeFail(TEXT("host"), TEXT("missing_bot_starter")); return; }
        R.Starter = R.Target = Starter->UnitId;
        if (!SubmitBot(AMRuntimeCommand(EProofCommandType::BuildRefinery, INDEX_NONE, FVector(1800, 0, 0)))) return;
        S = Model.GetSnapshot(); const auto* Miner = AMRuntimeKind(S, 100, EProofUnitKind::Harvester, R.Starter);
        const auto* Ore = S.Resources.FindByPredicate([](const FProofResourceRecord& Resource) { return Resource.OwnerId == 100; });
        if (!Miner || !Ore) { AMRuntimeFail(TEXT("host"), TEXT("missing_bot_grant")); return; }
        R.Miner = Miner->UnitId;
        auto Gather = AMRuntimeCommand(EProofCommandType::Gather, R.Miner); Gather.TargetUnitId = Ore->ResourceId;
        if (!SubmitBot(Gather) || !SubmitBot(AMRuntimeCommand(EProofCommandType::Move, R.Starter, FVector(-150, -700, 0)))) return;
        R.bStarted = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_START owners=1,2,100 target=%d simulatedSeconds=0"), R.Target);
    }
    R.Accumulator += DeltaSeconds;
    int32 Steps = 0;
    while (R.Accumulator >= 0.1f && Steps++ < 10)
    {
        const FProofSnapshot Before = Model.GetSnapshot();
        if (Before.MatchPhase != EProofMatchPhase::Playing) { AMRuntimeFail(TEXT("host"), TEXT("unexpected_terminal")); return; }
        Model.Advance(0.1f);
        R.SimulatedSeconds += static_cast<double>(0.1f); // Only actual advances contribute.
        const FProofSnapshot After = Model.GetSnapshot();
        RefineryEvidence.ObserveAdvance(MatchEpoch, Before, After);
        R.Accumulator -= 0.1f;
        if (++R.PublishSteps >= 2) { Publish(); R.PublishSteps = 0; }
    }
    S = Model.GetSnapshot();
    const auto* Bot = AMRuntimePlayer(S, 100); const auto* Miner = AMRuntimeUnit(S, R.Miner);
    if (!R.bBotStopped && Bot && Bot->TotalGathered >= 50 && Miner && Miner->Cargo == 0)
    {
        if (!SubmitBot(AMRuntimeCommand(EProofCommandType::Stop, R.Miner))) return;
        R.bBotStopped = true;
    }
    if (!R.bReady && R.StopAt >= 0.0 && R.SimulatedSeconds - R.StopAt >= 1.0)
    {
        const auto* Own = AMRuntimeUnit(S, R.Unit); const auto* Target = AMRuntimeUnit(S, R.Target);
        const auto* A = AMRuntimePlayer(S, 1); const auto* B = AMRuntimePlayer(S, 2);
        if (!Own || Own->OwnerId != 1 || Own->Kind != EProofUnitKind::Combat || Own->HitPoints <= 0.f
            || !Target || Target->OwnerId != 100 || Target->HitPoints != 0.f || !R.bBotStopped
            || !A || !B || !Bot || A->TotalGathered < 50 || B->TotalGathered < 50 || Bot->TotalGathered < 50
            || !Own->Position.Equals(R.StopPosition, 0.0001) || FVector::Dist(Own->Position, FVector(600, -700, 0)) <= 100.f)
        { AMRuntimeFail(TEXT("host"), TEXT("final_stopped_state")); return; }
        R.bReady = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_READY role=host status=pass unit=%d target=%d input=synthetic physicalInput=unverified"), R.Unit, R.Target);
    }
}

void AProofPlayerController::TickAttackMoveRuntime(float DeltaSeconds)
{
    // Owner 1 uses the rendered pawn/shared handlers. Owner 2 is an actual
    // receiving observer and performs only its required ordinary economy setup.
    if (OwnerId == INDEX_NONE || OwnerId == 1) return;
    auto& R = AttackMoveRuntime;
    if (!AMRuntimeConfig(R, TEXT("observer"))) return;
    R.Elapsed += DeltaSeconds;
    if (R.Elapsed > 55.f) { AMRuntimeFail(TEXT("observer"), TEXT("timeout")); return; }
    if (AMRuntimeRelease(R, TEXT("observer"))) return;
    const auto* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!GS || OwnerId == INDEX_NONE) return;
    if (OwnerId != 2) { AMRuntimeFail(TEXT("observer"), TEXT("wrong_owner")); return; }
    const FProofSnapshot& S = GS->Snapshot;
    if (!AMRuntimeRoster(S)) return;
    const auto Send = [this, &R, &S](FProofCommand C)
    {
        C.Sequence = NextLocalCommandSequence(); C.MatchEpoch = S.MatchEpoch; R.Sequence = C.Sequence;
        ServerSubmitCommand(C);
    };
    if (R.Stage > 0 && AMRuntimeReceipt(this, R.Sequence, TEXT("observer")) != 1) return;
    if (R.Stage == 0)
    {
        const auto* Starter = AMRuntimeKind(S, 2, EProofUnitKind::Harvester);
        if (!Starter) { AMRuntimeFail(TEXT("observer"), TEXT("missing_starter")); return; }
        R.Starter = Starter->UnitId;
        Send(AMRuntimeCommand(EProofCommandType::BuildRefinery, INDEX_NONE, FVector(-1050, 600, 0))); R.Stage = 1;
    }
    else if (R.Stage == 1)
    {
        const auto* Miner = AMRuntimeKind(S, 2, EProofUnitKind::Harvester, R.Starter);
        const auto* Ore = S.Resources.FindByPredicate([](const FProofResourceRecord& Resource) { return Resource.OwnerId == 2; });
        if (!Miner || !Ore) return;
        R.Miner = Miner->UnitId;
        auto Gather = AMRuntimeCommand(EProofCommandType::Gather, R.Miner); Gather.TargetUnitId = Ore->ResourceId;
        Send(Gather); R.Stage = 2;
    }
    else if (R.Stage == 2)
    {
        const auto* PlayerRecord = AMRuntimePlayer(S, 2); const auto* Miner = AMRuntimeUnit(S, R.Miner);
        if (!PlayerRecord || PlayerRecord->TotalGathered < 50 || !Miner || Miner->Cargo != 0) return;
        Send(AMRuntimeCommand(EProofCommandType::Stop, R.Miner)); R.Stage = 3;
    }
    else if (!R.bReady)
    {
        R.bReady = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_READY role=observer status=pass owner=2 input=synthetic physicalInput=unverified"));
    }
}

void AProofRTSPawn::TickAttackMoveRuntime(float DeltaSeconds)
{
    auto& R = AttackMoveRuntime;
    if (!AMRuntimeConfig(R, TEXT("rendered"))) return;
    R.Elapsed += DeltaSeconds;
    if (R.Elapsed > 55.f) { AMRuntimeFail(TEXT("rendered"), TEXT("timeout")); return; }
    if (AMRuntimeRelease(R, TEXT("rendered"))) return;
    auto* PC = Cast<AProofPlayerController>(GetController());
    const auto* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS || PC->OwnerId != 1) return;
    const FProofSnapshot& S = GS->Snapshot;
    int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
    if (!R.bViewportReady)
    {
        if (!bConfiguredLocalInput || Width != 1280 || Height != 720 || !PC->GetHUD()
            || FAssetCompilingManager::Get().GetNumRemainingAssets() != 0) return;
        const auto* FPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
        const auto* Optimize = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));
        if (!FPS || FPS->GetFloat() != 30.f || !Optimize || Optimize->GetInt() != 1
            || !FParse::Value(FCommandLine::Get(), TEXT("ProofAttackMoveOutput="), R.Output))
        { AMRuntimeFail(TEXT("rendered"), TEXT("render_configuration_or_output")); return; }
        IFileManager::Get().MakeDirectory(*R.Output, true);
        for (const TCHAR* Name : {TEXT("01-attack-move-moving.png"), TEXT("02-attack-move-firing.png"), TEXT("03-attack-move-resumed.png")})
            if (IFileManager::Get().FileExists(*FPaths::Combine(R.Output, Name)))
            { AMRuntimeFail(TEXT("rendered"), TEXT("stale_capture")); return; }
        const auto* Resolver = IConsoleManager::Get().FindConsoleVariable(TEXT("net.IpConnectionDisableResolution"));
        if (!Resolver || Resolver->GetInt() != 1) { AMRuntimeFail(TEXT("rendered"), TEXT("literal_resolver_configuration")); return; }
        UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_RENDER_CONFIG shadersOptimize=%d maxFPS=%.3f resolverDisabled=%d"), Optimize->GetInt(), FPS->GetFloat(), Resolver->GetInt());
        R.NativeRight = ManualRightPresses; R.NativeStop = ManualStopPresses; R.bViewportReady = true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_VIEW_READY owner=1 width=1280 height=720 maxFPS=30 shadersOptimize=1 physicalInput=unverified"));
    }
    if (!AMRuntimeRoster(S) || S.MatchPhase != EProofMatchPhase::Playing) return;
    const auto SelectId = [this, &S](int32 Id)
    {
        // Fixture identity selection is local-only. No native selection gesture
        // is claimed; AM and Stop still use the real shared action and queue.
        Selection.SelectSingle(S, 1, Id); SelectedUnitId = Selection.GetPrimaryUnitId();
        return SelectedUnitId == Id;
    };
    const auto Ground = [this](const FVector& Destination) { return SetControlsCursorFromWorld(Destination); };
    const auto Accepted = [this, PC](int32 Sequence)
    {
        if (LastRequestedSequence < Sequence) return false;
        if (LastRequestedSequence != Sequence) { AMRuntimeFail(TEXT("rendered"), TEXT("unexpected_sequence")); return false; }
        return AMRuntimeReceipt(PC, Sequence, TEXT("rendered")) == 1;
    };
    const auto Phase = [this, &S, &R](const TCHAR* Name, const FProofUnitRecord& Own, const FProofUnitRecord& Target)
    {
        UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_PHASE {\"phase\":\"%s\",\"epoch\":%d,\"revision\":%d,\"digest\":\"%s\",\"unit\":%d,\"target\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"targetHP\":%.9g,\"elapsed\":%.6f,\"sequence\":%d,\"selected\":%d,\"queue\":%d,\"nativeRight\":%d,\"nativeStop\":%d}"),
            Name, S.MatchEpoch, S.Revision, *S.ComputeDigest(), Own.UnitId, Target.UnitId, Own.Position.X, Own.Position.Y, Own.Position.Z,
            static_cast<double>(Target.HitPoints), R.Elapsed, LastRequestedSequence, SelectedUnitId, GroupOrders.NumPending(), ManualRightPresses, ManualStopPresses);
    };
    const auto Capture = [&R](const TCHAR* Name)
    {
        R.CaptureRequests.Add(Name, FDateTime::UtcNow());
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(R.Output, Name), true, false);
    };
    const auto CapturesWritten = [&R]()
    {
        if (R.CaptureRequests.Num() != 3) return false;
        for (const auto& Entry : R.CaptureRequests)
        {
            const FString Path = FPaths::Combine(R.Output, Entry.Key);
            if (IFileManager::Get().FileSize(*Path) <= 1024) return false;
            if (IFileManager::Get().GetTimeStamp(*Path).ToUnixTimestamp() < Entry.Value.ToUnixTimestamp())
            { AMRuntimeFail(TEXT("rendered"), TEXT("capture_timestamp")); return false; }
        }
        return true;
    };
    if (ManualRightPresses != R.NativeRight || ManualStopPresses != R.NativeStop)
    { AMRuntimeFail(TEXT("rendered"), TEXT("native_counter_changed")); return; }
    if (R.Stage == 0)
    {
        const auto* Starter = AMRuntimeKind(S, 1, EProofUnitKind::Harvester);
        const auto* Target = AMRuntimeKind(S, 100, EProofUnitKind::Harvester);
        if (!Starter || !Target || !Ground(FVector(-1050, -400, 0))) return;
        R.Starter = Starter->UnitId; R.Target = Target->UnitId;
        BuildUnderCursor(EProofCommandType::BuildRefinery); R.Stage = 1;
    }
    else if (R.Stage == 1 && Accepted(1))
    {
        const auto* Miner = AMRuntimeKind(S, 1, EProofUnitKind::Harvester, R.Starter);
        const auto* Ore = S.Resources.FindByPredicate([](const FProofResourceRecord& Resource) { return Resource.OwnerId == 1; });
        if (!Miner || !Ore || !SelectId(Miner->UnitId) || !Ground(Ore->Position)) return;
        R.Miner = Miner->UnitId; OrderUnderCursor(); R.Stage = 2;
    }
    else if (R.Stage == 2 && Accepted(2))
    {
        const auto* PlayerRecord = AMRuntimePlayer(S, 1); const auto* Miner = AMRuntimeUnit(S, R.Miner);
        if (!PlayerRecord || PlayerRecord->TotalGathered < 50 || !Miner || Miner->Cargo != 0) return;
        if (!SelectId(R.Miner)) { AMRuntimeFail(TEXT("rendered"), TEXT("miner_selection")); return; }
        StopSelected(); R.Stage = 3;
    }
    else if (R.Stage == 3 && Accepted(3))
    {
        if (!Ground(FVector(-900, -700, 0))) { AMRuntimeFail(TEXT("rendered"), TEXT("factory_projection")); return; }
        BuildUnderCursor(EProofCommandType::BuildFactory); R.Stage = 4;
    }
    else if (R.Stage == 4 && Accepted(4))
    {
        const auto* Factory = AMRuntimeKind(S, 1, EProofUnitKind::Factory);
        if (!Factory || !SelectId(Factory->UnitId)) return;
        Produce(); R.Stage = 5;
    }
    else if (R.Stage == 5 && Accepted(5))
    {
        const auto* Own = AMRuntimeKind(S, 1, EProofUnitKind::Combat); const auto* Target = AMRuntimeUnit(S, R.Target);
        const auto* A = AMRuntimePlayer(S, 1); const auto* B = AMRuntimePlayer(S, 2); const auto* Bot = AMRuntimePlayer(S, 100);
        if (!Own || !Target || !Target->Position.Equals(FVector(-150, -700, 0), 0.001)
            || !A || !B || !Bot || A->TotalGathered < 50 || B->TotalGathered < 50 || Bot->TotalGathered < 50) return;
        if (!SelectId(Own->UnitId) || !Ground(FVector(600, -700, 0))) { AMRuntimeFail(TEXT("rendered"), TEXT("attack_move_projection")); return; }
        R.Unit = Own->UnitId; R.Origin = Own->Position;
        AttackMoveUnderCursor(); R.Stage = 6;
    }
    else if (R.Stage >= 6)
    {
        const auto* Own = AMRuntimeUnit(S, R.Unit); const auto* Target = AMRuntimeUnit(S, R.Target);
        if (!Own || !Target || Own->OwnerId != 1 || Own->Kind != EProofUnitKind::Combat || Own->HitPoints <= 0.f
            || SelectedUnitId != R.Unit || Selection.GetSelectedUnitIds().Num() != 1)
        { AMRuntimeFail(TEXT("rendered"), TEXT("engagement_identity_or_selection")); return; }
        if (R.Stage == 6 && Accepted(6))
        {
            if (FVector::Dist(Own->Position, R.Origin) <= 50.f) return;
            if (Target->HitPoints != 100.f || FVector::Dist(Own->Position, Target->Position) <= FProofCommandModel::AttackRange)
            { AMRuntimeFail(TEXT("rendered"), TEXT("moving_phase_missed")); return; }
            Phase(TEXT("moving"), *Own, *Target); Capture(TEXT("01-attack-move-moving.png")); R.Stage = 7;
        }
        else if (R.Stage == 7)
        {
            if (Target->HitPoints <= 0.f) { AMRuntimeFail(TEXT("rendered"), TEXT("firing_phase_missed")); return; }
            if (Target->HitPoints >= 100.f) return;
            if (R.PhaseStarted < 0.f) { R.PhaseStarted = R.Elapsed; R.PausePosition = Own->Position; R.PauseHP = Target->HitPoints; }
            if (!Own->Position.Equals(R.PausePosition, 0.0001)) { AMRuntimeFail(TEXT("rendered"), TEXT("did_not_hold_position")); return; }
            R.Revisions.Add(S.Revision);
            if (R.Revisions.Num() < 3 || R.Elapsed - R.PhaseStarted < 1.5f || Target->HitPoints >= R.PauseHP) return;
            Phase(TEXT("firing"), *Own, *Target); Capture(TEXT("02-attack-move-firing.png")); R.Stage = 8;
        }
        else if (R.Stage == 8)
        {
            if (Target->HitPoints > 0.f || FVector::Dist(Own->Position, R.PausePosition) <= 50.f) return;
            if (FVector::Dist(Own->Position, FVector(600, -700, 0)) >= FVector::Dist(R.PausePosition, FVector(600, -700, 0)))
            { AMRuntimeFail(TEXT("rendered"), TEXT("resume_direction")); return; }
            Phase(TEXT("resumed"), *Own, *Target); Capture(TEXT("03-attack-move-resumed.png")); R.Stage = 9;
        }
        else if (R.Stage == 9 && CapturesWritten())
        {
            if (FVector::Dist(Own->Position, FVector(600, -700, 0)) <= 100.f)
            { AMRuntimeFail(TEXT("rendered"), TEXT("stop_after_destination")); return; }
            StopSelected(); R.Stage = 10;
        }
        else if (R.Stage == 10 && Accepted(7)) { R.ReceiptRevision = S.Revision; R.PhaseStarted = -1.f; R.Revisions.Reset(); R.Stage = 11; }
        else if (R.Stage == 11 && S.Revision > R.ReceiptRevision)
        {
            if (R.PhaseStarted < 0.f) { R.PhaseStarted = R.Elapsed; R.StopPosition = Own->Position; }
            if (!Own->Position.Equals(R.StopPosition, 0.0001)) { AMRuntimeFail(TEXT("rendered"), TEXT("stop_did_not_hold")); return; }
            R.Revisions.Add(S.Revision);
            if (R.Revisions.Num() < 3 || R.Elapsed - R.PhaseStarted < 1.5f) return;
            Phase(TEXT("stopped"), *Own, *Target);
            R.bReady = true; R.Stage = 12;
            UE_LOG(LogTemp, Display, TEXT("PROOF_ATTACK_MOVE_READY role=rendered status=pass unit=%d target=%d input=synthetic physicalInput=unverified"), R.Unit, R.Target);
        }
    }
}
