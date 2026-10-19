#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofBoxSelectionGesture.h"
#include "Presentation/ProofSelectionState.h"
#include <limits>

namespace
{
FProofSnapshot BoxFixture()
{
    FProofSnapshot S;
    S.MatchEpoch = 7;
    S.bSkirmish = true;
    S.MatchPhase = EProofMatchPhase::Playing;
    FProofPlayerRecord P; P.OwnerId = 1; S.Players.Add(P);
    P.OwnerId = 2; S.Players.Add(P);
    for (int32 Id = 1; Id <= 20; ++Id)
    {
        FProofUnitRecord U; U.UnitId = Id; U.OwnerId = 1;
        U.Kind = Id == 20 ? EProofUnitKind::Factory : (Id == 2 ? EProofUnitKind::Harvester : EProofUnitKind::Combat);
        U.Position = FVector(Id * 10, 0, 0);
        S.Units.Add(U);
    }
    S.Units[16].OwnerId = 2;
    S.Units[17].HitPoints = 0.f;
    S.Units[18].Position.X = std::numeric_limits<double>::infinity();
    S.Units[15].HitPoints = std::numeric_limits<float>::infinity();
    S.Units[14].Position.Y = std::numeric_limits<double>::quiet_NaN();
    S.Units[13].Position.Z = FProofCommandModel::WorldExtent + 1.0;
    return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBoxAtomicTest, "RedalertProof.Controls.BoxSelectionAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBoxAtomicTest::RunTest(const FString&)
{
    FProofSnapshot S = BoxFixture();
    const FString Digest = S.ComputeDigest();
    FProofSelectionState Selection;
    Selection.SelectSingle(S, 1, 20); // Click-selected structure remains on Shift-box.
    const uint64 Before = Selection.GetSelectionRevision();
    const ProofBoxSelection::Result Hits{true, {2, 1, 2, 14, 15, 16, 17, 18, 19, 20, -1, 0}};
    TestTrue(TEXT("Atomic additive accepted"), Selection.ApplyBoxSelection(S, 1, 1, 7, Hits, true));
    TestTrue(TEXT("One explicit revision"), Selection.GetSelectionRevision() == Before + 1);
    TestTrue(TEXT("Additive keeps structure and valid mobile hits, sorted"), Selection.GetSelectedUnitIds() == TArray<int32>({1, 2, 20}));
    TestEqual(TEXT("Preserves valid primary"), Selection.GetPrimaryUnitId(), 20);
    TestTrue(TEXT("Replacement accepted"), Selection.ApplyBoxSelection(S, 1, 1, 7, Hits, false));
    TestTrue(TEXT("Replacement excludes structure, foreign, dead, nonfinite"), Selection.GetSelectedUnitIds() == TArray<int32>({1, 2}));
    TestEqual(TEXT("Replacement primary lowest"), Selection.GetPrimaryUnitId(), 1);
    const ProofBoxSelection::Result Empty{true, {}};
    Selection.ApplyBoxSelection(S, 1, 1, 7, Empty, true);
    TestTrue(TEXT("Valid Shift-empty preserves selection"), Selection.GetSelectedUnitIds() == TArray<int32>({1, 2}));
    const uint64 InvalidBefore = Selection.GetSelectionRevision();
    TestFalse(TEXT("Invalid plan cancels"), Selection.ApplyBoxSelection(S, 1, 1, 7, {}, false));
    TestTrue(TEXT("Invalid has no explicit revision or clearing"), Selection.GetSelectionRevision() == InvalidBefore && Selection.GetSelectedUnitIds().Num() == 2);
    Selection.ApplyBoxSelection(S, 1, 1, 7, Empty, false);
    TestTrue(TEXT("Valid empty clears with no primary"), Selection.GetSelectedUnitIds().IsEmpty() && Selection.GetPrimaryUnitId() == INDEX_NONE);
    TestEqual(TEXT("Selection never mutates snapshot"), S.ComputeDigest(), Digest);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBoxContextTest, "RedalertProof.Controls.BoxSelectionContextAndCapacity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBoxContextTest::RunTest(const FString&)
{
    FProofSnapshot S = BoxFixture();
    FProofSelectionState Selection;
    Selection.SelectSingle(S, 1, 20);
    for (int32 Id = 1; Id <= 10; ++Id) Selection.Toggle(S, 1, Id);
    const uint64 Before = Selection.GetSelectionRevision();
    TestFalse(TEXT("Expected owner mismatch rejects"), Selection.ApplyBoxSelection(S, 1, 2, 7, {true, {11}}, false));
    TestFalse(TEXT("Expected epoch mismatch rejects"), Selection.ApplyBoxSelection(S, 1, 1, 8, {true, {11}}, false));
    FProofSnapshot Changed = S; Changed.MatchEpoch++;
    TestFalse(TEXT("Current epoch mismatch rejects"), Selection.ApplyBoxSelection(Changed, 1, 1, 7, {true, {11}}, false));
    Changed = S; Changed.MatchPhase = EProofMatchPhase::Finished;
    TestFalse(TEXT("Terminal rejects"), Selection.ApplyBoxSelection(Changed, 1, 1, 7, {true, {11}}, false));
    Changed = S; Changed.bSkirmish = false;
    TestFalse(TEXT("Legacy rejects box"), Selection.ApplyBoxSelection(Changed, 1, 1, 7, {true, {11}}, false));
    Changed = S; Changed.Players.Reset();
    TestFalse(TEXT("Missing current player rejects"), Selection.ApplyBoxSelection(Changed, 1, 1, 7, {true, {11}}, false));
    TestTrue(TEXT("Rejected plans preserve revision and current primary"), Selection.GetSelectionRevision() == Before && Selection.GetPrimaryUnitId() == 10);

    std::vector<ProofBoxSelection::Candidate> Candidates;
    for (int32 Id = 12; Id >= 1; --Id) Candidates.push_back({Id, {800, 400}, true});
    const auto FullPlan = ProofBoxSelection::Build(Candidates.data(), Candidates.size(), {750, 350}, {850, 450}, 1280, 720,
        FProofSelectionState::MaxSelectedUnits);
    TestTrue(TEXT("Full planner limit lets overlap precede new hit"), Selection.ApplyBoxSelection(S, 1, 1, 7, FullPlan, true));
    TestEqual(TEXT("Bounded at current cap"), Selection.GetSelectedUnitIds().Num(), FProofSelectionState::MaxSelectedUnits);
    TestTrue(TEXT("Existing 1..10 and structure retained; 11 fills slot, 12 omitted"), Selection.GetSelectedUnitIds().Contains(20)
        && Selection.GetSelectedUnitIds().Contains(11) && !Selection.GetSelectedUnitIds().Contains(12));
    TestEqual(TEXT("Primary unchanged by additive cap"), Selection.GetPrimaryUnitId(), 10);
    S.Units[9].HitPoints = 0.f;
    S.Units[19].HitPoints = std::numeric_limits<float>::infinity();
    TestTrue(TEXT("Shift-empty stages current valid state"), Selection.ApplyBoxSelection(S, 1, 1, 7, {true, {}}, true));
    TestTrue(TEXT("Prunes invalid existing structure and primary"), !Selection.GetSelectedUnitIds().Contains(20) && !Selection.GetSelectedUnitIds().Contains(10));
    TestEqual(TEXT("Fallback primary lowest"), Selection.GetPrimaryUnitId(), 1);
    Selection.Synchronize(S, 2);
    TestFalse(TEXT("Selection owner context mismatch rejects"), Selection.ApplyBoxSelection(S, 1, 1, 7, {true, {1}}, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBoxGestureTest, "RedalertProof.Controls.BoxSelectionGesture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBoxGestureTest::RunTest(const FString&)
{
    using namespace ProofBoxSelectionGesture;
    const Context C{1, 7, 1, 1280, 720, 1, 2};
    State G;
    TestTrue(TEXT("Valid press arms"), G.Begin(C, {800, 400}, true, 20, true));
    TestFalse(TEXT("Duplicate press ignored"), G.Begin(C, {900, 500}, false, 1, true));
    G.Sample(C, {802, 402}, true);
    TestFalse(TEXT("Euclidean subthreshold stays click"), G.IsDragging());
    const Release Click = G.EndPress(C, {802, 402}, true);
    TestTrue(TEXT("Click retains press target and Shift"), Click.Action == Kind::Click && Click.ClickTarget == 20 && Click.Additive);
    TestTrue(TEXT("Duplicate release inert"), G.EndPress(C, {802, 402}, true).Action == Kind::None);
    G.Begin(C, {800, 400}, false, -1, true);
    TestTrue(TEXT("Release-only threshold is drag"), G.EndPress(C, {804, 400}, true).Action == Kind::Box);
    G.Begin(C, {800, 400}, false, -1, true);
    TestTrue(TEXT("Next representable below threshold stays click"),
        G.EndPress(C, {std::nextafter(804.0, 800.0), 400}, true).Action == Kind::Click);
    G.Begin(C, {800, 400}, false, -1, true);
    TestTrue(TEXT("Next representable above threshold is drag"),
        G.EndPress(C, {std::nextafter(804.0, 810.0), 400}, true).Action == Kind::Box);
    for (int32 Direction = 0; Direction < 4; ++Direction)
    {
        G.Begin(C, {800, 400}, false, -1, true);
        const double X = Direction & 1 ? 810.0 : 790.0;
        const double Y = Direction & 2 ? 410.0 : 390.0;
        TestTrue(TEXT("Every drag direction becomes box"), G.EndPress(C, {X, Y}, true).Action == Kind::Box);
    }
    G.Begin(C, {800, 400}, false, -1, true);
    G.Sample(C, {803, 403}, true);
    TestTrue(TEXT("Diagonal threshold uses Euclidean distance"), G.IsDragging());
    G.Sample(C, {800, 400}, true);
    const Release Returned = G.EndPress(C, {800, 400}, true);
    TestTrue(TEXT("Returning to start remains zero-area box"), Returned.Action == Kind::Box && Returned.Start.X == Returned.End.X && Returned.Start.Y == Returned.End.Y);
    for (int32 Change = 0; Change < 7; ++Change)
    {
        Context Other = C;
        if (Change == 0) Other.Owner++;
        if (Change == 1) Other.Epoch++;
        if (Change == 2) Other.Phase++;
        if (Change == 3) Other.Width++;
        if (Change == 4) Other.Height++;
        if (Change == 5) Other.Controller++;
        if (Change == 6) Other.Viewport++;
        G.Begin(C, {800, 400}, false, 1, true);
        G.Sample(Other, {810, 410}, true);
        TestFalse(TEXT("Changed context cancels"), G.IsArmed());
        TestFalse(TEXT("Cannot rearm while held"), G.Begin(C, {800, 400}, false, 1, true));
        TestTrue(TEXT("Canceled release inert"), G.EndPress(C, {810, 410}, true).Action == Kind::None);
    }
    G.Begin(C, {800, 400}, false, 1, true);
    G.Sample(C, {810, 410}, false); // UI/focus/pointer invalidation.
    TestFalse(TEXT("Invalid sample hides outline"), G.IsDragging());
    G.ObserveButton(true);
    TestFalse(TEXT("Refocus while held cannot resume"), G.Begin(C, {800, 400}, false, 1, true));
    G.ObserveButton(false);
    TestTrue(TEXT("Observed up permits fresh press"), G.Begin(C, {800, 400}, false, 1, true));
    G.ObserveButton(false);
    TestTrue(TEXT("Lost release discarded"), G.EndPress(C, {800, 400}, true).Action == Kind::None);
    TestFalse(TEXT("Right viewport edge excluded"), G.Begin(C, {1280, 400}, false, 1, true));
    G.EndPress(C, {800, 400}, true);
    TestFalse(TEXT("Nonfinite pointer excluded"), G.Begin(C, {std::numeric_limits<double>::quiet_NaN(), 400}, false, 1, true));
    G.EndPress(C, {800, 400}, true);
    G.Begin(C, {800, 400}, false, 1, true);
    TestTrue(TEXT("Invalid release cancels"), G.EndPress(C, {810, 410}, false).Action == Kind::None);
    return true;
}
#endif
