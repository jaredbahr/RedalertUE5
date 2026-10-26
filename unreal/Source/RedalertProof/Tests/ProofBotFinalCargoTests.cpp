#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Skirmish/ProofSkirmishBot.h"

namespace
{
FProofUnitRecord& FinalCargoMutableUnit(FProofSnapshot& State, int32 Id)
{
    return *State.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; });
}

FProofSnapshot FinalCargoPlannerSnapshot()
{
    FProofSnapshot State;
    State.bSkirmish = true;
    State.MatchPhase = EProofMatchPhase::Playing;
    FProofPlayerRecord Owner;
    Owner.OwnerId = 100;
    Owner.TeamId = 1;
    Owner.Credits = 0;
    State.Players.Add(Owner);
    FProofPlayerRecord Enemy;
    Enemy.OwnerId = 1;
    Enemy.TeamId = 0;
    State.Players.Add(Enemy);

    const EProofUnitKind Kinds[] = { EProofUnitKind::HQ, EProofUnitKind::Factory,
        EProofUnitKind::Refinery, EProofUnitKind::Harvester, EProofUnitKind::Harvester,
        EProofUnitKind::Harvester, EProofUnitKind::Harvester, EProofUnitKind::Combat,
        EProofUnitKind::Combat, EProofUnitKind::Combat };
    const FVector Positions[] = { FVector::ZeroVector, FVector(-350, 0, 0), FVector(350, 0, 0),
        FVector(100, 0, 0), FVector(200, 0, 0), FVector(250, 0, 0), FVector(300, 0, 0),
        FVector(0, 500, 0), FVector(0, 800, 0), FVector(0, 1100, 0) };
    constexpr int32 UnitCount = static_cast<int32>(sizeof(Kinds) / sizeof(Kinds[0]));
    for (int32 Index = 0; Index < UnitCount; ++Index)
    {
        FProofUnitRecord Unit;
        Unit.UnitId = Index + 1;
        Unit.OwnerId = 100;
        Unit.Kind = Kinds[Index];
        Unit.Position = Positions[Index];
        Unit.HitPoints = Unit.Kind == EProofUnitKind::Combat ? 0.f : 100.f;
        Unit.Cargo = Unit.Kind == EProofUnitKind::Harvester ? FProofCommandModel::CargoCapacity : 0;
        State.Units.Add(Unit);
    }
    FProofResourceRecord Exhausted;
    Exhausted.ResourceId = 1;
    Exhausted.Remaining = 0;
    Exhausted.Position = FVector(550, 0, 0);
    State.Resources.Add(Exhausted);
    FProofUnitRecord EnemyCombat;
    EnemyCombat.UnitId = 30;
    EnemyCombat.OwnerId = 1;
    EnemyCombat.Kind = EProofUnitKind::Combat;
    EnemyCombat.Position = FVector(2000, 0, 0);
    State.Units.Add(EnemyCombat);
    return State;
}

int32 FinalCargoCount(const TArray<FProofCommand>& Commands, EProofCommandType Type)
{
    int32 Count = 0;
    for (const FProofCommand& Command : Commands) Count += Command.Type == Type ? 1 : 0;
    return Count;
}

const FProofCommand* FindFinalCargoIntent(const TArray<FProofCommand>& Commands, EProofCommandType Type)
{
    return Commands.FindByPredicate([Type](const FProofCommand& Command) { return Command.Type == Type; });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBotFinalCargoPlannerTest, "RedalertProof.Skirmish.BotFinalCargoPlanner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBotFinalCargoPlannerTest::RunTest(const FString&)
{
    FProofSnapshot State = FinalCargoPlannerSnapshot();
    FProofSkirmishBot Bot;
    const TArray<FProofCommand> First = Bot.Decide(State, 100, 0.0);
    TestTrue(TEXT("Loaded miner receives ReturnCargo to the live own refinery"), First.Num() == 1
        && First[0].Type == EProofCommandType::ReturnCargo && First[0].UnitId == 4
        && First[0].TargetUnitId == 3);
    TestEqual(TEXT("Repeated same-time decision is rate limited"), Bot.Decide(State, 100, 0.0).Num(), 0);
    TestEqual(TEXT("Global one-second decision interval still applies"), Bot.Decide(State, 100, 0.5).Num(), 0);
    const TArray<FProofCommand> AtOne = Bot.Decide(State, 100, 1.0);
    TestTrue(TEXT("One-second cadence rotates through the next loaded miner"), AtOne.Num() == 1
        && AtOne[0].Type == EProofCommandType::ReturnCargo && AtOne[0].UnitId == 5);
    const TArray<FProofCommand> AtTwo = Bot.Decide(State, 100, 2.0);
    TestTrue(TEXT("Rotation reaches the third loaded miner"), AtTwo.Num() == 1 && AtTwo[0].UnitId == 6);
    const TArray<FProofCommand> AtThree = Bot.Decide(State, 100, 3.0);
    TestTrue(TEXT("Rotation reaches the fourth loaded miner"), AtThree.Num() == 1 && AtThree[0].UnitId == 7);
    TestEqual(TEXT("All four miners were visited in stable order"),
        AtOne.Num() + AtTwo.Num() + AtThree.Num() + First.Num(), 4);
    TestEqual(TEXT("No miner retries at four seconds after its original issue"), Bot.Decide(State, 100, 4.0).Num(), 0);
    const TArray<FProofCommand> AtFive = Bot.Decide(State, 100, 5.0);
    TestTrue(TEXT("First miner retries at five seconds after its original issue"), AtFive.Num() == 1
        && AtFive[0].Type == EProofCommandType::ReturnCargo && AtFive[0].UnitId == 4);

    // Isolate one live miner: empty miners are still eligible for Gather.
    FProofSnapshot SingleLoaded = State;
    for (int32 MinerId : { 5, 6, 7 })
    {
        FinalCargoMutableUnit(SingleLoaded, MinerId).Cargo = 0;
        FinalCargoMutableUnit(SingleLoaded, MinerId).HitPoints = 0.f;
    }
    FProofSkirmishBot ChangeBot;
    SingleLoaded.Resources[0].Remaining = 1;
    const TArray<FProofCommand> Gather = ChangeBot.Decide(SingleLoaded, 100, 0.0);
    TestTrue(TEXT("Usable ore retains ordinary Gather behavior"), Gather.Num() == 1
        && Gather[0].Type == EProofCommandType::Gather && Gather[0].UnitId == 4
        && Gather[0].TargetUnitId == 1);
    SingleLoaded.Resources[0].Remaining = 0;
    const TArray<FProofCommand> Return = ChangeBot.Decide(SingleLoaded, 100, 1.0);
    TestTrue(TEXT("Gather to ReturnCargo type change is immediate for the same miner"), Return.Num() == 1
        && Return[0].Type == EProofCommandType::ReturnCargo && Return[0].UnitId == 4);
    SingleLoaded.Resources[0].Remaining = 1;
    const TArray<FProofCommand> GatherAgain = ChangeBot.Decide(SingleLoaded, 100, 2.0);
    TestTrue(TEXT("ReturnCargo to Gather type change is immediate for the same miner"), GatherAgain.Num() == 1
        && GatherAgain[0].Type == EProofCommandType::Gather && GatherAgain[0].UnitId == 4);

    for (int32 Ineligible : { 0, 1, 2, 3 })
    {
        FProofSnapshot Filtered = State;
        for (int32 MinerId : { 4, 5, 6, 7 }) FinalCargoMutableUnit(Filtered, MinerId).Cargo = 0;
        if (Ineligible != 0) FinalCargoMutableUnit(Filtered, 4).Cargo = 50;
        if (Ineligible == 1) FinalCargoMutableUnit(Filtered, 4).HitPoints = 0.f;
        if (Ineligible == 2) FinalCargoMutableUnit(Filtered, 4).OwnerId = 1;
        if (Ineligible == 3) FinalCargoMutableUnit(Filtered, 4).Kind = EProofUnitKind::Combat;
        FProofSkirmishBot FilterBot;
        TestEqual(TEXT("Empty, dead, foreign and non-Harvester units receive no ReturnCargo"),
            FinalCargoCount(FilterBot.Decide(Filtered, 100, 0.0), EProofCommandType::ReturnCargo), 0);
    }

    FProofSnapshot NoRefinery = State;
    NoRefinery.Players[0].Credits = FProofCommandModel::RefineryCost;
    FinalCargoMutableUnit(NoRefinery, 3).HitPoints = 0.f;
    FProofSkirmishBot NoRefineryBot;
    const TArray<FProofCommand> RebuildBatch = NoRefineryBot.Decide(NoRefinery, 100, 0.0);
    TestTrue(TEXT("Without a committed refinery the planner rebuilds but sends no ReturnCargo in that batch"),
        FindFinalCargoIntent(RebuildBatch, EProofCommandType::BuildRefinery) != nullptr
        && FindFinalCargoIntent(RebuildBatch, EProofCommandType::ReturnCargo) == nullptr);

    FProofSnapshot Nearest = State;
    for (int32 MinerId : { 5, 6, 7 }) FinalCargoMutableUnit(Nearest, MinerId).Cargo = 0;
    FinalCargoMutableUnit(Nearest, 4).Position = FVector(1000, 0, 0);
    FinalCargoMutableUnit(Nearest, 3).Position = FVector(600, 0, 0);
    FProofUnitRecord OtherRefinery;
    OtherRefinery.UnitId = 20;
    OtherRefinery.OwnerId = 100;
    OtherRefinery.Kind = EProofUnitKind::Refinery;
    OtherRefinery.Position = FVector(800, 0, 0);
    Nearest.Units.Add(OtherRefinery);
    FProofSkirmishBot NearestBot;
    const TArray<FProofCommand> NearestOrder = NearestBot.Decide(Nearest, 100, 0.0);
    TestTrue(TEXT("Nearest refinery wins over the farther lower-ID refinery"), NearestOrder.Num() == 1
        && NearestOrder[0].UnitId == 4 && NearestOrder[0].TargetUnitId == 20);

    FinalCargoMutableUnit(Nearest, 20).Position = FVector(1400, 0, 0);
    FProofSkirmishBot DistanceTieBot;
    const TArray<FProofCommand> DistanceTie = DistanceTieBot.Decide(Nearest, 100, 0.0);
    TestTrue(TEXT("Equal distance chooses lower refinery ID"), DistanceTie.Num() == 1 && DistanceTie[0].TargetUnitId == 3);

    FProofSnapshot TiedPermutation = Nearest;
    TiedPermutation.Units.Swap(0, TiedPermutation.Units.Num() - 1);
    TiedPermutation.Units.Swap(1, 5);
    FProofSkirmishBot TiedPermutationBot;
    const TArray<FProofCommand> PermutedTie = TiedPermutationBot.Decide(TiedPermutation, 100, 0.0);
    TestTrue(TEXT("Distance tie keeps lower ID across multiple-refinery record permutation"),
        PermutedTie.Num() == 1 && PermutedTie[0].Type == EProofCommandType::ReturnCargo
        && PermutedTie[0].UnitId == 4 && PermutedTie[0].TargetUnitId == 3);

    FinalCargoMutableUnit(Nearest, 20).Position = FVector(800, 0, 0);
    FinalCargoMutableUnit(Nearest, 20).OwnerId = 1;
    FProofSkirmishBot ForeignRefineryBot;
    const TArray<FProofCommand> ForeignRefinery = ForeignRefineryBot.Decide(Nearest, 100, 0.0);
    TestTrue(TEXT("A closer foreign refinery is excluded"), ForeignRefinery.Num() == 1 && ForeignRefinery[0].TargetUnitId == 3);
    FinalCargoMutableUnit(Nearest, 20).OwnerId = 100;
    FinalCargoMutableUnit(Nearest, 20).HitPoints = 0.f;
    FProofSkirmishBot DeadRefineryBot;
    const TArray<FProofCommand> DeadRefinery = DeadRefineryBot.Decide(Nearest, 100, 0.0);
    TestTrue(TEXT("A closer destroyed refinery is excluded"), DeadRefinery.Num() == 1 && DeadRefinery[0].TargetUnitId == 3);

    FProofSnapshot ChangedTarget = State;
    FinalCargoMutableUnit(ChangedTarget, 4).Position = FVector(1000, 0, 0);
    FinalCargoMutableUnit(ChangedTarget, 3).Position = FVector(600, 0, 0);
    for (FProofUnitRecord& Unit : ChangedTarget.Units)
    {
        if (Unit.Kind == EProofUnitKind::Harvester && Unit.UnitId != 4) { Unit.Cargo = 0; }
    }
    FProofSkirmishBot ChangedTargetBot;
    const TArray<FProofCommand> BeforeTargetChange = ChangedTargetBot.Decide(ChangedTarget, 100, 0.0);
    TestTrue(TEXT("Single miner initially returns to the current refinery"), BeforeTargetChange.Num() == 1
        && BeforeTargetChange[0].Type == EProofCommandType::ReturnCargo
        && BeforeTargetChange[0].UnitId == 4 && BeforeTargetChange[0].TargetUnitId == 3);
    FProofUnitRecord CloserRefinery = OtherRefinery;
    CloserRefinery.UnitId = 21;
    CloserRefinery.Position = FVector(800, 0, 0);
    ChangedTarget.Units.Add(CloserRefinery);
    const TArray<FProofCommand> AfterTargetChange = ChangedTargetBot.Decide(ChangedTarget, 100, 1.0);
    TestTrue(TEXT("Same miner changes refinery immediately within the five-second retry interval"),
        AfterTargetChange.Num() == 1 && AfterTargetChange[0].Type == EProofCommandType::ReturnCargo
        && AfterTargetChange[0].UnitId == 4 && AfterTargetChange[0].TargetUnitId == 21);

    FProofSnapshot Permuted = State;
    Permuted.Units.Swap(0, 7);
    Permuted.Units.Swap(1, 5);
    FProofSkirmishBot PermutedBot;
    const TArray<FProofCommand> Stable = PermutedBot.Decide(Permuted, 100, 0.0);
    TestTrue(TEXT("Miner choice and destination are stable across record permutation"), Stable.Num() == 1
        && Stable[0].UnitId == First[0].UnitId && Stable[0].TargetUnitId == First[0].TargetUnitId);

    FProofSnapshot Budget = State;
    Budget.Players[0].Credits = 200;
    for (int32 CombatId : { 8, 9, 10 }) FinalCargoMutableUnit(Budget, CombatId).HitPoints = 100.f;
    FProofSkirmishBot BudgetBot;
    const TArray<FProofCommand> Opening = BudgetBot.Decide(Budget, 100, 0.0);
    TestTrue(TEXT("ReturnCargo coexists with production within the four-command budget"), Opening.Num() <= 4
        && Opening.ContainsByPredicate([](const FProofCommand& C) { return C.Type == EProofCommandType::ReturnCargo; })
        && Opening.ContainsByPredicate([](const FProofCommand& C) { return C.Type == EProofCommandType::Produce; }));
    const TArray<FProofCommand> Later = BudgetBot.Decide(Budget, 100, 20.0);
    TestTrue(TEXT("ReturnCargo coexists with production and attack within the four-command budget"), Later.Num() <= 4
        && Later.ContainsByPredicate([](const FProofCommand& C) { return C.Type == EProofCommandType::ReturnCargo; })
        && Later.ContainsByPredicate([](const FProofCommand& C) { return C.Type == EProofCommandType::Produce; })
        && Later.ContainsByPredicate([](const FProofCommand& C) { return C.Type == EProofCommandType::Attack; }));

    // A production line consumes only the production slot; stranded miners
    // must keep rotating while work is active, held, cash-paused or exit-blocked.
    for (int32 JobState : { 0, 1, 2, 3 })
    {
        FProofSnapshot WithJob = Budget;
        FProofProductionJob Job;
        Job.OwnerId = 100;
        Job.StartSequence = 1;
        Job.FactoryId = 2;
        Job.bHeld = JobState == 1;
        Job.WorkMicroseconds = JobState == 3 ? FProofCommandModel::ProductionWorkMicroseconds : 0;
        Job.PaidCredits = JobState == 3 ? FProofCommandModel::ProductionCost : 0;
        WithJob.ProductionJobs.Add(Job);
        if (JobState == 2) WithJob.Players[0].Credits = 0;
        FProofSkirmishBot WorkingBot;
        const TArray<FProofCommand> Working = WorkingBot.Decide(WithJob, 100, 0.0);
        TestTrue(TEXT("Own production job preserves one cargo return and blocks duplicate production"),
            Working.Num() <= 4 && FinalCargoCount(Working, EProofCommandType::ReturnCargo) == 1
            && FinalCargoCount(Working, EProofCommandType::Produce) == 0);
        const TArray<FProofCommand> Rotated = WorkingBot.Decide(WithJob, 100, 1.0);
        const FProofCommand* FirstReturn = FindFinalCargoIntent(Working, EProofCommandType::ReturnCargo);
        const FProofCommand* NextReturn = FindFinalCargoIntent(Rotated, EProofCommandType::ReturnCargo);
        TestTrue(TEXT("Cargo rotation continues alongside the owner's production job"),
            FirstReturn && NextReturn && FirstReturn->UnitId != NextReturn->UnitId
            && Rotated.Num() <= 4 && FinalCargoCount(Rotated, EProofCommandType::Produce) == 0);
    }
    FProofSnapshot ForeignJob = Budget;
    FProofProductionJob OtherJob;
    OtherJob.OwnerId = 1;
    ForeignJob.ProductionJobs.Add(OtherJob);
    FProofSkirmishBot ForeignJobBot;
    const TArray<FProofCommand> Independent = ForeignJobBot.Decide(ForeignJob, 100, 0.0);
    TestTrue(TEXT("Another owner's production does not suppress own production or cargo recovery"),
        Independent.Num() <= 4 && FinalCargoCount(Independent, EProofCommandType::Produce) == 1
        && FinalCargoCount(Independent, EProofCommandType::ReturnCargo) == 1);
    return true;
}
#endif
