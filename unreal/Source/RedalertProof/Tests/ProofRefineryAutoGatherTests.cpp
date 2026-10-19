#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"

namespace
{
FProofCommand AutoMinerCommand(int32 Sequence, EProofCommandType Type, int32 Unit = INDEX_NONE,
    int32 Target = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand Command;
    Command.Sequence = Sequence; Command.Type = Type; Command.UnitId = Unit;
    Command.TargetUnitId = Target; Command.Destination = Destination;
    return Command;
}
FProofCommand AutoMinerBuild(int32 Sequence, FVector Position = FVector(300, 0, 0))
{ return AutoMinerCommand(Sequence, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, Position); }
const FProofUnitRecord& AutoMinerUnit(const FProofSnapshot& State, int32 Id)
{ return *State.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; }); }
const FProofPlayerRecord& AutoMinerPlayer(const FProofSnapshot& State, int32 Owner = 1)
{ return *State.Players.FindByPredicate([Owner](const FProofPlayerRecord& Player) { return Player.OwnerId == Owner; }); }
int32 AutoMinerTotal(const FProofSnapshot& State)
{
    int32 Total = 0;
    for (const auto& Resource : State.Resources) Total += Resource.Remaining;
    for (const auto& Unit : State.Units) Total += Unit.Cargo;
    for (const auto& Player : State.Players) Total += Player.TotalGathered;
    return Total;
}
bool AutoMinerSeed(FProofCommandModel& Model)
{ Model.StartSkirmish(); return Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector); }
bool AutoMinerDeliverAll(FProofCommandModel& Model)
{
    for (int32 Tick = 0; Tick < 240; ++Tick)
    {
        if (AutoMinerPlayer(Model.GetSnapshot()).TotalGathered == 500) return true;
        Model.Advance(0.5f);
    }
    return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryAutoStartTest, "RedalertProof.Skirmish.Economy.RefineryAutoGather.StartAndConservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryAutoStartTest::RunTest(const FString&)
{
    FProofCommandModel Model(1);
    if (!TestTrue(TEXT("Seed automatic start"), AutoMinerSeed(Model))) return false;
    const FProofSnapshot Before = Model.GetSnapshot();
    const int32 Starter = Before.Units[1].UnitId;
    if (!TestTrue(TEXT("Only BuildRefinery command accepted"), Model.Submit(1, AutoMinerBuild(1)).bAccepted)) return false;
    const FProofSnapshot Built = Model.GetSnapshot();
    const int32 Miner = Built.Units[Built.Units.Num() - 1].UnitId;
    TestEqual(TEXT("Two record insertions plus command revision only"), Built.Revision, Before.Revision + 3);
    TestEqual(TEXT("One cost, no initial income"), AutoMinerPlayer(Built).Credits, 250);
    TestEqual(TEXT("No ore extracted during submission"), Built.Resources[0].Remaining, 500);
    TestEqual(TEXT("No carried ore on submission"), AutoMinerUnit(Built, Miner).Cargo, 0);
    TestEqual(TEXT("Replay still rejects"), Model.Submit(1, AutoMinerBuild(1)).Reason, FString(TEXT("sequence")));
    TestEqual(TEXT("Replay leaves visible transaction unchanged"), Model.GetSnapshot().ComputeDigest(), Built.ComputeDigest());
    TestEqual(TEXT("Build consumes the existing single command allowance"),
        Model.Submit(1, AutoMinerCommand(2, EProofCommandType::Stop, Miner)).Reason, FString(TEXT("rate")));
    Model.Advance(0.f);
    TestEqual(TEXT("Zero advance cannot extract or move"), Model.GetSnapshot().ComputeDigest(), Built.ComputeDigest());
    Model.Advance(0.25f);
    const FProofSnapshot Travel = Model.GetSnapshot();
    TestTrue(TEXT("Granted miner automatically travels toward ore"),
        AutoMinerUnit(Travel, Miner).Position.Equals(FVector(562.5, 0, 0), 1.e-9));
    TestEqual(TEXT("Travel has not produced income"), AutoMinerPlayer(Travel).TotalGathered, 0);
    TestTrue(TEXT("Starter remains stationary without an order"), AutoMinerUnit(Travel, Starter).Position == Before.Units[1].Position);
    Model.Advance(0.25f);
    const FProofSnapshot Loaded = Model.GetSnapshot();
    TestEqual(TEXT("Granted miner loads through ordinary Advance"), AutoMinerUnit(Loaded, Miner).Cargo, 50);
    TestEqual(TEXT("Load consumes finite ore"), Loaded.Resources[0].Remaining, 450);
    TestEqual(TEXT("Load is not income"), AutoMinerPlayer(Loaded).Credits, 250);
    Model.Advance(2.f);
    const FProofSnapshot Deposited = Model.GetSnapshot();
    TestEqual(TEXT("Automatic trip deposits at own refinery"), AutoMinerPlayer(Deposited).TotalGathered, 50);
    TestEqual(TEXT("Deposit clears granted cargo"), AutoMinerUnit(Deposited, Miner).Cargo, 0);
    TestTrue(TEXT("Deposit physically reaches the refinery"), AutoMinerUnit(Deposited, Miner).Position.Equals(FVector(300, 0, 0), 1.e-9));
    for (int32 Tick = 0; Tick < 240; ++Tick)
    {
        Model.Advance(0.5f);
        const FProofSnapshot State = Model.GetSnapshot();
        TestEqual(TEXT("All ore stays remaining, carried or delivered"), AutoMinerTotal(State), 500);
        TestEqual(TEXT("Only deposits add credits"), AutoMinerPlayer(State).Credits, 250 + AutoMinerPlayer(State).TotalGathered);
        TestTrue(TEXT("Automatic order never enlists starter miner"), AutoMinerUnit(State, Starter).Position == Before.Units[1].Position && AutoMinerUnit(State, Starter).Cargo == 0);
    }
    const FProofSnapshot Finished = Model.GetSnapshot();
    TestEqual(TEXT("One granted miner completes the finite loop without Gather"), AutoMinerPlayer(Finished).TotalGathered, 500);
    TestEqual(TEXT("No extra units appear during mining"), Finished.Units.Num(), Built.Units.Num());
    TestTrue(TEXT("Auto order did not consume a command sequence"), Model.Submit(1, AutoMinerCommand(3, EProofCommandType::Stop, Miner)).bAccepted);
    // Two new grants run uninterrupted; no explicit Gather or Stop is issued.
    FProofCommandModel Multiple;
    if (!TestTrue(TEXT("Seed multiple auto miners"), AutoMinerSeed(Multiple))
        || !TestTrue(TEXT("First automatic grant"), Multiple.Submit(1, AutoMinerBuild(1)).bAccepted)) return false;
    const int32 First = Multiple.GetSnapshot().Units.Last().UnitId;
    if (!TestTrue(TEXT("Second automatic grant"), Multiple.Submit(1, AutoMinerBuild(2, FVector(0, 300, 0))).bAccepted)) return false;
    const int32 Second = Multiple.GetSnapshot().Units.Last().UnitId;
    bool bFirstLoaded = false, bSecondLoaded = false;
    for (int32 Tick = 0; Tick < 120; ++Tick)
    {
        Multiple.Advance(0.5f);
        const FProofSnapshot State = Multiple.GetSnapshot();
        bFirstLoaded |= AutoMinerUnit(State, First).Cargo > 0;
        bSecondLoaded |= AutoMinerUnit(State, Second).Cargo > 0;
        TestEqual(TEXT("Competing auto miners preserve aggregate finite ore"), AutoMinerTotal(State), 500);
        TestEqual(TEXT("Multiple auto miners credit only deliveries"), AutoMinerPlayer(State).Credits, 150 + AutoMinerPlayer(State).TotalGathered);
    }
    TestTrue(TEXT("Both granted miners actually load without manual orders"), bFirstLoaded && bSecondLoaded);
    TestEqual(TEXT("Multiple auto miners eventually deliver exactly the finite total"), AutoMinerPlayer(Multiple.GetSnapshot()).TotalGathered, 500);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryAutoTargetTest, "RedalertProof.Skirmish.Economy.RefineryAutoGather.InitialTargetAndTie", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryAutoTargetTest::RunTest(const FString&)
{
    // A foreign/teammate starter patch is ordinary shared ore, not ownership-restricted.
    // Both tied fields are 50 cm from the validated miner spawn (600,0,0).
    for (int32 Variant = 0; Variant < 4; ++Variant)
    {
        FProofCommandModel Model; Model.StartSkirmish();
        const FVector OtherHQ = (Variant == 0 || Variant == 3) ? FVector(-200, 0, 0) : FVector(100, 0, 0);
        if (Variant >= 2 && !TestTrue(TEXT("Seed lower-ID right ore"), Model.SeedSkirmishPlayer(2, 0, OtherHQ))) return false;
        if (!TestTrue(TEXT("Seed builder"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector))) return false;
        if (Variant < 2 && !TestTrue(TEXT("Seed alternate ore"), Model.SeedSkirmishPlayer(2, 0, OtherHQ))) return false;
        if (!TestTrue(TEXT("Build near two patches"), Model.Submit(1, AutoMinerBuild(1)).bAccepted)) return false;
        const FProofSnapshot Built = Model.GetSnapshot();
        const int32 Miner = Built.Units[Built.Units.Num() - 1].UnitId;
        TestTrue(TEXT("Known spawn anchors target comparison"), AutoMinerUnit(Built, Miner).Position == FVector(600, 0, 0));
        Model.Advance(0.5f);
        const FProofSnapshot After = Model.GetSnapshot();
        const double ExpectedX = Variant == 2 ? 650.0 : 550.0;
        TestTrue(TEXT("Nearest from miner spawn; exact ties choose lowest stable resource ID"),
            AutoMinerUnit(After, Miner).Position.Equals(FVector(ExpectedX, 0, 0), 1.e-9));
        TestEqual(TEXT("Selected nearest field is reached and loaded, not just approached in the same direction"), AutoMinerUnit(After, Miner).Cargo, 50);
        TestEqual(TEXT("Target selection cannot mint ore"), AutoMinerTotal(After), 1000);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryAutoOverrideTest, "RedalertProof.Skirmish.Economy.RefineryAutoGather.ExplicitOverrides", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryAutoOverrideTest::RunTest(const FString&)
{
    for (EProofCommandType Override : {EProofCommandType::Stop, EProofCommandType::Move, EProofCommandType::Gather})
    {
        FProofCommandModel Model;
        if (!TestTrue(TEXT("Seed override fixture"), AutoMinerSeed(Model))
            || !TestTrue(TEXT("Seed alternative field"), Model.SeedSkirmishPlayer(2, 0, FVector(0, 2000, 0)))) return false;
        const int32 Starter = Model.GetSnapshot().Units[1].UnitId;
        if (!TestTrue(TEXT("Existing miner receives manual Move"), Model.Submit(1,
            AutoMinerCommand(1, EProofCommandType::Move, Starter, INDEX_NONE, FVector(100, -600, 0))).bAccepted)
            || !TestTrue(TEXT("Build preserves existing order"), Model.Submit(1, AutoMinerBuild(2)).bAccepted)) return false;
        const FProofSnapshot Built = Model.GetSnapshot();
        const int32 Miner = Built.Units[Built.Units.Num() - 1].UnitId;
        const int32 OtherOre = Built.Resources[1].ResourceId;
        const FVector Destination(600, -600, 0);
        const FProofCommand Order = AutoMinerCommand(3, Override, Miner,
            Override == EProofCommandType::Gather ? OtherOre : INDEX_NONE, Destination);
        TestEqual(TEXT("Foreign command cannot replace automatic order"), Model.Submit(2, AutoMinerCommand(1, EProofCommandType::Stop, Miner)).Reason, FString(TEXT("ownership")));
        if (!TestTrue(TEXT("Explicit owner override accepted"), Model.Submit(1, Order).bAccepted)) return false;
        Model.Advance(0.5f);
        const FProofSnapshot After = Model.GetSnapshot();
        TestTrue(TEXT("Existing manually moving starter keeps its order"), AutoMinerUnit(After, Starter).Position == FVector(100, -75, 0));
        const FVector NewPosition = AutoMinerUnit(After, Miner).Position;
        TestTrue(TEXT("Explicit new order takes precedence immediately"), Override == EProofCommandType::Stop ? NewPosition == FVector(600, 0, 0)
            : Override == EProofCommandType::Move ? NewPosition == FVector(600, -75, 0) : NewPosition.Y > 0.0);
        if (Override != EProofCommandType::Gather)
        {
            for (int32 Tick = 0; Tick < 20; ++Tick) Model.Advance(1.f);
            const FProofSnapshot Waiting = Model.GetSnapshot();
            TestEqual(TEXT("Stop or completed Move never restarts harvesting"), AutoMinerPlayer(Waiting).TotalGathered, 0);
            TestEqual(TEXT("Explicit waiting miner retains empty cargo"), AutoMinerUnit(Waiting, Miner).Cargo, 0);
            TestTrue(TEXT("Explicit final position retained"), AutoMinerUnit(Waiting, Miner).Position ==
                (Override == EProofCommandType::Stop ? FVector(600, 0, 0) : Destination));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryAutoEmptyTest, "RedalertProof.Skirmish.Economy.RefineryAutoGather.NoOreAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryAutoEmptyTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    if (!TestTrue(TEXT("Seed depletion fixture"), AutoMinerSeed(Model))
        || !TestTrue(TEXT("Build automatic miner"), Model.Submit(1, AutoMinerBuild(1)).bAccepted)
        || !TestTrue(TEXT("Existing finite ore fully delivered"), AutoMinerDeliverAll(Model))) return false;
    const FProofSnapshot Empty = Model.GetSnapshot();
    if (!TestTrue(TEXT("Refinery still builds when no usable ore remains"), Model.Submit(1, AutoMinerBuild(2, FVector(0, 300, 0))).bAccepted)) return false;
    const FProofSnapshot Built = Model.GetSnapshot();
    const int32 Miner = Built.Units[Built.Units.Num() - 1].UnitId;
    TestEqual(TEXT("No-ore build still charges exactly one price"), AutoMinerPlayer(Built).Credits, AutoMinerPlayer(Empty).Credits - FProofCommandModel::RefineryCost);
    if (!TestTrue(TEXT("Later seed provides new ore without ordering waiting miners"), Model.SeedSkirmishPlayer(2, 0, FVector(0, 2000, 0)))) return false;
    for (int32 Tick = 0; Tick < 10; ++Tick) Model.Advance(1.f);
    const FProofSnapshot Waiting = Model.GetSnapshot();
    TestTrue(TEXT("No-ore grant remains idle even when new ore later appears"), AutoMinerUnit(Waiting, Miner).Position == AutoMinerUnit(Built, Miner).Position && AutoMinerUnit(Waiting, Miner).Cargo == 0);
    TestEqual(TEXT("No later income without an actual order"), AutoMinerPlayer(Waiting).TotalGathered, 500);
    if (!TestTrue(TEXT("Removing builder clears automatic orders"), Model.RemoveSkirmishPlayer(1))) return false;
    TestFalse(TEXT("Removed miners leave no records"), Model.GetSnapshot().Units.ContainsByPredicate([](const FProofUnitRecord& Unit) { return Unit.OwnerId == 1; }));
    if (!TestTrue(TEXT("Fresh model reset and seed"), AutoMinerSeed(Model))) return false;
    const FProofSnapshot Reset = Model.GetSnapshot();
    Model.Advance(10.f);
    const FProofSnapshot Advanced = Model.GetSnapshot();
    TestEqual(TEXT("Reset has only HQ and starter"), Advanced.Units.Num(), 2);
    TestTrue(TEXT("Reset starter never inherits an old automatic order"), Advanced.Units[1].Position == Reset.Units[1].Position && Advanced.Units[1].Cargo == 0);
    TestEqual(TEXT("Reset funds unchanged"), Advanced.Players[0].Credits, 350);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryAutoLossTest, "RedalertProof.Skirmish.Economy.RefineryAutoGather.RefineryLossAndRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryAutoLossTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    if (!TestTrue(TEXT("Seed recovery owner"), AutoMinerSeed(Model))
        || !TestTrue(TEXT("Seed attacking enemy"), Model.SeedSkirmishPlayer(100, 1, FVector(1800, 0, 0)))) return false;
    if (!TestTrue(TEXT("Enemy builds actual factory"), Model.Submit(100, AutoMinerCommand(1,
        EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(1500, 0, 0))).bAccepted)) return false;
    const int32 Factory = Model.GetSnapshot().Units.Last().UnitId;
    if (!TestTrue(TEXT("Enemy pays for combat"), Model.Submit(100, AutoMinerCommand(2, EProofCommandType::Produce, Factory)).bAccepted)) return false;
    Model.Advance(FProofCommandModel::ProductionDurationSeconds);
    const int32 Attacker = Model.GetSnapshot().Units.Last().UnitId;
    // Start the first auto-mining trip only after the real enemy is available,
    // preserving the loss-before-first-deposit premise without stopping Gather.
    if (!TestTrue(TEXT("Build original refinery"), Model.Submit(1, AutoMinerBuild(1)).bAccepted)) return false;
    const FProofSnapshot Built = Model.GetSnapshot();
    const int32 Miner = Built.Units[Built.Units.Num() - 1].UnitId;
    const int32 Refinery = Built.Units[Built.Units.Num() - 2].UnitId;
    if (!TestTrue(TEXT("Enemy attacks refinery through shared validator"), Model.Submit(100, AutoMinerCommand(3, EProofCommandType::Attack, Attacker, Refinery)).bAccepted)) return false;
    Model.Advance(5.f);
    TestEqual(TEXT("Automatic miner carries its first load"), AutoMinerUnit(Model.GetSnapshot(), Miner).Cargo, 50);
    Model.Advance(3.f);
    const FProofSnapshot Lost = Model.GetSnapshot();
    TestEqual(TEXT("Combat destroys refinery before deposit"), AutoMinerUnit(Lost, Refinery).HitPoints, 0.f);
    TestEqual(TEXT("No credit for destroyed destination"), AutoMinerPlayer(Lost).TotalGathered, 0);
    TestEqual(TEXT("Automatic order retains carried ore"), AutoMinerUnit(Lost, Miner).Cargo, 50);
    if (!TestTrue(TEXT("Enemy moves away without changing model health"), Model.Submit(100, AutoMinerCommand(4,
        EProofCommandType::Move, Attacker, INDEX_NONE, FVector(1800, 2000, 0))).bAccepted)) return false;
    Model.Advance(1.f);
    TestTrue(TEXT("Loaded miner waits at unchanged position without a live own refinery"), AutoMinerUnit(Model.GetSnapshot(), Miner).Position == AutoMinerUnit(Lost, Miner).Position);
    if (!TestTrue(TEXT("Owner rebuilds refinery"), Model.Submit(1, AutoMinerBuild(2, FVector(0, 300, 0))).bAccepted)) return false;
    Model.Advance(3.f);
    const FProofSnapshot Recovered = Model.GetSnapshot();
    TestEqual(TEXT("Existing automatic Gather deposits carried cargo after recovery"), AutoMinerUnit(Recovered, Miner).Cargo, 0);
    TestEqual(TEXT("Recovered cargo credited once"), AutoMinerPlayer(Recovered).TotalGathered, 50);
    TestEqual(TEXT("Combat losses do not duplicate finite ore"), AutoMinerTotal(Recovered), 1000);
    return true;
}
#endif
