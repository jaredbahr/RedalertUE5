#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"

namespace
{
FProofCommand MakeDisconnectCommand(int32 Sequence, EProofCommandType Type, int32 UnitId = INDEX_NONE,
    int32 TargetId = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand Command;
    Command.Sequence = Sequence;
    Command.Type = Type;
    Command.UnitId = UnitId;
    Command.TargetUnitId = TargetId;
    Command.Destination = Destination;
    return Command;
}

int32 FindDisconnectUnit(const FProofSnapshot& Snapshot, int32 OwnerId, EProofUnitKind Kind)
{
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([OwnerId, Kind](const FProofUnitRecord& Candidate)
        { return Candidate.OwnerId == OwnerId && Candidate.Kind == Kind; });
    return Unit ? Unit->UnitId : INDEX_NONE;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishDisconnectTest, "RedalertProof.Skirmish.Lifecycle.DisconnectAndRejoin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishDisconnectTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    if (!TestTrue(TEXT("Seed departing human"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector))
        || !TestTrue(TEXT("Seed surviving human"), Model.SeedSkirmishPlayer(2, 0, FVector(0, 2000, 0)))
        || !TestTrue(TEXT("Seed opposing bot"), Model.SeedSkirmishPlayer(100, 1, FVector(5000, 0, 0)))) return false;

    const FProofSnapshot Opening = Model.GetSnapshot();
    const int32 DepartingOre = Opening.Resources[0].ResourceId;
    const int32 SurvivingOre = Opening.Resources[1].ResourceId;
    const int32 DepartingHQ = FindDisconnectUnit(Opening, 1, EProofUnitKind::HQ);
    const int32 BotHQ = FindDisconnectUnit(Opening, 100, EProofUnitKind::HQ);
    const int32 SurvivingHarvester = FindDisconnectUnit(Opening, 2, EProofUnitKind::Harvester);
    const int32 BotHarvester = FindDisconnectUnit(Opening, 100, EProofUnitKind::Harvester);
    TestEqual(TEXT("Starter ore has departing owner"), Opening.Resources[0].OwnerId, 1);
    TestEqual(TEXT("Teammate ore has separate owner"), Opening.Resources[1].OwnerId, 2);

    // Queue both outbound and inbound references to the departing records.
    if (!TestTrue(TEXT("Departing player builds factory"), Model.Submit(1, MakeDisconnectCommand(1,
        EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(-300, 0, 0))).bAccepted)) return false;
    const int32 DepartingFactory = FindDisconnectUnit(Model.GetSnapshot(), 1, EProofUnitKind::Factory);
    if (!TestTrue(TEXT("Departing player produces combat unit"), Model.Submit(1,
        MakeDisconnectCommand(2, EProofCommandType::Produce, DepartingFactory)).bAccepted)) return false;
    const int32 DepartingCombat = FindDisconnectUnit(Model.GetSnapshot(), 1, EProofUnitKind::Combat);
    TestTrue(TEXT("Departing attack queued"), Model.Submit(1,
        MakeDisconnectCommand(3, EProofCommandType::Attack, DepartingCombat, BotHQ)).bAccepted);

    TestTrue(TEXT("Survivor builds refinery"), Model.Submit(2, MakeDisconnectCommand(1,
        EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, 2000, 0))).bAccepted);
    TestTrue(TEXT("Survivor can target departing starter ore"), Model.Submit(2,
        MakeDisconnectCommand(2, EProofCommandType::Gather, SurvivingHarvester, DepartingOre)).bAccepted);
    TestTrue(TEXT("Bot builds factory"), Model.Submit(100, MakeDisconnectCommand(1,
        EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(4700, 0, 0))).bAccepted);
    const int32 BotFactory = FindDisconnectUnit(Model.GetSnapshot(), 100, EProofUnitKind::Factory);
    TestTrue(TEXT("Bot produces combat unit"), Model.Submit(100,
        MakeDisconnectCommand(2, EProofCommandType::Produce, BotFactory)).bAccepted);
    const int32 BotCombat = FindDisconnectUnit(Model.GetSnapshot(), 100, EProofUnitKind::Combat);
    TestTrue(TEXT("Incoming attack queued"), Model.Submit(100,
        MakeDisconnectCommand(3, EProofCommandType::Attack, BotCombat, DepartingHQ)).bAccepted);
    TestTrue(TEXT("Unrelated bot order queued"), Model.Submit(100,
        MakeDisconnectCommand(4, EProofCommandType::Move, BotHarvester, INDEX_NONE, FVector(5200, 0, 0))).bAccepted);

    const FProofSnapshot Before = Model.GetSnapshot();
    if (!TestTrue(TEXT("Remove active skirmish owner"), Model.RemoveSkirmishPlayer(1))) return false;
    const FProofSnapshot Removed = Model.GetSnapshot();
    TestEqual(TEXT("One revision for removal"), Removed.Revision, Before.Revision + 1);
    TestTrue(TEXT("Playing phase survives disconnect"), Removed.MatchPhase == EProofMatchPhase::Playing);
    TestEqual(TEXT("Only teammate and bot remain"), Removed.Players.Num(), 2);
    TestEqual(TEXT("Only teammate and bot ore remain"), Removed.Resources.Num(), 2);
    TestFalse(TEXT("Departing resource removed"), Removed.Resources.ContainsByPredicate([DepartingOre]
        (const FProofResourceRecord& Resource) { return Resource.ResourceId == DepartingOre; }));
    TestTrue(TEXT("Surviving resource retained"), Removed.Resources.ContainsByPredicate([SurvivingOre]
        (const FProofResourceRecord& Resource) { return Resource.ResourceId == SurvivingOre; }));
    TestFalse(TEXT("Departing units removed"), Removed.Units.ContainsByPredicate([]
        (const FProofUnitRecord& Unit) { return Unit.OwnerId == 1; }));
    for (const FProofPlayerRecord& Player : Before.Players)
    {
        if (Player.OwnerId == 1) continue;
        const FProofPlayerRecord* Kept = Removed.Players.FindByPredicate([&Player]
            (const FProofPlayerRecord& Candidate) { return Candidate.OwnerId == Player.OwnerId; });
        if (!TestNotNull(TEXT("Other player record survives unchanged"), Kept)) return false;
        TestEqual(TEXT("Other player credits unchanged"), Kept->Credits, Player.Credits);
        TestEqual(TEXT("Other player team unchanged"), Kept->TeamId, Player.TeamId);
        TestEqual(TEXT("Other player harvest unchanged"), Kept->TotalGathered, Player.TotalGathered);
    }
    for (const FProofUnitRecord& Unit : Before.Units)
    {
        if (Unit.OwnerId == 1) continue;
        const FProofUnitRecord* Kept = Removed.Units.FindByPredicate([&Unit]
            (const FProofUnitRecord& Candidate) { return Candidate.UnitId == Unit.UnitId; });
        if (!TestNotNull(TEXT("Other unit record survives unchanged"), Kept)) return false;
        TestTrue(TEXT("Other unit position unchanged"), Kept->Position.Equals(Unit.Position));
        TestEqual(TEXT("Other unit health unchanged"), Kept->HitPoints, Unit.HitPoints);
        TestEqual(TEXT("Other unit cargo unchanged"), Kept->Cargo, Unit.Cargo);
    }
    TestFalse(TEXT("Duplicate disconnect is inert"), Model.RemoveSkirmishPlayer(1));
    TestEqual(TEXT("Duplicate disconnect leaves snapshot alone"), Model.GetSnapshot().ComputeDigest(), Removed.ComputeDigest());
    TestEqual(TEXT("Removed owner loses command authority"), Model.Submit(1,
        MakeDisconnectCommand(4, EProofCommandType::Move, DepartingCombat, INDEX_NONE, FVector(0, 100, 0))).Reason,
        FString(TEXT("unknown-player")));

    Model.Advance(1.f);
    const FProofSnapshot AfterAdvance = Model.GetSnapshot();
    const FProofUnitRecord* TeammateAfter = AfterAdvance.Units.FindByPredicate([SurvivingHarvester]
        (const FProofUnitRecord& Unit) { return Unit.UnitId == SurvivingHarvester; });
    const FProofUnitRecord* BotCombatAfter = AfterAdvance.Units.FindByPredicate([BotCombat]
        (const FProofUnitRecord& Unit) { return Unit.UnitId == BotCombat; });
    const FProofUnitRecord* BotHarvesterAfter = AfterAdvance.Units.FindByPredicate([BotHarvester]
        (const FProofUnitRecord& Unit) { return Unit.UnitId == BotHarvester; });
    if (!TestNotNull(TEXT("Teammate harvester survives"), TeammateAfter)
        || !TestNotNull(TEXT("Bot combat survives"), BotCombatAfter)
        || !TestNotNull(TEXT("Bot harvester survives"), BotHarvesterAfter)) return false;
    TestTrue(TEXT("Gather targeting removed ore is canceled"), TeammateAfter->Position.Equals(FVector(100, 2000, 0)));
    const FProofUnitRecord* BotCombatBefore = Removed.Units.FindByPredicate([BotCombat]
        (const FProofUnitRecord& Unit) { return Unit.UnitId == BotCombat; });
    TestTrue(TEXT("Attack targeting removed unit is canceled"), BotCombatAfter->Position.Equals(BotCombatBefore->Position));
    TestTrue(TEXT("Unrelated bot order keeps running"), BotHarvesterAfter->Position.Equals(FVector(5200, 0, 0)));

    TestTrue(TEXT("Vacated owner can seed fresh assets"), Model.SeedSkirmishPlayer(1, 0, FVector(0, -2000, 0)));
    const FProofSnapshot Rejoined = Model.GetSnapshot();
    TestEqual(TEXT("Rejoined owner has only new ore"), Rejoined.Resources.FilterByPredicate([]
        (const FProofResourceRecord& Resource) { return Resource.OwnerId == 1; }).Num(), 1);
    TestTrue(TEXT("Fresh owner admits sequence one"), Model.Submit(1, MakeDisconnectCommand(1,
        EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, -2000, 0))).bAccepted);
    TestEqual(TEXT("Survivor keeps its command sequence"), Model.Submit(2, MakeDisconnectCommand(1,
        EProofCommandType::Move, SurvivingHarvester, INDEX_NONE, FVector(0, 1800, 0))).Reason, FString(TEXT("sequence")));
    TestTrue(TEXT("Survivor's next command remains valid"), Model.Submit(2, MakeDisconnectCommand(3,
        EProofCommandType::Move, SurvivingHarvester, INDEX_NONE, FVector(0, 1800, 0))).bAccepted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishTerminalDisconnectTest, "RedalertProof.Skirmish.Lifecycle.TerminalDisconnect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishTerminalDisconnectTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Model.SeedSkirmishPlayer(100, 1, FVector(700, 0, 0));
    TestTrue(TEXT("Bot factory"), Model.Submit(100, MakeDisconnectCommand(1,
        EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(950, 0, 0))).bAccepted);
    const int32 Factory = FindDisconnectUnit(Model.GetSnapshot(), 100, EProofUnitKind::Factory);
    TestTrue(TEXT("Bot combat"), Model.Submit(100, MakeDisconnectCommand(2, EProofCommandType::Produce, Factory)).bAccepted);
    const FProofSnapshot Armed = Model.GetSnapshot();
    TestTrue(TEXT("Bot attacks HQ"), Model.Submit(100, MakeDisconnectCommand(3, EProofCommandType::Attack,
        FindDisconnectUnit(Armed, 100, EProofUnitKind::Combat), FindDisconnectUnit(Armed, 1, EProofUnitKind::HQ))).bAccepted);
    Model.Advance(12.f);
    const FProofSnapshot Finished = Model.GetSnapshot();
    if (!TestTrue(TEXT("Match finished"), Finished.MatchPhase == EProofMatchPhase::Finished)) return false;
    TestTrue(TEXT("Terminal owner removal works"), Model.RemoveSkirmishPlayer(1));
    const FProofSnapshot AfterRemoval = Model.GetSnapshot();
    TestTrue(TEXT("Terminal phase stays frozen"), AfterRemoval.MatchPhase == EProofMatchPhase::Finished);
    TestEqual(TEXT("Winner stays recorded"), AfterRemoval.WinnerTeam, Finished.WinnerTeam);
    TestEqual(TEXT("Bot survives"), AfterRemoval.Players.Num(), 1);
    TestFalse(TEXT("Terminal match refuses reseed"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishForfeitDisconnectTest, "RedalertProof.Skirmish.Lifecycle.LastTeamDisconnect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishForfeitDisconnectTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    TestTrue(TEXT("Startup first human"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    TestTrue(TEXT("Startup first human departs"), Model.RemoveSkirmishPlayer(1));
    TestTrue(TEXT("Startup departure has no false winner"), Model.GetSnapshot().MatchPhase == EProofMatchPhase::Playing);
    TestEqual(TEXT("Startup has no winner"), Model.GetSnapshot().WinnerTeam, INDEX_NONE);
    TestTrue(TEXT("New human joins vacant slot"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    TestTrue(TEXT("Bot joins opposing team"), Model.SeedSkirmishPlayer(100, 1, FVector(5000, 0, 0)));
    TestTrue(TEXT("Last human departs"), Model.RemoveSkirmishPlayer(1));
    const FProofSnapshot Forfeit = Model.GetSnapshot();
    TestTrue(TEXT("Last team departure finishes match"), Forfeit.MatchPhase == EProofMatchPhase::Finished);
    TestEqual(TEXT("Opposing team wins forfeit"), Forfeit.WinnerTeam, 1);
    TestFalse(TEXT("Finished match cannot reseed departing team"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));

    Model.StartSkirmish();
    Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Model.SeedSkirmishPlayer(100, 1, FVector(5000, 0, 0));
    TestTrue(TEXT("Bot departure can forfeit too"), Model.RemoveSkirmishPlayer(100));
    TestEqual(TEXT("Human wins opposing forfeit"), Model.GetSnapshot().WinnerTeam, 0);
    return true;
}
#endif
