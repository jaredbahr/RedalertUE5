# Focused human trial: ReturnCargo and factory fallback

Status: **not performed**. This supplements issue #17 and the existing
[playable trial](PLAYABLE_TRIAL.md). It adds no feature, launch authorization,
time allowance or automated physical-input claim. Native and rendered automation
remain separate from a person using the actual mouse and keyboard.

## Before the session

Use the approved prepared checkout, installed engine, existing generated map and
normal `start-small-match.ps1` practice launcher. Keep its existing single-process
15-minute session bound, two-CPU/Normal-priority controls, 30 FPS target, storage
checks and owned-process cleanup. Wait for the shared engine allocation; do not
start duplicate instances or extend a failed session.

The compiled gameplay must include the reviewed ReturnCargo command and
first-live-factory fallback fix. Record the exact source SHA, clean/dirty state,
build/module identity, launch mode and actual viewport. A prepared older DLL does
not satisfy this requirement. If only one change is integrated, mark the other
section blocked. Do not infer build identity from a document revision.

Do **not** enable `-ReturnCargoEvidence`, other scripted match fixtures, or engine
input injection for these human rows. Those routes can establish shared/native
binding behavior but cannot establish that a human performed the input. Preserve
`physicalInputVerified=false` and `humanConfirmationRequired=true` in the
launcher's receipt; record the person's observations separately.

Each section below can be checked during an approved practice session. Do not
combine it with the nine-group checklist and expect every action to have a raw
input row: `PROOF_MANUAL_INPUT` stops after 24 events, detailed focus rows after
eight transitions. Take the baseline counter/IDs and supporting observations
before each case. If the log cap is reached, state that limit and use the actual
HUD/captures and human observation; absence of a capped log is not a pass or a
failure. Do not raise those caps to complete this checklist.

Accepted and already queued orders continue while unfocused. Harvesting,
production results and combat do not pause on Alt-Tab. Distinguish completion of
an existing order from a new unintended input; do not fail a legitimate queued
ReturnCargo because it dispatches after focus leaves the window.

## A. Physical ReturnCargo

Use the starter Harvester A and the Harvester B granted by an ordinary paid
refinery build. Record their IDs and the live owned refinery ID. Leave B idle at
first, so A's deliveries are individually observable. Use the HUD cargo value,
not an assumption based on the miner's direction. Require remaining mineable ore
for saved-Gather reload observations; a depleted or unavailable source can make
the valid order stop or choose another remaining patch. Mark an unavailable
reload observation blocked rather than failing correct depletion behavior.
If a natural delivery happens
before the intended click, record that the case was not observed; do not count
it as a forced return.

1. **Single loaded miner, saved Gather.** Select A and right-click ore. After A
   has cargo, physically right-click the owned refinery before it unloads.
   Record the R counter increment and the ReturnCargo request/accepted receipt,
   including A's ID, target refinery ID and sequence. Submission must not grant
   money or empty cargo. Observe loaded travel, delivery at the refinery, then A
   returning to ore and loading again without another Gather command. With B
   still idle, delivered cargo should account for the credit increase.
2. **Loaded plus empty selection.** While A is loaded and B is still empty,
   select A and Shift-click B. Confirm exactly those two IDs. Right-click the
   refinery once. Expect one ReturnCargo for A, none for B, and one physical R
   edge. The queue can stamp the command later; do not confuse the click's
   counter with the command sequence. A should deposit and resume; B should not
   acquire a return or mining order from this action.
3. **Two loaded miners, different prior orders.** Once A is loaded, physically
   press S and require its accepted Stop receipt; confirm cargo is retained and
   A stays still. Select B, Gather ore and wait for a real load. Select both and
   right-click the refinery once. Expect two accepted ReturnCargo commands with
   distinct sequences and the same chosen target, one for each selected ID.
   Observe both deposits. A's previous Stop means it must wait empty afterward;
   B's saved Gather means it must reload without another Gather command. Restore
   A explicitly with a new Gather if continuing the match.
4. **Empty command boundary.** Select a known empty miner and right-click the
   owned refinery. The native R counter can increase, but there must be no new
   ReturnCargo request/sequence for that empty miner. Read the feedback rather
   than treating every key/click counter as an accepted order.
5. **Actual focus recovery.** Record the current selection, last request and
   input counters. Alt-Tab away and return using normal desktop interaction.
   Observe the real focus/foreground state and whether an activation click is
   needed. With a known loaded miner, perform another physical refinery click
   after focus is restored; correlate its counter, request and accepted receipt.
   Record stuck buttons/modifiers, unintended new requests, or missed input.
   Do not substitute a synthetic `LostFocus` notification for this row.

A refinery that dies, a terminal match, an empty load or a changed selection can
invalidate a planned case. Record the actual condition as blocked/not observed,
not a false pass. Invalid ownership, destroyed targets, repeated/retargeted
orders and saturated-credit cases also have separate native model tests; this
small physical checklist does not replace or re-claim those tests.

## B. Physical P fallback and selected-factory priority

Use two ordinary paid live owned factories, F1 created first and F2 created
second, with visibly different legal rally points. Record their IDs and rallies.
Keep enough credits for two paid Combat units after construction. Gathering can
fund this normally. Stop all miners after earning the required funds and require
their accepted Stop receipts before the isolated credit checks. Stop retains any
carried cargo. Do not
inject credits, pause the model, remove the bot, or change prices/capacity.

1. Clear selection by a physical click on empty ground, confirm it is clear,
   then press P once. Expect one Produce request whose source is F1, followed by
   acceptance, exactly the normal 50-credit cost, one new Combat unit spawned
   by F1 and movement toward F1's stored rally. Observe the physical P counter.
2. Physically select F2, then press P once. Expect F2 in the request, acceptance,
   the same normal cost, one spawn at F2 and F2's rally. This confirms selecting
   a later factory overrides the fallback rather than always using F1.

Allow space for both spawn routes and remain below the existing live-record
cap. A normal `spawn`, `resources`, `capacity`, `factory` or `finished` rejection is
not evidence that the fallback chose another factory. Record its actual source
ID and reason; if the setup is unsuitable, mark the observation blocked. Do not
expect automatic rerouting from a blocked F1 to F2: that is not this fix's
contract. The current fallback is the first owned live factory in snapshot order,
not original-game primary-factory behavior.

## Retain a scoped record

Keep the record with the launcher's `session.json`, `game.log` and relevant fresh
captures in ignored local evidence. Do not overwrite previous sessions or upload
machine paths/device details to the public repository. Start all rows unverified.

| Case | Result | Human action, IDs and counters | Request/receipt and visible outcome | UTC/evidence and limitation |
| --- | --- | --- | --- | --- |
| A1 single return | unverified | | | |
| A2 loaded/empty group | unverified | | | |
| A3 two loaded/prior Stop | unverified | | | |
| A4 empty miner | unverified | | | |
| A5 real focus recovery | unverified | | | |
| B1 unselected P fallback | unverified | | | |
| B2 selected later factory | unverified | | | |

Use pass only for a physically performed action with the expected observation;
use fail for an observed mismatch and blocked for missing prerequisites. Mark
unobserved subtasks explicitly. Logs and native counters support the observation
but do not identify who pressed a key or establish a hardware device by
themselves. Record a missing correlation rather than inferring one.

Complete the existing natural terminal/rematch and Escape/owned-exit checks if
that full trial is being claimed. Passing these focused rows alone does not pass
the whole human match, networking, performance, art, or original-game fidelity.
