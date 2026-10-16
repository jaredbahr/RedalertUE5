# Running notes

## First implementation batch

Decision: Jared authorized parallel implementation agents and repository housekeeping, including issues and draft PRs. Three build lanes owned separate paths (Blender pipeline, UE scene import, command experiment), followed by a fourth agent for source review. Work stays on the isolated D: checkout and `feat/first-proof`; other projects and branches are preserved. No merge/deployment is authorized.

Changes: added original primitive blockouts and local review views; procedural FBX generation/round-trip validation; content-only UE 5.7.4 importer and map/cameras; dependency-free shared-command/two-client loopback experiment; generated-output ignore rules. Existing planning docs now reflect implementation authorization, the selected first-proof engine baseline, likely internet play, and large-army ambition without numeric caps.

Validation: Blender generation and all three FBX round trips passed. Captures were visually reviewed. Six Node tests and an independent combined loopback smoke passed. UE fresh and repeat imports succeeded with expected centimeter bounds; the final report additionally checks tagged actor count/labels. Actual D: derived-data/shader-temp locations appeared in engine logs. See [evidence and limits](FIRST_PROOF.md).

Resolved during this batch:

- Animated camera data overrode the wide still; a separate unanimated wide camera now gives a distinct view.
- Exported primitive transforms skewed dimension metadata/origins; apply transforms and validate world bounds plus ground contact on reimport.
- UE commandlet import initially crashed in Interchange/Slate. Explicit `FbxFactory` avoids that path; fresh and repeat imports subsequently passed.
- Source review found machine-generated Android File Server settings/token before staging. Removed them and disabled the unused plugin in this project; repeat import did not regenerate the settings. No token or raw log is intended for public source.
- Source review found stale imports could be mistaken for success. Each task must now report the expected imported object path and produce a StaticMesh before a success report is written.

Next steps (planned work, not reproduced bugs):

- [#1: review the normal/wide/motion blockouts](https://github.com/jaredbahr/RedalertUE5/issues/1), record approval or concrete revisions before finishing art.
- [#2: implement the narrow UE command and two-client combined-workload proof](https://github.com/jaredbahr/RedalertUE5/issues/2), then derive practical budgets from measurements.
- Confirm hosting/connectivity, target hardware and acceptance thresholds, permanent D: layout, broader cache/residual C: behavior, roster scope and binary/LFS allowance.

Housekeeping: duplicate checks found no existing issues/PRs before these two planned issues were opened. Preserve local editable generated scenes/captures for review; track procedural source and configuration, excluding caches/build/render intermediates and secrets. Record further decisions, validation and actionable follow-ups here as work proceeds. Do not call the toy loopback result UE networking or eight-large-army validation.

## Mobile review delivery

Packaged the inspected normal/wide PNGs as byte-identical copies under ignored `generated/delivery/`, and encoded the existing 16 motion frames as a 2-second H.264 MP4 (644 x 428, 8 fps) using installed Blender. Decoder metadata validation passed. No model changes or finishing were added; art still awaits user review.

Library saving was attempted through the current supported prepared-upload helper. It stopped before transfer/finalization with `Library prepare_uploads is not available`; no Library item IDs were confirmed. Retain all three local delivery files and captures. Do not upload binaries to public GitHub as a delivery workaround. This is a delivery-environment blocker, not a reproduced game bug.

The six command tests and two-client TCP experiment run outside UE in Node. They validate shared human/bot command rules, rejected invalid/ownership/resource/sequence requests, configurable command limits, host-owned damage and matching loopback snapshots. They do not prove UE replication, internet sessions, skirmish AI or a practical large-army budget.

## Native command and validation batch

Jared authorized parallel implementation/review and focused draft PRs. Seven agents contributed isolated source lanes or reviews, with root scheduling heavy UE/Blender work sequentially to share the PC. Work remains on separate D: worktrees; no merges, deployments or binary uploads occurred.

Added native host-owned commands, shared human/bot validation, phased movement/combat, snapshot replication, basic local RTS presentation, a separate generated gameplay map and bounded native test runners. The build uses the installed UE 5.7.4 toolchain, three parallel actions and no UBA. Compiler include/shadowing/API issues were corrected. The network fixture explicitly binds loopback and starts its active deadline only after both clients connect, with a separate startup guard.

Validation: four exact native test groups passed. A real UE host plus two UE clients passed on UDP loopback with 53 common revisions and no digest mismatches; the host accepted six human commands and two bot commands and rejected two ownership violations. A separate reviewer recalculated the evidence and found no scope blocker. Details and retained local report paths are in [native proof](UE_NATIVE_PROOF.md). Graphical controls, bot-specific outcomes, internet play and scale remain unverified; issue #2 is only partially addressed.

Separate review reproduced command enum coercion and insertion-order combat bias in the Node prototype ([#4](https://github.com/jaredbahr/RedalertUE5/issues/4), [#5](https://github.com/jaredbahr/RedalertUE5/issues/5)); [draft PR #8](https://github.com/jaredbahr/RedalertUE5/pull/8) contains the fix, with nine passing tests and a combined local smoke. Asset validation gaps are tracked separately in [#6](https://github.com/jaredbahr/RedalertUE5/issues/6) and [#7](https://github.com/jaredbahr/RedalertUE5/issues/7). Do not conflate Node TCP evidence, native UE UDP evidence or asset evidence.

## Narrow human/bot owner-allocation fix

Issue #11's identity collision slice is isolated on fix/owner-allocation from native draft #9(462d2fa); tested controls #14(4b07de6) remains unchanged. Server allocation now skips reserved bot100 and uses a wider cursor to fail at int32 exhaustion without wrapping or reusing IDs. PostLogin rejects an exhausted admission before model player/resources/unit creation. Initial human IDs1/2 stay unchanged; native bot/fixture paths use the centralized reservation.

Exact tested source6e2748e00d88f7e840b3980b594050fc142be5d3 passed5/5native groups in15.226s, zero failures/errors/warnings. Report generated/native-model-20261016-070344-132/result.json. Focused tests cover99->101,reservedstart,256unique allocations with bot ownership/resources/order/sequence preservation, MAX-1/MAX/repeatedexhaustion and invalidstarts. Independent review found no material source blocker. Corrected incremental module build passed3.74s after an initial obsolete-counter compile failure. A runner-only32bit memory-observation failure is retained separately; its explicitint64fix was verified before this successful run.

Actual headless policy:60checks/4affinityrepairs,mask3/Normal, observed peak working set2,211,753,984bytes and last process CPU12.0625s; polling does not prove continuous confinement/descendant policy. Final run cache2,326,303bytes/temp0; isolated Intermediate2,575,399,785bytes/Binaries61,568,265bytes/Saved16,086bytes remain ignored on D:. Eight C: roots were audited over the build/test interval: seven unchanged; shared UE5.7Saved +34bytes/0files, without attribution or zero-write guarantee. All owned UE processes exited.

Broader issue #11 remains open: disconnect cleanup/retention, reconnect binding/resources, persistence and authentication. Tests are allocator/model execution, not256liveUEclients; existing spawn layout and network capacity remain fixture limitations. Exhaustion controller rejection is source-reviewed, not a massive engine login test. No graphical/art/network/roster expansion or main merge. See OWNER_ALLOCATION.md for exact evidence and retained failures. Subsequent evidence commits are documentation only.

Publication is blocked by automatic approval review: it rejected pushing fix/owner-allocation and creating its draft PR because the delegated authorization was not recognized as trusted over the earlier no-PR instruction. No publication command executed and no workaround was attempted. The complete tested patch and PR body remain local for direct approval; main and the controls branch remain unchanged.
