#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofSelectionState.h"
#include "Presentation/ProofTypeSelectGesture.h"
#include <limits>
namespace
{
FProofSnapshot MapTypeFixture()
{
    FProofSnapshot S; S.MatchEpoch=1;S.bSkirmish=true;S.MatchPhase=EProofMatchPhase::Playing;
    for(int32 Owner:{1,2}){FProofPlayerRecord P;P.OwnerId=Owner;P.TeamId=0;S.Players.Add(P);}
    for(int32 Id:{10,20,30,40,50,60,70}){FProofUnitRecord U;U.UnitId=Id;U.OwnerId=Id==50?2:1;U.Kind=Id==40?EProofUnitKind::Harvester:EProofUnitKind::Combat;U.HitPoints=Id==60?0.f:100.f;U.Position=FVector(Id*50,0,0);if(Id==70)U.Position.X=std::numeric_limits<double>::infinity();S.Units.Add(U);}
    return S;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMapTypeSelectionTest,"RedalertProof.MapTypeSelection.OwnershipAnchorAndGroups",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProofMapTypeSelectionTest::RunTest(const FString&)
{
    const FProofSnapshot S=MapTypeFixture();const FString Digest=S.ComputeDigest();FProofSelectionState Selection;
    Selection.SelectSingle(S,1,30);Selection.AssignGroup(1,S,1);
    TestTrue(TEXT("First visible pass unchanged"),Selection.SelectVisibleType(S,1,{10,20}));
    TestTrue(TEXT("First pass excludes offscreen anchor"),Selection.GetSelectedUnitIds()==TArray<int32>({10,20}));
    TestTrue(TEXT("Second pass uses original valid anchor"),Selection.SelectBattlefieldType(S,1,30));
    TestTrue(TEXT("Map pass includes offscreen owned live kind, excludes foreign/dead/nonfinite/different kind"),Selection.GetSelectedUnitIds()==TArray<int32>({10,20,30}));
    TestEqual(TEXT("Original offscreen anchor restored as primary"),Selection.GetPrimaryUnitId(),30);
    Selection.RecallGroup(1,S,1);TestTrue(TEXT("Saved group remains independent"),Selection.GetSelectedUnitIds()==TArray<int32>({30}));
    Selection.SelectVisibleType(S,1,{});TestTrue(TEXT("Internal first-pass empty set can expand from original anchor"),Selection.SelectBattlefieldType(S,1,30));
    TestEqual(TEXT("No authoritative mutation"),S.ComputeDigest(),Digest);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMapTypeContextTest,"RedalertProof.MapTypeSelection.ContextAndRevision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProofMapTypeContextTest::RunTest(const FString&)
{
    const FProofSnapshot S=MapTypeFixture();FProofSelectionState Selection;Selection.SelectSingle(S,1,30);
    const uint64 Revision=Selection.GetSelectionRevision();Selection.Synchronize(S,1);TestEqual(TEXT("Stable tick does not change local selection revision"),Selection.GetSelectionRevision(),Revision);
    Selection.Toggle(S,1,20);TestTrue(TEXT("External selection change invalidates context token"),Selection.GetSelectionRevision()>Revision);
    for(int32 Variant=0;Variant<8;++Variant){FProofSnapshot Changed=S;FProofSelectionState Probe=Selection;if(Variant==0)Changed.MatchEpoch++;if(Variant==1)Changed.MatchPhase=EProofMatchPhase::Finished;if(Variant==2)Changed.Players.RemoveAt(0);if(Variant==3)Changed.Units.RemoveAll([](const FProofUnitRecord& U){return U.UnitId==30;});if(Variant==4)Changed.Units[2].HitPoints=0;if(Variant==5)Changed.Units[2].OwnerId=2;if(Variant==6)Changed.Units[2].Position.X=std::numeric_limits<double>::quiet_NaN();if(Variant==7)Changed.MatchEpoch=0;TestFalse(TEXT("Invalid source/context fails closed"),Probe.SelectBattlefieldType(Changed,1,30));if(Variant==0 || Variant==7)TestEqual(TEXT("Epoch context rejection clears selection"),Probe.GetSelectedUnitIds().Num(),0);}
    TestFalse(TEXT("Owner change cannot carry anchor"),Selection.SelectBattlefieldType(S,2,30));
    TestEqual(TEXT("Owner context rejection clears selection"),Selection.GetSelectedUnitIds().Num(),0);
    TestFalse(TEXT("Nonpositive source fails"),Selection.SelectBattlefieldType(S,1,0));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofMapTypeCapTest,"RedalertProof.MapTypeSelection.DeterministicCap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProofMapTypeCapTest::RunTest(const FString&)
{
    FProofSnapshot S=MapTypeFixture();S.Units.Reset();for(int32 Id=90;Id>0;--Id){FProofUnitRecord U;U.UnitId=Id;U.OwnerId=1;U.Kind=EProofUnitKind::Combat;S.Units.Add(U);}
    FProofSelectionState Selection;Selection.SelectSingle(S,1,90);TestTrue(TEXT("Map type over cap succeeds"),Selection.SelectBattlefieldType(S,1,90));
    TestEqual(TEXT("Existing cap retained"),Selection.GetSelectedUnitIds().Num(),FProofSelectionState::MaxSelectedUnits);TestEqual(TEXT("High ID source primary retained"),Selection.GetPrimaryUnitId(),90);
    for(int32 I=0;I<FProofSelectionState::MaxSelectedUnits-1;++I)TestEqual(TEXT("Lowest other IDs deterministic"),Selection.GetSelectedUnitIds()[I],I+1);
    const TArray<int32> Expected=Selection.GetSelectedUnitIds();S.Units.Sort([](const FProofUnitRecord& A,const FProofUnitRecord& B){return A.UnitId<B.UnitId;});FProofSelectionState Twin;Twin.SelectSingle(S,1,90);Twin.SelectBattlefieldType(S,1,90);TestTrue(TEXT("Array order independent"),Twin.GetSelectedUnitIds()==Expected);return true;
}
#endif
