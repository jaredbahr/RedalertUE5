#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"

namespace
{
FProofCommand OwnershipCommand(int32 Sequence, EProofCommandType Type, int32 UnitId = INDEX_NONE,
    int32 TargetId = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand Result;
    Result.Sequence = Sequence;
    Result.Type = Type;
    Result.UnitId = UnitId;
    Result.TargetUnitId = TargetId;
    Result.Destination = Destination;
    return Result;
}

int32 OwnershipUnit(const FProofSnapshot& Snapshot, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* Unit = Snapshot.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& Candidate)
        { return Candidate.OwnerId == Owner && Candidate.Kind == Kind && Candidate.HitPoints > 0.f; });
    return Unit ? Unit->UnitId : INDEX_NONE;
}

int32 OwnershipCredits(const FProofSnapshot& Snapshot, int32 Owner)
{
    const FProofPlayerRecord* Player = Snapshot.Players.FindByPredicate([Owner](const FProofPlayerRecord& Candidate)
        { return Candidate.OwnerId == Owner; });
    return Player ? Player->Credits : INDEX_NONE;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofSkirmishOwnershipTest, "RedalertProof.Skirmish.OwnershipIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofSkirmishOwnershipTest::RunTest(const FString&)
{
    // Each attack starts with sufficient credits, a valid refinery/factory and
    // legal unit roles, so rejection exercises ownership/team validation.
    const EProofCommandType Cases[] = { EProofCommandType::Move, EProofCommandType::Gather,
        EProofCommandType::Produce, EProofCommandType::Attack };
    for (EProofCommandType Type : Cases)
    {
        FProofCommandModel Model;
        Model.StartSkirmish();
        if (!TestTrue(TEXT("Seed first human"), Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector))
            || !TestTrue(TEXT("Seed cooperative human"), Model.SeedSkirmishPlayer(2, 0, FVector(0, 2000, 0)))
            || !TestTrue(TEXT("Seed opposing bot"), Model.SeedSkirmishPlayer(100, 1, FVector(5000, 0, 0)))) return false;
        const FProofSnapshot Seeded = Model.GetSnapshot();
        for (int32 Owner : { 1, 2 })
        {
            const FVector Origin(0, Owner == 1 ? 0 : 2000, 0);
            if (!TestTrue(TEXT("Prepare own refinery"), Model.Submit(Owner, OwnershipCommand(1,
                EProofCommandType::BuildRefinery, INDEX_NONE, INDEX_NONE, Origin + FVector(300, 0, 0))).bAccepted)
                || !TestTrue(TEXT("Prepare own factory"), Model.Submit(Owner, OwnershipCommand(2,
                EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, Origin + FVector(-300, 0, 0))).bAccepted)) return false;
            const int32 Factory = OwnershipUnit(Model.GetSnapshot(), Owner, EProofUnitKind::Factory);
            if (!TestTrue(TEXT("Prepare own combat"), Model.Submit(Owner,
                OwnershipCommand(3, EProofCommandType::Produce, Factory)).bAccepted)) return false;
        }
        // This fixture checks for latent foreign orders against a stationary
        // teammate. Stop only its newly granted auto-miner through its own owner;
        // the seeded harvester remains the target of the forbidden requests.
        int32 StoppedGrants = 0;
        const FProofSnapshot Prepared = Model.GetSnapshot();
        for (const FProofUnitRecord& Unit : Prepared.Units)
        {
            if (Unit.OwnerId != 1 || Unit.Kind != EProofUnitKind::Harvester
                || Seeded.Units.ContainsByPredicate([&Unit](const FProofUnitRecord& Prior)
                    { return Prior.UnitId == Unit.UnitId; })) continue;
            if (!TestTrue(TEXT("Stop granted teammate miner for stationary ownership fixture"),
                Model.Submit(1, OwnershipCommand(4, EProofCommandType::Stop, Unit.UnitId)).bAccepted)) return false;
            ++StoppedGrants;
        }
        if (!TestEqual(TEXT("Exactly one incidental teammate grant stopped"), StoppedGrants, 1)) return false;
        const FProofSnapshot Before = Model.GetSnapshot();
        const int32 ForeignHarvester = OwnershipUnit(Before, 1, EProofUnitKind::Harvester);
        const int32 OwnHarvester = OwnershipUnit(Before, 2, EProofUnitKind::Harvester);
        const int32 OwnFactory = OwnershipUnit(Before, 2, EProofUnitKind::Factory);
        const int32 OwnCombat = OwnershipUnit(Before, 2, EProofUnitKind::Combat);
        const int32 Ore = Before.Resources[1].ResourceId;
        FProofCommand Forbidden = OwnershipCommand(4, Type, ForeignHarvester, Ore, FVector(100, 2100, 0));
        FProofCommand Legal = OwnershipCommand(5, Type, OwnHarvester, Ore, FVector(100, 2100, 0));
        FString ExpectedReason(TEXT("ownership"));
        if (Type == EProofCommandType::Produce)
        {
            Forbidden.UnitId = OwnershipUnit(Before, 1, EProofUnitKind::Factory);
            Legal.UnitId = OwnFactory;
            ExpectedReason = TEXT("factory");
        }
        else if (Type == EProofCommandType::Attack)
        {
            Forbidden.UnitId = OwnCombat;
            Forbidden.TargetUnitId = OwnershipUnit(Before, 1, EProofUnitKind::HQ);
            Legal.UnitId = OwnCombat;
            Legal.TargetUnitId = OwnershipUnit(Before, 100, EProofUnitKind::HQ);
            ExpectedReason = TEXT("team");
        }
        const FString Case = FString::Printf(TEXT("Command type %d"), static_cast<int32>(Type));
        const FProofCommandResult Rejected = Model.Submit(2, Forbidden);
        TestFalse(Case + TEXT(" is rejected"), Rejected.bAccepted);
        TestEqual(Case + TEXT(" rejects at intended authority boundary"), Rejected.Reason, ExpectedReason);
        TestEqual(Case + TEXT(" preserves all visible state on rejection"), Model.GetSnapshot().ComputeDigest(), Before.ComputeDigest());
        TestEqual(Case + TEXT(" leaves sender credits intact"), OwnershipCredits(Model.GetSnapshot(), 2), OwnershipCredits(Before, 2));
        TestEqual(Case + TEXT(" leaves teammate credits intact"), OwnershipCredits(Model.GetSnapshot(), 1), OwnershipCredits(Before, 1));
        TestEqual(Case + TEXT(" consumes admitted rejected sequence"), Model.Submit(2, Forbidden).Reason, FString(TEXT("sequence")));
        TestEqual(Case + TEXT(" replay also preserves visible state"), Model.GetSnapshot().ComputeDigest(), Before.ComputeDigest());
        TestTrue(Case + TEXT(" permits next own legal command"), Model.Submit(2, Legal).bAccepted);
        Model.Advance(0.1f);
        const FProofSnapshot After = Model.GetSnapshot();
        // Invalid requests must not leave a latent foreign order behind.
        for (const FProofUnitRecord& Unit : Before.Units)
        {
            if (Unit.OwnerId != 1) continue;
            const FProofUnitRecord* Actual = After.Units.FindByPredicate([&Unit](const FProofUnitRecord& Candidate)
                { return Candidate.UnitId == Unit.UnitId; });
            if (!TestNotNull(Case + TEXT(" keeps teammate unit"), Actual)) continue;
            TestTrue(Case + TEXT(" cannot move teammate"), Actual->Position.Equals(Unit.Position, 0.0));
            TestEqual(Case + TEXT(" cannot damage teammate"), Actual->HitPoints, Unit.HitPoints);
            TestEqual(Case + TEXT(" cannot load teammate cargo"), Actual->Cargo, Unit.Cargo);
        }
    }
    return true;
}
#endif
