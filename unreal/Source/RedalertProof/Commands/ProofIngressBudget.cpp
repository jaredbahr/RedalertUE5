#include "Commands/ProofIngressBudget.h"

bool FProofIngressBudget::TryAdmit(double NowSeconds)
{
    if (bTripped) { return false; }

    // PlatformTime is monotonic in the adapter. Nonfinite or regressing test
    // inputs never refill the budget, so a bad clock cannot grant more work.
    if (FMath::IsFinite(NowSeconds))
    {
        if (!bStarted)
        {
            LastSeconds = NowSeconds;
            bStarted = true;
        }
        else if (NowSeconds > LastSeconds)
        {
            Tokens = FMath::Min(Burst, Tokens + (NowSeconds - LastSeconds) * RefillPerSecond);
            LastSeconds = NowSeconds;
        }
    }
    if (Tokens < 1.0)
    {
        bTripped = true;
        return false;
    }
    Tokens -= 1.0;
    return true;
}
