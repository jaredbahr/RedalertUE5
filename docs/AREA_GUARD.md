# Bounded Area Guard prototype

Status: implementation candidate; portable, native, and runtime acceptance are recorded separately. Human G binding is pending. Base main `58aeb542d83d0a7c21963b4a4efacaef6a6f8576`, full tree `cfb3bb779fb0f57fc3e40d38bb145fbebbc154fe`, Source `9a122f896cba998fd1ef6567c690724754ab0191`.

## Reference boundary

- The publisher-authored [RA2 manual, printed pp21 and 23](https://manualmachine.com/gamespc/commandconquerredalert2/1117998-user-manual/) calls the G command Guard mode and describes engagement followed by return to the original position. It gives no acquisition radius, pursuit leash, target-retention rule, or timing. This is a third-party-hosted original document, not a verified EA-hosted download.
- EA's released [Red Alert 1 engine, FootClass::Mission_Guard_Area](https://github.com/electronicarts/CnC_Remastered_Collection/blob/f1f0d42bc2dcd06d5d1df943c6150ab34bf307ae/REDALERT/FOOT.CPP) documents scanning from home out to twice weapon range and a home-relative travel distance of one weapon range. Its implementation schedules mission updates and checks movement/firing conditions; it does not establish a strict continuous hard cap. This is original engine lineage evidence, **not RA2 implementation evidence**. No source from that GPL repository is copied into this project.
- [Phobos's own Yuri-engine documentation, target scan/guard range customizations](https://phobos.readthedocs.io/en/latest/Fixed-or-Improved-Logics.html#target-scan-guard-range-customizations) identifies Area Guard as the guard-mode command, with default GuardRange derived from weapon range and doubled effective scanning before a map-cell cap. Current upstream inspected at `0bd7b3b299b77679fefb299c6e89db2bc75e729e`. Its extension settings and cap do not prove this project's desired disengagement rule or establish any cell-to-centimeter conversion.
- Third-party Rules.ini mirrors contain GuardModeStray and ApproachTargetResetMultiplier, but their version/provenance and exact application are unverified. They are not used to choose gameplay values. In particular, a target-approach recalculation multiplier is not evidence for an Area Guard leash.

**Scoped decision:** implement a qualified prototype AreaGuard with scan = 2 * existing AttackRange and strict home-relative travel <= existing AttackRange. The numeric values remain derived from the existing prototype weapon scale. No BuildRadius dependency, new timing, original-cell conversion, or measured RA2 parity claim. Original RA2/Yuri observation under declared version/unit/game-speed remains a later calibration gate.

## Public command and observable behavior

1. Append `AreaGuard = 10`; no existing enum value changes. Only owned live Combat in a playing skirmish accepts it. The command's target must be INDEX_NONE and destination exactly zero: neither chooses the post. The server captures the current position. Existing envelope, sequence, rate, ownership, revision, and epoch-adapter rules remain.
2. At the post, acquire the nearest live opposing-team target within the closed 2*AttackRange sphere centered on the post; ties use UnitId. Exclude missing teams. This admits targets outside firing range, giving actual public-command movement beyond current idle defense.
3. Retain that target while it remains live, hostile, and in the post-centered scan sphere. Acquire/chase using the common pre-movement snapshot and existing movement speed. Firing stays post-movement/pre-damage with strict weapon range. Revalidate the retained target against post-move scan/hostility; invalidation clears target and marks return without a second movement budget. Do not grant extra damage or a second idle shot.
4. The guarding unit center-point path remains in the closed AttackRange sphere about its saved post. Before applying a proposed chase step, if its endpoint would leave that sphere (including a rounded endpoint), abandon the target and use this tick's ordinary movement budget to return home instead. The sphere is convex, so in-bounds endpoints also bound each linear step. Large DeltaSeconds must not overshoot the leash.
5. Dead/missing/nonhostile/out-of-zone targets also trigger return. After simultaneous damage is fully applied, surviving guards clear newly dead targets and publish return intent without another movement, shot, or acquisition. While returning, do not chase, reacquire, or fire. Reach the exact post using the existing Move-style endpoint rule, including its existing sub-epsilon endpoint snap. Resume scanning on the next advance, preventing same-tick reacquisition from erasing the return. This conservative prediction/return rule is a declared prototype choice, not an exact copy of EA's scheduled mission behavior.
6. A fresh accepted AreaGuard reanchors at the current authoritative position. Accepted Move/Attack/AttackMove/Stop clears AreaGuard completely. Rejected commands preserve it. Stop and ordinary idle defense remain unchanged. Harvesters/buildings do not receive this command; original miner special behavior is outside scope.
7. Removing the target owner preserves the surviving unit's post and puts it into return, unless the match becomes terminal. Death, terminal order clearing, Reset, and reseed clear all AreaGuard metadata. A fresh late observer sees authoritative post, target, and return state without needing local order history.

## Visible state and digest commitment

Add four fields to FProofUnitRecord: bAreaGuard=false, AreaGuardPost=zero, AreaGuardTargetId=INDEX_NONE, bAreaGuardReturning=false. These are authoritative command state, not client-selected destinations. Store the post/target/phase once in the unit record; the private order identifies AreaGuard, avoiding duplicate state representations.

Every nondefault combination must affect ComputeDigest, including disabled/noncanonical data in snapshot-only fault tests. All-default records preserve existing digests. Replication uses the existing UPROPERTY snapshot path. Extend the all-visible-field replay serializer with all four fields and test each field mutation, canonical clearing, copied late-observer snapshots, deterministic replay, and legacy projections that omit only the explicitly new fields/digest.

## Real bot producer

During the existing first 20 seconds, after existing economy/production intents, issue AreaGuard once for each live owned Combat as remaining room permits in the unchanged four-command decision budget. Stable ID order plus skipping already issued AreaGuard avoids starvation and repeated reanchoring. No pre-grace offensive Attack. At the existing grace boundary, the existing attack scheduler begins replacing AreaGuard as its unchanged bounded fair queue reaches each unit; this does not promise every unit changes on the same decision. An issued planner intent is not a receipt; one-shot guard issuance uses the existing ordinary submit pipeline, not a new retry/acknowledgment subsystem. Cadence, retries, costs, spawn/record capacity, production and economy priorities remain unchanged. Report actual deterministic match terminal differences rather than fitting old winners.

The generic authoritative command RPC already transports FProofCommand. Human G input, group binding, cursor, and HUD feedback are pending a separate integration after PR64; this slice does not claim those paths.

## Implementation boundaries

Production (3):
- unreal/Source/RedalertProof/Commands/ProofCommandModel.h
- unreal/Source/RedalertProof/Commands/ProofCommandModel.cpp
- unreal/Source/RedalertProof/Skirmish/ProofSkirmishBot.cpp

Tests/manifest/support (7):
- unreal/Source/RedalertProof/Tests/ProofAreaGuardTests.cpp (new)
- unreal/Source/RedalertProof/Tests/ProofSkirmishBotTests.cpp (update explicit pre-grace/command-count premises; retain economy and cap oracles)
- unreal/Source/RedalertProof/Tests/ProofBotMinerRecoveryTests.cpp (retain exact depletion/recovery economic oracles, assert one Produce plus three bounded guards for existing combat fixture)
- unreal/Source/RedalertProof/Tests/ProofReturnCargoTests.cpp (unknown enum sentinel 10 becomes 255; preserve the malformed-command oracle)
- unreal/Scripts/test-skirmish-model.ps1 (exact expected names only)
- tests/run_area_guard_tests.sh (new, compiles actual native test source using existing enum-shaped Unreal shim and MD5 bridge)
- tests/bot_match_replay_tests.cpp (all-visible serialization and AreaGuard count/producer assertions only)

Documentation (2):
- docs/AREA_GUARD.md (new)
- docs/FIDELITY_BASELINE.md

No Pawn, HUD, selection, control-group dispatch, GameMode, assets, resource budgets, or runtime-wrapper files claimed. Any required expansion will be reported before edits.

## Acceptance plan

- Actual public Submit/Advance fixtures: outside-weapon acquisition/chase, strict scan/leash boundaries including nextafter and diagonal/negative/world-edge positions, target moves across leash, large and partitioned advances, return after death/disappearance/owner removal, exact home arrival, target retention/switch and nearest tie, no return-phase firing, simultaneous lethal damage, ownership/role/malformed/sequence/replay/rate rejection preservation, all explicit overrides, repeated reanchor, terminal/reset/reseed, and economic conservation.
- Native groups planned under RedalertProof.Skirmish.AreaGuard: AdmissionAndReplay; AcquireChaseReturn; LeashAndTargetLoss; OverridesAndLifecycle; DamageAndConservation; DigestAndLateObserver. Add bot-specific PlannerAndGrace and PublicModelPath under the same prefix. Freeze exact names with implementation, not a relaxed count-only assertion.
- Permanent portable focused runner: normal, optimized, ASan+UBSan; actual all-name skirmish compilation and broad model regressions; real MD5 path. Full deterministic bot-match matrix with every original conservation/terminal assertion retained and outcomes reported.
- Deliberate faults must prove assertions reject: scan/weapon range conflation, unlimited chase, missing return, early reanchor, double damage, foreign acceptance, stale/partial digest omission, and changed grace/budget. Select minimal controls tied to implemented risks, not assertion-count goals.
- Independent source review before freeze/publication. Local exact-source UHT/build and native manifest; separately coordinated local-owner opt-in diagnostic composition for a focused host/client AreaGuard trip showing acquisition, bounded chase, return and copied common-revision guard state/digest; default Victory/Defeat/Coop compatibility under unchanged admission/deadlines. Human G and real input remain pending.

## Cloud validation recorded for this candidate

- Actual native AreaGuard source compiled through the portable shim and real MD5 bridge: all eight exact groups passed, 886 checks per mode, in normal, optimized, and ASan+UBSan runs. Independent review repeated these runs. LeakSanitizer is unsupported in this executor's traced process environment, so leak detection was explicitly disabled; no leak-check pass is claimed.
- The broad portable selection includes every one of the 54 exact names in the updated native Skirmish manifest, plus existing command/combat/control-model coverage: 87 groups, 5,437 checks, zero failures in each of the three modes. The six existing bot groups retain exact economy and capacity oracles and pass 390 checks per mode.
- The full candidate bot-match replay includes every new visible field and accepts seven guard intents per side in each of six deterministic setups. Each mode passed 911,384 checks, with zero rejected commands and identical complete candidate replays. Existing conservation and terminal assertions were retained. Measured terminal simulated times/winners match the baseline for this matrix: default and reversed-submit 85.2s/team1; horizontal 67.3s/team1; vertical 72.0s/team0; short horizontal 48.9s/team1; long diagonal 89.1s/team1. These are simulated seconds, not runtime or performance measurements, and do not establish unchanged outcomes outside this matrix.
- A separate old-command differential deliberately uses the baseline bot producer for both model versions. Commands/results, ordering, revisions, and every pre-AreaGuard visible field match byte-for-byte in all six full traces; the candidate also requires the new fields to remain canonical defaults. This separate projection does not omit old fields or claim the new producer's sequence/revisions equal the baseline.
- Production/bot fault controls exercise scan range, leash, double damage, ownership, post/target/return digest fields, return-before-reacquisition, stale override metadata, repeated reanchoring, premature offense, and changed command budget. The permanent focused runner separately rejects an empty test registry instead of accepting a count-only success.

These cloud results establish model/source regression evidence. UHT/native compilation, the 54-name Windows Skirmish run, exact-production Victory/Defeat/Coop, and source-bound host/client chase/return observation are still required. An observer composition must use the published match epoch, actual accepted-command prestate, actual pre/post displacement, and common-revision snapshots. Invoking the existing command adapter in a diagnostic does not prove physical G input. Human G integration and original-game calibration remain future work.
