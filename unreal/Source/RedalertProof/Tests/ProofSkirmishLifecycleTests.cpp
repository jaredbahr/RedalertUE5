#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"

namespace
{
FProofCommand LifecycleCommand(int32 Sequence, EProofCommandType Type, int32 UnitId = INDEX_NONE,
    int32 TargetId = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand Command;
    Command.Sequence = Sequence;
    Command.Type = Type;
    Command.UnitId = UnitId;
    Command.TargetUnitId = TargetId;
    Command.Destination = Destination;
    return Command;
}

int32 LifecycleUnit(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& Candidate)
        { return Candidate.OwnerId == Owner && Candidate.Kind == Kind; });
    return Unit ? Unit->UnitId : INDEX_NONE;
}

void SeedLifecycleMatch(FProofCommandModel& Model)
{
    Model.StartSkirmish();
    Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Model.SeedSkirmishPlayer(100, 1, FVector(700, 0, 0));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishLifecycleTest, "RedalertProof.Skirmish.Lifecycle.RestartAfterOutcomes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishLifecycleTest::RunTest(const FString&)
{
    // Exercise three complete matches and restarts on the same model instance.
    // Winner 0 is a human victory, winner 1 a defeat, and INDEX_NONE a draw.
    FProofCommandModel Model;
    const int32 ExpectedWinners[] = { 0, 1, INDEX_NONE };
    for (const int32 ExpectedWinner : ExpectedWinners)
    {
        SeedLifecycleMatch(Model);
        const FProofSnapshot Opening = Model.GetSnapshot();
        for (const int32 Owner : { 1, 100 })
        {
            const FVector HQ = Owner == 1 ? FVector::ZeroVector : FVector(700, 0, 0);
            TestTrue(TEXT("Prepare refinery through commands"), Model.Submit(Owner, LifecycleCommand(1,
                EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, HQ + FVector(0, -300, 0))).bAccepted);
            TestTrue(TEXT("Prepare factory through commands"), Model.Submit(Owner, LifecycleCommand(2,
                EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, HQ + FVector(250, 0, 0))).bAccepted);
            const FProofSnapshot Built = Model.GetSnapshot();
            const FProofResourceRecord* Ore = Opening.Resources.FindByPredicate([HQ](const FProofResourceRecord& Resource)
                { return Resource.Position.Equals(HQ + FVector(550, 0, 0)); });
            TestNotNull(TEXT("Owner ore exists"), Ore);
            if (!Ore) return false;
            TestTrue(TEXT("Queue harvest through commands"), Model.Submit(Owner, LifecycleCommand(3,
                EProofCommandType::Gather, LifecycleUnit(Built, Owner, EProofUnitKind::Harvester), Ore->ResourceId)).bAccepted);
            TestTrue(TEXT("Produce through commands"), Model.Submit(Owner, LifecycleCommand(4,
                EProofCommandType::Produce, LifecycleUnit(Built, Owner, EProofUnitKind::Factory))).bAccepted);
        }
        Model.Advance(4.f);
        const FProofSnapshot Armed = Model.GetSnapshot();
        TestEqual(TEXT("Preparation spends starting credits"), Armed.Players[0].Credits, 50);
        TestTrue(TEXT("Preparation mutates finite ore"), Armed.Resources[0].Remaining < 500);
        for (const int32 Owner : { 1, 100 })
        {
            const bool bAttacks = ExpectedWinner == INDEX_NONE || ExpectedWinner == (Owner == 1 ? 0 : 1);
            if (!bAttacks) continue;
            TestTrue(TEXT("Attack through commands"), Model.Submit(Owner, LifecycleCommand(5,
                EProofCommandType::Attack, LifecycleUnit(Armed, Owner, EProofUnitKind::Combat),
                LifecycleUnit(Armed, Owner == 1 ? 100 : 1, EProofUnitKind::HQ))).bAccepted);
        }
        Model.Advance(12.f);
        const FProofSnapshot Finished = Model.GetSnapshot();
        TestTrue(TEXT("Match reaches terminal phase"), Finished.MatchPhase == EProofMatchPhase::Finished);
        TestEqual(TEXT("Expected outcome recorded"), Finished.WinnerTeam, ExpectedWinner);
        const FString FrozenDigest = Finished.ComputeDigest();
        for (const int32 Owner : { 1, 100 })
        {
            TestEqual(TEXT("Terminal production rejected"), Model.Submit(Owner, LifecycleCommand(6,
                EProofCommandType::Produce, LifecycleUnit(Finished, Owner, EProofUnitKind::Factory))).Reason, FString(TEXT("finished")));
        }
        TestFalse(TEXT("Terminal seed rejected"), Model.SeedSkirmishPlayer(2, 0, FVector(0, 1500, 0)));
        Model.Advance(100.f);
        TestEqual(TEXT("Terminal requests and advance preserve full snapshot"), Model.GetSnapshot().ComputeDigest(), FrozenDigest);

        SeedLifecycleMatch(Model);
        FProofCommandModel Fresh;
        SeedLifecycleMatch(Fresh);
        const FProofSnapshot Restarted = Model.GetSnapshot();
        TestEqual(TEXT("Restart equals fresh match including IDs and revision"), Restarted.ComputeDigest(), Fresh.GetSnapshot().ComputeDigest());
        TestEqual(TEXT("Restart clears extra unit records"), Restarted.Units.Num(), 4);
        TestEqual(TEXT("Restart resets unit IDs"), Restarted.Units[0].UnitId, 1);
        TestEqual(TEXT("Restart resets resource IDs"), Restarted.Resources[0].ResourceId, 1);
        TestEqual(TEXT("Restart restores starting credits"), Restarted.Players[0].Credits, 350);
        TestEqual(TEXT("Restart clears deposited income"), Restarted.Players[0].TotalGathered, 0);
        TestEqual(TEXT("Restart restores finite ore"), Restarted.Resources[0].Remaining, 500);
        for (const FProofUnitRecord& Unit : Restarted.Units)
            TestEqual(TEXT("Restart clears cargo"), Unit.Cargo, 0);
        Model.Advance(1.f);
        Fresh.Advance(1.f);
        TestEqual(TEXT("Restart has no inherited harvest or attack orders"), Model.GetSnapshot().ComputeDigest(), Fresh.GetSnapshot().ComputeDigest());
        const FProofCommand First = LifecycleCommand(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(0, -300, 0));
        TestTrue(TEXT("Restart admits sequence one"), Model.Submit(1, First).bAccepted);
        TestEqual(TEXT("Restart still rejects duplicate sequence"), Model.Submit(1, First).Reason, FString(TEXT("sequence")));
        Model.Reset();
        const FProofSnapshot Empty = Model.GetSnapshot();
        TestFalse(TEXT("Explicit reset returns legacy mode"), Empty.bSkirmish);
        TestTrue(TEXT("Explicit reset clears terminal phase"), Empty.MatchPhase == EProofMatchPhase::Inactive);
        TestEqual(TEXT("Explicit reset clears winner"), Empty.WinnerTeam, INDEX_NONE);
        TestEqual(TEXT("Explicit reset clears players"), Empty.Players.Num(), 0);
        TestEqual(TEXT("Explicit reset clears units"), Empty.Units.Num(), 0);
        TestEqual(TEXT("Explicit reset clears resources"), Empty.Resources.Num(), 0);
        TestEqual(TEXT("Explicit reset clears revision"), Empty.Revision, 0);
        TestEqual(TEXT("Explicit reset forgets old owners"), Model.Submit(1, First).Reason, FString(TEXT("unknown-player")));
    }
    FProofCommandModel Limited(1);
    SeedLifecycleMatch(Limited);
    const FProofCommand FirstBuild = LifecycleCommand(1, EProofCommandType::BuildRefinery,
        INDEX_NONE, INDEX_NONE, FVector(0, -300, 0));
    TestTrue(TEXT("Use entire per-advance allowance"), Limited.Submit(1, FirstBuild).bAccepted);
    TestEqual(TEXT("Allowance is exhausted before restart"), Limited.Submit(1, LifecycleCommand(2,
        EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(250, 0, 0))).Reason, FString(TEXT("rate")));
    SeedLifecycleMatch(Limited);
    TestTrue(TEXT("Restart also restores per-owner allowance"), Limited.Submit(1, FirstBuild).bAccepted);
    return true;
}
#endif

