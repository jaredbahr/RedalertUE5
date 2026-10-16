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

## Asset acceptance-evidence hardening

Cloud static review identified two P2 gaps: the UE importer accepted any Z half-extent between 10 and 5000 cm without gating X/Y or exact scale, and geometry-only/failed validation could leave old captures or a passed report appearing current. This lane adds explicit numerical geometry gates and current-run/source/export/capture identities, archives only prior pipeline evidence, and invalidates current reports before checks. No silhouette/palette finishing was performed.

Validation: three stdlib regression cases passed, covering independent geometry mutations, stale-success invalidation, historical-capture preservation and actual entrypoint failures before malformed manifest parsing. Fresh geometry-only Blender generation, all three FBX round trips, fresh/repeat UE import and the engine preservation/mutation probe passed. The corrected probe records yaw=180, pitch=roll=0 and rejects reversed facing, 10x uniform scale, X-only scale and +100 cm actor Z. Eight generated actors, the untagged-by-generator sentinel and an external package's bytes were preserved; 14 material slots were recorded across three meshes. The first probe accidentally tested tilt through positional Rotator arguments; named arguments and applied-rotation assertions corrected it before acceptance. Independent review also found stale probe reports could survive new generation or same-run imports; both paths now invalidate them before work begins.

The final current run identity and reports are retained under ignored generated/. These tests do not exercise material-color refresh, generic material collisions, altered FBX-local vertices/pivots or visual semantic front. Those remain unverified. Art remains unapproved blockouts; no source silhouette or palette finishing was performed. See ART_PIPELINE.md and UE_SETUP.md for reproduction and boundaries.
