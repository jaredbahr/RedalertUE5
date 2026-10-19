#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Skirmish/ProofSkirmishBot.h"
#include <limits>

namespace
{
// These prepared snapshots isolate planner policy. They do not claim to be
// reachable match histories; PublicRecovery below uses only the public model.
const FVector BotExpansionOffsets[] = {
    FVector(350, 0, 0), FVector(-350, 0, 0), FVector(0, 350, 0), FVector(0, -350, 0),
    FVector(350, 350, 0), FVector(-350, 350, 0), FVector(350, -350, 0), FVector(-350, -350, 0),
    FVector(600, 0, 0), FVector(-600, 0, 0), FVector(0, 600, 0), FVector(0, -600, 0)
};
const FVector BotExpansionRing[] = {
    FVector(425, 175, 0), FVector(-175, 425, 0), FVector(-425, -175, 0), FVector(175, -425, 0)
};

void BotExpansionAdd(FProofSnapshot& Snapshot, int32 Id, int32 Owner, EProofUnitKind Kind,
    const FVector& Position, float HitPoints = 100.f)
{
    FProofUnitRecord Unit;
    Unit.UnitId = Id;
    Unit.OwnerId = Owner;
    Unit.Kind = Kind;
    Unit.Position = Position;
    Unit.HitPoints = HitPoints;
    Snapshot.Units.Add(Unit);
}

FProofSnapshot BotExpansionPrepared(EProofUnitKind AnchorKind = EProofUnitKind::Refinery)
{
    FProofSnapshot Snapshot;
    Snapshot.bSkirmish = true;
    Snapshot.MatchPhase = EProofMatchPhase::Playing;
    for (int32 Owner : {1, 2, 3})
    {
        FProofPlayerRecord Player;
        Player.OwnerId = Owner;
        Player.TeamId = Owner == 3 ? 1 : 0;
        Player.Credits = 100;
        Snapshot.Players.Add(Player);
    }
    BotExpansionAdd(Snapshot, 10, 1, EProofUnitKind::HQ, FVector::ZeroVector);
    BotExpansionAdd(Snapshot, 20, 1, AnchorKind, FVector(2000, 0, 0));
    for (int32 Index = 0; Index < 4; ++Index)
        BotExpansionAdd(Snapshot, 100 + Index, 3, EProofUnitKind::Factory, BotExpansionRing[Index]);
    FProofResourceRecord Ore;
    Ore.ResourceId = 1;
    Ore.Position = FVector(2500, 0, 0);
    Ore.Remaining = 500;
    Snapshot.Resources.Add(Ore);
    return Snapshot;
}

bool BotExpansionStructure(EProofUnitKind Kind)
{
    return Kind == EProofUnitKind::HQ || Kind == EProofUnitKind::Refinery || Kind == EProofUnitKind::Factory;
}

bool BotExpansionSiteClear(const FProofSnapshot& Snapshot, const FVector& Position, bool bRefinery)
{
    if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Position.Z)
        || FMath::Abs(Position.X) > FProofCommandModel::WorldExtent
        || FMath::Abs(Position.Y) > FProofCommandModel::WorldExtent
        || FMath::Abs(Position.Z) > FProofCommandModel::WorldExtent) { return false; }
    for (const FProofUnitRecord& Unit : Snapshot.Units)
        if (Unit.HitPoints > 0.f && BotExpansionStructure(Unit.Kind)
            && FVector::DistSquared(Position, Unit.Position) < 250.0 * 250.0) { return false; }
    FVector Spawn = FVector::ZeroVector;
    return !bRefinery || FProofCommandModel::FindSkirmishSpawn(Snapshot, Position, Spawn);
}

int32 BotExpansionCount(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    int32 Count = 0;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
        if (Unit.OwnerId == Owner && Unit.HitPoints > 0.f && Unit.Kind == Kind) { ++Count; }
    return Count;
}

int32 BotExpansionLive(const FProofSnapshot& Snapshot, int32 Owner)
{
    int32 Count = 0;
    for (const FProofUnitRecord& Unit : Snapshot.Units)
        if (Unit.OwnerId == Owner && Unit.HitPoints > 0.f) { ++Count; }
    return Count;
}

bool BotExpansionIntent(const FProofCommand& Command, EProofCommandType Type, int32 Sequence,
    const FVector& Position)
{
    return Command.Type == Type && Command.Sequence == Sequence && Command.UnitId == INDEX_NONE
        && Command.TargetUnitId == INDEX_NONE && Command.Destination == Position;
}

struct FBotExpansionPublicFixture
{
    FProofCommandModel Model;
    int32 OwnerSequence = 0;
    int32 EnemySequence = 0;
    int32 OwnerSpent = 0;
    double Seconds = 0.0;
    const FVector HQ = FVector(-1400, -400, 0);

    void Advance()
    {
        Model.Advance(1.f);
        Seconds += 1.0;
    }

    bool Submit(int32 Owner, EProofCommandType Type, const FVector& Position = FVector::ZeroVector,
        int32 Unit = INDEX_NONE, int32 Target = INDEX_NONE)
    {
        FProofCommand Command;
        Command.Sequence = Owner == 1 ? ++OwnerSequence : ++EnemySequence;
        Command.Type = Type;
        Command.Destination = Position;
        Command.UnitId = Unit;
        Command.TargetUnitId = Target;
        return SubmitIntent(Owner, Command);
    }

    bool SubmitIntent(int32 Owner, const FProofCommand& Command)
    {
        if (!Model.Submit(Owner, Command).bAccepted) { return false; }
        if (Owner == 1)
        {
            if (Command.Type == EProofCommandType::BuildRefinery) { OwnerSpent += FProofCommandModel::RefineryCost; }
            if (Command.Type == EProofCommandType::BuildFactory) { OwnerSpent += FProofCommandModel::FactoryCost; }
        }
        return true;
    }

    int32 Id(int32 Owner, EProofUnitKind Kind) const
    {
        int32 Result = INDEX_NONE;
        for (const FProofUnitRecord& Unit : Model.GetSnapshot().Units)
            if (Unit.OwnerId == Owner && Unit.Kind == Kind && Unit.HitPoints > 0.f
                && (Result == INDEX_NONE || Unit.UnitId < Result)) { Result = Unit.UnitId; }
        return Result;
    }

    FProofUnitRecord Unit(int32 IdValue) const
    {
        for (const FProofUnitRecord& Record : Model.GetSnapshot().Units)
            if (Record.UnitId == IdValue) { return Record; }
        return FProofUnitRecord();
    }

    FProofPlayerRecord Player() const
    {
        for (const FProofPlayerRecord& Record : Model.GetSnapshot().Players)
            if (Record.OwnerId == 1) { return Record; }
        return FProofPlayerRecord();
    }

    bool StopMiners()
    {
        const FProofSnapshot Snapshot = Model.GetSnapshot();
        for (const FProofUnitRecord& Record : Snapshot.Units)
            if (Record.OwnerId == 1 && Record.Kind == EProofUnitKind::Harvester && Record.HitPoints > 0.f
                && !Submit(1, EProofCommandType::Stop, FVector::ZeroVector, Record.UnitId)) { return false; }
        return true;
    }

    bool KillMiner(int32 Target)
    {
        const int32 Attacker = Id(100, EProofUnitKind::Combat);
        const FVector SafeEnemy(1000, 3000, 0);
        const FVector CombatSite(-1400, 1400, 0);
        if (!Submit(100, EProofCommandType::Move, SafeEnemy, Attacker)) { return false; }
        for (int32 Tick = 0; Tick < 100 && Unit(Attacker).Position != SafeEnemy; ++Tick) { Advance(); }
        if (Unit(Attacker).Position != SafeEnemy || !Submit(1, EProofCommandType::Move, CombatSite, Target)) { return false; }
        for (int32 Tick = 0; Tick < 100 && Unit(Target).Position != CombatSite; ++Tick) { Advance(); }
        if (Unit(Target).Position != CombatSite
            || !Submit(100, EProofCommandType::Attack, FVector::ZeroVector, Attacker, Target)) { return false; }
        for (int32 Tick = 0; Tick < 100 && Unit(Target).HitPoints > 0.f; ++Tick) { Advance(); }
        return Unit(Target).HitPoints == 0.f && Submit(100, EProofCommandType::Stop, FVector::ZeroVector, Attacker);
    }

    bool Conserved() const
    {
        const FProofSnapshot Snapshot = Model.GetSnapshot();
        int32 Total = 0;
        for (const FProofResourceRecord& Ore : Snapshot.Resources) { Total += Ore.Remaining; }
        for (const FProofUnitRecord& Record : Snapshot.Units) { Total += Record.Cargo; }
        for (const FProofPlayerRecord& Record : Snapshot.Players) { Total += Record.TotalGathered; }
        // No owner-one job is cancelled in this trace. Retained Combat records
        // count completed payments, including any later-dead Combat; active
        // jobs contribute only paid credits rather than their entire price.
        int32 ProductionPaid = 0;
        for (const FProofUnitRecord& Record : Snapshot.Units)
            if (Record.OwnerId == 1 && Record.Kind == EProofUnitKind::Combat)
                ProductionPaid += FProofCommandModel::ProductionCost;
        for (const FProofProductionJob& Job : Snapshot.ProductionJobs)
            if (Job.OwnerId == 1) ProductionPaid += Job.PaidCredits;
        return Total == 1000 && Player().Credits == 350 + Player().TotalGathered - OwnerSpent - ProductionPaid;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBotBaseExpansionPlannerTest,
    "RedalertProof.Skirmish.BotBaseExpansion.PlannerAndOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBotBaseExpansionPlannerTest::RunTest(const FString&)
{
    FProofCommandModel Opening;
    Opening.StartSkirmish();
    if (!TestTrue(TEXT("Public opening seed"), Opening.SeedSkirmishPlayer(1, 0, FVector::ZeroVector))) { return false; }
    const FProofSnapshot Start = Opening.GetSnapshot();
    FProofSkirmishBot OpeningBot;
    const TArray<FProofCommand> OpeningBatch = OpeningBot.Decide(Start, 1, 0.0);
    TestEqual(TEXT("Unchanged opening contains exactly refinery then factory"), OpeningBatch.Num(), 2);
    if (OpeningBatch.Num() != 2) { return false; }
    TestTrue(TEXT("Opening refinery retains first HQ offset and sequence"),
        BotExpansionIntent(OpeningBatch[0], EProofCommandType::BuildRefinery, 1, FVector(350, 0, 0)));
    TestTrue(TEXT("Opening factory retains second HQ offset and sequence"),
        BotExpansionIntent(OpeningBatch[1], EProofCommandType::BuildFactory, 2, FVector(-350, 0, 0)));
    for (const FProofCommand& Command : OpeningBatch)
        TestTrue(TEXT("Unchanged opening command admitted"), Opening.Submit(1, Command).bAccepted);
    TestEqual(TEXT("Opening spends exactly 250"), Opening.GetSnapshot().Players[0].Credits, 100);

    for (EProofUnitKind Kind : {EProofUnitKind::HQ, EProofUnitKind::Refinery, EProofUnitKind::Factory})
    {
        FProofSnapshot Snapshot = BotExpansionPrepared(Kind);
        FProofSkirmishBot Bot;
        const FString Before = Snapshot.ComputeDigest();
        const TArray<FProofCommand> Batch = Bot.Decide(Snapshot, 1, 0.0);
        TestTrue(TEXT("Each committed own live structure supports exactly one recovery intent"), Batch.Num() == 1
            && BotExpansionIntent(Batch[0], EProofCommandType::BuildRefinery, 1, FVector(2350, 0, 0)));
        TestEqual(TEXT("Planning leaves visible state intact"), Snapshot.ComputeDigest(), Before);
    }

    FProofSnapshot Sorted = BotExpansionPrepared();
    BotExpansionAdd(Sorted, 15, 1, EProofUnitKind::Refinery, FVector(-2000, 0, 0));
    for (int32 Rotation = 0; Rotation < Sorted.Units.Num(); ++Rotation)
    {
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(Sorted, 1, 0.0);
        TestTrue(TEXT("Lowest eligible stable ID wins independently of array position"), Batch.Num() == 1
            && BotExpansionIntent(Batch[0], EProofCommandType::BuildRefinery, 1, FVector(-1650, 0, 0)));
        for (int32 Index = 1; Index < Sorted.Units.Num(); ++Index) { Sorted.Units.Swap(Index - 1, Index); }
    }
    FProofSnapshot NextAnchor = Sorted;
    for (int32 Index = 0; Index < 12; ++Index)
        BotExpansionAdd(NextAnchor, 300 + Index, 3, EProofUnitKind::Factory,
            FVector(-2000, 0, 0) + BotExpansionOffsets[Index]);
    FProofSkirmishBot NextAnchorBot;
    const TArray<FProofCommand> NextAnchorBatch = NextAnchorBot.Decide(NextAnchor, 1, 0.0);
    TestTrue(TEXT("Exhausting the first eligible anchor continues to the next stable ID"), NextAnchorBatch.Num() == 1
        && BotExpansionIntent(NextAnchorBatch[0], EProofCommandType::BuildRefinery, 1, FVector(2350, 0, 0)));
    FProofSnapshot HQFirst = BotExpansionPrepared();
    HQFirst.Units.RemoveAll([](const FProofUnitRecord& Unit) { return Unit.OwnerId == 3; });
    HQFirst.Units[1].UnitId = 1;
    FProofSkirmishBot HQFirstBot;
    const TArray<FProofCommand> HQFirstBatch = HQFirstBot.Decide(HQFirst, 1, 0.0);
    TestTrue(TEXT("HQ search precedes even a lower-ID eligible refinery"), HQFirstBatch.Num() == 1
        && HQFirstBatch[0].Destination == FVector(350, 0, 0));
    HQFirst.Units[1].Kind = EProofUnitKind::HQ;
    FProofSkirmishBot PrimaryBot;
    const TArray<FProofCommand> PrimaryBatch = PrimaryBot.Decide(HQFirst, 1, 0.0);
    TestTrue(TEXT("Lowest-ID own live HQ remains the primary HQ"), PrimaryBatch.Num() == 1
        && PrimaryBatch[0].Destination == FVector(2350, 0, 0));

    for (int32 Case = 0; Case < 6; ++Case)
    {
        FProofSnapshot Ineligible = BotExpansionPrepared();
        if (Case == 0) { Ineligible.Units[1].OwnerId = 3; }
        if (Case == 1) { Ineligible.Units[1].OwnerId = 2; }
        if (Case == 2) { Ineligible.Units[1].OwnerId = INDEX_NONE; }
        if (Case == 3) { Ineligible.Units[1].HitPoints = 0.f; }
        if (Case == 4) { Ineligible.Units[1].Kind = EProofUnitKind::Harvester; }
        if (Case == 5) { Ineligible.Units[0].HitPoints = 0.f; }
        FProofSkirmishBot Bot;
        TestEqual(TEXT("Foreign, teammate, neutral, dead, mobile or missing-HQ anchors cannot build"),
            Bot.Decide(Ineligible, 1, 0.0).Num(), 0);
    }
    FProofSnapshot CombatAnchor = BotExpansionPrepared(EProofUnitKind::Combat);
    FProofSkirmishBot CombatBot;
    const TArray<FProofCommand> CombatBatch = CombatBot.Decide(CombatAnchor, 1, 0.0);
    TestTrue(TEXT("Combat record contributes only its ordinary guard, never a construction anchor"),
        CombatBatch.Num() == 1 && CombatBatch[0].Type == EProofCommandType::AreaGuard
        && CombatBatch[0].UnitId == 20 && CombatBatch[0].Sequence == 1);

    for (int32 Credits : {0, 99})
    {
        FProofSnapshot Poor = BotExpansionPrepared();
        Poor.Players[0].Credits = Credits;
        FProofSkirmishBot Bot;
        TestEqual(TEXT("Fallback cannot bypass refinery funds"), Bot.Decide(Poor, 1, 0.0).Num(), 0);
    }
    FProofSnapshot PoorFactory = BotExpansionPrepared();
    PoorFactory.Resources.Reset();
    PoorFactory.Players[0].Credits = FProofCommandModel::FactoryCost - 1;
    FProofSkirmishBot PoorFactoryBot;
    TestEqual(TEXT("Fallback cannot bypass factory funds"), PoorFactoryBot.Decide(PoorFactory, 1, 0.0).Num(), 0);
    for (int32 Live : {11, 12})
    {
        FProofSnapshot FactoryCapacity = PoorFactory;
        FactoryCapacity.Players[0].Credits = FProofCommandModel::FactoryCost;
        for (int32 Index = 2; Index < Live; ++Index)
            BotExpansionAdd(FactoryCapacity, 300 + Index, 1, EProofUnitKind::Harvester, FVector(4000, Index * 300, 0));
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(FactoryCapacity, 1, 0.0);
        TestEqual(TEXT("Factory-only fallback requires exactly one free live record slot"), Batch.Num(), Live == 11 ? 1 : 0);
        if (Live == 11 && Batch.Num() == 1)
            TestTrue(TEXT("Exactly 150 credits funds one fallback factory at eleven live records"),
                BotExpansionIntent(Batch[0], EProofCommandType::BuildFactory, 1, FVector(2350, 0, 0)));
    }
    for (int32 Live : {10, 11, 12})
    {
        FProofSnapshot Full = BotExpansionPrepared();
        for (int32 Index = 2; Index < Live; ++Index)
            BotExpansionAdd(Full, 200 + Index, 1, EProofUnitKind::Factory, FVector(4000, Index * 300, 0));
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(Full, 1, 0.0);
        TestEqual(TEXT("Exact batch obeys two-slot recovery and one-slot production capacity"), Batch.Num(), Live < 12 ? 1 : 0);
        if (Live < 12 && Batch.Num() == 1)
            TestTrue(TEXT("Capacity preserves existing economic priority and contiguous sequence"), Batch[0].Sequence == 1
                && Batch[0].Type == (Live == 10 ? EProofCommandType::BuildRefinery : EProofCommandType::Produce));
    }
    for (int32 OreCase = 0; OreCase < 4; ++OreCase)
    {
        FProofSnapshot NoOre = BotExpansionPrepared();
        if (OreCase == 0) { NoOre.Resources.Reset(); }
        if (OreCase == 1) { NoOre.Resources[0].Remaining = 0; }
        if (OreCase == 2) { NoOre.Resources[0].Remaining = -1; }
        if (OreCase == 3) { NoOre.Resources[0].ResourceId = 0; }
        FProofSkirmishBot Bot;
        TestEqual(TEXT("Fallback cannot turn missing or unusable ore into income"), Bot.Decide(NoOre, 1, 0.0).Num(), 0);
    }
    FProofSnapshot Cargo = BotExpansionPrepared(EProofUnitKind::Factory);
    Cargo.Resources.Reset();
    BotExpansionAdd(Cargo, 40, 1, EProofUnitKind::Harvester, FVector(5000, 0, 0));
    for (int32 Amount : {0, 1, FProofCommandModel::CargoCapacity})
    {
        Cargo.Units.Last().Cargo = Amount;
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(Cargo, 1, 0.0);
        TestTrue(TEXT("Missing refinery requires actual own cargo when ore is gone"), Batch.Num() == 1 && Batch[0].Sequence == 1
            && Batch[0].Type == (Amount > 0 ? EProofCommandType::BuildRefinery : EProofCommandType::Produce));
    }
    for (int32 Case = 0; Case < 2; ++Case)
    {
        FProofSnapshot InvalidCargo = Cargo;
        if (Case == 0) { InvalidCargo.Units.Last().OwnerId = 2; }
        else { InvalidCargo.Units.Last().HitPoints = 0.f; }
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(InvalidCargo, 1, 0.0);
        TestTrue(TEXT("Foreign or dead cargo cannot fund missing-refinery intent"), Batch.Num() == 1
            && Batch[0].Type == EProofCommandType::Produce && Batch[0].Sequence == 1);
    }
    FProofSnapshot Healthy = BotExpansionPrepared();
    Healthy.Players[0].Credits = 350;
    BotExpansionAdd(Healthy, 40, 1, EProofUnitKind::Harvester, FVector(2400, 0, 0));
    BotExpansionAdd(Healthy, 50, 1, EProofUnitKind::Factory, FVector(-2000, 0, 0));
    FProofSkirmishBot HealthyBot;
    const TArray<FProofCommand> HealthyBatch = HealthyBot.Decide(Healthy, 1, 0.0);
    TestTrue(TEXT("A healthy economy does not build merely to expand"), HealthyBatch.Num() == 2
        && HealthyBatch[0].Type == EProofCommandType::Gather && HealthyBatch[0].Sequence == 1
        && HealthyBatch[1].Type == EProofCommandType::Produce && HealthyBatch[1].Sequence == 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBotBaseExpansionPlacementTest,
    "RedalertProof.Skirmish.BotBaseExpansion.PlacementAndReservations",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBotBaseExpansionPlacementTest::RunTest(const FString&)
{
    for (int32 Available = 0; Available <= 12; ++Available)
    {
        FProofSnapshot Snapshot = BotExpansionPrepared();
        for (int32 Index = 0; Index < Available; ++Index)
            BotExpansionAdd(Snapshot, 200 + Index, 3, EProofUnitKind::Factory,
                FVector(2000, 0, 0) + BotExpansionOffsets[Index]);
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(Snapshot, 1, 0.0);
        TestEqual(TEXT("Exactly twelve fallback offsets, no wider implicit search"), Batch.Num(), Available < 12 ? 1 : 0);
        if (Available < 12 && Batch.Num() == 1)
            TestTrue(TEXT("Fallback retains exact original cardinal/diagonal order"),
                BotExpansionIntent(Batch[0], EProofCommandType::BuildRefinery, 1,
                    FVector(2000, 0, 0) + BotExpansionOffsets[Available]));
    }
    for (int32 Case = 0; Case < 8; ++Case)
    {
        FProofSnapshot Boundary = BotExpansionPrepared();
        const FVector Positions[] = {FVector(9650, 0, 0), FVector(9900, 0, 0),
            FVector(10601, 0, 0), FVector(0, 10601, 0), FVector(0, 0, 10001),
            FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0),
            FVector(0, std::numeric_limits<double>::infinity(), 0),
            FVector(0, 0, -std::numeric_limits<double>::infinity())};
        Boundary.Units[1].Position = Positions[Case];
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(Boundary, 1, 0.0);
        TestEqual(TEXT("World bounds apply to every fallback anchor candidate"), Batch.Num(), Case < 2 ? 1 : 0);
        if (Case < 2 && Batch.Num() == 1)
            TestTrue(TEXT("Inclusive boundary or next in-bounds offset is selected"),
                Batch[0].Destination == FVector(Case == 0 ? 10000 : 9550, 0, 0));
    }
    for (int32 Case = 0; Case < 3; ++Case)
    {
        FProofSnapshot Spacing = BotExpansionPrepared();
        BotExpansionAdd(Spacing, 200, 2, EProofUnitKind::Factory,
            FVector(2350, Case == 0 ? 250.0 : 249.0, 0), Case == 2 ? 0.f : 100.f);
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(Spacing, 1, 0.0);
        TestTrue(TEXT("Planner retains 250cm inclusive spacing against live structures of every owner"),
            Batch.Num() == 1 && Batch[0].Destination == FVector(Case == 1 ? 1650 : 2350, 0, 0));
    }
    FProofSnapshot SpawnBlocked = BotExpansionPrepared();
    for (int32 X = -1800; X <= 1800; X += 300)
        for (int32 Y = -1800; Y <= 1800; Y += 300)
            BotExpansionAdd(SpawnBlocked, 1000 + SpawnBlocked.Units.Num(), 3, EProofUnitKind::Harvester,
                FVector(2000 + X, Y, 0));
    FProofSkirmishBot SpawnBot;
    TestEqual(TEXT("All refinery grant spawn candidates exhausted prevents fallback"), SpawnBot.Decide(SpawnBlocked, 1, 0.0).Num(), 0);
    SpawnBlocked.Resources.Reset();
    SpawnBlocked.Players[0].Credits = FProofCommandModel::FactoryCost;
    FProofSkirmishBot FactoryBot;
    const TArray<FProofCommand> FactoryBatch = FactoryBot.Decide(SpawnBlocked, 1, 0.0);
    TestTrue(TEXT("Existing mobile records do not acquire a new factory footprint restriction"), FactoryBatch.Num() == 1
        && BotExpansionIntent(FactoryBatch[0], EProofCommandType::BuildFactory, 1, FVector(2350, 0, 0)));

    for (int32 Credits : {249, 250})
    {
        FProofSnapshot Budgeted = BotExpansionPrepared(EProofUnitKind::HQ);
        Budgeted.Players[0].Credits = Credits;
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(Budgeted, 1, 0.0);
        TestEqual(TEXT("Same-batch spending reserves refinery cost before factory affordability"), Batch.Num(), Credits == 250 ? 2 : 1);
        TestTrue(TEXT("Recovery remains first at the combined-price boundary"), Batch.Num() >= 1
            && Batch[0].Type == EProofCommandType::BuildRefinery && Batch[0].Sequence == 1);
        if (Batch.Num() == 2)
            TestTrue(TEXT("Only fully funded second construction consumes sequence two"),
                Batch[1].Type == EProofCommandType::BuildFactory && Batch[1].Sequence == 2);
    }
    for (int32 Live : {9, 10, 11, 12})
    {
        FProofSnapshot Capacity = BotExpansionPrepared(EProofUnitKind::HQ);
        Capacity.Players[0].Credits = 250;
        for (int32 Index = 2; Index < Live; ++Index)
            BotExpansionAdd(Capacity, 300 + Index, 1, EProofUnitKind::HQ, FVector(4000, Index * 400, 0));
        FProofSkirmishBot Bot;
        const TArray<FProofCommand> Batch = Bot.Decide(Capacity, 1, 0.0);
        TestEqual(TEXT("Same-batch reservation charges both refinery records before factory capacity"),
            Batch.Num(), Live == 9 ? 2 : (Live == 12 ? 0 : 1));
        if (Live < 12 && Batch.Num() > 0)
            TestTrue(TEXT("Capacity preserves exact first construction type and sequence"), Batch[0].Sequence == 1
                && Batch[0].Type == (Live == 11 ? EProofCommandType::BuildFactory : EProofCommandType::BuildRefinery));
        if (Live == 9 && Batch.Num() == 2)
            TestTrue(TEXT("Three available records permit exactly both constructions"),
                Batch[1].Type == EProofCommandType::BuildFactory && Batch[1].Sequence == 2);
    }
    FProofSnapshot Reserved = BotExpansionPrepared(EProofUnitKind::HQ);
    Reserved.Players[0].Credits = 250;
    for (int32 Index = 1; Index < 8; ++Index)
        BotExpansionAdd(Reserved, 200 + Index, 3, EProofUnitKind::Factory, FVector(2000, 0, 0) + BotExpansionOffsets[Index]);
    FVector MinerSpawn = FVector::ZeroVector;
    TestTrue(TEXT("Reservation fixture has deterministic grant spawn"),
        FProofCommandModel::FindSkirmishSpawn(Reserved, FVector(2350, 0, 0), MinerSpawn)
        && MinerSpawn == FVector(2650, 0, 0));
    FProofSkirmishBot ReservedBot;
    const TArray<FProofCommand> ReservedBatch = ReservedBot.Decide(Reserved, 1, 0.0);
    TestTrue(TEXT("Two-build batch reserves structure and miner before finding factory"), ReservedBatch.Num() == 2
        && BotExpansionIntent(ReservedBatch[0], EProofCommandType::BuildRefinery, 1, FVector(2350, 0, 0))
        && BotExpansionIntent(ReservedBatch[1], EProofCommandType::BuildFactory, 2, FVector(1400, 0, 0)));
    if (ReservedBatch.Num() == 2)
    {
        TestTrue(TEXT("Factory avoids otherwise-clear +600 offset occupied by planned miner footprint"),
            FVector::DistSquared(FVector(2600, 0, 0), MinerSpawn) < 260.0 * 260.0
            && FVector::DistSquared(ReservedBatch[1].Destination, MinerSpawn) >= 260.0 * 260.0
            && FVector::DistSquared(ReservedBatch[0].Destination, ReservedBatch[1].Destination) >= 250.0 * 250.0);
    }
    TestEqual(TEXT("Reservations never debit real snapshot credits"), Reserved.Players[0].Credits, 250);
    TestEqual(TEXT("Decision interval is unchanged"), ReservedBot.Decide(Reserved, 1, 0.5).Num(), 0);
    for (int32 Index = 8; Index < 12; ++Index)
        BotExpansionAdd(Reserved, 200 + Index, 3, EProofUnitKind::Factory, FVector(2000, 0, 0) + BotExpansionOffsets[Index]);
    FProofSkirmishBot NoChainBot;
    const TArray<FProofCommand> NoChain = NoChainBot.Decide(Reserved, 1, 0.0);
    TestTrue(TEXT("Newly reserved refinery cannot become another anchor in the same decision"), NoChain.Num() == 1
        && BotExpansionIntent(NoChain[0], EProofCommandType::BuildRefinery, 1, FVector(2350, 0, 0)));
    FProofSnapshot WouldChain = Reserved;
    BotExpansionAdd(WouldChain, 900, 1, EProofUnitKind::Refinery, FVector(2350, 0, 0));
    TestTrue(TEXT("No-chain fixture actually has a clear site around the uncommitted refinery"),
        BotExpansionSiteClear(WouldChain, FVector(2350, 600, 0), false));

    FProofSnapshot Budget = BotExpansionPrepared(EProofUnitKind::HQ);
    Budget.Players[0].Credits = 250;
    for (int32 Id : {62, 60, 61})
        BotExpansionAdd(Budget, Id, 1, EProofUnitKind::Combat, FVector(5000, Id * 20, 0));
    FProofSkirmishBot BudgetBot;
    const TArray<FProofCommand> BudgetBatch = BudgetBot.Decide(Budget, 1, 0.0);
    TestEqual(TEXT("Construction fallback retains exact four-command budget"), BudgetBatch.Num(), 4);
    if (BudgetBatch.Num() != 4) { return false; }
    TestTrue(TEXT("Budget prioritizes recovery then factory then sorted defenders"),
        BudgetBatch[0].Type == EProofCommandType::BuildRefinery && BudgetBatch[1].Type == EProofCommandType::BuildFactory
        && BudgetBatch[2].Type == EProofCommandType::AreaGuard && BudgetBatch[2].UnitId == 60
        && BudgetBatch[3].Type == EProofCommandType::AreaGuard && BudgetBatch[3].UnitId == 61);
    for (int32 Index = 0; Index < BudgetBatch.Num(); ++Index)
        TestEqual(TEXT("Every emitted sequence is contiguous"), BudgetBatch[Index].Sequence, Index + 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBotBaseExpansionPublicTest,
    "RedalertProof.Skirmish.BotBaseExpansion.PublicRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBotBaseExpansionPublicTest::RunTest(const FString&)
{
    // Scripted custom-map histories, not claims that ordinary autonomous bot
    // matches stall. No model internals or prepared-snapshot edits are used.
    for (bool bRecoverMiner : {true, false})
    {
        FBotExpansionPublicFixture Fixture;
        Fixture.Model.StartSkirmish();
        if (!TestTrue(TEXT("Seed actual opposing players"),
            Fixture.Model.SeedSkirmishPlayer(1, 0, Fixture.HQ)
            && Fixture.Model.SeedSkirmishPlayer(100, 1, FVector(1400, 0, 0)))) { return false; }
        if (!TestTrue(TEXT("Pay for actual expanded refinery at the 800cm HQ boundary"),
            Fixture.Submit(1, EProofCommandType::BuildRefinery, Fixture.HQ + FVector(800, 0, 0)))) { return false; }
        const int32 AnchorId = Fixture.Id(1, EProofUnitKind::Refinery);
        for (int32 Tick = 0; Tick < 400 && Fixture.Player().TotalGathered < 450; ++Tick) { Fixture.Advance(); }
        if (!TestEqual(TEXT("Setup earns exactly 450 through real extraction and deposit"), Fixture.Player().TotalGathered, 450)) { return false; }
        if (!TestTrue(TEXT("Stop actual miners before paid ring construction"), Fixture.StopMiners())) { return false; }
        for (const FVector& Offset : BotExpansionRing)
            if (!TestTrue(TEXT("Pay for each of the four actual HQ-ring blockers"), Fixture.Submit(1,
                bRecoverMiner ? EProofCommandType::BuildFactory : EProofCommandType::BuildRefinery,
                Fixture.HQ + Offset))) { return false; }
        if (!TestTrue(TEXT("Stop each newly granted miner before scripted hostile loss"), Fixture.StopMiners())) { return false; }
        if (!TestTrue(TEXT("Enemy pays for factory and attacker"),
            Fixture.Submit(100, EProofCommandType::BuildFactory, FVector(1050, 0, 0))
            && Fixture.Submit(100, EProofCommandType::Produce, FVector::ZeroVector, Fixture.Id(100, EProofUnitKind::Factory)))) { return false; }
        for (int32 Tick = 0; Tick < 5; ++Tick) { Fixture.Advance(); }
        if (!TestTrue(TEXT("Enemy attacker completes real paid production"), Fixture.Model.GetSnapshot().ProductionJobs.IsEmpty())) return false;
        const FProofSnapshot BeforeLoss = Fixture.Model.GetSnapshot();
        int32 LostMiners = 0;
        for (const FProofUnitRecord& Unit : BeforeLoss.Units)
        {
            if (Unit.OwnerId != 1 || Unit.Kind != EProofUnitKind::Harvester || Unit.HitPoints <= 0.f) { continue; }
            if (!TestTrue(TEXT("Real movement and hostile damage kill the selected miner"), Fixture.KillMiner(Unit.UnitId))) { return false; }
            ++LostMiners;
            if (!bRecoverMiner) { break; }
        }
        TestEqual(TEXT("Exact scripted miner loss count"), LostMiners, bRecoverMiner ? 2 : 1);
        const FProofSnapshot Before = Fixture.Model.GetSnapshot();
        TestTrue(TEXT("Recovery remains in a live public match"), Before.MatchPhase == EProofMatchPhase::Playing);
        TestEqual(TEXT("Exact pre-recovery live record count"), BotExpansionLive(Before, 1), bRecoverMiner ? 6 : 11);
        TestEqual(TEXT("Exact pre-recovery miner count"), BotExpansionCount(Before, 1, EProofUnitKind::Harvester), bRecoverMiner ? 0 : 5);
        TestEqual(TEXT("Exact earned pre-recovery balance"), Fixture.Player().Credits, bRecoverMiner ? 100 : 300);
        TestTrue(TEXT("Public setup conserves all ore, cargo and earned credits"), Fixture.Conserved());
        for (const FVector& Offset : BotExpansionOffsets)
            TestTrue(TEXT("All twelve former HQ-only sites are blocked"), !BotExpansionSiteClear(Before, Fixture.HQ + Offset, bRecoverMiner));
        const FVector Expanded = Fixture.Unit(AnchorId).Position + BotExpansionOffsets[0];
        TestTrue(TEXT("Expanded candidate is 1150cm from HQ and 350cm from committed refinery"),
            FVector::Dist(Expanded, Fixture.HQ) == 1150.0
            && FVector::Dist(Expanded, Fixture.Unit(AnchorId).Position) == 350.0
            && BotExpansionSiteClear(Before, Expanded, bRecoverMiner));
        FProofSkirmishBot Bot;
        const FString BeforeDigest = Before.ComputeDigest();
        const TArray<FProofCommand> Batch = Bot.Decide(Before, 1, Fixture.Seconds);
        TestEqual(TEXT("Exact public recovery batch count"), Batch.Num(), bRecoverMiner ? 1 : 2);
        if (Batch.Num() != (bRecoverMiner ? 1 : 2)) { return false; }
        TestTrue(TEXT("Candidate planner chooses the exact accepted fallback intent"),
            BotExpansionIntent(Batch[0], bRecoverMiner ? EProofCommandType::BuildRefinery : EProofCommandType::BuildFactory, 1, Expanded));
        if (!bRecoverMiner)
            TestTrue(TEXT("Factory recovery preserves following Gather and contiguous sequence"),
                Batch[1].Type == EProofCommandType::Gather && Batch[1].Sequence == 2
                && Batch[1].UnitId == Fixture.Id(1, EProofUnitKind::Harvester) && Batch[1].TargetUnitId == 1);
        TestEqual(TEXT("Planner cannot mutate the public model"), Fixture.Model.GetSnapshot().ComputeDigest(), BeforeDigest);
        int32 MaxId = 0;
        for (const FProofUnitRecord& Unit : Before.Units) { MaxId = FMath::Max(MaxId, Unit.UnitId); }
        FVector ExpectedSpawn = FVector::ZeroVector;
        if (bRecoverMiner)
            TestTrue(TEXT("Public recovery grant has a deterministic clear spawn"), FProofCommandModel::FindSkirmishSpawn(Before, Expanded, ExpectedSpawn));
        // Setup was scripted under this owner's established sequence stream.
        // A fresh planner emits intents starting at 1; this explicit adapter
        // stamps the next actual owner sequence. This is not autonomous takeover.
        const int32 EstablishedSequence = Fixture.OwnerSequence;
        for (int32 Index = 0; Index < Batch.Num(); ++Index)
        {
            FProofCommand Command = Batch[Index];
            TestEqual(TEXT("Planner intent sequence is contiguous before adaptation"), Command.Sequence, Index + 1);
            Command.Sequence = ++Fixture.OwnerSequence;
            if (!TestTrue(TEXT("Adapted planner intent accepted through public Submit"), Fixture.SubmitIntent(1, Command))) { return false; }
            TestEqual(TEXT("Adapter uses precisely the next established owner sequence"), Command.Sequence, EstablishedSequence + Index + 1);
        }
        const FProofSnapshot After = Fixture.Model.GetSnapshot();
        TestEqual(TEXT("Recovery makes exactly the normal record grants"), After.Units.Num() - Before.Units.Num(), bRecoverMiner ? 2 : 1);
        TestEqual(TEXT("Recovery has exact ordinary revision increments"), After.Revision - Before.Revision, 3);
        TestEqual(TEXT("Recovery debits exact ordinary cost"), Fixture.Player().Credits, bRecoverMiner ? 0 : 150);
        TestEqual(TEXT("Recovery cannot mint gathered income"), Fixture.Player().TotalGathered, 450);
        TestTrue(TEXT("New structure has exact next ID, owner, kind and site"),
            Fixture.Unit(MaxId + 1).OwnerId == 1 && Fixture.Unit(MaxId + 1).Position == Expanded
            && Fixture.Unit(MaxId + 1).Kind == (bRecoverMiner ? EProofUnitKind::Refinery : EProofUnitKind::Factory)
            && Fixture.Unit(MaxId + 1).HitPoints == 200.f);
        for (const FProofUnitRecord& Prior : Before.Units)
        {
            const FProofUnitRecord Current = Fixture.Unit(Prior.UnitId);
            TestTrue(TEXT("Submitting recovery preserves every existing unit's owner, kind, position, health and cargo"),
                Current.OwnerId == Prior.OwnerId && Current.Kind == Prior.Kind && Current.Position == Prior.Position
                && Current.HitPoints == Prior.HitPoints && Current.Cargo == Prior.Cargo);
        }
        TestTrue(TEXT("Paid fallback preserves economic conservation"), Fixture.Conserved());
        if (bRecoverMiner)
        {
            const int32 MinerId = MaxId + 2;
            TestTrue(TEXT("Exactly one empty, healthy own miner is granted at shared spawn"),
                BotExpansionCount(After, 1, EProofUnitKind::Harvester) == 1
                && Fixture.Unit(MinerId).OwnerId == 1 && Fixture.Unit(MinerId).Kind == EProofUnitKind::Harvester
                && Fixture.Unit(MinerId).Cargo == 0 && Fixture.Unit(MinerId).HitPoints == 100.f
                && Fixture.Unit(MinerId).Position == ExpectedSpawn);
            Fixture.Advance();
            TestTrue(TEXT("Granted replacement moves by actual model auto-gather"),
                Fixture.Unit(MinerId).Position != ExpectedSpawn && Fixture.Unit(MinerId).LastTravelDirection != FVector::ZeroVector);
            for (int32 Tick = 0; Tick < 200 && Fixture.Player().TotalGathered < 500; ++Tick) { Fixture.Advance(); }
            TestEqual(TEXT("Replacement deposits the next actual 50 ore"), Fixture.Player().TotalGathered, 500);
            TestEqual(TEXT("Deposit restores exactly 50 credits after paid recovery"), Fixture.Player().Credits, 50);
            TestTrue(TEXT("Movement, extraction and deposit preserve conservation"), Fixture.Conserved());
            const TArray<FProofCommand> Later = Bot.Decide(Fixture.Model.GetSnapshot(), 1, Fixture.Seconds);
            TestTrue(TEXT("Recovered miner rejoins normal Gather followed by affordable production"), Later.Num() == 2
                && Later[0].Type == EProofCommandType::Gather && Later[0].UnitId == MinerId && Later[0].Sequence == 2
                && Later[1].Type == EProofCommandType::Produce && Later[1].Sequence == 3);
        }
        else
        {
            TestEqual(TEXT("Factory grant reaches but cannot exceed owner cap"), BotExpansionLive(After, 1), FProofCommandModel::SkirmishRecordCap);
            if (!TestTrue(TEXT("Stop gather and free one slot through another real hostile loss"),
                Fixture.StopMiners() && Fixture.KillMiner(Fixture.Id(1, EProofUnitKind::Harvester)))) { return false; }
            const FProofSnapshot BeforeProduction = Fixture.Model.GetSnapshot();
            const TArray<FProofCommand> Later = Bot.Decide(BeforeProduction, 1, Fixture.Seconds);
            TestTrue(TEXT("Recovered factory enables ordinary Gather then paid production"), Later.Num() == 2
                && Later[0].Type == EProofCommandType::Gather && Later[0].Sequence == 3
                && Later[1].Type == EProofCommandType::Produce && Later[1].Sequence == 4
                && Later[1].UnitId == MaxId + 1);
            if (Later.Num() != 2) { return false; }
            const int32 Credits = Fixture.Player().Credits;
            for (const FProofCommand& Intent : Later)
            {
                FProofCommand Command = Intent;
                Command.Sequence = ++Fixture.OwnerSequence;
                if (!TestTrue(TEXT("Post-recovery planned production accepted publicly"), Fixture.SubmitIntent(1, Command))) { return false; }
            }
            TestEqual(TEXT("Recovered factory job starts without an instant unit"),
                BotExpansionCount(Fixture.Model.GetSnapshot(), 1, EProofUnitKind::Combat), 0);
            TestEqual(TEXT("Recovered factory job starts without a debit"), Fixture.Player().Credits, Credits);
            const int32 GatheredBeforeProduction = Fixture.Player().TotalGathered;
            for (int32 Tick = 0; Tick < 5; ++Tick) { Fixture.Advance(); }
            TestTrue(TEXT("Recovered factory job completes after five simulated seconds"), Fixture.Model.GetSnapshot().ProductionJobs.IsEmpty());
            TestEqual(TEXT("Recovered factory grants exactly one actual combat unit"),
                BotExpansionCount(Fixture.Model.GetSnapshot(), 1, EProofUnitKind::Combat), 1);
            TestEqual(TEXT("Production spends exactly 50 actual credits alongside actual Gather deposits"), Fixture.Player().Credits,
                Credits + Fixture.Player().TotalGathered - GatheredBeforeProduction - FProofCommandModel::ProductionCost);
            TestEqual(TEXT("Production adds exactly one record"), Fixture.Model.GetSnapshot().Units.Num(), BeforeProduction.Units.Num() + 1);
            TestEqual(TEXT("Production restores exactly the twelve-record cap"), BotExpansionLive(Fixture.Model.GetSnapshot(), 1), 12);
            TestTrue(TEXT("Post-recovery paid production preserves economic conservation"), Fixture.Conserved());
        }
    }
    return true;
}
#endif
