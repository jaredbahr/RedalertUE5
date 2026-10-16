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
    TestEqual(TEXT("Gather and produce share a bounded batch"), Economy.Num(), 2);
    if (Economy.Num() == 2)
    {
        TestTrue(TEXT("Harvester gathers a real resource"), Economy[0].Type == EProofCommandType::Gather
            && Economy[0].UnitId == 2 && Economy[0].TargetUnitId == 1);
        TestTrue(TEXT("Factory produces a combat unit"), Economy[1].Type == EProofCommandType::Produce && Economy[1].UnitId == 6);
        TestEqual(TEXT("Sequence continues across decisions"), Economy[0].Sequence, 3);
        TestEqual(TEXT("Sequence remains contiguous"), Economy[1].Sequence, 4);
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
    TestEqual(TEXT("Twelve live records prevent production"), CappedBot.Decide(AtCap, 100, 0.0).Num(), 0);
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
                if (Command.Type == EProofCommandType::Gather) { ++Gathers; }
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
    TestTrue(TEXT("Ore was actually extracted"), bOreExtracted);
    const FProofSnapshot Final = Model.GetSnapshot();
    const FProofPlayerRecord* BotPlayer = Final.Players.FindByPredicate(
        [](const FProofPlayerRecord& P) { return P.OwnerId == 100; });
    TestTrue(TEXT("Harvested ore was deposited as earned credits"), BotPlayer && BotPlayer->TotalGathered > 0);
    TestTrue(TEXT("Factory spent earned credits beyond two starting-credit units"), Produces > 2);
    TestTrue(TEXT("Combat units attacked through the model"), Attacks > 0);
    return true;
}
#endif
