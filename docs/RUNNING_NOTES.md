# Running notes

## First implementation batch

Decision: Jared authorized parallel implementation agents and repository housekeeping, including issues and draft PRs. Three build lanes owned separate paths (Blender pipeline, UE scene import, command experiment), followed by a fourth agent for source review. Work stays on the isolated the designated non-system drive checkout and `feat/first-proof`; other projects and branches are preserved. No merge/deployment is authorized.

Changes: added original primitive blockouts and local review views; procedural FBX generation/round-trip validation; content-only UE 5.7.4 importer and map/cameras; dependency-free shared-command/two-client loopback experiment; generated-output ignore rules. Existing planning docs now reflect implementation authorization, the selected first-proof engine baseline, likely internet play, and large-army ambition without numeric caps.

Validation: Blender generation and all three FBX round trips passed. Captures were visually reviewed. Six Node tests and an independent combined loopback smoke passed. UE fresh and repeat imports succeeded with expected centimeter bounds; the final report additionally checks tagged actor count/labels. Actual the designated non-system drive derived-data/shader-temp locations appeared in engine logs. See [evidence and limits](FIRST_PROOF.md).

Resolved during this batch:

- Animated camera data overrode the wide still; a separate unanimated wide camera now gives a distinct view.
- Exported primitive transforms skewed dimension metadata/origins; apply transforms and validate world bounds plus ground contact on reimport.
- UE commandlet import initially crashed in Interchange/Slate. Explicit `FbxFactory` avoids that path; fresh and repeat imports subsequently passed.
- Source review found machine-generated Android File Server settings/token before staging. Removed them and disabled the unused plugin in this project; repeat import did not regenerate the settings. No token or raw log is intended for public source.
- Source review found stale imports could be mistaken for success. Each task must now report the expected imported object path and produce a StaticMesh before a success report is written.

Next steps (planned work, not reproduced bugs):

- [#1: review the normal/wide/motion blockouts](https://github.com/jaredbahr/RedalertUE5/issues/1), record approval or concrete revisions before finishing art.
- [#2: implement the narrow UE command and two-client combined-workload proof](https://github.com/jaredbahr/RedalertUE5/issues/2), then derive practical budgets from measurements.
- Confirm hosting/connectivity, target hardware and acceptance thresholds, permanent the designated non-system drive layout, broader cache/residual the system drive behavior, roster scope and binary/LFS allowance.

Housekeeping: duplicate checks found no existing issues/PRs before these two planned issues were opened. Preserve local editable generated scenes/captures for review; track procedural source and configuration, excluding caches/build/render intermediates and secrets. Record further decisions, validation and actionable follow-ups here as work proceeds. Do not call the toy loopback result UE networking or eight-large-army validation.

## Mobile review delivery

Packaged the inspected normal/wide PNGs as byte-identical copies under ignored `generated/delivery/`, and encoded the existing 16 motion frames as a 2-second H.264 MP4 (644 x 428, 8 fps) using installed Blender. Decoder metadata validation passed. No model changes or finishing were added; art still awaits user review.

Library saving was attempted through the current supported prepared-upload helper. It stopped before transfer/finalization with `Library prepare_uploads is not available`; no Library item IDs were confirmed. Retain all three local delivery files and captures. Do not upload binaries to public GitHub as a delivery workaround. This is a delivery-environment blocker, not a reproduced game bug.

The six command tests and two-client TCP experiment run outside UE in Node. They validate shared human/bot command rules, rejected invalid/ownership/resource/sequence requests, configurable command limits, host-owned damage and matching loopback snapshots. They do not prove UE replication, internet sessions, skirmish AI or a practical large-army budget.

## Native command and validation batch

Jared authorized parallel implementation/review and focused draft PRs. Seven agents contributed isolated source lanes or reviews, with root scheduling heavy UE/Blender work sequentially to share the PC. Work remains on separate the designated non-system drive worktrees; no merges, deployments or binary uploads occurred.

Added native host-owned commands, shared human/bot validation, phased movement/combat, snapshot replication, basic local RTS presentation, a separate generated gameplay map and bounded native test runners. The build uses the installed UE 5.7.4 toolchain, three parallel actions and no UBA. Compiler include/shadowing/API issues were corrected. The network fixture explicitly binds loopback and starts its active deadline only after both clients connect, with a separate startup guard.

Validation: four exact native test groups passed. A real UE host plus two UE clients passed on UDP loopback with 53 common revisions and no digest mismatches; the host accepted six human commands and two bot commands and rejected two ownership violations. A separate reviewer recalculated the evidence and found no scope blocker. Details and retained local report paths are in [native proof](UE_NATIVE_PROOF.md). Graphical controls, bot-specific outcomes, internet play and scale remain unverified; issue #2 is only partially addressed.

Separate review reproduced command enum coercion and insertion-order combat bias in the Node prototype ([#4](https://github.com/jaredbahr/RedalertUE5/issues/4), [#5](https://github.com/jaredbahr/RedalertUE5/issues/5)); [draft PR #8](https://github.com/jaredbahr/RedalertUE5/pull/8) contains the fix, with nine passing tests and a combined local smoke. Asset validation gaps are tracked separately in [#6](https://github.com/jaredbahr/RedalertUE5/issues/6) and [#7](https://github.com/jaredbahr/RedalertUE5/issues/7). Do not conflate Node TCP evidence, native UE UDP evidence or asset evidence.

## Command-contract review fixes

Reproduced two review findings in the Node toy prototype: an array command type `['attack']` was accepted through property-key coercion; mutual attackers starting 2.5 units apart observed partially applied movement, ending at 0/10 HP depending on insertion order. Exact string-enum validation now precedes field lookup. Tick phases derive movement from one starting snapshot, commit all movement, then derive damage from one post-movement snapshot and apply it together.

Validation: all nine Node tests passed with approved localhost access, including array/object/null rejection directly and over TCP, unchanged state/sequence after malformed envelopes, and mirrored physical duels that swap insertion order. The separate 24-tick/two-client smoke matched all 48 host snapshot copies with movement, combat and bot commands combined. These remain outside-UE loopback correctness checks, not internet or scalability results. No transport redesign or art changes were included.

Next: review the focused fix diff; continue engine-native command validation separately. Connection-slot identity, exact-next sequence/ack handling, transport ingress limits, client buffering and disconnect handling remain controlled-fixture limitations before any production adapter.

Tracked reproduced defects: [non-string enum acceptance #4](https://github.com/jaredbahr/RedalertUE5/issues/4) and [movement/combat ordering #5](https://github.com/jaredbahr/RedalertUE5/issues/5). Root independently reran all nine regressions successfully before this focused fix was proposed.

## Asset acceptance-evidence hardening

Cloud static review identified two P2 gaps: the UE importer accepted any Z half-extent between 10 and 5000 cm without gating X/Y or exact scale, and geometry-only/failed validation could leave old captures or a passed report appearing current. This lane adds explicit numerical geometry gates and current-run/source/export/capture identities, archives only prior pipeline evidence, and invalidates current reports before checks. No silhouette/palette finishing was performed.

Validation: three stdlib regression cases passed, covering independent geometry mutations, stale-success invalidation, historical-capture preservation and actual entrypoint failures before malformed manifest parsing. Fresh geometry-only Blender generation, all three FBX round trips, fresh/repeat UE import and the engine preservation/mutation probe passed. The corrected probe records yaw=180, pitch=roll=0 and rejects reversed facing, 10x uniform scale, X-only scale and +100 cm actor Z. Eight generated actors, the untagged-by-generator sentinel and an external package's bytes were preserved; 14 material slots were recorded across three meshes. The first probe accidentally tested tilt through positional Rotator arguments; named arguments and applied-rotation assertions corrected it before acceptance. Independent review also found stale probe reports could survive new generation or same-run imports; both paths now invalidate them before work begins.

The final current run identity and reports are retained under ignored generated/. These tests do not exercise material-color refresh, generic material collisions, altered FBX-local vertices/pivots or visual semantic front. Those remain unverified. Art remains unapproved blockouts; no source silhouette or palette finishing was performed. See ART_PIPELINE.md and UE_SETUP.md for reproduction and boundaries.

Latest integration execution: native follow-up build succeeded; clean 2643bf8 graphical warmup failed at 300.120s (303.082s total), no gameplay/screenshots. Designated storage peak cache/temp 18.44/2.65 MiB; eight audited the system drive cache roots had zero measured net growth during this run, without a zero-write guarantee. Startup affinity resets were observed and later runner policy hardened. Details: docs/INTEGRATION_SLICE.md. No main merge or art approval.

Diagnostic follow-up: one clean c93d1ea retry verified real scheduling/cleanup but timed out at300s before gameplay. Active shader compilation and a custom DDC graph omitting installed EnginePak were identified. Added project-local read-only EnginePak node without changing writable designated-storage paths or acceptance; not yet UE-executed. No driver/headless fault or Jared install/security action is established. Details: docs/INTEGRATION_SLICE.md.

## Graphical controls fixture completed

Clean f836d939a9093ae320fa425db5b6d2705571ad06 passed the real UE loopback host + rendered client + passive observer fixture. Warmup14.740s/scenario87.909s/total103.929s; all role completions and exits passed, with53matching shared revision digests. Shared viewport selection/clear/zoom and four accepted Move/Attack/Produce/Move commands produced replicated movement, damage, a new unit and credits200->150. Both independently isolated bot outcomes passed. Initial5records became6(5live). Both fresh1280x720 screenshots were viewed; diagnostic asset/lighting messages overlap HUD, and art remains unapproved.

Targeted fixes addressed omitted installed EnginePak cache, unsafe first-frame warmup shutdown, fractional cursor truncation, overwritten FPS cap, diagonal pursuit rounding and a redundant completion wait after a synchronous capture stall. All native model groups4/4 and runner helper checks8/8 passed; prior failed runs remain recorded. Current tested source differs from later documentation-only publication commits. See INTEGRATION_SLICE.md and UE_CONTROLS.md for exact identities, acceptance and local evidence.

Actual optimized shader mode1,30FPS cap and literal-only client setup were observed. Peak the designated non-system drive cache/temp37,369,645/12,472bytes;78policy polls/14affinity repairs, no cleanup errors; no owned UE process remains. Across06:29:33->06:48:03, seven audited the system drive roots had zero net changes; D3DSCache shrank123944bytes with unchanged file count. Shared totals do not prove zero writes or sole attribution.

This proves a small scripted functional fixture with sampled three-party agreement, not continuous participation, physical input/focus, internet sessions, skirmish bots or an army budget. Retain blockout approval before finishing art, and #11/#12 lifecycle/ingress follow-ups. Main remains unchanged; draft integration only.

## Evidence handoff and next independent slice

Executed clean source SHA: `f836d939a9093ae320fa425db5b6d2705571ad06`. Published draft #14 initially recorded this at documentation head `4517e5a95326582480d73ccc20c002f8760a8b86`; later handoff documentation does not change the tested implementation. Exact test totals:4/4native groups,8/8runner helper cases,4typed requests/accepted receipts,53matching three-party revisions,2validated and visually inspected PNGs. Result: generated/controls-<local-run>/result.json.

Preserved capture paths:
- ./generated/controls-<local-run>/screenshots/01-selected.png
- ./generated/controls-<local-run>/screenshots/02-moved-combat-produced.png

Resource measurements/configuration:1280x720, actual30FPS cap; two logical CPUs assigned to owned UE processes with78policy polls/14reset repairs (not continuous hard confinement). Peak observed writable designated-storage cache37,369,645bytes and run temp12,472bytes; these soft polling measurements exclude Saved/Intermediate, installed EnginePak and driver caches. Peak RAM, CPU utilization and attributable GPU load were not measured in the successful run. the system drive baseline/post results and remaining validation limits are above.

Library image delivery was checked with the current supported skill/actions and refreshed official helper. One ordered2image attempt returned `Library prepare_uploads is not available`, with no confirmed Library IDs. No retry or alternate upload was attempted. The existing image bytes remain local/ignored; no binaries were added to GitHub.

Proposed next small independent work: the disjoint owner-allocation slice of existing issue #11. Replace unchecked human NextOwnerId++ with server-derived allocation that excludes reserved bot owner100 and fails safely at numeric exhaustion. Focused tests should cover the99/100boundary, uniqueness across repeated allocations, unchanged bot ownership/resources and no overflow, preserving initial human IDs1/2 used by fixtures. Keep disconnect/reconnect policy as separate remaining work. This is a proposed next task, not implemented or a reason to expand the art roster. No merge is authorized by this handoff.
