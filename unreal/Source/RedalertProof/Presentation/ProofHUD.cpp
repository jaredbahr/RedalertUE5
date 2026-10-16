#include "Presentation/ProofHUD.h"
#include "Presentation/ProofRTSPawn.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Runtime/ProofGameState.h"
#include "Runtime/ProofPlayerController.h"

void AProofHUD::DrawHUD()
{
    Super::DrawHUD();
    AProofPlayerController* PC = Cast<AProofPlayerController>(PlayerOwner);
    const AProofGameState* GS = GetWorld()->GetGameState<AProofGameState>();
    const AProofRTSPawn* Pawn = PC ? Cast<AProofRTSPawn>(PC->GetPawn()) : nullptr;
    if (!Canvas || !PC)
    {
        return;
    }
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), 8.f, 8.f, FMath::Min(790.f, Canvas->ClipX - 16.f), 126.f);
    DrawText(TEXT("UE COMMAND PROOF - generic unapproved blockout proxies"), FLinearColor::White, 18.f, 16.f);
    DrawText(TEXT("Left-click: select own unit | Right-click: move / attack enemy | P: produce | Wheel: zoom"),
        FLinearColor::White, 18.f, 36.f);
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
    DrawText(FString::Printf(TEXT("Owner %d | Host revision %d | Toy credits %d | Selected %d"),
        PC->OwnerId, GS ? GS->Snapshot.Revision : 0, Credits, Pawn ? Pawn->GetSelectedUnitId() : INDEX_NONE),
        FLinearColor::White, 18.f, 56.f);
    DrawText(Pawn ? Pawn->GetRequestStatus() : TEXT("Waiting for camera pawn."), FLinearColor::Yellow, 18.f, 76.f);
    DrawText(Pawn && Pawn->IsUsingBlockoutMesh()
        ? TEXT("All records shown as Rhino proxies at 0.3 display scale; no unit types or final art scale.")
        : TEXT("Generated Rhino mesh absent: engine cubes are generic proxies."),
        FLinearColor(0.75f, 0.8f, 0.85f), 18.f, 96.f);
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
        DrawText(FString::Printf(TEXT("#%d O%d HP%.0f"), Unit.UnitId, Unit.OwnerId, Unit.HitPoints),
            Color, Screen.X + 20.f, Screen.Y - 10.f);
    }
}
