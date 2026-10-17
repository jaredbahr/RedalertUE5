# HUD readability validation

Scoped acceptance passed at clean **2f3e5947b9907db023a9c27079ca43166fccf84f**. Publication changes afterward are evidence/documentation only. The [accepted-batch ledger](evidence/hud-readability-accepted-batch.json) retains result hashes, screenshot hashes, counts, projection records and runner/source identities.

| Final gate | Retained run | Result |
|---|---|---|
| Build | hud-readability-build-timeprecision.log | Passed11.16s |
| Actual helper cases | hud-layout-model-20261017-024302-244 |5/5,0failed,12.516s,verifiedexit |
| Victory1280x720 | small-match-victory-20261017-024342-373 |68.701scenario/68.918total,5freshPNG,cleanup[] |
| Victory960x540 | small-match-victory-20261017-024528-792 |68.958scenario/69.210total,5freshPNG,cleanup[] |
| Legacy controls | controls-20261017-024649-118 |41.422scenario/51.466total,55sharedrevisions,2freshPNG,cleanup.errors[] |

Root and independent review inspected all twelve images at native size. Both viewports retain primaryidentifier/panelHP100/Cargo0, ore text and contained left/right/panel-edge annotations. All six targets logged0.000pixelerror for the same selected unit32 perrun; actual meshes and frozen snapshot guards passed. All eligible groups were placed, with zero omissions. Eligible unit counts were12/14/5/10/11 at1280 and12/15/5/11/3 at960. Corresponding culls were0/0/9/4/3 and0/0/10/4/12; these include deliberate offscreen/UI occlusion, not unreadable groups hidden from the count.

This supports the bounded layout improvement. It does **not** satisfy all of issue #18: individual unselected bars/tethers remain ambiguous in the overlapping combat pile, and960panel-edge rematch text has weak contrast over light geometry. Keep #18 open with those gaps. Startup #33 remains unresolved. No physical-input, final-art, Internet or army-performance claim is added.

The rig move and precision fixes each followed a specific retained failure and independent source review. No assertion, two-pixel tolerance, deadline or gameplay outcome was weakened; no failed result was converted into a pass. The legacy path retains its original labels and selected-unit/control outcomes. Owned processes exited before publication work.

## First allocated batch (historical failure)

This batch does not pass PR #38's integration gates. Main remains `49bd2e080b07476d17889f00687ffa4081ad5a60`; issues #18 and #33 remain open. Raw logs/results and both PNGs remain locally ignored. Their hashes, exact source identities, counts and process cleanup are retained in [the first-batch ledger](evidence/hud-readability-first-batch.json).

| Gate / exact source | Result |
|---|---|
| Initial build, 25a79f0 | Failed after 252.89s: C4458, local Owner hides AActor::Owner |
| Corrected build, 7cc0861 | Passed after 721.55s; only fix was a behavior-preserving local rename, independently reviewed |
| Five actual-helper tests, clean 7cc0861 | 5/5 passed, zero failed, 18.022s; owned process exit verified, no forced termination |
| Victory 1280x720, clean 7cc0861 | Failed after 67.512s scenario / 67.753s total, within fixed240s; actual winner0/all gameplay flags1, then readability_projection_target at first edge; cleanupErrors=[] |
| Victory960 / legacy controls | Not run: queue stopped on the failed rendered gate |

Two fresh native1280 images exist: base and terminal result. Root and independent reviewer inspected them. Primary numeric HP/cargo, controls/status, ore amounts and terminal text are legible, and both layout records show zero omitted unit/resource groups. The base primary C#10 label is displaced above another marker; its yellow tether passes through that marker and the cluster. This remains a visual association limitation. There are no edge captures, no960 captures and no complete visual acceptance.

## Diagnosed fixture assumption and pending correction

Installed UE BaseEngine.ini sets LocalPlayer AspectRatioAxisConstraint to MaintainYFOV. The installed CameraStackTypes unconstrained orthographic branch scales world units per pixel by viewport height under that policy. The fixture instead assumed OrthoWidth divided by viewport width, producing only56.25% of the required translation at16:9. This is a source/configuration explanation of the invalid assumption; the failed run did not record its effective matrix or failed projected coordinates.

The prepared correction unprojects the old anchor screen position and desired target through the same actual inverse projection, then moves the camera by the origin difference. It preserves exactly one camera adjustment, the later-frame two-pixel limit, viewport/finite/snapshot guards and all gameplay/deadline assertions. Future projection failures log their actual coordinates. This corrected source still needs a fresh build and rendered validation; the passing7cc module cannot validate it.

The shared heavy slot was released at02:16:09.4215798 UTC after confirming zero owned build/test processes. Imagination has queued work. Resume the [unchanged full gate list](HUD_READABILITY.md) only after a new allocation. No automatic retries or deadline increases occurred: the single rebuild followed a specific compile fix, and no rendered retry has run.

Integration is additionally on hold because the parent reported automatic approval review rejected conditional PR38 integration and is seeking fresh explicit approval. No merge, issue closure, hosted Actions, binary upload or OS/settings change occurred. Startup33 remains unexplained; the long build does not establish its cause.

## Second allocated projection attempt

At clean596bb3d the fresh build passed25.72s, and5/5 helper cases passed13.564s with verified exit. The first1280 Victory again reached winner0/all gameplay flags but failed the first left-edge check after67.473 scenario seconds/67.697total; cleanupErrors=[]. Actual anchor631.153,392.115 missed24,468. No960 or legacy run followed this failure. The [attempt ledger](evidence/hud-readability-projection-attempt.json) preserves raw identities and hashes.

Installed CameraComponent derives CameraToViewTarget from the owning pawn; SceneView adds it to the projection origin. Moving only the camera relative to its fixed owner can cancel lateral translation. The next prepared correction translates the camera rig once while immediately restoring every attached unit view's world transform, then checks actual mesh centers, unchanged snapshot and the same later two-pixel limit. Rig/projection logs will expose the active movement path. No ortho defaults or acceptance limits are disabled. This correction still needs its own clean-source build and runtime evidence.

## Rig move and file-time precision

Clean ae098ec built in10.79s and passed5/5 helper cases in12.525s. Victory1280 reached its expected winner and passed the first projection and actual mesh-position checks, writing03-left-edge.png, but failed its timestamp assertion after66.172 scenario seconds; cleanupErrors=[]. [The rig-attempt ledger](evidence/hud-readability-rig-attempt.json) retains the failure and image hash.

Installed WindowsPlatformFile.cpp110-117 deliberately truncates FILETIME to whole seconds. The fixture compared that coarse value with a fractional request time, rejecting fresh images written in the same second. The correction compares both at whole-second precision, with request/file times logged. The path-absent-before-request, file-size, settle and runner's native freshness/hash/PNG/dimensions requirements remain mandatory. No stale image or failed run is accepted retroactively.
