#include "Presentation/ProofHUD.h"
#include "Presentation/ProofHUDPanelLayout.h"
#include "Presentation/ProofHUDStatusText.h"
#include "Presentation/ProofHUDTextChoice.h"
#include "Presentation/ProofMarkerLayout.h"
#include "Presentation/ProofLegacyLabelLayout.h"
#include "Presentation/ProofRTSPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"
#include "UnrealClient.h"

#include <limits>

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
    const bool bSkirmish = GS && GS->Snapshot.bSkirmish;
    const FProofHUDPanelLayout Panel(Canvas->ClipX, bSkirmish);
    const ProofCommandCard::Layout CommandCard(Canvas->ClipX, Canvas->ClipY);
    const float UiScale = Panel.Scale;
    const float Left = Panel.Left;
    const float Top = Panel.Top;
    const float PanelWidth = Panel.Width;
    // Observe only a requested UI screenshot in the explicit fixture mode.
    // This is a layout record for this DrawHUD, not proof that PNG encoding succeeded.
    const bool bCaptureLayout = bSkirmish
        && FParse::Param(FCommandLine::Get(), TEXT("ProofHUDReadabilityCaptures"))
        && FScreenshotRequest::IsScreenshotRequested() && FScreenshotRequest::ShouldShowUI()
        && !LoggedHUDLayoutCaptures.Contains(FScreenshotRequest::GetFilename());
    const FString CaptureFilename = bCaptureLayout ? FScreenshotRequest::GetFilename() : FString();
    struct FLayoutCaptureStats
    {
        int32 LiveUnits = 0;
        int32 VisibleUnitGlyphs = 0;
        int32 OffscreenUnitGlyphs = 0;
        int32 OccludedUnitGlyphs = 0;
        int32 PlacedUnitGroups = 0;
        int32 OmittedUnitGroups = 0;
        int32 VisibleResourceGlyphs = 0;
        int32 PlacedResourceGroups = 0;
        int32 OmittedResourceGroups = 0;
        bool bPrimaryGlyphVisible = false;
        bool bPrimaryGroupPlaced = false;
    } CaptureStats;
    const float PanelHeight = Panel.Height;
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

    // Critical text is selected as a complete measured variant, never ellipsized.
    // The dependency-free selector is owned by the coordinated cloud lane.
    const auto DrawCriticalHudLine = [this, Font, FontScale, TextX, MaxTextWidth,
        Row, Top, PanelHeight, Pad, bCaptureLayout, &CaptureFilename](
        const TCHAR* Group, const TArray<FString>& Variants, const FLinearColor& Color, float Y)
    {
        TArray<double> Widths;
        TArray<float> Heights;
        const float MaxHeight = FMath::Min(Row, Top + PanelHeight - Pad - Y);
        for (const FString& Variant : Variants)
        {
            float Width = 0.f, Height = 0.f;
            GetTextSize(Variant, Width, Height, Font, FontScale);
            const bool bMeasured = Font && !Variant.IsEmpty() && FMath::IsFinite(Width) && Width > 0.f
                && FMath::IsFinite(Height) && Height > 0.f && Height <= MaxHeight;
            Widths.Add(bMeasured ? static_cast<double>(Width) : std::numeric_limits<double>::quiet_NaN());
            Heights.Add(Height);
        }
        const std::size_t Choice = ProofHUDTextChoice::ChooseFirstFitting(
            Widths.GetData(), static_cast<std::size_t>(Widths.Num()), static_cast<double>(MaxTextWidth));
        const bool bComplete = Choice != ProofHUDTextChoice::NoChoice
            && Choice < static_cast<std::size_t>(Variants.Num());
        const int32 Index = bComplete ? static_cast<int32>(Choice) : INDEX_NONE;
        if (bComplete)
        {
            DrawText(Variants[Index], Color, TextX, Y, Font, FontScale, false);
        }
        else
        {
            const FString Overflow = TEXT("Status overflow");
            float Width = 0.f, Height = 0.f;
            GetTextSize(Overflow, Width, Height, Font, FontScale);
            if (Font && FMath::IsFinite(Width) && Width > 0.f && Width <= MaxTextWidth
                && FMath::IsFinite(Height) && Height > 0.f && Height <= MaxHeight)
                DrawText(Overflow, FLinearColor(1.f, 0.45f, 0.3f), TextX, Y, Font, FontScale, false);
        }
        if (bCaptureLayout)
        {
            UE_LOG(LogTemp, Display, TEXT("PROOF_HUD_CRITICAL_CAPTURE name=%s group=%s variant=%d complete=%d width=%.9g available=%.9g height=%.9g maxheight=%.9g"),
                *FPaths::GetCleanFilename(CaptureFilename), Group, Index, bComplete ? 1 : 0,
                bComplete ? Widths[Index] : -1.0, static_cast<double>(MaxTextWidth),
                bComplete ? static_cast<double>(Heights[Index]) : -1.0, static_cast<double>(MaxHeight));
        }
    };

    if (bSkirmish)
    {
        DrawHudLine(TEXT("SMALL SKIRMISH  |  Destroy the opposing team's live HQ"), FLinearColor::White, Top + Pad);
        DrawHudLine(TEXT("Select: left-click   Order/gather: right-click   Zoom: wheel   Exit: Esc"),
            FLinearColor(0.85f, 0.9f, 1.f), Top + Pad + Row);
        DrawCriticalHudLine(TEXT("economy"), ProofHUDStatusText::Economy(Credits, TotalGathered),
            FLinearColor(0.8f, 1.f, 0.8f), Top + Pad + Row * 2.f);
        DrawHudLine(Pawn ? Pawn->GetRequestStatus() : TEXT("Waiting for camera pawn."),
            FLinearColor(1.f, 0.88f, 0.45f), Top + Pad + Row * 3.f);
        DrawHudLine(Pawn ? Pawn->GetManualInputStatus() : TEXT("Manual input unavailable."),
            FLinearColor(0.85f, 0.78f, 1.f), Top + Pad + Row * 4.f);
        DrawHudLine(PC->GetReceiptStatus(), FLinearColor(0.65f, 0.88f, 1.f), Top + Pad + Row * 5.f);
        DrawHudLine(FString::Printf(TEXT("R refinery %d | F factory %d (at cursor) | P combat %d"),
            FProofCommandModel::RefineryCost, FProofCommandModel::FactoryCost, FProofCommandModel::ProductionCost),
            FLinearColor(0.72f, 0.88f, 0.96f), Top + Pad + Row * 6.f);
        // Full/compact variants retain primary identity, role, health, cargo,
        // group count and factory rally even when its world label is occluded.
        DrawCriticalHudLine(TEXT("selection"), ProofHUDStatusText::Selection(SelectedUnit,
            Pawn ? Pawn->GetSelectedUnitIds().Num() : 0),
            FLinearColor(0.8f, 1.f, 0.8f), Top + Pad + Row * 8.f);
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
        // Primary ID and health stay inspectable when its world label cannot fit.
        DrawHudLine(SelectedUnit ? FString::Printf(TEXT("Primary #%d  HP %.0f"),
            SelectedUnit->UnitId, SelectedUnit->HitPoints) : TEXT("Primary: none"),
            FLinearColor(0.8f, 1.f, 0.8f), Top + Pad + Row * 8.f);
    }

    DrawHudLine(Pawn && Pawn->IsCameraPanEnabled()
        ? TEXT("S: stop   T: type   Shift-click: toggle   Ctrl+1-9: save   1-9: recall   Arrows: pan")
        : TEXT("S: stop   T: type   Shift-click: toggle   Ctrl+1-9: save   1-9: recall"),
        FLinearColor(0.72f, 0.88f, 0.96f), Top + Pad + Row * 7.f);

    if (!GS)
    {
        return;
    }
    if (bSkirmish && CommandCard.Valid) DrawCommandCard(Pawn, PC);
    FBox2D TerminalHintBounds(ForceInit);
    FString TerminalHint;
    const float HintPadding = 5.f * UiScale;
    if (bSkirmish)
    {
        FProofMarkerLayout Layout(FVector2D(Canvas->ClipX, Canvas->ClipY),
            4.f * UiScale, 2.f * UiScale, 64.f * UiScale);
        TArray<FBox2D> Occluders;
        const auto Occlude = [&Layout, &Occluders](const FBox2D& Bounds)
        {
            Occluders.Add(Bounds);
            Layout.Reserve(Bounds);
        };
        Occlude(FBox2D(FVector2D(Left, Top), FVector2D(Left + PanelWidth, Top + PanelHeight)));
        if (CommandCard.Valid)
        {
            const ProofCommandCard::Rect& Card = CommandCard.Panel;
            Occlude(FBox2D(FVector2D(Card.X, Card.Y), FVector2D(Card.X + Card.Width, Card.Y + Card.Height)));
        }

        // Reserve both the terminal banner and its measured rematch hint before
        // considering world markers. The same geometry is used below to draw it.
        if (GS->Snapshot.MatchPhase == EProofMatchPhase::Finished)
        {
            const FString Result = GS->Snapshot.WinnerTeam == INDEX_NONE
                ? TEXT("DRAW") : (GS->Snapshot.WinnerTeam == LocalTeam ? TEXT("VICTORY") : TEXT("DEFEAT"));
            float ResultWidth = 0.f, ResultHeight = 0.f;
            GetTextSize(Result, ResultWidth, ResultHeight, Font, 2.f * UiScale);
            const float BannerWidth = ResultWidth + 42.f * UiScale;
            const float BannerHeight = ResultHeight + 24.f * UiScale;
            const float BannerX = FMath::Max(0.f, (Canvas->ClipX - BannerWidth) * 0.5f);
            const float BannerY = FMath::Max(PanelHeight + Top + 12.f * UiScale, Canvas->ClipY * 0.16f);
            TerminalHint = PC->OwnerId == 1 ? TEXT("N: rematch   Esc: exit") : TEXT("Waiting for owner 1 to request rematch");
            float HintWidth = 0.f, HintHeight = 0.f;
            GetTextSize(TerminalHint, HintWidth, HintHeight, Font, UiScale);
            const FVector2D HintSize(HintWidth + HintPadding * 2.f, HintHeight + HintPadding * 2.f);
            const FVector2D HintPosition(FMath::Clamp(BannerX, 0.f, FMath::Max(0.f, Canvas->ClipX - static_cast<float>(HintSize.X))),
                BannerY + BannerHeight + 8.f * UiScale);
            TerminalHintBounds = FBox2D(HintPosition, HintPosition + HintSize);
            Occlude(FBox2D(FVector2D(BannerX, BannerY), FVector2D(BannerX + BannerWidth, BannerY + BannerHeight)));
            Occlude(TerminalHintBounds);
        }
        // 0 visible, 1 outside viewport, 2 hidden by HUD/banner/hint.
        const auto ClassifyFootprint = [&Layout, &Occluders](const FBox2D& Bounds)
        {
            if (!Layout.IsAnchorVisible(Bounds.Min) || !Layout.IsAnchorVisible(Bounds.Max)) return 1;
            for (const FBox2D& Occluder : Occluders)
                if (Bounds.Intersect(Occluder)) return 2;
            return 0;
        };
        struct FUnitMarker
        {
            const FProofUnitRecord* Unit;
            FVector2D Screen;
            FLinearColor Color;
            float Radius;
            int32 Priority;
            int32 SourceReservation;
        };
        struct FResourceMarker
        {
            const FProofResourceRecord* Resource;
            FVector2D Screen;
            int32 SourceReservation;
        };
        TArray<FUnitMarker> Units;
        TArray<FResourceMarker> Resources;
        for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
        {
            if (Unit.HitPoints <= 0.f) continue;
            if (bCaptureLayout) ++CaptureStats.LiveUnits;
            FVector2D Screen;
            if (!PC->ProjectWorldLocationToScreen(Unit.Position, Screen))
            {
                if (bCaptureLayout) ++CaptureStats.OffscreenUnitGlyphs;
                continue;
            }
            const bool bOwn = Unit.OwnerId == PC->OwnerId;
            const bool bSelected = bOwn && Pawn && Pawn->GetSelectedUnitIds().Contains(Unit.UnitId);
            const bool bPrimary = bSelected && Unit.UnitId == Pawn->GetSelectedUnitId();
            const FProofPlayerRecord* UnitOwnerRecord = GS->Snapshot.Players.FindByPredicate([&Unit](const FProofPlayerRecord& Player)
            {
                return Player.OwnerId == Unit.OwnerId;
            });
            const FLinearColor Color = bSelected ? FLinearColor::Yellow :
                (UnitOwnerRecord && UnitOwnerRecord->TeamId == LocalTeam ? FLinearColor(0.18f, 0.95f, 0.55f) : FLinearColor(1.f, 0.32f, 0.25f));
            const float Radius = bSelected ? 18.f : 10.f;
            // Include line thickness; partially offscreen/occluded anchors are
            // hidden instead of being clamped into an unrelated border pile.
            const FBox2D Footprint(Screen - FVector2D(Radius + 1.f, Radius + 1.f),
                Screen + FVector2D(Radius + 1.f, Radius + 1.f));
            const int32 Visibility = ClassifyFootprint(Footprint);
            if (Visibility != 0)
            {
                if (bCaptureLayout)
                {
                    if (Visibility == 1) ++CaptureStats.OffscreenUnitGlyphs;
                    else ++CaptureStats.OccludedUnitGlyphs;
                }
                continue;
            }
            if (bCaptureLayout)
            {
                ++CaptureStats.VisibleUnitGlyphs;
                CaptureStats.bPrimaryGlyphVisible |= bPrimary;
            }
            Units.Add({&Unit, Screen, Color, Radius, bPrimary ? 0 : (bSelected ? 1 : 2), Layout.Reserve(Footprint)});
        }
        for (const FProofResourceRecord& Resource : GS->Snapshot.Resources)
        {
            FVector2D Screen;
            if (!PC->ProjectWorldLocationToScreen(Resource.Position, Screen)) continue;
            const FBox2D Footprint(Screen - FVector2D(8.f, 8.f), Screen + FVector2D(8.f, 8.f));
            if (ClassifyFootprint(Footprint) != 0) continue;
            if (bCaptureLayout) ++CaptureStats.VisibleResourceGlyphs;
            Resources.Add({&Resource, Screen, Layout.Reserve(Footprint)});
        }
        Units.Sort([](const FUnitMarker& A, const FUnitMarker& B)
        {
            return A.Priority != B.Priority ? A.Priority < B.Priority : A.Unit->UnitId < B.Unit->UnitId;
        });
        Resources.Sort([](const FResourceMarker& A, const FResourceMarker& B)
        {
            return A.Resource->ResourceId < B.Resource->ResourceId;
        });
        const auto DrawUnitGroup = [this, &Layout, &CaptureStats, bCaptureLayout, Font, UiScale](const FUnitMarker& Marker)
        {
            const FProofUnitRecord& Unit = *Marker.Unit;
            const float Scale = 0.88f * UiScale;
            const float Padding = 3.f * UiScale;
            const float BarWidth = 24.f * UiScale;
            const float BarHeight = 3.f * UiScale;
            FString Label;
            float TextWidth = 0.f, TextHeight = 0.f;
            if (Marker.Priority == 0)
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
                GetTextSize(Label, TextWidth, TextHeight, Font, Scale);
            }
            const FVector2D Size(FMath::Max(TextWidth, BarWidth) + Padding * 2.f,
                TextHeight + (Label.IsEmpty() ? 0.f : Padding) + BarHeight + Padding * 2.f);
            const FVector2D Preferred(Marker.Screen.X - Size.X * 0.5f,
                Marker.Priority == 0 ? Marker.Screen.Y - Marker.Radius - 4.f - Size.Y : Marker.Screen.Y + Marker.Radius + 4.f);
            FBox2D Bounds;
            FVector2D LeaderStart, LeaderEnd;
            if (!Layout.TryPlaceLinked(Preferred, Size, Marker.SourceReservation, Bounds, LeaderStart, LeaderEnd))
            {
                if (bCaptureLayout) ++CaptureStats.OmittedUnitGroups;
                return;
            }
            if (bCaptureLayout)
            {
                ++CaptureStats.PlacedUnitGroups;
                CaptureStats.bPrimaryGroupPlaced |= Marker.Priority == 0;
            }
            DrawLine(LeaderStart.X, LeaderStart.Y, LeaderEnd.X, LeaderEnd.Y, Marker.Color, 1.f);
            DrawRect(FLinearColor(0.015f, 0.025f, 0.035f, 0.9f), Bounds.Min.X, Bounds.Min.Y, Size.X, Size.Y);
            if (!Label.IsEmpty())
                DrawText(Label, Marker.Color, Bounds.Min.X + Padding, Bounds.Min.Y + Padding, Font, Scale, false);
            const float BarX = Bounds.Min.X + (Size.X - BarWidth) * 0.5f;
            const float BarY = Bounds.Max.Y - Padding - BarHeight;
            const float MaxHP = Unit.Kind == EProofUnitKind::HQ ? 300.f :
                (Unit.Kind == EProofUnitKind::Refinery || Unit.Kind == EProofUnitKind::Factory ? 200.f : 100.f);
            DrawRect(FLinearColor(0.15f, 0.17f, 0.19f, 1.f), BarX, BarY, BarWidth, BarHeight);
            DrawRect(Marker.Color, BarX, BarY, BarWidth * FMath::Clamp(Unit.HitPoints / MaxHP, 0.f, 1.f), BarHeight);
        };
        for (const FUnitMarker& Marker : Units)
        {
            const FVector2D& Screen = Marker.Screen;
            const float Radius = Marker.Radius;
            DrawLine(Screen.X - Radius, Screen.Y - Radius, Screen.X + Radius, Screen.Y - Radius, Marker.Color, 2.f);
            DrawLine(Screen.X - Radius, Screen.Y - Radius, Screen.X - Radius, Screen.Y + Radius, Marker.Color, 2.f);
            DrawLine(Screen.X + Radius, Screen.Y - Radius, Screen.X + Radius, Screen.Y + Radius, Marker.Color, 2.f);
            DrawLine(Screen.X - Radius, Screen.Y + Radius, Screen.X + Radius, Screen.Y + Radius, Marker.Color, 2.f);
            if (Marker.Priority < 2) DrawUnitGroup(Marker);
        }
        for (const FResourceMarker& Marker : Resources)
        {
            const FProofResourceRecord& Resource = *Marker.Resource;
            const FLinearColor Color = Resource.Remaining > 0 ? FLinearColor(0.25f, 0.88f, 1.f) : FLinearColor(0.5f, 0.55f, 0.6f);
            DrawLine(Marker.Screen.X - 7.f, Marker.Screen.Y, Marker.Screen.X + 7.f, Marker.Screen.Y, Color, 2.f);
            DrawLine(Marker.Screen.X, Marker.Screen.Y - 7.f, Marker.Screen.X, Marker.Screen.Y + 7.f, Color, 2.f);
            const FString Name = FString::Printf(TEXT("ORE #%d"), Resource.ResourceId);
            const FString Amount = FString::FromInt(Resource.Remaining);
            const float Scale = 0.86f * UiScale;
            const float Padding = 3.f * UiScale;
            float NameWidth = 0.f, NameHeight = 0.f, AmountWidth = 0.f, AmountHeight = 0.f;
            GetTextSize(Name, NameWidth, NameHeight, Font, Scale);
            GetTextSize(Amount, AmountWidth, AmountHeight, Font, Scale);
            const FVector2D Size(FMath::Max(NameWidth, AmountWidth) + Padding * 2.f, NameHeight + AmountHeight + Padding * 3.f);
            const FVector2D Preferred(Marker.Screen.X - Size.X * 0.5f, Marker.Screen.Y + 10.f);
            FBox2D Bounds;
            FVector2D LeaderStart, LeaderEnd;
            if (!Layout.TryPlaceLinked(Preferred, Size, Marker.SourceReservation, Bounds, LeaderStart, LeaderEnd))
            {
                if (bCaptureLayout) ++CaptureStats.OmittedResourceGroups;
                continue;
            }
            if (bCaptureLayout) ++CaptureStats.PlacedResourceGroups;
            DrawLine(LeaderStart.X, LeaderStart.Y, LeaderEnd.X, LeaderEnd.Y, Color, 1.f);
            DrawRect(FLinearColor(0.015f, 0.025f, 0.035f, 0.9f), Bounds.Min.X, Bounds.Min.Y, Size.X, Size.Y);
            DrawText(Name, Color, Bounds.Min.X + Padding, Bounds.Min.Y + Padding, Font, Scale, false);
            DrawText(Amount, Color, Bounds.Min.X + Padding, Bounds.Min.Y + Padding * 2.f + NameHeight, Font, Scale, false);
        }
        for (const FUnitMarker& Marker : Units)
            if (Marker.Priority == 2) DrawUnitGroup(Marker);
    }
    // Legacy proof labels share bounded placement, retaining full ID and HP text.
    if (!bSkirmish)
    {
        FProofMarkerLayout Layout(FVector2D(Canvas->ClipX, Canvas->ClipY),
            4.f * UiScale, 2.f * UiScale, 64.f * UiScale);
        const FBox2D PanelBounds = Panel.Bounds();
        Layout.Reserve(PanelBounds);
        struct FLegacyMarker
        {
            const FProofUnitRecord* Unit;
            FVector2D Screen;
            FLinearColor Color;
            float Radius;
            int32 Priority;
            int32 SourceReservation;
        };
        TArray<FLegacyMarker> Markers;
        // Reserve every eligible bracket before any label can cover a later glyph.
        for (const FProofUnitRecord& Unit : GS->Snapshot.Units)
        {
            if (Unit.HitPoints <= 0.f) continue;
            FVector2D Screen;
            if (!PC->ProjectWorldLocationToScreen(Unit.Position, Screen)) continue;
            const bool bOwn = Unit.OwnerId == PC->OwnerId;
            const bool bSelected = bOwn && Pawn && Pawn->GetSelectedUnitIds().Contains(Unit.UnitId);
            const bool bPrimary = bSelected && Unit.UnitId == Pawn->GetSelectedUnitId();
            const FLinearColor Color = bSelected ? FLinearColor::Yellow :
                (bOwn ? FLinearColor::Green : FLinearColor(1.f, 0.4f, 0.3f));
            const float Radius = bSelected ? 18.f : 10.f;
            const FBox2D Glyph(Screen - FVector2D(Radius + 1.f, Radius + 1.f),
                Screen + FVector2D(Radius + 1.f, Radius + 1.f));
            if (!Layout.IsAnchorVisible(Glyph.Min) || !Layout.IsAnchorVisible(Glyph.Max)
                || Glyph.Intersect(PanelBounds)) continue;
            const int32 Source = Layout.Reserve(Glyph);
            if (Source == INDEX_NONE) continue;
            Markers.Add({&Unit, Screen, Color, Radius, bPrimary ? 0 : (bSelected ? 1 : 2), Source});
            DrawLine(Screen.X - Radius, Screen.Y - Radius, Screen.X + Radius, Screen.Y - Radius, Color, 2.f);
            DrawLine(Screen.X - Radius, Screen.Y - Radius, Screen.X - Radius, Screen.Y + Radius, Color, 2.f);
            DrawLine(Screen.X + Radius, Screen.Y - Radius, Screen.X + Radius, Screen.Y + Radius, Color, 2.f);
            DrawLine(Screen.X - Radius, Screen.Y + Radius, Screen.X + Radius, Screen.Y + Radius, Color, 2.f);
        }
        Markers.Sort([](const FLegacyMarker& A, const FLegacyMarker& B)
        {
            return A.Priority != B.Priority ? A.Priority < B.Priority : A.Unit->UnitId < B.Unit->UnitId;
        });
        for (const FLegacyMarker& Marker : Markers)
        {
            const FString Label = FString::Printf(TEXT("#%d  HP %.0f"), Marker.Unit->UnitId, Marker.Unit->HitPoints);
            float TextWidth = 0.f, TextHeight = 0.f;
            GetTextSize(Label, TextWidth, TextHeight, Font, FontScale);
            FBox2D Bounds;
            FVector2D Start, End;
            if (!FProofLegacyLabelLayout::TryPlace(Layout, Marker.Screen, Marker.Radius,
                FVector2D(TextWidth, TextHeight), UiScale, Marker.SourceReservation, Bounds, Start, End)) continue;
            DrawLine(Start.X, Start.Y, End.X, End.Y, Marker.Color, 1.f);
            DrawRect(FLinearColor(0.015f, 0.025f, 0.035f, 0.9f), Bounds.Min.X, Bounds.Min.Y,
                Bounds.GetSize().X, Bounds.GetSize().Y);
            const float Padding = 3.f * UiScale;
            DrawText(Label, Marker.Color, Bounds.Min.X + Padding, Bounds.Min.Y + Padding, Font, FontScale, false);
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
        DrawRect(FLinearColor(0.01f, 0.02f, 0.04f, 1.f), TerminalHintBounds.Min.X, TerminalHintBounds.Min.Y,
            TerminalHintBounds.GetSize().X, TerminalHintBounds.GetSize().Y);
        DrawText(TerminalHint, FLinearColor::White, TerminalHintBounds.Min.X + HintPadding,
            TerminalHintBounds.Min.Y + HintPadding, Font, UiScale, false);
    }
    if (bCaptureLayout && !CaptureFilename.IsEmpty())
    {
        LoggedHUDLayoutCaptures.Add(CaptureFilename);
        UE_LOG(LogTemp, Display, TEXT("PROOF_HUD_LAYOUT_CAPTURE name=%s epoch=%d revision=%d liveunits=%d visibleeligibleglyphs=%d culledglyphs=%d offscreenglyphs=%d occludedglyphs=%d placedunitgroups=%d omittedunitgroups=%d primaryglyphvisible=%d primarygroupplaced=%d visibleresourceglyphs=%d placedresourcegroups=%d omittedresourcegroups=%d"),
            *FPaths::GetCleanFilename(CaptureFilename), GS->Snapshot.MatchEpoch, GS->Snapshot.Revision,
            CaptureStats.LiveUnits, CaptureStats.VisibleUnitGlyphs,
            CaptureStats.OffscreenUnitGlyphs + CaptureStats.OccludedUnitGlyphs,
            CaptureStats.OffscreenUnitGlyphs, CaptureStats.OccludedUnitGlyphs,
            CaptureStats.PlacedUnitGroups, CaptureStats.OmittedUnitGroups,
            CaptureStats.bPrimaryGlyphVisible ? 1 : 0, CaptureStats.bPrimaryGroupPlaced ? 1 : 0,
            CaptureStats.VisibleResourceGlyphs, CaptureStats.PlacedResourceGroups, CaptureStats.OmittedResourceGroups);
    }
}
