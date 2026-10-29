#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ProofHUD.generated.h"

class AProofPlayerController;
class AProofRTSPawn;

/** Text/selection overlay for the command proof, not final game UI. */
UCLASS()
class REDALERTPROOF_API AProofHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;

private:
    void DrawCommandCard(const AProofRTSPawn* Pawn, AProofPlayerController* PC);
    // Capture fixture diagnostics: one record for each requested full path.
    TSet<FString> LoggedHUDLayoutCaptures;
};
