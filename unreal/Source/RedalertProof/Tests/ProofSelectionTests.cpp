#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofSelectionState.h"
#include <limits>

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

    for (float InvalidHealth : { std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() })
    {
        FProofSnapshot Invalid = SelectionFixture();
        FProofSelectionState InvalidSelection;
        InvalidSelection.SelectSingle(Invalid, 1, 10);
        InvalidSelection.AssignGroup(1, Invalid, 1);
        Invalid.Units[0].HitPoints = InvalidHealth;
        TestEqual(TEXT("Latest non-finite health cannot plan movement before pruning"),
            InvalidSelection.PlanMove(Invalid, 1, FVector(800, 0, 0)).Num(), 0);
        TestEqual(TEXT("Latest non-finite health cannot plan an attack before pruning"),
            InvalidSelection.PlanAttack(Invalid, 1, 50).Num(), 0);
        InvalidSelection.Synchronize(Invalid, 1);
        TestEqual(TEXT("Synchronization prunes non-finite health"), InvalidSelection.GetSelectedUnitIds().Num(), 0);
        InvalidSelection.RecallGroup(1, Invalid, 1);
        TestEqual(TEXT("Saved group prunes non-finite health"), InvalidSelection.GetPrimaryUnitId(), INDEX_NONE);
        InvalidSelection.SelectSingle(Invalid, 1, 10);
        TestEqual(TEXT("Click does not select non-finite health"), InvalidSelection.GetSelectedUnitIds().Num(), 0);
        FProofSelectionState ToggleSelection;
        ToggleSelection.Toggle(Invalid, 1, 10);
        TestEqual(TEXT("Toggle does not select non-finite health"), ToggleSelection.GetSelectedUnitIds().Num(), 0);
    }
    FProofSnapshot Fractional = SelectionFixture();
    Fractional.Units[0].HitPoints = 0.01f;
    Selection.SelectSingle(Fractional, 1, 10);
    TestEqual(TEXT("Positive finite fractional health remains selectable"), Selection.GetPrimaryUnitId(), 10);
    TestEqual(TEXT("Positive finite fractional health remains commandable"),
        Selection.PlanMove(Fractional, 1, FVector(800, 0, 0)).Num(), 1);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSelectionNineGroupsTest, "RedalertProof.Controls.SelectionNineGroups",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSelectionNineGroupsTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = SelectionFixture();
    for (int32 UnitId = 70; UnitId <= 75; ++UnitId)
    {
        AddUnit(Snapshot, UnitId, 1, EProofUnitKind::Combat, FVector(UnitId, 0, 0));
    }
    const int32 GroupUnitIds[FProofSelectionState::MaxControlGroups] = { 10, 20, 30, 70, 71, 72, 73, 74, 75 };
    FProofSelectionState Selection;
    for (int32 Slot = 1; Slot <= FProofSelectionState::MaxControlGroups; ++Slot)
    {
        Selection.SelectSingle(Snapshot, 1, GroupUnitIds[Slot - 1]);
        Selection.AssignGroup(Slot, Snapshot, 1);
    }
    Selection.SelectSingle(Snapshot, 1, 10);
    const FString BeforeRecall = Snapshot.ComputeDigest();
    for (int32 Slot = 1; Slot <= FProofSelectionState::MaxControlGroups; ++Slot)
    {
        Selection.RecallGroup(Slot, Snapshot, 1);
        TestEqual(FString::Printf(TEXT("Group %d retains its independent copy"), Slot),
            Selection.GetPrimaryUnitId(), GroupUnitIds[Slot - 1]);
        TestEqual(FString::Printf(TEXT("Group %d has one selected unit"), Slot),
            Selection.GetSelectedUnitIds().Num(), 1);
    }
    TestEqual(TEXT("Recalling nine groups does not mutate snapshot"), Snapshot.ComputeDigest(), BeforeRecall);

    Selection.SelectSingle(Snapshot, 1, 20);
    Selection.AssignGroup(0, Snapshot, 1);
    Selection.AssignGroup(FProofSelectionState::MaxControlGroups + 1, Snapshot, 1);
    Selection.RecallGroup(0, Snapshot, 1);
    Selection.RecallGroup(FProofSelectionState::MaxControlGroups + 1, Snapshot, 1);
    TestEqual(TEXT("Invalid slots leave current selection unchanged"), Selection.GetPrimaryUnitId(), 20);
    Selection.RecallGroup(9, Snapshot, 1);
    TestEqual(TEXT("Invalid assignments leave group nine unchanged"), Selection.GetPrimaryUnitId(), 75);
    Selection.RecallGroup(1, Snapshot, 1);
    TestEqual(TEXT("Invalid assignments leave group one unchanged"), Selection.GetPrimaryUnitId(), 10);

    Selection.SelectSingle(Snapshot, 1, 73);
    Selection.Toggle(Snapshot, 1, 74);
    Selection.Toggle(Snapshot, 1, 75);
    Selection.AssignGroup(8, Snapshot, 1);
    for (FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.UnitId == 74) { Unit.HitPoints = 0.f; }
        if (Unit.UnitId == 75) { Unit.OwnerId = 2; }
    }
    Selection.SelectSingle(Snapshot, 1, 10);
    const FString BeforePrunedRecall = Snapshot.ComputeDigest();
    Selection.RecallGroup(8, Snapshot, 1);
    TestEqual(TEXT("Upper slot filters dead and foreign units"), Selection.GetSelectedUnitIds().Num(), 1);
    TestEqual(TEXT("Upper slot keeps still-owned live unit"), Selection.GetPrimaryUnitId(), 73);
    TestEqual(TEXT("Upper-slot filtering does not mutate snapshot"), Snapshot.ComputeDigest(), BeforePrunedRecall);

    Snapshot.MatchEpoch = 2;
    Selection.Synchronize(Snapshot, 1);
    Selection.RecallGroup(8, Snapshot, 1);
    TestEqual(TEXT("Epoch change clears upper group eight"), Selection.GetSelectedUnitIds().Num(), 0);
    Selection.RecallGroup(9, Snapshot, 1);
    TestEqual(TEXT("Epoch change clears upper group nine"), Selection.GetSelectedUnitIds().Num(), 0);
    Selection.SelectSingle(Snapshot, 1, 73);
    Selection.AssignGroup(9, Snapshot, 1);
    for (FProofUnitRecord& Unit : Snapshot.Units)
    {
        if (Unit.UnitId == 73) { Unit.OwnerId = 2; }
    }
    Selection.Synchronize(Snapshot, 2);
    Selection.RecallGroup(9, Snapshot, 2);
    TestEqual(TEXT("Owner change clears upper group nine despite reused ID"), Selection.GetSelectedUnitIds().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofCooperativeContextTargetsTest, "RedalertProof.Controls.CooperativeContextTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofCooperativeContextTargetsTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = SelectionFixture();
    const FString Before = Snapshot.ComputeDigest();
    TestFalse(TEXT("Owned marker follows friendly ground behavior"), FProofSelectionState::IsHostileTarget(Snapshot, 1, 10));
    TestFalse(TEXT("Cooperative teammate is friendly despite different owner"), FProofSelectionState::IsHostileTarget(Snapshot, 1, 40));
    TestTrue(TEXT("Opposing team remains hostile"), FProofSelectionState::IsHostileTarget(Snapshot, 1, 50));
    TestFalse(TEXT("Invalid target ID is not hostile"), FProofSelectionState::IsHostileTarget(Snapshot, 1, INDEX_NONE));
    TestFalse(TEXT("Missing target ID is not hostile"), FProofSelectionState::IsHostileTarget(Snapshot, 1, 999));
    TestFalse(TEXT("Unknown viewer cannot classify an enemy"), FProofSelectionState::IsHostileTarget(Snapshot, 99, 50));

    FProofSelectionState Selection;
    Selection.SelectSingle(Snapshot, 1, 10);
    Selection.Toggle(Snapshot, 1, 20);
    const FVector Destination(500, 0, 0);
    const auto AllyOrders = FProofSelectionState::IsHostileTarget(Snapshot, 1, 40)
        ? Selection.PlanAttack(Snapshot, 1, 40) : Selection.PlanMove(Snapshot, 1, Destination);
    TestEqual(TEXT("Ally ground route moves Combat and Harvester"), AllyOrders.Num(), 2);
    for (const FProofCommand& Command : AllyOrders)
        TestTrue(TEXT("Ally click cannot plan an Attack"), Command.Type == EProofCommandType::Move);
    TestEqual(TEXT("Direct attack planner still rejects allies"), Selection.PlanAttack(Snapshot, 1, 40).Num(), 0);
    const auto EnemyOrders = Selection.PlanAttack(Snapshot, 1, 50);
    TestEqual(TEXT("Enemy still gets one eligible Combat attack"), EnemyOrders.Num(), 1);
    TestEqual(TEXT("Routing cannot change authoritative snapshot"), Snapshot.ComputeDigest(), Before);

    FProofSnapshot MissingOwner = Snapshot;
    MissingOwner.Units.FindByPredicate([](const FProofUnitRecord& Unit) { return Unit.UnitId == 50; })->OwnerId = 99;
    TestFalse(TEXT("Missing target participant is not hostility"), FProofSelectionState::IsHostileTarget(MissingOwner, 1, 50));
    FProofSnapshot Dead = Snapshot;
    auto* Target = Dead.Units.FindByPredicate([](const FProofUnitRecord& Unit) { return Unit.UnitId == 50; });
    Target->HitPoints = 0.f;
    TestFalse(TEXT("Dead enemy is not a current attack marker"), FProofSelectionState::IsHostileTarget(Dead, 1, 50));
    Target->HitPoints = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("NaN health is not live"), FProofSelectionState::IsHostileTarget(Dead, 1, 50));
    Target->HitPoints = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Infinite health is not a valid target"), FProofSelectionState::IsHostileTarget(Dead, 1, 50));
    FProofSnapshot UnknownTeam = Snapshot;
    UnknownTeam.Players.FindByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == 3; })->TeamId = INDEX_NONE;
    TestFalse(TEXT("Unknown team cannot establish hostility"), FProofSelectionState::IsHostileTarget(UnknownTeam, 1, 50));

    FProofSnapshot Legacy = Snapshot;
    Legacy.bSkirmish = false;
    TestTrue(TEXT("Legacy distinct owners with default team0 remain enemies"), FProofSelectionState::IsHostileTarget(Legacy, 1, 40));
    Selection.SelectSingle(Legacy, 1, 10);
    TestEqual(TEXT("Legacy same-team-number attack planning preserved"), Selection.PlanAttack(Legacy, 1, 40).Num(), 1);
    Legacy.Players.RemoveAll([](const FProofPlayerRecord& Player) { return Player.OwnerId == 2; });
    TestFalse(TEXT("Legacy still requires target player record"), FProofSelectionState::IsHostileTarget(Legacy, 1, 40));
    return true;
}

#endif
