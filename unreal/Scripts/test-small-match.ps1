param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateSet('Victory','Defeat','Coop')][string]$Scenario = 'Victory',
    [ValidateRange(20000,60000)][int]$Port = 28989,
    [ValidateRange(30,240)][int]$TimeoutSeconds = 240,
    [ValidateRange(0,65536)][int]$MaxCacheMiB = 2048,
    [ValidateRange(0,65536)][int]$MaxTempMiB = 1024,
    [switch]$CleanPresentation
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $projectRoot '..')).Path
$project = Join-Path $projectRoot 'RedalertProof.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$workerExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\ShaderCompileWorker.exe'
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use a D: checkout.' }
if (-not (Test-Path -LiteralPath $editor)) { throw "Missing editor: $editor" }
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'Content\CommandProofGenerated\Maps\CommandProof.umap'))) { throw 'Generate and copy the existing CommandProof map first.' }
$probe = [Net.Sockets.UdpClient]::new()
try { $probe.Client.ExclusiveAddressUse=$true; $probe.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Loopback,$Port)) }
finally { $probe.Dispose() }

$run = Join-Path $repoRoot ('generated\small-match-' + $Scenario.ToLowerInvariant() + '-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$images = Join-Path $run 'screenshots'
$cache = Join-Path $repoRoot 'generated\small-match-cache'
$temp = Join-Path $run 'temp'
New-Item -ItemType Directory -Force -Path $run,$images,$cache,$temp | Out-Null
$literalClientCVars = Join-Path $run 'literal-client-cvars.ini'
"[Startup]`nnet.IpConnectionDisableResolution=1`n" | Set-Content -LiteralPath $literalClientCVars -Encoding ascii
$watch = [Diagnostics.Stopwatch]::StartNew()
$scenarioWatch = $null
$availableAffinity = [uint64]([Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64())
$fixtureAffinity = [uint64]0; $fixtureLogicalProcessors = @()
for ($cpuIndex=0; $cpuIndex -lt 64 -and $fixtureLogicalProcessors.Count -lt 2; $cpuIndex++) {
    $cpuBit = [uint64]1 -shl $cpuIndex
    if (($availableAffinity -band $cpuBit) -ne 0) { $fixtureAffinity = $fixtureAffinity -bor $cpuBit; $fixtureLogicalProcessors += $cpuIndex }
}
if ($fixtureLogicalProcessors.Count -ne 2) { throw 'Small match requires two available logical CPUs in the invoking process affinity mask.' }
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot record tested HEAD.' }
$dirty = [bool](& git -C $repoRoot status --porcelain)
$result = [ordered]@{
    passed=$false; scenario=$Scenario; head=$head; dirtyWorkingTree=$dirty
    startedUtc=[DateTime]::UtcNow.ToString('o'); finishedUtc=$null; elapsedSeconds=0; scenarioElapsedSeconds=0
    scenarioTimeoutSeconds=$TimeoutSeconds; scenarioCoreLimit=2; runDirectory=$run; map='/Game/CommandProofGenerated/Maps/CommandProof'
    topology=$(if($Scenario -eq 'Coop'){'dedicated NullRHI fixture host, one rendered owner-1 client, one NullRHI owner-2 observer'}else{'one rendered standalone match client'})
    transport='UE native UDP loopback'; physicalInputVerified=$false; internetTest=$false; performanceTest=$false; cleanPresentation=[bool]$CleanPresentation
    storageGuard=[ordered]@{softPollingGuard=$true;intervalSeconds=10;maxCacheMiB=$MaxCacheMiB;maxTempMiB=$MaxTempMiB;checks=0;latestCacheBytes=0;latestTempBytes=0;peakCacheBytes=0;peakTempBytes=0}
    cpuPolicy=[ordered]@{availableAffinityMask=('0x{0:X}' -f $availableAffinity);fixtureAffinityMask=('0x{0:X}' -f $fixtureAffinity);logicalProcessors=$fixtureLogicalProcessors;priority='Normal';pollIntervalSeconds=1;polls=0;repairs=0;processes=@()}
    literalClientSetup=[ordered]@{startupFile=$literalClientCVars;expectedResolverDisabled=1;startupConfirmedByRole=@{};hostnameOrInternetTest=$false;osDnsChanged=$false}
    renderConfiguration=[ordered]@{realViewport=$true;offscreen=$true;width=1280;height=720;optimizedShaders=1;targetFPS=30}
    processes=@(); screenshots=@(); commonRevisions=@(); expectedWinner=$(if($Scenario -eq 'Defeat'){1}else{0}); error=$null; cleanupErrors=@()
}
$entries = [Collections.Generic.List[object]]::new()
$ownedWorkers = [Collections.Generic.List[object]]::new()
$prior = @{}
$lastStorage = -10.0
$result.cpuPolicy.lastPollElapsedSeconds = -1.0
function Get-OwnedLiveProcess($Entry) {
    $candidate = Get-Process -Id $Entry.Process.Id -ErrorAction SilentlyContinue
    if (-not $candidate) { return $null }
    try { if ($candidate.StartTime -eq $Entry.Started -and $candidate.Path -eq $Entry.Path) { return $candidate } }
    catch { if ($candidate.HasExited) { return $null }; throw }
    return $null
}
function Apply-OwnedCpuPolicy($Entry) {
    $live = Get-OwnedLiveProcess $Entry
    if (-not $live) { return }
    try {
        $beforeMask=[uint64]$live.ProcessorAffinity.ToInt64(); $beforePriority=$live.PriorityClass.ToString()
        if ($beforeMask -ne $fixtureAffinity) { $live.ProcessorAffinity=[IntPtr]([int64]$fixtureAffinity) }
        if ($live.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { $live.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal }
        $live.Refresh()
        if ([uint64]$live.ProcessorAffinity.ToInt64() -ne $fixtureAffinity -or $live.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { throw "Owned $($Entry.Role) CPU policy revalidation failed." }
        if ($beforeMask -ne $fixtureAffinity -or $beforePriority -ne 'Normal') { $result.cpuPolicy.repairs++ }
    } catch { if (Get-OwnedLiveProcess $Entry) { throw } }
}
function Get-OwnedWorkerLiveProcess($Entry) {
    $candidate=Get-Process -Id $Entry.Process.Id -ErrorAction SilentlyContinue
    if (-not $candidate) { return $null }
    try {
        if ($candidate.StartTime -eq $Entry.Started -and [string]::Equals($candidate.Path,$workerExe,[StringComparison]::OrdinalIgnoreCase)) { return $candidate }
    } catch { if ($candidate.HasExited) { return $null }; throw }
    return $null
}
function Apply-OwnedWorkerCpuPolicy($Entry) {
    $live=Get-OwnedWorkerLiveProcess $Entry
    if (-not $live) { return }
    try {
        $beforeMask=[uint64]$live.ProcessorAffinity.ToInt64(); $beforePriority=$live.PriorityClass.ToString()
        if ($beforeMask -ne $fixtureAffinity) { $live.ProcessorAffinity=[IntPtr]([int64]$fixtureAffinity) }
        if ($live.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { $live.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal }
        $live.Refresh()
        if ([uint64]$live.ProcessorAffinity.ToInt64() -ne $fixtureAffinity -or $live.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { throw 'Owned shader-worker CPU policy revalidation failed.' }
        if ($beforeMask -ne $fixtureAffinity -or $beforePriority -ne 'Normal') { $result.cpuPolicy.repairs++ }
    } catch { if (Get-OwnedWorkerLiveProcess $Entry) { throw } }
}
function Maintain-OwnedCpuPolicy([switch]$Force) {
    if (-not $Force -and $watch.Elapsed.TotalSeconds-$result.cpuPolicy.lastPollElapsedSeconds -lt 1) { return }
    $result.cpuPolicy.lastPollElapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3); $result.cpuPolicy.polls++
    $liveParents=@{}
    foreach ($entry in $entries) { $live=Get-OwnedLiveProcess $entry; if ($live) { $liveParents[$live.Id]=$entry; Apply-OwnedCpuPolicy $entry } }
    if ($liveParents.Count -gt 0) {
        try { $candidates=@(Get-CimInstance Win32_Process -Filter "Name='ShaderCompileWorker.exe'" -OperationTimeoutSec 2 -ErrorAction Stop) }
        catch { throw 'Cannot verify ShaderCompileWorker ownership through process metadata; aborting CPU policy monitor.' }
        foreach ($candidate in $candidates) {
            $parentId=[int]$candidate.ParentProcessId
            if (-not $liveParents.ContainsKey($parentId)) { continue }
            $parent=$liveParents[$parentId]
            if (-not (Get-OwnedLiveProcess $parent)) { continue }
            $child=Get-Process -Id ([int]$candidate.ProcessId) -ErrorAction SilentlyContinue
            if (-not $child) { continue }
            try {
                if (-not [string]::Equals($child.Path,$workerExe,[StringComparison]::OrdinalIgnoreCase)) { throw 'Owned-parent ShaderCompileWorker executable path did not match the configured Engine binary.' }
                if ($child.StartTime -lt $parent.Started) { throw 'Owned-parent ShaderCompileWorker predates its UE parent.' }
                if (-not $candidate.CreationDate -or [math]::Abs(($child.StartTime.ToUniversalTime()-([DateTime]$candidate.CreationDate).ToUniversalTime()).Ticks) -gt 10000) { throw 'Cannot verify owned ShaderCompileWorker creation time.' }
                $command=[string]$candidate.CommandLine
                if (-not $command -or $command.Replace('/','\').IndexOf($project,[StringComparison]::OrdinalIgnoreCase) -lt 0) { throw 'Cannot verify owned ShaderCompileWorker project command line.' }
                $known=@($ownedWorkers | Where-Object {$_.Process.Id -eq $child.Id -and $_.Started -eq $child.StartTime})
                if (-not $known.Count) {
                    $worker=[pscustomobject]@{Process=$child;Started=$child.StartTime;Role='shader-worker';Path=$workerExe;ParentId=$parentId;ParentStarted=$parent.Started;CommandLine=$command}
                    $ownedWorkers.Add($worker)
                    $result.cpuPolicy.processes+=@{role='shader-worker';pid=$child.Id;parentPid=$parentId;startUtc=$child.StartTime.ToUniversalTime().ToString('o');path=$workerExe;affinityMask=('0x{0:X}' -f [uint64]$child.ProcessorAffinity.ToInt64());priority=$child.PriorityClass.ToString();projectCommandVerified=$true}
                }
            } catch { if ($child.HasExited) { continue }; throw }
        }
    }
    foreach ($worker in $ownedWorkers) { Apply-OwnedWorkerCpuPolicy $worker }
}
function Check-StorageBudget([switch]$Force) {
    if (-not $Force -and $watch.Elapsed.TotalSeconds-$script:lastStorage -lt 10) { return }
    $script:lastStorage=$watch.Elapsed.TotalSeconds
    $cacheBytes=[int64](Get-ChildItem -LiteralPath $cache -File -Recurse -Force | Measure-Object -Property Length -Sum).Sum
    $tempBytes=[int64](Get-ChildItem -LiteralPath $temp -File -Recurse -Force | Measure-Object -Property Length -Sum).Sum
    $result.storageGuard.checks++; $result.storageGuard.latestCacheBytes=$cacheBytes; $result.storageGuard.latestTempBytes=$tempBytes
    $result.storageGuard.peakCacheBytes=[math]::Max($result.storageGuard.peakCacheBytes,$cacheBytes); $result.storageGuard.peakTempBytes=[math]::Max($result.storageGuard.peakTempBytes,$tempBytes)
    if ($MaxCacheMiB -gt 0 -and $cacheBytes -gt ([int64]$MaxCacheMiB*1MB)) { throw "D: cache soft guard exceeded: $cacheBytes bytes." }
    if ($MaxTempMiB -gt 0 -and $tempBytes -gt ([int64]$MaxTempMiB*1MB)) { throw "D: temp soft guard exceeded: $tempBytes bytes." }
}
function Read-Log($Entry) { if (Test-Path -LiteralPath $Entry.Log) { return Get-Content -LiteralPath $Entry.Log -Raw }; return '' }
function Start-Role([string]$Role,[string]$Destination,[string[]]$Mode) {
    $log=Join-Path $run ($Role+'.log')
    $launchArguments=@(('"'+$project+'"'),$Destination)+$Mode+@('-unattended','-nosound','-nosplash','-NoSourceControl','-NoP4','-CoreLimit=2','-ddc=BlockoutDDC','-logcmds="LogDerivedDataCache Verbose"',('-port='+$Port),'-ini:GameUserSettings:[/Script/Engine.GameUserSettings]:FrameRateLimit=30','-ini:Engine:[DevOptions.Shaders]:WorkerProcessPriority=0,[ConsoleVariables]:t.MaxFPS=30,[AssetRegistry]:EditorGameScansAR=Async,[ShaderCompiler]:r.Shaders.Optimize=1,[ConsoleVariables]:r.Shaders.Optimize=1',('-abslog="'+$log+'"'))
    $needsLiteralLoopback = $Scenario -eq 'Coop' -and $Role -in @('rendered','observer')
    if ($needsLiteralLoopback) {
        if ($Destination -notmatch '^127\.0\.0\.1:\d+$') { throw 'Literal-client CVar override is restricted to loopback destinations.' }
        $launchArguments+=('-cvarsini="'+$literalClientCVars+'"')
    }
    if ($CleanPresentation -and $Role -eq 'rendered') { $launchArguments+='-ExecCmds="DisableAllScreenMessages"' }
    $p=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $entry=[pscustomobject]@{Process=$p;Started=$p.StartTime;Role=$Role;Log=$log;Path=$editor}
    $entries.Add($entry)
    $p.ProcessorAffinity=[IntPtr]([int64]$fixtureAffinity); $p.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal; $p.Refresh()
    if ([uint64]$p.ProcessorAffinity.ToInt64() -ne $fixtureAffinity -or $p.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { throw "Could not apply/verify owned $Role CPU policy." }
    $result.processes+=@{role=$Role;pid=$p.Id;startUtc=$entry.Started.ToUniversalTime().ToString('o');path=$editor;affinityMask=('0x{0:X}' -f [uint64]$p.ProcessorAffinity.ToInt64());priority=$p.PriorityClass.ToString()}
    $result.cpuPolicy.processes+=@{role=$Role;pid=$p.Id;startUtc=$entry.Started.ToUniversalTime().ToString('o');path=$editor;affinityMask=('0x{0:X}' -f [uint64]$p.ProcessorAffinity.ToInt64());priority=$p.PriorityClass.ToString()}
    return $entry
}
function Wait-Marker($Entry,[string]$Pattern) {
    while ($true) {
        Check-StorageBudget; Maintain-OwnedCpuPolicy
        if ((Read-Log $Entry) -match $Pattern) { return }
        if ($Entry.Process.HasExited) { throw "$($Entry.Role) exited before expected marker: $Pattern" }
        if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "Scenario timed out waiting for $Pattern" }
        Start-Sleep -Milliseconds 200
    }
}
function Stop-OwnedProcesses {
    foreach ($entry in $entries) {
        try { $live=Get-OwnedLiveProcess $entry; if ($live) { Stop-Process -Id $live.Id -Force -ErrorAction Stop } }
        catch { $result.cleanupErrors+=@{role=$entry.Role;pid=$entry.Process.Id;message=$_.Exception.Message}; $result.passed=$false }
    }
    foreach ($worker in $ownedWorkers) {
        try {
            $parentEntry=@($entries | Where-Object {$_.Process.Id -eq $worker.ParentId -and $_.Started -eq $worker.ParentStarted})
            if (-not $parentEntry.Count -and (Get-OwnedWorkerLiveProcess $worker)) { throw 'Owned shader-worker parent identity is unavailable during cleanup.' }
            $live=Get-OwnedWorkerLiveProcess $worker
            if ($live) { Stop-Process -Id $live.Id -Force -ErrorAction Stop }
        } catch { $result.cleanupErrors+=@{role='shader-worker';pid=$worker.Process.Id;message=$_.Exception.Message}; $result.passed=$false }
    }
}
try {
    Check-StorageBudget -Force
    foreach ($name in @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')) { $prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process') }
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$cache,'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',$temp,'Process'); [Environment]::SetEnvironmentVariable('TMP',$temp,'Process')
    $scenarioWatch=[Diagnostics.Stopwatch]::StartNew()
    if ($Scenario -eq 'Coop') {
        $serverEntry=Start-Role 'host' '/Game/CommandProofGenerated/Maps/CommandProof' @('-server','-nullrhi','-MULTIHOME=127.0.0.1','-ProofSkirmish','-ProofSkirmishFixtureHost')
        Wait-Marker $serverEntry 'PROOF_SKIRMISH_READY fixture=1'
        $client=Start-Role 'rendered' ('127.0.0.1:'+$Port) @('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ProofSkirmish','-ProofInteractive','-ProofSkirmishScenario=Coop',('-ProofSkirmishOutput="'+$images+'"'))
        Wait-Marker $client 'PROOF_SKIRMISH_READY owner=1 width=1280 height=720 maxFPS=30\.000'
        $observer=Start-Role 'observer' ('127.0.0.1:'+$Port) @('-game','-nullrhi','-ProofSkirmish','-ProofSkirmishObserver')
        $expected=@($serverEntry,$client,$observer)
        Wait-Marker $serverEntry 'PROOF_SKIRMISH_COMPLETE role=host status=pass scenario=Coop winnerTeam=0'
        Wait-Marker $observer 'PROOF_SKIRMISH_COMPLETE role=observer owner=2 status=pass scenario=Coop winnerTeam=0'
        Wait-Marker $client 'PROOF_SKIRMISH_COMPLETE role=client owner=1 status=pass scenario=Coop winnerTeam=0 built=1 gathered=1 produced=1 botBuilt=1 botProduced=1 botAttacked=1'
    } else {
        $client=Start-Role 'rendered' '/Game/CommandProofGenerated/Maps/CommandProof' @('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ProofSkirmish','-ProofInteractive',('-ProofSkirmishScenario='+$Scenario),('-ProofSkirmishOutput="'+$images+'"'))
        $expected=@($client)
        Wait-Marker $client 'PROOF_SKIRMISH_READY owner=1 width=1280 height=720 maxFPS=30\.000'
        $winner=if($Scenario -eq 'Victory'){0}else{1}
        Wait-Marker $client ('PROOF_SKIRMISH_COMPLETE role=client owner=1 status=pass scenario='+$Scenario+' winnerTeam='+$winner+' built=1 gathered=1 produced=1 botBuilt=1 botProduced=1 botAttacked=1')
    }
    Wait-Marker $client ('PROOF_SKIRMISH_CAPTURE name=02-result.png winnerTeam='+$result.expectedWinner+' phase=Finished written=1')
    while (@($expected | Where-Object { -not $_.Process.HasExited }).Count -gt 0) {
        Check-StorageBudget; Maintain-OwnedCpuPolicy
        if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Small-match scenario exceeded its strict timeout.' }
        Start-Sleep -Milliseconds 200
    }
    Check-StorageBudget -Force
    foreach ($entry in $expected) {
        if ($entry.Process.ExitCode -ne 0) { throw "$($entry.Role) exited with code $($entry.Process.ExitCode)." }
        $log=Read-Log $entry
        if ($log -match 'PROOF_SKIRMISH_(CHECK|COMPLETE)[^\r\n]*status=fail') { throw "$($entry.Role) reported a skirmish failure." }
        $result.literalClientSetup.startupConfirmedByRole[$entry.Role]=$log -match 'Set CVar \[\[net\.IpConnectionDisableResolution:1\]\]'
        if ($Scenario -eq 'Coop' -and $entry.Role -in @('rendered','observer') -and -not $result.literalClientSetup.startupConfirmedByRole[$entry.Role]) { throw "$($entry.Role) missing literal resolver startup CVar evidence." }
        $map=@{}
        foreach ($m in [regex]::Matches($log,'PROOF_SNAPSHOT role=(?:host|client|observer) owner=\d+ revision=(\d+) digest=([A-Za-z0-9]+)')) { $map[$m.Groups[1].Value]=$m.Groups[2].Value }
        $entry | Add-Member -NotePropertyName SnapshotMap -NotePropertyValue $map
    }
    if ($Scenario -eq 'Coop') {
        $common=@($expected[0].SnapshotMap.Keys | Where-Object { $expected[1].SnapshotMap.ContainsKey($_) -and $expected[2].SnapshotMap.ContainsKey($_) } | Sort-Object {[int]$_})
        if ($common.Count -lt 3) { throw 'Need at least three shared host/client/observer snapshot revisions.' }
        foreach ($revision in $common) { if ($expected[0].SnapshotMap[$revision] -ne $expected[1].SnapshotMap[$revision] -or $expected[0].SnapshotMap[$revision] -ne $expected[2].SnapshotMap[$revision]) { throw "Snapshot digest mismatch at revision $revision." } }
        $result.commonRevisions=$common
    }
    if ($Scenario -ne 'Coop') {
        $complete=Read-Log $expected[0]
        if ($complete -notmatch ('PROOF_SKIRMISH_COMPLETE role=client owner=1 status=pass scenario='+$Scenario+' winnerTeam='+$result.expectedWinner+' built=1 gathered=1 produced=1 botBuilt=1 botProduced=1 botAttacked=1')) { throw 'Standalone completion evidence or expected winner is missing.' }
    }
    foreach ($name in @('01-base.png','02-result.png')) {
        $path=Join-Path $images $name
        if (-not (Test-Path -LiteralPath $path)) { throw "Missing fresh engine screenshot $name." }
        $file=Get-Item -LiteralPath $path
        if ($file.Length -le 1024 -or $file.LastWriteTimeUtc -lt [DateTime]::Parse($result.startedUtc).ToUniversalTime()) { throw "Stale or empty screenshot $name." }
        $bytes=[IO.File]::ReadAllBytes($path)
        if ([BitConverter]::ToString($bytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A') { throw "Invalid PNG $name." }
        $width=($bytes[16]*16777216)+($bytes[17]*65536)+($bytes[18]*256)+$bytes[19]; $height=($bytes[20]*16777216)+($bytes[21]*65536)+($bytes[22]*256)+$bytes[23]
        if ($width -ne 1280 -or $height -ne 720) { throw "Unexpected screenshot size ${width}x${height}." }
        $result.screenshots+=@{name=$name;path=$path;bytes=$file.Length;width=$width;height=$height;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
    $result.passed=$true
} catch { $result.error=$_.Exception.Message }
finally {
    Stop-OwnedProcesses
    foreach ($name in $prior.Keys) { try { [Environment]::SetEnvironmentVariable($name,$prior[$name],'Process') } catch { $result.cleanupErrors+=@{role='environment';name=$name;message=$_.Exception.Message}; $result.passed=$false } }
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o'); $result.elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    if ($scenarioWatch) { $result.scenarioElapsedSeconds=[math]::Round($scenarioWatch.Elapsed.TotalSeconds,3) }
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
}
if (-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')
