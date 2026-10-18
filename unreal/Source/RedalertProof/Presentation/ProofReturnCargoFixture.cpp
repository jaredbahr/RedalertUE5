#include "Presentation/ProofRTSPawn.h"
#include "Runtime/ProofGameMode.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
const FProofUnitRecord* ReturnUnit(const FProofSnapshot& State, int32 Id)
{
    return State.Units.FindByPredicate([Id](const FProofUnitRecord& Unit) { return Unit.UnitId == Id; });
}
const FProofPlayerRecord* ReturnPlayer(const FProofSnapshot& State, int32 Owner)
{
    return State.Players.FindByPredicate([Owner](const FProofPlayerRecord& Player) { return Player.OwnerId == Owner; });
}
int64 ReturnAccounted(const FProofSnapshot& State)
{
    int64 Value = 0;
    for (const FProofResourceRecord& Resource : State.Resources) Value += Resource.Remaining;
    for (const FProofUnitRecord& Unit : State.Units) Value += Unit.Cargo;
    for (const FProofPlayerRecord& Player : State.Players) Value += Player.TotalGathered;
    return Value;
}
const FProofUnitRecord* ReturnDepot(const FProofSnapshot& State, const FProofUnitRecord& Miner)
{
    return State.Units.FindByPredicate([&Miner](const FProofUnitRecord& Unit)
        { return Unit.OwnerId == Miner.OwnerId && Unit.Kind == EProofUnitKind::Refinery && Unit.HitPoints > 0.f
            && FVector::DistSquared(Miner.Position, Unit.Position) <= 1.0; });
}
}

bool AProofRTSPawn::ReturnCargoCheck(bool bCondition, const TCHAR* Reason)
{
    if (!bCondition)
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_RETURN_CARGO_ERROR %s"), Reason);
        FinishSkirmishScenario(false, Reason);
    }
    return bCondition;
}

void AProofRTSPawn::LogReturnCargoPhase(const TCHAR* Phase, const FProofSnapshot& Snapshot) const
{
    const FProofReturnCargoRuntimeState& S = ReturnCargoRuntime;
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const FProofPlayerRecord* Player = ReturnPlayer(Snapshot, PC->OwnerId);
    const FProofUnitRecord* A = ReturnUnit(Snapshot, S.Miners[0]);
    const FProofUnitRecord* B = ReturnUnit(Snapshot, S.Miners[1]);
    UE_LOG(LogTemp, Display, TEXT("PROOF_RETURN_CARGO_PHASE {\"phase\":\"%s\",\"owner\":%d,\"epoch\":%d,\"revision\":%d,\"a\":%d,\"b\":%d,\"refinery\":%d,\"resource\":%d,\"beforeSequence\":%d,\"nextBefore\":%d,\"nextNow\":%d,\"lastSequence\":%d,\"queue\":%d,\"count\":%d,\"cargoA\":%d,\"cargoB\":%d,\"credits\":%d,\"gathered\":%d,\"elapsed\":%.6f,\"physicalInputVerified\":0,\"nativeCallbackInvoked\":0,\"networkTransportVerified\":0,\"sharedHandlers\":1}"),
        Phase, PC->OwnerId, Snapshot.MatchEpoch, Snapshot.Revision, S.Miners[0], S.Miners[1], S.Refinery, S.Resource,
        S.BeforeSequence, S.NextBefore, PC->PeekNextLocalCommandSequence(), LastRequestedSequence,
        GroupOrders.NumPending(), Selection.GetSelectedUnitIds().Num(), A ? A->Cargo : -1, B ? B->Cargo : -1,
        Player ? Player->Credits : -1, Player ? Player->TotalGathered : -1, SkirmishElapsed);
}

bool AProofRTSPawn::ReturnCargoReceiptsReady(const FProofSnapshot& Snapshot, int32 Count)
{
    FProofReturnCargoRuntimeState& S = ReturnCargoRuntime;
    if (S.bReceiptsConfirmed) return true;
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FProofCommandResult Receipt;
        if (!PC->FindReceipt(S.NextBefore + Index, Receipt)) return false;
        if (!ReturnCargoCheck(Receipt.bAccepted && Receipt.MatchEpoch == S.Epoch, TEXT("return_cargo_receipt"))) return false;
    }
    if (!ReturnCargoCheck(GroupOrders.NumPending() == 0
        && PC->PeekNextLocalCommandSequence() == S.NextBefore + Count
        && LastRequestedSequence == S.NextBefore + Count - 1, TEXT("return_cargo_dispatch_sequence"))) return false;
    // Validation acknowledgements alone do not establish published state.
    if (S.ReceiptRevision == INDEX_NONE) { S.ReceiptRevision = Snapshot.Revision; return false; }
    if (Snapshot.Revision <= S.ReceiptRevision) return false;
    S.bReceiptsConfirmed = true;
    return true;
}

// Returns whether ordinary Victory actions may run this tick. Only receipt and
// screenshot barriers hold them; mining travel never starves production/defense.
// The real shared handlers run with a projected cursor. Native input callbacks
// are not invoked, and standalone authority does not test network transport.
bool AProofRTSPawn::TickReturnCargoFixture()
{
    FProofReturnCargoRuntimeState& S = ReturnCargoRuntime;
    if (S.Stage == 15) return true;
    if (!S.bPublishedSnapshotPreviouslySeen) return false;
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    // Copy before any Request: local authority can replace the published state.
    const FProofSnapshot Snapshot = GetWorld()->GetGameState<AProofGameState>()->Snapshot;
    const FProofPlayerRecord* Player = ReturnPlayer(Snapshot, PC->OwnerId);
    if (!ReturnCargoCheck(Player && PC->OwnerId == 1, TEXT("return_cargo_owner"))) return false;
    const auto BeginBatch = [&]()
    {
        S.BeforeSequence = LastRequestedSequence;
        S.NextBefore = PC->PeekNextLocalCommandSequence();
        S.ReceiptRevision = INDEX_NONE;
        S.bReceiptsConfirmed = false;
    };
    const auto SelectIds = [&](bool bBoth)
    {
        // Existing selection APIs avoid ambiguous projected clicks when two
        // miners occupy the same depot/ore center. Initial selection below and
        // selection of the still-idle B use the real SelectUnderCursor handler.
        Selection.SelectSingle(Snapshot, PC->OwnerId, S.Miners[0]);
        if (bBoth) Selection.Toggle(Snapshot, PC->OwnerId, S.Miners[1]);
        SelectedUnitId = Selection.GetPrimaryUnitId();
        return ReturnCargoCheck(Selection.GetSelectedUnitIds().Num() == (bBoth ? 2 : 1)
            && Selection.IsAllHarvesters(Snapshot, PC->OwnerId), TEXT("return_cargo_selection"));
    };
    const auto IssuePointer = [&](const FVector& Target, const TCHAR* Type, int32 TargetId, int32 QueueCount)
    {
        if (!ReturnCargoCheck(SetControlsCursorFromWorld(Target), TEXT("return_cargo_projection"))) return false;
        BeginBatch();
        OrderUnderCursor();
        if (QueueCount > 0)
            return ReturnCargoCheck(LastRequestedSequence == S.BeforeSequence
                && PC->PeekNextLocalCommandSequence() == S.NextBefore && GroupOrders.NumPending() == QueueCount,
                TEXT("return_cargo_queued"));
        return ReturnCargoCheck(LastRequestedSequence == S.NextBefore
            && PC->PeekNextLocalCommandSequence() == S.NextBefore + 1 && LastRequestedType == Type
            && LastRequestedTarget == TargetId && GroupOrders.NumPending() == 0, TEXT("return_cargo_pointer_request"));
    };
    if (S.Stage == 0)
    {
        TArray<const FProofUnitRecord*> Miners;
        const FProofUnitRecord* Refinery = nullptr;
        for (const FProofUnitRecord& Unit : Snapshot.Units)
        {
            if (Unit.OwnerId != PC->OwnerId || Unit.HitPoints <= 0.f) continue;
            if (Unit.Kind == EProofUnitKind::Harvester) Miners.Add(&Unit);
            if (Unit.Kind == EProofUnitKind::Refinery && (!Refinery || Unit.UnitId < Refinery->UnitId)) Refinery = &Unit;
        }
        Miners.Sort([](const FProofUnitRecord& A, const FProofUnitRecord& B) { return A.UnitId < B.UnitId; });
        if (!ReturnCargoCheck(Miners.Num() == 2 && Refinery && GroupOrders.NumPending() == 0,
            TEXT("return_cargo_population"))) return false;
        const FProofResourceRecord* Resource = nullptr;
        for (const FProofResourceRecord& Candidate : Snapshot.Resources)
            if (Candidate.Remaining > 0 && (!Resource || FVector::DistSquared(Candidate.Position, Miners[0]->Position)
                < FVector::DistSquared(Resource->Position, Miners[0]->Position))) Resource = &Candidate;
        if (!ReturnCargoCheck(Resource && Miners[0]->Cargo == 0 && Miners[1]->Cargo == 0,
            TEXT("return_cargo_initial_state"))) return false;
        S.Epoch = Snapshot.MatchEpoch;
        S.Miners[0] = Miners[0]->UnitId; S.Miners[1] = Miners[1]->UnitId;
        S.Refinery = Refinery->UnitId; S.Resource = Resource->ResourceId;
        S.NativeLeft = ManualLeftPresses; S.NativeRight = ManualRightPresses; S.NativeStop = ManualStopPresses;
        BeginBatch();
        if (!ReturnCargoCheck(SetControlsCursorFromWorld(Miners[0]->Position), TEXT("return_cargo_select_projection"))) return false;
        SelectUnderCursor();
        if (!ReturnCargoCheck(SelectedUnitId == S.Miners[0] && Selection.GetSelectedUnitIds().Num() == 1,
            TEXT("return_cargo_select_pointer"))) return false;
        LogReturnCargoPhase(TEXT("selected"), Snapshot);
        if (!IssuePointer(Resource->Position, TEXT("Gather"), S.Resource, 0)) return false;
        LogReturnCargoPhase(TEXT("gather-a"), Snapshot);
        S.Stage = 1;
        return false;
    }
    const FProofUnitRecord* A = ReturnUnit(Snapshot, S.Miners[0]);
    const FProofUnitRecord* B = ReturnUnit(Snapshot, S.Miners[1]);
    const FProofUnitRecord* Depot = ReturnUnit(Snapshot, S.Refinery);
    const FProofResourceRecord* Ore = Snapshot.Resources.FindByPredicate([&S](const FProofResourceRecord& Resource)
        { return Resource.ResourceId == S.Resource; });
    if (!ReturnCargoCheck(Snapshot.MatchEpoch == S.Epoch && A && B && Depot && Ore
        && A->OwnerId == PC->OwnerId && B->OwnerId == PC->OwnerId && Depot->OwnerId == PC->OwnerId
        && A->Kind == EProofUnitKind::Harvester && B->Kind == EProofUnitKind::Harvester
        && Depot->Kind == EProofUnitKind::Refinery && A->HitPoints > 0.f && B->HitPoints > 0.f && Depot->HitPoints > 0.f,
        TEXT("return_cargo_source_lost"))) return false;
    if (!ReturnCargoCheck(ManualLeftPresses == S.NativeLeft && ManualRightPresses == S.NativeRight
        && ManualStopPresses == S.NativeStop, TEXT("return_cargo_native_input_changed"))) return false;
    if (S.Stage == 1)
    {
        if (!ReturnCargoReceiptsReady(Snapshot, 1)) return false;
        if (A->Cargo <= 0) return true;
        if (!SelectIds(false) || !IssuePointer(Depot->Position, TEXT("ReturnCargo"), S.Refinery, 0)) return false;
        S.PriorCargo[0] = A->Cargo; S.PriorGathered = Player->TotalGathered;
        LogReturnCargoPhase(TEXT("single-return"), Snapshot);
        S.Stage = 2;
        return false;
    }
    if (S.Stage == 2 || S.Stage == 5 || S.Stage == 10)
    {
        if (!ReturnCargoReceiptsReady(Snapshot, S.Stage == 10 ? 2 : 1)) return false;
        LogReturnCargoPhase(S.Stage == 2 ? TEXT("single-accepted") : S.Stage == 5 ? TEXT("loaded-only-accepted") : TEXT("group-accepted"), Snapshot);
        ++S.Stage;
        return false;
    }
    if (S.Stage == 3 || S.Stage == 6)
    {
        if (S.PriorCargo[0] <= 0 || A->Cargo != 0 || Player->TotalGathered <= S.PriorGathered) return true;
        // Replication can skip the exact arrival frame. The independent host
        // DEPOSIT row, not this observation, proves the <=1cm boundary.
        LogReturnCargoPhase(S.Stage == 3 ? TEXT("single-deposited") : TEXT("loaded-only-deposited"), Snapshot);
        ++S.Stage;
        return true;
    }
    if (S.Stage == 4)
    {
        if (A->Cargo <= 0) return true;
        if (!ReturnCargoCheck(B->Cargo == 0, TEXT("return_cargo_empty_member"))) return false;
        LogReturnCargoPhase(TEXT("single-resumed"), Snapshot);
        if (!SelectIds(true) || !IssuePointer(Depot->Position, TEXT("ReturnCargo"), S.Refinery, 1)) return false;
        S.PriorCargo[0] = A->Cargo; S.PriorGathered = Player->TotalGathered;
        LogReturnCargoPhase(TEXT("loaded-only-queued"), Snapshot);
        S.Stage = 5;
        return false;
    }
    if (S.Stage == 7)
    {
        if (A->Cargo <= 0) return true;
        LogReturnCargoPhase(TEXT("loaded-only-resumed"), Snapshot);
        if (!SelectIds(false)) return false;
        BeginBatch();
        StopSelected();
        if (!ReturnCargoCheck(GroupOrders.NumPending() == 1 && LastRequestedSequence == S.BeforeSequence
            && PC->PeekNextLocalCommandSequence() == S.NextBefore, TEXT("return_cargo_stop_queue"))) return false;
        LogReturnCargoPhase(TEXT("hold-a"), Snapshot);
        S.Stage = 8;
        return false;
    }
    if (S.Stage == 8)
    {
        if (!ReturnCargoReceiptsReady(Snapshot, 1)) return false;
        if (!ReturnCargoCheck(A->Cargo > 0, TEXT("return_cargo_stop_lost_load"))) return false;
        if (S.HeldRevision == INDEX_NONE)
        {
            S.HeldPosition = A->Position; S.HeldCargo = A->Cargo; S.HeldRevision = Snapshot.Revision;
            return false;
        }
        if (Snapshot.Revision <= S.HeldRevision) return false;
        if (!ReturnCargoCheck(A->Cargo == S.HeldCargo && FVector::DistSquared(A->Position, S.HeldPosition) <= 1.0,
            TEXT("return_cargo_stop_not_held"))) return false;
        LogReturnCargoPhase(TEXT("held-a"), Snapshot);
        if (!ReturnCargoCheck(SetControlsCursorFromWorld(B->Position), TEXT("return_cargo_b_projection"))) return false;
        SelectUnderCursor();
        if (!ReturnCargoCheck(SelectedUnitId == B->UnitId && Selection.GetSelectedUnitIds().Num() == 1,
            TEXT("return_cargo_b_selection"))) return false;
        if (!IssuePointer(Ore->Position, TEXT("Gather"), S.Resource, 0)) return false;
        LogReturnCargoPhase(TEXT("gather-b"), Snapshot);
        S.Stage = 9;
        return false;
    }
    if (S.Stage == 9)
    {
        if (!ReturnCargoReceiptsReady(Snapshot, 1)) return false;
        if (!ReturnCargoCheck(A->Cargo == S.HeldCargo && FVector::DistSquared(A->Position, S.HeldPosition) <= 1.0,
            TEXT("return_cargo_hold_changed"))) return false;
        if (B->Cargo <= 0) return true;
        if (!SelectIds(true) || !IssuePointer(Depot->Position, TEXT("ReturnCargo"), S.Refinery, 2)) return false;
        S.PriorCargo[0] = A->Cargo; S.PriorCargo[1] = B->Cargo;
        S.PriorGathered = Player->TotalGathered;
        LogReturnCargoPhase(TEXT("group-queued"), Snapshot);
        S.Stage = 10;
        return false;
    }
    if (S.Stage == 11)
    {
        const FProofUnitRecord* Miners[] = { A, B };
        for (int32 Index = 0; Index < 2; ++Index)
        {
            if (!S.Deposited[Index] && S.PriorCargo[Index] > 0 && Miners[Index]->Cargo == 0
                && Player->TotalGathered > S.PriorGathered)
            {
                S.Deposited[Index] = true;
                if (Index == 0) S.WaitingPosition = A->Position;
            }
        }
        if (!S.Deposited[0] || !S.Deposited[1]) return true;
        LogReturnCargoPhase(TEXT("group-deposited"), Snapshot);
        S.Stage = 12;
        return true;
    }
    if (S.Stage == 12)
    {
        if (!ReturnCargoCheck(A->Cargo == 0 && FVector::DistSquared(A->Position, S.WaitingPosition) <= 1.0,
            TEXT("return_cargo_stopped_miner_resumed"))) return false;
        if (B->Cargo <= 0) return true;
        LogReturnCargoPhase(TEXT("group-resumed"), Snapshot);
        if (!SelectIds(false) || !IssuePointer(Ore->Position, TEXT("Gather"), S.Resource, 0)) return false;
        LogReturnCargoPhase(TEXT("restore-a"), Snapshot);
        S.Stage = 13;
        return false;
    }
    if (S.Stage == 13)
    {
        if (!ReturnCargoReceiptsReady(Snapshot, 1)) return false;
        const FString Path = FPaths::Combine(SkirmishOutput, TEXT("03-return-cargo.png"));
        if (!ReturnCargoCheck(!IFileManager::Get().FileExists(*Path), TEXT("return_cargo_stale_capture"))) return false;
        if (!SelectIds(true)) return false;
        IFileManager::Get().MakeDirectory(*SkirmishOutput, true);
        S.CaptureRequestedAt = FDateTime::UtcNow();
        FScreenshotRequest::RequestScreenshot(Path, true, false);
        S.bCaptureRequested = true;
        S.Stage = 14;
        return false;
    }
    if (S.Stage == 14)
    {
        const FString Path = FPaths::Combine(SkirmishOutput, TEXT("03-return-cargo.png"));
        if (IFileManager::Get().FileSize(*Path) <= 1024) return false;
        if (!ReturnCargoCheck(S.bCaptureRequested
            && IFileManager::Get().GetTimeStamp(*Path).ToUnixTimestamp() >= S.CaptureRequestedAt.ToUnixTimestamp()
            && Selection.GetSelectedUnitIds().Num() == 2, TEXT("return_cargo_capture_timestamp_or_selection"))) return false;
        LogReturnCargoPhase(TEXT("complete"), Snapshot);
        UE_LOG(LogTemp, Display, TEXT("PROOF_RETURN_CARGO_CAPTURE name=03-return-cargo.png selectedCount=2 written=1 sharedHandlers=1 nativeCallbackInvoked=0 physicalInputVerified=0 networkTransportVerified=0"));
        S.Stage = 15;
        return true;
    }
    return ReturnCargoCheck(false, TEXT("return_cargo_unknown_stage"));
}

bool AProofGameMode::ReturnCargoEvidenceRow()
{
    if (++ReturnCargoEvidenceRows <= 512) return true;
    UE_LOG(LogTemp, Error, TEXT("PROOF_RETURN_CARGO_ERROR evidence_row_limit"));
    FPlatformMisc::RequestExitWithStatus(false, 1);
    return false;
}

// Read-only snapshots immediately around the real Submit. resumeResource is
// accepted-command provenance, not an assertion that private Orders were read.
void AProofGameMode::ObserveReturnCargoCommand(int32 Owner, const FProofCommand& Command,
    const FProofCommandResult& Result, const FProofSnapshot& Before, const FProofSnapshot& After)
{
    const FProofPlayerRecord* Prior = ReturnPlayer(Before, Owner);
    const FProofPlayerRecord* Current = ReturnPlayer(After, Owner);
    if (!Prior || !Current || !ReturnCargoEvidenceRow()) return;
    if (Result.bAccepted)
    {
        ReturnCargoAwaitingResume.Remove(Command.UnitId);
        if (Command.Type == EProofCommandType::ReturnCargo)
        {
            FReturnCargoEvidenceOrder Order;
            Order.Sequence = Command.Sequence;
            Order.Depot = Command.TargetUnitId;
            const int32* Source = ReturnCargoGatherSources.Find(Command.UnitId);
            Order.ResumeResource = Source ? *Source : INDEX_NONE;
            ReturnCargoActive.Add(Command.UnitId, Order);
        }
        else
        {
            ReturnCargoActive.Remove(Command.UnitId);
            if (Command.Type == EProofCommandType::Gather) ReturnCargoGatherSources.Add(Command.UnitId, Command.TargetUnitId);
            else ReturnCargoGatherSources.Remove(Command.UnitId);
        }
    }
    const FProofUnitRecord* A = ReturnUnit(Before, Command.UnitId);
    const FProofUnitRecord* B = ReturnUnit(After, Command.UnitId);
    const FVector From = A ? A->Position : FVector::ZeroVector;
    const FVector To = B ? B->Position : FVector::ZeroVector;
    const int32* Resume = ReturnCargoGatherSources.Find(Command.UnitId);
    UE_LOG(LogTemp, Display, TEXT("PROOF_RETURN_CARGO_COMMAND {\"owner\":%d,\"epoch\":%d,\"sequence\":%d,\"type\":%d,\"unit\":%d,\"target\":%d,\"accepted\":%d,\"revisionBefore\":%d,\"revision\":%d,\"cargoBefore\":%d,\"cargoAfter\":%d,\"creditsBefore\":%d,\"creditsAfter\":%d,\"gatheredBefore\":%d,\"gatheredAfter\":%d,\"xBefore\":%.17g,\"yBefore\":%.17g,\"zBefore\":%.17g,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"resumeResource\":%d,\"conservedBefore\":%lld,\"conservedAfter\":%lld}"),
        Owner, MatchEpoch, Command.Sequence, static_cast<int32>(Command.Type), Command.UnitId, Command.TargetUnitId,
        Result.bAccepted ? 1 : 0, Before.Revision, After.Revision, A ? A->Cargo : 0, B ? B->Cargo : 0,
        Prior->Credits, Current->Credits, Prior->TotalGathered, Current->TotalGathered,
        From.X, From.Y, From.Z, To.X, To.Y, To.Z, Resume ? *Resume : INDEX_NONE,
        static_cast<long long>(ReturnAccounted(Before)), static_cast<long long>(ReturnAccounted(After)));
    if (Command.Type == EProofCommandType::ReturnCargo && (Prior->Credits != Current->Credits
        || Prior->TotalGathered != Current->TotalGathered || !A || !B || A->Cargo != B->Cargo
        || A->Position != B->Position || ReturnAccounted(Before) != ReturnAccounted(After)))
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_RETURN_CARGO_ERROR submit_mutated_economy_or_position"));
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
}

// Runs immediately around each actual Model.Advance, after bot submissions.
// Aggregate every owner's ore/cargo/gathered balance and every owner-1 deposit;
// a later replicated frame can never stand in for the arrival boundary.
void AProofGameMode::ObserveReturnCargoAdvance(const FProofSnapshot& Before, const FProofSnapshot& After)
{
    const FProofPlayerRecord* Prior = ReturnPlayer(Before, 1);
    const FProofPlayerRecord* Current = ReturnPlayer(After, 1);
    if (!Prior || !Current) return;
    int32 Sum = 0;
    FString Contributors;
    for (const FProofUnitRecord& A : Before.Units)
    {
        const FProofUnitRecord* B = ReturnUnit(After, A.UnitId);
        const FProofUnitRecord* Depot = B ? ReturnDepot(After, *B) : nullptr;
        if (A.OwnerId != 1 || A.Kind != EProofUnitKind::Harvester || A.HitPoints <= 0.f
            || !B || B->OwnerId != 1 || B->Kind != EProofUnitKind::Harvester || B->HitPoints <= 0.f) continue;
        FReturnCargoEvidenceOrder* Forced = ReturnCargoActive.Find(A.UnitId);
        if (Forced && !Forced->bTravelLogged && A.Cargo > 0 && B->Cargo == A.Cargo
            && FVector::DistSquared(A.Position, B->Position) > 1.0)
        {
            const FProofUnitRecord* Target = ReturnUnit(After, Forced->Depot);
            if (!Target || !ReturnCargoEvidenceRow()) return;
            UE_LOG(LogTemp, Display, TEXT("PROOF_RETURN_CARGO_TRAVEL {\"owner\":1,\"epoch\":%d,\"revision\":%d,\"sequence\":%d,\"miner\":%d,\"depot\":%d,\"cargoBefore\":%d,\"cargoAfter\":%d,\"xBefore\":%.17g,\"yBefore\":%.17g,\"zBefore\":%.17g,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"depotX\":%.17g,\"depotY\":%.17g,\"depotZ\":%.17g}"),
                MatchEpoch, After.Revision, Forced->Sequence, A.UnitId, Target->UnitId, A.Cargo, B->Cargo,
                A.Position.X, A.Position.Y, A.Position.Z, B->Position.X, B->Position.Y, B->Position.Z,
                Target->Position.X, Target->Position.Y, Target->Position.Z);
            Forced->bTravelLogged = true;
        }
        if (B->Cargo >= A.Cargo || !Depot) continue;
        Sum += A.Cargo - B->Cargo;
        if (!Contributors.IsEmpty()) Contributors += TEXT(",");
        Contributors += FString::Printf(TEXT("{\"miner\":%d,\"before\":%d,\"after\":%d,\"depot\":%d,\"sequence\":%d}"),
            A.UnitId, A.Cargo, B->Cargo, Depot->UnitId, Forced ? Forced->Sequence : 0);
    }
    if (Current->Credits - Prior->Credits != Sum || Current->TotalGathered - Prior->TotalGathered != Sum
        || ReturnAccounted(Before) != ReturnAccounted(After))
    {
        UE_LOG(LogTemp, Error, TEXT("PROOF_RETURN_CARGO_ERROR deposit_attribution_or_conservation"));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    for (const FProofUnitRecord& A : Before.Units)
    {
        const FProofUnitRecord* B = ReturnUnit(After, A.UnitId);
        const FProofUnitRecord* Depot = B ? ReturnDepot(After, *B) : nullptr;
        if (A.OwnerId != 1 || A.Kind != EProofUnitKind::Harvester || A.HitPoints <= 0.f
            || !B || B->OwnerId != 1 || B->Kind != EProofUnitKind::Harvester || B->HitPoints <= 0.f) continue;
        if (A.Cargo > B->Cargo && Depot)
        {
            const FReturnCargoEvidenceOrder* Forced = ReturnCargoActive.Find(A.UnitId);
            if (Forced && (Forced->Depot != Depot->UnitId || !Forced->bTravelLogged))
            {
                UE_LOG(LogTemp, Error, TEXT("PROOF_RETURN_CARGO_ERROR deposit_without_targeted_travel"));
                FPlatformMisc::RequestExitWithStatus(false, 1);
                return;
            }
            if (!ReturnCargoEvidenceRow()) return;
            UE_LOG(LogTemp, Display, TEXT("PROOF_RETURN_CARGO_DEPOSIT {\"owner\":1,\"epoch\":%d,\"revision\":%d,\"sequence\":%d,\"resumeResource\":%d,\"miner\":%d,\"depot\":%d,\"before\":%d,\"after\":%d,\"sum\":%d,\"creditsBefore\":%d,\"creditsAfter\":%d,\"gatheredBefore\":%d,\"gatheredAfter\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"depotX\":%.17g,\"depotY\":%.17g,\"depotZ\":%.17g,\"conservedBefore\":%lld,\"conservedAfter\":%lld,\"contributors\":[%s]}"),
                MatchEpoch, After.Revision, Forced ? Forced->Sequence : 0, Forced ? Forced->ResumeResource : INDEX_NONE,
                A.UnitId, Depot->UnitId, A.Cargo, B->Cargo, Sum, Prior->Credits, Current->Credits,
                Prior->TotalGathered, Current->TotalGathered, B->Position.X, B->Position.Y, B->Position.Z,
                Depot->Position.X, Depot->Position.Y, Depot->Position.Z,
                static_cast<long long>(ReturnAccounted(Before)), static_cast<long long>(ReturnAccounted(After)), *Contributors);
            if (Forced && B->Cargo == 0)
            {
                if (Forced->ResumeResource != INDEX_NONE) ReturnCargoAwaitingResume.Add(A.UnitId, *Forced);
                ReturnCargoActive.Remove(A.UnitId);
            }
        }
        const FReturnCargoEvidenceOrder* Resume = ReturnCargoAwaitingResume.Find(A.UnitId);
        if (!Resume || B->Cargo <= A.Cargo) continue;
        const FProofResourceRecord* OreBefore = Before.Resources.FindByPredicate([Resume](const FProofResourceRecord& Resource)
            { return Resource.ResourceId == Resume->ResumeResource; });
        const FProofResourceRecord* OreAfter = After.Resources.FindByPredicate([Resume](const FProofResourceRecord& Resource)
            { return Resource.ResourceId == Resume->ResumeResource; });
        // This fixture deliberately stages one reloading miner at a time:
        // B is still idle for both A cycles; A is stopped for the B cycle.
        // Exact equality fails closed if that exclusivity is ever violated.
        if (!OreBefore || !OreAfter || FVector::DistSquared(B->Position, OreAfter->Position) > 1.0
            || OreBefore->Remaining - OreAfter->Remaining != B->Cargo - A.Cargo)
        {
            UE_LOG(LogTemp, Error, TEXT("PROOF_RETURN_CARGO_ERROR resume_source_attribution"));
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        if (!ReturnCargoEvidenceRow()) return;
        UE_LOG(LogTemp, Display, TEXT("PROOF_RETURN_CARGO_RESUME {\"owner\":1,\"epoch\":%d,\"revision\":%d,\"sequence\":%d,\"miner\":%d,\"resource\":%d,\"cargoBefore\":%d,\"cargoAfter\":%d,\"remainingBefore\":%d,\"remainingAfter\":%d,\"x\":%.17g,\"y\":%.17g,\"z\":%.17g,\"resourceX\":%.17g,\"resourceY\":%.17g,\"resourceZ\":%.17g}"),
            MatchEpoch, After.Revision, Resume->Sequence, A.UnitId, OreAfter->ResourceId, A.Cargo, B->Cargo,
            OreBefore->Remaining, OreAfter->Remaining, B->Position.X, B->Position.Y, B->Position.Z,
            OreAfter->Position.X, OreAfter->Position.Y, OreAfter->Position.Z);
        ReturnCargoAwaitingResume.Remove(A.UnitId);
    }
}
