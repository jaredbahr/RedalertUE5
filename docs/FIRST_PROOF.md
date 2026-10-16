# First proof: evidence and limits

## What is implemented

- Original Blender primitive blockouts: Tesla Reactor, Rhino Tank, and Conscript together, with a repeatable RTS camera, a genuinely wider view, and a short placeholder motion review.
- Source-only content pipeline: editable Blender scene, individual FBX exports, mesh manifest, round-trip validation, and offline review player regenerated on D:. No third-party content or Python packages are required.
- Content-only UE 5.7.4 project: explicit FBX factory import, centimeter-scale bounds checks, generated map, lighting, ground and two cameras. Generated assets are local; the importer rebuilds only its own tagged actors and namespace.
- Separate Node command-contract experiment: host validation shared by human/bot command drivers, two actual loopback TCP clients, and exactly matching host/client snapshots.

## Executed validation

Blender 4.5.10 LTS generated two 960 x 640 stills and a 16-frame, 8 fps placeholder motion sequence. Normal/wide images have distinct hashes and were visually inspected, along with motion endpoints. The offline `generated/previews/review.html` plays the local frames. The infantry motion is whole-body translation/bob, not a finished walk cycle.

FBX reimport validation passed for all three assets: one combined mesh each, origin at zero, ground contact within 5 mm, and preserved world-axis dimensions within 5 mm. The exporter correction removes inherited primitive rotations before reporting bounds.

UE 5.7.4 fresh import and repeat import both exited successfully, emitted `REDALERT_PROOF_OK`, wrote a fresh report and saved the map. Reported mesh dimensions in centimeters:

| Asset | X | Y | Z |
| --- | ---: | ---: | ---: |
| Tesla Reactor | 680.00 | 560.00 | 545.00 |
| Rhino Tank | 333.75 | 620.00 | 240.00 |
| Conscript | 96.64 | 74.45 | 195.00 |

The engine tests use NullRHI: they prove import/map generation, not graphical rendering, a playable build or packaged-game compatibility. Blender supplies the currently reviewed renders. A final tagged-actor count check is recorded in the generated import report.

All six Node tests passed independently. The combined smoke used 12 initial toy units, two clients and 24 manual ticks; it observed 10 ticks with simultaneous movement and damage, 96 accepted bot commands and 48 matching snapshots. Snapshot JSON payload totaled 55,006 bytes. These are small correctness observations, not a real-time benchmark or a unit-capacity budget. See [command experiment](COMMAND_PROTOTYPE.md) for its limitations.

## Storage and housekeeping

The UE log confirmed writable derived-data cache at `generated/ue-cache` and shader working data under `generated/ue-temp` / project `Intermediate`, all on D:. The launcher sets process-local TEMP/TMP and a filesystem-only DDC graph. This is limited path evidence; engine/OS/plugin activity outside those paths has not been exhaustively audited, and zero C: writes are not claimed.

Source/configuration is tracked; `generated/`, Unreal Content/Saved/Intermediate/DDC and editor files are ignored. Local generated `.blend`, FBXs and captures are retained for review, not deleted or uploaded. Binary/LFS budget review is still required before any binary upload. Raw engine logs are not committed because they can contain machine/session identifiers and generated settings.

Read-only inspection verified installed UE 5.7.4, Blender 4.5.10 LTS, Node 24.19.0, VS 2022/MSVC and a Windows SDK. UE logs identify a Ryzen 7 7800X3D and approximately 64 GB physical RAM. Installed tools and machine observations do not establish a target hardware budget. No new tools, dependencies, assets, purchases, agreements or security changes were needed.

## Review gates and next work

Art fidelity, relative scale and readability await Jared's approval; nothing here is finished art. Review the normal, wide and motion views together before authoring finishes. [Issue 1](https://github.com/jaredbahr/RedalertUE5/issues/1) tracks this planned approval work.

No UE gameplay command adapter, skirmish AI, real UE replication, internet session setup, eight-player match, pathfinding stress result or practical large-army budget is implemented or proven. [Issue 2](https://github.com/jaredbahr/RedalertUE5/issues/2) tracks the next narrow UE command/combined-workload proof. Internet is provisionally intended; exact per-player and total unit counts remain undetermined.

See [running notes](RUNNING_NOTES.md) for changes and resolved problems, and [milestones](MILESTONES.md) for the larger acceptance gates. None of those game milestones is marked complete by this pipeline proof.
