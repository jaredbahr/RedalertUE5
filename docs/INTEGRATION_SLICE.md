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

The integrated Node contract suite passed all nine tests before controls source changes; that outside-UE result does not validate the new graphical scenario.

| Source/run | Observed result |
| --- | --- |
| `4689d5e` native build | Installed UE 5.7.4 editor module built successfully in 56.57 seconds. |
| `4689d5e` model tests | Four exact groups passed; zero failures. Local report: `generated/native-model-20261016-044859-783/result.json`. |
| `4689d5e` cold controls attempt | Failed at 90.358 seconds waiting for rendered viewport readiness. Only host and rendered startup were launched; passive client was never admitted. No screenshot or graphical pass. |
| `da58ca5` standalone warm-up | Failed at 180.417 seconds before controls scenario startup. The shader worker was active and the D: cache grew; scenario duration was zero. No graphical acceptance. |
| Asset lane `d99a693` | Full Blender/UE negative pipeline passed in 78.190 seconds; see [draft #13](https://github.com/jaredbahr/RedalertUE5/pull/13). Three cases and exact original restoration independently verified. |

The asset lane is a separate child of #10; its measured result does not validate this native branch's renderer. Its published head includes a later PowerShell-only bounded-wait correction, explicitly distinguished from the measured source in its evidence document. Rendered controls acceptance remains blocked at graphical startup; later failed attempts and unchanged strict acceptance gates are recorded below.

## Latest bounded execution and cache audit

The incremental native build containing `9a5d94eb96d90c578c3d985fd6938b7a01702eb0` succeeded (seven actions, 1069.82 seconds wall time). Host contention starved BelowNormal compiler/metadata children; only verified owned build processes were moved to Normal priority and affinity mask `0x3`. This elapsed time is not a build-performance benchmark. No duplicate build was launched after executor reconnection.

Clean tested head `2643bf88c6290d02d42045fbb4ebe50ac4087660`, run `generated/controls-20261016-053354-892`, failed before gameplay: 2026-10-16T05:33:57.2203537Z to 05:38:57.9830022Z, total 303.082 seconds, warmup 300.120 seconds, scenario zero. Three shader workers ran at Normal priority. Startup log applied `r.Shaders.Optimize=0`; no viewport-ready observation exists, so the runtime CVar gate did not pass. No scenario owners/counts, command assertions, shared revisions or screenshots were observed. Planned topology remains host + rendered client + passive client; only standalone warmup actually launched.

UE reset its startup affinity to `0xFF` after the runner's initial assignment/verification of `0x3`. Root observed and manually repaired this twice, then polled verified owned processes. Continuous two-CPU confinement is **not established for that run**. Later runner hardening periodically verifies exact process identity and repairs affinity; its cheap mocked tests are distinguished from a full UE rerun. No heavy retry was launched on the loaded shared PC. All owned UE/shader processes were confirmed gone after expiry.

The D: soft guard made 29 observations: peak observed controls-cache 19,334,873 bytes (18.44 MiB), run temp 2,773,988 bytes (2.65 MiB), below 2048/1024 MiB limits. These are polling measurements, not hard quotas. Cache contents/evidence remain on D: and ignored by Git.

A fresh pre/post audit from 05:33:53.5373923Z to 05:39:08.3330410Z found zero net byte/file change in eight inspected C: roots: UE Common DDC, Common Zen, 5.7 Saved/DDC, Direct3D cache, NVIDIA DXCache, shader temp and UnrealBuildTool. Shared caches cannot be solely attributed to this project; equal aggregate sizes do not prove no writes or zero C: use. An earlier audit found 17 MiB of recently modified NVIDIA cache files but had no baseline, so it was never measured as net growth. Local JSON snapshots preserve the measurements.

Graphical acceptance is blocked at renderer startup, not passed. Physical input/focus, internet play, army-scale performance and full skirmish AI remain unverified. The next graphical attempt should use a quieter host while retaining finite limits and all acceptance gates; no install, global cache relocation, priority escalation beyond Normal, or other-project process change is required by this result.

The final runner change passed seven cheap mock cases covering owned/foreign processes, reset repair, PID reuse, unavailable metadata, cleanup continuation and exit races. The mocks exercise actual policy/stop helpers; restoration/result flags use a test-local finally skeleton. Actual environment restoration and result persistence were source-reviewed, not integration-tested after this change. No C++ changed after the successful native build.

## Diagnostic follow-up: active compilation and omitted installed cache

A single bounded retry of the reviewed runner at clean `c93d1ea3063379ebfe9cfb76059a340a85a5045e` ran from 2026-10-16T05:43:50.3272996Z to 05:48:51.1947665Z. It failed before gameplay: total300.959s, warmup300.078s, scenario0; no images, input assertions or observed scenario counts. Report: `generated/controls-20261016-054350-231/result.json`.

The real runner recorded267 policy polls, two UE affinity reset repairs, three exact owned shader children and zero cleanup/discovery errors. All four sampled processes stayed Normal/mask3 after those repairs. Polling is best-effort and cannot eliminate the startup race. All UE/worker processes were confirmed gone after the run. Seven mock policy/cleanup checks also passed immediately before launch.

Fifteen read-only process/cache samples show active compilation, not a frozen startup: cache files increased4106->5996 from05:44:11 to05:47:18; worker CPU times rose, with about300.84 worker CPU-seconds accumulated by the last sample. Final cache enumeration found6545files/35,047,772bytes. Parent threads mostly waited while shader workers were Ready/Running. Earlier retained logs explicitly named long FTSRRejectShadingCS compiles. Latest log reached D3D12/PCD3D_SM5 and shader startup within seconds, not the earlier two-minute platform-config delay. Current host sampling also showed unrelated Normal/high-priority CPU work; none was altered.

The RTX3080 driver initialized successfully; read-only nvidia-smi reported driver617.14, roughly19-25% GPU utilization and1886-2071MiB used of10240MiB during diagnosis. No device-removed/fatal error was found. These observations favor incomplete CPU shader compilation, with shared-host contention, over a startup deadlock or demonstrated driver/headless failure. They do not prove that later offscreen frames will work.

A concrete configuration omission was found: `[BlockoutDDC]` had only the Local writable node, excluding the installed engine's read-only `D:/Epic Games/UE_5.7/Engine/DerivedDataCache/Compressed.ddp` (1,813,702,940bytes). Installed BaseEngine.ini2756-2765 includes this compressed EnginePak node; DerivedDataBackends.cpp366-407 opens ReadPak with bWriting=false. The project graph now tries that existing read-only cache before Local, using the same installed path/type/compression syntax. Local writable D: routing, soft limits, timeouts and graphical acceptance gates are unchanged. No engine/global file was edited and no cache was copied or uploaded.

The cache correction is source-reviewed and path-checked, **not UE-executed yet**. Whether this installed cache contains matching PCD3D_SM5 keys remains unverified. The optional unoptimized-shader flag may miss normal optimized keys; the next cache-validation attempt should use the default optimized profile and require an EnginePak-open log plus the existing actual frame/control gates. This is a proposed targeted next run, not a claim that the configuration edit solved startup. The requested single bounded diagnostic rerun has been used; another run requires an expanded run budget. No specific driver/security/install action by Jared is supported by the evidence.

## Installed-cache validation and first actual frame

Clean3b941a070229e36950ccc6f04d00c343a2d07b15, run `generated/controls-20261016-055158-162`, completed warmup in35.000s with the default optimized profile. UE opened the installed1729MiB EnginePak, loaded CommandProof, reported zero shaders left and exited0. Four explicitly named material shadermaps missed and compiled; per-key EnginePak hits were not logged at default verbosity, so opening the pak alone does not quantify cache hits or prove every SM5 key matches. The startup blockage was overcome without extending the300s limit or changing drivers/global settings.

The actual host + rendered owner1 + passive owner2 scenario then ran and **failed**, ending at the unchanged90.225s scenario timeout (126.511s total). Real viewport logged1280x720 and shadersOptimize=1. Enemy refusal, own selection, empty clearing, zoom in/out and projection/deprojection passed. Host independently observed exact bot mover arrival and attacker damage on its isolated passive target. Initial counts were2human/3bot records; final production count failed because the human scenario did not reach production. No full controls pass or final screenshot exists.

Fresh `screenshots/01-selected.png` was visually inspected:1280x720,788522bytes,SHA256e1bd1adddecb177f93dbea60ba7c224724511e8d526ccf74805e0fbf495c6ca5. It shows UE-rendered ground, Tesla landmark, five generic tank proxies, owner labels and selection marker/HUD. Lighting-rebuild debug text overlaps part of the HUD; this is unfinished diagnostic presentation, not art approval. Physical input/focus remains unchecked.

Source diagnosis found the intended (-250,0) ground destination is only about36pixels from the enemy at the origin at this camera, within the common40pixel picker. Right-click therefore can choose Attack; an accepted receipt alone is insufficient to establish Move. The next fixture correction must use a clear destination and assert no unit is under that viewport cursor before dispatch, then explicitly record/require request types Move/Attack/Produce/Move. All movement/combat/production snapshot gates remain necessary. This is a distinct controls-fixture defect exposed after startup succeeded, not evidence to repeat identical cold starts.

## Fixture repair build and warmup-shutdown fault

The clear-ground/type-check fixture at clean1b388513ad6ab0a8825326767b226c3ea990c8e8 built successfully in29.87s (seven incremental actions; verified owned build processes limited to mask3/Normal). Eight helper checks passed. Run `generated/controls-20261016-055957-367` stopped before the scenario after12.704s total/11.100s warmup, exit3. It is a failed run, not controls validation.

Verbose DDC logs directly matched559 successful EnginePak key-hit lines, including331 `GlobalShaderMap` hits naming PCD3D_SM5; this now proves matching installed shader keys beyond merely opening the file. Thirty lines originally classified by material-related path included textures, so they are not claimed as30material-shader-map hits. Counts describe matched modern verbose log lines, not an exhaustive hit rate.

Warmup requested normal exit0 through UGameEngine::HandleExitCommand on its first gameplay frame. During teardown, logs show pending shader-job cancellation, then the retained crash context reports EXCEPTION_ACCESS_VIOLATION reading0xffffffffffffffff with RenderCore at the top of the unsymbolized stack. No local RenderCore PDB is installed. This establishes a shutdown-time engine crash; it does not identify a precise engine function or prove driver causality. Crash evidence remains local under ignored Saved/Crashes.

The next correction replaces first-frame deferred quit with an explicit warmup-only asset-compilation idle gate, rather than accepting exit3 or suppressing the crash. Installed ShaderCompiler.cpp879 registers its manager with FAssetCompilingManager, whose remaining-assets API covers that pending work. The normal exit0, RHI/map initialization,300s warmup deadline and90s scenario gates remain required; warmup still makes no screenshot/controls acceptance claim.

## Idle warmup passed; fractional cursor gate failed

At cleanbf1a5767844e51a7338ed69717ad853703a2ceee, the native build passed in22.25s. Run `generated/controls-20261016-060554-958` completed the strengthened warmup in27.646s: remaining assets15->7->0, then2.024real seconds idle over335game ticks and normal exit0. This is one successful execution of the proposed shutdown safety change, not proof that all possible RHI work was drained. Warmup directly logged584EnginePak key hits including331GlobalShaderMap hits.

The scenario still failed:119.665s total/90.175s scenario. The new ground cursor passed empty-pick, then failed the unchanged projection/deprojection<2cm gate before any human command was dispatched. Initial five records and isolated bot move/damage passed; production/final controls did not. No final capture or complete controls pass is claimed.96CPU-policy polls recorded16repairs and zero cleanup errors; peak observed cache37,347,179bytes/temp11,945bytes. All owned processes exited.

Installed SceneView.cpp1483-1484 truncates fractional screen coordinates during stock deprojection; projection preserves fractions. Both engine deprojection overloads do this. That quantization is a source-supported explanation for an arbitrary fractional ground point failing a2cm round-trip gate at this coarse RTS zoom; numeric native-versus-fractional ray diagnostics are required to confirm this run's precise discrepancy. The shared manual/test resolver is being corrected to inverse-project the actual constrained viewport without this truncation, with existing non-finite/degenerate/ground checks retained. The tolerance and authoritative outcome gates are not loosened.

A separate resource-control defect was found: LaunchEngineLoop.cpp4660-4665 uses `-FPS` to set a fixed timestep value, not a render-frequency limit. Installed UnrealEngine.cpp11850-11852 defines `t.MaxFPS` as the actual cap. The runner now applies process-local startup t.MaxFPS30 and will require observed CVar30 at warmup/viewport readiness; previous runs do not establish a30FPS cap. Whole-machine GPU utilization sampled79% during the prior run cannot be solely attributed to this fixture. No global setting or other-project process was changed.
