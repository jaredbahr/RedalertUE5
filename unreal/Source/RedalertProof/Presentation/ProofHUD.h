#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ProofHUD.generated.h"

/** Text/selection overlay for the command proof, not final game UI. */
UCLASS()
class REDALERTPROOF_API AProofHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
