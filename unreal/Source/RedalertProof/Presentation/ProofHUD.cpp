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
    const float PanelHeight = 164.f * UiScale;
    const float Pad = 12.f * UiScale;
    const float FontScale = 1.05f * UiScale;
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

    DrawRect(FLinearColor(0.015f, 0.025f, 0.045f, 0.88f), Left, Top, PanelWidth, PanelHeight);

    int32 Credits = 0;
    if (GS)
    {
        for (const FProofPlayerRecord& Player : GS->Snapshot.Players)
        {
            if (Player.OwnerId == PC->OwnerId)
            {
                Credits = Player.Credits;
                break;
            }
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

    if (!GS)
    {
        return;
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
        const FLinearColor Color = bSelected ? FLinearColor::Yellow : (bOwn ? FLinearColor::Green : FLinearColor(1.f, 0.4f, 0.3f));
        const float Radius = bSelected ? 18.f : 10.f;
        // Screen-space brackets make ownership legible without changing art.
        DrawLine(Screen.X - Radius, Screen.Y - Radius, Screen.X + Radius, Screen.Y - Radius, Color, 2.f);
        DrawLine(Screen.X - Radius, Screen.Y - Radius, Screen.X - Radius, Screen.Y + Radius, Color, 2.f);
        DrawLine(Screen.X + Radius, Screen.Y - Radius, Screen.X + Radius, Screen.Y + Radius, Color, 2.f);
        DrawLine(Screen.X - Radius, Screen.Y + Radius, Screen.X + Radius, Screen.Y + Radius, Color, 2.f);

        const FString Label = FString::Printf(TEXT("#%d  HP %.0f"), Unit.UnitId, Unit.HitPoints);
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
}
