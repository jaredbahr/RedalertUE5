#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofGroupOrderQueue.h"
#include <limits>

namespace
{
FProofSnapshot QueueFixture()
{
    FProofSnapshot Snapshot;
    Snapshot.MatchEpoch = 1;
    Snapshot.bSkirmish = true;
    Snapshot.MatchPhase = EProofMatchPhase::Playing;
    for (int32 OwnerId : { 1, 2, 3 })
    {
        FProofPlayerRecord Player;
        Player.OwnerId = OwnerId;
        Player.TeamId = OwnerId == 3 ? 1 : 0;
        Snapshot.Players.Add(Player);
    }
    for (int32 UnitId = 1; UnitId <= 66; ++UnitId)
    {
        FProofUnitRecord Unit;
        Unit.UnitId = UnitId;
        Unit.OwnerId = 1;
        Unit.Kind = EProofUnitKind::Combat;
        Unit.Position = FVector(UnitId, 0, 0);
        Snapshot.Units.Add(Unit);
    }
    for (int32 UnitId : { 80, 81, 200, 201 })
    {
        FProofUnitRecord Unit;
        Unit.UnitId = UnitId;
        Unit.OwnerId = UnitId == 200 ? 3 : (UnitId == 201 ? 2 : 1);
        Unit.Kind = UnitId == 80 ? EProofUnitKind::Harvester
            : (UnitId == 81 ? EProofUnitKind::HQ : EProofUnitKind::Combat);
        Snapshot.Units.Add(Unit);
    }
    return Snapshot;
}

FProofCommand Move(int32 UnitId, double X)
{
    FProofCommand Command;
    Command.Type = EProofCommandType::Move;
    Command.UnitId = UnitId;
    Command.Destination = FVector(X, 0, 0);
    return Command;
}

FProofCommand Attack(int32 UnitId, int32 TargetUnitId)
{
    FProofCommand Command;
    Command.Type = EProofCommandType::Attack;
    Command.UnitId = UnitId;
    Command.TargetUnitId = TargetUnitId;
    return Command;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofGroupQueueCoalesceTest, "RedalertProof.Controls.QueueCoalescesAndPreservesOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofGroupQueueCoalesceTest::RunTest(const FString&)
{
    const FProofSnapshot Snapshot = QueueFixture();
    const FString Digest = Snapshot.ComputeDigest();
    FProofGroupOrderQueue Queue;
    TestTrue(TEXT("Initial group enqueues"), Queue.Enqueue(Snapshot, 1, { Move(1, 100), Move(2, 200), Attack(3, 200) }));
    TestTrue(TEXT("New intent replaces old unit in place"), Queue.Enqueue(Snapshot, 1, { Move(2, 300), Attack(1, 200) }));
    TestEqual(TEXT("Repeated units do not increase depth"), Queue.NumPending(), 3);
    const TArray<FProofCommand> Ready = Queue.PopReady(Snapshot, 1, 0.0);
    TestEqual(TEXT("First interval releases two intents"), Ready.Num(), 2);
    if (Ready.Num() == 2)
    {
        TestEqual(TEXT("First unit retains original position"), Ready[0].UnitId, 1);
        TestTrue(TEXT("Latest intent wins for first unit"), Ready[0].Type == EProofCommandType::Attack);
        TestEqual(TEXT("Second unit retains original position"), Ready[1].UnitId, 2);
        TestEqual(TEXT("Latest destination wins"), Ready[1].Destination.X, 300.0);
        for (const FProofCommand& Command : Ready)
        {
            TestEqual(TEXT("Queue never allocates sequence"), Command.Sequence, 0);
            TestEqual(TEXT("Queue never stamps epoch"), Command.MatchEpoch, 0);
        }
    }
    TestEqual(TEXT("Dispatch leaves snapshot unchanged"), Snapshot.ComputeDigest(), Digest);
    const TArray<FProofCommand> Third = Queue.PopReady(Snapshot, 1, 0.1);
    TestEqual(TEXT("Third unit waits for next interval"), Third.Num(), 1);
    if (Third.Num() == 1) { TestEqual(TEXT("Third unit stays behind prior units"), Third[0].UnitId, 3); }
    TestTrue(TEXT("Second group enqueues"), Queue.Enqueue(Snapshot, 1, { Move(4, 10), Move(5, 20) }));
    Queue.CancelUnit(4);
    const TArray<FProofCommand> Cancelled = Queue.PopReady(Snapshot, 1, 0.2);
    TestEqual(TEXT("Single-unit override cancels earlier queued group intent"), Cancelled.Num(), 1);
    if (Cancelled.Num() == 1) { TestEqual(TEXT("Other unit remains queued"), Cancelled[0].UnitId, 5); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofGroupQueuePacingTest, "RedalertProof.Controls.QueueWallPacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofGroupQueuePacingTest::RunTest(const FString&)
{
    const FProofSnapshot Snapshot = QueueFixture();
    FProofGroupOrderQueue Queue;
    TestTrue(TEXT("First group enqueues"), Queue.Enqueue(Snapshot, 1,
        { Move(1, 1), Move(2, 2), Move(3, 3), Move(4, 4), Move(5, 5), Move(6, 6) }));
    TestEqual(TEXT("Nonfinite time cannot dispatch"),
        Queue.PopReady(Snapshot, 1, std::numeric_limits<double>::quiet_NaN()).Num(), 0);
    const TArray<FProofCommand> First = Queue.PopReady(Snapshot, 1, 100.0);
    TestEqual(TEXT("First interval dispatches at most two"), First.Num(), 2);
    TestTrue(TEXT("Other group stays behind pending units"), Queue.Enqueue(Snapshot, 1, { Move(7, 7), Move(8, 8) }));
    TestEqual(TEXT("Backwards clock cannot dispatch"), Queue.PopReady(Snapshot, 1, 99.0).Num(), 0);
    TestEqual(TEXT("Within interval cannot dispatch"), Queue.PopReady(Snapshot, 1, 100.09).Num(), 0);
    const TArray<FProofCommand> Second = Queue.PopReady(Snapshot, 1, 100.1);
    TestEqual(TEXT("Next interval dispatches two"), Second.Num(), 2);
    if (Second.Num() == 2)
    {
        for (int32 Index = 0; Index < 2; ++Index)
        {
            TestEqual(TEXT("First group preserves FIFO position"), Second[Index].UnitId, Index + 3);
        }
    }
    TestTrue(TEXT("Later group enqueues"), Queue.Enqueue(Snapshot, 1, { Move(9, 9), Move(10, 10), Move(11, 11), Move(12, 12), Move(13, 13) }));
    TestEqual(TEXT("No immediate burst after another enqueue"), Queue.PopReady(Snapshot, 1, 100.11).Num(), 0);
    const TArray<FProofCommand> AfterPause = Queue.PopReady(Snapshot, 1, 1000.0);
    TestEqual(TEXT("Long pause still releases only two"), AfterPause.Num(), 2);
    if (AfterPause.Num() == 2)
    {
        TestEqual(TEXT("First pending unit follows prior group"), AfterPause[0].UnitId, 5);
        TestEqual(TEXT("Second pending unit follows prior group"), AfterPause[1].UnitId, 6);
    }
    TestEqual(TEXT("No catch-up burst in same interval"), Queue.PopReady(Snapshot, 1, 1000.0).Num(), 0);
    const TArray<FProofCommand> Next = Queue.PopReady(Snapshot, 1, 1000.1);
    TestEqual(TEXT("Following interval releases two from second group"), Next.Num(), 2);
    if (Next.Num() == 2)
    {
        TestEqual(TEXT("Second group first unit retains FIFO order"), Next[0].UnitId, 7);
        TestEqual(TEXT("Second group second unit retains FIFO order"), Next[1].UnitId, 8);
    }
    TestEqual(TEXT("Later group remains pending"), Queue.NumPending(), 5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofGroupQueueStaleTest, "RedalertProof.Controls.QueueSnapshotRevalidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofGroupQueueStaleTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = QueueFixture();
    FProofGroupOrderQueue Queue;
    TestTrue(TEXT("Intents can queue before snapshot changes"), Queue.Enqueue(Snapshot, 1,
        { Move(1, 100), Move(80, 100), Move(81, 100), Move(201, 100), Attack(2, 200), Attack(3, 201) }));
    for (FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.UnitId == 1 || Unit.UnitId == 200) { Unit.HitPoints = 0.f; }
        if (Unit.UnitId == 80) { Unit.OwnerId = 3; }
    }
    TestEqual(TEXT("Dead, foreign, immobile and same-team intents are pruned"), Queue.PopReady(Snapshot, 1, 0.0).Num(), 0);
    TestEqual(TEXT("Pruning empties queue"), Queue.NumPending(), 0);
    TestTrue(TEXT("Live command enqueues"), Queue.Enqueue(Snapshot, 1, { Move(2, 100) }));
    Snapshot.MatchPhase = EProofMatchPhase::Finished;
    TestEqual(TEXT("Terminal skirmish prunes pending command"), Queue.PopReady(Snapshot, 1, 1.0).Num(), 0);

    Snapshot.MatchPhase = EProofMatchPhase::Playing;
    TestTrue(TEXT("Command enqueues after terminal fixture reset"), Queue.Enqueue(Snapshot, 1, { Move(2, 100) }));
    Snapshot.MatchEpoch = 2;
    TestEqual(TEXT("Epoch change clears same-ID pending command"), Queue.PopReady(Snapshot, 1, 2.0).Num(), 0);
    TestTrue(TEXT("Fresh epoch command enqueues"), Queue.Enqueue(Snapshot, 1, { Move(2, 100) }));
    TestEqual(TEXT("Owner change clears pending command"), Queue.PopReady(Snapshot, 2, 3.0).Num(), 0);

    FProofSnapshot Legacy = QueueFixture();
    Legacy.bSkirmish = false;
    FProofGroupOrderQueue LegacyQueue;
    TestTrue(TEXT("Legacy attack enqueues"), LegacyQueue.Enqueue(Legacy, 1, { Attack(2, 201) }));
    TestEqual(TEXT("Legacy attack accepts distinct owner sharing default team"), LegacyQueue.PopReady(Legacy, 1, 0.0).Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofGroupQueueCapacityTest, "RedalertProof.Controls.QueueCapacityIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofGroupQueueCapacityTest::RunTest(const FString&)
{
    const FProofSnapshot Snapshot = QueueFixture();
    FProofGroupOrderQueue Queue;
    TArray<FProofCommand> SixtyThree;
    for (int32 UnitId = 1; UnitId <= 63; ++UnitId) { SixtyThree.Add(Move(UnitId, UnitId)); }
    TestTrue(TEXT("63 distinct intents fit"), Queue.Enqueue(Snapshot, 1, SixtyThree));
    TestFalse(TEXT("Batch adding two distinct IDs beyond cap is rejected"), Queue.Enqueue(Snapshot, 1,
        { Move(1, 999), Move(64, 64), Move(65, 65) }));
    TestEqual(TEXT("Overflow cannot partially add units"), Queue.NumPending(), 63);
    TestTrue(TEXT("64th distinct intent fits"), Queue.Enqueue(Snapshot, 1, { Move(64, 64) }));
    TestFalse(TEXT("65th distinct intent cannot fit"), Queue.Enqueue(Snapshot, 1, { Move(65, 65) }));
    TestEqual(TEXT("Queue does not exceed capacity"), Queue.NumPending(), FProofGroupOrderQueue::MaxPendingCommands);
    const TArray<FProofCommand> First = Queue.PopReady(Snapshot, 1, 0.0);
    TestEqual(TEXT("First interval releases two"), First.Num(), 2);
    if (First.Num() == 2) { TestEqual(TEXT("Rejected coalesce did not change prior destination"), First[0].Destination.X, 1.0); }
    TestTrue(TEXT("One additional ID fits after dispatch"), Queue.Enqueue(Snapshot, 1, { Move(66, 1e9) }));
    TestEqual(TEXT("Second interval releases two"), Queue.PopReady(Snapshot, 1, 0.1).Num(), 2);
    Queue.Clear();
    TestTrue(TEXT("Fresh queue accepts clamped move"), Queue.Enqueue(Snapshot, 1, { Move(66, 1e9) }));
    const TArray<FProofCommand> Clamped = Queue.PopReady(Snapshot, 1, 0.0);
    TestEqual(TEXT("One clamped intent released"), Clamped.Num(), 1);
    if (Clamped.Num() == 1)
    {
        TestEqual(TEXT("Move X remains within world"), Clamped[0].Destination.X,
            static_cast<double>(FProofCommandModel::WorldExtent));
    }
    FProofCommand Nonfinite = Move(2, std::numeric_limits<double>::infinity());
    TestFalse(TEXT("Nonfinite move cannot enqueue"), Queue.Enqueue(Snapshot, 1, { Nonfinite }));
    return true;
}
#endif
