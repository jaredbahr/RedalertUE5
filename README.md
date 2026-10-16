# RedalertUE5

A personal full-3D Unreal Engine 5 RTS project inspired by Red Alert 2 and Yuri's Revenge: faithful unit identities, an RA2 visual style, capable AI opponents, and eventually games with up to eight friends.

## First implementation proof

This branch adds original authored Blender blockouts of a Tesla Reactor, Rhino Tank, and Conscript; normal and zoomed-out RTS views; and a short placeholder motion review. Source scripts regenerate the local scene, FBX exports and captures. These silhouettes and relative scales await Jared's approval before finishing assets.

A content-only UE 5.7 project and editor import script provide the engine scene foundation. A separate dependency-free Node experiment exercises shared human/bot commands through host validation with two loopback TCP clients. It is not UE networking, a complete RTS, internet readiness, or proof of eight large armies. See [first-proof evidence and limits](docs/FIRST_PROOF.md).

Internet multiplayer is provisionally intended and large armies are the ambition. Exact per-player and total counts remain undetermined; combined movement, combat, bot and replication measurements must set practical budgets. Begin with Allies and Soviets on one simple map, complete a small bot skirmish, then expand toward Yuri and four/eight players.

## Run and review

- [Art pipeline](docs/ART_PIPELINE.md): generate original blockouts and local still/motion previews with the installed Blender; no downloads required.
- [UE setup](docs/UE_SETUP.md): regenerate the engine scene with installed UE 5.7.4 and inspect import/cache evidence.
- [Command experiment](docs/COMMAND_PROTOTYPE.md): run `node --test prototype/simulation.test.mjs` and `node prototype/smoke.mjs`; no packages to install.

## Project documents

- [Project brief](docs/PROJECT_BRIEF.md): goals, initial scope, constraints, and storage policy.
- [Art direction](docs/ART_DIRECTION.md): reference workflow and the first visual approval gate.
- [Milestones](docs/MILESTONES.md): ordered deliverables and observable pass criteria.
- [Decisions](docs/DECISIONS.md): confirmed choices, proposed defaults, and unanswered questions.

## Working rules

Keep project files, source art, caches, and builds on D: using explicit project-specific paths. Verify actual cache and temporary-file locations; this is not a promise of zero writes to C:. Generated output is ignored by Git. Review Git LFS storage/bandwidth before uploading binary assets; this branch tracks source/configuration only.

Use newly created reference-based art. Do not upload raw EA assets or music. Ownership of the original games is recorded as reference availability, not redistribution permission. No license or permission grant is asserted by these documents.
