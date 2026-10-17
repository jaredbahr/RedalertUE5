# Pointer bounds follow-up

Local branch `fix/pointer-viewport-bounds` starts directly at validated commit `8d9e4ddab4c51cc4ad9270844b738dba4e99fbcc`. That base subsequently merged through [PR #43](https://github.com/jaredbahr/RedalertUE5/pull/43), whose merge tree exactly matches the validated tree. This follow-up addresses pointer handling within issue #17.

Source inspection found that `GetPointerScreenPosition` accepted any successful mouse lookup (or synthetic pointer), and `PointerWorldRay` accepted finite points outside its constrained view rectangle. Such coordinates could become an offscreen ground order. A failed lookup also made ordinary selection clear itself through the no-hit path. This is a source-verified gap, not a reproduced physical desktop failure.

The follow-up rejects nonfinite/outside viewport points for both real and projected synthetic input, rejects ground rays outside the constrained view rectangle, and preserves selection when pointer lookup is unavailable or outside the viewport. Valid empty-ground selection still clears; valid world orders retain their existing shared command path. It does not change HUD hit regions, focus rules, bindings, camera defaults, authority, queues or simulation.

Run `cmd /d /c tests\run-pointer-bounds-tests.cmd` from this D: checkout. The script uses the existing installed Visual Studio environment (or the existing D: installation), compiles the actual production header with warnings as errors, and runs 28 boundary cases. Root and independent review each passed all 28. Tests cover finite fractional interior points, all viewport edges, nonfinite inputs/bounds, empty/reversed rectangles, 960x540 and offset constrained rectangles. Outputs stay under ignored `generated/`; this focused runner launches no UE process.

## Scoped engine compatibility

A fresh installed UE5.7.4 editor-module build passed at clean `8783edc7e2641d8ba33316834ea310d4bc94212e` in 82.62 seconds, using two CPUs and one parallel build action at a time. DLL SHA256: `8E0FBBF5E8ECA6E49A349953300A8C25D332D0324756639ABEC550AB0E797CA8`. All 14 reused local assets matched their recorded sizes and hashes. Subsequent documentation-only descendants do not change those engine inputs.

Six serial runtime checks passed at that source with unchanged runner deadlines and assertions. All 23 fresh engine PNGs were checked for signatures, dimensions, hashes and timestamps and inspected at native size by root. Independent review records are retained locally alongside the results.

| Gate | Local directory under generated/ | Scenario seconds / unchanged limit | PNGs |
| --- | --- | ---: | ---: |
| Default controls | controls-20261017-140103-721 | 44.220 / 90 | 2 |
| Stop controls | controls-20261017-140228-721 | 45.403 / 90 | 4 |
| Type selection | controls-type-selection-20261017-140329-638 | 44.530 / 90 | 4 |
| Victory, HUD/camera 960x540 | small-match-victory-20261017-140429-206 | 95.977 / 240 | 7 |
| Cooperative factory rally | small-match-coop-20261017-140608-480 | 130.370 / 240 | 3 |
| Attack Move | attack-move-proof-20261017-140822-224 | 53.022 / 90 | 3 |

Controls and Attack Move graphics warmup remained separately bounded at 300 seconds. Exact invocations, clean source/config/project/DLL bindings, asset hashes and per-row result hashes are in local `generated/pointer-runtime-invocations.json`, `pointer-build-binding.json` and `pointer-runtime-batch.json`. Root raw/image review and independent receipts remain beside them. The heavy allocation was released with no UE/build processes remaining.

These scenarios establish valid-pointer compatibility through synthetic shared handlers, with host receipts and matching replicated state. They do not inject an unavailable mouse lookup or a constrained/invalid pointer into the engine. Those negative branches remain covered by the production-header tests and independent source review. Physical input, Alt-Tab/focus, and rendered selection preservation after a failed lookup remain unverified. Keep issue #17 open. The 960x540 base frame still omits its primary world annotation group; other dense labels remain omitted, so issue #18 stays open too.

Earlier 84-case/13-scenario/36-image evidence belongs to its original source bindings and is not transferred to this changed source. No full model-suite rerun, Internet, performance, final-art or broader faction acceptance is claimed here. Publication does not close issue #17 or #18; the acceptance limits above remain in force.
