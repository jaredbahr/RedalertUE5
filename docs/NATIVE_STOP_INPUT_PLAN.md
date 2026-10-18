# Native S-key Stop acceptance

Local branch `test/native-stop-input-binding` starts at main `f1a106ad8be722ce31bc4cc568c043b4b398d857` (merged PR43). The tested pointer-bounds follow-up remains separate and unchanged.

This narrow issue #17 fixture sends one S press and release through installed UE's timestamped `UGameViewportClient::InputKey`, using the local player's mapped device. It waits for the registered pawn input component to complete a normal input-stack pass, then observes the bound `OnManualStop` callback. It has no direct-handler fallback, OS cursor movement or forced focus. Ordinary Stop automation still calls the shared handler and requires zero native S callbacks.

Run the opt-in mode with `./unreal/Scripts/test-controls-proof.ps1 -CleanPresentation -StopControls -StopInputBinding`. The new flag reaches only the rendered role; invalid combinations fail before paths, ports or processes exist. Existing 55-second client active limit, 90-second scenario limit, 300-second graphics warmup, storage guards and owned-process cleanup remain unchanged.

## Accepted scoped evidence

A fresh installed UE5.7.4 editor-module build passed in 20.14 seconds at clean `516b77ccb2ae30bf466e77180228b1b0ff21d50f`, with two CPUs and one parallel build action at a time. DLL SHA256: `3B907B3D700F15666607B773EB220520E612667048EA4793200CBC4B7B9AA0D3`. All 14 reused local assets matched recorded sizes and hashes. Documentation-only descendants preserve the tested engine inputs.

| Gate | Local directory under generated/ | Scenario seconds / limit | PNGs |
| --- | --- | ---: | ---: |
| Default controls | controls-20261017-143441-169 | 43.650 / 90 | 2 |
| Ordinary Stop | controls-20261017-143551-031 | 44.909 / 90 | 4 |
| Engine-injected S Stop | controls-native-stop-20261017-143648-977 | 44.710 / 90 | 4 |

All three scenarios passed their original assertions. Root independently reran raw parsers and checked all ten fresh 1280x720 PNG signatures, dimensions, hashes and timestamps, then viewed them at native size. Independent raw/image review records remain local alongside the results. Exact arguments and source/config/project/DLL/asset bindings are in `generated/native-stop-runtime-plan.json` and `native-stop-build-binding.json`; the per-row result hashes are in `native-stop-runtime-batch.json`.

Host and both clients agreed at 55 default and 68 Stop/injected shared revisions. Ordinary Stop retained native count0 and an eight-revision stationary window of 1.4 host-simulation seconds. The injected run used owner1/unit4/epoch0/device0/user0: press frame496, callback/release frame497 and processed release frame498. Exactly one callback and two edges preserved selection, produced accepted Stop sequence6, held position through nine common revisions/1.6 host-simulation seconds, and then accepted sequence7 Move/resumption. The press API returned unhandled despite the later callback: that return value is diagnostic, not acceptance evidence. Injected captures show native S1 and accepted host receipts6/7; ordinary captures retain S0.

Lightweight checks passed in root and independent review: 60 existing Stop evidence cases, 62 injection-policy cases, 14 fixture/source/preflight guards and eight controls ownership/cleanup checks. These synthetic/source checks are separate from the engine runs. No full model-suite, match, Internet, performance or final-art acceptance is transferred here.

## Retained failures and limits

The first build at `8482aa256f9cb208384bfcbca43e254fdf16ce17` failed with a viewport const mismatch in 41.72 seconds. The next at `c2e487cfed911a59c9a55770d1a1694c9cc4eba9` compiled the correction but failed linking in 13.50 seconds: the actual link response omitted ApplicationCore.lib required by the device mapper. The accepted source declares that direct private dependency. Both failed logs and the failed link response remain archived; indices are `generated/native-stop-failure-8482aa2-index.json` and `native-stop-c2-build-failure.json`. Neither failed head launched a runtime row. The later pass does not relabel those failures.

The allocation was released at 2026-10-17 14:38:03 UTC with zero UE/build processes remaining. Per-run owned cleanup receipts provide cleanup evidence; the final inventory is only a current workload observation.

The accepted run reports `inputSource=engineInjection`, `pointerSource=projectedSynthetic`, `physicalInputVerified=false` and `osWindowFocusVerified=false`. Physical S, other bindings/modifiers, Alt-Tab and OS focus remain open under issue #17. The old broad input/higher-digit fixture evidence is not transferred. Dense legacy labels and the clipped native-status suffix remain visible; issue #18 remains open.

Historical preparation status (superseded by the later publication and combined-acceptance checkpoints below): the candidate was then local and unpublished, with publication authorization pending.

## Combined acceptance checkpoint

The combined candidate at `9b9403f51417fcce3de159b779943c49bae31023` was subsequently built and checked after pointer-bounds reconciliation. Source tree `7592f7c0e3fcd30fc9b9002855acb8c7c7e8690d`, Config and project inputs bind the fresh DLL `82269BA43E6E2EDE4E82734C6C82D1242265C7142D68A8BCDA516311FC3BFB3E`.

The unchanged exact 12-case native suite, default controls, native S-key Stop binding and cooperative context-routing scenarios passed. All 11 fresh captures were inspected at native size by root and independent review. Cooperative checks recorded 324 matching revisions, ten context phases and 126 commands. The initial combined native timeout remains a failed attempt; a separate readiness diagnostic passed before the new unchanged acceptance run. No startup cause is declared fixed.

This supersedes the earlier pending-combination statement for that exact checkpoint. Original standalone and failed evidence is retained privately; later documentation-only publication changes do not change the bound engine inputs. Physical keyboard/mouse, OS focus, internet play, army-scale performance, art and broader HUD acceptance remain unverified. Issues #17/#18 remain open. No paid Actions or raw private logs are needed for publication.