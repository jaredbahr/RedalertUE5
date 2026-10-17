# Pointer bounds follow-up

Local branch `fix/pointer-viewport-bounds` starts directly at frozen validated commit `8d9e4ddab4c51cc4ad9270844b738dba4e99fbcc`. It is a separate follow-up for issue #17; the pending integration branch and its earlier receipts remain unchanged.

Source inspection found that `GetPointerScreenPosition` accepted any successful mouse lookup (or synthetic pointer), and `PointerWorldRay` accepted finite points outside its constrained view rectangle. Such coordinates could become an offscreen ground order. A failed lookup also made ordinary selection clear itself through the no-hit path. This is a source-verified gap, not a reproduced physical desktop failure.

The follow-up rejects nonfinite/outside viewport points for both real and projected synthetic input, rejects ground rays outside the constrained view rectangle, and preserves selection when pointer lookup is unavailable or outside the viewport. Valid empty-ground selection still clears; valid world orders retain their existing shared command path. It does not change HUD hit regions, focus rules, bindings, camera defaults, authority, queues or simulation.

Run `cmd /d /c tests\run-pointer-bounds-tests.cmd` from this D: checkout. The script uses the existing installed Visual Studio environment (or the existing D: installation), compiles the actual production header with warnings as errors, and runs 28 boundary cases. Tests cover finite fractional interior points, all viewport edges, nonfinite inputs/bounds, empty/reversed rectangles, 960x540 and offset constrained rectangles. Outputs stay under ignored `generated/`; no UE process is launched.

This focused test verifies the production bounds predicate. Pawn integration is source-reviewed only until a separately allocated UE build and controls/runtime compatibility run. Physical pointer routing, focus/Alt-Tab, selection preservation in the rendered game and all previous runtime scenarios remain unverified at this changed source. Earlier 84/13/36 evidence belongs to its original source bindings and must not be transferred here. Keep issue #17 open; publish/integrate only after the pending authorization is resolved.
