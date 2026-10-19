# Bot construction around existing structures

## Functional gap and reference boundary

The [base-expansion policy](BASE_EXPANSION.md) permits construction near owned live structures, but the bot originally searched only twelve fixed HQ-relative sites. A crowded HQ candidate set could therefore prevent an affordable missing factory or refinery/miner recovery even when another owned structure offered a valid site.

The publisher-authored [RA2 manual](https://manualmachine.com/gamespc/commandconquerredalert2/1117998-user-manual/), hosted by a third party, describes structure placement near the player's other buildings and the Construction Yard's construction capability (printed pp18,38). [Phobos's own documentation](https://phobos.readthedocs.io/en/latest/New-or-Enhanced-Logics.html#build-area-customizations) also describes building-to-building adjacency. These references support the placement relationship, not this prototype bot's exact search algorithm. No original-game AI parity is claimed.

## Planner contract

- Preserve the existing lowest-ID live own HQ requirement and the entire original twelve-site search first. Ordinary successful HQ-first choices do not change.
- After every HQ candidate fails, search other already-committed live own HQ/refinery/factory records in ascending UnitId. At each anchor reuse the identical twelve offsets and their order.
- Teammate, enemy, neutral, dead and mobile records never extend the bot's placement reach. Missing own HQ still prevents all construction. A planned building in the same decision is only a collision reservation, not an eligible committed anchor.
- Retain the bot's existing 250 cm structure spacing, model world bounds, refinery miner-spawn preflight and same-batch miner reservation. These are existing prototype values. The model still independently enforces its 800 cm build radius, 180 cm structure separation, ownership, funds, capacity and spawn rules.
- Factory construction needs its existing one free record slot. It does not add a future Combat-spawn precondition; later Produce remains independently validated. Refinery construction still requires two free slots and a valid granted-miner spawn.
- Fallback does not create a new desire to expand. It only serves existing missing-factory, useful missing-refinery and last-miner-recovery decisions. Existing no-ore/no-cargo policy prevents an income-less refinery rebuild.
- Credits, command sequences, four-command decision budget, one-second cadence, five-second retry and twenty-second attack grace remain unchanged. The planner only emits ordinary commands; shared Submit remains authoritative.

For a valid authoritative snapshot at the existing twelve-live-record owner cap, each build search examines at most twelve committed anchors times twelve candidates. The two possible build searches each have that bound. This is not a measured performance result or a guarantee for arbitrarily oversized hand-authored snapshots.

## Reachability and tests

The public-command reproduction starts from seeded HQs, pays for an expanded refinery, earns ore through actual harvesting, then pays for four structures whose positions obstruct every old HQ candidate. Actual hostile combat causes the miner loss/free capacity used by the recovery cases. No resource, HP, position or record state is injected into that history.

This is a scripted reachable history followed by a fresh planner-intent check, not an autonomous default-map bot stall or hot takeover. The setup has its own command sequence history; submitting a chosen planner intent uses that owner's actual next sequence. Tests keep this distinction explicit. Separate prepared-snapshot controls cover ordering, invalid anchors and boundary conditions.

The older no-spawn recovery fixture originally obstructed only the HQ candidate envelope. Its prepared lattice is extended to cover every eligible committed anchor; explicit spawn-exhaustion assertions preserve the original zero-build/unchanged-credit oracle. A positive free-slot control retains the old lattice and proves that a genuine expanded spawn permits recovery, while the public-command histories establish actual model reachability.

Three exact native groups are registered under `RedalertProof.Skirmish.BotBaseExpansion`: `PlannerAndOwnership`, `PlacementAndReservations`, and `PublicRecovery`. The focused portable runner compiles the actual native test sources with the production planner/model and real digest bridge in normal, optimized and ASan+UBSan modes, and rejects missing, duplicate or extra registrations. Native Skirmish's exact manifest includes these groups without changing its runtime limit.

Cloud results establish only the executed source/model checks. UHT/Windows compilation, exact native tests, ordinary matches and a separately source-bound public-RPC fallback observation remain local acceptance gates after draft publication. Physical input, internet behavior, large-army performance and original-game AI calibration are not established.
