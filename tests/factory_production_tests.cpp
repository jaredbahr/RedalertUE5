#include "Commands/ProofCommandModel.h"
#include <iostream>

namespace {
int Checks = 0;
int Failures = 0;
void Check(bool Passed, const char* Message)
{
    ++Checks;
    if (!Passed) { ++Failures; std::cerr << "FAIL: " << Message << '\n'; }
}
}

// Test-only presentation adapters. The include contains the entire current
// Produce method extracted unchanged by the runner, not a recreated selector.
struct AProofPlayerController { int32 OwnerId = 1; };
struct AProofGameState { FProofSnapshot Snapshot; };
struct FTestWorld {
    AProofGameState State;
    bool bHasGameState = true;
    template<class T> T* GetGameState() { return bHasGameState ? static_cast<T*>(&State) : nullptr; }
};
template<class T, class U> T* Cast(U* Value) { return static_cast<T*>(Value); }
struct AProofRTSPawn {
    AProofPlayerController Controller;
    bool bHasController = true;
    FTestWorld World;
    int32 SelectedUnitId = INDEX_NONE;
    int32 SkirmishProduceRecordsBefore = -77;
    FString RequestStatus = TEXT("untouched");
    int Requests = 0;
    FProofCommand Requested;
    AProofPlayerController* GetController() { return bHasController ? &Controller : nullptr; }
    FTestWorld* GetWorld() { return &World; }
    void Request(FProofCommand Command) { Requested = Command; ++Requests; }
    void Produce();
};
#include "factory_produce_method.inc"

namespace {
int32 Credits(const FProofSnapshot& State, int32 Owner = 1)
{
    const auto* Player = State.Players.FindByPredicate([Owner](const auto& P) { return P.OwnerId == Owner; });
    return Player ? Player->Credits : INDEX_NONE;
}

const FProofUnitRecord* UnitById(const FProofSnapshot& State, int32 Id)
{
    return State.Units.FindByPredicate([Id](const auto& U) { return U.UnitId == Id; });
}

bool SameVisibleState(const FProofSnapshot& A, const FProofSnapshot& B)
{
    if (A.Revision != B.Revision || A.MatchEpoch != B.MatchEpoch || A.bSkirmish != B.bSkirmish
        || A.MatchPhase != B.MatchPhase || A.WinnerTeam != B.WinnerTeam
        || A.Players.Num() != B.Players.Num() || A.Units.Num() != B.Units.Num()
        || A.Resources.Num() != B.Resources.Num() || A.ProductionJobs.Num() != B.ProductionJobs.Num()) return false;
    for (int32 I = 0; I < A.Players.Num(); ++I) {
        const auto& X = A.Players[I]; const auto& Y = B.Players[I];
        if (X.OwnerId != Y.OwnerId || X.Credits != Y.Credits || X.TeamId != Y.TeamId
            || X.TotalGathered != Y.TotalGathered) return false;
    }
    for (int32 I = 0; I < A.Units.Num(); ++I) {
        const auto& X = A.Units[I]; const auto& Y = B.Units[I];
        if (X.UnitId != Y.UnitId || X.OwnerId != Y.OwnerId || X.Position != Y.Position
            || X.HitPoints != Y.HitPoints || X.Kind != Y.Kind || X.Cargo != Y.Cargo
            || X.bHasRallyPoint != Y.bHasRallyPoint || X.RallyPoint != Y.RallyPoint
            || X.LastTravelDirection != Y.LastTravelDirection || X.bAreaGuard != Y.bAreaGuard
            || X.AreaGuardPost != Y.AreaGuardPost || X.AreaGuardTargetId != Y.AreaGuardTargetId
            || X.bAreaGuardReturning != Y.bAreaGuardReturning) return false;
    }
    for (int32 I = 0; I < A.Resources.Num(); ++I) {
        const auto& X = A.Resources[I]; const auto& Y = B.Resources[I];
        if (X.ResourceId != Y.ResourceId || X.OwnerId != Y.OwnerId || X.Position != Y.Position
            || X.Remaining != Y.Remaining) return false;
    }
    for (int32 I = 0; I < A.ProductionJobs.Num(); ++I) {
        const auto& X = A.ProductionJobs[I]; const auto& Y = B.ProductionJobs[I];
        if (X.OwnerId != Y.OwnerId || X.StartSequence != Y.StartSequence || X.FactoryId != Y.FactoryId
            || X.WorkMicroseconds != Y.WorkMicroseconds || X.PaidCredits != Y.PaidCredits || X.bHeld != Y.bHeld)
            return false;
    }
    return true;
}

struct Fixture {
    FProofCommandModel Model;
    int32 Sequence = 0;
    int32 FirstFactory = INDEX_NONE;
    int32 LastFactory = INDEX_NONE;
    const FVector FirstRally{-1000, 0, 0};
    const FVector LastRally{0, -1000, 0};

    FProofCommandResult Submit(FProofCommand Command) {
        Command.Sequence = ++Sequence;
        return Model.Submit(1, Command);
    }
    explicit Fixture(int32 FactoryCount = 2) {
        Model.StartSkirmish();
        Check(Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector), "seed human normally");
        Check(Model.SeedSkirmishPlayer(100, 1, FVector(8000, 0, 0)), "seed opposing HQ normally");
        FProofCommand Build;
        Build.Type = EProofCommandType::BuildFactory;
        if (FactoryCount > 0) {
            Build.Destination = FVector(-350, 0, 0);
            Check(Submit(Build).bAccepted, "pay for first factory");
            FirstFactory = Model.GetSnapshot().Units.Last().UnitId;
            FProofCommand Rally;
            Rally.Type = EProofCommandType::SetRally;
            Rally.UnitId = FirstFactory;
            Rally.Destination = FirstRally;
            Check(Submit(Rally).bAccepted, "set first factory rally");
        }
        if (FactoryCount > 1) {
            Build.Destination = FVector(0, -350, 0);
            Check(Submit(Build).bAccepted, "pay for second factory");
            LastFactory = Model.GetSnapshot().Units.Last().UnitId;
            FProofCommand Rally;
            Rally.Type = EProofCommandType::SetRally;
            Rally.UnitId = LastFactory;
            Rally.Destination = LastRally;
            Check(Submit(Rally).bAccepted, "set second factory rally");
        }
        Check(Credits(Model.GetSnapshot()) == 350 - FactoryCount * FProofCommandModel::FactoryCost,
            "building factories charges the unchanged price");
    }
};

void CheckRequest(const AProofRTSPawn& Pawn, int32 ExpectedId)
{
    Check(Pawn.Requests == 1, "Produce emits exactly one request");
    Check(Pawn.Requested.Type == EProofCommandType::Produce, "request keeps Produce command type");
    Check(Pawn.Requested.UnitId == ExpectedId, "selected eligible factory wins; otherwise first eligible factory");
    Check(Pawn.Requested.TargetUnitId == INDEX_NONE && Pawn.Requested.Destination == FVector::ZeroVector
        && Pawn.Requested.Sequence == 0 && Pawn.Requested.MatchEpoch == 0,
        "routing does not alter request envelope fields");
    Check(Pawn.RequestStatus == FString(TEXT("untouched")), "successful routing keeps request status");
}

void CheckNoFactory(const AProofRTSPawn& Pawn)
{
    Check(Pawn.Requests == 0, "no eligible factory emits no request");
    Check(Pawn.RequestStatus == FString(TEXT("Build a live factory first (F at cursor, 150 credits).")),
        "no-factory rejection keeps the existing status");
    Check(Pawn.SkirmishProduceRecordsBefore == -77, "no-factory rejection keeps prior receipt count");
}

void PaidProductionCases()
{
    // All factories, credits and rallies here come from real accepted commands.
    // Reversal changes only the replicated array order, not unit identities.
    for (int32 Reverse = 0; Reverse < 2; ++Reverse) {
        for (int32 Selection = 0; Selection < 6; ++Selection) {
            Fixture F;
            const auto Before = F.Model.GetSnapshot();
            AProofRTSPawn Pawn;
            Pawn.World.State.Snapshot = Before;
            if (Reverse) Pawn.World.State.Snapshot.Units.Swap(4, 5);
            Pawn.SelectedUnitId = Selection == 0 ? INDEX_NONE
                : Selection == 1 ? 1 // Own HQ, not a factory.
                : Selection == 2 ? 2 // Own Harvester, not a factory.
                : Selection == 3 ? 9999 // Missing/stale selection.
                : Selection == 4 ? F.FirstFactory : F.LastFactory;
            const int32 Expected = Selection == 4 ? F.FirstFactory
                : Selection == 5 ? F.LastFactory : Reverse ? F.LastFactory : F.FirstFactory;
            const auto* Factory = UnitById(Before, Expected);
            Check(Factory != nullptr, "expected paid factory exists");
            if (!Factory) continue;
            FVector ExpectedSpawn;
            Check(FProofCommandModel::FindSkirmishSpawn(Before, Factory->Position, ExpectedSpawn),
                "real model finds a legal spawn at the expected factory");
            Pawn.Produce();
            CheckRequest(Pawn, Expected);
            Check(Pawn.SkirmishProduceRecordsBefore == 0, "zero own combat records before first production");
            Check(F.Submit(Pawn.Requested).bAccepted, "actual emitted request passes authoritative model");
            auto After = F.Model.GetSnapshot();
            Check(After.Units.Num() == Before.Units.Num() && Credits(After) == Credits(Before),
                "actual input route starts a job without an immediate unit or debit");
            Check(After.ProductionJobs.Num() == 1 && After.ProductionJobs[0].FactoryId == Expected
                && After.ProductionJobs[0].OwnerId == 1 && After.ProductionJobs[0].StartSequence == F.Sequence,
                "authoritative job preserves routed factory and accepted command identity");
            F.Model.Advance(FProofCommandModel::ProductionDurationSeconds);
            After = F.Model.GetSnapshot();
            Check(After.ProductionJobs.IsEmpty(), "completed timed job is removed");
            Check(After.Units.Num() == Before.Units.Num() + 1, "one combat record created after paid work");
            const auto Produced = After.Units.Last();
            Check(Produced.Kind == EProofUnitKind::Combat && Produced.OwnerId == 1,
                "produced unit is owned Combat");
            Check(Produced.Position == ExpectedSpawn, "spawn comes from the chosen factory");
            Check(Credits(After) == 0, "exactly one unchanged production cost deducted");
            Check(Factory->RallyPoint == (Expected == F.FirstFactory ? F.FirstRally : F.LastRally),
                "factory rallies remain distinct");
            const FVector Delta = Factory->RallyPoint - ExpectedSpawn;
            const FVector ExpectedLanding = ExpectedSpawn + Delta * (FProofCommandModel::MovementSpeed / Delta.Size());
            F.Model.Advance(1.f);
            After = F.Model.GetSnapshot();
            const auto* Moved = UnitById(After, Produced.UnitId);
            Check(Moved && Moved->Position.Equals(ExpectedLanding, 1.e-8),
                "authoritative produced unit follows chosen factory rally after one second");
            Check(Credits(After) == 0, "rally movement does not alter credits");
        }
    }
}

void RoutingFilterCases()
{
    Fixture F;
    const auto Base = F.Model.GetSnapshot();
    // Snapshot-only negative controls test client routing. They do not claim
    // these modified snapshots were produced by an authoritative match.
    for (int32 Variant = 0; Variant < 10; ++Variant) {
        for (int32 SelectLast = 0; SelectLast < 2; ++SelectLast) {
            AProofRTSPawn Pawn;
            Pawn.World.State.Snapshot = Base;
            Pawn.SelectedUnitId = SelectLast ? F.LastFactory : F.FirstFactory;
            for (auto& Unit : Pawn.World.State.Snapshot.Units) {
                if (Variant == 0 && Unit.UnitId == F.FirstFactory) Unit.HitPoints = 0;
                if (Variant == 1 && Unit.UnitId == F.FirstFactory) Unit.HitPoints = -1;
                if (Variant == 2 && Unit.UnitId == F.FirstFactory) Unit.OwnerId = 100;
                if (Variant == 3 && Unit.UnitId == F.FirstFactory) Unit.OwnerId = 2; // Foreign teammate.
                if (Variant == 4 && Unit.UnitId == F.LastFactory) Unit.HitPoints = 0;
                if (Variant == 5 && Unit.UnitId == F.LastFactory) Unit.OwnerId = 100;
                if (Variant == 6 && Unit.Kind == EProofUnitKind::Factory) Unit.HitPoints = 0;
                if (Variant == 7 && Unit.Kind == EProofUnitKind::Factory) Unit.OwnerId = 100;
            }
            if (Variant == 8) Pawn.Controller.OwnerId = INDEX_NONE;
            if (Variant == 9) Pawn.Controller.OwnerId = 404;
            Pawn.Produce();
            if (Variant >= 6) CheckNoFactory(Pawn);
            else {
                CheckRequest(Pawn, Variant < 4 ? F.LastFactory : F.FirstFactory);
                Check(Pawn.SkirmishProduceRecordsBefore == 0, "filtering keeps combat receipt count");
            }
        }
    }
    // Three eligible factories test middle selection and prevent a last-two
    // special case from satisfying the multiple-factory contract.
    for (int32 Selection = 0; Selection < 4; ++Selection) {
        AProofRTSPawn Pawn;
        Pawn.World.State.Snapshot = Base;
        auto Third = *UnitById(Base, F.LastFactory);
        Third.UnitId = 1234;
        Pawn.World.State.Snapshot.Units.Add(Third);
        Pawn.SelectedUnitId = Selection == 0 ? INDEX_NONE : Selection == 1 ? F.FirstFactory
            : Selection == 2 ? F.LastFactory : Third.UnitId;
        Pawn.Produce();
        CheckRequest(Pawn, Selection <= 1 ? F.FirstFactory : Selection == 2 ? F.LastFactory : Third.UnitId);
    }
}

void OneOrNoFactoryCases()
{
    for (int32 Count = 0; Count <= 1; ++Count) {
        Fixture F(Count);
        for (const int32 Selection : {INDEX_NONE, F.FirstFactory, 9999}) {
            AProofRTSPawn Pawn;
            Pawn.World.State.Snapshot = F.Model.GetSnapshot();
            Pawn.SelectedUnitId = Selection;
            Pawn.Produce();
            if (Count == 0) CheckNoFactory(Pawn);
            else CheckRequest(Pawn, F.FirstFactory);
        }
    }
    // Empty replication before any unit arrives remains a local rejection.
    AProofRTSPawn Empty;
    Empty.World.State.Snapshot.bSkirmish = true;
    Empty.Produce();
    CheckNoFactory(Empty);
}

void BusyAndUnfundedSecondPurchase()
{
    Fixture F;
    AProofRTSPawn Pawn;
    Pawn.World.State.Snapshot = F.Model.GetSnapshot();
    Pawn.Produce();
    CheckRequest(Pawn, F.FirstFactory);
    Check(F.Submit(Pawn.Requested).bAccepted, "first request starts the 50-credit timed job");
    Pawn.World.State.Snapshot = F.Model.GetSnapshot();
    Pawn.Requests = 0;
    Pawn.Produce();
    CheckRequest(Pawn, F.FirstFactory);
    Check(Pawn.SkirmishProduceRecordsBefore == 0, "pending job is not counted as a completed combat record");
    const auto Before = F.Model.GetSnapshot();
    const auto Result = F.Submit(Pawn.Requested);
    Check(!Result.bAccepted && Result.Reason == FString(TEXT("production-busy")),
        "authoritative model rejects a second simultaneous owner job");
    Check(SameVisibleState(Before, F.Model.GetSnapshot()), "busy rejection changes no visible state or credits");
    F.Model.Advance(FProofCommandModel::ProductionDurationSeconds);
    const auto Completed = F.Model.GetSnapshot();
    Check(Credits(Completed) == 0 && Completed.Units.Num() == Before.Units.Num() + 1
        && Completed.ProductionJobs.IsEmpty(), "only the original paid job completes");
    Pawn.World.State.Snapshot = Completed;
    Pawn.Requests = 0;
    Pawn.Produce();
    CheckRequest(Pawn, F.FirstFactory);
    Check(Pawn.SkirmishProduceRecordsBefore == 1, "completed own combat is counted before another job");
    Check(F.Submit(Pawn.Requested).bAccepted, "zero cash now accepts an automatically suspended production job");
    F.Model.Advance(1.f);
    const auto Paused = F.Model.GetSnapshot();
    Check(Credits(Paused) == 0 && Paused.Units.Num() == Completed.Units.Num()
        && Paused.ProductionJobs.Num() == 1 && Paused.ProductionJobs[0].WorkMicroseconds == 0
        && Paused.ProductionJobs[0].PaidCredits == 0,
        "cash shortage creates no unpaid progress, delayed unit or charge");
}

void PreserveReceiptCounting()
{
    Fixture F;
    AProofRTSPawn Pawn;
    Pawn.World.State.Snapshot = F.Model.GetSnapshot();
    FProofUnitRecord Combat;
    Combat.UnitId = 3000; Combat.OwnerId = 1;
    Pawn.World.State.Snapshot.Units.Add(Combat);
    Combat.UnitId = 3001; Combat.HitPoints = 0;
    Pawn.World.State.Snapshot.Units.Add(Combat);
    Combat.UnitId = 3002; Combat.OwnerId = 100; Combat.HitPoints = 100;
    Pawn.World.State.Snapshot.Units.Add(Combat);
    Pawn.Produce();
    CheckRequest(Pawn, F.FirstFactory);
    Check(Pawn.SkirmishProduceRecordsBefore == 2,
        "existing receipt count includes all own Combat records and excludes foreign records");
}

void PreserveLegacyPaths()
{
    Fixture F;
    for (int32 Path = 0; Path < 4; ++Path) {
        AProofRTSPawn Pawn;
        Pawn.World.State.Snapshot = F.Model.GetSnapshot();
        Pawn.SelectedUnitId = F.LastFactory;
        if (Path == 0) Pawn.World.State.Snapshot.bSkirmish = false;
        if (Path == 1) Pawn.bHasController = false;
        if (Path == 2) Pawn.World.bHasGameState = false;
        if (Path == 3) { Pawn.bHasController = false; Pawn.World.bHasGameState = false; }
        Pawn.Produce();
        CheckRequest(Pawn, INDEX_NONE);
        Check(Pawn.SkirmishProduceRecordsBefore == -77, "legacy/missing-adapter paths do not set skirmish receipt count");
    }
    // The unchanged non-skirmish envelope still works through the real model.
    FProofCommandModel Model;
    Model.AddPlayer(1, FProofCommandModel::ProductionCost);
    Model.SeedUnit(1, FVector(1000, 500, 0));
    AProofRTSPawn Pawn;
    Pawn.World.State.Snapshot = Model.GetSnapshot();
    Pawn.Produce();
    CheckRequest(Pawn, INDEX_NONE);
    Pawn.Requested.Sequence = 1;
    Check(Model.Submit(1, Pawn.Requested).bAccepted, "non-skirmish production remains accepted without factory ID");
    const auto State = Model.GetSnapshot();
    Check(Credits(State) == 0 && State.Units.Num() == 2, "non-skirmish production keeps unit and debit behavior");
    Check(State.Units[1].Position == FVector(1000, 500, 0), "non-skirmish production keeps its owned-unit anchor");
}
}

int main()
{
    PaidProductionCases();
    RoutingFilterCases();
    OneOrNoFactoryCases();
    BusyAndUnfundedSecondPurchase();
    PreserveReceiptCounting();
    PreserveLegacyPaths();
    std::cout << (Failures ? "FAIL: " : "PASS: ") << Checks << " active checks, " << Failures << " failures.\n";
    return Failures ? 1 : 0;
}
