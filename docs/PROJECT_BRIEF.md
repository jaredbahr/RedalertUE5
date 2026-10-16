# Project brief

## Goal and confirmed scope

Build a personal full-3D RTS in Unreal Engine 5 that preserves recognizable Red Alert 2 / Yuri's Revenge units and the RA2 visual character. Avoid the childish RA3-like look Jared does not want. AI bots are critical to the playable game, not an optional stretch feature.

The long-term scope includes Allies, Soviets, and Yuri, with games for up to eight friends. Begin with Allies and Soviets on a simple map; establish a complete small skirmish before expanding the roster, faction count, maps, or player count.

Jared says multiplayer will likely be internet-based, making internet play the provisional intended mode. Large armies are the ambition; exact per-player and total unit counts remain undetermined. Eight large armies are not yet proven.

## First proof

Show a Tesla Reactor, Rhino Tank, and Conscript together in the same scene at the intended RTS camera. Supply a normal gameplay view, a zoomed-out view, and motion footage. Jared must approve the blockout proportions, relative scale, silhouettes, and camera readability before asset finishing. This proof is not a commitment to final unit-to-building scale.

## Proposed implementation defaults

These are proposals awaiting validation, not approved architecture:

- A host-authoritative simulation owns economy, production, damage, ownership, and victory. Clients submit commands; the host validates them and distributes resulting state.
- Human players and bots use the same command model for move, attack, build, and production. Bots obey the same validation and resource rules by default.
- Start with one compact, readable skirmish map, a minimal functional Allied/Soviet roster, and placeholder assets where needed. Prove the full match loop before extending content.

Two-client correctness and active-unit performance are early gates. Scale tests must combine active movement, combat, bot decisions, and network replication in the same workload; measured results will set practical unit budgets. No unit count, frame rate, network latency, or target hardware budget is established yet. Installed UE 5.7.4 is the first-proof baseline; this does not lock a long-term engine support policy. Set measurable acceptance thresholds before calling those gates passed.

## Tools and references

Blender is installed. Jared owns original games through Steam/Origin for private visual reference. Jared has now authorized the first scoped implementation batch: original authored blockouts, a reproducible scene/assets pipeline, and early technical groundwork with existing tools. No purchases, new agreements, security changes, proprietary asset uploads, or unapproved installs are authorized. Asset finishing still requires blockout approval.

Prefer newly authored art guided by references. Maintain provenance for future assets: creator, source/reference, usage terms, and any distribution limits. Do not upload raw EA game assets or music, and do not infer permission from ownership or invent a project license.

## D: storage and repository hygiene

The documentation checkout was created at `D:\UserTemp\Codex-RedalertUE5-docs-20261016`. It is an isolated preparation workspace, not a verified permanent project/cache location.

Proposed permanent layout, to confirm before creating an engine project:

| Purpose | Proposed location |
| --- | --- |
| Repository and Unreal project | `D:\Projects\RedalertUE5` |
| Blender/source art | `D:\Projects\RedalertUE5-Art` |
| Derived-data, shader, and tool caches | `D:\Caches\RedalertUE5` |
| Packaged builds and captures | `D:\Builds\RedalertUE5` |

Before engine/art work, verify free space and resolved paths for Unreal derived data (including the configured backend), shader/compiler work, project intermediate/saved output, Blender temporary/autosave output, logs, crash reports, and packaging/staging output. Inspect settings and actual file creation during an authorized smoke run. Record any remaining C: use and adjust supported settings where practical. D: placement alone does not establish cache behavior; consult [First-proof evidence](FIRST_PROOF.md) for the paths observed during this batch; other paths remain unverified.

Before importing or generating content, add and verify appropriate ignore rules for generated output such as `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, editor/IDE caches, staged builds, and renders. Keep source/configuration files and authored assets distinct from generated data. The implementation branch now includes ignore rules for generated proof output and Unreal content/caches; verify them again before extending the pipeline.

Before any binary upload, review file sizes, expected growth, Git LFS tracking, and the account's current storage/bandwidth budget. Do not assume the current allowance or upload binaries to test it. Review staged files for secrets and unrelated content before every push.

## Decisions still needed

Determine exact per-player and total unit counts through measurement, confirm hosting/connectivity and any LAN requirement for the provisionally intended internet mode, and agree on target hardware and performance/network budgets, engine version, and the minimum roster. See [Decisions](DECISIONS.md). These choices affect technical feasibility and milestone acceptance, and are not silently resolved by this brief.
