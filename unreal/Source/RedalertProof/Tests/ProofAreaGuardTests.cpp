#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"
#include "Skirmish/ProofSkirmishBot.h"
#include <cmath>
#include <limits>

// The native target may use /fp:fast. Keep strict boundary test oracles precise.
#if defined(_MSC_VER)
#pragma float_control(precise, on, push)
#endif

namespace
{
FProofCommand AreaGuardCommand(int32 Sequence, EProofCommandType Type, int32 UnitId = INDEX_NONE,
    FVector Destination = FVector::ZeroVector, int32 TargetId = INDEX_NONE)
{
    FProofCommand Command;
    Command.Sequence = Sequence;
    Command.Type = Type;
    Command.UnitId = UnitId;
    Command.Destination = Destination;
    Command.TargetUnitId = TargetId;
    return Command;
}
FProofUnitRecord AreaGuardUnit(const FProofSnapshot& Snapshot, int32 Id)
{
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([Id](const FProofUnitRecord& Candidate)
        { return Candidate.UnitId == Id; });
    check(Unit);
    return *Unit;
}
int32 AreaGuardKind(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& Candidate)
        { return Candidate.OwnerId == Owner && Candidate.Kind == Kind && Candidate.HitPoints > 0.f; });
    return Unit ? Unit->UnitId : INDEX_NONE;
}
FProofPlayerRecord AreaGuardPlayer(const FProofSnapshot& Snapshot, int32 Owner)
{
    const FProofPlayerRecord* Player = Snapshot.Players.FindByPredicate([Owner](const FProofPlayerRecord& Candidate)
        { return Candidate.OwnerId == Owner; });
    check(Player);
    return *Player;
}
bool AreaGuardCleared(const FProofUnitRecord& Unit)
{
    return !Unit.bAreaGuard && Unit.AreaGuardPost == FVector::ZeroVector
        && Unit.AreaGuardTargetId == INDEX_NONE && !Unit.bAreaGuardReturning;
}
bool AreaGuardWithinLeash(const FProofUnitRecord& Unit)
{
    return FVector::DistSquared(Unit.Position, Unit.AreaGuardPost)
        <= static_cast<double>(FProofCommandModel::AttackRange) * FProofCommandModel::AttackRange;
}
int32 AreaGuardConservedOre(const FProofSnapshot& Snapshot)
{
    int32 Total = 0;
    for (const FProofResourceRecord& Ore : Snapshot.Resources) Total += Ore.Remaining;
    for (const FProofUnitRecord& Unit : Snapshot.Units) Total += Unit.Cargo;
    for (const FProofPlayerRecord& Player : Snapshot.Players) Total += Player.TotalGathered;
    return Total;
}
bool AreaGuardSameCommands(const TArray<FProofCommand>& A, const TArray<FProofCommand>& B)
{
    if (A.Num() != B.Num()) return false;
    for (int32 Index = 0; Index < A.Num(); ++Index)
        if (A[Index].Sequence != B[Index].Sequence || A[Index].MatchEpoch != B[Index].MatchEpoch
            || A[Index].Type != B[Index].Type || A[Index].UnitId != B[Index].UnitId
            || A[Index].TargetUnitId != B[Index].TargetUnitId || A[Index].Destination != B[Index].Destination)
            return false;
    return true;
}
// No private-state fixture: all actors, positions, health changes and orders are
// reachable through the same public commands used by authoritative adapters.
struct FAreaGuardScenario
{
    FProofCommandModel Model;
    int32 Guard = INDEX_NONE;
    int32 Target = INDEX_NONE;
    int32 OwnSequence = 4;
    int32 EnemySequence = 2;
    explicit FAreaGuardScenario(int32 Allowance = 32) : Model(Allowance) {}
    bool Init(const FVector& Post = FVector::ZeroVector, const FVector& Enemy = FVector(450, 0, 0))
    {
        Model.StartSkirmish();
        if (!Model.SeedSkirmishPlayer(1, 0, FVector(-8000, -8000, 0))
            || !Model.SeedSkirmishPlayer(100, 1, FVector(8000, -8000, 0))) return false;
        if (!Model.Submit(1, AreaGuardCommand(1, EProofCommandType::BuildFactory, INDEX_NONE, FVector(-7700, -8000, 0))).bAccepted)
            return false;
        if (!Model.Submit(1, AreaGuardCommand(2, EProofCommandType::Produce,
            AreaGuardKind(Model.GetSnapshot(), 1, EProofUnitKind::Factory))).bAccepted) return false;
        Model.Advance(0.f);
        Guard = AreaGuardKind(Model.GetSnapshot(), 1, EProofUnitKind::Combat);
        Target = AreaGuardKind(Model.GetSnapshot(), 100, EProofUnitKind::Harvester);
        if (!Model.Submit(1, AreaGuardCommand(3, EProofCommandType::Move, Guard, Post)).bAccepted
            || !Model.Submit(100, AreaGuardCommand(1, EProofCommandType::Move, Target, Enemy)).bAccepted) return false;
        Model.Advance(200.f);
        return AreaGuardUnit(Model.GetSnapshot(), Guard).Position == Post
            && AreaGuardUnit(Model.GetSnapshot(), Target).Position == Enemy
            && AreaGuardUnit(Model.GetSnapshot(), Guard).HitPoints == 100.f
            && AreaGuardUnit(Model.GetSnapshot(), Target).HitPoints == 100.f;
    }
    FProofCommandResult Own(EProofCommandType Type, FVector Destination = FVector::ZeroVector, int32 TargetId = INDEX_NONE)
    { return Model.Submit(1, AreaGuardCommand(OwnSequence++, Type, Guard, Destination, TargetId)); }
    FProofCommandResult MoveEnemy(const FVector& Destination)
    { return Model.Submit(100, AreaGuardCommand(EnemySequence++, EProofCommandType::Move, Target, Destination)); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAreaGuardAdmissionTest, "RedalertProof.Skirmish.AreaGuard.AdmissionAndReplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAreaGuardAdmissionTest::RunTest(const FString&)
{
    TestEqual(TEXT("AreaGuard is appended at ordinal ten"), static_cast<uint8>(EProofCommandType::AreaGuard), static_cast<uint8>(10));
    TestEqual(TEXT("ReturnCargo ordinal is unchanged"), static_cast<uint8>(EProofCommandType::ReturnCargo), static_cast<uint8>(9));
    TestTrue(TEXT("New records have canonical cleared metadata"), AreaGuardCleared(FProofUnitRecord()));
    FAreaGuardScenario S;
    if (!TestTrue(TEXT("Public production and positioning fixture"), S.Init())) return false;
    const FString Initial = S.Model.GetSnapshot().ComputeDigest();
    const int32 InitialRevision = S.Model.GetSnapshot().Revision;
    const FProofCommand Malformed[] = {
        AreaGuardCommand(4, EProofCommandType::AreaGuard, S.Guard, FVector::ZeroVector, S.Target),
        AreaGuardCommand(4, EProofCommandType::AreaGuard, S.Guard, FVector(0, 0, 1.e-12)),
        AreaGuardCommand(4, EProofCommandType::AreaGuard, S.Guard, FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0)),
        AreaGuardCommand(4, EProofCommandType::AreaGuard, INDEX_NONE),
        AreaGuardCommand(0, EProofCommandType::AreaGuard, S.Guard),
        AreaGuardCommand(4, static_cast<EProofCommandType>(255), S.Guard)
    };
    for (const FProofCommand& Command : Malformed)
    {
        TestEqual(TEXT("Noncanonical envelope is malformed"), S.Model.Submit(1, Command).Reason, FString(TEXT("malformed")));
        TestEqual(TEXT("Malformed envelope preserves every visible field and revision"), S.Model.GetSnapshot().ComputeDigest(), Initial);
    }
    TestTrue(TEXT("Malformed commands did not consume valid sequence"), S.Own(EProofCommandType::AreaGuard).bAccepted);
    const FProofSnapshot Guarding = S.Model.GetSnapshot();
    const FProofUnitRecord Guard = AreaGuardUnit(Guarding, S.Guard);
    TestTrue(TEXT("Accepted command captures authoritative current position"), Guard.bAreaGuard
        && Guard.AreaGuardPost == Guard.Position && Guard.AreaGuardTargetId == INDEX_NONE && !Guard.bAreaGuardReturning);
    TestEqual(TEXT("One accepted guard bumps revision once"), Guarding.Revision, InitialRevision + 1);
    TestEqual(TEXT("Duplicate sequence is rejected"), S.Model.Submit(1, AreaGuardCommand(4, EProofCommandType::AreaGuard, S.Guard)).Reason, FString(TEXT("sequence")));
    TestEqual(TEXT("Foreign actor is rejected"), S.Model.Submit(100, AreaGuardCommand(2, EProofCommandType::AreaGuard, S.Guard)).Reason, FString(TEXT("ownership")));
    TestEqual(TEXT("Ownership rejection consumes ordered envelope"), S.Model.Submit(100, AreaGuardCommand(2, EProofCommandType::AreaGuard, S.Guard)).Reason, FString(TEXT("sequence")));
    TestEqual(TEXT("Unknown trusted owner is rejected"), S.Model.Submit(999, AreaGuardCommand(1, EProofCommandType::AreaGuard, S.Guard)).Reason, FString(TEXT("unknown-player")));
    for (EProofUnitKind Kind : {EProofUnitKind::Harvester, EProofUnitKind::HQ, EProofUnitKind::Factory})
        TestEqual(TEXT("Noncombat role cannot guard"), S.Model.Submit(1, AreaGuardCommand(S.OwnSequence++, EProofCommandType::AreaGuard,
            AreaGuardKind(S.Model.GetSnapshot(), 1, Kind))).Reason, FString(TEXT("role")));
    TestEqual(TEXT("Rejected Move cannot replace guard"), S.Own(EProofCommandType::Move, FVector(10001, 0, 0)).Reason, FString(TEXT("position")));
    TestEqual(TEXT("Rejected Attack cannot replace guard"), S.Own(EProofCommandType::Attack, FVector::ZeroVector, MAX_int32).Reason, FString(TEXT("target")));
    TestEqual(TEXT("All gameplay and envelope rejections preserve active guard state"), S.Model.GetSnapshot().ComputeDigest(), Guarding.ComputeDigest());
    FAreaGuardScenario Limited(2);
    if (!TestTrue(TEXT("Rate-limited public fixture"), Limited.Init())) return false;
    TestTrue(TEXT("First guard fits allowance"), Limited.Own(EProofCommandType::AreaGuard).bAccepted);
    TestTrue(TEXT("Fresh guard uses second ordinary allowance"), Limited.Own(EProofCommandType::AreaGuard).bAccepted);
    const FString BeforeRate = Limited.Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Third guard cannot bypass rate limit"), Limited.Own(EProofCommandType::AreaGuard).Reason, FString(TEXT("rate")));
    TestEqual(TEXT("Rate rejection preserves guard"), Limited.Model.GetSnapshot().ComputeDigest(), BeforeRate);
    TestEqual(TEXT("Rate failure consumes sequence"), Limited.Model.Submit(1, AreaGuardCommand(6, EProofCommandType::Stop, Limited.Guard)).Reason, FString(TEXT("sequence")));
    Limited.Model.Advance(0.f);
    TestTrue(TEXT("Zero advance resets allowance without simulation"), Limited.Own(EProofCommandType::AreaGuard).bAccepted);
    FProofCommandModel Legacy;
    Legacy.AddPlayer(1);
    const int32 LegacyId = Legacy.SeedUnit(1, FVector::ZeroVector);
    TestEqual(TEXT("Legacy fixture does not silently enable skirmish guard"), Legacy.Submit(1, AreaGuardCommand(1, EProofCommandType::AreaGuard, LegacyId)).Reason, FString(TEXT("malformed")));
    TestTrue(TEXT("Legacy malformed guard leaves ordinary Move sequence usable"), Legacy.Submit(1, AreaGuardCommand(1, EProofCommandType::Move, LegacyId)).bAccepted);
    FAreaGuardScenario A, B;
    if (!TestTrue(TEXT("Independent replay fixtures"), A.Init() && B.Init())) return false;
    for (int32 Tick = 0; Tick < 12; ++Tick)
    {
        if (Tick == 0 || Tick == 6) { A.Own(EProofCommandType::AreaGuard); B.Own(EProofCommandType::AreaGuard); }
        if (Tick == 3) { A.MoveEnemy(FVector(1000, 0, 0)); B.MoveEnemy(FVector(1000, 0, 0)); }
        A.Model.Advance(.25f); B.Model.Advance(.25f);
        TestEqual(TEXT("Identical admitted command history replays every visible digest"), A.Model.GetSnapshot().ComputeDigest(), B.Model.GetSnapshot().ComputeDigest());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAreaGuardAcquireTest, "RedalertProof.Skirmish.AreaGuard.AcquireChaseReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAreaGuardAcquireTest::RunTest(const FString&)
{
    FAreaGuardScenario S;
    if (!TestTrue(TEXT("Acquisition fixture"), S.Init())) return false;
    TestTrue(TEXT("Public guard accepted"), S.Own(EProofCommandType::AreaGuard).bAccepted);
    S.Model.Advance(.5f);
    FProofUnitRecord Guard = AreaGuardUnit(S.Model.GetSnapshot(), S.Guard);
    TestEqual(TEXT("Acquires enemy outside strict weapon range"), Guard.AreaGuardTargetId, S.Target);
    TestEqual(TEXT("Chases using existing movement speed"), Guard.Position, FVector(75, 0, 0));
    TestEqual(TEXT("Out-of-range chase cannot fire"), AreaGuardUnit(S.Model.GetSnapshot(), S.Target).HitPoints, 100.f);
    FAreaGuardScenario Stopped;
    if (!TestTrue(TEXT("Equivalent Stop differential fixture"), Stopped.Init())) return false;
    TestTrue(TEXT("Ordinary Stop remains accepted"), Stopped.Own(EProofCommandType::Stop).bAccepted);
    Stopped.Model.Advance(.5f);
    TestEqual(TEXT("Stop cannot autonomously chase the same outside-weapon enemy"), AreaGuardUnit(Stopped.Model.GetSnapshot(), Stopped.Guard).Position, FVector::ZeroVector);
    TestEqual(TEXT("Stop cannot fire at the outside-weapon enemy"), AreaGuardUnit(Stopped.Model.GetSnapshot(), Stopped.Target).HitPoints, 100.f);
    TestTrue(TEXT("AreaGuard public behavior differs from stationary Stop"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).Position
        != AreaGuardUnit(Stopped.Model.GetSnapshot(), Stopped.Guard).Position);
    S.Model.Advance(.5f);
    TestEqual(TEXT("Chase stops exactly at weapon range"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).Position, FVector(150, 0, 0));
    TestEqual(TEXT("Arrival fires once using existing DPS"), AreaGuardUnit(S.Model.GetSnapshot(), S.Target).HitPoints, 87.5f);
    TestTrue(TEXT("Enemy actually flees through Move"), S.MoveEnemy(FVector(1000, 0, 0)).bAccepted);
    S.Model.Advance(2.f);
    Guard = AreaGuardUnit(S.Model.GetSnapshot(), S.Guard);
    TestEqual(TEXT("All movers read common pre-move target position"), Guard.Position, FVector(150, 0, 0));
    TestTrue(TEXT("Post-move scan loss is published before firing"), Guard.bAreaGuardReturning && Guard.AreaGuardTargetId == INDEX_NONE);
    TestEqual(TEXT("Scan loss cannot grant a stale shot"), AreaGuardUnit(S.Model.GetSnapshot(), S.Target).HitPoints, 87.5f);
    S.Model.Advance(.25f);
    TestEqual(TEXT("Return starts next advance with one normal movement budget"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).Position, FVector(112.5, 0, 0));
    TestEqual(TEXT("Return records actual homeward travel heading"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).LastTravelDirection, FVector(-1, 0, 0));
    TestTrue(TEXT("Enemy returns to firing distance during homeward trip"), S.MoveEnemy(FVector(250, 0, 0)).bAccepted);
    S.Model.Advance(4.f);
    Guard = AreaGuardUnit(S.Model.GetSnapshot(), S.Guard);
    TestEqual(TEXT("Large return lands exactly at original post"), Guard.Position, FVector::ZeroVector);
    TestTrue(TEXT("Home arrival still publishes returning phase without reacquisition"), Guard.bAreaGuardReturning && Guard.AreaGuardTargetId == INDEX_NONE);
    TestEqual(TEXT("Return and arrival cannot fire even with an enemy in range"), AreaGuardUnit(S.Model.GetSnapshot(), S.Target).HitPoints, 87.5f);
    const FString HomeDigest = S.Model.GetSnapshot().ComputeDigest();
    S.Model.Advance(0.f); S.Model.Advance(-1.f);
    S.Model.Advance(std::numeric_limits<float>::infinity());
    S.Model.Advance(std::numeric_limits<float>::quiet_NaN());
    TestEqual(TEXT("Zero negative and nonfinite advances cannot complete return phase or acquire"), S.Model.GetSnapshot().ComputeDigest(), HomeDigest);
    TestTrue(TEXT("Exact-home return remains committed until a positive advance"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).bAreaGuardReturning);
    S.Model.Advance(.5f);
    Guard = AreaGuardUnit(S.Model.GetSnapshot(), S.Guard);
    TestTrue(TEXT("Following advance resumes acquisition"), !Guard.bAreaGuardReturning && Guard.AreaGuardTargetId == S.Target);
    TestEqual(TEXT("Resumed guard fires only its ordinary single shot"), AreaGuardUnit(S.Model.GetSnapshot(), S.Target).HitPoints, 75.f);

    FAreaGuardScenario Entering;
    if (!TestTrue(TEXT("Incoming target fixture"), Entering.Init(FVector::ZeroVector, FVector(750, 0, 0)))) return false;
    Entering.Own(EProofCommandType::AreaGuard);
    TestTrue(TEXT("Outside-scan target moves into weapon range"), Entering.MoveEnemy(FVector(250, 0, 0)).bAccepted);
    Entering.Model.Advance(4.f);
    TestEqual(TEXT("Target entering scan only after movement is not acquired early"), AreaGuardUnit(Entering.Model.GetSnapshot(), Entering.Guard).AreaGuardTargetId, static_cast<int32>(INDEX_NONE));
    TestEqual(TEXT("Guard with no pre-move acquisition stays at post"), AreaGuardUnit(Entering.Model.GetSnapshot(), Entering.Guard).Position, FVector::ZeroVector);
    TestEqual(TEXT("Target entering weapon range receives no same-tick guard or idle shot"), AreaGuardUnit(Entering.Model.GetSnapshot(), Entering.Target).HitPoints, 100.f);
    Entering.Model.Advance(.1f);
    TestEqual(TEXT("Next positive advance acquires newly arrived target"), AreaGuardUnit(Entering.Model.GetSnapshot(), Entering.Guard).AreaGuardTargetId, Entering.Target);
    TestEqual(TEXT("Newly acquired target receives exactly one normal shot"), AreaGuardUnit(Entering.Model.GetSnapshot(), Entering.Target).HitPoints, 97.5f);

    for (int32 Reverse = 0; Reverse < 2; ++Reverse)
    {
        FAreaGuardScenario Tie;
        if (!TestTrue(TEXT("Tie fixture"), Tie.Init())) return false;
        const int32 OtherOwner = 200;
        TestTrue(TEXT("Additional hostile seat seeded"), Tie.Model.SeedSkirmishPlayer(OtherOwner, 1, FVector(0, 8000, 0)));
        const int32 Other = AreaGuardKind(Tie.Model.GetSnapshot(), OtherOwner, EProofUnitKind::Harvester);
        if (Reverse) TestTrue(TEXT("Original target placed on other tie axis"), Tie.MoveEnemy(FVector(0, 450, 0)).bAccepted);
        TestTrue(TEXT("Other target positioned equally far from post"), Tie.Model.Submit(OtherOwner,
            AreaGuardCommand(1, EProofCommandType::Move, Other, Reverse ? FVector(450, 0, 0) : FVector(0, 450, 0))).bAccepted);
        Tie.Model.Advance(100.f);
        Tie.Own(EProofCommandType::AreaGuard); Tie.Model.Advance(.25f);
        TestEqual(TEXT("Lowest stable UnitId resolves equal post distance"), AreaGuardUnit(Tie.Model.GetSnapshot(), Tie.Guard).AreaGuardTargetId, Tie.Target);
    }
    FAreaGuardScenario Nearest;
    if (!TestTrue(TEXT("Nearest hostile and friendly-team fixture"), Nearest.Init())) return false;
    TestTrue(TEXT("Closer friendly seat exists inside scan and weapon range"), Nearest.Model.SeedSkirmishPlayer(2, 0, FVector(0, 300, 0)));
    TestTrue(TEXT("Second opposing seat seeded"), Nearest.Model.SeedSkirmishPlayer(200, 1, FVector(0, 8000, 0)));
    const int32 NearestEnemy = AreaGuardKind(Nearest.Model.GetSnapshot(), 200, EProofUnitKind::Harvester);
    TestTrue(TEXT("Higher-ID hostile placed nearer than first hostile"), Nearest.Model.Submit(200,
        AreaGuardCommand(1, EProofCommandType::Move, NearestEnemy, FVector(0, 400, 0))).bAccepted);
    Nearest.Model.Advance(100.f);
    Nearest.Own(EProofCommandType::AreaGuard); Nearest.Model.Advance(.25f);
    TestEqual(TEXT("Nearest hostile beats lower ID and closer friendly structures"), AreaGuardUnit(Nearest.Model.GetSnapshot(), Nearest.Guard).AreaGuardTargetId, NearestEnemy);
    TestEqual(TEXT("Friendly team never receives guard damage"), AreaGuardUnit(Nearest.Model.GetSnapshot(),
        AreaGuardKind(Nearest.Model.GetSnapshot(), 2, EProofUnitKind::HQ)).HitPoints, 300.f);
    FAreaGuardScenario Retain;
    if (!TestTrue(TEXT("Retention fixture"), Retain.Init())) return false;
    TestTrue(TEXT("Closer-candidate seat seeded"), Retain.Model.SeedSkirmishPlayer(200, 1, FVector(0, 8000, 0)));
    const int32 Other = AreaGuardKind(Retain.Model.GetSnapshot(), 200, EProofUnitKind::Harvester);
    Retain.Model.Submit(200, AreaGuardCommand(1, EProofCommandType::Move, Other, FVector(0, 500, 0)));
    Retain.Model.Advance(100.f);
    Retain.Own(EProofCommandType::AreaGuard); Retain.Model.Advance(.25f);
    Retain.Model.Submit(200, AreaGuardCommand(2, EProofCommandType::Move, Other, FVector(0, 350, 0)));
    Retain.Model.Advance(1.f); Retain.Model.Advance(.1f);
    TestEqual(TEXT("Valid retained target wins over newly nearer hostile"), AreaGuardUnit(Retain.Model.GetSnapshot(), Retain.Guard).AreaGuardTargetId, Retain.Target);
    TestTrue(TEXT("Fresh guard explicitly reanchors and clears target"), Retain.Own(EProofCommandType::AreaGuard).bAccepted);
    const FVector FreshPost = AreaGuardUnit(Retain.Model.GetSnapshot(), Retain.Guard).Position;
    TestEqual(TEXT("Fresh guard records current authoritative position"), AreaGuardUnit(Retain.Model.GetSnapshot(), Retain.Guard).AreaGuardPost, FreshPost);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAreaGuardLeashTest, "RedalertProof.Skirmish.AreaGuard.LeashAndTargetLoss", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAreaGuardLeashTest::RunTest(const FString&)
{
    const double Scan = 2.0 * FProofCommandModel::AttackRange;
    const double OutsideScan = std::nextafter(Scan, std::numeric_limits<double>::infinity());
    for (double Distance : {Scan, OutsideScan})
    {
        FAreaGuardScenario S;
        if (!TestTrue(TEXT("Strict scan boundary fixture"), S.Init(FVector::ZeroVector, FVector(Distance, 0, 0)))) return false;
        S.Own(EProofCommandType::AreaGuard); S.Model.Advance(.5f);
        const FProofUnitRecord Guard = AreaGuardUnit(S.Model.GetSnapshot(), S.Guard);
        TestEqual(TEXT("Closed scan has no epsilon beyond nextafter boundary"), Guard.AreaGuardTargetId,
            Distance == Scan ? S.Target : static_cast<int32>(INDEX_NONE));
        TestEqual(TEXT("Outside scan never starts pursuit"), Guard.Position, Distance == Scan ? FVector(75, 0, 0) : FVector::ZeroVector);
    }
    const FVector Posts[] = {FVector::ZeroVector, FVector(-1100, -2300, 0), FVector(9400, 9000, 0), FVector(0, 0, 75)};
    const FVector Offsets[] = {FVector(600, 0, 0), FVector(-360, -480, 0), FVector(360, 480, 0), FVector(0, 360, 480)};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        for (int32 Partition = 0; Partition < 2; ++Partition)
        {
            FAreaGuardScenario S;
            if (!TestTrue(TEXT("Negative diagonal vertical and world-edge fixture"), S.Init(Posts[Index], Posts[Index] + Offsets[Index]))) return false;
            S.Own(EProofCommandType::AreaGuard);
            for (int32 Step = 0; Step < (Partition ? 8 : 1); ++Step)
            {
                S.Model.Advance(Partition ? .25f : 2.f);
                const FProofUnitRecord Guard = AreaGuardUnit(S.Model.GetSnapshot(), S.Guard);
                TestTrue(TEXT("Every endpoint remains in strict closed home-relative leash"), AreaGuardWithinLeash(Guard));
                TestEqual(TEXT("Travel never silently reanchors post"), Guard.AreaGuardPost, Posts[Index]);
            }
            S.Model.Advance(std::numeric_limits<float>::max());
            TestTrue(TEXT("Maximum finite DeltaSeconds cannot overshoot leash"), AreaGuardWithinLeash(AreaGuardUnit(S.Model.GetSnapshot(), S.Guard)));
        }
    }
    // A target can be inside the closed scan yet require an endpoint only one
    // representable step outside the leash. No tolerance may admit that step.
    FAreaGuardScenario Rounded;
    if (!TestTrue(TEXT("Rounded strict-leash fixture"), Rounded.Init(FVector::ZeroVector, FVector(600, 0, 0)))) return false;
    Rounded.Own(EProofCommandType::AreaGuard); Rounded.Model.Advance(2.f);
    const FVector NearlyTangent(std::nextafter(600.0, 0.0), 1.e-5, 0);
    TestTrue(TEXT("Adversarial target remains in closed scan"), NearlyTangent.SizeSquared() <= 360000.0);
    TestTrue(TEXT("Adversarial target is strictly outside weapon range at leash endpoint"),
        FVector::DistSquared(FVector(300, 0, 0), NearlyTangent) > 90000.0);
    TestTrue(TEXT("Target makes representable near-tangent Move"), Rounded.MoveEnemy(NearlyTangent).bAccepted);
    Rounded.Model.Advance(.001f);
    const float BeforeRoundedHP = AreaGuardUnit(Rounded.Model.GetSnapshot(), Rounded.Target).HitPoints;
    TestEqual(TEXT("Common pre-move snapshot keeps boundary guard stationary for target's move"),
        AreaGuardUnit(Rounded.Model.GetSnapshot(), Rounded.Guard).Position, FVector(300, 0, 0));
    Rounded.Model.Advance(.001f);
    const FProofUnitRecord RoundedGuard = AreaGuardUnit(Rounded.Model.GetSnapshot(), Rounded.Guard);
    TestTrue(TEXT("Even a rounded one-ULP outside endpoint must abandon pursuit"),
        RoundedGuard.bAreaGuardReturning && RoundedGuard.AreaGuardTargetId == INDEX_NONE);
    TestEqual(TEXT("Rejected rounded chase uses this step's sole budget toward home"),
        RoundedGuard.Position, FVector(300.0 - FProofCommandModel::MovementSpeed * static_cast<double>(.001f), 0, 0));
    TestEqual(TEXT("Strict rounded-leash return grants no shot"), AreaGuardUnit(Rounded.Model.GetSnapshot(), Rounded.Target).HitPoints, BeforeRoundedHP);
    FAreaGuardScenario Bend;
    if (!TestTrue(TEXT("Changing-direction leash fixture"), Bend.Init(FVector::ZeroVector, FVector(600, 0, 0)))) return false;
    Bend.Own(EProofCommandType::AreaGuard); Bend.Model.Advance(2.f);
    TestEqual(TEXT("Closed leash boundary is reachable"), AreaGuardUnit(Bend.Model.GetSnapshot(), Bend.Guard).Position, FVector(300, 0, 0));
    TestTrue(TEXT("Live target moves around post inside scan"), Bend.MoveEnemy(FVector(0, 600, 0)).bAccepted);
    Bend.Model.Advance(4.f); Bend.Model.Advance(2.f);
    const FProofUnitRecord BeforeBend = AreaGuardUnit(Bend.Model.GetSnapshot(), Bend.Guard);
    TestTrue(TEXT("Bent approach remains displaced and in leash"), BeforeBend.Position != FVector::ZeroVector && AreaGuardWithinLeash(BeforeBend));
    const float BeforeReturnHP = AreaGuardUnit(Bend.Model.GetSnapshot(), Bend.Target).HitPoints;
    Bend.Model.Advance(2.f);
    const FProofUnitRecord AfterBend = AreaGuardUnit(Bend.Model.GetSnapshot(), Bend.Guard);
    TestEqual(TEXT("Predicted outside-leash step spends budget returning, not chasing"), AfterBend.Position, FVector::ZeroVector);
    TestTrue(TEXT("Leash abandonment does not reacquire at home in same advance"), AfterBend.bAreaGuardReturning && AfterBend.AreaGuardTargetId == INDEX_NONE);
    TestEqual(TEXT("Leash return cannot fire"), AreaGuardUnit(Bend.Model.GetSnapshot(), Bend.Target).HitPoints, BeforeReturnHP);

    FAreaGuardScenario Removed;
    if (!TestTrue(TEXT("Owner-removal fixture"), Removed.Init())) return false;
    TestTrue(TEXT("Surviving opposing seat prevents terminal shortcut"), Removed.Model.SeedSkirmishPlayer(200, 1, FVector(0, 8000, 0)));
    Removed.Own(EProofCommandType::AreaGuard); Removed.Model.Advance(.5f);
    TestTrue(TEXT("Actual target owner removed"), Removed.Model.RemoveSkirmishPlayer(100));
    FProofUnitRecord Guard = AreaGuardUnit(Removed.Model.GetSnapshot(), Removed.Guard);
    TestEqual(TEXT("Match remains playing after target-owner removal"), Removed.Model.GetSnapshot().MatchPhase, EProofMatchPhase::Playing);
    TestTrue(TEXT("Missing target immediately publishes return with preserved post"), Guard.bAreaGuard && Guard.bAreaGuardReturning
        && Guard.AreaGuardTargetId == INDEX_NONE && Guard.AreaGuardPost == FVector::ZeroVector);
    Removed.Model.Advance(.25f);
    TestEqual(TEXT("Owner removal returns surviving guard with ordinary speed"), AreaGuardUnit(Removed.Model.GetSnapshot(), Removed.Guard).Position, FVector(37.5, 0, 0));
    Removed.Model.Advance(.25f);
    TestEqual(TEXT("Missing-target home endpoint is exact"), AreaGuardUnit(Removed.Model.GetSnapshot(), Removed.Guard).Position, FVector::ZeroVector);
    FAreaGuardScenario Tiny;
    const FVector TinyPost(123.25, -85.75, 0);
    if (!TestTrue(TEXT("Sub-epsilon return fixture"), Tiny.Init(TinyPost, TinyPost + FVector(450, 0, 0)))) return false;
    TestTrue(TEXT("Sub-epsilon fixture retains another enemy seat"), Tiny.Model.SeedSkirmishPlayer(200, 1, FVector(0, 8000, 0)));
    Tiny.Own(EProofCommandType::AreaGuard); Tiny.Model.Advance(1.e-12f);
    const double TinyDistance = FVector::Dist(AreaGuardUnit(Tiny.Model.GetSnapshot(), Tiny.Guard).Position, TinyPost);
    TestTrue(TEXT("Public chase produces a representable sub-epsilon displacement"), TinyDistance > 0.0 && TinyDistance < UE_SMALL_NUMBER);
    TestTrue(TEXT("Tiny displaced guard loses its actual target"), Tiny.Model.RemoveSkirmishPlayer(100));
    Tiny.Model.Advance(1.f);
    TestEqual(TEXT("Sub-epsilon return lands exactly rather than sticking near home"), AreaGuardUnit(Tiny.Model.GetSnapshot(), Tiny.Guard).Position, TinyPost);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAreaGuardLifecycleTest, "RedalertProof.Skirmish.AreaGuard.OverridesAndLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAreaGuardLifecycleTest::RunTest(const FString&)
{
    for (EProofCommandType Type : {EProofCommandType::Move, EProofCommandType::Attack, EProofCommandType::AttackMove, EProofCommandType::Stop})
    {
        FAreaGuardScenario S;
        if (!TestTrue(TEXT("Override fixture"), S.Init())) return false;
        S.Own(EProofCommandType::AreaGuard); S.Model.Advance(.5f);
        TestTrue(TEXT("Explicit ordinary order replaces guard"), S.Own(Type,
            Type == EProofCommandType::Move || Type == EProofCommandType::AttackMove ? FVector(-1000, 0, 0) : FVector::ZeroVector,
            Type == EProofCommandType::Attack ? S.Target : static_cast<int32>(INDEX_NONE)).bAccepted);
        TestTrue(TEXT("Every accepted override clears all four guard fields"), AreaGuardCleared(AreaGuardUnit(S.Model.GetSnapshot(), S.Guard)));
        S.Model.Advance(.5f);
        TestTrue(TEXT("Old guard cannot reappear during replacement execution"), AreaGuardCleared(AreaGuardUnit(S.Model.GetSnapshot(), S.Guard)));
        if (Type == EProofCommandType::Stop)
            TestEqual(TEXT("Stop still cancels chase without returning home"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).Position, FVector(75, 0, 0));
        if (Type == EProofCommandType::Move || Type == EProofCommandType::AttackMove)
            TestEqual(TEXT("Replacement route travels west rather than old eastward chase"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).Position, FVector::ZeroVector);
    }
    FAreaGuardScenario Reanchor;
    if (!TestTrue(TEXT("Reanchor fixture"), Reanchor.Init())) return false;
    Reanchor.Own(EProofCommandType::AreaGuard); Reanchor.Model.Advance(.5f);
    const FVector NewPost = AreaGuardUnit(Reanchor.Model.GetSnapshot(), Reanchor.Guard).Position;
    TestTrue(TEXT("Fresh accepted guard reanchors"), Reanchor.Own(EProofCommandType::AreaGuard).bAccepted);
    FProofUnitRecord Guard = AreaGuardUnit(Reanchor.Model.GetSnapshot(), Reanchor.Guard);
    TestTrue(TEXT("Fresh reanchor clears acquisition and return phase"), Guard.bAreaGuard && Guard.AreaGuardPost == NewPost
        && Guard.AreaGuardTargetId == INDEX_NONE && !Guard.bAreaGuardReturning);
    TestTrue(TEXT("Last enemy removal reaches actual terminal path"), Reanchor.Model.RemoveSkirmishPlayer(100));
    TestEqual(TEXT("Terminal match is finished"), Reanchor.Model.GetSnapshot().MatchPhase, EProofMatchPhase::Finished);
    for (const FProofUnitRecord& Unit : Reanchor.Model.GetSnapshot().Units)
        TestTrue(TEXT("Terminal order clearing canonicalizes every guard record"), AreaGuardCleared(Unit));
    const FString Terminal = Reanchor.Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Finished match rejects guard"), Reanchor.Own(EProofCommandType::AreaGuard).Reason, FString(TEXT("finished")));
    Reanchor.Model.Advance(100.f);
    TestEqual(TEXT("Terminal advance cannot resume guard"), Reanchor.Model.GetSnapshot().ComputeDigest(), Terminal);
    FAreaGuardScenario DamageTerminal;
    if (!TestTrue(TEXT("Damage-triggered terminal fixture"), DamageTerminal.Init())) return false;
    const int32 EnemyHQ = AreaGuardKind(DamageTerminal.Model.GetSnapshot(), 100, EProofUnitKind::HQ);
    const FVector SiegePost = AreaGuardUnit(DamageTerminal.Model.GetSnapshot(), EnemyHQ).Position + FVector(0, 450, 0);
    TestTrue(TEXT("Guard moves to siege post through ordinary command"), DamageTerminal.Own(EProofCommandType::Move, SiegePost).bAccepted);
    DamageTerminal.Model.Advance(200.f);
    TestTrue(TEXT("Guard captures siege post"), DamageTerminal.Own(EProofCommandType::AreaGuard).bAccepted);
    DamageTerminal.Model.Advance(1.f);
    TestEqual(TEXT("Guard attacks actual hostile HQ"), AreaGuardUnit(DamageTerminal.Model.GetSnapshot(), EnemyHQ).HitPoints, 275.f);
    DamageTerminal.Model.Advance(11.f);
    TestEqual(TEXT("Actual guard damage kills last enemy HQ"), AreaGuardUnit(DamageTerminal.Model.GetSnapshot(), EnemyHQ).HitPoints, 0.f);
    TestEqual(TEXT("Damage evaluation reaches Finished"), DamageTerminal.Model.GetSnapshot().MatchPhase, EProofMatchPhase::Finished);
    TestEqual(TEXT("Actual HQ kill preserves normal winner evaluation"), DamageTerminal.Model.GetSnapshot().WinnerTeam, 0);
    for (const FProofUnitRecord& Unit : DamageTerminal.Model.GetSnapshot().Units)
        TestTrue(TEXT("Damage-terminal order clearing removes every guard field"), AreaGuardCleared(Unit));
    Reanchor.Model.Reset();
    TestTrue(TEXT("Reset removes all actors and guard state"), Reanchor.Model.GetSnapshot().Units.IsEmpty());
    if (!TestTrue(TEXT("Same model reseeded through normal public path"), Reanchor.Init(FVector(-500, -200, 0), FVector(-50, -200, 0)))) return false;
    TestEqual(TEXT("Reseed reuses expected identity safely"), Reanchor.Guard, 6);
    for (const FProofUnitRecord& Unit : Reanchor.Model.GetSnapshot().Units)
        TestTrue(TEXT("Reseed cannot inherit any old guard metadata"), AreaGuardCleared(Unit));
    Reanchor.Model.Advance(1.f);
    TestEqual(TEXT("Reused actor does not execute prior guard order"), AreaGuardUnit(Reanchor.Model.GetSnapshot(), Reanchor.Guard).Position, FVector(-500, -200, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAreaGuardDamageTest, "RedalertProof.Skirmish.AreaGuard.DamageAndConservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAreaGuardDamageTest::RunTest(const FString&)
{
    FAreaGuardScenario S;
    if (!TestTrue(TEXT("Target-death fixture"), S.Init())) return false;
    TestTrue(TEXT("Alternative enemy seat survives target death"), S.Model.SeedSkirmishPlayer(200, 1, FVector(0, 8000, 0)));
    const int32 Alternative = AreaGuardKind(S.Model.GetSnapshot(), 200, EProofUnitKind::Harvester);
    TestTrue(TEXT("Alternative target placed at equal distance with higher ID"), S.Model.Submit(200,
        AreaGuardCommand(1, EProofCommandType::Move, Alternative, FVector(-450, 0, 0))).bAccepted);
    S.Model.Advance(100.f);
    S.Own(EProofCommandType::AreaGuard); S.Model.Advance(1.f);
    TestEqual(TEXT("Guard shot cannot stack idle automatic damage"), AreaGuardUnit(S.Model.GetSnapshot(), S.Target).HitPoints, 75.f);
    S.Model.Advance(3.f);
    TestEqual(TEXT("Existing DPS kills actual target"), AreaGuardUnit(S.Model.GetSnapshot(), S.Target).HitPoints, 0.f);
    FProofUnitRecord Survivor = AreaGuardUnit(S.Model.GetSnapshot(), S.Guard);
    TestTrue(TEXT("Target death publishes return during the same damage advance"), Survivor.bAreaGuardReturning && Survivor.AreaGuardTargetId == INDEX_NONE);
    TestEqual(TEXT("Damage-phase death cannot grant a second movement budget"), Survivor.Position, FVector(150, 0, 0));
    S.Model.Advance(1.f);
    TestEqual(TEXT("Surviving guard subsequently returns exactly home"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).Position, FVector::ZeroVector);
    TestEqual(TEXT("Alternative is not acquired on return-arrival advance"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).AreaGuardTargetId, static_cast<int32>(INDEX_NONE));
    S.Model.Advance(.5f);
    TestEqual(TEXT("Next advance switches to surviving hostile after death and return"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).AreaGuardTargetId, Alternative);
    TestEqual(TEXT("New hostile is pursued from original post"), AreaGuardUnit(S.Model.GetSnapshot(), S.Guard).Position, FVector(-75, 0, 0));
    TestEqual(TEXT("Dead own unit cannot be ordered by hostile owner"), S.Model.Submit(100,
        AreaGuardCommand(S.EnemySequence++, EProofCommandType::AreaGuard, S.Target)).Reason, FString(TEXT("ownership")));

    FAreaGuardScenario Mutual;
    if (!TestTrue(TEXT("Mutual-damage fixture"), Mutual.Init(FVector::ZeroVector, FVector(2000, 2000, 0)))) return false;
    TestTrue(TEXT("Enemy factory built publicly"), Mutual.Model.Submit(100,
        AreaGuardCommand(Mutual.EnemySequence++, EProofCommandType::BuildFactory, INDEX_NONE, FVector(7700, -8000, 0))).bAccepted);
    TestTrue(TEXT("Enemy Combat produced publicly"), Mutual.Model.Submit(100,
        AreaGuardCommand(Mutual.EnemySequence++, EProofCommandType::Produce, AreaGuardKind(Mutual.Model.GetSnapshot(), 100, EProofUnitKind::Factory))).bAccepted);
    const int32 EnemyCombat = AreaGuardKind(Mutual.Model.GetSnapshot(), 100, EProofUnitKind::Combat);
    TestTrue(TEXT("Enemy Combat positioned without private health edits"), Mutual.Model.Submit(100,
        AreaGuardCommand(Mutual.EnemySequence++, EProofCommandType::Move, EnemyCombat, FVector(450, 0, 0))).bAccepted);
    Mutual.Model.Advance(200.f);
    Mutual.Own(EProofCommandType::AreaGuard);
    Mutual.Model.Submit(100, AreaGuardCommand(Mutual.EnemySequence++, EProofCommandType::AreaGuard, EnemyCombat));
    Mutual.Model.Advance(1.f);
    TestEqual(TEXT("First guarding Combat receives one shot"), AreaGuardUnit(Mutual.Model.GetSnapshot(), Mutual.Guard).HitPoints, 75.f);
    TestEqual(TEXT("Second guarding Combat receives one shot"), AreaGuardUnit(Mutual.Model.GetSnapshot(), EnemyCombat).HitPoints, 75.f);
    Mutual.Model.Advance(3.f);
    TestEqual(TEXT("First lethal guard fires before simultaneous damage applies"), AreaGuardUnit(Mutual.Model.GetSnapshot(), Mutual.Guard).HitPoints, 0.f);
    TestEqual(TEXT("Second lethal guard also fires"), AreaGuardUnit(Mutual.Model.GetSnapshot(), EnemyCombat).HitPoints, 0.f);
    TestTrue(TEXT("Death clears complete guard state for both actors"), AreaGuardCleared(AreaGuardUnit(Mutual.Model.GetSnapshot(), Mutual.Guard))
        && AreaGuardCleared(AreaGuardUnit(Mutual.Model.GetSnapshot(), EnemyCombat)));
    TestEqual(TEXT("Dead Combat rejects guard rather than reviving intent"), Mutual.Own(EProofCommandType::AreaGuard).Reason, FString(TEXT("ownership")));

    FAreaGuardScenario Economy;
    if (!TestTrue(TEXT("Economy fixture"), Economy.Init(FVector::ZeroVector, FVector(3000, 3000, 0)))) return false;
    TestTrue(TEXT("Real refinery and auto-gather miner grant"), Economy.Model.Submit(1,
        AreaGuardCommand(Economy.OwnSequence++, EProofCommandType::BuildRefinery, INDEX_NONE, FVector(-8000, -7650, 0))).bAccepted);
    const int32 Credits = AreaGuardPlayer(Economy.Model.GetSnapshot(), 1).Credits;
    const int32 Conserved = AreaGuardConservedOre(Economy.Model.GetSnapshot());
    TestTrue(TEXT("Guard costs no economy resources"), Economy.Own(EProofCommandType::AreaGuard).bAccepted);
    TestEqual(TEXT("Guard does not spend credits"), AreaGuardPlayer(Economy.Model.GetSnapshot(), 1).Credits, Credits);
    for (int32 Tick = 0; Tick < 30; ++Tick)
    {
        Economy.Model.Advance(1.f);
        const FProofSnapshot Snapshot = Economy.Model.GetSnapshot();
        TestEqual(TEXT("Ore plus cargo plus deposits conserved while guard executes"), AreaGuardConservedOre(Snapshot), Conserved);
        TestEqual(TEXT("Guard creates no income or hidden expense"), AreaGuardPlayer(Snapshot, 1).Credits,
            Credits + AreaGuardPlayer(Snapshot, 1).TotalGathered);
    }
    TestTrue(TEXT("Conservation case exercised actual deposits"), AreaGuardPlayer(Economy.Model.GetSnapshot(), 1).TotalGathered > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAreaGuardDigestTest, "RedalertProof.Skirmish.AreaGuard.DigestAndLateObserver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAreaGuardDigestTest::RunTest(const FString&)
{
    TestEqual(TEXT("Default snapshot retains fixed legacy MD5"), FProofSnapshot().ComputeDigest(), FString(TEXT("aa1d59044f1a11b0547336759a1cefb2")));
    FProofSnapshot Default;
    FProofUnitRecord Unit;
    Unit.UnitId = 7; Unit.OwnerId = 1; Unit.Position = FVector(1, 2, 3);
    Default.Units.Add(Unit);
    // Precomputed from the pre-AreaGuard canonical serialization, independently
    // of the production ComputeDigest implementation and the portable bridge.
    TestEqual(TEXT("All-default guard fields preserve existing unit digest"), Default.ComputeDigest(), FString(TEXT("e0b478196f377fe16e60ca422ac64a1e")));
    for (int32 Field = 0; Field < 6; ++Field)
    {
        FProofSnapshot Changed = Default;
        if (Field == 0) Changed.Units[0].bAreaGuard = true;
        if (Field == 1) Changed.Units[0].AreaGuardPost.X = 1.e-12;
        if (Field == 2) Changed.Units[0].AreaGuardPost.Y = -2;
        if (Field == 3) Changed.Units[0].AreaGuardPost.Z = 3;
        if (Field == 4) Changed.Units[0].AreaGuardTargetId = 99;
        if (Field == 5) Changed.Units[0].bAreaGuardReturning = true;
        TestTrue(TEXT("Each field independently changes real digest, even with guard disabled"), Changed.ComputeDigest() != Default.ComputeDigest());
        TestTrue(TEXT("Snapshot fault mutation never changes its original"), AreaGuardCleared(Default.Units[0]));
    }
    FAreaGuardScenario S;
    if (!TestTrue(TEXT("Late-observer public fixture"), S.Init(FVector(-200, 50, 0), FVector(250, 50, 0)))) return false;
    S.Own(EProofCommandType::AreaGuard); S.Model.Advance(.5f);
    const FProofSnapshot Chasing = S.Model.GetSnapshot();
    const FProofSnapshot LateChasing = Chasing;
    TestEqual(TEXT("Copied late observer preserves complete chasing digest"), LateChasing.ComputeDigest(), Chasing.ComputeDigest());
    const FProofUnitRecord CopiedGuard = AreaGuardUnit(LateChasing, S.Guard);
    TestTrue(TEXT("Snapshot alone exposes saved post and acquired target"), CopiedGuard.bAreaGuard
        && CopiedGuard.AreaGuardPost == FVector(-200, 50, 0) && CopiedGuard.AreaGuardTargetId == S.Target && !CopiedGuard.bAreaGuardReturning);
    S.MoveEnemy(FVector(2000, 50, 0)); S.Model.Advance(4.f);
    const FProofSnapshot Returning = S.Model.GetSnapshot();
    TestTrue(TEXT("Actual target departure publishes returning state"), AreaGuardUnit(Returning, S.Guard).bAreaGuardReturning);
    for (int32 Field = 0; Field < 6; ++Field)
    {
        FProofSnapshot Changed = Returning;
        FProofUnitRecord* Guard = Changed.Units.FindByPredicate([&S](const FProofUnitRecord& Candidate) { return Candidate.UnitId == S.Guard; });
        if (Field == 0) Guard->bAreaGuard = false;
        if (Field == 1) Guard->AreaGuardPost.X += .001;
        if (Field == 2) Guard->AreaGuardPost.Y -= .001;
        if (Field == 3) Guard->AreaGuardPost.Z = 1;
        if (Field == 4) Guard->AreaGuardTargetId = S.Target;
        if (Field == 5) Guard->bAreaGuardReturning = false;
        TestTrue(TEXT("No partially omitted live guard field can match authoritative digest"), Changed.ComputeDigest() != Returning.ComputeDigest());
    }
    FProofSnapshot Permuted = Returning;
    Permuted.Players.Swap(0, 1); Permuted.Units.Swap(0, Permuted.Units.Num() - 1); Permuted.Resources.Swap(0, 1);
    TestEqual(TEXT("Guard commitment remains independent of snapshot array order"), Permuted.ComputeDigest(), Returning.ComputeDigest());
    S.Model.Advance(1.f);
    TestEqual(TEXT("Late snapshot stays immutable as authoritative model advances"), Returning.ComputeDigest(), Permuted.ComputeDigest());
    TestTrue(TEXT("Public Stop canonicalizes guard for late observers"), S.Own(EProofCommandType::Stop).bAccepted);
    TestTrue(TEXT("Copied post-Stop snapshot requires no local order history"), AreaGuardCleared(AreaGuardUnit(S.Model.GetSnapshot(), S.Guard)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAreaGuardPlannerTest, "RedalertProof.Skirmish.AreaGuard.PlannerAndGrace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAreaGuardPlannerTest::RunTest(const FString&)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    if (!TestTrue(TEXT("Planner fixture seeded through public model"), Model.SeedSkirmishPlayer(100, 1, FVector(-5000, 0, 0))
        && Model.SeedSkirmishPlayer(1, 0, FVector(5000, 0, 0)))) return false;
    int32 Sequence = 1;
    TestTrue(TEXT("Planner fixture refinery built"), Model.Submit(100, AreaGuardCommand(Sequence++, EProofCommandType::BuildRefinery, INDEX_NONE, FVector(-4650, 0, 0))).bAccepted);
    TestTrue(TEXT("Planner fixture factory built"), Model.Submit(100, AreaGuardCommand(Sequence++, EProofCommandType::BuildFactory, INDEX_NONE, FVector(-5350, 0, 0))).bAccepted);
    for (int32 Tick = 0; Tick < 80 && AreaGuardPlayer(Model.GetSnapshot(), 100).Credits < 400; ++Tick) Model.Advance(1.f);
    TestTrue(TEXT("Planner fixture uses earned credits rather than state mutation"), AreaGuardPlayer(Model.GetSnapshot(), 100).Credits >= 400);
    const int32 Factory = AreaGuardKind(Model.GetSnapshot(), 100, EProofUnitKind::Factory);
    for (int32 Index = 0; Index < 7; ++Index)
        if (!TestTrue(TEXT("Populate seven actual combat records at cap"), Model.Submit(100, AreaGuardCommand(Sequence++, EProofCommandType::Produce, Factory)).bAccepted)) return false;
    const FProofSnapshot Snapshot = Model.GetSnapshot();
    FProofSnapshot Permuted = Snapshot;
    Permuted.Units.Swap(0, Permuted.Units.Num() - 1); Permuted.Players.Swap(0, 1);
    FProofSkirmishBot Bot, Replay;
    TSet<int32> Guarded;
    TArray<FProofCommand> FirstBatch;
    int32 LastSequence = 0;
    int32 LastGuardId = 0;
    for (int32 Decision = 0; Decision < 3; ++Decision)
    {
        const TArray<FProofCommand> Batch = Bot.Decide(Snapshot, 100, static_cast<double>(Decision));
        TestTrue(TEXT("Stable command sequence ignores snapshot record order"), AreaGuardSameCommands(Batch, Replay.Decide(Permuted, 100, static_cast<double>(Decision))));
        TestTrue(TEXT("Guard and economy share unchanged four-command decision cap"), Batch.Num() <= 4);
        if (Decision == 0)
        {
            FirstBatch = Batch;
            TestEqual(TEXT("First capped batch fills available four commands"), Batch.Num(), 4);
            if (!Batch.IsEmpty()) TestEqual(TEXT("Existing economy retains priority over guard"), Batch[0].Type, EProofCommandType::Gather);
        }
        for (const FProofCommand& Command : Batch)
        {
            TestEqual(TEXT("Planner sequences stay contiguous"), Command.Sequence, ++LastSequence);
            TestFalse(TEXT("No offensive Attack before grace"), Command.Type == EProofCommandType::Attack);
            if (Command.Type != EProofCommandType::AreaGuard) continue;
            TestFalse(TEXT("Planner never repeatedly reanchors an already issued Combat"), Guarded.Contains(Command.UnitId));
            Guarded.Add(Command.UnitId);
            TestTrue(TEXT("Guard allocation follows ascending stable unit IDs"), Command.UnitId > LastGuardId);
            LastGuardId = Command.UnitId;
            TestTrue(TEXT("Planner guard envelope is canonical and owned Combat"), Command.TargetUnitId == INDEX_NONE
                && Command.Destination == FVector::ZeroVector && AreaGuardUnit(Snapshot, Command.UnitId).OwnerId == 100
                && AreaGuardUnit(Snapshot, Command.UnitId).Kind == EProofUnitKind::Combat);
        }
    }
    TestEqual(TEXT("Stable-ID skipping prevents guard starvation behind first budget"), Guarded.Num(), 7);
    TestTrue(TEXT("Decision cadence still rejects duplicate time"), Bot.Decide(Snapshot, 100, 2.5).IsEmpty());
    const TArray<FProofCommand> BeforeGrace = Bot.Decide(Snapshot, 100, 19.0);
    for (const FProofCommand& Command : BeforeGrace)
        TestTrue(TEXT("Before exact grace only prior economy retry remains"), Command.Type != EProofCommandType::AreaGuard && Command.Type != EProofCommandType::Attack);
    const TArray<FProofCommand> AtGrace = Bot.Decide(Snapshot, 100, 20.0);
    int32 AttackCount = 0;
    for (const FProofCommand& Command : AtGrace)
    {
        TestFalse(TEXT("Grace boundary never emits new guard"), Command.Type == EProofCommandType::AreaGuard);
        if (Command.Type == EProofCommandType::Attack) ++AttackCount;
    }
    TestTrue(TEXT("Existing attack scheduler replaces guard at exactly twenty seconds"), AttackCount > 0 && AtGrace.Num() <= 4);
    FProofSkirmishBot Delayed;
    TestTrue(TEXT("Invalid time cannot start grace clock"), Delayed.Decide(Snapshot, 100, std::numeric_limits<double>::quiet_NaN()).IsEmpty());
    const TArray<FProofCommand> DelayedFirst = Delayed.Decide(Snapshot, 100, 250.0);
    for (const FProofCommand& Command : DelayedFirst)
        TestFalse(TEXT("Delayed first decision starts its own grace interval"), Command.Type == EProofCommandType::Attack);
    const TArray<FProofCommand> DelayedBefore = Delayed.Decide(Snapshot, 100, 269.0);
    for (const FProofCommand& Command : DelayedBefore)
        TestFalse(TEXT("Nineteen elapsed seconds remains defensive regardless of absolute clock"), Command.Type == EProofCommandType::Attack);
    const TArray<FProofCommand> DelayedBoundary = Delayed.Decide(Snapshot, 100, 270.0);
    TestTrue(TEXT("Delayed bot attacks exactly twenty seconds after first valid decision"), DelayedBoundary.ContainsByPredicate(
        [](const FProofCommand& Command) { return Command.Type == EProofCommandType::Attack; }));
    Bot.Reset();
    TestTrue(TEXT("Planner reset clears once-issued guard bookkeeping"), AreaGuardSameCommands(FirstBatch, Bot.Decide(Snapshot, 100, 0.0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofAreaGuardPublicPathTest, "RedalertProof.Skirmish.AreaGuard.PublicModelPath", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofAreaGuardPublicPathTest::RunTest(const FString&)
{
    FProofCommandModel Model, ReplayModel;
    for (FProofCommandModel* Current : {&Model, &ReplayModel})
    {
        Current->StartSkirmish();
        if (!TestTrue(TEXT("Real bot-model seats"), Current->SeedSkirmishPlayer(100, 1, FVector(-7000, 0, 0))
            && Current->SeedSkirmishPlayer(1, 0, FVector(7000, 0, 0)))) return false;
    }
    FProofSkirmishBot Bot, ReplayBot;
    TSet<int32> Guarded;
    TMap<int32, FVector> Posts;
    int32 GuardCount = 0, AttackCount = 0, Spend = 0;
    int32 FirstAttackAt = INDEX_NONE;
    for (int32 Tick = 0; Tick <= 25; ++Tick)
    {
        const FProofSnapshot Before = Model.GetSnapshot();
        const TArray<FProofCommand> Commands = Bot.Decide(Before, 100, static_cast<double>(Tick));
        const TArray<FProofCommand> ReplayCommands = ReplayBot.Decide(ReplayModel.GetSnapshot(), 100, static_cast<double>(Tick));
        TestTrue(TEXT("Real bot command history is deterministic"), AreaGuardSameCommands(Commands, ReplayCommands));
        TestTrue(TEXT("Public decision remains within four-command budget"), Commands.Num() <= 4);
        for (const FProofCommand& Command : Commands)
        {
            const FProofSnapshot BeforeCommand = Model.GetSnapshot();
            const FProofCommandResult Accepted = Model.Submit(100, Command);
            const FProofCommandResult Replayed = ReplayModel.Submit(100, Command);
            TestTrue(FString::Printf(TEXT("Real bot command %d accepted through public validator (%s)"), Command.Sequence, *Accepted.Reason), Accepted.bAccepted && Replayed.bAccepted);
            if (!Accepted.bAccepted) return false;
            if (Command.Type == EProofCommandType::AreaGuard)
            {
                ++GuardCount;
                TestTrue(TEXT("Bot produces AreaGuard only before grace"), Tick < 20);
                TestFalse(TEXT("Public model path never repeats guard for existing Combat"), Guarded.Contains(Command.UnitId));
                Guarded.Add(Command.UnitId);
                const FProofUnitRecord Guard = AreaGuardUnit(Model.GetSnapshot(), Command.UnitId);
                TestTrue(TEXT("Bot guard really becomes authoritative state"), Guard.bAreaGuard && !Guard.bAreaGuardReturning && Guard.AreaGuardTargetId == INDEX_NONE);
                TestEqual(TEXT("Authoritative post comes from actual unit position"), Guard.AreaGuardPost, AreaGuardUnit(BeforeCommand, Command.UnitId).Position);
                Posts.Add(Command.UnitId, Guard.AreaGuardPost);
            }
            if (Command.Type == EProofCommandType::Attack)
            {
                if (FirstAttackAt == INDEX_NONE) FirstAttackAt = Tick;
                ++AttackCount;
                TestTrue(TEXT("No offensive Attack before existing twenty-second grace"), Tick >= 20);
                TestTrue(TEXT("Accepted bot Attack clears preceding guard metadata"), AreaGuardCleared(AreaGuardUnit(Model.GetSnapshot(), Command.UnitId)));
            }
            if (Command.Type == EProofCommandType::Produce) Spend += FProofCommandModel::ProductionCost;
            if (Command.Type == EProofCommandType::BuildRefinery) Spend += FProofCommandModel::RefineryCost;
            if (Command.Type == EProofCommandType::BuildFactory) Spend += FProofCommandModel::FactoryCost;
        }
        Model.Advance(1.f); ReplayModel.Advance(1.f);
        const FProofSnapshot After = Model.GetSnapshot();
        TestEqual(TEXT("Real public bot/model replay matches every revision digest"), After.ComputeDigest(), ReplayModel.GetSnapshot().ComputeDigest());
        TestEqual(TEXT("Bot guard cannot change economic conservation"), AreaGuardConservedOre(After), 1000);
        TestEqual(TEXT("All bot expense remains existing builds and production"), AreaGuardPlayer(After, 100).Credits,
            350 + AreaGuardPlayer(After, 100).TotalGathered - Spend);
        for (const FProofUnitRecord& Unit : After.Units)
        {
            if (!Unit.bAreaGuard) continue;
            TestTrue(TEXT("All bot guard positions stay inside saved leash"), AreaGuardWithinLeash(Unit));
            const FVector* Post = Posts.Find(Unit.UnitId);
            TestTrue(TEXT("Bot does not reanchor between decisions"), Post && Unit.AreaGuardPost == *Post);
        }
    }
    TestTrue(TEXT("Accepted bot path actually issued defensive guards"), GuardCount > 0);
    TestTrue(TEXT("Accepted bot path transitioned into offensive attacks"), AttackCount > 0);
    TestEqual(TEXT("First public-model Attack occurs exactly at existing grace boundary"), FirstAttackAt, 20);
    return true;
}

#if defined(_MSC_VER)
#pragma float_control(pop)
#endif
#endif
