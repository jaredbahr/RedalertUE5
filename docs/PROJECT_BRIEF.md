# Project brief

## Goal and confirmed scope

Build a personal full-3D RTS in Unreal Engine 5 that preserves recognizable Red Alert 2 / Yuri's Revenge units and the RA2 visual character. Avoid the childish RA3-like look Jared does not want. AI bots are critical to the playable game, not an optional stretch feature.

The long-term scope includes Allies, Soviets, and Yuri, with games for up to eight friends. Begin with Allies and Soviets on a simple map; establish a complete small skirmish before expanding the roster, faction count, maps, or player count.

Jared says multiplayer will likely be internet-based, making internet play the provisional intended mode. The confirmed army-size target is 300+ units per player: 2,400+ units across an eventual eight-player match. This is a design target to prove through combined active workloads, not a supported capacity claim.

Campaigns are an eventual, distant goal. First establish a satisfying, complete skirmish with bots; campaign design and production follow the skirmish, faction and multiplayer gates rather than competing with the first playable slice.

## Gameplay fidelity and acceptable modernization

Jared's clarified north star is a near-identical-feeling UE5 recreation of Red Alert 2 / Yuri's Revenge: the old-school RTS playstyle he remembers, with modern visuals. Preserve original-style pace, economy, base building, unit roles, combat and responsiveness. Modern presentation, camera freedom and readability should support that playstyle. Do not substitute mechanics from other modern RTS games or drift toward RA3's rejected visual style. Routine fidelity work can proceed within this direction.

The smallest trial should prioritize a responsive human-controlled bot skirmish with a usable launch, valid finish and restart. Original-game comparison must establish timings and behavior before claiming exact gameplay parity. The small-match prototype is an engineering foundation, not evidence of gameplay parity. Prioritize concrete fidelity gaps after lifecycle correctness and measured scale evidence: responsive selection/orders and camera, recognizable role-specific movement/combat, and an economy/production loop calibrated against private original-game references. Record what is measured or confirmed and keep placeholder values explicit. Check newly filed repository issues between bounded batches.
## First proof

Show a Tesla Reactor, Rhino Tank, and Conscript together in the same scene at the intended RTS camera. Supply a normal gameplay view, a zoomed-out view, and motion footage. Jared must approve the blockout proportions, relative scale, silhouettes, and camera readability before asset finishing. This proof is not a commitment to final unit-to-building scale.

## Proposed implementation defaults

These are proposals awaiting validation, not approved architecture:

- A host-authoritative simulation owns economy, production, damage, ownership, and victory. Clients submit commands; the host validates them and distributes resulting state.
- Human players and bots use the same command model for move, attack, build, and production. Bots obey the same validation and resource rules by default.
- Start with one compact, readable skirmish map, a minimal functional Allied/Soviet roster, and placeholder assets where needed. Prove the full match loop before extending content.

Two-client correctness and active-unit performance are early gates. Scale tests must combine active movement, combat, bot decisions, and network replication in the same workload; measured results will set practical supported unit budgets against the 300+ per-player target. Frame-time, network latency, memory and target hardware budgets remain open. Installed UE 5.7.4 is the first-proof baseline; this does not lock a long-term engine support policy. Set measurable acceptance thresholds before calling those gates passed. Raising a prototype record cap or measuring idle/model-only units does not establish this target.

## Tools and references

Blender is installed. Jared owns original games through Steam/Origin for private visual reference. Jared has now authorized the first scoped implementation batch: original authored blockouts, a reproducible scene/assets pipeline, and early technical groundwork with existing tools. No purchases, new agreements, security changes, proprietary asset uploads, or unapproved installs are authorized. Asset finishing still requires blockout approval.

Prefer newly authored art guided by references. Maintain provenance for future assets: creator, source/reference, usage terms, and any distribution limits. Do not upload raw EA game assets or music, and do not infer permission from ownership or invent a project license.

## Reusable design and pipeline

This project is a new UE5 reimplementation/remake of the original game's behavior and feel, not a port of EA's original source code. Another goal is to establish a documented core design and 3D production process that could later support an original RTS or other 3D games.

Keep general command/simulation boundaries, human/bot adapters, import/validation scripts and evidence practices distinguishable from RA2-specific factions, rules, balance, art, maps and audio. Record provenance and applicable licenses for code, references and assets so later projects can identify what they may reuse. Current prototypes do not establish a finished reusable framework.

Document the practical [asset workflow](ASSET_PIPELINE_EVALUATION.md#reusable-workflow-and-trial-records), including failed attempts, cleanup time and accepted results. Extract reusable lessons from working trials. The playable RA2-style trial comes first; speculative framework work or a large architecture rewrite is outside this goal.

## Storage and repository hygiene

Use isolated project, source-art, cache and build directories on the designated non-system storage. Keep their exact machine locations private. The current scripts enforce a locally configured drive constraint; confirm the prepared workspace with the maintainer rather than guessing from a public example.

| Purpose | Public path convention |
| --- | --- |
| Repository and Unreal project | `<PROJECT_ROOT>/unreal` |
| Blender/source art | `<ART_ROOT>` |
| Derived-data, shader and tool caches | `<CACHE_ROOT>` |
| Packaged builds and captures | `<BUILD_ROOT>` |

Before engine/art work, verify free space and resolved paths for Unreal derived data (including the configured backend), shader/compiler work, project intermediate/saved output, Blender temporary/autosave output, logs, crash reports, and packaging/staging output. Inspect settings and actual file creation during an authorized smoke run. Record any remaining system-drive use and adjust supported settings where practical. Designated storage placement alone does not establish cache behavior; consult [First-proof evidence](FIRST_PROOF.md) for the paths observed during this batch; other paths remain unverified.

Before importing or generating content, add and verify appropriate ignore rules for generated output such as `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, editor/IDE caches, staged builds, and renders. Keep source/configuration files and authored assets distinct from generated data. The implementation branch now includes ignore rules for generated proof output and Unreal content/caches; verify them again before extending the pipeline.

Before any binary upload, review file sizes, expected growth, Git LFS tracking, and the account's current storage/bandwidth budget. Do not assume the current allowance or upload binaries to test it. Review staged files for secrets and unrelated content before every push.

## Decisions still needed

Determine practical supported unit counts and density through measurements against the confirmed army-size target, confirm hosting/connectivity and any LAN requirement for the provisionally intended internet mode, and agree on target hardware and performance/network budgets, engine version, and the minimum roster. See [Decisions](DECISIONS.md). These choices affect technical feasibility and milestone acceptance, and are not silently resolved by this brief.

Asset-tool candidates and their unverified evaluation gates are recorded in [asset pipeline evaluation](ASSET_PIPELINE_EVALUATION.md); asset tooling and lower-priority audio should support this gameplay direction.
