#pragma once
#include "Commands/ProofCommandModel.h"

// Read-only evidence oracle. This deliberately reconstructs debit/refund from
// jobs and retained Combat records, never from the observed credit difference.
namespace ProofProductionAccounting
{
inline const FProofProductionJob* Job(const FProofSnapshot& S, int32 Owner)
{ return S.ProductionJobs.FindByPredicate([Owner](const FProofProductionJob& J) { return J.OwnerId == Owner; }); }
inline const FProofPlayerRecord* Player(const FProofSnapshot& S, int32 Owner)
{ return S.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; }); }
inline const FProofUnitRecord* Unit(const FProofSnapshot& S, int32 Id)
{ return S.Units.FindByPredicate([Id](const FProofUnitRecord& U) { return U.UnitId == Id; }); }
inline bool SameJob(const FProofProductionJob& A, const FProofProductionJob& B)
{ return A.OwnerId == B.OwnerId && A.StartSequence == B.StartSequence && A.FactoryId == B.FactoryId
    && A.WorkMicroseconds == B.WorkMicroseconds && A.PaidCredits == B.PaidCredits && A.bHeld == B.bHeld; }
inline bool Valid(const FProofSnapshot& S)
{
    TSet<int32> Players, Units;
    for (const FProofPlayerRecord& P : S.Players)
    {
        if (P.OwnerId <= 0 || Players.Contains(P.OwnerId) || P.Credits < 0 || P.TotalGathered < 0) return false;
        Players.Add(P.OwnerId);
    }
    for (const FProofUnitRecord& U : S.Units)
    {
        if (U.UnitId <= 0 || Units.Contains(U.UnitId) || !Players.Contains(U.OwnerId)
            || !FMath::IsFinite(U.HitPoints) || U.HitPoints < 0.f || U.Cargo < 0 || U.Cargo > 50) return false;
        Units.Add(U.UnitId);
    }
    int32 PreviousOwner = 0;
    for (const FProofProductionJob& J : S.ProductionJobs)
    {
        const FProofUnitRecord* F = Unit(S, J.FactoryId);
        if (J.OwnerId <= PreviousOwner || J.StartSequence <= 0 || !Player(S, J.OwnerId)
            || !F || F->OwnerId != J.OwnerId || F->Kind != EProofUnitKind::Factory
            || J.WorkMicroseconds < 0 || J.WorkMicroseconds > 5000000
            || J.PaidCredits != (J.WorkMicroseconds + 99999) / 100000) return false;
        PreviousOwner = J.OwnerId;
    }
    return true;
}
inline int64 Ledger(const FProofSnapshot& S, int32 Owner)
{
    int64 Paid = 0;
    for (const FProofUnitRecord& U : S.Units)
        if (U.OwnerId == Owner && U.Kind == EProofUnitKind::Combat) Paid += 50;
    if (const FProofProductionJob* J = Job(S, Owner)) Paid += J->PaidCredits;
    return Paid;
}
// Reconstruct the completion-time placement independently of the model helper.
// Movement/combat already happened; earlier-owner completions occupy their exits.
inline bool CompletionSpawn(const FProofSnapshot& Before, const FProofSnapshot& After,
    int32 Owner, const FVector& Origin, FVector& Spawn)
{
    const FVector Directions[] = { FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0),
        FVector(1,1,0), FVector(-1,1,0), FVector(1,-1,0), FVector(-1,-1,0) };
    for (int32 Ring = 1; Ring <= 3; ++Ring)
        for (const FVector& D : Directions)
        {
            const FVector C = Origin + D * (300.0 * Ring);
            if (!FMath::IsFinite(C.X) || !FMath::IsFinite(C.Y) || !FMath::IsFinite(C.Z)
                || FMath::Abs(C.X) > 10000 || FMath::Abs(C.Y) > 10000 || FMath::Abs(C.Z) > 10000) continue;
            bool Clear = true;
            for (const FProofUnitRecord& U : After.Units)
                if (U.HitPoints > 0.f && (Unit(Before, U.UnitId) || U.OwnerId < Owner)
                    && FVector::DistSquared(C, U.Position) < 67600.0) { Clear = false; break; }
            if (Clear) { Spawn = C; return true; }
        }
    return false;
}
inline bool ValidateAdvance(const FProofSnapshot& Before, const FProofSnapshot& After,
    int32 Owner, int32& Payment, int32& Refund, int32 WorkMicroseconds = 100000)
{
    Payment = Refund = 0;
    if (!Valid(Before) || !Valid(After) || WorkMicroseconds < 0 || WorkMicroseconds > 5000000) return false;
    const FProofPlayerRecord* P = Player(Before, Owner);
    if (!P || !Player(After, Owner)) return false;
    for (const FProofUnitRecord& U : Before.Units)
    {
        const FProofUnitRecord* V = Unit(After, U.UnitId);
        if (!V || V->OwnerId != U.OwnerId || V->Kind != U.Kind || V->HitPoints > U.HitPoints) return false;
        if ((U.Kind == EProofUnitKind::Factory || U.Kind == EProofUnitKind::HQ || U.Kind == EProofUnitKind::Refinery)
            && V->Position != U.Position) return false;
        if (U.Kind == EProofUnitKind::Factory && V->HitPoints > 0.f
            && (V->bHasRallyPoint != U.bHasRallyPoint || V->RallyPoint != U.RallyPoint)) return false;
    }
    for (const FProofProductionJob& J : After.ProductionJobs)
    {
        const FProofProductionJob* Old = Job(Before, J.OwnerId);
        if (!Old || Old->StartSequence != J.StartSequence || Old->FactoryId != J.FactoryId || Old->bHeld != J.bHeld) return false;
    }
    int32 Added = 0;
    for (const FProofUnitRecord& U : After.Units)
        if (!Unit(Before, U.UnitId))
        {
            if (!Job(Before, U.OwnerId) || Job(After, U.OwnerId) || U.Kind != EProofUnitKind::Combat || U.HitPoints != 100.f || U.Cargo != 0
                || U.bHasRallyPoint || U.RallyPoint != FVector::ZeroVector
                || U.LastTravelDirection != FVector::ZeroVector || U.bAreaGuard
                || U.AreaGuardPost != FVector::ZeroVector || U.AreaGuardTargetId != INDEX_NONE || U.bAreaGuardReturning) return false;
            if (U.OwnerId == Owner) ++Added;
        }
    const FProofProductionJob* A = Job(Before, Owner);
    const FProofProductionJob* B = Job(After, Owner);
    if (!A && (B || Added != 0)) return false;
    if (A && B && (B->StartSequence != A->StartSequence || B->FactoryId != A->FactoryId || B->bHeld != A->bHeld)) return false;
    const FProofUnitRecord* Factory = A ? Unit(After, A->FactoryId) : nullptr;
    if (!A) { }
    else if (After.MatchPhase != EProofMatchPhase::Playing)
    { if (!B || !SameJob(*A, *B) || Added != 0) return false; }
    else if (!Factory || Factory->OwnerId != Owner || Factory->Kind != EProofUnitKind::Factory || Factory->HitPoints <= 0.f)
    {
        if (Added != 0) return false;
        if (static_cast<int64>(P->Credits) + A->PaidCredits <= MAX_int32)
        { if (B) return false; Refund = A->PaidCredits; }
        else if (!B || !SameJob(*A, *B)) return false;
    }
    else if (A->bHeld)
    { if (!B || !SameJob(*A, *B) || Added != 0) return false; }
    else
    {
        const int32 AffordableWork = (A->PaidCredits + FMath::Min(P->Credits, 50 - A->PaidCredits)) * 100000;
        const int32 NextWork = FMath::Min(A->WorkMicroseconds + WorkMicroseconds, AffordableWork);
        const int32 NextPaid = (NextWork + 99999) / 100000;
        Payment = NextPaid - A->PaidCredits;
        // A paid retained job may be blocked by live capacity, exit placement,
        // or the private monotonic ID allocator. Evidence cannot infer allocator
        // exhaustion from public records after owner removal, so it does not
        // claim to distinguish that final blocker from a retained ready job.
        if (B)
        { if (Added != 0 || B->WorkMicroseconds != NextWork || B->PaidCredits != NextPaid) return false; }
        else
        {
            if (Added != 1 || NextWork != 5000000 || NextPaid != 50) return false;
            const FProofUnitRecord* NewUnit = After.Units.FindByPredicate([&Before, Owner](const FProofUnitRecord& U)
                { return U.OwnerId == Owner && !Unit(Before, U.UnitId); });
            FVector Spawn = FVector::ZeroVector;
            if (!NewUnit || !CompletionSpawn(Before, After, Owner, Factory->Position, Spawn) || NewUnit->Position != Spawn) return false;
            int32 LiveBeforeCompletion = 0;
            for (const FProofUnitRecord& U : After.Units)
                if (U.OwnerId == Owner && U.HitPoints > 0.f && Unit(Before, U.UnitId)) ++LiveBeforeCompletion;
            if (LiveBeforeCompletion >= 12) return false;
        }
    }
    int32 Deposit = 0;
    for (const FProofUnitRecord& U : Before.Units)
    {
        const FProofUnitRecord* V = Unit(After, U.UnitId);
        if (U.OwnerId != Owner || U.Kind != EProofUnitKind::Harvester || !V || U.Cargo <= V->Cargo) continue;
        const bool AtDepot = After.Units.ContainsByPredicate([Owner, V](const FProofUnitRecord& D)
            { return D.OwnerId == Owner && D.Kind == EProofUnitKind::Refinery && D.HitPoints > 0.f
                && FVector::DistSquared(D.Position, V->Position) <= 1.0; });
        if (U.HitPoints <= 0.f || V->HitPoints <= 0.f || !AtDepot) return false;
        Deposit += U.Cargo - V->Cargo;
    }
    const FProofPlayerRecord* Q = Player(After, Owner);
    return Ledger(After, Owner) - Ledger(Before, Owner) == static_cast<int64>(Payment) - Refund
        && static_cast<int64>(Q->Credits) - P->Credits == Deposit - Payment + Refund
        && static_cast<int64>(Q->TotalGathered) - P->TotalGathered == Deposit;
}
inline FString JobsJson(const FProofSnapshot& S)
{
    FString Json = TEXT("["); bool First = true;
    for (const FProofProductionJob& J : S.ProductionJobs)
    {
        if (!First) { Json += TEXT(","); } First = false;
        Json += FString::Printf(TEXT("{\"owner\":%d,\"sequence\":%d,\"factory\":%d,\"work\":%d,\"paid\":%d,\"held\":%d}"),
            J.OwnerId, J.StartSequence, J.FactoryId, J.WorkMicroseconds, J.PaidCredits, J.bHeld ? 1 : 0);
    }
    Json += TEXT("]"); return Json;
}
inline FString ContextJson(const FProofSnapshot& S)
{
    FString Json = FString::Printf(TEXT("{\"phase\":%d,\"jobs\":%s,\"units\":["), static_cast<int32>(S.MatchPhase), *JobsJson(S));
    bool First = true;
    for (const FProofUnitRecord& U : S.Units)
    {
        if (!First) { Json += TEXT(","); } First = false;
        Json += FString::Printf(TEXT("{\"id\":%d,\"owner\":%d,\"kind\":%d,\"hp\":%.17g,\"cargo\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"rally\":%d,\"rallyX\":%.17g,\"rallyY\":%.17g,\"rallyZ\":%.17g}"),
            U.UnitId, U.OwnerId, static_cast<int32>(U.Kind), U.HitPoints, U.Cargo, U.Position.X, U.Position.Y, U.Position.Z,
            U.bHasRallyPoint ? 1 : 0, U.RallyPoint.X, U.RallyPoint.Y, U.RallyPoint.Z);
    }
    Json += TEXT("],\"players\":["); First = true;
    for (const FProofPlayerRecord& P : S.Players)
    {
        if (!First) { Json += TEXT(","); } First = false;
        Json += FString::Printf(TEXT("{\"id\":%d,\"credits\":%d,\"gathered\":%d}"), P.OwnerId, P.Credits, P.TotalGathered);
    }
    Json += TEXT("]}"); return Json;
}
}
