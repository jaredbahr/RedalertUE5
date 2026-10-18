#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofReturnCargoSelection.h"
#include "Presentation/ProofGroupOrderQueue.h"
#include <limits>

namespace
{
FProofSnapshot ReturnSelectionSnapshot()
{
    FProofSnapshot Snapshot;
    Snapshot.MatchEpoch = 7;
    Snapshot.bSkirmish = true;
    Snapshot.MatchPhase = EProofMatchPhase::Playing;
    for (int32 OwnerId : {1, 2, 3})
    {
        FProofPlayerRecord Player;
        Player.OwnerId = OwnerId;
        Player.TeamId = OwnerId == 3 ? 1 : 0;
        Player.Credits = 100;
        Snapshot.Players.Add(Player);
    }
    for (int32 Id = 1; Id <= 14; ++Id)
    {
        FProofUnitRecord Unit;
        Unit.UnitId = Id;
        Unit.OwnerId = Id == 6 || Id == 11 ? 2 : (Id == 12 ? 3 : 1);
        Unit.Kind = Id >= 10 ? EProofUnitKind::Refinery : EProofUnitKind::Harvester;
        if (Id == 4) Unit.Kind = EProofUnitKind::Combat;
        if (Id == 13) Unit.Kind = EProofUnitKind::Factory;
        Unit.Cargo = Id < 10 && Id != 3 ? 25 : 0;
        Unit.HitPoints = Id == 5 || Id == 14 ? 0.f : 100.f;
        Unit.Position = FVector(Id * 50, 0, 0);
        Snapshot.Units.Add(Unit);
    }
    return Snapshot;
}
FProofCommand ReturnSelectionIntent(int32 UnitId, int32 RefineryId = 10)
{
    FProofCommand Command;
    Command.Type = EProofCommandType::ReturnCargo;
    Command.UnitId = UnitId;
    Command.TargetUnitId = RefineryId;
    return Command;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoSelectionTest, "RedalertProof.Controls.ReturnCargoSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoSelectionTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = ReturnSelectionSnapshot();
    const FProofSnapshot Before = Snapshot;
    FProofSelectionState Selection;
    Selection.SelectSingle(Snapshot, 1, 2);
    Selection.Toggle(Snapshot, 1, 3);
    Selection.Toggle(Snapshot, 1, 1);
    const TArray<FProofCommand> Planned = FProofReturnCargoSelection::Plan(Snapshot, 1, Selection, 10);
    TestEqual(TEXT("All-miner group skips the empty miner"), Planned.Num(), 2);
    if (Planned.Num() == 2)
    {
        TestEqual(TEXT("Return intents retain canonical first ID"), Planned[0].UnitId, 1);
        TestEqual(TEXT("Return intents retain canonical second ID"), Planned[1].UnitId, 2);
    }
    for (const FProofCommand& Command : Planned)
    {
        TestTrue(TEXT("Planner emits ReturnCargo"), Command.Type == EProofCommandType::ReturnCargo);
        TestEqual(TEXT("Planner retains selected refinery"), Command.TargetUnitId, 10);
        TestEqual(TEXT("Planner does not reserve sequence"), Command.Sequence, 0);
        TestEqual(TEXT("Planner does not reserve epoch"), Command.MatchEpoch, 0);
        TestTrue(TEXT("Planner has no movement payload"), Command.Destination == FVector::ZeroVector);
    }
    for (int32 Id : {3, 4, 5, 6, 10, -1, 999})
        TestFalse(TEXT("Only loaded owned live harvesters can return"), FProofReturnCargoSelection::CanReturn(Snapshot, 1, Id, 10));
    for (int32 Target : {11, 12, 13, 14, -1, 999})
        TestTrue(TEXT("Invalid/foreign/ally/dead/wrong-role target has no return plan"),
            FProofReturnCargoSelection::Plan(Snapshot, 1, Selection, Target).IsEmpty());
    Selection.Toggle(Snapshot, 1, 4);
    TestTrue(TEXT("Mixed combat/miner group does not become a partial context action"),
        FProofReturnCargoSelection::Plan(Snapshot, 1, Selection, 10).IsEmpty());
    Selection.Toggle(Snapshot, 1, 4);
    FProofSnapshot NewEpoch = Snapshot;
    ++NewEpoch.MatchEpoch;
    TestTrue(TEXT("Stale epoch selection cannot issue return"),
        FProofReturnCargoSelection::Plan(NewEpoch, 1, Selection, 10).IsEmpty());
    TestTrue(TEXT("Stale owner selection cannot issue return"),
        FProofReturnCargoSelection::Plan(Snapshot, 2, Selection, 11).IsEmpty());
    FProofSnapshot Ended = Snapshot;
    Ended.MatchPhase = EProofMatchPhase::Finished;
    TestTrue(TEXT("Terminal snapshot cannot issue return"), FProofReturnCargoSelection::Plan(Ended, 1, Selection, 10).IsEmpty());
    Ended = Snapshot;
    Ended.bSkirmish = false;
    TestTrue(TEXT("Legacy snapshot cannot issue return"), FProofReturnCargoSelection::Plan(Ended, 1, Selection, 10).IsEmpty());
    Ended = Snapshot;
    Ended.Players.RemoveAll([](const FProofPlayerRecord& Player) { return Player.OwnerId == 1; });
    TestTrue(TEXT("Missing owner record cannot issue return"), FProofReturnCargoSelection::Plan(Ended, 1, Selection, 10).IsEmpty());
    Ended = Snapshot;
    Ended.Units[9].HitPoints = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite refinery health fails closed"), FProofReturnCargoSelection::CanReturn(Ended, 1, 1, 10));
    Ended = Snapshot;
    Ended.Units[0].HitPoints = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite miner health fails closed"), FProofReturnCargoSelection::CanReturn(Ended, 1, 1, 10));
    TestEqual(TEXT("Planning preserves player credits"), Snapshot.Players[0].Credits, Before.Players[0].Credits);
    TestEqual(TEXT("Planning preserves unit count"), Snapshot.Units.Num(), Before.Units.Num());
    for (int32 Index = 0; Index < Snapshot.Units.Num(); ++Index)
    {
        TestEqual(TEXT("Planning preserves cargo"), Snapshot.Units[Index].Cargo, Before.Units[Index].Cargo);
        TestTrue(TEXT("Planning preserves position"), Snapshot.Units[Index].Position == Before.Units[Index].Position);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoQueueTest, "RedalertProof.Controls.ReturnCargoQueue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoQueueTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = ReturnSelectionSnapshot();
    FProofGroupOrderQueue Queue;
    FProofCommand Move;
    Move.Type = EProofCommandType::Move;
    Move.UnitId = 1;
    Move.Destination = FVector(1000, 0, 0);
    TestTrue(TEXT("Initial Move enqueues"), Queue.Enqueue(Snapshot, 1, {Move, ReturnSelectionIntent(2)}));
    FProofCommand Return = ReturnSelectionIntent(1);
    Return.Sequence = 91;
    Return.MatchEpoch = 92;
    Return.Destination = FVector(std::numeric_limits<double>::infinity(), 2, 3);
    TestTrue(TEXT("Return replaces pending Move in place"), Queue.Enqueue(Snapshot, 1, {Return}));
    TestEqual(TEXT("Replacement keeps bounded queue depth"), Queue.NumPending(), 2);
    const TArray<FProofCommand> Ready = Queue.PopReady(Snapshot, 1, 0.0);
    TestEqual(TEXT("Existing two-command rate is unchanged"), Ready.Num(), 2);
    if (Ready.Num() == 2)
    {
        TestEqual(TEXT("Replaced unit retains queue order"), Ready[0].UnitId, 1);
        TestTrue(TEXT("Latest Return wins over prior Move"), Ready[0].Type == EProofCommandType::ReturnCargo);
        TestEqual(TEXT("Second unit remains second"), Ready[1].UnitId, 2);
        TestEqual(TEXT("Queue clears supplied sequence"), Ready[0].Sequence, 0);
        TestEqual(TEXT("Queue clears supplied epoch"), Ready[0].MatchEpoch, 0);
        TestTrue(TEXT("Queue normalizes irrelevant destination"), Ready[0].Destination == FVector::ZeroVector);
    }
    TestTrue(TEXT("Returns can be queued again"), Queue.Enqueue(Snapshot, 1, {ReturnSelectionIntent(1), ReturnSelectionIntent(2)}));
    TestEqual(TEXT("No extra batch in same interval"), Queue.PopReady(Snapshot, 1, 0.05).Num(), 0);
    Snapshot.Units[0].Cargo = 0;
    const TArray<FProofCommand> Pruned = Queue.PopReady(Snapshot, 1, 0.1);
    TestTrue(TEXT("A miner unloaded before dispatch is pruned"), Pruned.Num() == 1 && Pruned[0].UnitId == 2);
    Snapshot = ReturnSelectionSnapshot();
    Queue.Clear();
    TestTrue(TEXT("Return before refinery death enqueues"), Queue.Enqueue(Snapshot, 1, {ReturnSelectionIntent(1)}));
    Snapshot.Units[9].HitPoints = 0.f;
    TestEqual(TEXT("Destroyed refinery cancels queued return"), Queue.PopReady(Snapshot, 1, 0.0).Num(), 0);
    TestEqual(TEXT("Destroyed target leaves no queued return"), Queue.NumPending(), 0);
    Snapshot = ReturnSelectionSnapshot();
    TestTrue(TEXT("Valid Move survives later bad batch"), Queue.Enqueue(Snapshot, 1, {Move}));
    TestFalse(TEXT("Empty miner makes Return batch fail atomically"),
        Queue.Enqueue(Snapshot, 1, {ReturnSelectionIntent(1), ReturnSelectionIntent(3)}));
    const TArray<FProofCommand> Unchanged = Queue.PopReady(Snapshot, 1, 1.0);
    TestTrue(TEXT("Rejected Return batch does not replace earlier Move"), Unchanged.Num() == 1
        && Unchanged[0].Type == EProofCommandType::Move && Unchanged[0].Destination == Move.Destination);
    Queue.Clear();
    TestTrue(TEXT("Valid return enqueues for cancellation"), Queue.Enqueue(Snapshot, 1, {ReturnSelectionIntent(1)}));
    FProofCommand Stop;
    Stop.Type = EProofCommandType::Stop;
    Stop.UnitId = 1;
    TestTrue(TEXT("Stop replaces forced return in ordinary queue"), Queue.Enqueue(Snapshot, 1, {Stop}));
    const TArray<FProofCommand> Stopped = Queue.PopReady(Snapshot, 1, 0.0);
    TestTrue(TEXT("Latest Stop is dispatched"), Stopped.Num() == 1 && Stopped[0].Type == EProofCommandType::Stop);
    for (int32 RefineryId : {-1, 0, 11, 12, 13, 14, 999})
    {
        Queue.Clear();
        TestFalse(TEXT("Invalid refinery cannot enter queue"), Queue.Enqueue(Snapshot, 1, {ReturnSelectionIntent(1, RefineryId)}));
        TestEqual(TEXT("Invalid intent cannot grow queue"), Queue.NumPending(), 0);
    }
    Queue.Clear();
    TestTrue(TEXT("Queue before epoch change"), Queue.Enqueue(Snapshot, 1, {ReturnSelectionIntent(1)}));
    ++Snapshot.MatchEpoch;
    TestEqual(TEXT("Epoch change retires old return"), Queue.PopReady(Snapshot, 1, 0.0).Num(), 0);
    return true;
}
#endif
