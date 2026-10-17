#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"
#include "Misc/SecureHash.h"
#include <limits>

namespace
{
FProofCommand RallyCommand(int32 Sequence, EProofCommandType Type, int32 UnitId = INDEX_NONE,
    FVector Destination = FVector::ZeroVector, int32 TargetId = INDEX_NONE)
{
    FProofCommand Command;
    Command.Sequence = Sequence; Command.Type = Type; Command.UnitId = UnitId;
    Command.Destination = Destination; Command.TargetUnitId = TargetId;
    return Command;
}
const FProofUnitRecord& RallyUnit(const FProofSnapshot& Snapshot, int32 Id)
{
    return *Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; });
}
int32 RallyKind(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    int32 Id = INDEX_NONE;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
        if (Unit.OwnerId == Owner && Unit.Kind == Kind) Id = FMath::Max(Id, Unit.UnitId);
    return Id;
}
int32 RallyCredits(const FProofSnapshot& Snapshot, int32 Owner = 1)
{
    return Snapshot.Players.FindByPredicate([Owner](const FProofPlayerRecord& Player) { return Player.OwnerId == Owner; })->Credits;
}
void RallySeed(FProofCommandModel& Model)
{
    Model.StartSkirmish();
    Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Model.SeedSkirmishPlayer(100, 1, FVector(5000, 0, 0));
}
int32 RallyBuild(FProofCommandModel& Model, int32 Owner = 1)
{
    const FVector Site = Owner == 1 ? FVector(0, 350, 0) : FVector(5000, 350, 0);
    if (!Model.Submit(Owner, RallyCommand(1, EProofCommandType::BuildFactory, INDEX_NONE, Site)).bAccepted) return INDEX_NONE;
    return RallyKind(Model.GetSnapshot(), Owner, EProofUnitKind::Factory);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofFactoryRallyProductionTest, "RedalertProof.FactoryRally.PaidSpawnAndOverrides", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofFactoryRallyProductionTest::RunTest(const FString&)
{
    FProofCommandModel Model, Baseline;
    RallySeed(Model); RallySeed(Baseline);
    const int32 Factory = RallyBuild(Model), BaselineFactory = RallyBuild(Baseline);
    if (!TestTrue(TEXT("Ordinary factory built"), Factory > 0 && BaselineFactory > 0)) return false;
    TestEqual(TEXT("Stop retains ordinal six"), static_cast<uint8>(EProofCommandType::Stop), static_cast<uint8>(6));
    TestEqual(TEXT("SetRally appends ordinal seven"), static_cast<uint8>(EProofCommandType::SetRally), static_cast<uint8>(7));
    const FProofSnapshot Before = Model.GetSnapshot();
    TestFalse(TEXT("New factory has no rally"), RallyUnit(Before, Factory).bHasRallyPoint);
    const FVector FirstTarget(-800, 700, 0), SecondTarget(800, 700, 0);
    TestTrue(TEXT("Owned factory rally accepted"), Model.Submit(1, RallyCommand(2, EProofCommandType::SetRally, Factory, FirstTarget)).bAccepted);
    TestEqual(TEXT("Rally setting is free"), RallyCredits(Model.GetSnapshot()), RallyCredits(Before));
    TestEqual(TEXT("Rally setting creates no units"), Model.GetSnapshot().Units.Num(), Before.Units.Num());
    TestTrue(TEXT("Normally paid production accepted"), Model.Submit(1, RallyCommand(3, EProofCommandType::Produce, Factory)).bAccepted);
    TestTrue(TEXT("Baseline production accepted"), Baseline.Submit(1, RallyCommand(2, EProofCommandType::Produce, BaselineFactory)).bAccepted);
    const FProofSnapshot Spawned = Model.GetSnapshot();
    const int32 First = RallyKind(Spawned, 1, EProofUnitKind::Combat);
    TestEqual(TEXT("Exactly ordinary production cost"), RallyCredits(Spawned), RallyCredits(Before) - FProofCommandModel::ProductionCost);
    TestTrue(TEXT("Rally does not teleport or change spawn selection"), RallyUnit(Spawned, First).Position.Equals(
        RallyUnit(Baseline.GetSnapshot(), RallyKind(Baseline.GetSnapshot(), 1, EProofUnitKind::Combat)).Position, 0.0));
    Model.Advance(0.1f); Baseline.Advance(0.1f);
    TestTrue(TEXT("Produced unit actually moves toward rally"), FVector::Distance(RallyUnit(Model.GetSnapshot(), First).Position, FirstTarget)
        < FVector::Distance(RallyUnit(Spawned, First).Position, FirstTarget));
    TestTrue(TEXT("Default factory production stays at spawn"), RallyUnit(Baseline.GetSnapshot(), First).Position.Equals(RallyUnit(Spawned, First).Position, 0.0));
    TestTrue(TEXT("Replace factory rally"), Model.Submit(1, RallyCommand(4, EProofCommandType::SetRally, Factory, SecondTarget)).bAccepted);
    TestTrue(TEXT("Second paid production"), Model.Submit(1, RallyCommand(5, EProofCommandType::Produce, Factory)).bAccepted);
    const FProofSnapshot Both = Model.GetSnapshot();
    const int32 Second = RallyKind(Both, 1, EProofUnitKind::Combat);
    Model.Advance(0.1f);
    TestTrue(TEXT("Existing unit keeps first rally order"), FVector::Distance(RallyUnit(Model.GetSnapshot(), First).Position, FirstTarget)
        < FVector::Distance(RallyUnit(Both, First).Position, FirstTarget));
    TestTrue(TEXT("Next unit uses replaced rally"), FVector::Distance(RallyUnit(Model.GetSnapshot(), Second).Position, SecondTarget)
        < FVector::Distance(RallyUnit(Both, Second).Position, SecondTarget));
    const FVector Override(400, -400, 0);
    TestTrue(TEXT("Ordinary Move replaces generated order"), Model.Submit(1, RallyCommand(6, EProofCommandType::Move, First, Override)).bAccepted);
    TestTrue(TEXT("Ordinary Stop cancels generated order"), Model.Submit(1, RallyCommand(7, EProofCommandType::Stop, Second)).bAccepted);
    const FProofSnapshot Overridden = Model.GetSnapshot();
    Model.Advance(0.1f);
    TestTrue(TEXT("Stopped produced unit stays stationary"), RallyUnit(Model.GetSnapshot(), Second).Position.Equals(RallyUnit(Overridden, Second).Position, 0.0));
    TestTrue(TEXT("Move override makes real progress"), FVector::Distance(RallyUnit(Model.GetSnapshot(), First).Position, Override)
        < FVector::Distance(RallyUnit(Overridden, First).Position, Override));
    TestTrue(TEXT("Mobile Stop preserves factory rally"), RallyUnit(Model.GetSnapshot(), Factory).bHasRallyPoint
        && RallyUnit(Model.GetSnapshot(), Factory).RallyPoint.Equals(SecondTarget, 0.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofFactoryRallyAuthorityTest, "RedalertProof.FactoryRally.AuthorityAndAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofFactoryRallyAuthorityTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    RallySeed(Model);
    const int32 Factory = RallyBuild(Model);
    const int32 Miner = RallyKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester);
    const FVector Target(-700, 900, 0);
    const auto RejectUnchanged = [this, &Model](int32 Owner, const FProofCommand& Command, const TCHAR* Reason)
    {
        const FString Before = Model.GetSnapshot().ComputeDigest();
        const FProofCommandResult Receipt = Model.Submit(Owner, Command);
        TestFalse(TEXT("Invalid rally rejected"), Receipt.bAccepted);
        TestEqual(TEXT("Exact rejection reason"), Receipt.Reason, FString(Reason));
        TestEqual(TEXT("Rejection does not mutate snapshot"), Model.GetSnapshot().ComputeDigest(), Before);
    };
    RejectUnchanged(1, RallyCommand(2, EProofCommandType::SetRally, Factory, FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0)), TEXT("malformed"));
    RejectUnchanged(1, RallyCommand(2, EProofCommandType::SetRally, Factory, Target, Miner), TEXT("malformed"));
    RejectUnchanged(1, RallyCommand(2, EProofCommandType::SetRally, INDEX_NONE, Target), TEXT("malformed"));
    RejectUnchanged(1, RallyCommand(2, EProofCommandType::SetRally, Factory, FVector(0, 0, 1)), TEXT("position"));
    RejectUnchanged(1, RallyCommand(3, EProofCommandType::SetRally, Factory, FVector(10001, 0, 0)), TEXT("position"));
    RejectUnchanged(1, RallyCommand(4, EProofCommandType::SetRally, Miner, Target), TEXT("role"));
    RejectUnchanged(100, RallyCommand(1, EProofCommandType::SetRally, Factory, Target), TEXT("ownership"));
    RejectUnchanged(1, RallyCommand(5, EProofCommandType::SetRally, MAX_int32, Target), TEXT("ownership"));
    TestTrue(TEXT("Gameplay failures consumed sequences; valid next request accepted"), Model.Submit(1, RallyCommand(6, EProofCommandType::SetRally, Factory, Target)).bAccepted);
    RejectUnchanged(1, RallyCommand(6, EProofCommandType::SetRally, Factory, FVector::ZeroVector), TEXT("sequence"));
    const int32 BotFactory = RallyBuild(Model, 100); // Owner100 consumed one failed request.
    TestEqual(TEXT("Bot replay cannot build with sequence one"), BotFactory, INDEX_NONE);
    TestTrue(TEXT("Bot next ordinary build"), Model.Submit(100, RallyCommand(2, EProofCommandType::BuildFactory, INDEX_NONE, FVector(5000, 350, 0))).bAccepted);
    const int32 ActualBotFactory = RallyKind(Model.GetSnapshot(), 100, EProofUnitKind::Factory);
    TestTrue(TEXT("Bot uses identical rally validator"), Model.Submit(100, RallyCommand(3, EProofCommandType::SetRally, ActualBotFactory, FVector(4200, 700, 0))).bAccepted);
    TestTrue(TEXT("Bot pays for ordinary rally production"), Model.Submit(100, RallyCommand(4, EProofCommandType::Produce, ActualBotFactory)).bAccepted);
    const int32 BotCombat = RallyKind(Model.GetSnapshot(), 100, EProofUnitKind::Combat);
    const FVector BotSpawn = RallyUnit(Model.GetSnapshot(), BotCombat).Position;
    Model.Advance(0.1f);
    TestTrue(TEXT("Bot-produced unit uses same generated Move"), FVector::Distance(RallyUnit(Model.GetSnapshot(), BotCombat).Position, FVector(4200, 700, 0))
        < FVector::Distance(BotSpawn, FVector(4200, 700, 0)));
    FProofCommandModel Limited(1);
    RallySeed(Limited);
    const int32 LimitedFactory = RallyBuild(Limited);
    TestEqual(TEXT("SetRally observes ordinary allowance"), Limited.Submit(1, RallyCommand(2, EProofCommandType::SetRally, LimitedFactory, Target)).Reason, FString(TEXT("rate")));
    Limited.Advance(0.f);
    TestTrue(TEXT("Rate rejection consumes sequence without resetting allowance"), Limited.Submit(1, RallyCommand(3, EProofCommandType::SetRally, LimitedFactory, Target)).bAccepted);
    FProofCommandModel Legacy;
    Legacy.AddPlayer(1);
    const int32 LegacyUnit = Legacy.SeedUnit(1, FVector::ZeroVector);
    const FString LegacyBefore = Legacy.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Legacy rejects unsupported rally before mutation"), Legacy.Submit(1, RallyCommand(1, EProofCommandType::SetRally, LegacyUnit, Target)).Reason, FString(TEXT("malformed")));
    TestEqual(TEXT("Legacy snapshot unchanged"), Legacy.GetSnapshot().ComputeDigest(), LegacyBefore);
    TestTrue(TEXT("Unsupported legacy request does not consume sequence"), Legacy.Submit(1, RallyCommand(1, EProofCommandType::Move, LegacyUnit, Target)).bAccepted);
    FProofCommandModel Finished;
    RallySeed(Finished);
    const int32 FinishedFactory = RallyBuild(Finished);
    TestTrue(TEXT("Normal teardown reaches terminal fixture"), Finished.RemoveSkirmishPlayer(100));
    TestEqual(TEXT("Terminal fixture actually finished"), Finished.GetSnapshot().MatchPhase, EProofMatchPhase::Finished);
    const FString TerminalDigest = Finished.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Finished match rejects rally"), Finished.Submit(1, RallyCommand(2, EProofCommandType::SetRally, FinishedFactory, Target)).Reason, FString(TEXT("finished")));
    TestEqual(TEXT("Terminal rejection changes no snapshot"), Finished.GetSnapshot().ComputeDigest(), TerminalDigest);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofFactoryRallyFailureTest, "RedalertProof.FactoryRally.FailedProductionIsAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofFactoryRallyFailureTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    RallySeed(Model);
    const int32 Factory = RallyBuild(Model);
    TestTrue(TEXT("Set rally before blocked production"), Model.Submit(1, RallyCommand(2, EProofCommandType::SetRally, Factory, FVector(-900, 600, 0))).bAccepted);
    const FVector Center = RallyUnit(Model.GetSnapshot(), Factory).Position;
    const FVector Directions[] = {FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0),
        FVector(1,1,0),FVector(-1,1,0),FVector(1,-1,0),FVector(-1,-1,0)};
    int32 Owner = 2;
    for (int32 Ring = 1; Ring <= 3; ++Ring)
        for (const FVector& Direction : Directions)
            TestTrue(TEXT("Seed ordinary blocker seat"), Model.SeedSkirmishPlayer(Owner++, 0, Center + Direction * (300.0 * Ring)));
    FVector Spawn;
    TestFalse(TEXT("Fixture exhausts actual spawn helper"), FProofCommandModel::FindSkirmishSpawn(Model.GetSnapshot(), Center, Spawn));
    const FString Before = Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Rally cannot bypass spawn failure"), Model.Submit(1, RallyCommand(3, EProofCommandType::Produce, Factory)).Reason, FString(TEXT("spawn")));
    TestEqual(TEXT("Failed production preserves all snapshot state/cost/IDs/rally"), Model.GetSnapshot().ComputeDigest(), Before);
    for (int32 BlockerOwner = 2; BlockerOwner < Owner; ++BlockerOwner) Model.RemoveSkirmishPlayer(BlockerOwner);
    TestTrue(TEXT("After room becomes available normal production succeeds"), Model.Submit(1, RallyCommand(4, EProofCommandType::Produce, Factory)).bAccepted);
    const int32 Combat = RallyKind(Model.GetSnapshot(), 1, EProofUnitKind::Combat);
    const FProofSnapshot Produced = Model.GetSnapshot();
    Model.Advance(0.1f);
    TestTrue(TEXT("Only successful production receives rally movement"), !RallyUnit(Model.GetSnapshot(), Combat).Position.Equals(RallyUnit(Produced, Combat).Position, 0.0));
    // Spend remaining funds through ordinary paid production, then verify no
    // phantom spawn/order/cost change on a rejected production request.
    for (int32 Sequence = 5; Sequence <= 7; ++Sequence)
        TestTrue(TEXT("Ordinary paid production spends remaining credits"), Model.Submit(1, RallyCommand(Sequence, EProofCommandType::Produce, Factory)).bAccepted);
    const FString Empty = Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Rally cannot bypass production cost"), Model.Submit(1, RallyCommand(8, EProofCommandType::Produce, Factory)).Reason, FString(TEXT("resources")));
    TestEqual(TEXT("Resource rejection leaves state unchanged"), Model.GetSnapshot().ComputeDigest(), Empty);
    FProofCommandModel Capacity;
    RallySeed(Capacity);
    TestTrue(TEXT("Build ordinary refinery to fund cap fixture"), Capacity.Submit(1, RallyCommand(1, EProofCommandType::BuildRefinery, INDEX_NONE, FVector(350, 0, 0))).bAccepted);
    TestTrue(TEXT("Build ordinary factory for cap fixture"), Capacity.Submit(1, RallyCommand(2, EProofCommandType::BuildFactory, INDEX_NONE, FVector(0, 350, 0))).bAccepted);
    const int32 CapacityFactory = RallyKind(Capacity.GetSnapshot(), 1, EProofUnitKind::Factory);
    const int32 Miner = RallyKind(Capacity.GetSnapshot(), 1, EProofUnitKind::Harvester);
    const int32 Ore = Capacity.GetSnapshot().Resources[0].ResourceId;
    TestTrue(TEXT("Fund capacity through real Gather"), Capacity.Submit(1, RallyCommand(3, EProofCommandType::Gather, Miner, FVector::ZeroVector, Ore)).bAccepted);
    for (int32 Tick = 0; Tick < 600; ++Tick) Capacity.Advance(0.1f);
    TestTrue(TEXT("Cap fixture has enough normal resources"), RallyCredits(Capacity.GetSnapshot()) >= 400);
    TestTrue(TEXT("Rally at ordinary capacity fixture"), Capacity.Submit(1, RallyCommand(4, EProofCommandType::SetRally, CapacityFactory, FVector(-900, 600, 0))).bAccepted);
    for (int32 Sequence = 5; Sequence <= 11; ++Sequence)
        TestTrue(TEXT("Produce up to existing live-unit cap"), Capacity.Submit(1, RallyCommand(Sequence, EProofCommandType::Produce, CapacityFactory)).bAccepted);
    const FString AtCap = Capacity.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Rally cannot bypass existing cap"), Capacity.Submit(1, RallyCommand(12, EProofCommandType::Produce, CapacityFactory)).Reason, FString(TEXT("capacity")));
    TestEqual(TEXT("Capacity failure preserves credits/units/rally"), Capacity.GetSnapshot().ComputeDigest(), AtCap);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofFactoryRallyLifecycleTest, "RedalertProof.FactoryRally.DeathResetAndDigest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofFactoryRallyLifecycleTest::RunTest(const FString&)
{
    FProofSnapshot Plain;
    FProofUnitRecord PlainUnit;
    PlainUnit.UnitId = 1; PlainUnit.OwnerId = 1;
    Plain.Units.Add(PlainUnit);
    TestEqual(TEXT("Default record retains pre-rally digest contract"), Plain.ComputeDigest(),
        FMD5::HashAnsiString(TEXT("revision:0;epoch:0;skirmish:0;phase:0;winner:-1;u:1,1,0,0,0,100,0,0;")));
    FProofCommandModel Model;
    RallySeed(Model);
    const int32 Factory = RallyBuild(Model);
    const FVector Target(-700, 800, 0);
    TestTrue(TEXT("Set lifecycle rally"), Model.Submit(1, RallyCommand(2, EProofCommandType::SetRally, Factory, Target)).bAccepted);
    FProofSnapshot Original = Model.GetSnapshot(), Changed = Original;
    Changed.Units.FindByPredicate([Factory](const FProofUnitRecord& U) { return U.UnitId == Factory; })->RallyPoint.X += 0.000001;
    TestTrue(TEXT("Digest commits full precision rally destination"), Changed.ComputeDigest() != Original.ComputeDigest());
    Changed = Original;
    Changed.Units.FindByPredicate([Factory](const FProofUnitRecord& U) { return U.UnitId == Factory; })->bHasRallyPoint = false;
    TestTrue(TEXT("Digest commits rally enabled flag even for nonzero disabled point"), Changed.ComputeDigest() != Original.ComputeDigest());
    const int32 BotFactory = RallyBuild(Model, 100);
    TestTrue(TEXT("Produce ordinary enemy attacker"), Model.Submit(100, RallyCommand(2, EProofCommandType::Produce, BotFactory)).bAccepted);
    const int32 Attacker = RallyKind(Model.GetSnapshot(), 100, EProofUnitKind::Combat);
    TestTrue(TEXT("Attack rally factory through ordinary command"), Model.Submit(100, RallyCommand(3, EProofCommandType::Attack, Attacker, FVector::ZeroVector, Factory)).bAccepted);
    for (int32 Tick = 0; Tick < 600; ++Tick) Model.Advance(0.1f);
    const FProofSnapshot Dead = Model.GetSnapshot();
    TestEqual(TEXT("Death fixture keeps normal playing phase"), Dead.MatchPhase, EProofMatchPhase::Playing);
    TestTrue(TEXT("Actual combat killed factory"), RallyUnit(Dead, Factory).HitPoints <= 0.f);
    TestFalse(TEXT("Dead factory clears rally flag"), RallyUnit(Dead, Factory).bHasRallyPoint);
    TestTrue(TEXT("Dead factory clears rally coordinates"), RallyUnit(Dead, Factory).RallyPoint.Equals(FVector::ZeroVector, 0.0));
    TestEqual(TEXT("Dead factory cannot set rally"), Model.Submit(1, RallyCommand(3, EProofCommandType::SetRally, Factory, Target)).Reason, FString(TEXT("ownership")));
    TestEqual(TEXT("Dead factory cannot produce"), Model.Submit(1, RallyCommand(4, EProofCommandType::Produce, Factory)).Reason, FString(TEXT("factory")));
    RallySeed(Model);
    const int32 ReusedFactory = RallyBuild(Model);
    TestEqual(TEXT("Reset fixture reuses same factory ID"), ReusedFactory, Factory);
    TestFalse(TEXT("Reset factory never inherits old rally"), RallyUnit(Model.GetSnapshot(), ReusedFactory).bHasRallyPoint);
    TestTrue(TEXT("Set rally before seat removal"), Model.Submit(1, RallyCommand(2, EProofCommandType::SetRally, ReusedFactory, Target)).bAccepted);
    // Keep a real teammate HQ so ordinary seat removal does not end the match
    // before the rejoin assertion. Terminal removal is covered separately.
    TestTrue(TEXT("Seed teammate for continuing-session teardown"), Model.SeedSkirmishPlayer(2, 0, FVector(-5000, 0, 0)));
    TestTrue(TEXT("Remove rally owner through normal teardown"), Model.RemoveSkirmishPlayer(1));
    TestFalse(TEXT("Removal erases rally owner's records"), Model.GetSnapshot().Units.ContainsByPredicate(
        [](const FProofUnitRecord& Unit) { return Unit.OwnerId == 1; }));
    TestTrue(TEXT("Rejoin normal seat"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    const int32 RejoinedFactory = RallyBuild(Model);
    TestFalse(TEXT("Rejoined factory has no rally"), RallyUnit(Model.GetSnapshot(), RejoinedFactory).bHasRallyPoint);
    return true;
}
#endif
