# Controls and negative-evidence slice

This isolated D: integration branch combines the existing drafts for testing. It does not merge main, approve art, or establish internet play or an army budget.

## Current result: graphical fixture passed

Clean tested source **f836d939a9093ae320fa425db5b6d2705571ad06**, report `generated/controls-20261016-064537-040/result.json`: **PASS**, 2026-10-16T06:45:37.1498059Z to06:47:20.981958Z. Warmup14.740s; scenario87.909s including final verification; total103.929s. The unchanged300s/90s deadlines passed; the scenario had little timing margin on this shared host. This is one functional execution, not a reliability or performance benchmark.

Actual topology was a UE dedicated loopback host, rendered owner1 and NullRHI observer owner2. All three completion markers and zero exits passed;53common revision digests matched. Initial records:2human owners/1bot owner,2human+3bot units; final:3human+3bot records,5live/6total. Counts include retained dead/disconnected-owner records. The rendered client can finish before the host observation ends; agreement proves sampled overlap rather than continuous three-party connectivity throughout20seconds.

The shared viewport handlers passed selection refusal/selection/clear, zoom, subpixel ground ray, Move/Attack/Produce/Move requests and accepted receipts, replicated destination/damage/production with credits200->150, separation and captures. Bot mover1 reached(1000,1000,0) with0cm remaining (1cm gate); bot attacker2 exclusively damaged owner2 target5 from100 to97.5HP. Scripted bots are not skirmish AI. Optimized shaders1, actual t.MaxFPS30 and literal-client resolver mode1 were observed; both clients logged the local startup CVar. EnginePak matched584warmup/635rendered key-hit lines including331GlobalShaderMap/PCD3D_SM5 lines per role; material-related classifications include textures and are not material-shadermap totals.

Both fresh1280x720 captures were visually inspected. Selected:786824bytes, SHA256`dbe92294e2344b90e24acb902950ec3a1cad40ce097f4121dfa197f53d712e4b`; final:795913bytes, SHA256`3709162e47548249efe8afb33b700141c5dda5a3fd3d14effc0a38b61ca9e289`. Final shows selected original, separate produced unit6, target3HP90 and credits150. Initial asset-preparation messages and lighting-rebuild text overlapping HUD remain visible. These generic proxies/captures do not approve Tesla/Rhino/Conscript art or presentation polish.

The latest module build passed23.36s; the unchanged command model passed4/4native groups after its numerical fix, and8lightweight runner checks passed. No further heavy rerun is needed absent a new concern.78CPU-policy polls recorded14affinity repairs, zero discovery/cleanup errors; polling cannot ensure continuous confinement between observations. Peak observed D: cache37,369,645bytes/temp12,472bytes stayed below soft2048/1024MiB limits. All owned engine processes exited.

Eight C: roots were audited from06:29:33.6057033Z to06:48:03.0222583Z across this repair/testing interval: seven had zero net byte/file change; D3DSCache had0file/-123944byte net change. Shared cache totals are not project attribution or proof of zero writes. Source, generated evidence and writable UE cache/temp remained on D:; generated/binary files remain ignored.

Physical mouse/keyboard/focus, internet/adverse networking, full skirmish AI, army scale, continuous session lifecycle and art approval remain open. #11/#12 retain their lifecycle/ingress scope. Main and original draft heads are preserved; no binary upload or merge occurred. Historical failed evidence follows so earlier outcomes are not relabeled as passes.

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

## FPS setting overwrite caught before scenario

The shared subpixel/FPS source at cleanb516c9a697d211d71a5522db2ca223e2b09ebe4f built successfully in23.75s. Run `generated/controls-20261016-061551-079` exited normally after asset idle but was correctly rejected before scenario startup:22.170s warmup/23.830s total, observed t.MaxFPS0 despite startup CVar30. Subpixel correction was therefore not exercised by this run.

Installed GameUserSettings.cpp346-348 applies FrameRateLimit through GEngine->SetMaxFPS; UnrealEngine.cpp11961+ preserves the CVar's existing set priority, allowing game settings0 to overwrite the startup30 at that same priority. The project Saved GameUserSettings version5 had FrameRateLimit0. The runner now additionally passes a per-invocation `-ini:GameUserSettings:[/Script/Engine.GameUserSettings]:FrameRateLimit=30`, using the engine's basename-specific INI override syntax. No global settings file was edited. Observed warmup/viewport CVar30 remains mandatory; this PowerShell-only correction does not change the previously built native code or relax timing/controls gates.

## Precise ground input verified; range and admission follow-up

Clean8c348f4c50058db51d4a5784b47146822f6ac316, run `generated/controls-20261016-061850-573`, confirmed actual t.MaxFPS30 after the game-settings override; warmup41.776s completed assetidle2.029s/61ticks. The shared fractional resolver measured0.000164cm round-trip error at(-300,-300), versus5.548618cm from the stock pixel-truncated ray. It sent the intended Move and Attack through normal controller RPCs; movement arrival and both accepted receipts passed. The scenario nevertheless failed at90.354s (134.904s total); no production/final-image/full-pass claim is made.

A reproducible native combat boundary discrepancy was identified: movement stops when computed linear distance is300, but combat's squared comparison can see90000.00000000001 and refuse damage forever. The model now uses the same linear distance for firing as pursuit, without adding range tolerance. Native regressions exercise three perturbed diagonal approaches, continued damage after settling, and harmless advancement just outside300.001cm. The isolated bot-damage witness uses the same boundary; ownership/exclusive-target checks are unchanged. Native execution of these additions is required below.

The admission timeline also recorded zero sent packets during two10s client intervals; first handshake arrived27s after Browse. Installed NetAddressResolution.cpp91-149 queues address resolution even for literal IPs. A source-reviewed fixture-only setup now uses UIpConnection's supported direct numeric-address route: a generated D: `-cvarsini` file sets net.IpConnectionDisableResolution1 only for the rendered and observer clients, guarded to127.0.0.1:port. Startup logs for both and the rendered effective CVar must confirm it. The UDP/stateless handshake, command validation,90s/300s deadlines and20s host observation remain unchanged. This bypasses hostname lookup only for this literal-loopback test; it is not hostname/internet evidence and changes no OS DNS, firewall, driver or global policy. The earlier delay alone did not conclusively prove the resolver was its sole cause.

## Native regression caught an incomplete range correction

At clean508b02dd857a4463d8068d08aa030c78c7134a5e, the incremental build passed in14.67s, but the new native suite correctly failed:3/4groups passed; SimulationAndSnapshots reported four assertions across two perturbed diagonal starts (reaching damage and continued settled damage). Report: generated/native-model-20261016-063240-814/result.json. No graphical run was launched at this head. Matching linear-distance expressions alone was insufficient to resolve the rounded endpoint; the health-based regressions remain required and the outside-range boundary must stay strict.

Corrective build succeeded in8.26s (six actions). Native report generated/native-model-20261016-063738-373/result.json passed all4exact groups, zero failures. Pursuit and combat now share the original strict squared sphere; only a reached rounded endpoint receives at most8iterations of inward representable coordinate steps. There is no added firing-radius tolerance. Independent source review found no material blocker; these tests do not establish exhaustive per-tick ULP/platform robustness. The graphical scenario still requires a fresh run.

## All gameplay outcomes passed; timed client completion failed

Clean60fd954af748d5a6dfbb9e0eb44965b2b18ecf44, generated/controls-20261016-064014-053, failed overall:89.950s warmup,90.785s scenario,183.076s total (06:40:14.883117Z to06:43:17.1411419Z). A prior executor disconnect did not start a run; recovery verified no new directory or UE process before this launch. Warmup exited normally. Actual literal resolver mode1 and t.MaxFPS30 were confirmed; rendered Browse06:42:08.250 reached challenge response08.531 and welcome08.649, eliminating the prior long zero-packet admission interval in this run.

All actual command-path, selection, clear, zoom, subpixel, replicated movement, attack damage, production/credit and separation checks passed. Host independently passed both bot outcomes and final counts after20seconds:2human owners/1bot owner, initial2human+3bot records, final3human+3bot records (5live/6total). Observer explicitly completed. Both fresh1280x720 captures were viewed: selected original unit, then moved original, separate produced proxy, targetHP90 and credits150. Initial capture still shows asset-preparation/debug messages; both show lighting-rebuild text overlapping diagnostic HUD. These are unfinished functional captures, not art approval.

The rendered client's final23game-second idle wait did not complete: the first screenshot stalled game-clock progress for about10wall seconds, while the host closed at its unchanged24seconds after both clients joined. The client then reopened the default map rather than producing its completion marker. Overall acceptance correctly remained failed. The corrective fixture change should finish after all rendered outcome/capture checks succeed; runner still requires host20second observation, observer completion, zero exits and at least3matching3party snapshot revisions. No timeout or gameplay gate should be relaxed.153policy polls recorded16repairs, no cleanup errors; peak observed D: cache37,369,645bytes/temp12,472bytes. All owned UE processes were gone after cleanup.

Completion-fix build passed in23.36s (four actions); eight lightweight runner checks passed. The rendered client now emits successful completion immediately after every gameplay and screenshot check passes. No model code changed after the4/4native pass; all runner deadlines, host/observer observation and three-party digest/capture/exit gates remain unchanged. Fresh graphical execution is still required.
Independent completion review found no material blocker. The rendered client can finish before the host observation ends; final human counts describe retained unit records, and shared revisions establish sampled overlap, not continuous three-party connectivity throughout20seconds.

Independent final evidence review recalculated53shared revisions with zero cross-process/duplicate digest mismatches, correlated all4typed requests/receipts, checked exact bot IDs/outcomes and counts, and verified both PNG signatures/dimensions/hashes/fresh UTC times from raw logs/files. Both captures were viewed independently; diagnostic overlap remains. No evidence blocker was found. The final f836 run also directly measured0.000164cm fractional round-trip error versus5.548618cm stock pixel-truncated error. Subsequent publication edits are documentation only; the tested source SHA above remains the execution identity.

Delivery follow-up: supported Library actions were exposed, but one current-helper batch attempt for both successful PNGs returned `Library prepare_uploads is not available`; no Library item IDs were confirmed and no repeated/alternate upload was attempted. Files remain at their recorded ignored D: capture paths. Next proposed independent work is #11's human/bot owner-allocation boundary, with99/100, uniqueness and exhaustion tests; broader reconnect behavior remains separate. No art roster expansion or merge was performed.
