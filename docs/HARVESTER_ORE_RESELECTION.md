# Harvester ore reselection

Prepared follow-up to the existing finite economy (#22) and combined skirmish proof (#2). No public issue or publication action was made.

An existing Gather order ends when its patch runs out, even if other ore remains. This candidate continues that same order: retain a nonempty assigned patch; after depletion choose the nearest remaining patch to the harvester, breaking equal distances by resource ID. A loaded harvester returns to its own live refinery first. If all ore is gone, the order ends. Explicit Move, Stop and new Gather orders still replace it.

Missing resource records retain the disconnect policy: return saved cargo, then end the old order. Depletion of an existing patch is the only new trigger. Existing movement, extraction capacity, deposit rules, finite ore, command validation and replication remain in use. This adds no route finding, collision avoidance, new unit, balance tuning or general idle harvesting.

Four focused native groups cover nearest/tied/reordered resources, final-cargo return and conservation, competing miners, explicit-order and total-depletion behavior. A closer alternative must not steal a live assignment. The existing loaded-ore disconnect test also checks that the miner remains stopped after cargo return.

## Validation state

Source preparation and independent review are separate from execution. No candidate build, native case or rendered scenario has run. Earlier economy and HUD passes belong to their original checkpoints.

## Queued acceptance

After an explicit heavy-work allocation and fresh capacity/source checks:

1. Build this source with the existing bounded resource policy; bind the fresh DLL.
2. Run test-ore-reselection-model.ps1 with the installed EngineRoot explicitly configured: exact four cases, 90-second bound.
3. Run the exact 26-case skirmish suite and existing Stop cases with their original bounds.
4. Run Victory, Defeat and cooperative scenarios with unchanged assertions and 240-second runner/220-second pawn bounds. Inspect fresh captures and cleanup independently.

Retain failures and compare changed traces before retrying. Runtime economy pacing and bot interactions remain unverified. Keep the prepared human trial unchanged.
