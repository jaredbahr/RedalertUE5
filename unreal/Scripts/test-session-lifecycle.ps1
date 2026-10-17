param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(0,60000)][int]$Port = 0,
    [ValidateRange(60,240)][int]$TimeoutSeconds = 240,
    [ValidateRange(0,65536)][int]$MaxCacheMiB = 2048,
    [ValidateRange(0,65536)][int]$MaxTempMiB = 1024
)
$ErrorActionPreference = 'Stop'
# Read-only observations of the original process handle.
function Initialize-LifecycleProcessProbe {
    if('SessionLifecycleHandleProbe' -as [type]){return}
    Add-Type @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public sealed class SessionLifecycleHandleProbe {
    public string observedUtc,error,exitTimeUtc;
    public uint processId,waitResult,exitCode;
    public long creationFileTime,exitFileTime;
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint GetProcessId(IntPtr h);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h,uint ms);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr h,out uint code);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetProcessTimes(IntPtr h,out long created,out long exited,out long kernel,out long user);
    public static SessionLifecycleHandleProbe Read(SafeHandle safe,int expectedPid,long expectedCreation,int milliseconds) {
        var r=new SessionLifecycleHandleProbe();bool leased=false;
        try {
            if(milliseconds<0 || (milliseconds>200 && milliseconds!=5000)) throw new ArgumentOutOfRangeException("milliseconds");
            if(safe==null || safe.IsClosed || safe.IsInvalid) throw new InvalidOperationException("Original pinned handle unavailable.");
            safe.DangerousAddRef(ref leased);IntPtr h=safe.DangerousGetHandle();
            r.processId=GetProcessId(h);if(r.processId==0) throw new Win32Exception(Marshal.GetLastWin32Error());
            long kernel,user;
            if(!GetProcessTimes(h,out r.creationFileTime,out r.exitFileTime,out kernel,out user)) throw new Win32Exception(Marshal.GetLastWin32Error());
            if(r.processId!=expectedPid || r.creationFileTime!=expectedCreation) throw new InvalidOperationException("Pinned PID/creation identity mismatch.");
            r.waitResult=WaitForSingleObject(h,(uint)milliseconds);
            if(r.waitResult==0xFFFFFFFF) throw new Win32Exception(Marshal.GetLastWin32Error());
            if(r.waitResult!=0 && r.waitResult!=258) throw new InvalidOperationException("Unexpected native process wait result.");
            if(!GetExitCodeProcess(h,out r.exitCode)) throw new Win32Exception(Marshal.GetLastWin32Error());
            if(!GetProcessTimes(h,out r.creationFileTime,out r.exitFileTime,out kernel,out user)) throw new Win32Exception(Marshal.GetLastWin32Error());
            if(r.waitResult==0 && r.exitFileTime>0) r.exitTimeUtc=DateTime.FromFileTimeUtc(r.exitFileTime).ToString("o");
        }catch(Exception e){r.error=e.Message;}
        finally {
            if(leased){try{safe.DangerousRelease();}catch(Exception e){r.error=(r.error??"")+"; lease release: "+e.Message;}}
            r.observedUtc=DateTime.UtcNow.ToString("o");
        }
        return r;
    }
}
'@
}
function New-LifecycleWaitEvidence {
    return [ordered]@{calls=0;observations=@();last=$null;droppedObservations=0;scenarioTimeoutDecided=$false;cleanupTimeoutDecided=$false;kernelExitConfirmed=$false}
}
function Get-LifecycleNativeProbe($Entry,[int]$Milliseconds) {
    return [SessionLifecycleHandleProbe]::Read($Entry.PinnedHandle,$Entry.Process.Id,$Entry.Started.ToUniversalTime().ToFileTimeUtc(),$Milliseconds)
}
function Test-LifecycleNativeIdentity($Entry,$Probe) {
    return ($null -ne $Probe -and -not $Probe.error -and $Probe.processId -eq $Entry.Process.Id -and $Probe.creationFileTime -eq $Entry.Started.ToUniversalTime().ToFileTimeUtc())
}
function Test-LifecycleNativeSignal($Entry,$Probe) {
    return ((Test-LifecycleNativeIdentity $Entry $Probe) -and $Probe.waitResult -eq 0)
}
function Observe-LifecycleWait($Entry,[int]$Milliseconds,[string]$Phase) {
    $before=Get-LifecycleNativeProbe $Entry 0
    $managed=$null;$managedStatus=$null;$managedError=$null
    try{$managed=$Entry.Process.WaitForExit(0);$managedStatus=$Entry.Process.HasExited}catch{$managedError=$_.Exception.Message}
    $after=Get-LifecycleNativeProbe $Entry $Milliseconds
    $observation=@{phase=$Phase;waitMilliseconds=$Milliseconds;nativeBefore=$before;managedWait0=$managed;managedHasExited=$managedStatus;managedError=$managedError;nativeAfter=$after}
    $evidence=$Entry.ExitWaitEvidence;$evidence.calls++;$evidence.last=$observation
    if($evidence.observations.Count -lt 8){$evidence.observations+=@($observation)}else{$evidence.droppedObservations++}
    if(-not (Test-LifecycleNativeIdentity $Entry $before) -or -not (Test-LifecycleNativeIdentity $Entry $after)){throw ('Owned native process observation failed: '+$before.error+'; '+$after.error)}
    return $after
}

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
    exitWaitPolicy=@{ordinaryScenarioCompletion='Existing managed HasExited and exit-code-zero checks; native confirmation occurs separately in final cleanup.';plannedDisconnect='Original pinned native signal under remaining scenario budget, polls at most 200ms.';finalCleanup='One native 5000ms wait per owned process; a decided timeout cannot be cleared by later evidence.'}
    lifecycle=[ordered]@{initialOwner1Healthy=$false;owner2Disconnected=$false;owner2RejoinedSlot2=$false;rejoinDeferredUntilRematch=$true;rejoinNormalEpochs=@();thirdRejectedWhenFull=$false;terminalEpochs=@();rematchEpochs=@();staleEpochRejected=$false;freshReceiptSequences=@();commonRevisionDigests=@();terminalSnapshotAgreements=@();finishSignaledUtc=$null}
    storageGuard=[ordered]@{softPollingGuard=$true;intervalSeconds=10;maxCacheMiB=$MaxCacheMiB;maxTempMiB=$MaxTempMiB;checks=0;latestCacheBytes=0;latestTempBytes=0;peakCacheBytes=0;peakTempBytes=0}
    cpuPolicy=[ordered]@{availableAffinityMask=('0x{0:X}' -f $availableAffinity);fixtureAffinityMask=('0x{0:X}' -f $fixtureAffinity);logicalProcessors=$fixtureLogicalProcessors;priority='Normal';pollIntervalSeconds=1;polls=0;repairs=0;processes=@()}
    literalClientSetup=[ordered]@{startupFile=$cvars;expectedResolverDisabled=1;startupConfirmedByRole=@{};hostnameOrInternetTest=$false;osDnsChanged=$false}
    timing=[ordered]@{scenarioStartedUtc=$null;clock='Stopwatch monotonic; UTC is correlation only';maxEvents=256;droppedEvents=0;activePhase=$null;failurePhase=$null;events=@()}
    processes=@(); commonRevisions=@(); error=$null; cleanupErrors=@()
}
$entries = [Collections.Generic.List[object]]::new()
$ownedWorkers = [Collections.Generic.List[object]]::new()
$prior = @{}
$lastStorage = -10.0
$result.cpuPolicy.lastPollElapsedSeconds = -1.0
# Diagnostic observations only: these never reset or extend the scenario clock.
function Record-TimingEvent([string]$Kind,[string]$Role,[string]$Detail,[string]$EngineLogUtc='') {
    $elapsed = if ($scenarioWatch) { $scenarioWatch.Elapsed.TotalSeconds } else { 0.0 }
    $phase = [ordered]@{kind=$Kind;role=$Role;detail=$Detail;observedUtc=[DateTime]::UtcNow.ToString('o');scenarioElapsedSeconds=[math]::Round($elapsed,3);remainingBudgetSeconds=[math]::Round([math]::Max(0.0,([double]$TimeoutSeconds-$elapsed)),3);engineLogUtc=$EngineLogUtc}
    $result.timing.activePhase=$phase
    if ($result.timing.events.Count -lt $result.timing.maxEvents) { $result.timing.events+=@($phase) }
    else { $result.timing.droppedEvents++ }
}
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
                    $worker=[pscustomobject]@{Process=$child;PinnedHandle=$child.SafeHandle;ExitWaitEvidence=(New-LifecycleWaitEvidence);Started=$child.StartTime;Role='shader-worker';Path=$workerExe;ParentId=$parentId;ParentStarted=$parent.Started;CommandLine=$command}
                    $ownedWorkers.Add($worker)
                    $result.cpuPolicy.processes+=@{role='shader-worker';pid=$child.Id;parentPid=$parentId;startUtc=$child.StartTime.ToUniversalTime().ToString('o');path=$workerExe;affinityMask=('0x{0:X}' -f [uint64]$child.ProcessorAffinity.ToInt64());priority=$child.PriorityClass.ToString();projectCommandVerified=$true;exitWaitEvidence=$worker.ExitWaitEvidence}
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
function Assert-OwnedRolesHealthy {
    foreach ($entry in $script:entries) {
        # Intentional disconnect exempts only owner2's exit code, never crash/failure evidence.
        if ((Read-Log $entry) -match 'PROOF_LIFECYCLE_[A-Z_]+[^\r\n]*status=fail|Fatal error|Assertion failed') { throw "$($entry.Role) has failure/crash evidence." }
        $entry.Process.Refresh()
        if ($entry.Process.HasExited -and $entry.Process.ExitCode -ne 0 -and -not ($entry.Role -ceq 'owner2' -and $entry.IntentionalStop)) {
            throw "$($entry.Role) exited with code $($entry.Process.ExitCode)."
        }
    }
}
function Request-OwnedStop($Entry,[switch]$Worker,[switch]$IntentionalStop) {
    if ($IntentionalStop -and $Entry.Role -cne 'owner2') { throw 'Only owner2 may be intentionally disconnected.' }
    $original=$Entry.Process
    if (-not $Entry.PinnedHandle -or $Entry.PinnedHandle.IsClosed -or $Entry.PinnedHandle.IsInvalid) { throw 'Original owned process handle is unavailable.' }
    $probe=Observe-LifecycleWait $Entry 0 'before-stop-request'
    if((Test-LifecycleNativeSignal $Entry $probe) -or $probe.exitCode -ne 259){return}
    # Never signal a freshly reopened PID. The Process retains its original handle.
    try {
        $original.Refresh()
        if (-not $original.HasExited) {
            $live=if ($Worker) { Get-OwnedWorkerLiveProcess $Entry } else { Get-OwnedLiveProcess $Entry }
            if (-not $live) {
                $original.Refresh()
                if (-not $original.HasExited) { throw 'Owned process identity unavailable before termination.' }
            } else {
                $original.Refresh()
                if (-not $original.HasExited) {
                    $original.Kill()
                    if ($IntentionalStop) {
                        $Entry.IntentionalStop=$true
                    }
                }
            }
        }
    } catch {
        # Only pre-wait normal-exit/lookup races may be recovered here.
        $original.Refresh()
        if (-not $original.HasExited) { throw }
    }
}
function Stop-OwnedEntry($Entry,[switch]$Worker) {
    if($Entry.ExitWaitEvidence.cleanupTimeoutDecided){throw 'Owned cleanup timeout was already decided; later signal cannot clear it.'}
    Request-OwnedStop $Entry -Worker:$Worker
    # One native wait decides final cleanup. Managed status is diagnostic only.
    $probe=Observe-LifecycleWait $Entry 5000 'final-cleanup'
    if(-not (Test-LifecycleNativeSignal $Entry $probe)){
        $Entry.ExitWaitEvidence.cleanupTimeoutDecided=$true
        throw 'Owned process did not signal exit within the 5000ms cleanup bound.'
    }
    if($Entry.ExitWaitEvidence.cleanupTimeoutDecided){throw 'Owned cleanup timeout was already decided; later signal cannot clear it.'}
    $Entry.ExitWaitEvidence.kernelExitConfirmed=$true
}
function Wait-PlannedDisconnect($Entry) {
    Record-TimingEvent 'wait-planned-disconnect' $Entry.Role 'native original handle; remaining existing scenario budget'
    while($true){
        if($Entry.ExitWaitEvidence.scenarioTimeoutDecided){throw 'Planned disconnect timeout was already decided; later signal cannot clear it.'}
        Assert-OwnedRolesHealthy
        $remaining=[double]$TimeoutSeconds-$scenarioWatch.Elapsed.TotalSeconds
        $milliseconds=[int][math]::Min(200,[math]::Floor($remaining*1000))
        if($milliseconds -le 0){$Entry.ExitWaitEvidence.scenarioTimeoutDecided=$true;throw 'Lifecycle scenario exceeded its strict timeout waiting for planned disconnect.'}
        $probe=Observe-LifecycleWait $Entry $milliseconds 'planned-disconnect'
        Assert-OwnedRolesHealthy
        if($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds){$Entry.ExitWaitEvidence.scenarioTimeoutDecided=$true;throw 'Lifecycle scenario exceeded its strict timeout waiting for planned disconnect.'}
        if($Entry.ExitWaitEvidence.scenarioTimeoutDecided){throw 'Planned disconnect timeout was already decided; later signal cannot clear it.'}
        if(Test-LifecycleNativeSignal $Entry $probe){$Entry.ExitWaitEvidence.kernelExitConfirmed=$true;return}
        Check-StorageBudget;Maintain-OwnedCpuPolicy
    }
}
function Start-Role([string]$Role,[string]$Destination,[string[]]$Mode) {
    $log=Join-Path $run ($Role+'.log')
    $launchArguments=@(('"'+$project+'"'),$Destination)+$Mode+@('-unattended','-nosound','-nosplash','-NoSourceControl','-NoP4','-CoreLimit=2','-ddc=BlockoutDDC','-logcmds="LogDerivedDataCache Verbose"',('-port='+$Port),'-ini:Engine:[DevOptions.Shaders]:WorkerProcessPriority=0,[ConsoleVariables]:t.MaxFPS=30,[AssetRegistry]:EditorGameScansAR=Async,[ShaderCompiler]:r.Shaders.Optimize=1,[ConsoleVariables]:r.Shaders.Optimize=1',('-abslog="'+$log+'"'))
    if ($Role -ne 'host') { $launchArguments+=('-cvarsini="'+$cvars+'"') }
    Record-TimingEvent 'role-start-request' $Role $Destination
    $p=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    Record-TimingEvent 'role-process-started' $Role ($p.StartTime.ToUniversalTime().ToString('o'))
    $entry=[pscustomobject]@{Process=$p;PinnedHandle=$p.SafeHandle;ExitWaitEvidence=(New-LifecycleWaitEvidence);Started=$p.StartTime;Role=$Role;Log=$log;Path=$editor;IntentionalStop=$false}
    $entries.Add($entry)
    $p.ProcessorAffinity=[IntPtr]([int64]$fixtureAffinity); $p.PriorityClass=[Diagnostics.ProcessPriorityClass]::Normal; $p.Refresh()
    if ([uint64]$p.ProcessorAffinity.ToInt64() -ne $fixtureAffinity -or $p.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { throw "Could not apply/verify owned $Role CPU policy." }
    $result.processes+=@{role=$Role;pid=$p.Id;startUtc=$entry.Started.ToUniversalTime().ToString('o');path=$editor;affinityMask=('0x{0:X}' -f [uint64]$p.ProcessorAffinity.ToInt64());priority=$p.PriorityClass.ToString();log=$log;exitWaitEvidence=$entry.ExitWaitEvidence}
    $result.cpuPolicy.processes+=@{role=$Role;pid=$p.Id;startUtc=$entry.Started.ToUniversalTime().ToString('o');path=$editor;affinityMask=('0x{0:X}' -f [uint64]$p.ProcessorAffinity.ToInt64());priority=$p.PriorityClass.ToString()}
    return $entry
}
function Wait-Marker($Entry,[string]$Pattern) {
    Record-TimingEvent 'wait-marker' $Entry.Role $Pattern
    while ($true) {
        Assert-OwnedRolesHealthy
        Check-StorageBudget; Maintain-OwnedCpuPolicy
        $log=Read-Log $Entry
        if ($log -match $Pattern) {
            Assert-OwnedRolesHealthy
            $line=[regex]::Match($log,'(?m)^\[(?<stamp>\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\][^\r\n]*'+$Pattern)
            Record-TimingEvent 'marker-observed' $Entry.Role $Pattern $line.Groups['stamp'].Value
            return
        }
        if ($Entry.Process.HasExited) { throw "$($Entry.Role) exited before expected marker: $Pattern" }
        if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "Scenario timed out waiting for $Pattern" }
        Start-Sleep -Milliseconds 200
    }
}
function Wait-Exited($Entries) {
    Record-TimingEvent 'wait-exit' 'runner' (($Entries | ForEach-Object { $_.Role }) -join ',')
    Assert-OwnedRolesHealthy
    while (@($Entries | Where-Object { -not $_.Process.HasExited }).Count -gt 0) {
        Assert-OwnedRolesHealthy
        Check-StorageBudget; Maintain-OwnedCpuPolicy
        if ($scenarioWatch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Lifecycle scenario exceeded its strict timeout.' }
        Start-Sleep -Milliseconds 200
    }
    Assert-OwnedRolesHealthy
    foreach ($entry in $Entries) {
        if ($entry.Process.ExitCode -ne 0) { throw "$($entry.Role) exited with code $($entry.Process.ExitCode)." }
        $record=@($result.processes | Where-Object {$_.role -eq $entry.Role})[0]
        $record.exitCode=$entry.Process.ExitCode; $record.expectedExitCode=0
    }
}
function Signal-Gate([string]$Name) {
    Assert-OwnedRolesHealthy
    [DateTime]::UtcNow.ToString('o') | Set-Content -LiteralPath (Join-Path $control $Name) -Encoding ascii
    Record-TimingEvent 'gate-written' 'runner' $Name
}
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
    Record-TimingEvent 'wait-common-snapshots' 'runner' ('epoch='+$Epoch)
    if ($ActiveEntries.Count -lt 3) { throw "Need host plus two clients for epoch $Epoch snapshot agreement." }
    while ($true) {
        Assert-OwnedRolesHealthy
        Check-StorageBudget; Maintain-OwnedCpuPolicy
        $maps=@(); foreach ($entry in $ActiveEntries) { $maps+=,(Get-EpochSnapshots (Read-Log $entry)) }
        # Use the same epoch:revision key across host and clients; dictionary
        # keys deliberately include epoch because model revisions restart after rematch.
        $common=@($maps[0].Keys | Where-Object {
            $_ -like ([string]$Epoch+':*') -and $maps[1].ContainsKey($_) -and $maps[2].ContainsKey($_)
        } | Sort-Object { [int](($_ -split ':')[1]) })
        if ($common.Count -gt 0) {
            Assert-OwnedRolesHealthy
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
    Record-TimingEvent 'wait-terminal-agreement' 'runner' ('epoch='+$Epoch+' winner='+$Winner)
    while ($true) {
        Assert-OwnedRolesHealthy
        Check-StorageBudget; Maintain-OwnedCpuPolicy
        $maps=@(); foreach ($entry in $ActiveEntries) { $maps+=,(Get-EpochSnapshots (Read-Log $entry)) }
        $hostKeys=@($maps[0].Keys | Where-Object { $_ -like ([string]$Epoch+':*') } | Sort-Object { [int](($_ -split ':')[1]) })
        if ($hostKeys.Count -gt 0) {
            $key=$hostKeys[-1]; $shared=$true
            for ($index=1; $index -lt $maps.Count; $index++) {
                if (-not $maps[$index].ContainsKey($key) -or $maps[$index][$key] -ne $maps[0][$key]) { $shared=$false; break }
            }
            if ($shared) {
                Assert-OwnedRolesHealthy
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
        try { Stop-OwnedEntry $entry }
        catch { $result.cleanupErrors+=@{role=$entry.Role;pid=$entry.Process.Id;message=$_.Exception.Message}; $result.passed=$false; if (-not $result.error) { $result.error='Owned-process cleanup failed; see cleanupErrors.' } }
    }
    foreach ($worker in $ownedWorkers) {
        try {
            $parentEntry=@($entries | Where-Object {$_.Process.Id -eq $worker.ParentId -and $_.Started -eq $worker.ParentStarted})
            if (-not $parentEntry.Count -and (Get-OwnedWorkerLiveProcess $worker)) { throw 'Owned shader-worker parent identity is unavailable during cleanup.' }
            Stop-OwnedEntry $worker -Worker
        } catch { $result.cleanupErrors+=@{role='shader-worker';pid=$worker.Process.Id;message=$_.Exception.Message}; $result.passed=$false; if (-not $result.error) { $result.error='Owned-process cleanup failed; see cleanupErrors.' } }
    }
}
try {
    Check-StorageBudget -Force
    foreach ($name in @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')) { $prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process') }
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$cache,'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',$temp,'Process'); [Environment]::SetEnvironmentVariable('TMP',$temp,'Process')
    Initialize-LifecycleProcessProbe
    $scenarioWatch=[Diagnostics.Stopwatch]::StartNew()
    $result.timing.scenarioStartedUtc=[DateTime]::UtcNow.ToString('o')
    Record-TimingEvent 'scenario-start' 'runner' ('fixed total budget seconds='+$TimeoutSeconds)
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
    Assert-OwnedRolesHealthy
    Record-TimingEvent 'planned-disconnect-request' $owner2.Role 'original pinned process; no separate phase deadline'
    Request-OwnedStop $owner2 -IntentionalStop
    Wait-PlannedDisconnect $owner2
    $owner2ProcessRecord=@($result.processes | Where-Object {$_.role -eq 'owner2'})[0]
    $owner2ProcessRecord.intentionalDisconnect=$true; $owner2ProcessRecord.exitCode=$owner2.Process.ExitCode
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
    Assert-OwnedRolesHealthy
    $result.passed=$true
} catch {
    $result.error=$_.Exception.Message
    $result.timing.failurePhase=$result.timing.activePhase
    Record-TimingEvent 'failure' 'runner' $result.error
}
finally {
    Stop-OwnedProcesses
    foreach ($name in $prior.Keys) { try { [Environment]::SetEnvironmentVariable($name,$prior[$name],'Process') } catch { $result.cleanupErrors+=@{role='environment';name=$name;message=$_.Exception.Message}; $result.passed=$false } }
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o'); $result.elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3)
    if ($scenarioWatch) { $result.scenarioElapsedSeconds=[math]::Round($scenarioWatch.Elapsed.TotalSeconds,3) }
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
}
if (-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')
