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
void FacingVector(const TSharedRef<FJsonObject>& Row, const TCHAR* Key, const FVector& Value)
{
    Row->SetArrayField(Key, {MakeShared<FJsonValueNumber>(Value.X),
        MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)});
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
    const FVector Expected = FacingPreflightStep == 1 ? FVector(1.f,0.f,0.f) : FVector(0.f,-1.f,0.f);
    if (FacingPreflightStep == 1 || FacingPreflightStep == 3)
    {
        if (FVector::DistSquared(Unit->Position, FacingPreflightOrigin) < FMath::Square(120.f)) return true;
        if (!Unit->LastTravelDirection.Equals(Expected, 0.001f)) { Fail(TEXT("authoritative_direction")); return true; }
        ++FacingPreflightStep;
        return true;
    }
    if (FacingPreflightStep == 4)
    {
        if (!Unit->Position.Equals(FacingPreflightGoal, 0.01f)) return true;
        if (FacingPreflightStationaryAt < 0.0) FacingPreflightStationaryAt = FPlatformTime::Seconds();
        if (!Unit->LastTravelDirection.Equals(Expected, 0.001f)) { Fail(TEXT("stationary_direction")); return true; }
        if (FPlatformTime::Seconds() - FacingPreflightStationaryAt < 1.2) return true;
        ++FacingPreflightStep;
        UE_LOG(LogTemp, Display, TEXT("PROOF_FACING_PREFLIGHT status=pass unit=%d epoch=1 stationarySeconds=%.6f"),
            Unit->UnitId, FPlatformTime::Seconds() - FacingPreflightStationaryAt);
    }
    return true;
}
