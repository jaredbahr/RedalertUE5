# Controls and negative-evidence slice

This isolated D: integration branch combines the existing drafts for testing. It does not merge main, approve art, or establish internet play or an army budget.

## Dependency map

| Draft | Source head | Integration treatment |
| --- | --- | --- |
| #3 first proof | `68a5a767a77812c40d4fca8095df2528ce464aae` | Ancestor of all three later drafts. |
| #8 Node fixes | `ea1f9a32b43fcf61f821ecc3ca094501b407520d` | Cherry-picked as `053bc7c`; preserved both additive running-note sections. |
| #9 native fixture | `462d2fa013ec9421f8a9e1a09a352a6b4aa3672f` | Integration starting point. Parent independently reviewed this head; no duplicate networking review is claimed here. |
| #10 asset evidence | `42d518f6026868f63d78af6b6d0e0c69977c3999` | Cherry-picked as `70193c5`; preserved both additive running-note histories. |

The starting integrated head is `70193c5`. No implementation conflicts occurred; only additive documentation conflicts needed reconciliation. Original draft heads and main are preserved. Subsequent source/test heads are recorded with each run, rather than attributing earlier results to changed code.

## Finite acceptance criteria

Controls lane: one local UE host, one rendered UE client and one passive UE client, with exactly two human owners and three initial bot records. Exercise the same pawn selection and order handlers through real viewport projection/deprojection. Require own-only selection, cleared selection, accepted movement/attack/production receipts and corresponding replicated state, local zoom change, fresh screenshots and explicit completion before timeout. Inspect captures for markers/HUD. Engine-rendered automation cannot establish physical mouse/focus behavior; that manual gate stays open.

Bot lane: the host's scripted mover reaches its exact destination within 1 cm; the bot attacker reduces the HP of its exact passive human target. Record attacker/target IDs, starting/ending HP and rejection of ambiguous human damage attribution. This is a finite script, not a skirmish AI or performance benchmark.

Asset lane: in a separate disposable D: worktree based on #10, verify actual palette refresh on generated UE slots, preservation of an unrelated same-name material, and rejection or explicit correction classification for actual FBX-local vertex/pivot corruption. Restore original bytes and clean imports. Do not treat an actor-transform mutation as a corrupt-FBX test, or successful engine correction as rejection.

Every run must identify source SHA, topology, counts, start/end timing and outcomes. Heavy jobs run sequentially with bounded core counts and deadlines. Generated evidence remains ignored and local; no binary uploads are authorized.

## Review follow-ups

Parent source review found no blocker in #9's bounded fixture. Before persistent sessions, [#11](https://github.com/jaredbahr/RedalertUE5/issues/11) requires disconnect/reconnect semantics and disjoint human/bot identity allocation. Before untrusted internet sessions, [#12](https://github.com/jaredbahr/RedalertUE5/issues/12) requires bounded RPC ingress and acknowledgement/log load. These are scoped follow-ups, not reproduced production exploits. Shared sampled revisions do not establish every-update agreement, adverse-network handling or independent determinism.

## Results

Implementation and root-scheduled evidence are pending. The integrated Node contract suite already passed all nine tests before controls source changes; that outside-UE result does not validate the new graphical scenario.
