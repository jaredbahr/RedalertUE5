#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Presentation/ProofSelectionState.h"
#include "Presentation/ProofTypeSelectionAdapter.h"
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
    TestFalse(TEXT("Nonpositive source fails"),Selection.SelectBattlefieldType(S,1,0));

    // Invoke the same production adapter used by Pawn, with real selection
    // actions. Visible candidate/failure inputs are controlled here; this is
    // not a claim of engine viewport projection or OS keyboard/focus delivery.
    using Scope=ProofTypeSelectGesture::Scope;
    for(int32 Variant=0;Variant<3;++Variant)
    {
        FProofSnapshot Changed=S;
        FProofUnitRecord Extra;Extra.UnitId=80;Extra.OwnerId=1;Extra.Kind=EProofUnitKind::Harvester;Changed.Units.Add(Extra);
        FProofSelectionState Local;Local.SelectSingle(Changed,1,40);Local.Toggle(Changed,1,30);
        if(Variant==0)Changed.Units[2].HitPoints=0;
        if(Variant==1)Changed.Units.RemoveAll([](const FProofUnitRecord& Unit){return Unit.UnitId==30;});
        if(Variant==2)Changed.Units[2].OwnerId=2;
        const FString Before=Changed.ComputeDigest();int32 Calls=0;ProofTypeSelectGesture::State Gesture;
        const Scope Action=ProofTypeSelectionAdapter::Apply(Local,Gesture,Changed,1,1.0,true,[&](){++Calls;return Local.SelectVisibleType(Changed,1,{40,80});});
        TestTrue(TEXT("Shared adapter rejects dead/removed/foreign pre-sync primary"),Action==Scope::Ignored);
        TestEqual(TEXT("No fallback-kind visible action was invoked"),Calls,0);
        TestTrue(TEXT("Normal pruning preserves the secondary without expanding its kind"),Local.GetSelectedUnitIds()==TArray<int32>({40}));
        TestFalse(TEXT("Stale primary cannot arm a new pair"),Gesture.IsArmed());
        TestEqual(TEXT("Rejected adapter leaves snapshot unchanged"),Changed.ComputeDigest(),Before);
    }
    for(int32 Empty=0;Empty<2;++Empty)
    {
        FProofSelectionState Local;Local.SelectSingle(S,1,30);ProofTypeSelectGesture::State Gesture;int32 Calls=0;
        const TArray<int32> Visible=Empty?TArray<int32>():TArray<int32>({10,20});
        const auto VisibleAction=[&](){++Calls;return Local.SelectVisibleType(S,1,Visible);};
        TestTrue(TEXT("Actual shared adapter first press remains visible-only"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.0,true,VisibleAction)==Scope::Visible);
        TestTrue(TEXT("First action uses exact controlled visible set"),Local.GetSelectedUnitIds()==Visible);
        TestTrue(TEXT("Successful empty/offscreen internal transition arms original source"),Gesture.IsArmed() && Gesture.Expected().Anchor==30);
        Gesture.Release();
        TestTrue(TEXT("Actual shared adapter expands from retained original anchor"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.1,true,VisibleAction)==Scope::Battlefield);
        TestTrue(TEXT("Battlefield result remains exact owned live kind"),Local.GetSelectedUnitIds()==TArray<int32>({10,20,30}));
        TestEqual(TEXT("Offscreen source restored as primary"),Local.GetPrimaryUnitId(),30);
        TestEqual(TEXT("Second map action does not invoke another visible action"),Calls,1);
    }
    for(int32 Variant=0;Variant<3;++Variant)
    {
        FProofSnapshot Changed=S;FProofSelectionState Local;Local.SelectSingle(S,1,30);ProofTypeSelectGesture::State Gesture;int32 Calls=0;
        const auto VisibleAction=[&](){++Calls;return Local.SelectVisibleType(Changed,1,{10,20,40});};
        ProofTypeSelectionAdapter::Apply(Local,Gesture,Changed,1,1.0,true,VisibleAction);Gesture.Release();
        if(Variant==0)Changed.Units[2].HitPoints=0;
        if(Variant==1)Changed.Units.RemoveAll([](const FProofUnitRecord& Unit){return Unit.UnitId==30;});
        if(Variant==2)Changed.Units[2].OwnerId=2;
        TestTrue(TEXT("Armed original source invalidation cannot silently restart from fallback"),ProofTypeSelectionAdapter::Apply(Local,Gesture,Changed,1,1.1,true,VisibleAction)==Scope::Ignored);
        TestEqual(TEXT("Source invalidation never invokes a second visible action"),Calls,1);
        TestFalse(TEXT("Source invalidation cancels pending pair"),Gesture.IsArmed());
    }
    {
        FProofSelectionState Local;Local.SelectSingle(S,1,30);ProofTypeSelectGesture::State Gesture;
        const auto VisibleAction=[&](){return Local.SelectVisibleType(S,1,{10,20,40});};
        ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.0,true,VisibleAction);Gesture.Release();
        Local.SelectSingle(S,1,40);
        TestTrue(TEXT("External selection starts a fresh visible pair, not prior map expansion"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.1,true,VisibleAction)==Scope::Visible);
        TestTrue(TEXT("Fresh pair uses newly selected kind/source"),Local.GetSelectedUnitIds()==TArray<int32>({40}) && Gesture.Expected().Anchor==40);
    }
    for(int32 Variant=0;Variant<3;++Variant)
    {
        FProofSnapshot Changed=S;FProofSelectionState Local;Local.SelectSingle(S,1,30);ProofTypeSelectGesture::State Gesture;int32 Calls=0;
        const auto VisibleAction=[&](){++Calls;return Local.SelectVisibleType(Changed,1,{10,20,40});};
        TestTrue(TEXT("Original combat pair arms before explicit replacement"),ProofTypeSelectionAdapter::Apply(Local,Gesture,Changed,1,1.0,true,VisibleAction)==Scope::Visible);
        Gesture.Release();Local.SelectSingle(Changed,1,40);
        if(Variant==0)Changed.Units[2].HitPoints=0;
        if(Variant==1)Changed.Units.RemoveAll([](const FProofUnitRecord& Unit){return Unit.UnitId==30;});
        if(Variant==2)Changed.Units[2].OwnerId=2;
        const FString Before=Changed.ComputeDigest();
        TestTrue(TEXT("Explicit new selection retires dead/removed/foreign old anchor before validation"),ProofTypeSelectionAdapter::Apply(Local,Gesture,Changed,1,1.1,true,VisibleAction)==Scope::Visible);
        TestTrue(TEXT("Fresh visible pair uses only newly selected harvester kind"),Local.GetSelectedUnitIds()==TArray<int32>({40}) && Gesture.IsArmed() && Gesture.Expected().Anchor==40);
        TestEqual(TEXT("Explicit replacement invokes its own visible action"),Calls,2);
        Gesture.Release();
        TestTrue(TEXT("Replacement pair completes independently of invalid old anchor"),ProofTypeSelectionAdapter::Apply(Local,Gesture,Changed,1,1.2,true,VisibleAction)==Scope::Battlefield);
        TestTrue(TEXT("Replacement battlefield selection stays exact"),Local.GetSelectedUnitIds()==TArray<int32>({40}));
        TestEqual(TEXT("Replacement map action does not call visible callback again"),Calls,2);
        TestEqual(TEXT("Replacement adapter does not mutate snapshot"),Changed.ComputeDigest(),Before);
    }
    {
        FProofSelectionState Local;Local.SelectSingle(S,1,30);ProofTypeSelectGesture::State Gesture;int32 Calls=0;
        const auto VisibleAction=[&](){++Calls;return Local.SelectVisibleType(S,1,{10,20});};
        ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.0,true,VisibleAction);
        TestTrue(TEXT("Held duplicate press is ignored by actual adapter"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.05,true,VisibleAction)==Scope::Ignored);
        TestTrue(TEXT("Focus loss while held cancels pair"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.1,false,VisibleAction)==Scope::Ignored && !Gesture.IsArmed());
        Gesture.Release(); // Logical release outside the viewport, not an OS event.
        TestTrue(TEXT("Focus recovery starts visible first"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.2,true,VisibleAction)==Scope::Visible);
        Gesture.Release();
        TestTrue(TEXT("Fresh recovered pair may expand"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.3,true,VisibleAction)==Scope::Battlefield);
        TestEqual(TEXT("Ignored held/focus actions never call visible action"),Calls,2);
    }
    {
        FProofSelectionState Local;Local.SelectSingle(S,1,30);ProofTypeSelectGesture::State Gesture;int32 Calls=0;
        const auto VisibleAction=[&](){++Calls;return Local.SelectVisibleType(S,1,{10,20});};
        TestTrue(TEXT("Controlled projection/viewport callback failure rejects action"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.0,true,[&](){++Calls;return false;})==Scope::Ignored);
        TestFalse(TEXT("Failed visible action cannot arm pending expansion"),Gesture.IsArmed());Gesture.Release();
        TestTrue(TEXT("Recovery after failed callback is a fresh visible action"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.1,true,VisibleAction)==Scope::Visible);
        Gesture.Release();
        TestTrue(TEXT("Recovered released pair completes battlefield expansion"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.2,true,VisibleAction)==Scope::Battlefield);
        TestTrue(TEXT("Recovered pair has exact owned live battlefield IDs"),Local.GetSelectedUnitIds()==TArray<int32>({10,20,30}));
        TestEqual(TEXT("Recovered map second press bypasses visible callback"),Calls,2);
        Gesture.Release();
        TestTrue(TEXT("A later first press arms a separate fresh pair"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,1.3,true,VisibleAction)==Scope::Visible && Gesture.IsArmed());
        Gesture.Release();
        TestTrue(TEXT("Invalid clock cancels that separate adapter pair"),ProofTypeSelectionAdapter::Apply(Local,Gesture,S,1,std::numeric_limits<double>::quiet_NaN(),true,VisibleAction)==Scope::Ignored && !Gesture.IsArmed());
        TestEqual(TEXT("Invalid clock never invokes visible callback"),Calls,3);
    }
    return true;
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
