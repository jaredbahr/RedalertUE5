#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"

namespace
{
FProofCommand Command(int32 Sequence, EProofCommandType Type, int32 UnitId = INDEX_NONE,
    int32 TargetId = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand Result;
    Result.Sequence = Sequence;
    Result.Type = Type;
    Result.UnitId = UnitId;
    Result.TargetUnitId = TargetId;
    Result.Destination = Destination;
    return Result;
}

const FProofUnitRecord* UnitById(const FProofSnapshot& Snapshot, int32 Id)
{
    return Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; });
}

const FProofPlayerRecord* PlayerById(const FProofSnapshot& Snapshot, int32 Id)
{
    return Snapshot.Players.FindByPredicate([Id](const FProofPlayerRecord& Player) { return Player.OwnerId == Id; });
}

int32 FirstKind(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& Candidate)
        { return Candidate.OwnerId == Owner && Candidate.Kind == Kind && Candidate.HitPoints > 0.f; });
    return Unit ? Unit->UnitId : INDEX_NONE;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishPlacementTest, "RedalertProof.Skirmish.Economy.PlacementAndRoles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishPlacementTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.AddPlayer(1);
    TestFalse(TEXT("New commands remain opt-in"), Model.Submit(1, Command(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(250, 0, 0))).bAccepted);
    Model.StartSkirmish();
    TestTrue(TEXT("Skirmish starts playing"), Model.GetSnapshot().bSkirmish && Model.GetSnapshot().MatchPhase == EProofMatchPhase::Playing);
    TestTrue(TEXT("First team is seeded"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    TestTrue(TEXT("Cooperative owner is seeded"), Model.SeedSkirmishPlayer(2, 0, FVector(0, 1500, 0)));
    TestTrue(TEXT("Bot team is seeded"), Model.SeedSkirmishPlayer(100, 1, FVector(2000, 0, 0)));
    TestFalse(TEXT("Duplicate owner rejected"), Model.SeedSkirmishPlayer(1, 0, FVector(3000, 0, 0)));
    TestFalse(TEXT("Invalid team rejected"), Model.SeedSkirmishPlayer(3, 2, FVector(3000, 0, 0)));
    const FProofSnapshot Seeded = Model.GetSnapshot();
    TestEqual(TEXT("Opening credits"), PlayerById(Seeded, 1)->Credits, 350);
    TestEqual(TEXT("One finite ore patch per owner"), Seeded.Resources.Num(), 3);
    TestEqual(TEXT("HQ health"), UnitById(Seeded, FirstKind(Seeded, 1, EProofUnitKind::HQ))->HitPoints, 300.f);
    const int32 Harvester = FirstKind(Seeded, 1, EProofUnitKind::Harvester);
    const int32 HQ = FirstKind(Seeded, 1, EProofUnitKind::HQ);
    TestEqual(TEXT("Structure cannot move"), Model.Submit(1, Command(1, EProofCommandType::Move, HQ, INDEX_NONE, FVector(20, 0, 0))).Reason, FString(TEXT("role")));
    TestEqual(TEXT("Harvester cannot attack"), Model.Submit(1, Command(2, EProofCommandType::Attack, Harvester, FirstKind(Seeded, 100, EProofUnitKind::HQ))).Reason, FString(TEXT("role")));
    TestEqual(TEXT("No free production without factory"), Model.Submit(1, Command(3, EProofCommandType::Produce, HQ)).Reason, FString(TEXT("factory")));
    TestEqual(TEXT("No gathering without refinery"), Model.Submit(1, Command(4, EProofCommandType::Gather, Harvester, Seeded.Resources[0].ResourceId)).Reason, FString(TEXT("refinery")));
    TestEqual(TEXT("Too far from HQ"), Model.Submit(1, Command(5, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(900, 0, 0))).Reason, FString(TEXT("placement")));
    TestEqual(TEXT("Structure overlap"), Model.Submit(1, Command(6, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(100, 0, 0))).Reason, FString(TEXT("overlap")));
    TestTrue(TEXT("Refinery built"), Model.Submit(1, Command(7, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0))).bAccepted);
    TestTrue(TEXT("Factory built"), Model.Submit(1, Command(8, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(-300, 0, 0))).bAccepted);
    const FProofSnapshot Built = Model.GetSnapshot();
    TestEqual(TEXT("Both costs debited"), PlayerById(Built, 1)->Credits, 100);
    TestEqual(TEXT("No passive earnings"), PlayerById(Built, 1)->TotalGathered, 0);
    const int32 Factory = FirstKind(Built, 1, EProofUnitKind::Factory);
    TestEqual(TEXT("Cannot use teammate factory"), Model.Submit(2, Command(1, EProofCommandType::Produce, Factory)).Reason, FString(TEXT("factory")));
    TestTrue(TEXT("Own factory produces combat"), Model.Submit(1, Command(9, EProofCommandType::Produce, Factory)).bAccepted);
    const FProofSnapshot Produced = Model.GetSnapshot();
    const int32 Combat = FirstKind(Produced, 1, EProofUnitKind::Combat);
    TestEqual(TEXT("Production cost debited"), PlayerById(Produced, 1)->Credits, 50);
    TestTrue(TEXT("First combat uses clear deterministic factory offset"), UnitById(Produced, Combat)->Position.Equals(FVector(-600, 0, 0), 0.001));
    TestTrue(TEXT("Combat marker is separate from factory"), FVector::Dist(UnitById(Produced, Combat)->Position,
        UnitById(Produced, Factory)->Position) >= 260.0);
    TestEqual(TEXT("Cooperative team attack denied"), Model.Submit(1, Command(10, EProofCommandType::Attack, Combat, FirstKind(Produced, 2, EProofUnitKind::HQ))).Reason, FString(TEXT("team")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishHarvestTest, "RedalertProof.Skirmish.Economy.FiniteHarvestAndCapacity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishHarvestTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    TestTrue(TEXT("Seed worker"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    const FProofSnapshot Opening = Model.GetSnapshot();
    const int32 Harvester = FirstKind(Opening, 1, EProofUnitKind::Harvester);
    const int32 Ore = Opening.Resources[0].ResourceId;
    TestTrue(TEXT("Refinery built"), Model.Submit(1, Command(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0))).bAccepted);
    Model.Advance(50.f);
    TestEqual(TEXT("Time does not mint credits"), PlayerById(Model.GetSnapshot(), 1)->Credits, 250);
    TestTrue(TEXT("Gather ordered"), Model.Submit(1, Command(2, EProofCommandType::Gather, Harvester, Ore)).bAccepted);
    Model.Advance(1.f);
    TestEqual(TEXT("Travel alone leaves ore intact"), Model.GetSnapshot().Resources[0].Remaining, 500);
    Model.Advance(2.f);
    const FProofSnapshot Loaded = Model.GetSnapshot();
    TestEqual(TEXT("First arrival extracts a full load"), UnitById(Loaded, Harvester)->Cargo, 50);
    TestEqual(TEXT("Ore decreases by cargo"), Loaded.Resources[0].Remaining, 450);
    TestEqual(TEXT("Extraction itself earns nothing"), PlayerById(Loaded, 1)->Credits, 250);
    Model.Advance(2.f);
    const FProofSnapshot Deposited = Model.GetSnapshot();
    TestEqual(TEXT("Deposit empties cargo"), UnitById(Deposited, Harvester)->Cargo, 0);
    TestEqual(TEXT("Deposit earns credits"), PlayerById(Deposited, 1)->Credits, 300);
    TestEqual(TEXT("Earned counter records deposit"), PlayerById(Deposited, 1)->TotalGathered, 50);
    for (int32 Trip = 0; Trip < 9; ++Trip)
    {
        Model.Advance(2.f);
        Model.Advance(2.f);
    }
    const FProofSnapshot Exhausted = Model.GetSnapshot();
    TestEqual(TEXT("Ore is finite and nonnegative"), Exhausted.Resources[0].Remaining, 0);
    TestEqual(TEXT("All ore deposited exactly once"), PlayerById(Exhausted, 1)->TotalGathered, 500);
    TestEqual(TEXT("All deposits credited"), PlayerById(Exhausted, 1)->Credits, 750);
    Model.Advance(100.f);
    TestEqual(TEXT("Exhausted patch grants no income"), PlayerById(Model.GetSnapshot(), 1)->Credits, 750);
    TestEqual(TEXT("Depleted resource rejected"), Model.Submit(1, Command(3, EProofCommandType::Gather, Harvester, Ore)).Reason, FString(TEXT("resource")));
    TestTrue(TEXT("Factory built"), Model.Submit(1, Command(4, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(-300, 0, 0))).bAccepted);
    const int32 Factory = FirstKind(Model.GetSnapshot(), 1, EProofUnitKind::Factory);
    for (int32 Index = 0; Index < 8; ++Index)
    {
        TestTrue(TEXT("Produce up to live cap"), Model.Submit(1, Command(5 + Index, EProofCommandType::Produce, Factory)).bAccepted);
    }
    TestEqual(TEXT("Ninth combat unit is over cap"), Model.Submit(1, Command(13, EProofCommandType::Produce, Factory)).Reason, FString(TEXT("capacity")));
    const FProofSnapshot Capped = Model.GetSnapshot();
    TestEqual(TEXT("Failed production does not spend"), PlayerById(Capped, 1)->Credits, 200);
    TestEqual(TEXT("Live record cap"), Capped.Units.Num(), FProofCommandModel::SkirmishRecordCap);
    for (int32 A = 0; A < Capped.Units.Num(); ++A)
    {
        if (Capped.Units[A].Kind != EProofUnitKind::Combat) { continue; }
        for (int32 B = 0; B < Capped.Units.Num(); ++B)
        {
            if (A == B) { continue; }
            TestTrue(TEXT("Produced combat centers stay pickable and separate"),
                FVector::Dist(Capped.Units[A].Position, Capped.Units[B].Position) >= 260.0);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishInterruptedCargoTest, "RedalertProof.Skirmish.Economy.InterruptedFinalCargo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishInterruptedCargoTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    TestTrue(TEXT("Seed worker"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    const FProofSnapshot Opening = Model.GetSnapshot();
    const int32 Harvester = FirstKind(Opening, 1, EProofUnitKind::Harvester);
    const int32 Ore = Opening.Resources[0].ResourceId;
    TestTrue(TEXT("Build own refinery"), Model.Submit(1, Command(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0))).bAccepted);
    TestTrue(TEXT("Gather finite patch"), Model.Submit(1, Command(2, EProofCommandType::Gather, Harvester, Ore)).bAccepted);
    Model.Advance(3.f);
    Model.Advance(2.f);
    for (int32 Trip = 0; Trip < 8; ++Trip)
    {
        Model.Advance(2.f);
        Model.Advance(2.f);
    }
    Model.Advance(2.f);
    const FProofSnapshot FinalLoad = Model.GetSnapshot();
    TestEqual(TEXT("Final extraction exhausts patch"), FinalLoad.Resources[0].Remaining, 0);
    TestEqual(TEXT("Last load still needs deposit"), UnitById(FinalLoad, Harvester)->Cargo, 50);
    TestEqual(TEXT("Only delivered ore earned"), PlayerById(FinalLoad, 1)->TotalGathered, 450);
    TestTrue(TEXT("Move can interrupt return"), Model.Submit(1, Command(3, EProofCommandType::Move, Harvester, INDEX_NONE, FVector(550, 300, 0))).bAccepted);
    Model.Advance(2.f);
    TestEqual(TEXT("Move grants no deposit"), PlayerById(Model.GetSnapshot(), 1)->TotalGathered, 450);
    TestTrue(TEXT("Loaded harvester can resume from depleted patch"), Model.Submit(1, Command(4, EProofCommandType::Gather, Harvester, Ore)).bAccepted);
    TestTrue(TEXT("Repeated resume accepted without minting cargo"), Model.Submit(1, Command(5, EProofCommandType::Gather, Harvester, Ore)).bAccepted);
    Model.Advance(3.f);
    const FProofSnapshot Deposited = Model.GetSnapshot();
    TestEqual(TEXT("Resumed return empties final cargo"), UnitById(Deposited, Harvester)->Cargo, 0);
    TestEqual(TEXT("All extracted ore deposited once"), PlayerById(Deposited, 1)->TotalGathered, 500);
    TestEqual(TEXT("Exactly starting credits minus refinery plus finite ore"), PlayerById(Deposited, 1)->Credits, 750);
    TestEqual(TEXT("Empty harvester still cannot gather depleted patch"), Model.Submit(1, Command(6, EProofCommandType::Gather, Harvester, Ore)).Reason, FString(TEXT("resource")));
    Model.Advance(20.f);
    TestEqual(TEXT("No further income after final deposit"), PlayerById(Model.GetSnapshot(), 1)->Credits, 750);
    TestEqual(TEXT("Ore remains nonnegative"), Model.GetSnapshot().Resources[0].Remaining, 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishDeterminismTest, "RedalertProof.Skirmish.Economy.DeterminismAndTerminal", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishDeterminismTest::RunTest(const FString&)
{
    FProofCommandModel Left, Right;
    FProofCommandModel* Models[] = { &Left, &Right };
    for (FProofCommandModel* Model : Models)
    {
        Model->StartSkirmish();
        TestTrue(TEXT("Seed team 0"), Model->SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
        TestTrue(TEXT("Seed team 1"), Model->SeedSkirmishPlayer(100, 1, FVector(700, 0, 0)));
        TestTrue(TEXT("Build refinery"), Model->Submit(1, Command(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(-300, 0, 0))).bAccepted);
        TestTrue(TEXT("Build factory"), Model->Submit(1, Command(2, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(250, 0, 0))).bAccepted);
        const FProofSnapshot Prepared = Model->GetSnapshot();
        const int32 Factory = FirstKind(Prepared, 1, EProofUnitKind::Factory);
        const int32 Harvester = FirstKind(Prepared, 1, EProofUnitKind::Harvester);
        TestTrue(TEXT("Gather command"), Model->Submit(1, Command(3, EProofCommandType::Gather, Harvester, Prepared.Resources[0].ResourceId)).bAccepted);
        TestTrue(TEXT("Produce command"), Model->Submit(1, Command(4, EProofCommandType::Produce, Factory)).bAccepted);
        Model->Advance(4.f);
    }
    TestEqual(TEXT("Identical command traces have identical digest"), Left.GetSnapshot().ComputeDigest(), Right.GetSnapshot().ComputeDigest());
    FProofSnapshot Altered = Left.GetSnapshot();
    ++Altered.Players[0].TotalGathered;
    TestNotEqual(TEXT("Earned credits affect digest"), Left.GetSnapshot().ComputeDigest(), Altered.ComputeDigest());
    Altered = Left.GetSnapshot();
    ++Altered.Resources[0].Remaining;
    TestNotEqual(TEXT("Ore quantity affects digest"), Left.GetSnapshot().ComputeDigest(), Altered.ComputeDigest());
    const FProofSnapshot Ready = Left.GetSnapshot();
    const int32 Combat = FirstKind(Ready, 1, EProofUnitKind::Combat);
    const int32 EnemyHQ = FirstKind(Ready, 100, EProofUnitKind::HQ);
    TestTrue(TEXT("Combat can attack opposing HQ"), Left.Submit(1, Command(5, EProofCommandType::Attack, Combat, EnemyHQ)).bAccepted);
    Left.Advance(12.f);
    const FProofSnapshot Won = Left.GetSnapshot();
    TestEqual(TEXT("Enemy HQ destroyed"), UnitById(Won, EnemyHQ)->HitPoints, 0.f);
    TestTrue(TEXT("Terminal state recorded"), Won.MatchPhase == EProofMatchPhase::Finished);
    TestEqual(TEXT("Winning team recorded"), Won.WinnerTeam, 0);
    const FString Frozen = Won.ComputeDigest();
    Left.Advance(100.f);
    TestEqual(TEXT("Terminal advance freezes state"), Left.GetSnapshot().ComputeDigest(), Frozen);
    TestEqual(TEXT("Terminal command rejected"), Left.Submit(1, Command(6, EProofCommandType::Move, Combat, INDEX_NONE, FVector(0, 0, 0))).Reason, FString(TEXT("finished")));
    TestEqual(TEXT("Terminal rejection leaves state frozen"), Left.GetSnapshot().ComputeDigest(), Frozen);

    FProofCommandModel Draw;
    Draw.StartSkirmish();
    TestTrue(TEXT("Draw seed team 0"), Draw.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    TestTrue(TEXT("Draw seed team 1"), Draw.SeedSkirmishPlayer(100, 1, FVector(700, 0, 0)));
    TestTrue(TEXT("Team 0 factory"), Draw.Submit(1, Command(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(250, 0, 0))).bAccepted);
    TestTrue(TEXT("Team 1 factory"), Draw.Submit(100, Command(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(950, 0, 0))).bAccepted);
    TestTrue(TEXT("Team 0 combat"), Draw.Submit(1, Command(2, EProofCommandType::Produce, FirstKind(Draw.GetSnapshot(), 1, EProofUnitKind::Factory))).bAccepted);
    TestTrue(TEXT("Team 1 combat"), Draw.Submit(100, Command(2, EProofCommandType::Produce, FirstKind(Draw.GetSnapshot(), 100, EProofUnitKind::Factory))).bAccepted);
    const FProofSnapshot BothArmed = Draw.GetSnapshot();
    TestTrue(TEXT("Team 0 attacks HQ"), Draw.Submit(1, Command(3, EProofCommandType::Attack,
        FirstKind(BothArmed, 1, EProofUnitKind::Combat), FirstKind(BothArmed, 100, EProofUnitKind::HQ))).bAccepted);
    TestTrue(TEXT("Team 1 attacks HQ"), Draw.Submit(100, Command(3, EProofCommandType::Attack,
        FirstKind(BothArmed, 100, EProofUnitKind::Combat), FirstKind(BothArmed, 1, EProofUnitKind::HQ))).bAccepted);
    Draw.Advance(12.f);
    const FProofSnapshot Drawn = Draw.GetSnapshot();
    TestTrue(TEXT("Mutual HQ loss finishes game"), Drawn.MatchPhase == EProofMatchPhase::Finished);
    TestEqual(TEXT("Mutual HQ loss is a draw"), Drawn.WinnerTeam, INDEX_NONE);
    TestEqual(TEXT("Both HQs take simultaneous lethal damage"), UnitById(Drawn, FirstKind(BothArmed, 1, EProofUnitKind::HQ))->HitPoints, 0.f);
    TestEqual(TEXT("Other HQ also destroyed"), UnitById(Drawn, FirstKind(BothArmed, 100, EProofUnitKind::HQ))->HitPoints, 0.f);
    return true;
}
#endif
