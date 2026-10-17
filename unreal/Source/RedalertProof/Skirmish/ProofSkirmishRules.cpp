#include "Skirmish/ProofSkirmishRules.h"

FProofMatchOutcome FProofSkirmishRules::Evaluate(const FProofSnapshot& Snapshot)
{
    if (!Snapshot.bSkirmish)
    {
        return {};
    }

    // The authoritative model owns the terminal transition. Rules evaluation
    // must return its recorded result unchanged on later ticks.
    if (Snapshot.MatchPhase == EProofMatchPhase::Finished)
    {
        FProofMatchOutcome Outcome;
        Outcome.bFinished = true;
        Outcome.WinnerTeam = Snapshot.WinnerTeam;
        return Outcome;
    }

    if (Snapshot.MatchPhase != EProofMatchPhase::Playing)
    {
        return {};
    }

    bool bTeam0Represented = false;
    bool bTeam1Represented = false;
    for (const FProofPlayerRecord& Player : Snapshot.Players)
    {
        bTeam0Represented |= Player.TeamId == 0;
        bTeam1Represented |= Player.TeamId == 1;
    }
    if (!bTeam0Represented || !bTeam1Represented)
    {
        return {};
    }

    bool bTeam0HasLiveHQ = false;
    bool bTeam1HasLiveHQ = false;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.Kind != EProofUnitKind::HQ || Unit.HitPoints <= 0.f)
        {
            continue;
        }

        const FProofPlayerRecord* Owner = Snapshot.Players.FindByPredicate(
            [&Unit](const FProofPlayerRecord& Player) { return Player.OwnerId == Unit.OwnerId; });
        if (!Owner)
        {
            continue;
        }
        bTeam0HasLiveHQ |= Owner->TeamId == 0;
        bTeam1HasLiveHQ |= Owner->TeamId == 1;
    }

    if (bTeam0HasLiveHQ && bTeam1HasLiveHQ)
    {
        return {};
    }

    FProofMatchOutcome Outcome;
    Outcome.bFinished = true;
    if (bTeam0HasLiveHQ)
    {
        Outcome.WinnerTeam = 0;
    }
    else if (bTeam1HasLiveHQ)
    {
        Outcome.WinnerTeam = 1;
    }
    return Outcome;
}
