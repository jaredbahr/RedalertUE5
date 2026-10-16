param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(0,60000)][int]$Port = 0,
    [ValidateRange(60,240)][int]$TimeoutSeconds = 240,
    [ValidateRange(0,65536)][int]$MaxCacheMiB = 2048,
    [ValidateRange(0,65536)][int]$MaxTempMiB = 1024
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
if ($Port -eq 0) {
    $portFound=$false
    for ($attempt=0; $attempt -lt 100 -and -not $portFound; $attempt++) {
        $candidatePort=Get-Random -Minimum 20000 -Maximum 60001
        $probe=[Net.Sockets.UdpClient]::new()
        try { $probe.Client.ExclusiveAddressUse=$true; $probe.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Loopback,$candidatePort)); $Port=$candidatePort; $portFound=$true }
        catch [Net.Sockets.SocketException] { }
        finally { $probe.Dispose() }
    }
    if (-not $portFound) { throw 'Cannot reserve a unique UDP loopback port for the lifecycle fixture.' }
} else {
    $probe = [Net.Sockets.UdpClient]::new()
    try { $probe.Client.ExclusiveAddressUse = $true; $probe.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Loopback, $Port)) }
    finally { $probe.Dispose() }
}

$run = Join-Path $repoRoot ('generated\session-lifecycle-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$control = Join-Path $run 'control'
$cache = Join-Path $repoRoot 'generated\session-lifecycle-cache'
$temp = Join-Path $run 'temp'
New-Item -ItemType Directory -Force -Path $run,$control,$cache,$temp | Out-Null
$cvars = Join-Path $run 'literal-client-cvars.ini'
"[Startup]`nnet.IpConnectionDisableResolution=1`n" | Set-Content -LiteralPath $cvars -Encoding ascii
$watch = [Diagnostics.Stopwatch]::StartNew()
$scenarioWatch = $null
$availableAffinity = [uint64]([Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64())
$fixtureAffinity = [uint64]0; $fixtureLogicalProcessors = @()
for ($cpuIndex = 0; $cpuIndex -lt 64 -and $fixtureLogicalProcessors.Count -lt 2; $cpuIndex++) {
    $cpuBit = [uint64]1 -shl $cpuIndex
    if (($availableAffinity -band $cpuBit) -ne 0) { $fixtureAffinity = $fixtureAffinity -bor $cpuBit; $fixtureLogicalProcessors += $cpuIndex }
}
if ($fixtureLogicalProcessors.Count -ne 2) { throw 'Session lifecycle requires two available logical CPUs in the invoking process affinity mask.' }
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot record tested HEAD.' }
$dirty = [bool](& git -C $repoRoot status --porcelain)
$result = [ordered]@{
    passed=$false; head=$head; dirtyWorkingTree=$dirty; startedUtc=[DateTime]::UtcNow.ToString('o'); finishedUtc=$null
    elapsedSeconds=0; scenarioElapsedSeconds=0; scenarioTimeoutSeconds=$TimeoutSeconds; scenarioCoreLimit=2
    runDirectory=$run; map='/Game/CommandProofGenerated/Maps/CommandProof'; port=$Port; topology='dedicated NullRHI host and four sequential loopback NullRHI client processes'
    transport='UE native UDP loopback'; physicalInputVerified=$false; internetTest=$false; performanceTest=$false
    lifecycle=[ordered]@{initialOwner1Healthy=$false;owner2Disconnected=$false;owner2RejoinedSlot2=$false;rejoinDeferredUntilRematch=$true;rejoinNormalEpochs=@();thirdRejectedWhenFull=$false;terminalEpochs=@();rematchEpochs=@();staleEpochRejected=$false;freshReceiptSequences=@();commonRevisionDigests=@();terminalSnapshotAgreements=@();finishSignaledUtc=$null}
    storageGuard=[ordered]@{softPollingGuard=$true;intervalSeconds=10;maxCacheMiB=$MaxCacheMiB;maxTempMiB=$MaxTempMiB;checks=0;latestCacheBytes=0;latestTempBytes=0;peakCacheBytes=0;peakTempBytes=0}
    cpuPolicy=[ordered]@{availableAffinityMask=('0x{0:X}' -f $availableAffinity);fixtureAffinityMask=('0x{0:X}' -f $fixtureAffinity);logicalProcessors=$fixtureLogicalProcessors;priority='Normal';pollIntervalSeconds=1;polls=0;repairs=0;processes=@()}
    literalClientSetup=[ordered]@{startupFile=$cvars;expectedResolverDisabled=1;startupConfirmedByRole=@{};hostnameOrInternetTest=$false;osDnsChanged=$false}
    processes=@(); commonRevisions=@(); error=$null; cleanupErrors=@()
}
$entries = [Collections.Generic.List[object]]::new()
$ownedWorkers = [Collections.Generic.List[object]]::new()
$prior = @{}
$lastStorage = -10.0
$result.cpuPolicy.lastPollElapsedSeconds = -1.0
function Get-OwnedLiveProcess($Entry) {
    $candidate = Get-Process -Id $Entry.Process.Id -ErrorAction SilentlyContinue
    if (-not $candidate) { return $null }
    try {
        if ($candidate.StartTime -ne $Entry.Started) { return $null }
        for ($attempt = 0; $attempt -lt 20; $attempt++) {
            $candidate.Refresh()
            if ($candidate.HasExited) { return $null }
            if ($candidate.Path -eq $Entry.Path) { return $candidate }
            if ($candidate.Path) { return $null }
            Start-Sleep -Milliseconds 100
        }
        throw 'Cannot verify executable path of a live owned startup process.'
    } catch { if ($candidate.HasExited) { return $null }; throw }
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
    try { if ($candidate.StartTime -eq $Entry.Started -and [string]::Equals($candidate.Path,$workerExe,[StringComparison]::OrdinalIgnoreCase)) { return $candidate } }
    catch { if ($candidate.HasExited) { return $null }; throw }
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
    $launchArguments=@(('"'+$project+'"'),$Destination)+$Mode+@('-unattended','-nosound','-nosplash','-NoSourceControl','-NoP4','-CoreLimit=2','-ddc=BlockoutDDC','-logcmds="LogDerivedDataCache Verbose"',('-port='+$Port),'-ini:Engine:[DevOptions.Shaders]:WorkerProcessPriority=0,[ConsoleVariables]:t.MaxFPS=30,[AssetRegistry]:EditorGameScansAR=Async,[ShaderCompiler]:r.Shaders.Optimize=1,[ConsoleVariables]:r.Shaders.Optimize=1',('-abslog="'+$log+'"'))
    if ($Role -ne 'host') { $launchArguments+=('-cvarsini="'+$cvars+'"') }
    $p=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $entry=[pscustomobject]@{Process=$p;Started=$p.StartTime;Role=$Role;Log=$log;Path=$editor;IntentionalStop=$false}
    $entries.Add($entry)
    $p.ProcessorAffinity=[IntPtr]([int64]$fixtureAffinity); $p.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal; $p.Refresh()
    if ([uint64]$p.ProcessorAffinity.ToInt64() -ne $fixtureAffinity -or $p.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { throw "Could not apply/verify owned $Role CPU policy." }
    $result.processes+=@{role=$Role;pid=$p.Id;startUtc=$entry.Started.ToUniversalTime().ToString('o');path=$editor;affinityMask=('0x{0:X}' -f [uint64]$p.ProcessorAffinity.ToInt64());priority=$p.PriorityClass.ToString();log=$log}
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
function Wait-Exited($Entries) {
    while (@($Entries | Where-Object { -not $_.Process.HasExited }).Count -gt 0) {
        Check-StorageBudget; Maintain-OwnedCpuPolicy
        if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Lifecycle scenario exceeded its strict timeout.' }
        Start-Sleep -Milliseconds 200
    }
    foreach ($entry in $Entries) {
        if ($entry.Process.ExitCode -ne 0) { throw "$($entry.Role) exited with code $($entry.Process.ExitCode)." }
        $record=@($result.processes | Where-Object {$_.role -eq $entry.Role})[0]
        $record.exitCode=$entry.Process.ExitCode; $record.expectedExitCode=0
    }
}
function Signal-Gate([string]$Name) { [DateTime]::UtcNow.ToString('o') | Set-Content -LiteralPath (Join-Path $control $Name) -Encoding ascii }
function Assert-OwnerOneActiveBeforeFinish($Entry,[DateTime]$FinishSignaledUtc) {
    $lines=[regex]::Matches((Read-Log $Entry),'(?m)^([^\r\n]*PROOF_LIFECYCLE_LOGOUT owner=1(?:\s|$)[^\r\n]*)')
    foreach ($match in $lines) {
        $stamp=[regex]::Match($match.Groups[1].Value,'^\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]')
        if (-not $stamp.Success) { throw 'Cannot classify owner 1 logout without the engine log timestamp.' }
        $loggedUtc=[DateTime]::ParseExact($stamp.Groups[1].Value,'yyyy.MM.dd-HH.mm.ss:fff',[Globalization.CultureInfo]::InvariantCulture,([Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal))
        if ($loggedUtc -lt $FinishSignaledUtc) { throw 'Owner 1 disconnected before the lifecycle finish gate.' }
    }
}
function Get-EpochSnapshots([string]$Text) {
    $snapshots=@{}
    foreach ($m in [regex]::Matches($Text,'PROOF_SNAPSHOT[^\r\n]*?epoch=(\d+)[^\r\n]*?revision=(\d+)[^\r\n]*?digest=([A-Za-z0-9]+)')) { $snapshots[($m.Groups[1].Value+':'+$m.Groups[2].Value)]=$m.Groups[3].Value }
    return $snapshots
}
function Assert-CommonEpochSnapshots([int]$Epoch,$ActiveEntries) {
    if ($ActiveEntries.Count -lt 3) { throw "Need host plus two clients for epoch $Epoch snapshot agreement." }
    while ($true) {
        Check-StorageBudget; Maintain-OwnedCpuPolicy
        $maps=@(); foreach ($entry in $ActiveEntries) { $maps+=,(Get-EpochSnapshots (Read-Log $entry)) }
        # Use the same epoch:revision key across host and clients; dictionary
        # keys deliberately include epoch because model revisions restart after rematch.
        $common=@($maps[0].Keys | Where-Object {
            $_ -like ([string]$Epoch+':*') -and $maps[1].ContainsKey($_) -and $maps[2].ContainsKey($_)
        } | Sort-Object { [int](($_ -split ':')[1]) })
        if ($common.Count -gt 0) {
            foreach ($key in $common) {
                if ($maps[0][$key] -ne $maps[1][$key] -or $maps[0][$key] -ne $maps[2][$key]) { throw "Snapshot digest mismatch at epoch/revision $key." }
                $result.lifecycle.commonRevisionDigests+=@{epoch=$Epoch;revision=[int](($key -split ':')[1]);digest=$maps[0][$key]}
            }
            $result.commonRevisions+=@{epoch=$Epoch;revisions=@($common | ForEach-Object {[int](($_ -split ':')[1])})}
            return
        }
        if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "No common host/client snapshot digest for epoch $Epoch." }
        Start-Sleep -Milliseconds 200
    }
}
function Wait-TerminalSnapshotAgreement([int]$Epoch,[int]$Winner,$ActiveEntries) {
    while ($true) {
        Check-StorageBudget; Maintain-OwnedCpuPolicy
        $maps=@(); foreach ($entry in $ActiveEntries) { $maps+=,(Get-EpochSnapshots (Read-Log $entry)) }
        $hostKeys=@($maps[0].Keys | Where-Object { $_ -like ([string]$Epoch+':*') } | Sort-Object { [int](($_ -split ':')[1]) })
        if ($hostKeys.Count -gt 0) {
            $key=$hostKeys[-1]; $shared=$true
            for ($index=1; $index -lt $maps.Count; $index++) {
                if (-not $maps[$index].ContainsKey($key) -or $maps[$index][$key] -ne $maps[0][$key]) { $shared=$false; break }
            }
            if ($shared) {
                $revision=[int](($key -split ':')[1])
                $result.lifecycle.terminalSnapshotAgreements+=@{epoch=$Epoch;winnerTeam=$Winner;revision=$revision;digest=$maps[0][$key]}
                return
            }
        }
        if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "Clients did not receive the host terminal snapshot for epoch $Epoch." }
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
            $live=Get-OwnedWorkerLiveProcess $worker; if ($live) { Stop-Process -Id $live.Id -Force -ErrorAction Stop }
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
    $serverEntry=Start-Role 'host' '/Game/CommandProofGenerated/Maps/CommandProof' @('-server','-nullrhi','-MULTIHOME=127.0.0.1','-ProofSkirmish','-ProofLifecycleFixtureHost',('-ProofLifecycleControl="'+$control+'"'))
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_READY epoch=1'
    $lead=Start-Role 'lead' ('127.0.0.1:'+$Port) @('-game','-nullrhi','-ProofSkirmish','-ProofLifecycleRole=lead',('-ProofLifecycleControl="'+$control+'"'))
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_JOIN owner=1 epoch=1 seeded=1'
    $owner2=Start-Role 'owner2' ('127.0.0.1:'+$Port) @('-game','-nullrhi','-ProofSkirmish','-ProofLifecycleRole=owner2',('-ProofLifecycleControl="'+$control+'"'))
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_JOIN owner=2 epoch=1 seeded=1'
    Wait-Marker $lead 'PROOF_SNAPSHOT role=client owner=1 epoch=1 revision=\d+ digest=[A-Za-z0-9]+'
    Wait-Marker $owner2 'PROOF_SNAPSHOT role=client owner=2 epoch=1 revision=\d+ digest=[A-Za-z0-9]+'
    Assert-CommonEpochSnapshots 1 @($serverEntry,$lead,$owner2)
    $result.lifecycle.initialOwner1Healthy=$true
    $owner2Live=Get-OwnedLiveProcess $owner2
    if (-not $owner2Live) { throw 'Owner 2 exited before the planned disconnect.' }
    Stop-Process -Id $owner2Live.Id -Force -ErrorAction Stop
    $owner2Live.WaitForExit(); $owner2Live.Refresh(); $owner2.IntentionalStop=$true
    $owner2ProcessRecord=@($result.processes | Where-Object {$_.role -eq 'owner2'})[0]
    $owner2ProcessRecord.intentionalDisconnect=$true; $owner2ProcessRecord.exitCode=$owner2Live.ExitCode
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_LOGOUT owner=2 epoch=1'
    $result.lifecycle.owner2Disconnected=$true
    $rejoin=Start-Role 'rejoin' ('127.0.0.1:'+$Port) @('-game','-nullrhi','-ProofSkirmish','-ProofLifecycleRole=rejoin',('-ProofLifecycleControl="'+$control+'"'))
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_JOIN owner=2 epoch=1 seeded=0'
    $result.lifecycle.owner2RejoinedSlot2=$true
    $third=Start-Role 'third' ('127.0.0.1:'+$Port) @('-game','-nullrhi','-ProofSkirmish','-ProofLifecycleRole=third',('-ProofLifecycleControl="'+$control+'"'))
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_REJECT reason=full'
    Wait-Marker $third 'PROOF_LIFECYCLE_CLIENT_REJECT reason=full'
    $result.lifecycle.thirdRejectedWhenFull=$true

    Signal-Gate 'play-epoch-1'
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_TERMINAL epoch=1 winnerTeam=-?\d+'
    $terminal1=[regex]::Match((Read-Log $serverEntry),'PROOF_LIFECYCLE_TERMINAL epoch=1 winnerTeam=(-?\d+)')
    $winner1=[int]$terminal1.Groups[1].Value
    if (@(-1,0,1) -notcontains $winner1) { throw "Unexpected terminal winnerTeam $winner1 for epoch 1." }
    $result.lifecycle.terminalEpochs+=@{epoch=1;winnerTeam=$winner1}
    Assert-CommonEpochSnapshots 1 @($serverEntry,$lead,$rejoin)
    Wait-TerminalSnapshotAgreement 1 $winner1 @($serverEntry,$lead,$rejoin)
    Signal-Gate 'rematch-epoch-1'
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_REMATCH epoch=2'
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_RESEED owner=2 epoch=2 credits=350'
    $result.lifecycle.rejoinNormalEpochs+=2
    $result.lifecycle.rematchEpochs+=2
    Signal-Gate 'stale-epoch-1'
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_STALE_REJECT epoch=1 current=2 owner=1 sequence=1'
    Wait-Marker $lead 'PROOF_STALE_RECEIPT_IGNORED owner=1 epoch=1 current=2 sequence=1'
    $result.lifecycle.staleEpochRejected=$true
    Signal-Gate 'play-epoch-2'
    Wait-Marker $lead 'PROOF_LIFECYCLE_CLIENT_RESULT owner=1 epoch=2 sequence=1 accepted=1 reason=accepted'
    Wait-Marker $rejoin 'PROOF_LIFECYCLE_CLIENT_RESULT owner=2 epoch=2 sequence=1 accepted=1 reason=accepted'
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_TERMINAL epoch=2 winnerTeam=-?\d+'
    $terminal2=[regex]::Match((Read-Log $serverEntry),'PROOF_LIFECYCLE_TERMINAL epoch=2 winnerTeam=(-?\d+)')
    $winner2=[int]$terminal2.Groups[1].Value
    if (@(-1,0,1) -notcontains $winner2) { throw "Unexpected terminal winnerTeam $winner2 for epoch 2." }
    $result.lifecycle.terminalEpochs+=@{epoch=2;winnerTeam=$winner2}
    Assert-CommonEpochSnapshots 2 @($serverEntry,$lead,$rejoin)
    Wait-TerminalSnapshotAgreement 2 $winner2 @($serverEntry,$lead,$rejoin)
    Signal-Gate 'rematch-epoch-2'
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_REMATCH epoch=3'
    $result.lifecycle.rematchEpochs+=3
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_RESEED owner=2 epoch=3 credits=350'
    $result.lifecycle.rejoinNormalEpochs+=3
    Signal-Gate 'play-epoch-3'
    Wait-Marker $lead 'PROOF_LIFECYCLE_CLIENT_RESULT owner=1 epoch=3 sequence=1 accepted=1 reason=accepted'
    Wait-Marker $rejoin 'PROOF_LIFECYCLE_CLIENT_RESULT owner=2 epoch=3 sequence=1 accepted=1 reason=accepted'
    Assert-CommonEpochSnapshots 3 @($serverEntry,$lead,$rejoin)
    $result.lifecycle.freshReceiptSequences+=@(
        @{owner=1;epoch=2;sequence=1;accepted=$true},
        @{owner=2;epoch=2;sequence=1;accepted=$true},
        @{owner=1;epoch=3;sequence=1;accepted=$true},
        @{owner=2;epoch=3;sequence=1;accepted=$true}
    )
    $preFinishBoundaryUtc=[DateTime]::MaxValue
    Assert-OwnerOneActiveBeforeFinish $serverEntry $preFinishBoundaryUtc
    $finishSignaledUtc=[DateTime]::UtcNow
    $finishSignaledUtc=$finishSignaledUtc.AddTicks(-($finishSignaledUtc.Ticks % [TimeSpan]::TicksPerMillisecond))
    Signal-Gate 'finish'
    $result.lifecycle.finishSignaledUtc=$finishSignaledUtc.ToString('o')
    Wait-Marker $serverEntry 'PROOF_LIFECYCLE_COMPLETE role=host status=pass'
    Wait-Marker $lead 'PROOF_LIFECYCLE_CLIENT_COMPLETE role=lead status=pass'
    Wait-Marker $rejoin 'PROOF_LIFECYCLE_CLIENT_COMPLETE role=rejoin status=pass'
    Wait-Marker $third 'PROOF_LIFECYCLE_CLIENT_COMPLETE role=third status=pass'
    Wait-Exited @($serverEntry,$lead,$rejoin,$third)
    Check-StorageBudget -Force; Maintain-OwnedCpuPolicy -Force
    foreach ($entry in @($serverEntry,$lead,$owner2,$rejoin,$third)) {
        $log=Read-Log $entry
        if ($log -match 'PROOF_LIFECYCLE_[A-Z_]+[^\r\n]*status=fail|Fatal error|Assertion failed') { throw "$($entry.Role) has failure/crash evidence." }
        if ($entry.Role -ne 'host' -and $entry.Role -ne 'owner2') {
            $result.literalClientSetup.startupConfirmedByRole[$entry.Role]=$log -match 'Set CVar \[\[net\.IpConnectionDisableResolution:1\]\]'
            if (-not $result.literalClientSetup.startupConfirmedByRole[$entry.Role]) { throw "$($entry.Role) missing literal loopback resolver startup CVar evidence." }
        }
    }
    Assert-OwnerOneActiveBeforeFinish $serverEntry $finishSignaledUtc
    $result.passed=$true
} catch { $result.error=$_.Exception.Message }
finally {
    Stop-OwnedProcesses
    foreach ($name in $prior.Keys) { try { [Environment]::SetEnvironmentVariable($name,$prior[$name],'Process') } catch { $result.cleanupErrors+=@{role='environment';name=$name;message=$_.Exception.Message}; $result.passed=$false } }
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o'); $result.elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    if ($scenarioWatch) { $result.scenarioElapsedSeconds=[math]::Round($scenarioWatch.Elapsed.TotalSeconds,3) }
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
}
if (-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')
