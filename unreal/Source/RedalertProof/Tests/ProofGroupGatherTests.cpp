#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofSelectionState.h"
#include "Presentation/ProofGroupOrderQueue.h"
#include <limits>

namespace
{
FProofCommand GatherTestOrder(int32 UnitId, int32 Target, EProofCommandType Type = EProofCommandType::Gather)
{
    FProofCommand Command;
    Command.Type = Type; Command.UnitId = UnitId; Command.TargetUnitId = Target;
    return Command;
}
bool GatherTestSetup(FProofCommandModel& Model, int32& Starter, int32& Granted, int32& Patch)
{
    Model.StartSkirmish();
    if (!Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector)) return false;
    // Same-team second owner keeps this a nonterminal economy-only fixture.
    if (!Model.SeedSkirmishPlayer(2, 0, FVector(2000, 0, 0))) return false;
    const FProofSnapshot Opening = Model.GetSnapshot();
    Starter = Opening.Units.FindByPredicate([](const FProofUnitRecord& Unit)
        { return Unit.OwnerId == 1 && Unit.Kind == EProofUnitKind::Harvester; })->UnitId;
    Patch = Opening.Resources[0].ResourceId;
    FProofCommand Build;
    Build.Sequence = 1; Build.Type = EProofCommandType::BuildRefinery; Build.Destination = FVector(300, 0, 0);
    if (!Model.Submit(1, Build).bAccepted) return false;
    Granted = Model.GetSnapshot().Units.Last().UnitId;
    return true;
}
FProofSelectionState GatherTestSelection(const FProofSnapshot& Snapshot, int32 Starter, int32 Granted)
{
    FProofSelectionState Selection;
    Selection.SelectSingle(Snapshot, 1, Granted);
    Selection.Toggle(Snapshot, 1, Starter);
    return Selection;
}
FProofUnitRecord& GatherTestUnit(FProofSnapshot& Snapshot, int32 Id)
{
    return *Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofGroupGatherSubmitTest, "RedalertProof.GroupGather.PlanningAndSubmit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofGroupGatherSubmitTest::RunTest(const FString&)
{
    FProofCommandModel Model; int32 Starter, Granted, Patch;
    if (!TestTrue(TEXT("Build real refinery and its granted second miner"), GatherTestSetup(Model, Starter, Granted, Patch))) return false;
    FProofSnapshot Snapshot = Model.GetSnapshot();
    const FString Digest = Snapshot.ComputeDigest();
    FProofSelectionState Selection = GatherTestSelection(Snapshot, Starter, Granted);
    TArray<FProofCommand> Planned = Selection.PlanGather(Snapshot, 1, Patch);
    if (!TestEqual(TEXT("Both starter and granted miner plan Gather"), Planned.Num(), 2)) return false;
    TestEqual(TEXT("Ascending canonical first ID despite selection order"), Planned[0].UnitId, FMath::Min(Starter, Granted));
    TestEqual(TEXT("Ascending canonical second ID"), Planned[1].UnitId, FMath::Max(Starter, Granted));
    FProofGroupOrderQueue Queue;
    TestTrue(TEXT("Production queue accepts both Gather intents"), Queue.Enqueue(Snapshot, 1, Planned));
    TArray<FProofCommand> Ready = Queue.PopReady(Snapshot, 1, 0.0);
    if (!TestEqual(TEXT("Existing interval releases two"), Ready.Num(), 2)) return false;
    int32 Sequence = 2;
    for (FProofCommand Command : Ready)
    {
        TestEqual(TEXT("Unstamped sequence"), Command.Sequence, 0);
        TestEqual(TEXT("Unstamped epoch"), Command.MatchEpoch, 0);
        TestTrue(TEXT("Canonical Gather destination"), Command.Destination.IsZero());
        Command.Sequence = Sequence++; Command.MatchEpoch = Snapshot.MatchEpoch;
        TestTrue(TEXT("Unmodified authoritative Submit accepts planned Gather"), Model.Submit(1, Command).bAccepted);
    }
    TestEqual(TEXT("Planning and queue leave source snapshot unchanged"), Snapshot.ComputeDigest(), Digest);
    Model.Advance(1.f);
    FProofSnapshot After = Model.GetSnapshot();
    TestTrue(TEXT("Starter follows real Gather movement"), GatherTestUnit(After, Starter).Position != GatherTestUnit(Snapshot, Starter).Position);
    TestTrue(TEXT("Granted miner follows real Gather movement"), GatherTestUnit(After, Granted).Position != GatherTestUnit(Snapshot, Granted).Position);
    for (int32 Tick = 0; Tick < 50; ++Tick) Model.Advance(1.f);
    After = Model.GetSnapshot();
    const FProofPlayerRecord* Owner = After.Players.FindByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == 1; });
    TestTrue(TEXT("Real group orders extract and deposit finite ore"), Owner && Owner->TotalGathered > 0);
    int32 Accounted = 0;
    for (const FProofResourceRecord& Resource : After.Resources) Accounted += Resource.Remaining;
    for (const FProofUnitRecord& Unit : After.Units) Accounted += Unit.Cargo;
    for (const FProofPlayerRecord& Player : After.Players) Accounted += Player.TotalGathered;
    TestEqual(TEXT("Group issue does not create ore"), Accounted, 1000);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofGroupGatherSelectionTest, "RedalertProof.GroupGather.SelectionBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofGroupGatherSelectionTest::RunTest(const FString&)
{
    FProofCommandModel Model; int32 Starter, Granted, Patch;
    if (!GatherTestSetup(Model, Starter, Granted, Patch)) return false;
    const FProofSnapshot Snapshot = Model.GetSnapshot();
    FProofSelectionState Selection = GatherTestSelection(Snapshot, Starter, Granted);
    const int32 HQ = Snapshot.Units.FindByPredicate([](const FProofUnitRecord& Unit)
        { return Unit.OwnerId == 1 && Unit.Kind == EProofUnitKind::HQ; })->UnitId;
    Selection.Toggle(Snapshot, 1, HQ);
    TestFalse(TEXT("Mixed structure/miner selection is not group Gather"), Selection.IsAllHarvesters(Snapshot, 1));
    TestTrue(TEXT("Mixed selection plans no Gather"), Selection.PlanGather(Snapshot, 1, Patch).IsEmpty());
    TestEqual(TEXT("Existing mixed Move still moves two miners"), Selection.PlanMove(Snapshot, 1, FVector(600, 600, 0)).Num(), 2);
    Selection = GatherTestSelection(Snapshot, Starter, Granted);
    FProofSnapshot Changed = Snapshot;
    Changed.MatchEpoch++;
    TestTrue(TEXT("Epoch-stale selection cannot Gather"), Selection.PlanGather(Changed, 1, Patch).IsEmpty());
    TestTrue(TEXT("Owner-stale selection cannot Gather"), Selection.PlanGather(Snapshot, 2, Patch).IsEmpty());
    Changed = Snapshot; Changed.MatchPhase = EProofMatchPhase::Finished;
    TestTrue(TEXT("Terminal cannot Gather"), Selection.PlanGather(Changed, 1, Patch).IsEmpty());
    Changed = Snapshot; Changed.bSkirmish = false;
    TestTrue(TEXT("Legacy cannot Gather"), Selection.PlanGather(Changed, 1, Patch).IsEmpty());
    Changed = Snapshot; GatherTestUnit(Changed, Granted).OwnerId = 2;
    TestTrue(TEXT("Newly foreign selection rejected as whole group"), Selection.PlanGather(Changed, 1, Patch).IsEmpty());
    Changed = Snapshot; GatherTestUnit(Changed, Granted).HitPoints = 0;
    TestTrue(TEXT("Dead selection rejected until normal synchronize prunes it"), Selection.PlanGather(Changed, 1, Patch).IsEmpty());
    Changed = Snapshot; Changed.Players.RemoveAll([](const FProofPlayerRecord& Player) { return Player.OwnerId == 1; });
    TestTrue(TEXT("Missing participant cannot Gather"), Selection.PlanGather(Changed, 1, Patch).IsEmpty());
    Changed = Snapshot; Changed.Units.RemoveAll([](const FProofUnitRecord& Unit) { return Unit.Kind == EProofUnitKind::Refinery; });
    TestTrue(TEXT("No own live refinery cannot Gather"), Selection.PlanGather(Changed, 1, Patch).IsEmpty());
    TestTrue(TEXT("Invalid resource ID cannot Gather"), Selection.PlanGather(Snapshot, 1, INDEX_NONE).IsEmpty());
    TestTrue(TEXT("Missing resource cannot Gather"), Selection.PlanGather(Snapshot, 1, MAX_int32).IsEmpty());
    // Combat semantics still flow through the existing planner, including ally protection.
    Changed = Snapshot; GatherTestUnit(Changed, Starter).Kind = EProofUnitKind::Combat;
    Selection = GatherTestSelection(Changed, Starter, Granted);
    TestTrue(TEXT("Combat/miner selection cannot Gather"), Selection.PlanGather(Changed, 1, Patch).IsEmpty());
    const int32 OtherHQ = Changed.Units.FindByPredicate([](const FProofUnitRecord& Unit)
        { return Unit.OwnerId == 2 && Unit.Kind == EProofUnitKind::HQ; })->UnitId;
    TestTrue(TEXT("Allied right click retains no attack"), Selection.PlanAttack(Changed, 1, OtherHQ).IsEmpty());
    Changed.Players.Last().TeamId = 1;
    TestEqual(TEXT("Hostile group attack still filters to combat"), Selection.PlanAttack(Changed, 1, OtherHQ).Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofGroupGatherPruneTest, "RedalertProof.GroupGather.QueueRechecks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofGroupGatherPruneTest::RunTest(const FString&)
{
    FProofCommandModel Model; int32 Starter, Granted, Patch;
    if (!GatherTestSetup(Model, Starter, Granted, Patch)) return false;
    const FProofSnapshot Snapshot = Model.GetSnapshot();
    const FProofSelectionState Selection = GatherTestSelection(Snapshot, Starter, Granted);
    const TArray<FProofCommand> Commands = Selection.PlanGather(Snapshot, 1, Patch);
    for (int32 Variant = 0; Variant < 8; ++Variant)
    {
        FProofGroupOrderQueue Queue;
        TestTrue(TEXT("Fresh Gather batch enqueues"), Queue.Enqueue(Snapshot, 1, Commands));
        FProofSnapshot Changed = Snapshot;
        if (Variant == 0) Changed.Resources.Reset();
        if (Variant == 1) Changed.Resources[0].Remaining = 0;
        if (Variant == 2) for (FProofUnitRecord& Unit : Changed.Units) if (Unit.Kind == EProofUnitKind::Refinery) Unit.HitPoints = 0;
        if (Variant == 3) Changed.MatchPhase = EProofMatchPhase::Finished;
        if (Variant == 4) Changed.MatchEpoch++;
        if (Variant == 5) Changed.Players.RemoveAll([](const FProofPlayerRecord& Player) { return Player.OwnerId == 1; });
        if (Variant == 6) for (FProofUnitRecord& Unit : Changed.Units) if (Unit.Kind == EProofUnitKind::Refinery) Unit.OwnerId = 2;
        if (Variant == 7) Changed.bSkirmish = false;
        TestTrue(TEXT("Invalidated context prunes before sequence stamp"), Queue.PopReady(Changed, 1, 0.0).IsEmpty());
        TestEqual(TEXT("Invalidated queue empty"), Queue.NumPending(), 0);
    }
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        FProofGroupOrderQueue Queue;
        Queue.Enqueue(Snapshot, 1, Commands);
        FProofSnapshot Changed = Snapshot;
        if (Variant == 0) GatherTestUnit(Changed, Starter).HitPoints = 0;
        if (Variant == 1) GatherTestUnit(Changed, Starter).OwnerId = 2;
        if (Variant == 2) GatherTestUnit(Changed, Starter).Kind = EProofUnitKind::Combat;
        const TArray<FProofCommand> Ready = Queue.PopReady(Changed, 1, 0.0);
        TestEqual(TEXT("Only stale source is pruned"), Ready.Num(), 1);
        if (Ready.Num() == 1) TestEqual(TEXT("Other miner remains"), Ready[0].UnitId, Granted);
    }
    FProofGroupOrderQueue OwnerQueue;
    OwnerQueue.Enqueue(Snapshot, 1, Commands);
    TestTrue(TEXT("Owner switch clears queue"), OwnerQueue.PopReady(Snapshot, 2, 0.0).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofGroupGatherCargoTest, "RedalertProof.GroupGather.DepletedCargoAndOverrides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofGroupGatherCargoTest::RunTest(const FString&)
{
    FProofCommandModel Model; int32 Starter, Granted, Patch;
    if (!GatherTestSetup(Model, Starter, Granted, Patch)) return false;
    FProofSnapshot Snapshot = Model.GetSnapshot();
    Snapshot.Resources[0].Remaining = 0;
    GatherTestUnit(Snapshot, Granted).Cargo = 50;
    FProofSelectionState Selection = GatherTestSelection(Snapshot, Starter, Granted);
    const TArray<FProofCommand> Loaded = Selection.PlanGather(Snapshot, 1, Patch);
    TestEqual(TEXT("Only loaded miner eligible for depleted patch"), Loaded.Num(), 1);
    if (Loaded.Num() != 1) return false;
    TestEqual(TEXT("Loaded eligible miner retained"), Loaded[0].UnitId, Granted);
    FProofGroupOrderQueue SubsetQueue;
    FProofCommand EmptyMinerMove = GatherTestOrder(Starter, INDEX_NONE, EProofCommandType::Move);
    EmptyMinerMove.Destination = FVector(750, 250, 0);
    TestTrue(TEXT("Empty miner has unrelated pending Move"), SubsetQueue.Enqueue(Snapshot, 1, { EmptyMinerMove }));
    TestTrue(TEXT("Loaded-only Gather subset enqueues"), SubsetQueue.Enqueue(Snapshot, 1, Loaded));
    const TArray<FProofCommand> SubsetReady = SubsetQueue.PopReady(Snapshot, 1, 0.0);
    if (TestEqual(TEXT("Subset Gather preserves omitted empty miner order"), SubsetReady.Num(), 2))
    {
        TestTrue(TEXT("Omitted miner retains original pending Move slot"), SubsetReady[0].UnitId == Starter
            && SubsetReady[0].Type == EProofCommandType::Move);
        TestTrue(TEXT("Omitted miner retains exact Move destination"), SubsetReady[0].Destination == EmptyMinerMove.Destination);
        TestTrue(TEXT("Loaded miner receives Gather in its own slot"), SubsetReady[1].UnitId == Granted
            && SubsetReady[1].Type == EProofCommandType::Gather && SubsetReady[1].TargetUnitId == Patch);
    }
    FProofGroupOrderQueue Queue;
    TestTrue(TEXT("Loaded depleted intent valid"), Queue.Enqueue(Snapshot, 1, Loaded));
    GatherTestUnit(Snapshot, Granted).Cargo = 0;
    TestTrue(TEXT("Cargo exhaustion while pending prunes depleted intent"), Queue.PopReady(Snapshot, 1, 0.0).IsEmpty());
    Snapshot.Resources[0].Remaining = 500;
    TArray<FProofCommand> Commands = Selection.PlanGather(Snapshot, 1, Patch);
    Commands[0].Sequence = 123; Commands[0].MatchEpoch = 456;
    Commands[0].Destination = FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0);
    TestTrue(TEXT("Gather ignores caller destination and unstamps intent"), Queue.Enqueue(Snapshot, 1, Commands));
    FProofCommand Stop = GatherTestOrder(Starter, INDEX_NONE, EProofCommandType::Stop);
    TestTrue(TEXT("Explicit Stop replaces pending Gather"), Queue.Enqueue(Snapshot, 1, { Stop }));
    FProofCommand Move = GatherTestOrder(Granted, INDEX_NONE, EProofCommandType::Move);
    Move.Destination = FVector(900, 0, 0);
    TestTrue(TEXT("Explicit Move replaces pending Gather"), Queue.Enqueue(Snapshot, 1, { Move }));
    const TArray<FProofCommand> Ready = Queue.PopReady(Snapshot, 1, 0.0);
    TestEqual(TEXT("Overrides retain both unit slots"), Ready.Num(), 2);
    if (Ready.Num() == 2)
    {
        TestTrue(TEXT("Stop in first slot"), Ready[0].Type == EProofCommandType::Stop);
        TestTrue(TEXT("Move in second slot"), Ready[1].Type == EProofCommandType::Move);
        TestEqual(TEXT("Queue sequence remains unstamped"), Ready[0].Sequence, 0);
        TestEqual(TEXT("Queue epoch remains unstamped"), Ready[0].MatchEpoch, 0);
    }
    Queue.Enqueue(Snapshot, 1, { GatherTestOrder(Starter, Patch) });
    FProofCommand Invalid = GatherTestOrder(Granted, MAX_int32);
    TestFalse(TEXT("Invalid second Gather rejects atomic batch"), Queue.Enqueue(Snapshot, 1, { Move, Invalid }));
    TestEqual(TEXT("Rejected batch leaves previous queue depth"), Queue.NumPending(), 1);
    const TArray<FProofCommand> Later = Queue.PopReady(Snapshot, 1, 0.1);
    if (TestEqual(TEXT("Previous valid intent survives rejected batch"), Later.Num(), 1))
        TestTrue(TEXT("Surviving intent remains Gather"), Later[0].Type == EProofCommandType::Gather && Later[0].UnitId == Starter);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofGroupGatherPacingTest, "RedalertProof.GroupGather.PacingAndCanonicalIntent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofGroupGatherPacingTest::RunTest(const FString&)
{
    FProofCommandModel Model; int32 Starter, Granted, Patch;
    if (!GatherTestSetup(Model, Starter, Granted, Patch)) return false;
    FProofSnapshot Snapshot = Model.GetSnapshot();
    FProofUnitRecord Third = GatherTestUnit(Snapshot, Granted);
    Third.UnitId = 999; Snapshot.Units.Add(Third);
    FProofSelectionState Selection = GatherTestSelection(Snapshot, Starter, Granted);
    Selection.Toggle(Snapshot, 1, Third.UnitId);
    FProofGroupOrderQueue Queue;
    TArray<FProofCommand> Commands = Selection.PlanGather(Snapshot, 1, Patch);
    Commands[0].Sequence = 19; Commands[0].MatchEpoch = 47; Commands[0].Destination = FVector(123, 456, 789);
    TestTrue(TEXT("Three miner batch accepted"), Queue.Enqueue(Snapshot, 1, Commands));
    const TArray<FProofCommand> First = Queue.PopReady(Snapshot, 1, 100.0);
    TestEqual(TEXT("Unchanged two per interval"), First.Num(), 2);
    if (First.Num() == 2)
    {
        TestEqual(TEXT("Gather sequence cleared"), First[0].Sequence, 0);
        TestEqual(TEXT("Gather epoch cleared"), First[0].MatchEpoch, 0);
        TestTrue(TEXT("Unused destination canonical zero"), First[0].Destination.IsZero());
        TestEqual(TEXT("Target retained"), First[0].TargetUnitId, Patch);
    }
    TestTrue(TEXT("No same-time burst"), Queue.PopReady(Snapshot, 1, 100.0).IsEmpty());
    TestTrue(TEXT("No release before replacement between dispatches"), Queue.PopReady(Snapshot, 1, 100.05).IsEmpty());
    const int32 ReplacementPatch = Snapshot.Resources[1].ResourceId;
    TestTrue(TEXT("Pending third miner Gather replaced between dispatches"),
        Queue.Enqueue(Snapshot, 1, { GatherTestOrder(Third.UnitId, ReplacementPatch) }));
    TestEqual(TEXT("Replacement retains a single pending slot"), Queue.NumPending(), 1);
    TestTrue(TEXT("Replacement does not permit immediate burst"), Queue.PopReady(Snapshot, 1, 100.05).IsEmpty());
    TestTrue(TEXT("No early burst after replacement"), Queue.PopReady(Snapshot, 1, 100.099).IsEmpty());
    TestTrue(TEXT("No backward-clock burst"), Queue.PopReady(Snapshot, 1, 99.0).IsEmpty());
    TestTrue(TEXT("No nonfinite-clock burst"), Queue.PopReady(Snapshot, 1, std::numeric_limits<double>::infinity()).IsEmpty());
    const TArray<FProofCommand> Last = Queue.PopReady(Snapshot, 1, 100.1);
    TestEqual(TEXT("Third releases on unchanged boundary"), Last.Num(), 1);
    if (Last.Num() == 1)
    {
        TestEqual(TEXT("Ascending third miner"), Last[0].UnitId, 999);
        TestTrue(TEXT("Latest Gather replaces prior target without resetting original interval"),
            Last[0].Type == EProofCommandType::Gather && Last[0].TargetUnitId == ReplacementPatch);
    }
    return true;
}
#endif
