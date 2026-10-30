# Design plan: faithful-feeling RTS, modern 3D presentation

Status: **proposed execution plan, not fidelity validation or approved final balance**.
The live [roadmap and claim index is issue #85](https://github.com/jaredbahr/RedalertUE5/issues/85).
This plan extends, rather than replaces, [Project brief](PROJECT_BRIEF.md),
[Decisions](DECISIONS.md), [Milestones](MILESTONES.md) and the
[Fidelity baseline](FIDELITY_BASELINE.md).

## 1. Product contract

Preserve the old-school Red Alert 2 / Yuri's Revenge play Jared remembers:
pace, finite economy, base construction, distinct unit roles, combat and
responsive selection/orders. UE5 full-3D visuals, pan/zoom, clear status and modest
usability improvements must support that contract. Do not drift into a different
modern RTS ruleset or the rejected RA3-like visual character.

Bots are essential participants, not a stretch feature. First deliver an agreed
small Allied/Soviet skirmish on one simple map; Yuri and four/eight-player friend
sessions follow only after their own scoped design and measured gates.
Large armies and likely Internet play are ambitions, not established capacities.

### Evidence vocabulary

| Kind | Meaning |
| --- | --- |
| Confirmed direction | Owner product intent in the brief/decision ledger. |
| Documented original behavior | A cited qualitative source; not necessarily measured timings or patch parity. |
| Observed reference value | A private repeatable original-game observation with version, speed, scenario and uncertainty. |
| Prototype policy | A named current engineering rule/calibration; no original-game fidelity inferred. |
| Verified engineering result | Exact source/build/test/capture and conditions; only its stated scope passed. |
| Open design choice | Must be resolved explicitly before dependent commitment. |

Unit/mesh names, attractive renders and passing synthetic tests do not establish
gameplay likeness. Closed #21–24 retain their bounded generic skirmish achievement;
they are not full faction completion.

## 2. Preserve and extend the existing boundaries

These are incremental design seams around the current foundation, not permission
for a framework rewrite or a claim that all future architecture is selected.

| Area | Design responsibility / next bounded work | Acceptance boundary |
| --- | --- | --- |
| Authority and commands | Preserve host-owned ownership, economy, production, damage, terminal state and epochs. Humans and bots submit through shared validation. | Rejections do not corrupt state/sequence; common multi-client state/digests and exact native suites. |
| Rules and faction data | Separate reusable command/state boundaries from RA2-specific roles, costs, prerequisites, weapons/armor, power and producer categories. Choose the smallest roster in #82. | Trace each supported mechanic to accepted references; explicit exclusions and prototype values. No speculative complete roster. |
| Economy and production | Preserve finite resources and paid shared-command progression; use #81 to calibrate extraction/unloading, work/payment and controls. | Per-transition conservation, cash pause/resume, hold/cancel/refund, ready-blocked completion, rally and lifecycle cleanup on exact source. |
| Movement and combat | Distinguish agreed infantry/vehicle behavior, targeting, weapon cadence and damage interaction. Retain simultaneous resolution. Investigate map obstacles/congestion before selecting navigation/collision architecture. | Measured reference contract, hostile/invalid-target cases, actual congested movement, bot and replicated combat; no model-only pathfinding claim. |
| Player interaction | Familiar click/drag/modifier/type/group orders, Stop, rally, Attack Move, production and pan/zoom. UI is an adapter, not an alternate authority. | #17 observed physical actions/focus/terminal/rematch; #18 actual viewport readability; #83 truthful replicated production status. |
| Bots | Same legality, costs and command route as humans; deterministic bounded decisions with threat response and ordinary-loss recovery. | Repeated natural complete matches, conservation, no invalid free state writes, recorded stalls/outcomes. |
| Presentation and assets | Original reference-guided 3D silhouettes, faction/selection readability, consistent scale/pivots/animation at RTS distance. | #1 owner approval precedes finishing; existing geometry/provenance gates #6/#7 and rights/LFS checks precede binary publication. |
| Sessions and transport | Preserve tested bounded local slots/epochs/cleanup; design wider connectivity/identity/abuse behavior in #11/#12. | Actual supported topology and adverse-network tests; loopback never implies Internet or authenticated persistence. |

Do not silently add queue backlog, parallel-factory bonuses, power rules,
projectile/armor systems, fog-of-war, host migration or faction mechanics merely
because a design seam mentions them. #82 must classify each as required, deferred
or unresolved for the minimum loop, using the fidelity direction rather than
unrelated RTS conventions.

## 3. Dependency-ordered design to execution

### A. Reconcile the current generic candidate

Review current GitHub before touching code. On the 2026-10-30 planning inspection:
#71 → #75 → #76 is a stacked production/cargo line; #79 and #80 are **siblings**
based on #76, and #77 is independent on main. Old open draft stacks are historical
preparation as well as possible work; don't automatically replay them.

The [live index](https://github.com/jaredbahr/RedalertUE5/issues/85) records observed
refs and open gates. Production-control diagnostic acceptance remains distinct
from scoped ordinary-match/rally/routing passes. #77's published native tests are
uncompiled/unrun. #79's physical drag/focus/terminal/rematch and launcher/cleanup
gates remain open. #80's scoped native/capture passes do not close broad #18, and
its final repair/evidence review remains separately tracked.

Do not infer a combined #79/#80 pass from separate successes. A future reviewed
composition must identify the exact included deltas and rerun affected gates.
All these PRs stay draft/unmerged under their existing gates; this plan makes no
formal approval, readiness or merge decision. Private diagnostic passes do not
automatically validate a different public PR head.

### B. Establish the minimum design and feasibility contract in parallel

- [#81](https://github.com/jaredbahr/RedalertUE5/issues/81): repeated permitted
  private reference observations, uncertainty, conversion/calibration proposals
  and explicit tolerances before replacing placeholder values.
- [#82](https://github.com/jaredbahr/RedalertUE5/issues/82): owner-accepted minimal
  Allied/Soviet roster, map, mechanics/exclusions and role-specific acceptance
  matrix. Do not invent the final unit list.
- [#2](https://github.com/jaredbahr/RedalertUE5/issues/2): agree hardware/budgets
  and a combined rendered two-client protocol, then obtain preliminary generic
  feasibility evidence under an allocated native window.

These are ready to **plan when the future goal starts**, not tasks executing now.
A preliminary scale protocol informs role design; representative roles/assets
later feed back into scale revalidation. Requiring final representative-scale
acceptance before creating any roles would create a circular dependency.

### C. Implement only the accepted next slice later

[#83](https://github.com/jaredbahr/RedalertUE5/issues/83) covers the deferred human
production controls and truthful replicated status, blocked on applicable
production acceptance and a reviewed interaction contract. It must not absorb or
erase #71's missing diagnostic gates.

[#84](https://github.com/jaredbahr/RedalertUE5/issues/84) covers the first agreed
role-specific loop after the reference/roster/preliminary-feasibility contract.
Use existing proxies for technical work where appropriate; art finishing remains
gated. Preserve legacy/generic proof behavior unless its retirement is an explicit
reviewed migration. Feed actual role, animation/material, navigation, bot and
replication costs back into #2 before raising practical budgets.

### D. Visual pilot, Internet and expansion

Prepare #1's Reactor/Rhino/Conscript normal/wide/motion review; record Jared's
explicit approval or revisions before finishing. The later building/vehicle/
infantry production pilot and tool candidates remain as scoped in
[Asset pipeline evaluation](ASSET_PIPELINE_EVALUATION.md); a production building
subject and any service/rights/spend decisions are not silently selected here.
No extracted EA assets/music/reference packs may be published.

#11/#12 design the remaining persistent/Internet and transport behavior. Existing
local slot cleanup/epochs and controller ingress limits are partial engineering
successes, not missing work to recreate or proof of the broader system.
Only then advance the existing milestone 5–6 Yuri/four/eight-player gates.
Do not create a speculative whole-game issue tree now.

## 4. Cross-cutting acceptance for future changes

1. Bind every result to exact clean source, dependency composition, engine/toolchain,
   native test identities, build and reused asset/DLL identity. Retain failed and
   unrun stages separately. Unchanged dependency evidence needs an explicit,
   reviewed source-identity argument; a later sibling pass is not enough.
2. Installed Windows UHT/MSVC build and exact affected native suites precede
   applicable runtime acceptance. Cloud compilation/source review is useful
   independent evidence, never a substitute for Windows UE or rendering.
3. Actual host/two-client proofs compare common epoch/revision/state/digests for
   costs, ownership, production, movement, combat and lifecycle transitions.
   Keep ordinary Victory/Defeat/Coop and relevant legacy compatibility gates.
4. Inspect actual 960×540 and 1280×720 engine captures and normal/wide/motion
   where relevant; include dense/coincident units, selected ID/HP/team,
   controls/costs/rejections, production and terminal/rematch. Record omissions
   and overlays honestly; bounded geometry tests complement visual inspection.
5. Observe real input and focus-loss/recovery separately from injected callbacks.
   A human terminal/rematch/fresh-command cycle and successful owned cleanup are
   required for #17. Preserve pending/denied app-control actions; no workaround
   or elapsed-time inference grants permission.
6. Run combined movement/navigation/congestion, combat/acquisition, bots,
   production and replication on declared hardware against agreed budgets.
   Include representative assets before deriving practical army limits.
   Explicitly distinguish localhost, LAN and Internet/adverse-network tests.
7. Preserve the single allocated heavy-job policy, fresh resource admission,
   unchanged deadlines, exact process identity and fail-closed owned cleanup.
   Sampled soft guards are not hard confinement; forced zero survivors do not
   turn a failed enclosing run into a pass. #33 still requires a causal finding.

## 5. Decisions deliberately left open

The minimum roster and defining mechanics; original-game measurements and UE
conversion/tolerances; target hardware/performance/network budgets and practical
army sizes; supported hosting/LAN/authentication/reconnect outcomes; long-term
engine/toolchain policy; scale/zoom and blockout approval; binary/LFS/storage
budget and asset rights; Yuri scope and four/eight-player acceptance.
Keep these in [Decisions](DECISIONS.md) with evidence/owner decisions, not invented
answers or retroactive approvals.

## 6. Future-goal operating contract

Use [#85's claim-verification protocol](https://github.com/jaredbahr/RedalertUE5/issues/85).
GitHub work-issue comments are the shared source of truth: verify a posted claim
and recheck races before editing; include public worker/goal handle, UTC time,
scope, branch/base, intended files, dependencies, status and blocker. Preserve
existing coordinator reservations. Ambiguous/denied/conflicting claims block
dependent work; stale claims never silently authorize takeover. Keep private
machine paths and local task/session IDs private.

A claim is not a heavy-slot allocation or app-control approval. Report exact
passed/failed/unrun evidence, retain failures, close only observable accepted
scope and update dependents/release ownership. Normal applicable independent
review and merge gates still apply. No implementation, native execution,
deployment, formal approval, security/workflow/credential change, new install,
service or spending is authorized by this planning publication.
