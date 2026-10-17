# Extract actual policy/publisher helpers only; never execute the engine runner.
$ErrorActionPreference='Stop'
$t=$null;$e=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'test-attack-move-proof.ps1'),[ref]$t,[ref]$e)
if($e){throw ($e.Message -join '; ')}
foreach($name in @('Get-AttackMoveReleasePayload','Publish-AttackMoveRelease','Test-AttackMoveClientsReleased','Publish-AttackMoveHostRelease')){
    $f=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true)
    if(-not $f){throw "Missing actual release helper $name"};Invoke-Expression $f.Extent.Text
}
$checks=0
function Check([bool]$Ok,[string]$Name){if(-not $Ok){throw "Attack release policy failed: $Name"};$script:checks++}
function Rejects([scriptblock]$Action,[string]$Expected,[string]$Name){$message='';try{$null=& $Action}catch{$message=$_.Exception.Message};Check ($message -like ('*'+$Expected+'*')) ($Name+' ['+$message+']')}
$token='0123456789abcdef0123456789abcdef'
function New-Entry([string]$Role,[bool]$Exited=$true){
    $p=[pscustomobject]@{Id=101;HasExited=$Exited;ExitCode=0;KernelSignal=$Exited;Waits=[Collections.Generic.List[int]]::new()}
    $p|Add-Member ScriptMethod WaitForExit {param([int]$Milliseconds)$this.Waits.Add($Milliseconds);if($Milliseconds -ne 0){throw 'Unexpected blocking wait in release policy'};return $this.KernelSignal}
    return [pscustomobject]@{Role=$Role;Process=$p;PinnedHandle=[pscustomobject]@{IsClosed=$false;IsInvalid=$false};Started=[datetime]'2026-10-17T10:00:00Z'}
}
function New-Fixture {
    $logs=@{host=''}
    foreach($role in @('rendered','observer')){$logs[$role]='PROOF_ATTACK_MOVE_RELEASE role='+$role+' phase=clients token='+$token+"`n"+'PROOF_ATTACK_MOVE_COMPLETE role='+$role+' status=pass input=synthetic physicalInput=unverified'}
    $script:freshLogs=$logs
    return [pscustomobject]@{HostEntry=(New-Entry 'host' $false);Clients=@((New-Entry 'rendered'),(New-Entry 'observer'));Logs=$logs;Barrier=[ordered]@{preReleaseValidationPassed=$true;created=$true;token=$token;hostPath='D:\never-write-from-pure-test';hostCreated=$false}}
}
function Read-AttackMoveRoleLog($Entry){return $script:freshLogs[$Entry.Role]}
function Decide($F,[double]$Elapsed=1,[bool]$HostExists=$false){Test-AttackMoveClientsReleased $F.HostEntry $F.Clients $F.Logs $F.Barrier $HostExists $Elapsed 90}
$f=New-Fixture;Check (Decide $f) 'both exact complete clients with original signaled handles permit host release'
Check ($f.Clients[0].Process.Waits[0] -eq 0 -and $f.Clients[1].Process.Waits[0] -eq 0) 'confirmation uses only original-object zero kernel waits'
$stale=New-Fixture;$oldLogs=@{host='';rendered='';observer=''}
Check (Test-AttackMoveClientsReleased $stale.HostEntry $stale.Clients $oldLogs $stale.Barrier $false 1 90) 'confirmed client exit rereads final valid markers instead of rejecting stale caller snapshot'
$f=New-Fixture
$f.Clients[0].Process.KernelSignal=$false
Check (-not (Decide $f)) 'HasExited exit0 without kernel signal keeps host waiting'
$f.Clients[0].Process.HasExited=$false
Check (-not (Decide $f)) 'still-running client keeps host waiting'
$f=New-Fixture;$f.Clients[0].Process.ExitCode=3
Rejects {Decide $f} 'exited 3' 'nonzero client prevents host release'
$f=New-Fixture;$f.Logs.rendered=$f.Logs.rendered.Replace('COMPLETE role=rendered','COMPLETE role=observer')
Rejects {Decide $f} 'wrong completion' 'wrong-role completion rejected'
$f=New-Fixture;$f.Logs.rendered+="`n"+($f.Logs.rendered -split "`n")[-1]
Rejects {Decide $f} 'duplicate or wrong completion' 'duplicate completion rejected'
$f=New-Fixture;$f.Logs.rendered=''
Rejects {Decide $f} 'lacks exact' 'missing completion fails once client kernel exit is confirmed'
$f.Clients[0].Process.KernelSignal=$false;$f.Clients[0].Process.HasExited=$false
Check (-not (Decide $f)) 'missing live-client markers wait without inventing a phase timeout'
foreach($wrong in @('phase=clients token=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','phase=host token='+$token)){
    $f=New-Fixture;$f.Logs.rendered=$f.Logs.rendered.Replace('phase=clients token='+$token,$wrong)
    Rejects {Decide $f} 'wrong release markers' ('wrong token/phase rejected '+$wrong)
}
$f=New-Fixture;$f.Logs.observer+="`nPROOF_ATTACK_MOVE_RELEASE role=observer phase=clients token=$token"
Rejects {Decide $f} 'duplicate or wrong release' 'duplicate release marker rejected'
$f=New-Fixture;$f.Logs.host='PROOF_ATTACK_MOVE_COMPLETE role=host status=pass input=synthetic physicalInput=unverified'
Rejects {Decide $f} 'Host completed or exited' 'early host completion rejected even after both clients complete'
$f=New-Fixture;$f.HostEntry.Process.HasExited=$true
Rejects {Decide $f} 'Host completed or exited' 'early clean host exit rejected'
$f=New-Fixture
Rejects {Decide $f -HostExists $true} 'Host release existed' 'preexisting host phase rejected'
foreach($field in @('preReleaseValidationPassed','created')){
    $f=New-Fixture;$f.Barrier[$field]=$false
    Rejects {Decide $f} 'requires strict pre-release' ('missing barrier stage rejected '+$field)
}
foreach($role in @('host','rendered','observer')){
    $f=New-Fixture;$f.Logs[$role]+="`nPROOF_ATTACK_MOVE_ERROR role=$role reason=synthetic"
    Rejects {Decide $f} 'fixture/runtime failure' ('error never exempt for '+$role)
}
$f=New-Fixture
Check (Decide $f -Elapsed 89.999) 'confirmed clients before unchanged90deadline remain eligible'
Rejects {Decide $f -Elapsed 90} 'strict wall timeout' 'unchanged90deadline rejects host release'
$f.Clients=@($f.Clients[0])
Rejects {Decide $f} 'exactly the two' 'missing client identity rejected'
$f=New-Fixture;$f.Clients[1].Role='rendered'
Rejects {Decide $f} 'exactly the two' 'duplicated client role rejected'
$f=New-Fixture;$f.Clients[0].PinnedHandle.IsClosed=$true
Rejects {Decide $f} 'Original client handle' 'closed original handle rejected'
Check ((Get-AttackMoveReleasePayload 'clients' $token) -ceq ('clients:'+$token)) 'client payload exact phase/run token'
Check ((Get-AttackMoveReleasePayload 'host' $token) -ceq ('host:'+$token)) 'host payload distinct phase/same run token'
Rejects {Get-AttackMoveReleasePayload 'clients' 'bad-token'} 'Invalid Attack Move' 'malformed token rejected'
Rejects {Get-AttackMoveReleasePayload 'other' $token} 'ValidateSet' 'unknown phase rejected'
$f=New-Fixture;$f.Barrier.token='bad-token'
Rejects {Decide $f} 'Invalid Attack Move' 'malformed barrier token rejected before client confirmation'

# Small D:-only file fixtures exercise actual no-newline bytes, stale checks,
# closed pending content and same-directory no-overwrite publication. No children.
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if([IO.Path]::GetPathRoot($repo) -ne 'D:\'){throw 'Use D: policy workspace'}
$dir=Join-Path $repo ('generated\attack-release-policy-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $dir|Out-Null
foreach($phase in @('clients','host')){
    $path=Join-Path $dir $phase;Publish-AttackMoveRelease $path $phase $token
    $bytes=[IO.File]::ReadAllBytes($path);$expected=[Text.Encoding]::ASCII.GetBytes($phase+':'+$token)
    Check ([Convert]::ToHexString($bytes) -ceq [Convert]::ToHexString($expected)) ('published exact ASCII/no-newline/no-BOM '+$phase)
    Check (-not (Test-Path -LiteralPath ($path+'.pending'))) ('closed pending file moved to final '+$phase)
    Rejects {Publish-AttackMoveRelease $path $phase $token} 'Stale Attack Move' ('existing destination never overwritten '+$phase)
}
$pending=Join-Path $dir 'stale';[IO.File]::WriteAllText($pending+'.pending','incomplete')
Rejects {Publish-AttackMoveRelease $pending 'clients' $token} 'Stale Attack Move' 'existing pending marker fails without publication'
Check (-not (Test-Path -LiteralPath $pending)) 'stale pending cannot produce final release'
$f=New-Fixture;$f.Barrier.hostPath=Join-Path $dir 'guarded-host';$f.Clients[0].Process.KernelSignal=$false
Rejects {Publish-AttackMoveHostRelease $f.HostEntry $f.Clients $f.Logs $f.Barrier 1 90} 'have not completed' 'publisher cannot release host while client lacks kernel confirmation'
Check (-not (Test-Path -LiteralPath $f.Barrier.hostPath) -and -not $f.Barrier.hostCreated) 'failed ordering creates no host marker or successful metadata'
$f.Clients[0].Process.KernelSignal=$true
Publish-AttackMoveHostRelease $f.HostEntry $f.Clients $f.Logs $f.Barrier 1 90
Check (([IO.File]::ReadAllText($f.Barrier.hostPath) -ceq ('host:'+$token)) -and $f.Barrier.hostCreated) 'host marker published only after actual shared validator approves both clients'
Write-Output "PASS $checks synthetic release-policy/mock and D:-file publication checks; no process or Unreal launched. Fixtures: $dir"
