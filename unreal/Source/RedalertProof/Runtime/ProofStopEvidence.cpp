#include "Runtime/ProofStopEvidence.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
bool StopFinite(const FVector& Position)
{
    return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z);
}

FString StopDigest(FProofSnapshot Snapshot, int32 Epoch)
{
    Snapshot.MatchEpoch = Epoch;
    return Snapshot.ComputeDigest();
}

// Serialize the legacy non-skirmish Stop fixture schema, with round-trip
// precision for its vectors/HP. The full digest also commits display metadata.
// Timed skirmish jobs require their separate evidence path; reject them here.
// Limits bound diagnostic allocations, not gameplay capacity.
bool StopSnapshotJson(const FProofSnapshot& Snapshot, int32 Epoch, FString& Json)
{
    if (!Snapshot.ProductionJobs.IsEmpty() || Epoch < 0 || Snapshot.Revision < 0 || Snapshot.Units.Num() > 128
        || Snapshot.Players.Num() > 16 || Snapshot.Resources.Num() > 64) { return false; }
    Json = FString::Printf(TEXT("{\"epoch\":%d,\"revision\":%d,\"digest\":\"%s\",\"skirmish\":%d,\"phase\":%d,\"winner\":%d,\"units\":["),
        Epoch, Snapshot.Revision, *StopDigest(Snapshot, Epoch), Snapshot.bSkirmish ? 1 : 0,
        static_cast<int32>(Snapshot.MatchPhase), Snapshot.WinnerTeam);
    bool bFirst = true;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (!StopFinite(Unit.Position) || !FMath::IsFinite(Unit.HitPoints) || Unit.Cargo < 0
            || !StopFinite(Unit.RallyPoint) || FMath::Abs(Unit.RallyPoint.X) > FProofCommandModel::WorldExtent
            || FMath::Abs(Unit.RallyPoint.Y) > FProofCommandModel::WorldExtent || Unit.RallyPoint.Z != 0.0
            || (!Unit.bHasRallyPoint && Unit.RallyPoint != FVector::ZeroVector)
            || (Unit.bHasRallyPoint && (Unit.Kind != EProofUnitKind::Factory || Unit.HitPoints <= 0.f))) { return false; }
        if (!bFirst) { Json += TEXT(","); }
        bFirst = false;
        Json += FString::Printf(TEXT("{\"id\":%d,\"owner\":%d,\"kind\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"hp\":%.9g,\"cargo\":%d,\"rally\":%d,\"rallyX\":%.17g,\"rallyY\":%.17g,\"rallyZ\":%.17g}"),
            Unit.UnitId, Unit.OwnerId, static_cast<int32>(Unit.Kind), Unit.Position.X, Unit.Position.Y, Unit.Position.Z,
            static_cast<double>(Unit.HitPoints), Unit.Cargo, Unit.bHasRallyPoint ? 1 : 0, Unit.RallyPoint.X, Unit.RallyPoint.Y, Unit.RallyPoint.Z);
    }
    Json += TEXT("],\"players\":[");
    bFirst = true;
    for (const FProofPlayerRecord& Player : Snapshot.Players)
    {
        if (Player.Credits < 0 || Player.TotalGathered < 0) { return false; }
        if (!bFirst) { Json += TEXT(","); }
        bFirst = false;
        Json += FString::Printf(TEXT("{\"id\":%d,\"team\":%d,\"credits\":%d,\"gathered\":%d}"),
            Player.OwnerId, Player.TeamId, Player.Credits, Player.TotalGathered);
    }
    Json += TEXT("],\"resources\":[");
    bFirst = true;
    for (const FProofResourceRecord& Resource : Snapshot.Resources)
    {
        if (!StopFinite(Resource.Position) || Resource.Remaining < 0) { return false; }
        if (!bFirst) { Json += TEXT(","); }
        bFirst = false;
        Json += FString::Printf(TEXT("{\"id\":%d,\"owner\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"remaining\":%d}"),
            Resource.ResourceId, Resource.OwnerId, Resource.Position.X, Resource.Position.Y, Resource.Position.Z, Resource.Remaining);
    }
    Json += TEXT("]}");
    return true;
}

bool StopSafeReason(const FString& Reason)
{
    if (Reason.IsEmpty() || Reason.Len() > 64) { return false; }
    for (const TCHAR Character : Reason)
    {
        if (!((Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z')
            || (Character >= '0' && Character <= '9') || Character == '-' || Character == '_')) { return false; }
    }
    return true;
}
}

FProofStopEvidence::FProofStopEvidence()
{
    bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofStopControls"));
    if (bEnabled && !FParse::Param(FCommandLine::Get(), TEXT("ProofControlsAutomation"))
        && !FParse::Param(FCommandLine::Get(), TEXT("ProofControlsObserver"))) { Fail(TEXT("controls-fixture-required")); }
}

bool FProofStopEvidence::WantsCommand(EProofCommandType Type) const
{
    return Enabled() && (Type == EProofCommandType::Move || Type == EProofCommandType::Stop);
}

void FProofStopEvidence::EnterEpoch(int32 NewEpoch)
{
    if (NewEpoch < 0) { Fail(TEXT("invalid-epoch")); return; }
    if (Epoch != NewEpoch) { Epoch = NewEpoch; SimulatedSeconds = 0.0; }
}

void FProofStopEvidence::Fail(const TCHAR* Reason)
{
    if (bFailed) { return; }
    bFailed = true;
    UE_LOG(LogTemp, Error, TEXT("PROOF_STOP_ERROR reason=%s"), Reason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

bool FProofStopEvidence::Emit(const TCHAR* Prefix, const FString& Json)
{
    if (!Enabled()) { return false; }
    if (Json.Len() > 32 * 1024 || PayloadBytes + Json.Len() > 4 * 1024 * 1024)
    { Fail(TEXT("payload-limit")); return false; }
    for (const TCHAR Character : Json)
    {
        if (Character < 0x20 || Character > 0x7e) { Fail(TEXT("non-ascii-payload")); return false; }
    }
    PayloadBytes += Json.Len(); // Combined command/state JSON payload, excluding UE log prefixes.
    UE_LOG(LogTemp, Display, TEXT("%s %s"), Prefix, *Json);
    return true;
}

void FProofStopEvidence::ObserveCommand(int32 NewEpoch, int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (!WantsCommand(Command.Type)) { return; }
    EnterEpoch(NewEpoch);
    if (!Enabled()) { return; }
    if (++CommandRows > 32 || ++OwnerCommandRows.FindOrAdd(Owner) > 32)
    { Fail(TEXT("command-row-limit")); return; }
    if (!StopFinite(Command.Destination)) { Fail(TEXT("invalid-command-destination")); return; }
    if (!StopSafeReason(Result.Reason)) { Fail(TEXT("invalid-command-reason")); return; }
    FProofSnapshot Expected = Before;
    if (Result.bAccepted) { Expected.Revision = After.Revision; }
    if (StopDigest(Expected, Epoch) != StopDigest(After, Epoch))
    { Fail(TEXT("command-visible-state-mutation")); return; }
    if (Result.bAccepted)
    {
        const FProofUnitRecord* Unit = After.Units.FindByPredicate([&Command](const FProofUnitRecord& Candidate) { return Candidate.UnitId == Command.UnitId; });
        if (!Unit || Unit->OwnerId != Owner || Unit->HitPoints <= 0.f)
        { Fail(TEXT("accepted-command-unit-identity")); return; }
    }
    FString BeforeJson, AfterJson;
    if (!StopSnapshotJson(Before, Epoch, BeforeJson) || !StopSnapshotJson(After, Epoch, AfterJson))
    { Fail(TEXT("invalid-command-snapshot")); return; }
    const FString Json = FString::Printf(TEXT("{\"role\":\"host\",\"owner\":%d,\"unit\":%d,\"sequence\":%lld,\"type\":\"%s\",\"destinationX\":%.17g,\"destinationY\":%.17g,\"destinationZ\":%.17g,\"accepted\":%d,\"reason\":\"%s\",\"epoch\":%d,\"beforeRevision\":%d,\"afterRevision\":%d,\"beforeDigest\":\"%s\",\"afterDigest\":\"%s\",\"immutable\":1,\"before\":%s,\"after\":%s}"),
        Owner, Command.UnitId, static_cast<long long>(Command.Sequence), Command.Type == EProofCommandType::Stop ? TEXT("Stop") : TEXT("Move"),
        Command.Destination.X, Command.Destination.Y, Command.Destination.Z,
        Result.bAccepted ? 1 : 0, *Result.Reason, Epoch, Before.Revision, After.Revision,
        *StopDigest(Before, Epoch), *StopDigest(After, Epoch), *BeforeJson, *AfterJson);
    Emit(TEXT("PROOF_STOP_COMMAND"), Json);
}

void FProofStopEvidence::ObserveAdvance(int32 NewEpoch, float DeltaSeconds)
{
    if (!Enabled()) { return; }
    EnterEpoch(NewEpoch);
    if (!Enabled()) { return; }
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { Fail(TEXT("invalid-advance-step")); return; }
    SimulatedSeconds += static_cast<double>(DeltaSeconds);
    if (!FMath::IsFinite(SimulatedSeconds)) { Fail(TEXT("invalid-simulation-clock")); }
}

void FProofStopEvidence::LogSnapshot(const FProofSnapshot& Snapshot, const TCHAR* Role, int32 Viewer)
{
    if (!Enabled() || (LastLogEpoch == Snapshot.MatchEpoch && LastLogRevision == Snapshot.Revision)) { return; }
    EnterEpoch(Snapshot.MatchEpoch);
    if (!Enabled()) { return; }
    const bool bHost = FCString::Strcmp(Role, TEXT("host")) == 0;
    if ((!bHost && FCString::Strcmp(Role, TEXT("client")) != 0) || (bHost && Viewer != 0))
    { Fail(TEXT("invalid-view-role")); return; }
    if (++StateRows > 1024) { Fail(TEXT("state-row-limit")); return; }
    if (LastLogEpoch == Snapshot.MatchEpoch && Snapshot.Revision < LastLogRevision)
    { Fail(TEXT("snapshot-revision-regressed")); return; }
    FString Body;
    if (!StopSnapshotJson(Snapshot, Epoch, Body)) { Fail(TEXT("invalid-state-snapshot")); return; }
    const FString Clock = bHost ? FString::Printf(TEXT("\"simulatedSeconds\":%.9f,"), SimulatedSeconds) : FString();
    const FString Json = FString::Printf(TEXT("{\"role\":\"%s\",\"viewer\":%d,%s%s"), Role, Viewer, *Clock, *Body.Mid(1));
    if (Emit(TEXT("PROOF_STOP_STATE"), Json))
    {
        LastLogEpoch = Snapshot.MatchEpoch;
        LastLogRevision = Snapshot.Revision;
    }
}
