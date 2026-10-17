#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofSelectionState.h"
#include <limits>

namespace
{
void TypeSelectionAddRecord(FProofSnapshot& S, int32 Id, int32 Owner, EProofUnitKind Kind, float HP = 100.f)
{
    FProofUnitRecord U; U.UnitId = Id; U.OwnerId = Owner; U.Kind = Kind; U.HitPoints = HP;
    U.Position = FVector(Id * 10.0, 0, 0); S.Units.Add(U);
}
FProofSnapshot TypeSelectionFixtureSnapshot()
{
    FProofSnapshot S; S.MatchEpoch = 1; S.bSkirmish = true; S.MatchPhase = EProofMatchPhase::Playing;
    for (int32 Owner : {1, 2, 100}) { FProofPlayerRecord P; P.OwnerId = Owner; P.TeamId = Owner == 100 ? 1 : 0; S.Players.Add(P); }
    TypeSelectionAddRecord(S, 10, 1, EProofUnitKind::Combat);
    TypeSelectionAddRecord(S, 20, 1, EProofUnitKind::Harvester);
    TypeSelectionAddRecord(S, 30, 1, EProofUnitKind::Combat);
    TypeSelectionAddRecord(S, 40, 1, EProofUnitKind::Combat); // Not in supplied visible centers.
    TypeSelectionAddRecord(S, 50, 1, EProofUnitKind::Factory);
    TypeSelectionAddRecord(S, 55, 1, EProofUnitKind::Factory);
    TypeSelectionAddRecord(S, 60, 2, EProofUnitKind::Combat);
    TypeSelectionAddRecord(S, 70, 100, EProofUnitKind::Combat);
    TypeSelectionAddRecord(S, 80, 1, EProofUnitKind::Combat, 0.f);
    TypeSelectionAddRecord(S, 90, 1, EProofUnitKind::Combat);
    S.Units.Last().Position.X = std::numeric_limits<double>::quiet_NaN();
    TypeSelectionAddRecord(S, 95, 1, EProofUnitKind::Combat, std::numeric_limits<float>::infinity());
    return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofVisibleTypeSelectionTest, "RedalertProof.TypeSelection.VisibleKindOwnershipAndPrimary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofVisibleTypeSelectionTest::RunTest(const FString&)
{
    const FProofSnapshot S = TypeSelectionFixtureSnapshot();
    const FString Before = S.ComputeDigest();
    const TArray<int32> Visible = {95, 90, 80, 70, 60, 55, 50, 30, 20, 10, 10, 999, 0, -1};
    FProofSelectionState Selection;
    Selection.SelectSingle(S, 1, 20); Selection.Toggle(S, 1, 30); Selection.AssignGroup(1, S, 1);
    TestTrue(TEXT("Current live primary determines kind of mixed selection"), Selection.SelectVisibleType(S, 1, Visible));
    TestTrue(TEXT("Only live owned visible Combat selected, IDs sorted and unique"), Selection.GetSelectedUnitIds() == TArray<int32>({10, 30}));
    TestEqual(TEXT("Visible primary retained"), Selection.GetPrimaryUnitId(), 30);
    TestFalse(TEXT("Same-kind offscreen record excluded"), Selection.GetSelectedUnitIds().Contains(40));
    TestEqual(TEXT("Selection never changes any authoritative record"), S.ComputeDigest(), Before);
    TestTrue(TEXT("Repeated T remains viewport-only"), Selection.SelectVisibleType(S, 1, Visible));
    TestTrue(TEXT("Repeated T cannot expand globally"), Selection.GetSelectedUnitIds() == TArray<int32>({10, 30}));
    Selection.RecallGroup(1, S, 1);
    TestTrue(TEXT("Saved group remains an independent mixed copy"), Selection.GetSelectedUnitIds() == TArray<int32>({20, 30}));
    TestEqual(TEXT("Group primary not overwritten"), Selection.GetPrimaryUnitId(), 30);
    Selection.SelectSingle(S, 1, 20); Selection.SelectVisibleType(S, 1, Visible);
    TestTrue(TEXT("Harvester kind does not select Combat"), Selection.GetSelectedUnitIds() == TArray<int32>({20}));
    Selection.SelectSingle(S, 1, 50); Selection.SelectVisibleType(S, 1, Visible);
    TestTrue(TEXT("Structures use their own current generic kind"), Selection.GetSelectedUnitIds() == TArray<int32>({50, 55}));
    TestEqual(TEXT("Structures retain primary"), Selection.GetPrimaryUnitId(), 50);
    Selection.SelectSingle(S, 1, 40);
    TestTrue(TEXT("An offscreen primary can supply kind without being selected"), Selection.SelectVisibleType(S, 1, Visible));
    TestTrue(TEXT("Only supplied visible centers selected when anchor is offscreen"), Selection.GetSelectedUnitIds() == TArray<int32>({10, 30}));
    TestEqual(TEXT("Offscreen anchor replaced by lowest visible ID"), Selection.GetPrimaryUnitId(), 10);
    TestTrue(TEXT("Valid kind with no visible candidates selects none"), Selection.SelectVisibleType(S, 1, TArray<int32>()));
    TestEqual(TEXT("No visible candidates clears current selection"), Selection.GetSelectedUnitIds().Num(), 0);
    TestFalse(TEXT("Empty primary cannot invent a kind"), Selection.SelectVisibleType(S, 1, Visible));
    TestEqual(TEXT("No-primary action leaves empty selection"), Selection.GetSelectedUnitIds().Num(), 0);
    TestEqual(TEXT("All local operations leave snapshot unchanged"), S.ComputeDigest(), Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofTypeSelectionCapContextTest, "RedalertProof.TypeSelection.DeterministicCapAndContext", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofTypeSelectionCapContextTest::RunTest(const FString&)
{
    FProofSnapshot S; S.MatchEpoch = 1;
    TArray<int32> Visible;
    for (int32 Id = 80; Id > 0; --Id) { TypeSelectionAddRecord(S, Id, 1, EProofUnitKind::Combat); Visible.Add(Id); }
    Visible.Add(80); Visible.Add(1);
    FProofSelectionState Selection; Selection.SelectSingle(S, 1, 80); Selection.AssignGroup(9, S, 1);
    TestTrue(TEXT("Type selection succeeds above local cap"), Selection.SelectVisibleType(S, 1, Visible));
    TestEqual(TEXT("Existing selection cap retained"), Selection.GetSelectedUnitIds().Num(), FProofSelectionState::MaxSelectedUnits);
    TestEqual(TEXT("High-ID visible primary reserves a slot"), Selection.GetPrimaryUnitId(), 80);
    TestTrue(TEXT("Visible primary is actually selected"), Selection.GetSelectedUnitIds().Contains(80));
    for (int32 I = 0; I < FProofSelectionState::MaxSelectedUnits - 1; ++I)
        TestEqual(TEXT("Remaining slots take deterministic lowest IDs"), Selection.GetSelectedUnitIds()[I], I + 1);
    const TArray<int32> Expected = Selection.GetSelectedUnitIds();
    S.Units.Sort([](const FProofUnitRecord& A, const FProofUnitRecord& B) { return A.UnitId < B.UnitId; });
    Visible.Sort();
    FProofSelectionState Twin; Twin.SelectSingle(S, 1, 80); Twin.SelectVisibleType(S, 1, Visible);
    TestTrue(TEXT("Snapshot/candidate insertion order cannot change capped result"), Twin.GetSelectedUnitIds() == Expected);
    Selection.RecallGroup(9, S, 1);
    TestTrue(TEXT("T does not overwrite saved group with capped result"), Selection.GetSelectedUnitIds() == TArray<int32>({80}));
    S.MatchEpoch = 2;
    TestFalse(TEXT("T cannot carry anchor across epoch ID reuse"), Selection.SelectVisibleType(S, 1, Visible));
    TestEqual(TEXT("New epoch clears current selection"), Selection.GetSelectedUnitIds().Num(), 0);
    Selection.RecallGroup(9, S, 1);
    TestEqual(TEXT("Normal epoch rules still clear groups"), Selection.GetSelectedUnitIds().Num(), 0);
    Selection.SelectSingle(S, 1, 1); Selection.Toggle(S, 1, 2);
    for (FProofUnitRecord& U : S.Units) if (U.UnitId == 2) U.HitPoints = 0.f;
    TestFalse(TEXT("A newly dead primary cannot implicitly expand a fallback type"), Selection.SelectVisibleType(S, 1, Visible));
    TestTrue(TEXT("Normal pruning preserves other live selection"), Selection.GetSelectedUnitIds() == TArray<int32>({1}));
    TestFalse(TEXT("Owner change cannot reuse previous owner's anchor"), Selection.SelectVisibleType(S, 2, Visible));
    TestEqual(TEXT("Owner change leaves no foreign selection"), Selection.GetSelectedUnitIds().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofTypeSelectionViewportTest, "RedalertProof.TypeSelection.ViewportCenterBounds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofTypeSelectionViewportTest::RunTest(const FString&)
{
    const FIntPoint Viewport(1280, 720);
    TestTrue(TEXT("Top-left projected center is inside"), FProofSelectionState::IsViewportCenterVisible(FVector2D(0, 0), Viewport));
    TestTrue(TEXT("Last fractional in-bounds center is inside"), FProofSelectionState::IsViewportCenterVisible(FVector2D(1279.999, 719.999), Viewport));
    TestFalse(TEXT("Right boundary is outside"), FProofSelectionState::IsViewportCenterVisible(FVector2D(1280, 400), Viewport));
    TestFalse(TEXT("Bottom boundary is outside"), FProofSelectionState::IsViewportCenterVisible(FVector2D(400, 720), Viewport));
    TestFalse(TEXT("Negative X center is outside"), FProofSelectionState::IsViewportCenterVisible(FVector2D(-.001, 400), Viewport));
    TestFalse(TEXT("Negative Y center is outside"), FProofSelectionState::IsViewportCenterVisible(FVector2D(400, -.001), Viewport));
    TestFalse(TEXT("Missing viewport cannot select centers"), FProofSelectionState::IsViewportCenterVisible(FVector2D(0, 0), FIntPoint(0, 720)));
    TestFalse(TEXT("Invalid viewport cannot select centers"), FProofSelectionState::IsViewportCenterVisible(FVector2D(0, 0), FIntPoint(1280, -1)));
    TestFalse(TEXT("NaN projection excluded"), FProofSelectionState::IsViewportCenterVisible(FVector2D(std::numeric_limits<double>::quiet_NaN(), 0), Viewport));
    TestFalse(TEXT("Infinite projection excluded"), FProofSelectionState::IsViewportCenterVisible(FVector2D(0, std::numeric_limits<double>::infinity()), Viewport));
    TestTrue(TEXT("Smaller viewport uses its actual dimensions"), FProofSelectionState::IsViewportCenterVisible(FVector2D(959.99, 539.99), FIntPoint(960, 540)));
    TestFalse(TEXT("Smaller right edge remains excluded"), FProofSelectionState::IsViewportCenterVisible(FVector2D(960, 300), FIntPoint(960, 540)));
    return true;
}
#endif
