#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"
#include "Presentation/ProofSelectionState.h"
#include "Presentation/ProofGroupOrderQueue.h"
#include <limits>

namespace
{
FProofCommand AttackMoveOrder(int32 Sequence, EProofCommandType Type, int32 Unit = INDEX_NONE,
    FVector Destination = FVector::ZeroVector)
{
    FProofCommand C; C.Sequence = Sequence; C.Type = Type; C.UnitId = Unit; C.Destination = Destination; return C;
}
const FProofUnitRecord& AttackMoveUnit(const FProofSnapshot& S, int32 Id)
{
    return *S.Units.FindByPredicate([Id](const FProofUnitRecord& U) { return U.UnitId == Id; });
}
int32 AttackMoveKind(const FProofSnapshot& S, int32 Owner, EProofUnitKind Kind)
{
    const auto* U = S.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& R) { return R.OwnerId == Owner && R.Kind == Kind; });
    return U ? U->UnitId : INDEX_NONE;
}
bool AttackMoveSetup(FProofCommandModel& M, int32& Combat, int32& Enemy)
{
    M.StartSkirmish();
    if (!M.SeedSkirmishPlayer(1, 0, FVector::ZeroVector) || !M.SeedSkirmishPlayer(100, 1, FVector(1800, 300, 0))) return false;
    if (!M.Submit(1, AttackMoveOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, FVector(0, 300, 0))).bAccepted) return false;
    const int32 Factory = AttackMoveKind(M.GetSnapshot(), 1, EProofUnitKind::Factory);
    if (!M.Submit(1, AttackMoveOrder(2, EProofCommandType::Produce, Factory)).bAccepted) return false;
    M.Advance(FProofCommandModel::ProductionDurationSeconds);
    Combat = AttackMoveKind(M.GetSnapshot(), 1, EProofUnitKind::Combat);
    Enemy = AttackMoveKind(M.GetSnapshot(), 100, EProofUnitKind::Harvester);
    if (!M.Submit(100, AttackMoveOrder(1, EProofCommandType::Move, Enemy, FVector(900, 300, 0))).bAccepted) return false;
    M.Advance(10.f); // Target reaches 600 cm from the stationary combat; no setup firing.
    return Combat > 0 && Enemy > 0 && AttackMoveUnit(M.GetSnapshot(), Combat).Position == FVector(300, 300, 0)
        && AttackMoveUnit(M.GetSnapshot(), Enemy).HitPoints == 100.f;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAttackMoveTravelTest, "RedalertProof.AttackMove.TravelPauseResume", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAttackMoveTravelTest::RunTest(const FString&)
{
    FProofCommandModel M; int32 Combat, Enemy;
    if (!TestTrue(TEXT("Real factory/production/target Move setup"), AttackMoveSetup(M, Combat, Enemy))) return false;
    const FVector Destination(1400, 300, 0);
    TestTrue(TEXT("Attack Move admitted through shared model"), M.Submit(1, AttackMoveOrder(3, EProofCommandType::AttackMove, Combat, Destination)).bAccepted);
    M.Advance(1.f);
    TestEqual(TEXT("Travel uses existing 150 cm/s"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, FVector(450, 300, 0));
    TestEqual(TEXT("No out-of-range shot"), AttackMoveUnit(M.GetSnapshot(), Enemy).HitPoints, 100.f);
    M.Advance(1.f);
    TestEqual(TEXT("Travel enters the closed 300 cm range"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, FVector(600, 300, 0));
    TestEqual(TEXT("Existing damage applied once"), AttackMoveUnit(M.GetSnapshot(), Enemy).HitPoints, 75.f);
    M.Advance(1.f);
    TestEqual(TEXT("Acquired enemy holds locomotion"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, FVector(600, 300, 0));
    TestEqual(TEXT("Stationary fire uses existing DPS"), AttackMoveUnit(M.GetSnapshot(), Enemy).HitPoints, 50.f);
    M.Advance(2.f);
    TestEqual(TEXT("Actual target dies before resume"), AttackMoveUnit(M.GetSnapshot(), Enemy).HitPoints, 0.f);
    M.Advance(1.f);
    TestEqual(TEXT("Original route resumes after death"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, FVector(750, 300, 0));
    M.Advance(5.f);
    TestEqual(TEXT("Destination is reached exactly without overshoot"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, Destination);
    M.Advance(1.f);
    TestEqual(TEXT("Finished route remains stationary"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, Destination);
    TestEqual(TEXT("No resource mutation"), M.GetSnapshot().Players[0].Credits, 150);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAttackMoveRangeTest, "RedalertProof.AttackMove.TargetLeavesRange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAttackMoveRangeTest::RunTest(const FString&)
{
    FProofCommandModel M; int32 Combat, Enemy;
    if (!TestTrue(TEXT("Setup"), AttackMoveSetup(M, Combat, Enemy))) return false;
    TestTrue(TEXT("Approach target with ordinary Move"), M.Submit(1, AttackMoveOrder(3, EProofCommandType::Move, Combat, FVector(600, 300, 0))).bAccepted);
    M.Advance(2.f);
    const float HP = AttackMoveUnit(M.GetSnapshot(), Enemy).HitPoints;
    TestTrue(TEXT("Route points away from target"), M.Submit(1, AttackMoveOrder(4, EProofCommandType::AttackMove, Combat, FVector(-1000, 300, 0))).bAccepted);
    TestTrue(TEXT("Enemy receives its own real escape Move"), M.Submit(100, AttackMoveOrder(2, EProofCommandType::Move, Enemy, FVector(1400, 300, 0))).bAccepted);
    M.Advance(1.f);
    TestEqual(TEXT("Pre-move acquisition pauses this tick"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, FVector(600, 300, 0));
    TestEqual(TEXT("Post-move range forbids stale shot"), AttackMoveUnit(M.GetSnapshot(), Enemy).HitPoints, HP);
    M.Advance(1.f);
    TestEqual(TEXT("Leaving range resumes original westward destination, never pursuit"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, FVector(450, 300, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAttackMoveTieTest, "RedalertProof.AttackMove.DeterministicTeams", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAttackMoveTieTest::RunTest(const FString&)
{
    FProofCommandModel M; M.StartSkirmish();
    TestTrue(TEXT("Own player"), M.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
    TestTrue(TEXT("First enemy"), M.SeedSkirmishPlayer(100, 1, FVector(500, 100, 0)));
    TestTrue(TEXT("Second enemy"), M.SeedSkirmishPlayer(200, 1, FVector(500, 500, 0)));
    TestTrue(TEXT("Factory"), M.Submit(1, AttackMoveOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, FVector(0, 300, 0))).bAccepted);
    TestTrue(TEXT("Combat"), M.Submit(1, AttackMoveOrder(2, EProofCommandType::Produce, AttackMoveKind(M.GetSnapshot(), 1, EProofUnitKind::Factory))).bAccepted);
    M.Advance(FProofCommandModel::ProductionDurationSeconds);
    const int32 Combat = AttackMoveKind(M.GetSnapshot(), 1, EProofUnitKind::Combat);
    TestEqual(TEXT("Exact setup spawn"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, FVector(300, 300, 0));
    TestTrue(TEXT("Attack Move"), M.Submit(1, AttackMoveOrder(3, EProofCommandType::AttackMove, Combat, FVector(1200, 300, 0))).bAccepted);
    M.Advance(1.f);
    TestEqual(TEXT("Lowest ID wins equal distance"), AttackMoveUnit(M.GetSnapshot(), AttackMoveKind(M.GetSnapshot(), 100, EProofUnitKind::HQ)).HitPoints, 275.f);
    TestEqual(TEXT("No second automatic shot"), AttackMoveUnit(M.GetSnapshot(), AttackMoveKind(M.GetSnapshot(), 200, EProofUnitKind::HQ)).HitPoints, 300.f);
    const int32 Closer = AttackMoveKind(M.GetSnapshot(), 200, EProofUnitKind::Harvester);
    TestTrue(TEXT("Other enemy moves a closer target into range"), M.Submit(200, AttackMoveOrder(1, EProofCommandType::Move, Closer, FVector(400, 300, 0))).bAccepted);
    M.Advance(1.f);
    TestEqual(TEXT("Current in-range target remains preferred over newly closer enemy"), AttackMoveUnit(M.GetSnapshot(), AttackMoveKind(M.GetSnapshot(), 100, EProofUnitKind::HQ)).HitPoints, 250.f);
    TestEqual(TEXT("No double shot at closer enemy"), AttackMoveUnit(M.GetSnapshot(), Closer).HitPoints, 100.f);
    TestTrue(TEXT("Fresh Attack Move clears prior acquisition"), M.Submit(1, AttackMoveOrder(4, EProofCommandType::AttackMove, Combat, FVector(1200, 300, 0))).bAccepted);
    M.Advance(1.f);
    TestEqual(TEXT("Fresh acquisition chooses nearer enemy"), AttackMoveUnit(M.GetSnapshot(), Closer).HitPoints, 75.f);
    TestEqual(TEXT("Former target receives no second shot"), AttackMoveUnit(M.GetSnapshot(), AttackMoveKind(M.GetSnapshot(), 100, EProofUnitKind::HQ)).HitPoints, 250.f);
    FProofCommandModel Friends; Friends.StartSkirmish();
    Friends.SeedSkirmishPlayer(1, 0, FVector::ZeroVector); Friends.SeedSkirmishPlayer(2, 0, FVector(300, 600, 0)); Friends.SeedSkirmishPlayer(100, 1, FVector(5000, 0, 0));
    Friends.Submit(1, AttackMoveOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, FVector(0, 300, 0)));
    Friends.Submit(1, AttackMoveOrder(2, EProofCommandType::Produce, AttackMoveKind(Friends.GetSnapshot(), 1, EProofUnitKind::Factory)));
    Friends.Advance(FProofCommandModel::ProductionDurationSeconds);
    const int32 FriendlyCombat = AttackMoveKind(Friends.GetSnapshot(), 1, EProofUnitKind::Combat);
    TestTrue(TEXT("Friendly-only nearby route"), Friends.Submit(1, AttackMoveOrder(3, EProofCommandType::AttackMove, FriendlyCombat, FVector(300, 1000, 0))).bAccepted);
    Friends.Advance(1.f);
    TestEqual(TEXT("Friendly team does not interrupt travel"), AttackMoveUnit(Friends.GetSnapshot(), FriendlyCombat).Position, FVector(300, 450, 0));
    TestEqual(TEXT("Friendly HQ never damaged"), AttackMoveUnit(Friends.GetSnapshot(), AttackMoveKind(Friends.GetSnapshot(), 2, EProofUnitKind::HQ)).HitPoints, 300.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAttackMoveStopTest, "RedalertProof.AttackMove.StopAndReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAttackMoveStopTest::RunTest(const FString&)
{
    FProofCommandModel M; int32 Combat, Enemy;
    if (!TestTrue(TEXT("Setup"), AttackMoveSetup(M, Combat, Enemy))) return false;
    M.Submit(1, AttackMoveOrder(3, EProofCommandType::AttackMove, Combat, FVector(1400, 300, 0)));
    M.Advance(1.f);
    TestTrue(TEXT("Stop overrides traveling Attack Move"), M.Submit(1, AttackMoveOrder(4, EProofCommandType::Stop, Combat)).bAccepted);
    M.Advance(5.f);
    TestEqual(TEXT("Stopped route never resumes"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, FVector(450, 300, 0));
    TestTrue(TEXT("Fresh Attack Move replaces Stop"), M.Submit(1, AttackMoveOrder(5, EProofCommandType::AttackMove, Combat, FVector(-1000, 300, 0))).bAccepted);
    TestTrue(TEXT("Ordinary Move replaces Attack Move"), M.Submit(1, AttackMoveOrder(6, EProofCommandType::Move, Combat, FVector(450, 600, 0))).bAccepted);
    M.Advance(1.f);
    TestEqual(TEXT("Only final Move executes"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, FVector(450, 450, 0));
    TestTrue(TEXT("Attack Move can replace active Move"), M.Submit(1, AttackMoveOrder(7, EProofCommandType::AttackMove, Combat, FVector(-1000, 450, 0))).bAccepted);
    FProofCommand Attack = AttackMoveOrder(8, EProofCommandType::Attack, Combat); Attack.TargetUnitId = Enemy;
    TestTrue(TEXT("Explicit Attack can replace Attack Move"), M.Submit(1, Attack).bAccepted);
    M.Advance(1.f);
    const FVector Pursuing = AttackMoveUnit(M.GetSnapshot(), Combat).Position;
    TestTrue(TEXT("Only explicit target chase executes, not either old route"), Pursuing.X > 450.0 && Pursuing.Y < 450.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAttackMoveValidationTest, "RedalertProof.AttackMove.ValidationAndOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAttackMoveValidationTest::RunTest(const FString&)
{
    FProofCommandModel M; int32 Combat, Enemy;
    if (!TestTrue(TEXT("Setup"), AttackMoveSetup(M, Combat, Enemy))) return false;
    TestEqual(TEXT("Stop ordinal preserved"), static_cast<uint8>(EProofCommandType::Stop), static_cast<uint8>(6));
    TestEqual(TEXT("Attack Move reserves ordinal after SetRally"), static_cast<uint8>(EProofCommandType::AttackMove), static_cast<uint8>(8));
    const FString Before = M.GetSnapshot().ComputeDigest();
    FProofCommand Targeted = AttackMoveOrder(3, EProofCommandType::AttackMove, Combat); Targeted.TargetUnitId = Enemy;
    TestEqual(TEXT("Ground Attack Move rejects a target payload before consuming sequence"), M.Submit(1, Targeted).Reason, FString(TEXT("malformed")));
    TestEqual(TEXT("Nonfinite shape rejected without sequence consumption"), M.Submit(1, AttackMoveOrder(3, EProofCommandType::AttackMove, Combat, FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0))).Reason, FString(TEXT("malformed")));
    TestEqual(TEXT("Foreign actor rejected"), M.Submit(1, AttackMoveOrder(3, EProofCommandType::AttackMove, Enemy, FVector(1000, 0, 0))).Reason, FString(TEXT("ownership")));
    TestEqual(TEXT("Gameplay rejection consumed sequence"), M.Submit(1, AttackMoveOrder(3, EProofCommandType::AttackMove, Combat)).Reason, FString(TEXT("sequence")));
    TestEqual(TEXT("Harvester cannot Attack Move"), M.Submit(1, AttackMoveOrder(4, EProofCommandType::AttackMove, AttackMoveKind(M.GetSnapshot(), 1, EProofUnitKind::Harvester))).Reason, FString(TEXT("role")));
    TestEqual(TEXT("HQ cannot Attack Move"), M.Submit(1, AttackMoveOrder(5, EProofCommandType::AttackMove, AttackMoveKind(M.GetSnapshot(), 1, EProofUnitKind::HQ))).Reason, FString(TEXT("role")));
    TestEqual(TEXT("Out of world rejected"), M.Submit(1, AttackMoveOrder(6, EProofCommandType::AttackMove, Combat, FVector(10001, 0, 0))).Reason, FString(TEXT("position")));
    TestEqual(TEXT("All rejected commands preserve visible state"), M.GetSnapshot().ComputeDigest(), Before);
    FProofCommandModel Legacy; Legacy.AddPlayer(1); const int32 Id = Legacy.SeedUnit(1, FVector::ZeroVector);
    TestEqual(TEXT("Legacy fixture remains outside scoped skirmish feature"), Legacy.Submit(1, AttackMoveOrder(1, EProofCommandType::AttackMove, Id)).Reason, FString(TEXT("malformed")));
    TestTrue(TEXT("Legacy rejection did not consume sequence"), Legacy.Submit(1, AttackMoveOrder(1, EProofCommandType::Move, Id)).bAccepted);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAttackMoveRateTest, "RedalertProof.AttackMove.BotParityAndRate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAttackMoveRateTest::RunTest(const FString&)
{
    FProofCommandModel M(2); int32 Combat, Enemy;
    if (!TestTrue(TEXT("Setup"), AttackMoveSetup(M, Combat, Enemy))) return false;
    TestTrue(TEXT("Bot factory via same trusted route"), M.Submit(100, AttackMoveOrder(2, EProofCommandType::BuildFactory, INDEX_NONE, FVector(1800, 600, 0))).bAccepted);
    TestTrue(TEXT("Bot combat via same trusted route"), M.Submit(100, AttackMoveOrder(3, EProofCommandType::Produce, AttackMoveKind(M.GetSnapshot(), 100, EProofUnitKind::Factory))).bAccepted);
    M.Advance(FProofCommandModel::ProductionDurationSeconds);
    const int32 Bot = AttackMoveKind(M.GetSnapshot(), 100, EProofUnitKind::Combat);
    TestTrue(TEXT("Human Attack Move accepted"), M.Submit(1, AttackMoveOrder(3, EProofCommandType::AttackMove, Combat, FVector(-1000, 300, 0))).bAccepted);
    TestTrue(TEXT("Bot Attack Move accepted"), M.Submit(100, AttackMoveOrder(4, EProofCommandType::AttackMove, Bot, FVector(3000, 600, 0))).bAccepted);
    TestTrue(TEXT("Human Stop remains within allowance"), M.Submit(1, AttackMoveOrder(4, EProofCommandType::Stop, Combat)).bAccepted);
    TestEqual(TEXT("Attack Move cannot bypass rate allowance"), M.Submit(1, AttackMoveOrder(5, EProofCommandType::AttackMove, Combat)).Reason, FString(TEXT("rate")));
    TestEqual(TEXT("Rate rejection consumed envelope"), M.Submit(1, AttackMoveOrder(5, EProofCommandType::Stop, Combat)).Reason, FString(TEXT("sequence")));
    M.Advance(0.f);
    TestTrue(TEXT("Next ordered envelope after real reset"), M.Submit(1, AttackMoveOrder(6, EProofCommandType::AttackMove, Combat, FVector(-1000, 300, 0))).bAccepted);
    TestTrue(TEXT("Hold own combat for simultaneous-damage setup"), M.Submit(1, AttackMoveOrder(7, EProofCommandType::Stop, Combat)).bAccepted);
    TestTrue(TEXT("Bot moves through same validator"), M.Submit(100, AttackMoveOrder(5, EProofCommandType::Move, Bot, FVector(500, 300, 0))).bAccepted);
    for (int32 Tick = 0; Tick < 120 && AttackMoveUnit(M.GetSnapshot(), Bot).Position != FVector(500, 300, 0); ++Tick) M.Advance(0.1f);
    if (!TestEqual(TEXT("Bot actually reaches test engagement"), AttackMoveUnit(M.GetSnapshot(), Bot).Position, FVector(500, 300, 0))) return false;
    if (!TestTrue(TEXT("Both attackers alive before mutually lethal advance"), AttackMoveUnit(M.GetSnapshot(), Combat).HitPoints > 0.f && AttackMoveUnit(M.GetSnapshot(), Bot).HitPoints > 0.f)) return false;
    TestTrue(TEXT("Human Attack Move reacquires nearby bot"), M.Submit(1, AttackMoveOrder(8, EProofCommandType::AttackMove, Combat, FVector(-1000, 300, 0))).bAccepted);
    FProofCommand BotAttack = AttackMoveOrder(6, EProofCommandType::Attack, Bot); BotAttack.TargetUnitId = Combat;
    TestTrue(TEXT("Bot explicit attack admitted"), M.Submit(100, BotAttack).bAccepted);
    M.Advance(4.f);
    TestEqual(TEXT("Attack Move shooter dies in simultaneous damage"), AttackMoveUnit(M.GetSnapshot(), Combat).HitPoints, 0.f);
    TestEqual(TEXT("Explicit shooter also dies in same advance"), AttackMoveUnit(M.GetSnapshot(), Bot).HitPoints, 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAttackMoveLifecycleTest, "RedalertProof.AttackMove.DisconnectAndReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAttackMoveLifecycleTest::RunTest(const FString&)
{
    FProofCommandModel M; int32 Combat, Enemy;
    if (!TestTrue(TEXT("Setup"), AttackMoveSetup(M, Combat, Enemy))) return false;
    TestTrue(TEXT("Other enemy seat keeps match playing"), M.SeedSkirmishPlayer(200, 1, FVector(5000, 0, 0)));
    M.Submit(1, AttackMoveOrder(3, EProofCommandType::AttackMove, Combat, FVector(1400, 300, 0))); M.Advance(2.f);
    const FVector Paused = AttackMoveUnit(M.GetSnapshot(), Combat).Position;
    TestTrue(TEXT("Target owner actually disconnects"), M.RemoveSkirmishPlayer(100));
    M.Advance(1.f);
    TestEqual(TEXT("Missing target resumes remaining route"), AttackMoveUnit(M.GetSnapshot(), Combat).Position, Paused + FVector(150, 0, 0));
    TestTrue(TEXT("Remove attacking owner"), M.RemoveSkirmishPlayer(1));
    TestFalse(TEXT("No ghost actor"), M.GetSnapshot().Units.ContainsByPredicate([Combat](const FProofUnitRecord& U) { return U.UnitId == Combat; }));
    TestEqual(TEXT("Last allied HQ removal finishes match"), M.GetSnapshot().MatchPhase, EProofMatchPhase::Finished);
    const FString TerminalDigest = M.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Terminal match rejects new Attack Move"), M.Submit(200, AttackMoveOrder(1, EProofCommandType::AttackMove, Enemy)).Reason, FString(TEXT("finished")));
    M.Advance(100.f);
    TestEqual(TEXT("Terminal Advance cannot revive any route"), M.GetSnapshot().ComputeDigest(), TerminalDigest);
    M.Reset(); M.AddPlayer(1); const int32 Fresh = M.SeedUnit(1, FVector::ZeroVector); M.Advance(1.f);
    TestEqual(TEXT("Reset cannot leave an Attack Move for reused unit IDs"), AttackMoveUnit(M.GetSnapshot(), Fresh).Position, FVector::ZeroVector);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAttackMoveSelectionTest, "RedalertProof.AttackMove.SelectionAndQueue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAttackMoveSelectionTest::RunTest(const FString&)
{
    FProofCommandModel M; int32 Combat, Enemy;
    if (!TestTrue(TEXT("Setup"), AttackMoveSetup(M, Combat, Enemy))) return false;
    const FProofSnapshot S = M.GetSnapshot();
    FProofSelectionState Selection; Selection.SelectSingle(S, 1, Combat);
    Selection.Toggle(S, 1, AttackMoveKind(S, 1, EProofUnitKind::Harvester)); Selection.Toggle(S, 1, AttackMoveKind(S, 1, EProofUnitKind::Factory));
    const TArray<FProofCommand> Plans = Selection.PlanAttackMove(S, 1, FVector(1400, 300, 0));
    if (!TestEqual(TEXT("Mixed selection plans only Combat"), Plans.Num(), 1)) return false;
    TestEqual(TEXT("Correct shared command template"), Plans[0].Type, EProofCommandType::AttackMove);
    TestEqual(TEXT("No preallocated sequence"), Plans[0].Sequence, 0);
    FProofGroupOrderQueue Q;
    TestTrue(TEXT("Queue shared Attack Move"), Q.Enqueue(S, 1, Plans));
    TestTrue(TEXT("Stop atomically replaces pending Attack Move"), Q.Enqueue(S, 1, Selection.PlanStop(S, 1)));
    const TArray<FProofCommand> Stops = Q.PopReady(S, 1, 1.0);
    if (!TestEqual(TEXT("Two mobile Stop intents fit existing per-interval allowance"), Stops.Num(), 2)) return false;
    TestEqual(TEXT("First Stop is original Combat"), Stops[0].Type, EProofCommandType::Stop);
    TestTrue(TEXT("Queue another Attack Move after dispatch"), Q.Enqueue(S, 1, Plans));
    TestTrue(TEXT("No dispatch-clock reset when order changes"), Q.PopReady(S, 1, 1.01).IsEmpty());
    const auto Ready = Q.PopReady(S, 1, 1.1);
    if (!TestEqual(TEXT("Original .1 second pacing retained"), Ready.Num(), 1)) return false;
    TestEqual(TEXT("Same command survives queue"), Ready[0].Type, EProofCommandType::AttackMove);
    TestEqual(TEXT("Queue never allocates sequence"), Ready[0].Sequence, 0);
    FProofGroupOrderQueue Atomic;
    TestTrue(TEXT("Initial pending Attack Move"), Atomic.Enqueue(S, 1, Plans));
    FProofCommand Changed = Plans[0]; Changed.Destination = FVector(-1000, 300, 0);
    FProofCommand Invalid = Plans[0]; Invalid.UnitId = AttackMoveKind(S, 1, EProofUnitKind::Harvester);
    TestFalse(TEXT("Invalid mixed batch rejected atomically"), Atomic.Enqueue(S, 1, {Changed, Invalid}));
    const auto Retained = Atomic.PopReady(S, 1, 1.0);
    if (!TestEqual(TEXT("Rejected batch preserved exactly one intent"), Retained.Num(), 1)) return false;
    TestEqual(TEXT("Rejected batch cannot alter previous destination"), Retained[0].Destination, Plans[0].Destination);
    FProofSnapshot Finished = S; Finished.MatchPhase = EProofMatchPhase::Finished;
    TestTrue(TEXT("Terminal state cannot plan Attack Move"), Selection.PlanAttackMove(Finished, 1, FVector::ZeroVector).IsEmpty());
    TestTrue(TEXT("Stale epoch cannot plan Attack Move"), Selection.PlanAttackMove(FProofSnapshot(), 1, FVector::ZeroVector).IsEmpty());
    return true;
}
#endif
