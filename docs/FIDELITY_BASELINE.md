# Economy and combat fidelity baseline

This baseline separates documented original-game behavior from the current generic UE proof. It is not a patch-specific balance specification or a claim of RA2/Yuri parity. No game, extractor or new tool was installed or launched for this research; no proprietary assets, rules files, music or manual copies are committed.

## Reference provenance

The [original RA2 English manual scan](https://www.scribd.com/document/760314562/Red-Alert-2-English-3-Manual) is publisher-authored material hosted by a third party, not a currently verified EA-hosted download. Page numbers below are the manual's printed numbers. The [Yuri's Revenge manual OCR](https://archive.org/stream/Command_Conquer_RED_ALERT_2_YURI_S_REVENGE/Command__Conquer_RED_ALERT_2_YURI_S_REVENGE_djvu.txt) is also a third-party-hosted original-document transcription; use section names because OCR pagination is unreliable. Manual descriptions support qualitative behavior, not exact acquisition rules or present-day multiplayer patch behavior.

Authorized local lookup found a residual RA2 folder in an existing Steam library, but no installed manuals in that folder. Ownership remains reference availability, not redistribution permission. No original gameplay measurements were made.

## Documented behavior versus current proof

| Documented original behavior | Evidence | Current UE proof / gap |
| --- | --- | --- |
| Full miners unload at a friendly refinery and resume harvesting. | RA2 pp16-17 | Travel, finite extraction, deposit and repeat on the selected patch already exist; extraction/unloading happen instantly on arrival. |
| Production takes time; funds shortages suspend it until money returns. | RA2 p17 | `Submit` immediately debits the full price and creates the record. No production job, progress, hold/resume or queue. |
| Power shortages affect production and some defenses/radar. | RA2 p18 | No power model. |
| Idle units engage approaching enemies; Guard adds return-to-position behavior. | RA2 pp21,23 | Previously required explicit Attack. This branch adds only the stationary, in-weapon-range idle-fire subset described below. |
| Refineries include a miner; Barracks and War Factories produce different categories. | RA2 pp38,41 | One starter Harvester is seeded independently. A new Refinery grants no miner. One Factory produces only generic Combat. |
| Conscript, Rhino and armed War Miner have distinct roles. | RA2 pp33-35 | Generic Combat does not distinguish infantry/vehicles/armor; generic Harvester remains unarmed. Mesh names do not prove these roles. |
| Slave Miner uses deployed workers/local processing; Industrial Plant modifies vehicle production. | Yuri manual: Slave Miner, Industrial Plant | Neither system exists. Do not treat the generic return-to-refinery Harvester as Yuri mining. |

## Existing prototype defaults, not original-game values

Source: `Commands/ProofCommandModel.h/.cpp` and `Skirmish/ProofSkirmishBot.cpp`.

| Area | Current proof value or rule |
| --- | --- |
| Start | 350 credits, one HQ, one Harvester, one 500-unit ore patch per seeded owner |
| Costs | Refinery 100, Factory 150, Combat 50 |
| Health | HQ 300; constructed Refinery/Factory 200; Harvester/Combat 100 |
| Movement/combat | 150 cm/s; 300 cm closed attack sphere; 25 damage/s with simultaneous damage application |
| Economy | Cargo 50; one extracted ore unit becomes one credit; instant extraction and deposit at arrival |
| Bounds | World coordinates ±10000 cm; build within 800 cm of HQ; structure separation 180 cm |
| Proof capacity | 12 live records per owner, including structures; not an army budget |
| Bot pacing | 1-second decisions, up to four commands/decision, 5-second retry, 20-second initial attack grace |

These values are retained in this slice. There is no established conversion from original map cells, game-speed settings, weapon rules or production ticks to UE centimeters/seconds. No verified credit-storage cap is inferred from other C&C games. Armor, weapon versus target multipliers, projectile/cooldown behavior, miner differences, prerequisites and faction costs remain uncalibrated.

## Bounded implementation: stationary idle fire

Only skirmish Combat records acquire targets automatically. Eligibility is no current order or a completed Move. An explicit Attack keeps its target and gets exactly one damage contribution; active Move, Gather and other orders suppress idle acquisition. The unit stays in place and selects the nearest live opposing-team target already inside the existing strict attack sphere; equal distances choose the smaller UnitId. Missing team identities are excluded. Both sides resolve damage from the same post-movement/pre-damage state. Terminal matches remain frozen; the legacy non-skirmish proof remains opt-in-command-only.

The manual supports automatic engagement, but the stationary restriction, nearest-target policy, tie-break, radius and damage cadence here are explicit prototype choices. This is not full Guard, retaliation, pursuit, attack-move, fog-of-war or original weapon behavior. Newly idle units may fire on the tick that a Move finishes. Exact completed-Move endpoints are retained so rounding cannot leave an arrived unit permanently ineligible.

Automatic acquisition scans candidates for each eligible unit. Its cost is unmeasured; the earlier legacy-mode stress workload does not exercise this new skirmish path. Include acquisition in the eventual combined active-scale proof before increasing practical army limits.

## Validation gate

**New gameplay source and focused UE tests are not compiled or executed yet.** Imagination owns the shared heavy-test slot. Static checks can find syntax/format issues but do not establish this C++ behavior. Prior passing evidence for parent PR27 applies to its unchanged parent source, not this branch's idle-fire implementation.

When the heavy slot returns: build this exact committed revision with the existing one-action/two-CPU runner; run the focused fidelity tests, native model, skirmish model, bot/lifecycle, controls, native network/RPC and Coop regressions serially. Idle fire changes skirmish outcomes, so inspect any fixture failure before altering expectations; do not force winners, weaken phase flags or substitute old results. Retain exact source hashes and failed attempts. Stress measurements must be repeated only as part of the later combined workload, not relabeled as new evidence from the old model-only baseline.

Next, measure original production and economy under declared version, map, faction and game-speed conditions before replacing instant jobs or assigning role-specific balance values. Keep the Allied/Soviet loop and bots ahead of Yuri expansion. [Input automation planning](INPUT_AUTOMATION_PLAN.md) addresses the separate remaining controls acceptance gap.
