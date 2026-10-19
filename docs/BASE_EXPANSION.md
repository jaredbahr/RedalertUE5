# Structure-based base expansion

## Purpose and reference boundary

The original publisher-authored [RA2 manual](https://manualmachine.com/gamespc/commandconquerredalert2/1117998-user-manual/) describes placement near the player's other structures under “Placing Structures on the Battlefield” (printed p18), while the Construction Yard enables new structures (printed p38). The original document is hosted by a third party. [Phobos's own Yuri-engine documentation](https://phobos.readthedocs.io/en/latest/New-or-Enhanced-Logics.html#build-area-customizations) also describes building-to-building adjacency. Its additional allow/disallow flags are extension features and are not adopted here.

The previous proof accepted construction only inside an 800 cm sphere around the owner's HQ. A paid refinery/factory near the edge of that ring therefore could not extend the base's placement reach beyond the HQ ring. This slice implements the qualitative expansion behavior through existing build commands. It does not reproduce original map-cell distances, rectangular foundations, terrain, shroud, buildup animation, production timing, tech prerequisites, or an optional build-near-ally setting.

## Authoritative contract

- The owner must still have at least one live HQ. Construction capability is separate from placement reach. A teammate's HQ does not replace that capability, even when the teammate keeps the match playing after this owner's HQ is lost.
- Placement may be inside the existing closed `BuildRadius` sphere of any live HQ, Refinery, or Factory belonging to the command's trusted owner. Other players' buildings, friendly or hostile, cannot extend the owner's base. Combat and Harvester records cannot anchor placement.
- The radius remains the existing **800 cm center-to-center prototype value**. Existing 180 cm all-live-structure separation, finite/world bounds, prices, record/ID capacity, refinery-plus-miner spawn preflight, and command sequence/rate rules are unchanged. The existing three-dimensional distance convention is retained; this does not establish a terrain/flat-ground gate.
- All current structures are synchronously completed on accepted Submit. There is no unfinished-building record or construction-progress flag. Queued, malformed, rejected, or otherwise unapplied commands are not anchors because no structure record was committed. If asynchronous production is added later, unfinished records will need an explicit eligibility check before participating in adjacency.
- Eligibility is rechecked against current authoritative state on every command. A destroyed anchor no longer grants reach, but a surviving distant building does not require a currently connected chain of live buildings back to HQ. This follows the bounded placement rule; no graph-connectivity system is introduced.
- Successful refinery construction still grants exactly one new Harvester and its existing automatic Gather order. Factory production/rally behavior is unchanged. Failed placement cannot spend credits, grant records, consume IDs, change a digest/revision, or create a hidden future anchor; existing sequence/rate bookkeeping on structurally valid rejection remains unchanged.
- No new command type, payload, replicated field, digest format, bot policy or runtime budget is added. Reset, removal, death and terminal rules continue to govern existing records.

## Existing integration path

`AProofRTSPawn::BuildUnderCursor` already submits the R/F build request's ground destination unchanged through the authoritative command adapter. The new policy is therefore reachable through the existing human command producer without a new control binding or helper-only scaffold. No Pawn, HUD, selection, gesture, RPC or input source is changed. The old cursor-failure hint still suggests pointing near HQ, which remains a valid subset of the expanded placement area; a broader hint can be reconciled with the separately owned input work.

The bot keeps its deterministic HQ-first site search. If all its HQ candidate sites fail, the [bot construction fallback](BOT_BASE_EXPANSION.md) also searches around already-committed own live structures using the same candidate offsets. Construction capability, authoritative placement admission and existing economy priorities still apply equally to human and bot commands.

## Tests and acceptance

Five exact native groups are registered under `RedalertProof.Skirmish.BaseExpansion`:

- `ChainAndEconomy`
- `PlacementBoundaries`
- `OwnershipAndLiveAnchors`
- `RejectionAtomicity`
- `LifecycleAndDeterminism`

The focused portable runner compiles these actual native tests against the production model and existing real MD5 bridge. It must execute all five names in normal, optimized and ASan+UBSan modes; empty, missing, duplicate or extra registration is a failure. Public Submit/Advance histories cover paid expansion through both eligible constructed kinds, invalid anchors, HQ/anchor loss, boundary rejection, exact economic/spawn behavior, replay/rate rejection and lifecycle state. Existing Skirmish tests remain required by exact name; native runtime limits are unchanged.

Cloud source/model evidence does not establish UHT or Windows/native compilation, bound keyboard delivery, rendered placement or host/client transport. After reviewed draft publication, the existing local owner must validate exact source with the 59-name Skirmish manifest and existing ordinary match gates. A separately bound observer can prove an accepted build beyond HQ range beside an owned structure, exact debit/spawn and common-revision replicated state. Actual physical R/F input and the deferred PR64 drag/focus gate remain separate claims.
