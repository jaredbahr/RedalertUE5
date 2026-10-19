#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"
#include "Commands/ProofOwnerIds.h"
#include <limits>

namespace
{
FProofCommand StopOrder(int32 Sequence, EProofCommandType Type, int32 Unit = INDEX_NONE,
    int32 Target = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand C;
    C.Sequence = Sequence; C.Type = Type; C.UnitId = Unit;
    C.TargetUnitId = Target; C.Destination = Destination;
    return C;
}
const FProofUnitRecord& StopUnit(const FProofSnapshot& S, int32 Id)
{
    return *S.Units.FindByPredicate([Id](const FProofUnitRecord& U) { return U.UnitId == Id; });
}
int32 StopKind(const FProofSnapshot& S, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* U = S.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& R)
        { return R.OwnerId == Owner && R.Kind == Kind; });
    return U ? U->UnitId : INDEX_NONE;
}
const FProofPlayerRecord& StopPlayer(const FProofSnapshot& S, int32 Owner)
{
    return *S.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; });
}
FString StopVisibleDigest(FProofSnapshot S, int32 Revision)
{
    S.Revision = Revision;
    return S.ComputeDigest();
}
void StopSeedPair(FProofCommandModel& Model)
{
    Model.StartSkirmish();
    Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Model.SeedSkirmishPlayer(ProofOwnerIds::Bot, 1, FVector(5000, 0, 0));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopLegacyMoveTest, "RedalertProof.Stop.LegacyMoveAndNoMutation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopLegacyMoveTest::RunTest(const FString&)
{
    TestEqual(TEXT("Existing Move ordinal"), static_cast<uint8>(EProofCommandType::Move), static_cast<uint8>(0));
    TestEqual(TEXT("Existing Attack ordinal"), static_cast<uint8>(EProofCommandType::Attack), static_cast<uint8>(1));
    TestEqual(TEXT("Existing Produce ordinal"), static_cast<uint8>(EProofCommandType::Produce), static_cast<uint8>(2));
    TestEqual(TEXT("Existing BuildRefinery ordinal"), static_cast<uint8>(EProofCommandType::BuildRefinery), static_cast<uint8>(3));
    TestEqual(TEXT("Existing BuildFactory ordinal"), static_cast<uint8>(EProofCommandType::BuildFactory), static_cast<uint8>(4));
    TestEqual(TEXT("Existing Gather ordinal"), static_cast<uint8>(EProofCommandType::Gather), static_cast<uint8>(5));
    TestEqual(TEXT("Stop is appended"), static_cast<uint8>(EProofCommandType::Stop), static_cast<uint8>(6));
    FProofCommandModel Model; Model.AddPlayer(1);
    const int32 Id = Model.SeedUnit(1, FVector::ZeroVector);
    if (!TestTrue(TEXT("Move accepted"), Model.Submit(1, StopOrder(1, EProofCommandType::Move, Id, INDEX_NONE, FVector(1000, 0, 0))).bAccepted)) return false;
    Model.Advance(1.f);
    const FProofSnapshot Before = Model.GetSnapshot();
    TestEqual(TEXT("Move genuinely progressed before Stop"), StopUnit(Before, Id).Position, FVector(150, 0, 0));
    TestTrue(TEXT("Legacy mobile Stop accepted; unused finite destination is not movement"), Model.Submit(1, StopOrder(2, EProofCommandType::Stop, Id, INDEX_NONE, FVector(9000, 0, 0))).bAccepted);
    const FProofSnapshot Stopped = Model.GetSnapshot();
    TestEqual(TEXT("Accepted Stop revises once"), Stopped.Revision, Before.Revision + 1);
    TestEqual(TEXT("Stop changes no visible record, HP, cargo or credits"), StopVisibleDigest(Stopped, Before.Revision), Before.ComputeDigest());
    Model.Advance(10.f);
    TestEqual(TEXT("Old Move never resumes or teleports"), StopVisibleDigest(Model.GetSnapshot(), Before.Revision), Before.ComputeDigest());
    TestTrue(TEXT("Stopping an already idle mobile is accepted"), Model.Submit(1, StopOrder(3, EProofCommandType::Stop, Id)).bAccepted);
    TestTrue(TEXT("A later Move can replace Stop"), Model.Submit(1, StopOrder(4, EProofCommandType::Move, Id, INDEX_NONE, FVector(450, 0, 0))).bAccepted);
    Model.Advance(1.f);
    TestEqual(TEXT("New Move progresses from stopped position"), StopUnit(Model.GetSnapshot(), Id).Position, FVector(300, 0, 0));
    const int32 Other = Model.SeedUnit(1, FVector(0, 1000, 0));
    TestTrue(TEXT("Another mobile has its own active order"), Model.Submit(1, StopOrder(5, EProofCommandType::Move, Other, INDEX_NONE, FVector(1000, 1000, 0))).bAccepted);
    TestTrue(TEXT("Stop only the first mobile"), Model.Submit(1, StopOrder(6, EProofCommandType::Stop, Id)).bAccepted);
    Model.Advance(1.f);
    TestEqual(TEXT("First mobile remains stopped"), StopUnit(Model.GetSnapshot(), Id).Position, FVector(300, 0, 0));
    TestEqual(TEXT("Stop cannot clear unrelated mobile orders"), StopUnit(Model.GetSnapshot(), Other).Position, FVector(150, 1000, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopLegacyAttackTest, "RedalertProof.Stop.LegacyAttackCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopLegacyAttackTest::RunTest(const FString&)
{
    FProofCommandModel Model; Model.AddPlayer(1); Model.AddPlayer(2);
    const int32 A = Model.SeedUnit(1, FVector::ZeroVector), B = Model.SeedUnit(2, FVector(1000, 0, 0));
    TestTrue(TEXT("Distant attack accepted"), Model.Submit(1, StopOrder(1, EProofCommandType::Attack, A, B)).bAccepted);
    Model.Advance(1.f);
    const FProofSnapshot Pursuing = Model.GetSnapshot();
    TestEqual(TEXT("Attack genuinely pursued first"), StopUnit(Pursuing, A).Position, FVector(150, 0, 0));
    TestEqual(TEXT("Out-of-range target untouched"), StopUnit(Pursuing, B).HitPoints, 100.f);
    TestTrue(TEXT("Stop cancels attack"), Model.Submit(1, StopOrder(2, EProofCommandType::Stop, A)).bAccepted);
    Model.Advance(20.f);
    TestEqual(TEXT("Cancelled chase stays at current position"), StopUnit(Model.GetSnapshot(), A).Position, FVector(150, 0, 0));
    TestEqual(TEXT("Cancelled attack never reaches or damages target"), StopUnit(Model.GetSnapshot(), B).HitPoints, 100.f);
    TestTrue(TEXT("Fresh attack can resume"), Model.Submit(1, StopOrder(3, EProofCommandType::Attack, A, B)).bAccepted);
    Model.Advance(1.f);
    TestEqual(TEXT("Fresh pursuit resumes without teleport"), StopUnit(Model.GetSnapshot(), A).Position, FVector(300, 0, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopGatherResumeTest, "RedalertProof.Stop.GatherPauseAndResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopGatherResumeTest::RunTest(const FString&)
{
    FProofCommandModel Model; StopSeedPair(Model);
    const FProofSnapshot Opening = Model.GetSnapshot();
    const int32 Miner = StopKind(Opening, 1, EProofUnitKind::Harvester), Ore = Opening.Resources[0].ResourceId;
    if (!TestTrue(TEXT("Real refinery built"), Model.Submit(1, StopOrder(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0))).bAccepted)) return false;
    // Isolate the explicitly commanded miner; automatic-grant behavior has its own tests.
    if (!TestTrue(TEXT("Stop incidental granted miner"), Model.Submit(1, StopOrder(2, EProofCommandType::Stop, Model.GetSnapshot().Units.Last().UnitId)).bAccepted)) return false;
    TestTrue(TEXT("Gather accepted"), Model.Submit(1, StopOrder(3, EProofCommandType::Gather, Miner, Ore)).bAccepted);
    Model.Advance(1.f);
    const FProofSnapshot EnRoute = Model.GetSnapshot();
    TestEqual(TEXT("Miner genuinely traveled toward ore"), StopUnit(EnRoute, Miner).Position, FVector(250, 0, 0));
    TestTrue(TEXT("Stop empty Gather traveler"), Model.Submit(1, StopOrder(4, EProofCommandType::Stop, Miner)).bAccepted);
    Model.Advance(10.f);
    TestEqual(TEXT("Empty stopped miner cannot travel/extract/deposit or alter economy"), StopVisibleDigest(Model.GetSnapshot(), EnRoute.Revision), EnRoute.ComputeDigest());
    TestTrue(TEXT("Resume Gather by ordinary command"), Model.Submit(1, StopOrder(5, EProofCommandType::Gather, Miner, Ore)).bAccepted);
    Model.Advance(2.f);
    const FProofSnapshot Loaded = Model.GetSnapshot();
    TestEqual(TEXT("Resumed miner reaches ore"), StopUnit(Loaded, Miner).Position, FVector(550, 0, 0));
    TestEqual(TEXT("Real extraction loads capacity"), StopUnit(Loaded, Miner).Cargo, 50);
    TestEqual(TEXT("Finite ore debited once"), Loaded.Resources[0].Remaining, 450);
    TestTrue(TEXT("Stop loaded return"), Model.Submit(1, StopOrder(6, EProofCommandType::Stop, Miner)).bAccepted);
    TestEqual(TEXT("Stop preserves all loaded state except revision"), StopVisibleDigest(Model.GetSnapshot(), Loaded.Revision), Loaded.ComputeDigest());
    Model.Advance(10.f);
    TestEqual(TEXT("Loaded Stop cannot move, extract, deposit or lose cargo"), StopVisibleDigest(Model.GetSnapshot(), Loaded.Revision), Loaded.ComputeDigest());
    TestEqual(TEXT("No income while paused"), StopPlayer(Model.GetSnapshot(), 1).TotalGathered, 0);
    TestTrue(TEXT("Loaded Gather resume accepted"), Model.Submit(1, StopOrder(7, EProofCommandType::Gather, Miner, Ore)).bAccepted);
    Model.Advance(2.f);
    const FProofSnapshot Deposited = Model.GetSnapshot();
    TestEqual(TEXT("Resume returns to actual owned refinery"), StopUnit(Deposited, Miner).Position, FVector(300, 0, 0));
    TestEqual(TEXT("Original load is deposited"), StopUnit(Deposited, Miner).Cargo, 0);
    TestEqual(TEXT("Delivered credits exactly once"), StopPlayer(Deposited, 1).Credits, 300);
    TestEqual(TEXT("Delivered ore exactly once"), StopPlayer(Deposited, 1).TotalGathered, 50);
    TestEqual(TEXT("Return/deposit does not extract more ore"), Deposited.Resources[0].Remaining, 450);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopAtOreTest, "RedalertProof.Stop.EmptyGatherAtOre", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopAtOreTest::RunTest(const FString&)
{
    FProofCommandModel Model; StopSeedPair(Model);
    const int32 Miner = StopKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester);
    const FProofResourceRecord Ore = Model.GetSnapshot().Resources[0];
    if (!TestTrue(TEXT("Build refinery"), Model.Submit(1, StopOrder(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0))).bAccepted)) return false;
    // Isolate the explicitly commanded miner; automatic-grant behavior has its own tests.
    if (!TestTrue(TEXT("Stop incidental granted miner"), Model.Submit(1, StopOrder(2, EProofCommandType::Stop, Model.GetSnapshot().Units.Last().UnitId)).bAccepted)) return false;
    TestTrue(TEXT("Move empty miner onto ore"), Model.Submit(1, StopOrder(3, EProofCommandType::Move, Miner, INDEX_NONE, Ore.Position)).bAccepted);
    Model.Advance(3.f);
    TestEqual(TEXT("Miner actually on ore with empty cargo"), StopUnit(Model.GetSnapshot(), Miner).Position, Ore.Position);
    TestEqual(TEXT("Move alone does not extract"), StopUnit(Model.GetSnapshot(), Miner).Cargo, 0);
    TestTrue(TEXT("Gather admitted at resource"), Model.Submit(1, StopOrder(4, EProofCommandType::Gather, Miner, Ore.ResourceId)).bAccepted);
    TestTrue(TEXT("Stop before next extraction step"), Model.Submit(1, StopOrder(5, EProofCommandType::Stop, Miner)).bAccepted);
    const FProofSnapshot Paused = Model.GetSnapshot();
    Model.Advance(20.f);
    TestEqual(TEXT("Standing on ore is insufficient after Stop"), StopVisibleDigest(Model.GetSnapshot(), Paused.Revision), Paused.ComputeDigest());
    TestTrue(TEXT("Fresh Gather admitted"), Model.Submit(1, StopOrder(6, EProofCommandType::Gather, Miner, Ore.ResourceId)).bAccepted);
    Model.Advance(.1f);
    TestEqual(TEXT("Fresh Gather really extracts"), StopUnit(Model.GetSnapshot(), Miner).Cargo, 50);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopIdleFireTest, "RedalertProof.Stop.IdleFireAfterStop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopIdleFireTest::RunTest(const FString&)
{
    FProofCommandModel Model; Model.StartSkirmish();
    Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Model.SeedSkirmishPlayer(ProofOwnerIds::Bot, 1, FVector(900, 0, 0));
    if (!TestTrue(TEXT("Build factory"), Model.Submit(1, StopOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0))).bAccepted)) return false;
    const int32 Factory = StopKind(Model.GetSnapshot(), 1, EProofUnitKind::Factory);
    if (!TestTrue(TEXT("Produce actual Combat"), Model.Submit(1, StopOrder(2, EProofCommandType::Produce, Factory)).bAccepted)) return false;
    const int32 Combat = StopKind(Model.GetSnapshot(), 1, EProofUnitKind::Combat);
    const int32 EnemyHQ = StopKind(Model.GetSnapshot(), ProofOwnerIds::Bot, EProofUnitKind::HQ);
    const int32 EnemyMiner = StopKind(Model.GetSnapshot(), ProofOwnerIds::Bot, EProofUnitKind::Harvester);
    TestTrue(TEXT("Explicit farther target accepted"), Model.Submit(1, StopOrder(3, EProofCommandType::Attack, Combat, EnemyMiner)).bAccepted);
    Model.Advance(.5f);
    const FProofSnapshot Chase = Model.GetSnapshot();
    TestEqual(TEXT("Explicit out-of-range pursuit suppresses nearby idle shot"), StopUnit(Chase, EnemyHQ).HitPoints, 300.f);
    TestEqual(TEXT("Pursuit moved only one step"), StopUnit(Chase, Combat).Position, FVector(675, 0, 0));
    TestTrue(TEXT("Skirmish Combat Stop accepted"), Model.Submit(1, StopOrder(4, EProofCommandType::Stop, Combat)).bAccepted);
    TestEqual(TEXT("Stop does not itself attack or mutate visible state"), StopVisibleDigest(Model.GetSnapshot(), Chase.Revision), Chase.ComputeDigest());
    Model.Advance(1.f);
    TestEqual(TEXT("Stopped Combat acquires nearby enemy with one ordinary shot"), StopUnit(Model.GetSnapshot(), EnemyHQ).HitPoints, 275.f);
    TestEqual(TEXT("Cancelled farther attack target is untouched"), StopUnit(Model.GetSnapshot(), EnemyMiner).HitPoints, 100.f);
    TestEqual(TEXT("Idle firing cannot resume chase"), StopUnit(Model.GetSnapshot(), Combat).Position, FVector(675, 0, 0));
    Model.Advance(1.f);
    TestEqual(TEXT("No duplicate explicit plus idle firing"), StopUnit(Model.GetSnapshot(), EnemyHQ).HitPoints, 250.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopValidationTest, "RedalertProof.Stop.ValidationRolesAndMalformed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopValidationTest::RunTest(const FString&)
{
    FProofCommandModel Model; StopSeedPair(Model);
    Model.SeedSkirmishPlayer(2, 0, FVector(0, 3000, 0));
    const int32 Miner = StopKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester);
    Model.Submit(1, StopOrder(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0)));
    Model.Submit(1, StopOrder(2, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(-300, 0, 0)));
    const FString Before = Model.GetSnapshot().ComputeDigest();
    int32 Sequence = 3;
    for (const EProofUnitKind Kind : {EProofUnitKind::HQ, EProofUnitKind::Refinery, EProofUnitKind::Factory})
    {
        TestEqual(TEXT("Structures cannot Stop"), Model.Submit(1, StopOrder(Sequence++, EProofCommandType::Stop, StopKind(Model.GetSnapshot(), 1, Kind))).Reason, FString(TEXT("role")));
    }
    TestEqual(TEXT("Teammate ownership is not shared control"), Model.Submit(1, StopOrder(Sequence++, EProofCommandType::Stop, StopKind(Model.GetSnapshot(), 2, EProofUnitKind::Harvester))).Reason, FString(TEXT("ownership")));
    TestEqual(TEXT("Missing positive ID rejects"), Model.Submit(1, StopOrder(Sequence++, EProofCommandType::Stop, 999999)).Reason, FString(TEXT("ownership")));
    TestEqual(TEXT("Unknown trusted owner rejects"), Model.Submit(99, StopOrder(1, EProofCommandType::Stop, Miner)).Reason, FString(TEXT("unknown-player")));
    TestEqual(TEXT("Zero ID malformed without consuming envelope"), Model.Submit(1, StopOrder(Sequence, EProofCommandType::Stop, 0)).Reason, FString(TEXT("malformed")));
    TestEqual(TEXT("Negative ID malformed"), Model.Submit(1, StopOrder(Sequence, EProofCommandType::Stop, -1)).Reason, FString(TEXT("malformed")));
    TestEqual(TEXT("Nonpositive sequence malformed"), Model.Submit(1, StopOrder(0, EProofCommandType::Stop, Miner)).Reason, FString(TEXT("malformed")));
    for (const double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        TestEqual(TEXT("Unused Stop destination still obeys finite-envelope rule"), Model.Submit(1, StopOrder(Sequence, EProofCommandType::Stop, Miner, INDEX_NONE, FVector(Invalid, 0, 0))).Reason, FString(TEXT("malformed")));
    }
    TestEqual(TEXT("Unknown enum still malformed"), Model.Submit(1, StopOrder(Sequence, static_cast<EProofCommandType>(255), Miner)).Reason, FString(TEXT("malformed")));
    TestEqual(TEXT("Skipped sequence rejects"), Model.Submit(1, StopOrder(Sequence + 1, EProofCommandType::Stop, Miner)).Reason, FString(TEXT("sequence")));
    TestEqual(TEXT("All rejections preserve visible state"), Model.GetSnapshot().ComputeDigest(), Before);
    const FProofCommand Valid = StopOrder(Sequence, EProofCommandType::Stop, Miner);
    TestTrue(TEXT("Gameplay failures consumed sequence but malformed/reordered did not"), Model.Submit(1, Valid).bAccepted);
    const FString Accepted = Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Replay rejects"), Model.Submit(1, Valid).Reason, FString(TEXT("sequence")));
    TestEqual(TEXT("Replay cannot revise or alter state"), Model.GetSnapshot().ComputeDigest(), Accepted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopBotRateTest, "RedalertProof.Stop.TrustedBotParityAndRate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopBotRateTest::RunTest(const FString&)
{
    for (const int32 Owner : {1, ProofOwnerIds::Bot})
    {
        FProofCommandModel Model(1); Model.AddPlayer(Owner);
        const int32 Id = Model.SeedUnit(Owner, FVector::ZeroVector);
        TestTrue(TEXT("Human/bot ordinary move uses shared validator"), Model.Submit(Owner, StopOrder(1, EProofCommandType::Move, Id, INDEX_NONE, FVector(1000, 0, 0))).bAccepted);
        const FString Moving = Model.GetSnapshot().ComputeDigest();
        TestEqual(TEXT("Stop respects shared command budget"), Model.Submit(Owner, StopOrder(2, EProofCommandType::Stop, Id)).Reason, FString(TEXT("rate")));
        TestEqual(TEXT("Rate rejection changes no published state"), Model.GetSnapshot().ComputeDigest(), Moving);
        Model.Advance(1.f);
        TestEqual(TEXT("Rejected Stop did not cancel Move"), StopUnit(Model.GetSnapshot(), Id).Position, FVector(150, 0, 0));
        TestEqual(TEXT("Rate-rejected envelope cannot replay"), Model.Submit(Owner, StopOrder(2, EProofCommandType::Stop, Id)).Reason, FString(TEXT("sequence")));
        TestTrue(TEXT("Next Stop accepted after shared allowance reset"), Model.Submit(Owner, StopOrder(3, EProofCommandType::Stop, Id)).bAccepted);
        Model.Advance(10.f);
        TestEqual(TEXT("Trusted human/bot Stop has identical motion effect"), StopUnit(Model.GetSnapshot(), Id).Position, FVector(150, 0, 0));
    }
    FProofCommandModel Dead; Dead.AddPlayer(1); Dead.AddPlayer(2);
    const int32 Victim = Dead.SeedUnit(1, FVector::ZeroVector), Shooter = Dead.SeedUnit(2, FVector(100, 0, 0));
    TestTrue(TEXT("Real lethal attack admitted"), Dead.Submit(2, StopOrder(1, EProofCommandType::Attack, Shooter, Victim)).bAccepted);
    Dead.Advance(4.f);
    TestEqual(TEXT("Target actually dead"), StopUnit(Dead.GetSnapshot(), Victim).HitPoints, 0.f);
    const FString Killed = Dead.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Dead owner unit cannot Stop"), Dead.Submit(1, StopOrder(1, EProofCommandType::Stop, Victim)).Reason, FString(TEXT("ownership")));
    TestEqual(TEXT("Dead rejection cannot resurrect, revise or mutate"), Dead.GetSnapshot().ComputeDigest(), Killed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopResetTerminalTest, "RedalertProof.Stop.ResetAndTerminal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopResetTerminalTest::RunTest(const FString&)
{
    FProofCommandModel Model; Model.AddPlayer(1);
    const int32 First = Model.SeedUnit(1, FVector::ZeroVector);
    Model.Submit(1, StopOrder(1, EProofCommandType::Move, First, INDEX_NONE, FVector(1000, 0, 0)));
    Model.Submit(1, StopOrder(2, EProofCommandType::Stop, First));
    Model.Reset(); Model.AddPlayer(1);
    const int32 Reused = Model.SeedUnit(1, FVector::ZeroVector);
    TestEqual(TEXT("Reset reuses fixture IDs"), Reused, First);
    TestTrue(TEXT("Reset accepts first sequence again"), Model.Submit(1, StopOrder(1, EProofCommandType::Move, Reused, INDEX_NONE, FVector(1000, 0, 0))).bAccepted);
    Model.Advance(1.f);
    TestEqual(TEXT("Old Stop does not persist across model Reset"), StopUnit(Model.GetSnapshot(), Reused).Position, FVector(150, 0, 0));
    StopSeedPair(Model);
    const int32 Miner = StopKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester);
    TestTrue(TEXT("New skirmish accepts sequence one Stop"), Model.Submit(1, StopOrder(1, EProofCommandType::Stop, Miner)).bAccepted);
    TestTrue(TEXT("Ordinary opponent teardown ends match"), Model.RemoveSkirmishPlayer(ProofOwnerIds::Bot));
    const FProofSnapshot Finished = Model.GetSnapshot();
    TestTrue(TEXT("Actually terminal"), Finished.MatchPhase == EProofMatchPhase::Finished);
    TestEqual(TEXT("Stop cannot bypass terminal gate"), Model.Submit(1, StopOrder(2, EProofCommandType::Stop, Miner)).Reason, FString(TEXT("finished")));
    Model.Advance(10.f);
    TestEqual(TEXT("Terminal Stop and Advance cannot alter frozen state"), Model.GetSnapshot().ComputeDigest(), Finished.ComputeDigest());
    StopSeedPair(Model);
    const int32 NewMiner = StopKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester);
    TestEqual(TEXT("New skirmish may reuse unit IDs"), NewMiner, Miner);
    TestTrue(TEXT("New model generation resets sequence and terminal state"), Model.Submit(1, StopOrder(1, EProofCommandType::Move, NewMiner, INDEX_NONE, FVector(1000, 0, 0))).bAccepted);
    Model.Advance(1.f);
    TestEqual(TEXT("New generation can move normally"), StopUnit(Model.GetSnapshot(), NewMiner).Position, FVector(250, 0, 0));
    TestTrue(TEXT("New generation can then Stop"), Model.Submit(1, StopOrder(2, EProofCommandType::Stop, NewMiner)).bAccepted);
    return true;
}
#endif
