# Stop selected units

Status: the bounded slice passed build, all 62 model cases, Stop/default controls, Victory/Defeat/Co-op and lifecycle at clean b10e1ff. See [exact-source validation and retained failure](STOP_VALIDATION.md). Native and physical S input remain unverified.

The original RA2 manual's Basic Keyboard Functions table and the Yuri manual's matching table document Stop on S. References: [RA2 manual scan](https://www.scribd.com/document/760314562/Red-Alert-2-English-3-Manual), [Yuri manual OCR](https://archive.org/stream/Command_Conquer_RED_ALERT_2_YURI_S_REVENGE/Command__Conquer_RED_ALERT_2_YURI_S_REVENGE_djvu.txt). These are third-party hosts of publisher-authored material; no manual or game asset is uploaded here.

## Player behavior

Select one or more owned mobile units and press S. The existing bounded command queue replaces their pending Move/Attack intents with Stop; unselected units retain their orders. Dispatch remains two commands per 0.1-second interval with at most 64 pending intents. A failed batch leaves the previous queue intact.

The host validates each unit and removes only its current order. Stop preserves position, health, cargo, credits and ore. A stopped miner retains its load and can resume Gather, including a return from a depleted patch. Skirmish Combat still fires at eligible enemies already in range: Stop is not hold-fire or a full Guard implementation. Structures are excluded. The existing bot planner's strategy is unchanged; trusted bots share the same Stop validator.

The enum appends Stop at ordinal 6 without changing existing values. Snapshot/RPC payload shapes, production/economy defaults, caps, budgets and match rules are unchanged. Server-derived identity, epoch, sequence, ingress and command allowance remain authoritative.

## Acceptance

- Build exact clean source, then require all 11 named Stop cases, existing native/skirmish/group/fidelity cases and HUD layout cases.
- Run existing controls with and without the opt-in -StopControls switch. Keep the original four requests, gameplay assertions and two captures. Opt-in adds real Move 5, Stop 6 and resumed Move 7 plus fresh 03-stop.png and 04-stop-resumed.png.
- Require accepted actual host commands, matching whole semantic snapshots on host/rendered/observer, real movement before Stop, at least three common stopped revisions spanning one actual host-simulated second, unchanged stopped position/cargo, and movement toward the accepted resumed destination afterward. Configured cursor targets must agree with actual payloads within the existing 2 cm projection contract.
- Keep the existing client 55-second, host 20/24-second, observer 23-second and runner 90-second bounds. No clock reset, forced gameplay state or timeout relaxation.
- Recheck normal skirmish outcomes, refinery mining/deposit evidence and lifecycle before integration. Retain every failed attempt and stop the queue for diagnosis.

Opt-in diagnostics have 32 host command events, 1024 snapshot rows per role, 32 KiB per JSON row and 4 MiB combined command/state payload per helper. Overflow, malformed/contradictory data, duplicate accepted sequences or absent phase evidence fail acceptance. Default collection is off.

Synthetic parser passes are policy evidence only. The shared-handler runtime scenario does not press the native S callback, inject input or prove Jared's physical keyboard/focus behavior. Actual human S-key testing remains in [the manual checklist](INTERACTIVE_SESSION.md). Generic proxies, approved art, Internet play and performance remain separate; issues #2, #17, #18 and #33 retain unsatisfied scope.
