# Synthetic policy regressions only. No Unreal, files outside this script, or processes.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'StopControlsEvidence.ps1')
$script:checks=0
function New-State([int]$Revision,[double]$X,[double]$Seconds,[string]$Role='host',[int]$Viewer=0){
    return [ordered]@{role=$Role;viewer=$Viewer;epoch=0;revision=$Revision;digest=('{0:x32}' -f $Revision);simulatedSeconds=$Seconds;skirmish=0;phase=0;winner=-1;units=@([ordered]@{id=1;owner=1;kind=0;x=$X;y=0;z=0;hp=100;cargo=0;rally=0;rallyX=0;rallyY=0;rallyZ=0});players=@([ordered]@{id=1;team=0;credits=200;gathered=0});resources=@()}
}
function New-Command($Before,$After,[int]$Sequence,[string]$Type){
    return [ordered]@{role='host';owner=1;unit=1;sequence=$Sequence;type=$Type;destinationX=1000;destinationY=0;destinationZ=0;accepted=1;epoch=0;beforeRevision=$Before.revision;afterRevision=$After.revision;beforeDigest=$Before.digest;afterDigest=$After.digest;immutable=1;before=$Before;after=$After}
}
function New-Logs {
    $states=@((New-State 1 0 0),(New-State 2 0 0),(New-State 3 60 .4),(New-State 4 60 .4),(New-State 5 60 .6),(New-State 6 60 .8),(New-State 7 60 1),(New-State 8 60 1.2),(New-State 9 60 1.4),(New-State 10 60 1.4),(New-State 11 90 1.6),(New-State 12 120 1.8))
    $logs=@{}
    foreach($role in @('host','rendered','observer')){
        $rows=@();$view=if($role -eq 'host'){'host'}else{'client'};$viewer=if($role -eq 'host'){0}elseif($role -eq 'rendered'){1}else{2}
        foreach($state in $states){
            $s=New-State $state.revision $state.units[0].x $state.simulatedSeconds $view $viewer
            $rows+=('PROOF_STOP_STATE '+($s|ConvertTo-Json -Depth 10 -Compress))
            $rows+=("PROOF_SNAPSHOT role=$view owner=$viewer revision=$($s.revision) digest=$($s.digest)")
        }
        $logs[$role]=$rows -join [Environment]::NewLine
    }
    foreach($command in @((New-Command $states[0] $states[1] 5 'Move'),(New-Command $states[2] $states[3] 6 'Stop'),(New-Command $states[8] $states[9] 7 'Move'))){$logs.host+=[Environment]::NewLine+'PROOF_STOP_COMMAND '+($command|ConvertTo-Json -Depth 12 -Compress)}
    foreach($phase in @(@('moving',3,5,60,.5),@('stopped',9,6,60,2),@('resumed',12,7,120,2.5))){
        $event=[ordered]@{phase=$phase[0];owner=1;unit=1;epoch=0;revision=$phase[1];sequence=$phase[2];x=$phase[3];y=0;z=0;elapsed=$phase[4];destinationX=1000;destinationY=0;destinationZ=0;nativeStopCount=0}
        $logs.rendered+=[Environment]::NewLine+'PROOF_STOP_FIXTURE '+($event|ConvertTo-Json -Compress)
    }
    return $logs
}
function Reject([string]$Name,[scriptblock]$Edit){
    $logs=New-Logs;& $Edit $logs
    $rejected=$false;try{Get-StopControlsEvidence -Logs $logs|Out-Null}catch{$rejected=$true}
    if(-not $rejected){throw "Incorrect acceptance: $Name"};$script:checks++
}
$valid=Get-StopControlsEvidence -Logs (New-Logs)
if(-not $valid.passed -or $valid.unit -ne 1 -or $valid.stationaryCommonRevisions.Count -ne 6 -or $valid.stationarySimulationSeconds -lt .99999){throw 'Valid synthetic failed'};$script:checks++
Reject 'missing observer' {param($x)$x.Remove('observer')}
Reject 'runtime error' {param($x)$x.host+=[Environment]::NewLine+'PROOF_STOP_ERROR reason=test'}
Reject 'missing Stop command' {param($x)$x.host=($x.host -split '\r?\n'|Where-Object {$_ -notmatch 'PROOF_STOP_COMMAND.*"type":"Stop"'}) -join [Environment]::NewLine}
Reject 'wrong Stop unit' {param($x)$x.host=$x.host.Replace('"unit":1,"sequence":6','"unit":2,"sequence":6')}
Reject 'rejected Stop' {param($x)$x.host=$x.host.Replace('"destinationZ":0,"accepted":1','"destinationZ":0,"accepted":0')}
Reject 'immutable false' {param($x)$x.host=$x.host.Replace('"immutable":1','"immutable":0')}
Reject 'duplicate command' {param($x)$line=($x.host -split '\r?\n'|Where-Object {$_ -match 'PROOF_STOP_COMMAND.*"type":"Stop"'});$x.host+=[Environment]::NewLine+$line}
Reject 'missing resumed phase' {param($x)$x.rendered=($x.rendered -split '\r?\n'|Where-Object {$_ -notmatch 'PROOF_STOP_FIXTURE.*"phase":"resumed"'}) -join [Environment]::NewLine}
Reject 'native callback inflated' {param($x)$x.rendered=$x.rendered.Replace('"nativeStopCount":0','"nativeStopCount":1')}
Reject 'wrong viewer' {param($x)$x.observer=$x.observer.Replace('"viewer":2','"viewer":1')}
Reject 'malformed JSON' {param($x)$x.host+=[Environment]::NewLine+'PROOF_STOP_STATE {'}
Reject 'empty JSON suffix' {param($x)$x.host+=[Environment]::NewLine+'PROOF_STOP_STATE'}
Reject 'duplicate key' {param($x)$x.host=$x.host.Replace('"revision":4,','"revision":4,"revision":4,')}
Reject 'case duplicate key' {param($x)$x.host=$x.host.Replace('"revision":4,','"revision":4,"Revision":4,')}
Reject 'bool revision' {param($x)$x.host=$x.host.Replace('"revision":4,','"revision":true,')}
Reject 'string position' {param($x)$x.observer=$x.observer.Replace('"x":60.0','"x":"60"')}
Reject 'state semantic mismatch' {param($x)$x.observer=$x.observer.Replace('"credits":200','"credits":199')}
Reject 'ordinary digest mismatch' {param($x)$x.observer=$x.observer.Replace('digest=00000000000000000000000000000004','digest=00000000000000000000000000000005')}
Reject 'no movement before stop' {param($x)foreach($r in @('host','rendered','observer')){$x[$r]=$x[$r].Replace('"x":60.0','"x":0.0')}}
Reject 'stop already at destination' {param($x)$x.rendered=$x.rendered.Replace('"destinationX":1000','"destinationX":60')}
Reject 'short host simulation window' {param($x)$x.host=$x.host.Replace('"simulatedSeconds":1.4','"simulatedSeconds":1.3')}
Reject 'nonfinite host clock' {param($x)$x.host=$x.host.Replace('"simulatedSeconds":1.4','"simulatedSeconds":"NaN"')}
Reject 'nonmonotonic clock' {param($x)$x.host=$x.host.Replace('"simulatedSeconds":1.4','"simulatedSeconds":0.1')}
Reject 'cross epoch splice' {param($x)$x.observer=$x.observer.Replace('"epoch":0','"epoch":1')}
Reject 'lost cargo while stopped' {param($x)$x.observer=$x.observer.Replace('"cargo":0','"cargo":1')}
Reject 'row oversized' {param($x)$x.host+=[Environment]::NewLine+'PROOF_STOP_STATE '+(' ' * 32769)}
Reject 'snapshot count cap' {param($x)$line=($x.host -split '\r?\n'|Where-Object {$_ -match '^PROOF_STOP_STATE'}|Select-Object -First 1);$x.host+=([Environment]::NewLine+$line)*1025}
Reject 'string command revision' {param($x)$x.host=$x.host.Replace('"beforeRevision":3','"beforeRevision":"3"')}
Reject 'forged phase destination' {param($x)$x.rendered=$x.rendered.Replace('"destinationX":1000','"destinationX":2000')}
Reject 'missing command destination' {param($x)$x.host=$x.host.Replace('"destinationX":1000,','')}
$logs=New-Logs
$logs.rendered=$logs.rendered.Replace('"phase":"stopped","owner":1,"unit":1,"epoch":0,"revision":9','"phase":"stopped","owner":1,"unit":1,"epoch":0,"revision":6')
$logs.observer=($logs.observer -split '\r?\n'|Where-Object {$_ -notmatch '^PROOF_STOP_STATE.*"revision":6,'}) -join [Environment]::NewLine
if(-not (Get-StopControlsEvidence -Logs $logs).passed){throw 'Later common checkpoints should allow observer to skip an immediate phase revision'};$script:checks++
Reject 'resume destination forgery' {param($x)$lines=$x.host -split '\r?\n';$x.host=($lines|ForEach-Object {if($_ -match '^PROOF_STOP_COMMAND.*"sequence":7,'){ $_.Replace('"destinationX":1000','"destinationX":-9999') }else{$_}}) -join [Environment]::NewLine}
Reject 'resume wrong owner' {param($x)foreach($role in @('host','rendered','observer')){$lines=$x[$role] -split '\r?\n';$x[$role]=($lines|ForEach-Object {if($_ -match '^PROOF_STOP_STATE.*"revision":12,'){ $_.Replace('"id":1,"owner":1','"id":1,"owner":2') }else{$_}}) -join [Environment]::NewLine}}
Reject 'embedded command wrong mode' {param($x)$x.host=($x.host -split '\r?\n'|ForEach-Object {if($_ -match '^PROOF_STOP_COMMAND.*"type":"Stop"'){ $_.Replace('"skirmish":0','"skirmish":1').Replace('"phase":0','"phase":2') }else{$_}}) -join [Environment]::NewLine}
Reject 'stopped destination forgery' {param($x)$x.rendered=($x.rendered -split '\r?\n'|ForEach-Object {if($_ -match '^PROOF_STOP_FIXTURE.*"phase":"stopped"'){ $_.Replace('"destinationX":1000','"destinationX":-9999') }else{$_}}) -join [Environment]::NewLine}
Reject 'duplicate sequence alien unit' {param($x)$line=($x.host -split '\r?\n'|Where-Object {$_ -match '^PROOF_STOP_COMMAND.*"type":"Stop"'});$x.host+=[Environment]::NewLine+$line.Replace('"unit":1,','"unit":999,')}
Reject 'embedded published state contradiction' {param($x)$x.host=($x.host -split '\r?\n'|ForEach-Object {if($_ -match '^PROOF_STOP_COMMAND.*"sequence":5,'){ $_.Replace('"x":0.0','"x":-999.0') }else{$_}}) -join [Environment]::NewLine}
$logs=New-Logs;$logs.host=$logs.host.Replace('"destinationX":1000','"destinationX":1000.0003')
if(-not (Get-StopControlsEvidence -Logs $logs).passed){throw 'Configured phase target may differ within existing 2cm viewport projection contract'};$script:checks++
# Current semantic schema requires rally fields even when no rally is configured.
function Edit-StopStates($Logs,[scriptblock]$Edit){
    foreach($role in @('host','rendered','observer')){
        $Logs[$role]=($Logs[$role] -split '\r?\n'|ForEach-Object{
            if($_ -match '^(PROOF_STOP_STATE|PROOF_STOP_COMMAND) (.*)$'){
                $prefix=$Matches[1];$row=$Matches[2]|ConvertFrom-Json -Depth 20
                if($prefix -eq 'PROOF_STOP_STATE'){& $Edit $row}else{& $Edit $row.before;& $Edit $row.after}
                $prefix+' '+($row|ConvertTo-Json -Depth 20 -Compress)
            }else{$_}
        }) -join [Environment]::NewLine
    }
}
function Add-RallyFactory($Logs){Edit-StopStates $Logs {param($s)$s.units+= [pscustomobject]@{id=2;owner=1;kind=4;x=500;y=500;z=0;hp=200;cargo=0;rally=1;rallyX=123.0000001;rallyY=-456.25;rallyZ=0}}}
$logs=New-Logs;Add-RallyFactory $logs
if(-not (Get-StopControlsEvidence -Logs $logs).passed){throw 'Valid nonzero factory rally semantic state rejected'};$script:checks++
foreach($field in @('rally','rallyX','rallyY','rallyZ')){
    $missingField=$field
    Reject "missing required unit $field" {param($x)Edit-StopStates $x {param($s)$s.units[0].PSObject.Properties.Remove($missingField)}}
}
Reject 'bool rally flag' {param($x)Edit-StopStates $x {param($s)$s.units[0].rally=$false}}
Reject 'string rally flag' {param($x)Edit-StopStates $x {param($s)$s.units[0].rally='0'}}
Reject 'array rally flag' {param($x)Edit-StopStates $x {param($s)$s.units[0].rally=@(0)}}
Reject 'fractional rally flag' {param($x)Edit-StopStates $x {param($s)$s.units[0].rally=.5}}
Reject 'out of range rally flag' {param($x)Edit-StopStates $x {param($s)$s.units[0].rally=2}}
Reject 'string rally coordinate' {param($x)Edit-StopStates $x {param($s)$s.units[0].rallyX='0'}}
Reject 'bool rally coordinate' {param($x)Edit-StopStates $x {param($s)$s.units[0].rallyY=$false}}
Reject 'array rally coordinate' {param($x)Edit-StopStates $x {param($s)$s.units[0].rallyZ=@(0)}}
Reject 'nonfinite rally coordinate' {param($x)Edit-StopStates $x {param($s)$s.units[0].rallyX='NaN'}}
Reject 'out of world rally coordinate' {param($x)Edit-StopStates $x {param($s)$s.units[0].rallyX=10001}}
Reject 'disabled nonzero rally tuple' {param($x)Edit-StopStates $x {param($s)$s.units[0].rallyX=1}}
Reject 'rally must be ground point' {param($x)Add-RallyFactory $x;Edit-StopStates $x {param($s)$s.units[1].rallyZ=1}}
Reject 'nonfactory cannot carry rally' {param($x)Edit-StopStates $x {param($s)$s.units[0].rally=1}}
Reject 'dead factory cannot carry rally' {param($x)Add-RallyFactory $x;Edit-StopStates $x {param($s)$s.units[1].hp=0}}
Reject 'same digest different rally across roles' {param($x)Add-RallyFactory $x;$x.observer=$x.observer.Replace('123.0000001','123.0000002')}
Reject 'Stop mutates unrelated factory rally' {param($x)Add-RallyFactory $x;$x.host=($x.host -split '\r?\n'|ForEach-Object{if($_ -match '^PROOF_STOP_COMMAND .*"type":"Stop"'){$row=$_.Substring(19)|ConvertFrom-Json -Depth 20;$row.after.units[1].rallyX=124;'PROOF_STOP_COMMAND '+($row|ConvertTo-Json -Depth 20 -Compress)}else{$_}}) -join [Environment]::NewLine}
Write-Output "PASS synthetic-only Stop evidence checks=$script:checks; no engine/runtime/physical evidence"
