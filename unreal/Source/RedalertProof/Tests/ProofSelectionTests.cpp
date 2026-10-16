#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofSelectionState.h"

namespace
{
void AddPlayer(FProofSnapshot& Snapshot, int32 OwnerId, int32 TeamId)
{
    FProofPlayerRecord Player;
    Player.OwnerId = OwnerId;
    Player.TeamId = TeamId;
    Snapshot.Players.Add(Player);
}

void AddUnit(FProofSnapshot& Snapshot, int32 UnitId, int32 OwnerId, EProofUnitKind Kind,
    const FVector& Position, float HitPoints = 100.f)
{
    FProofUnitRecord Unit;
    Unit.UnitId = UnitId;
    Unit.OwnerId = OwnerId;
    Unit.Kind = Kind;
    Unit.Position = Position;
    Unit.HitPoints = HitPoints;
    Snapshot.Units.Add(Unit);
}

FProofSnapshot SelectionFixture()
{
    FProofSnapshot Snapshot;
    Snapshot.MatchEpoch = 1;
    Snapshot.bSkirmish = true;
    Snapshot.MatchPhase = EProofMatchPhase::Playing;
    AddPlayer(Snapshot, 1, 0);
    AddPlayer(Snapshot, 2, 0);
    AddPlayer(Snapshot, 3, 1);
    AddUnit(Snapshot, 10, 1, EProofUnitKind::Combat, FVector(100, 0, 0));
    AddUnit(Snapshot, 20, 1, EProofUnitKind::Harvester, FVector(200, 100, 0));
    AddUnit(Snapshot, 30, 1, EProofUnitKind::HQ, FVector(0, 0, 0));
    AddUnit(Snapshot, 40, 2, EProofUnitKind::Combat, FVector(500, 0, 0));
    AddUnit(Snapshot, 50, 3, EProofUnitKind::Combat, FVector(800, 0, 0));
    AddUnit(Snapshot, 60, 1, EProofUnitKind::Combat, FVector(300, 0, 0), 0.f);
    return Snapshot;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSelectionOwnershipTest, "RedalertProof.Controls.SelectionOwnershipAndPruning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSelectionOwnershipTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = SelectionFixture();
    FProofSelectionState Selection;
    Selection.SelectSingle(Snapshot, 1, 40);
    TestEqual(TEXT("Foreign click clears selection"), Selection.GetSelectedUnitIds().Num(), 0);
    Selection.SelectSingle(Snapshot, 1, 60);
    TestEqual(TEXT("Dead click clears selection"), Selection.GetPrimaryUnitId(), INDEX_NONE);
    Selection.SelectSingle(Snapshot, 1, 20);
    Selection.Toggle(Snapshot, 1, 10);
    TestEqual(TEXT("Two owned live units selected"), Selection.GetSelectedUnitIds().Num(), 2);
    TestEqual(TEXT("Selection order follows IDs"), Selection.GetSelectedUnitIds()[0], 10);
    TestEqual(TEXT("Last clicked unit is primary"), Selection.GetPrimaryUnitId(), 10);
    Selection.Toggle(Snapshot, 1, 40);
    Selection.Toggle(Snapshot, 1, 60);
    TestEqual(TEXT("Foreign and dead toggles are ignored"), Selection.GetSelectedUnitIds().Num(), 2);
    Selection.Toggle(Snapshot, 1, 10);
    TestEqual(TEXT("Shift toggle removes an owned unit"), Selection.GetPrimaryUnitId(), 20);
    Selection.AssignGroup(1, Snapshot, 1);

    for (FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.UnitId == 20) { Unit.HitPoints = 0.f; }
    }
    Selection.Synchronize(Snapshot, 1);
    TestEqual(TEXT("Current selection prunes dead unit"), Selection.GetSelectedUnitIds().Num(), 0);
    Selection.RecallGroup(1, Snapshot, 1);
    TestEqual(TEXT("Group recall prunes dead unit"), Selection.GetPrimaryUnitId(), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSelectionGroupsTest, "RedalertProof.Controls.SelectionGroupsAndEpochs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSelectionGroupsTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = SelectionFixture();
    FProofSelectionState Selection;
    Selection.SelectSingle(Snapshot, 1, 20);
    Selection.Toggle(Snapshot, 1, 10);
    Selection.AssignGroup(2, Snapshot, 1);
    Selection.SelectSingle(Snapshot, 1, 30);
    const FString BeforeRecall = Snapshot.ComputeDigest();
    Selection.RecallGroup(2, Snapshot, 1);
    TestEqual(TEXT("Recall restores bounded copied IDs"), Selection.GetSelectedUnitIds().Num(), 2);
    TestEqual(TEXT("Recall restores assigned primary"), Selection.GetPrimaryUnitId(), 10);
    TestEqual(TEXT("Recall never changes snapshot"), Snapshot.ComputeDigest(), BeforeRecall);

    for (FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.UnitId == 20) { Unit.OwnerId = 2; }
    }
    Selection.SelectSingle(Snapshot, 1, 30);
    const FString BeforeForeignRecall = Snapshot.ComputeDigest();
    Selection.RecallGroup(2, Snapshot, 1);
    TestEqual(TEXT("Recall drops a unit transferred to another owner"), Selection.GetSelectedUnitIds().Num(), 1);
    TestEqual(TEXT("Recall keeps still owned unit"), Selection.GetPrimaryUnitId(), 10);
    TestEqual(TEXT("Filtered recall leaves snapshot unchanged"), Snapshot.ComputeDigest(), BeforeForeignRecall);

    // Same numeric ID must not carry a selection or control group into a new match.
    Snapshot.MatchEpoch = 2;
    Selection.Synchronize(Snapshot, 1);
    TestEqual(TEXT("Epoch change clears selection despite ID reuse"), Selection.GetSelectedUnitIds().Num(), 0);
    Selection.RecallGroup(2, Snapshot, 1);
    TestEqual(TEXT("Epoch change clears saved group despite ID reuse"), Selection.GetSelectedUnitIds().Num(), 0);
    Selection.Toggle(Snapshot, 1, 10);
    TestEqual(TEXT("Fresh epoch permits new selection"), Selection.GetPrimaryUnitId(), 10);
    Selection.AssignGroup(2, Snapshot, 1);
    for (FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.UnitId == 10) { Unit.OwnerId = 2; }
    }
    TestEqual(TEXT("Old owner's selection cannot plan for new owner before synchronization"),
        Selection.PlanMove(Snapshot, 2, FVector::ZeroVector).Num(), 0);
    Selection.Synchronize(Snapshot, 2);
    TestEqual(TEXT("Owner change clears current selection despite ID reuse"), Selection.GetSelectedUnitIds().Num(), 0);
    Selection.RecallGroup(2, Snapshot, 2);
    TestEqual(TEXT("Owner change clears saved group despite ID reuse"), Selection.GetSelectedUnitIds().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSelectionPlansTest, "RedalertProof.Controls.SelectionDeterministicPlans",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSelectionPlansTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = SelectionFixture();
    FProofSelectionState Selection;
    Selection.SelectSingle(Snapshot, 1, 30);
    Selection.Toggle(Snapshot, 1, 20);
    Selection.Toggle(Snapshot, 1, 10);
    const FString BeforePlans = Snapshot.ComputeDigest();

    const TArray<FProofCommand> Moves = Selection.PlanMove(Snapshot, 1, FVector(10000, 0, 0));
    TestEqual(TEXT("Immobile HQ excluded from group move"), Moves.Num(), 2);
    if (Moves.Num() == 2)
    {
        TestEqual(TEXT("Move intent order is numeric"), Moves[0].UnitId, 10);
        TestEqual(TEXT("Second move intent order is numeric"), Moves[1].UnitId, 20);
        TestTrue(TEXT("Formation reaches bound without crossing it"),
            Moves[0].Destination.Equals(FVector(9900, -50, 0), .001)
            && Moves[1].Destination.Equals(FVector(10000, 50, 0), .001));
        TestEqual(TEXT("Planner leaves sequence to pawn"), Moves[0].Sequence, 0);
        TestEqual(TEXT("Planner leaves epoch to pawn"), Moves[0].MatchEpoch, 0);
    }
    const TArray<FProofCommand> Attacks = Selection.PlanAttack(Snapshot, 1, 50);
    TestEqual(TEXT("Only combat unit attacks enemy team"), Attacks.Num(), 1);
    if (Attacks.Num() == 1)
    {
        TestEqual(TEXT("Attack belongs to owned combat unit"), Attacks[0].UnitId, 10);
        TestEqual(TEXT("Attack targets requested enemy"), Attacks[0].TargetUnitId, 50);
    }
    TestEqual(TEXT("Same-team target yields no attack"), Selection.PlanAttack(Snapshot, 1, 40).Num(), 0);
    TestEqual(TEXT("Own target yields no attack"), Selection.PlanAttack(Snapshot, 1, 10).Num(), 0);
    TestEqual(TEXT("Planning does not mutate snapshot"), Snapshot.ComputeDigest(), BeforePlans);
    TestEqual(TEXT("Planning does not mutate selection"), Selection.GetSelectedUnitIds().Num(), 3);
    FProofSnapshot Legacy = Snapshot;
    Legacy.bSkirmish = false;
    TestEqual(TEXT("Legacy attack permits a different owner with the same default team"),
        Selection.PlanAttack(Legacy, 1, 40).Num(), 1);
    for (FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.UnitId == 50) { Unit.HitPoints = 0.f; }
    }
    TestEqual(TEXT("Dead target yields no attack"), Selection.PlanAttack(Snapshot, 1, 50).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSelectionCapTest, "RedalertProof.Controls.SelectionBoundedAndStale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSelectionCapTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = SelectionFixture();
    for (int32 Id = 100; Id < 114; ++Id)
    {
        AddUnit(Snapshot, Id, 1, EProofUnitKind::Combat, FVector(Id, 0, 0));
    }
    FProofSelectionState Selection;
    for (int32 Id = 113; Id >= 100; --Id) { Selection.Toggle(Snapshot, 1, Id); }
    TestEqual(TEXT("Selection is capped at fixture record cap"), Selection.GetSelectedUnitIds().Num(), FProofCommandModel::SkirmishRecordCap);
    TestEqual(TEXT("Bounded selection remains sorted"), Selection.GetSelectedUnitIds()[0], 102);
    TestEqual(TEXT("Move plan stays bounded"), Selection.PlanMove(Snapshot, 1, FVector::ZeroVector).Num(),
        FProofCommandModel::SkirmishRecordCap);

    // A client can plan with a retained copy; revalidation must still reject a
    // unit whose ownership or life has changed in the newer snapshot.
    for (FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.UnitId == 102) { Unit.OwnerId = 2; }
        if (Unit.UnitId == 103) { Unit.HitPoints = 0.f; }
    }
    TestEqual(TEXT("Plan rechecks ownership and life in latest snapshot"),
        Selection.PlanMove(Snapshot, 1, FVector::ZeroVector).Num(), 10);
    Selection.Synchronize(Snapshot, 1);
    TestEqual(TEXT("Selection prunes stale IDs in latest snapshot"), Selection.GetSelectedUnitIds().Num(), 10);
    return true;
}
#endif
