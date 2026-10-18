# Integration candidate validation

All prepared gates passed once at clean **`4fdc7e27f55bf38ea43314dbea58a40fc2f1afe2`**. No failed build/test attempt, repeated launch, source fix, assertion relaxation or deadline extension occurred in this batch. Installed UE 5.7.4 built six actions in 45.16 seconds total / 36.61 executor. Independent evidence review found no material blocker for the scoped candidate. Later publication changes documentation only.

The selected chain remains #3 -> #9 -> #14 -> #19 -> #25 -> #26 -> #27 -> #28 -> #30. PR #29/#31 implementation is excluded. See [provenance](candidate_provenance.json) and [candidate scope](INTEGRATION_CANDIDATE.md).

## Exact retained results

All directories below are under ignored `./generated/`; each result records the tested SHA above and a clean worktree.

| Gate / directory | Outcome |
| --- | --- |
| `group-controls-model-<local-run>` | 11/11, zero failures, 18.958 seconds |
| `native-model-<local-run>` | 7/7, zero failures, 12.531 seconds |
| `skirmish-model-<local-run>` | 14/14, zero failures, 12.315 seconds |
| `fidelity-model-<local-run>` | 4/4, zero failures, 12.121 seconds |
| `controls-<local-run>` | Pass, 60.970 seconds, 55 matching shared revisions; two fresh captures; cleanup errors 0 |
| `native-network-<local-run>` | Pass, 59 matching shared revisions; runner does not record elapsed time |
| `rpc-ingress-<local-run>` | Pass, 18.162 seconds; flood connection closed while healthy client continued moving/producing |
| `small-match-victory-<local-run>` | Pass, 81.830 seconds; actual log winnerTeam 0; all required economy/production/bot phases |
| `small-match-defeat-<local-run>` | Pass, 51.294 seconds; actual log winnerTeam 1; all required phases |
| `small-match-coop-<local-run>` | Pass, 85.333 seconds; winnerTeam 0, 307 matching shared revisions; cleanup errors 0 |
| `session-lifecycle-<local-run>` | Pass, 123.899 seconds; 420 matching epoch/revision digests; cleanup errors 0 |

The four model suites total **36 successful cases**. Lifecycle verified owner-2 disconnect cleanup and slot-2 rejoin without a fresh grant, deferred reseeding until rematch, third-connection rejection when full, stale-epoch rejection, and accepted fresh sequence-1 receipts for both humans in epochs 2 and 3. Two exact terminal snapshot agreements both recorded winnerTeam 1 (epochs 1 and 2), followed by normal rematches into epochs 2/3. Those are two human defeats, not extra victory evidence; Victory and Coop retain their separate explicit winnerTeam-0 passes.

Controls and ordinary match captures were fresh 1280x720 PNGs with recorded SHA256 hashes: eight captures total. The controls pair and Victory/Defeat/Coop terminal captures were inspected. They show readable nine-group hints, selected-unit/host-receipt state and correct Victory/Defeat banners. Dense labels and generic reused proxies remain functional proof presentation, not final art/scale approval. These fixtures call existing handlers; native callback counts remain zero/human input unverified. Separate #31 injection results are not attributed to this candidate run.

## Resource handoff and limits

All jobs were serialized with existing deadlines, normal priority, CPU-affinity assignment/repair and explicit the designated non-system drive build/temp/cache paths. Rendered controls recorded t.MaxFPS 30 and optimized shaders; that is configuration evidence, not measured frame-time or army performance. Warmup CoreLimit is 4, shared affinity policy uses two logical CPUs, worker inheritance is unverified and polling is not hard confinement. Fourteen existing ignored asset copies matched their source SHA256 hashes; no art/import/build assets were generated anew beyond ordinary compilation. Logs, assets, binaries, caches and captures remain ignored and were not uploaded. No zero-the system drive-writes claim is made.

`generated/resource-handoff.json` records all owned UE/build processes exited and releases the shared heavy-test slot. Independent read-only review confirmed the result metadata, positive and negative gates and scope limits. No additional test run is requested by this publication.

No main promotion or issue closure occurred. The scoped candidate is ready for Jared's promotion decision; physical input/real focus, final art, internet conditions and combined active-army performance remain separate open gates. PR #29's unresolved original readiness, missing-receipt and later pre-map failures remain preserved and unexplained.
