#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofSelectionState.h"
#include "Presentation/ProofGroupOrderQueue.h"

namespace
{
void StopUnit(FProofSnapshot& S, int32 Id, int32 Owner, EProofUnitKind Kind)
{
    FProofUnitRecord U;
    U.UnitId = Id;
    U.OwnerId = Owner;
    U.Kind = Kind;
    U.Position = FVector(Id, 0, 0);
    S.Units.Add(U);
}

FProofSnapshot StopSelectionFixture()
{
    FProofSnapshot S;
    S.bSkirmish = true;
    S.MatchEpoch = 1;
    S.MatchPhase = EProofMatchPhase::Playing;
    for (int32 Owner : {1, 2})
    {
        FProofPlayerRecord P;
        P.OwnerId = Owner;
        P.TeamId = Owner - 1;
        S.Players.Add(P);
    }
    StopUnit(S, 10, 1, EProofUnitKind::Combat);
    StopUnit(S, 20, 1, EProofUnitKind::Harvester);
    StopUnit(S, 30, 1, EProofUnitKind::HQ);
    StopUnit(S, 31, 1, EProofUnitKind::Refinery);
    StopUnit(S, 32, 1, EProofUnitKind::Factory);
    StopUnit(S, 40, 2, EProofUnitKind::Combat);
    StopUnit(S, 60, 1, EProofUnitKind::Combat);
    StopUnit(S, 70, 1, EProofUnitKind::Harvester);
    return S;
}

FProofSnapshot StopQueueFixture()
{
    FProofSnapshot S = StopSelectionFixture();
    S.Units.Reset();
    // Exercise the independent 64-intent queue bound, not a larger army claim.
    for (int32 Id = 1; Id <= 66; ++Id) StopUnit(S, Id, 1, EProofUnitKind::Combat);
    StopUnit(S, 80, 1, EProofUnitKind::HQ);
    StopUnit(S, 81, 1, EProofUnitKind::Harvester);
    StopUnit(S, 90, 2, EProofUnitKind::Combat);
    return S;
}

FProofCommand StopIntent(EProofCommandType Type, int32 Id)
{
    FProofCommand C;
    C.Type = Type;
    C.UnitId = Id;
    C.Destination = FVector(1000 + Id, 0, 0);
    C.TargetUnitId = 90;
    C.Sequence = 77;
    C.MatchEpoch = 99;
    return C;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopSelectionPolicyTest, "RedalertProof.Stop.SelectionPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopSelectionPolicyTest::RunTest(const FString&)
{
    FProofSnapshot S = StopSelectionFixture();
    FProofSelectionState Selection;
    Selection.SelectSingle(S, 1, INDEX_NONE);
    TestEqual(TEXT("Empty selection plans no Stop"), Selection.PlanStop(S, 1).Num(), 0);
    Selection.SelectSingle(S, 1, 30);
    TestEqual(TEXT("Structure-only selection plans no Stop"), Selection.PlanStop(S, 1).Num(), 0);
    Selection.SelectSingle(S, 1, 40);
    TestEqual(TEXT("Foreign unit cannot become a Stop selection"), Selection.PlanStop(S, 1).Num(), 0);
    for (int32 Id : {70, 60, 40, 32, 31, 30, 20, 10}) Selection.Toggle(S, 1, Id);
    for (FProofUnitRecord& U : S.Units)
    {
        if (U.UnitId == 60) U.OwnerId = 2;
        if (U.UnitId == 70) U.HitPoints = 0.f;
    }
    const TArray<int32> BeforeSelection = Selection.GetSelectedUnitIds();
    const int32 BeforePrimary = Selection.GetPrimaryUnitId();
    const FString BeforeSnapshot = S.ComputeDigest();
    const TArray<FProofCommand> Stops = Selection.PlanStop(S, 1);
    TestEqual(TEXT("Only live owned combat/harvester selected units stop"), Stops.Num(), 2);
    if (Stops.Num() == 2)
    {
        TestEqual(TEXT("Stop plan uses stable ID order"), Stops[0].UnitId, 10);
        TestEqual(TEXT("Harvester remains eligible"), Stops[1].UnitId, 20);
        for (const FProofCommand& C : Stops)
        {
            TestTrue(TEXT("Planned intent is Stop"), C.Type == EProofCommandType::Stop);
            TestEqual(TEXT("Sequence allocated only at dispatch"), C.Sequence, 0);
            TestEqual(TEXT("Epoch assigned only at dispatch"), C.MatchEpoch, 0);
            TestEqual(TEXT("Stop has no target"), C.TargetUnitId, INDEX_NONE);
        }
    }
    TestTrue(TEXT("Stop planning preserves selection"), Selection.GetSelectedUnitIds() == BeforeSelection);
    TestEqual(TEXT("Stop planning preserves primary"), Selection.GetPrimaryUnitId(), BeforePrimary);
    TestEqual(TEXT("Stop planning preserves authoritative snapshot"), S.ComputeDigest(), BeforeSnapshot);
    TestEqual(TEXT("Different owner context cannot stop"), Selection.PlanStop(S, 2).Num(), 0);
    FProofSnapshot Legacy = S;
    Legacy.bSkirmish = false;
    TestEqual(TEXT("Legacy owned mobile selection can Stop"), Selection.PlanStop(Legacy, 1).Num(), 2);
    S.MatchPhase = EProofMatchPhase::Finished;
    TestEqual(TEXT("Terminal skirmish plans no Stop"), Selection.PlanStop(S, 1).Num(), 0);
    S.MatchPhase = EProofMatchPhase::Playing;
    ++S.MatchEpoch;
    TestEqual(TEXT("Stale epoch plans no Stop"), Selection.PlanStop(S, 1).Num(), 0);
    --S.MatchEpoch;
    S.Players.RemoveAll([](const FProofPlayerRecord& P) { return P.OwnerId == 1; });
    TestEqual(TEXT("Missing player context plans no Stop"), Selection.PlanStop(S, 1).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopQueueReplacementTest, "RedalertProof.Stop.QueueReplacementAndPacing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopQueueReplacementTest::RunTest(const FString&)
{
    const FProofSnapshot S = StopQueueFixture();
    FProofGroupOrderQueue Q;
    TestTrue(TEXT("Initial mixed intents queue"), Q.Enqueue(S, 1, {
        StopIntent(EProofCommandType::Move, 1), StopIntent(EProofCommandType::Attack, 2),
        StopIntent(EProofCommandType::Move, 3), StopIntent(EProofCommandType::Attack, 4), StopIntent(EProofCommandType::Move, 5)}));
    TestEqual(TEXT("Initial dispatch keeps two-command budget"), Q.PopReady(S, 1, 10.0).Num(), 2);
    FProofSelectionState Selection;
    Selection.Toggle(S, 1, 4);
    Selection.Toggle(S, 1, 3);
    TestTrue(TEXT("Selected Stop batch replaces pending Move and Attack atomically"), Q.Enqueue(S, 1, Selection.PlanStop(S, 1)));
    TestEqual(TEXT("Stop replacement preserves queue depth and unselected intent"), Q.NumPending(), 3);
    TestEqual(TEXT("Stop does not reset dispatch clock"), Q.PopReady(S, 1, 10.05).Num(), 0);
    const TArray<FProofCommand> Stopped = Q.PopReady(S, 1, 10.1);
    TestEqual(TEXT("Stop batch respects two-command budget"), Stopped.Num(), 2);
    if (Stopped.Num() == 2)
        for (int32 Index = 0; Index < 2; ++Index)
        {
            TestEqual(TEXT("Replacements retain original queue position"), Stopped[Index].UnitId, Index + 3);
            TestTrue(TEXT("Old selected Move/Attack cannot restart"), Stopped[Index].Type == EProofCommandType::Stop);
            TestEqual(TEXT("Stop queue sequence remains unallocated"), Stopped[Index].Sequence, 0);
            TestEqual(TEXT("Stop queue epoch remains unallocated"), Stopped[Index].MatchEpoch, 0);
            TestEqual(TEXT("Stop target is canonical"), Stopped[Index].TargetUnitId, INDEX_NONE);
            TestTrue(TEXT("Stop destination is canonical"), Stopped[Index].Destination.IsZero());
        }
    TestEqual(TEXT("No same-interval Stop burst"), Q.PopReady(S, 1, 10.1).Num(), 0);
    const TArray<FProofCommand> Other = Q.PopReady(S, 1, 10.2);
    TestTrue(TEXT("Unselected unit keeps its queued Move"), Other.Num() == 1
        && Other[0].UnitId == 5 && Other[0].Type == EProofCommandType::Move && Other[0].Destination.X == 1005.0);
    TestEqual(TEXT("No superseded selected intent remains"), Q.PopReady(S, 1, 11.0).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofStopQueueAtomicityTest, "RedalertProof.Stop.QueueAtomicityAndPruning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofStopQueueAtomicityTest::RunTest(const FString&)
{
    FProofSnapshot S = StopQueueFixture();
    FProofGroupOrderQueue Q;
    TArray<FProofCommand> Full;
    for (int32 Id = 1; Id <= FProofGroupOrderQueue::MaxPendingCommands; ++Id) Full.Add(StopIntent(EProofCommandType::Move, Id));
    TestTrue(TEXT("Original 64-intent capacity remains available"), Q.Enqueue(S, 1, Full));
    TestFalse(TEXT("Overflow rejects entire Stop replacement batch"), Q.Enqueue(S, 1,
        {StopIntent(EProofCommandType::Stop, 1), StopIntent(EProofCommandType::Stop, 65)}));
    TestEqual(TEXT("Rejected Stop preserves prior queue depth"), Q.NumPending(), 64);
    const TArray<FProofCommand> Original = Q.PopReady(S, 1, 0.0);
    TestTrue(TEXT("Failed batch did not replace earlier valid Move"), Original.Num() == 2
        && Original[0].UnitId == 1 && Original[0].Type == EProofCommandType::Move && Original[0].Destination.X == 1001.0);
    TestFalse(TEXT("Foreign Stop rejects atomically"), Q.Enqueue(S, 1,
        {StopIntent(EProofCommandType::Stop, 3), StopIntent(EProofCommandType::Stop, 90)}));
    const TArray<FProofCommand> StillOriginal = Q.PopReady(S, 1, 0.1);
    TestTrue(TEXT("Foreign batch did not discard prior selected intent"), StillOriginal.Num() == 2
        && StillOriginal[0].UnitId == 3 && StillOriginal[0].Type == EProofCommandType::Move);

    FProofGroupOrderQueue Pruning;
    TestTrue(TEXT("Valid mobile Stops enqueue"), Pruning.Enqueue(S, 1,
        {StopIntent(EProofCommandType::Stop, 1), StopIntent(EProofCommandType::Stop, 2), StopIntent(EProofCommandType::Stop, 81)}));
    TestFalse(TEXT("Structures cannot queue Stop"), Pruning.Enqueue(S, 1, {StopIntent(EProofCommandType::Stop, 80)}));
    for (FProofUnitRecord& U : S.Units)
    {
        if (U.UnitId == 1) U.HitPoints = 0.f;
        if (U.UnitId == 2) U.OwnerId = 2;
        if (U.UnitId == 81) U.Kind = EProofUnitKind::Factory;
    }
    TestEqual(TEXT("Stop rechecks life, owner and mobile role at dispatch"), Pruning.PopReady(S, 1, 0.0).Num(), 0);
    TestTrue(TEXT("Fresh Stop enqueues"), Pruning.Enqueue(S, 1, {StopIntent(EProofCommandType::Stop, 3)}));
    ++S.MatchEpoch;
    TestEqual(TEXT("Epoch change removes stale Stop"), Pruning.PopReady(S, 1, 1.0).Num(), 0);
    TestTrue(TEXT("Fresh epoch Stop enqueues"), Pruning.Enqueue(S, 1, {StopIntent(EProofCommandType::Stop, 3)}));
    TestEqual(TEXT("Owner change removes stale Stop"), Pruning.PopReady(S, 2, 2.0).Num(), 0);
    TestTrue(TEXT("Stop queues before terminal state"), Pruning.Enqueue(S, 1, {StopIntent(EProofCommandType::Stop, 3)}));
    S.MatchPhase = EProofMatchPhase::Finished;
    TestEqual(TEXT("Terminal state prunes pending Stop"), Pruning.PopReady(S, 1, 3.0).Num(), 0);
    return true;
}
#endif
