#include "Runtime/ProofFactoryRallyEvidence.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

bool FProofFactoryRallyEvidence::WantsCommand(int32 Owner, EProofCommandType Type) const
{
    return Owner == 1 && !bProduced && FParse::Param(FCommandLine::Get(), TEXT("ProofFactoryRallyEvidence"))
        && (Type == EProofCommandType::SetRally || (Factory != INDEX_NONE && Type == EProofCommandType::Produce));
}

void FProofFactoryRallyEvidence::ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (!WantsCommand(Owner, Command.Type)) return;
    const auto Fail = [](const TCHAR* Reason)
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_FACTORY_RALLY_ERROR reason=%s"), Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    if (++Rows > 4 || !Result.bAccepted || Epoch <= 0
        || !FParse::Param(FCommandLine::Get(), TEXT("ProofRefineryMinerEvidence")))
    { Fail(TEXT("command-or-observation-contract")); return; }
    const FProofUnitRecord* BeforeFactory = Before.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; });
    if (!BeforeFactory || BeforeFactory->OwnerId != Owner || BeforeFactory->Kind != EProofUnitKind::Factory || BeforeFactory->HitPoints <= 0.f)
    { Fail(TEXT("factory-identity")); return; }
    FProofSnapshot Expected = Before;
    const FProofPlayerRecord* BeforePlayer = Before.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; });
    const FProofPlayerRecord* AfterPlayer = After.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; });
    if (!BeforePlayer || !AfterPlayer) { Fail(TEXT("player-identity")); return; }
    int32 NewUnit = INDEX_NONE;
    FVector Spawn = FVector::ZeroVector;
    if (Command.Type == EProofCommandType::SetRally)
    {
        if (Factory != INDEX_NONE || After.Revision != static_cast<int64>(Before.Revision) + 1)
        { Fail(TEXT("rally-revision-or-duplicate")); return; }
        FProofUnitRecord* Changed = Expected.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; });
        Changed->bHasRallyPoint = true;
        Changed->RallyPoint = Command.Destination;
        Factory = Command.UnitId;
    }
    else
    {
        if (Command.UnitId != Factory || !BeforeFactory->bHasRallyPoint
            || After.Revision != static_cast<int64>(Before.Revision) + 2
            || After.Units.Num() != Before.Units.Num() + 1
            || !FProofCommandModel::FindSkirmishSpawn(Before, BeforeFactory->Position, Spawn))
        { Fail(TEXT("production-contract")); return; }
        const FProofUnitRecord* Produced = After.Units.FindByPredicate([&Before](const FProofUnitRecord& U)
            { return !Before.Units.ContainsByPredicate([&U](const FProofUnitRecord& Old) { return Old.UnitId == U.UnitId; }); });
        if (!Produced || Produced->OwnerId != Owner || Produced->Kind != EProofUnitKind::Combat || Produced->HitPoints != 100.f
            || Produced->Cargo != 0 || Produced->Position != Spawn || Produced->bHasRallyPoint || Produced->RallyPoint != FVector::ZeroVector)
        { Fail(TEXT("produced-unit")); return; }
        NewUnit = Produced->UnitId;
        Expected.Units.Add(*Produced);
        Expected.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; })->Credits -= FProofCommandModel::ProductionCost;
        bProduced = true;
    }
    Expected.Revision = After.Revision;
    if (Expected.ComputeDigest() != After.ComputeDigest()) { Fail(TEXT("unexpected-snapshot-mutation")); return; }
    FString BeforeIds, AfterIds;
    for (const FProofUnitRecord& U : Before.Units) { if (!BeforeIds.IsEmpty()) BeforeIds += TEXT(","); BeforeIds += FString::FromInt(U.UnitId); }
    for (const FProofUnitRecord& U : After.Units) { if (!AfterIds.IsEmpty()) AfterIds += TEXT(","); AfterIds += FString::FromInt(U.UnitId); }
    const FVector Point = Command.Type == EProofCommandType::SetRally ? Command.Destination : BeforeFactory->RallyPoint;
    // Model snapshots are epoch-neutral; published/replicated states are stamped
    // by GameMode. Attribute emitted digests to that same actual match epoch.
    FProofSnapshot PublishedBefore = Before, PublishedAfter = After;
    PublishedBefore.MatchEpoch = PublishedAfter.MatchEpoch = Epoch;
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_RALLY_EVENT {\"type\":\"%s\",\"owner\":%d,\"epoch\":%d,\"sequence\":%d,\"factory\":%d,\"unit\":%d,\"beforeRevision\":%d,\"revision\":%d,\"beforeDigest\":\"%s\",\"digest\":\"%s\",\"creditsBefore\":%d,\"creditsAfter\":%d,\"rallyX\":%.17g,\"rallyY\":%.17g,\"rallyZ\":%.17g,\"spawnX\":%.17g,\"spawnY\":%.17g,\"spawnZ\":%.17g,\"beforeIds\":[%s],\"afterIds\":[%s]}"),
        Command.Type == EProofCommandType::SetRally ? TEXT("SetRally") : TEXT("Produce"), Owner, Epoch, Command.Sequence,
        Command.UnitId, NewUnit, Before.Revision, After.Revision, *PublishedBefore.ComputeDigest(), *PublishedAfter.ComputeDigest(),
        BeforePlayer->Credits, AfterPlayer->Credits, Point.X, Point.Y, Point.Z, Spawn.X, Spawn.Y, Spawn.Z, *BeforeIds, *AfterIds);
}
