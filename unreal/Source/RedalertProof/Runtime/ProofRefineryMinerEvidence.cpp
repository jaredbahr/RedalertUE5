#include "Runtime/ProofRefineryMinerEvidence.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

namespace
{
const FProofUnitRecord* EvidenceUnit(const FProofSnapshot& State, int32 Id)
{ return State.Units.FindByPredicate([Id](const FProofUnitRecord& U) { return U.UnitId == Id; }); }
const FProofPlayerRecord* EvidencePlayer(const FProofSnapshot& State, int32 Id)
{ return State.Players.FindByPredicate([Id](const FProofPlayerRecord& P) { return P.OwnerId == Id; }); }
const FProofResourceRecord* EvidenceOre(const FProofSnapshot& State, int32 Id)
{ return State.Resources.FindByPredicate([Id](const FProofResourceRecord& R) { return R.ResourceId == Id; }); }
FString EvidenceDigest(FProofSnapshot State, int32 Epoch)
{ State.MatchEpoch = Epoch; return State.ComputeDigest(); }
bool EvidenceFinite(const FVector& P)
{ return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z); }
const FProofUnitRecord* DepositRefinery(const FProofSnapshot& State, const FProofUnitRecord& Miner)
{
    return State.Units.FindByPredicate([&Miner](const FProofUnitRecord& U)
    { return U.Kind == EProofUnitKind::Refinery && U.OwnerId == Miner.OwnerId && U.HitPoints > 0.f
        && FVector::DistSquared(U.Position, Miner.Position) <= 1.0; });
}
}
FProofRefineryMinerEvidence::FProofRefineryMinerEvidence()
{ bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofRefineryMinerEvidence")); }
void FProofRefineryMinerEvidence::EnterEpoch(int32 NewEpoch)
{ if (Epoch != NewEpoch) { Epoch = NewEpoch; Grants.Reset(); Rejections = 0; } }
void FProofRefineryMinerEvidence::Fail(const TCHAR* Reason)
{
    if (bFailed) return;
    bFailed = true;
    UE_LOG(LogTemp, Error, TEXT("PROOF_REFINERY_ERROR reason=%s"), Reason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
void FProofRefineryMinerEvidence::ObserveCommand(int32 NewEpoch, int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (!bEnabled || bFailed || !Before.bSkirmish) return;
    EnterEpoch(NewEpoch);
    if (Command.Type == EProofCommandType::BuildRefinery)
    {
        if (!Result.bAccepted)
        {
            if (++Rejections > 32 || Before.ComputeDigest() != After.ComputeDigest())
            { Fail(TEXT("rejected-refinery-mutation-or-limit")); return; }
            UE_LOG(LogTemp, Display, TEXT("PROOF_REFINERY_EVENT {\"event\":\"rejected\",\"epoch\":%d,\"owner\":%d,\"sequence\":%lld,\"beforeDigest\":\"%s\",\"afterDigest\":\"%s\"}"),
                Epoch, Owner, static_cast<long long>(Command.Sequence), *EvidenceDigest(Before, Epoch), *EvidenceDigest(After, Epoch));
            return;
        }
        if (Grants.Num() >= 32) { Fail(TEXT("grant-limit")); return; }
        TSet<int32> BeforeIds, AfterIds;
        for (const FProofUnitRecord& U : Before.Units) BeforeIds.Add(U.UnitId);
        const FProofUnitRecord* Refinery = nullptr; const FProofUnitRecord* Miner = nullptr;
        int32 Added = 0;
        for (const FProofUnitRecord& U : After.Units)
        {
            AfterIds.Add(U.UnitId);
            if (BeforeIds.Contains(U.UnitId)) continue;
            ++Added;
            if (U.OwnerId != Owner || U.HitPoints <= 0.f || U.Cargo != 0) { Fail(TEXT("grant-owner-or-state")); return; }
            if (U.Kind == EProofUnitKind::Refinery && !Refinery) Refinery = &U;
            else if (U.Kind == EProofUnitKind::Harvester && !Miner) Miner = &U;
            else { Fail(TEXT("grant-kinds")); return; }
        }
        const FProofPlayerRecord* Prior = EvidencePlayer(Before, Owner);
        const FProofPlayerRecord* Current = EvidencePlayer(After, Owner);
        if (Added != 2 || !Refinery || !Miner || BeforeIds.Num() != Before.Units.Num() || AfterIds.Num() != After.Units.Num()
            || After.Units.Num() != Before.Units.Num() + 2 || !Prior || !Current
            || Prior->Credits - Current->Credits != FProofCommandModel::RefineryCost
            || Current->TotalGathered != Prior->TotalGathered || Refinery->UnitId <= 0 || Miner->UnitId <= 0
            || !Refinery->Position.Equals(Command.Destination, 0.001) || !EvidenceFinite(Miner->Position)
            || FMath::Abs(Miner->Position.X) > FProofCommandModel::WorldExtent
            || FMath::Abs(Miner->Position.Y) > FProofCommandModel::WorldExtent
            || FMath::Abs(Miner->Position.Z) > FProofCommandModel::WorldExtent
            || FVector::DistSquared(Miner->Position, Refinery->Position) < 260.0 * 260.0)
        { Fail(TEXT("grant-transaction")); return; }
        for (const FProofUnitRecord& U : Before.Units)
            if (U.HitPoints > 0.f && FVector::DistSquared(U.Position, Miner->Position) < 260.0 * 260.0)
            { Fail(TEXT("grant-spawn-separation")); return; }
        FProofSnapshot Expected = Before;
        for (FProofPlayerRecord& P : Expected.Players) if (P.OwnerId == Owner) P.Credits -= FProofCommandModel::RefineryCost;
        Expected.Units.Add(*Refinery); Expected.Units.Add(*Miner); Expected.Revision = After.Revision;
        if (Expected.ComputeDigest() != After.ComputeDigest()) { Fail(TEXT("grant-unrelated-mutation")); return; }
        for (const FGrant& G : Grants)
            if ((G.Owner == Owner && G.Sequence == Command.Sequence) || G.Miner == Miner->UnitId || G.Refinery == Refinery->UnitId)
            { Fail(TEXT("duplicate-grant-identity")); return; }
        FGrant Grant; Grant.Owner = Owner; Grant.Sequence = Command.Sequence; Grant.Refinery = Refinery->UnitId; Grant.Miner = Miner->UnitId;
        Grants.Add(Grant);
        FString PriorIds, CurrentIds;
        for (const FProofUnitRecord& U : Before.Units) { if (!PriorIds.IsEmpty()) PriorIds += TEXT(","); PriorIds += FString::FromInt(U.UnitId); }
        for (const FProofUnitRecord& U : After.Units) { if (!CurrentIds.IsEmpty()) CurrentIds += TEXT(","); CurrentIds += FString::FromInt(U.UnitId); }
        UE_LOG(LogTemp, Display, TEXT("PROOF_REFINERY_EVENT {\"event\":\"grant\",\"epoch\":%d,\"owner\":%d,\"sequence\":%lld,\"refinery\":%d,\"miner\":%d,\"beforeRevision\":%d,\"revision\":%d,\"beforeDigest\":\"%s\",\"digest\":\"%s\",\"added\":2,\"refineriesAdded\":1,\"minersAdded\":1,\"creditsBefore\":%d,\"creditsAfter\":%d,\"cost\":%d,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"rx\":%.6f,\"ry\":%.6f,\"rz\":%.6f,\"beforeIds\":[%s],\"afterIds\":[%s]}"),
            Epoch, Owner, static_cast<long long>(Command.Sequence), Grant.Refinery, Grant.Miner, Before.Revision, After.Revision,
            *EvidenceDigest(Before, Epoch), *EvidenceDigest(After, Epoch), Prior->Credits, Current->Credits, FProofCommandModel::RefineryCost,
            Miner->Position.X, Miner->Position.Y, Miner->Position.Z, Refinery->Position.X, Refinery->Position.Y, Refinery->Position.Z, *PriorIds, *CurrentIds);
        return;
    }
    if (Command.Type == EProofCommandType::Gather && Result.bAccepted)
        for (FGrant& G : Grants) if (G.Owner == Owner && G.Miner == Command.UnitId && !G.bGather)
        {
            const FProofResourceRecord* Ore = EvidenceOre(After, Command.TargetUnitId);
            if (!Ore || Ore->Remaining <= 0) { Fail(TEXT("gather-resource")); return; }
            G.bGather = true; G.Resource = Ore->ResourceId;
            UE_LOG(LogTemp, Display, TEXT("PROOF_REFINERY_EVENT {\"event\":\"gather\",\"epoch\":%d,\"owner\":%d,\"sequence\":%lld,\"refinery\":%d,\"miner\":%d,\"resource\":%d,\"revision\":%d}"),
                Epoch, Owner, static_cast<long long>(Command.Sequence), G.Refinery, G.Miner, G.Resource, After.Revision);
        }
}
void FProofRefineryMinerEvidence::ObserveAdvance(int32 NewEpoch, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (!bEnabled || bFailed) return;
    EnterEpoch(NewEpoch);
    auto ConservedOre = [](const FProofSnapshot& S) -> int64
    {
        int64 Total = 0;
        for (const FProofResourceRecord& R : S.Resources) { if (R.Remaining < 0) return -1; Total += R.Remaining; }
        for (const FProofUnitRecord& U : S.Units) { if (U.Cargo < 0 || U.Cargo > FProofCommandModel::CargoCapacity) return -1; Total += U.Cargo; }
        for (const FProofPlayerRecord& P : S.Players) { if (P.TotalGathered < 0) return -1; Total += P.TotalGathered; }
        return Total;
    };
    const int64 BeforeOre = ConservedOre(Before), AfterOre = ConservedOre(After);
    if (BeforeOre < 0 || AfterOre != BeforeOre) { Fail(TEXT("ore-conservation")); return; }
    for (FGrant& G : Grants)
    {
        if (!G.bGather || G.bDeposited) continue;
        const FProofUnitRecord* A = EvidenceUnit(Before, G.Miner); const FProofUnitRecord* B = EvidenceUnit(After, G.Miner);
        if (!A || !B || A->HitPoints <= 0.f || B->HitPoints <= 0.f || A->OwnerId != G.Owner || B->OwnerId != G.Owner || A->Kind != EProofUnitKind::Harvester || B->Kind != EProofUnitKind::Harvester) continue;
        if (!G.bLoaded && B->Cargo > A->Cargo)
        {
            const FProofResourceRecord* OreA = EvidenceOre(Before, G.Resource); const FProofResourceRecord* OreB = EvidenceOre(After, G.Resource);
            if (!OreA || !OreB || OreA->Remaining - OreB->Remaining < B->Cargo - A->Cargo
                || FVector::DistSquared(B->Position, OreB->Position) > 1.0)
            { Fail(TEXT("load-attribution")); return; }
            G.bLoaded = true;
            UE_LOG(LogTemp, Display, TEXT("PROOF_REFINERY_EVENT {\"event\":\"load\",\"epoch\":%d,\"owner\":%d,\"refinery\":%d,\"miner\":%d,\"resource\":%d,\"revision\":%d,\"cargo\":%d,\"oreBefore\":%d,\"oreAfter\":%d}"),
                Epoch, G.Owner, G.Refinery, G.Miner, G.Resource, After.Revision, B->Cargo, OreA->Remaining, OreB->Remaining);
        }
        if (G.bLoaded && A->Cargo > 0 && B->Cargo == 0)
        {
            const FProofUnitRecord* Depot = DepositRefinery(After, *B);
            const FProofPlayerRecord* Prior = EvidencePlayer(Before, G.Owner); const FProofPlayerRecord* Current = EvidencePlayer(After, G.Owner);
            int32 DepositSum = 0; FString Contributors;
            for (const FProofUnitRecord& U : Before.Units)
            {
                const FProofUnitRecord* V = EvidenceUnit(After, U.UnitId);
                if (U.OwnerId == G.Owner && U.Kind == EProofUnitKind::Harvester && U.HitPoints > 0.f && U.Cargo > 0
                    && V && V->OwnerId == G.Owner && V->Kind == EProofUnitKind::Harvester && V->HitPoints > 0.f && V->Cargo < U.Cargo && DepositRefinery(After, *V))
                {
                    DepositSum += U.Cargo - V->Cargo;
                    if (!Contributors.IsEmpty()) Contributors += TEXT(",");
                    Contributors += FString::Printf(TEXT("{\"miner\":%d,\"before\":%d,\"after\":%d,\"depot\":%d}"), U.UnitId, U.Cargo, V->Cargo, DepositRefinery(After, *V)->UnitId);
                }
            }
            if (!Depot || !Prior || !Current || Current->Credits - Prior->Credits != DepositSum
                || Current->TotalGathered - Prior->TotalGathered != DepositSum || DepositSum < A->Cargo)
            { Fail(TEXT("deposit-attribution")); return; }
            G.bDeposited = true;
            UE_LOG(LogTemp, Display, TEXT("PROOF_REFINERY_EVENT {\"event\":\"deposit\",\"epoch\":%d,\"owner\":%d,\"refinery\":%d,\"depot\":%d,\"miner\":%d,\"revision\":%d,\"cargoBefore\":%d,\"cargoAfter\":0,\"depositSum\":%d,\"creditsBefore\":%d,\"creditsAfter\":%d,\"gatheredBefore\":%d,\"gatheredAfter\":%d,\"contributors\":[%s]}"),
                Epoch, G.Owner, G.Refinery, Depot->UnitId, G.Miner, After.Revision, A->Cargo, DepositSum,
                Prior->Credits, Current->Credits, Prior->TotalGathered, Current->TotalGathered, *Contributors);
        }
    }
}
void FProofRefineryMinerEvidence::LogSnapshot(const FProofSnapshot& State, const TCHAR* Role, int32 Viewer)
{
    if (!bEnabled || bFailed || !State.bSkirmish || State.MatchEpoch <= 0
        || (LastLogEpoch == State.MatchEpoch && LastLogRevision == State.Revision)) return;
    if (++SnapshotRows > 1536) { Fail(TEXT("snapshot-row-limit")); return; }
    LastLogEpoch = State.MatchEpoch; LastLogRevision = State.Revision;
    FString Json = FString::Printf(TEXT("{\"role\":\"%s\",\"viewer\":%d,\"epoch\":%d,\"revision\":%d,\"digest\":\"%s\",\"phase\":%d,\"winner\":%d,\"units\":["),
        Role, Viewer, State.MatchEpoch, State.Revision, *State.ComputeDigest(), static_cast<int32>(State.MatchPhase), State.WinnerTeam);
    bool First = true;
    for (const FProofUnitRecord& U : State.Units)
    {
        if (!EvidenceFinite(U.Position) || !FMath::IsFinite(U.HitPoints)) { Fail(TEXT("nonfinite-snapshot")); return; }
        if (!First) Json += TEXT(","); First = false;
        Json += FString::Printf(TEXT("{\"id\":%d,\"owner\":%d,\"kind\":%d,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"hp\":%.3f,\"cargo\":%d}"),
            U.UnitId, U.OwnerId, static_cast<int32>(U.Kind), U.Position.X, U.Position.Y, U.Position.Z, U.HitPoints, U.Cargo);
    }
    Json += TEXT("],\"players\":["); First = true;
    for (const FProofPlayerRecord& P : State.Players)
    {
        if (!First) Json += TEXT(","); First = false;
        Json += FString::Printf(TEXT("{\"id\":%d,\"team\":%d,\"credits\":%d,\"gathered\":%d}"), P.OwnerId, P.TeamId, P.Credits, P.TotalGathered);
    }
    Json += TEXT("],\"resources\":["); First = true;
    for (const FProofResourceRecord& R : State.Resources)
    {
        if (!EvidenceFinite(R.Position)) { Fail(TEXT("nonfinite-resource")); return; }
        if (!First) Json += TEXT(","); First = false;
        Json += FString::Printf(TEXT("{\"id\":%d,\"owner\":%d,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"remaining\":%d}"), R.ResourceId, R.OwnerId, R.Position.X, R.Position.Y, R.Position.Z, R.Remaining);
    }
    Json += TEXT("]}");
    SnapshotBytes += Json.Len(); // ASCII-only serialized schema; one byte per character.
    if (Json.Len() > 32768 || SnapshotBytes > 8 * 1024 * 1024) { Fail(TEXT("snapshot-byte-limit")); return; }
    UE_LOG(LogTemp, Display, TEXT("PROOF_REFINERY_STATE %s"), *Json);
}
