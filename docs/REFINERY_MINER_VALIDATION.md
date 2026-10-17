# Refinery miner validation

The bounded slice passed at clean commit `8ab9b55c00daf6dd46f807cd3eec5e17f03a4ad3`. [The accepted batch ledger](evidence/refinery-miner-accepted-batch.json) records result/log hashes, exact source, screenshots and granted-miner checkpoints. Raw logs and captures remain locally ignored under `D:\UserTemp\RedalertUE5-integration-candidate\generated`.

| Gate | Result |
|---|---|
| Build | Passed, 10.97 seconds |
| Skirmish model | 22/22, 12.861 seconds; grant/replay, existing cargo, capacity/ID/spawn atomicity, conservation, reseed and bot planning/rotation |
| Existing model suites | Native 7/7, group controls 11/11, fidelity 4/4; 44 total model cases passed |
| Victory | Passed; expected winner 0; 83.221 scenario seconds |
| Co-op | Passed; expected winner 0; 103.159 scenario seconds; host and both receiving clients |
| Defeat | Passed; expected winner 1; 57.241 scenario seconds |
| Legacy native network | Passed; 59 common revisions; ownership rejection, human/bot commands, move/damage/production |
| Legacy rendered controls | Passed; 44.973 scenario seconds, 56 common revisions, two fresh captures |
| Lifecycle | Passed; 124.567 scenario seconds; terminal outcomes, two rematches, stale requests, reseed, full seat, disconnect/rejoin |
| Independent review | Core/bot/helper/parser and runtime results reviewed independently; all six skirmish and two legacy PNGs inspected at native size |

The accepted human/bot refinery-miner pairs were 8/9 and 5/6 in Victory and Defeat. Co-op pairs were owner 1: 5/6, owner 2: 9/10 and bot 100: 12/13. Every grant added exactly two records, charged 100 once, preserved existing state, received an accepted Gather, loaded 50 cargo from finite ore and deposited 50 at an owned live refinery. The opt-in parser required matching identity, loaded-cargo and post-deposit semantic snapshots at common epoch/revisions across every required role. Explicit contributor IDs distinguish granted-miner income from the starter miner.

Base/result PNGs show match context. Defeat's base capture selects granted Harvester 9 with HP 100/Cargo 0 after delivery. Loaded cargo and exact extraction/deposit attribution are log/snapshot evidence; these are not screenshots of every mining phase. The 35 synthetic parser cases establish policy checks only.

Jobs ran sequentially within unchanged bounds. All result cleanup gates passed where present; the older native runner has no cleanup field. A separate process check found zero remaining engine/build/shader-worker processes before releasing the heavy slot. No further engine job is needed for documentation publication.

## Retained first failure

The first exact-source build at clean `e40c59be0fff9efc7b6b556a124003e0f63c211a` passed in 15.42 seconds. Its 22-case skirmish run finished in 12.893 seconds with 21 passing and one failed BotRefineryPlanning assertion. The queue stopped there; no renderer or replication result is attributed to that source.

The no-spawn test's 400 cm blocker grid left legal centers at (+/-600,+/-600), 282.843 cm from the nearest blocker versus 260 cm clearance. The correction uses a 300 cm grid and explicit no-spawn preconditions at all 12 planner sites. The final factory/no-refinery expectation, gameplay code, cap, deadlines and acceptance assertions were preserved. [The retained attempt ledger](evidence/refinery-miner-first-model-attempt.json) keeps the failed assertion and raw-result hashes.

This proves a generic prototype behavior slice. Physical input, Internet play, performance, approved faction art and calibrated RA2 balance remain outside this evidence. Issues #2, #17, #18 and #33 retain unsatisfied scope.
