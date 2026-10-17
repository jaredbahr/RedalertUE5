param([switch]$ProbeNative)
# Extract only actual helper declarations; never execute the Unreal runner body.
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$runner=Join-Path $PSScriptRoot 'test-native-proof.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($runner,[ref]$tokens,[ref]$errors)
if($errors){throw ($errors.Message -join '; ')}
foreach($name in @('Initialize-NativeTransportProbe','Test-NativeTransportProbeIdentity','Observe-NativeTransportProbe','Test-NativeTransportLiveIdentity','Complete-NativeTransportOwnedCleanup')){
 $d=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true)
 if(-not $d){throw "Missing helper $name"};Invoke-Expression $d.Extent.Text
}
$checks=0
function Check([bool]$ok,[string]$name){if(-not $ok){throw "Native transport policy check failed: $name"};$script:checks++}
function Throws([scriptblock]$body,[string]$name){$threw=$false;try{$null=& $body}catch{$threw=$true};Check $threw $name}
$start=[datetime]::SpecifyKind([datetime]'2026-10-17T12:00:00',[DateTimeKind]::Utc)
function Sample([int]$wait=0,[int]$code=0,[int]$id=101,[long]$created=$start.ToFileTimeUtc(),[string]$error=''){
 [pscustomobject]@{error=$error;processId=$id;creationFileTime=$created;waitResult=$wait;exitCode=$code;exitTimeUtc=$(if($wait -eq 0){$start.AddSeconds(2).ToString('o')}else{$null})}
}
function Entry([bool]$managed=$true){
 [pscustomobject]@{Process=[pscustomobject]@{Id=101;HasExited=$managed};Started=$start;Path='D:\UE\UnrealEditor.exe';Role='host';Evidence=[ordered]@{calls=0;observations=@();last=$null;droppedObservations=0;cleanupTimeoutDecided=$false;kernelExitConfirmed=$false;terminationRequested=$false;managedHasExitedAtCleanup=$null;waitStartedUtc=$null;waitReturnedUtc=$null;confirmedExitCode=$null;confirmedExitTimeUtc=$null};Samples=[Collections.Generic.Queue[object]]::new();Waits=[Collections.Generic.List[int]]::new();StopRequests=0;StopError=$null}
}
function Get-NativeTransportProbe($Entry,[int]$Milliseconds){$Entry.Waits.Add($Milliseconds);if(-not $Entry.Samples.Count){throw 'Unexpected extra wait'};return $Entry.Samples.Dequeue()}
function Request-NativeTransportOwnedStop($Entry,[string]$Project){$Entry.StopRequests++;if($Entry.StopError){throw $Entry.StopError};$Entry.Evidence.terminationRequested=$true}
$project='D:\Proof\RedalertProof.uproject';$cmd='"D:\UE\UnrealEditor.exe" "D:\Proof\RedalertProof.uproject" -nullrhi'
$e=Entry
Check (Test-NativeTransportLiveIdentity $e 101 $start $e.Path $cmd $project) 'exact live identity'
Check (Test-NativeTransportLiveIdentity $e 101 $start 'd:/ue/UnrealEditor.exe' ($cmd.Replace('\','/')) $project) 'Windows path normalization'
Check (-not (Test-NativeTransportLiveIdentity $e 102 $start $e.Path $cmd $project)) 'unrelated PID refused'
Check (-not (Test-NativeTransportLiveIdentity $e 101 $start.AddSeconds(1) $e.Path $cmd $project)) 'PID reuse creation mismatch refused'
Check (-not (Test-NativeTransportLiveIdentity $e 101 $start 'D:\Other\UnrealEditor.exe' $cmd $project)) 'wrong executable refused'
Check (-not (Test-NativeTransportLiveIdentity $e 101 $start $e.Path ($cmd.Replace('RedalertProof.uproject','RedalertProof.uproject.backup')) $project)) 'project suffix refused'
Check (-not (Test-NativeTransportLiveIdentity $e 101 $start $e.Path ('-Other="'+$project+'"') $project)) 'project in unrelated argument refused'
Check (-not (Test-NativeTransportLiveIdentity $e 101 $null $e.Path $cmd $project)) 'missing creation refused'
Check (-not (Test-NativeTransportLiveIdentity $e 101 $start $e.Path '' $project)) 'missing command refused'
$space='D:\Proof Space\RedalertProof.uproject'
Check (Test-NativeTransportLiveIdentity $e 101 $start $e.Path ('"'+$space+'"') $space) 'quoted space project accepted'
Check (-not (Test-NativeTransportLiveIdentity $e 101 $start $e.Path $space $space)) 'split unquoted project refused'
foreach($bad in @((Sample -id 102),(Sample -created 1),(Sample -error 'handle missing'),$null)){
 Check (-not (Test-NativeTransportProbeIdentity $e $bad)) 'bad original handle identity refused'
}
$e=Entry;$e.Samples.Enqueue((Sample));$e.Samples.Enqueue((Sample));$probe=Complete-NativeTransportOwnedCleanup $e $project
Check ($e.Evidence.kernelExitConfirmed -and $probe.exitCode -eq 0) 'normal original signal recorded'
Check (($e.Waits -join ',') -eq '0,5000') 'one bounded cleanup wait even already exited'
Check ($e.StopRequests -eq 0) 'no termination of exited process'
$e=Entry;$e.Samples.Enqueue((Sample -wait 258 -code 0));$e.Samples.Enqueue((Sample));$null=Complete-NativeTransportOwnedCleanup $e $project
Check ($e.Evidence.kernelExitConfirmed) 'status0 before signal waits for actual original handle'
Check ($e.StopRequests -eq 0) 'managed exit race does not kill'
$e=Entry $false;$e.Samples.Enqueue((Sample -wait 258 -code 259));$e.Samples.Enqueue((Sample -code 1));$probe=Complete-NativeTransportOwnedCleanup $e $project
Check ($e.StopRequests -eq 1 -and $e.Evidence.terminationRequested) 'live owned stop requested once'
Check ($probe.exitCode -eq 1 -and $e.Evidence.confirmedExitCode -eq 1) 'nonzero final exit retained not rewritten0'
$e=Entry;$e.Samples.Enqueue((Sample -wait 258 -code 0));$e.Samples.Enqueue((Sample -wait 258 -code 0));Throws {Complete-NativeTransportOwnedCleanup $e $project} 'unsignaled cleanup fails'
Check ($e.Evidence.cleanupTimeoutDecided -and -not $e.Evidence.kernelExitConfirmed) 'cleanup timeout latched'
$e.Samples.Enqueue((Sample));Throws {Complete-NativeTransportOwnedCleanup $e $project} 'late signal cannot clear timeout'
Check ($e.Samples.Count -eq 1 -and $e.Waits.Count -eq 2) 'no second cleanup wait after timeout'
$e=Entry;$e.Samples.Enqueue((Sample -id 102));Throws {Complete-NativeTransportOwnedCleanup $e $project} 'bad entry identity fails before wait or stop'
Check ($e.StopRequests -eq 0 -and $e.Waits.Count -eq 1) 'identity failure never terminates'
$e=Entry;$e.Samples.Enqueue((Sample));$e.Samples.Enqueue((Sample -created 1));Throws {Complete-NativeTransportOwnedCleanup $e $project} 'final wrong creation fails'
Check (-not $e.Evidence.kernelExitConfirmed) 'wrong final identity never confirmed'
$e=Entry;for($i=0;$i -lt 10;$i++){$e.Samples.Enqueue((Sample));$null=Observe-NativeTransportProbe $e 0 'mock'}
Check ($e.Evidence.observations.Count -eq 8 -and $e.Evidence.droppedObservations -eq 2 -and $e.Evidence.calls -eq 10) 'observations bounded with last retained'
$e=Entry $false;$e.StopError='fresh lookup disappeared';$e.Samples.Enqueue((Sample -wait 258 -code 259));$e.Samples.Enqueue((Sample));$e.Samples.Enqueue((Sample));$null=Complete-NativeTransportOwnedCleanup $e $project
Check ($e.Evidence.normalExitRaceObserved -and $e.Evidence.kernelExitConfirmed -and -not $e.Evidence.terminationRequested) 'pre-wait lookup exit race recovers only actual original signal'
Check (($e.Waits -join ',') -eq '0,0,5000') 'exit race still uses only one bounded wait'
$e=Entry $false;$e.StopError='fresh identity mismatch';$e.Samples.Enqueue((Sample -wait 258 -code 259));$e.Samples.Enqueue((Sample -wait 258 -code 0));Throws {Complete-NativeTransportOwnedCleanup $e $project} 'managed/status exit is insufficient to recover stop exception'
Check (-not $e.Evidence.kernelExitConfirmed -and $e.Waits.Count -eq 2) 'unsignaled lookup error stays failed without retry'
$e=Entry $false;$e.StopError='kill raced';$e.Samples.Enqueue((Sample -wait 258 -code 259));$e.Samples.Enqueue((Sample -id 102));Throws {Complete-NativeTransportOwnedCleanup $e $project} 'wrong original identity cannot recover kill race'
Check (-not $e.Evidence.kernelExitConfirmed) 'bad native identity stays unconfirmed'
# The runner's functional loop, command assertions and deadline stay separate.
$body=Get-Content $runner -Raw
Check ($body.Contains('while (@($processes | Where-Object {-not $_.Process.HasExited}).Count)')) 'functional completion loop unchanged'
Check ($body.Contains("if ([DateTime]::UtcNow -ge `$deadline) { throw 'Native proof timed out.' }")) 'original functional deadline retained'
Check ($body.Contains('$Entry.Process.Kill()')) 'termination targets original retained process'
if($ProbeNative){
 Initialize-NativeTransportProbe
 $self=[Diagnostics.Process]::GetCurrentProcess();$safe=$self.SafeHandle;$created=$self.StartTime.ToUniversalTime().ToFileTimeUtc()
 $probe=[NativeTransportHandleProbe]::Read($safe,$self.Id,$created,0)
 Check (-not $probe.error -and $probe.waitResult -eq 258 -and $probe.exitCode -eq 259) 'actual kernel probe of live original process'
 $bad=[NativeTransportHandleProbe]::Read($safe,($self.Id+1),$created,0);Check ([bool]$bad.error) 'actual kernel probe rejects wrong PID'
 $bad=[NativeTransportHandleProbe]::Read($safe,$self.Id,($created+1),0);Check ([bool]$bad.error) 'actual kernel probe rejects wrong creation'
 $bad=[NativeTransportHandleProbe]::Read($safe,$self.Id,$created,1);Check ([bool]$bad.error) 'actual probe refuses unreviewed wait duration'
 foreach($exit in @(0,7)){
  $process=Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c',('exit '+$exit)) -WindowStyle Hidden -PassThru
  $pinned=$process.SafeHandle;$at=$process.StartTime.ToUniversalTime().ToFileTimeUtc()
  $probe=[NativeTransportHandleProbe]::Read($pinned,$process.Id,$at,5000)
  Check (-not $probe.error -and $probe.waitResult -eq 0 -and $probe.exitCode -eq $exit -and $probe.exitTimeUtc) ('actual short owned cmd exit'+$exit+' signal/code')
  $process.Dispose()
 }
}
Write-Output ('NATIVE_TRANSPORT_POLICY_PASS checks='+$checks+' nativeProbe='+[bool]$ProbeNative)
