#pragma once

#include "Commands/ProofCommandModel.h"

// Read-only text derived from one replicated production snapshot.
namespace ProofProductionStatusText
{
inline TArray<FString> Local(const FProofSnapshot& Snapshot, int32 OwnerId, int32 ExpectedEpoch)
{
    if (OwnerId <= 0 || ExpectedEpoch <= 0 || Snapshot.MatchEpoch <= 0
        || Snapshot.MatchEpoch != ExpectedEpoch || !Snapshot.bSkirmish
        || Snapshot.MatchPhase != EProofMatchPhase::Playing)
    {
        return {};
    }

    const FProofPlayerRecord* LocalPlayer = nullptr;
    for (const FProofPlayerRecord& Player : Snapshot.Players)
    {
        if (Player.OwnerId != OwnerId)
        {
            continue;
        }
        if (LocalPlayer || Player.Credits < 0)
        {
            return {};
        }
        LocalPlayer = &Player;
    }
    if (!LocalPlayer)
    {
        return {};
    }

    const FProofProductionJob* LocalJob = nullptr;
    for (const FProofProductionJob& Job : Snapshot.ProductionJobs)
    {
        if (Job.OwnerId != OwnerId)
        {
            continue;
        }
        if (LocalJob)
        {
            return {};
        }
        LocalJob = &Job;
    }

    if (!LocalJob)
    {
        return { TEXT("Production idle"), TEXT("Idle") };
    }

    if (LocalJob->StartSequence <= 0 || LocalJob->FactoryId <= 0
        || LocalJob->WorkMicroseconds < 0
        || LocalJob->WorkMicroseconds > FProofCommandModel::ProductionWorkMicroseconds
        || LocalJob->PaidCredits < 0
        || LocalJob->PaidCredits > FProofCommandModel::ProductionCost
        || static_cast<int64>(LocalJob->WorkMicroseconds)
            > static_cast<int64>(LocalJob->PaidCredits) * FProofCommandModel::ProductionWorkPerCredit)
    {
        return {};
    }

    const bool bFactoryAvailable = Snapshot.Units.ContainsByPredicate([LocalJob, OwnerId](const FProofUnitRecord& Unit)
    {
        return Unit.UnitId == LocalJob->FactoryId && Unit.OwnerId == OwnerId
            && Unit.Kind == EProofUnitKind::Factory && FMath::IsFinite(Unit.HitPoints)
            && Unit.HitPoints > 0.f;
    });

    const TCHAR* State = TEXT("Building");
    if (!bFactoryAvailable)
    {
        State = TEXT("Factory unavailable");
    }
    else if (LocalJob->bHeld)
    {
        State = TEXT("On hold");
    }
    else if (LocalJob->WorkMicroseconds == FProofCommandModel::ProductionWorkMicroseconds)
    {
        State = TEXT("Ready; waiting for release");
    }
    else if (LocalPlayer->Credits == 0
        && static_cast<int64>(LocalJob->WorkMicroseconds)
            >= static_cast<int64>(LocalJob->PaidCredits) * FProofCommandModel::ProductionWorkPerCredit
        && LocalJob->PaidCredits < FProofCommandModel::ProductionCost)
    {
        State = TEXT("Waiting for credits");
    }

    const int32 Percent = static_cast<int32>(static_cast<int64>(LocalJob->WorkMicroseconds) * 100
        / FProofCommandModel::ProductionWorkMicroseconds);
    const FString Full = FString::Printf(TEXT("Production: %s | Factory %d | %d%% | %d/%d credits"),
        State, LocalJob->FactoryId, Percent, LocalJob->PaidCredits, FProofCommandModel::ProductionCost);
    const FString Compact = FString::Printf(TEXT("%s | F%d | %d%% | %d/%d"),
        State, LocalJob->FactoryId, Percent, LocalJob->PaidCredits, FProofCommandModel::ProductionCost);
    return { Full, Compact };
}
}
