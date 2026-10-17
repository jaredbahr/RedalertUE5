param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(20000, 60000)][int]$Port = 28777,
    [ValidateRange(35, 90)][int]$TimeoutSeconds = 60
)
$ErrorActionPreference = 'Stop'
# Original-handle observations are separate from the unchanged functional deadline.
function Initialize-NativeTransportProbe {
    if('NativeTransportHandleProbe' -as [type]){return}
    Add-Type @"
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public sealed class NativeTransportHandleProbe {
 public string observedUtc,error,exitTimeUtc;
 public uint processId,waitResult,exitCode;
 public long creationFileTime,exitFileTime;
 [DllImport("kernel32.dll",SetLastError=true)] static extern uint GetProcessId(IntPtr h);
 [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr h,uint ms);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr h,out uint code);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetProcessTimes(IntPtr h,out long created,out long exited,out long kernel,out long user);
 public static NativeTransportHandleProbe Read(SafeHandle safe,int pid,long created,int milliseconds) {
  var r=new NativeTransportHandleProbe();bool leased=false;
  try {
   if(milliseconds!=0 && milliseconds!=5000) throw new ArgumentOutOfRangeException("milliseconds");
   if(safe==null || safe.IsInvalid || safe.IsClosed) throw new InvalidOperationException("Original owned handle unavailable.");
   safe.DangerousAddRef(ref leased);IntPtr h=safe.DangerousGetHandle();long kernel,user;
   r.processId=GetProcessId(h);
   if(!GetProcessTimes(h,out r.creationFileTime,out r.exitFileTime,out kernel,out user)) throw new Win32Exception(Marshal.GetLastWin32Error());
   if(r.processId!=pid || r.creationFileTime!=created) throw new InvalidOperationException("Original PID/creation identity mismatch.");
   r.waitResult=WaitForSingleObject(h,(uint)milliseconds);
   if(r.waitResult==0xFFFFFFFF) throw new Win32Exception(Marshal.GetLastWin32Error());
   if(r.waitResult!=0 && r.waitResult!=258) throw new InvalidOperationException("Unexpected native wait result.");
   if(!GetExitCodeProcess(h,out r.exitCode)) throw new Win32Exception(Marshal.GetLastWin32Error());
   if(!GetProcessTimes(h,out r.creationFileTime,out r.exitFileTime,out kernel,out user)) throw new Win32Exception(Marshal.GetLastWin32Error());
   if(r.waitResult==0 && r.exitFileTime>0) r.exitTimeUtc=DateTime.FromFileTimeUtc(r.exitFileTime).ToString("o");
  }catch(Exception e){r.error=e.Message;}
  finally {if(leased){try{safe.DangerousRelease();}catch(Exception e){r.error=(r.error??"")+"; lease release: "+e.Message;}}r.observedUtc=DateTime.UtcNow.ToString("o");}
  return r;
 }
}
"@
}
function Get-NativeTransportProbe($Entry,[int]$Milliseconds) {
    return [NativeTransportHandleProbe]::Read($Entry.PinnedHandle,$Entry.Process.Id,$Entry.Started.ToUniversalTime().ToFileTimeUtc(),$Milliseconds)
}
function Test-NativeTransportProbeIdentity($Entry,$Probe) {
    return ($null -ne $Probe -and -not $Probe.error -and $Probe.processId -eq $Entry.Process.Id -and $Probe.creationFileTime -eq $Entry.Started.ToUniversalTime().ToFileTimeUtc())
}
function Observe-NativeTransportProbe($Entry,[int]$Milliseconds,[string]$Phase) {
    $sample=Get-NativeTransportProbe $Entry $Milliseconds
    $e=$Entry.Evidence;$e.calls++;$e.last=@{phase=$Phase;waitMilliseconds=$Milliseconds;sample=$sample}
    if($e.observations.Count -lt 8){$e.observations+=@($e.last)}else{$e.droppedObservations++}
    if(-not (Test-NativeTransportProbeIdentity $Entry $sample)){throw ('Owned native identity observation failed: '+$sample.error)}
    return $sample
}
function Test-NativeTransportLiveIdentity($Entry,$ActualId,$ActualStart,[string]$ActualPath,[string]$CommandLine,[string]$Project) {
    if($ActualId -ne $Entry.Process.Id -or $ActualStart -isnot [datetime] -or $ActualStart.ToUniversalTime() -ne $Entry.Started.ToUniversalTime() -or -not $ActualPath -or -not $CommandLine -or -not $Project){return $false}
    if(-not [string]::Equals($ActualPath.Replace('/','\'),$Entry.Path.Replace('/','\'),[StringComparison]::OrdinalIgnoreCase)){return $false}
    $pattern=[regex]::Escape($Project.Replace('/','\'));$arg='"'+$pattern+'"'
    if($Project -notmatch '\s'){$arg='(?:'+$arg+'|'+$pattern+')'}
    return [regex]::IsMatch($CommandLine.Replace('/','\'),('(?:^|\s)'+$arg+'(?=\s|$)'),[Text.RegularExpressions.RegexOptions]::IgnoreCase)
}
function Request-NativeTransportOwnedStop($Entry,[string]$Project) {
    $live=Get-Process -Id $Entry.Process.Id -ErrorAction SilentlyContinue
    $identity=Get-CimInstance Win32_Process -Filter ('ProcessId='+$Entry.Process.Id) -ErrorAction SilentlyContinue
    if(-not $live -or -not $identity){throw 'Live owned process identity unavailable before termination.'}
    if(-not (Test-NativeTransportLiveIdentity $Entry $live.Id $live.StartTime $live.Path $identity.CommandLine $Project)){throw 'Refusing termination: owned PID/start/executable/project identity mismatch.'}
    $Entry.Process.Kill() # Uses the original retained Process handle, never a reopened PID.
    $Entry.Evidence.terminationRequested=$true
}
function Complete-NativeTransportOwnedCleanup($Entry,[string]$Project) {
    $e=$Entry.Evidence
    if($e.cleanupTimeoutDecided){throw 'Prior cleanup timeout cannot be cleared by a later observation.'}
    $before=Observe-NativeTransportProbe $Entry 0 'cleanupEntry'
    $e.managedHasExitedAtCleanup=$Entry.Process.HasExited
    if($before.waitResult -ne 0 -and $before.exitCode -eq 259 -and -not $e.managedHasExitedAtCleanup){
        try { Request-NativeTransportOwnedStop $Entry $Project }
        catch {
            $stopError=$_.Exception.Message
            # Only a pre-wait lookup/termination race may be recovered, and only
            # by the same original PID/creation handle already signaling exit.
            $race=Observe-NativeTransportProbe $Entry 0 'stopExitRace'
            if($race.waitResult -ne 0){throw $stopError}
            $e.normalExitRaceObserved=$true
        }
    }
    $e.waitStartedUtc=[DateTime]::UtcNow.ToString('o')
    $after=Observe-NativeTransportProbe $Entry 5000 'finalCleanup'
    $e.waitReturnedUtc=[DateTime]::UtcNow.ToString('o')
    if($after.waitResult -ne 0){$e.cleanupTimeoutDecided=$true;throw 'Original owned process did not signal within the 5000ms cleanup bound.'}
    $e.kernelExitConfirmed=$true;$e.confirmedExitCode=$after.exitCode;$e.confirmedExitTimeUtc=$after.exitTimeUtc
    return $after
}
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $projectRoot '..')).Path
$project = Join-Path $projectRoot 'RedalertProof.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use a D: checkout.' }
if (-not (Test-Path -LiteralPath $editor)) { throw "Missing installed editor: $editor" }
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'Content\CommandProofGenerated\Maps\CommandProof.umap'))) { throw 'Generate CommandProof first.' }
# Probe the actual UDP game port without altering firewall or machine settings.
$probe = [Net.Sockets.UdpClient]::new()
try { $probe.Client.ExclusiveAddressUse = $true; $probe.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Loopback,$Port)) }
finally { $probe.Dispose() }
$run = Join-Path $repoRoot ('generated\native-network-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force -Path $run,(Join-Path $run 'cache'),(Join-Path $run 'temp') | Out-Null
Initialize-NativeTransportProbe
$prior = @{}; $processes = [Collections.Generic.List[object]]::new()
$result = [ordered]@{ head=(& git -C $repoRoot rev-parse HEAD).Trim();dirtyWorkingTree=[bool](& git -C $repoRoot status --porcelain); passed=$false; transport='UE native UDP loopback'; internetTest=$false; performanceTest=$false; runDirectory=$run; port=$Port; commonRevisions=@(); error=$null }
$names = @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$mask=[int64]0;$count=0;$available=[Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64()
for($cpu=0;$cpu -lt 63 -and $count -lt 2;$cpu++){ $bit=[int64]1 -shl $cpu;if($available -band $bit){$mask=$mask -bor $bit;$count++} }
if($count -ne 2){throw 'Need two available CPUs.'}
$result.cpuPolicy=@{mask=$mask;priority='Normal';polls=0;repairs=0;pollingIsNotHardConfinement=$true}
$result.startedUtc=[DateTime]::UtcNow.ToString('o');$result.finishedUtc=$null;$result.scenarioDeadlineUtc=$null;$result.functionalPassed=$false;$result.processes=@();$result.cleanupErrors=@();$result.exitWaitPolicy=@{functionalDeadline=('Original unchanged'+$TimeoutSeconds+'-second readiness/completion deadline, managed HasExited/ExitCode gates');finalCleanup='Original pinned PID/creation handle, one5000ms cleanup wait per owned process; separate from functional deadline'}
function Maintain-OwnedPolicy {
    foreach($entry in $processes){
        $live=Get-Process -Id $entry.Process.Id -ErrorAction SilentlyContinue
        if(-not $live -or $live.StartTime -ne $entry.Started -or $live.Path -ne $entry.Path){continue}
        try{
            if($live.ProcessorAffinity.ToInt64() -ne $mask -or $live.PriorityClass -ne 'Normal'){$result.cpuPolicy.repairs++}
            $live.ProcessorAffinity=[IntPtr]$mask;$live.PriorityClass='Normal';$live.Refresh();$result.cpuPolicy.polls++
            if($live.ProcessorAffinity.ToInt64() -ne $mask -or $live.PriorityClass -ne 'Normal'){throw 'Native network CPU policy failed.'}
        }catch{if(-not $entry.Process.HasExited){throw}}
    }
}
$cvars=Join-Path $run 'literal-client.ini'
"[Startup]`nnet.IpConnectionDisableResolution=1`n" | Set-Content $cvars -Encoding ascii
try {
    foreach ($n in $names) { $prior[$n]=[Environment]::GetEnvironmentVariable($n,'Process') }
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $run 'cache'),'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',(Join-Path $run 'temp'),'Process')
    [Environment]::SetEnvironmentVariable('TMP',(Join-Path $run 'temp'),'Process')
    function Start-Proof([string]$Role,[string]$Destination,[string[]]$Mode) {
        $log = Join-Path $run ($Role+'.log')
        $launchArguments = @(('"'+$project+'"'),$Destination) + $Mode + @('-ProofAutomation','-unattended','-nullrhi','-nosound','-nosplash','-NoSourceControl','-NoP4','-ddc=BlockoutDDC','-CoreLimit=2','-ini:Engine:[AssetRegistry]:EditorGameScansAR=Async,[ConsoleVariables]:t.MaxFPS=30',('-cvarsini="'+$cvars+'"'),('-port='+$Port),('-abslog="'+$log+'"'))
        $p=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
                $e=[ordered]@{role=$Role;pid=$p.Id;startUtc=$null;path=$editor;log=$log;calls=0;observations=@();last=$null;droppedObservations=0;cleanupTimeoutDecided=$false;kernelExitConfirmed=$false;terminationRequested=$false;normalExitRaceObserved=$false;managedHasExitedAtCleanup=$null;waitStartedUtc=$null;waitReturnedUtc=$null;confirmedExitCode=$null;confirmedExitTimeUtc=$null}
        $entry=[pscustomobject]@{Process=$p;PinnedHandle=$null;Started=$null;Role=$Role;Log=$log;Path=$editor;Evidence=$e}
        $processes.Add($entry);$result.processes+=@($e)
        $entry.PinnedHandle=$p.SafeHandle;$entry.Started=$p.StartTime;$e.startUtc=$entry.Started.ToUniversalTime().ToString('o')
        $null=Observe-NativeTransportProbe $entry 0 'launch'
        Maintain-OwnedPolicy
    }
    Start-Proof 'host' '/Game/CommandProofGenerated/Maps/CommandProof' @('-server','-MULTIHOME=127.0.0.1')
    $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds);$result.scenarioDeadlineUtc=$deadline.ToString('o')
    # Readiness marker rather than a fixed startup delay.
    do {
        if ($processes[0].Process.HasExited) { throw 'Server exited before readiness.' }
        $hostText = if(Test-Path $processes[0].Log){Get-Content $processes[0].Log -Raw}else{''}
        if ($hostText -match 'PROOF_READY') { break }
        if ([DateTime]::UtcNow -ge $deadline) { throw 'Server readiness timed out.' }
        Maintain-OwnedPolicy
        Start-Sleep -Milliseconds 200
    } while ($true)
    Start-Proof 'client1' ('127.0.0.1:'+$Port) @('-game')
    Start-Proof 'client2' ('127.0.0.1:'+$Port) @('-game')
    while (@($processes | Where-Object {-not $_.Process.HasExited}).Count) {
        if ([DateTime]::UtcNow -ge $deadline) { throw 'Native proof timed out.' }
        Maintain-OwnedPolicy
        Start-Sleep -Milliseconds 250
    }
    $snapshots=@(); $owners=@()
    foreach ($entry in $processes) {
        if ($entry.Process.ExitCode -ne 0) { throw "$($entry.Role) exited $($entry.Process.ExitCode)." }
        $log=Get-Content -LiteralPath $entry.Log -Raw
        if ($log -match 'PROOF_AUTOMATION_COMPLETE[^\r\n]*status=fail') { throw "$($entry.Role) reported failure." }
        $complete=[regex]::Match($log,'PROOF_AUTOMATION_COMPLETE role=(host|client) owner=(\d+) status=pass[^\r\n]*')
        if (-not $complete.Success) { throw "$($entry.Role) missing successful completion." }
        foreach ($flag in @('moved=1','damaged=1','produced=1')) { if (-not $complete.Value.Contains($flag)) { throw "$($entry.Role) missing $flag." } }
        if ($entry.Role -eq 'host') {
            foreach ($field in @('humanAccepted','rejectedOwnership','botAccepted')) { if ($complete.Value -notmatch ($field+'=([1-9]\d*)')) { throw "Host missing positive $field." } }
        } else {
            $owner=[int]$complete.Groups[2].Value; $owners+=$owner
            if (-not $complete.Value.Contains('ack=1')) { throw 'Client missing acknowledgements.' }
            foreach ($seq in 1..4) {
                $accepted=if($seq -eq 4){0}else{1}
                if ($log -notmatch "PROOF_RESULT owner=$owner sequence=$seq accepted=$accepted reason=\S+") { throw "Missing expected client $owner sequence $seq result." }
            }
        }
        $map=@{}
        foreach ($m in [regex]::Matches($log,'PROOF_SNAPSHOT role=(host|client) owner=\d+(?: epoch=\d+)? revision=(\d+) digest=([A-Za-z0-9]+)')) {
            $rev=$m.Groups[2].Value; $digest=$m.Groups[3].Value
            if ($map.ContainsKey($rev) -and $map[$rev] -ne $digest) { throw "Inconsistent revision $rev within $($entry.Role)." }
            $map[$rev]=$digest
        }
        $snapshots+=,$map
    }
    if (($owners | Sort-Object -Unique).Count -ne 2 -or @($owners | Where-Object {$_ -notin 1,2}).Count) { throw 'Expected distinct native player owners 1 and 2.' }
    $common=@($snapshots[0].Keys | Where-Object {$snapshots[1].ContainsKey($_) -and $snapshots[2].ContainsKey($_)} | Sort-Object {[int]$_})
    if ($common.Count -lt 3) { throw 'Need at least three shared snapshot revisions.' }
    foreach ($rev in $common) { if ($snapshots[0][$rev] -ne $snapshots[1][$rev] -or $snapshots[0][$rev] -ne $snapshots[2][$rev]) { throw "Replica digest mismatch at revision $rev." } }
    $result.commonRevisions=$common; $result.functionalPassed=$true; $result.passed=$true;$result.functionalCompletedUtc=[DateTime]::UtcNow.ToString('o')
} catch { $result.error=$_.Exception.Message }
finally {
    # Prove cleanup only for original owned identities. Unrelated process names are irrelevant.
    foreach ($entry in $processes) {
        try {
            $probe=Complete-NativeTransportOwnedCleanup $entry $project
            if($result.functionalPassed -and $probe.exitCode -ne 0){throw ('Owned '+$entry.Role+' final native exit code '+$probe.exitCode)}
        }catch{
            $result.passed=$false;$result.cleanupErrors+=@{role=$entry.Role;pid=$entry.Process.Id;message=$_.Exception.Message}
            if(-not $result.error){$result.error='Owned native cleanup confirmation failed; see cleanupErrors.'}
        }
    }
    foreach ($n in $names) { if ($prior.ContainsKey($n)) { [Environment]::SetEnvironmentVariable($n,$prior[$n],'Process') } }
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o')
    $result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
}
if (-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')
