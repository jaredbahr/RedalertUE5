param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(20000,60000)][int]$Port = 28939,
    [ValidateRange(45,90)][int]$TimeoutSeconds = 90,
    [ValidateRange(15,300)][int]$WarmupTimeoutSeconds = 300,
    [ValidateRange(0,65536)][int]$MaxCacheMiB = 2048,
    [ValidateRange(0,65536)][int]$MaxTempMiB = 1024,
    [switch]$UnoptimizedShaders,
    [switch]$CleanPresentation
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'RefineryMinerEvidence.ps1')
. (Join-Path $PSScriptRoot 'AttackMoveEvidence.ps1')
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
$run = Join-Path $repoRoot ('generated\attack-move-proof-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
if(Test-Path -LiteralPath $run){throw 'Fresh Attack Move run directory already exists.'}
$images = Join-Path $run 'screenshots'
$release = Join-Path $run 'release'
$cache = Join-Path $repoRoot 'generated\attack-move-proof-cache'
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
if($fixtureLogicalProcessors.Count -ne 2) { throw 'Attack Move fixture requires two available logical CPUs in the invoking process affinity mask.' }
$head = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot record tested HEAD.' }
$dirty = [bool](& git -C $repoRoot status --porcelain)
$result = [ordered]@{passed=$false;head=$head;dirtyWorkingTree=$dirty;startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;elapsedSeconds=0;scenarioElapsedSeconds=0;warmup=[ordered]@{passed=$false;startedUtc=$null;finishedUtc=$null;elapsedSeconds=0;timeoutSeconds=$WarmupTimeoutSeconds;coreLimit=4;reportedShaderWorkers=$null;unusedShaderThreads=1;unusedShaderThreadsDuringGame=1;screenshotAcceptance=$false};scenarioTimeoutSeconds=$TimeoutSeconds;scenarioCoreLimit=2;totalTimeoutBudgetSeconds=($WarmupTimeoutSeconds+$TimeoutSeconds);cachePath=$cache;stages=[ordered]@{};runDirectory=$run;topology='one dedicated UE server, one rendered Attack Move fixture client, one NullRHI owner-2 observer receiving snapshots and performing ordinary Refinery/Gather/Stop economy setup (after standalone graphical warmup)';transport='UE native UDP loopback';cleanPresentation=[bool]$CleanPresentation;physicalMouseAndFocusVerified=$false;internetTest=$false;performanceTest=$false;port=$Port;screenshots=@();error=$null}
$prior=@{}; $processes=[Collections.Generic.List[object]]::new()
$ownedWorkers=[Collections.Generic.List[object]]::new()
$storageGuard=[ordered]@{softPollingGuard=$true;intervalSeconds=10;maxCacheMiB=$MaxCacheMiB;maxTempMiB=$MaxTempMiB;lastCheckElapsedSeconds=-10;checks=0;latestCacheBytes=0;latestTempBytes=0;peakObservedCacheBytes=0;peakObservedTempBytes=0}
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
$result.mode='attackMoveFixture'
$result.refineryEvidenceContext='Refinery'
$result.inputSource='syntheticSharedHandlers'
$result.physicalInputVerified=$false
$result.nativeInputVerified=$false
$result.osWindowFocusVerified=$false
$result.engineActiveTimeoutSecondsPerRole=55
$result.requiredProof='ordinary mining for owners 1/2/100 and paid production; shared Attack Move/Stop; three-role semantic snapshot equality; actual-host-clock movement/fire/death/resume/Stop; no forged victory'
$result.globalMovementProofVerified=$false
$result.readyMarkersByRole=@{}
$result.completeMarkersByRole=@{}
$result.attackMoveEvidence=$null
$result.releaseBarrier=[ordered]@{path=$release;preReleaseValidationPassed=$false;created=$false;createdUtc=$null;preReleaseEvidence=$null}
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
    $confirmationStarted=$false
    try {
        $live=Get-OwnedLiveProcess $Entry
        if($live) {
            Stop-Process -Id $live.Id -Force -ErrorAction Stop
            $confirmationStarted=$true
            if(-not $live.WaitForExit(5000)) { throw "Owned $($Entry.Role) did not exit within the 5000ms cleanup bound." }
            $live.Refresh()
            if(-not $live.HasExited) { throw "Owned $($Entry.Role) exit was not confirmed after cleanup wait." }
            $remaining=Get-OwnedLiveProcess $Entry
            if($remaining) {
                $remaining.Refresh()
                if(-not $remaining.HasExited) { throw "Owned $($Entry.Role) is still live after cleanup confirmation." }
            }
        }
    } catch {
        $failure=$_.Exception.Message
        if(-not $confirmationStarted) {
            try {
                # A normal exit/PID reuse during the initial stop needs no action.
                $remaining=Get-OwnedLiveProcess $Entry
                if(-not $remaining) { $result.cleanup.ignoredExitRaces++; return }
            } catch { $failure+='; recheck: '+$_.Exception.Message }
        }
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
    function Start-AttackMove([string]$Role,[string]$Destination,[string[]]$Mode,[int]$CoreLimit=2) {
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
    Start-AttackMove 'warmup' '/Game/CommandProofGenerated/Maps/CommandProof' @('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ProofGraphicsWarmup') 4
    try {
        while(-not $processes[0].Process.HasExited) {
            Check-StorageBudget
            Maintain-OwnedCpuPolicy
            if($warmupWatch.Elapsed.TotalSeconds -ge $WarmupTimeoutSeconds) { throw 'Graphics warmup timed out before the Attack Move scenario started.' }
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
    function Read-AttackMoveLogs {
        $logs=@{}
        foreach($entry in $processes){
            if($entry.Role -eq 'warmup'){continue}
            $logs[$entry.Role]=if(Test-Path -LiteralPath $entry.Log){Get-Content -LiteralPath $entry.Log -Raw}else{''}
        }
        return $logs
    }
    function Check-AttackMoveProgress([switch]$AllowExited) {
        Check-StorageBudget
        Maintain-OwnedCpuPolicy
        if($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds){throw 'Attack Move scenario exceeded its strict wall timeout.'}
        $currentLogs=Read-AttackMoveLogs
        foreach($entry in $processes){
            if($entry.Role -eq 'warmup'){continue}
            $text=$currentLogs[$entry.Role]
            if($text -match 'PROOF_ATTACK_MOVE_ERROR|PROOF_ATTACK_MOVE_[^\r\n]*status=fail|Fatal error:|Assertion failed:|Unhandled Exception:'){throw "$($entry.Role) reported an Attack Move fixture/runtime failure."}
            if(-not $AllowExited -and $entry.Process.HasExited){throw "$($entry.Role) exited before the validated release barrier."}
            if($entry.Process.HasExited -and $entry.Process.ExitCode -ne 0){throw "$($entry.Role) exited $($entry.Process.ExitCode)."}
        }
    }
    function Wait-AttackMoveMarker($Entry,[string]$Pattern) {
        while($true){
            Check-AttackMoveProgress
            $text=if(Test-Path -LiteralPath $Entry.Log){Get-Content -LiteralPath $Entry.Log -Raw}else{''}
            if($text -match $Pattern){return}
            Start-Sleep -Milliseconds 200
        }
    }
    $commonFlags=@('-ProofSkirmish','-ProofRefineryMinerEvidence',('-ProofAttackMoveRelease="'+$release+'"'))
    Start-AttackMove 'host' '/Game/CommandProofGenerated/Maps/CommandProof' (@('-server','-nullrhi','-MULTIHOME=127.0.0.1','-ProofAttackMoveFixtureHost')+$commonFlags)
    Wait-AttackMoveMarker $processes[1] 'PROOF_SKIRMISH_READY\b'
    Mark-Stage 'hostSkirmishReady'
    Start-AttackMove 'rendered' ('127.0.0.1:'+$Port) (@('-game','-RenderOffscreen','-windowed','-ResX=1280','-ResY=720','-ProofAttackMoveFixture',('-ProofAttackMoveOutput="'+$images+'"'))+$commonFlags)
    # The real viewport client must receive owner 1 before the owner-2 economy observer joins.
    Wait-AttackMoveMarker $processes[2] 'PROOF_ATTACK_MOVE_VIEW_READY owner=1 width=1280 height=720\b'
    Mark-Stage 'renderedProjectionReady'
    Start-AttackMove 'observer' ('127.0.0.1:'+$Port) (@('-game','-nullrhi','-ProofAttackMoveFixture')+$commonFlags)
    $readyPatterns=@{
        host='(?m)PROOF_ATTACK_MOVE_READY role=host status=pass unit=[1-9]\d* target=[1-9]\d* input=synthetic physicalInput=unverified\r?$'
        rendered='(?m)PROOF_ATTACK_MOVE_READY role=rendered status=pass unit=[1-9]\d* target=[1-9]\d* input=synthetic physicalInput=unverified\r?$'
        observer='(?m)PROOF_ATTACK_MOVE_READY role=observer status=pass owner=2 input=synthetic physicalInput=unverified\r?$'
    }
    while($true){
        Check-AttackMoveProgress
        $fixtureLogs=Read-AttackMoveLogs
        $allReady=$true
        foreach($role in @('host','rendered','observer')){
            $marker=[regex]::Match($fixtureLogs[$role],$readyPatterns[$role])
            if(-not $marker.Success){$allReady=$false}else{$result.readyMarkersByRole[$role]=$marker.Value.TrimEnd()}
        }
        if($allReady){break}
        Start-Sleep -Milliseconds 200
    }
    Mark-Stage 'allRolesReady'
    foreach($role in @('rendered','observer')){
        $resolverStartup=[regex]::Match($fixtureLogs[$role],'Set CVar \[\[net\.IpConnectionDisableResolution:1\]\]')
        if(-not $resolverStartup.Success){throw "$role missing literal resolver startup CVar evidence."}
        $result.literalClientSetup.startupConfirmedByRole[$role]=$resolverStartup.Value
    }
    $renderConfig=[regex]::Matches($fixtureLogs.rendered,'PROOF_ATTACK_MOVE_RENDER_CONFIG shadersOptimize=(-?\d+) maxFPS=([\d.]+) resolverDisabled=(-?\d+)')
    if($renderConfig.Count -ne 1){throw 'Need exactly one observed Attack Move render configuration.'}
    $result.renderConfiguration.observedShadersOptimize=[int]$renderConfig[0].Groups[1].Value
    $result.renderConfiguration.observedMaxFPS=[double]::Parse($renderConfig[0].Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture)
    $result.literalClientSetup.observedRenderedResolverDisabled=[int]$renderConfig[0].Groups[3].Value
    if($result.renderConfiguration.observedMaxFPS -ne 30 -or $result.literalClientSetup.observedRenderedResolverDisabled -ne 1 -or $result.renderConfiguration.observedShadersOptimize -ne $result.renderConfiguration.expectedShadersOptimize){throw 'Actual Attack Move render/resolver configuration differs from the requested policy.'}
    # One strict attempt. A parser failure aborts without releasing the fixtures.
    $preReleaseEvidence=Get-AttackMoveEvidence -Logs $fixtureLogs
    if($preReleaseEvidence.passed -isnot [bool] -or -not $preReleaseEvidence.passed){throw 'Strict pre-release Attack Move evidence did not pass.'}
    $result.releaseBarrier.preReleaseEvidence=$preReleaseEvidence
    $result.releaseBarrier.preReleaseValidationPassed=$true
    Mark-Stage 'preReleaseEvidenceVerified'
    Check-AttackMoveProgress
    if(Test-Path -LiteralPath $release){throw 'Release file existed before strict evidence validation.'}
    'release' | Set-Content -LiteralPath $release -Encoding ascii
    $result.releaseBarrier.created=$true
    $result.releaseBarrier.createdUtc=[DateTime]::UtcNow.ToString('o')
    Mark-Stage 'validatedReleaseWritten'
    while(@($processes|Where-Object {-not $_.Process.HasExited}).Count){
        Check-AttackMoveProgress -AllowExited
        Start-Sleep -Milliseconds 200
    }
    Check-AttackMoveProgress -AllowExited
    Mark-Stage 'processesExited'
    Check-StorageBudget -Force
    $fixtureLogs=Read-AttackMoveLogs
    foreach($entry in $processes){
        if($entry.Role -eq 'warmup'){continue}
        if($entry.Process.ExitCode -ne 0){throw "$($entry.Role) exited $($entry.Process.ExitCode)."}
        $pattern='(?m)PROOF_ATTACK_MOVE_COMPLETE role='+$entry.Role+' status=pass input=synthetic physicalInput=unverified\r?$'
        $complete=[regex]::Matches($fixtureLogs[$entry.Role],$pattern)
        if($complete.Count -ne 1){throw "$($entry.Role) missing one exact successful synthetic completion marker."}
        $result.completeMarkersByRole[$entry.Role]=$complete[0].Value.TrimEnd()
    }
    $result.attackMoveEvidence=Get-AttackMoveEvidence -Logs $fixtureLogs -RequireComplete
    if($result.attackMoveEvidence.passed -isnot [bool] -or -not $result.attackMoveEvidence.passed){throw 'Strict final Attack Move evidence did not pass.'}
    $result.globalMovementProofVerified=$true
    foreach($name in @('01-attack-move-moving.png','02-attack-move-firing.png','03-attack-move-resumed.png')){
        $path=Join-Path $images $name
        if(-not(Test-Path -LiteralPath $path)){throw "Missing fresh engine screenshot $name."}
        $file=Get-Item -LiteralPath $path
        if($file.Length -le 1024 -or $file.LastWriteTimeUtc -lt [DateTime]::Parse($result.startedUtc).ToUniversalTime()){throw "Stale/empty screenshot $name."}
        $bytes=[IO.File]::ReadAllBytes($path)
        if([BitConverter]::ToString($bytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A'){throw "Invalid PNG $name."}
        $width=($bytes[16]*16777216)+($bytes[17]*65536)+($bytes[18]*256)+$bytes[19]
        $height=($bytes[20]*16777216)+($bytes[21]*65536)+($bytes[22]*256)+$bytes[23]
        if($width -ne 1280 -or $height -ne 720){throw "Unexpected screenshot size ${width}x${height}."}
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
            $result | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
        }
    }
}
if(-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')
