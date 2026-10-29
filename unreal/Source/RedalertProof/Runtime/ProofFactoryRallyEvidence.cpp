#include "Runtime/ProofFactoryRallyEvidence.h"
#include "Runtime/ProofProductionAccounting.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

namespace
{
void FailRally(const TCHAR* Reason)
{
    UE_LOG(LogTemp, Error, TEXT("PROOF_FACTORY_RALLY_ERROR reason=%s"), Reason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
void RallyComplete( int32 Epoch, int32 Sequence, int32 Factory, int32 Unit,
    const FVector& Rally, const FVector& Spawn, const FProofSnapshot& Before, const FProofSnapshot& After,
    int32 Payment = 0, int32 Refund = 0, int32 AdvanceCount = 0)
{
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
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_RALLY_EVENT {\"type\":\"%s\",\"owner\":1,\"epoch\":%d,\"sequence\":%d,\"factory\":%d,\"unit\":%d,\"beforeRevision\":%d,\"revision\":%d,\"beforeDigest\":\"%s\",\"digest\":\"%s\",\"creditsBefore\":%d,\"creditsAfter\":%d,\"rallyX\":%.17g,\"rallyY\":%.17g,\"rallyZ\":%.17g,\"spawnX\":%.17g,\"spawnY\":%.17g,\"spawnZ\":%.17g,\"beforeIds\":[%s],\"afterIds\":[%s],\"eligible\":[%s],\"productionPayment\":%d,\"productionRefund\":%d,\"productionBefore\":%s,\"productionAfter\":%s,\"advanceCount\":%d}"),
        TEXT("Complete"), Epoch, Sequence, Factory, Unit, Before.Revision, After.Revision, *A.ComputeDigest(), *B.ComputeDigest(), PA->Credits, PB->Credits,
        Rally.X, Rally.Y, Rally.Z, Spawn.X, Spawn.Y, Spawn.Z, *BeforeIds, *AfterIds, *Eligible, Payment, Refund,
        *ProofProductionAccounting::ContextJson(Before), *ProofProductionAccounting::ContextJson(After), AdvanceCount);
}
}
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
    if (++Rows > 4 || !Result.bAccepted || Result.Sequence != Command.Sequence || Epoch <= 0
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
        if (Command.UnitId != Factory || !BeforeFactory->bHasRallyPoint || PendingSequence != 0
            || ProofProductionAccounting::Job(Before, Owner) || After.Revision != static_cast<int64>(Before.Revision) + 1)
        { Fail(TEXT("production-start-contract")); return; }
        FProofProductionJob Job; Job.OwnerId = Owner; Job.StartSequence = Command.Sequence; Job.FactoryId = Command.UnitId;
        Expected.ProductionJobs.Add(Job);
        Expected.ProductionJobs.Sort([](const FProofProductionJob& A, const FProofProductionJob& B) { return A.OwnerId < B.OwnerId; });
        PendingSequence = Command.Sequence; PendingEpoch = Epoch; AdvanceRows = 0;
        bProduced = true;
    }
    Expected.Revision = After.Revision;
    if (Expected.ComputeDigest() != After.ComputeDigest()) { Fail(TEXT("unexpected-snapshot-mutation")); return; }
    FString BeforeIds, AfterIds;
    for (const FProofUnitRecord& U : Before.Units) { if (!BeforeIds.IsEmpty()) { BeforeIds += TEXT(","); } BeforeIds += FString::FromInt(U.UnitId); }
    for (const FProofUnitRecord& U : After.Units) { if (!AfterIds.IsEmpty()) { AfterIds += TEXT(","); } AfterIds += FString::FromInt(U.UnitId); }
    const FVector Point = Command.Type == EProofCommandType::SetRally ? Command.Destination : BeforeFactory->RallyPoint;
    // Model snapshots are epoch-neutral; published/replicated states are stamped
    // by GameMode. Attribute emitted digests to that same actual match epoch.
    FProofSnapshot PublishedBefore = Before, PublishedAfter = After;
    PublishedBefore.MatchEpoch = PublishedAfter.MatchEpoch = Epoch;
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_RALLY_EVENT {\"type\":\"%s\",\"owner\":%d,\"epoch\":%d,\"sequence\":%d,\"factory\":%d,\"unit\":%d,\"beforeRevision\":%d,\"revision\":%d,\"beforeDigest\":\"%s\",\"digest\":\"%s\",\"creditsBefore\":%d,\"creditsAfter\":%d,\"rallyX\":%.17g,\"rallyY\":%.17g,\"rallyZ\":%.17g,\"spawnX\":%.17g,\"spawnY\":%.17g,\"spawnZ\":%.17g,\"beforeIds\":[%s],\"afterIds\":[%s],\"productionPayment\":0,\"productionRefund\":0,\"productionBefore\":%s,\"productionAfter\":%s}"),
        Command.Type == EProofCommandType::SetRally ? TEXT("SetRally") : TEXT("Produce"), Owner, Epoch, Command.Sequence,
        Command.UnitId, NewUnit, Before.Revision, After.Revision, *PublishedBefore.ComputeDigest(), *PublishedAfter.ComputeDigest(),
        BeforePlayer->Credits, AfterPlayer->Credits, Point.X, Point.Y, Point.Z, Spawn.X, Spawn.Y, Spawn.Z, *BeforeIds, *AfterIds, *ProofProductionAccounting::ContextJson(Before), *ProofProductionAccounting::ContextJson(After));
}

void FProofFactoryRallyEvidence::ObserveAdvance(int32 Epoch, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (PendingSequence == 0) return;
    const FProofProductionJob* A = ProofProductionAccounting::Job(Before, 1);
    int32 Payment = 0, Refund = 0;
    if (++AdvanceRows > 2200 || Epoch != PendingEpoch || !A || A->StartSequence != PendingSequence || A->FactoryId != Factory
        || !ProofProductionAccounting::ValidateAdvance(Before, After, 1, Payment, Refund))
    { FailRally(TEXT("production-advance")); return; }
    FProofSnapshot PublishedBefore = Before, PublishedAfter = After; PublishedBefore.MatchEpoch = PublishedAfter.MatchEpoch = Epoch;
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_RALLY_ADVANCE {\"index\":%d,\"owner\":1,\"epoch\":%d,\"sequence\":%d,\"factory\":%d,\"beforeRevision\":%d,\"revision\":%d,\"beforeDigest\":\"%s\",\"digest\":\"%s\",\"productionPayment\":%d,\"productionRefund\":%d,\"productionBefore\":%s,\"productionAfter\":%s}"),
        AdvanceRows, Epoch, PendingSequence, Factory, Before.Revision, After.Revision, *PublishedBefore.ComputeDigest(), *PublishedAfter.ComputeDigest(), Payment, Refund,
        *ProofProductionAccounting::ContextJson(Before), *ProofProductionAccounting::ContextJson(After));
    if (ProofProductionAccounting::Job(After, 1)) return;
    const FProofUnitRecord* U = After.Units.FindByPredicate([&Before](const FProofUnitRecord& V) { return V.OwnerId == 1 && !ProofProductionAccounting::Unit(Before, V.UnitId); });
    const FProofUnitRecord* F = ProofProductionAccounting::Unit(After, Factory);
    if (!U || !F || F->HitPoints <= 0.f || !F->bHasRallyPoint || Refund != 0)
    { FailRally(TEXT("production-completion")); return; }
    RallyComplete( Epoch, PendingSequence, Factory, U->UnitId, F->RallyPoint, U->Position, Before, After, Payment, Refund, AdvanceRows);
    PendingSequence = 0;
}
