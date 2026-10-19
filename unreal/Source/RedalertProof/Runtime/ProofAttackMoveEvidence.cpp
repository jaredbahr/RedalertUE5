#include "Runtime/ProofAttackMoveEvidence.h"
#include "Runtime/ProofProductionAccounting.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
void AttackMoveEvidenceFail(const TCHAR* Reason)
{
    UE_LOG(LogTemp, Error, TEXT("PROOF_ATTACK_MOVE_ERROR role=host reason=%s"), Reason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
void AttackMoveJsonLog(const TCHAR* Marker, const TSharedRef<FJsonObject>& Row)
{
    FString Json;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    const int32 Limit = FCString::Strcmp(Marker, TEXT("PROOF_ATTACK_MOVE_COMMAND")) == 0 ? 128 * 1024 : 8192;
    if (!FJsonSerializer::Serialize(Row, Writer) || Json.Len() > Limit)
    { AttackMoveEvidenceFail(TEXT("event_serialization_or_size")); return; }
    UE_LOG(LogTemp, Display, TEXT("%s %s"), Marker, *Json);
}
TArray<TSharedPtr<FJsonValue>> AttackMoveIds(const FProofSnapshot& State)
{
    TArray<TSharedPtr<FJsonValue>> Ids;
    for (const auto& Unit : State.Units) Ids.Add(MakeShared<FJsonValueNumber>(Unit.UnitId));
    return Ids;
}
}
FProofAttackMoveEvidence::FProofAttackMoveEvidence()
{
    bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofAttackMoveFixtureHost"));
}
void FProofAttackMoveEvidence::ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, FProofSnapshot Before, FProofSnapshot After)
{
    if (!bEnabled) return;
    if (++Commands > 32 || Before.Units.Num() > 128 || After.Units.Num() > 128)
    { AttackMoveEvidenceFail(TEXT("command_row_cap")); return; }
    Before.MatchEpoch = After.MatchEpoch = Epoch;
    const auto* Source = Before.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; });
    const auto* BeforePlayer = Before.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; });
    const auto* AfterPlayer = After.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; });
    if (!BeforePlayer || !AfterPlayer) { AttackMoveEvidenceFail(TEXT("event_owner")); return; }
    const FString BeforeDigest = Before.ComputeDigest(), AfterDigest = After.ComputeDigest();
    const int32 ActualRevision = After.Revision;
    After.Revision = Before.Revision;
    const bool bVisibleUnchanged = After.ComputeDigest() == BeforeDigest;
    After.Revision = ActualRevision;
    const auto Row = MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("owner"), Owner); Row->SetNumberField(TEXT("epoch"), Epoch);
    Row->SetNumberField(TEXT("sequence"), Command.Sequence); Row->SetNumberField(TEXT("type"), static_cast<uint8>(Command.Type));
    Row->SetNumberField(TEXT("unit"), Command.UnitId); Row->SetNumberField(TEXT("target"), Command.TargetUnitId);
    Row->SetNumberField(TEXT("accepted"), Result.bAccepted ? 1 : 0); Row->SetStringField(TEXT("reason"), Result.Reason);
    Row->SetNumberField(TEXT("beforeRevision"), Before.Revision); Row->SetNumberField(TEXT("revision"), After.Revision);
    Row->SetStringField(TEXT("beforeDigest"), BeforeDigest); Row->SetStringField(TEXT("digest"), AfterDigest);
    Row->SetNumberField(TEXT("destinationX"), Command.Destination.X); Row->SetNumberField(TEXT("destinationY"), Command.Destination.Y); Row->SetNumberField(TEXT("destinationZ"), Command.Destination.Z);
    Row->SetNumberField(TEXT("creditsBefore"), BeforePlayer->Credits); Row->SetNumberField(TEXT("creditsAfter"), AfterPlayer->Credits);
    Row->SetNumberField(TEXT("kind"), Source ? static_cast<int32>(Source->Kind) : -1);
    Row->SetNumberField(TEXT("sourceOwner"), Source ? Source->OwnerId : -1);
    Row->SetNumberField(TEXT("x"), Source ? Source->Position.X : 0.0); Row->SetNumberField(TEXT("y"), Source ? Source->Position.Y : 0.0); Row->SetNumberField(TEXT("z"), Source ? Source->Position.Z : 0.0);
    Row->SetNumberField(TEXT("hp"), Source ? Source->HitPoints : 0.0);
    Row->SetNumberField(TEXT("visibleUnchanged"), bVisibleUnchanged ? 1 : 0);
    Row->SetArrayField(TEXT("beforeIds"), AttackMoveIds(Before)); Row->SetArrayField(TEXT("afterIds"), AttackMoveIds(After));
    TSharedPtr<FJsonObject> ProductionBefore, ProductionAfter;
    const auto BeforeReader = TJsonReaderFactory<>::Create(ProofProductionAccounting::ContextJson(Before));
    const auto AfterReader = TJsonReaderFactory<>::Create(ProofProductionAccounting::ContextJson(After));
    if (!FJsonSerializer::Deserialize(BeforeReader, ProductionBefore) || !FJsonSerializer::Deserialize(AfterReader, ProductionAfter)
        || !ProductionBefore.IsValid() || !ProductionAfter.IsValid())
    { AttackMoveEvidenceFail(TEXT("production_context_serialization")); return; }
    Row->SetObjectField(TEXT("productionBefore"), ProductionBefore); Row->SetObjectField(TEXT("productionAfter"), ProductionAfter);
    AttackMoveJsonLog(TEXT("PROOF_ATTACK_MOVE_COMMAND"), Row);
}
void FProofAttackMoveEvidence::LogClock(int32 Epoch, const FProofSnapshot& Snapshot, double SimulatedSeconds)
{
    if (!bEnabled || (LastEpoch == Epoch && LastRevision == Snapshot.Revision)) return;
    if (++ClockRows > 1024 || !FMath::IsFinite(SimulatedSeconds) || SimulatedSeconds < 0.0)
    { AttackMoveEvidenceFail(TEXT("clock_row_cap_or_time")); return; }
    LastEpoch = Epoch; LastRevision = Snapshot.Revision;
    FProofSnapshot Stamped = Snapshot; Stamped.MatchEpoch = Epoch;
    const auto Row = MakeShared<FJsonObject>();
    Row->SetNumberField(TEXT("epoch"), Epoch); Row->SetNumberField(TEXT("revision"), Snapshot.Revision);
    Row->SetStringField(TEXT("digest"), Stamped.ComputeDigest()); Row->SetNumberField(TEXT("simulatedSeconds"), SimulatedSeconds);
    AttackMoveJsonLog(TEXT("PROOF_ATTACK_MOVE_CLOCK"), Row);
}
