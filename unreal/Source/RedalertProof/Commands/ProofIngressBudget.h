#pragma once

#include "CoreMinimal.h"

// A controller owns one budget for its connection lifetime. This is deliberately
// independent of the model's per-Advance gameplay command allowance.
class REDALERTPROOF_API FProofIngressBudget
{
public:
    static constexpr double Burst = 64.0;
    static constexpr double RefillPerSecond = 32.0;

    // Every received RPC calls this before command validation or result logging.
    // The first excess request trips the budget for the rest of this controller's
    // lifetime; the adapter must close a remote connection at that point.
    bool TryAdmit(double NowSeconds);
    bool IsTripped() const { return bTripped; }

private:
    double Tokens = Burst;
    double LastSeconds = 0.0;
    bool bStarted = false;
    bool bTripped = false;
};
