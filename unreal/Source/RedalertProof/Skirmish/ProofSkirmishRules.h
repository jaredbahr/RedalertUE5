#pragma once

#include "CoreMinimal.h"
#include "Commands/ProofCommandModel.h"

struct REDALERTPROOF_API FProofMatchOutcome
{
    bool bFinished = false;
    int32 WinnerTeam = INDEX_NONE;
};

/** Pure terminal-state rules for the opt-in skirmish fixture. */
class REDALERTPROOF_API FProofSkirmishRules
{
public:
    static FProofMatchOutcome Evaluate(const FProofSnapshot& Snapshot);
};
