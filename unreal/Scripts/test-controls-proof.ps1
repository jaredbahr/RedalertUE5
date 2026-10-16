param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(20000,60000)][int]$Port = 28879,
    [ValidateRange(45,90)][int]$TimeoutSeconds = 90,
    [ValidateRange(15,600)][int]$WarmupTimeoutSeconds = 300,
    [ValidateRange(0,65536)][int]$MaxCacheMiB = 2048,
    [ValidateRange(0,65536)][int]$MaxTempMiB = 1024,
    [switch]$UnoptimizedShaders
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
$run = Join-Path $repoRoot ('generated\controls-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$images = Join-Path $run 'screenshots'
$cache = Join-Path $repoRoot 'generated\controls-cache'
New-Item -ItemType Directory -Force -Path $run,$images,$cache,(Join-Path $run 'temp') | Out-Null
$watch = [Diagnostics.Stopwatch]::StartNew()
$scenarioWatch = $null
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot record tested HEAD.' }
$dirty = [bool](& git -C $repoRoot status --porcelain)
$result = [ordered]@{passed=$false;head=$head;dirtyWorkingTree=$dirty;startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;elapsedSeconds=0;scenarioElapsedSeconds=0;warmup=[ordered]@{passed=$false;startedUtc=$null;finishedUtc=$null;elapsedSeconds=0;timeoutSeconds=$WarmupTimeoutSeconds;coreLimit=4;reportedShaderWorkers=$null;unusedShaderThreads=1;unusedShaderThreadsDuringGame=1;screenshotAcceptance=$false};scenarioTimeoutSeconds=$TimeoutSeconds;scenarioCoreLimit=2;totalTimeoutBudgetSeconds=($WarmupTimeoutSeconds+$TimeoutSeconds);cachePath=$cache;stages=[ordered]@{};observedCounts=@{};runDirectory=$run;topology='one dedicated UE server, one rendered controls client, one NullRHI passive observer (after standalone graphical warmup)';transport='UE native UDP loopback';physicalMouseAndFocusVerified=$false;internetTest=$false;performanceTest=$false;port=$Port;screenshots=@();commonRevisions=@();error=$null}
$prior=@{}; $processes=[Collections.Generic.List[object]]::new()
$storageGuard=[ordered]@{softPollingGuard=$true;intervalSeconds=10;maxCacheMiB=$MaxCacheMiB;maxTempMiB=$MaxTempMiB;lastCheckElapsedSeconds=-10;checks=0;latestCacheBytes=0;latestTempBytes=0;peakObservedCacheBytes=0;peakObservedTempBytes=0}
$result.storageGuard=$storageGuard
$result.renderConfiguration=[ordered]@{unoptimizedShaders=[bool]$UnoptimizedShaders;expectedShadersOptimize=[int](-not $UnoptimizedShaders);observedShadersOptimize=$null;performanceRepresentative=$false}
$names=@('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
function Mark-Stage([string]$Name) { $result.stages[$Name]=[ordered]@{utc=[DateTime]::UtcNow.ToString('o');elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)} }
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
        $launchArguments=@(('"'+$project+'"'),$Destination)+$Mode+@('-unattended','-nosound','-nosplash','-NoSourceControl','-NoP4','-ddc=BlockoutDDC',('-CoreLimit='+$CoreLimit),'-FPS=30',('-port='+$Port),('-abslog="'+$log+'"'))
        # One Engine override string prevents duplicate -ini:Engine switches
        # from shadowing warmup worker settings or shader optimization mode.
        $engineOverrides=@()
        if($Role -eq 'warmup') { $engineOverrides+=@('[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=1','[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=1') }
        if($UnoptimizedShaders) { $engineOverrides+=@('[ShaderCompiler]:r.Shaders.Optimize=0','[ConsoleVariables]:r.Shaders.Optimize=0') }
        if($engineOverrides.Count) { $launchArguments+=('-ini:Engine:'+($engineOverrides -join ',')) }
        $p=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
        $processes.Add([pscustomobject]@{Process=$p;Started=$p.StartTime;Role=$Role;Log=$log;Path=$editor})
        Mark-Stage ($Role+'Started')
    }
    function Wait-Marker($Entry,[string]$Pattern) {
        do {
            Check-StorageBudget
            $content=if(Test-Path -LiteralPath $Entry.Log){Get-Content -LiteralPath $Entry.Log -Raw}else{''}
            if ($content -match $Pattern) { return }
            if ($Entry.Process.HasExited) { throw "$($Entry.Role) exited before $Pattern." }
            if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "Scenario timed out waiting for $Pattern." }
            Start-Sleep -Milliseconds 200
        } while ($true)
    }
    # ExecCmds is queued in UEngine::Init and executed by the engine loop's
    # TickDeferredCommands after initialization. This warms startup shaders/RHI;
    # it is neither a screenshot check nor controls acceptance.
    $result.warmup.startedUtc=[DateTime]::UtcNow.ToString('o')
    $warmupWatch=[Diagnostics.Stopwatch]::StartNew()
    Start-Controls 'warmup' '/Game/CommandProofGenerated/Maps/CommandProof' @('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ExecCmds="quit"') 4
    try {
        while(-not $processes[0].Process.HasExited) {
            Check-StorageBudget
            if($warmupWatch.Elapsed.TotalSeconds -ge $WarmupTimeoutSeconds) { throw 'Graphics warmup timed out before the controls scenario started.' }
            Start-Sleep -Milliseconds 200
        }
        if($processes[0].Process.ExitCode -ne 0) { throw "Graphics warmup exited $($processes[0].Process.ExitCode)." }
        $warmupLog=Get-Content -LiteralPath $processes[0].Log -Raw
        if($warmupLog -notmatch 'LogRHI:.*(Using Default RHI|RHI Name)' -or $warmupLog -notmatch 'LogLoad:.*LoadMap') { throw 'Warmup exited without expected RHI/map initialization evidence.' }
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
    Start-Controls 'host' '/Game/CommandProofGenerated/Maps/CommandProof' @('-server','-nullrhi','-MULTIHOME=127.0.0.1','-ProofControlsAutomation')
    Wait-Marker $processes[1] 'PROOF_READY'
    Mark-Stage 'hostReady'
    Start-Controls 'rendered' ('127.0.0.1:'+$Port) @('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ProofControlsAutomation',('-ProofControlsOutput="'+$images+'"'))
    # Admission is deliberately ordered: the slower graphics client must own 1.
    Wait-Marker $processes[2] 'PROOF_CONTROLS_VIEW_READY owner=1 width=1280 height=720'
    Mark-Stage 'renderedProjectionReady'
    Start-Controls 'observer' ('127.0.0.1:'+$Port) @('-game','-nullrhi','-ProofControlsObserver')
    while (@($processes | Where-Object {-not $_.Process.HasExited}).Count) {
        Check-StorageBudget
        if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Controls scenario exceeded its strict timeout.' }
        Start-Sleep -Milliseconds 200
    }
    Mark-Stage 'processesExited'
    Check-StorageBudget -Force
    $maps=@()
    foreach ($entry in $processes) {
        if ($entry.Role -eq 'warmup') { continue }
        if ($entry.Process.ExitCode -ne 0) { throw "$($entry.Role) exited $($entry.Process.ExitCode)." }
        $log=Get-Content -LiteralPath $entry.Log -Raw
        if ($log -match 'PROOF_CONTROLS_(CHECK|COMPLETE)[^\r\n]*status=fail') { throw "$($entry.Role) reported controls failure." }
        $role=if($entry.Role -eq 'host'){'host'}elseif($entry.Role -eq 'rendered'){'client'}else{'observer'}
        if ($log -notmatch ('PROOF_CONTROLS_COMPLETE role='+$role+' owner=\d+ status=pass')) { throw "$role missing successful completion." }
        if ($role -eq 'host') {
            foreach($flag in @('botAccepted=2','botMoved=1','botDamaged=1','exclusive=1')) {
                if($log -notmatch ('PROOF_CONTROLS_COMPLETE[^\r\n]*'+$flag)) { throw "Host missing $flag." }
            }
            if($log -notmatch 'PROOF_CONTROLS_TARGET unit=\d+ owner=100') { throw 'Missing explicit static attack target fixture.' }
            foreach($phase in @('initial','final')) {
                $counts=[regex]::Match($log,('PROOF_CONTROLS_COUNTS phase='+$phase+' humanOwners=(\d+) botOwners=(\d+) humanUnits=(\d+) botUnits=(\d+) liveUnits=(\d+) totalUnits=(\d+)'))
                if(-not $counts.Success) { throw "Missing observed $phase runtime counts." }
                $observed=[ordered]@{humanOwners=[int]$counts.Groups[1].Value;botOwners=[int]$counts.Groups[2].Value;humanUnits=[int]$counts.Groups[3].Value;botUnits=[int]$counts.Groups[4].Value;liveUnits=[int]$counts.Groups[5].Value;totalUnits=[int]$counts.Groups[6].Value}
                $expectedHuman=if($phase -eq 'initial'){2}else{3}
                if($observed.humanOwners -ne 2 -or $observed.botOwners -ne 1 -or $observed.humanUnits -ne $expectedHuman -or $observed.botUnits -ne 3 -or $observed.totalUnits -ne ($expectedHuman+3) -or $observed.liveUnits -lt 1 -or $observed.liveUnits -gt $observed.totalUnits) { throw "Unexpected $phase runtime counts: $($counts.Value)" }
                $result.observedCounts[$phase]=$observed
            }
        }
        if ($role -eq 'client') {
            if($log -notmatch 'PROOF_CONTROLS_SCENARIO_READY owner=1 ') { throw 'Missing both-humans scenario clock marker.' }
            $renderConfig=[regex]::Match($log,'PROOF_CONTROLS_RENDER_CONFIG shadersOptimize=(-?\d+)')
            if(-not $renderConfig.Success) { throw 'Missing observed shader optimization setting.' }
            $result.renderConfiguration.observedShadersOptimize=[int]$renderConfig.Groups[1].Value
            if($result.renderConfiguration.observedShadersOptimize -ne $result.renderConfiguration.expectedShadersOptimize) { throw 'Shader optimization setting differs from requested rendering configuration.' }
            foreach($check in @('enemy_not_selected','owned_unit_selected','empty_clears_selection','zoom_in_changes_camera','zoom_out_restores_camera','actual_viewport_projection_deprojection_roundtrip_lt2cm','move_host_receipt_accepted','move_replicated_destination_reached','attack_host_receipt_accepted','attack_replicated_target_damage','produce_host_receipt_accepted','produce_replicated_unit_and_credit_delta','separation_move_host_receipt_accepted','engine_rendered_screenshots_written')) {
                if($log -notmatch ('PROOF_CONTROLS_CHECK[^\r\n]*status=pass evidence='+$check+'\b')) { throw "Missing controls assertion: $check" }
            }
            foreach($seq in 1..4) { if($log -notmatch "PROOF_RESULT owner=1 sequence=$seq accepted=1") { throw "Missing accepted host receipt $seq." } }
        }
        $map=@{}
        foreach($m in [regex]::Matches($log,'PROOF_SNAPSHOT role=(host|client) owner=\d+ revision=(\d+) digest=([A-Za-z0-9]+)')) {
            $revision=$m.Groups[2].Value; $digest=$m.Groups[3].Value
            if($map.ContainsKey($revision) -and $map[$revision] -ne $digest) { throw 'Inconsistent digest within process.' }
            $map[$revision]=$digest
        }
        $maps+=,$map
    }
    $common=@($maps[0].Keys | Where-Object {$maps[1].ContainsKey($_) -and $maps[2].ContainsKey($_)} | Sort-Object {[int]$_})
    if($common.Count -lt 3) { throw 'Need at least three shared host/rendered/observer revisions.' }
    foreach($revision in $common) { if($maps[0][$revision] -ne $maps[1][$revision] -or $maps[0][$revision] -ne $maps[2][$revision]) { throw "Replicated digest mismatch at $revision." } }
    $result.commonRevisions=$common
    foreach($name in @('01-selected.png','02-moved-combat-produced.png')) {
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
    foreach($entry in $processes) {
        if(-not $entry.Process.HasExited) {
            $live=Get-Process -Id $entry.Process.Id -ErrorAction SilentlyContinue
            if($live -and $live.StartTime -eq $entry.Started -and $live.Path -eq $entry.Path) {
                $identity=Get-CimInstance Win32_Process -Filter ('ProcessId='+$entry.Process.Id) -ErrorAction SilentlyContinue
                if($identity -and $identity.CommandLine.Contains($project)) { Stop-Process -Id $entry.Process.Id -Force }
            }
        }
    }
    foreach($n in $names) { if($prior.ContainsKey($n)) { [Environment]::SetEnvironmentVariable($n,$prior[$n],'Process') } }
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o'); $result.elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    if($scenarioWatch) { $result.scenarioElapsedSeconds=[math]::Round($scenarioWatch.Elapsed.TotalSeconds,3) }
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
}
if(-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')
