#include "Runtime/ProofJobControlDiagnostic.h"
#include "Runtime/ProofGameMode.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/StringConv.h"

namespace ProofJobControlLocal
{
constexpr int32 MaxRows = 8192;
constexpr int32 MaxRowCharacters = 65536;
constexpr int64 MaxJsonBytes = 32 * 1024 * 1024;
constexpr double OverallSeconds = 90.0;
constexpr double StageSeconds = 20.0;
constexpr double ExitGraceSeconds = 6.0;

bool Need(bool& Failed, bool Condition, const TCHAR* Reason)
{
    if (!Condition && !Failed)
    {
        Failed = true;
        UE_LOG(LogTemp, Error, TEXT("PROOF_JOB_CONTROL_ERROR reason=%s"), Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
    return Condition && !Failed;
}

const TCHAR* Bool(bool Value) { return Value ? TEXT("true") : TEXT("false"); }

FString Quoted(const FString& Value)
{
    FString Out(TEXT("\""));
    for (TCHAR C : Value)
    {
        if (C == TEXT('\\')) Out += TEXT("\\\\");
        else if (C == TEXT('"')) Out += TEXT("\\\"");
        else if (C < 0x20) Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(C));
        else Out.AppendChar(C);
    }
    return Out + TEXT("\"");
}

bool Hex(const FString& Value, int32 Length)
{
    if (Value.Len() != Length) return false;
    for (TCHAR C : Value)
        if (!((C >= TEXT('0') && C <= TEXT('9')) || (C >= TEXT('a') && C <= TEXT('f')))) return false;
    return true;
}

bool Configuration(FString& Run, FString& Source, int32& Seat, bool Host, bool& Failed)
{
    FParse::Value(FCommandLine::Get(), TEXT("ProofJobControlRun="), Run);
    FParse::Value(FCommandLine::Get(), TEXT("ProofJobControlSource="), Source);
    FParse::Value(FCommandLine::Get(), TEXT("ProofJobControlSeat="), Seat);
    TArray<FString> Tokens;
    FString(FCommandLine::Get()).ParseIntoArrayWS(Tokens);
    int32 Runs = 0, Sources = 0, Seats = 0, Flags = 0, Skirmish = 0, Hosts = 0;
    for (const FString& Token : Tokens)
    {
        if (!Token.StartsWith(TEXT("-Proof"), ESearchCase::IgnoreCase)) continue;
        if (Token.StartsWith(TEXT("-ProofJobControlRun="), ESearchCase::IgnoreCase)) ++Runs;
        else if (Token.StartsWith(TEXT("-ProofJobControlSource="), ESearchCase::IgnoreCase)) ++Sources;
        else if (Token.StartsWith(TEXT("-ProofJobControlSeat="), ESearchCase::IgnoreCase)) ++Seats;
        else if (Token.Equals(TEXT("-ProofJobControlDiagnostic"), ESearchCase::IgnoreCase)) ++Flags;
        else if (Token.Equals(TEXT("-ProofJobControlHost"), ESearchCase::IgnoreCase)) ++Hosts;
        else if (Token.Equals(TEXT("-ProofSkirmish"), ESearchCase::IgnoreCase)) ++Skirmish;
        else return Need(Failed, false, TEXT("incompatible_proof_flag"));
    }
    return Need(Failed, Hex(Run, 32) && Hex(Source, 40) && Runs == 1 && Sources == 1 && Flags == 1
        && Skirmish == 1 && Hosts == (Host ? 1 : 0) && Seats == (Host ? 0 : 1)
        && (Host ? Seat == 0 : (Seat == 1 || Seat == 2)), TEXT("configuration"));
}

FString Vector(const FVector& V)
{
    return FString::Printf(TEXT("[%.17g,%.17g,%.17g]"), V.X, V.Y, V.Z);
}

// Every public field in the pinned FProofSnapshot schema is represented.
// Native ComputeDigest is recorded; no independent MD5 recomputation is claimed.
FString State(const FProofSnapshot& S)
{
    TArray<FString> Players, Units, Resources, Jobs;
    for (const FProofPlayerRecord& P : S.Players)
        Players.Add(FString::Printf(TEXT("[%d,%d,%d,%d]"), P.OwnerId, P.Credits, P.TeamId, P.TotalGathered));
    for (const FProofUnitRecord& U : S.Units)
        Units.Add(FString::Printf(TEXT("[%d,%d,%s,%s,%.17g,%d,%d,%s,%s,%s,%s,%d,%s]"),
            U.UnitId, U.OwnerId, *Vector(U.Position), *Vector(U.LastTravelDirection), static_cast<double>(U.HitPoints),
            static_cast<int32>(U.Kind), U.Cargo, Bool(U.bHasRallyPoint), *Vector(U.RallyPoint), Bool(U.bAreaGuard),
            *Vector(U.AreaGuardPost), U.AreaGuardTargetId, Bool(U.bAreaGuardReturning)));
    for (const FProofResourceRecord& R : S.Resources)
        Resources.Add(FString::Printf(TEXT("[%d,%d,%s,%d]"), R.ResourceId, R.OwnerId, *Vector(R.Position), R.Remaining));
    for (const FProofProductionJob& J : S.ProductionJobs)
        Jobs.Add(FString::Printf(TEXT("[%d,%d,%d,%d,%d,%s]"), J.OwnerId, J.StartSequence, J.FactoryId,
            J.WorkMicroseconds, J.PaidCredits, Bool(J.bHeld)));
    return FString::Printf(TEXT("{\"revision\":%d,\"epoch\":%d,\"skirmish\":%s,\"phase\":%d,\"winner\":%d,\"players\":[%s],\"units\":[%s],\"resources\":[%s],\"jobs\":[%s],\"digest\":%s}"),
        S.Revision, S.MatchEpoch, Bool(S.bSkirmish), static_cast<int32>(S.MatchPhase), S.WinnerTeam,
        *FString::Join(Players, TEXT(",")), *FString::Join(Units, TEXT(",")), *FString::Join(Resources, TEXT(",")),
        *FString::Join(Jobs, TEXT(",")), *Quoted(S.ComputeDigest()));
}

FString Command(const FProofCommand& C)
{
    return FString::Printf(TEXT("{\"sequence\":%d,\"epoch\":%d,\"type\":%d,\"unit\":%d,\"target\":%d,\"destination\":%s}"),
        C.Sequence, C.MatchEpoch, static_cast<int32>(C.Type), C.UnitId, C.TargetUnitId, *Vector(C.Destination));
}

bool SameCommand(const FProofCommand& A, const FProofCommand& B)
{
    return A.Sequence == B.Sequence && A.MatchEpoch == B.MatchEpoch && A.Type == B.Type
        && A.UnitId == B.UnitId && A.TargetUnitId == B.TargetUnitId && A.Destination == B.Destination;
}

bool Emit(const FString& Run, const FString& Source, const TCHAR* Role, int32 Owner, int32 Phase,
    int32& Rows, int64& Bytes, bool& Failed, const TCHAR* Kind, const FString& Data)
{
    if (Failed) return false;
    const FString Row = FString::Printf(TEXT("{\"v\":1,\"run\":%s,\"source\":%s,\"role\":\"%s\",\"owner\":%d,\"row\":%d,\"phase\":%d,\"kind\":\"%s\",\"data\":%s}"),
        *Quoted(Run), *Quoted(Source), Role, Owner, Rows + 1, Phase, Kind, *Data);
    const int64 Size = FTCHARToUTF8(*Row).Length();
    if (!Need(Failed, Rows < MaxRows && Row.Len() <= MaxRowCharacters && Bytes + Size <= MaxJsonBytes, TEXT("evidence_cap"))) return false;
    ++Rows; Bytes += Size;
    UE_LOG(LogTemp, Display, TEXT("PROOF_JOB_CONTROL %s"), *Row);
    return true;
}

int32 Cash(const FProofSnapshot& S)
{
    const FProofPlayerRecord* P = S.Players.FindByPredicate([](const FProofPlayerRecord& V) { return V.OwnerId == 1; });
    return P ? P->Credits : -1;
}

int32 Gathered(const FProofSnapshot& S)
{
    const FProofPlayerRecord* P = S.Players.FindByPredicate([](const FProofPlayerRecord& V) { return V.OwnerId == 1; });
    return P ? P->TotalGathered : -1;
}
}



FProofSnapshot FProofJobControlDiagnostic::Snapshot(const AProofGameMode& Mode) const
{
    FProofSnapshot S = Mode.Model.GetSnapshot();
    S.MatchEpoch = Mode.MatchEpoch;
    return S;
}

void FProofJobControlDiagnostic::Initialize(AProofGameMode& Mode)
{
    Started = StageStarted = FPlatformTime::Seconds();
    int32 Seat = 0;
    if (!ProofJobControlLocal::Configuration(Run, Source, Seat, true, bFailed)
        || !ProofJobControlLocal::Need(bFailed, Mode.bSkirmish && Mode.GetNetMode() == NM_DedicatedServer, TEXT("host_role"))) return;
    bInitialized = true;
    ProofJobControlLocal::Emit(Run, Source, TEXT("host"), 0, -1, Rows, Bytes, bFailed, TEXT("begin"),
        TEXT("{\"base\":\"9bb850cd5ac0cabdc005caca1ae3546e32fbc791\",\"schema\":\"full-public-v1\",\"scripted\":true}"));
}

bool FProofJobControlDiagnostic::ModelCommand(AProofGameMode& Mode, EProofCommandType Type, int32 Unit,
    int32 Target, const FVector& Destination, const TCHAR* Scope)
{
    FProofCommand C;
    C.Sequence = ++Sequence; C.MatchEpoch = Mode.MatchEpoch; C.Type = Type;
    C.UnitId = Unit; C.TargetUnitId = Target; C.Destination = Destination;
    const FProofSnapshot Before = Snapshot(Mode);
    const FProofCommandResult Result = Mode.SubmitForOwner(1, C);
    const FProofSnapshot After = Snapshot(Mode);
    if (!ProofJobControlLocal::Emit(Run, Source, TEXT("host"), 0, Phase, Rows, Bytes, bFailed, TEXT("model-command"),
        FString::Printf(TEXT("{\"scope\":\"%s\",\"owner\":1,\"command\":%s,\"accepted\":%s,\"reason\":%s,\"before\":%s,\"after\":%s}"),
            Scope, *ProofJobControlLocal::Command(C), ProofJobControlLocal::Bool(Result.bAccepted), *ProofJobControlLocal::Quoted(Result.Reason), *ProofJobControlLocal::State(Before), *ProofJobControlLocal::State(After)))) return false;
    return ProofJobControlLocal::Need(bFailed, Result.bAccepted && Result.Sequence == C.Sequence && Result.MatchEpoch == C.MatchEpoch, TEXT("setup_command"));
}

bool FProofJobControlDiagnostic::Advance(AProofGameMode& Mode, float Delta, const TCHAR* Scope)
{
    const FProofSnapshot Before = Snapshot(Mode);
    Mode.Model.Advance(Delta);
    const FProofSnapshot After = Snapshot(Mode);
    return ProofJobControlLocal::Emit(Run, Source, TEXT("host"), 0, Phase, Rows, Bytes, bFailed, TEXT("advance"),
        FString::Printf(TEXT("{\"advance\":%d,\"delta\":%.17g,\"scope\":\"%s\",\"before\":%s,\"after\":%s}"),
            ++Advances, static_cast<double>(Delta), Scope, *ProofJobControlLocal::State(Before), *ProofJobControlLocal::State(After)));
}

bool FProofJobControlDiagnostic::Prepare(AProofGameMode& Mode)
{
    const FProofSnapshot Initial = Snapshot(Mode);
    const FProofUnitRecord* HQ = Initial.Units.FindByPredicate([](const FProofUnitRecord& U)
        { return U.OwnerId == 1 && U.Kind == EProofUnitKind::HQ && U.HitPoints > 0; });
    if (!ProofJobControlLocal::Need(bFailed, HQ && ProofJobControlLocal::Cash(Initial) == 350 && Initial.ProductionJobs.IsEmpty(), TEXT("initial_state"))) return false;
    const FVector HQPosition = HQ->Position;
    if (!ModelCommand(Mode, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, HQPosition + FVector(-500,-300,0), TEXT("setup"))
        || !ModelCommand(Mode, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, HQPosition + FVector(500,-300,0), TEXT("setup"))) return false;
    const FProofSnapshot Built = Snapshot(Mode);
    for (const FProofUnitRecord& U : Built.Units)
    {
        if (U.OwnerId != 1) continue;
        if (U.Kind == EProofUnitKind::Factory) Factory = U.UnitId;
        if (U.Kind == EProofUnitKind::Refinery) Refinery = U.UnitId;
        if (U.Kind == EProofUnitKind::Harvester)
        {
            if (Miner == INDEX_NONE) Miner = U.UnitId;
            if (!ModelCommand(Mode, EProofCommandType::Stop, U.UnitId, INDEX_NONE, FVector::ZeroVector, TEXT("setup"))) return false;
        }
    }
    for (const FProofResourceRecord& R : Built.Resources) if (R.OwnerId == 1) Resource = R.ResourceId;
    if (!ProofJobControlLocal::Need(bFailed, Factory > 0 && Refinery > 0 && Miner > 0 && Resource > 0 && ProofJobControlLocal::Cash(Built) == 100, TEXT("setup_records"))) return false;
    for (int32 Job = 0; Job < 2; ++Job)
        if (!ModelCommand(Mode, EProofCommandType::Produce, Factory, INDEX_NONE, FVector::ZeroVector, TEXT("setup"))
            || !Advance(Mode, 5.f, TEXT("setup"))) return false;
    if (!ModelCommand(Mode, EProofCommandType::SetRally, Factory, INDEX_NONE, HQPosition + FVector(1100,100,0), TEXT("setup"))) return false;
    return ProofJobControlLocal::Need(bFailed, ProofJobControlLocal::Cash(Snapshot(Mode)) == 0 && Snapshot(Mode).ProductionJobs.IsEmpty(), TEXT("setup_zero_cash"));
}

bool FProofJobControlDiagnostic::Fund(AProofGameMode& Mode)
{
    const FProofSnapshot Before = Snapshot(Mode);
    const int32 PriorGathered = ProofJobControlLocal::Gathered(Before);
    if (!ProofJobControlLocal::Need(bFailed, ProofJobControlLocal::Cash(Before) == 0 && Before.ProductionJobs.Num() == 1, TEXT("funding_precondition"))) return false;
    if (!ModelCommand(Mode, EProofCommandType::Gather, Miner, Resource, FVector::ZeroVector, TEXT("fixture"))) return false;
    int32 Steps = 0;
    while (ProofJobControlLocal::Gathered(Snapshot(Mode)) == PriorGathered && Steps++ < 1000)
        if (!Advance(Mode, .1f, TEXT("measure"))) return false;
    const FProofSnapshot Deposited = Snapshot(Mode);
    if (!ProofJobControlLocal::Need(bFailed, ProofJobControlLocal::Gathered(Deposited) == PriorGathered + 50 && ProofJobControlLocal::Cash(Deposited) == 50
        && Deposited.ProductionJobs.Num() == 1 && Deposited.ProductionJobs[0].WorkMicroseconds == 0
        && Deposited.ProductionJobs[0].PaidCredits == 0, TEXT("actual_deposit_before_resumption"))) return false;
    for (const FProofUnitRecord& U : Deposited.Units)
        if (U.OwnerId == 1 && U.Kind == EProofUnitKind::Harvester
            && !ModelCommand(Mode, EProofCommandType::Stop, U.UnitId, INDEX_NONE, FVector::ZeroVector, TEXT("fixture"))) return false;
    return true;
}

void FProofJobControlDiagnostic::PublishPhase(AProofGameMode& Mode, int32 NextPhase)
{
    Phase = NextPhase; AcknowledgedOwners.Reset(); bWaitingRpc = false;
    StageStarted = FPlatformTime::Seconds(); Mode.Publish(); Published = Snapshot(Mode);
    if (!ProofJobControlLocal::Emit(Run, Source, TEXT("host"), 0, Phase, Rows, Bytes, bFailed, TEXT("snapshot"), ProofJobControlLocal::State(Published))) return;
    FProofCommand Empty;
    Mode.SkirmishOwner1->ClientJobControlCue(Run, Phase, 1, Published.MatchEpoch, Published.Revision, Published.ComputeDigest(), false, Empty);
    Mode.SkirmishOwner2->ClientJobControlCue(Run, Phase, 2, Published.MatchEpoch, Published.Revision, Published.ComputeDigest(), false, Empty);
}

void FProofJobControlDiagnostic::QueueRpc(AProofGameMode& Mode, EProofCommandType Type, int32 Target, const FVector& Destination)
{
    ExpectedCommand = FProofCommand();
    ExpectedCommand.Sequence = ++Sequence; ExpectedCommand.MatchEpoch = Mode.MatchEpoch;
    ExpectedCommand.Type = Type; ExpectedCommand.UnitId = Factory; ExpectedCommand.TargetUnitId = Target;
    ExpectedCommand.Destination = Destination; bWaitingRpc = true; StageStarted = FPlatformTime::Seconds();
    if (!ProofJobControlLocal::Emit(Run, Source, TEXT("host"), 0, Phase, Rows, Bytes, bFailed, TEXT("cue"), ProofJobControlLocal::Command(ExpectedCommand))) return;
    Mode.SkirmishOwner1->ClientJobControlCue(Run, Phase, 1, Published.MatchEpoch, Published.Revision, Published.ComputeDigest(), true, ExpectedCommand);
}

bool FProofJobControlDiagnostic::TrySubmitRpc(AProofGameMode& Mode, AProofPlayerController& Controller, const FProofCommand& C)
{
    if (!Mode.JobControlDiagnostic.bEnabled) return false;
    Mode.JobControlDiagnostic.SubmitRpc(Mode, Controller, C);
    return true;
}

void FProofJobControlDiagnostic::SubmitRpc(AProofGameMode& Mode, AProofPlayerController& Controller, const FProofCommand& C)
{
    if (!ProofJobControlLocal::Need(bFailed, bWaitingRpc && Controller.OwnerId == 1 && Mode.SkirmishOwner1.Get() == &Controller
        && Mode.IsCurrentController(&Controller) && ProofJobControlLocal::SameCommand(C, ExpectedCommand), TEXT("unexpected_actual_rpc"))) return;
    const FProofSnapshot Before = Snapshot(Mode);
    const FProofCommandResult Result = Mode.SubmitForController(&Controller, C);
    const FProofSnapshot After = Snapshot(Mode);
    ProofJobControlLocal::Emit(Run, Source, TEXT("host"), 0, Phase, Rows, Bytes, bFailed, TEXT("rpc"),
        FString::Printf(TEXT("{\"owner\":%d,\"command\":%s,\"accepted\":%s,\"reason\":%s,\"before\":%s,\"after\":%s}"),
            Controller.OwnerId, *ProofJobControlLocal::Command(C), ProofJobControlLocal::Bool(Result.bAccepted), *ProofJobControlLocal::Quoted(Result.Reason), *ProofJobControlLocal::State(Before), *ProofJobControlLocal::State(After)));
    Controller.ClientCommandResult(Result); // Ordinary owning-client receipt, before the next reliable cue.
    if (!ProofJobControlLocal::Need(bFailed, Result.bAccepted == (Phase != 16) && Result.Sequence == C.Sequence && Result.MatchEpoch == C.MatchEpoch
        && Result.Reason == (Phase == 16 ? TEXT("production") : TEXT("accepted")),
        TEXT("actual_rpc_result"))) return;
    if (Phase == 0) StartA = C.Sequence;
    if (Phase == 11) StartB = C.Sequence;
    PublishPhase(Mode, Phase + 1);
}

void FProofJobControlDiagnostic::AckForController(AProofGameMode& Mode, AProofPlayerController& Controller,
    const FString& AckRun, int32 AckPhase, int32 Epoch, int32 Revision, const FString& Digest)
{
    if (Mode.JobControlDiagnostic.bEnabled && Controller.AdmitIngress())
        Mode.JobControlDiagnostic.Ack(Mode, Controller, AckRun, AckPhase, Epoch, Revision, Digest);
}

void FProofJobControlDiagnostic::Ack(AProofGameMode& Mode, AProofPlayerController& Controller,
    const FString& AckRun, int32 AckPhase, int32 Epoch, int32 Revision, const FString& Digest)
{
    const int32 Owner = Controller.OwnerId;
    const bool Bound = (Owner == 1 && Mode.SkirmishOwner1.Get() == &Controller) || (Owner == 2 && Mode.SkirmishOwner2.Get() == &Controller);
    if (!ProofJobControlLocal::Need(bFailed, bPrepared && !bFinished && Bound && Mode.IsCurrentController(&Controller) && !bWaitingRpc
        && AckRun == Run && AckPhase == Phase && Epoch == Published.MatchEpoch && Revision == Published.Revision
        && Digest == Published.ComputeDigest() && !AcknowledgedOwners.Contains(Owner), TEXT("ack_identity_or_duplicate"))) return;
    AcknowledgedOwners.Add(Owner);
    ProofJobControlLocal::Emit(Run, Source, TEXT("host"), 0, Phase, Rows, Bytes, bFailed, TEXT("ack"),
        FString::Printf(TEXT("{\"ackOwner\":%d,\"epoch\":%d,\"revision\":%d,\"digest\":%s}"), Owner, Epoch, Revision, *ProofJobControlLocal::Quoted(Digest)));
}

void FProofJobControlDiagnostic::Tick(AProofGameMode& Mode)
{
    if (!bInitialized || bFailed) return;
    const double Now = FPlatformTime::Seconds();
    if (bFinished) { if (Now - FinishedAt >= ProofJobControlLocal::ExitGraceSeconds) FPlatformMisc::RequestExitWithStatus(false, 0); return; }
    if (!ProofJobControlLocal::Need(bFailed, Now - Started < ProofJobControlLocal::OverallSeconds && Now - StageStarted < ProofJobControlLocal::StageSeconds, TEXT("host_timeout"))) return;
    if (!Mode.SkirmishOwner1.IsValid() || !Mode.SkirmishOwner2.IsValid())
    { if (bPrepared) ProofJobControlLocal::Need(bFailed, false, TEXT("owner_disconnected")); return; }
    if (!bPrepared)
    {
        if (!Prepare(Mode)) return;
        bPrepared = true; PublishPhase(Mode, 0); return;
    }
    if (bWaitingRpc || AcknowledgedOwners.Num() != 2) return;
    switch (Phase)
    {
    case 0: QueueRpc(Mode, EProofCommandType::Produce); break;
    case 1: if (Advance(Mode, 10.f, TEXT("measure"))) PublishPhase(Mode, 2); break;
    case 2: if (Advance(Mode, 1.f, TEXT("measure"))) PublishPhase(Mode, 3); break;
    case 3: if (Fund(Mode)) PublishPhase(Mode, 4); break;
    case 4: if (Advance(Mode, .1f, TEXT("measure"))) PublishPhase(Mode, 5); break;
    case 5:
    {
        const FProofSnapshot S = Snapshot(Mode);
        const FProofUnitRecord* HQ = S.Units.FindByPredicate([](const FProofUnitRecord& U) { return U.OwnerId == 1 && U.Kind == EProofUnitKind::HQ; });
        if (ProofJobControlLocal::Need(bFailed, HQ != nullptr, TEXT("rally_hq"))) QueueRpc(Mode, EProofCommandType::SetRally, INDEX_NONE, HQ->Position + FVector(500,-1300,0));
        break;
    }
    case 6: QueueRpc(Mode, EProofCommandType::HoldProduction, StartA); break;
    case 7: if (Advance(Mode, 1.f, TEXT("measure"))) PublishPhase(Mode, 8); break;
    case 8: QueueRpc(Mode, EProofCommandType::ResumeProduction, StartA); break;
    case 9: if (Advance(Mode, 5.f, TEXT("measure"))) PublishPhase(Mode, 10); break;
    case 10: if (Advance(Mode, .1f, TEXT("measure"))) PublishPhase(Mode, 11); break;
    case 11: QueueRpc(Mode, EProofCommandType::Produce); break;
    case 12: if (Fund(Mode)) PublishPhase(Mode, 13); break;
    case 13: if (Advance(Mode, .1f, TEXT("measure"))) PublishPhase(Mode, 14); break;
    case 14: QueueRpc(Mode, EProofCommandType::HoldProduction, StartB); break;
    case 15: QueueRpc(Mode, EProofCommandType::CancelProduction, StartB); break;
    case 16: QueueRpc(Mode, EProofCommandType::CancelProduction, StartB); break;
    case 17:
        if (!ProofJobControlLocal::Emit(Run, Source, TEXT("host"), 0, Phase, Rows, Bytes, bFailed, TEXT("finish"), TEXT("{\"status\":\"pass\",\"phases\":18,\"rpcCommands\":8}"))) return;
        bFinished = true; FinishedAt = Now;
        Mode.SkirmishOwner1->ClientJobControlFinish(Run);
        Mode.SkirmishOwner2->ClientJobControlFinish(Run);
        break;
    default: ProofJobControlLocal::Need(bFailed, false, TEXT("host_phase"));
    }
}

void FProofJobControlDiagnostic::Cue(AProofPlayerController& Controller, const FString& CueRun, int32 CuePhase,
    int32 Seat, int32 Epoch, int32 Revision, const FString& Digest, bool bSend, const FProofCommand& C)
{
    FProofJobControlClientState& S = Controller.JobControlDiagnostic;
    if (!S.bEnabled || S.bFailed) return;
    if (!S.bConfigured)
    {
        S.Started = S.StageStarted = FPlatformTime::Seconds();
        if (!ProofJobControlLocal::Configuration(S.Run, S.Source, S.Seat, false, S.bFailed)) return;
        S.bConfigured = true;
    }
    if (!ProofJobControlLocal::Need(S.bFailed, Controller.GetNetMode() == NM_Client && CueRun == S.Run && Seat == S.Seat
        && Epoch > 0 && Revision >= 0 && ProofJobControlLocal::Hex(Digest, 32) && !S.bFinished, TEXT("client_cue_identity"))) return;
    if (bSend)
    {
        if (!ProofJobControlLocal::Need(S.bFailed, CuePhase == S.Phase && S.bObserved && !S.bSend && !S.bSent && S.bReceiptSeen
            && Epoch == S.Epoch && Revision == S.Revision && Digest == S.Digest && Seat == 1, TEXT("client_command_cue"))) return;
        S.Command = C; S.bSend = true;
    }
    else
    {
        if (!ProofJobControlLocal::Need(S.bFailed, CuePhase == S.Phase + 1 && (!S.bHasCue || S.bObserved), TEXT("client_phase_order"))) return;
        S.Phase = CuePhase; S.Epoch = Epoch; S.Revision = Revision; S.Digest = Digest;
        S.bHasCue = true; S.bObserved = false; S.bSend = false; S.bSent = false;
    }
    S.StageStarted = FPlatformTime::Seconds();
}

void FProofJobControlDiagnostic::TickClient(AProofPlayerController& Controller)
{
    FProofJobControlClientState& S = Controller.JobControlDiagnostic;
    if (S.bFailed || S.bFinished) return;
    if (!S.bConfigured)
    {
        S.Started = S.StageStarted = FPlatformTime::Seconds();
        if (!ProofJobControlLocal::Configuration(S.Run, S.Source, S.Seat, false, S.bFailed)) return;
        S.bConfigured = true;
    }
    const double Now = FPlatformTime::Seconds();
    if (!ProofJobControlLocal::Need(S.bFailed, Controller.GetNetMode() == NM_Client && Now - S.Started < ProofJobControlLocal::OverallSeconds
        && Now - S.StageStarted < ProofJobControlLocal::StageSeconds, TEXT("client_role_or_timeout"))) return;
    AProofGameState* GS = Controller.GetWorld()->GetGameState<AProofGameState>();
    Controller.SynchronizeMatchEpoch();
    if (!GS || Controller.OwnerId == INDEX_NONE) return;
    if (!ProofJobControlLocal::Need(S.bFailed, Controller.OwnerId == S.Seat, TEXT("replicated_owner"))) return;
    if (!S.bBegan)
    {
        S.bBegan = true;
        if (!ProofJobControlLocal::Emit(S.Run, S.Source, TEXT("client"), S.Seat, -1, S.Rows, S.Bytes, S.bFailed, TEXT("begin"),
            TEXT("{\"base\":\"9bb850cd5ac0cabdc005caca1ae3546e32fbc791\",\"schema\":\"full-public-v1\",\"scripted\":true}"))) return;
    }
    if (!S.bHasCue || !S.bReceiptSeen) return;
    // After our ACK the server may publish the next state before its reliable
    // cue reaches this controller. Our sealed observation is already complete.
    if (S.bObserved && (!S.bSend || S.bSent)) return;
    const FProofSnapshot& Actual = GS->Snapshot;
    if (Actual.MatchEpoch == 0 || Actual.Revision < S.Revision) return;
    if (!ProofJobControlLocal::Need(S.bFailed, Actual.MatchEpoch == S.Epoch && Actual.Revision == S.Revision
        && Actual.ComputeDigest() == S.Digest, TEXT("published_state_identity"))) return;
    if (!S.bObserved)
    {
        if (!ProofJobControlLocal::Emit(S.Run, S.Source, TEXT("client"), S.Seat, S.Phase, S.Rows, S.Bytes, S.bFailed, TEXT("snapshot"), ProofJobControlLocal::State(Actual))) return;
        S.bObserved = true;
        if (!ProofJobControlLocal::Emit(S.Run, S.Source, TEXT("client"), S.Seat, S.Phase, S.Rows, S.Bytes, S.bFailed, TEXT("ack"),
            FString::Printf(TEXT("{\"ackOwner\":%d,\"epoch\":%d,\"revision\":%d,\"digest\":%s}"), S.Seat, S.Epoch, S.Revision, *ProofJobControlLocal::Quoted(S.Digest)))) return;
        Controller.ServerJobControlAck(S.Run, S.Phase, S.Epoch, S.Revision, S.Digest);
    }
    if (S.bSend && !S.bSent)
    {
        Controller.NextLocalSequence = FMath::Max(Controller.NextLocalSequence, S.Command.Sequence);
        FProofCommand C = S.Command;
        C.Sequence = Controller.NextLocalCommandSequence(); C.MatchEpoch = Actual.MatchEpoch;
        if (!ProofJobControlLocal::Need(S.bFailed, ProofJobControlLocal::SameCommand(C, S.Command), TEXT("scripted_sequence_adaptation"))) return;
        if (!ProofJobControlLocal::Emit(S.Run, S.Source, TEXT("client"), S.Seat, S.Phase, S.Rows, S.Bytes, S.bFailed, TEXT("send"), ProofJobControlLocal::Command(C))) return;
        S.LastSent = C; S.bSent = true; S.bReceiptSeen = false;
        Controller.ServerSubmitCommand(C);
    }
}

void FProofJobControlDiagnostic::Receipt(AProofPlayerController& Controller, const FProofCommandResult& Result)
{
    FProofJobControlClientState& S = Controller.JobControlDiagnostic;
    if (!S.bEnabled || S.bFailed) return;
    if (!ProofJobControlLocal::Need(S.bFailed, S.bBegan && S.Seat == 1 && S.bSent && !S.bReceiptSeen
        && Result.Sequence == S.LastSent.Sequence && Result.MatchEpoch == S.LastSent.MatchEpoch
        && Result.bAccepted == (S.Phase != 16)
        && Result.Reason == (S.Phase == 16 ? TEXT("production") : TEXT("accepted")), TEXT("actual_client_receipt"))) return;
    S.bReceiptSeen = true;
    ProofJobControlLocal::Emit(S.Run, S.Source, TEXT("client"), S.Seat, S.Phase, S.Rows, S.Bytes, S.bFailed, TEXT("receipt"),
        FString::Printf(TEXT("{\"command\":%s,\"accepted\":%s,\"reason\":%s}"), *ProofJobControlLocal::Command(S.LastSent), ProofJobControlLocal::Bool(Result.bAccepted), *ProofJobControlLocal::Quoted(Result.Reason)));
}

void FProofJobControlDiagnostic::FinishClient(AProofPlayerController& Controller, const FString& FinishRun)
{
    FProofJobControlClientState& S = Controller.JobControlDiagnostic;
    if (!S.bEnabled || S.bFailed) return;
    if (!ProofJobControlLocal::Need(S.bFailed, FinishRun == S.Run && S.bBegan && S.bObserved && S.Phase == 17 && S.bReceiptSeen && !S.bFinished,
        TEXT("client_finish_barrier"))) return;
    if (!ProofJobControlLocal::Emit(S.Run, S.Source, TEXT("client"), S.Seat, S.Phase, S.Rows, S.Bytes, S.bFailed, TEXT("finish"), TEXT("{\"status\":\"pass\",\"phases\":18}"))) return;
    S.bFinished = true;
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

void AProofPlayerController::ClientJobControlCue_Implementation(const FString& Run, int32 Phase, int32 Seat,
    int32 Epoch, int32 Revision, const FString& Digest, bool bSend, FProofCommand Command)
{
    FProofJobControlDiagnostic::Cue(*this, Run, Phase, Seat, Epoch, Revision, Digest, bSend, Command);
}

void AProofPlayerController::ServerJobControlAck_Implementation(const FString& Run, int32 Phase,
    int32 Epoch, int32 Revision, const FString& Digest)
{
    if (AProofGameMode* Mode = GetWorld()->GetAuthGameMode<AProofGameMode>())
        FProofJobControlDiagnostic::AckForController(*Mode, *this, Run, Phase, Epoch, Revision, Digest);
}

void AProofPlayerController::ClientJobControlFinish_Implementation(const FString& Run)
{
    FProofJobControlDiagnostic::FinishClient(*this, Run);
}
