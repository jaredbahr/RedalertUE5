# Player features and scoped validation

This engineering slice adds bounded HUD annotation placement, optional edge scrolling, visible type selection, factory rally, and Attack Move to the existing generic-unit prototype. It preserves the shared host validation path used by humans and bots. It does not complete an Allied/Soviet roster or establish RA2 balance parity.

## Try the controls

From a prepared, clean the designated non-system drive checkout with the existing local DLL and generated map:

```powershell
& .\unreal\Scripts\start-small-match.ps1 -CameraEdgePan
```

Arrow-key pan is a separate `-CameraPan` option. Both pan options are off by default; the wheel changes the existing orthographic zoom. These launchers provide local practice, not an Internet session.

| Feature | Current behavior and bounds |
| --- | --- |
| HUD placement | Bounded displacement and leader lines keep accepted annotation rectangles out of reserved UI areas. Whole annotation groups can be omitted when no valid placement fits; selection brackets and the primary panel remain separate. See the important 960-pixel limitation below. |
| Command panel candidate | Mouse-driven Refinery/Factory placement, Combat production and Stop, with costs and prerequisite hints. Shared geometry blocks world clicks/edge scrolling under the panel. The standalone policy passed 229 checks in normal and optimized builds; Unreal compilation, rendering and actual input/RPC validation are pending. See [command panel](COMMAND_CARD.md). |
| Edge scroll | With `-CameraEdgePan`, the outer 16 viewport pixels request translation. HUD regions, held mouse buttons, loss of focus and lack of foreground ownership suppress it. No rotation or zoom change. Physical desktop behavior still needs human acceptance. |
| Type selection | T selects live owned records of the primary unit's generic kind whose projected centers are inside the viewport, up to 12. A visible primary keeps a slot; remaining IDs are deterministic. Saved groups and pending commands stay unchanged. No fog, occlusion or repeated/global expansion test is implied. |
| Factory rally | In a playing skirmish, select one owned live factory and right-click ground away from enemy markers. Later paid Combat production from that factory gets the rally movement order. Setting rally does not spawn a free unit or move the structure. |
| Attack Move | In a playing skirmish, Ctrl+Shift+right-click ground queues Attack Move for selected owned live Combat units. They travel, stop to fire within existing range, then resume after target death or loss of range. S cancels it; ordinary Move/Attack replaces it. No out-of-range pursuit or navigation is added. |

These are generic Combat/Harvester/HQ/Refinery/Factory kinds, not a completed RA2 roster. See [selection/groups](GROUP_CONTROLS.md), [small match](SMALL_MATCH.md) and [fidelity baseline](FIDELITY_BASELINE.md) for existing behavior and provisional values.

## Verified engineering scope

A fresh installed UE **5.7.4** `RedalertProofEditor Win64 Development` build passed at `c54f50325e24c4aed75ec650e6208999578ce22b`. Its DLL SHA256 is `FD948BDFC7627BAACB91B4FFC903367B719EFDA980ACE8D13447514DAE9AA571`.

All **84 functional native cases** passed at that clean source: Commands 7, controls/groups 11, fidelity 4, skirmish 22, Stop 11, ownership recovery 2, HUD 8, camera edge 3, Type 3, rally 4 and Attack Move 9. The separate stress case is excluded and gives no active-army performance credit.

Four runtime passes below belong to clean `c54f503`. The remaining nine belong to clean script-only descendant `42ef9e9b8e7aa52ba639f3ff18949f5d235f6c29`. Source, Config, project, DLL and all 14 local asset hashes were unchanged and independently verified before reuse; these earlier passes are not relabeled as executions at the later head. `42ef9e9` changes only native transport cleanup instrumentation and its focused tests. Root and independent review each passed 46 focused checks, including real original-handle probes against short-lived commands; no gameplay rebuild was needed.

All **13 required runtime rows passed**, with **36 fresh engine PNGs** checked for hashes/dimensions/freshness and inspected at native size by root and independent reviewers. Reports remain local under `generated/`; they are intentionally not uploaded to Git.

| Runtime gate | Tested head | Local run directory | PNGs |
| --- | --- | --- | ---: |
| attack-move-proof | `c54f503` | `attack-move-proof-<local-run>` | 3 |
| lifecycle-compatibility | `c54f503` | `session-lifecycle-<local-run>` | 0 |
| factory-rally-coop | `c54f503` | `small-match-coop-<local-run>` | 3 |
| ordinary-victory-flags-off | `c54f503` | `small-match-victory-<local-run>` | 2 |
| native-command-transport | `42ef9e9` | `native-network-<local-run>` | 0 |
| rpc-ingress | `42ef9e9` | `rpc-ingress-<local-run>` | 0 |
| controls-default | `42ef9e9` | `controls-<local-run>` | 2 |
| controls-stop | `42ef9e9` | `controls-<local-run>` | 4 |
| controls-type | `42ef9e9` | `controls-type-selection-<local-run>` | 4 |
| victory-hud-camera-1280x720 | `42ef9e9` | `small-match-victory-<local-run>` | 7 |
| victory-hud-camera-960x540 | `42ef9e9` | `small-match-victory-<local-run>` | 7 |
| defeat-compatibility | `42ef9e9` | `small-match-defeat-<local-run>` | 2 |
| coop-compatibility | `42ef9e9` | `small-match-coop-<local-run>` | 2 |

The local hash index is `generated/player-features-acceptance.json`; raw and image reviews are retained beside it. Source-only documentation descendants keep the tested engine inputs unchanged. Individual runners under `unreal/Scripts/` retain their original assertions and deadlines; there was no automatic retry or deadline extension of a failed functional run.

Attack Move's three-role proof verifies travel, host-timed in-range damage, target death, automatic resumption and shared Stop. Clients complete and kernel-confirm exit before the separate host release; final all-role completion remains required. Native transport now records the original PID/creation handle, native signal and exit code for each of its three roles. One five-second final cleanup wait is separate from the unchanged functional deadline. A status-only exit is not kernel confirmation, and unrelated process names are not cleanup evidence.

The camera-only pairs preserve authoritative state/digest, world transforms, zoom and rotation. A -600 cm rig translation gives the expected 86.4 px shift at 1280x720 and 64.8 px at 960x540. These are synthetic fixture observations, not physical edge-scroll/focus acceptance. A configured 30 FPS cap is not a measured performance result.

## Important remaining limits

**Issue #18 remains open.** At 960x540, the primary world ID/HP annotation group is omitted in `01-base.png`, `05-panel-edge.png`, `06-camera-before.png` and `07-camera-after.png`, despite a visible selection glyph. The bracket and panel ID/HP100/Cargo0 remain readable. The base frame also omits one resource group. Other dense unselected groups are omitted in both viewport sizes. All seven 1280x720 primary groups were placed; that result must not be transferred to 960x540. The legacy non-skirmish controls view also retains overlapping #5/#3 labels. Complete selected/all-unit world readability is not established.

Shared-handler fixtures do not prove hardware T/S/modifier delivery, Alt-Tab/focus behavior, physical edge scrolling or commands issued after panning. Keep issue #17 and the remaining input/HUD acceptance open. Art and relative scale still need Jared's blockout approval. Navigation, complete faction rosters, Internet connectivity, four/eight humans and combined large-army performance remain unverified.

A source-only build-hint refinement shortens the skirmish panel's R/F/P help row and reads all three displayed prices from the command model's existing cost constants. The ore explanation is removed from that row; world ore markers are unchanged. This preserves the panel footprint, status/receipt rows, input handlers and existing economy/selection capture fields. The refinement has not been compiled or rendered. Check complete key/role/price text at 1280x720 and 960x540 before accepting it; it does not resolve the remaining selected-world-label omissions or close issue #18.

Earlier native/lifecycle timeouts and the earlier Attack Move failure remain failed. The old native transport runner returned a functional pass, but its c54 batch row remains failed for the unidentified post-run process guard. The new owned-handle pass is a separate receipt; it does not establish a cause or fix for the earlier lifecycle assertion/crash (issue #33). Capacity holds before launch are not functional test failures.
