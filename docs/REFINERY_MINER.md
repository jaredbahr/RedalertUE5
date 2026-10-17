# Refinery-provided miner: bounded fidelity slice

Status: source preparation only. Build, Unreal automation, rendered matches and two-client acceptance are pending. Queue this after Imagination's allocated work and PR #38's remaining validation; do not launch a build or engine without an explicit shared-slot handoff.

## Intended behavior

An accepted skirmish BuildRefinery completes immediately under the existing prototype construction model and creates exactly one refinery plus one owned, unarmed Harvester. It charges the existing refinery price once. The starting miner remains. This follows the qualitative refinery/miner behavior documented in [the fidelity baseline](FIDELITY_BASELINE.md); costs, timing and proportions are still prototype values, not calibrated RA2 balance.

The authoritative command path checks both live-record slots, both usable IDs and a deterministic clear in-world miner spawn before inserting either record or debiting credits. Capacity or spawn failure leaves units, credits and IDs unchanged; normal request sequence/allowance handling still applies. Replayed commands cannot grant again, and later advances or replicated observations do not create miners.

The miner uses the existing selectable generic proxy and Gather command. No automatic initial harvesting, new unit kind, faction art, production queue, power model, movement/cargo/ore-value change or new cap is part of this slice. Bots plan for both records and the spawn reservation, and use the shared validator for their miners' Gather commands. Bounded rotation leaves command opportunities for production and combat.

## Acceptance before integration

| Gate | Required evidence |
|---|---|
| Reviewed exact source | Core and bot changes reviewed independently; current-main reconciliation has no unrelated gameplay changes; fresh clean source identity retained |
| Atomic model regression | Accepted completion adds exactly one refinery and one miner, debits one price; replay/advances do not duplicate; original miner cargo/order survives; exact capacity, ID exhaustion and blocked-spawn failures produce no partial state |
| Economy and lifecycle | Both miners use finite ore without duplicated extraction/income; foreign commands rejected; terminal freeze and removal/reseed/rematch preserve existing semantics |
| Bot parity and budgets | Granted miner receives accepted Gather through the shared path; planning accounts for two slots and reserved spawn; stable rotation uses all eligible miners without exceeding existing cadence/command/retry bounds or starving production/attack |
| Existing model suites | New cases and all existing skirmish/economy/bot/rules/lifecycle/ownership tests pass; native, group-control and fidelity models retain their invariants |
| Actual matches and replication | Fresh Victory/Defeat/Coop and lifecycle runs retain expected outcomes, shared snapshot/digest checks, deadlines and cleanup. Identify the granted miner in evidence; a single gather flag must not be presented as both human miners gathering |
| Player-visible behavior | Native-size capture and shared-handler selection/order evidence identify the new miner separately, show cargo travel/extraction/deposit and continued gathering, with authoritative credit/ore conservation. Physical-input claims still need Jared's human session |

Stop on failures and retain all evidence before any justified correction/retry. Do not weaken expected winners, deadlines, authoritative checks or cleanup gates to accommodate changed economy. Existing build/resource policies apply; no additional GitHub fees, hosted Actions, installs, binary uploads or zero-C:-writes claim.

The model regressions are prepared source, not executed evidence until the queued Unreal run passes. PR #38's prior passes do not validate this gameplay change. Issues #2, #17, #18 and #33 retain their own unsatisfied scope; this draft alone satisfies none of their full acceptance gates.

## Prepared runtime identity observations

Use `-RefineryMinerEvidence` with the existing Victory/Defeat/Coop runner. The opt-in helper records synchronous accepted command before/after states: exact new ID pair, one cost, existing-state preservation, valid spawn and accepted Gather for that miner. Actual advances verify ore/cargo/delivered conservation and track load plus owned-refinery deposit, including simultaneous contributions from other miners. No new commands, RPCs, simulation gates or gameplay mutations are introduced.

Host and actual receiving clients log semantic snapshots once per new epoch/revision. The offline gate requires matching epoch/revision/digest and semantic state for later identity/load/deposit checkpoints, including owners1/2/100 across allthreeCooproles (1/100 standalone). It does not claim that every immediate command revision replicated. Missing, malformed, contradictory or over-budget evidence fails the opt-in run. Limits are1536snapshot rows per role,8MiB ASCII snapshot payload,32KiB perrow and32 grants and 32 rejections per epoch. Existing outcome/deadline/screenshot/cleanup gates remain required.

The strict offline policy passed35synthetic cases and independent review. Those synthetic passes do not establish actual mining or replication; C++ compilation and real-match phase observations remain pending until recorded in a validation ledger. Current source has been reconciled with independently validated HUD main d3510e5.
