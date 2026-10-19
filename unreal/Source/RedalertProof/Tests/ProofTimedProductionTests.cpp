#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "Commands/ProofCommandModel.h"
#include "Skirmish/ProofSkirmishBot.h"
#include <cmath>
#include <limits>

// Only the int32 representation/ID-exhaustion cases use private access. All
// ordinary jobs, funding, orders, factory losses and lifecycle changes below
// are real public command histories, including the all-exits-blocked case.
struct FProofTimedProductionFixture
{
    static void SetCreditsAtRepresentationBoundary(FProofCommandModel& Model, int32 Owner, int32 Credits)
    {
        Model.State.Players.FindByPredicate([Owner](const FProofPlayerRecord& P) { return P.OwnerId == Owner; })->Credits = Credits;
    }
    static void ExhaustIds(FProofCommandModel& Model) { Model.NextUnitId = MAX_int32; }
};

namespace
{
FProofCommand TimedCommand(int32 Sequence, EProofCommandType Type, int32 Unit = INDEX_NONE,
    int32 Target = INDEX_NONE, const FVector& Destination = FVector::ZeroVector)
{
    FProofCommand C;
    C.Sequence = Sequence; C.Type = Type; C.UnitId = Unit; C.TargetUnitId = Target; C.Destination = Destination;
    return C;
}
FProofUnitRecord TimedUnit(const FProofSnapshot& S, int32 Id)
{
    for (const FProofUnitRecord& U : S.Units) if (U.UnitId == Id) return U;
    return FProofUnitRecord();
}
FProofPlayerRecord TimedPlayer(const FProofSnapshot& S, int32 Owner)
{
    for (const FProofPlayerRecord& P : S.Players) if (P.OwnerId == Owner) return P;
    return FProofPlayerRecord();
}
FProofProductionJob TimedJob(const FProofSnapshot& S, int32 Owner = 1)
{
    for (const FProofProductionJob& J : S.ProductionJobs) if (J.OwnerId == Owner) return J;
    return FProofProductionJob();
}
int32 TimedKind(const FProofSnapshot& S, int32 Owner, EProofUnitKind Kind)
{
    int32 Id = INDEX_NONE;
    for (const FProofUnitRecord& U : S.Units)
        if (U.OwnerId == Owner && U.Kind == Kind && U.HitPoints > 0.f && (Id == INDEX_NONE || U.UnitId < Id)) Id = U.UnitId;
    return Id;
}
int32 TimedCount(const FProofSnapshot& S, int32 Owner, EProofUnitKind Kind)
{
    int32 Count = 0;
    for (const FProofUnitRecord& U : S.Units) if (U.OwnerId == Owner && U.Kind == Kind && U.HitPoints > 0.f) ++Count;
    return Count;
}
int32 TimedLive(const FProofSnapshot& S, int32 Owner)
{
    int32 Count = 0;
    for (const FProofUnitRecord& U : S.Units) if (U.OwnerId == Owner && U.HitPoints > 0.f) ++Count;
    return Count;
}
bool TimedSameJob(const FProofProductionJob& A, const FProofProductionJob& B)
{
    return A.OwnerId == B.OwnerId && A.StartSequence == B.StartSequence && A.FactoryId == B.FactoryId
        && A.WorkMicroseconds == B.WorkMicroseconds && A.PaidCredits == B.PaidCredits && A.bHeld == B.bHeld;
}
bool TimedHasProduce(const TArray<FProofCommand>& Commands)
{
    return Commands.ContainsByPredicate([](const FProofCommand& C) { return C.Type == EProofCommandType::Produce; });
}
FString TimedWithoutRevision(FProofSnapshot S) { S.Revision = 0; return S.ComputeDigest(); }
struct FTimedPublic
{
    FProofCommandModel Model;
    TMap<int32, int32> Sequences;
    explicit FTimedPublic(int32 Rate = 32) : Model(Rate) {}
    bool Init()
    {
        Model.StartSkirmish();
        return Model.SeedSkirmishPlayer(1, 0, FVector(-3000, 0, 0))
            && Model.SeedSkirmishPlayer(2, 1, FVector(3000, 0, 0));
    }
    FProofCommandResult Send(int32 Owner, EProofCommandType Type, int32 Unit = INDEX_NONE,
        int32 Target = INDEX_NONE, const FVector& Destination = FVector::ZeroVector)
    { return Model.Submit(Owner, TimedCommand(++Sequences.FindOrAdd(Owner), Type, Unit, Target, Destination)); }
    bool Build(int32 Owner, EProofCommandType Type, const FVector& Position)
    { return Send(Owner, Type, INDEX_NONE, INDEX_NONE, Position).bAccepted; }
    bool Factory(int32 Owner = 1)
    { return Build(Owner, EProofCommandType::BuildFactory, FVector(Owner == 1 ? -2650 : 3350, 0, 0)); }
    int32 Id(int32 Owner, EProofUnitKind Kind) const { return TimedKind(Model.GetSnapshot(), Owner, Kind); }
    int32 Credits(int32 Owner = 1) const { return TimedPlayer(Model.GetSnapshot(), Owner).Credits; }
    FProofProductionJob Job(int32 Owner = 1) const { return TimedJob(Model.GetSnapshot(), Owner); }
    bool Start(int32 Owner = 1) { return Send(Owner, EProofCommandType::Produce, Id(Owner, EProofUnitKind::Factory)).bAccepted; }
    bool Control(EProofCommandType Type, int32 Owner = 1)
    { const FProofProductionJob J = Job(Owner); return Send(Owner, Type, J.FactoryId, J.StartSequence).bAccepted; }
    bool StopMiners(int32 Owner = 1)
    {
        for (const FProofUnitRecord& U : Model.GetSnapshot().Units)
            if (U.OwnerId == Owner && U.Kind == EProofUnitKind::Harvester && U.HitPoints > 0.f
                && !Send(Owner, EProofCommandType::Stop, U.UnitId).bAccepted) return false;
        return true;
    }
    bool EnemyAttacker()
    {
        if (Id(2, EProofUnitKind::Combat) != INDEX_NONE) return true;
        if (Id(2, EProofUnitKind::Factory) == INDEX_NONE && !Factory(2)) return false;
        if (!Start(2)) return false;
        Model.Advance(5.f);
        return Id(2, EProofUnitKind::Combat) != INDEX_NONE;
    }
    bool Kill(int32 Target)
    {
        if (!EnemyAttacker()) return false;
        const int32 Attacker = Id(2, EProofUnitKind::Combat);
        if (!Send(2, EProofCommandType::Attack, Attacker, Target).bAccepted) return false;
        for (int32 Step = 0; Step < 200 && TimedUnit(Model.GetSnapshot(), Target).HitPoints > 0.f; ++Step) Model.Advance(1.f);
        return TimedUnit(Model.GetSnapshot(), Target).HitPoints == 0.f;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofTimedProductionFundingTest,
    "RedalertProof.Skirmish.TimedProduction.StartAndFunding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofTimedProductionFundingTest::RunTest(const FString&)
{
    FTimedPublic F;
    if (!TestTrue(TEXT("Public funded fixture"), F.Init() && F.Factory())) return false;
    const FProofSnapshot Before = F.Model.GetSnapshot();
    const int32 Factory = F.Id(1, EProofUnitKind::Factory);
    if (!TestTrue(TEXT("Produce accepts one actual job"), F.Start())) return false;
    FProofSnapshot Started = F.Model.GetSnapshot();
    TestEqual(TEXT("Starting does not grant a record"), Started.Units.Num(), Before.Units.Num());
    TestEqual(TEXT("Starting does not debit credits"), F.Credits(), 200);
    TestEqual(TEXT("Starting has just the ordinary command revision"), Started.Revision, Before.Revision + 1);
    TestTrue(TEXT("Exact initial public job fields"), F.Job().OwnerId == 1 && F.Job().StartSequence == 2
        && F.Job().FactoryId == Factory && F.Job().WorkMicroseconds == 0 && F.Job().PaidCredits == 0 && !F.Job().bHeld);
    TestEqual(TEXT("No backlog is accepted"), F.Send(1, EProofCommandType::Produce, Factory).Reason, FString(TEXT("production-busy")));
    TestEqual(TEXT("Busy rejection leaves complete visible state intact"), F.Model.GetSnapshot().ComputeDigest(), Started.ComputeDigest());
    F.Model.Advance(0.1f);
    TestTrue(TEXT("First tenth pays exactly one credit"), F.Job().WorkMicroseconds == 100000 && F.Job().PaidCredits == 1 && F.Credits() == 199);
    F.Model.Advance(0.000001f);
    TestTrue(TEXT("Fractional next credit is charged with ceiling"), F.Job().WorkMicroseconds == 100001 && F.Job().PaidCredits == 2 && F.Credits() == 198);
    F.Model.Advance(0.099999f);
    TestTrue(TEXT("Previously purchased fractional work is retained"), F.Job().WorkMicroseconds == 200000 && F.Job().PaidCredits == 2 && F.Credits() == 198);
    F.Model.Advance(4.8f);
    const FProofSnapshot Completed = F.Model.GetSnapshot();
    TestEqual(TEXT("Exactly one completion removes the job"), Completed.ProductionJobs.Num(), 0);
    TestEqual(TEXT("Completion spends exactly total cost"), F.Credits(), 150);
    TestEqual(TEXT("Exactly one actual Combat exists"), TimedCount(Completed, 1, EProofUnitKind::Combat), 1);
    TestEqual(TEXT("No unit ID was allocated at start"), F.Id(1, EProofUnitKind::Combat), Before.Units[Before.Units.Num() - 1].UnitId + 1);
    F.Model.Advance(100.f);
    TestEqual(TEXT("No double spawn after job removal"), F.Model.GetSnapshot().Units.Num(), Completed.Units.Num());
    TestEqual(TEXT("No double debit after completion"), F.Credits(), 150);

    FTimedPublic Empty;
    if (!TestTrue(TEXT("Zero-cash public setup spends actual construction costs"), Empty.Init() && Empty.Factory()
        && Empty.Build(1, EProofCommandType::BuildRefinery, FVector(-3350, 0, 0))
        && Empty.Build(1, EProofCommandType::BuildRefinery, FVector(-3000, 350, 0)) && Empty.StopMiners())) return false;
    TestEqual(TEXT("Zero-cash balance is reached without a money setter"), Empty.Credits(), 0);
    if (!TestTrue(TEXT("Unfunded start does not require the whole price"), Empty.Start())) return false;
    Empty.Model.Advance(50.f);
    TestTrue(TEXT("Cash shortage has no work or debt and is not manual hold"), Empty.Job().WorkMicroseconds == 0
        && Empty.Job().PaidCredits == 0 && !Empty.Job().bHeld && Empty.Credits() == 0);
    if (!TestTrue(TEXT("Resume real income through public Gather"), Empty.Send(1, EProofCommandType::Gather,
        Empty.Id(1, EProofUnitKind::Harvester), Empty.Model.GetSnapshot().Resources[0].ResourceId).bAccepted)) return false;
    bool bDeposit = false;
    for (int32 Step = 0; Step < 300 && !bDeposit; ++Step)
    {
        const int32 Prior = Empty.Credits();
        Empty.Model.Advance(0.1f);
        if (Empty.Credits() > Prior)
        {
            bDeposit = true;
            TestTrue(TEXT("Same-step deposit cannot fund earlier production stage"), Prior == 0 && Empty.Credits() == 50
                && Empty.Job().WorkMicroseconds == 0 && Empty.Job().PaidCredits == 0);
        }
    }
    if (!TestTrue(TEXT("Actual ore extraction/deposit restores funding"), bDeposit && Empty.StopMiners())) return false;
    Empty.Model.Advance(0.1f);
    TestTrue(TEXT("Automatic funding resumption has no shortage catch-up"), Empty.Job().WorkMicroseconds == 100000
        && Empty.Job().PaidCredits == 1 && Empty.Credits() == 49);
    Empty.Model.Advance(4.9f);
    TestTrue(TEXT("Exactly new simulated work completes the resumed job"), Empty.Job().OwnerId == INDEX_NONE
        && Empty.Credits() == 0 && TimedCount(Empty.Model.GetSnapshot(), 1, EProofUnitKind::Combat) == 1);
    TestEqual(TEXT("Earned-money oracle independently accounts for all actual spending"),
        Empty.Credits(), 350 + TimedPlayer(Empty.Model.GetSnapshot(), 1).TotalGathered - 150 - 200 - 50);

    FProofCommandModel Proof;
    Proof.AddPlayer(1, 100);
    const int32 Seed = Proof.SeedUnit(1, FVector::ZeroVector);
    TestTrue(TEXT("Non-skirmish proof production remains immediate"), Proof.Submit(1, TimedCommand(1, EProofCommandType::Produce, Seed)).bAccepted
        && Proof.GetSnapshot().Units.Num() == 2 && Proof.GetSnapshot().Players[0].Credits == 50 && Proof.GetSnapshot().ProductionJobs.Num() == 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofTimedProductionControlsTest,
    "RedalertProof.Skirmish.TimedProduction.ControlsAndIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofTimedProductionControlsTest::RunTest(const FString&)
{
    TestTrue(TEXT("Existing enum ordinals remain unchanged and controls append"), static_cast<int32>(EProofCommandType::Produce) == 2
        && static_cast<int32>(EProofCommandType::AreaGuard) == 10 && static_cast<int32>(EProofCommandType::HoldProduction) == 11
        && static_cast<int32>(EProofCommandType::ResumeProduction) == 12 && static_cast<int32>(EProofCommandType::CancelProduction) == 13);
    FTimedPublic F;
    if (!TestTrue(TEXT("Control history setup"), F.Init() && F.Factory() && F.Start())) return false;
    const FProofProductionJob StartedJob = F.Job();
    const int32 StartCredits = F.Credits();
    const int32 Miner = F.Id(1, EProofUnitKind::Harvester);
    TestTrue(TEXT("Ordinary mobile Move remains independent of active factory work"),
        F.Send(1, EProofCommandType::Move, Miner, INDEX_NONE, FVector(-2500, 400, 0)).bAccepted);
    TestTrue(TEXT("Ordinary mobile Stop preserves active production intent and cash"),
        F.Send(1, EProofCommandType::Stop, Miner).bAccepted && TimedSameJob(F.Job(), StartedJob) && F.Credits() == StartCredits);
    const FString BeforeFactoryStop = F.Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Stop on producing factory remains a role rejection, not production hold/cancel"),
        F.Send(1, EProofCommandType::Stop, StartedJob.FactoryId).Reason, FString(TEXT("role")));
    TestEqual(TEXT("Rejected factory Stop preserves every published field"), F.Model.GetSnapshot().ComputeDigest(), BeforeFactoryStop);
    const FVector StoppedPosition = TimedUnit(F.Model.GetSnapshot(), Miner).Position;
    F.Model.Advance(0.100001f);
    TestTrue(TEXT("Stopped miner stays still while separate production continues"),
        TimedUnit(F.Model.GetSnapshot(), Miner).Position == StoppedPosition && F.Job().WorkMicroseconds == 100001);
    const FProofProductionJob Job = F.Job();
    const int32 Paid = Job.PaidCredits;
    if (!TestTrue(TEXT("Public manual hold"), F.Control(EProofCommandType::HoldProduction))) return false;
    const FProofSnapshot Held = F.Model.GetSnapshot();
    F.Model.Advance(40.f);
    TestTrue(TEXT("Manual hold prevents all work and debit"), TimedSameJob(F.Job(), TimedJob(Held)) && F.Credits() == 200 - Paid);
    TestTrue(TEXT("Repeated hold is idempotent"), F.Control(EProofCommandType::HoldProduction) && TimedSameJob(F.Job(), TimedJob(Held)));
    const FProofSnapshot BeforeBad = F.Model.GetSnapshot();
    TestEqual(TEXT("Wrong job start sequence is rejected"), F.Send(1, EProofCommandType::ResumeProduction,
        Job.FactoryId, Job.StartSequence + 1).Reason, FString(TEXT("production")));
    TestEqual(TEXT("Wrong factory is rejected even with valid job sequence"), F.Send(1, EProofCommandType::CancelProduction,
        F.Id(1, EProofUnitKind::HQ), Job.StartSequence).Reason, FString(TEXT("production")));
    TestEqual(TEXT("Cross-owner control cannot address another owner's job"), F.Send(2, EProofCommandType::CancelProduction,
        Job.FactoryId, Job.StartSequence).Reason, FString(TEXT("production")));
    TestEqual(TEXT("False-control negative histories leave full visible state unchanged"), F.Model.GetSnapshot().ComputeDigest(), BeforeBad.ComputeDigest());
    const int32 Next = F.Sequences.FindOrAdd(1) + 1;
    for (int32 Case = 0; Case < 5; ++Case)
    {
        FProofCommand Bad = TimedCommand(Next, EProofCommandType::HoldProduction, Job.FactoryId, Job.StartSequence);
        if (Case == 0) Bad.UnitId = INDEX_NONE;
        if (Case == 1) Bad.TargetUnitId = INDEX_NONE;
        if (Case == 2) Bad.Destination.X = 1.0;
        if (Case == 3) Bad.Destination.Y = std::numeric_limits<double>::quiet_NaN();
        if (Case == 4) Bad.Type = static_cast<EProofCommandType>(14);
        TestEqual(TEXT("Malformed control rejects before sequence consumption"), F.Model.Submit(1, Bad).Reason, FString(TEXT("malformed")));
        TestEqual(TEXT("Malformed control cannot mutate visible job/money"), F.Model.GetSnapshot().ComputeDigest(), BeforeBad.ComputeDigest());
    }
    TestEqual(TEXT("Stale replay rejected by the actual sequence validator"), F.Model.Submit(1,
        TimedCommand(1, EProofCommandType::CancelProduction, Job.FactoryId, Job.StartSequence)).Reason, FString(TEXT("sequence")));
    TestTrue(TEXT("Resume is accepted after malformed/replay controls"), F.Control(EProofCommandType::ResumeProduction));
    TestTrue(TEXT("Repeated resume is idempotent"), F.Control(EProofCommandType::ResumeProduction) && !F.Job().bHeld);
    F.Model.Advance(0.099999f);
    TestTrue(TEXT("Resume consumes previously paid remainder without charging again"), F.Job().WorkMicroseconds == 200000 && F.Job().PaidCredits == 2 && F.Credits() == 198);
    TestTrue(TEXT("Cancel refunds precisely the actual ledger"), F.Control(EProofCommandType::CancelProduction)
        && F.Job().OwnerId == INDEX_NONE && F.Credits() == 200);
    const FString Cancelled = F.Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Repeated cancel is rejected without double refund"), F.Send(1, EProofCommandType::CancelProduction,
        Job.FactoryId, Job.StartSequence).Reason, FString(TEXT("production")));
    TestEqual(TEXT("Repeated cancel complete-state negative control"), F.Model.GetSnapshot().ComputeDigest(), Cancelled);
    TestTrue(TEXT("A new job receives a new public identity"), F.Start() && F.Job().StartSequence != Job.StartSequence);
    const FString NewJob = F.Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Old identity cannot cancel replacement job"), F.Send(1, EProofCommandType::CancelProduction,
        Job.FactoryId, Job.StartSequence).Reason, FString(TEXT("production")));
    TestEqual(TEXT("Replacement remains unchanged"), F.Model.GetSnapshot().ComputeDigest(), NewJob);

    FTimedPublic Rate(1);
    if (!TestTrue(TEXT("Rate fixture"), Rate.Init() && Rate.Factory())) return false;
    Rate.Model.Advance(0.f);
    if (!TestTrue(TEXT("Rate fixture starts job"), Rate.Start())) return false;
    const FString RateBefore = Rate.Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("Controls use ordinary rate budget"), Rate.Send(1, EProofCommandType::HoldProduction,
        Rate.Job().FactoryId, Rate.Job().StartSequence).Reason, FString(TEXT("rate")));
    TestEqual(TEXT("Rate failure preserves state"), Rate.Model.GetSnapshot().ComputeDigest(), RateBefore);
    Rate.Model.Advance(0.f);
    TestTrue(TEXT("Rate-rejected envelope consumed sequence and next command succeeds"), Rate.Control(EProofCommandType::HoldProduction));

    FTimedPublic Epoch;
    if (!TestTrue(TEXT("Direct model envelope compatibility fixture"), Epoch.Init() && Epoch.Factory())) return false;
    FProofCommand Compatible = TimedCommand(2, EProofCommandType::Produce, Epoch.Id(1, EProofUnitKind::Factory), 987, FVector(1, 2, 3));
    Compatible.MatchEpoch = 12345;
    TestTrue(TEXT("Model keeps ignored Produce fields and does not pretend to validate adapter epoch"), Epoch.Model.Submit(1, Compatible).bAccepted);
    // Epoch mismatch/rejoin transport rejection belongs to the real GameMode
    // adapter suite. This pure-model test deliberately does not fake that gate.
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofTimedProductionQuantizationTest,
    "RedalertProof.Skirmish.TimedProduction.QuantizedTimeAndDigest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofTimedProductionQuantizationTest::RunTest(const FString&)
{
    TestTrue(TEXT("Named prototype constants have exact calibrated values"), FProofCommandModel::ProductionCost == 50
        && FProofCommandModel::ProductionWorkPerCredit == 100000 && FProofCommandModel::ProductionWorkMicroseconds == 5000000
        && FProofCommandModel::ProductionDurationSeconds == 5.f);
    const FProofProductionJob Default;
    TestTrue(TEXT("Every new job field has a canonical default"), Default.OwnerId == INDEX_NONE && Default.StartSequence == 0
        && Default.FactoryId == INDEX_NONE && Default.WorkMicroseconds == 0 && Default.PaidCredits == 0 && !Default.bHeld);
    FTimedPublic A, B;
    if (!TestTrue(TEXT("Equal public prefixes"), A.Init() && B.Init() && A.Factory() && B.Factory() && A.Start() && B.Start())) return false;
    TestEqual(TEXT("Identical accepted prefixes have complete-state equality"), A.Model.GetSnapshot().ComputeDigest(), B.Model.GetSnapshot().ComputeDigest());
    const FString Initial = A.Model.GetSnapshot().ComputeDigest();
    for (float Delta : {0.f, -1.f, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        A.Model.Advance(Delta);
        TestEqual(TEXT("Zero/invalid Advance performs no visible production work"), A.Model.GetSnapshot().ComputeDigest(), Initial);
    }
    const float BelowHalf = std::nextafter(0.0000005f, 0.f);
    const float AboveHalf = std::nextafter(0.0000005f, std::numeric_limits<float>::infinity());
    A.Model.Advance(BelowHalf); A.Model.Advance(BelowHalf);
    TestTrue(TEXT("Sub-half-microsecond calls do not carry fractional work"), A.Job().WorkMicroseconds == 0 && A.Job().PaidCredits == 0 && A.Credits() == 200);
    B.Model.Advance(BelowHalf + BelowHalf);
    TestTrue(TEXT("Unequal rounded partitions are explicitly not equivalent"), B.Job().WorkMicroseconds == 1 && B.Job().PaidCredits == 1
        && TimedWithoutRevision(A.Model.GetSnapshot()) != TimedWithoutRevision(B.Model.GetSnapshot()));
    A.Model.Advance(AboveHalf);
    TestTrue(TEXT("Next representable duration above half-microsecond rounds up"), A.Job().WorkMicroseconds == 1 && A.Job().PaidCredits == 1);
    TestEqual(TEXT("Equal summed rounded work has equal full state apart from ordinary tick revision"), TimedWithoutRevision(A.Model.GetSnapshot()), TimedWithoutRevision(B.Model.GetSnapshot()));
    A.Model.Advance(0.25f); B.Model.Advance(0.125f); B.Model.Advance(0.125f);
    TestTrue(TEXT("Exact binary partition agrees on work and fractional-credit ceiling"), A.Job().WorkMicroseconds == 250001 && A.Job().PaidCredits == 3);
    TestEqual(TEXT("Equal rounded partition preserves every published field"), TimedWithoutRevision(A.Model.GetSnapshot()), TimedWithoutRevision(B.Model.GetSnapshot()));
    const FProofSnapshot Original = A.Model.GetSnapshot();
    for (int32 Field = 0; Field < 6; ++Field)
    {
        FProofSnapshot Changed = Original;
        FProofProductionJob& J = Changed.ProductionJobs[0];
        if (Field == 0) ++J.OwnerId;
        if (Field == 1) ++J.StartSequence;
        if (Field == 2) ++J.FactoryId;
        if (Field == 3) ++J.WorkMicroseconds;
        if (Field == 4) ++J.PaidCredits;
        if (Field == 5) J.bHeld = !J.bHeld;
        TestTrue(TEXT("Digest independently commits each complete job field"), Changed.ComputeDigest() != Original.ComputeDigest());
    }
    FProofSnapshot Removed = Original; Removed.ProductionJobs.Reset();
    TestTrue(TEXT("Digest commits job existence"), Removed.ComputeDigest() != Original.ComputeDigest());
    FProofSnapshot Plain;
    TestEqual(TEXT("Empty jobs retain legacy default digest"), Plain.ComputeDigest(),
        FMD5::HashAnsiString(TEXT("revision:0;epoch:0;skirmish:0;phase:0;winner:-1;")));
    FTimedPublic Boundary;
    // Two actual factory purchases leave exactly the one job's 50-credit cost.
    // Its final already-paid microsecond must work even when available cash is zero.
    if (!TestTrue(TEXT("Exact completion boundary uses public fifty-credit job"), Boundary.Init() && Boundary.Factory()
        && Boundary.Build(1, EProofCommandType::BuildFactory, FVector(-3350,0,0)) && Boundary.Start())) return false;
    Boundary.Model.Advance(4.999999f);
    TestTrue(TEXT("One microsecond short is fully paid but cannot release early"), Boundary.Job().WorkMicroseconds == 4999999
        && Boundary.Job().PaidCredits == 50 && Boundary.Credits() == 0 && TimedCount(Boundary.Model.GetSnapshot(),1,EProofUnitKind::Combat) == 0);
    Boundary.Model.Advance(0.000001f);
    TestTrue(TEXT("Exact final microsecond completes without further debit"), Boundary.Job().OwnerId == INDEX_NONE
        && Boundary.Credits() == 0 && TimedCount(Boundary.Model.GetSnapshot(),1,EProofUnitKind::Combat) == 1);
    A.Model.Advance(std::numeric_limits<float>::max());
    TestTrue(TEXT("Largest finite float clamps before integer conversion and completes only once"), A.Job().OwnerId == INDEX_NONE
        && A.Credits() == 150 && TimedCount(A.Model.GetSnapshot(), 1, EProofUnitKind::Combat) == 1);
    A.Model.Advance(std::numeric_limits<float>::max());
    TestTrue(TEXT("Huge Advance cannot create a backlog or repeat payment"), A.Credits() == 150 && TimedCount(A.Model.GetSnapshot(), 1, EProofUnitKind::Combat) == 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofTimedProductionBlockedTest,
    "RedalertProof.Skirmish.TimedProduction.BlockedReleaseAndRally",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofTimedProductionBlockedTest::RunTest(const FString&)
{
    FTimedPublic F;
    if (!TestTrue(TEXT("Blocked-exit public setup"), F.Init() && F.Factory())) return false;
    const int32 Factory = F.Id(1, EProofUnitKind::Factory);
    const FVector Anchor = TimedUnit(F.Model.GetSnapshot(), Factory).Position;
    const FVector Directions[] = {FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0),
        FVector(1,1,0), FVector(-1,1,0), FVector(1,-1,0), FVector(-1,-1,0)};
    int32 Blocker = 100;
    for (int32 Ring = 1; Ring <= 3; ++Ring)
        for (const FVector& Direction : Directions)
            if (!TestTrue(TEXT("Public seeded allied records block every deterministic exit"),
                F.Model.SeedSkirmishPlayer(Blocker++, 0, Anchor + Direction * (300.0 * Ring)))) return false;
    FVector Spawn(1,2,3);
    TestFalse(TEXT("Shared spawn helper confirms actual geometric exhaustion"), FProofCommandModel::FindSkirmishSpawn(F.Model.GetSnapshot(), Anchor, Spawn));
    TestTrue(TEXT("Blocked exit does not reject a valid start"), F.Start());
    const int32 Records = F.Model.GetSnapshot().Units.Num();
    F.Model.Advance(5.f);
    TestTrue(TEXT("Fully paid blocked job is retained without a unit"), F.Job().WorkMicroseconds == 5000000 && F.Job().PaidCredits == 50
        && F.Credits() == 150 && F.Model.GetSnapshot().Units.Num() == Records);
    F.Model.Advance(100.f);
    TestTrue(TEXT("Repeated blocked completion has no additional charge"), F.Credits() == 150 && F.Model.GetSnapshot().Units.Num() == Records);
    FProofSkirmishBot BlockedBot;
    TestFalse(TEXT("Planner does not repeat Produce for an actual ReadyBlocked job"), TimedHasProduce(BlockedBot.Decide(F.Model.GetSnapshot(), 1, 0.0)));
    if (!TestTrue(TEXT("Hold ready job before public unblock"), F.Control(EProofCommandType::HoldProduction) && F.Model.RemoveSkirmishPlayer(100))) return false;
    TestTrue(TEXT("Removing just first blocker exposes exact preferred exit"), FProofCommandModel::FindSkirmishSpawn(F.Model.GetSnapshot(), Anchor, Spawn)
        && Spawn == Anchor + FVector(300,0,0));
    F.Model.Advance(1.f);
    TestTrue(TEXT("Hold suppresses release as well as work"), F.Job().bHeld && F.Credits() == 150 && TimedCount(F.Model.GetSnapshot(), 1, EProofUnitKind::Combat) == 0);
    const FVector Rally = Spawn + FVector(0,600,0);
    if (!TestTrue(TEXT("Change factory rally after work completion"), F.Send(1, EProofCommandType::SetRally, Factory, INDEX_NONE, Rally).bAccepted
        && F.Control(EProofCommandType::ResumeProduction))) return false;
    F.Model.Advance(0.f);
    TestTrue(TEXT("Zero Advance cannot release a ready job"), F.Job().OwnerId == 1);
    F.Model.Advance(0.1f);
    const int32 Combat = F.Id(1, EProofUnitKind::Combat);
    TestTrue(TEXT("Release uses current factory and exact shared spawn without extra debit"), Combat != INDEX_NONE && F.Job().OwnerId == INDEX_NONE
        && F.Credits() == 150 && TimedUnit(F.Model.GetSnapshot(), Combat).Position == Spawn
        && TimedUnit(F.Model.GetSnapshot(), Combat).LastTravelDirection == FVector::ZeroVector);
    F.Model.Advance(1.f);
    TestTrue(TEXT("Produced unit actually follows current rally on the next Advance"),
        TimedUnit(F.Model.GetSnapshot(), Combat).Position == Spawn + FVector(0,150,0)
        && TimedUnit(F.Model.GetSnapshot(), Combat).LastTravelDirection == FVector(0,1,0));

    FTimedPublic Fire;
    if (!TestTrue(TEXT("Post-production fire ordering public setup"), Fire.Init() && Fire.Factory()
        && Fire.Model.SeedSkirmishPlayer(3,1,FVector(-2350,300,0)) && Fire.Start())) return false;
    const int32 EnemyHQ = Fire.Id(3,EProofUnitKind::HQ);
    Fire.Model.Advance(5.f);
    TestTrue(TEXT("New Combat is spawned in range but cannot fire on its completion tick"),
        TimedUnit(Fire.Model.GetSnapshot(),Fire.Id(1,EProofUnitKind::Combat)).Position == FVector(-2350,0,0)
        && TimedUnit(Fire.Model.GetSnapshot(),EnemyHQ).HitPoints == 300.f);
    Fire.Model.Advance(1.f);
    TestEqual(TEXT("Same Combat really fires on the following Advance"),TimedUnit(Fire.Model.GetSnapshot(),EnemyHQ).HitPoints,275.f);

    FTimedPublic Cap;
    if (!TestTrue(TEXT("Capacity public economy setup"), Cap.Init() && Cap.Factory()
        && Cap.Build(1, EProofCommandType::BuildRefinery, FVector(-3350,0,0)))) return false;
    for (int32 Tick = 0; Tick < 300 && TimedPlayer(Cap.Model.GetSnapshot(), 1).TotalGathered < 500; ++Tick) Cap.Model.Advance(1.f);
    if (!TestTrue(TEXT("Real miner funds capacity construction"), TimedPlayer(Cap.Model.GetSnapshot(),1).TotalGathered >= 500 && Cap.StopMiners())) return false;
    for (const FVector& Site : {FVector(-3000,350,0), FVector(-3000,-350,0), FVector(-3350,350,0)})
        if (!TestTrue(TEXT("Actual refinery grants fill two records each"), Cap.Build(1,EProofCommandType::BuildRefinery,Site))) return false;
    if (!TestTrue(TEXT("Stop grants and start with exactly one remaining slot"), Cap.StopMiners() && TimedLive(Cap.Model.GetSnapshot(),1) == 11 && Cap.Start())) return false;
    if (!TestTrue(TEXT("Concurrent ordinary construction consumes last slot"), Cap.Build(1,EProofCommandType::BuildFactory,FVector(-3350,-350,0)))) return false;
    Cap.Model.Advance(5.f);
    TestTrue(TEXT("Capacity becoming full retains paid ready job"), TimedLive(Cap.Model.GetSnapshot(),1) == 12 && Cap.Job().PaidCredits == 50 && Cap.Job().WorkMicroseconds == 5000000);
    const int32 BeforeCredits = Cap.Credits();
    const int32 Victim = Cap.Id(1,EProofUnitKind::Harvester);
    if (!TestTrue(TEXT("Actual hostile loss opens the occupied record slot"), Cap.Kill(Victim))) return false;
    TestTrue(TEXT("Paid ready job releases exactly once in the post-combat stage"), Cap.Job().OwnerId == INDEX_NONE
        && TimedCount(Cap.Model.GetSnapshot(),1,EProofUnitKind::Combat) == 1 && TimedLive(Cap.Model.GetSnapshot(),1) == 12 && Cap.Credits() == BeforeCredits);
    const FString AtCap = Cap.Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("New start cannot bypass live-record cap"), Cap.Send(1,EProofCommandType::Produce,Cap.Id(1,EProofUnitKind::Factory)).Reason,FString(TEXT("capacity")));
    TestEqual(TEXT("Cap rejection preserves complete state"),Cap.Model.GetSnapshot().ComputeDigest(),AtCap);

    FTimedPublic Ids;
    if (!TestTrue(TEXT("ID-exhaustion boundary uses a real started job"), Ids.Init() && Ids.Factory() && Ids.Start())) return false;
    FProofTimedProductionFixture::ExhaustIds(Ids.Model);
    Ids.Model.Advance(5.f); Ids.Model.Advance(1.f);
    TestTrue(TEXT("ID exhaustion retains exact paid liability without duplicate debit/spawn"), Ids.Job().PaidCredits == 50
        && Ids.Job().WorkMicroseconds == 5000000 && Ids.Credits() == 150 && TimedCount(Ids.Model.GetSnapshot(),1,EProofUnitKind::Combat) == 0);
    TestTrue(TEXT("ID-blocked job remains cancellable for full actual payment"), Ids.Control(EProofCommandType::CancelProduction) && Ids.Credits() == 200);
    TestEqual(TEXT("Exhausted ID allocator rejects a new job atomically"), Ids.Send(1,EProofCommandType::Produce,Ids.Id(1,EProofUnitKind::Factory)).Reason,FString(TEXT("capacity")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofTimedProductionLossTest,
    "RedalertProof.Skirmish.TimedProduction.LossAndRefundLiability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofTimedProductionLossTest::RunTest(const FString&)
{
    for (bool bOverflow : {false, true})
    {
        FTimedPublic F;
        if (!TestTrue(TEXT("Loss fixture uses real construction and enemy production"), F.Init() && F.Factory() && F.EnemyAttacker() && F.Start())) return false;
        F.Model.Advance(1.200001f);
        const FProofProductionJob Before = F.Job();
        TestTrue(TEXT("Loss starts with exact fractional debit ledger"), Before.WorkMicroseconds == 1200001 && Before.PaidCredits == 13 && F.Credits() == 187);
        if (!TestTrue(TEXT("Manual hold before actual factory death"), F.Control(EProofCommandType::HoldProduction))) return false;
        if (bOverflow) FProofTimedProductionFixture::SetCreditsAtRepresentationBoundary(F.Model,1,MAX_int32-5);
        if (!TestTrue(TEXT("Factory is destroyed by actual public Attack history"), F.Kill(Before.FactoryId))) return false;
        TestEqual(TEXT("Factory loss alone does not finish a surviving-HQ match"),F.Model.GetSnapshot().MatchPhase,EProofMatchPhase::Playing);
        if (!bOverflow)
        {
            TestTrue(TEXT("Loss refund precedes hold and returns exact actual payment"), F.Job().OwnerId == INDEX_NONE && F.Credits() == 200);
            F.Model.Advance(1.f);
            TestEqual(TEXT("Factory loss cannot refund twice"),F.Credits(),200);
        }
        else
        {
            TestTrue(TEXT("Overflow preserves held dead-factory liability intact"), F.Job().OwnerId == 1 && F.Job().FactoryId == Before.FactoryId
                && F.Job().WorkMicroseconds == Before.WorkMicroseconds && F.Job().PaidCredits == 13 && F.Job().bHeld && F.Credits() == MAX_int32-5);
            const FString Liability = F.Model.GetSnapshot().ComputeDigest();
            TestEqual(TEXT("Cancel refuses overflowing full refund atomically"),F.Send(1,EProofCommandType::CancelProduction,Before.FactoryId,Before.StartSequence).Reason,FString(TEXT("refund-capacity")));
            TestEqual(TEXT("Rejected overflow cancellation changes no published field"),F.Model.GetSnapshot().ComputeDigest(),Liability);
            TestTrue(TEXT("Dead factory control resolves job before live-unit ownership rejection"),F.Control(EProofCommandType::ResumeProduction));
            F.Model.Advance(2.f);
            TestTrue(TEXT("Unheld dead-factory liability cannot progress or spawn"),F.Job().PaidCredits == 13 && F.Job().WorkMicroseconds == Before.WorkMicroseconds
                && F.Credits() == MAX_int32-5 && TimedCount(F.Model.GetSnapshot(),1,EProofUnitKind::Combat) == 0);
            if (!TestTrue(TEXT("Ordinary spending creates representable refund headroom"),F.Build(1,EProofCommandType::BuildFactory,FVector(-3350,0,0)))) return false;
            const int32 Spent = MAX_int32-5-150;
            TestEqual(TEXT("Spending alone does not silently erase liability"),F.Credits(),Spent);
            F.Model.Advance(0.1f);
            TestTrue(TEXT("Next positive Advance refunds all thirteen and removes liability"),F.Job().OwnerId == INDEX_NONE && F.Credits() == Spent+13);
            F.Model.Advance(1.f);
            TestEqual(TEXT("Resolved representation liability never refunds twice"),F.Credits(),Spent+13);
        }
        TestEqual(TEXT("Dead factory cannot accept a replacement production start"),F.Send(1,EProofCommandType::Produce,Before.FactoryId).Reason,FString(TEXT("factory")));
    }
    FTimedPublic LossOrder;
    if (!TestTrue(TEXT("Unheld loss-order public setup"), LossOrder.Init() && LossOrder.Factory() && LossOrder.EnemyAttacker() && LossOrder.Start())) return false;
    LossOrder.Model.Advance(1.2f);
    const FProofProductionJob Losing = LossOrder.Job();
    if (!TestTrue(TEXT("Real enemy order targets actively working factory"), LossOrder.Send(2,EProofCommandType::Attack,
        LossOrder.Id(2,EProofUnitKind::Combat),Losing.FactoryId).bAccepted)) return false;
    LossOrder.Model.Advance(100.f);
    TestTrue(TEXT("Same-tick factory destruction refunds before enough work could complete"), TimedUnit(LossOrder.Model.GetSnapshot(),Losing.FactoryId).HitPoints == 0.f
        && LossOrder.Job().OwnerId == INDEX_NONE && LossOrder.Credits() == 200 && TimedCount(LossOrder.Model.GetSnapshot(),1,EProofUnitKind::Combat) == 0);

    FTimedPublic CancelBoundary;
    if (!TestTrue(TEXT("Live-factory exact refund boundary"),CancelBoundary.Init() && CancelBoundary.Factory() && CancelBoundary.Start())) return false;
    CancelBoundary.Model.Advance(0.100001f);
    FProofTimedProductionFixture::SetCreditsAtRepresentationBoundary(CancelBoundary.Model,1,MAX_int32-1);
    const FString Before = CancelBoundary.Model.GetSnapshot().ComputeDigest();
    TestEqual(TEXT("One credit of insufficient headroom rejects"),CancelBoundary.Send(1,EProofCommandType::CancelProduction,
        CancelBoundary.Job().FactoryId,CancelBoundary.Job().StartSequence).Reason,FString(TEXT("refund-capacity")));
    TestEqual(TEXT("Failed refund is fully atomic"),CancelBoundary.Model.GetSnapshot().ComputeDigest(),Before);
    FProofTimedProductionFixture::SetCreditsAtRepresentationBoundary(CancelBoundary.Model,1,MAX_int32-2);
    TestTrue(TEXT("Exact int32 headroom accepts full refund"),CancelBoundary.Control(EProofCommandType::CancelProduction) && CancelBoundary.Credits() == MAX_int32);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofTimedProductionLifecycleTest,
    "RedalertProof.Skirmish.TimedProduction.OwnersAndLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofTimedProductionLifecycleTest::RunTest(const FString&)
{
    FTimedPublic F;
    if (!TestTrue(TEXT("Concurrent owner setup"),F.Init() && F.Factory(1) && F.Factory(2) && F.Start(2) && F.Start(1))) return false;
    FProofSnapshot Both = F.Model.GetSnapshot();
    TestTrue(TEXT("Jobs are published sorted by owner regardless of start order"),Both.ProductionJobs.Num() == 2
        && Both.ProductionJobs[0].OwnerId == 1 && Both.ProductionJobs[1].OwnerId == 2);
    FProofSnapshot Reordered = Both; Reordered.ProductionJobs.Swap(0,1);
    TestEqual(TEXT("Digest ordering is independent of job array order"),Both.ComputeDigest(),Reordered.ComputeDigest());
    F.Model.Advance(1.f);
    TestTrue(TEXT("Both owners progress with independent exact ledgers"),F.Job(1).WorkMicroseconds == 1000000 && F.Job(2).WorkMicroseconds == 1000000
        && F.Credits(1) == 190 && F.Credits(2) == 190);
    TestTrue(TEXT("Hold one owner only"),F.Control(EProofCommandType::HoldProduction,1));
    F.Model.Advance(1.f);
    TestTrue(TEXT("Other owner continues while first is held"),F.Job(1).WorkMicroseconds == 1000000 && F.Job(2).WorkMicroseconds == 2000000
        && F.Credits(1) == 190 && F.Credits(2) == 180);
    if (!TestTrue(TEXT("Second owned factory can be built without parallel line"),F.Build(1,EProofCommandType::BuildFactory,FVector(-3350,0,0)))) return false;
    int32 OtherFactory = INDEX_NONE;
    for (const FProofUnitRecord& U : F.Model.GetSnapshot().Units)
        if (U.OwnerId == 1 && U.Kind == EProofUnitKind::Factory && U.UnitId != F.Job().FactoryId) OtherFactory = U.UnitId;
    TestEqual(TEXT("A second factory does not create a second owner job"),F.Send(1,EProofCommandType::Produce,OtherFactory).Reason,FString(TEXT("production-busy")));
    if (!TestTrue(TEXT("Teammate keeps match live during seat removal"),F.Model.SeedSkirmishPlayer(3,0,FVector(-6000,0,0)))) return false;
    const FProofProductionJob Survivor = F.Job(2);
    const int32 SurvivorCredits = F.Credits(2);
    if (!TestTrue(TEXT("Remove job owner through real lifecycle"),F.Model.RemoveSkirmishPlayer(1))) return false;
    TestTrue(TEXT("Removal erases only departed job/economy"),F.Job(1).OwnerId == INDEX_NONE && F.Model.GetSnapshot().ProductionJobs.Num() == 1
        && TimedSameJob(F.Job(2),Survivor) && F.Credits(2) == SurvivorCredits
        && !F.Model.GetSnapshot().Players.ContainsByPredicate([](const FProofPlayerRecord& P){return P.OwnerId == 1;}));
    if (!TestTrue(TEXT("Rejoin creates a fresh seat without inherited job"),F.Model.SeedSkirmishPlayer(1,0,FVector(-3000,0,0)))) return false;
    F.Sequences.Add(1,0);
    TestTrue(TEXT("Rejoined owner has initial money and no job"),F.Credits() == 350 && F.Job().OwnerId == INDEX_NONE);
    TestTrue(TEXT("Rejoined owner can build/start through fresh sequence stream"),F.Factory() && F.Start());
    const FProofSnapshot LateObserver = F.Model.GetSnapshot();
    TestTrue(TEXT("Snapshot copy contains complete concurrent job state for observers"),LateObserver.ProductionJobs.Num() == 2
        && TimedSameJob(TimedJob(LateObserver,1),F.Job(1)) && TimedSameJob(TimedJob(LateObserver,2),F.Job(2)));
    F.Model.Reset();
    TestTrue(TEXT("Reset clears all jobs/economy and phase"),F.Model.GetSnapshot().ProductionJobs.Num() == 0
        && F.Model.GetSnapshot().Players.Num() == 0 && F.Model.GetSnapshot().MatchPhase == EProofMatchPhase::Inactive);
    F.Sequences.Reset();
    TestTrue(TEXT("Rematch creates clean job identity from actual commands"),F.Init() && F.Factory() && F.Start()
        && F.Job().StartSequence == 2 && F.Job().WorkMicroseconds == 0 && F.Job().PaidCredits == 0 && !F.Job().bHeld);

    FTimedPublic Terminal;
    if (!TestTrue(TEXT("Terminal public history setup"),Terminal.Init() && Terminal.Factory() && Terminal.EnemyAttacker() && Terminal.Start())) return false;
    Terminal.Model.Advance(0.5f);
    if (!TestTrue(TEXT("Hold published paid state before losing HQ"),Terminal.Control(EProofCommandType::HoldProduction))) return false;
    const FProofProductionJob Frozen = Terminal.Job();
    const int32 FrozenCredits = Terminal.Credits();
    if (!TestTrue(TEXT("Real hostile damage destroys last friendly HQ"),Terminal.Kill(Terminal.Id(1,EProofUnitKind::HQ)))) return false;
    TestTrue(TEXT("Terminal state retains exact frozen job without refund"),Terminal.Model.GetSnapshot().MatchPhase == EProofMatchPhase::Finished
        && Terminal.Model.GetSnapshot().WinnerTeam == 1 && TimedSameJob(Terminal.Job(),Frozen) && Terminal.Credits() == FrozenCredits);
    const FString Final = Terminal.Model.GetSnapshot().ComputeDigest();
    Terminal.Model.Advance(100.f);
    TestEqual(TEXT("Finished match never progresses/refunds jobs"),Terminal.Model.GetSnapshot().ComputeDigest(),Final);
    TestEqual(TEXT("Terminal controls retain ordinary finished precedence"),Terminal.Send(1,EProofCommandType::CancelProduction,
        Frozen.FactoryId,Frozen.StartSequence).Reason,FString(TEXT("finished")));
    TestEqual(TEXT("Finished rejection leaves final snapshot intact"),Terminal.Model.GetSnapshot().ComputeDigest(),Final);
    Terminal.Model.StartSkirmish();
    TestEqual(TEXT("Rematch clears terminal frozen jobs"),Terminal.Model.GetSnapshot().ProductionJobs.Num(),0);

    FTimedPublic Simultaneous;
    if (!TestTrue(TEXT("Simultaneous terminal/loss public setup"), Simultaneous.Init() && Simultaneous.Factory() && Simultaneous.EnemyAttacker()
        && Simultaneous.Start(2))) return false;
    Simultaneous.Model.Advance(5.f);
    int32 SecondAttacker = INDEX_NONE;
    for (const FProofUnitRecord& U : Simultaneous.Model.GetSnapshot().Units)
        if (U.OwnerId == 2 && U.Kind == EProofUnitKind::Combat && U.UnitId != Simultaneous.Id(2,EProofUnitKind::Combat)) SecondAttacker = U.UnitId;
    if (!TestTrue(TEXT("Start and partly pay job before simultaneous lethal attack"), SecondAttacker != INDEX_NONE && Simultaneous.Start())) return false;
    Simultaneous.Model.Advance(0.5f);
    const FProofProductionJob TerminalJob = Simultaneous.Job();
    if (!TestTrue(TEXT("Two ordinary attackers target HQ and active factory independently"), Simultaneous.Send(2,EProofCommandType::Attack,
        Simultaneous.Id(2,EProofUnitKind::Combat),Simultaneous.Id(1,EProofUnitKind::HQ)).bAccepted
        && Simultaneous.Send(2,EProofCommandType::Attack,SecondAttacker,TerminalJob.FactoryId).bAccepted)) return false;
    Simultaneous.Model.Advance(100.f);
    TestTrue(TEXT("Terminal freeze takes precedence over same-tick dead-factory refund"), Simultaneous.Model.GetSnapshot().MatchPhase == EProofMatchPhase::Finished
        && TimedUnit(Simultaneous.Model.GetSnapshot(),TerminalJob.FactoryId).HitPoints == 0.f
        && TimedSameJob(Simultaneous.Job(),TerminalJob) && Simultaneous.Credits() == 195);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofTimedProductionBotTest,
    "RedalertProof.Skirmish.TimedProduction.BotPlanningAndReplay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofTimedProductionBotTest::RunTest(const FString&)
{
    FTimedPublic F;
    if (!TestTrue(TEXT("Bot suppression uses real active job"),F.Init() && F.Factory() && F.Start())) return false;
    FProofSkirmishBot ActiveBot;
    TestFalse(TEXT("Active job suppresses repeated planner Produce"),TimedHasProduce(ActiveBot.Decide(F.Model.GetSnapshot(),1,0.0)));
    F.Model.Advance(1.f);
    TestFalse(TEXT("Progressing job remains suppressed on next decision"),TimedHasProduce(ActiveBot.Decide(F.Model.GetSnapshot(),1,1.0)));
    if (!TestTrue(TEXT("Hold actual job for planner inspection"),F.Control(EProofCommandType::HoldProduction))) return false;
    TestFalse(TEXT("Manual hold is not interpreted as an empty production slot"),TimedHasProduce(ActiveBot.Decide(F.Model.GetSnapshot(),1,2.0)));
    if (!TestTrue(TEXT("Cancel actual job before planning again"),F.Control(EProofCommandType::CancelProduction))) return false;
    FProofSkirmishBot Available;
    TestTrue(TEXT("Existing funded planning policy starts again after cancellation"),TimedHasProduce(Available.Decide(F.Model.GetSnapshot(),1,0.0)));
    FProofSnapshot Poor = F.Model.GetSnapshot();
    Poor.Players[0].Credits = 49; // Snapshot-only planner boundary, never injected into a live model.
    FProofSkirmishBot PoorBot;
    TestFalse(TEXT("Planner retains full-price threshold even though model permits unfunded start"),TimedHasProduce(PoorBot.Decide(Poor,1,0.0)));

    FTimedPublic A, B;
    if (!TestTrue(TEXT("Ordinary autonomous replay begins from same public match seeds"),A.Init() && B.Init())) return false;
    FProofSkirmishBot BotsA[2], BotsB[2];
    int32 Started = 0, Completed = 0, Progressed = 0;
    for (int32 Tick = 0; Tick < 1200 && A.Model.GetSnapshot().MatchPhase == EProofMatchPhase::Playing; ++Tick)
    {
        for (int32 Owner = 1; Owner <= 2; ++Owner)
        {
            const FProofSnapshot Prior = A.Model.GetSnapshot();
            const TArray<FProofCommand> BatchA = BotsA[Owner-1].Decide(Prior,Owner,Tick*0.25);
            const TArray<FProofCommand> BatchB = BotsB[Owner-1].Decide(B.Model.GetSnapshot(),Owner,Tick*0.25);
            if (!TestEqual(TEXT("Replay planner exact batch length"),BatchA.Num(),BatchB.Num())) return false;
            TestTrue(TEXT("Unchanged bot command budget"),BatchA.Num() <= 4);
            if (TimedJob(Prior,Owner).OwnerId != INDEX_NONE)
                TestFalse(TEXT("No repeated Produce while owner's public job exists"),TimedHasProduce(BatchA));
            for (int32 Index = 0; Index < BatchA.Num(); ++Index)
            {
                const FProofCommand& X = BatchA[Index]; const FProofCommand& Y = BatchB[Index];
                if (!TestTrue(TEXT("Replay complete command payload equality"),X.Sequence == Y.Sequence && X.MatchEpoch == Y.MatchEpoch
                    && X.Type == Y.Type && X.UnitId == Y.UnitId && X.TargetUnitId == Y.TargetUnitId && X.Destination == Y.Destination)) return false;
                const FProofSnapshot Before = A.Model.GetSnapshot();
                const FProofCommandResult RA = A.Model.Submit(Owner,X), RB = B.Model.Submit(Owner,Y);
                if (!TestTrue(TEXT("Replay exact authority receipt equality"),RA.bAccepted == RB.bAccepted && RA.Reason == RB.Reason && RA.Sequence == RB.Sequence)) return false;
                if (RA.bAccepted && X.Type == EProofCommandType::Produce)
                {
                    ++Started;
                    TestTrue(TEXT("Every bot Produce really starts without immediate unit/debit"),A.Model.GetSnapshot().Units.Num() == Before.Units.Num()
                        && TimedPlayer(A.Model.GetSnapshot(),Owner).Credits == TimedPlayer(Before,Owner).Credits
                        && TimedJob(A.Model.GetSnapshot(),Owner).StartSequence == X.Sequence);
                }
                if (!TestEqual(TEXT("Complete visible equality after every replayed command"),A.Model.GetSnapshot().ComputeDigest(),B.Model.GetSnapshot().ComputeDigest())) return false;
            }
        }
        const FProofSnapshot Before = A.Model.GetSnapshot();
        A.Model.Advance(0.25f); B.Model.Advance(0.25f);
        const FProofSnapshot After = A.Model.GetSnapshot();
        if (!TestEqual(TEXT("Complete visible replay equality after every real Advance"),After.ComputeDigest(),B.Model.GetSnapshot().ComputeDigest())) return false;
        for (const FProofProductionJob& J : Before.ProductionJobs)
        {
            const FProofProductionJob Later = TimedJob(After,J.OwnerId);
            if (Later.OwnerId != INDEX_NONE && Later.WorkMicroseconds > J.WorkMicroseconds) ++Progressed;
            if (Later.OwnerId == INDEX_NONE && TimedCount(After,J.OwnerId,EProofUnitKind::Combat) > TimedCount(Before,J.OwnerId,EProofUnitKind::Combat)) ++Completed;
        }
        for (const FProofPlayerRecord& P : After.Players) TestTrue(TEXT("Ordinary replay credits never go negative"),P.Credits >= 0);
    }
    TestTrue(TEXT("Ordinary autonomous matches exercised starts, progress and completion"),Started >= 4 && Progressed >= 4 && Completed >= 4);
    TestEqual(TEXT("Bounded ordinary autonomous match reaches a terminal result"),A.Model.GetSnapshot().MatchPhase,EProofMatchPhase::Finished);
    return true;
}
#endif
