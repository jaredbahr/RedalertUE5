#include "Runtime/ProofBaseExpansionObservation.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
bool BaseExpansionIsHexToken(const FString& Value, int32 Length)
{
    if (Value.Len() != Length) return false;
    for (TCHAR Character : Value) if (!FChar::IsHexDigit(Character)) return false;
    return true;
}
TSharedRef<FJsonObject> BaseExpansionPoint(const FVector& Value)
{
    auto J = MakeShared<FJsonObject>();
    J->SetNumberField(TEXT("x"), Value.X);
    J->SetNumberField(TEXT("y"), Value.Y);
    J->SetNumberField(TEXT("z"), Value.Z);
    return J;
}
TSharedRef<FJsonObject> BaseExpansionUnit(const FProofUnitRecord& Value)
{
    auto J = MakeShared<FJsonObject>();
    J->SetNumberField(TEXT("id"), Value.UnitId);
    J->SetNumberField(TEXT("owner"), Value.OwnerId);
    J->SetNumberField(TEXT("kind"), static_cast<int32>(Value.Kind));
    J->SetNumberField(TEXT("hp"), Value.HitPoints);
    J->SetObjectField(TEXT("position"), BaseExpansionPoint(Value.Position));
    J->SetObjectField(TEXT("direction"), BaseExpansionPoint(Value.LastTravelDirection));
    J->SetNumberField(TEXT("cargo"), Value.Cargo);
    J->SetBoolField(TEXT("rally"), Value.bHasRallyPoint);
    J->SetObjectField(TEXT("rallyPoint"), BaseExpansionPoint(Value.RallyPoint));
    J->SetBoolField(TEXT("guard"), Value.bAreaGuard);
    J->SetObjectField(TEXT("post"), BaseExpansionPoint(Value.AreaGuardPost));
    J->SetNumberField(TEXT("target"), Value.AreaGuardTargetId);
    J->SetBoolField(TEXT("returning"), Value.bAreaGuardReturning);
    return J;
}
TSharedRef<FJsonObject> BaseExpansionState(FProofSnapshot Value, int32 Epoch)
{
    // Model snapshots carry epoch zero. The GameMode is the trusted epoch adapter.
    Value.MatchEpoch = Epoch;
    auto J = MakeShared<FJsonObject>();
    J->SetNumberField(TEXT("epoch"), Epoch);
    J->SetNumberField(TEXT("revision"), Value.Revision);
    J->SetStringField(TEXT("digest"), Value.ComputeDigest());
    J->SetBoolField(TEXT("skirmish"), Value.bSkirmish);
    J->SetNumberField(TEXT("phase"), static_cast<int32>(Value.MatchPhase));
    J->SetNumberField(TEXT("winner"), Value.WinnerTeam);
    TArray<TSharedPtr<FJsonValue>> Players;
    for (const auto& Record : Value.Players)
    {
        auto Player = MakeShared<FJsonObject>();
        Player->SetNumberField(TEXT("owner"), Record.OwnerId);
        Player->SetNumberField(TEXT("team"), Record.TeamId);
        Player->SetNumberField(TEXT("credits"), Record.Credits);
        Player->SetNumberField(TEXT("gathered"), Record.TotalGathered);
        Players.Add(MakeShared<FJsonValueObject>(Player));
    }
    J->SetArrayField(TEXT("players"), Players);
    TArray<TSharedPtr<FJsonValue>> Units;
    for (const auto& Record : Value.Units) Units.Add(MakeShared<FJsonValueObject>(BaseExpansionUnit(Record)));
    J->SetArrayField(TEXT("units"), Units);
    TArray<TSharedPtr<FJsonValue>> Resources;
    for (const auto& Record : Value.Resources)
    {
        auto Resource = MakeShared<FJsonObject>();
        Resource->SetNumberField(TEXT("id"), Record.ResourceId);
        Resource->SetNumberField(TEXT("owner"), Record.OwnerId);
        Resource->SetObjectField(TEXT("position"), BaseExpansionPoint(Record.Position));
        Resource->SetNumberField(TEXT("remaining"), Record.Remaining);
        Resources.Add(MakeShared<FJsonValueObject>(Resource));
    }
    J->SetArrayField(TEXT("resources"), Resources);
    return J;
}
}

FProofBaseExpansionObservation::FProofBaseExpansionObservation()
{
    bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofBaseExpansionObserve"));
    if (!bEnabled) return;
    FParse::Value(FCommandLine::Get(), TEXT("ProofBaseExpansionRun="), Run);
    FParse::Value(FCommandLine::Get(), TEXT("ProofBaseExpansionSource="), Source);
    if (!BaseExpansionIsHexToken(Run, 32) || !BaseExpansionIsHexToken(Source, 40)) Fail(TEXT("invalid-run-or-source"));
}
void FProofBaseExpansionObservation::Fail(const TCHAR* Reason)
{
    if (bFailed) return;
    bFailed = true;
    UE_LOG(LogTemp, Error, TEXT("PROOF_BASE_EXPANSION_OBSERVATION_ERROR reason=%s"), Reason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
void FProofBaseExpansionObservation::Emit(const TCHAR* Role, int32 Viewer, const TCHAR* Event,
    const TSharedRef<FJsonObject>& Row)
{
    if (!bEnabled || bFailed) return;
    if (bComplete) { Fail(TEXT("event-after-finish")); return; }
    Row->SetNumberField(TEXT("v"), 1);
    Row->SetStringField(TEXT("run"), Run);
    Row->SetStringField(TEXT("source"), Source);
    Row->SetStringField(TEXT("role"), Role);
    Row->SetNumberField(TEXT("viewer"), Viewer);
    Row->SetStringField(TEXT("event"), Event);
    Row->SetNumberField(TEXT("row"), ++Rows);
    FString Json;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    if (!FJsonSerializer::Serialize(Row, Writer)) { Fail(TEXT("serialize")); return; }
    Bytes += Json.Len();
    if (Rows > 8192 || Json.Len() > 65536 || Bytes > 32 * 1024 * 1024)
    { Fail(TEXT("stream-cap")); return; }
    UE_LOG(LogTemp, Display, TEXT("PROOF_BASE_EXPANSION_OBSERVATION %s"), *Json);
}
void FProofBaseExpansionObservation::ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (!bEnabled || bFailed) return;
    if (Epoch <= 0 || (EpochSeen && EpochSeen != Epoch)) { Fail(TEXT("epoch")); return; }
    EpochSeen = Epoch;
    auto J = MakeShared<FJsonObject>();
    J->SetNumberField(TEXT("owner"), Owner);
    J->SetNumberField(TEXT("type"), static_cast<int32>(Command.Type));
    J->SetNumberField(TEXT("sequence"), Command.Sequence);
    J->SetNumberField(TEXT("payloadEpoch"), Command.MatchEpoch);
    J->SetNumberField(TEXT("unit"), Command.UnitId);
    J->SetNumberField(TEXT("target"), Command.TargetUnitId);
    J->SetObjectField(TEXT("destination"), BaseExpansionPoint(Command.Destination));
    J->SetBoolField(TEXT("accepted"), Result.bAccepted);
    J->SetStringField(TEXT("reason"), Result.Reason);
    J->SetObjectField(TEXT("before"), BaseExpansionState(Before, Epoch));
    J->SetObjectField(TEXT("after"), BaseExpansionState(After, Epoch));
    ++CommandRows;
    Emit(TEXT("host"), 0, TEXT("command"), J);
}
void FProofBaseExpansionObservation::LogSnapshot(const FProofSnapshot& Snapshot, const TCHAR* Role, int32 Viewer)
{
    // This observer ends at this role's first terminal snapshot. Ordinary
    // Logout teardown can publish later terminal revisions; it is out of scope.
    if (!bEnabled || bFailed || bComplete || Snapshot.MatchEpoch <= 0 || !Snapshot.bSkirmish) return;
    if (EpochSeen && EpochSeen != Snapshot.MatchEpoch) { Fail(TEXT("epoch")); return; }
    EpochSeen = Snapshot.MatchEpoch;
    if (LastRevision == Snapshot.Revision) return;
    LastRevision = Snapshot.Revision;
    auto J = MakeShared<FJsonObject>();
    J->SetObjectField(TEXT("state"), BaseExpansionState(Snapshot, Snapshot.MatchEpoch));
    ++SnapshotRows;
    Emit(Role, Viewer, TEXT("snapshot"), J);
    if (!bFailed && Snapshot.MatchPhase == EProofMatchPhase::Finished)
    {
        // Bind a complete bounded stream to its terminal published state. Later
        // teardown publications are excluded by bComplete above.
        auto Finish = MakeShared<FJsonObject>();
        Finish->SetNumberField(TEXT("epoch"), Snapshot.MatchEpoch);
        Finish->SetNumberField(TEXT("revision"), Snapshot.Revision);
        Finish->SetStringField(TEXT("digest"), Snapshot.ComputeDigest());
        Finish->SetNumberField(TEXT("snapshots"), SnapshotRows);
        Finish->SetNumberField(TEXT("commands"), CommandRows);
        Emit(Role, Viewer, TEXT("finish"), Finish);
        bComplete = !bFailed;
    }
}
