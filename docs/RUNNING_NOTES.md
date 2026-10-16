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

## Command-contract review fixes

Reproduced two review findings in the Node toy prototype: an array command type `['attack']` was accepted through property-key coercion; mutual attackers starting 2.5 units apart observed partially applied movement, ending at 0/10 HP depending on insertion order. Exact string-enum validation now precedes field lookup. Tick phases derive movement from one starting snapshot, commit all movement, then derive damage from one post-movement snapshot and apply it together.

Validation: all nine Node tests passed with approved localhost access, including array/object/null rejection directly and over TCP, unchanged state/sequence after malformed envelopes, and mirrored physical duels that swap insertion order. The separate 24-tick/two-client smoke matched all 48 host snapshot copies with movement, combat and bot commands combined. These remain outside-UE loopback correctness checks, not internet or scalability results. No transport redesign or art changes were included.

Next: review the focused fix diff; continue engine-native command validation separately. Connection-slot identity, exact-next sequence/ack handling, transport ingress limits, client buffering and disconnect handling remain controlled-fixture limitations before any production adapter.

Tracked reproduced defects: [non-string enum acceptance #4](https://github.com/jaredbahr/RedalertUE5/issues/4) and [movement/combat ordering #5](https://github.com/jaredbahr/RedalertUE5/issues/5). Root independently reran all nine regressions successfully before this focused fix was proposed.
