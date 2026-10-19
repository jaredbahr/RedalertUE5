// Portable complete-loop coverage of the real command model and bot planner.
// This does not launch Unreal or validate rendering, networking, or RA2 balance.
#include "Commands/ProofCommandModel.h"
#include "Skirmish/ProofSkirmishBot.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>

namespace
{
constexpr int Owners[] = {1, 100};
constexpr int MaxTicks = 2200;
constexpr float StepSeconds = 0.1f;
int Checks = 0;
int Failures = 0;
const char* CurrentSetup = "setup";

void Check(bool Pass, const char* Message)
{
    ++Checks;
    if (!Pass)
    {
        ++Failures;
        if (Failures <= 20) { std::cerr << "FAIL " << CurrentSetup << ": " << Message << '\n'; }
    }
}

// Serialize every currently defined visible field, rather than using the
// portable SecureHash stub. Array order is deliberately part of the comparison.
std::string Visible(const FProofSnapshot& Snapshot)
{
    std::ostringstream Out;
    Out.imbue(std::locale::classic());
    Out << std::setprecision(17) << Snapshot.Revision << ',' << Snapshot.MatchEpoch << ','
        << Snapshot.bSkirmish << ',' << int(Snapshot.MatchPhase) << ',' << Snapshot.WinnerTeam << ';';
    for (const auto& Player : Snapshot.Players)
    {
        Out << 'P' << Player.OwnerId << ',' << Player.TeamId << ',' << Player.Credits << ','
            << Player.TotalGathered << ';';
    }
    for (const auto& Unit : Snapshot.Units)
    {
        Out << 'U' << Unit.UnitId << ',' << Unit.OwnerId << ',' << int(Unit.Kind) << ','
            << Unit.HitPoints << ',' << Unit.Cargo << ',' << Unit.Position.X << ','
            << Unit.Position.Y << ',' << Unit.Position.Z << ',' << Unit.bHasRallyPoint << ','
            << Unit.RallyPoint.X << ',' << Unit.RallyPoint.Y << ',' << Unit.RallyPoint.Z << ','
            << Unit.LastTravelDirection.X << ',' << Unit.LastTravelDirection.Y << ',' << Unit.LastTravelDirection.Z << ','
            << Unit.bAreaGuard << ',' << Unit.AreaGuardPost.X << ',' << Unit.AreaGuardPost.Y << ',' << Unit.AreaGuardPost.Z << ','
            << Unit.AreaGuardTargetId << ',' << Unit.bAreaGuardReturning << ';';
    }
    for (const auto& Resource : Snapshot.Resources)
    {
        Out << 'R' << Resource.ResourceId << ',' << Resource.OwnerId << ',' << Resource.Remaining << ','
            << Resource.Position.X << ',' << Resource.Position.Y << ',' << Resource.Position.Z << ';';
    }
    for (const auto& Job : Snapshot.ProductionJobs)
    {
        Out << 'J' << Job.OwnerId << ',' << Job.StartSequence << ',' << Job.FactoryId << ','
            << Job.WorkMicroseconds << ',' << Job.PaidCredits << ',' << Job.bHeld << ';';
    }
    return Out.str();
}

struct FRun
{
    int Refineries[2] = {};
    int Factories[2] = {};
    int Gathers[2] = {};
    int Produces[2] = {};
    int Attacks[2] = {};
    int Guards[2] = {};
    std::int64_t BuildingSpent[2] = {};
    int Rejected = 0;
    int Ticks = 0;
    std::string Trace;
    FProofSnapshot Final;
};

bool InWorld(const FVector& Position)
{
    return std::isfinite(Position.X) && std::isfinite(Position.Y) && std::isfinite(Position.Z)
        && std::abs(Position.X) <= FProofCommandModel::WorldExtent
        && std::abs(Position.Y) <= FProofCommandModel::WorldExtent
        && std::abs(Position.Z) <= FProofCommandModel::WorldExtent;
}

void CheckState(const FProofSnapshot& Snapshot, const FRun& Run,
    const int InitialCredits[2], std::int64_t InitialOre)
{
    Check(Snapshot.bSkirmish, "skirmish remains active");
    Check(Snapshot.MatchPhase == EProofMatchPhase::Playing
        || Snapshot.MatchPhase == EProofMatchPhase::Finished, "valid active/terminal phase");
    Check(Snapshot.Players.Num() == 2, "both player records retained");
    std::int64_t OreAndCargoAndGathered = 0;
    for (int Index = 0; Index < 2; ++Index)
    {
        const auto* Player = Snapshot.Players.FindByPredicate(
            [Index](const auto& Candidate) { return Candidate.OwnerId == Owners[Index]; });
        Check(Player != nullptr, "expected owner record retained");
        if (!Player) { continue; }
        Check(Player->Credits >= 0 && Player->TotalGathered >= 0, "nonnegative credits/income");
        // This matrix starts with no Combat and never removes an owner. Dead
        // records remain, so every retained Combat proves one completed paid
        // job. Active job debits account independently for partial production;
        // loss refunds remove that liability without inferring it from cash.
        std::int64_t ProductionSpent = 0;
        for (const auto& Unit : Snapshot.Units)
        {
            if (Unit.OwnerId == Player->OwnerId && Unit.Kind == EProofUnitKind::Combat)
                ProductionSpent += FProofCommandModel::ProductionCost;
        }
        int ActiveJobs = 0;
        for (const auto& Job : Snapshot.ProductionJobs)
        {
            if (Job.OwnerId != Player->OwnerId) { continue; }
            ++ActiveJobs;
            ProductionSpent += Job.PaidCredits;
            Check(Job.StartSequence > 0 && Job.FactoryId > 0, "production identity is positive");
            Check(Job.WorkMicroseconds >= 0 && Job.WorkMicroseconds <= FProofCommandModel::ProductionWorkMicroseconds,
                "production work is bounded");
            Check(Job.PaidCredits == (Job.WorkMicroseconds + FProofCommandModel::ProductionWorkPerCredit - 1)
                / FProofCommandModel::ProductionWorkPerCredit, "actual payment exactly funds completed work");
        }
        Check(ActiveJobs <= 1, "one active generic production line per owner");
        Check(std::int64_t(Player->Credits) == InitialCredits[Index]
            + std::int64_t(Player->TotalGathered) - Run.BuildingSpent[Index] - ProductionSpent,
            "credits equal initial + mined - construction - completed and partial production");
        OreAndCargoAndGathered += Player->TotalGathered;
        int Live = 0;
        for (const auto& Unit : Snapshot.Units)
        {
            if (Unit.OwnerId == Player->OwnerId && Unit.HitPoints > 0) { ++Live; }
        }
        Check(Live <= FProofCommandModel::SkirmishRecordCap, "unchanged live record cap");
    }
    for (const auto& Unit : Snapshot.Units)
    {
        Check(Unit.Cargo >= 0 && Unit.Cargo <= FProofCommandModel::CargoCapacity, "bounded cargo");
        // Dead records retain cargo; destruction is not an implicit credit deposit.
        OreAndCargoAndGathered += Unit.Cargo;
        Check(std::isfinite(Unit.HitPoints) && Unit.HitPoints >= 0, "finite nonnegative health");
        Check(InWorld(Unit.Position), "finite in-world unit positions");
    }
    for (const auto& Resource : Snapshot.Resources)
    {
        Check(Resource.Remaining >= 0, "nonnegative remaining ore");
        OreAndCargoAndGathered += Resource.Remaining;
    }
    Check(OreAndCargoAndGathered == InitialOre, "ore, retained cargo, and gathered credit conservation");
}

void CheckTerminal(const FProofSnapshot& Snapshot)
{
    Check(Snapshot.MatchPhase == EProofMatchPhase::Finished, "complete match reaches terminal within observation bound");
    if (Snapshot.MatchPhase != EProofMatchPhase::Finished) { return; }
    bool LiveHQ[2] = {};
    for (const auto& Unit : Snapshot.Units)
    {
        if (Unit.Kind != EProofUnitKind::HQ || Unit.HitPoints <= 0) { continue; }
        for (int Index = 0; Index < 2; ++Index)
        {
            if (Unit.OwnerId == Owners[Index]) { LiveHQ[Index] = true; }
        }
    }
    Check(!(LiveHQ[0] && LiveHQ[1]), "terminal state requires an eliminated HQ");
    const int ExpectedWinner = LiveHQ[0] ? 0 : (LiveHQ[1] ? 1 : INDEX_NONE);
    Check(Snapshot.WinnerTeam == ExpectedWinner, "winner agrees with surviving HQ team (or simultaneous draw)");
}

FRun RunMatch(const FVector& FirstHQ, const FVector& SecondHQ, bool ReverseSubmit)
{
    FProofCommandModel Model;
    Model.StartSkirmish();
    Check(Model.SeedSkirmishPlayer(Owners[0], 0, FirstHQ), "first legal seed");
    Check(Model.SeedSkirmishPlayer(Owners[1], 1, SecondHQ), "second legal seed");
    const auto Initial = Model.GetSnapshot();
    int InitialCredits[2] = {};
    for (int Index = 0; Index < 2; ++Index)
    {
        const auto* Player = Initial.Players.FindByPredicate(
            [Index](const auto& Candidate) { return Candidate.OwnerId == Owners[Index]; });
        Check(Player != nullptr, "seeded player exists");
        if (Player) { InitialCredits[Index] = Player->Credits; }
    }
    std::int64_t InitialOre = 0;
    for (const auto& Resource : Initial.Resources) { InitialOre += Resource.Remaining; }
    FProofSkirmishBot Bots[2];
    FRun Run;
    std::ostringstream Trace;
    Trace.imbue(std::locale::classic());
    Trace << std::setprecision(17) << Visible(Initial) << '\n';
    for (int Tick = 0; Tick < MaxTicks; ++Tick)
    {
        const auto Before = Model.GetSnapshot();
        Trace << "tick=" << Tick << ';';
        // Both planners see the same pre-step snapshot. Only submission order
        // changes in the reverse case; planner cadence and budgets are untouched.
        for (int Order = 0; Order < 2; ++Order)
        {
            const int Index = ReverseSubmit ? 1 - Order : Order;
            const auto Commands = Bots[Index].Decide(Before, Owners[Index], Tick / 10.0);
            Check(Commands.Num() <= 4, "unchanged four-command planner limit");
            for (const auto& Command : Commands)
            {
                const auto Result = Model.Submit(Owners[Index], Command);
                Trace << 'C' << Owners[Index] << ',' << Command.Sequence << ',' << Command.MatchEpoch << ','
                    << int(Command.Type) << ',' << Command.UnitId << ',' << Command.TargetUnitId << ','
                    << Command.Destination.X << ',' << Command.Destination.Y << ',' << Command.Destination.Z << ','
                    << Result.bAccepted << ',' << *Result.Reason << ',' << Result.Sequence << ',' << Result.MatchEpoch << ';';
                Check(Result.bAccepted, "real planner command accepted in bounded matrix");
                if (!Result.bAccepted) { ++Run.Rejected; continue; }
                if (Command.Type == EProofCommandType::BuildRefinery)
                {
                    ++Run.Refineries[Index]; Run.BuildingSpent[Index] += FProofCommandModel::RefineryCost;
                }
                if (Command.Type == EProofCommandType::BuildFactory)
                {
                    ++Run.Factories[Index]; Run.BuildingSpent[Index] += FProofCommandModel::FactoryCost;
                }
                if (Command.Type == EProofCommandType::Gather) { ++Run.Gathers[Index]; }
                if (Command.Type == EProofCommandType::Produce)
                {
                    ++Run.Produces[Index];
                }
                if (Command.Type == EProofCommandType::Attack) { ++Run.Attacks[Index]; }
                if (Command.Type == EProofCommandType::AreaGuard)
                {
                    ++Run.Guards[Index];
                    Check(Tick / 10.0 < 20.0, "AreaGuard is submitted only before the unchanged attack grace");
                }
            }
        }
        Model.Advance(StepSeconds);
        const auto Snapshot = Model.GetSnapshot();
        Trace << Visible(Snapshot) << '\n';
        CheckState(Snapshot, Run, InitialCredits, InitialOre);
        Run.Ticks = Tick + 1;
        if (Snapshot.MatchPhase == EProofMatchPhase::Finished)
        {
            const auto Frozen = Visible(Snapshot);
            Model.Advance(StepSeconds);
            Check(Visible(Model.GetSnapshot()) == Frozen, "terminal visible state freezes on Advance");
            for (int Index = 0; Index < 2; ++Index)
            {
                Check(Bots[Index].Decide(Snapshot, Owners[Index], 221.0).Num() == 0, "terminal planner stops");
                FProofCommand LateCommand;
                LateCommand.Sequence = 1000000;
                LateCommand.Type = EProofCommandType::Produce;
                const auto Result = Model.Submit(Owners[Index], LateCommand);
                Check(!Result.bAccepted && Result.Reason == TEXT("finished"), "terminal command explicitly rejected");
                Check(Visible(Model.GetSnapshot()) == Frozen, "terminal visible state freezes on Submit");
            }
            break;
        }
    }
    Run.Final = Model.GetSnapshot();
    Run.Trace = Trace.str();
    CheckTerminal(Run.Final);
    for (int Index = 0; Index < 2; ++Index)
    {
        Check(Run.Refineries[Index] > 0 && Run.Factories[Index] > 0, "each bot builds refinery and factory");
        Check(Run.Guards[Index] > 0, "each bot issues real pre-grace AreaGuard through the shared validator");
        Check(Run.Gathers[Index] > 0 && Run.Produces[Index] > 0 && Run.Attacks[Index] > 0,
            "each bot gathers, produces, and attacks");
        Check(Run.Final.Units.ContainsByPredicate([Index](const auto& Unit)
            { return Unit.OwnerId == Owners[Index] && Unit.Kind == EProofUnitKind::Combat; }),
            "each bot completes a paid production job, beyond accepted starts");
        const auto* Player = Run.Final.Players.FindByPredicate(
            [Index](const auto& Candidate) { return Candidate.OwnerId == Owners[Index]; });
        Check(Player && Player->TotalGathered > 0, "each bot actually deposits mined cargo");
    }
    return Run;
}
}

int main()
{
    struct FSetup { const char* Name; FVector FirstHQ; FVector SecondHQ; bool ReverseSubmit; };
    const FSetup Setups[] = {
        {"default-seeds-two-planners", {-1400, -400, 0}, {1400, 0, 0}, false},
        {"default-seeds-reverse-submit", {-1400, -400, 0}, {1400, 0, 0}, true},
        {"horizontal-symmetric", {-1400, 0, 0}, {1400, 0, 0}, false},
        {"vertical-symmetric", {0, -1400, 0}, {0, 1400, 0}, false},
        {"short-horizontal", {-900, 0, 0}, {900, 0, 0}, false},
        {"long-diagonal", {-2200, -1200, 0}, {2200, 1200, 0}, false}
    };
    for (const auto& Setup : Setups)
    {
        CurrentSetup = Setup.Name;
        const auto First = RunMatch(Setup.FirstHQ, Setup.SecondHQ, Setup.ReverseSubmit);
        const auto Replay = RunMatch(Setup.FirstHQ, Setup.SecondHQ, Setup.ReverseSubmit);
        const bool ExactReplay = First.Trace == Replay.Trace && Visible(First.Final) == Visible(Replay.Final);
        Check(ExactReplay, "exact full command/result and visible-state replay");
        std::cout << Setup.Name << " simulated_seconds=" << First.Ticks / 10.0
            << " terminal=" << (First.Final.MatchPhase == EProofMatchPhase::Finished)
            << " winner=" << First.Final.WinnerTeam;
        for (int Index = 0; Index < 2; ++Index)
        {
            const auto* Player = First.Final.Players.FindByPredicate(
                [Index](const auto& Candidate) { return Candidate.OwnerId == Owners[Index]; });
            std::cout << " owner" << Owners[Index] << "[refinery=" << First.Refineries[Index]
                << ",factory=" << First.Factories[Index] << ",gather=" << First.Gathers[Index]
                << ",production_starts=" << First.Produces[Index] << ",attack=" << First.Attacks[Index] << ",guard=" << First.Guards[Index]
                << ",earned=" << (Player ? Player->TotalGathered : -1) << ']';
        }
        std::cout << " rejected=" << First.Rejected << " exact_replay=" << ExactReplay << '\n';
    }
    std::cout << "checks=" << Checks << " failures=" << Failures
        << " Unreal=0 network=0 physical_input=0 performance=0\n";
    return Failures ? 1 : 0;
}
