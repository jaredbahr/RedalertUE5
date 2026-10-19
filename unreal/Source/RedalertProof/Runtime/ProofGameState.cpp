#include "Runtime/ProofGameState.h"
#include "Net/UnrealNetwork.h"

AProofGameState::AProofGameState()
{
    bReplicates = true;
    SetNetUpdateFrequency(5.f);
    SetMinNetUpdateFrequency(5.f);
}

void AProofGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AProofGameState, Snapshot);
}

void AProofGameState::Publish(const FProofSnapshot& NewSnapshot)
{
    check(HasAuthority());
    Snapshot = NewSnapshot;
    LogSnapshot(TEXT("host"), 0);
    ForceNetUpdate();
}

void AProofGameState::LogSnapshot(const TCHAR* ProofRole, int32 OwnerId) const
{
    UE_LOG(LogTemp, Display, TEXT("PROOF_SNAPSHOT role=%s owner=%d epoch=%d revision=%lld digest=%s"),
        ProofRole, OwnerId, Snapshot.MatchEpoch, static_cast<long long>(Snapshot.Revision), *Snapshot.ComputeDigest());
}

void AProofGameState::OnRepSnapshot()
{
    // The local controller logs after its server-assigned identity has replicated.
}
