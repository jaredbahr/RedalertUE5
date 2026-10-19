#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"
#include <limits>

// Boundary setup only: normal travel, extraction, orders, credit conversion and
// combat still run through the production model. No production state setters.
struct FProofReturnCargoFixture
{
    static FProofUnitRecord& Unit(FProofCommandModel& Model, int32 Id)
    {
        return *Model.State.Units.FindByPredicate([Id](const FProofUnitRecord& Item) { return Item.UnitId == Id; });
    }
    static FProofPlayerRecord& Player(FProofCommandModel& Model, int32 Owner)
    {
        return *Model.State.Players.FindByPredicate([Owner](const FProofPlayerRecord& Item) { return Item.OwnerId == Owner; });
    }
    static FProofResourceRecord& Resource(FProofCommandModel& Model, int32 Id)
    {
        return *Model.State.Resources.FindByPredicate([Id](const FProofResourceRecord& Item) { return Item.ResourceId == Id; });
    }
    static void RemoveUnit(FProofCommandModel& Model, int32 Id)
    {
        Model.State.Units.RemoveAll([Id](const FProofUnitRecord& Item) { return Item.UnitId == Id; });
    }
    static void ForgetOrder(FProofCommandModel& Model, int32 Id) { Model.Orders.Remove(Id); }
    static bool HasOrder(const FProofCommandModel& Model, int32 Id) { return Model.Orders.Contains(Id); }
    static bool IsOrder(const FProofCommandModel& Model, int32 Id, EProofCommandType Type, int32 Target, int32 Resume = INDEX_NONE)
    {
        const FProofCommandModel::FOrder* Order = Model.Orders.Find(Id);
        return Order && Order->Type == Type && Order->TargetUnitId == Target && Order->ResumeResourceId == Resume;
    }
};

namespace
{
FProofCommand ReturnCommand(int32 Sequence, EProofCommandType Type, int32 UnitId = INDEX_NONE,
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

int32 ReturnFirstKind(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* Found = Snapshot.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& Unit)
        { return Unit.OwnerId == Owner && Unit.Kind == Kind && Unit.HitPoints > 0.f; });
    return Found ? Found->UnitId : INDEX_NONE;
}

bool ReturnSnapshotsEqual(const FProofSnapshot& A, const FProofSnapshot& B)
{
    if (A.Revision != B.Revision || A.MatchEpoch != B.MatchEpoch || A.bSkirmish != B.bSkirmish
        || A.MatchPhase != B.MatchPhase || A.WinnerTeam != B.WinnerTeam
        || A.Players.Num() != B.Players.Num() || A.Units.Num() != B.Units.Num()
        || A.Resources.Num() != B.Resources.Num()) { return false; }
    for (int32 Index = 0; Index < A.Players.Num(); ++Index)
    {
        const FProofPlayerRecord& Left = A.Players[Index];
        const FProofPlayerRecord& Right = B.Players[Index];
        if (Left.OwnerId != Right.OwnerId || Left.Credits != Right.Credits
            || Left.TeamId != Right.TeamId || Left.TotalGathered != Right.TotalGathered) { return false; }
    }
    for (int32 Index = 0; Index < A.Units.Num(); ++Index)
    {
        const FProofUnitRecord& Left = A.Units[Index];
        const FProofUnitRecord& Right = B.Units[Index];
        if (Left.UnitId != Right.UnitId || Left.OwnerId != Right.OwnerId || Left.Position != Right.Position
            || Left.HitPoints != Right.HitPoints || Left.Kind != Right.Kind || Left.Cargo != Right.Cargo
            || Left.bHasRallyPoint != Right.bHasRallyPoint || Left.RallyPoint != Right.RallyPoint) { return false; }
    }
    for (int32 Index = 0; Index < A.Resources.Num(); ++Index)
    {
        const FProofResourceRecord& Left = A.Resources[Index];
        const FProofResourceRecord& Right = B.Resources[Index];
        if (Left.ResourceId != Right.ResourceId || Left.OwnerId != Right.OwnerId
            || Left.Position != Right.Position || Left.Remaining != Right.Remaining) { return false; }
    }
    return true;
}

struct FReturnSetup
{
    FProofCommandModel Model;
    int32 Sequence = 0;
    int32 Harvester = INDEX_NONE;
    int32 HQ = INDEX_NONE;
    int32 NearRefinery = INDEX_NONE;
    int32 FarRefinery = INDEX_NONE;
    int32 Ore = INDEX_NONE;

    explicit FReturnSetup(int32 CommandLimit = 32) : Model(CommandLimit) {}
    FProofCommandResult Submit(EProofCommandType Type, int32 Unit = INDEX_NONE,
        int32 Target = INDEX_NONE, FVector Destination = FVector::ZeroVector)
    {
        return Model.Submit(1, ReturnCommand(++Sequence, Type, Unit, Target, Destination));
    }
    bool Initialize(int32 OreQuantity = 500)
    {
        Model.StartSkirmish();
        if (!Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector)) { return false; }
        const FProofSnapshot Opening = Model.GetSnapshot();
        Harvester = ReturnFirstKind(Opening, 1, EProofUnitKind::Harvester);
        HQ = ReturnFirstKind(Opening, 1, EProofUnitKind::HQ);
        Ore = Opening.Resources[0].ResourceId;
        FProofReturnCargoFixture::Resource(Model, Ore).Remaining = OreQuantity;
        if (!Submit(EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0)).bAccepted) { return false; }
        // ReturnCargo cases measure only the explicitly commanded starter miner.
        // Reset only command allowance so the one-command rate fixture can Stop too.
        Model.Advance(0.f);
        if (!Submit(EProofCommandType::Stop, Model.GetSnapshot().Units.Last().UnitId).bAccepted) return false;
        NearRefinery = ReturnFirstKind(Model.GetSnapshot(), 1, EProofUnitKind::Refinery);
        Model.Advance(0.f);
        if (!Submit(EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(-300, 0, 0)).bAccepted) { return false; }
        // ReturnCargo cases measure only the explicitly commanded starter miner.
        // Reset only command allowance so the one-command rate fixture can Stop too.
        Model.Advance(0.f);
        if (!Submit(EProofCommandType::Stop, Model.GetSnapshot().Units.Last().UnitId).bAccepted) return false;
        const FProofSnapshot Built = Model.GetSnapshot();
        const FProofUnitRecord* Far = Built.Units.FindByPredicate([this](const FProofUnitRecord& Unit)
            { return Unit.Kind == EProofUnitKind::Refinery && Unit.UnitId != NearRefinery; });
        if (!Far) { return false; }
        FarRefinery = Far->UnitId;
        Model.Advance(0.f);
        if (!Submit(EProofCommandType::Gather, Harvester, Ore).bAccepted) { return false; }
        Model.Advance(3.f);
        return Miner().Cargo == FMath::Min(OreQuantity, FProofCommandModel::CargoCapacity);
    }
    FProofUnitRecord Miner() const
    {
        const FProofSnapshot Snapshot = Model.GetSnapshot();
        return *Snapshot.Units.FindByPredicate([this](const FProofUnitRecord& Unit) { return Unit.UnitId == Harvester; });
    }
    FProofPlayerRecord Owner() const
    {
        const FProofSnapshot Snapshot = Model.GetSnapshot();
        return *Snapshot.Players.FindByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == 1; });
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoTravelTest, "RedalertProof.Skirmish.ReturnCargo.ChosenRefineryTravelAndResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoTravelTest::RunTest(const FString&)
{
    FReturnSetup Test;
    if (!TestTrue(TEXT("Loaded fixture"), Test.Initialize())) { return false; }
    TestTrue(TEXT("Add a nearer alternative ore patch"), Test.Model.SeedSkirmishPlayer(2, 0, FVector(-1500, 0, 0)));
    const FVector Start = Test.Miner().Position;
    const int32 Credits = Test.Owner().Credits;
    TestTrue(TEXT("Select farther refinery"), Test.Submit(EProofCommandType::ReturnCargo, Test.Harvester, Test.FarRefinery).bAccepted);
    TestTrue(TEXT("Submit never moves"), Test.Miner().Position == Start);
    TestEqual(TEXT("Submit never deposits"), Test.Owner().Credits, Credits);
    TestEqual(TEXT("Submit preserves all carried ore"), Test.Miner().Cargo, 50);
    Test.Model.Advance(1.f);
    TestTrue(TEXT("Return travels at the existing movement speed"), Test.Miner().Position.Equals(FVector(400, 0, 0), 0.001));
    TestEqual(TEXT("Travel has not deposited"), Test.Owner().TotalGathered, 0);
    TestTrue(TEXT("Repeated return is accepted while travelling"), Test.Submit(EProofCommandType::ReturnCargo, Test.Harvester, Test.FarRefinery).bAccepted);
    Test.Model.Advance(2.f / 3.f);
    TestTrue(TEXT("Miner passes the nearer refinery"), Test.Miner().Position.Equals(FVector(300, 0, 0), 0.001));
    TestEqual(TEXT("Passing another refinery cannot deposit"), Test.Miner().Cargo, 50);
    TestEqual(TEXT("Passing another refinery earns nothing"), Test.Owner().Credits, Credits);
    Test.Model.Advance(4.f);
    TestTrue(TEXT("Miner reaches the clicked refinery"), Test.Miner().Position.Equals(FVector(-300, 0, 0), 0.001));
    TestEqual(TEXT("Arrival deposits one load"), Test.Miner().Cargo, 0);
    TestEqual(TEXT("Exactly one load credited"), Test.Owner().Credits, Credits + 50);
    TestEqual(TEXT("Exactly one load counted"), Test.Owner().TotalGathered, 50);
    TestTrue(TEXT("Original Gather target survives repeated return"), FProofReturnCargoFixture::IsOrder(
        Test.Model, Test.Harvester, EProofCommandType::Gather, Test.Ore));
    Test.Model.Advance(1.f);
    TestTrue(TEXT("Resume heads toward the original live ore, not nearer ore"), Test.Miner().Position.Equals(FVector(-150, 0, 0), 0.001));
    TestEqual(TEXT("Resume travel does not mint another load"), Test.Owner().TotalGathered, 50);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoRetargetTest, "RedalertProof.Skirmish.ReturnCargo.RepeatedRetargetConservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoRetargetTest::RunTest(const FString&)
{
    FReturnSetup Test;
    if (!TestTrue(TEXT("Loaded fixture"), Test.Initialize())) { return false; }
    const int32 Credits = Test.Owner().Credits;
    TestTrue(TEXT("First explicit return"), Test.Submit(EProofCommandType::ReturnCargo, Test.Harvester, Test.NearRefinery).bAccepted);
    TestTrue(TEXT("Retarget to farther refinery"), Test.Submit(EProofCommandType::ReturnCargo, Test.Harvester, Test.FarRefinery).bAccepted);
    for (int32 Repeat = 0; Repeat < 4; ++Repeat)
    {
        TestTrue(TEXT("Repeated return accepted"), Test.Submit(EProofCommandType::ReturnCargo, Test.Harvester, Test.FarRefinery).bAccepted);
    }
    TestTrue(TEXT("Repeats preserve the saved Gather"), FProofReturnCargoFixture::IsOrder(Test.Model,
        Test.Harvester, EProofCommandType::ReturnCargo, Test.FarRefinery, Test.Ore));
    TestEqual(TEXT("No repeated command grants credits"), Test.Owner().Credits, Credits);
    Test.Model.Advance(6.f);
    TestEqual(TEXT("One return converts only existing cargo"), Test.Owner().TotalGathered, 50);
    TestEqual(TEXT("Ore, cargo and delivered ore are conserved"), Test.Model.GetSnapshot().Resources[0].Remaining
        + Test.Miner().Cargo + Test.Owner().TotalGathered, 500);
    const FProofSnapshot Deposited = Test.Model.GetSnapshot();
    TestEqual(TEXT("Return after unloading is rejected"), Test.Submit(EProofCommandType::ReturnCargo,
        Test.Harvester, Test.FarRefinery).Reason, FString(TEXT("cargo")));
    TestTrue(TEXT("Empty repeat does not change visible state"), ReturnSnapshotsEqual(Test.Model.GetSnapshot(), Deposited));
    TestTrue(TEXT("Empty repeat preserves resumed Gather"), FProofReturnCargoFixture::IsOrder(
        Test.Model, Test.Harvester, EProofCommandType::Gather, Test.Ore));
    Test.Model.Advance(6.f);
    TestEqual(TEXT("Resumed Gather extracts the next finite load"), Test.Miner().Cargo, 50);
    TestEqual(TEXT("Extraction alone cannot duplicate credits"), Test.Owner().TotalGathered, 50);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoNoAutoGatherTest, "RedalertProof.Skirmish.ReturnCargo.MoveStopIdleUnloadThenWait", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoNoAutoGatherTest::RunTest(const FString&)
{
    for (int32 Prior = 0; Prior < 3; ++Prior)
    {
        FReturnSetup Test;
        if (!TestTrue(TEXT("Loaded fixture"), Test.Initialize())) { return false; }
        if (Prior == 0)
        {
            TestTrue(TEXT("Move replaces Gather"), Test.Submit(EProofCommandType::Move,
                Test.Harvester, INDEX_NONE, FVector(550, 900, 0)).bAccepted);
        }
        else if (Prior == 1)
        {
            TestTrue(TEXT("Stop replaces Gather"), Test.Submit(EProofCommandType::Stop, Test.Harvester).bAccepted);
        }
        else { FProofReturnCargoFixture::ForgetOrder(Test.Model, Test.Harvester); }
        TestTrue(TEXT("Non-Gather miner can return its cargo"), Test.Submit(EProofCommandType::ReturnCargo,
            Test.Harvester, Test.FarRefinery).bAccepted);
        TestTrue(TEXT("Repeated non-Gather return accepted"), Test.Submit(EProofCommandType::ReturnCargo,
            Test.Harvester, Test.FarRefinery).bAccepted);
        Test.Model.Advance(6.f);
        TestEqual(TEXT("Existing cargo deposited"), Test.Owner().TotalGathered, 50);
        TestEqual(TEXT("Cargo emptied"), Test.Miner().Cargo, 0);
        TestFalse(TEXT("No Gather or old Move is invented"), FProofReturnCargoFixture::HasOrder(Test.Model, Test.Harvester));
        const FVector Arrived = Test.Miner().Position;
        Test.Model.Advance(100.f);
        TestTrue(TEXT("Miner waits at the refinery"), Test.Miner().Position == Arrived);
        TestEqual(TEXT("Waiting cannot grant more income"), Test.Owner().TotalGathered, 50);
        TestEqual(TEXT("Remaining ore stays untouched"), Test.Model.GetSnapshot().Resources[0].Remaining, 450);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoReplacementTest, "RedalertProof.Skirmish.ReturnCargo.StopMoveGatherReplaceReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoReplacementTest::RunTest(const FString&)
{
    for (int32 Replacement = 0; Replacement < 3; ++Replacement)
    {
        FReturnSetup Test;
        if (!TestTrue(TEXT("Loaded fixture"), Test.Initialize())) { return false; }
        TestTrue(TEXT("Return ordered"), Test.Submit(EProofCommandType::ReturnCargo, Test.Harvester, Test.FarRefinery).bAccepted);
        Test.Model.Advance(0.5f);
        const FVector Interrupted = Test.Miner().Position;
        if (Replacement == 0)
        {
            TestTrue(TEXT("Stop cancels forced return"), Test.Submit(EProofCommandType::Stop, Test.Harvester).bAccepted);
            Test.Model.Advance(10.f);
            TestTrue(TEXT("Stopped miner stays put"), Test.Miner().Position == Interrupted);
            TestEqual(TEXT("Stop preserves cargo"), Test.Miner().Cargo, 50);
            TestEqual(TEXT("Stop earns nothing"), Test.Owner().TotalGathered, 0);
        }
        else if (Replacement == 1)
        {
            const FVector MoveTarget(550, 300, 0);
            TestTrue(TEXT("Move replaces forced return"), Test.Submit(EProofCommandType::Move,
                Test.Harvester, INDEX_NONE, MoveTarget).bAccepted);
            Test.Model.Advance(10.f);
            TestTrue(TEXT("Replacement Move reaches its own destination"), Test.Miner().Position == MoveTarget);
            TestEqual(TEXT("Move preserves cargo"), Test.Miner().Cargo, 50);
            TestEqual(TEXT("Move earns nothing"), Test.Owner().TotalGathered, 0);
        }
        else
        {
            TestTrue(TEXT("Add replacement ore"), Test.Model.SeedSkirmishPlayer(2, 0, FVector(0, 1500, 0)));
            const int32 NewOre = Test.Model.GetSnapshot().Resources[1].ResourceId;
            TestTrue(TEXT("Gather replaces forced return"), Test.Submit(EProofCommandType::Gather, Test.Harvester, NewOre).bAccepted);
            Test.Model.Advance(2.f);
            TestTrue(TEXT("Ordinary Gather uses the nearer refinery"), Test.Miner().Position.Equals(FVector(300, 0, 0), 0.001));
            TestEqual(TEXT("Replacement Gather deposits only one load"), Test.Owner().TotalGathered, 50);
            TestTrue(TEXT("Replacement Gather keeps its new source"), FProofReturnCargoFixture::IsOrder(
                Test.Model, Test.Harvester, EProofCommandType::Gather, NewOre));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoLostTargetTest, "RedalertProof.Skirmish.ReturnCargo.LostRefineryCancelsWithoutFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoLostTargetTest::RunTest(const FString&)
{
    for (int32 Loss = 0; Loss < 4; ++Loss)
    {
        FReturnSetup Test;
        if (!TestTrue(TEXT("Loaded fixture"), Test.Initialize())) { return false; }
        TestTrue(TEXT("Return ordered"), Test.Submit(EProofCommandType::ReturnCargo, Test.Harvester, Test.FarRefinery).bAccepted);
        Test.Model.Advance(1.f);
        const FVector Interrupted = Test.Miner().Position;
        if (Loss == 0) { FProofReturnCargoFixture::Unit(Test.Model, Test.FarRefinery).HitPoints = 0.f; }
        else if (Loss == 1) { FProofReturnCargoFixture::RemoveUnit(Test.Model, Test.FarRefinery); }
        else if (Loss == 2) { FProofReturnCargoFixture::Unit(Test.Model, Test.FarRefinery).OwnerId = 2; }
        else { FProofReturnCargoFixture::Unit(Test.Model, Test.FarRefinery).Kind = EProofUnitKind::Factory; }
        Test.Model.Advance(20.f);
        TestTrue(TEXT("Invalid chosen refinery cancels before further movement"), Test.Miner().Position == Interrupted);
        TestFalse(TEXT("Forced return is cancelled"), FProofReturnCargoFixture::HasOrder(Test.Model, Test.Harvester));
        TestEqual(TEXT("No fallback can credit cargo"), Test.Owner().TotalGathered, 0);
        TestEqual(TEXT("All undelivered cargo survives"), Test.Miner().Cargo, 50);
        Test.Model.Advance(20.f);
        TestTrue(TEXT("Later ticks do not restart cancelled return"), Test.Miner().Position == Interrupted);
        if (Loss % 2 == 0)
        {
            TestTrue(TEXT("Explicit replacement Return recovers cargo"), Test.Submit(EProofCommandType::ReturnCargo,
                Test.Harvester, Test.NearRefinery).bAccepted);
        }
        else
        {
            TestTrue(TEXT("Explicit Gather recovers cargo"), Test.Submit(EProofCommandType::Gather, Test.Harvester, Test.Ore).bAccepted);
        }
        Test.Model.Advance(1.f);
        TestEqual(TEXT("Recovery credits exactly the saved cargo"), Test.Owner().TotalGathered, 50);
        TestEqual(TEXT("Recovery empties cargo"), Test.Miner().Cargo, 0);
        if (Loss % 2 == 0)
        {
            TestFalse(TEXT("Cancelled Gather is not revived by a later Return"), FProofReturnCargoFixture::HasOrder(Test.Model, Test.Harvester));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoSaturationTest, "RedalertProof.Skirmish.ReturnCargo.PartialCargoAndSaturatedCredits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoSaturationTest::RunTest(const FString&)
{
    FReturnSetup Test;
    if (!TestTrue(TEXT("Partially loaded fixture"), Test.Initialize(17))) { return false; }
    TestEqual(TEXT("Final extraction can be below capacity"), Test.Miner().Cargo, 17);
    TestEqual(TEXT("Final patch is depleted"), Test.Model.GetSnapshot().Resources[0].Remaining, 0);
    FProofReturnCargoFixture::Player(Test.Model, 1).Credits = MAX_int32 - 7;
    FProofReturnCargoFixture::Player(Test.Model, 1).TotalGathered = MAX_int32 - 3;
    const int64 BeforeValue = static_cast<int64>(Test.Owner().Credits) + Test.Miner().Cargo;
    TestTrue(TEXT("Partial cargo can be returned"), Test.Submit(EProofCommandType::ReturnCargo, Test.Harvester, Test.FarRefinery).bAccepted);
    Test.Model.Advance(6.f);
    TestEqual(TEXT("Credits saturate without overflow"), Test.Owner().Credits, MAX_int32);
    TestEqual(TEXT("Gathered counter saturates without overflow"), Test.Owner().TotalGathered, MAX_int32);
    TestEqual(TEXT("Uncredited cargo is retained"), Test.Miner().Cargo, 10);
    TestEqual(TEXT("Credit and cargo value is conserved"), static_cast<int64>(Test.Owner().Credits) + Test.Miner().Cargo, BeforeValue);
    TestTrue(TEXT("Incomplete deposit retains return and original Gather"), FProofReturnCargoFixture::IsOrder(Test.Model,
        Test.Harvester, EProofCommandType::ReturnCargo, Test.FarRefinery, Test.Ore));
    TestTrue(TEXT("Repeat at full credits preserves remaining cargo"), Test.Submit(EProofCommandType::ReturnCargo,
        Test.Harvester, Test.FarRefinery).bAccepted);
    Test.Model.Advance(10.f);
    TestEqual(TEXT("Full credits cannot discard cargo"), Test.Miner().Cargo, 10);
    TestTrue(TEXT("Full credits cannot resume mining"), Test.Miner().Position.Equals(FVector(-300, 0, 0), 0.001));
    TestTrue(TEXT("Ordinary spending makes room for the remainder"), Test.Submit(EProofCommandType::BuildFactory,
        INDEX_NONE, INDEX_NONE, FVector(0, 300, 0)).bAccepted);
    Test.Model.Advance(0.1f);
    TestEqual(TEXT("Remainder converts after room is available"), Test.Miner().Cargo, 0);
    TestEqual(TEXT("Only the remainder is credited"), Test.Owner().Credits, MAX_int32 - FProofCommandModel::FactoryCost + 10);
    TestFalse(TEXT("Depleted only source ends resumed Gather"), FProofReturnCargoFixture::HasOrder(Test.Model, Test.Harvester));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoResumePolicyTest, "RedalertProof.Skirmish.ReturnCargo.DepletedReselectsMissingStops", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoResumePolicyTest::RunTest(const FString&)
{
    FReturnSetup Depleted;
    if (!TestTrue(TEXT("Partial final load fixture"), Depleted.Initialize(17))) { return false; }
    TestTrue(TEXT("Add alternative ore"), Depleted.Model.SeedSkirmishPlayer(2, 0, FVector(-1500, 0, 0)));
    const int32 ReplacementOre = Depleted.Model.GetSnapshot().Resources[1].ResourceId;
    TestTrue(TEXT("Return depleted source cargo"), Depleted.Submit(EProofCommandType::ReturnCargo,
        Depleted.Harvester, Depleted.FarRefinery).bAccepted);
    Depleted.Model.Advance(6.f);
    TestEqual(TEXT("Partial load is credited once"), Depleted.Owner().TotalGathered, 17);
    TestTrue(TEXT("Existing depleted source uses unchanged reselection policy"), FProofReturnCargoFixture::IsOrder(
        Depleted.Model, Depleted.Harvester, EProofCommandType::Gather, ReplacementOre));
    Depleted.Model.Advance(5.f);
    TestTrue(TEXT("Reselected source is reached"), Depleted.Miner().Position.Equals(FVector(-950, 0, 0), 0.001));
    TestEqual(TEXT("Reselected Gather extracts normally"), Depleted.Miner().Cargo, 50);

    FReturnSetup Missing;
    if (!TestTrue(TEXT("Loaded fixture for source disconnect"), Missing.Initialize())) { return false; }
    TestTrue(TEXT("Seed source owner"), Missing.Model.SeedSkirmishPlayer(2, 0, FVector(0, 1500, 0)));
    const int32 RemovedOre = Missing.Model.GetSnapshot().Resources[1].ResourceId;
    TestTrue(TEXT("Gather another owner's ore"), Missing.Submit(EProofCommandType::Gather, Missing.Harvester, RemovedOre).bAccepted);
    Missing.Model.Advance(2.f); // Deliver the first carried load before visiting the new source.
    Missing.Model.Advance(11.f);
    TestEqual(TEXT("Source owner ore is loaded"), Missing.Miner().Cargo, 50);
    TestTrue(TEXT("Force return from source owner ore"), Missing.Submit(EProofCommandType::ReturnCargo,
        Missing.Harvester, Missing.FarRefinery).bAccepted);
    TestTrue(TEXT("Disconnect removes remembered ore"), Missing.Model.RemoveSkirmishPlayer(2));
    Missing.Model.Advance(12.f);
    TestEqual(TEXT("Carried ore still unloads after source disconnect"), Missing.Miner().Cargo, 0);
    TestEqual(TEXT("Only the two extracted loads are credited"), Missing.Owner().TotalGathered, 100);
    TestFalse(TEXT("Missing source stops instead of reselecting existing own ore"), FProofReturnCargoFixture::HasOrder(
        Missing.Model, Missing.Harvester));
    const FVector Stopped = Missing.Miner().Position;
    Missing.Model.Advance(100.f);
    TestTrue(TEXT("Missing-source miner stays stopped"), Missing.Miner().Position == Stopped);
    TestEqual(TEXT("Other ore is not harvested automatically"), Missing.Model.GetSnapshot().Resources[0].Remaining, 450);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoCombatTest, "RedalertProof.Skirmish.ReturnCargo.CombatPrecedesDeposit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoCombatTest::RunTest(const FString&)
{
    for (int32 Victim = 0; Victim < 2; ++Victim)
    {
        FReturnSetup Test;
        if (!TestTrue(TEXT("Loaded fixture"), Test.Initialize())) { return false; }
        TestTrue(TEXT("Seed opposing team"), Test.Model.SeedSkirmishPlayer(100, 1, FVector(3000, 0, 0)));
        TestTrue(TEXT("Enemy builds factory"), Test.Model.Submit(100, ReturnCommand(1,
            EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(2700, 0, 0))).bAccepted);
        const int32 Factory = ReturnFirstKind(Test.Model.GetSnapshot(), 100, EProofUnitKind::Factory);
        TestTrue(TEXT("Enemy produces combat"), Test.Model.Submit(100, ReturnCommand(2, EProofCommandType::Produce, Factory)).bAccepted);
        const int32 Enemy = ReturnFirstKind(Test.Model.GetSnapshot(), 100, EProofUnitKind::Combat);
        FProofReturnCargoFixture::Unit(Test.Model, Enemy).Position = FVector(-300, 250, 0);
        if (Victim == 0)
        {
            FProofReturnCargoFixture::Unit(Test.Model, Test.FarRefinery).HitPoints = 25.f;
            TestTrue(TEXT("Enemy attacks chosen refinery"), Test.Model.Submit(100, ReturnCommand(3,
                EProofCommandType::Attack, Enemy, Test.FarRefinery)).bAccepted);
        }
        else { FProofReturnCargoFixture::Unit(Test.Model, Test.Harvester).HitPoints = 25.f; }
        TestTrue(TEXT("Return before combat tick"), Test.Submit(EProofCommandType::ReturnCargo,
            Test.Harvester, Test.FarRefinery).bAccepted);
        Test.Model.Advance(6.f);
        const int32 VictimId = Victim == 0 ? Test.FarRefinery : Test.Harvester;
        TestEqual(TEXT("Arrival tick applies lethal combat"), FProofReturnCargoFixture::Unit(Test.Model, VictimId).HitPoints, 0.f);
        TestEqual(TEXT("Death before economic phase grants no credits"), Test.Owner().TotalGathered, 0);
        TestEqual(TEXT("Undelivered cargo is not converted"), Test.Miner().Cargo, 50);
        TestFalse(TEXT("Death cancels the forced order"), FProofReturnCargoFixture::HasOrder(Test.Model, Test.Harvester));
        Test.Model.Advance(1.f);
        TestEqual(TEXT("No later fallback deposit"), Test.Owner().TotalGathered, 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoValidationTest, "RedalertProof.Skirmish.ReturnCargo.OwnershipRolesAndGameplayAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoValidationTest::RunTest(const FString&)
{
    FReturnSetup Test;
    if (!TestTrue(TEXT("Loaded fixture"), Test.Initialize())) { return false; }
    TestTrue(TEXT("Seed teammate"), Test.Model.SeedSkirmishPlayer(2, 0, FVector(0, 2000, 0)));
    TestTrue(TEXT("Teammate builds refinery"), Test.Model.Submit(2, ReturnCommand(1,
        EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(300, 2000, 0))).bAccepted);
    TestTrue(TEXT("Seed opponent"), Test.Model.SeedSkirmishPlayer(100, 1, FVector(4000, 0, 0)));
    TestTrue(TEXT("Opponent builds refinery"), Test.Model.Submit(100, ReturnCommand(1,
        EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(4300, 0, 0))).bAccepted);
    const FProofSnapshot Ready = Test.Model.GetSnapshot();
    const auto Reject = [&](int32 Unit, int32 Target, const TCHAR* Reason)
    {
        const FProofSnapshot Before = Test.Model.GetSnapshot();
        const FProofCommandResult Result = Test.Submit(EProofCommandType::ReturnCargo, Unit, Target);
        TestFalse(TEXT("Invalid gameplay return is rejected"), Result.bAccepted);
        TestEqual(TEXT("Gameplay rejection reason"), Result.Reason, FString(Reason));
        TestTrue(TEXT("Gameplay rejection changes no visible state"), ReturnSnapshotsEqual(Test.Model.GetSnapshot(), Before));
        TestTrue(TEXT("Gameplay rejection preserves active Gather"), FProofReturnCargoFixture::IsOrder(
            Test.Model, Test.Harvester, EProofCommandType::Gather, Test.Ore));
    };
    Reject(ReturnFirstKind(Ready, 2, EProofUnitKind::Harvester), Test.FarRefinery, TEXT("ownership"));
    Reject(ReturnFirstKind(Ready, 100, EProofUnitKind::Harvester), Test.FarRefinery, TEXT("ownership"));
    Reject(9999, Test.FarRefinery, TEXT("ownership"));
    Reject(Test.HQ, Test.FarRefinery, TEXT("role"));
    Reject(Test.NearRefinery, Test.FarRefinery, TEXT("role"));
    FProofReturnCargoFixture::Unit(Test.Model, Test.NearRefinery).Kind = EProofUnitKind::Combat;
    Reject(Test.NearRefinery, Test.FarRefinery, TEXT("role"));
    Reject(Test.Harvester, Test.NearRefinery, TEXT("refinery"));
    FProofReturnCargoFixture::Unit(Test.Model, Test.NearRefinery).Kind = EProofUnitKind::Factory;
    Reject(Test.NearRefinery, Test.FarRefinery, TEXT("role"));
    Reject(Test.Harvester, Test.NearRefinery, TEXT("refinery"));
    FProofReturnCargoFixture::Unit(Test.Model, Test.NearRefinery).Kind = EProofUnitKind::Refinery;
    Reject(Test.Harvester, Test.HQ, TEXT("refinery"));
    Reject(Test.Harvester, Test.Harvester, TEXT("refinery"));
    Reject(Test.Harvester, 9999, TEXT("refinery"));
    Reject(Test.Harvester, ReturnFirstKind(Ready, 2, EProofUnitKind::Refinery), TEXT("refinery"));
    Reject(Test.Harvester, ReturnFirstKind(Ready, 100, EProofUnitKind::Refinery), TEXT("refinery"));
    FProofReturnCargoFixture::Unit(Test.Model, Test.FarRefinery).HitPoints = 0.f;
    Reject(Test.Harvester, Test.FarRefinery, TEXT("refinery"));
    FProofReturnCargoFixture::Unit(Test.Model, Test.FarRefinery).HitPoints = 200.f;
    FProofReturnCargoFixture::Unit(Test.Model, Test.Harvester).HitPoints = 0.f;
    Reject(Test.Harvester, Test.FarRefinery, TEXT("ownership"));
    FProofReturnCargoFixture::Unit(Test.Model, Test.Harvester).HitPoints = 100.f;
    FProofReturnCargoFixture::Unit(Test.Model, Test.Harvester).Cargo = 0;
    Reject(Test.Harvester, Test.FarRefinery, TEXT("cargo"));
    FProofReturnCargoFixture::Unit(Test.Model, Test.Harvester).Cargo = 50;
    TestTrue(TEXT("Gameplay rejections consume their sequence and next valid command succeeds"),
        Test.Submit(EProofCommandType::ReturnCargo, Test.Harvester, Test.FarRefinery).bAccepted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoEnvelopeTest, "RedalertProof.Skirmish.ReturnCargo.EnvelopeSequenceAndLegacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoEnvelopeTest::RunTest(const FString&)
{
    TestEqual(TEXT("Move enum is stable"), static_cast<int32>(EProofCommandType::Move), 0);
    TestEqual(TEXT("Attack enum is stable"), static_cast<int32>(EProofCommandType::Attack), 1);
    TestEqual(TEXT("Produce enum is stable"), static_cast<int32>(EProofCommandType::Produce), 2);
    TestEqual(TEXT("BuildRefinery enum is stable"), static_cast<int32>(EProofCommandType::BuildRefinery), 3);
    TestEqual(TEXT("BuildFactory enum is stable"), static_cast<int32>(EProofCommandType::BuildFactory), 4);
    TestEqual(TEXT("Gather enum is stable"), static_cast<int32>(EProofCommandType::Gather), 5);
    TestEqual(TEXT("Stop enum is stable"), static_cast<int32>(EProofCommandType::Stop), 6);
    TestEqual(TEXT("Rally enum is stable"), static_cast<int32>(EProofCommandType::SetRally), 7);
    TestEqual(TEXT("AttackMove enum is stable"), static_cast<int32>(EProofCommandType::AttackMove), 8);
    TestEqual(TEXT("ReturnCargo appends an explicit value"), static_cast<int32>(EProofCommandType::ReturnCargo), 9);
    FReturnSetup Test;
    if (!TestTrue(TEXT("Loaded fixture"), Test.Initialize())) { return false; }
    const int32 Next = Test.Sequence + 1;
    const FProofCommand Valid = ReturnCommand(Next, EProofCommandType::ReturnCargo, Test.Harvester, Test.FarRefinery);
    const auto Reject = [&](FProofCommand Command, const TCHAR* Reason)
    {
        const FProofSnapshot Before = Test.Model.GetSnapshot();
        TestEqual(TEXT("Envelope rejection reason"), Test.Model.Submit(1, Command).Reason, FString(Reason));
        TestTrue(TEXT("Rejected envelope is atomic"), ReturnSnapshotsEqual(Test.Model.GetSnapshot(), Before));
        TestTrue(TEXT("Rejected envelope cannot replace Gather"), FProofReturnCargoFixture::IsOrder(
            Test.Model, Test.Harvester, EProofCommandType::Gather, Test.Ore));
    };
    FProofCommand Bad = Valid;
    Bad.Sequence = 0; Reject(Bad, TEXT("malformed"));
    Bad = Valid; Bad.UnitId = INDEX_NONE; Reject(Bad, TEXT("malformed"));
    Bad = Valid; Bad.UnitId = 0; Reject(Bad, TEXT("malformed"));
    Bad = Valid; Bad.TargetUnitId = INDEX_NONE; Reject(Bad, TEXT("malformed"));
    Bad = Valid; Bad.TargetUnitId = 0; Reject(Bad, TEXT("malformed"));
    Bad = Valid; Bad.Destination.X = std::numeric_limits<double>::quiet_NaN(); Reject(Bad, TEXT("malformed"));
    Bad = Valid; Bad.Destination.Y = std::numeric_limits<double>::infinity(); Reject(Bad, TEXT("malformed"));
    Bad = Valid; Bad.Type = static_cast<EProofCommandType>(10); Reject(Bad, TEXT("malformed"));
    Bad = Valid; Bad.Sequence = Next + 1; Reject(Bad, TEXT("sequence"));
    TestTrue(TEXT("Malformed envelopes do not consume the expected sequence"), Test.Model.Submit(1, Valid).bAccepted);
    const FProofSnapshot Accepted = Test.Model.GetSnapshot();
    TestEqual(TEXT("Duplicate ordered envelope rejected"), Test.Model.Submit(1, Valid).Reason, FString(TEXT("sequence")));
    TestTrue(TEXT("Duplicate return cannot credit or move"), ReturnSnapshotsEqual(Test.Model.GetSnapshot(), Accepted));

    FProofCommandModel Legacy;
    Legacy.AddPlayer(1);
    const int32 LegacyUnit = Legacy.SeedUnit(1, FVector::ZeroVector);
    const FProofSnapshot BeforeLegacy = Legacy.GetSnapshot();
    TestEqual(TEXT("ReturnCargo remains unavailable in legacy mode"), Legacy.Submit(1,
        ReturnCommand(1, EProofCommandType::ReturnCargo, LegacyUnit, LegacyUnit)).Reason, FString(TEXT("malformed")));
    TestTrue(TEXT("Legacy rejection leaves snapshot untouched"), ReturnSnapshotsEqual(Legacy.GetSnapshot(), BeforeLegacy));
    TestTrue(TEXT("Legacy rejection preserves sequence for valid Move"), Legacy.Submit(1,
        ReturnCommand(1, EProofCommandType::Move, LegacyUnit, INDEX_NONE, FVector(100, 0, 0))).bAccepted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoRateTest, "RedalertProof.Skirmish.ReturnCargo.RateAtomicityAndZeroAdvance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoRateTest::RunTest(const FString&)
{
    FReturnSetup Test(1);
    if (!TestTrue(TEXT("Loaded rate-limited fixture"), Test.Initialize())) { return false; }
    TestTrue(TEXT("First return fits rate allowance"), Test.Submit(EProofCommandType::ReturnCargo,
        Test.Harvester, Test.FarRefinery).bAccepted);
    const FProofSnapshot Before = Test.Model.GetSnapshot();
    TestEqual(TEXT("Second return is rate limited"), Test.Submit(EProofCommandType::ReturnCargo,
        Test.Harvester, Test.NearRefinery).Reason, FString(TEXT("rate")));
    TestTrue(TEXT("Rate rejection is visibly atomic"), ReturnSnapshotsEqual(Test.Model.GetSnapshot(), Before));
    TestTrue(TEXT("Rate rejection preserves clicked target and saved Gather"), FProofReturnCargoFixture::IsOrder(
        Test.Model, Test.Harvester, EProofCommandType::ReturnCargo, Test.FarRefinery, Test.Ore));
    Test.Model.Advance(0.f);
    TestTrue(TEXT("Zero advance cannot move or deposit"), ReturnSnapshotsEqual(Test.Model.GetSnapshot(), Before));
    TestEqual(TEXT("Rate-rejected sequence stays consumed"), Test.Model.Submit(1,
        ReturnCommand(Test.Sequence, EProofCommandType::ReturnCargo, Test.Harvester, Test.NearRefinery)).Reason, FString(TEXT("sequence")));
    TestTrue(TEXT("New next sequence works after allowance reset"), Test.Submit(EProofCommandType::ReturnCargo,
        Test.Harvester, Test.FarRefinery).bAccepted);
    Test.Model.Advance(6.f);
    TestEqual(TEXT("Admitted repeats still deposit only one load"), Test.Owner().TotalGathered, 50);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofReturnCargoTerminalTest, "RedalertProof.Skirmish.ReturnCargo.TerminalFreezeAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofReturnCargoTerminalTest::RunTest(const FString&)
{
    FReturnSetup Test;
    if (!TestTrue(TEXT("Loaded fixture"), Test.Initialize())) { return false; }
    TestTrue(TEXT("Seed opposing HQ"), Test.Model.SeedSkirmishPlayer(100, 1, FVector(3000, 0, 0)));
    TestTrue(TEXT("Return is active before match finishes"), Test.Submit(EProofCommandType::ReturnCargo,
        Test.Harvester, Test.FarRefinery).bAccepted);
    const int32 EnemyHQ = ReturnFirstKind(Test.Model.GetSnapshot(), 100, EProofUnitKind::HQ);
    FProofReturnCargoFixture::Unit(Test.Model, EnemyHQ).HitPoints = 0.f;
    Test.Model.Advance(6.f);
    TestTrue(TEXT("HQ loss produces terminal state"), Test.Model.GetSnapshot().MatchPhase == EProofMatchPhase::Finished);
    TestEqual(TEXT("Terminal transition preempts deposit"), Test.Owner().TotalGathered, 0);
    TestEqual(TEXT("Terminal transition preserves carried amount"), Test.Miner().Cargo, 50);
    const FProofSnapshot Frozen = Test.Model.GetSnapshot();
    Test.Model.Advance(100.f);
    TestTrue(TEXT("Return cannot move terminal state"), ReturnSnapshotsEqual(Test.Model.GetSnapshot(), Frozen));
    TestEqual(TEXT("New terminal Return is rejected"), Test.Submit(EProofCommandType::ReturnCargo,
        Test.Harvester, Test.FarRefinery).Reason, FString(TEXT("finished")));
    TestTrue(TEXT("Terminal rejection is immutable"), ReturnSnapshotsEqual(Test.Model.GetSnapshot(), Frozen));
    Test.Model.StartSkirmish();
    TestTrue(TEXT("Fresh match seeded"), Test.Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    const int32 FreshMiner = ReturnFirstKind(Test.Model.GetSnapshot(), 1, EProofUnitKind::Harvester);
    TestFalse(TEXT("Reset cannot retain forced return or remembered Gather"), FProofReturnCargoFixture::HasOrder(Test.Model, FreshMiner));
    Test.Model.Advance(10.f);
    TestEqual(TEXT("Fresh match has no inherited income"), Test.Model.GetSnapshot().Players[0].TotalGathered, 0);
    return true;
}
#endif
