#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Skirmish/ProofSkirmishBot.h"
#include <limits>

namespace
{
FProofPlayerRecord Player(int32 OwnerId, int32 TeamId, int32 Credits)
{
    FProofPlayerRecord P;
    P.OwnerId = OwnerId;
    P.TeamId = TeamId;
    P.Credits = Credits;
    return P;
}

FProofUnitRecord Unit(int32 UnitId, int32 OwnerId, EProofUnitKind Kind, const FVector& Position)
{
    FProofUnitRecord U;
    U.UnitId = UnitId;
    U.OwnerId = OwnerId;
    U.Kind = Kind;
    U.Position = Position;
    return U;
}

FProofSnapshot SmallSnapshot()
{
    FProofSnapshot S;
    S.bSkirmish = true;
    S.MatchPhase = EProofMatchPhase::Playing;
    S.Players.Add(Player(100, 1, 350));
    S.Players.Add(Player(1, 0, 350));
    S.Units.Add(Unit(1, 100, EProofUnitKind::HQ, FVector::ZeroVector));
    S.Units.Add(Unit(2, 100, EProofUnitKind::Harvester, FVector(100, 0, 0)));
    S.Units.Add(Unit(3, 1, EProofUnitKind::HQ, FVector(2000, 0, 0)));
    S.Units.Add(Unit(4, 1, EProofUnitKind::Combat, FVector(1000, 0, 0)));
    FProofResourceRecord Ore;
    Ore.ResourceId = 1;
    Ore.Position = FVector(200, 200, 0);
    Ore.Remaining = 500;
    S.Resources.Add(Ore);
    return S;
}

int32 ProofBotAreaGuardCount(const TArray<FProofCommand>& Commands)
{
    int32 Count = 0;
    for (const FProofCommand& Command : Commands)
        Count += Command.Type == EProofCommandType::AreaGuard ? 1 : 0;
    return Count;
}

bool SameCommands(const TArray<FProofCommand>& A, const TArray<FProofCommand>& B)
{
    if (A.Num() != B.Num()) { return false; }
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        if (A[Index].Sequence != B[Index].Sequence || A[Index].Type != B[Index].Type
            || A[Index].UnitId != B[Index].UnitId || A[Index].TargetUnitId != B[Index].TargetUnitId
            || !A[Index].Destination.Equals(B[Index].Destination))
        {
            return false;
        }
    }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishBotPlannerTest, "RedalertProof.Skirmish.BotPlanner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishBotPlannerTest::RunTest(const FString&)
{
    FProofSnapshot S = SmallSnapshot();
    FProofSkirmishBot A;
    const TArray<FProofCommand> Open = A.Decide(S, 100, 0.0);
    TestEqual(TEXT("Builds one refinery and one factory from starting credits"), Open.Num(), 2);
    if (Open.Num() != 2) { return false; }
    TestEqual(TEXT("First sequence"), Open[0].Sequence, 1);
    TestEqual(TEXT("Second sequence"), Open[1].Sequence, 2);
    TestTrue(TEXT("Refinery first"), Open[0].Type == EProofCommandType::BuildRefinery);
    TestTrue(TEXT("Factory second"), Open[1].Type == EProofCommandType::BuildFactory);
    TestTrue(TEXT("Structures have distinct sites near HQ"),
        FVector::Dist(Open[0].Destination, Open[1].Destination) >= 250.0
        && Open[0].Destination.Size() <= 800.0 && Open[1].Destination.Size() <= 800.0);
    TestEqual(TEXT("Decision cadence bounds duplicate calls"), A.Decide(S, 100, 0.5).Num(), 0);

    FProofSnapshot Permuted = SmallSnapshot();
    Permuted.Players.Swap(0, 1);
    Permuted.Units.Swap(0, 3);
    Permuted.Units.Swap(1, 2);
    FProofSkirmishBot B;
    TestTrue(TEXT("Unsorted records yield the same commands"), SameCommands(Open, B.Decide(Permuted, 100, 0.0)));

    S.Units.Add(Unit(5, 100, EProofUnitKind::Refinery, Open[0].Destination));
    S.Units.Add(Unit(6, 100, EProofUnitKind::Factory, Open[1].Destination));
    S.Units.Add(Unit(7, 100, EProofUnitKind::Combat, FVector(200, 0, 0)));
    S.Players[0].Credits = 100;
    const TArray<FProofCommand> Economy = A.Decide(S, 100, 1.0);
    TestEqual(TEXT("Gather and produce precede one defensive guard in the bounded batch"), Economy.Num(), 3);
    if (Economy.Num() == 3)
    {
        TestTrue(TEXT("Harvester gathers a real resource"), Economy[0].Type == EProofCommandType::Gather
            && Economy[0].UnitId == 2 && Economy[0].TargetUnitId == 1);
        TestTrue(TEXT("Factory produces a combat unit"), Economy[1].Type == EProofCommandType::Produce && Economy[1].UnitId == 6);
        TestEqual(TEXT("Sequence continues across decisions"), Economy[0].Sequence, 3);
        TestEqual(TEXT("Sequence remains contiguous"), Economy[1].Sequence, 4);
        TestTrue(TEXT("Pre-grace Combat receives one guard after economy commands"),
            Economy[2].Type == EProofCommandType::AreaGuard && Economy[2].UnitId == 7 && Economy[2].Sequence == 5);
    }
    S.Players[0].Credits = 0;
    TestEqual(TEXT("Nonfinite time produces no commands"), A.Decide(S, 100, std::numeric_limits<double>::quiet_NaN()).Num(), 0);
    const TArray<FProofCommand> BeforeGrace = A.Decide(S, 100, 19.0);
    TestTrue(TEXT("Before grace only gathering continues"), BeforeGrace.Num() == 1
        && BeforeGrace[0].Type == EProofCommandType::Gather);
    const TArray<FProofCommand> Attack = A.Decide(S, 100, 20.0);
    TestEqual(TEXT("Grace elapsed: one combat attack"), Attack.Num(), 1);
    if (Attack.Num() == 1)
    {
        TestTrue(TEXT("Attacks nearest opposing-team live unit"), Attack[0].Type == EProofCommandType::Attack
            && Attack[0].UnitId == 7 && Attack[0].TargetUnitId == 4);
    }
    TestEqual(TEXT("Backward time emits nothing"), A.Decide(S, 100, 18.0).Num(), 0);
    S.Units[3].HitPoints = 0.f;
    const TArray<FProofCommand> Retarget = A.Decide(S, 100, 21.0);
    TestEqual(TEXT("Dead target is replaced"), Retarget.Num(), 1);
    if (Retarget.Num() == 1) { TestEqual(TEXT("Retargets live enemy HQ"), Retarget[0].TargetUnitId, 3); }
    S.MatchPhase = EProofMatchPhase::Finished;
    TestEqual(TEXT("Terminal game emits no commands"), A.Decide(S, 100, 22.0).Num(), 0);
    A.Reset();
    TestTrue(TEXT("Reset restores first command sequence"), SameCommands(Open, A.Decide(SmallSnapshot(), 100, 0.0)));

    FProofSnapshot AtCap = SmallSnapshot();
    AtCap.Units.Add(Unit(5, 100, EProofUnitKind::Refinery, FVector(350, 0, 0)));
    AtCap.Units.Add(Unit(6, 100, EProofUnitKind::Factory, FVector(-350, 0, 0)));
    for (int32 Id = 7; Id <= 14; ++Id)
    {
        AtCap.Units.Add(Unit(Id, 100, EProofUnitKind::Combat, FVector(-350, 0, 0)));
    }
    AtCap.Resources.Reset();
    AtCap.Players[0].Credits = 500;
    FProofSkirmishBot CappedBot;
    const TArray<FProofCommand> CappedDefense = CappedBot.Decide(AtCap, 100, 0.0);
    TestTrue(TEXT("Twelve live records prevent production but allow four defensive guards"),
        CappedDefense.Num() == 4 && ProofBotAreaGuardCount(CappedDefense) == 4);

    // An opponent alternates which live target is nearest each second. Without
    // fair rotation the first four combat IDs retarget forever and the other
    // four never receive an order, despite remaining eligible throughout.
    FProofSkirmishBot FairBot;
    FProofSkirmishBot SameFairBot;
    const TArray<FProofCommand> FairDefense = FairBot.Decide(AtCap, 100, 0.0);
    TestTrue(TEXT("Fairness fixture starts with four guards before unchanged attack grace"),
        FairDefense.Num() == 4 && ProofBotAreaGuardCount(FairDefense) == 4);
    SameFairBot.Decide(AtCap, 100, 0.0);
    TSet<int32> OrderedCombatIds;
    TArray<FProofCommand> FirstFairBatch;
    for (int32 Decision = 0; Decision < 4; ++Decision)
    {
        AtCap.Units[3].Position = FVector(Decision % 2 == 0 ? 900 : 2100, 0, 0);
        const TArray<FProofCommand> Batch = FairBot.Decide(AtCap, 100, 20.0 + Decision);
        TestTrue(TEXT("Fair scheduling remains deterministic"),
            SameCommands(Batch, SameFairBot.Decide(AtCap, 100, 20.0 + Decision)));
        TestTrue(TEXT("Moving-target decision remains bounded"), Batch.Num() <= 4);
        for (const FProofCommand& Command : Batch)
        {
            TestTrue(TEXT("Capped fixture emits only attacks"), Command.Type == EProofCommandType::Attack);
            OrderedCombatIds.Add(Command.UnitId);
        }
        if (Decision == 0) { FirstFairBatch = Batch; }
        if (Decision == 1)
        {
            TestEqual(TEXT("All eight combat units get orders within two changing-target decisions"), OrderedCombatIds.Num(), 8);
        }
    }
    FairBot.Reset();
    FairBot.Decide(AtCap, 100, 0.0);
    AtCap.Units[3].Position = FVector(900, 0, 0);
    TestTrue(TEXT("Reset restores fair scheduling cursor and sequence"),
        SameCommands(FirstFairBatch, FairBot.Decide(AtCap, 100, 20.0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishBotModelTest, "RedalertProof.Skirmish.BotModelPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishBotModelTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    TestTrue(TEXT("Human skirmish seed"), Model.SeedSkirmishPlayer(1, 0, FVector(1800, 0, 0)));
    TestTrue(TEXT("Bot skirmish seed"), Model.SeedSkirmishPlayer(100, 1, FVector::ZeroVector));
    FProofSkirmishBot Bot;
    int32 Builds = 0, Gathers = 0, Produces = 0, Attacks = 0;
    int32 StarterMinerId = INDEX_NONE;
    int32 GrantedMinerId = INDEX_NONE;
    bool bGrantedMinerGathered = false;
    for (const FProofUnitRecord& Record : Model.GetSnapshot().Units)
        if (Record.OwnerId == 100 && Record.Kind == EProofUnitKind::Harvester) StarterMinerId = Record.UnitId;
    bool bOreExtracted = false;
    int32 InitialOre = 0;
    for (const FProofResourceRecord& Resource : Model.GetSnapshot().Resources) { InitialOre += Resource.Remaining; }
    for (int32 Tick = 0; Tick < 180; ++Tick)
    {
        const FProofSnapshot Before = Model.GetSnapshot();
        const TArray<FProofCommand> Commands = Bot.Decide(Before, 100, static_cast<double>(Tick));
        TestTrue(TEXT("Bot command batch is bounded"), Commands.Num() <= 4);
        for (const FProofCommand& Command : Commands)
        {
            const FProofCommandResult Result = Model.Submit(100, Command);
            TestTrue(FString::Printf(TEXT("Bot command %d accepted (%s)"), Command.Sequence, *Result.Reason), Result.bAccepted);
            if (Result.bAccepted)
            {
                if (Command.Type == EProofCommandType::BuildRefinery || Command.Type == EProofCommandType::BuildFactory) { ++Builds; }
                if (Command.Type == EProofCommandType::BuildRefinery)
                {
                    for (const FProofUnitRecord& Record : Model.GetSnapshot().Units)
                        if (Record.OwnerId == 100 && Record.Kind == EProofUnitKind::Harvester && Record.UnitId != StarterMinerId)
                            GrantedMinerId = Record.UnitId;
                }
                if (Command.Type == EProofCommandType::Gather) { ++Gathers; }
                bGrantedMinerGathered |= Command.Type == EProofCommandType::Gather && Command.UnitId == GrantedMinerId;
                if (Command.Type == EProofCommandType::Produce) { ++Produces; }
                if (Command.Type == EProofCommandType::Attack) { ++Attacks; }
            }
        }
        Model.Advance(1.f);
        int32 RemainingOre = 0;
        for (const FProofResourceRecord& Resource : Model.GetSnapshot().Resources) { RemainingOre += Resource.Remaining; }
        bOreExtracted |= RemainingOre < InitialOre;
        if (Model.GetSnapshot().MatchPhase == EProofMatchPhase::Finished) { break; }
    }
    TestEqual(TEXT("Both structures were built by accepted commands"), Builds, 2);
    TestTrue(TEXT("Harvester received an accepted gather command"), Gathers > 0);
    TestTrue(TEXT("Refinery-granted miner was discovered and received accepted Gather"),
        GrantedMinerId != INDEX_NONE && GrantedMinerId != StarterMinerId && bGrantedMinerGathered);
    TestTrue(TEXT("Ore was actually extracted"), bOreExtracted);
    const FProofSnapshot Final = Model.GetSnapshot();
    const FProofPlayerRecord* BotPlayer = Final.Players.FindByPredicate(
        [](const FProofPlayerRecord& P) { return P.OwnerId == 100; });
    TestTrue(TEXT("Harvested ore was deposited as earned credits"), BotPlayer && BotPlayer->TotalGathered > 0);
    TestTrue(TEXT("Factory spent earned credits beyond two starting-credit units"), Produces > 2);
    TestTrue(TEXT("Combat units attacked through the model"), Attacks > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishBotRefineryPlanningTest, "RedalertProof.Skirmish.BotRefineryPlanning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishBotRefineryPlanningTest::RunTest(const FString&)
{
    const auto WithLiveCount = [](int32 Count)
    {
        FProofSnapshot S = SmallSnapshot();
        for (int32 Index = 2; Index < Count; ++Index)
            S.Units.Add(Unit(100 + Index, 100, EProofUnitKind::Combat, FVector(5000, Index * 300, 0)));
        return S;
    };
    FProofSkirmishBot TwoSlots;
    const TArray<FProofCommand> AtTen = TwoSlots.Decide(WithLiveCount(10), 100, 0.0);
    TestTrue(TEXT("Two free slots plan refinery+miner, leaving no slot for factory"), AtTen.Num() == 4
        && AtTen[0].Type == EProofCommandType::BuildRefinery && ProofBotAreaGuardCount(AtTen) == 3);
    FProofSkirmishBot OneSlot;
    const TArray<FProofCommand> AtEleven = OneSlot.Decide(WithLiveCount(11), 100, 0.0);
    TestTrue(TEXT("One free slot cannot plan refinery+miner but still permits factory"), AtEleven.Num() == 4
        && AtEleven[0].Type == EProofCommandType::BuildFactory && ProofBotAreaGuardCount(AtEleven) == 3);
    FProofSkirmishBot ThreeSlots;
    const TArray<FProofCommand> AtNine = ThreeSlots.Decide(WithLiveCount(9), 100, 0.0);
    TestTrue(TEXT("Three free slots permit refinery+miner and factory together"), AtNine.Num() == 4
        && AtNine[0].Type == EProofCommandType::BuildRefinery && AtNine[1].Type == EProofCommandType::BuildFactory
        && ProofBotAreaGuardCount(AtNine) == 2);

    // Force the first grant to (50,300), next to the ordinary third factory
    // candidate (0,350). Only this newly planned miner changes factory planning.
    FProofSnapshot Reserved = SmallSnapshot();
    Reserved.Units.Add(Unit(20, 1, EProofUnitKind::HQ, FVector(-350, 0, 0)));
    Reserved.Units.Add(Unit(21, 1, EProofUnitKind::Combat, FVector(650, 0, 0)));
    Reserved.Units.Add(Unit(22, 1, EProofUnitKind::Combat, FVector(350, 300, 0)));
    Reserved.Units.Add(Unit(23, 1, EProofUnitKind::Combat, FVector(350, -300, 0)));
    Reserved.Units.Add(Unit(24, 1, EProofUnitKind::Combat, FVector(650, 300, 0)));
    FProofSkirmishBot ReservationBot;
    const TArray<FProofCommand> ReservedCommands = ReservationBot.Decide(Reserved, 100, 0.0);
    TestEqual(TEXT("Reservation fixture still plans both structures"), ReservedCommands.Num(), 2);
    if (ReservedCommands.Num() == 2)
    {
        FVector MinerPosition;
        TestTrue(TEXT("Shared spawn preflight finds grant"),
            FProofCommandModel::FindSkirmishSpawn(Reserved, ReservedCommands[0].Destination, MinerPosition));
        TestTrue(TEXT("Fixture grant occupies the expected potential factory footprint"), MinerPosition.Equals(FVector(50, 300, 0)));
        TestTrue(TEXT("Planned factory preserves granted miner's 260cm clearance"),
            FVector::DistSquared(MinerPosition, ReservedCommands[1].Destination) >= 260.0 * 260.0);
        TestTrue(TEXT("Factory skips occupied third site and uses fourth"), ReservedCommands[1].Destination.Equals(FVector(0, -350, 0)));
    }

    // Cover the bounded spawn search around all fixed near-HQ build sites.
    // These are foreign mobile blockers, so factory's existing structure-only
    // placement policy can still find a site; only the refinery grant lacks room.
    FProofSnapshot Blocked = SmallSnapshot();
    for (int32 Owner = 2; Owner <= 11; ++Owner) Blocked.Players.Add(Player(Owner, 0, 0));
    int32 BlockerId = 10000;
    int32 ForeignSlot = 2; // Existing owner 1 already has two records.
    for (int32 X = -1500; X <= 1500; X += 300)
        for (int32 Y = -1500; Y <= 1500; Y += 300)
            Blocked.Units.Add(Unit(BlockerId++, 1 + ForeignSlot++ / 12, EProofUnitKind::Combat, FVector(X, Y, 0)));
    // Check the fixture itself against the shared search. A sparse grid can
    // leave diagonal holes even when cardinal spawn candidates are blocked.
    const FVector BuildSites[] = {
        FVector(350, 0, 0), FVector(-350, 0, 0), FVector(0, 350, 0), FVector(0, -350, 0),
        FVector(350, 350, 0), FVector(-350, 350, 0), FVector(350, -350, 0), FVector(-350, -350, 0),
        FVector(600, 0, 0), FVector(-600, 0, 0), FVector(0, 600, 0), FVector(0, -600, 0)
    };
    bool bAllSitesBlocked = true;
    for (const FVector& Site : BuildSites)
    {
        FVector Spawn;
        const bool bHasSpawn = FProofCommandModel::FindSkirmishSpawn(Blocked, Site, Spawn);
        TestFalse(FString::Printf(TEXT("No-space fixture exhausts miner search at %s"), *Site.ToString()), bHasSpawn);
        bAllSitesBlocked &= !bHasSpawn;
    }
    if (!bAllSitesBlocked) { return false; }
    FProofSkirmishBot BlockedBot;
    const TArray<FProofCommand> NoSpace = BlockedBot.Decide(Blocked, 100, 0.0);
    TestTrue(TEXT("No miner spawn space suppresses refinery without blocking affordable factory"), NoSpace.Num() == 1
        && NoSpace[0].Type == EProofCommandType::BuildFactory);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishBotMinerRotationTest, "RedalertProof.Skirmish.BotMinerRotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishBotMinerRotationTest::RunTest(const FString&)
{
    FProofSnapshot S = SmallSnapshot();
    S.Players[0].Credits = 500;
    S.Units.Add(Unit(5, 100, EProofUnitKind::Refinery, FVector(350, 0, 0)));
    S.Units.Add(Unit(6, 100, EProofUnitKind::Factory, FVector(-350, 0, 0)));
    for (int32 Id = 7; Id <= 9; ++Id)
        S.Units.Add(Unit(Id, 100, EProofUnitKind::Harvester, FVector(Id * 30, 100, 0)));
    S.Units.Add(Unit(10, 100, EProofUnitKind::Combat, FVector::ZeroVector));
    S.Units.Add(Unit(11, 100, EProofUnitKind::Combat, FVector::ZeroVector));
    S.Units.Add(Unit(12, 100, EProofUnitKind::Harvester, FVector::ZeroVector));
    S.Units.Last().HitPoints = 0.f;
    S.Units.Add(Unit(13, 1, EProofUnitKind::Harvester, FVector(4000, 0, 0)));
    FProofResourceRecord EqualOre = S.Resources[0];
    EqualOre.ResourceId = 2;
    S.Resources.Add(EqualOre);
    const FProofSnapshot Initial = S;
    FProofSkirmishBot Bot, PermutedBot;
    TSet<int32> OrderedMiners;
    TArray<FProofCommand> First;
    for (int32 Decision = 0; Decision < 6; ++Decision)
    {
        const double Now = Decision == 0 ? 0.0 : 19.0 + Decision;
        S.Units[3].Position = FVector(Decision % 2 == 0 ? 900 : 2100, 0, 0);
        FProofSnapshot Permuted = S;
        Permuted.Units.Swap(0, Permuted.Units.Num() - 1);
        Permuted.Units.Swap(1, Permuted.Units.Num() - 2);
        Permuted.Resources.Swap(0, 1);
        const TArray<FProofCommand> Commands = Bot.Decide(S, 100, Now);
        TestTrue(TEXT("Miner scheduling and ore ties are independent of record order"),
            SameCommands(Commands, PermutedBot.Decide(Permuted, 100, Now)));
        TestTrue(TEXT("Multi-miner decisions retain four-command budget"), Commands.Num() <= 4);
        int32 Gathers = 0, Produces = 0, Attacks = 0;
        for (const FProofCommand& Command : Commands)
        {
            if (Command.Type == EProofCommandType::Gather)
            {
                ++Gathers;
                OrderedMiners.Add(Command.UnitId);
                TestTrue(TEXT("Only owned live miners receive Gather"), Command.UnitId == 2 || (Command.UnitId >= 7 && Command.UnitId <= 9));
                TestEqual(TEXT("Equal-distance ore uses stable resource ID"), Command.TargetUnitId, 1);
            }
            Produces += Command.Type == EProofCommandType::Produce ? 1 : 0;
            Attacks += Command.Type == EProofCommandType::Attack ? 1 : 0;
        }
        TestTrue(TEXT("At most one Gather leaves room for other command roles"), Gathers <= 1);
        if (Decision == 5) TestEqual(TEXT("No miner retries before the existing five-second interval"), Gathers, 0);
        TestEqual(TEXT("Affordable production progresses alongside miners"), Produces, 1);
        if (Decision > 0) TestEqual(TEXT("Both combat units progress against changing targets despite miners"), Attacks, 2);
        if (Decision == 0) First = Commands;
        if (Decision == 3) TestEqual(TEXT("All four live own miners receive commands within four decisions"), OrderedMiners.Num(), 4);
    }
    Bot.Reset();
    FProofSnapshot Restart = Initial;
    Restart.Units[3].Position = FVector(900, 0, 0);
    TestTrue(TEXT("Reset restores miner rotation and command sequences"), SameCommands(First, Bot.Decide(Restart, 100, 0.0)));
    return true;
}
#endif
