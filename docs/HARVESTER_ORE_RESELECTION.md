# Harvester ore reselection

Follow-up to the finite economy (#22) and combined skirmish proof (#2).

An existing Gather order ends when its patch runs out, even if other ore remains. This candidate continues that same order: retain a nonempty assigned patch; after depletion choose the nearest remaining patch to the harvester, breaking equal distances by resource ID. A loaded harvester returns to its own live refinery first. If all ore is gone, the order ends. Explicit Move, Stop and new Gather orders still replace it.

Missing resource records retain the disconnect policy: return saved cargo, then end the old order. Depletion of an existing patch is the only new trigger. Existing movement, extraction capacity, deposit rules, finite ore, command validation and replication remain in use. This adds no route finding, collision avoidance, new unit, balance tuning or general idle harvesting.

Four focused native groups cover nearest/tied/reordered resources, final-cargo return and conservation, competing miners, explicit-order and total-depletion behavior. A closer alternative must not steal a live assignment. The existing loaded-ore disconnect test also checks that the miner remains stopped after cargo return.

## Verified acceptance

At clean `3d4997d1df45fa782caf1acb0a0e74852be89cba`, the fresh editor build, exact four focused ore groups,26-case skirmish suite and11-case Stop suite passed. The four ore groups are included in26; these counts are not disjoint. Victory, Defeat and cooperative scenarios passed their unchanged assertions and original240-second runner/220-second pawn bounds. Root and independent review inspected all nine fresh native-size captures; cooperative evidence included303 matching revisions and ten context phases.

That gameplay checkpoint has Source tree `13f7555f8dfc5e0fca8c8203326fd59920c3a02c` and DLL `CBC126535211B83DF6F9439C1EFA49229CC8B428346C138246093ECDEFB31088`. Original broad/runtime acceptance remains bound to it.

The subsequent test-only commit `3610f230d467ff8935b9b916643ab347b06f1b00` adds a discriminating case to NearestAndTie: the nearest replacement differs at the refinery and at the exhausted patch, so reselection must use the actual post-deposit departure position. Its fresh build passed in12.643 seconds and the unchanged exact four-group runner passed in38.529 seconds, within90 seconds, with zero warnings/errors. Source tree `5b99452082dfa3c8eafd12127bd41b0836b61ee2` binds DLL `26284668A74FD9BAF1B65D269BC9BBF0A46AFE503D5A39893BBA9DBEF996C252`. No production/config/project file changed in that test delta; unrelated broad/runtime suites were not rerun. Later documentation-only updates preserve those inputs.

All allocation cleanup checks passed. Raw logs, reports, resource-policy receipts and captures remain private in ignored storage. The prepared human trial stays unchanged. These synthetic/model checks do not establish physical input/focus, internet play, army-scale performance, final art, balance or broader HUD acceptance. Existing crowded-HUD and terminal selected-HP0 presentation limits remain open. The graphical scenarios provide full-loop regression evidence; focused production-model native cases directly demonstrate the new depletion behavior.
