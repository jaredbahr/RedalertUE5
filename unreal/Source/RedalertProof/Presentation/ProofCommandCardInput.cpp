#include "Presentation/ProofRTSPawn.h"
#include "Commands/ProofCommandModel.h"
#include "Presentation/ProofHUDPanelLayout.h"
#include "Presentation/ProofPointerBounds.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

ProofCommandCard::Context AProofRTSPawn::GetCommandCardContext() const
{
    ProofCommandCard::Context Result;
    Result.RefineryCost = FProofCommandModel::RefineryCost;
    Result.FactoryCost = FProofCommandModel::FactoryCost;
    Result.ProductionCost = FProofCommandModel::ProductionCost;
    Result.RecordCap = FProofCommandModel::SkirmishRecordCap;
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    if (!PC || !GS) return Result;
    Result.Owner = PC->OwnerId;
    Result.Epoch = GS->Snapshot.MatchEpoch;
    const FProofPlayerRecord* Player = GS->Snapshot.Players.FindByPredicate([PC](const FProofPlayerRecord& Record)
        { return Record.OwnerId == PC->OwnerId; });
    Result.Playing = Player && PC->IsLocalController()
        && GS->Snapshot.bSkirmish && GS->Snapshot.MatchPhase == EProofMatchPhase::Playing;
    if (Player) Result.Credits = Player->Credits;
    for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
    {
        if (Unit.OwnerId != PC->OwnerId || !(Unit.HitPoints > 0.f)) continue;
        ++Result.LiveRecords;
        Result.HasHQ |= Unit.Kind == EProofUnitKind::HQ;
        Result.HasFactory |= Unit.Kind == EProofUnitKind::Factory;
        Result.HasSelectedMobile |= (Unit.Kind == EProofUnitKind::Combat || Unit.Kind == EProofUnitKind::Harvester)
            && Selection.GetSelectedUnitIds().Contains(Unit.UnitId);
    }
    return Result;
}

FString AProofRTSPawn::GetCommandCardHint(ProofCommandCard::Action Command) const
{
    using namespace ProofCommandCard;
    switch (Check(GetCommandCardContext(), Command))
    {
    case Availability::NeedHQ: return TEXT("Requires a live HQ.");
    case Availability::NeedFactory: return TEXT("Requires a live factory.");
    case Availability::NeedMobile: return TEXT("Select mobile units for Stop.");
    case Availability::NeedCredits: return TEXT("Insufficient credits.");
    case Availability::AtCapacity: return TEXT("Unit capacity reached.");
    case Availability::Ready:
        if (Command == Action::Stop) return TEXT("Stop selected mobile units.");
        if (Command == Action::ProduceCombat) return TEXT("Uses selected or first owned factory.");
        return TEXT("Click ground; right-click cancels.");
    default: return TEXT("Waiting for a playable match.");
    }
}

bool AProofRTSPawn::GetPendingBuildTargetScreen(FVector2D& OutPoint, float& OutRadius) const
{
    using namespace ProofCommandCard;
    OutPoint = FVector2D::ZeroVector;
    OutRadius = 0.f;
    const Action RawPending = CommandCardInput.PendingAction();
    if ((RawPending != Action::BuildRefinery && RawPending != Action::BuildFactory)
        || bGraphicsWarmup || !SkirmishScenario.IsEmpty()) return false;
    // This process-wide fixture flag is also cached by the controller. Its
    // skirmish observer mode is separate from the generic automation flags.
    static const bool bScriptedObserver = FParse::Param(FCommandLine::Get(), TEXT("ProofSkirmishObserver"));
    if (bScriptedObserver) return false;
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    if (!PC || !PC->IsLocalController() || PC->IsProofAutomation()
        || PC->IsControlsAutomation() || PC->IsControlsObserver() || bControlsCursor) return false;
    const UGameViewportClient* GameViewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    const FViewport* Viewport = GameViewport ? GameViewport->Viewport : nullptr;
    if (!Viewport || !Viewport->HasFocus() || !Viewport->IsForegroundWindow()) return false;

    // Recheck owner, epoch and availability without changing pending input or
    // sending an order from the draw path.
    InputState CurrentInput = CommandCardInput;
    CurrentInput.Synchronize(GetCommandCardContext());
    const Action Pending = CurrentInput.PendingAction();
    if (Pending != Action::BuildRefinery && Pending != Action::BuildFactory) return false;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    const Layout Card(Width, Height);
    if (!Card.Valid) return false;
    FVector Ground;
    FVector2D Screen;
    if (!CursorGroundPoint(Ground) || !FMath::IsFinite(Ground.X) || !FMath::IsFinite(Ground.Y)
        || !FMath::IsFinite(Ground.Z) || FMath::Abs(Ground.X) > FProofCommandModel::WorldExtent
        || FMath::Abs(Ground.Y) > FProofCommandModel::WorldExtent
        || !PC->ProjectWorldLocationToScreen(Ground, Screen) || !Card.PointerValid(Screen.X, Screen.Y)) return false;

    const float Radius = static_cast<float>(12.0 * Card.Scale);
    const double Extent = Radius + Radius / 12.0; // Includes half the line thickness.
    const double Left = Screen.X - Extent, Top = Screen.Y - Extent;
    const double Right = Screen.X + Extent, Bottom = Screen.Y + Extent;
    if (Left < 0.0 || Top < 0.0 || Right >= Width || Bottom >= Height) return false;
    const auto Overlaps = [Left, Top, Right, Bottom](double X, double Y, double W, double H)
    {
        return Left < X + W && Right > X && Top < Y + H && Bottom > Y;
    };
    const FProofHUDPanelLayout Panel(static_cast<float>(Width), true);
    if (Overlaps(Panel.Left, Panel.Top, Panel.Width, Panel.Height)
        || Overlaps(Card.Panel.X, Card.Panel.Y, Card.Panel.Width, Card.Panel.Height)) return false;
    OutPoint = Screen;
    OutRadius = Radius;
    return true;
}

bool AProofRTSPawn::HandleCommandCardClick(bool bRightClick)
{
    using namespace ProofCommandCard;
    const AProofPlayerController* PC = Cast<AProofPlayerController>(GetController());
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    if (!PC || !GS || !GS->Snapshot.bSkirmish) return false;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    const Layout Card(Width, Height);
    FVector2D Point;
    if (!GetPointerScreenPosition(Point)) { CommandCardInput.Cancel(); return false; }
    const FProofHUDPanelLayout Panel(static_cast<float>(Width), true);
    const bool bOtherHUD = FProofPointerBounds::Contains(Point.X, Point.Y,
        Panel.Left, Panel.Top, Panel.Left + Panel.Width, Panel.Top + Panel.Height);
    if (!Card.Valid) { CommandCardInput.Cancel(); return bOtherHUD; }
    if (PC->IsProofAutomation())
    {
        CommandCardInput.Cancel();
        if (!Card.Contains(Point.X, Point.Y) && !bOtherHUD) return false;
        RequestStatus = TEXT("Manual orders are disabled during the scripted automation fixture.");
        return true;
    }
    const bool bHadPlacement = CommandCardInput.PendingAction() != Action::None;
    ObserveViewportFocus();
    if (!bViewportFocused || !bViewportForeground)
    {
        RequestStatus = TEXT("Focus the game window to use the command panel.");
        return bHadPlacement || Card.Contains(Point.X, Point.Y) || bOtherHUD;
    }
    const Intent RequestIntent = bRightClick
        ? CommandCardInput.RightClick(Card, Point.X, Point.Y, bOtherHUD)
        : CommandCardInput.LeftClick(Card, GetCommandCardContext(), Point.X, Point.Y, bOtherHUD);
    switch (RequestIntent.Result)
    {
    case Effect::PassToWorld: return false;
    case Effect::Armed:
        RequestStatus = RequestIntent.Command == Action::BuildRefinery
            ? TEXT("Place refinery: left-click ground near your base; right-click cancels.")
            : TEXT("Place factory: left-click ground near your base; right-click cancels.");
        break;
    case Effect::PlaceBuilding:
    {
        const int32 BeforeSequence = LastRequestedSequence;
        BuildUnderCursor(RequestIntent.Command == Action::BuildRefinery
            ? EProofCommandType::BuildRefinery : EProofCommandType::BuildFactory);
        // Bad ground projection retains placement for another location. A sent
        // command clears it; only the existing host receipt establishes acceptance.
        CommandCardInput.PlacementSubmitted(LastRequestedSequence != BeforeSequence);
        break;
    }
    case Effect::IssueOrder:
        if (RequestIntent.Command == Action::ProduceCombat) Produce();
        else if (RequestIntent.Command == Action::Stop) StopSelected();
        break;
    case Effect::Rejected: RequestStatus = GetCommandCardHint(RequestIntent.Command); break;
    case Effect::UIOnly:
        if (bRightClick && bHadPlacement) RequestStatus = TEXT("Building placement cancelled.");
        break;
    }
    return true;
}
