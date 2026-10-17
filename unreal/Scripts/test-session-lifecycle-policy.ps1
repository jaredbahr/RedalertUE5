# Pure mocks: load only actual runner function declarations, never its body.
# No process, Unreal, filesystem log, network or timing waits are exercised.
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'test-session-lifecycle.ps1'),[ref]$tokens,[ref]$errors)
if($errors){throw ($errors.Message -join '; ')}
foreach($name in @('Assert-OwnedRolesHealthy','Stop-OwnedEntry','Get-OwnedLiveProcess','Get-OwnedWorkerLiveProcess','Stop-OwnedProcesses','Wait-Marker','Wait-Exited','Assert-CommonEpochSnapshots','Wait-TerminalSnapshotAgreement','Get-EpochSnapshots')){
    $definition=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true)
    if(-not $definition){throw "Missing runner helper $name"}
    Invoke-Expression $definition.Extent.Text
}
$checks=0
function Check([bool]$Ok,[string]$Name){if(-not $Ok){throw "Lifecycle pure check failed: $Name"};$script:checks++}
function Rejects([scriptblock]$Action,[string]$Expected,[string]$Name){$message='';try{$null=& $Action}catch{$message=$_.Exception.Message};Check ($message -like ('*'+$Expected+'*')) ($Name+' ['+$message+']')}
$start=[datetime]'2026-10-17T09:00:00Z';$workerExe='D:\UE\ShaderCompileWorker.exe'
function New-ProcessMock([bool]$Exited=$false,[int]$Code=0,[bool]$WaitResult=$true){
    $p=[pscustomobject]@{Id=101;StartTime=$start;Path='D:\UE\UnrealEditor.exe';HasExited=$Exited;ExitCode=$Code;WaitResult=$WaitResult;Waits=[Collections.Generic.List[int]]::new();Kills=0;Refreshes=0;CrashOnRefresh=0;KillError=$false}
    $p|Add-Member ScriptMethod Refresh { $this.Refreshes++;if($this.CrashOnRefresh -gt 0 -and $this.Refreshes -ge $this.CrashOnRefresh){$this.HasExited=$true;$this.ExitCode=7} }
    $p|Add-Member ScriptMethod Kill { $this.Kills++;if($this.KillError){throw 'mock kill failure'};$this.ExitCode=1 }
    $p|Add-Member ScriptMethod WaitForExit {param([int]$Milliseconds)$this.Waits.Add($Milliseconds);if($this.WaitResult){$this.HasExited=$true};return $this.WaitResult}
    return $p
}
function New-Entry([string]$Role='host',[bool]$Exited=$false,[int]$Code=0,[bool]$Intentional=$false){
    $p=New-ProcessMock $Exited $Code
    return [pscustomobject]@{Process=$p;PinnedHandle=[pscustomobject]@{IsClosed=$false;IsInvalid=$false};Started=$start;Path=$p.Path;Role=$Role;IntentionalStop=$Intentional;Text='';FatalOnRead=0;Reads=0;ParentId=999;ParentStarted=$start}
}
function Reset-Fixture($Items){
    $script:entries=[Collections.Generic.List[object]]::new();foreach($item in $Items){$script:entries.Add($item)}
    $script:ownedWorkers=[Collections.Generic.List[object]]::new()
    $script:result=[ordered]@{passed=$true;error='Original scenario failure';cleanupErrors=@();processes=@();lifecycle=[ordered]@{commonRevisionDigests=@();terminalSnapshotAgreements=@()};commonRevisions=@()}
    $script:events=[Collections.Generic.List[string]]::new();$script:candidate=$null
    $script:scenarioWatch=[pscustomobject]@{Elapsed=[pscustomobject]@{TotalSeconds=0}};$script:TimeoutSeconds=240
}
# Replace external boundaries only. Policy, marker ordering and ownership checks
# above execute unchanged against these controlled records.
function Read-Log($Entry){$Entry.Reads++;if($Entry.FatalOnRead -gt 0 -and $Entry.Reads -ge $Entry.FatalOnRead){return ($Entry.Text+"`nAssertion failed: synthetic")};return $Entry.Text}
function Get-Process {param($Id,$ErrorAction)return $script:candidate}
function Record-TimingEvent($Kind,$Role,$Detail,$EngineLogUtc){$script:events.Add($Kind)}
function Check-StorageBudget {}
function Maintain-OwnedCpuPolicy {}
function Start-Sleep {throw 'Unexpected polling sleep in pure regression'}

$hostEntry=New-Entry;$third=New-Entry 'third' $true 0
Reset-Fixture @($hostEntry,$third);Assert-OwnedRolesHealthy
Check ($true) 'clean third early exit0 allowed'
$third.Process.ExitCode=9
Rejects {Assert-OwnedRolesHealthy} 'third exited with code 9' 'nonzero third fails despite another healthy role'
$owner2=New-Entry 'owner2' $true 1 $false
Reset-Fixture @($hostEntry,$owner2)
Rejects {Assert-OwnedRolesHealthy} 'owner2 exited with code 1' 'unplanned owner2 nonzero not exempt'
$owner2.IntentionalStop=$true;Assert-OwnedRolesHealthy
Check ($true) 'exact owner2 intentional nonzero exempt'
foreach($role in @('host','lead','rejoin','third','Owner2')){
    $wrong=New-Entry $role $true 1 $true;Reset-Fixture @($wrong)
    Rejects {Assert-OwnedRolesHealthy} 'exited with code 1' ('intentional flag does not exempt '+$role)
}
foreach($badLog in @('Fatal error: crash','Assertion failed: guard','PROOF_LIFECYCLE_CLIENT_COMPLETE role=owner2 status=fail')){
    $owner2.Text=$badLog;Reset-Fixture @($owner2)
    Rejects {Assert-OwnedRolesHealthy} 'failure/crash evidence' ('intentional owner2 never exempts '+$badLog)
}
$hostEntry=New-Entry;$hostEntry.Text='READY';$third=New-Entry 'third' $true 0;$third.FatalOnRead=2
Reset-Fixture @($hostEntry,$third)
Rejects {Wait-Marker $hostEntry 'READY'} 'third has failure/crash evidence' 'fatal appended after first check rejects before already-existing marker acceptance'
Check (-not $events.Contains('marker-observed')) 'failed marker guard never records marker accepted'
$third.FatalOnRead=0;$third.Reads=0;Wait-Marker $hostEntry 'READY'
Check ($events.Contains('marker-observed')) 'healthy existing marker remains accepted with clean exited third'
$hostEntry=New-Entry 'host' $true 0;$hidden=New-Entry 'owner2' $true 1 $true;$hidden.Text='Fatal error: hidden outside Wait-Exited subset'
Reset-Fixture @($hostEntry,$hidden)
Rejects {Wait-Exited @($hostEntry)} 'owner2 has failure/crash evidence' 'Wait-Exited before condition checks all script-owned roles, not parameter subset'
$hostEntry=New-Entry;$hostEntry.Process.CrashOnRefresh=2;Reset-Fixture @($hostEntry)
Rejects {Wait-Exited @($hostEntry)} 'host exited with code 7' 'Wait-Exited inside polling loop fails immediately on new crash'
$hostEntry=New-Entry 'host' $true 0;Reset-Fixture @($hostEntry);$result.processes=@(@{role='host'})
Wait-Exited @($hostEntry)
Check ($result.processes[0].exitCode -eq 0) 'clean completed Wait-Exited preserves final exit acceptance'
$hostEntry=New-Entry;Reset-Fixture @($hostEntry);$scenarioWatch.Elapsed.TotalSeconds=240;$result.passed=$false
Rejects {Wait-Exited @($hostEntry)} 'strict timeout' 'healthy unfinished process at unchanged240deadline fails without another poll'
Rejects {Wait-Marker $hostEntry 'ABSENT'} 'Scenario timed out' 'absent marker at unchanged240deadline fails without sleep'
Check (-not $result.passed -and -not $events.Contains('marker-observed')) 'deadline failures cannot promote failed gate or marker acceptance'
foreach($poll in @('common','terminal')){
    $hostEntry=New-Entry;$hostEntry.Text='PROOF_SNAPSHOT epoch=1 revision=2 digest=abc'
    $lead=New-Entry 'lead';$lead.Text=$hostEntry.Text;$lead.Process.HasExited=$true;$lead.Process.ExitCode=4
    $rejoin=New-Entry 'rejoin';$rejoin.Text=$hostEntry.Text;Reset-Fixture @($hostEntry,$lead,$rejoin)
    if($poll -eq 'common'){Rejects {Assert-CommonEpochSnapshots 1 @($hostEntry,$lead,$rejoin)} 'lead exited with code 4' 'common digest cannot mask already-exited nonzero client'}
    else{Rejects {Wait-TerminalSnapshotAgreement 1 0 @($hostEntry,$lead,$rejoin)} 'lead exited with code 4' 'terminal digest cannot mask already-exited nonzero client'}
}

$entry=New-Entry;Reset-Fixture @($entry);$script:candidate=$entry.Process
Stop-OwnedEntry $entry
Check ($entry.Process.Kills -eq 1 -and $entry.Process.Waits.Count -eq 1 -and $entry.Process.Waits[0] -eq 5000) 'owned live termination uses original object and one finite5000 kernel wait'
$entry=New-Entry 'owner2';Reset-Fixture @($entry);$script:candidate=$entry.Process
Stop-OwnedEntry $entry -IntentionalStop
Check ($entry.IntentionalStop -and $entry.Process.Kills -eq 1) 'intentional owner2 flag set only following actual successful termination request'
$entry=New-Entry 'lead';Reset-Fixture @($entry);$script:candidate=$entry.Process
Rejects {Stop-OwnedEntry $entry -IntentionalStop} 'Only owner2' 'wrong intentional role rejected before termination'
Check ($entry.Process.Kills -eq 0 -and -not $entry.IntentionalStop) 'invalid intentional role does not kill or exempt'
$entry=New-Entry 'owner2';$entry.Process.KillError=$true;Reset-Fixture @($entry);$script:candidate=$entry.Process
Rejects {Stop-OwnedEntry $entry -IntentionalStop} 'mock kill failure' 'failed intentional termination preserves error'
Check (-not $entry.IntentionalStop) 'failed kill never sets intentional exemption'
$entry=New-Entry 'third' $true 0;$entry.Process.WaitResult=$false;Reset-Fixture @($entry)
Rejects {Stop-OwnedEntry $entry} 'did not signal exit' 'status-only exit without kernel signal fails cleanup'
Check ($entry.Process.Kills -eq 0 -and $entry.Process.Waits[0] -eq 5000) 'already-status-exited child still waits bound and is never killed'
$entry=New-Entry;Reset-Fixture @($entry);$replacement=New-ProcessMock;$replacement.StartTime=$start.AddSeconds(1);$script:candidate=$replacement
Rejects {Stop-OwnedEntry $entry} 'identity unavailable' 'PID replacement cannot be signaled'
Check ($entry.Process.Kills -eq 0 -and $replacement.Kills -eq 0) 'neither original nor PID replacement killed on identity mismatch'
$entry=New-Entry 'third' $true 0;Reset-Fixture @($entry);$script:candidate=$replacement
Stop-OwnedEntry $entry
Check ($replacement.Kills -eq 0 -and $entry.Process.Waits[0] -eq 5000) 'fresh PID replacement never bypasses original exited-handle confirmation'
$entry=New-Entry;Reset-Fixture @($entry);$entry.PinnedHandle.IsClosed=$true
Rejects {Stop-OwnedEntry $entry} 'handle is unavailable' 'missing original handle fails safely'
$entry=New-Entry 'third' $true 0;$entry.Process.WaitResult=$false;Reset-Fixture @($entry)
Stop-OwnedProcesses
Check (-not $result.passed -and $result.cleanupErrors.Count -eq 1 -and $result.error -eq 'Original scenario failure') 'cleanup timeout cannot preserve passed gate or replace original scenario error'
$result.passed=$true;$result.error=$null;Stop-OwnedProcesses
Check (-not $result.passed -and $result.error -eq 'Owned-process cleanup failed; see cleanupErrors.') 'cleanup-only failure gets explicit result error'
Write-Output "PASS $checks pure lifecycle fail-fast/owned-cleanup checks; no process or Unreal launched."
