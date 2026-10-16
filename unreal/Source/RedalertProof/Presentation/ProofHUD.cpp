#include "Presentation/ProofHUD.h"
#include "Presentation/ProofRTSPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"

void AProofHUD::DrawHUD()
{
    Super::DrawHUD();
    AProofPlayerController* PC = Cast<AProofPlayerController>(PlayerOwner);
    const AProofGameState* GS = GetWorld() ? GetWorld()->GetGameState<AProofGameState>() : nullptr;
    const AProofRTSPawn* Pawn = PC ? Cast<AProofRTSPawn>(PC->GetPawn()) : nullptr;
    if (!Canvas || !PC)
    {
        return;
    }

    // Keep the information panel readable while taking only a small, fixed part
    // of the view. Scale its footprint down on smaller windows.
    const float UiScale = FMath::Clamp(Canvas->ClipX / 1280.f, 0.72f, 1.f);
    const float Left = 12.f * UiScale;
    const float Top = 12.f * UiScale;
    const float PanelWidth = FMath::Min(720.f * UiScale, FMath::Max(0.f, Canvas->ClipX - Left * 2.f));
    const bool bSkirmish = GS && GS->Snapshot.bSkirmish;
    const float PanelHeight = (bSkirmish ? 178.f : 164.f) * UiScale;
    const float Pad = 12.f * UiScale;
    const float FontScale = 1.05f * UiScale;
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

    DrawRect(FLinearColor(0.015f, 0.025f, 0.045f, 0.88f), Left, Top, PanelWidth, PanelHeight);

    int32 Credits = 0;
    int32 TotalGathered = 0;
    int32 LocalTeam = 0;
    const FProofUnitRecord* SelectedUnit = nullptr;
    if (GS)
    {
        for (const FProofPlayerRecord& Player : GS->Snapshot.Players)
        {
            if (Player.OwnerId == PC->OwnerId)
            {
                Credits = Player.Credits;
                TotalGathered = Player.TotalGathered;
                LocalTeam = Player.TeamId;
                break;
            }
        }
        if (Pawn)
        {
            SelectedUnit = GS->Snapshot.Units.FindByPredicate([Pawn](const FProofUnitRecord& Unit)
            {
                return Unit.UnitId == Pawn->GetSelectedUnitId() && Unit.HitPoints > 0.f;
            });
        }
    }

    const float TextX = Left + Pad;
    const float Row = 19.f * UiScale;
    const float MaxTextWidth = FMath::Max(0.f, PanelWidth - Pad * 2.f);
    const auto FitToWidth = [this, Font, FontScale, MaxTextWidth](const FString& Text)
    {
        float TextWidth = 0.f;
        float TextHeight = 0.f;
        GetTextSize(Text, TextWidth, TextHeight, Font, FontScale);
        if (TextWidth <= MaxTextWidth)
        {
            return Text;
        }

        FString Fitted = Text;
        const FString Ellipsis = TEXT("…");
        while (!Fitted.IsEmpty())
        {
            Fitted.LeftChopInline(1, EAllowShrinking::No);
            const FString Candidate = Fitted + Ellipsis;
            GetTextSize(Candidate, TextWidth, TextHeight, Font, FontScale);
            if (TextWidth <= MaxTextWidth)
            {
                return Candidate;
            }
        }
        return FString();
    };
    const auto DrawHudLine = [this, Font, FontScale, TextX, FitToWidth](const FString& Text, const FLinearColor& Color, float Y)
    {
        DrawText(FitToWidth(Text), Color, TextX, Y, Font, FontScale, false);
    };

    if (bSkirmish)
    {
        FString SelectedDescription = TEXT("none");
        if (SelectedUnit)
        {
            const TCHAR* KindName = TEXT("Combat");
            switch (SelectedUnit->Kind)
            {
            case EProofUnitKind::Harvester: KindName = TEXT("Harvester"); break;
            case EProofUnitKind::HQ: KindName = TEXT("HQ"); break;
            case EProofUnitKind::Refinery: KindName = TEXT("Refinery"); break;
            case EProofUnitKind::Factory: KindName = TEXT("Factory"); break;
            default: break;
            }
            SelectedDescription = FString::Printf(TEXT("#%d %s  Cargo %d"), SelectedUnit->UnitId, KindName, SelectedUnit->Cargo);
        }
        DrawHudLine(TEXT("SMALL SKIRMISH  |  Destroy the opposing team's live HQ"), FLinearColor::White, Top + Pad);
        DrawHudLine(TEXT("Select: left-click   Order/gather: right-click   Zoom: wheel   Exit: Esc"),
            FLinearColor(0.85f, 0.9f, 1.f), Top + Pad + Row);
        DrawHudLine(FString::Printf(TEXT("Credits: %d   Delivered ore: %d   Selected: %s"), Credits, TotalGathered, *SelectedDescription),
            FLinearColor(0.8f, 1.f, 0.8f), Top + Pad + Row * 2.f);
        DrawHudLine(Pawn ? Pawn->GetRequestStatus() : TEXT("Waiting for camera pawn."),
            FLinearColor(1.f, 0.88f, 0.45f), Top + Pad + Row * 3.f);
        DrawHudLine(Pawn ? Pawn->GetManualInputStatus() : TEXT("Manual input unavailable."),
            FLinearColor(0.85f, 0.78f, 1.f), Top + Pad + Row * 4.f);
        DrawHudLine(PC->GetReceiptStatus(), FLinearColor(0.65f, 0.88f, 1.f), Top + Pad + Row * 5.f);
        DrawHudLine(TEXT("Build: R refinery 100   F factory 150   P combat unit 50   Ore markers show remaining resources"),
            FLinearColor(0.72f, 0.88f, 0.96f), Top + Pad + Row * 6.f);
    }
    else
    {
        DrawHudLine(TEXT("COMMAND PROOF  |  Generic blockout units"), FLinearColor::White, Top + Pad);
        const FString Controls = Pawn && Pawn->IsInteractiveSession()
            ? TEXT("Select: left-click   Order: right-click   Produce: P   Zoom: wheel   Exit: Esc")
            : TEXT("Select: left-click own unit   Order: right-click   Produce: P   Zoom: mouse wheel");
        DrawHudLine(Controls,
            FLinearColor(0.85f, 0.9f, 1.f), Top + Pad + Row);
        DrawHudLine(FString::Printf(TEXT("Credits: %d   Selected unit: %s"), Credits,
            Pawn && Pawn->GetSelectedUnitId() != INDEX_NONE
                ? *FString::FromInt(Pawn->GetSelectedUnitId()) : TEXT("none")),
            FLinearColor(0.8f, 1.f, 0.8f), Top + Pad + Row * 2.f);
        DrawHudLine(Pawn ? Pawn->GetRequestStatus() : TEXT("Waiting for camera pawn."),
            FLinearColor(1.f, 0.88f, 0.45f), Top + Pad + Row * 3.f);
        DrawHudLine(Pawn ? Pawn->GetManualInputStatus() : TEXT("Manual input unavailable."),
            FLinearColor(0.85f, 0.78f, 1.f), Top + Pad + Row * 4.f);
        DrawHudLine(PC->GetReceiptStatus(), FLinearColor(0.65f, 0.88f, 1.f), Top + Pad + Row * 5.f);
        DrawHudLine(Pawn && Pawn->IsUsingBlockoutMesh()
            ? TEXT("Functional proof only; generic proxies, no approved art or final scale.")
            : TEXT("Functional proof only; engine cubes are generic proxies, no approved art."),
            FLinearColor(0.72f, 0.77f, 0.84f), Top + Pad + Row * 6.f);
    }

    if (!GS)
    {
        return;
    }
    if (bSkirmish)
    {
        for (const FProofResourceRecord& Resource : GS->Snapshot.Resources)
        {
            if (Resource.Remaining <= 0)
            {
                continue;
            }
            FVector2D Screen;
            if (!PC->ProjectWorldLocationToScreen(Resource.Position, Screen))
            {
                continue;
            }
            const FLinearColor MarkerColor(0.25f, 0.88f, 1.f);
            DrawLine(Screen.X - 7.f, Screen.Y, Screen.X + 7.f, Screen.Y, MarkerColor, 2.f);
            DrawLine(Screen.X, Screen.Y - 7.f, Screen.X, Screen.Y + 7.f, MarkerColor, 2.f);
            const float ResourceLabelScale = 0.86f * UiScale;
            const float ResourceLabelY = FMath::Clamp(Screen.Y + 9.f, PanelHeight + Top + 2.f,
                FMath::Max(PanelHeight + Top + 2.f, Canvas->ClipY - 34.f));
            DrawText(FString::Printf(TEXT("ORE #%d"), Resource.ResourceId), MarkerColor,
                FMath::Clamp(Screen.X, 0.f, Canvas->ClipX), ResourceLabelY, Font, ResourceLabelScale, true);
            DrawText(FString::Printf(TEXT("%d"), Resource.Remaining), MarkerColor,
                FMath::Clamp(Screen.X, 0.f, Canvas->ClipX), ResourceLabelY + 13.f * UiScale, Font, ResourceLabelScale, true);
        }
    }
    for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
    {
        if (Unit.HitPoints <= 0.f)
        {
            continue;
        }
        FVector2D Screen;
        if (!PC->ProjectWorldLocationToScreen(Unit.Position, Screen))
        {
            continue;
        }
        const bool bOwn = Unit.OwnerId == PC->OwnerId;
        const bool bSelected = bOwn && Pawn && Unit.UnitId == Pawn->GetSelectedUnitId();
        const FProofPlayerRecord* UnitOwner = bSkirmish ? GS->Snapshot.Players.FindByPredicate([&Unit](const FProofPlayerRecord& Player)
        {
            return Player.OwnerId == Unit.OwnerId;
        }) : nullptr;
        const bool bFriendlyTeam = bSkirmish && UnitOwner && UnitOwner->TeamId == LocalTeam;
        const FLinearColor Color = bSelected ? FLinearColor::Yellow : (bSkirmish
            ? (bFriendlyTeam ? FLinearColor(0.18f, 0.95f, 0.55f) : FLinearColor(1.f, 0.32f, 0.25f))
            : (bOwn ? FLinearColor::Green : FLinearColor(1.f, 0.4f, 0.3f)));
        const float Radius = bSelected ? 18.f : 10.f;
        // Screen-space brackets make ownership legible without changing art.
        DrawLine(Screen.X - Radius, Screen.Y - Radius, Screen.X + Radius, Screen.Y - Radius, Color, 2.f);
        DrawLine(Screen.X - Radius, Screen.Y - Radius, Screen.X - Radius, Screen.Y + Radius, Color, 2.f);
        DrawLine(Screen.X + Radius, Screen.Y - Radius, Screen.X + Radius, Screen.Y + Radius, Color, 2.f);
        DrawLine(Screen.X - Radius, Screen.Y + Radius, Screen.X + Radius, Screen.Y + Radius, Color, 2.f);

        FString Label;
        if (bSkirmish)
        {
            const TCHAR* KindName = TEXT("C");
            switch (Unit.Kind)
            {
            case EProofUnitKind::Harvester: KindName = TEXT("H"); break;
            case EProofUnitKind::HQ: KindName = TEXT("HQ"); break;
            case EProofUnitKind::Refinery: KindName = TEXT("R"); break;
            case EProofUnitKind::Factory: KindName = TEXT("F"); break;
            default: break;
            }
            Label = FString::Printf(TEXT("%s#%d"), KindName, Unit.UnitId);

            const float LabelScale = 0.88f * UiScale;
            const float LabelY = FMath::Clamp(Screen.Y - Radius - 13.f * UiScale,
                PanelHeight + Top + 2.f, FMath::Max(PanelHeight + Top + 2.f, Canvas->ClipY - 28.f));
            DrawText(Label, Color, FMath::Clamp(Screen.X, 0.f, Canvas->ClipX), LabelY, Font, LabelScale, true);

            float MaxHP = 100.f;
            if (Unit.Kind == EProofUnitKind::HQ) MaxHP = 300.f;
            else if (Unit.Kind == EProofUnitKind::Refinery || Unit.Kind == EProofUnitKind::Factory) MaxHP = 200.f;
            const float BarWidth = 24.f * UiScale;
            const float BarHeight = 3.f * UiScale;
            const float BarX = FMath::Clamp(Screen.X - BarWidth * 0.5f, 0.f, FMath::Max(0.f, Canvas->ClipX - BarWidth));
            const float BarY = FMath::Clamp(Screen.Y + Radius + 2.f * UiScale,
                PanelHeight + Top + 2.f, FMath::Max(PanelHeight + Top + 2.f, Canvas->ClipY - BarHeight));
            DrawRect(FLinearColor(0.03f, 0.04f, 0.05f, 0.9f), BarX, BarY, BarWidth, BarHeight);
            DrawRect(Color, BarX, BarY, BarWidth * FMath::Clamp(Unit.HitPoints / MaxHP, 0.f, 1.f), BarHeight);
            continue;
        }
        else
        {
            Label = FString::Printf(TEXT("#%d  HP %.0f"), Unit.UnitId, Unit.HitPoints);
        }
        const float MaxLabelX = FMath::Max(0.f, Canvas->ClipX - 4.f);
        const float LabelX = FMath::Clamp(Screen.X + Radius + 4.f, 0.f, MaxLabelX);
        const float MaxLabelY = FMath::Max(0.f, Canvas->ClipY - 24.f);
        const float MinLabelY = FMath::Min(PanelHeight + Top + 2.f, MaxLabelY);
        const float LabelY = FMath::Clamp(Screen.Y - 10.f, MinLabelY, MaxLabelY);
        float LabelWidth = 0.f;
        float LabelHeight = 0.f;
        GetTextSize(Label, LabelWidth, LabelHeight, Font, FontScale);
        const float AvailableLabelWidth = FMath::Max(0.f, Canvas->ClipX - LabelX - 4.f);
        if (LabelWidth > AvailableLabelWidth)
        {
            FString FittedLabel = Label;
            while (!FittedLabel.IsEmpty())
            {
                FittedLabel.LeftChopInline(1, EAllowShrinking::No);
                const FString Candidate = FittedLabel + TEXT("…");
                GetTextSize(Candidate, LabelWidth, LabelHeight, Font, FontScale);
                if (LabelWidth <= AvailableLabelWidth)
                {
                    DrawText(Candidate, Color, LabelX, LabelY, Font, FontScale, false);
                    break;
                }
            }
        }
        else
        {
            DrawText(Label, Color, LabelX, LabelY, Font, FontScale, false);
        }
    }

    if (bSkirmish && GS->Snapshot.MatchPhase == EProofMatchPhase::Finished)
    {
        const FString Result = GS->Snapshot.WinnerTeam == INDEX_NONE
            ? TEXT("DRAW") : (GS->Snapshot.WinnerTeam == LocalTeam ? TEXT("VICTORY") : TEXT("DEFEAT"));
        float ResultWidth = 0.f;
        float ResultHeight = 0.f;
        GetTextSize(Result, ResultWidth, ResultHeight, Font, 2.f * UiScale);
        const float BannerWidth = ResultWidth + 42.f * UiScale;
        const float BannerHeight = ResultHeight + 24.f * UiScale;
        const float BannerX = FMath::Max(0.f, (Canvas->ClipX - BannerWidth) * 0.5f);
        const float BannerY = FMath::Max(PanelHeight + Top + 12.f * UiScale, Canvas->ClipY * 0.16f);
        DrawRect(FLinearColor(0.01f, 0.02f, 0.04f, 0.86f), BannerX, BannerY, BannerWidth, BannerHeight);
        DrawText(Result, Result == TEXT("VICTORY") ? FLinearColor(0.25f, 1.f, 0.5f) :
            (Result == TEXT("DEFEAT") ? FLinearColor(1.f, 0.35f, 0.3f) : FLinearColor(0.85f, 0.9f, 1.f)),
            BannerX + 21.f * UiScale, BannerY + 12.f * UiScale, Font, 2.f * UiScale, false);
    }
}
