#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Commands/ProofCommandModel.h"
#include "Commands/ProofOwnerIds.h"
#include "Presentation/ProofSkirmishOwnershipRecovery.h"

namespace
{
FProofCommand RaceCommand(int32 Sequence, EProofCommandType Type, int32 Unit = INDEX_NONE,
    int32 Target = INDEX_NONE, FVector Destination = FVector::ZeroVector)
{
    FProofCommand C;
    C.Sequence = Sequence; C.Type = Type; C.UnitId = Unit;
    C.TargetUnitId = Target; C.Destination = Destination;
    return C;
}
int32 RaceKind(const FProofSnapshot& S, int32 Owner, EProofUnitKind Kind)
{
    const FProofUnitRecord* U = S.Units.FindByPredicate([Owner, Kind](const FProofUnitRecord& R)
        { return R.OwnerId == Owner && R.Kind == Kind; });
    return U ? U->UnitId : INDEX_NONE;
}
const FProofUnitRecord* RaceUnit(const FProofSnapshot& S, int32 Id)
{
    return S.Units.FindByPredicate([Id](const FProofUnitRecord& U) { return U.UnitId == Id; });
}
struct FOwnershipRaceFixture
{
    FProofCommandModel Model;
    int32 Attacker = INDEX_NONE, EnemyHQ = INDEX_NONE, Factory = INDEX_NONE, Harvester = INDEX_NONE;
    FProofSnapshot AtSend, Dead;
    FProofCommand Pending;
    FProofCommandResult Rejected;
    bool Prepare()
    {
        Model.StartSkirmish();
        if (!Model.SeedSkirmishPlayer(1, 0, FVector::ZeroVector)
            || !Model.SeedSkirmishPlayer(ProofOwnerIds::Bot, 1, FVector(700, 0, 0))) return false;
        if (!Model.Submit(1, RaceCommand(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(250, 0, 0))).bAccepted
            || !Model.Submit(ProofOwnerIds::Bot, RaceCommand(1, EProofCommandType::BuildFactory, INDEX_NONE, INDEX_NONE, FVector(950, 0, 0))).bAccepted) return false;
        Factory = RaceKind(Model.GetSnapshot(), 1, EProofUnitKind::Factory);
        const int32 BotFactory = RaceKind(Model.GetSnapshot(), ProofOwnerIds::Bot, EProofUnitKind::Factory);
        if (!Model.Submit(1, RaceCommand(2, EProofCommandType::Produce, Factory)).bAccepted
            || !Model.Submit(ProofOwnerIds::Bot, RaceCommand(2, EProofCommandType::Produce, BotFactory)).bAccepted) return false;
        const FProofSnapshot Armed = Model.GetSnapshot();
        Attacker = RaceKind(Armed, 1, EProofUnitKind::Combat);
        EnemyHQ = RaceKind(Armed, ProofOwnerIds::Bot, EProofUnitKind::HQ);
        Harvester = RaceKind(Armed, 1, EProofUnitKind::Harvester);
        const int32 BotCombat = RaceKind(Armed, ProofOwnerIds::Bot, EProofUnitKind::Combat);
        if (!RaceUnit(Armed, Attacker) || !RaceUnit(Armed, BotCombat) || !RaceUnit(Armed, EnemyHQ)) return false;
        if (!Model.Submit(1, RaceCommand(3, EProofCommandType::Move, Attacker, INDEX_NONE, FVector(400, 0, 0))).bAccepted) return false;
        Model.Advance(3.f);
        AtSend = Model.GetSnapshot();
        const FProofUnitRecord* Source = RaceUnit(AtSend, Attacker);
        if (!Source || Source->OwnerId != 1 || Source->HitPoints <= 0.f || Source->Kind != EProofUnitKind::Combat
            || AtSend.MatchPhase != EProofMatchPhase::Playing) return false;
        // Prepare the request from a real live snapshot, then advance the host
        // before delivering it. No snapshot, health, credits or ore are edited.
        Pending = RaceCommand(4, EProofCommandType::Attack, Attacker, EnemyHQ);
        if (!Model.Submit(ProofOwnerIds::Bot, RaceCommand(3, EProofCommandType::Attack, BotCombat, Attacker)).bAccepted) return false;
        // One fixed schedule, never repeat-until-pass or a wall-time wait.
        for (int32 Step = 0; Step < 80; ++Step) Model.Advance(.1f);
        Dead = Model.GetSnapshot();
        Source = RaceUnit(Dead, Attacker);
        const FProofUnitRecord* Target = RaceUnit(Dead, EnemyHQ);
        if (!Source || Source->OwnerId != 1 || Source->HitPoints != 0.f || !Target || Target->HitPoints <= 0.f
            || Dead.MatchPhase != EProofMatchPhase::Playing) return false;
        Rejected = Model.Submit(1, Pending);
        return !Rejected.bAccepted && Rejected.Reason == TEXT("ownership") && Rejected.Sequence == Pending.Sequence
            && Model.GetSnapshot().ComputeDigest() == Dead.ComputeDigest();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofOwnershipRaceRecoveryTest, "RedalertProof.Fixtures.OwnershipRace.DeadAttackerRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofOwnershipRaceRecoveryTest::RunTest(const FString&)
{
    FOwnershipRaceFixture F;
    if (!TestTrue(TEXT("Ordinary commands and fixed advances create live-at-send then dead-at-validation attacker"), F.Prepare())) return false;
    const FProofUnitRecord* Source = RaceUnit(F.AtSend, F.Attacker);
    const bool bWasOwnedLive = Source && Source->OwnerId == 1 && Source->HitPoints > 0.f;
    TestTrue(TEXT("Saved send snapshot really owns a live Combat"), bWasOwnedLive && Source->Kind == EProofUnitKind::Combat);
    TestEqual(TEXT("Actual Model rejected the prepared Attack for ownership"), F.Rejected.Reason, FString(TEXT("ownership")));
    TestEqual(TEXT("Actual receipt identifies consumed pending sequence"), F.Rejected.Sequence, 4);
    TestTrue(TEXT("Real dead snapshot remains Playing with live target HQ"), F.Dead.MatchPhase == EProofMatchPhase::Playing && RaceUnit(F.Dead, F.EnemyHQ)->HitPoints > 0.f);
    const FString ModelDigest = F.Model.GetSnapshot().ComputeDigest();
    const FString LiveDigest = F.AtSend.ComputeDigest(), DeadDigest = F.Dead.ComputeDigest();
    TMap<int32, int32> IssuedTargets;
    IssuedTargets.Add(F.Attacker, F.EnemyHQ);
    // A fixture-local unrelated mapping checks that recovery never clears the map.
    // This is metadata only, not an extra Model unit or command.
    constexpr int32 UnrelatedKey = MAX_int32;
    IssuedTargets.Add(UnrelatedKey, F.EnemyHQ);
    int32 PendingSequence = F.Rejected.Sequence;
    const auto Wait = FProofSkirmishOwnershipRecovery::Apply(F.AtSend, 1, TEXT("Attack"), F.Rejected,
        F.Attacker, bWasOwnedLive, IssuedTargets, PendingSequence);
    TestTrue(TEXT("Actual shared Pawn policy waits for death replication while client record is live"), Wait == EProofSkirmishOwnershipRecovery::Wait);
    TestEqual(TEXT("Wait preserves receipt latch"), PendingSequence, 4);
    TestEqual(TEXT("Wait preserves both mappings"), IssuedTargets.Num(), 2);
    TestEqual(TEXT("Wait preserves source mapping"), IssuedTargets.FindRef(F.Attacker), F.EnemyHQ);
    TestEqual(TEXT("Wait preserves unrelated mapping"), IssuedTargets.FindRef(UnrelatedKey), F.EnemyHQ);
    const auto Recover = FProofSkirmishOwnershipRecovery::Apply(F.Dead, 1, TEXT("Attack"), F.Rejected,
        F.Attacker, bWasOwnedLive, IssuedTargets, PendingSequence);
    TestTrue(TEXT("Actual shared Pawn policy recovers exact owned-dead source"), Recover == EProofSkirmishOwnershipRecovery::Recovered);
    TestEqual(TEXT("Recovery clears only the local receipt latch"), PendingSequence, 0);
    TestFalse(TEXT("Recovery removes exactly the rejected source mapping"), IssuedTargets.Contains(F.Attacker));
    TestEqual(TEXT("Unrelated mapping survives"), IssuedTargets.FindRef(UnrelatedKey), F.EnemyHQ);
    TestEqual(TEXT("Exactly one mapping remains"), IssuedTargets.Num(), 1);
    TestEqual(TEXT("Policy cannot change authoritative model state"), F.Model.GetSnapshot().ComputeDigest(), ModelDigest);
    TestEqual(TEXT("Policy cannot mutate stale live snapshot"), F.AtSend.ComputeDigest(), LiveDigest);
    TestEqual(TEXT("Policy cannot mutate actual dead snapshot"), F.Dead.ComputeDigest(), DeadDigest);
    TestEqual(TEXT("Consumed rejected sequence cannot replay after receipt latch clears"), F.Model.Submit(1, F.Pending).Reason, FString(TEXT("sequence")));
    TestEqual(TEXT("Replay rejection does not change model state"), F.Model.GetSnapshot().ComputeDigest(), ModelDigest);
    // Ordinary next-sequence production spends normal credits; recovery cannot
    // grant a replacement unit or reset the server's per-owner sequence.
    if (!TestTrue(TEXT("Next ordinary sequence still accepted"), F.Model.Submit(1, RaceCommand(5, EProofCommandType::Produce, F.Factory)).bAccepted)) return false;
    const FProofSnapshot Replaced = F.Model.GetSnapshot();
    const FProofUnitRecord* Replacement = Replaced.Units.FindByPredicate([&F](const FProofUnitRecord& U)
        { return U.OwnerId == 1 && U.Kind == EProofUnitKind::Combat && U.UnitId != F.Attacker && U.HitPoints > 0.f; });
    if (!TestNotNull(TEXT("Normal paid production creates live replacement"), Replacement)) return false;
    TestTrue(TEXT("New attacker uses following ordinary sequence"), F.Model.Submit(1, RaceCommand(6, EProofCommandType::Attack, Replacement->UnitId, F.EnemyHQ)).bAccepted);
    const FProofPlayerRecord* BeforePlayer = F.Dead.Players.FindByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 1; });
    const FProofPlayerRecord* AfterPlayer = Replaced.Players.FindByPredicate([](const FProofPlayerRecord& P) { return P.OwnerId == 1; });
    TestTrue(TEXT("Replacement is paid at unchanged normal price"), BeforePlayer && AfterPlayer && BeforePlayer->Credits - AfterPlayer->Credits == FProofCommandModel::ProductionCost);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProofOwnershipRaceGuardsTest, "RedalertProof.Fixtures.OwnershipRace.GuardsRemainFailOrWait", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProofOwnershipRaceGuardsTest::RunTest(const FString&)
{
    FOwnershipRaceFixture F;
    if (!TestTrue(TEXT("Real model race prepared once for guard checks"), F.Prepare())) return false;
    const auto NotHandled = [this, &F](const TCHAR* Label, const FProofSnapshot& View, int32 Owner,
        const FString& Type, const FProofCommandResult& Receipt, int32 RequestedUnit, bool bWasOwnedLive, int32 InitialLatch)
    {
        TMap<int32, int32> Targets; Targets.Add(F.Attacker, F.EnemyHQ); Targets.Add(MAX_int32, F.EnemyHQ);
        int32 Latch = InitialLatch;
        const FString ViewBefore = View.ComputeDigest(), ModelBefore = F.Model.GetSnapshot().ComputeDigest();
        const auto Result = FProofSkirmishOwnershipRecovery::Apply(View, Owner, Type, Receipt, RequestedUnit, bWasOwnedLive, Targets, Latch);
        TestTrue(Label, Result == EProofSkirmishOwnershipRecovery::NotHandled);
        TestEqual(TEXT("Unrecoverable context preserves latch"), Latch, InitialLatch);
        TestEqual(TEXT("Unrecoverable context preserves mappings"), Targets.Num(), 2);
        TestEqual(TEXT("Source mapping retained"), Targets.FindRef(F.Attacker), F.EnemyHQ);
        TestEqual(TEXT("Unrelated mapping retained"), Targets.FindRef(MAX_int32), F.EnemyHQ);
        TestEqual(TEXT("Guard cannot mutate view"), View.ComputeDigest(), ViewBefore);
        TestEqual(TEXT("Guard cannot mutate model"), F.Model.GetSnapshot().ComputeDigest(), ModelBefore);
    };
    NotHandled(TEXT("No saved owned-live provenance means no recovery"), F.Dead, 1, TEXT("Attack"), F.Rejected, F.Attacker, false, 4);
    NotHandled(TEXT("Ownership failure on a different command type is not swallowed"), F.Dead, 1, TEXT("Move"), F.Rejected, F.Attacker, true, 4);
    NotHandled(TEXT("A foreign record cannot be treated as owned death"), F.Dead, ProofOwnerIds::Bot, TEXT("Attack"), F.Rejected, F.Attacker, true, 4);
    NotHandled(TEXT("A missing exact source cannot be treated as death"), F.Dead, 1, TEXT("Attack"), F.Rejected, MAX_int32, true, 4);
    NotHandled(TEXT("An empty receipt latch cannot recover twice"), F.Dead, 1, TEXT("Attack"), F.Rejected, F.Attacker, true, 0);
    const FProofCommandResult Role = F.Model.Submit(1, RaceCommand(5, EProofCommandType::Attack, F.Harvester, F.EnemyHQ));
    TestEqual(TEXT("Actual role rejection obtained without editing a receipt"), Role.Reason, FString(TEXT("role")));
    NotHandled(TEXT("Other actual rejection reasons remain with existing handlers"), F.Model.GetSnapshot(), 1, TEXT("Attack"), Role, F.Harvester, true, Role.Sequence);
    const FProofCommandResult Accepted = F.Model.Submit(1, RaceCommand(6, EProofCommandType::Move, F.Harvester, INDEX_NONE, FVector(100, 200, 0)));
    TestTrue(TEXT("Actual accepted receipt available"), Accepted.bAccepted);
    NotHandled(TEXT("Accepted command is not ownership recovery"), F.Model.GetSnapshot(), 1, TEXT("Move"), Accepted, F.Harvester, true, Accepted.Sequence);
    // Terminal state is reached by ordinary roster teardown, never a forced
    // phase/health write. The caller keeps its existing outcome handling.
    TestTrue(TEXT("Ordinary removal creates terminal outcome"), F.Model.RemoveSkirmishPlayer(ProofOwnerIds::Bot));
    const FProofSnapshot Terminal = F.Model.GetSnapshot();
    TestTrue(TEXT("Actually terminal"), Terminal.MatchPhase == EProofMatchPhase::Finished);
    NotHandled(TEXT("Terminal outcome cannot enter ownership recovery"), Terminal, 1, TEXT("Attack"), F.Rejected, F.Attacker, true, 4);
    TestTrue(TEXT("Remove own player through ordinary teardown"), F.Model.RemoveSkirmishPlayer(1));
    TestTrue(TEXT("Teardown really removes exact record"), RaceUnit(F.Model.GetSnapshot(), F.Attacker) == nullptr);
    NotHandled(TEXT("Removed record cannot masquerade as an owned-dead retained unit"), F.Model.GetSnapshot(), 1, TEXT("Attack"), F.Rejected, F.Attacker, true, 4);
    return true;
}
#endif
