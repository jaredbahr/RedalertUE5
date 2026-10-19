#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Skirmish/ProofSkirmishBot.h"

namespace
{
FProofSnapshot MinerLossSnapshot()
{
    FProofSnapshot Snapshot;
    Snapshot.bSkirmish = true;
    Snapshot.MatchPhase = EProofMatchPhase::Playing;
    FProofPlayerRecord Owner;
    Owner.OwnerId = 100;
    Owner.TeamId = 1;
    Owner.Credits = 100;
    Snapshot.Players.Add(Owner);
    const EProofUnitKind Kinds[] = {EProofUnitKind::HQ, EProofUnitKind::Refinery,
        EProofUnitKind::Factory, EProofUnitKind::Harvester};
    const FVector Positions[] = {FVector::ZeroVector, FVector(350, 0, 0),
        FVector(-350, 0, 0), FVector(100, 0, 0)};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FProofUnitRecord Record;
        Record.UnitId = Index + 1;
        Record.OwnerId = 100;
        Record.Kind = Kinds[Index];
        Record.Position = Positions[Index];
        Record.HitPoints = Index == 3 ? 0.f : 100.f;
        Snapshot.Units.Add(Record);
    }
    FProofResourceRecord Ore;
    Ore.ResourceId = 1;
    Ore.Position = FVector(550, 0, 0);
    Ore.Remaining = 500;
    Snapshot.Resources.Add(Ore);
    return Snapshot;
}

int32 CountRecoveryCommands(const TArray<FProofCommand>& Commands)
{
    int32 Count = 0;
    for (const FProofCommand& Command : Commands)
        Count += Command.Type == EProofCommandType::BuildRefinery ? 1 : 0;
    return Count;
}

int32 CountLiveKind(const FProofSnapshot& Snapshot, int32 OwnerId, EProofUnitKind Kind)
{
    int32 Count = 0;
    for (const FProofUnitRecord& Record : Snapshot.Units)
        Count += Record.OwnerId == OwnerId && Record.Kind == Kind && Record.HitPoints > 0.f ? 1 : 0;
    return Count;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBotMinerRecoveryPlannerTest, "RedalertProof.Skirmish.BotMinerRecoveryPlanner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBotMinerRecoveryPlannerTest::RunTest(const FString&)
{
    FProofSnapshot Snapshot = MinerLossSnapshot();
    FProofSkirmishBot Bot;
    const TArray<FProofCommand> Commands = Bot.Decide(Snapshot, 100, 0.0);
    TestEqual(TEXT("Last miner loss plans one replacement refinery"), CountRecoveryCommands(Commands), 1);
    TestEqual(TEXT("Last 100 credits go to recovery before combat production"), Commands.Num(), 1);
    if (Commands.Num() != 1) { return false; }
    TestTrue(TEXT("Recovery uses an ordinary refinery command"), Commands[0].Type == EProofCommandType::BuildRefinery
        && Commands[0].Sequence == 1 && Commands[0].UnitId == INDEX_NONE && Commands[0].TargetUnitId == INDEX_NONE);
    FVector Spawn;
    TestTrue(TEXT("Recovery site has a valid miner spawn"),
        FProofCommandModel::FindSkirmishSpawn(Snapshot, Commands[0].Destination, Spawn));
    TestTrue(TEXT("Recovery keeps existing structures distinct"),
        FVector::DistSquared(Commands[0].Destination, Snapshot.Units[1].Position) >= 250.0 * 250.0
        && FVector::DistSquared(Commands[0].Destination, Snapshot.Units[2].Position) >= 250.0 * 250.0);
    TestEqual(TEXT("Planner leaves credits unchanged"), Snapshot.Players[0].Credits, 100);
    TestEqual(TEXT("Planner cannot create a miner itself"), CountLiveKind(Snapshot, 100, EProofUnitKind::Harvester), 0);
    TestEqual(TEXT("Subsecond repeat does not spam commands"), Bot.Decide(Snapshot, 100, 0.5).Num(), 0);

    FProofSnapshot Permuted = Snapshot;
    Permuted.Units.Swap(0, 3);
    Permuted.Units.Swap(1, 2);
    FProofSkirmishBot PermutedBot;
    const TArray<FProofCommand> Reordered = PermutedBot.Decide(Permuted, 100, 0.0);
    TestTrue(TEXT("Recovery is stable across unit record order"), Reordered.Num() == 1
        && Reordered[0].Type == Commands[0].Type && Reordered[0].Destination == Commands[0].Destination);

    FProofSnapshot Survivor = Snapshot;
    Survivor.Units[3].HitPoints = 1.f;
    FProofSkirmishBot SurvivorBot;
    TestEqual(TEXT("Even one damaged owned miner suppresses extra refinery"),
        CountRecoveryCommands(SurvivorBot.Decide(Survivor, 100, 0.0)), 0);

    FProofSnapshot ForeignMiner = Snapshot;
    ForeignMiner.Units[3].OwnerId = 1;
    ForeignMiner.Units[3].HitPoints = 100.f;
    FProofSkirmishBot ForeignBot;
    TestEqual(TEXT("A foreign survivor cannot satisfy own recovery"),
        CountRecoveryCommands(ForeignBot.Decide(ForeignMiner, 100, 0.0)), 1);

    for (int32 Remaining : {0, -1})
    {
        FProofSnapshot Empty = Snapshot;
        Empty.Resources[0].Remaining = Remaining;
        FProofSkirmishBot EmptyBot;
        TestEqual(TEXT("Depleted ore does not trigger pointless miner recovery"),
            CountRecoveryCommands(EmptyBot.Decide(Empty, 100, 0.0)), 0);
    }
    FProofSnapshot Missing = Snapshot;
    Missing.Resources.Reset();
    FProofSkirmishBot MissingBot;
    TestEqual(TEXT("No ore records means no extra refinery"), CountRecoveryCommands(MissingBot.Decide(Missing, 100, 0.0)), 0);
    FProofSnapshot InvalidOre = Snapshot;
    InvalidOre.Resources[0].ResourceId = 0;
    FProofSkirmishBot InvalidBot;
    TestEqual(TEXT("An invalid ore ID is not a recovery target"), CountRecoveryCommands(InvalidBot.Decide(InvalidOre, 100, 0.0)), 0);

    for (int32 Credits : {0, 99})
    {
        FProofSnapshot Poor = Snapshot;
        Poor.Players[0].Credits = Credits;
        FProofSkirmishBot PoorBot;
        TestEqual(TEXT("Unaffordable recovery is not submitted"), CountRecoveryCommands(PoorBot.Decide(Poor, 100, 0.0)), 0);
        TestEqual(TEXT("Unaffordable planning does not debit funds"), Poor.Players[0].Credits, Credits);
    }
    FProofSnapshot NoHQ = Snapshot;
    NoHQ.Units[0].HitPoints = 0.f;
    FProofSkirmishBot NoHQBot;
    TestEqual(TEXT("No live HQ prevents new construction"), CountRecoveryCommands(NoHQBot.Decide(NoHQ, 100, 0.0)), 0);

    for (int32 LiveCount : {10, 11, 12})
    {
        FProofSnapshot Crowded = Snapshot;
        for (int32 Id = 3; Id < LiveCount; ++Id)
        {
            FProofUnitRecord Combat;
            Combat.UnitId = 100 + Id;
            Combat.OwnerId = 100;
            Combat.Position = FVector(5000, Id * 300, 0);
            Crowded.Units.Add(Combat);
        }
        FProofSkirmishBot CrowdedBot;
        const TArray<FProofCommand> Batch = CrowdedBot.Decide(Crowded, 100, 0.0);
        TestEqual(TEXT("Recovery requires two free live record slots"), CountRecoveryCommands(Batch), LiveCount == 10 ? 1 : 0);
        TestTrue(TEXT("Recovery cannot exceed decision budget"), Batch.Num() <= 4);
    }
    FProofSnapshot Blocked = Snapshot;
    for (int32 X = -1500; X <= 1500; X += 300)
        for (int32 Y = -1500; Y <= 1500; Y += 300)
        {
            FProofUnitRecord Blocker;
            Blocker.UnitId = 1000 + Blocked.Units.Num();
            Blocker.OwnerId = 1;
            Blocker.Position = FVector(X, Y, 0);
            Blocked.Units.Add(Blocker);
        }
    // The old HQ-only obstruction has a real free spawn beyond that envelope.
    // This remains a prepared planner control; BotBaseExpansion.PublicRecovery
    // separately proves paid recovery through a reachable public-command history.
    FVector ExpandedSpawn = FVector::ZeroVector;
    TestTrue(TEXT("Former HQ-only obstruction leaves a legitimate expanded spawn"),
        FProofCommandModel::FindSkirmishSpawn(Blocked, FVector(950, 0, 0), ExpandedSpawn)
        && ExpandedSpawn == FVector(1850, 0, 0));
    FProofSkirmishBot FreeSlotBot;
    const TArray<FProofCommand> FreeSlot = FreeSlotBot.Decide(Blocked, 100, 0.0);
    TestTrue(TEXT("A valid expanded spawn must still allow exactly one recovery"), FreeSlot.Num() == 1
        && FreeSlot[0].Type == EProofCommandType::BuildRefinery && FreeSlot[0].Sequence == 1
        && FreeSlot[0].Destination == FVector(950, 0, 0));
    // Cover every committed anchor's candidates, not just the HQ candidates.
    // Existing anchors are within 350cm, offsets within 600cm and spawn rings
    // within 900cm, so this 300cm lattice covers the whole search envelope.
    for (int32 X = -2100; X <= 2100; X += 300)
        for (int32 Y = -2100; Y <= 2100; Y += 300)
        {
            if (FMath::Abs(X) <= 1500 && FMath::Abs(Y) <= 1500) { continue; }
            FProofUnitRecord Blocker;
            Blocker.UnitId = 1000 + Blocked.Units.Num();
            Blocker.OwnerId = 1;
            Blocker.Position = FVector(X, Y, 0);
            Blocked.Units.Add(Blocker);
        }
    const FVector RecoveryBlockedOffsets[] = {
        FVector(350, 0, 0), FVector(-350, 0, 0), FVector(0, 350, 0), FVector(0, -350, 0),
        FVector(350, 350, 0), FVector(-350, 350, 0), FVector(350, -350, 0), FVector(-350, -350, 0),
        FVector(600, 0, 0), FVector(-600, 0, 0), FVector(0, 600, 0), FVector(0, -600, 0)
    };
    for (const FProofUnitRecord& Anchor : Blocked.Units)
        if (Anchor.OwnerId == 100 && Anchor.HitPoints > 0.f
            && (Anchor.Kind == EProofUnitKind::HQ || Anchor.Kind == EProofUnitKind::Refinery || Anchor.Kind == EProofUnitKind::Factory))
            for (const FVector& Offset : RecoveryBlockedOffsets)
            {
                FVector NoSpawn = FVector::ZeroVector;
                TestFalse(TEXT("Obstruction exhausts spawn search at every eligible committed anchor candidate"),
                    FProofCommandModel::FindSkirmishSpawn(Blocked, Anchor.Position + Offset, NoSpawn));
            }
    FProofSkirmishBot BlockedBot;
    TestEqual(TEXT("No spawn space prevents a recovery build"), CountRecoveryCommands(BlockedBot.Decide(Blocked, 100, 0.0)), 0);
    TestEqual(TEXT("Blocked planner preserves credits"), Blocked.Players[0].Credits, 100);

    // A destroyed last refinery is not by itself an income opportunity. Keep
    // the existing replacement-miner policy, but avoid a purposeless rebuild.
    FProofSnapshot Exhausted = MinerLossSnapshot();
    Exhausted.Units[1].HitPoints = 0.f;
    Exhausted.Units[3].HitPoints = 100.f;
    Exhausted.Resources[0].Remaining = 0;
    for (int32 OreCase = 0; OreCase < 4; ++OreCase)
    {
        FProofSnapshot NoIncome = Exhausted;
        if (OreCase == 1) { NoIncome.Resources.Reset(); }
        if (OreCase == 2) { NoIncome.Resources[0].Remaining = -1; }
        if (OreCase == 3) { NoIncome.Resources[0].Remaining = 500; NoIncome.Resources[0].ResourceId = 0; }
        FProofSkirmishBot NoIncomeBot;
        const TArray<FProofCommand> Batch = NoIncomeBot.Decide(NoIncome, 100, 0.0);
        TestEqual(TEXT("Missing refinery without usable ore or cargo is not rebuilt"), CountRecoveryCommands(Batch), 0);
        TestTrue(TEXT("Skipped refinery leaves the existing production choice and first sequence available"),
            Batch.Num() == 1 && Batch[0].Type == EProofCommandType::Produce
            && Batch[0].UnitId == 3 && Batch[0].Sequence == 1);
        TestEqual(TEXT("No-income planning preserves the last refinery-cost credits"), NoIncome.Players[0].Credits, 100);
        const TArray<FProofCommand> Later = NoIncomeBot.Decide(NoIncome, 100, 1.0);
        TestTrue(TEXT("Repeated no-income decisions do not add rebuilds or sequence gaps"),
            Later.Num() == 1 && Later[0].Type == EProofCommandType::Produce && Later[0].Sequence == 2);
    }
    FProofSnapshot OreAvailable = Exhausted;
    OreAvailable.Resources[0].Remaining = 1;
    FProofSkirmishBot OreAvailableBot;
    TestEqual(TEXT("One remaining usable ore still permits missing-refinery recovery"),
        CountRecoveryCommands(OreAvailableBot.Decide(OreAvailable, 100, 0.0)), 1);

    for (int32 Cargo : {1, FProofCommandModel::CargoCapacity})
    {
        FProofSnapshot Loaded = Exhausted;
        Loaded.Resources.Reset();
        Loaded.Units[3].Cargo = Cargo;
        FProofSkirmishBot LoadedBot;
        const TArray<FProofCommand> Batch = LoadedBot.Decide(Loaded, 100, 0.0);
        TestTrue(TEXT("A surviving own loaded miner keeps refinery recovery ahead of production"),
            Batch.Num() == 1 && Batch[0].Type == EProofCommandType::BuildRefinery && Batch[0].Sequence == 1);
        TestEqual(TEXT("Loaded planning does not spend or unload itself"), Loaded.Units[3].Cargo, Cargo);
        Loaded.Units.Swap(0, 3);
        Loaded.Units.Swap(1, 2);
        FProofSkirmishBot LoadedPermutationBot;
        const TArray<FProofCommand> ReorderedLoaded = LoadedPermutationBot.Decide(Loaded, 100, 0.0);
        TestTrue(TEXT("Loaded recovery site is independent of unit record order"),
            Batch.Num() == 1 && ReorderedLoaded.Num() == 1
            && Batch[0].Destination == ReorderedLoaded[0].Destination);
    }
    for (int32 InvalidCargoCase = 0; InvalidCargoCase < 3; ++InvalidCargoCase)
    {
        FProofSnapshot Ineligible = Exhausted;
        Ineligible.Units[3].Cargo = FProofCommandModel::CargoCapacity;
        if (InvalidCargoCase == 0) { Ineligible.Units[3].HitPoints = 0.f; }
        if (InvalidCargoCase == 1) { Ineligible.Units[3].OwnerId = 1; }
        if (InvalidCargoCase == 2) { Ineligible.Units[3].Kind = EProofUnitKind::Combat; }
        FProofSkirmishBot IneligibleBot;
        TestEqual(TEXT("Dead, foreign or non-miner cargo cannot justify a refinery rebuild"),
            CountRecoveryCommands(IneligibleBot.Decide(Ineligible, 100, 0.0)), 0);
    }
    FProofSnapshot NoFactory = Exhausted;
    NoFactory.Units[2].HitPoints = 0.f;
    NoFactory.Players[0].Credits = FProofCommandModel::FactoryCost;
    FProofSkirmishBot NoFactoryBot;
    const TArray<FProofCommand> FactoryOnly = NoFactoryBot.Decide(NoFactory, 100, 0.0);
    TestTrue(TEXT("Skipping a futile refinery does not suppress affordable factory construction"),
        FactoryOnly.Num() == 1 && FactoryOnly[0].Type == EProofCommandType::BuildFactory && FactoryOnly[0].Sequence == 1);

    // Applying the returned grant to the next visible snapshot is sufficient
    // to suppress further recovery. No hidden planner flag blocks future loss.
    Snapshot.Units[3].HitPoints = 100.f;
    Snapshot.Units[3].UnitId = 50;
    const TArray<FProofCommand> Recovered = Bot.Decide(Snapshot, 100, 1.0);
    TestEqual(TEXT("Recovered miner suppresses another refinery"), CountRecoveryCommands(Recovered), 0);
    TestTrue(TEXT("Recovered miner joins the normal Gather path"), Recovered.ContainsByPredicate(
        [](const FProofCommand& C) { return C.Type == EProofCommandType::Gather && C.UnitId == 50; }));
    Snapshot.Units[3].HitPoints = 0.f;
    TestEqual(TEXT("A later complete loss can recover again"), CountRecoveryCommands(Bot.Decide(Snapshot, 100, 2.0)), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBotMinerRecoveryModelTest, "RedalertProof.Skirmish.BotMinerRecoveryModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBotMinerRecoveryModelTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    TestTrue(TEXT("Seed opposing human"), Model.SeedSkirmishPlayer(1, 0, FVector(5000, 0, 0)));
    TestTrue(TEXT("Seed bot"), Model.SeedSkirmishPlayer(100, 1, FVector::ZeroVector));
    FProofSkirmishBot Bot;

    int32 HumanSequence = 0;
    const auto HumanCommand = [&](EProofCommandType Type, int32 UnitId, int32 TargetId, const FVector& Destination)
    {
        FProofCommand Command;
        Command.Sequence = ++HumanSequence;
        Command.Type = Type;
        Command.UnitId = UnitId;
        Command.TargetUnitId = TargetId;
        Command.Destination = Destination;
        return Model.Submit(1, Command).bAccepted;
    };
    TestTrue(TEXT("Human builds attack factory"), HumanCommand(EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(4700, 0, 0)));
    FProofSnapshot State = Model.GetSnapshot();
    const FProofUnitRecord* Factory = State.Units.FindByPredicate([](const FProofUnitRecord& U)
        { return U.OwnerId == 1 && U.Kind == EProofUnitKind::Factory; });
    if (!Factory) { TestTrue(TEXT("Human factory exists"), false); return false; }
    TestTrue(TEXT("Human produces attacker"), HumanCommand(EProofCommandType::Produce, Factory->UnitId, INDEX_NONE, FVector::ZeroVector));
    Model.Advance(FProofCommandModel::ProductionDurationSeconds);
    // Begin the bot economy after the paid attacker is available. This keeps
    // the original first-load-loss premise without suppressing auto-gather.
    // Preserve elapsed bot intervals while accounting for real setup time.
    const double EconomyStartedAt = FProofCommandModel::ProductionDurationSeconds;
    for (const FProofCommand& Command : Bot.Decide(Model.GetSnapshot(), 100, EconomyStartedAt))
        TestTrue(TEXT("Opening bot build passes shared validator"), Model.Submit(100, Command).bAccepted);
    TestEqual(TEXT("Opening refinery grants a second miner"), CountLiveKind(Model.GetSnapshot(), 100, EProofUnitKind::Harvester), 2);
    TestEqual(TEXT("Opening creates one refinery"), CountLiveKind(Model.GetSnapshot(), 100, EProofUnitKind::Refinery), 1);
    State = Model.GetSnapshot();
    const FProofUnitRecord* Attacker = State.Units.FindByPredicate([](const FProofUnitRecord& U)
        { return U.OwnerId == 1 && U.Kind == EProofUnitKind::Combat; });
    if (!Attacker) { TestTrue(TEXT("Human attacker exists"), false); return false; }
    const int32 AttackerId = Attacker->UnitId;
    TArray<int32> MinerIds;
    for (const FProofUnitRecord& U : State.Units)
        if (U.OwnerId == 100 && U.Kind == EProofUnitKind::Harvester) MinerIds.Add(U.UnitId);
    for (int32 MinerId : MinerIds)
    {
        TestTrue(TEXT("Attack miner accepted"), HumanCommand(EProofCommandType::Attack, AttackerId, MinerId, FVector::ZeroVector));
        Model.Advance(40.f);
    }
    TestTrue(TEXT("Retire attacker away from economic recovery"), HumanCommand(EProofCommandType::Move, AttackerId, INDEX_NONE, FVector(5000, 0, 0)));
    Model.Advance(40.f);
    State = Model.GetSnapshot();
    TestEqual(TEXT("Actual combat killed both bot miners"), CountLiveKind(State, 100, EProofUnitKind::Harvester), 0);
    TestEqual(TEXT("Existing refinery survived combat"), CountLiveKind(State, 100, EProofUnitKind::Refinery), 1);
    const FProofPlayerRecord* Before = State.Players.FindByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 100; });
    TestTrue(TEXT("Bot retains exactly the replacement cost"), Before && Before->Credits == FProofCommandModel::RefineryCost);
    if (!Before) return false;
    const int32 GatheredBefore = Before->TotalGathered;
    const TArray<FProofCommand> Recovery = Bot.Decide(State, 100, EconomyStartedAt + 120.0);
    TestEqual(TEXT("Surviving refinery does not block replacement miner"), CountRecoveryCommands(Recovery), 1);
    for (const FProofCommand& Command : Recovery)
        TestTrue(TEXT("Recovery uses accepted shared command"), Model.Submit(100, Command).bAccepted);
    State = Model.GetSnapshot();
    TestEqual(TEXT("Exactly one replacement miner is granted"), CountLiveKind(State, 100, EProofUnitKind::Harvester), 1);
    TestEqual(TEXT("Exactly one additional refinery is constructed"), CountLiveKind(State, 100, EProofUnitKind::Refinery), 2);
    const FProofPlayerRecord* After = State.Players.FindByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 100; });
    TestTrue(TEXT("Recovery charges exactly one refinery cost"), After && After->Credits == 0);
    int32 LaterRefineries = 0;
    bool bReplacementGather = false;
    bool bDeposited = false;
    for (int32 Tick = 1; Tick <= 30; ++Tick)
    {
        for (const FProofCommand& Command : Bot.Decide(Model.GetSnapshot(), 100, EconomyStartedAt + 120.0 + Tick))
        {
            LaterRefineries += Command.Type == EProofCommandType::BuildRefinery ? 1 : 0;
            bReplacementGather |= Command.Type == EProofCommandType::Gather;
            TestTrue(TEXT("Post-recovery command remains authoritative"), Model.Submit(100, Command).bAccepted);
        }
        Model.Advance(1.f);
        const FProofSnapshot Current = Model.GetSnapshot();
        const FProofPlayerRecord* Owner = Current.Players.FindByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 100; });
        bDeposited |= Owner && Owner->TotalGathered > GatheredBefore;
    }
    TestTrue(TEXT("Replacement actually receives Gather"), bReplacementGather);
    TestTrue(TEXT("Replacement travels, extracts and deposits earned credits"), bDeposited);
    TestEqual(TEXT("No repeated refinery construction after recovery"), LaterRefineries, 0);
    TestTrue(TEXT("Recovery fixture remains a playing match"), Model.GetSnapshot().MatchPhase == EProofMatchPhase::Playing);
    // Reach both policy states through public setup/commands/combat only. This
    // is a snapshot-to-planner intent test, not an autonomous bot-match trace:
    // setup has its own sequence history, so accepted intents below receive
    // that model owner's next sequence rather than the fresh planner's one.
    for (bool bKeepLoadedCargo : {false, true})
    {
        FProofCommandModel DepletedModel;
        DepletedModel.StartSkirmish();
        TestTrue(TEXT("Depletion trace seeds bot owner"), DepletedModel.SeedSkirmishPlayer(100, 1, FVector::ZeroVector));
        TestTrue(TEXT("Depletion trace seeds opposing owner"), DepletedModel.SeedSkirmishPlayer(1, 0, FVector(6000, 0, 0)));
        int32 BotSequence = 0, OpponentSequence = 0;
        const auto DepletedSubmit = [&](int32 OwnerId, EProofCommandType Type, int32 UnitId,
            int32 TargetId, const FVector& Destination)
        {
            FProofCommand Command;
            Command.Sequence = OwnerId == 100 ? ++BotSequence : ++OpponentSequence;
            Command.Type = Type; Command.UnitId = UnitId; Command.TargetUnitId = TargetId; Command.Destination = Destination;
            return TestTrue(TEXT("Public depletion trace command is accepted"), DepletedModel.Submit(OwnerId, Command).bAccepted);
        };
        const auto LiveKindId = [&](int32 OwnerId, EProofUnitKind Kind) -> int32
        {
            for (const FProofUnitRecord& Record : DepletedModel.GetSnapshot().Units)
                if (Record.OwnerId == OwnerId && Record.Kind == Kind && Record.HitPoints > 0.f) return Record.UnitId;
            return INDEX_NONE;
        };
        if (!DepletedSubmit(100, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(350, 0, 0))) return false;
        const int32 LostRefineryId = LiveKindId(100, EProofUnitKind::Refinery);
        if (!DepletedSubmit(100, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(-350, 0, 0))
            || !DepletedSubmit(1, EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, FVector(6350, 0, 0))
            || !DepletedSubmit(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(5650, 0, 0))
            || !DepletedSubmit(1, EProofCommandType::Produce, LiveKindId(1, EProofUnitKind::Factory), INDEX_NONE, FVector::ZeroVector)) return false;

        for (const FProofUnitRecord& Record : DepletedModel.GetSnapshot().Units)
            if (Record.Kind == EProofUnitKind::Harvester
                && !DepletedSubmit(Record.OwnerId, EProofCommandType::Gather, Record.UnitId,
                    Record.OwnerId == 100 ? 1 : 2, FVector::ZeroVector)) return false;
        int32 OreRemaining = 1;
        for (int32 Tick = 0; Tick < 200 && OreRemaining > 0; ++Tick)
        {
            DepletedModel.Advance(1.f);
            OreRemaining = 0;
            for (const FProofResourceRecord& Resource : DepletedModel.GetSnapshot().Resources) OreRemaining += Resource.Remaining;
        }
        // Production progresses alongside the actual bounded harvesting loop.
        const int32 DepletionAttackerId = LiveKindId(1, EProofUnitKind::Combat);
        if (!TestTrue(TEXT("Actual harvesting time completed the opponent's paid job"), DepletionAttackerId > 0
            && DepletedModel.GetSnapshot().ProductionJobs.IsEmpty())) return false;
        TestEqual(TEXT("Public harvesting exhausts every real ore patch"), OreRemaining, 0);
        if (OreRemaining != 0) return false;
        if (!bKeepLoadedCargo)
        {
            DepletedModel.Advance(20.f); // Existing Gather unloads the final cargo.
            for (int32 Index = 0; Index < 7; ++Index)
            {
                if (!DepletedSubmit(100, EProofCommandType::Produce, LiveKindId(100, EProofUnitKind::Factory),
                    INDEX_NONE, FVector::ZeroVector)) return false;
                DepletedModel.Advance(FProofCommandModel::ProductionDurationSeconds);
                if (!TestTrue(TEXT("Each finite-ore-funded job actually completes"), DepletedModel.GetSnapshot().ProductionJobs.IsEmpty())) return false;
            }
        }
        if (!DepletedSubmit(1, EProofCommandType::Attack, DepletionAttackerId, LostRefineryId, FVector::ZeroVector)) return false;
        // Movement then combat precede deposit: a loaded Gather survives the
        // refinery loss with undelivered cargo, without any direct state edit.
        DepletedModel.Advance(40.f);
        TestEqual(TEXT("Actual enemy combat destroys the last own refinery"),
            CountLiveKind(DepletedModel.GetSnapshot(), 100, EProofUnitKind::Refinery), 0);
        if (!bKeepLoadedCargo)
        {
            if (!DepletedSubmit(1, EProofCommandType::Attack, DepletionAttackerId,
                LiveKindId(100, EProofUnitKind::Harvester), FVector::ZeroVector)) return false;
            DepletedModel.Advance(4.f); // Free exactly the second required grant slot.
        }
        if (!DepletedSubmit(1, EProofCommandType::Move, DepletionAttackerId, INDEX_NONE, FVector(6000, 0, 0))) return false;
        DepletedModel.Advance(40.f);
        const FProofSnapshot BeforeIntent = DepletedModel.GetSnapshot();
        const FProofPlayerRecord* BeforeOwner = BeforeIntent.Players.FindByPredicate(
            [](const FProofPlayerRecord& P) { return P.OwnerId == 100; });
        int32 OwnCargoBefore = 0, LiveBefore = 0;
        for (const FProofUnitRecord& Record : BeforeIntent.Units)
            if (Record.OwnerId == 100 && Record.HitPoints > 0.f)
            {
                ++LiveBefore;
                if (Record.Kind == EProofUnitKind::Harvester) OwnCargoBefore += Record.Cargo;
            }
        TestTrue(TEXT("Public depletion trace remains live and affordable"), BeforeOwner
            && BeforeOwner->Credits >= FProofCommandModel::RefineryCost && BeforeIntent.MatchPhase == EProofMatchPhase::Playing);
        if (!BeforeOwner) return false;
        TestTrue(TEXT("Public trace distinguishes loaded versus empty surviving miners"),
            bKeepLoadedCargo ? OwnCargoBefore > 0 : OwnCargoBefore == 0);
        if (!bKeepLoadedCargo) TestEqual(TEXT("Empty public loss leaves exactly two grant slots"), LiveBefore, 10);
        FProofSkirmishBot IntentPlanner;
        const TArray<FProofCommand> Intents = IntentPlanner.Decide(BeforeIntent, 100, 0.0);
        TestEqual(TEXT("Public no-ore refinery recovery depends on surviving own cargo"),
            CountRecoveryCommands(Intents), bKeepLoadedCargo ? 1 : 0);
        TestTrue(TEXT("Public depletion intent remains bounded"), Intents.Num() <= 4);
        int32 ProduceIntents = 0, GuardIntents = 0;
        for (const FProofCommand& Intent : Intents)
        {
            ProduceIntents += Intent.Type == EProofCommandType::Produce ? 1 : 0;
            GuardIntents += Intent.Type == EProofCommandType::AreaGuard ? 1 : 0;
        }
        if (!bKeepLoadedCargo)
            TestTrue(TEXT("Exhausted empty public trace produces once, then guards three existing Combat within budget"),
                Intents.Num() == 4 && Intents[0].Type == EProofCommandType::Produce
                && ProduceIntents == 1 && GuardIntents == 3);
        else
            TestTrue(TEXT("Loaded recovery fixture keeps refinery plus production and has no existing Combat to guard"),
                Intents.Num() == 2 && ProduceIntents == 1 && GuardIntents == 0);
        int32 ExpectedSpend = 0;
        for (const FProofCommand& Intent : Intents)
        {
            if (Intent.Type == EProofCommandType::BuildRefinery) ExpectedSpend += FProofCommandModel::RefineryCost;
            else if (Intent.Type == EProofCommandType::Produce)
            {
                // The sole next Advance is one second: exactly ten of fifty
                // credits buy that work; job admission itself is free.
                ExpectedSpend += 10;
            }
            else if (Intent.Type == EProofCommandType::AreaGuard)
            {
                const FProofUnitRecord* Defender = BeforeIntent.Units.FindByPredicate(
                    [&Intent](const FProofUnitRecord& Record) { return Record.UnitId == Intent.UnitId; });
                TestTrue(TEXT("Only live owned Combat gets a cost-free defensive intent"), Defender
                    && Defender->OwnerId == 100 && Defender->HitPoints > 0.f && Defender->Kind == EProofUnitKind::Combat
                    && Intent.TargetUnitId == INDEX_NONE && Intent.Destination == FVector::ZeroVector);
            }
            else { TestTrue(TEXT("Unexpected public depletion planner intent"), false); return false; }
            if (!DepletedSubmit(100, Intent.Type, Intent.UnitId, Intent.TargetUnitId, Intent.Destination)) return false;
        }
        DepletedModel.Advance(1.f);
        const FProofSnapshot AfterIntent = DepletedModel.GetSnapshot();
        const FProofPlayerRecord* AfterOwner = AfterIntent.Players.FindByPredicate(
            [](const FProofPlayerRecord& P) { return P.OwnerId == 100; });
        const FProofProductionJob* ActiveJob = AfterIntent.ProductionJobs.FindByPredicate(
            [](const FProofProductionJob& Job) { return Job.OwnerId == 100; });
        TestTrue(TEXT("One actual second yields exactly ten paid credits and no completion"), ActiveJob
            && ActiveJob->PaidCredits == 10 && ActiveJob->WorkMicroseconds == 1000000
            && CountLiveKind(AfterIntent, 100, EProofUnitKind::Combat) == CountLiveKind(BeforeIntent, 100, EProofUnitKind::Combat));
        TestTrue(TEXT("Actual intents charge only their costs and conserve recovered cargo"), AfterOwner
            && AfterOwner->Credits == BeforeOwner->Credits - ExpectedSpend + OwnCargoBefore
            && AfterOwner->TotalGathered == BeforeOwner->TotalGathered + OwnCargoBefore);
        TestEqual(TEXT("Public trace creates a new refinery only for loaded recovery"),
            CountLiveKind(AfterIntent, 100, EProofUnitKind::Refinery), bKeepLoadedCargo ? 1 : 0);
        int32 OwnCargoAfter = 0;
        for (const FProofUnitRecord& Record : AfterIntent.Units)
            if (Record.OwnerId == 100 && Record.HitPoints > 0.f && Record.Kind == EProofUnitKind::Harvester)
                OwnCargoAfter += Record.Cargo;
        TestEqual(TEXT("Surviving Gather actually unloads all recovered cargo"), OwnCargoAfter, 0);
        TestTrue(TEXT("Recovery does not mint any new ore"), !AfterIntent.Resources.ContainsByPredicate(
            [](const FProofResourceRecord& Resource) { return Resource.Remaining > 0; }));
        TestTrue(TEXT("Public recovery does not force a terminal outcome"), AfterIntent.MatchPhase == EProofMatchPhase::Playing);
    }
    return true;
}
#endif
