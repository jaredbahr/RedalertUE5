#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"
#include <limits>

// Keep strict sphere and world-boundary test inputs precise on the native target.
#if defined(_MSC_VER)
#pragma float_control(precise, on, push)
#endif

namespace
{
FProofCommand BaseExpansionCommand(int32 Sequence, EProofCommandType Type,
    FVector Destination = FVector::ZeroVector, int32 UnitId = INDEX_NONE, int32 TargetId = INDEX_NONE)
{
    FProofCommand Command;
    Command.Sequence = Sequence;
    Command.Type = Type;
    Command.Destination = Destination;
    Command.UnitId = UnitId;
    Command.TargetUnitId = TargetId;
    return Command;
}
FProofUnitRecord BaseExpansionUnit(const FProofSnapshot& State, int32 Id)
{
    const FProofUnitRecord* Unit = State.Units.FindByPredicate([Id](const FProofUnitRecord& Candidate)
        { return Candidate.UnitId == Id; });
    check(Unit);
    return *Unit;
}
FProofPlayerRecord BaseExpansionPlayer(const FProofSnapshot& State, int32 Owner = 1)
{
    const FProofPlayerRecord* Player = State.Players.FindByPredicate([Owner](const FProofPlayerRecord& Candidate)
        { return Candidate.OwnerId == Owner; });
    check(Player);
    return *Player;
}
int32 BaseExpansionKindId(const FProofSnapshot& State, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* Unit = State.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& Candidate)
        { return Candidate.OwnerId == Owner && Candidate.Kind == Kind && Candidate.HitPoints > 0.f; });
    return Unit ? Unit->UnitId : INDEX_NONE;
}
int32 BaseExpansionMaximumId(const FProofSnapshot& State)
{
    int32 Maximum = 0;
    for (const FProofUnitRecord& Unit : State.Units) Maximum = FMath::Max(Maximum, Unit.UnitId);
    return Maximum;
}
int32 BaseExpansionOreTotal(const FProofSnapshot& State)
{
    int32 Total = 0;
    for (const FProofResourceRecord& Ore : State.Resources) Total += Ore.Remaining;
    for (const FProofUnitRecord& Unit : State.Units) Total += Unit.Cargo;
    for (const FProofPlayerRecord& Player : State.Players) Total += Player.TotalGathered;
    return Total;
}
bool BaseExpansionSeed(FProofCommandModel& Model, FVector HQ = FVector::ZeroVector)
{
    Model.StartSkirmish();
    return Model.SeedSkirmishPlayer(1, 0, HQ);
}
bool BaseExpansionEarnOre(FProofCommandModel& Model)
{
    for (int32 Tick = 0; Tick < 400; ++Tick)
    {
        if (BaseExpansionPlayer(Model.GetSnapshot()).TotalGathered == 500) return true;
        Model.Advance(1.f);
    }
    return false;
}
// The model has synchronous completed structures, not a construction queue or
// unfinished flag. Nonexistent anchors are tested by actually rejected builds.
// No fixture mutates private state, unit health, credits, IDs, roles or orders.
template<typename TestType>
void BaseExpansionReject(TestType& Test, FProofCommandModel& Model, int32 Owner,
    const FProofCommand& Command, const TCHAR* Reason)
{
    const FProofSnapshot Before = Model.GetSnapshot();
    const FProofCommandResult Result = Model.Submit(Owner, Command);
    const FProofSnapshot After = Model.GetSnapshot();
    Test.TestFalse(TEXT("Rejected build is never accepted"), Result.bAccepted);
    Test.TestEqual(TEXT("Exact rejection contract"), Result.Reason, FString(Reason));
    Test.TestEqual(TEXT("Rejection preserves complete visible snapshot"), After.ComputeDigest(), Before.ComputeDigest());
    Test.TestEqual(TEXT("Rejection preserves revision"), After.Revision, Before.Revision);
    Test.TestEqual(TEXT("Rejection preserves record count"), After.Units.Num(), Before.Units.Num());
    Test.TestEqual(TEXT("Rejection preserves visible IDs"), BaseExpansionMaximumId(After), BaseExpansionMaximumId(Before));
    Test.TestEqual(TEXT("Rejection cannot debit builder"), BaseExpansionPlayer(After).Credits, BaseExpansionPlayer(Before).Credits);
}
template<typename TestType>
bool BaseExpansionBuild(TestType& Test, FProofCommandModel& Model, int32 Sequence,
    EProofCommandType Type, const FVector& Site)
{
    const FProofSnapshot Before = Model.GetSnapshot();
    const bool bRefinery = Type == EProofCommandType::BuildRefinery;
    FVector ExpectedSpawn;
    if (bRefinery && !Test.TestTrue(TEXT("Refinery has a bounded clear grant spawn"),
        FProofCommandModel::FindSkirmishSpawn(Before, Site, ExpectedSpawn))) return false;
    if (!Test.TestTrue(TEXT("Public build command succeeds"),
        Model.Submit(1, BaseExpansionCommand(Sequence, Type, Site)).bAccepted)) return false;
    const FProofSnapshot After = Model.GetSnapshot();
    const int32 Records = bRefinery ? 2 : 1;
    const int32 OldMaximum = BaseExpansionMaximumId(Before);
    Test.TestEqual(TEXT("Exactly the existing structure and optional grant records"), After.Units.Num(), Before.Units.Num() + Records);
    Test.TestEqual(TEXT("Only record insertions and command bump revision"), After.Revision, Before.Revision + Records + 1);
    Test.TestEqual(TEXT("No hidden allocation between successful builds"), BaseExpansionMaximumId(After), OldMaximum + Records);
    Test.TestEqual(TEXT("Exactly the unchanged structure cost"), BaseExpansionPlayer(After).Credits,
        BaseExpansionPlayer(Before).Credits - (bRefinery ? FProofCommandModel::RefineryCost : FProofCommandModel::FactoryCost));
    Test.TestEqual(TEXT("Submission does not invent gathered income"), BaseExpansionPlayer(After).TotalGathered, BaseExpansionPlayer(Before).TotalGathered);
    const FProofUnitRecord Structure = BaseExpansionUnit(After, OldMaximum + 1);
    Test.TestEqual(TEXT("Structure is immediately at commanded site"), Structure.Position, Site);
    Test.TestEqual(TEXT("Structure is owned by builder"), Structure.OwnerId, 1);
    Test.TestEqual(TEXT("Completed structure uses existing health"), Structure.HitPoints, 200.f);
    Test.TestEqual(TEXT("Build preserves intended structure kind"), Structure.Kind,
        bRefinery ? EProofUnitKind::Refinery : EProofUnitKind::Factory);
    for (const FProofUnitRecord& Old : Before.Units)
    {
        const FProofUnitRecord Retained = BaseExpansionUnit(After, Old.UnitId);
        Test.TestTrue(TEXT("Build does not move, damage or unload existing units"),
            Retained.Position == Old.Position && Retained.HitPoints == Old.HitPoints && Retained.Cargo == Old.Cargo);
    }
    if (bRefinery)
    {
        const FProofUnitRecord Miner = BaseExpansionUnit(After, OldMaximum + 2);
        Test.TestTrue(TEXT("One ordinary owned empty full-health harvester is granted"),
            Miner.OwnerId == 1 && Miner.Kind == EProofUnitKind::Harvester && Miner.Cargo == 0 && Miner.HitPoints == 100.f);
        Test.TestEqual(TEXT("Grant uses existing deterministic spawn search"), Miner.Position, ExpectedSpawn);
        for (const FProofUnitRecord& Other : After.Units)
            if (Other.UnitId != Miner.UnitId && Other.HitPoints > 0.f)
                Test.TestTrue(TEXT("Grant keeps existing 260 cm live-center separation"),
                    FVector::DistSquared(Miner.Position, Other.Position) >= 260.0 * 260.0);
    }
    Test.TestEqual(TEXT("Build conserves all finite ore"), BaseExpansionOreTotal(After), BaseExpansionOreTotal(Before));
    return true;
}
// Produce and attack through public commands to destroy a chosen structure.
bool BaseExpansionEnemyAttack(FProofCommandModel& Model, int32 Target)
{
    if (!Model.SeedSkirmishPlayer(3, 1, FVector(-2000, 0, 0))
        || !Model.Submit(3, BaseExpansionCommand(1, EProofCommandType::BuildFactory, FVector(-1200, 0, 0))).bAccepted) return false;
    const int32 Factory = BaseExpansionKindId(Model.GetSnapshot(), 3, EProofUnitKind::Factory);
    if (!Model.Submit(3, BaseExpansionCommand(2, EProofCommandType::Produce, FVector::ZeroVector, Factory)).bAccepted) return false;
    const int32 Attacker = BaseExpansionKindId(Model.GetSnapshot(), 3, EProofUnitKind::Combat);
    if (!Model.Submit(3, BaseExpansionCommand(3, EProofCommandType::Attack, FVector::ZeroVector, Attacker, Target)).bAccepted) return false;
    Model.Advance(30.f);
    return BaseExpansionUnit(Model.GetSnapshot(), Target).HitPoints == 0.f;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBaseExpansionChainTest, "RedalertProof.Skirmish.BaseExpansion.ChainAndEconomy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBaseExpansionChainTest::RunTest(const FString&)
{
    for (EProofCommandType First : {EProofCommandType::BuildFactory, EProofCommandType::BuildRefinery})
    {
        FProofCommandModel Model;
        if (!TestTrue(TEXT("Seed ordinary expansion economy"), BaseExpansionSeed(Model))) return false;
        const int32 Starter = BaseExpansionKindId(Model.GetSnapshot(), 1, EProofUnitKind::Harvester);
        const FVector StarterPosition = BaseExpansionUnit(Model.GetSnapshot(), Starter).Position;
        const EProofCommandType Second = First == EProofCommandType::BuildFactory
            ? EProofCommandType::BuildRefinery : EProofCommandType::BuildFactory;
        // No Advance between these commands: successful synchronous completion
        // is sufficient, and both kinds extend reach beyond the original HQ.
        if (!BaseExpansionBuild(*this, Model, 1, First, FVector(800, 0, 0))
            || !BaseExpansionBuild(*this, Model, 2, Second, FVector(1600, 0, 0))
            || !BaseExpansionBuild(*this, Model, 3, EProofCommandType::BuildRefinery, FVector(2400, 0, 0))) return false;
        const FProofSnapshot Built = Model.GetSnapshot();
        TestEqual(TEXT("Three chained structures spend exactly opening credits"), BaseExpansionPlayer(Built).Credits, 0);
        TestEqual(TEXT("Two refineries grant exactly two additional miners"), Built.Units.Num(), 7);
        TestEqual(TEXT("No pre-simulation extraction"), Built.Resources[0].Remaining, 500);
        const int32 LastMiner = Built.Units[Built.Units.Num() - 1].UnitId;
        Model.Advance(0.f);
        TestEqual(TEXT("Zero advance neither completes a hidden queue nor starts travel"), Model.GetSnapshot().ComputeDigest(), Built.ComputeDigest());
        Model.Advance(0.5f);
        TestTrue(TEXT("Expansion refinery grant automatically starts ordinary mining"),
            BaseExpansionUnit(Model.GetSnapshot(), LastMiner).Position != BaseExpansionUnit(Built, LastMiner).Position);
        for (int32 Tick = 0; Tick < 240; ++Tick)
        {
            Model.Advance(1.f);
            const FProofSnapshot State = Model.GetSnapshot();
            TestEqual(TEXT("Expansion mining conserves remaining, carried and delivered ore"), BaseExpansionOreTotal(State), 500);
            TestEqual(TEXT("Only delivered ore increases post-build credits"), BaseExpansionPlayer(State).Credits, BaseExpansionPlayer(State).TotalGathered);
            TestEqual(TEXT("Existing idle starter never receives the grant's order"), BaseExpansionUnit(State, Starter).Position, StarterPosition);
            TestEqual(TEXT("Simulation never grants another hidden unit"), State.Units.Num(), Built.Units.Num());
            for (const FProofUnitRecord& Unit : State.Units)
                TestTrue(TEXT("Cargo remains in existing bounds"), Unit.Cargo >= 0 && Unit.Cargo <= FProofCommandModel::CargoCapacity);
            if (BaseExpansionPlayer(State).TotalGathered == 500) break;
        }
        Model.Advance(100.f);
        const FProofSnapshot Mined = Model.GetSnapshot();
        TestEqual(TEXT("Exhausted economy cannot mint extra income"), BaseExpansionPlayer(Mined).Credits, 500);
        TestEqual(TEXT("Exhausted economy preserves all finite ore"), BaseExpansionOreTotal(Mined), 500);
        TestEqual(TEXT("Exhausted economy cannot grant extra units"), Mined.Units.Num(), Built.Units.Num());
        TestEqual(TEXT("Automatic grants finish exactly the finite starter ore"), BaseExpansionPlayer(Mined).TotalGathered, 500);
        const int32 Factory = BaseExpansionKindId(Mined, 1, EProofUnitKind::Factory);
        FVector ExpectedSpawn;
        if (!TestTrue(TEXT("Completed expansion factory has an ordinary production spawn"),
            FProofCommandModel::FindSkirmishSpawn(Mined, BaseExpansionUnit(Mined, Factory).Position, ExpectedSpawn))) return false;
        if (!TestTrue(TEXT("Expansion factory produces with earned credits"),
            Model.Submit(1, BaseExpansionCommand(4, EProofCommandType::Produce, FVector::ZeroVector, Factory)).bAccepted)) return false;
        const FProofSnapshot Produced = Model.GetSnapshot();
        TestEqual(TEXT("Factory preserves production price"), BaseExpansionPlayer(Produced).Credits, 500 - FProofCommandModel::ProductionCost);
        TestEqual(TEXT("Factory preserves production spawn"), Produced.Units[Produced.Units.Num() - 1].Position, ExpectedSpawn);
        TestEqual(TEXT("Factory produces exactly one combat record"), Produced.Units[Produced.Units.Num() - 1].Kind, EProofUnitKind::Combat);
        TestEqual(TEXT("Production consumes exactly next ID"), Produced.Units[Produced.Units.Num() - 1].UnitId, BaseExpansionMaximumId(Mined) + 1);
        TestEqual(TEXT("Production keeps existing revision semantics"), Produced.Revision, Mined.Revision + 2);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBaseExpansionBoundaryTest, "RedalertProof.Skirmish.BaseExpansion.PlacementBoundaries", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBaseExpansionBoundaryTest::RunTest(const FString&)
{
    TestEqual(TEXT("Expansion uses existing 800 cm build radius"), FProofCommandModel::BuildRadius, 800.f);
    TestEqual(TEXT("Expansion uses existing 180 cm structure separation"), FProofCommandModel::StructureSeparation, 180.f);
    const FVector Sites[] = {FVector(1600, 0, 0), FVector(1280, 640, 0), FVector(980, 0, 0),
        FVector(1600.001, 0, 0), FVector(1280, 640.001, 0), FVector(979.999, 0, 0), FVector(1600, 0, 1)};
    for (EProofCommandType Type : {EProofCommandType::BuildFactory, EProofCommandType::BuildRefinery})
    {
        for (int32 Case = 0; Case < 7; ++Case)
        {
            FProofCommandModel Model;
            if (!TestTrue(TEXT("Seed boundary fixture"), BaseExpansionSeed(Model))
                || !BaseExpansionBuild(*this, Model, 1, EProofCommandType::BuildFactory, FVector(800, 0, 0))) return false;
            if (Case < 3)
            {
                if (!BaseExpansionBuild(*this, Model, 2, Type, Sites[Case])) return false;
            }
            else BaseExpansionReject(*this, Model, 1, BaseExpansionCommand(2, Type, Sites[Case]), Case == 5 ? TEXT("overlap") : TEXT("placement"));
        }
        // Existing world bounds apply to every axis and remain closed. Z is
        // intentionally tested because this model's build contract is 3D.
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            for (int32 Sign : {-1, 1})
            {
                FVector HQ = FVector::ZeroVector, Anchor = FVector::ZeroVector;
                HQ[Axis] = Sign * 9200.0;
                Anchor[Axis] = Sign * 10000.0;
                FProofCommandModel Model;
                if (!TestTrue(TEXT("Seed world-edge construction capability"), BaseExpansionSeed(Model, HQ))
                    || !BaseExpansionBuild(*this, Model, 1, EProofCommandType::BuildFactory, Anchor)) return false;
                FVector Outside = Anchor;
                Outside[Axis] = Sign * 10000.001;
                BaseExpansionReject(*this, Model, 1, BaseExpansionCommand(2, Type, Outside), TEXT("position"));
                FVector Boundary = Anchor;
                Boundary[(Axis + 1) % 3] = 800.0;
                if (!BaseExpansionBuild(*this, Model, 3, Type, Boundary)) return false;
            }
        }
        for (int32 ForeignTeam : {0, 1})
        {
            FProofCommandModel Model;
            if (!TestTrue(TEXT("Seed overlap fixture"), BaseExpansionSeed(Model))
                || !TestTrue(TEXT("Seed foreign blocking HQ"), Model.SeedSkirmishPlayer(2, ForeignTeam, FVector(650, 0, 0)))) return false;
            BaseExpansionReject(*this, Model, 1, BaseExpansionCommand(1, Type, FVector(800, 0, 0)), TEXT("overlap"));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBaseExpansionOwnershipTest, "RedalertProof.Skirmish.BaseExpansion.OwnershipAndLiveAnchors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBaseExpansionOwnershipTest::RunTest(const FString&)
{
    for (int32 ForeignTeam : {0, 1})
    {
        for (EProofUnitKind Kind : {EProofUnitKind::HQ, EProofUnitKind::Refinery, EProofUnitKind::Factory})
        {
            FProofCommandModel Model;
            if (!TestTrue(TEXT("Seed owner and foreign anchor"), BaseExpansionSeed(Model)
                && Model.SeedSkirmishPlayer(2, ForeignTeam, FVector(3000, 0, 0)))) return false;
            FVector Site(3800, 0, 0);
            if (Kind != EProofUnitKind::HQ)
            {
                if (!TestTrue(TEXT("Foreign completed structure built publicly"), Model.Submit(2,
                    BaseExpansionCommand(1, Kind == EProofUnitKind::Refinery ? EProofCommandType::BuildRefinery : EProofCommandType::BuildFactory, Site)).bAccepted)) return false;
                Site = FVector(4600, 0, 0);
            }
            BaseExpansionReject(*this, Model, 1, BaseExpansionCommand(1, EProofCommandType::BuildFactory, Site), TEXT("placement"));
            BaseExpansionReject(*this, Model, 1, BaseExpansionCommand(2, EProofCommandType::BuildRefinery, Site), TEXT("placement"));
        }
    }
    for (EProofUnitKind Kind : {EProofUnitKind::Harvester, EProofUnitKind::Combat})
    {
        FProofCommandModel Model;
        if (!TestTrue(TEXT("Seed own mobile non-anchor"), BaseExpansionSeed(Model))) return false;
        int32 Sequence = 1;
        if (Kind == EProofUnitKind::Combat)
        {
            if (!BaseExpansionBuild(*this, Model, Sequence++, EProofCommandType::BuildFactory, FVector(-300, 0, 0))) return false;
            if (!TestTrue(TEXT("Produce owned combat non-anchor"), Model.Submit(1, BaseExpansionCommand(Sequence++, EProofCommandType::Produce,
                FVector::ZeroVector, BaseExpansionKindId(Model.GetSnapshot(), 1, EProofUnitKind::Factory))).bAccepted)) return false;
        }
        const int32 Mobile = BaseExpansionKindId(Model.GetSnapshot(), 1, Kind);
        if (!TestTrue(TEXT("Move own mobile to remote site"), Model.Submit(1,
            BaseExpansionCommand(Sequence++, EProofCommandType::Move, FVector(1600, 0, 0), Mobile)).bAccepted)) return false;
        Model.Advance(30.f);
        TestEqual(TEXT("Mobile actually reached proposed anchor"), BaseExpansionUnit(Model.GetSnapshot(), Mobile).Position, FVector(1600, 0, 0));
        BaseExpansionReject(*this, Model, 1, BaseExpansionCommand(Sequence++, EProofCommandType::BuildFactory, FVector(2400, 0, 0)), TEXT("placement"));
        BaseExpansionReject(*this, Model, 1, BaseExpansionCommand(Sequence, EProofCommandType::BuildRefinery, FVector(2400, 0, 0)), TEXT("placement"));
    }
    for (EProofCommandType AnchorType : {EProofCommandType::BuildFactory, EProofCommandType::BuildRefinery})
    {
        FProofCommandModel NoHQ;
        if (!TestTrue(TEXT("Seed HQ-loss owner and surviving teammate"), BaseExpansionSeed(NoHQ)
            && NoHQ.SeedSkirmishPlayer(2, 0, FVector(0, 5000, 0)))
            || !BaseExpansionBuild(*this, NoHQ, 1, AnchorType, FVector(800, 0, 0))) return false;
        const int32 HQ = BaseExpansionKindId(NoHQ.GetSnapshot(), 1, EProofUnitKind::HQ);
        const int32 Anchor = BaseExpansionKindId(NoHQ.GetSnapshot(), 1,
            AnchorType == EProofCommandType::BuildFactory ? EProofUnitKind::Factory : EProofUnitKind::Refinery);
        if (!TestTrue(TEXT("Enemy public combat destroys own HQ"), BaseExpansionEnemyAttack(NoHQ, HQ))) return false;
        TestEqual(TEXT("Teammate HQ keeps match playing after own HQ loss"), NoHQ.GetSnapshot().MatchPhase, EProofMatchPhase::Playing);
        TestTrue(TEXT("Own completed expansion anchor survives HQ loss"), BaseExpansionUnit(NoHQ.GetSnapshot(), Anchor).HitPoints > 0.f);
        BaseExpansionReject(*this, NoHQ, 1, BaseExpansionCommand(2, EProofCommandType::BuildFactory, FVector(1600, 0, 0)), TEXT("placement"));
        BaseExpansionReject(*this, NoHQ, 1, BaseExpansionCommand(3, EProofCommandType::BuildRefinery, FVector(1600, 0, 0)), TEXT("placement"));
        // Also probe the dead HQ's own former sphere: teammate construction
        // capability must never substitute for this owner's lost HQ.
        BaseExpansionReject(*this, NoHQ, 1, BaseExpansionCommand(4, EProofCommandType::BuildRefinery, FVector(0, 600, 0)), TEXT("placement"));

        FProofCommandModel DeadAnchor;
        if (!TestTrue(TEXT("Seed live HQ with destructible anchor"), BaseExpansionSeed(DeadAnchor))
            || !BaseExpansionBuild(*this, DeadAnchor, 1, AnchorType, FVector(800, 0, 0))) return false;
        const int32 Doomed = BaseExpansionKindId(DeadAnchor.GetSnapshot(), 1,
            AnchorType == EProofCommandType::BuildFactory ? EProofUnitKind::Factory : EProofUnitKind::Refinery);
        if (!TestTrue(TEXT("Enemy public combat destroys expansion anchor"), BaseExpansionEnemyAttack(DeadAnchor, Doomed))) return false;
        TestTrue(TEXT("Own HQ remains alive"), BaseExpansionKindId(DeadAnchor.GetSnapshot(), 1, EProofUnitKind::HQ) != INDEX_NONE);
        TestEqual(TEXT("Dead anchor does not end otherwise-live match"), DeadAnchor.GetSnapshot().MatchPhase, EProofMatchPhase::Playing);
        BaseExpansionReject(*this, DeadAnchor, 1, BaseExpansionCommand(2, EProofCommandType::BuildFactory, FVector(1600, 0, 0)), TEXT("placement"));
        BaseExpansionReject(*this, DeadAnchor, 1, BaseExpansionCommand(3, EProofCommandType::BuildRefinery, FVector(1600, 0, 0)), TEXT("placement"));
        if (!BaseExpansionBuild(*this, DeadAnchor, 4, EProofCommandType::BuildRefinery, FVector(800, 0, 0))
            || !BaseExpansionBuild(*this, DeadAnchor, 5, EProofCommandType::BuildRefinery, FVector(1600, 0, 0))) return false;
        TestEqual(TEXT("Dead record is retained without becoming an overlap blocker or anchor"),
            BaseExpansionUnit(DeadAnchor.GetSnapshot(), Doomed).HitPoints, 0.f);

        FProofCommandModel BrokenChain;
        if (!TestTrue(TEXT("Seed disconnected surviving anchor fixture"), BaseExpansionSeed(BrokenChain))
            || !BaseExpansionBuild(*this, BrokenChain, 1, AnchorType, FVector(800, 0, 0))) return false;
        const int32 Intermediate = BaseExpansionKindId(BrokenChain.GetSnapshot(), 1,
            AnchorType == EProofCommandType::BuildFactory ? EProofUnitKind::Factory : EProofUnitKind::Refinery);
        const EProofCommandType DownstreamType = AnchorType == EProofCommandType::BuildFactory
            ? EProofCommandType::BuildRefinery : EProofCommandType::BuildFactory;
        if (!BaseExpansionBuild(*this, BrokenChain, 2, DownstreamType, FVector(1600, 0, 0))) return false;
        const int32 Downstream = BaseExpansionKindId(BrokenChain.GetSnapshot(), 1,
            DownstreamType == EProofCommandType::BuildFactory ? EProofUnitKind::Factory : EProofUnitKind::Refinery);
        if (!TestTrue(TEXT("Public combat breaks only the intermediate chain structure"), BaseExpansionEnemyAttack(BrokenChain, Intermediate))) return false;
        const FProofSnapshot Disconnected = BrokenChain.GetSnapshot();
        TestTrue(TEXT("Disconnected downstream structure remains alive"), BaseExpansionUnit(Disconnected, Downstream).HitPoints > 0.f);
        TestTrue(TEXT("Construction HQ survives broken chain"), BaseExpansionKindId(Disconnected, 1, EProofUnitKind::HQ) != INDEX_NONE);
        TestEqual(TEXT("Broken chain match is still playing"), Disconnected.MatchPhase, EProofMatchPhase::Playing);
        // Reach comes from any completed owned live structure. It is not a
        // connectivity graph that must still lead back to the construction HQ.
        if (!BaseExpansionBuild(*this, BrokenChain, 3, EProofCommandType::BuildRefinery, FVector(2400, 0, 0))) return false;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBaseExpansionAtomicityTest, "RedalertProof.Skirmish.BaseExpansion.RejectionAtomicity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBaseExpansionAtomicityTest::RunTest(const FString&)
{
    FProofCommandModel Invalid;
    if (!TestTrue(TEXT("Seed invalid-build fixture"), BaseExpansionSeed(Invalid))
        || !BaseExpansionBuild(*this, Invalid, 1, EProofCommandType::BuildFactory, FVector(800, 0, 0))) return false;
    for (double InvalidValue : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
    {
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            FVector Site(1600, 0, 0); Site[Axis] = InvalidValue;
            BaseExpansionReject(*this, Invalid, 1, BaseExpansionCommand(2, EProofCommandType::BuildRefinery, Site), TEXT("malformed"));
        }
    }
    BaseExpansionReject(*this, Invalid, 999, BaseExpansionCommand(2, EProofCommandType::BuildFactory, FVector(1600, 0, 0)), TEXT("unknown-player"));
    BaseExpansionReject(*this, Invalid, 1, BaseExpansionCommand(3, EProofCommandType::BuildFactory, FVector(1600, 0, 0)), TEXT("sequence"));
    BaseExpansionReject(*this, Invalid, 1, BaseExpansionCommand(2, EProofCommandType::BuildFactory, FVector(1601, 0, 0)), TEXT("placement"));
    BaseExpansionReject(*this, Invalid, 1, BaseExpansionCommand(2, EProofCommandType::BuildFactory, FVector(1600, 0, 0)), TEXT("sequence"));
    BaseExpansionReject(*this, Invalid, 1, BaseExpansionCommand(3, EProofCommandType::BuildRefinery, FVector(2401, 0, 0)), TEXT("placement"));
    Invalid.Advance(0.f);
    BaseExpansionReject(*this, Invalid, 1, BaseExpansionCommand(4, EProofCommandType::BuildRefinery, FVector(2401, 0, 0)), TEXT("placement"));
    if (!BaseExpansionBuild(*this, Invalid, 5, EProofCommandType::BuildRefinery, FVector(1600, 0, 0))) return false;
    BaseExpansionReject(*this, Invalid, 1, BaseExpansionCommand(5, EProofCommandType::BuildRefinery, FVector(2400, 0, 0)), TEXT("sequence"));
    BaseExpansionReject(*this, Invalid, 1, BaseExpansionCommand(6, EProofCommandType::BuildRefinery, FVector(3200, 0, 0)), TEXT("placement"));
    if (!BaseExpansionBuild(*this, Invalid, 7, EProofCommandType::BuildRefinery, FVector(2400, 0, 0))) return false;

    FProofCommandModel Limited(1), Control(1);
    if (!TestTrue(TEXT("Seed rate-limit differential fixtures"), BaseExpansionSeed(Limited) && BaseExpansionSeed(Control))
        || !BaseExpansionBuild(*this, Limited, 1, EProofCommandType::BuildFactory, FVector(800, 0, 0))
        || !BaseExpansionBuild(*this, Control, 1, EProofCommandType::BuildFactory, FVector(800, 0, 0))) return false;
    BaseExpansionReject(*this, Limited, 1, BaseExpansionCommand(2, EProofCommandType::BuildRefinery, FVector(1600, 0, 0)), TEXT("rate"));
    BaseExpansionReject(*this, Limited, 1, BaseExpansionCommand(2, EProofCommandType::BuildRefinery, FVector(1600, 0, 0)), TEXT("sequence"));
    Limited.Advance(0.f); Control.Advance(0.f);
    BaseExpansionReject(*this, Limited, 1, BaseExpansionCommand(3, EProofCommandType::BuildRefinery, FVector(2400, 0, 0)), TEXT("placement"));
    Limited.Advance(0.f);
    if (!BaseExpansionBuild(*this, Limited, 4, EProofCommandType::BuildRefinery, FVector(1600, 0, 0))
        || !BaseExpansionBuild(*this, Control, 2, EProofCommandType::BuildRefinery, FVector(1600, 0, 0))) return false;
    for (int32 Tick = 0; Tick < 400; ++Tick)
    {
        Limited.Advance(1.f); Control.Advance(1.f);
        TestEqual(TEXT("Failed/replayed/rate builds leave no hidden simulation order or allocator difference"),
            Limited.GetSnapshot().ComputeDigest(), Control.GetSnapshot().ComputeDigest());
        if (BaseExpansionPlayer(Limited.GetSnapshot()).TotalGathered == 500) break;
    }
    TestEqual(TEXT("Differential path completes the automatic finite mining order"), BaseExpansionPlayer(Limited.GetSnapshot()).TotalGathered, 500);
    Limited.Advance(100.f); Control.Advance(100.f);
    TestEqual(TEXT("Rejected commands cannot schedule delayed orders after ore exhaustion"),
        Limited.GetSnapshot().ComputeDigest(), Control.GetSnapshot().ComputeDigest());
    TestEqual(TEXT("Differential exhausted economy cannot mint income"), BaseExpansionPlayer(Limited.GetSnapshot()).Credits, 600);

    FProofCommandModel Poor;
    if (!TestTrue(TEXT("Seed resource rejection"), BaseExpansionSeed(Poor))
        || !BaseExpansionBuild(*this, Poor, 1, EProofCommandType::BuildRefinery, FVector(800, 0, 0))
        || !BaseExpansionBuild(*this, Poor, 2, EProofCommandType::BuildFactory, FVector(1600, 0, 0))) return false;
    BaseExpansionReject(*this, Poor, 1, BaseExpansionCommand(3, EProofCommandType::BuildFactory, FVector(2400, 0, 0)), TEXT("resources"));
    BaseExpansionReject(*this, Poor, 1, BaseExpansionCommand(4, EProofCommandType::BuildRefinery, FVector(3200, 0, 0)), TEXT("placement"));
    if (!BaseExpansionBuild(*this, Poor, 5, EProofCommandType::BuildRefinery, FVector(2400, 0, 0))) return false;

    FProofCommandModel Full;
    if (!TestTrue(TEXT("Seed capacity rejection"), BaseExpansionSeed(Full))
        || !BaseExpansionBuild(*this, Full, 1, EProofCommandType::BuildRefinery, FVector(800, 0, 0))
        || !BaseExpansionBuild(*this, Full, 2, EProofCommandType::BuildFactory, FVector(1600, 0, 0))
        || !TestTrue(TEXT("Earn finite credits for public capacity fixture"), BaseExpansionEarnOre(Full))) return false;
    const int32 Factory = BaseExpansionKindId(Full.GetSnapshot(), 1, EProofUnitKind::Factory);
    int32 Sequence = 3;
    for (int32 Count = 0; Count < 6; ++Count)
        if (!TestTrue(TEXT("Fill capacity using ordinary production"), Full.Submit(1,
            BaseExpansionCommand(Sequence++, EProofCommandType::Produce, FVector::ZeroVector, Factory)).bAccepted)) return false;
    TestEqual(TEXT("Exactly one live slot remains"), Full.GetSnapshot().Units.Num(), FProofCommandModel::SkirmishRecordCap - 1);
    BaseExpansionReject(*this, Full, 1, BaseExpansionCommand(Sequence++, EProofCommandType::BuildRefinery, FVector(2400, 0, 0)), TEXT("capacity"));
    BaseExpansionReject(*this, Full, 1, BaseExpansionCommand(Sequence++, EProofCommandType::BuildFactory, FVector(3200, 0, 0)), TEXT("placement"));
    if (!BaseExpansionBuild(*this, Full, Sequence, EProofCommandType::BuildFactory, FVector(2400, 0, 0))) return false;
    TestEqual(TEXT("Single-record expansion still fills final slot"), Full.GetSnapshot().Units.Num(), FProofCommandModel::SkirmishRecordCap);

    FProofCommandModel Blocked;
    const FVector BlockedSite(1600, 0, 0);
    if (!TestTrue(TEXT("Seed bounded spawn failure"), BaseExpansionSeed(Blocked))
        || !BaseExpansionBuild(*this, Blocked, 1, EProofCommandType::BuildFactory, FVector(800, 0, 0))) return false;
    FVector Spawn;
    int32 Blockers = 0;
    while (Blockers < 24 && FProofCommandModel::FindSkirmishSpawn(Blocked.GetSnapshot(), BlockedSite, Spawn))
    {
        if (!TestTrue(TEXT("Public foreign HQ blocks one real spawn candidate"), Blocked.SeedSkirmishPlayer(10 + Blockers, 0, Spawn))) return false;
        ++Blockers;
    }
    TestTrue(TEXT("Spawn-failure fixture has actual blockers"), Blockers > 0);
    TestFalse(TEXT("All existing bounded spawn candidates are occupied"), FProofCommandModel::FindSkirmishSpawn(Blocked.GetSnapshot(), BlockedSite, Spawn));
    BaseExpansionReject(*this, Blocked, 1, BaseExpansionCommand(2, EProofCommandType::BuildRefinery, BlockedSite), TEXT("spawn"));
    BaseExpansionReject(*this, Blocked, 1, BaseExpansionCommand(3, EProofCommandType::BuildFactory, FVector(2350, 250, 0)), TEXT("placement"));
    if (!BaseExpansionBuild(*this, Blocked, 4, EProofCommandType::BuildFactory, BlockedSite)) return false;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofBaseExpansionLifecycleTest, "RedalertProof.Skirmish.BaseExpansion.LifecycleAndDeterminism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofBaseExpansionLifecycleTest::RunTest(const FString&)
{
    FProofCommandModel Reset;
    if (!TestTrue(TEXT("Seed reset fixture"), BaseExpansionSeed(Reset))
        || !BaseExpansionBuild(*this, Reset, 1, EProofCommandType::BuildFactory, FVector(800, 0, 0))
        || !BaseExpansionBuild(*this, Reset, 2, EProofCommandType::BuildRefinery, FVector(1600, 0, 0))) return false;
    const FString FreshBuiltDigest = Reset.GetSnapshot().ComputeDigest();
    Reset.Reset();
    const FProofSnapshot Empty = Reset.GetSnapshot();
    TestTrue(TEXT("Reset removes all construction and economy records"), Empty.Units.Num() == 0 && Empty.Players.Num() == 0 && Empty.Resources.Num() == 0);
    TestEqual(TEXT("Reset returns to inactive"), Empty.MatchPhase, EProofMatchPhase::Inactive);
    TestEqual(TEXT("Old owner cannot build after Reset"), Reset.Submit(1,
        BaseExpansionCommand(3, EProofCommandType::BuildRefinery, FVector(2400, 0, 0))).Reason, FString(TEXT("unknown-player")));
    if (!TestTrue(TEXT("Start fresh skirmish after Reset"), BaseExpansionSeed(Reset))) return false;
    BaseExpansionReject(*this, Reset, 1, BaseExpansionCommand(1, EProofCommandType::BuildRefinery, FVector(1600, 0, 0)), TEXT("placement"));
    if (!BaseExpansionBuild(*this, Reset, 2, EProofCommandType::BuildFactory, FVector(800, 0, 0))
        || !BaseExpansionBuild(*this, Reset, 3, EProofCommandType::BuildRefinery, FVector(1600, 0, 0))) return false;
    TestEqual(TEXT("Reset clears hidden IDs, reach and orders despite admitted rejected envelope"), Reset.GetSnapshot().ComputeDigest(), FreshBuiltDigest);

    FProofCommandModel Finished;
    if (!TestTrue(TEXT("Seed terminal expansion fixture"), BaseExpansionSeed(Finished))
        || !BaseExpansionBuild(*this, Finished, 1, EProofCommandType::BuildFactory, FVector(800, 0, 0))) return false;
    const int32 HQ = BaseExpansionKindId(Finished.GetSnapshot(), 1, EProofUnitKind::HQ);
    if (!TestTrue(TEXT("Public combat reaches terminal phase"), BaseExpansionEnemyAttack(Finished, HQ))) return false;
    TestEqual(TEXT("Last allied HQ loss finishes match"), Finished.GetSnapshot().MatchPhase, EProofMatchPhase::Finished);
    TestEqual(TEXT("Enemy wins terminal fixture"), Finished.GetSnapshot().WinnerTeam, 1);
    BaseExpansionReject(*this, Finished, 1, BaseExpansionCommand(2, EProofCommandType::BuildFactory, FVector(1600, 0, 0)), TEXT("finished"));
    BaseExpansionReject(*this, Finished, 1, BaseExpansionCommand(2, EProofCommandType::BuildRefinery, FVector(1600, 0, 0)), TEXT("finished"));
    const FString TerminalDigest = Finished.GetSnapshot().ComputeDigest();
    Finished.Advance(0.f); Finished.Advance(120.f);
    TestEqual(TEXT("Terminal advances cannot finish or resume any rejected construction"), Finished.GetSnapshot().ComputeDigest(), TerminalDigest);

    FProofCommandModel Legacy;
    Legacy.AddPlayer(1);
    Legacy.SeedUnit(1, FVector::ZeroVector);
    BaseExpansionReject(*this, Legacy, 1, BaseExpansionCommand(1, EProofCommandType::BuildFactory, FVector(300, 0, 0)), TEXT("malformed"));
    BaseExpansionReject(*this, Legacy, 1, BaseExpansionCommand(1, EProofCommandType::BuildRefinery, FVector(300, 0, 0)), TEXT("malformed"));

    FProofCommandModel A, B;
    if (!TestTrue(TEXT("Seed independent deterministic models"), BaseExpansionSeed(A) && BaseExpansionSeed(B))) return false;
    const FProofCommand Commands[] = {
        BaseExpansionCommand(1, EProofCommandType::BuildRefinery, FVector(800, 0, 0)),
        BaseExpansionCommand(2, EProofCommandType::BuildFactory, FVector(1600, 0, 0)),
        BaseExpansionCommand(3, EProofCommandType::BuildRefinery, FVector(3000, 0, 0)),
        BaseExpansionCommand(4, EProofCommandType::BuildRefinery, FVector(2400, 0, 0)),
        BaseExpansionCommand(4, EProofCommandType::BuildRefinery, FVector(3200, 0, 0))
    };
    const bool Accepted[] = {true, true, false, true, false};
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const FProofCommandResult RA = A.Submit(1, Commands[Index]), RB = B.Submit(1, Commands[Index]);
        TestEqual(TEXT("Deterministic replay exercises expected acceptance, not two identically broken paths"), RA.bAccepted, Accepted[Index]);
        TestEqual(TEXT("Identical command history returns identical acceptance"), RA.bAccepted, RB.bAccepted);
        TestEqual(TEXT("Identical command history returns identical reason"), RA.Reason, RB.Reason);
        TestEqual(TEXT("Identical build history produces identical full digest"), A.GetSnapshot().ComputeDigest(), B.GetSnapshot().ComputeDigest());
    }
    for (int32 Tick = 0; Tick < 240; ++Tick)
    {
        const float Delta = Tick % 3 == 0 ? 0.f : Tick % 3 == 1 ? .5f : 2.f;
        A.Advance(Delta); B.Advance(Delta);
        TestEqual(TEXT("Identical advances preserve deterministic mining, positions and IDs"), A.GetSnapshot().ComputeDigest(), B.GetSnapshot().ComputeDigest());
        TestEqual(TEXT("Deterministic path preserves finite ore"), BaseExpansionOreTotal(A.GetSnapshot()), 500);
        if (BaseExpansionPlayer(A.GetSnapshot()).TotalGathered == 500) break;
    }
    TestEqual(TEXT("Deterministic economy reaches actual ore exhaustion"), BaseExpansionPlayer(A.GetSnapshot()).TotalGathered, 500);
    A.Advance(100.f); B.Advance(100.f);
    TestEqual(TEXT("Exhausted deterministic paths remain identical"), A.GetSnapshot().ComputeDigest(), B.GetSnapshot().ComputeDigest());
    TestEqual(TEXT("Exhausted deterministic paths cannot mint extra income"), BaseExpansionPlayer(A.GetSnapshot()).Credits, 500);
    if (!BaseExpansionBuild(*this, A, 5, EProofCommandType::BuildFactory, FVector(3200, 0, 0))
        || !BaseExpansionBuild(*this, B, 5, EProofCommandType::BuildFactory, FVector(3200, 0, 0))
        || !BaseExpansionBuild(*this, A, 6, EProofCommandType::BuildRefinery, FVector(4000, 0, 0))
        || !BaseExpansionBuild(*this, B, 6, EProofCommandType::BuildRefinery, FVector(4000, 0, 0))) return false;
    TestEqual(TEXT("Earned-credit expansion beyond two more anchors remains deterministic"), A.GetSnapshot().ComputeDigest(), B.GetSnapshot().ComputeDigest());
    FProofSnapshot Copy = A.GetSnapshot();
    const FString OriginalDigest = Copy.ComputeDigest();
    Copy.Units.Last().Position.X += 1.0;
    TestTrue(TEXT("Real MD5 digest commits changed visible unit position"), Copy.ComputeDigest() != OriginalDigest);
    TestEqual(TEXT("Copied-snapshot diagnostic changes cannot mutate model anchors"), A.GetSnapshot().ComputeDigest(), OriginalDigest);
    return true;
}

#if defined(_MSC_VER)
#pragma float_control(pop)
#endif
#endif
