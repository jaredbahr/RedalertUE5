#include "Presentation/ProofRTSPawn.h"
#include "Presentation/ProofHUDPanelLayout.h"
#include "Presentation/ProofPointerBounds.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UnrealClient.h"

namespace
{
bool IsOutsidePanel(const FVector2D& Point, int32 Width)
{
    const FProofHUDPanelLayout Panel(static_cast<float>(Width), true);
    return !FProofPointerBounds::Contains(Point.X, Point.Y, Panel.Left, Panel.Top,
        Panel.Left + Panel.Width, Panel.Top + Panel.Height);
}

bool IsBoxEligible(const FProofUnitRecord& Unit, int32 OwnerId, bool bMobileOnly)
{
    const double Extent = FProofCommandModel::WorldExtent;
    return Unit.UnitId > 0 && Unit.OwnerId == OwnerId && FMath::IsFinite(Unit.HitPoints) && Unit.HitPoints > 0.f
        && (!bMobileOnly || Unit.Kind == EProofUnitKind::Combat || Unit.Kind == EProofUnitKind::Harvester)
        && FMath::IsFinite(Unit.Position.X) && FMath::IsFinite(Unit.Position.Y) && FMath::IsFinite(Unit.Position.Z)
        && FMath::Abs(Unit.Position.X) <= Extent && FMath::Abs(Unit.Position.Y) <= Extent
        && FMath::Abs(Unit.Position.Z) <= Extent;
}
}

bool AProofRTSPawn::ReadBoxSelectionContext(ProofBoxSelectionGesture::Context& OutContext, FVector2D& OutPointer) const
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    const UGameViewportClient* Client = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    const FViewport* Viewport = Client ? Client->Viewport : nullptr;
    if (!PC || !PC->IsLocalController() || !GS || !Viewport || !Viewport->HasFocus()
        || !Viewport->IsForegroundWindow() || !GS->Snapshot.bSkirmish || GS->Snapshot.MatchEpoch <= 0
        || GS->Snapshot.MatchPhase != EProofMatchPhase::Playing || PC->OwnerId < 0
        || !GS->Snapshot.Players.ContainsByPredicate([PC](const FProofPlayerRecord& P) { return P.OwnerId == PC->OwnerId; })) return false;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0 || !GetPointerScreenPosition(OutPointer) || !IsOutsidePanel(OutPointer, Width)) return false;
    OutContext = {PC->OwnerId, GS->Snapshot.MatchEpoch, static_cast<int32>(GS->Snapshot.MatchPhase),
        static_cast<double>(Width), static_cast<double>(Height), reinterpret_cast<std::uintptr_t>(PC),
        reinterpret_cast<std::uintptr_t>(Viewport)};
    return true;
}

bool AProofRTSPawn::BeginBoxSelection()
{
    ObserveViewportFocus();
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    ProofBoxSelectionGesture::Context Context;
    FVector2D Pointer = FVector2D::ZeroVector;
    const bool bValid = ReadBoxSelectionContext(Context, Pointer);
    const bool bShift = PC && (PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift));
    const bool bArmed = BoxGesture.Begin(Context, {Pointer.X, Pointer.Y}, bShift,
        bValid ? FindUnitUnderCursor(true) : INDEX_NONE, bValid);
    TypeGesture.Cancel();
    return bArmed;
}

void AProofRTSPawn::TickBoxSelection()
{
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!PC || !PC->IsLocalController()) { BoxGesture.Cancel(); return; }
    ProofBoxSelectionGesture::Context Context;
    FVector2D Pointer = FVector2D::ZeroVector;
    const bool bValid = ReadBoxSelectionContext(Context, Pointer);
    if (!bValid) { BoxGesture.Cancel(); return; }
    // Native release runs through its bound callback. If it was lost, an up
    // observation in a valid focused viewport discards the pending gesture.
    // Background key-state flushing is not used to rearm a canceled gesture.
    BoxGesture.ObserveButton(PC->IsInputKeyDown(EKeys::LeftMouseButton));
    BoxGesture.Sample(Context, {Pointer.X, Pointer.Y}, bValid);
}

void AProofRTSPawn::OnManualSelectRelease()
{
    // Count every observed bound release, even unmatched/canceled input. Shared
    // selection actions never increment either native LMB edge counter.
    RecordManualInput(TEXT("LMB up"), ManualLeftReleases, TEXT("release observed"));
    ProofBoxSelectionGesture::Context Context;
    FVector2D Pointer = FVector2D::ZeroVector;
    const bool bValid = ReadBoxSelectionContext(Context, Pointer);
    const auto Released = BoxGesture.EndPress(Context, {Pointer.X, Pointer.Y}, bValid);
    if (Released.Action == ProofBoxSelectionGesture::Kind::None) return;
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    // Context was revalidated at release; no asynchronous handoff occurs here.
    if (!PC || !GS || PC->OwnerId != Released.PressContext.Owner
        || GS->Snapshot.MatchEpoch != Released.PressContext.Epoch) return;
    Selection.Synchronize(GS->Snapshot, PC->OwnerId);
    if (Released.Action == ProofBoxSelectionGesture::Kind::Click)
    {
        // Latched press target and Shift preserve click intent under small jitter.
        // SelectSingle/Toggle recheck current ownership and life, structures included.
        const FProofUnitRecord* Target = GS->Snapshot.Units.FindByPredicate([&Released](const FProofUnitRecord& U)
            { return U.UnitId == Released.ClickTarget; });
        const int32 TargetId = Target && IsBoxEligible(*Target, PC->OwnerId, false) ? Target->UnitId : INDEX_NONE;
        if (Released.Additive) Selection.Toggle(GS->Snapshot, PC->OwnerId, TargetId);
        else Selection.SelectSingle(GS->Snapshot, PC->OwnerId, TargetId);
    }
    else
    {
        std::vector<ProofBoxSelection::Candidate> Candidates;
        Candidates.reserve(GS->Snapshot.Units.Num());
        for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
        {
            if (!IsBoxEligible(Unit, PC->OwnerId, true)) continue;
            FVector2D Center;
            if (PC->ProjectWorldLocationToScreen(Unit.Position, Center)
                && FProofSelectionState::IsViewportCenterVisible(Center, FIntPoint(static_cast<int32>(Context.Width), static_cast<int32>(Context.Height)))
                && IsOutsidePanel(Center, static_cast<int32>(Context.Width)))
                Candidates.push_back({Unit.UnitId, {Center.X, Center.Y}, true});
        }
        // Full cap, never remaining Shift capacity: overlaps must not hide new hits.
        const auto Plan = ProofBoxSelection::Build(Candidates.data(), Candidates.size(), Released.Start, Released.End,
            Context.Width, Context.Height, FProofSelectionState::MaxSelectedUnits);
        if (!Selection.ApplyBoxSelection(GS->Snapshot, PC->OwnerId, Released.PressContext.Owner,
            Released.PressContext.Epoch, Plan, Released.Additive)) return;
    }
    SelectedUnitId = Selection.GetPrimaryUnitId();
    RequestStatus = FString::Printf(TEXT("%d owned live units selected."), Selection.GetSelectedUnitIds().Num());
    if (Released.Action == ProofBoxSelectionGesture::Kind::Click && !Released.Additive)
    {
        RequestStatus = SelectedUnitId == INDEX_NONE ? TEXT("No owned live unit selected.") : TEXT("Owned unit selected. Right-click ground or an enemy.");
        const FProofUnitRecord* Selected = GS->Snapshot.Units.FindByPredicate(
            [this](const FProofUnitRecord& Unit) { return Unit.UnitId == SelectedUnitId; });
        if (Selected && Selected->Kind == EProofUnitKind::Harvester)
            RequestStatus = TEXT("Harvester: right-click ore to mine, your refinery to unload.");
    }
}

bool AProofRTSPawn::GetSelectionDragBounds(FBox2D& OutBounds) const
{
    ProofBoxSelectionGesture::Context Context;
    FVector2D Pointer = FVector2D::ZeroVector;
    if (!BoxGesture.IsDragging() || !ReadBoxSelectionContext(Context, Pointer) || !BoxGesture.Matches(Context)) return false;
    const auto Start = BoxGesture.GetStart();
    const auto End = BoxGesture.GetEnd();
    OutBounds = FBox2D(FVector2D(FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y)),
        FVector2D(FMath::Max(Start.X, End.X), FMath::Max(Start.Y, End.Y)));
    return true;
}
