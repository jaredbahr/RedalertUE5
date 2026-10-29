#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"

#include <limits>

namespace
{
FProofCommand MinerCommand(int32 Sequence, EProofCommandType Type, FVector Destination = FVector::ZeroVector,
    int32 UnitId = INDEX_NONE, int32 TargetId = INDEX_NONE)
{
    FProofCommand Command;
    Command.Sequence = Sequence;
    Command.Type = Type;
    Command.Destination = Destination;
    Command.UnitId = UnitId;
    Command.TargetUnitId = TargetId;
    return Command;
}

int32 CountMinerKind(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    return Snapshot.Units.FilterByPredicate([Owner, Kind](const FProofUnitRecord& Unit)
        { return Unit.OwnerId == Owner && Unit.Kind == Kind && Unit.HitPoints > 0.f; }).Num();
}

int32 MinerKindId(const FProofSnapshot& Snapshot, EProofUnitKind Kind)
{
    const FProofUnitRecord* Found = Snapshot.Units.FindByPredicate([Kind](const FProofUnitRecord& Unit)
        { return Unit.OwnerId == 1 && Unit.Kind == Kind && Unit.HitPoints > 0.f; });
    return Found ? Found->UnitId : INDEX_NONE;
}

int32 MaximumMinerId(const FProofSnapshot& Snapshot)
{
    int32 Maximum = 0;
    for (const FProofUnitRecord& Unit : Snapshot.Units) { Maximum = FMath::Max(Maximum, Unit.UnitId); }
    return Maximum;
}

int32 MinerCredits(const FProofSnapshot& Snapshot)
{
    const FProofPlayerRecord* Player = Snapshot.Players.FindByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 1; });
    return Player ? Player->Credits : INDEX_NONE;
}

// Build through real commands and finite ore to reach capacity without a
// mutable snapshot seam or granting fixture credits.
bool PrepareMinerCapacity(FProofCommandModel& Model, int32 CombatCount)
{
    Model.StartSkirmish();
    if (!Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector)) { return false; }
    const FProofSnapshot Opening = Model.GetSnapshot();
    const int32 Starter = MinerKindId(Opening, EProofUnitKind::Harvester);
    if (!Model.Submit(1, MinerCommand(1, EProofCommandType::BuildRefinery, FVector(300, 0, 0))).bAccepted
        || !Model.Submit(1, MinerCommand(2, EProofCommandType::BuildFactory, FVector(-300, 0, 0))).bAccepted
        || !Model.Submit(1, MinerCommand(3, EProofCommandType::Gather, FVector::ZeroVector, Starter, Opening.Resources[0].ResourceId)).bAccepted) { return false; }
    for (int32 Tick = 0; Tick < 40; ++Tick) { Model.Advance(2.f); }
    const int32 Factory = MinerKindId(Model.GetSnapshot(), EProofUnitKind::Factory);
    for (int32 Index = 0; Index < CombatCount; ++Index)
    {
        if (!Model.Submit(1, MinerCommand(4 + Index, EProofCommandType::Produce, FVector::ZeroVector, Factory)).bAccepted) { return false; }
        Model.Advance(FProofCommandModel::ProductionDurationSeconds);
        if (!Model.GetSnapshot().ProductionJobs.IsEmpty()) { return false; }
    }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryMinerGrantTest, "RedalertProof.Skirmish.Economy.RefineryMiner.GrantReplayAndExistingCargo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryMinerGrantTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    if (!TestTrue(TEXT("Seed owner"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector))
        || !TestTrue(TEXT("Seed teammate"), Model.SeedSkirmishPlayer(2, 0, FVector(0, 3000, 0)))) { return false; }
    const FProofSnapshot Opening = Model.GetSnapshot();
    TestEqual(TEXT("Starting miner remains exactly one"), CountMinerKind(Opening, 1, EProofUnitKind::Harvester), 1);
    const int32 Starter = MinerKindId(Opening, EProofUnitKind::Harvester);
    const FProofCommand Build = MinerCommand(1, EProofCommandType::BuildRefinery, FVector(300, 0, 0));
    if (!TestTrue(TEXT("Refinery build accepted"), Model.Submit(1, Build).bAccepted)) { return false; }
    const FProofSnapshot Built = Model.GetSnapshot();
    TestEqual(TEXT("One refinery"), CountMinerKind(Built, 1, EProofUnitKind::Refinery), 1);
    TestEqual(TEXT("One additional existing harvester kind"), CountMinerKind(Built, 1, EProofUnitKind::Harvester), 2);
    TestEqual(TEXT("Only the existing refinery price is charged"), MinerCredits(Built), 250);
    const FProofUnitRecord& Granted = Built.Units.Last();
    TestTrue(TEXT("Granted miner belongs to builder"), Granted.OwnerId == 1 && Granted.Kind == EProofUnitKind::Harvester);
    TestEqual(TEXT("Granted cargo starts empty"), Granted.Cargo, 0);
    TestEqual(TEXT("Existing miner health is used"), Granted.HitPoints, 100.f);
    TestTrue(TEXT("Grant uses the first clear existing spawn preference"), Granted.Position.Equals(FVector(600, 0, 0), 0.0));
    for (const FProofUnitRecord& Other : Built.Units)
    {
        if (Other.UnitId != Granted.UnitId) { TestTrue(TEXT("Granted center is separately pickable"), FVector::Dist(Granted.Position, Other.Position) >= 260.0); }
    }
    TestEqual(TEXT("Replayed build rejects"), Model.Submit(1, Build).Reason, FString(TEXT("sequence")));
    TestEqual(TEXT("Replay cannot grant, debit or revise"), Model.GetSnapshot().ComputeDigest(), Built.ComputeDigest());
    TestEqual(TEXT("Teammate cannot command granted miner"), Model.Submit(2, MinerCommand(1, EProofCommandType::Move, FVector(900, 0, 0), Granted.UnitId)).Reason, FString(TEXT("ownership")));
    TestEqual(TEXT("Ownership rejection is snapshot atomic"), Model.GetSnapshot().ComputeDigest(), Built.ComputeDigest());
    // Keep this existing-order preservation test to one active miner.
    if (!TestTrue(TEXT("Stop first incidental grant"), Model.Submit(1, MinerCommand(2, EProofCommandType::Stop, FVector::ZeroVector, Granted.UnitId)).bAccepted)) return false;
    if (!TestTrue(TEXT("Original miner gathers"), Model.Submit(1, MinerCommand(3, EProofCommandType::Gather, FVector::ZeroVector, Starter, Opening.Resources[0].ResourceId)).bAccepted)) { return false; }
    Model.Advance(3.f);
    const FProofSnapshot Loaded = Model.GetSnapshot();
    const FProofUnitRecord* LoadedStarter = Loaded.Units.FindByPredicate([Starter](const FProofUnitRecord& U) { return U.UnitId == Starter; });
    if (!TestNotNull(TEXT("Original miner retained"), LoadedStarter)) { return false; }
    TestEqual(TEXT("Original miner carries a full load"), LoadedStarter->Cargo, 50);
    if (!TestTrue(TEXT("Second refinery while miner returns"), Model.Submit(1, MinerCommand(4, EProofCommandType::BuildRefinery, FVector(0, 300, 0))).bAccepted)) { return false; }
    const FProofSnapshot Second = Model.GetSnapshot();
    const FProofUnitRecord* Retained = Second.Units.FindByPredicate([Starter](const FProofUnitRecord& U) { return U.UnitId == Starter; });
    if (!TestNotNull(TEXT("Original miner remains after second grant"), Retained)) { return false; }
    TestTrue(TEXT("Build preserves original cargo and position"), Retained->Cargo == LoadedStarter->Cargo && Retained->Position.Equals(LoadedStarter->Position, 0.0));
    if (!TestTrue(TEXT("Stop second incidental grant"), Model.Submit(1, MinerCommand(5, EProofCommandType::Stop, FVector::ZeroVector, Second.Units.Last().UnitId)).bAccepted)) return false;
    Model.Advance(2.f);
    TestEqual(TEXT("Original return order still deposits cargo"), Model.GetSnapshot().Players[0].TotalGathered, 50);
    for (int32 Tick = 0; Tick < 20; ++Tick) { Model.Advance(1.f); }
    TestEqual(TEXT("Advance cannot regrant miners"), CountMinerKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester), 3);
    TestEqual(TEXT("Teammate miner count unchanged"), CountMinerKind(Model.GetSnapshot(), 2, EProofUnitKind::Harvester), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryMinerCapacityTest, "RedalertProof.Skirmish.Economy.RefineryMiner.CapacityAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryMinerCapacityTest::RunTest(const FString&)
{
    for (int32 CombatCount : {5, 6})
    {
        FProofCommandModel Model;
        if (!TestTrue(TEXT("Reach capacity using earned credits"), PrepareMinerCapacity(Model, CombatCount))) { return false; }
        const FProofSnapshot Before = Model.GetSnapshot();
        TestEqual(TEXT("Declared pre-build live count"), Before.Units.Num(), 5 + CombatCount);
        const int32 NextSequence = 4 + CombatCount;
        const FProofCommandResult Build = Model.Submit(1, MinerCommand(NextSequence, EProofCommandType::BuildRefinery, FVector(0, 600, 0)));
        if (CombatCount == 5)
        {
            TestTrue(TEXT("Ten live records leave both required slots"), Build.bAccepted);
            const FProofSnapshot After = Model.GetSnapshot();
            TestEqual(TEXT("Exactly twelve records after grant"), After.Units.Num(), FProofCommandModel::SkirmishRecordCap);
            TestEqual(TEXT("Exactly two consecutive IDs consumed"), MaximumMinerId(After), MaximumMinerId(Before) + 2);
            TestEqual(TEXT("Single cost at capacity boundary"), MinerCredits(After), MinerCredits(Before) - FProofCommandModel::RefineryCost);
        }
        else
        {
            TestEqual(TEXT("Eleven live records reject two-record build"), Build.Reason, FString(TEXT("capacity")));
            TestEqual(TEXT("Rejected build leaves snapshot unchanged"), Model.GetSnapshot().ComputeDigest(), Before.ComputeDigest());
            const int32 Factory = MinerKindId(Before, EProofUnitKind::Factory);
            TestTrue(TEXT("Rejected envelope consumes normal sequence only"), Model.Submit(1, MinerCommand(NextSequence + 1, EProofCommandType::Produce, FVector::ZeroVector, Factory)).bAccepted);
            TestEqual(TEXT("Neither rejected build nor production start consumes IDs"), MaximumMinerId(Model.GetSnapshot()), MaximumMinerId(Before));
            Model.Advance(FProofCommandModel::ProductionDurationSeconds);
            TestEqual(TEXT("Rejected build consumes no IDs before real production completes"), MaximumMinerId(Model.GetSnapshot()), MaximumMinerId(Before) + 1);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryMinerIdBoundaryTest, "RedalertProof.Skirmish.Economy.RefineryMiner.IdExhaustionAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryMinerIdBoundaryTest::RunTest(const FString&)
{
    for (int32 Available : {0, 1, 2})
    {
        FProofCommandModel Model;
        Model.StartSkirmish();
        if (!TestTrue(TEXT("Seed ID exhaustion fixture"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector))) { return false; }
        Model.NextUnitId = MAX_int32 - Available;
        const FProofSnapshot Before = Model.GetSnapshot();
        const FProofCommandResult Build = Model.Submit(1, MinerCommand(1, EProofCommandType::BuildRefinery, FVector(300, 0, 0)));
        if (Available < 2)
        {
            TestEqual(TEXT("Both IDs are required before any allocation"), Build.Reason, FString(TEXT("capacity")));
            TestEqual(TEXT("ID exhaustion preserves snapshot"), Model.GetSnapshot().ComputeDigest(), Before.ComputeDigest());
            TestEqual(TEXT("ID exhaustion preserves allocator"), Model.NextUnitId, MAX_int32 - Available);
        }
        else
        {
            TestTrue(TEXT("Last two allocatable IDs can complete atomic build"), Build.bAccepted);
            const FProofSnapshot After = Model.GetSnapshot();
            TestEqual(TEXT("Refinery receives penultimate allocatable ID"), After.Units[2].UnitId, MAX_int32 - 2);
            TestEqual(TEXT("Miner receives last allocatable ID"), After.Units[3].UnitId, MAX_int32 - 1);
            TestEqual(TEXT("Allocator ends at exhausted sentinel without overflow"), Model.NextUnitId, MAX_int32);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryMinerSpawnFailureTest, "RedalertProof.Skirmish.Economy.RefineryMiner.NoSpaceAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryMinerSpawnFailureTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    if (!TestTrue(TEXT("Seed builder"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector))) { return false; }
    const FVector Site(0, 600, 0);
    FVector Spawn;
    int32 Blockers = 0;
    while (Blockers < 24 && FProofCommandModel::FindSkirmishSpawn(Model.GetSnapshot(), Site, Spawn))
    {
        if (!TestTrue(TEXT("Fill a candidate with another owner's live HQ"), Model.SeedSkirmishPlayer(2 + Blockers, 0, Spawn))) { return false; }
        ++Blockers;
    }
    TestTrue(TEXT("Fixture occupies real candidates"), Blockers > 0);
    TestFalse(TEXT("All bounded spawn candidates are blocked"), FProofCommandModel::FindSkirmishSpawn(Model.GetSnapshot(), Site, Spawn));
    const FProofSnapshot Before = Model.GetSnapshot();
    TestEqual(TEXT("No-space rejects whole refinery build"), Model.Submit(1, MinerCommand(1, EProofCommandType::BuildRefinery, Site)).Reason, FString(TEXT("spawn")));
    TestEqual(TEXT("No-space leaves credits, revision and all records unchanged"), Model.GetSnapshot().ComputeDigest(), Before.ComputeDigest());
    TestTrue(TEXT("Single-record factory remains buildable at same legal site"), Model.Submit(1, MinerCommand(2, EProofCommandType::BuildFactory, Site)).bAccepted);
    TestEqual(TEXT("No-space did not consume a hidden ID"), Model.GetSnapshot().Units.Last().UnitId, MaximumMinerId(Before) + 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryMinerConservationTest, "RedalertProof.Skirmish.Economy.RefineryMiner.TwoMinerConservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryMinerConservationTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    if (!TestTrue(TEXT("Seed finite economy"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector))
        || !TestTrue(TEXT("Build grants second miner"), Model.Submit(1, MinerCommand(1, EProofCommandType::BuildRefinery, FVector(300, 0, 0))).bAccepted)) { return false; }
    const FProofSnapshot Built = Model.GetSnapshot();
    int32 Sequence = 2;
    for (const FProofUnitRecord& Unit : Built.Units)
    {
        if (Unit.Kind == EProofUnitKind::Harvester)
        {
            TestTrue(TEXT("Both miners use real Gather commands"), Model.Submit(1, MinerCommand(Sequence++, EProofCommandType::Gather, FVector::ZeroVector, Unit.UnitId, Built.Resources[0].ResourceId)).bAccepted);
        }
    }
    for (int32 Tick = 0; Tick < 200; ++Tick)
    {
        Model.Advance(0.5f);
        const FProofSnapshot State = Model.GetSnapshot();
        int32 Cargo = 0;
        for (const FProofUnitRecord& Unit : State.Units)
        {
            if (Unit.Kind == EProofUnitKind::Harvester) { Cargo += Unit.Cargo; TestTrue(TEXT("Cargo remains within existing capacity"), Unit.Cargo >= 0 && Unit.Cargo <= FProofCommandModel::CargoCapacity); }
        }
        TestTrue(TEXT("Finite ore never becomes negative"), State.Resources[0].Remaining >= 0);
        TestEqual(TEXT("Every ore unit is remaining, carried or delivered exactly once"), State.Resources[0].Remaining + Cargo + State.Players[0].TotalGathered, 500);
        TestEqual(TEXT("Credits change only on deposit after the one refinery charge"), State.Players[0].Credits, 250 + State.Players[0].TotalGathered);
    }
    const FProofSnapshot Finished = Model.GetSnapshot();
    TestEqual(TEXT("Two miners exhaust and deliver the finite patch"), Finished.Players[0].TotalGathered, 500);
    TestEqual(TEXT("No extra income from extra miner"), MinerCredits(Finished), 750);
    Model.Advance(100.f);
    TestEqual(TEXT("Extra idle advances cannot mint credits"), MinerCredits(Model.GetSnapshot()), 750);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofRefineryMinerReseedTest, "RedalertProof.Skirmish.Economy.RefineryMiner.SeatReseedAndSpawnContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofRefineryMinerReseedTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    if (!TestTrue(TEXT("Keep another owner alive"), Model.SeedSkirmishPlayer(2, 0, FVector(0, 3000, 0)))) { return false; }
    for (int32 Cycle = 0; Cycle < 3; ++Cycle)
    {
        if (!TestTrue(TEXT("Reseed opens with starter only"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector))) { return false; }
        TestEqual(TEXT("Exactly one starter after reseed"), CountMinerKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester), 1);
        TestTrue(TEXT("Fresh sequence builds one grant"), Model.Submit(1, MinerCommand(1, EProofCommandType::BuildRefinery, FVector(300, 0, 0))).bAccepted);
        TestEqual(TEXT("Only one new grant on reseeded seat"), CountMinerKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester), 2);
        TestTrue(TEXT("Seat removal succeeds"), Model.RemoveSkirmishPlayer(1));
        TestFalse(TEXT("Removal includes starter and granted records"), Model.GetSnapshot().Units.ContainsByPredicate([](const FProofUnitRecord& Unit) { return Unit.OwnerId == 1; }));
        TestEqual(TEXT("Surviving owner is unaffected"), CountMinerKind(Model.GetSnapshot(), 2, EProofUnitKind::Harvester), 1);
    }
    const FProofSnapshot Snapshot = Model.GetSnapshot();
    FVector A, B;
    TestTrue(TEXT("World-edge search fits inward"), FProofCommandModel::FindSkirmishSpawn(Snapshot, FVector(10000, 10000, 0), A));
    TestTrue(TEXT("World-edge candidate retains production preference"), A.Equals(FVector(9700, 10000, 0), 0.0));
    TestTrue(TEXT("Repeated history yields same spawn"), FProofCommandModel::FindSkirmishSpawn(Snapshot, FVector(10000, 10000, 0), B) && A.Equals(B, 0.0));
    B = FVector(10000, 10000, 0);
    TestTrue(TEXT("Anchor can share the output variable without changing the search origin"), FProofCommandModel::FindSkirmishSpawn(Snapshot, B, B) && A.Equals(B, 0.0));
    TestFalse(TEXT("Nonfinite anchor rejects"), FProofCommandModel::FindSkirmishSpawn(Snapshot, FVector(std::numeric_limits<double>::infinity(), 0, 0), B));
    TestTrue(TEXT("Failure clears stale spawn output"), B.IsZero());
    TestEqual(TEXT("Shared helper never mutates input snapshot"), Model.GetSnapshot().ComputeDigest(), Snapshot.ComputeDigest());
    return true;
}
#endif
