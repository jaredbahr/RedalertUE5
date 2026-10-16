param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(20000,60000)][int]$Port = 28879,
    [ValidateRange(90,180)][int]$TimeoutSeconds = 180,
    [ValidateRange(15,600)][int]$WarmupTimeoutSeconds = 300,
    [ValidateRange(0,65536)][int]$MaxCacheMiB = 2048,
    [ValidateRange(0,65536)][int]$MaxTempMiB = 1024,
    [switch]$UnoptimizedShaders,
    [switch]$CleanPresentation
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $projectRoot '..')).Path
$project = Join-Path $projectRoot 'RedalertProof.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use a D: checkout.' }
if (-not (Test-Path -LiteralPath $editor)) { throw "Missing editor: $editor" }
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'Content\CommandProofGenerated\Maps\CommandProof.umap'))) { throw 'Generate CommandProof first.' }
$probe = [Net.Sockets.UdpClient]::new()
try { $probe.Client.ExclusiveAddressUse=$true; $probe.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Loopback,$Port)) }
finally { $probe.Dispose() }
$run = Join-Path $repoRoot ('generated\input-bindings-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$images = Join-Path $run 'screenshots'
$control = Join-Path $run 'host-control.txt'
$cache = Join-Path $repoRoot 'generated\input-binding-cache'
New-Item -ItemType Directory -Force -Path $run,$images,$cache,(Join-Path $run 'temp') | Out-Null
$literalClientCVars=Join-Path $run 'client-literal-cvars.ini'
"[Startup]`nnet.IpConnectionDisableResolution=1`n" | Set-Content -LiteralPath $literalClientCVars -Encoding ascii
$watch = [Diagnostics.Stopwatch]::StartNew()
$scenarioWatch = $null
# Restrict every owned UE process to two CPUs allowed to this invoking process.
# Windows children normally inherit their parent's affinity; root separately
# verifies workers because Start-Process + affinity assignment is not atomic.
$availableAffinity=[uint64]([Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64())
$fixtureAffinity=[uint64]0; $fixtureLogicalProcessors=@()
for($cpuIndex=0; $cpuIndex -lt 64 -and $fixtureLogicalProcessors.Count -lt 2; $cpuIndex++) {
    $cpuBit=[uint64]1 -shl $cpuIndex
    if(($availableAffinity -band $cpuBit) -ne 0) { $fixtureAffinity=$fixtureAffinity -bor $cpuBit; $fixtureLogicalProcessors+=$cpuIndex }
}
if($fixtureLogicalProcessors.Count -ne 2) { throw 'Controls fixture requires two available logical CPUs in the invoking process affinity mask.' }
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot record tested HEAD.' }
$dirty = [bool](& git -C $repoRoot status --porcelain)
$result = [ordered]@{passed=$false;head=$head;dirtyWorkingTree=$dirty;startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;elapsedSeconds=0;scenarioElapsedSeconds=0;warmup=[ordered]@{passed=$false;startedUtc=$null;finishedUtc=$null;elapsedSeconds=0;timeoutSeconds=$WarmupTimeoutSeconds;coreLimit=4;reportedShaderWorkers=$null;unusedShaderThreads=1;unusedShaderThreadsDuringGame=1;screenshotAcceptance=$false};scenarioTimeoutSeconds=$TimeoutSeconds;scenarioCoreLimit=2;totalTimeoutBudgetSeconds=($WarmupTimeoutSeconds+$TimeoutSeconds);cachePath=$cache;stages=[ordered]@{};observedCounts=@{};runDirectory=$run;topology='one dedicated UE server, one rendered controls client, one NullRHI passive observer (after standalone graphical warmup)';transport='UE native UDP loopback';cleanPresentation=[bool]$CleanPresentation;physicalMouseAndFocusVerified=$false;internetTest=$false;performanceTest=$false;port=$Port;screenshots=@();commonRevisions=@();error=$null}
$prior=@{}; $processes=[Collections.Generic.List[object]]::new()
$ownedWorkers=[Collections.Generic.List[object]]::new()
$storageGuard=[ordered]@{softPollingGuard=$true;intervalSeconds=10;maxCacheMiB=$MaxCacheMiB;maxTempMiB=$MaxTempMiB;lastCheckElapsedSeconds=-10;checks=0;latestCacheBytes=0;latestTempBytes=0;peakObservedCacheBytes=0;peakObservedTempBytes=0}
$result.inputSource='engineInjection'
$result.pointerSource='projectedSynthetic'
$result.physicalInputVerified=$false
$result.osWindowFocusVerified=$false
$result.positiveRealFocusPanVerified=$false
$result.virtualFocusStagesExplicit=$true
$result.hostControlPath=$control
$result.observerShutdown='runner identity cleanup after rendered evidence and host completion'
$result.storageGuard=$storageGuard
$result.renderConfiguration=[ordered]@{unoptimizedShaders=[bool]$UnoptimizedShaders;expectedShadersOptimize=[int](-not $UnoptimizedShaders);observedShadersOptimize=$null;targetFPS=30;gameUserSettingsFrameRateLimit=30;observedMaxFPS=$null;warmupObservedMaxFPS=$null;performanceRepresentative=$false}
$result.cpuPolicy=[ordered]@{availableAffinityMask=('0x{0:X}' -f $availableAffinity);fixtureAffinityMask=('0x{0:X}' -f $fixtureAffinity);logicalProcessors=$fixtureLogicalProcessors;uePriority='Normal';workerPrioritySetting=0;assignedAfterProcessStart=$true;workerInheritanceVerified=$false;processes=@()}
$result.cpuPolicy.pollIntervalSeconds=1
$result.literalClientSetup=[ordered]@{mode='disable asynchronous resolution for literal 127.0.0.1 clients only';startupFile=$literalClientCVars;expectedResolverDisabled=1;observedRenderedResolverDisabled=$null;startupConfirmedByRole=@{};hostnameOrInternetTest=$false;osDnsChanged=$false}
$result.cpuPolicy.lastPollElapsedSeconds=-1
$result.cpuPolicy.pollCount=0
$result.cpuPolicy.events=@()
$result.cpuPolicy.eventsTruncated=$false
$result.cpuPolicy.discoveryError=$null
$result.cleanup=[ordered]@{errors=@();ignoredExitRaces=0;parentsFirst=$true}
$result.commandRequests=@()
$result.cacheDiagnostics=[ordered]@{verboseEnabled=$true;diagnosticOnly=$true;classification='Engine/DerivedDataCache/*.ddp path; global only GlobalShaderMap/ keys; material-related request/key text may include textures';byProcess=@{};readErrors=@()}
$workerExe=Join-Path $EngineRoot 'Engine\Binaries\Win64\ShaderCompileWorker.exe'
$names=@('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
function Mark-Stage([string]$Name) { $result.stages[$Name]=[ordered]@{utc=[DateTime]::UtcNow.ToString('o');elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)} }
function Get-OwnedLiveProcess($Entry) {
    $candidate=Get-Process -Id $Entry.Process.Id -ErrorAction SilentlyContinue
    if(-not $candidate) { return $null }
    try {
        if($candidate.StartTime -ne $Entry.Started -or $candidate.Path -ne $Entry.Path) { return $null }
    } catch { if($candidate.HasExited) { return $null }; throw }
    return $candidate
}
function Apply-OwnedCpuPolicy($Entry) {
    $live=Get-OwnedLiveProcess $Entry
    if(-not $live) { return }
    try {
    $beforeMask=[uint64]$live.ProcessorAffinity.ToInt64()
    $beforePriority=$live.PriorityClass.ToString()
    if($beforeMask -ne $fixtureAffinity) { $live.ProcessorAffinity=[IntPtr]([int64]$fixtureAffinity) }
    if($live.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { $live.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal }
    $live.Refresh()
    if([uint64]$live.ProcessorAffinity.ToInt64() -ne $fixtureAffinity -or $live.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { throw "Owned $($Entry.Role) CPU policy revalidation failed." }
    if($beforeMask -ne $fixtureAffinity -or $beforePriority -ne 'Normal') {
        $event=@{role=$Entry.Role;pid=$live.Id;startUtc=$Entry.Started.ToUniversalTime().ToString('o');path=$Entry.Path;beforeMask=('0x{0:X}' -f $beforeMask);afterMask=('0x{0:X}' -f $fixtureAffinity);beforePriority=$beforePriority;afterPriority='Normal';utc=[DateTime]::UtcNow.ToString('o');elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)}
        if($result.cpuPolicy.events.Count -lt 256) { $result.cpuPolicy.events+=$event } else { $result.cpuPolicy.eventsTruncated=$true }
    }
    } catch { if(-not(Get-OwnedLiveProcess $Entry)) { return }; throw }
}
function Maintain-OwnedCpuPolicy([switch]$Force) {
    if(-not $Force -and $watch.Elapsed.TotalSeconds-$result.cpuPolicy.lastPollElapsedSeconds -lt 1) { return }
    $result.cpuPolicy.lastPollElapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    $result.cpuPolicy.pollCount++
    $liveParents=@{}
    foreach($entry in $processes) {
        $live=Get-OwnedLiveProcess $entry
        if($live) { $liveParents[$live.Id]=$entry; Apply-OwnedCpuPolicy $entry }
    }
    if($liveParents.Count) {
        try { $candidates=@(Get-CimInstance Win32_Process -Filter "Name='ShaderCompileWorker.exe'" -OperationTimeoutSec 2 -ErrorAction Stop) }
        catch { $result.cpuPolicy.discoveryError=$_.Exception.Message; throw 'Cannot verify shader-worker ownership through process metadata; aborting CPU policy monitor.' }
        foreach($candidate in $candidates) {
            if(-not $liveParents.ContainsKey([int]$candidate.ParentProcessId)) { continue }
            $parent=$liveParents[[int]$candidate.ParentProcessId]
            if(-not(Get-OwnedLiveProcess $parent)) { continue }
            $child=Get-Process -Id $candidate.ProcessId -ErrorAction SilentlyContinue
            if(-not $child) { continue }
            if($child.Path -ne $workerExe -or $child.StartTime -lt $parent.Started) { continue }
            if(-not $candidate.CreationDate -or [math]::Abs(($child.StartTime.ToUniversalTime()-([DateTime]$candidate.CreationDate).ToUniversalTime()).Ticks) -gt 10000) { continue }
            $command=[string]$candidate.CommandLine
            if(-not $command -or $command.Replace('/','\').IndexOf($repoRoot,[StringComparison]::OrdinalIgnoreCase) -lt 0) { throw 'Cannot verify project command line for an owned-parent shader worker.' }
            $known=@($ownedWorkers | Where-Object {$_.Process.Id -eq $child.Id -and $_.Started -eq $child.StartTime})
            if(-not $known.Count) {
                $ownedWorkers.Add([pscustomobject]@{Process=$child;Started=$child.StartTime;Role='shader-worker';Path=$workerExe;ParentId=$parent.Process.Id;CommandLine=$command})
                $result.cpuPolicy.processes+=@{role='shader-worker';pid=$child.Id;parentPid=$parent.Process.Id;startUtc=$child.StartTime.ToUniversalTime().ToString('o');path=$workerExe;affinityMask=('0x{0:X}' -f [uint64]$child.ProcessorAffinity.ToInt64());priority=$child.PriorityClass.ToString();assignedUtc=[DateTime]::UtcNow.ToString('o')}
            }
        }
    }
    foreach($worker in $ownedWorkers) { Apply-OwnedCpuPolicy $worker }
}
function Stop-OwnedEntry($Entry) {
    try {
        $live=Get-OwnedLiveProcess $Entry
        if($live) { Stop-Process -Id $live.Id -Force -ErrorAction Stop }
    } catch {
        $failure=$_.Exception.Message
        try {
            # A normal exit/PID reuse after the first check requires no action.
            $remaining=Get-OwnedLiveProcess $Entry
            if(-not $remaining) { $result.cleanup.ignoredExitRaces++; return }
        } catch { $failure+='; recheck: '+$_.Exception.Message }
        $result.cleanup.errors+=@{role=$Entry.Role;pid=$Entry.Process.Id;message=$failure}
        $result.passed=$false
        if(-not $result.error) { $result.error='Owned-process cleanup failed; see cleanup.errors.' }
    }
}
function Stop-OwnedProcesses {
    # Parents first: they must not replace workers while cleanup is in progress.
    foreach($entry in $processes) { Stop-OwnedEntry $entry }
    foreach($worker in $ownedWorkers) { Stop-OwnedEntry $worker }
}
function Get-PakCacheEvidence([string]$Log) {
    $hits=[regex]::Matches($Log,"(?m)^.*LogDerivedDataCache:\s*(?:Verbose:\s*)?(?<path>[^\r\n]*?\.ddp): Cache hit for (?<key>\S+) from '(?<request>[^']*)'")
    $evidence=[ordered]@{allDdpHitCount=$hits.Count;enginePakHitCount=0;globalShaderHitCount=0;materialRelatedHitCount=0;otherEnginePakHitCount=0;globalExamples=@();materialExamples=@();otherExamples=@()}
    foreach($hit in $hits) {
        $path=$hit.Groups['path'].Value.Replace('\','/').Trim()
        if($path -notmatch '(^|/)Engine/DerivedDataCache/(Compressed|DDC)\.ddp$') { continue }
        $evidence.enginePakHitCount++
        $key=$hit.Groups['key'].Value; $request=$hit.Groups['request'].Value
        $example=@{cachePath=$path;key=$key;request=$request}
        if($key -match '^GlobalShaderMap/') {
            $evidence.globalShaderHitCount++
            if($evidence.globalExamples.Count -lt 3){$evidence.globalExamples+=$example}
        } elseif(($key+' '+$request) -match 'material') {
            $evidence.materialRelatedHitCount++
            if($evidence.materialExamples.Count -lt 3){$evidence.materialExamples+=$example}
        } else {
            $evidence.otherEnginePakHitCount++
            if($evidence.otherExamples.Count -lt 3){$evidence.otherExamples+=$example}
        }
    }
    return $evidence
}
function Check-StorageBudget([switch]$Force) {
    if(-not $Force -and $watch.Elapsed.TotalSeconds - $storageGuard.lastCheckElapsedSeconds -lt 10) { return }
    $cacheBytes=[int64](Get-ChildItem -LiteralPath $cache -File -Recurse -Force | Measure-Object -Property Length -Sum).Sum
    $tempBytes=[int64](Get-ChildItem -LiteralPath (Join-Path $run 'temp') -File -Recurse -Force | Measure-Object -Property Length -Sum).Sum
    $storageGuard.lastCheckElapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    $storageGuard.checks++
    $storageGuard.latestCacheBytes=$cacheBytes; $storageGuard.latestTempBytes=$tempBytes
    $storageGuard.peakObservedCacheBytes=[math]::Max($storageGuard.peakObservedCacheBytes,$cacheBytes)
    $storageGuard.peakObservedTempBytes=[math]::Max($storageGuard.peakObservedTempBytes,$tempBytes)
    if($MaxCacheMiB -gt 0 -and $cacheBytes -gt ([int64]$MaxCacheMiB * 1MB)) { throw "Soft D: cache guard exceeded: $cacheBytes bytes > $MaxCacheMiB MiB." }
    if($MaxTempMiB -gt 0 -and $tempBytes -gt ([int64]$MaxTempMiB * 1MB)) { throw "Soft D: temp guard exceeded: $tempBytes bytes > $MaxTempMiB MiB." }
}
try {
    Check-StorageBudget -Force
    foreach ($n in $names) { $prior[$n]=[Environment]::GetEnvironmentVariable($n,'Process') }
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$cache,'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',(Join-Path $run 'temp'),'Process')
    [Environment]::SetEnvironmentVariable('TMP',(Join-Path $run 'temp'),'Process')
    function Start-Controls([string]$Role,[string]$Destination,[string[]]$Mode,[int]$CoreLimit=2) {
        $log=Join-Path $run ($Role+'.log')
        $launchArguments=@(('"'+$project+'"'),$Destination)+$Mode+@('-unattended','-nosound','-nosplash','-NoSourceControl','-NoP4','-ini:GameUserSettings:[/Script/Engine.GameUserSettings]:FrameRateLimit=30','-ddc=BlockoutDDC','-logcmds="LogDerivedDataCache Verbose"',('-CoreLimit='+$CoreLimit),('-port='+$Port),('-abslog="'+$log+'"'))
        if($Role -in @('rendered','observer')) {
            if($Destination -notmatch '^127\.0\.0\.1:\d+$') { throw 'Literal-client startup override is restricted to this fixture loopback destination.' }
            $launchArguments+=('-cvarsini="'+$literalClientCVars+'"')
        }
        if($CleanPresentation -and $Role -in @('warmup','rendered')) { $launchArguments+='-ExecCmds="DisableAllScreenMessages"' }
        # One Engine override string prevents duplicate -ini:Engine switches
        # from shadowing warmup worker settings or shader optimization mode.
        $engineOverrides=@('[DevOptions.Shaders]:WorkerProcessPriority=0','[ConsoleVariables]:t.MaxFPS=30','[AssetRegistry]:EditorGameScansAR=Async')
        if($Role -eq 'warmup') { $engineOverrides+=@('[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=1','[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=1') }
        if($UnoptimizedShaders) { $engineOverrides+=@('[ShaderCompiler]:r.Shaders.Optimize=0','[ConsoleVariables]:r.Shaders.Optimize=0') }
        if($engineOverrides.Count) { $launchArguments+=('-ini:Engine:'+($engineOverrides -join ',')) }
        $p=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
        $processes.Add([pscustomobject]@{Process=$p;Started=$p.StartTime;Role=$Role;Log=$log;Path=$editor})
        $p.ProcessorAffinity=[IntPtr]([int64]$fixtureAffinity)
        $p.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal
        $p.Refresh()
        if([uint64]$p.ProcessorAffinity.ToInt64() -ne $fixtureAffinity -or $p.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { throw "Could not apply/verify owned $Role CPU policy." }
        $result.cpuPolicy.processes+=@{role=$Role;pid=$p.Id;affinityMask=('0x{0:X}' -f [uint64]$p.ProcessorAffinity.ToInt64());priority=$p.PriorityClass.ToString();assignedUtc=[DateTime]::UtcNow.ToString('o')}
        Mark-Stage ($Role+'Started')
    }
    function Wait-Marker($Entry,[string]$Pattern) {
        do {
            Check-StorageBudget
            Maintain-OwnedCpuPolicy
            $content=if(Test-Path -LiteralPath $Entry.Log){Get-Content -LiteralPath $Entry.Log -Raw}else{''}
            if ($content -match $Pattern) { return }
            if ($Entry.Process.HasExited) { throw "$($Entry.Role) exited before $Pattern." }
            if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "Scenario timed out waiting for $Pattern." }
            Start-Sleep -Milliseconds 200
        } while ($true)
    }
    # Warmup waits for a valid viewport and asset-compilation idle interval;
    # first-frame deferred quit can race shader cancellation/RHI shutdown.
    $result.warmup.startedUtc=[DateTime]::UtcNow.ToString('o')
    $warmupWatch=[Diagnostics.Stopwatch]::StartNew()
    Start-Controls 'warmup' '/Game/CommandProofGenerated/Maps/CommandProof' @('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ProofGraphicsWarmup') 4
    try {
        while(-not $processes[0].Process.HasExited) {
            Check-StorageBudget
            Maintain-OwnedCpuPolicy
            if($warmupWatch.Elapsed.TotalSeconds -ge $WarmupTimeoutSeconds) { throw 'Graphics warmup timed out before the controls scenario started.' }
            Start-Sleep -Milliseconds 200
        }
        if($processes[0].Process.ExitCode -ne 0) { throw "Graphics warmup exited $($processes[0].Process.ExitCode)." }
        $warmupLog=Get-Content -LiteralPath $processes[0].Log -Raw
        if($warmupLog -notmatch 'LogRHI:.*(Using Default RHI|RHI Name)' -or $warmupLog -notmatch 'LogLoad:.*LoadMap') { throw 'Warmup exited without expected RHI/map initialization evidence.' }
        $idleEvidence=[regex]::Match($warmupLog,'PROOF_GRAPHICS_WARMUP_COMPLETE status=pass remaining=0 idleSeconds=([\d.]+) frames=(\d+) width=1280 height=720 maxFPS=([\d.]+)')
        if(-not $idleEvidence.Success -or [double]::Parse($idleEvidence.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture) -lt 2 -or [int]$idleEvidence.Groups[2].Value -lt 3) { throw 'Warmup missing zero-assets idle/viewport completion evidence.' }
        $result.warmup.idleEvidence=$idleEvidence.Value
        $result.renderConfiguration.warmupObservedMaxFPS=[double]::Parse($idleEvidence.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture)
        if($result.renderConfiguration.warmupObservedMaxFPS -ne 30) { throw 'Warmup actual t.MaxFPS is not 30.' }
        $result.warmup.passed=$true
        Mark-Stage 'warmupCompleted'
    } finally {
        $result.warmup.finishedUtc=[DateTime]::UtcNow.ToString('o')
        $result.warmup.elapsedSeconds=[math]::Round($warmupWatch.Elapsed.TotalSeconds,3)
        if(Test-Path -LiteralPath $processes[0].Log) {
            $workerEvidence=[regex]::Match((Get-Content -LiteralPath $processes[0].Log -Raw),'Using (\d+) local workers for shader compilation')
            if($workerEvidence.Success) { $result.warmup.reportedShaderWorkers=[int]$workerEvidence.Groups[1].Value }
        }
    }
    $scenarioWatch=[Diagnostics.Stopwatch]::StartNew()
    Mark-Stage 'scenarioClockStarted'
    Start-Controls 'host' '/Game/CommandProofGenerated/Maps/CommandProof' @('-server','-nullrhi','-MULTIHOME=127.0.0.1','-ProofSkirmish','-ProofInputBindingServer',('-ProofInputBindingControl="'+$control+'"'))
    Wait-Marker $processes[1] 'PROOF_INPUT_BINDING_SERVER_READY epoch=1'
    Mark-Stage 'hostReady'
    Start-Controls 'rendered' ('127.0.0.1:'+$Port) @('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ProofSkirmish','-ProofInputBindingFixture','-ProofCameraPan',('-ProofInputBindingOutput="'+$images+'"'),('-ProofInputBindingControl="'+$control+'"'))
    Wait-Marker $processes[2] 'PROOF_INPUT_BINDING_READY owner=1'
    Mark-Stage 'renderedInputReady'
    # No observer automation flag: older fixtures submit commands or autoexit at 23s.
    Start-Controls 'observer' ('127.0.0.1:'+$Port) @('-game','-nullrhi','-ProofSkirmish')
    Wait-Marker $processes[2] 'PROOF_INPUT_BINDING_COMPLETE role=client status=pass'
    Mark-Stage 'clientCompleted'
    'finish' | Set-Content -LiteralPath $control -Encoding ascii
    Wait-Marker $processes[1] 'PROOF_INPUT_BINDING_COMPLETE role=host status=pass epoch=2'
    Mark-Stage 'hostCompleted'
    foreach($entry in @($processes[1],$processes[2])) {
        while(-not $entry.Process.HasExited) {
            Check-StorageBudget; Maintain-OwnedCpuPolicy
            if($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Input binding scenario exceeded its strict timeout during exit.' }
            Start-Sleep -Milliseconds 200
        }
        if($entry.Process.ExitCode -ne 0) { throw "$($entry.Role) exited $($entry.Process.ExitCode)." }
    }
    $evidencePath=Join-Path $images 'client-evidence.json'
    if(-not(Test-Path -LiteralPath $evidencePath)) { throw 'Missing client-evidence.json.' }
    $clientEvidence=Get-Content -LiteralPath $evidencePath -Raw | ConvertFrom-Json
    $result.clientEvidence=$clientEvidence
    if($clientEvidence.status -ne 'pass' -or $clientEvidence.positivePanFocusSource -ne 'virtualFixture' -or $clientEvidence.simulatedLostFocus -ne $true) { throw 'Missing successful explicitly virtual-focus client evidence.' }
    $requiredChecks=@('native_select_and_release','native_shift_add_and_modifier_release','native_shift_toggle_off','native_shift_toggle_on','three_groups_repeated_binding_callbacks','three_groups_independent_contents_recalled','mixed_group_move_mobile_only_structure_fixed','mixed_group_attack_combat_only_replicated_damage','native_group_recall_prunes_dead_member','native_wheel_up_and_release','native_wheel_down_restores_width','injected_arrow_down_polled','virtual_focus_injected_pan_hits_world_limit','simulated_lost_focus_flushes_arrow_and_modifiers','native_rematch_epoch_reset_reuses_ids_clears_selection_queue','old_group_empty_after_reused_ids_epoch_reset','rendered_captures_written')
    $renderedLog=Get-Content -LiteralPath $processes[2].Log -Raw
    foreach($check in $requiredChecks) {
        if($check -notin $clientEvidence.checks -or $renderedLog -notmatch ('PROOF_INPUT_BINDING_CHECK[^\r\n]*status=pass evidence='+[regex]::Escape($check)+'\b')) { throw "Missing exact engine-input assertion: $check" }
    }
    $result.requiredChecks=$requiredChecks
    if($clientEvidence.injectedEdges -lt 40 -or $clientEvidence.nativeCallbacks.left -lt 8 -or $clientEvidence.nativeCallbacks.right -lt 4 -or $clientEvidence.nativeCallbacks.groups -ne 17 -or $clientEvidence.nativeCallbacks.produce -ne 2 -or $clientEvidence.nativeCallbacks.build -ne 2 -or $clientEvidence.nativeCallbacks.wheelUp -ne 1 -or $clientEvidence.nativeCallbacks.wheelDown -ne 1 -or $clientEvidence.nativeCallbacks.rematch -ne 1) { throw 'Missing expected native binding callback/edge counts.' }
    if($clientEvidence.inputSource -ne 'engineInjection' -or $clientEvidence.pointerSource -ne 'projectedSynthetic' -or $clientEvidence.physicalInputVerified -ne $false -or $clientEvidence.osWindowFocusVerified -ne $false -or $clientEvidence.positiveRealFocusPanVerified -ne $false) { throw 'Incorrect client input-source or desktop-acceptance classification.' }
    $maps=@()
    foreach($entry in @($processes[1],$processes[2],$processes[3])) {
        $log=Get-Content -LiteralPath $entry.Log -Raw
        if($log -match 'PROOF_INPUT_BINDING_(CHECK|COMPLETE)[^\r\n]*status=fail') { throw "$($entry.Role) reported input fixture failure." }
        if($entry.Role -eq 'host' -and $log -notmatch 'PROOF_INPUT_BINDING_DUEL epoch=1 victim=\d+ attacker=\d+ commandsAccepted=3') { throw 'Missing ordinary bot duel commands.' }
        if($entry.Role -in @('rendered','observer') -and $log -notmatch 'Set CVar \[\[net\.IpConnectionDisableResolution:1\]\]') { throw 'Missing literal resolver startup evidence.' }
        $map=@{}
        foreach($m in [regex]::Matches($log,'PROOF_SNAPSHOT role=(host|client) owner=\d+ epoch=(\d+) revision=(\d+) digest=([A-Za-z0-9]+)')) {
            $key=$m.Groups[2].Value+':'+$m.Groups[3].Value; $digest=$m.Groups[4].Value
            if($map.ContainsKey($key) -and $map[$key] -ne $digest) { throw "Inconsistent digest inside $($entry.Role) at $key." }
            $map[$key]=$digest
        }
        $maps+=,$map
    }
    $common=@($maps[0].Keys | Where-Object {$maps[1].ContainsKey($_) -and $maps[2].ContainsKey($_)} | Sort-Object)
    if($common.Count -lt 10) { throw 'Need at least ten matching host/rendered/passive-observer epoch/revision snapshots.' }
    foreach($key in $common) {
        if($maps[0][$key] -ne $maps[1][$key] -or $maps[0][$key] -ne $maps[2][$key]) { throw "Replicated digest mismatch at $key." }
    }
    if(-not @($common | Where-Object {$_ -like '2:*'}).Count) { throw 'Missing shared snapshot after ordinary rematch.' }
    $result.commonRevisions=$common
    $result.clientEvidencePath=$evidencePath
    foreach($name in @('01-selected-group.png','02-group-moved.png','03-group-attacked.png','04-terminal.png')) {
        $path=Join-Path $images $name
        if(-not(Test-Path -LiteralPath $path)) { throw "Missing fresh engine screenshot $name." }
        $file=Get-Item -LiteralPath $path
        if($file.Length -le 1024 -or $file.LastWriteTimeUtc -lt [DateTime]::Parse($result.startedUtc).ToUniversalTime()) { throw "Stale/empty screenshot $name." }
        $bytes=[IO.File]::ReadAllBytes($path)
        if([BitConverter]::ToString($bytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A') { throw "Invalid PNG $name." }
        $width=($bytes[16]*16777216)+($bytes[17]*65536)+($bytes[18]*256)+$bytes[19]
        $height=($bytes[20]*16777216)+($bytes[21]*65536)+($bytes[22]*256)+$bytes[23]
        if($width -ne 1280 -or $height -ne 720) { throw "Unexpected screenshot size ${width}x${height}." }
        $result.screenshots+=@{path=$path;bytes=$file.Length;width=$width;height=$height;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
    Mark-Stage 'evidenceVerified'
    $result.passed=$true
} catch { $result.error=$_.Exception.Message }
finally {
    try { Stop-OwnedProcesses }
    finally {
        try {
            foreach($n in $names) {
                if($prior.ContainsKey($n)) {
                    try { [Environment]::SetEnvironmentVariable($n,$prior[$n],'Process') }
                    catch { $result.cleanup.errors+=@{role='environment';name=$n;message=$_.Exception.Message}; $result.passed=$false; if(-not $result.error){$result.error='Process environment restoration failed.'} }
                }
            }
        } finally {
            foreach($entry in $processes) {
                if(Test-Path -LiteralPath $entry.Log) {
                    try {
                        $finalLog=Get-Content -LiteralPath $entry.Log -Raw
                        $result.cacheDiagnostics.byProcess[$entry.Role]=Get-PakCacheEvidence $finalLog
                        if($entry.Role -in @('rendered','observer')) {
                            $startup=[regex]::Match($finalLog,'Set CVar \[\[net\.IpConnectionDisableResolution:1\]\]')
                            if($startup.Success) { $result.literalClientSetup.startupConfirmedByRole[$entry.Role]=$startup.Value }
                        }
                    }
                    catch { $result.cacheDiagnostics.readErrors+=@{role=$entry.Role;message=$_.Exception.Message} }
                }
            }
            $result.finishedUtc=[DateTime]::UtcNow.ToString('o'); $result.elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
            if($scenarioWatch) { $result.scenarioElapsedSeconds=[math]::Round($scenarioWatch.Elapsed.TotalSeconds,3) }
            $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
        }
    }
}
if(-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')
