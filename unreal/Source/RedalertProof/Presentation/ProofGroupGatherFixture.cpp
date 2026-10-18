#include "Presentation/ProofRTSPawn.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void AProofRTSPawn::LogGroupGatherPhase(const TCHAR* Phase) const
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const FProofSnapshot& Snapshot = GetWorld()->GetGameState<AProofGameState>()->Snapshot;
    UE_LOG(LogTemp, Display, TEXT("PROOF_GROUP_GATHER_PHASE {\"phase\":\"%s\",\"owner\":%d,\"epoch\":%d,\"revision\":%d,\"a\":%d,\"b\":%d,\"resource\":%d,\"beforeSequence\":%d,\"nextBefore\":%d,\"nextNow\":%d,\"lastSequence\":%d,\"queue\":%d,\"count\":%d,\"elapsed\":%.6f}"),
        Phase, PC->OwnerId, Snapshot.MatchEpoch, Snapshot.Revision, GroupGatherMiners[0], GroupGatherMiners[1],
        GroupGatherResource, GroupGatherBeforeSequence, GroupGatherNextBefore, PC->PeekNextLocalCommandSequence(),
        LastRequestedSequence, GroupOrders.NumPending(), Selection.GetSelectedUnitIds().Num(), SkirmishElapsed);
}

bool AProofRTSPawn::TickGroupGatherFixture()
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const FProofSnapshot& Snapshot = GetWorld()->GetGameState<AProofGameState>()->Snapshot;
    const FProofPlayerRecord* Player = Snapshot.Players.FindByPredicate(
        [PC](const FProofPlayerRecord& Record) { return Record.OwnerId == PC->OwnerId; });
    if (!Player) { FinishSkirmishScenario(false, TEXT("group_gather_missing_player")); return false; }
    if (GroupGatherStage == 0)
    {
        TArray<const FProofUnitRecord*> Miners;
        for (const FProofUnitRecord& Unit : Snapshot.Units)
            if (Unit.OwnerId == PC->OwnerId && Unit.Kind == EProofUnitKind::Harvester && Unit.HitPoints > 0.f) Miners.Add(&Unit);
        Miners.Sort([](const FProofUnitRecord& A, const FProofUnitRecord& B) { return A.UnitId < B.UnitId; });
        if (Miners.Num() != 2 || GroupOrders.NumPending() != 0)
        { FinishSkirmishScenario(false, TEXT("group_gather_fixture_population")); return false; }
        const FProofResourceRecord* Resource = nullptr;
        for (const FProofResourceRecord& Candidate : Snapshot.Resources)
            if (Candidate.Remaining > 0 && (!Resource || FVector::DistSquared(Candidate.Position, Miners[0]->Position)
                < FVector::DistSquared(Resource->Position, Miners[0]->Position))) Resource = &Candidate;
        if (!Resource || !SetControlsCursorFromWorld(Miners[0]->Position)) return false;
        SelectUnderCursor();
        if (SelectedUnitId != Miners[0]->UnitId || Selection.GetSelectedUnitIds().Num() != 1)
        { FinishSkirmishScenario(false, TEXT("group_gather_select_pointer")); return false; }
        Selection.Toggle(Snapshot, PC->OwnerId, Miners[1]->UnitId);
        SelectedUnitId = Selection.GetPrimaryUnitId();
        if (Selection.GetSelectedUnitIds().Num() != 2 || !Selection.IsAllHarvesters(Snapshot, PC->OwnerId))
        { FinishSkirmishScenario(false, TEXT("group_gather_selection")); return false; }
        GroupGatherEpoch = Snapshot.MatchEpoch;
        GroupGatherResource = Resource->ResourceId;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            GroupGatherMiners[Index] = Miners[Index]->UnitId;
            GroupGatherInitial[Index] = Miners[Index]->Position;
            GroupGatherPriorCargo[Index] = Miners[Index]->Cargo;
        }
        GroupGatherPriorDelivered = Player->TotalGathered;
        GroupGatherBeforeSequence = LastRequestedSequence;
        GroupGatherNextBefore = PC->PeekNextLocalCommandSequence();
        LogGroupGatherPhase(TEXT("selected"));
        if (!SetControlsCursorFromWorld(Resource->Position))
        { FinishSkirmishScenario(false, TEXT("group_gather_ore_projection")); return false; }
        OrderUnderCursor();
        if (LastRequestedSequence != GroupGatherBeforeSequence || PC->PeekNextLocalCommandSequence() != GroupGatherNextBefore
            || GroupOrders.NumPending() != 2)
        { FinishSkirmishScenario(false, TEXT("group_gather_not_queued")); return false; }
        LogGroupGatherPhase(TEXT("queued"));
        GroupGatherStage = 1;
        return false;
    }
    if (GroupGatherEpoch != Snapshot.MatchEpoch)
    { FinishSkirmishScenario(false, TEXT("group_gather_epoch")); return false; }
    if (GroupGatherStage == 1)
    {
        FProofCommandResult Receipts[2];
        if (!PC->FindReceipt(GroupGatherNextBefore, Receipts[0]) || !PC->FindReceipt(GroupGatherNextBefore + 1, Receipts[1])) return false;
        if (!Receipts[0].bAccepted || !Receipts[1].bAccepted || Receipts[0].MatchEpoch != GroupGatherEpoch
            || Receipts[1].MatchEpoch != GroupGatherEpoch || GroupOrders.NumPending() != 0
            || PC->PeekNextLocalCommandSequence() != GroupGatherNextBefore + 2 || LastRequestedSequence != GroupGatherNextBefore + 1)
        { FinishSkirmishScenario(false, TEXT("group_gather_receipts")); return false; }
        // Receipts acknowledge validation before its snapshot necessarily
        // replicates. Bind acceptance to a later published snapshot, not a
        // guessed revision increment or a timing delay.
        if (GroupGatherReceiptSnapshotRevision == INDEX_NONE)
        {
            GroupGatherReceiptSnapshotRevision = Snapshot.Revision;
            return false;
        }
        if (Snapshot.Revision <= GroupGatherReceiptSnapshotRevision) return false;
        LogGroupGatherPhase(TEXT("accepted"));
        GroupGatherStage = 2;
    }
    bool bComplete = true;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const FProofUnitRecord* Miner = Snapshot.Units.FindByPredicate([this, Index](const FProofUnitRecord& Unit)
            { return Unit.UnitId == GroupGatherMiners[Index]; });
        if (!Miner || Miner->OwnerId != PC->OwnerId || Miner->Kind != EProofUnitKind::Harvester || Miner->HitPoints <= 0.f)
        { FinishSkirmishScenario(false, TEXT("group_gather_source_lost")); return false; }
        GroupGatherMoved[Index] |= FVector::DistSquared(Miner->Position, GroupGatherInitial[Index]) > 1.0;
        GroupGatherLoaded[Index] |= Miner->Cargo > 0;
        // Replication can skip the exact depot frame. This is the observed
        // empty-cargo/delivery transition; the strict parser separately demands
        // the actual authoritative <=1cm deposit boundary and attribution.
        GroupGatherDeposited[Index] |= GroupGatherLoaded[Index] && GroupGatherPriorCargo[Index] > 0 && Miner->Cargo == 0
            && Player->TotalGathered > GroupGatherPriorDelivered;
        GroupGatherPriorCargo[Index] = Miner->Cargo;
        bComplete &= GroupGatherMoved[Index] && GroupGatherLoaded[Index] && GroupGatherDeposited[Index];
    }
    GroupGatherPriorDelivered = Player->TotalGathered;
    if (!bComplete) return false;
    const FString Path = FPaths::Combine(SkirmishOutput, TEXT("03-group-gather.png"));
    if (!bGroupGatherCaptureRequested)
    {
        if (IFileManager::Get().FileExists(*Path))
        { FinishSkirmishScenario(false, TEXT("group_gather_stale_capture")); return false; }
        // Normal combat decisions can select other units after acceptance.
        // Restore the two verified live miners through existing selection APIs;
        // do not rely on an ambiguous projected click at the refinery center.
        Selection.SelectSingle(Snapshot, PC->OwnerId, GroupGatherMiners[0]);
        Selection.Toggle(Snapshot, PC->OwnerId, GroupGatherMiners[1]);
        SelectedUnitId = Selection.GetPrimaryUnitId();
        if (Selection.GetSelectedUnitIds().Num() != 2 || !Selection.IsAllHarvesters(Snapshot, PC->OwnerId))
        { FinishSkirmishScenario(false, TEXT("group_gather_capture_selection")); return false; }
        IFileManager::Get().MakeDirectory(*SkirmishOutput, true);
        GroupGatherCaptureRequestedAt = FDateTime::UtcNow();
        FScreenshotRequest::RequestScreenshot(Path, true, false);
        bGroupGatherCaptureRequested = true;
        return false;
    }
    if (IFileManager::Get().FileSize(*Path) <= 1024) return false;
    if (IFileManager::Get().GetTimeStamp(*Path).ToUnixTimestamp() < GroupGatherCaptureRequestedAt.ToUnixTimestamp())
    { FinishSkirmishScenario(false, TEXT("group_gather_capture_timestamp")); return false; }
    LogGroupGatherPhase(TEXT("complete"));
    UE_LOG(LogTemp, Display, TEXT("PROOF_GROUP_GATHER_CAPTURE name=03-group-gather.png selectedCount=2 written=1 physicalInputVerified=0"));
    GroupGatherStage = 3;
    return true;
}
