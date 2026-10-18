#include "Presentation/ProofRTSPawn.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void AProofRTSPawn::LogFactoryProductionPhase(const TCHAR* Phase) const
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const FProofSnapshot& S = GetWorld()->GetGameState<AProofGameState>()->Snapshot;
    const FProofUnitRecord* U = S.Units.FindByPredicate([this](const FProofUnitRecord& Candidate) { return Candidate.UnitId == FactoryProduction.Unit; });
    const FVector Position = U ? U->Position : FVector::ZeroVector;
    FProofCommandResult Receipt;
    const bool bReceipt = PC->FindReceipt(FactoryProduction.Sequence, Receipt);
    UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_PRODUCTION_PHASE {\"phase\":\"%s\",\"case\":%d,\"owner\":%d,\"epoch\":%d,\"revision\":%d,\"first\":%d,\"second\":%d,\"factory\":%d,\"unit\":%d,\"sequence\":%d,\"requestSelected\":%d,\"requestCount\":%d,\"nativeProduce\":%d,\"nativeRight\":%d,\"receipt\":%d,\"receiptSequence\":%d,\"receiptEpoch\":%d,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"elapsed\":%.6f}"),
        Phase, FactoryProduction.Case, PC->OwnerId, S.MatchEpoch, S.Revision,
        FactoryProduction.Factories[0], FactoryProduction.Factories[1], FactoryProduction.ExpectedFactory,
        FactoryProduction.Unit, FactoryProduction.Sequence, FactoryProduction.SelectionAtRequest,
        FactoryProduction.SelectionCountAtRequest, ManualProducePresses, ManualRightPresses,
        bReceipt && Receipt.bAccepted ? 1 : 0, bReceipt ? Receipt.Sequence : 0,
        bReceipt ? Receipt.MatchEpoch : 0, Position.X, Position.Y, Position.Z, SkirmishElapsed);
}

// True means this tick sent a command or holds a pending screenshot. Waiting
// for earned credits/replication/movement returns false so normal defense runs.
bool AProofRTSPawn::TickFactoryProductionFixture()
{
    AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    if (!PC || !GS || SkirmishScenario != TEXT("Coop"))
    { FinishSkirmishScenario(false, TEXT("factory_production_context")); return true; }
    const FProofSnapshot& S = GS->Snapshot;
    const FProofPlayerRecord* Player = S.Players.FindByPredicate([PC](const FProofPlayerRecord& P) { return P.OwnerId == PC->OwnerId; });
    const FProofUnitRecord* HQ = nullptr;
    TArray<const FProofUnitRecord*> Factories;
    for (const FProofUnitRecord& U : S.Units)
    {
        if (U.OwnerId != PC->OwnerId || U.HitPoints <= 0.f) continue;
        if (U.Kind == EProofUnitKind::HQ) HQ = &U;
        if (U.Kind == EProofUnitKind::Factory) Factories.Add(&U);
    }
    if (!Player || !HQ || Factories.IsEmpty()) return false;
    const auto Fail = [this](const TCHAR* Reason) { FinishSkirmishScenario(false, Reason); return true; };
    const auto Select = [this, PC, &S](int32 Id)
    {
        // Synthetic local selection uses the production snapshot filter.
        Selection.SelectSingle(S, PC->OwnerId, Id);
        SelectedUnitId = Selection.GetPrimaryUnitId();
        return SelectedUnitId == Id;
    };
    if (FactoryProduction.Stage == 0)
    {
        FactoryProduction.NativeProduce = ManualProducePresses;
        FactoryProduction.NativeRight = ManualRightPresses;
        FactoryProduction.Factories[0] = Factories[0]->UnitId;
        // The normal setup gathered with its granted miner. Give the other
        // ordinary live miner an actual Gather command too; no credit grants.
        const FProofUnitRecord* Miner = S.Units.FindByPredicate([PC, this](const FProofUnitRecord& U)
            { return U.OwnerId == PC->OwnerId && U.HitPoints > 0.f && U.Kind == EProofUnitKind::Harvester && U.UnitId != LastRequestedUnitId; });
        const FProofResourceRecord* Ore = S.Resources.FindByPredicate([](const FProofResourceRecord& R) { return R.Remaining > 0; });
        if (!Miner || !Ore || !Select(Miner->UnitId) || !SetControlsCursorFromWorld(Ore->Position)) return Fail(TEXT("factory_production_second_miner"));
        const int32 Before = LastRequestedSequence;
        OrderUnderCursor();
        if (LastRequestedSequence <= Before || LastRequestedType != TEXT("Gather")) return Fail(TEXT("factory_production_gather"));
        FactoryProduction.Stage = 1;
        return true;
    }
    if (ManualProducePresses != FactoryProduction.NativeProduce || ManualRightPresses != FactoryProduction.NativeRight)
        return Fail(TEXT("factory_production_native_counter"));
    if (FactoryProduction.Stage == 1)
    {
        if (Player->Credits < FProofCommandModel::FactoryCost) return false;
        if (Factories.Num() != 1 || !SetControlsCursorFromWorld(HQ->Position + FVector(-350, 0, 0))) return Fail(TEXT("factory_production_second_site"));
        const int32 Before = LastRequestedSequence;
        BuildUnderCursor(EProofCommandType::BuildFactory);
        if (LastRequestedSequence <= Before || LastRequestedType != TEXT("BuildFactory")) return Fail(TEXT("factory_production_build"));
        FactoryProduction.Stage = 2;
        return true;
    }
    if (Factories.Num() < 2) return false;
    if (Factories.Num() != 2 || Factories[0]->UnitId != FactoryProduction.Factories[0]) return Fail(TEXT("factory_production_array_order"));
    FactoryProduction.Factories[1] = Factories[1]->UnitId;
    if (FactoryProduction.Stage == 2)
    {
        FactoryProduction.Rallies[0] = Factories[0]->Position + FVector(-600, -350, 0);
        FactoryProduction.Rallies[1] = Factories[1]->Position + FVector(-600, 350, 0);
        if (!Select(Factories[0]->UnitId) || !SetFactoryRally(FactoryProduction.Rallies[0])) return Fail(TEXT("factory_production_first_rally"));
        FactoryProduction.SetSequences[0] = LastRequestedSequence;
        FactoryProduction.Stage = 3;
        return true;
    }
    if (FactoryProduction.Stage == 3)
    {
        FProofCommandResult Receipt;
        if (!PC->FindReceipt(FactoryProduction.SetSequences[0], Receipt) || !Receipt.bAccepted
            || !Factories[0]->bHasRallyPoint || Factories[0]->RallyPoint != FactoryProduction.Rallies[0]) return false;
        if (!Select(Factories[1]->UnitId) || !SetFactoryRally(FactoryProduction.Rallies[1])) return Fail(TEXT("factory_production_second_rally"));
        FactoryProduction.SetSequences[1] = LastRequestedSequence;
        FactoryProduction.Stage = 4;
        FactoryProduction.SeenAt = -1.f;
        return true;
    }
    if (FactoryProduction.Stage == 4)
    {
        FProofCommandResult Receipt;
        if (!PC->FindReceipt(FactoryProduction.SetSequences[1], Receipt) || !Receipt.bAccepted
            || !Factories[1]->bHasRallyPoint || Factories[1]->RallyPoint != FactoryProduction.Rallies[1]) return false;
        if (FactoryProduction.SeenAt < 0.f) FactoryProduction.SeenAt = SkirmishElapsed;
        if (SkirmishElapsed - FactoryProduction.SeenAt < 1.f) return false;
        FactoryProduction.Stage = 5;
    }
    if (FactoryProduction.Stage == 5)
    {
        if (Player->Credits < FProofCommandModel::ProductionCost) return false;
        const int32 Chosen = FactoryProduction.Case == 0 ? INDEX_NONE
            : FactoryProduction.Case == 1 ? Factories[1]->UnitId : HQ->UnitId;
        if (!Select(Chosen)) return Fail(TEXT("factory_production_selection"));
        FactoryProduction.SelectionAtRequest = SelectedUnitId;
        FactoryProduction.SelectionCountAtRequest = Selection.GetSelectedUnitIds().Num();
        FactoryProduction.ExpectedFactory = FactoryProduction.Case == 1 ? Factories[1]->UnitId : Factories[0]->UnitId;
        FactoryProduction.BeforeIds.Reset();
        for (const FProofUnitRecord& U : S.Units) FactoryProduction.BeforeIds.Add(U.UnitId);
        const int32 Before = LastRequestedSequence;
        Produce(); // Actual production handler -> Request -> controller RPC/receipt.
        if (LastRequestedType != TEXT("Produce") || LastRequestedSequence <= Before
            || LastRequestedUnitId != FactoryProduction.ExpectedFactory) return Fail(TEXT("factory_production_wrong_request"));
        FactoryProduction.Sequence = LastRequestedSequence;
        FactoryProduction.Unit = INDEX_NONE;
        LogFactoryProductionPhase(TEXT("requested"));
        FactoryProduction.Stage = 6;
        return true;
    }
    if (FactoryProduction.Stage == 6)
    {
        FProofCommandResult Receipt;
        if (!PC->FindReceipt(FactoryProduction.Sequence, Receipt) || !Receipt.bAccepted) return false;
        const FProofUnitRecord* Spawned = S.Units.FindByPredicate([this, PC](const FProofUnitRecord& U)
            { return U.OwnerId == PC->OwnerId && U.Kind == EProofUnitKind::Combat && U.HitPoints > 0.f && !FactoryProduction.BeforeIds.Contains(U.UnitId); });
        if (!Spawned) return false;
        FactoryProduction.Unit = Spawned->UnitId;
        FactoryProduction.ObservedSpawn = Spawned->Position;
        FactoryProduction.SpawnRevision = S.Revision;
        FactoryProduction.SeenAt = SkirmishElapsed;
        FactoryProduction.ProgressRevisions.Reset();
        LogFactoryProductionPhase(TEXT("spawned"));
        FactoryProduction.Stage = 7;
    }
    if (FactoryProduction.Stage == 7)
    {
        const FProofUnitRecord* U = S.Units.FindByPredicate([this](const FProofUnitRecord& Candidate) { return Candidate.UnitId == FactoryProduction.Unit; });
        if (!U || U->HitPoints <= 0.f) return Fail(TEXT("factory_production_unit_lost"));
        if (S.Revision > FactoryProduction.SpawnRevision) FactoryProduction.ProgressRevisions.Add(S.Revision);
        const FVector Rally = FactoryProduction.Rallies[FactoryProduction.Case == 1 ? 1 : 0];
        if (FactoryProduction.ProgressRevisions.Num() < 2 || SkirmishElapsed - FactoryProduction.SeenAt < 1.f
            || FVector::Distance(U->Position, FactoryProduction.ObservedSpawn) <= 50.f
            || FVector::Distance(U->Position, Rally) >= FVector::Distance(FactoryProduction.ObservedSpawn, Rally)) return false;
        LogFactoryProductionPhase(TEXT("moved"));
        const FString Name = FString::Printf(TEXT("%02d-factory-production.png"), FactoryProduction.Case + 3);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(SkirmishOutput, Name), true, false);
        FactoryProduction.SeenAt = SkirmishElapsed;
        FactoryProduction.Stage = 8;
        return true;
    }
    if (FactoryProduction.Stage == 8)
    {
        const FString Name = FString::Printf(TEXT("%02d-factory-production.png"), FactoryProduction.Case + 3);
        if (SkirmishElapsed - FactoryProduction.SeenAt < .5f || IFileManager::Get().FileSize(*FPaths::Combine(SkirmishOutput, Name)) <= 1024) return true;
        UE_LOG(LogTemp, Display, TEXT("PROOF_FACTORY_PRODUCTION_CAPTURE name=%s case=%d written=1 physicalInputVerified=0"), *Name, FactoryProduction.Case);
        ++FactoryProduction.Case;
        FactoryProduction.Stage = FactoryProduction.Case == 3 ? 9 : 5;
    }
    return false;
}