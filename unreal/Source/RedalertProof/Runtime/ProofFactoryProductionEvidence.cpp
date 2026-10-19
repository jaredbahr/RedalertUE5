#include "Runtime/ProofFactoryProductionEvidence.h"
#include "Runtime/ProofProductionAccounting.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

namespace
{
void FailProduction(const TCHAR* Reason)
{
    UE_LOG(LogTemp, Error, TEXT("PROOF_FACTORY_PRODUCTION_ERROR reason=%s"), Reason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
void ProductionEvent(const TCHAR* Type, int32 Epoch, int32 Sequence, int32 Factory, int32 Unit,
    const FVector& Rally, const FVector& Spawn, const FProofSnapshot& Before, const FProofSnapshot& After,
    int32 Payment = 0, int32 Refund = 0, int32 AdvanceCount = 0)
{
    const FString CompletionCount = AdvanceCount > 0 ? FString::Printf(TEXT(",\"advanceCount\":%d"), AdvanceCount) : FString();
    FString BeforeIds, AfterIds, Eligible;
    for (const FProofUnitRecord& U : Before.Units)
    {
        if (!BeforeIds.IsEmpty()) { BeforeIds += TEXT(","); } BeforeIds += FString::FromInt(U.UnitId);
        if (U.OwnerId == 1 && U.Kind == EProofUnitKind::Factory && U.HitPoints > 0.f)
        { if (!Eligible.IsEmpty()) { Eligible += TEXT(","); } Eligible += FString::FromInt(U.UnitId); }
    }
    for (const FProofUnitRecord& U : After.Units) { if (!AfterIds.IsEmpty()) { AfterIds += TEXT(","); } AfterIds += FString::FromInt(U.UnitId); }
    FProofSnapshot A = Before, B = After; A.MatchEpoch = B.MatchEpoch = Epoch;
    const FProofPlayerRecord* PA = ProofProductionAccounting::Player(Before, 1);
    const FProofPlayerRecord* PB = ProofProductionAccounting::Player(After, 1);
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_PRODUCTION_EVENT {\"type\":\"%s\",\"owner\":1,\"epoch\":%d,\"sequence\":%d,\"factory\":%d,\"unit\":%d,\"beforeRevision\":%d,\"revision\":%d,\"beforeDigest\":\"%s\",\"digest\":\"%s\",\"creditsBefore\":%d,\"creditsAfter\":%d,\"rallyX\":%.17g,\"rallyY\":%.17g,\"rallyZ\":%.17g,\"spawnX\":%.17g,\"spawnY\":%.17g,\"spawnZ\":%.17g,\"beforeIds\":[%s],\"afterIds\":[%s],\"eligible\":[%s],\"productionPayment\":%d,\"productionRefund\":%d,\"productionBefore\":%s,\"productionAfter\":%s%s}"),
        Type, Epoch, Sequence, Factory, Unit, Before.Revision, After.Revision, *A.ComputeDigest(), *B.ComputeDigest(), PA->Credits, PB->Credits,
        Rally.X, Rally.Y, Rally.Z, Spawn.X, Spawn.Y, Spawn.Z, *BeforeIds, *AfterIds, *Eligible, Payment, Refund,
        *ProofProductionAccounting::ContextJson(Before), *ProofProductionAccounting::ContextJson(After), *CompletionCount);
}
}
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
    if (!Result.bAccepted || Result.Sequence != Command.Sequence || Epoch <= 0 || !Before.bSkirmish
        || !FParse::Param(FCommandLine::Get(), TEXT("ProofRefineryMinerEvidence")))
    { FailProduction(TEXT("command-context")); return; }
    FProofSnapshot Expected = Before;
    const FProofPlayerRecord* Prior = ProofProductionAccounting::Player(Before, Owner);
    const FProofPlayerRecord* Current = ProofProductionAccounting::Player(After, Owner);
    if (!Prior || !Current) { FailProduction(TEXT("player")); return; }
    const FProofUnitRecord* Factory = ProofProductionAccounting::Unit(Before, Command.UnitId);
    const bool Build = Command.Type == EProofCommandType::BuildFactory;
    const bool Produce = Command.Type == EProofCommandType::Produce;
    if (!Build && (!Factory || Factory->OwnerId != Owner || Factory->HitPoints <= 0.f || Factory->Kind != EProofUnitKind::Factory))
    { FailProduction(TEXT("source")); return; }
    int32 Unit = INDEX_NONE; FVector Spawn = FVector::ZeroVector; FVector Rally = Command.Destination;
    if (Build)
    {
        const FProofUnitRecord* Added = nullptr; int32 AddedCount = 0;
        for (const FProofUnitRecord& U : After.Units)
            if (!ProofProductionAccounting::Unit(Before, U.UnitId)) { Added = &U; ++AddedCount; }
        if (!Added || AddedCount != 1 || After.Units.Num() != Before.Units.Num() + 1
            || Added->OwnerId != Owner || Added->Kind != EProofUnitKind::Factory || Added->HitPoints != 200.f
            || Added->Cargo != 0 || Added->bHasRallyPoint || Added->RallyPoint != FVector::ZeroVector
            || Prior->Credits - Current->Credits != 150 || Added->Position != Command.Destination
            || After.Revision != static_cast<int64>(Before.Revision) + 2)
        { FailProduction(TEXT("paid-build")); return; }
        Unit = Added->UnitId; Spawn = Command.Destination; ++Builds;
        Expected.Units.Add(*Added);
        Expected.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; })->Credits -= 150;
    }
    else if (Produce)
    {
        if (Builds != 2 || Rallies != 2 || PendingSequence != 0 || !Factory->bHasRallyPoint
            || ProofProductionAccounting::Job(Before, Owner) || After.Revision != static_cast<int64>(Before.Revision) + 1)
        { FailProduction(TEXT("production-start")); return; }
        FProofProductionJob J; J.OwnerId = Owner; J.StartSequence = Command.Sequence; J.FactoryId = Command.UnitId;
        Expected.ProductionJobs.Add(J);
        Expected.ProductionJobs.Sort([](const FProofProductionJob& A, const FProofProductionJob& B) { return A.OwnerId < B.OwnerId; });
        Rally = Factory->RallyPoint;
        PendingSequence = Command.Sequence; PendingFactory = Command.UnitId; PendingEpoch = Epoch; AdvanceRows = 0; ++Produces;
    }
    else
    {
        if (Builds != 2 || After.Revision != static_cast<int64>(Before.Revision) + 1) { FailProduction(TEXT("rally-revision")); return; }
        FProofUnitRecord* Changed = Expected.Units.FindByPredicate([&Command](const FProofUnitRecord& U) { return U.UnitId == Command.UnitId; });
        Changed->bHasRallyPoint = true; Changed->RallyPoint = Command.Destination; ++Rallies;
    }
    Expected.Revision = After.Revision;
    if (Expected.ComputeDigest() != After.ComputeDigest()) { FailProduction(TEXT("unexpected-snapshot-mutation")); return; }
    ProductionEvent(Build ? TEXT("BuildFactory") : Produce ? TEXT("Produce") : TEXT("SetRally"),
        Epoch, Command.Sequence, Build ? Unit : Command.UnitId, Unit, Rally, Spawn, Before, After);
}
void FProofFactoryProductionEvidence::ObserveAdvance(int32 Epoch, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (PendingSequence == 0) return;
    const FProofProductionJob* A = ProofProductionAccounting::Job(Before, 1);
    int32 Payment = 0, Refund = 0;
    if (++AdvanceRows > 2200 || Epoch != PendingEpoch || !A || A->StartSequence != PendingSequence || A->FactoryId != PendingFactory
        || !ProofProductionAccounting::ValidateAdvance(Before, After, 1, Payment, Refund))
    { FailProduction(TEXT("production-advance")); return; }
    FProofSnapshot PublishedBefore = Before, PublishedAfter = After; PublishedBefore.MatchEpoch = PublishedAfter.MatchEpoch = Epoch;
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_PRODUCTION_ADVANCE {\"index\":%d,\"owner\":1,\"epoch\":%d,\"sequence\":%d,\"factory\":%d,\"beforeRevision\":%d,\"revision\":%d,\"beforeDigest\":\"%s\",\"digest\":\"%s\",\"productionPayment\":%d,\"productionRefund\":%d,\"productionBefore\":%s,\"productionAfter\":%s}"),
        AdvanceRows, Epoch, PendingSequence, PendingFactory, Before.Revision, After.Revision, *PublishedBefore.ComputeDigest(), *PublishedAfter.ComputeDigest(), Payment, Refund,
        *ProofProductionAccounting::ContextJson(Before), *ProofProductionAccounting::ContextJson(After));
    if (ProofProductionAccounting::Job(After, 1)) return;
    const FProofUnitRecord* U = After.Units.FindByPredicate([&Before](const FProofUnitRecord& V) { return V.OwnerId == 1 && !ProofProductionAccounting::Unit(Before, V.UnitId); });
    const FProofUnitRecord* F = ProofProductionAccounting::Unit(After, PendingFactory);
    if (!U || !F || F->HitPoints <= 0.f || !F->bHasRallyPoint || Refund != 0)
    { FailProduction(TEXT("production-completion")); return; }
    ProductionEvent(TEXT("Complete"), Epoch, PendingSequence, PendingFactory, U->UnitId, F->RallyPoint, U->Position, Before, After, Payment, Refund, AdvanceRows);
    PendingSequence = 0;
}
