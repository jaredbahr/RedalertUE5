#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"

namespace
{
FProofCommand IdleCombatOrder(int32 Sequence, EProofCommandType Type, int32 UnitId = INDEX_NONE,
    int32 TargetId = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand C;
    C.Sequence = Sequence; C.Type = Type; C.UnitId = UnitId;
    C.TargetUnitId = TargetId; C.Destination = Destination;
    return C;
}
const FProofUnitRecord& IdleCombatUnit(const FProofSnapshot& Snapshot, int32 Id)
{
    return *Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& U) { return U.UnitId == Id; });
}
int32 IdleCombatKind(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* Found = Snapshot.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& U)
        { return U.OwnerId == Owner && U.Kind == Kind; });
    return Found ? Found->UnitId : INDEX_NONE;
}
void IdleCombatPair(FProofCommandModel& Model, int32& A, int32& B)
{
    Model.StartSkirmish();
    Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Model.SeedSkirmishPlayer(2, 1, FVector(3000, 0, 0));
    Model.Submit(1, IdleCombatOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0)));
    Model.Submit(2, IdleCombatOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(2700, 0, 0)));
    Model.Submit(1, IdleCombatOrder(2, EProofCommandType::Produce, IdleCombatKind(Model.GetSnapshot(), 1, EProofUnitKind::Factory)));
    Model.Submit(2, IdleCombatOrder(2, EProofCommandType::Produce, IdleCombatKind(Model.GetSnapshot(), 2, EProofUnitKind::Factory)));
    Model.Advance(FProofCommandModel::ProductionDurationSeconds);
    A = IdleCombatKind(Model.GetSnapshot(), 1, EProofUnitKind::Combat);
    B = IdleCombatKind(Model.GetSnapshot(), 2, EProofUnitKind::Combat);
    Model.Submit(1, IdleCombatOrder(3, EProofCommandType::Move, A, INDEX_NONE, FVector(1000, 0, 0)));
    Model.Submit(2, IdleCombatOrder(3, EProofCommandType::Move, B, INDEX_NONE, FVector(1450, 0, 0)));
    Model.Advance(20.f); // Final separation is 450: setup cannot cause auto-fire.
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofIdleRangeTest, "RedalertProof.Fidelity.IdleCombat.RangeTeamsAndRoles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofIdleRangeTest::RunTest(const FString&)
{
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        FProofCommandModel Model;
        Model.StartSkirmish();
        const double EnemyX = Variant == 1 ? 900.001 : 900.0;
        Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
        Model.SeedSkirmishPlayer(2, Variant == 2 ? 0 : 1, FVector(EnemyX, 0, 0));
        Model.Submit(1, IdleCombatOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0)));
        Model.Submit(1, IdleCombatOrder(2, EProofCommandType::Produce, IdleCombatKind(Model.GetSnapshot(), 1, EProofUnitKind::Factory)));
        Model.Advance(FProofCommandModel::ProductionDurationSeconds);
        const FProofSnapshot Before = Model.GetSnapshot();
        const int32 Combat = IdleCombatKind(Before, 1, EProofUnitKind::Combat);
        const int32 HQ = IdleCombatKind(Before, 2, EProofUnitKind::HQ);
        Model.Advance(1.f);
        const FProofSnapshot After = Model.GetSnapshot();
        TestEqual(TEXT("Only opposing target at closed range receives idle damage"), IdleCombatUnit(After, HQ).HitPoints, Variant == 0 ? 275.f : 300.f);
        TestEqual(TEXT("Idle fire never moves the shooter"), IdleCombatUnit(After, Combat).Position, IdleCombatUnit(Before, Combat).Position);
        TestEqual(TEXT("HQ and Harvester do not automatically shoot"), IdleCombatUnit(After, Combat).HitPoints, 100.f);
    }
    FProofCommandModel Roles;
    Roles.StartSkirmish();
    Roles.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Roles.SeedSkirmishPlayer(2, 1, FVector(900, 0, 0));
    Roles.Submit(1, IdleCombatOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0)));
    Roles.Submit(1, IdleCombatOrder(2, EProofCommandType::Produce, IdleCombatKind(Roles.GetSnapshot(), 1, EProofUnitKind::Factory)));
    Roles.Advance(FProofCommandModel::ProductionDurationSeconds);
    Roles.Submit(2, IdleCombatOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(700, 0, 0)));
    const int32 Harvester = IdleCombatKind(Roles.GetSnapshot(), 2, EProofUnitKind::Harvester);
    const int32 Combat = IdleCombatKind(Roles.GetSnapshot(), 1, EProofUnitKind::Combat);
    Roles.Submit(2, IdleCombatOrder(2, EProofCommandType::Move, Harvester, INDEX_NONE, FVector(650, 0, 0)));
    Roles.Advance(3.f);
    TestEqual(TEXT("Nearby idle factory and completed Harvester cannot fire"), IdleCombatUnit(Roles.GetSnapshot(), Combat).HitPoints, 100.f);
    TestEqual(TEXT("Generic Harvester remains an unarmed prototype role"), IdleCombatUnit(Roles.GetSnapshot(), Harvester).HitPoints, 25.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofIdleOrdersTest, "RedalertProof.Fidelity.IdleCombat.OrdersAndSimultaneity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofIdleOrdersTest::RunTest(const FString&)
{
    FProofCommandModel Model; int32 A, B;
    IdleCombatPair(Model, A, B);
    TestEqual(TEXT("Setup leaves both combat units undamaged"), IdleCombatUnit(Model.GetSnapshot(), A).HitPoints, 100.f);
    TestTrue(TEXT("Move into weapon range accepted"), Model.Submit(1, IdleCombatOrder(4, EProofCommandType::Move, A, INDEX_NONE, FVector(1150, 0, 0))).bAccepted);
    Model.Advance(1.f);
    const FProofSnapshot Arrived = Model.GetSnapshot();
    TestEqual(TEXT("Completed move can fire on its arrival tick"), IdleCombatUnit(Arrived, B).HitPoints, 75.f);
    TestEqual(TEXT("Both completed movers fire using pre-damage health"), IdleCombatUnit(Arrived, A).HitPoints, 75.f);
    Model.Advance(3.f);
    const FProofSnapshot Dead = Model.GetSnapshot();
    TestEqual(TEXT("First idle attacker takes simultaneous lethal damage"), IdleCombatUnit(Dead, A).HitPoints, 0.f);
    TestEqual(TEXT("Second idle attacker still fires its lethal shot"), IdleCombatUnit(Dead, B).HitPoints, 0.f);

    FProofCommandModel Moving; IdleCombatPair(Moving, A, B);
    Moving.Submit(1, IdleCombatOrder(4, EProofCommandType::Move, A, INDEX_NONE, FVector(1600, 0, 0)));
    Moving.Advance(1.f); // A reaches 1150 but still has an active destination.
    const FProofSnapshot EnRoute = Moving.GetSnapshot();
    TestEqual(TEXT("Active move suppresses auto-fire"), IdleCombatUnit(EnRoute, B).HitPoints, 100.f);
    TestEqual(TEXT("Idle enemy may fire at the moving unit"), IdleCombatUnit(EnRoute, A).HitPoints, 75.f);

    FProofCommandModel Priority; IdleCombatPair(Priority, A, B);
    Priority.Submit(1, IdleCombatOrder(4, EProofCommandType::Move, A, INDEX_NONE, FVector(1150, 0, 0)));
    Priority.Advance(1.f);
    const int32 EnemyHQ = IdleCombatKind(Priority.GetSnapshot(), 2, EProofUnitKind::HQ);
    Priority.Submit(1, IdleCombatOrder(5, EProofCommandType::Attack, A, EnemyHQ));
    Priority.Advance(.1f);
    TestEqual(TEXT("Explicit distant attack forbids fallback fire at nearer enemy"), IdleCombatUnit(Priority.GetSnapshot(), B).HitPoints, 75.f);
    TestTrue(TEXT("Explicit attack still pursues its selected target"), IdleCombatUnit(Priority.GetSnapshot(), A).Position.X > 1150.0);

    FProofCommandModel SingleShot; IdleCombatPair(SingleShot, A, B);
    SingleShot.Submit(1, IdleCombatOrder(4, EProofCommandType::Move, A, INDEX_NONE, FVector(1150, 0, 0)));
    SingleShot.Advance(1.f);
    SingleShot.Submit(1, IdleCombatOrder(5, EProofCommandType::Attack, A, B));
    SingleShot.Advance(.1f);
    TestEqual(TEXT("In-range explicit attack never stacks automatic damage"), IdleCombatUnit(SingleShot.GetSnapshot(), B).HitPoints, 72.5f);

    FProofCommandModel Fractional;
    Fractional.StartSkirmish();
    Fractional.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Fractional.SeedSkirmishPlayer(2, 1, FVector(900, 0, 0));
    Fractional.Submit(1, IdleCombatOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0)));
    Fractional.Submit(1, IdleCombatOrder(2, EProofCommandType::Produce, IdleCombatKind(Fractional.GetSnapshot(), 1, EProofUnitKind::Factory)));
    Fractional.Advance(FProofCommandModel::ProductionDurationSeconds);
    const int32 FractionalCombat = IdleCombatKind(Fractional.GetSnapshot(), 1, EProofUnitKind::Combat);
    const FVector Destination(650.000164, 17.3, 0);
    Fractional.Submit(1, IdleCombatOrder(3, EProofCommandType::Move, FractionalCombat, INDEX_NONE, Destination));
    Fractional.Advance(1.f);
    TestEqual(TEXT("Fractional diagonal Move lands exactly at requested endpoint"), IdleCombatUnit(Fractional.GetSnapshot(), FractionalCombat).Position, Destination);
    TestEqual(TEXT("Fractional completed Move is eligible for idle fire"), IdleCombatUnit(Fractional.GetSnapshot(), IdleCombatKind(Fractional.GetSnapshot(), 2, EProofUnitKind::HQ)).HitPoints, 275.f);
    const FVector TinyDestination = Destination + FVector(1.e-10, 0, 0);
    TestTrue(TEXT("Tiny endpoint is distinct but below movement division threshold"),
        TinyDestination != Destination && FVector::Dist(TinyDestination, Destination) < UE_SMALL_NUMBER);
    TestTrue(TEXT("Tiny Move accepted"), Fractional.Submit(1,
        IdleCombatOrder(4, EProofCommandType::Move, FractionalCombat, INDEX_NONE, TinyDestination)).bAccepted);
    Fractional.Advance(1.f);
    TestEqual(TEXT("Sub-epsilon Move completes exactly without division"), IdleCombatUnit(Fractional.GetSnapshot(), FractionalCombat).Position, TinyDestination);
    TestEqual(TEXT("Sub-epsilon completed Move cannot permanently suppress idle fire"),
        IdleCombatUnit(Fractional.GetSnapshot(), IdleCombatKind(Fractional.GetSnapshot(), 2, EProofUnitKind::HQ)).HitPoints, 250.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofIdleNearestTest, "RedalertProof.Fidelity.IdleCombat.NearestAndTie", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofIdleNearestTest::RunTest(const FString&)
{
    // Seed order varies enemy record IDs while spatial distances remain equal.
    // Lowest UnitId, rather than owner number or team, resolves each tie.
    for (int32 Reverse = 0; Reverse < 2; ++Reverse)
    {
        FProofCommandModel Model;
        Model.StartSkirmish();
        Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
        const int32 FirstOwner = Reverse ? 3 : 2;
        const int32 SecondOwner = Reverse ? 2 : 3;
        Model.SeedSkirmishPlayer(FirstOwner, 1, FVector(600, 300, 0));
        Model.SeedSkirmishPlayer(SecondOwner, 1, FVector(600, -300, 0));
        Model.Submit(1, IdleCombatOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0)));
        Model.Submit(1, IdleCombatOrder(2, EProofCommandType::Produce, IdleCombatKind(Model.GetSnapshot(), 1, EProofUnitKind::Factory)));
        Model.Advance(FProofCommandModel::ProductionDurationSeconds);
        const FProofSnapshot Before = Model.GetSnapshot();
        const int32 Combat = IdleCombatKind(Before, 1, EProofUnitKind::Combat);
        TestEqual(TEXT("Combat spawn is centered between equidistant enemies"), IdleCombatUnit(Before, Combat).Position, FVector(600, 0, 0));
        const int32 LowerIdHQ = IdleCombatKind(Before, FirstOwner, EProofUnitKind::HQ);
        const int32 OtherHQ = IdleCombatKind(Before, SecondOwner, EProofUnitKind::HQ);
        Model.Advance(1.f);
        TestEqual(TEXT("Equal-distance target uses lower UnitId"), IdleCombatUnit(Model.GetSnapshot(), LowerIdHQ).HitPoints, 275.f);
        TestEqual(TEXT("One shot cannot also hit tied target"), IdleCombatUnit(Model.GetSnapshot(), OtherHQ).HitPoints, 300.f);
        Model.Submit(1, IdleCombatOrder(3, EProofCommandType::Move, Combat, INDEX_NONE, FVector(600, -10, 0)));
        Model.Advance(.1f);
        TestEqual(TEXT("Closer higher-ID target receives next idle shot"), IdleCombatUnit(Model.GetSnapshot(), OtherHQ).HitPoints, 297.5f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofIdleCompatibilityTest, "RedalertProof.Fidelity.IdleCombat.TerminalAndLegacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofIdleCompatibilityTest::RunTest(const FString&)
{
    FProofCommandModel Legacy;
    Legacy.AddPlayer(1); Legacy.AddPlayer(2);
    const int32 A = Legacy.SeedUnit(1, FVector::ZeroVector);
    const int32 B = Legacy.SeedUnit(2, FVector(100, 0, 0));
    Legacy.Advance(10.f);
    TestEqual(TEXT("Legacy idle attacker remains passive"), IdleCombatUnit(Legacy.GetSnapshot(), A).HitPoints, 100.f);
    TestEqual(TEXT("Legacy nearby enemy remains undamaged"), IdleCombatUnit(Legacy.GetSnapshot(), B).HitPoints, 100.f);
    Legacy.Submit(1, IdleCombatOrder(1, EProofCommandType::Attack, A, B));
    Legacy.Advance(1.f);
    TestEqual(TEXT("Explicit attack deals exactly one shot of existing damage"), IdleCombatUnit(Legacy.GetSnapshot(), B).HitPoints, 75.f);

    FProofCommandModel Terminal;
    Terminal.StartSkirmish();
    Terminal.SeedSkirmishPlayer(1, 0, FVector::ZeroVector);
    Terminal.SeedSkirmishPlayer(2, 1, FVector(900, 0, 0));
    Terminal.Submit(1, IdleCombatOrder(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(300, 0, 0)));
    Terminal.Submit(1, IdleCombatOrder(2, EProofCommandType::Produce, IdleCombatKind(Terminal.GetSnapshot(), 1, EProofUnitKind::Factory)));
    Terminal.Advance(FProofCommandModel::ProductionDurationSeconds);
    Terminal.Advance(12.f);
    const FProofSnapshot Finished = Terminal.GetSnapshot();
    TestTrue(TEXT("Idle combat can trigger normal HQ terminal rules"), Finished.MatchPhase == EProofMatchPhase::Finished);
    TestEqual(TEXT("Existing winner semantics apply"), Finished.WinnerTeam, 0);
    Terminal.Advance(100.f);
    TestEqual(TEXT("Finished match remains frozen"), Terminal.GetSnapshot().ComputeDigest(), Finished.ComputeDigest());
    return true;
}
#endif

