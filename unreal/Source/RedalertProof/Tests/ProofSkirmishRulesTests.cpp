#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Skirmish/ProofSkirmishRules.h"

namespace
{
FProofSnapshot MakePlayingSnapshot(bool bTeam0HQ = true, bool bTeam1HQ = true)
{
    FProofSnapshot Snapshot;
    Snapshot.bSkirmish = true;
    Snapshot.MatchPhase = EProofMatchPhase::Playing;

    FProofPlayerRecord Human;
    Human.OwnerId = 1;
    Human.TeamId = 0;
    Snapshot.Players.Add(Human);

    FProofPlayerRecord Bot;
    Bot.OwnerId = 100;
    Bot.TeamId = 1;
    Snapshot.Players.Add(Bot);

    if (bTeam0HQ)
    {
        FProofUnitRecord HQ;
        HQ.UnitId = 1;
        HQ.OwnerId = 1;
        HQ.Kind = EProofUnitKind::HQ;
        HQ.HitPoints = 300.f;
        Snapshot.Units.Add(HQ);
    }
    if (bTeam1HQ)
    {
        FProofUnitRecord HQ;
        HQ.UnitId = 2;
        HQ.OwnerId = 100;
        HQ.Kind = EProofUnitKind::HQ;
        HQ.HitPoints = 300.f;
        Snapshot.Units.Add(HQ);
    }
    return Snapshot;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishRulesOutcomeTest, "RedalertProof.Skirmish.RulesOutcome", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishRulesOutcomeTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = MakePlayingSnapshot();
    FProofMatchOutcome Outcome = FProofSkirmishRules::Evaluate(Snapshot);
    TestFalse(TEXT("Both teams with live HQs remain in play"), Outcome.bFinished);

    Snapshot = MakePlayingSnapshot(false, true);
    Outcome = FProofSkirmishRules::Evaluate(Snapshot);
    TestTrue(TEXT("Team 1 HQ standing finishes the match"), Outcome.bFinished);
    TestEqual(TEXT("Team 1 wins when team 0 has no live HQ"), Outcome.WinnerTeam, 1);

    Snapshot = MakePlayingSnapshot(true, false);
    Outcome = FProofSkirmishRules::Evaluate(Snapshot);
    TestTrue(TEXT("Team 0 HQ standing finishes the match"), Outcome.bFinished);
    TestEqual(TEXT("Team 0 wins when team 1 has no live HQ"), Outcome.WinnerTeam, 0);

    Snapshot = MakePlayingSnapshot(false, false);
    Outcome = FProofSkirmishRules::Evaluate(Snapshot);
    TestTrue(TEXT("Both teams losing their HQs finishes the match"), Outcome.bFinished);
    TestEqual(TEXT("Both teams absent is a draw"), Outcome.WinnerTeam, INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishRulesReadinessAndStabilityTest, "RedalertProof.Skirmish.RulesReadinessAndStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishRulesReadinessAndStabilityTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = MakePlayingSnapshot(false, true);
    Snapshot.Players.RemoveAt(1);
    FProofMatchOutcome Outcome = FProofSkirmishRules::Evaluate(Snapshot);
    TestFalse(TEXT("Missing opposing team does not cause premature victory"), Outcome.bFinished);

    Snapshot = MakePlayingSnapshot(false, true);
    Snapshot.bSkirmish = false;
    Outcome = FProofSkirmishRules::Evaluate(Snapshot);
    TestFalse(TEXT("Legacy snapshot does not trigger skirmish outcome"), Outcome.bFinished);

    Snapshot = MakePlayingSnapshot(true, true);
    Snapshot.MatchPhase = EProofMatchPhase::Finished;
    Snapshot.WinnerTeam = 1;
    Outcome = FProofSkirmishRules::Evaluate(Snapshot);
    TestTrue(TEXT("Finished phase remains terminal"), Outcome.bFinished);
    TestEqual(TEXT("Recorded winner is retained"), Outcome.WinnerTeam, 1);

    Snapshot = MakePlayingSnapshot(false, true);
    Snapshot.MatchPhase = EProofMatchPhase::Finished;
    Snapshot.WinnerTeam = INDEX_NONE;
    Outcome = FProofSkirmishRules::Evaluate(Snapshot);
    TestTrue(TEXT("Finished draw remains terminal even with only one HQ visible"), Outcome.bFinished);
    TestEqual(TEXT("Recorded draw remains stable"), Outcome.WinnerTeam, INDEX_NONE);
    return true;
}
#endif
