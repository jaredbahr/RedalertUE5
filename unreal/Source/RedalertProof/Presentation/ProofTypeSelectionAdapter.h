#pragma once
#include "Presentation/ProofSelectionState.h"
#include "Presentation/ProofTypeSelectGesture.h"
#include "Templates/Function.h"

// The actual shared action adapter. The pawn supplies projection/viewport work
// through VisibleAction; this adapter owns gesture/context ordering for both
// real input and opt-in shared-handler fixtures.
namespace ProofTypeSelectionAdapter
{
REDALERTPROOF_API ProofTypeSelectGesture::Scope Apply(FProofSelectionState& Selection,
    ProofTypeSelectGesture::State& Gesture, const FProofSnapshot& Snapshot, int32 OwnerId,
    double WallSeconds, bool Focused, TFunctionRef<bool()> VisibleAction);
}
