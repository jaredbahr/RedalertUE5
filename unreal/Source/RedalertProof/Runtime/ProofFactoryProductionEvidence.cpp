#include "Runtime/ProofFactoryProductionEvidence.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

bool FProofFactoryProductionEvidence::WantsCommand(int32 Owner, EProofCommandType Type) const
{
    return Owner == 1 && FParse::Param(FCommandLine::Get(), TEXT("ProofFactoryProductionEvidence"))
        && ((Type == EProofCommandType::BuildFactory && Builds < 2)
            || (Type == EProofCommandType::SetRally && Rallies < 2)
            || (Type == EProofCommandType::Produce && Produces < 3));
}

void FProofFactoryProductionEvidence::ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (!WantsCommand(Owner, Command.Type)) return;
    const auto Fail = [](const TCHAR* Reason)
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_FACTORY_PRODUCTION_ERROR reason=%s"), Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    if (!Result.bAccepted || Epoch <= 0 || !Before.bSkirmish
        || !FParse::Param(FCommandLine::Get(), TEXT("ProofRefineryMinerEvidence")))
    { Fail(TEXT("command-context")); return; }
    FProofSnapshot Expected = Before;
    const FProofPlayerRecord* Prior = Before.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; });
    const FProofPlayerRecord* Current = After.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; });
    if (!Prior || !Current) { Fail(TEXT("player")); return; }
    const FProofUnitRecord* Factory = Before.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; });
    const bool bBuild = Command.Type == EProofCommandType::BuildFactory;
    const bool bProduce = Command.Type == EProofCommandType::Produce;
    if (!bBuild && (!Factory || Factory->OwnerId != Owner || Factory->HitPoints <= 0.f || Factory->Kind != EProofUnitKind::Factory))
    { Fail(TEXT("source")); return; }
    int32 Unit = INDEX_NONE;
    FVector Spawn = FVector::ZeroVector;
    FVector Rally = Command.Destination;
    if (bBuild || bProduce)
    {
        const FProofUnitRecord* Added = nullptr;
        int32 AddedCount = 0;
        for (const FProofUnitRecord& U : After.Units)
            if (!Before.Units.ContainsByPredicate([&U](const FProofUnitRecord& Old) { return Old.UnitId == U.UnitId; })) { Added = &U; ++AddedCount; }
        const int32 Cost = bBuild ? FProofCommandModel::FactoryCost : FProofCommandModel::ProductionCost;
        if (!Added || AddedCount != 1 || After.Units.Num() != Before.Units.Num() + 1
            || Added->OwnerId != Owner || Added->Kind != (bBuild ? EProofUnitKind::Factory : EProofUnitKind::Combat)
            || Added->HitPoints != (bBuild ? 200.f : 100.f) || Added->Cargo != 0 || Added->bHasRallyPoint
            || Added->RallyPoint != FVector::ZeroVector || Prior->Credits - Current->Credits != Cost
            || After.Revision != static_cast<int64>(Before.Revision) + 2)
        { Fail(TEXT("paid-creation")); return; }
        if (bBuild)
        {
            Spawn = Command.Destination;
            if (Added->Position != Spawn) { Fail(TEXT("build-site")); return; }
            ++Builds;
        }
        else
        {
            if (Builds != 2 || Rallies != 2 || !Factory->bHasRallyPoint
                || !FProofCommandModel::FindSkirmishSpawn(Before, Factory->Position, Spawn) || Added->Position != Spawn)
            { Fail(TEXT("production-spawn")); return; }
            Rally = Factory->RallyPoint;
            ++Produces;
        }
        Unit = Added->UnitId;
        Expected.Units.Add(*Added);
        Expected.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; })->Credits -= Cost;
    }
    else
    {
        if (Builds != 2 || After.Revision != static_cast<int64>(Before.Revision) + 1) { Fail(TEXT("rally-revision")); return; }
        FProofUnitRecord* Changed = Expected.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; });
        Changed->bHasRallyPoint = true;
        Changed->RallyPoint = Command.Destination;
        ++Rallies;
    }
    Expected.Revision = After.Revision;
    if (Expected.ComputeDigest() != After.ComputeDigest()) { Fail(TEXT("unexpected-snapshot-mutation")); return; }
    FString BeforeIds, AfterIds, Eligible;
    for (const FProofUnitRecord& U : Before.Units)
    {
        if (!BeforeIds.IsEmpty()) BeforeIds += TEXT(","); BeforeIds += FString::FromInt(U.UnitId);
        if (U.OwnerId == Owner && U.Kind == EProofUnitKind::Factory && U.HitPoints > 0.f)
        { if (!Eligible.IsEmpty()) Eligible += TEXT(","); Eligible += FString::FromInt(U.UnitId); }
    }
    for (const FProofUnitRecord& U : After.Units) { if (!AfterIds.IsEmpty()) AfterIds += TEXT(","); AfterIds += FString::FromInt(U.UnitId); }
    FProofSnapshot PublishedBefore = Before, PublishedAfter = After;
    PublishedBefore.MatchEpoch = PublishedAfter.MatchEpoch = Epoch;
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_PRODUCTION_EVENT {\"type\":\"%s\",\"owner\":%d,\"epoch\":%d,\"sequence\":%d,\"factory\":%d,\"unit\":%d,\"beforeRevision\":%d,\"revision\":%d,\"beforeDigest\":\"%s\",\"digest\":\"%s\",\"creditsBefore\":%d,\"creditsAfter\":%d,\"rallyX\":%.17g,\"rallyY\":%.17g,\"rallyZ\":%.17g,\"spawnX\":%.17g,\"spawnY\":%.17g,\"spawnZ\":%.17g,\"beforeIds\":[%s],\"afterIds\":[%s],\"eligible\":[%s]}"),
        bBuild ? TEXT("BuildFactory") : bProduce ? TEXT("Produce") : TEXT("SetRally"), Owner, Epoch, Command.Sequence,
        bBuild ? Unit : Command.UnitId, Unit, Before.Revision, After.Revision,
        *PublishedBefore.ComputeDigest(), *PublishedAfter.ComputeDigest(), Prior->Credits, Current->Credits,
        Rally.X, Rally.Y, Rally.Z, Spawn.X, Spawn.Y, Spawn.Z, *BeforeIds, *AfterIds, *Eligible);
}