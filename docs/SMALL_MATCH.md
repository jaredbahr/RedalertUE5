# Small skirmish prototype

The opt-in small match now supports building a base, harvesting finite ore, producing combat units, fighting an economy-driven bot, and a stable victory/defeat result. This is a functional prototype using existing blockouts, not approved final art or balance.

Jared's earlier “it works. lot of work to do though.” confirms general usability of PR19's opened slice only. Individual manual controls, Alt-Tab, and the new match have not all been human-verified. That practice session exited normally at 17:59:21 UTC on 2026-10-16, before its guard timeout; its close method was not established. No new visible game was opened for this milestone.

## Run and controls

From the prepared D: checkout, run `./unreal/Scripts/start-small-match.ps1`. This wraps the existing guarded interactive launcher with `-ProofSkirmish`: five-second warning, 15-minute default session limit, 30 FPS, two-CPU/Normal polling policy, and identity-checked cleanup. Generated map/assets must already be prepared; binaries and generated content are intentionally excluded from Git.

| Input | Action |
| --- | --- |
| Left click | Select an owned unit |
| R / F at ground cursor | Build refinery / factory |
| P | Produce a combat unit from selected or first own live factory |
| Selected Harvester, right click ore | Gather and return cargo to refinery |
| Selected Combat, right click enemy / ground | Attack / move |
| Wheel / Escape | Zoom / exit |

The HUD shows credits, delivered ore, selected role/cargo, command receipt, role labels/HP, and terminal result. Selection is one unit at a time; camera panning, formations, collision/pathfinding, polished readability and group controls remain work.

## Provisional rules

One human, or two cooperative human owners on team 0, fights bot owner 100 on team 1. Each starts with HQ, Harvester, 350 credits and 500 finite ore. Refinery costs 100, factory 150, combat unit 50. Structures must be within 800 cm of the owner's live HQ and at least 180 cm from other live structures. Production requires the owner's live factory and a free spawn position. The cap is 12 live records per owner, including structures; this is a fixture limit, not a measured performance budget.

Harvesters travel to ore, extract up to 50 cargo immediately on arrival, then travel to a live owned refinery to deposit and repeat. Income is recorded only on deposit. HQ has 300 HP, other structures 200, mobiles 100; existing speed 150, range 300 and damage 25 remain provisional. Structures cannot move, and only Combat attacks opposing teams. Destroy all opposing live HQs to win; simultaneous loss is a draw. Terminal simulation and commands freeze.

The bot builds, gathers, spends, produces and attacks through the same authoritative command model as humans. It decides once per second with at most four commands, has a 20-simulation-second attack grace, and retries attack orders. No direct resource grants bypass the economy. The map, role proxies, unit scale and balance are provisional. Large-delta pure-model tests are not real-time combat timing measurements; runtime advances at 0.1-second steps.

## Integration and validation

Based on PR19 `fd7eb1f`, with local cherry-picks of owner allocation from PR15 and RPC ingress from PR20. Those dependency PRs remain open; no remote merge occurred. Five independent economy, bot, rules/UI, runtime and harness workers contributed, with cross-review and one serialized build/test lane.

Installed UE 5.7.4 built the final C++ at `73d908b546559bc4dacc944b330fa9693dda4886` successfully (7 actions, 14.44 seconds). Later commits `e88961a` and `dd19fea` repair only the PowerShell harness. All results below are local acceptance evidence, not Internet, physical-input or performance claims. Paths are relative to `D:/UserTemp/RedalertUE5-small-match/generated/` and remain ignored local artifacts.

| Gate | Result | Evidence directory |
| --- | --- | --- |
| Existing native command groups | 7/7, zero failures | `native-model-20261016-181540-309` |
| New economy, bot and terminal-rule groups | 7/7, zero failures | `skirmish-model-20261016-181641-616` |
| Rendered Victory | Pass, winner 0, 73.082 s | `small-match-victory-20261016-182455-198` |
| Rendered Defeat | Pass, winner 1, 57.879 s | `small-match-defeat-20261016-182646-483` |
| Cooperative two-client match | Pass, 222 matching common revision digests, 80.275 s | `small-match-coop-20261016-183029-101` |
| Existing rendered controls | Pass, 55 matching revisions, 74.848 s | `controls-20261016-183218-675` |
| Existing native two-client fixture | Pass, 51 matching revisions | `native-network-20261016-183333-591` |
| RPC flood close and healthy-client isolation | Pass, 64 admitted / 128 sent, 29.078 s | `rpc-ingress-20261016-183411-867` |

Each directory contains `result.json` and logs. Pure groups ran at `b484529`; authoritative model/bot/rules/tests were unchanged by subsequent Pawn/HUD/harness repairs. Victory/Defeat ran at `73d908b`; cooperative and legacy regressions at `dd19fea` with the same compiled gameplay. The lightweight controls-policy suite also passed all eight cases.

The rendered fixtures project cursor positions through the same action handlers, check accepted receipts plus replicated build/gather/production state, bot activity and actual allied HP loss, and require the expected terminal result. Cooperative owner 2 sends and verifies its own four build/gather/produce commands. This is a dedicated loopback host, one offscreen rendered client and one NullRHI client. Each successful match has base/result PNGs; all six were visually inspected. Cooperative result SHA256: `5E2A6FAF9554D035BFA58D81B623FCC81A61F7407617FFBEEC37CFC9EE68FFC1`.

Retained failures explain the repairs: `small-match-victory-20261016-181725-884` completed the loop but lost; scripted defensive tactics were improved without weakening model/bot rules. `small-match-victory-20261016-182229-497` exposed a stale snapshot budget after a production receipt; automation now waits for the replicated unit. `small-match-coop-20261016-182744-402` hit PowerShell's reserved `$Host` variable. Its host survived immediate cleanup despite an empty cleanup-errors array, so that report is not successful-cleanup evidence. A following UDP preflight correctly refused the occupied port before starting another server. The exact retained PID/start/path/project identity was verified and only that process stopped; see `coop-startup-cleanup-recovery.json`. The variable was renamed and live-process path verification now retries for up to two seconds before failing explicitly. The original cleanup miss's precise cause was not reproduced. Final runs exited normally with no remaining owned engine/build processes.

## Storage, boundaries and next acceptance

Fourteen existing generated assets were reused with matching SHA256 records in `reused-assets.json`; no new art, installs or binary uploads. All builds, project caches, logs and captures live on D:. Cooperative sampled cache peak was 37,373,710 bytes and temp peak 12,904 bytes, below its 2048/1024 MiB soft polling guards. C: audits (`c-drive-before-small-match.json`, `c-drive-after-small-match.json`) found UE Saved +25 bytes, D3DSCache -32,768 bytes, and the other six monitored roots unchanged. This shared-PC observation is not exhaustive attribution or a zero-C-write promise.

Next acceptance is Jared playing the new loop, checking controls/focus, ore route and readable outcomes, then reviewing map density and role/scale presentation. Persistent session cleanup/reconnect, LAN/Internet conditions, loss/latency, active-unit performance, final faction roster, Yuri and four/eight players remain separate milestones. Army scale and LAN-versus-Internet priority remain decisions for Jared. Related issues: #21, #22, #23, #24; lifecycle #11 and dependency #12 remain separate. No merge, deployment, paid tooling, hosted Actions or fee-bearing service is part of this change.
