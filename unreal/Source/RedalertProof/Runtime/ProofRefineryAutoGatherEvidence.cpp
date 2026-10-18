#include "Runtime/ProofRefineryAutoGatherEvidence.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

namespace
{
const FProofUnitRecord* AutoUnit(const FProofSnapshot& S, int32 Id)
{ return S.Units.FindByPredicate([Id](const FProofUnitRecord& U) { return U.UnitId == Id; }); }
const FProofPlayerRecord* AutoPlayer(const FProofSnapshot& S, int32 Id)
{ return S.Players.FindByPredicate([Id](const FProofPlayerRecord& P) { return P.OwnerId == Id; }); }
const FProofResourceRecord* AutoOre(const FProofSnapshot& S, int32 Id)
{ return S.Resources.FindByPredicate([Id](const FProofResourceRecord& R) { return R.ResourceId == Id; }); }
const FProofUnitRecord* AutoDepot(const FProofSnapshot& S, const FProofUnitRecord& Miner)
{ return S.Units.FindByPredicate([&Miner](const FProofUnitRecord& U) { return U.Kind == EProofUnitKind::Refinery
    && U.OwnerId == Miner.OwnerId && U.HitPoints > 0.f && FVector::DistSquared(U.Position, Miner.Position) <= 1.0; }); }
bool AutoFinite(const FVector& V)
{ return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z); }
bool AutoToken(const FString& S, int32 Length)
{ if (S.Len() != Length) return false; for (TCHAR C : S) if (!FChar::IsHexDigit(C)) return false; return true; }
int64 AutoTotal(const FProofSnapshot& S)
{
    int64 Total = 0;
    for (const auto& R : S.Resources) { if (R.Remaining < 0) return -1; Total += R.Remaining; }
    for (const auto& U : S.Units) { if (U.Cargo < 0 || U.Cargo > FProofCommandModel::CargoCapacity) return -1; Total += U.Cargo; }
    for (const auto& P : S.Players) { if (P.TotalGathered < 0) return -1; Total += P.TotalGathered; }
    return Total;
}
}

FProofRefineryAutoGatherEvidence::FProofRefineryAutoGatherEvidence()
{
    bEnabled = FParse::Param(FCommandLine::Get(), TEXT("ProofRefineryAutoGather"));
    if (!bEnabled) return;
    FParse::Value(FCommandLine::Get(), TEXT("ProofAutoGatherRun="), Run);
    FParse::Value(FCommandLine::Get(), TEXT("ProofAutoGatherSource="), Source);
    if (!AutoToken(Run, 32) || !AutoToken(Source, 40)) Fail(TEXT("invalid-run-source-binding"));
}
void FProofRefineryAutoGatherEvidence::Fail(const TCHAR* Reason)
{
    if (bFailed) return;
    bFailed = true;
    UE_LOG(LogTemp, Error, TEXT("PROOF_AUTO_GATHER_ERROR reason=%s"), Reason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
void FProofRefineryAutoGatherEvidence::EnterEpoch(int32 Epoch)
{
    if (Epoch <= 0 || (TrustedEpoch != 0 && TrustedEpoch != Epoch)) { Fail(TEXT("unexpected-epoch")); return; }
    TrustedEpoch = Epoch;
}
void FProofRefineryAutoGatherEvidence::Row(const TCHAR* Event, const TCHAR* Role, int32 Viewer, const FString& Fields)
{
    if (!bEnabled || bFailed || bFinished) return;
    FString Json = FString::Printf(TEXT("{\"v\":1,\"run\":\"%s\",\"source\":\"%s\",\"role\":\"%s\",\"viewer\":%d,\"row\":%d,\"event\":\"%s\",\"epoch\":%d%s}"),
        *Run, *Source, Role, Viewer, ++Rows, Event, TrustedEpoch, *Fields);
    Bytes += Json.Len();
    if (Rows > 4096 || Json.Len() > 65536 || Bytes > 16 * 1024 * 1024) { Fail(TEXT("stream-cap")); return; }
    UE_LOG(LogTemp, Display, TEXT("PROOF_AUTO_GATHER %s"), *Json);
}
FString FProofRefineryAutoGatherEvidence::SnapshotJson(FProofSnapshot S, int32 Epoch)
{
    S.MatchEpoch = Epoch; // Raw model epoch is zero. Only GameMode supplies this authority.
    S.Players.Sort([](const FProofPlayerRecord& A, const FProofPlayerRecord& B) { return A.OwnerId < B.OwnerId; });
    S.Units.Sort([](const FProofUnitRecord& A, const FProofUnitRecord& B) { return A.UnitId < B.UnitId; });
    S.Resources.Sort([](const FProofResourceRecord& A, const FProofResourceRecord& B) { return A.ResourceId < B.ResourceId; });
    FString J = FString::Printf(TEXT("{\"revision\":%d,\"epoch\":%d,\"skirmish\":%s,\"phase\":%d,\"winner\":%d,\"digest\":\"%s\",\"players\":["),
        S.Revision, S.MatchEpoch, S.bSkirmish ? TEXT("true") : TEXT("false"), static_cast<int32>(S.MatchPhase), S.WinnerTeam, *S.ComputeDigest());
    bool First = true;
    for (const auto& P : S.Players)
    { if (!First) J += TEXT(","); First = false; J += FString::Printf(TEXT("{\"id\":%d,\"credits\":%d,\"team\":%d,\"gathered\":%d}"), P.OwnerId, P.Credits, P.TeamId, P.TotalGathered); }
    J += TEXT("],\"units\":["); First = true;
    for (const auto& U : S.Units)
    {
        if (!First) J += TEXT(","); First = false;
        J += FString::Printf(TEXT("{\"id\":%d,\"owner\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"dx\":%.17g,\"dy\":%.17g,\"dz\":%.17g,\"hp\":%.17g,\"kind\":%d,\"cargo\":%d,\"rally\":%s,\"rx\":%.17g,\"ry\":%.17g,\"rz\":%.17g}"),
            U.UnitId, U.OwnerId, U.Position.X, U.Position.Y, U.Position.Z, U.LastTravelDirection.X, U.LastTravelDirection.Y, U.LastTravelDirection.Z,
            static_cast<double>(U.HitPoints), static_cast<int32>(U.Kind), U.Cargo, U.bHasRallyPoint ? TEXT("true") : TEXT("false"), U.RallyPoint.X, U.RallyPoint.Y, U.RallyPoint.Z);
    }
    J += TEXT("],\"resources\":["); First = true;
    for (const auto& R : S.Resources)
    { if (!First) J += TEXT(","); First = false; J += FString::Printf(TEXT("{\"id\":%d,\"owner\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"remaining\":%d}"), R.ResourceId, R.OwnerId, R.Position.X, R.Position.Y, R.Position.Z, R.Remaining); }
    return J + TEXT("]}");
}
void FProofRefineryAutoGatherEvidence::ObserveCommand(int32 Epoch, int32 Owner, const FProofCommand& C,
    const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofCommandModel& Model)
{
    if (!bEnabled || bFailed || bFinished) return;
    EnterEpoch(Epoch);
    const FProofSnapshot After = Model.GetSnapshot();
    FString GrantJson = TEXT("null");
    for (const FGrant& G : Grants)
        if (!G.bDeposited && Result.bAccepted && C.UnitId == G.Miner)
        { Fail(TEXT("accepted-tracked-miner-override")); return; }
    if (Result.bAccepted && C.Type == EProofCommandType::BuildRefinery && (Owner == 1 || Owner == 2))
    {
        if (Grants.ContainsByPredicate([Owner](const FGrant& G) { return G.Owner == Owner; })) { Fail(TEXT("duplicate-owner-grant")); return; }
        const FProofUnitRecord* Miner = nullptr; const FProofUnitRecord* Refinery = nullptr;
        int32 Added = 0;
        for (const auto& U : After.Units) if (!AutoUnit(Before, U.UnitId))
        { ++Added; if (U.Kind == EProofUnitKind::Harvester) Miner = &U; if (U.Kind == EProofUnitKind::Refinery) Refinery = &U; }
        if (Added != 2 || !Miner || !Refinery || Miner->OwnerId != Owner || Refinery->OwnerId != Owner) { Fail(TEXT("grant-pair")); return; }
        // Independent scan, intentionally does not call the production target helper.
        const FProofResourceRecord* Nearest = nullptr;
        for (const auto& R : After.Resources) if (R.ResourceId > 0 && R.Remaining > 0 && AutoFinite(R.Position))
        {
            const double Distance = FVector::DistSquared(Miner->Position, R.Position);
            const double PriorDistance = Nearest ? FVector::DistSquared(Miner->Position, Nearest->Position) : 0.0;
            if (!Nearest || Distance < PriorDistance || (Distance == PriorDistance && R.ResourceId < Nearest->ResourceId)) Nearest = &R;
        }
        const auto* Order = Model.Orders.Find(Miner->UnitId); // Named const-reader friendship, diagnostic composition only.
        if (!Nearest || !Order || Order->Type != EProofCommandType::Gather || Order->TargetUnitId != Nearest->ResourceId
            || Order->bReturning || Order->ResumeResourceId != INDEX_NONE || Order->Destination != FVector::ZeroVector)
        { Fail(TEXT("actual-initial-order")); return; }
        FGrant G; G.Owner = Owner; G.Miner = Miner->UnitId; G.Refinery = Refinery->UnitId; G.Resource = Nearest->ResourceId; Grants.Add(G);
        GrantJson = FString::Printf(TEXT("{\"miner\":%d,\"refinery\":%d,\"resource\":%d,\"orderType\":%d,\"orderTarget\":%d,\"returning\":false,\"resume\":%d,\"ox\":%.17g,\"oy\":%.17g,\"oz\":%.17g}"),
            G.Miner, G.Refinery, G.Resource, static_cast<int32>(Order->Type), Order->TargetUnitId, Order->ResumeResourceId, Order->Destination.X, Order->Destination.Y, Order->Destination.Z);
    }
    Row(TEXT("command"), TEXT("host"), 0, FString::Printf(TEXT(",\"owner\":%d,\"type\":%d,\"unit\":%d,\"target\":%d,\"sequence\":%d,\"payloadEpoch\":%d,\"cx\":%.17g,\"cy\":%.17g,\"cz\":%.17g,\"accepted\":%s,\"grant\":%s,\"before\":%s,\"after\":%s"),
        Owner, static_cast<int32>(C.Type), C.UnitId, C.TargetUnitId, C.Sequence, C.MatchEpoch, C.Destination.X, C.Destination.Y, C.Destination.Z,
        Result.bAccepted ? TEXT("true") : TEXT("false"), *GrantJson, *SnapshotJson(Before, Epoch), *SnapshotJson(After, Epoch)));
}
void FProofRefineryAutoGatherEvidence::ObserveAdvance(int32 Epoch, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    if (!bEnabled || bFailed || bFinished) return;
    EnterEpoch(Epoch);
    for (FGrant& G : Grants)
    {
        if (G.bDeposited) continue;
        const auto* A = AutoUnit(Before, G.Miner); const auto* B = AutoUnit(After, G.Miner);
        const auto* OreA = AutoOre(Before, G.Resource); const auto* OreB = AutoOre(After, G.Resource);
        if (!A || !B || A->HitPoints <= 0.f || B->HitPoints <= 0.f || A->OwnerId != G.Owner || B->OwnerId != G.Owner
            || !OreA || !OreB || AutoTotal(Before) < 0 || AutoTotal(Before) != AutoTotal(After)) { Fail(TEXT("advance-identity-or-conservation")); return; }
        const FString Fields = FString::Printf(TEXT(",\"owner\":%d,\"miner\":%d,\"refinery\":%d,\"resource\":%d,\"before\":%s,\"after\":%s"),
            G.Owner, G.Miner, G.Refinery, G.Resource, *SnapshotJson(Before, Epoch), *SnapshotJson(After, Epoch));
        if (!G.bTravel && A->Position != B->Position && B->Cargo == 0
            && FVector::DistSquared(B->Position, OreB->Position) < FVector::DistSquared(A->Position, OreB->Position))
        { G.bTravel = true; Row(TEXT("travel"), TEXT("host"), 0, Fields); }
        if (!G.bLoaded && B->Cargo > A->Cargo)
        {
            if (!G.bTravel || A->Cargo != 0 || OreA->Remaining - OreB->Remaining < B->Cargo - A->Cargo
                || FVector::DistSquared(B->Position, OreB->Position) > 1.0) { Fail(TEXT("load-attribution")); return; }
            G.bLoaded = true; Row(TEXT("load"), TEXT("host"), 0, Fields);
        }
        if (G.bLoaded && !G.bReturn && A->Cargo > 0 && A->Position != B->Position)
        {
            double BeforeDistance = TNumericLimits<double>::Max(), AfterDistance = TNumericLimits<double>::Max();
            for (const auto& U : After.Units) if (U.OwnerId == G.Owner && U.Kind == EProofUnitKind::Refinery && U.HitPoints > 0.f)
            { BeforeDistance = FMath::Min(BeforeDistance, FVector::DistSquared(A->Position, U.Position)); AfterDistance = FMath::Min(AfterDistance, FVector::DistSquared(B->Position, U.Position)); }
            if (AfterDistance < BeforeDistance) { G.bReturn = true; Row(TEXT("return"), TEXT("host"), 0, Fields); }
        }
        if (G.bLoaded && A->Cargo > 0 && B->Cargo == 0)
        {
            int32 Sum = 0;
            for (const auto& U : Before.Units)
            {
                const auto* V = AutoUnit(After, U.UnitId);
                if (U.OwnerId == G.Owner && U.Kind == EProofUnitKind::Harvester && U.HitPoints > 0.f && U.Cargo > 0
                    && V && V->OwnerId == G.Owner && V->Kind == EProofUnitKind::Harvester && V->HitPoints > 0.f && U.Cargo > V->Cargo && AutoDepot(After, *V)) Sum += U.Cargo - V->Cargo;
            }
            const auto* P = AutoPlayer(Before, G.Owner); const auto* Q = AutoPlayer(After, G.Owner);
            if (!G.bReturn || !AutoDepot(After, *B) || !P || !Q || Q->Credits - P->Credits != Sum || Q->TotalGathered - P->TotalGathered != Sum || Sum < A->Cargo)
            { Fail(TEXT("deposit-attribution")); return; }
            G.bDeposited = true; Row(TEXT("deposit"), TEXT("host"), 0, Fields);
        }
    }
}
void FProofRefineryAutoGatherEvidence::LogSnapshot(const FProofSnapshot& S, const TCHAR* Role, int32 Viewer)
{
    if (!bEnabled || bFailed || bFinished || S.MatchEpoch <= 0 || !S.bSkirmish || LastRevision == S.Revision) return;
    EnterEpoch(S.MatchEpoch);
    if (++SnapshotRows > 1536) { Fail(TEXT("snapshot-cap")); return; }
    LastRevision = S.Revision;
    Row(TEXT("snapshot"), Role, Viewer, FString::Printf(TEXT(",\"state\":%s"), *SnapshotJson(S, S.MatchEpoch)));
}
void FProofRefineryAutoGatherEvidence::Finish(int32 Epoch, const TCHAR* Role, int32 Viewer)
{
    if (!bEnabled || bFailed || bFinished) return;
    EnterEpoch(Epoch);
    if (Viewer == 0)
    {
        if (Grants.Num() != 2 || !Grants.ContainsByPredicate([](const FGrant& G) { return G.Owner == 1 && G.bDeposited; })
            || !Grants.ContainsByPredicate([](const FGrant& G) { return G.Owner == 2 && G.bDeposited; })) { Fail(TEXT("incomplete-human-trips")); return; }
    }
    Row(TEXT("finish"), Role, Viewer, FString::Printf(TEXT(",\"passed\":true,\"records\":%d,\"snapshots\":%d"), Rows + 1, SnapshotRows));
    bFinished = true;
}
