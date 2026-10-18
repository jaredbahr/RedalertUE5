#pragma once

#include "Commands/ProofCommandModel.h"

/** Complete critical values only; Canvas chooses a measured variant separately. */
namespace ProofHUDStatusText
{
inline TArray<FString> Economy(int32 Credits, int32 DeliveredOre)
{
    return {
        FString::Printf(TEXT("Credits: %d   Delivered ore: %d"), Credits, DeliveredOre),
        FString::Printf(TEXT("Credits %d  Ore %d"), Credits, DeliveredOre)
    };
}

inline TArray<FString> Selection(const FProofUnitRecord* Primary, int32 Count)
{
    if (!Primary) return {TEXT("Selected: none   Primary: none"), TEXT("No selection")};
    if (Count <= 0 || !FMath::IsFinite(Primary->HitPoints) || Primary->HitPoints <= 0.f) return {};
    const TCHAR* Kind = nullptr;
    switch (Primary->Kind)
    {
    case EProofUnitKind::Combat: Kind = TEXT("Combat"); break;
    case EProofUnitKind::Harvester: Kind = TEXT("Harvester"); break;
    case EProofUnitKind::HQ: Kind = TEXT("HQ"); break;
    case EProofUnitKind::Refinery: Kind = TEXT("Refinery"); break;
    case EProofUnitKind::Factory: Kind = TEXT("Factory"); break;
    default: return {};
    }
    FString RallyFull, RallyCompact;
    if (Primary->Kind == EProofUnitKind::Factory)
    {
        if (Primary->bHasRallyPoint)
        {
            if (!FMath::IsFinite(Primary->RallyPoint.X) || !FMath::IsFinite(Primary->RallyPoint.Y)) return {};
            RallyFull = FString::Printf(TEXT("   Rally %.0f,%.0f"), Primary->RallyPoint.X, Primary->RallyPoint.Y);
            RallyCompact = FString::Printf(TEXT("  Rally %.0f,%.0f"), Primary->RallyPoint.X, Primary->RallyPoint.Y);
        }
        else
        {
            RallyFull = TEXT("   Rally none: right-click ground");
            RallyCompact = TEXT("  Rally none");
        }
    }
    return {
        FString::Printf(TEXT("Selected: %d   Primary #%d %s   HP %.0f   Cargo %d%s"),
            Count, Primary->UnitId, Kind, Primary->HitPoints, Primary->Cargo, *RallyFull),
        FString::Printf(TEXT("%d selected  #%d %s  HP %.0f  Cargo %d%s"),
            Count, Primary->UnitId, Kind, Primary->HitPoints, Primary->Cargo, *RallyCompact)
    };
}
}