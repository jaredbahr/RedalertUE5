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
| Power shortages affect production and some defenses/radar. | RA2 p18 | No power model; a Tesla mesh is only a display proxy. |
| Idle units engage approaching enemies; Guard adds return-to-position behavior. | RA2 pp21,23 | Previously required explicit Attack. This branch adds only the stationary, in-weapon-range idle-fire subset described below. |
| Refineries include a miner; Barracks and War Factories produce different categories. | RA2 pp38,41 | One starter Harvester is seeded independently. The [refinery-miner candidate](REFINERY_MINER.md) adds one unarmed Harvester per accepted refinery; source reviewed, build/runtime acceptance pending. One Factory still produces only generic Combat. |
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

## Validation and retained failure

Latest compiled/tested source is `3cc410c8125004207e9ea2e8164c8eed3b87e605`. The last build passed in four actions / 9.40 seconds total with installed UE5.7.4. Gameplay model source is unchanged from `1f91ea79f623bf5a9c7a8eee470427cacd071eb3`; the later commit changes only stale-target handling inside the scripted match fixture. All tests started with clean source. Model/scenario jobs used two logical CPUs; the existing graphics warmup used four. Jobs were serialized, with existing deadlines, D: cache/temp settings and owned-process cleanup. Polling policies are not hard confinement.

Evidence below is under ignored `D:/UserTemp/RedalertUE5-fidelity-baseline/generated/`. Short revision labels identify the exact full hashes above; later commits update documentation only.

| Gate | Tested revision | Result | Directory |
| --- | --- | --- | --- |
| Focused idle combat | `3cc410c` | 4/4, zero failures, 13.161s | `fidelity-model-20261016-220835-543` |
| Existing command model | `3cc410c` | 7/7, zero failures, 12.652s | `native-model-20261016-220848-729` |
| Existing skirmish/bot model | `3cc410c` | 14/14, zero failures, 12.213s | `skirmish-model-20261016-220901-394` |
| Existing selection/group/camera | `3cc410c` | 10/10, zero failures, 12.507s | `group-controls-model-20261016-220913-621` |
| Native two-client consistency | `1f91ea7` | Pass, 59 matching revisions | `native-network-20261016-220417-901` |
| RPC flood/healthy-client isolation | `1f91ea7` | Pass, 17.633s | `rpc-ingress-20261016-220449-041` |
| Offscreen controls compatibility | `1f91ea7` | Pass, 58.495s, 55 matching revisions | `controls-20261016-220506-712` |
| Victory | `3cc410c` | Pass, winner0, 65.563s | `small-match-victory-20261016-220926-160` |
| Defeat | `3cc410c` | Pass, winner1, 50.857s | `small-match-defeat-20261016-221031-736` |
| Cooperative match | `3cc410c` | Pass, winner0, 85.204s, 308 matching revisions | `small-match-coop-20261016-221122-603` |
| Disconnect/rejoin/rematch lifecycle | `3cc410c` | Pass, 115.362s, 422 matching epoch/revision digests, two exact terminal agreements | `session-lifecycle-20261016-221247-839` |

The lifecycle run ended both epochs in human defeat (winner1), then accepted rematches into epochs2/3 and fresh sequence1 commands. Its unchanged runner requires valid terminal agreement, not a particular winner; it also verified deferred rejoin, full-slot rejection and stale-epoch rejection. The different epoch2 outcome versus earlier historical runs is a real combat-behavior observation, not a human-victory claim. Separate Victory and Coop tests retain their explicit winner0 requirements and passed. No terminal state, health or economy was forced to obtain these results.

Retained failed attempt: `small-match-victory-20261016-220605-239` at clean `1f91ea7` failed in 51.121s when an Attack was rejected with `target` after the selected enemy ceased to be valid. Every phase flag was set, but the fixture exited before a terminal result (winner -1); winner1 logged afterward belongs to teardown, not a completed defeat scenario. The narrow repair waits for the latest replicated target to be dead/missing, logs the observed state and clears only that target's fixture bookkeeping/receipt latch. It never resets controller sequences, ignores other rejection reasons, grants resources, changes gameplay damage or weakens the 220-second scenario deadline, phase flags, screenshots or winner assertions. The successful Victory rerun exercised `PROOF_SKIRMISH_RETRY ... replicatedTarget=dead` before completing normally. The failed report/logs remain retained.

The first build at `319dd9c` passed in 43.89s; independent review then found that a sub-epsilon accepted Move could prevent exact completion. `1f91ea7` added an endpoint fix/regression and rebuilt in 9.09s before tests. No failed builds or failed model tests occurred in this batch. Earlier model passes at `1f91ea7` remain retained separately from the final model reruns above. Victory/HUD capture was visually inspected. No visible game or engine input-injection fixture was launched; physical input, focus, Internet and performance remain unverified.

Fourteen existing ignored content files were reused with matching SHA256. No proprietary assets or binaries were uploaded. The eight-root C: audit since the previous controls observation found UE5.7 Saved -2 bytes, D3DSCache -32768 bytes, and six roots unchanged; this shared-PC byte-count observation is neither exhaustive nor exclusive attribution (`c-drive-after-fidelity.json`). It does not establish zero C: writes. The earlier legacy model stress measurements are not new idle-acquisition or combined-scale evidence.

Next, measure original production and economy under declared version, map, faction and game-speed conditions before replacing instant jobs or assigning role-specific balance values. Keep the Allied/Soviet loop and bots ahead of Yuri expansion. [Input automation planning](INPUT_AUTOMATION_PLAN.md) addresses the separate remaining controls acceptance gap.
