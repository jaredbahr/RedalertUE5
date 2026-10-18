#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"

// Test-only resource ordering/depletion setup. No production snapshot setter.
struct FProofOreReselectionFixture
{
    static void ReverseResources(FProofCommandModel& Model)
    {
        for (int32 A = 0, B = Model.State.Resources.Num() - 1; A < B; ++A, --B)
            Model.State.Resources.Swap(A, B);
    }
    static void SetPosition(FProofCommandModel& Model, int32 Id, const FVector& Position)
    {
        for (FProofResourceRecord& Resource : Model.State.Resources)
            if (Resource.ResourceId == Id) Resource.Position = Position;
    }
    static void SetRemaining(FProofCommandModel& Model, int32 Id, int32 Amount)
    {
        for (FProofResourceRecord& Resource : Model.State.Resources)
            if (Resource.ResourceId == Id) Resource.Remaining = Amount;
    }
};

namespace
{
FProofCommand OreOrder(int32 Sequence, EProofCommandType Type, int32 Unit,
    int32 Target = INDEX_NONE, const FVector& Destination = FVector::ZeroVector)
{
    FProofCommand Command;
    Command.Sequence = Sequence; Command.Type = Type; Command.UnitId = Unit;
    Command.TargetUnitId = Target; Command.Destination = Destination;
    return Command;
}
const FProofUnitRecord& OreMiner(const FProofSnapshot& Snapshot, int32 Id)
{
    return *Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; });
}
const FProofPlayerRecord& OreOwner(const FProofSnapshot& Snapshot)
{
    return *Snapshot.Players.FindByPredicate([](const FProofPlayerRecord& Player) { return Player.OwnerId == 1; });
}
const FProofResourceRecord& OrePatch(const FProofSnapshot& Snapshot, int32 Id)
{
    return *Snapshot.Resources.FindByPredicate([Id](const FProofResourceRecord& Resource) { return Resource.ResourceId == Id; });
}
bool OreSetup(FProofCommandModel& Model, int32& Miner, int32& Patch, int32& Granted, bool bOtherOre = true,
    bool bThirdOre = false, double ThirdY = -1000.0)
{
    Model.StartSkirmish();
    if (!Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector)) return false;
    if (bOtherOre && !Model.SeedSkirmishPlayer(2, 0, FVector(0, 1000, 0))) return false;
    if (bThirdOre && !Model.SeedSkirmishPlayer(3, 0, FVector(0, ThirdY, 0))) return false;
    const FProofSnapshot Opening = Model.GetSnapshot();
    Miner = Opening.Units.FindByPredicate([](const FProofUnitRecord& Unit)
        { return Unit.OwnerId == 1 && Unit.Kind == EProofUnitKind::Harvester; })->UnitId;
    Patch = Opening.Resources[0].ResourceId;
    if (!Model.Submit(1, OreOrder(1, EProofCommandType::BuildRefinery, INDEX_NONE,
        INDEX_NONE, FVector(300, 0, 0))).bAccepted) return false;
    Granted = Model.GetSnapshot().Units.Last().UnitId;
    // These tests control when each miner starts, so isolate the initial grant.
    if (!Model.Submit(1, OreOrder(2, EProofCommandType::Stop, Granted)).bAccepted) return false;
    return Model.Submit(1, OreOrder(3, EProofCommandType::Gather, Miner, Patch)).bAccepted;
}
bool OreFinishFirstPatch(FProofCommandModel& Model, int32 Miner, int32 Patch)
{
    for (int32 Tick = 0; Tick < 100; ++Tick)
    {
        const FProofSnapshot State = Model.GetSnapshot();
        if (OrePatch(State, Patch).Remaining == 0 && OreMiner(State, Miner).Cargo == 0
            && OreOwner(State).TotalGathered == 500) return true;
        Model.Advance(1.f);
    }
    return false;
}
int32 OreAccounted(const FProofSnapshot& Snapshot)
{
    int32 Total = 0;
    for (const FProofResourceRecord& Resource : Snapshot.Resources) Total += Resource.Remaining;
    for (const FProofUnitRecord& Unit : Snapshot.Units) Total += Unit.Cargo;
    for (const FProofPlayerRecord& Player : Snapshot.Players) Total += Player.TotalGathered;
    return Total;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofOreNearestTest, "RedalertProof.Skirmish.Economy.OreReselection.NearestAndTie", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofOreNearestTest::RunTest(const FString&)
{
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        FProofCommandModel Model; int32 Miner, Patch, Granted;
        if (!TestTrue(TEXT("Seed and submit real Gather"), OreSetup(Model, Miner, Patch, Granted,
            true, true, Variant == 0 ? -600.0 : -1000.0))) return false;
        if (Variant == 2) FProofOreReselectionFixture::ReverseResources(Model);
        if (!TestTrue(TEXT("Original finite patch fully delivered"), OreFinishFirstPatch(Model, Miner, Patch))) return false;
        const FVector Before = OreMiner(Model.GetSnapshot(), Miner).Position;
        Model.Advance(1.f);
        const FProofSnapshot After = Model.GetSnapshot();
        const FVector Step = OreMiner(After, Miner).Position - Before;
        TestTrue(TEXT("Reselection travels one bounded ordinary movement step"), Step.Size() > 0.0 && Step.Size() <= FProofCommandModel::MovementSpeed + 0.000001);
        TestTrue(TEXT("Nearest patch wins; equal distances use lowest ID even after array reversal"),
            Variant == 0 ? Step.Y < 0.0 : Step.Y > 0.0);
        TestEqual(TEXT("No new credit merely from choosing or approaching ore"), OreOwner(After).TotalGathered, 500);
        TestEqual(TEXT("Reselection cannot create or destroy finite resources"), OreAccounted(After), 1500);
    }
    // The nearer replacement differs at the exhausted patch and at the
    // refinery. Choose after unloading, from the actual departure position.
    FProofCommandModel Returning; int32 Miner, Patch, Granted;
    if (!TestTrue(TEXT("Seed post-deposit distance fixture"),
        OreSetup(Returning, Miner, Patch, Granted, true, true))) return false;
    const FProofSnapshot Opening = Returning.GetSnapshot();
    const int32 NearRefinery = Opening.Resources[1].ResourceId;
    const int32 NearExhaustedPatch = Opening.Resources[2].ResourceId;
    FProofOreReselectionFixture::SetPosition(Returning, NearRefinery, FVector(0, 100, 0));
    FProofOreReselectionFixture::SetPosition(Returning, NearExhaustedPatch, FVector(800, 0, 0));
    if (!TestTrue(TEXT("Unload entire original patch before reselection"),
        OreFinishFirstPatch(Returning, Miner, Patch))) return false;
    const FVector Departure = OreMiner(Returning.GetSnapshot(), Miner).Position;
    TestEqual(TEXT("Replacement departure is the refinery"), Departure, FVector(300, 0, 0));
    Returning.Advance(1.f);
    const FProofSnapshot After = Returning.GetSnapshot();
    const FVector Step = OreMiner(After, Miner).Position - Departure;
    TestTrue(TEXT("Nearest is evaluated from post-deposit position, not the depleted patch"),
        Step.X < 0.0 && Step.Y > 0.0 && Step.Size() <= FProofCommandModel::MovementSpeed + 0.000001);
    TestEqual(TEXT("Selection does not remotely extract nearby replacement ore"), OrePatch(After, NearRefinery).Remaining, 500);
    TestEqual(TEXT("The patch nearest the old extraction site remains untouched"), OrePatch(After, NearExhaustedPatch).Remaining, 500);
    TestEqual(TEXT("Post-deposit selection retains the credited total"), OreOwner(After).TotalGathered, 500);
    TestEqual(TEXT("Post-deposit selection conserves finite ore"), OreAccounted(After), 1500);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofOreCargoTest, "RedalertProof.Skirmish.Economy.OreReselection.CargoAndConservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofOreCargoTest::RunTest(const FString&)
{
    FProofCommandModel Model; int32 Miner, Patch, Granted;
    if (!TestTrue(TEXT("Seed cargo return fixture"), OreSetup(Model, Miner, Patch, Granted))) return false;
    bool bFinalLoad = false;
    for (int32 Tick = 0; Tick < 100; ++Tick)
    {
        const FProofSnapshot State = Model.GetSnapshot();
        if (OrePatch(State, Patch).Remaining == 0 && OreMiner(State, Miner).Cargo == 50)
        { bFinalLoad = true; break; }
        Model.Advance(1.f);
    }
    if (!TestTrue(TEXT("Observed the actual final extraction before deposit"), bFinalLoad)) return false;
    const FProofSnapshot Loaded = Model.GetSnapshot();
    TestEqual(TEXT("Final cargo is not yet credited"), OreOwner(Loaded).TotalGathered, 450);
    Model.Advance(1.f);
    const FProofSnapshot Returning = Model.GetSnapshot();
    TestTrue(TEXT("Loaded miner returns toward its refinery, not the next patch"),
        OreMiner(Returning, Miner).Position.X < OreMiner(Loaded, Miner).Position.X
        && OreMiner(Returning, Miner).Position.Y == 0.0);
    TestEqual(TEXT("In-transit cargo retained"), OreMiner(Returning, Miner).Cargo, 50);
    TestEqual(TEXT("In-transit credit unchanged"), OreOwner(Returning).TotalGathered, 450);
    Model.Advance(1.f);
    TestEqual(TEXT("Final old-patch load deposited exactly once"), OreOwner(Model.GetSnapshot()).TotalGathered, 500);
    TestEqual(TEXT("Cargo cleared before seeking next patch"), OreMiner(Model.GetSnapshot(), Miner).Cargo, 0);
    for (int32 Tick = 0; Tick < 60; ++Tick)
    {
        Model.Advance(1.f);
        const FProofSnapshot State = Model.GetSnapshot();
        TestEqual(TEXT("Remaining ore plus cargo plus delivered total is conserved"), OreAccounted(State), 1000);
        TestEqual(TEXT("Credits change only on delivery after one refinery cost"),
            OreOwner(State).Credits, 250 + OreOwner(State).TotalGathered);
    }
    TestTrue(TEXT("The existing order earns ore from the replacement patch without another command"),
        OreOwner(Model.GetSnapshot()).TotalGathered > 500);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofOreContestedTest, "RedalertProof.Skirmish.Economy.OreReselection.ContestedPatch", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofOreContestedTest::RunTest(const FString&)
{
    FProofCommandModel Model; int32 Miner, Patch, Granted;
    if (!TestTrue(TEXT("Seed two-miner fixture"), OreSetup(Model, Miner, Patch, Granted))) return false;
    // One ordinary cargo load remains. Both miners arrive at that same center
    // through Move commands, then their Gather orders compete in one advance.
    FProofOreReselectionFixture::SetRemaining(Model, Patch, 50);
    const FVector Center = OrePatch(Model.GetSnapshot(), Patch).Position;
    if (!TestTrue(TEXT("Move original miner"), Model.Submit(1, OreOrder(4, EProofCommandType::Move, Miner, INDEX_NONE, Center)).bAccepted)
        || !TestTrue(TEXT("Move granted miner"), Model.Submit(1, OreOrder(5, EProofCommandType::Move, Granted, INDEX_NONE, Center)).bAccepted)) return false;
    Model.Advance(10.f);
    if (!TestTrue(TEXT("First miner gathers"), Model.Submit(1, OreOrder(6, EProofCommandType::Gather, Miner, Patch)).bAccepted)
        || !TestTrue(TEXT("Second miner gathers"), Model.Submit(1, OreOrder(7, EProofCommandType::Gather, Granted, Patch)).bAccepted)) return false;
    Model.Advance(1.f);
    const FProofSnapshot Contested = Model.GetSnapshot();
    TestEqual(TEXT("First stable miner gets the final finite load"), OreMiner(Contested, Miner).Cargo, 50);
    TestEqual(TEXT("Second cannot duplicate the final load"), OreMiner(Contested, Granted).Cargo, 0);
    TestEqual(TEXT("Patch depleted exactly once"), OrePatch(Contested, Patch).Remaining, 0);
    TestEqual(TEXT("Replacement not harvested remotely in the same tick"), Contested.Resources[1].Remaining, 500);
    Model.Advance(1.f);
    const FProofSnapshot Next = Model.GetSnapshot();
    TestTrue(TEXT("Empty competing miner keeps Gather and heads toward the replacement"),
        OreMiner(Next, Granted).Position.Y > 0.0);
    TestTrue(TEXT("Loaded miner still returns to refinery"), OreMiner(Next, Miner).Position.X < Center.X);
    TestEqual(TEXT("Contested extraction preserves all finite ore"), OreAccounted(Next), 550);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofOreOrdersTest, "RedalertProof.Skirmish.Economy.OreReselection.ExplicitOrdersAndExhaustion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofOreOrdersTest::RunTest(const FString&)
{
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        FProofCommandModel Model; int32 Miner, Patch, Granted;
        if (!TestTrue(TEXT("Seed priority fixture"), OreSetup(Model, Miner, Patch, Granted, Variant != 2))) return false;
        if (!TestTrue(TEXT("Finish old assigned patch"), OreFinishFirstPatch(Model, Miner, Patch))) return false;
        const FVector Before = OreMiner(Model.GetSnapshot(), Miner).Position;
        if (Variant == 0)
        {
            TestTrue(TEXT("Explicit Stop replaces Gather"), Model.Submit(1, OreOrder(4, EProofCommandType::Stop, Miner)).bAccepted);
        }
        else if (Variant == 1)
        {
            TestTrue(TEXT("Explicit Move replaces Gather"), Model.Submit(1, OreOrder(4, EProofCommandType::Move,
                Miner, INDEX_NONE, FVector(-300, 0, 0))).bAccepted);
        }
        Model.Advance(100.f);
        const FProofSnapshot After = Model.GetSnapshot();
        TestEqual(TEXT("Explicit order or total depletion prevents new income"), OreOwner(After).TotalGathered, 500);
        TestEqual(TEXT("Cargo stays empty"), OreMiner(After, Miner).Cargo, 0);
        TestEqual(TEXT("Stop/depletion stays put; Move reaches its chosen point"), OreMiner(After, Miner).Position,
            Variant == 1 ? FVector(-300, 0, 0) : Before);
        if (Variant != 2) TestEqual(TEXT("Alternative ore untouched after overriding Gather"), After.Resources[1].Remaining, 500);
    }
    FProofCommandModel Retain; int32 Miner, Patch, Granted;
    if (!TestTrue(TEXT("Seed current-patch priority fixture"), OreSetup(Retain, Miner, Patch, Granted, false))) return false;
    // The current live patch remains assigned even if another patch becomes
    // closer to the miner on its trip. No opportunistic target switching.
    if (!TestTrue(TEXT("Seed a closer alternative"), Retain.SeedSkirmishPlayer(2, 0, FVector(-500, 100, 0)))) return false;
    Retain.Advance(1.f);
    TestEqual(TEXT("Live explicit patch retained while travelling"), OreMiner(Retain.GetSnapshot(), Miner).Position, FVector(250, 0, 0));
    return true;
}
#endif
