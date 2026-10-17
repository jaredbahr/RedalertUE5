#pragma once
#include "CoreMinimal.h"

// Pure policy for this opt-in fixture's two-phase, run-specific release files.
struct FProofAttackMoveReleaseDecision
{
    bool bStopTick = false, bComplete = false;
    const TCHAR* Error = nullptr;
};

struct FProofAttackMoveReleasePolicy
{
    static bool IsToken(const FString& Token)
    {
        if (Token.Len() != 32) return false;
        for (TCHAR C : Token) if (!FChar::IsHexDigit(C)) return false;
        return true;
    }

    static FProofAttackMoveReleaseDecision Evaluate(bool bStartup, bool bHost, bool bReady,
        bool bClientsExist, const FString& Clients, bool bHostExists, const FString& Host, const FString& Token)
    {
        if (!IsToken(Token)) return {true, false, TEXT("release_token")};
        if (bStartup) return (bClientsExist || bHostExists)
            ? FProofAttackMoveReleaseDecision{true, false, TEXT("stale_release")}
            : FProofAttackMoveReleaseDecision{};
        if (bClientsExist && Clients != TEXT("clients:") + Token) return {true, false, TEXT("wrong_client_release")};
        if (bHostExists && Host != TEXT("host:") + Token) return {true, false, TEXT("wrong_host_release")};
        if (bHostExists && (!bClientsExist || !bHost)) return {true, false, TEXT("early_host_release")};
        if (!bClientsExist) return {};
        if (!bReady) return {true, false, TEXT("premature_release")};
        if (bHost && !bHostExists) return {true, false, nullptr};
        return {true, true, nullptr};
    }
};
