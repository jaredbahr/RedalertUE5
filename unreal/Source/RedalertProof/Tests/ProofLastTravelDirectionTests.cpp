#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"
#include <cmath>
#include <limits>

// Synthetic numeric and saturation boundaries only. Travel, orders, damage,
// economy, production and lifecycle cases execute the production model.
struct FProofLastTravelDirectionFixture
{
    static void Record(FProofUnitRecord& Unit, const FVector& Before)
    { FProofCommandModel::RecordLastTravelDirection(Unit, Before); }
    static void SaturateRevision(FProofCommandModel& Model) { Model.State.Revision = MAX_int32; }
    static void Credits(FProofCommandModel& Model, int32 Owner, int32 Value)
    {
        Model.State.Players.FindByPredicate([Owner](const FProofPlayerRecord& P)
            { return P.OwnerId == Owner; })->Credits = Value;
    }
    static void Health(FProofCommandModel& Model, int32 Id, float Value)
    {
        Model.State.Units.FindByPredicate([Id](const FProofUnitRecord& U)
            { return U.UnitId == Id; })->HitPoints = Value;
    }
};

namespace
{
FProofCommand TravelCommand(int32 Sequence, EProofCommandType Type, int32 UnitId,
    const FVector& Destination = FVector::ZeroVector, int32 Target = INDEX_NONE)
{
    FProofCommand Command;
    Command.Sequence = Sequence;
    Command.Type = Type;
    Command.UnitId = UnitId;
    Command.Destination = Destination;
    Command.TargetUnitId = Target;
    return Command;
}
FProofUnitRecord TravelUnit(const FProofSnapshot& Snapshot, int32 Id)
{
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& U)
        { return U.UnitId == Id; });
    check(Unit);
    return *Unit;
}
int32 TravelKind(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& U)
        { return U.OwnerId == Owner && U.Kind == Kind && U.HitPoints > 0.f; });
    check(Unit);
    return Unit->UnitId;
}
bool TravelNear(const FVector& A, const FVector& B)
{
    return std::abs(A.X - B.X) <= 1.e-12 && std::abs(A.Y - B.Y) <= 1.e-12 && A.Z == B.Z;
}
// Independent ordinary-world oracle: hypot rather than the production rescale.
bool TravelMatchesActual(const FProofUnitRecord& Before, const FProofUnitRecord& After)
{
    const double X = After.Position.X - Before.Position.X;
    const double Y = After.Position.Y - Before.Position.Y;
    const double Length = std::hypot(X, Y);
    return Before.UnitId == After.UnitId && Length > 0.0
        && TravelNear(After.LastTravelDirection, FVector(X / Length, Y / Length, 0.0));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofLastTravelDirectionTest,
    "RedalertProof.Skirmish.LastTravelDirection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProofLastTravelDirectionTest::RunTest(const FString&)
{
    TestTrue(TEXT("default record direction is unset"), FProofUnitRecord().LastTravelDirection == FVector::ZeroVector);
    {
        FProofCommandModel Model;
        Model.AddPlayer(1);
        const int32 A = Model.SeedUnit(1, FVector::ZeroVector);
        const int32 B = Model.SeedUnit(1, FVector(500, 500, 0));
        const int32 C = Model.SeedUnit(1, FVector(-500, -500, 0));
        for (const FProofUnitRecord& Unit : Model.GetSnapshot().Units)
            TestTrue(TEXT("every seeded direction is unset"), Unit.LastTravelDirection == FVector::ZeroVector);
        TestTrue(TEXT("first multi-record Move accepted"), Model.Submit(1, TravelCommand(1, EProofCommandType::Move, A, FVector(150, 0, 0))).bAccepted);
        TestTrue(TEXT("second multi-record Move accepted"), Model.Submit(1, TravelCommand(2, EProofCommandType::Move, B, FVector(500, 650, 0))).bAccepted);
        TestTrue(TEXT("third multi-record Move accepted"), Model.Submit(1, TravelCommand(3, EProofCommandType::Move, C, FVector(-590, -620, 0))).bAccepted);
        const FProofSnapshot Before = Model.GetSnapshot();
        Model.Advance(0.5f);
        const FProofSnapshot After = Model.GetSnapshot();
        TestEqual(TEXT("movement preserves record count"), After.Units.Num(), Before.Units.Num());
        for (int32 Index = 0; Index < Before.Units.Num(); ++Index)
        {
            TestEqual(TEXT("movement preserves each index identity"), After.Units[Index].UnitId, Before.Units[Index].UnitId);
            TestTrue(TEXT("multi-record identity actual displacement"), TravelMatchesActual(Before.Units[Index], After.Units[Index]));
        }
        TestTrue(TEXT("first cardinal direction"), TravelUnit(After, A).LastTravelDirection == FVector(1, 0, 0));
        TestTrue(TEXT("second cardinal direction"), TravelUnit(After, B).LastTravelDirection == FVector(0, 1, 0));
        TestTrue(TEXT("diagonal direction"), TravelNear(TravelUnit(After, C).LastTravelDirection, FVector(-0.6, -0.8, 0)));
        Model.Advance(0.f);
        Model.Advance(-1.f);
        Model.Advance(std::numeric_limits<float>::infinity());
        Model.Advance(std::numeric_limits<float>::quiet_NaN());
        TestEqual(TEXT("zero and invalid time do not mutate snapshot"), Model.GetSnapshot().ComputeDigest(), After.ComputeDigest());
        TestTrue(TEXT("direction-change Move accepted"), Model.Submit(1, TravelCommand(4, EProofCommandType::Move, A, FVector(75, -150, 0))).bAccepted);
        Model.Advance(1.f);
        TestTrue(TEXT("changed direction uses latest actual step"), TravelUnit(Model.GetSnapshot(), A).LastTravelDirection == FVector(0, -1, 0));
        const FVector HeadingBeforeStop = TravelUnit(Model.GetSnapshot(), A).LastTravelDirection;
        TestTrue(TEXT("Stop accepted"), Model.Submit(1, TravelCommand(5, EProofCommandType::Stop, A)).bAccepted);
        const FProofUnitRecord Stopped = TravelUnit(Model.GetSnapshot(), A);
        Model.Advance(1.f);
        TestTrue(TEXT("Stop preserves direction"), Stopped.LastTravelDirection == HeadingBeforeStop
            && TravelUnit(Model.GetSnapshot(), A).LastTravelDirection == HeadingBeforeStop);
        TestTrue(TEXT("Stop preserves position"), TravelUnit(Model.GetSnapshot(), A).Position == Stopped.Position);
        TestTrue(TEXT("vertical Move accepted"), Model.Submit(1, TravelCommand(6, EProofCommandType::Move, A, Stopped.Position + FVector(0, 0, 150))).bAccepted);
        Model.Advance(1.f);
        TestEqual(TEXT("vertical Move actually changes Z"), TravelUnit(Model.GetSnapshot(), A).Position.Z, Stopped.Position.Z + 150.0);
        TestTrue(TEXT("vertical-only movement preserves heading"), TravelUnit(Model.GetSnapshot(), A).LastTravelDirection == Stopped.LastTravelDirection);
    }
    {
        // Separate simulation steps may be coalesced into a stationary snapshot.
        FProofCommandModel Model;
        Model.AddPlayer(1);
        const int32 Id = Model.SeedUnit(1, FVector::ZeroVector);
        TestTrue(TEXT("outward Move accepted"), Model.Submit(1, TravelCommand(1, EProofCommandType::Move, Id, FVector(0, 150, 0))).bAccepted);
        Model.Advance(1.f);
        TestTrue(TEXT("outward travel recorded"), TravelUnit(Model.GetSnapshot(), Id).LastTravelDirection == FVector(0, 1, 0));
        TestTrue(TEXT("return Move accepted"), Model.Submit(1, TravelCommand(2, EProofCommandType::Move, Id)).bAccepted);
        Model.Advance(1.f);
        const FProofSnapshot CopiedStationarySnapshot = Model.GetSnapshot();
        TestTrue(TEXT("out-and-back final position equals origin"), TravelUnit(CopiedStationarySnapshot, Id).Position == FVector::ZeroVector);
        TestTrue(TEXT("copied snapshot publishes final reverse direction without history"), TravelUnit(CopiedStationarySnapshot, Id).LastTravelDirection == FVector(0, -1, 0));
        FProofSnapshot NoHistory = CopiedStationarySnapshot;
        NoHistory.Units[0].LastTravelDirection = FVector::ZeroVector;
        TestTrue(TEXT("copied stationary snapshot digest commits final heading"), CopiedStationarySnapshot.ComputeDigest() != NoHistory.ComputeDigest());
        Model.Advance(1.f);
        TestTrue(TEXT("arrival preserves final reverse direction"), TravelUnit(Model.GetSnapshot(), Id).LastTravelDirection == FVector(0, -1, 0));
        FProofLastTravelDirectionFixture::SaturateRevision(Model);
        TestTrue(TEXT("saturated revision Move accepted"), Model.Submit(1, TravelCommand(3, EProofCommandType::Move, Id, FVector(150, 0, 0))).bAccepted);
        Model.Advance(1.f);
        TestEqual(TEXT("revision stays saturated"), Model.GetSnapshot().Revision, MAX_int32);
        TestTrue(TEXT("positive Advance updates direction at saturated revision"), TravelUnit(Model.GetSnapshot(), Id).LastTravelDirection == FVector(1, 0, 0));
        Model.Reset();
        Model.AddPlayer(1);
        const int32 Reused = Model.SeedUnit(1, FVector::ZeroVector);
        TestEqual(TEXT("reset reuses expected unit ID"), Reused, Id);
        TestTrue(TEXT("reset cannot inherit prior lifetime heading"), TravelUnit(Model.GetSnapshot(), Reused).LastTravelDirection == FVector::ZeroVector);
    }
    {
        // Ordinary straight segments cannot distinguish destination-derived code.
        FProofCommandModel Model;
        Model.AddPlayer(1);
        const int32 Id = Model.SeedUnit(1, FVector(9000.0, 0.0, 0.0));
        const FVector Destination(9000.0 + 1.e-7, 1.e-7, 0.0);
        TestTrue(TEXT("rounded-step Move accepted"), Model.Submit(1, TravelCommand(1, EProofCommandType::Move, Id, Destination)).bAccepted);
        const FProofUnitRecord Before = TravelUnit(Model.GetSnapshot(), Id);
        Model.Advance(1.e-15f);
        const FProofUnitRecord After = TravelUnit(Model.GetSnapshot(), Id);
        TestEqual(TEXT("rounded-step actual X is unchanged"), After.Position.X, Before.Position.X);
        TestTrue(TEXT("rounded-step actual Y increases"), After.Position.Y > Before.Position.Y);
        TestTrue(TEXT("rounded-step records actual positive Y, not requested bearing"), After.LastTravelDirection == FVector(0, 1, 0));
        TestTrue(TEXT("rounded-step requested X is independently nonzero"), Destination.X > Before.Position.X);
        TestTrue(TEXT("rounded-step actual displacement oracle"), TravelMatchesActual(Before, After));
    }
    {
        FProofCommandModel Model;
        Model.StartSkirmish();
        TestTrue(TEXT("tiny-path seat seeded"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
        const int32 Id = TravelKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester);
        const FVector Start = TravelUnit(Model.GetSnapshot(), Id).Position;
        const FVector Tiny = Start + FVector(-1.e-10, 1.e-10, 0);
        TestTrue(TEXT("tiny endpoint Move accepted"), Model.Submit(1, TravelCommand(1, EProofCommandType::Move, Id, Tiny)).bAccepted);
        const FProofUnitRecord Before = TravelUnit(Model.GetSnapshot(), Id);
        TestTrue(TEXT("tiny path is representable and below threshold"), Tiny != Start && (Tiny - Start).Size() <= UE_SMALL_NUMBER);
        Model.Advance(0.1f);
        const FProofUnitRecord After = TravelUnit(Model.GetSnapshot(), Id);
        TestTrue(TEXT("tiny endpoint lands exactly"), After.Position == Tiny);
        TestTrue(TEXT("tiny endpoint actual direction"), TravelMatchesActual(Before, After));
        Model.Advance(0.1f);
        TestTrue(TEXT("tiny arrival preserves heading"), TravelUnit(Model.GetSnapshot(), Id).LastTravelDirection == After.LastTravelDirection);
        const FVector Exact(-137.25, 268.5, 0);
        TestTrue(TEXT("exact endpoint AttackMove rejected for harvester"), !Model.Submit(1, TravelCommand(2, EProofCommandType::AttackMove, Id, Exact)).bAccepted);
        TestTrue(TEXT("exact endpoint Move accepted"), Model.Submit(1, TravelCommand(3, EProofCommandType::Move, Id, Exact)).bAccepted);
        Model.Advance(10.f);
        TestTrue(TEXT("skirmish normal endpoint is exact"), TravelUnit(Model.GetSnapshot(), Id).Position == Exact);
        TestTrue(TEXT("skirmish normal endpoint actual direction"), TravelMatchesActual(After, TravelUnit(Model.GetSnapshot(), Id)));
    }
    {
        FProofCommandModel Model;
        Model.AddPlayer(1); Model.AddPlayer(2);
        const int32 Attacker = Model.SeedUnit(1, FVector::ZeroVector);
        const int32 Target = Model.SeedUnit(2, FVector(100, 150, 0));
        TestTrue(TEXT("fire setup Move accepted"), Model.Submit(1, TravelCommand(1, EProofCommandType::Move, Attacker, FVector(0, 150, 0))).bAccepted);
        Model.Advance(1.f);
        const FProofUnitRecord Before = TravelUnit(Model.GetSnapshot(), Attacker);
        TestTrue(TEXT("in-range Attack accepted"), Model.Submit(1, TravelCommand(2, EProofCommandType::Attack, Attacker, FVector::ZeroVector, Target)).bAccepted);
        Model.Advance(1.f);
        TestEqual(TEXT("stationary in-range fire deals damage"), TravelUnit(Model.GetSnapshot(), Target).HitPoints, 75.f);
        TestTrue(TEXT("stationary fire preserves position"), TravelUnit(Model.GetSnapshot(), Attacker).Position == Before.Position);
        TestTrue(TEXT("stationary fire preserves direction"), TravelUnit(Model.GetSnapshot(), Attacker).LastTravelDirection == Before.LastTravelDirection);
    }
    {
        FProofCommandModel Model;
        Model.AddPlayer(1); Model.AddPlayer(2);
        const int32 Attacker = Model.SeedUnit(1, FVector::ZeroVector);
        const int32 Target = Model.SeedUnit(2, FVector(997.3, 321.7, 0));
        TestTrue(TEXT("pursuit Attack accepted"), Model.Submit(1, TravelCommand(1, EProofCommandType::Attack, Attacker, FVector::ZeroVector, Target)).bAccepted);
        const FProofUnitRecord Before = TravelUnit(Model.GetSnapshot(), Attacker);
        Model.Advance(6.f);
        const FProofUnitRecord After = TravelUnit(Model.GetSnapshot(), Attacker);
        TestTrue(TEXT("pursuit reaches closed attack boundary"), FVector::DistSquared(After.Position, TravelUnit(Model.GetSnapshot(), Target).Position) <= 300.0 * 300.0);
        TestTrue(TEXT("pursuit includes final representable landing direction"), TravelMatchesActual(Before, After));
    }
    {
        FProofCommandModel Model;
        Model.AddPlayer(1); Model.AddPlayer(2);
        const int32 Victim = Model.SeedUnit(1, FVector::ZeroVector);
        const int32 Attacker = Model.SeedUnit(2, FVector(100, 100, 0));
        TestTrue(TEXT("lethal-tick Move accepted"), Model.Submit(1, TravelCommand(1, EProofCommandType::Move, Victim, FVector(0, 100, 0))).bAccepted);
        TestTrue(TEXT("lethal-tick Attack accepted"), Model.Submit(2, TravelCommand(1, EProofCommandType::Attack, Attacker, FVector::ZeroVector, Victim)).bAccepted);
        Model.Advance(4.f);
        const FProofUnitRecord Dead = TravelUnit(Model.GetSnapshot(), Victim);
        TestEqual(TEXT("mover dies after movement"), Dead.HitPoints, 0.f);
        TestTrue(TEXT("movement before same-tick lethal damage records direction"), Dead.LastTravelDirection == FVector(0, 1, 0));
        Model.Advance(1.f);
        TestTrue(TEXT("already-dead record retains its direction"), TravelUnit(Model.GetSnapshot(), Victim).LastTravelDirection == Dead.LastTravelDirection);
        TestTrue(TEXT("already-dead record remains stationary"), TravelUnit(Model.GetSnapshot(), Victim).Position == Dead.Position);
    }
    {
        FProofCommandModel Model;
        Model.StartSkirmish();
        TestTrue(TEXT("economy seat seeded"), Model.SeedSkirmishPlayer(1, 0, FVector(-3000, 0, 0)));
        TestTrue(TEXT("economy opposing seat seeded"), Model.SeedSkirmishPlayer(2, 1, FVector(3000, 0, 0)));
        const int32 Miner = TravelKind(Model.GetSnapshot(), 1, EProofUnitKind::Harvester);
        const FProofResourceRecord Ore = Model.GetSnapshot().Resources[0];
        TestTrue(TEXT("refinery build accepted"), Model.Submit(1, TravelCommand(1, EProofCommandType::BuildRefinery, INDEX_NONE, FVector(-3000, 400, 0))).bAccepted);
        const int32 Refinery = TravelKind(Model.GetSnapshot(), 1, EProofUnitKind::Refinery);
        for (const FProofUnitRecord& Unit : Model.GetSnapshot().Units)
            TestTrue(TEXT("seed and refinery-granted records start unset"), Unit.LastTravelDirection == FVector::ZeroVector);
        TestTrue(TEXT("ore approach Move accepted"), Model.Submit(1, TravelCommand(2, EProofCommandType::Move, Miner, Ore.Position)).bAccepted);
        Model.Advance(3.f);
        const FProofUnitRecord AtOre = TravelUnit(Model.GetSnapshot(), Miner);
        TestTrue(TEXT("ore approach traveled positive X"), AtOre.LastTravelDirection == FVector(1, 0, 0));
        TestTrue(TEXT("Gather accepted"), Model.Submit(1, TravelCommand(3, EProofCommandType::Gather, Miner, FVector::ZeroVector, Ore.ResourceId)).bAccepted);
        Model.Advance(0.1f);
        TestEqual(TEXT("stationary Gather extracts cargo"), TravelUnit(Model.GetSnapshot(), Miner).Cargo, FProofCommandModel::CargoCapacity);
        TestTrue(TEXT("stationary Gather preserves direction"), TravelUnit(Model.GetSnapshot(), Miner).LastTravelDirection == AtOre.LastTravelDirection);
        FProofLastTravelDirectionFixture::Health(Model, Refinery, 0.f);
        Model.Advance(0.1f);
        TestTrue(TEXT("loaded Gather waits without refinery"), TravelUnit(Model.GetSnapshot(), Miner).Position == AtOre.Position);
        TestTrue(TEXT("Gather wait preserves direction"), TravelUnit(Model.GetSnapshot(), Miner).LastTravelDirection == AtOre.LastTravelDirection);
        FProofLastTravelDirectionFixture::Health(Model, Refinery, 200.f);
        TestTrue(TEXT("Stop before explicit cargo return accepted"), Model.Submit(1, TravelCommand(4, EProofCommandType::Stop, Miner)).bAccepted);
        TestTrue(TEXT("ReturnCargo accepted"), Model.Submit(1, TravelCommand(5, EProofCommandType::ReturnCargo, Miner, FVector::ZeroVector, Refinery)).bAccepted);
        FProofLastTravelDirectionFixture::Credits(Model, 1, MAX_int32);
        Model.Advance(10.f);
        const FProofUnitRecord AtRefinery = TravelUnit(Model.GetSnapshot(), Miner);
        TestTrue(TEXT("ReturnCargo travels to chosen refinery"), AtRefinery.Position == TravelUnit(Model.GetSnapshot(), Refinery).Position);
        TestTrue(TEXT("ReturnCargo actual displacement direction"), TravelMatchesActual(AtOre, AtRefinery));
        TestEqual(TEXT("saturated-credit cargo cannot deposit"), AtRefinery.Cargo, FProofCommandModel::CargoCapacity);
        Model.Advance(0.1f);
        TestTrue(TEXT("ReturnCargo deposit wait preserves direction"), TravelUnit(Model.GetSnapshot(), Miner).LastTravelDirection == AtRefinery.LastTravelDirection);
        FProofLastTravelDirectionFixture::Credits(Model, 1, MAX_int32 - FProofCommandModel::CargoCapacity);
        Model.Advance(0.1f);
        TestEqual(TEXT("stationary deposit empties cargo"), TravelUnit(Model.GetSnapshot(), Miner).Cargo, 0);
        TestTrue(TEXT("stationary deposit preserves direction"), TravelUnit(Model.GetSnapshot(), Miner).LastTravelDirection == AtRefinery.LastTravelDirection);
        Model.Advance(1.f);
        TestTrue(TEXT("completed ReturnCargo stays at refinery"), TravelUnit(Model.GetSnapshot(), Miner).Position == AtRefinery.Position);
        TestTrue(TEXT("completed ReturnCargo preserves heading"), TravelUnit(Model.GetSnapshot(), Miner).LastTravelDirection == AtRefinery.LastTravelDirection);
        TestTrue(TEXT("second Gather accepted"), Model.Submit(1, TravelCommand(6, EProofCommandType::Gather, Miner, FVector::ZeroVector, Ore.ResourceId)).bAccepted);
        Model.Advance(10.f);
        const FProofUnitRecord GatherArrival = TravelUnit(Model.GetSnapshot(), Miner);
        TestTrue(TEXT("Gather travel actual displacement"), TravelMatchesActual(AtRefinery, GatherArrival));
        TestEqual(TEXT("Gather arrival extracts cargo"), GatherArrival.Cargo, FProofCommandModel::CargoCapacity);
        Model.Advance(10.f);
        TestTrue(TEXT("automatic Gather return actual displacement"), TravelMatchesActual(GatherArrival, TravelUnit(Model.GetSnapshot(), Miner)));
        TestTrue(TEXT("automatic Gather return arrives at refinery"), TravelUnit(Model.GetSnapshot(), Miner).Position == AtRefinery.Position);
    }
    {
        FProofCommandModel Model;
        Model.StartSkirmish();
        TestTrue(TEXT("production seat seeded"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector));
        TestTrue(TEXT("factory build accepted"), Model.Submit(1, TravelCommand(1, EProofCommandType::BuildFactory, INDEX_NONE, FVector(-500, 0, 0))).bAccepted);
        const int32 Factory = TravelKind(Model.GetSnapshot(), 1, EProofUnitKind::Factory);
        TestTrue(TEXT("factory rally accepted"), Model.Submit(1, TravelCommand(2, EProofCommandType::SetRally, Factory, FVector(-500, 900, 0))).bAccepted);
        TestTrue(TEXT("rallied Produce accepted"), Model.Submit(1, TravelCommand(3, EProofCommandType::Produce, Factory)).bAccepted);
        const int32 Combat = TravelKind(Model.GetSnapshot(), 1, EProofUnitKind::Combat);
        const FProofUnitRecord Produced = TravelUnit(Model.GetSnapshot(), Combat);
        TestTrue(TEXT("production rally does not fabricate heading"), Produced.LastTravelDirection == FVector::ZeroVector);
        Model.Advance(0.5f);
        TestTrue(TEXT("production rally actual movement records heading"), TravelMatchesActual(Produced, TravelUnit(Model.GetSnapshot(), Combat)));
        TestTrue(TEXT("AttackMove accepted for Combat"), Model.Submit(1, TravelCommand(4, EProofCommandType::AttackMove, Combat, FVector(-800, -500, 0))).bAccepted);
        const FProofUnitRecord BeforeAttackMove = TravelUnit(Model.GetSnapshot(), Combat);
        Model.Advance(0.5f);
        TestTrue(TEXT("AttackMove actual movement records heading"), TravelMatchesActual(BeforeAttackMove, TravelUnit(Model.GetSnapshot(), Combat)));
        TestTrue(TEXT("production structure stays unset"), TravelUnit(Model.GetSnapshot(), Factory).LastTravelDirection == FVector::ZeroVector);
    }
    {
        FProofCommandModel Model;
        Model.StartSkirmish();
        TestTrue(TEXT("lifecycle first seat seeded"), Model.SeedSkirmishPlayer(1, 0, FVector(-3000, 0, 0)));
        TestTrue(TEXT("lifecycle removable seat seeded"), Model.SeedSkirmishPlayer(2, 1, FVector(3000, 0, 0)));
        TestTrue(TEXT("lifecycle surviving seat seeded"), Model.SeedSkirmishPlayer(3, 1, FVector(0, 3000, 0)));
        const int32 Survivor = TravelKind(Model.GetSnapshot(), 3, EProofUnitKind::Harvester);
        TestTrue(TEXT("survivor Move accepted"), Model.Submit(3, TravelCommand(1, EProofCommandType::Move, Survivor, FVector(100, 3150, 0))).bAccepted);
        Model.Advance(1.f);
        const FProofUnitRecord Before = TravelUnit(Model.GetSnapshot(), Survivor);
        TestTrue(TEXT("owner removal succeeds"), Model.RemoveSkirmishPlayer(2));
        TestFalse(TEXT("removed owner records are absent"), Model.GetSnapshot().Units.ContainsByPredicate([](const FProofUnitRecord& U) { return U.OwnerId == 2; }));
        TestTrue(TEXT("owner removal preserves survivor direction"), TravelUnit(Model.GetSnapshot(), Survivor).LastTravelDirection == Before.LastTravelDirection);
        Model.Advance(1.f);
        TestTrue(TEXT("survivor arrival after array compaction retains heading"), TravelUnit(Model.GetSnapshot(), Survivor).LastTravelDirection == Before.LastTravelDirection);
        Model.StartSkirmish();
        TestTrue(TEXT("fresh rematch seeded"), Model.SeedSkirmishPlayer(3, 1, FVector::ZeroVector));
        for (const FProofUnitRecord& Unit : Model.GetSnapshot().Units)
            TestTrue(TEXT("rematch records have zero lifetime default"), Unit.LastTravelDirection == FVector::ZeroVector);
    }
    {
        // These are numeric helper controls, not claims about legal world positions.
        const double Huge = std::numeric_limits<double>::max();
        const double Tiny = std::numeric_limits<double>::denorm_min();
        const FVector Prior(0, -1, 0);
        const FVector Inputs[] = { FVector(Huge, Huge, 0), FVector(-Huge, Huge, 0),
            FVector(Tiny, Tiny, 0), FVector(-Tiny, 0, 0), FVector(0, -Tiny, 0),
            FVector(Huge, Tiny, 0), FVector(-0.0, Tiny, 0) };
        for (const FVector& Position : Inputs)
        {
            FProofUnitRecord Unit;
            Unit.Position = Position;
            Unit.LastTravelDirection = Prior;
            FProofLastTravelDirectionFixture::Record(Unit, FVector::ZeroVector);
            const FVector Direction = Unit.LastTravelDirection;
            TestTrue(TEXT("extreme and subnormal output is finite normalized XY"), std::isfinite(Direction.X)
                && std::isfinite(Direction.Y) && Direction.Z == 0.0 && std::abs(std::hypot(Direction.X, Direction.Y) - 1.0) < 1.e-12);
            TestTrue(TEXT("numeric update preserves source position"), Unit.Position == Position);
            TestTrue(TEXT("numeric direction preserves nonzero component signs"),
                (Position.X == 0.0 || Direction.X == 0.0 || std::signbit(Direction.X) == std::signbit(Position.X))
                && (Position.Y == 0.0 || Direction.Y == 0.0 || std::signbit(Direction.Y) == std::signbit(Position.Y)));
            TestTrue(TEXT("zero output components are canonical positive zero"),
                (Direction.X != 0.0 || !std::signbit(Direction.X)) && (Direction.Y != 0.0 || !std::signbit(Direction.Y)) && !std::signbit(Direction.Z));
            if (std::abs(Position.X) == std::abs(Position.Y))
                TestTrue(TEXT("equal extreme components stay diagonal"), std::abs(std::abs(Direction.X) - std::sqrt(0.5)) < 1.e-12
                    && std::abs(std::abs(Direction.Y) - std::sqrt(0.5)) < 1.e-12);
        }
        const double NaN = std::numeric_limits<double>::quiet_NaN();
        const double Inf = std::numeric_limits<double>::infinity();
        const FVector Invalid[] = { FVector(NaN, 1, 0), FVector(1, NaN, 0), FVector(Inf, 1, 0),
            FVector(1, -Inf, 0), FVector(1, 1, NaN), FVector(1, 1, Inf) };
        for (const FVector& Position : Invalid)
        {
            FProofUnitRecord Unit;
            Unit.Position = Position; Unit.LastTravelDirection = Prior;
            FProofLastTravelDirectionFixture::Record(Unit, FVector::ZeroVector);
            TestTrue(TEXT("invalid current position retains prior direction"), Unit.LastTravelDirection == Prior);
            Unit.Position = FVector(1, 1, 0);
            FProofLastTravelDirectionFixture::Record(Unit, Position);
            TestTrue(TEXT("invalid previous position retains prior direction"), Unit.LastTravelDirection == Prior);
        }
        FProofUnitRecord Unit;
        Unit.Position = FVector(Huge, 0, 0); Unit.LastTravelDirection = Prior;
        FProofLastTravelDirectionFixture::Record(Unit, FVector(-Huge, 0, 0));
        TestTrue(TEXT("overflowed displacement retains prior direction"), Unit.LastTravelDirection == Prior);
        Unit.Position = FVector(0, 0, 100);
        FProofLastTravelDirectionFixture::Record(Unit, FVector::ZeroVector);
        TestTrue(TEXT("synthetic vertical displacement preserves prior direction"), Unit.LastTravelDirection == Prior);
        FProofLastTravelDirectionFixture::Record(Unit, Unit.Position);
        TestTrue(TEXT("exact zero displacement preserves prior direction"), Unit.LastTravelDirection == Prior);
        Unit.Position = FVector(100, 0, 0); Unit.HitPoints = 0.f;
        FProofLastTravelDirectionFixture::Record(Unit, FVector::ZeroVector);
        TestTrue(TEXT("dead record cannot acquire a synthetic direction"), Unit.LastTravelDirection == Prior);
        FProofCommandModel Model; Model.AddPlayer(1);
        TestEqual(TEXT("extreme helper coverage does not relax world-bound validation"), Model.SeedUnit(1, FVector(Huge, 0, 0)), INDEX_NONE);
        TestEqual(TEXT("nonfinite helper coverage does not relax seed validation"), Model.SeedUnit(1, FVector(NaN, 0, 0)), INDEX_NONE);
    }
    {
        FProofSnapshot Snapshot;
        TestEqual(TEXT("empty snapshot default digest remains stable"), Snapshot.ComputeDigest(), FString(TEXT("aa1d59044f1a11b0547336759a1cefb2")));
        FProofUnitRecord Unit; Unit.UnitId = 7; Unit.OwnerId = 1;
        Snapshot.Units.Add(Unit);
        const FString DefaultDigest = Snapshot.ComputeDigest();
        TestEqual(TEXT("unset direction retains legacy record digest"), DefaultDigest, FString(TEXT("04e8728c43d9dec145fd8c201a8b4cf4")));
        Snapshot.Units[0].LastTravelDirection = FVector(1, 0, 0);
        const FString DirectionDigest = Snapshot.ComputeDigest();
        TestEqual(TEXT("direction digest commits ordered d tag"), DirectionDigest, FString(TEXT("5cf4de28c247fbfdf7afff6d04c50d60")));
        TestTrue(TEXT("direction change affects digest"), DirectionDigest != DefaultDigest);
        Snapshot.Units[0].bHasRallyPoint = true;
        Snapshot.Units[0].RallyPoint = FVector(10, 20, 0);
        const FString RallyDirectionDigest = Snapshot.ComputeDigest();
        TestEqual(TEXT("rally and direction tags both retain exact canonical order"), RallyDirectionDigest, FString(TEXT("b68c9dcee9aa79eb270c56caf455f570")));
        Snapshot.Units[0].LastTravelDirection = FVector::ZeroVector;
        TestTrue(TEXT("direction remains committed beside optional rally"), Snapshot.ComputeDigest() != RallyDirectionDigest);
        Snapshot.Units[0].LastTravelDirection = FVector(1, 0, 0);
        Snapshot.Units[0].bHasRallyPoint = false;
        Snapshot.Units[0].RallyPoint = FVector::ZeroVector;
        TestTrue(TEXT("rally remains committed beside optional direction"), Snapshot.ComputeDigest() != RallyDirectionDigest);

        Snapshot.Units[0].LastTravelDirection.X = std::nextafter(1.0, 2.0);
        TestTrue(TEXT("direction digest preserves full scalar precision"), Snapshot.ComputeDigest() != DirectionDigest);
        Snapshot.Units[0].LastTravelDirection = FVector(0, 1, 0);
        TestTrue(TEXT("direction Y affects digest"), Snapshot.ComputeDigest() != DirectionDigest && Snapshot.ComputeDigest() != DefaultDigest);
        Snapshot.Units[0].LastTravelDirection = FVector(0, 0, 0.5);
        TestTrue(TEXT("unexpected nonzero direction Z is committed"), Snapshot.ComputeDigest() != DefaultDigest);
        Snapshot.Units[0].LastTravelDirection = FVector(1, 0, 0);
        Unit.UnitId = 2; Unit.LastTravelDirection = FVector(-0.6, 0.8, 0); Snapshot.Units.Add(Unit);
        const FString SortedDigest = Snapshot.ComputeDigest();
        Snapshot.Units.Swap(0, 1);
        TestEqual(TEXT("direction digest is unit-order canonical"), Snapshot.ComputeDigest(), SortedDigest);
        TestEqual(TEXT("digest does not reorder live snapshot"), Snapshot.Units[0].UnitId, 2);
    }
    return true;
}
#endif
