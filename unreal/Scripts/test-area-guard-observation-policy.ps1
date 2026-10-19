param([string]$OutputPath = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AreaGuardObservation.ps1')
$run = 'a' * 32; $source = 'b' * 40
function Copy-GuardTest($Value) { return (ConvertTo-Json -InputObject $Value -Depth 18 -Compress | ConvertFrom-Json -AsHashtable -Depth 18) }
function New-GuardTestState([int]$Revision, [double]$X, [int]$Target, [bool]$Returning, [double]$FoeX = 600, [double]$FoeHP = 100) {
    $units = @(
        @{id=1;owner=1;kind=0;hp=100.0;position=@{x=$X;y=0.0;z=0.0};direction=@{x=0.0;y=0.0;z=0.0};cargo=0;rally=$false;rallyPoint=@{x=0.0;y=0.0;z=0.0};guard=($Revision -ge 2);post=@{x=0.0;y=0.0;z=0.0};target=$Target;returning=$Returning},
        @{id=2;owner=100;kind=1;hp=$FoeHP;position=@{x=$FoeX;y=0.0;z=0.0};direction=@{x=0.0;y=0.0;z=0.0};cargo=0;rally=$false;rallyPoint=@{x=0.0;y=0.0;z=0.0};guard=$false;post=@{x=0.0;y=0.0;z=0.0};target=-1;returning=$false}
    )
    $suffix=$Revision.ToString('x')
    return @{epoch=1;revision=$Revision;digest=('a' * (32-$suffix.Length) + $suffix);skirmish=$true;phase=1;winner=-1;
        players=@(@{owner=1;team=0;credits=350;gathered=0},@{owner=100;team=1;credits=350;gathered=0});units=$units;resources=@()}
}
function New-GuardTestLogs {
    param([switch]$FastChase,[switch]$OutsideLeash,[switch]$DelayedPublication)
    $states = [Collections.Generic.List[object]]::new()
    $states.Add((New-GuardTestState 1 0 -1 $false))
    $states.Add((New-GuardTestState 2 0 -1 $false))
    for($step=1;$step -le 20;$step++) {
        $x = 15.0*$step
        if($FastChase -and $step -eq 2){$x=200.0}
        if($OutsideLeash -and $step -eq 20){$x=301.0}
        $hp=if($step -eq 20){97.5}else{100.0}
        $states.Add((New-GuardTestState (2+$step) $x 2 $false 600 $hp))
    }
    $states.Add((New-GuardTestState 23 300 -1 $true 615 97.5))
    for($step=1;$step -le 20;$step++) {
        $states.Add((New-GuardTestState (23+$step) (300.0-15.0*$step) -1 $true 615 97.5))
    }
    $rows = @{host=[Collections.Generic.List[object]]::new();rendered=[Collections.Generic.List[object]]::new();observer=[Collections.Generic.List[object]]::new()}
    foreach ($role in @('host','rendered','observer')) { $rows[$role].Add(@{event='snapshot';state=$states[0]}) }
    $rows.host.Add(@{event='command';owner=1;type=10;sequence=1;payloadEpoch=1;unit=1;target=-1;
        destination=@{x=0.0;y=0.0;z=0.0};accepted=$true;reason='accepted';before=$states[0];after=$states[1]})
    for ($index=1; $index -lt $states.Count; $index++) {
        if ($index -gt 1) { $rows.host.Add(@{event='advance';simulatedSeconds=($index-1)*0.1;deltaSeconds=0.1;before=$states[$index-1];after=$states[$index]}) }
        foreach ($role in @('host','rendered','observer')) { $rows[$role].Add(@{event='snapshot';state=$states[$index]}) }
    }
    $logs = @{}
    foreach ($role in @('host','rendered','observer')) {
        $viewer = @{host=0;rendered=1;observer=2}[$role]
        $lines = [Collections.Generic.List[string]]::new(); $number = 0
        foreach ($row in $rows[$role]) {
            if ($DelayedPublication -and $role -ne 'host' -and $row.event -eq 'snapshot' -and $row.state.revision -eq 3) { continue }
            $record = Copy-GuardTest $row
            $record.v=1; $record.run=$run; $record.source=$source; $record.role=if($viewer -eq 0){'host'}else{'client'}
            $record.viewer=$viewer; $record.row=++$number
            if ($record.event -eq 'snapshot') {
                $state = $record.state
                $lines.Add("PROOF_SNAPSHOT role=$($record.role) owner=$viewer epoch=1 revision=$($state.revision) digest=$($state.digest)")
            }
            $lines.Add('PROOF_AREA_GUARD_OBSERVATION ' + (ConvertTo-Json -InputObject $record -Depth 18 -Compress))
        }
        $logs[$role] = $lines -join "`r`n"
    }
    return $logs
}
function Require-Test($Condition, [string]$Reason) { if (-not $Condition) { throw "AreaGuard policy test: $Reason" } }
function Reject-Test([string]$Name, [scriptblock]$Mutation) {
    $logs = New-GuardTestLogs
    & $Mutation $logs
    $rejected = $false
    try { Get-AreaGuardObservation -Logs $logs -RunId $run -SourceTree $source | Out-Null }
    catch { $rejected = $true }
    Require-Test $rejected "accepted $Name"
}
$positive = Get-AreaGuardObservation -Logs (New-GuardTestLogs) -RunId $run -SourceTree $source
Require-Test ($positive.passed -and $positive.unit -eq 1 -and $positive.commonRevisions.Count -eq 43) 'positive route'
$delayed = Get-AreaGuardObservation -Logs (New-GuardTestLogs -DelayedPublication) -RunId $run -SourceTree $source
Require-Test ($delayed.passed -and $delayed.commonRevisions.Count -eq 42) 'delayed client publication'
foreach ($case in @('FastChase','OutsideLeash')) {
    $rejected = $false
    $caseLogs = if ($case -eq 'FastChase') { New-GuardTestLogs -FastChase } else { New-GuardTestLogs -OutsideLeash }
    try { Get-AreaGuardObservation -Logs $caseLogs -RunId $run -SourceTree $source | Out-Null }
    catch { $rejected = $true }
    Require-Test $rejected "accepted $case"
}
Reject-Test 'source mismatch' { param($logs) $logs.observer = $logs.observer.Replace(('b' * 40), ('c' * 40)) }
Reject-Test 'client guard mismatch' { param($logs) $logs.rendered = $logs.rendered.Replace('"target":2', '"target":-1') }
Reject-Test 'missing actual host advance' { param($logs) $logs.host = @(($logs.host -split '\r?\n') | Where-Object {
    $_ -notmatch 'PROOF_AREA_GUARD_OBSERVATION' -or $_ -notmatch '"event":"advance"'
}) -join "`n" }
Reject-Test 'duplicate JSON field' { param($logs) $logs.host = $logs.host.Replace('"v":1', '"v":1,"v":1') }
Reject-Test 'runtime error' { param($logs) $logs.observer += "`nPROOF_AREA_GUARD_OBSERVATION_ERROR reason=failed" }
Reject-Test 'truncated row' { param($logs) $logs.observer += "`n" + 'PROOF_AREA_GUARD_OBSERVATION {"v":1' }
Reject-Test 'missing ordinary digest' { param($logs) $logs.observer = $logs.observer.Replace('PROOF_SNAPSHOT role=client', 'PROOF_SNAPSHOT role=missing') }
$result = [ordered]@{passed=$true;checks=11;scope='pure parser and synthetic stream faults only; no Unreal runtime'}
if ($OutputPath) { $result | ConvertTo-Json -Compress | Set-Content -LiteralPath $OutputPath -Encoding utf8 }
$result | ConvertTo-Json -Compress
