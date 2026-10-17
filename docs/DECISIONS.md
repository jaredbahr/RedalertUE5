# Decisions

Status vocabulary: **Confirmed** records Jared's direction; **Proposed** is a candidate to validate; **Open** requires a choice. Planned features and gates are not implementation results.

## Confirmed

| Topic | Decision |
| --- | --- |
| Project | Personal full-3D RA2 / Yuri's Revenge project using UE5. Installed UE 5.7.4 is the first-proof baseline; long-term version policy remains open. |
| Visual character | Faithful RA2 unit identities and RA2 style; avoid the childish RA3-like look. |
| Opponents | AI bots are critical. |
| Long-term scope | Allies, Soviets, and Yuri; eventually play with up to eight friends. |
| First playable scope | Allies and Soviets first, on one simple map. |
| Multiplayer intent | Jared says likely internet: internet multiplayer is the provisional intended mode; hosting/connectivity details remain open. |
| Army-size ambition | Jared expects large armies. Exact per-player and total unit counts remain undetermined; practical budgets must follow measured tests. Eight large armies are not proven. |
| First art proof | Tesla Reactor, Rhino Tank, and Conscript together at the RTS camera; include zoomed-out and motion views. Jared approves blockouts before finishing. |
| Existing tools/references | Blender installed; Jared owns Steam/Origin originals. Ownership does not establish redistribution rights. |
| Storage | Put project/art/cache/build data on the designated non-system drive; verify effective cache paths and disclose residual system-drive use. |
| Current work | Jared has authorized a first scoped implementation batch using parallel agents: original authored blockouts, reproducible scene/assets pipeline, and early technical groundwork with existing tools. No purchases, new agreements, security changes, proprietary asset uploads, or unapproved installs. Blockout approval still precedes finishing. |
| Repository changes | Use isolated implementation branches and preserve others' edits. Jared subsequently authorized routine validated publication, including branch pushes, draft PRs and normal merges after applicable checks and independent review. Force pushes, unrelated changes, sensitive actions and spending remain outside that routine authorization. |

Jared subsequently clarified the product goal: a modernized 3D RA2/Yuri-style remake that preserves gameplay feel, pacing, economy, unit roles and combat. Camera panning and modest usability improvements are accepted implementation discretion. This is a direction decision, not approval of prototype balance or evidence of gameplay parity. The current engineering results are recorded in [Small match](SMALL_MATCH.md); broad milestone gates remain open.

## Proposed defaults, not yet approved decisions

| Topic | Proposal and validation |
| --- | --- |
| Authority | Host-authoritative game state; validate with two clients before broad implementation. |
| Commands | Shared command model for humans and bots, with host validation and common economy/ownership rules. |
| Implementation order | Two-client and active-unit proofs early; complete a small bot skirmish before Yuri and four/eight-player expansion. See milestone gates. |
| Art creation | Newly authored reference-based assets, checked at RTS distance and tracked with provenance. |
| Permanent storage layout | Separate project, source art, caches, and build folders as proposed in the brief; paths and tool settings still need verification. |

## Open choices

| Question | Needed before |
| --- | --- |
| What exact per-player and total unit counts/unit density should typical and worst-case matches support, given the large-army ambition? | Setting practical unit budgets from combined movement, combat, bot, and network-replication measurements. |
| With internet provisionally intended, what hosting/connectivity approach is acceptable, and is LAN also required? | Selecting transport/session setup and meaningful internet network tests. |
| Which hardware, frame-time target, memory limit, and command-latency/traffic budgets define success? | Calling a technical performance gate passed. |
| Will UE 5.7.4 remain the project baseline, and what future version/toolchain policy is needed? | Creating the engine project; no version or build compatibility assumed. |
| What exact Allied/Soviet roster and mechanics make the first skirmish complete? What Yuri mechanics follow? | Committing to content production and faction completion criteria. |
| What relative infantry/vehicle/building scale and camera zoom range are approved? | Finishing assets after the visual blockout review. |
| What binary sizes, LFS rules, and current storage/bandwidth allowance are acceptable? | Any binary asset upload. |

## Evidence and future changes

Repository inspection found a public repository with only `README.md` (the title `# RedalertUE5`) at base commit `2a41dadc72b7bb1153925b1949cfba510518dd4c`, on default branch `main`. No engine project, repository instructions, license file, or existing implementation was present in that commit. Visibility can change; this is an inspection record, not an ongoing guarantee.

The first implementation batch adds reproducible blockout and technical experiments; see [First-proof evidence](FIRST_PROOF.md) for that batch. Subsequent partial UE command, loopback and small-match evidence is recorded in [Small match](SMALL_MATCH.md), including exact revisions and limitations. Blockout approval, gameplay parity, practical army-size budgets, complete faction rosters and Internet readiness remain unproven. When a proposal is accepted or a gate passes, record the decision, approver, evidence, and any scope change here. Do not backfill an invented date, performance result, or permission grant.

## Player-feature engineering evidence

The [current feature/evidence record](PLAYER_FEATURES.md) documents a fresh UE 5.7.4 build at c54, 84 functional native cases and 13 runtime passes across verified c54/42 bindings, with 36 native-size reviewed PNGs. Script-only native cleanup instrumentation reuses the identical Source/Config/project/DLL and 14 local assets; prior results retain their original tested heads.

The native cleanup criterion is an original PID/creation handle with a native signal and final exit code, separate from the functional deadline. Unrelated process names cannot establish our cleanup failure. Previous failures, including the old native functional-pass/batch-guard-failure distinction, remain recorded; historical crash causes are not declared fixed.

This evidence does not close the 960-pixel primary-label fallback/dense-HUD issue #18, physical input/focus issue #17, art/scale approval, complete roster, Internet or active-army gates. It records engineering results, not a new approval by Jared of those unresolved choices.
