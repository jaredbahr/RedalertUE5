# HUD readability validation: first allocated batch

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
