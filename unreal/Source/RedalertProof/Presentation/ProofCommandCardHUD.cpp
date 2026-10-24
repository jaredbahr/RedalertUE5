#include "Presentation/ProofHUD.h"
#include "Presentation/ProofRTSPawn.h"
#include "Commands/ProofCommandModel.h"
#include "Runtime/ProofPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void AProofHUD::DrawCommandCard(const AProofRTSPawn* Pawn, AProofPlayerController* PC)
{
    using namespace ProofCommandCard;
    if (!Canvas || !PC) return;
    const Layout Card(Canvas->ClipX, Canvas->ClipY);
    if (!Card.Valid) return;
    const float Scale = static_cast<float>(Card.Scale);
    const float Pad = 6.f * Scale;
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
    const Context State = Pawn ? Pawn->GetCommandCardContext() : Context();
    const Action Pending = Pawn ? Pawn->GetPendingBuildAction() : Action::None;
    float MouseX = -1.f, MouseY = -1.f;
    const bool bPointer = PC->GetMousePosition(MouseX, MouseY);
    const Action Hovered = bPointer ? Card.HitTest(MouseX, MouseY) : Action::None;
    const FLinearColor Accent(0.92f, 0.72f, 0.30f);
    const auto Fill = [this](const Rect& Bounds, const FLinearColor& Color)
    {
        DrawRect(Color, static_cast<float>(Bounds.X), static_cast<float>(Bounds.Y),
            static_cast<float>(Bounds.Width), static_cast<float>(Bounds.Height));
    };
    // Keys and prices are drawn whole or explicitly replaced with an overflow
    // indicator. Real font/viewport inspection remains a separate acceptance gate.
    const auto Text = [this, Font, Scale](const FString& Value, double X, double Y,
        double Width, float FontScale, const FLinearColor& Color)
    {
        float MeasuredWidth = 0.f, MeasuredHeight = 0.f;
        GetTextSize(Value, MeasuredWidth, MeasuredHeight, Font, FontScale);
        if (Font && FMath::IsFinite(MeasuredWidth) && MeasuredWidth > 0.f && MeasuredWidth <= Width
            && FMath::IsFinite(MeasuredHeight) && MeasuredHeight > 0.f && MeasuredHeight <= 17.f * Scale)
            DrawText(Value, Color, static_cast<float>(X), static_cast<float>(Y), Font, FontScale, false);
        else
        {
            const FString Overflow = TEXT("UI overflow");
            GetTextSize(Overflow, MeasuredWidth, MeasuredHeight, Font, FontScale);
            if (Font && FMath::IsFinite(MeasuredWidth) && MeasuredWidth > 0.f && MeasuredWidth <= Width
                && FMath::IsFinite(MeasuredHeight) && MeasuredHeight > 0.f && MeasuredHeight <= 17.f * Scale)
                DrawText(Overflow, FLinearColor(1.f, 0.4f, 0.3f), static_cast<float>(X), static_cast<float>(Y), Font, FontScale, false);
        }
    };
    Fill(Card.Panel, FLinearColor(0.018f, 0.025f, 0.034f, 0.97f));
    DrawRect(Accent, static_cast<float>(Card.Panel.X), static_cast<float>(Card.Panel.Y),
        static_cast<float>(Card.Panel.Width), 2.f * Scale);
    const FString Heading = Pending == Action::BuildRefinery ? TEXT("PLACE REFINERY")
        : Pending == Action::BuildFactory ? TEXT("PLACE FACTORY") : TEXT("COMMANDS");
    Text(Heading, Card.Panel.X + 12.0 * Scale, Card.Panel.Y + 10.0 * Scale,
        Card.Panel.Width - 24.0 * Scale, 0.95f * Scale, Accent);
    const TCHAR* Labels[] = {TEXT("REFINERY [R]"), TEXT("FACTORY [F]"), TEXT("COMBAT [P]"), TEXT("STOP [S]")};
    const int32 Costs[] = {FProofCommandModel::RefineryCost, FProofCommandModel::FactoryCost, FProofCommandModel::ProductionCost};
    for (std::size_t Index = 0; Index < Card.Buttons.size(); ++Index)
    {
        const Action Command = Actions[Index];
        const bool bEnabled = Check(State, Command) == Availability::Ready;
        const bool bHovered = Hovered == Command;
        const bool bArmed = Pending == Command;
        const Rect& Button = Card.Buttons[Index];
        Fill(Button, bArmed ? Accent : bHovered && bEnabled ? FLinearColor(0.45f, 0.55f, 0.62f)
            : FLinearColor(0.17f, 0.23f, 0.28f));
        const double Inset = Scale;
        Fill({Button.X + Inset, Button.Y + Inset, Button.Width - Inset * 2.0, Button.Height - Inset * 2.0},
            bHovered && bEnabled ? FLinearColor(0.10f, 0.16f, 0.21f) : FLinearColor(0.045f, 0.065f, 0.08f));
        Text(Labels[Index], Button.X + Pad, Button.Y + 6.0 * Scale, Button.Width - Pad * 2.0,
            0.9f * Scale, bEnabled ? FLinearColor(0.90f, 0.94f, 0.96f) : FLinearColor(0.48f, 0.54f, 0.58f));
        const FString Detail = Index < 3 ? FString::Printf(TEXT("%d credits"), Costs[Index]) : TEXT("Mobile units");
        const bool bAffordable = Index >= 3 || State.Credits >= Costs[Index];
        Text(Detail, Button.X + Pad, Button.Y + 27.0 * Scale, Button.Width - Pad * 2.0,
            0.8f * Scale, bAffordable ? FLinearColor(0.66f, 0.78f, 0.78f) : FLinearColor(0.96f, 0.48f, 0.38f));
    }
    const FString Hint = Pending != Action::None ? TEXT("Left-click ground; right-click cancels.")
        : Pawn && Hovered != Action::None ? Pawn->GetCommandCardHint(Hovered) : TEXT("Hover an action for details.");
    Text(Hint, Card.Panel.X + 12.0 * Scale, Card.Panel.Y + 155.0 * Scale,
        Card.Panel.Width - 24.0 * Scale, 0.72f * Scale, FLinearColor(0.72f, 0.81f, 0.86f));
}
