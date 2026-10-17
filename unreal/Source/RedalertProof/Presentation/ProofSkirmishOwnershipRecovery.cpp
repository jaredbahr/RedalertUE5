#include "Presentation/ProofSkirmishOwnershipRecovery.h"

EProofSkirmishOwnershipRecovery FProofSkirmishOwnershipRecovery::Apply(const FProofSnapshot& Snapshot,
    int32 OwnerId, const FString& RequestedType, const FProofCommandResult& Receipt, int32 RequestedUnitId,
    bool bSourceWasOwnedLive, TMap<int32, int32>& IssuedTargets, int32& PendingSequence)
{
    if (Snapshot.MatchPhase != EProofMatchPhase::Playing || PendingSequence <= 0 || Receipt.bAccepted
        || RequestedType != TEXT("Attack") || Receipt.Reason != TEXT("ownership") || !bSourceWasOwnedLive)
        return EProofSkirmishOwnershipRecovery::NotHandled;

    // The selected attacker can die between the client snapshot and host
    // validation. Missing/foreign records still reach normal rejection failure.
    const FProofUnitRecord* RejectedUnit = Snapshot.Units.FindByPredicate(
        [RequestedUnitId](const FProofUnitRecord& Unit) { return Unit.UnitId == RequestedUnitId; });
    if (RejectedUnit && RejectedUnit->OwnerId == OwnerId)
    {
        if (RejectedUnit->HitPoints > 0.f) return EProofSkirmishOwnershipRecovery::Wait;
        if (RejectedUnit->HitPoints <= 0.f)
        {
            IssuedTargets.Remove(RequestedUnitId);
            // Validation consumed the ordinary sequence. Only the fixture
            // receipt latch is cleared, never its allocator.
            PendingSequence = 0;
            return EProofSkirmishOwnershipRecovery::Recovered;
        }
    }
    return EProofSkirmishOwnershipRecovery::NotHandled;
}
