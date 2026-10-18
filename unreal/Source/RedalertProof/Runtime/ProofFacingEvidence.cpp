#include "Runtime/ProofFacingEvidence.h"
#include "Runtime/ProofPlayerController.h"
#include "Runtime/ProofGameState.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
struct FObservedMove { int32 Epoch; int32 Sequence; };
TMap<int32, FObservedMove> ObservedMoves;
int32 DisplacementRows = 0;

void FacingVector(const TSharedRef<FJsonObject>& Row, const TCHAR* Key, const FVector& Value)
{
    Row->SetArrayField(Key, {MakeShared<FJsonValueNumber>(Value.X),
        MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)});
}
}

bool ProofFacingDisplacementEnabled()
{
    return FParse::Param(FCommandLine::Get(), TEXT("ProofFacingEvidence"))
        || FParse::Param(FCommandLine::Get(), TEXT("ProofVisualGeometryEvidence"));
}

// Diagnostic bookkeeping only, after the real authoritative Submit returns.
void ObserveProofFacingCommand(int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, const FProofSnapshot& After)
{
    if (!ProofFacingDisplacementEnabled() || Owner != 1 || !Result.bAccepted) return;
    ObservedMoves.Remove(Command.UnitId);
    if (Command.Type != EProofCommandType::Move) return;
    const FProofUnitRecord* Unit = After.Units.FindByPredicate([&Command](const FProofUnitRecord& U)
        { return U.UnitId == Command.UnitId && U.OwnerId == 1 && U.HitPoints > 0.f; });
    if (!Unit) return;
    ObservedMoves.Add(Unit->UnitId, {After.MatchEpoch, Command.Sequence});
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACING_MOVE_ACCEPTED epoch=%d unit=%d sequence=%d revision=%d"),
        After.MatchEpoch, Unit->UnitId, Command.Sequence, After.Revision);
}

// Read-only snapshots directly around one real Advance. No candidate direction,
// destination, visual axis, or production normalization helper enters this trace.
void ObserveProofFacingAdvance(const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (!ProofFacingDisplacementEnabled()) return;
    for (const auto& Entry : ObservedMoves)
    {
        if (Entry.Value.Epoch != After.MatchEpoch || Before.MatchEpoch != After.MatchEpoch) continue;
        const auto Find = [&Entry](const FProofSnapshot& S) { return S.Units.FindByPredicate(
            [&Entry](const FProofUnitRecord& U) { return U.UnitId == Entry.Key && U.OwnerId == 1 && U.HitPoints > 0.f; }); };
        const FProofUnitRecord* A = Find(Before);
        const FProofUnitRecord* B = Find(After);
        if (!A || !B) continue;
        if (++DisplacementRows > 4096)
        { UE_LOG(LogTemp, Error, TEXT("PROOF_FACING_DISPLACEMENT_ERROR row_limit")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
        const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("epoch"), After.MatchEpoch);
        Row->SetNumberField(TEXT("unit"), Entry.Key);
        Row->SetNumberField(TEXT("sequence"), Entry.Value.Sequence);
        Row->SetNumberField(TEXT("beforeRevision"), Before.Revision);
        Row->SetNumberField(TEXT("afterRevision"), After.Revision);
        FacingVector(Row, TEXT("before"), A->Position);
        FacingVector(Row, TEXT("after"), B->Position);
        FString Json;
        if (!FJsonSerializer::Serialize(Row, TJsonWriterFactory<>::Create(&Json)))
        { UE_LOG(LogTemp, Error, TEXT("PROOF_FACING_DISPLACEMENT_ERROR serialize")); return; }
        Json.ReplaceInline(TEXT("\r"), TEXT("")); Json.ReplaceInline(TEXT("\n"), TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("PROOF_FACING_DISPLACEMENT %s"), *Json);
    }
}

void LogProofFacingSnapshot(const FProofSnapshot& Snapshot, const TCHAR* Role, int32 Viewer)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ProofFacingEvidence"))) return;
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([](const FProofUnitRecord& U)
        { return U.OwnerId == 1 && U.Kind == EProofUnitKind::Harvester && U.HitPoints > 0.f; });
    if (!Unit) return;
    static int32 Rows = 0;
    if (++Rows > 4096)
    { UE_LOG(LogTemp, Error, TEXT("PROOF_FACING_STATE_ERROR row_limit")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
    const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("role"), Role);
    Row->SetNumberField(TEXT("viewer"), Viewer);
    Row->SetNumberField(TEXT("epoch"), Snapshot.MatchEpoch);
    Row->SetNumberField(TEXT("revision"), Snapshot.Revision);
    Row->SetStringField(TEXT("digest"), Snapshot.ComputeDigest());
    Row->SetNumberField(TEXT("unit"), Unit->UnitId);
    Row->SetNumberField(TEXT("owner"), Unit->OwnerId);
    Row->SetNumberField(TEXT("kind"), static_cast<int32>(Unit->Kind));
    Row->SetNumberField(TEXT("hp"), Unit->HitPoints);
    FacingVector(Row, TEXT("position"), Unit->Position);
    FacingVector(Row, TEXT("lastTravelDirection"), Unit->LastTravelDirection);
    FString Json;
    if (!FJsonSerializer::Serialize(Row, TJsonWriterFactory<>::Create(&Json))) return;
    Json.ReplaceInline(TEXT("\r"), TEXT("")); Json.ReplaceInline(TEXT("\n"), TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACING_STATE %s"), *Json);
}

bool AProofPlayerController::TickFacingLifecyclePreflight(AProofGameState* State)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ProofFacingEvidence")) || LifecycleRole != TEXT("lead")
        || !State || State->Snapshot.MatchEpoch != 1) return false;
    const auto Gate = [this](const TCHAR* Name)
        { return IFileManager::Get().FileExists(*FPaths::Combine(LifecycleControl, Name)); };
    if (!Gate(TEXT("facing-preflight")) || Gate(TEXT("play-epoch-1"))) return false;
    const FProofUnitRecord* Unit = State->Snapshot.Units.FindByPredicate([this](const FProofUnitRecord& U)
        { return U.OwnerId == OwnerId && U.Kind == EProofUnitKind::Harvester && U.HitPoints > 0.f
            && (FacingPreflightUnit == INDEX_NONE || U.UnitId == FacingPreflightUnit); });
    if (!Unit || OwnerId != 1) return true;
    FacingPreflightUnit = Unit->UnitId;
    auto Fail = [this](const TCHAR* Reason)
    {
        bFinished = true;
        UE_LOG(LogTemp, Error, TEXT("PROOF_FACING_PREFLIGHT status=fail reason=%s"), Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    if (FacingPreflightPending != INDEX_NONE)
    {
        FProofCommandResult Receipt;
        if (!FindReceipt(FacingPreflightPending, Receipt)) return true;
        if (!Receipt.bAccepted) { Fail(TEXT("receipt")); return true; }
        FacingPreflightPending = INDEX_NONE;
        FacingPreflightOrigin = Unit->Position; // actual post-receipt travel baseline
    }
    if (FacingPreflightStep == 0 || FacingPreflightStep == 2)
    {
        FacingPreflightOrigin = Unit->Position;
        FacingPreflightGoal = Unit->Position + (FacingPreflightStep == 0 ? FVector(300.f,0.f,0.f) : FVector(0.f,-300.f,0.f));
        FProofCommand Command;
        Command.MatchEpoch = State->Snapshot.MatchEpoch;
        Command.Sequence = NextLocalCommandSequence();
        Command.Type = EProofCommandType::Move;
        Command.UnitId = Unit->UnitId;
        Command.Destination = FacingPreflightGoal;
        FacingPreflightPending = Command.Sequence;
        ServerSubmitCommand(Command);
        UE_LOG(LogTemp, Display, TEXT("PROOF_FACING_PREFLIGHT_MOVE step=%d unit=%d sequence=%d x=%.17g y=%.17g"),
            FacingPreflightStep, Unit->UnitId, Command.Sequence, Command.Destination.X, Command.Destination.Y);
        ++FacingPreflightStep;
        return true;
    }
    // Direction is checked offline against the independent authoritative
    // before/after Advance trace, not a request-time cardinal assumption.
    if (FacingPreflightStep == 1 || FacingPreflightStep == 3)
    {
        if (FVector::DistSquared(Unit->Position, FacingPreflightOrigin) < FMath::Square(120.f)) return true;
        ++FacingPreflightStep;
        return true;
    }
    if (FacingPreflightStep == 4)
    {
        if (!Unit->Position.Equals(FacingPreflightGoal, 0.01f)) return true;
        if (FacingPreflightStationaryAt < 0.0) FacingPreflightStationaryAt = FPlatformTime::Seconds();
        if (FPlatformTime::Seconds() - FacingPreflightStationaryAt < 1.2) return true;
        ++FacingPreflightStep;
        UE_LOG(LogTemp, Display, TEXT("PROOF_FACING_PREFLIGHT status=pass unit=%d epoch=1 stationarySeconds=%.6f"),
            Unit->UnitId, FPlatformTime::Seconds() - FacingPreflightStationaryAt);
    }
    return true;
}
