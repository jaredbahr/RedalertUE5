# Native UE command proof

This narrow fixture moves the shared command route into UE C++. The host derives each human's identity from its player controller, validates Move/Attack/Produce requests, advances the model and replicates snapshots. A scripted bot submits through the same validator. Movement is calculated from a common starting state; damage is accumulated before applying it. Constants are fixture values, not proposed game balance or army budgets.

## Reproduce locally

Use this the designated non-system drive checkout and installed UE 5.7.4, Visual Studio 2022/MSVC 14.44 and Windows SDK 10.0.26100.0. No install is required. First generate the original FBXs as described in [the art pipeline](ART_PIPELINE.md), then run from the repository root:

```powershell
./unreal/Scripts/build-native-proof.ps1
./unreal/Scripts/launch-proof.ps1 -NativeSetup -NullRHI
./unreal/Scripts/test-native-model.ps1
./unreal/Scripts/test-native-proof.ps1 -TimeoutSeconds 90
```

Builds use three parallel actions with UBA disabled. The launch/test helpers set process-local the designated non-system drive cache and temporary paths. Verify the emitted logs; this does not promise zero system-drive writes by installed tools. Do not run a second heavy build/render alongside these checks.

Native setup imports the unchanged, unapproved blockouts and creates `/Game/CommandProofGenerated/Maps/CommandProof`, with four tagged fixture actors. `/Game/BlockoutGenerated/Maps/VisualProof` remains the separate art review scene. Source scripts are tracked; generated maps, assets, logs, reports and binaries are ignored.

## Observed evidence

The editor module compiled successfully with the installed UE 5.7.4 toolchain. The following ignored reports are retained locally under `generated/`:

| Run | Result |
| --- | --- |
| `native-model-<local-run>/result.json` | Four exact native test groups succeeded; zero failures. |
| `native-network-<local-run>/result.json` | One headless UE host and two headless UE clients exited successfully; 53 common revisions had matching snapshot digests. |

The host bound to `127.0.0.1:28777`. It accepted six human commands and two bot commands and rejected two wrong-owner requests. Both clients acknowledged the expected ordered requests and observed movement, damage and production. These flags describe aggregate replicated activity; they do not prove bot-specific movement/combat outcomes or a skirmish AI. Independent source/evidence review found no blocker within this fixture's scope.

See [the test plan](UE_TEST_PLAN.md) for exact acceptance checks and [controls](UE_CONTROLS.md) for manual graphical checks. Snapshot agreement proves replication from this authoritative host, not independent lockstep determinism. Digests are diagnostic, not security credentials.

## Remaining gates

Graphical controls and readability have compiled but have not been manually exercised. Internet traversal, reconnects, adverse networks, eight players, large armies, practical CPU/network budgets, navigation, varied units and a complete bot skirmish remain unverified. The current whole-snapshot replication is correctness scaffolding; measurements must guide the eventual replication design. Art still requires Jared's blockout approval before finishing.
