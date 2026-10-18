# Synthetic parser checks only: no engine, processes or hardware input.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test-stop-evidence-policy.ps1')
. (Join-Path $PSScriptRoot 'StopInputBindingEvidence.ps1')
$script:inputChecks=0
function New-NativeStopLogs {
    $logs=New-Logs
    $logs.rendered=($logs.rendered -split '\r?\n'|ForEach-Object{
        if($_ -match '^PROOF_STOP_FIXTURE.*"phase":"(stopped|resumed)"'){$_.Replace('"nativeStopCount":0','"nativeStopCount":1')}else{$_}
    }) -join [Environment]::NewLine
    return $logs
}
function New-InputRows {
    $rows=@()
    foreach($phase in @('press','callback','release','released')){
        $index=$rows.Count
        $rows+=[ordered]@{phase=$phase;owner=1;unit=1;epoch=0;frame=@(10,12,12,14)[$index];processedFrame=@(9,11,11,13)[$index];device=0;user=0;edges=@(1,1,2,2)[$index];nativeStop=@(0,1,1,1)[$index];selected=1;selectionCount=1;sequence=@(5,6,6,6)[$index];keyDown=@(0,1,1,0)[$index];handled=1;ready=1;key='S';inputSource='engineInjection';pointerSource='projectedSynthetic';physicalInputVerified=$false;osWindowFocusVerified=$false}
    }
    return ,$rows
}
function Input-Log($Rows){return (($Rows|ForEach-Object{'PROOF_STOP_INPUT '+($_|ConvertTo-Json -Compress)}) -join [Environment]::NewLine)}
function Reject-Input([string]$Name,[scriptblock]$Edit){
    $rows=New-InputRows;$evidence=Get-StopControlsEvidence -Logs (New-NativeStopLogs) -ExpectedNativeStopDelta 1
    & $Edit $rows $evidence
    $rejected=$false;try{Get-StopInputBindingEvidence -Log (Input-Log $rows) -StopEvidence $evidence|Out-Null}catch{$rejected=$true}
    if(-not $rejected){throw "Incorrect input acceptance: $Name"};$script:inputChecks++
}
$native=Get-StopControlsEvidence -Logs (New-NativeStopLogs) -ExpectedNativeStopDelta 1
$valid=Get-StopInputBindingEvidence -Log (Input-Log (New-InputRows)) -StopEvidence $native
if(-not $valid.passed -or $valid.nativeStopCallbacks -ne 1 -or $valid.physicalInputVerified -or $valid.pressFrame -ne 10){throw 'Valid native synthetic trace failed'};$script:inputChecks++
foreach($case in @('unchanged','two','early')){
    $logs=New-NativeStopLogs
    if($case -eq 'unchanged'){$logs=New-Logs}elseif($case -eq 'two'){$logs.rendered=$logs.rendered.Replace('"nativeStopCount":1','"nativeStopCount":2')}else{$logs.rendered=$logs.rendered.Replace('"nativeStopCount":0','"nativeStopCount":1')}
    $rejected=$false;try{Get-StopControlsEvidence -Logs $logs -ExpectedNativeStopDelta 1|Out-Null}catch{$rejected=$true};if(-not $rejected){throw "Counterfeit native count $case accepted"};$script:inputChecks++
}
$rejected=$false;try{Get-StopControlsEvidence -Logs (New-NativeStopLogs)|Out-Null}catch{$rejected=$true};if(-not $rejected){throw 'Ordinary mode accepted native callbacks'};$script:inputChecks++
Reject-Input 'phase reorder' {param($r,$e)$r[0].phase='callback'}
Reject-Input 'missing key' {param($r,$e)$r[0].Remove('device')}
Reject-Input 'unknown key' {param($r,$e)$r[0].extra=1}
Reject-Input 'wrong identity' {param($r,$e)$r[2].device=1}
Reject-Input 'wrong unit' {param($r,$e)$r[0].unit=2;$r[0].selected=2}
Reject-Input 'foreign owner' {param($r,$e)$r[0].owner=2}
Reject-Input 'epoch' {param($r,$e)$r[0].epoch=1}
Reject-Input 'negative device' {param($r,$e)$r[0].device=-1}
Reject-Input 'zero frame' {param($r,$e)$r[0].frame=0}
Reject-Input 'fractional frame' {param($r,$e)$r[0].frame=10.5}
Reject-Input 'string frame' {param($r,$e)$r[0].frame='10'}
Reject-Input 'bool frame' {param($r,$e)$r[0].frame=$true}
Reject-Input 'uint64 overflow' {param($r,$e)$r[0].frame=[decimal]'18446744073709551616'}
Reject-Input 'fractional counter' {param($r,$e)$r[1].nativeStop=1.5}
Reject-Input 'string counter' {param($r,$e)$r[1].nativeStop='1'}
Reject-Input 'bool counter' {param($r,$e)$r[1].nativeStop=$true}
Reject-Input 'counter multiple' {param($r,$e)$r[1].nativeStop=2}
Reject-Input 'edge multiple' {param($r,$e)$r[2].edges=3}
Reject-Input 'sequence wrong' {param($r,$e)$r[1].sequence=5}
Reject-Input 'selection changed' {param($r,$e)$r[1].selected=2}
Reject-Input 'selection multiple' {param($r,$e)$r[1].selectionCount=2}
Reject-Input 'not ready' {param($r,$e)$r[0].ready=0}
Reject-Input 'handled invalid' {param($r,$e)$r[0].handled=2}
Reject-Input 'callback same frame' {param($r,$e)$r[1].frame=10}
Reject-Input 'callback unprocessed' {param($r,$e)$r[1].processedFrame=9}
Reject-Input 'processed future' {param($r,$e)$r[1].processedFrame=13}
Reject-Input 'stale press frame' {param($r,$e)$r[0].processedFrame=8}
Reject-Input 'stale callback frame' {param($r,$e)$r[1].processedFrame=10}
Reject-Input 'stale release frame' {param($r,$e)$r[2].processedFrame=10}
Reject-Input 'stale released frame' {param($r,$e)$r[3].frame=15;$r[3].processedFrame=13}
# Both callback/release are individually fresh here, but no input stack runs between them.
Reject-Input 'fresh processed regression' {param($r,$e)$r[1].processedFrame=12;$r[2].processedFrame=11}
Reject-Input 'fresh release processing change' {param($r,$e)$r[2].processedFrame=12}
Reject-Input 'callback not down' {param($r,$e)$r[1].keyDown=0}
Reject-Input 'release different frame' {param($r,$e)$r[2].frame=13}
Reject-Input 'release unprocessed' {param($r,$e)$r[3].processedFrame=12}
Reject-Input 'still held' {param($r,$e)$r[3].keyDown=1}
Reject-Input 'wrong key' {param($r,$e)$r[0].key='s'}
Reject-Input 'wrong source' {param($r,$e)$r[0].inputSource='physical'}
Reject-Input 'wrong pointer source' {param($r,$e)$r[0].pointerSource='physical'}
Reject-Input 'physical claim' {param($r,$e)$r[0].physicalInputVerified=$true}
Reject-Input 'string false claim' {param($r,$e)$r[0].physicalInputVerified='false'}
Reject-Input 'focus claim' {param($r,$e)$r[0].osWindowFocusVerified=$true}
Reject-Input 'unvalidated Stop' {param($r,$e)$e.passed=$false}
Reject-Input 'Stop unit mismatch' {param($r,$e)$e.unit=2}
Reject-Input 'no stationary window' {param($r,$e)$e.stationarySimulationSeconds=0}
Reject-Input 'wrong Stop sequences' {param($r,$e)$e.sequences=@(5,7,8)}
foreach($kind in @('duplicate','caseDuplicate','malformed','extra','missing','empty','error')){
    $log=Input-Log (New-InputRows)
    switch($kind){
        duplicate{$log=$log.Replace('"frame":10,','"frame":10,"frame":10,')}
        caseDuplicate{$log=$log.Replace('"frame":10,','"frame":10,"Frame":10,')}
        malformed{$log+="`nPROOF_STOP_INPUT {"}
        extra{$log+="`n"+($log -split '\r?\n')[0]}
        missing{$log=($log -split '\r?\n'|Select-Object -First 3) -join "`n"}
        empty{$log='PROOF_STOP_INPUT'}
        error{$log+="`nPROOF_STOP_INPUT_ERROR reason=synthetic"}
    }
    $rejected=$false;try{Get-StopInputBindingEvidence -Log $log -StopEvidence $native|Out-Null}catch{$rejected=$true};if(-not $rejected){throw "Incorrect acceptance: $kind"};$script:inputChecks++
}
# Exact uint64 frames above double precision must retain order; handled is diagnostic only.
$rows=New-InputRows
foreach($row in $rows){$row.frame=[uint64]$row.frame+[uint64]9007199254740992;$row.processedFrame=[uint64]$row.processedFrame+[uint64]9007199254740992;$row.handled=0}
if(-not (Get-StopInputBindingEvidence -Log (Input-Log $rows) -StopEvidence $native).passed){throw 'Exact uint64/diagnostic handled acceptance failed'};$script:inputChecks++
# Current-frame and previous-frame processing are both fresh; preserve equality at release.
$rows=New-InputRows
foreach($row in $rows){$row.processedFrame=$row.frame}
if(-not (Get-StopInputBindingEvidence -Log (Input-Log $rows) -StopEvidence $native).passed){throw 'Current-frame processing acceptance failed'};$script:inputChecks++
# Adjacent frames near UInt64.MaxValue must not round a two-frame stale press to one.
$rows=New-InputRows
foreach($row in $rows){$row.frame=[uint64]$row.frame+[uint64]18446744073709551590;$row.processedFrame=[uint64]$row.processedFrame+[uint64]18446744073709551590}
if(-not (Get-StopInputBindingEvidence -Log (Input-Log $rows) -StopEvidence $native).passed){throw 'Near-maximum uint64 fresh trace failed'};$script:inputChecks++
$rows[0].processedFrame=[uint64]($rows[0].processedFrame-[uint64]1)
$rejected=$false;try{Get-StopInputBindingEvidence -Log (Input-Log $rows) -StopEvidence $native|Out-Null}catch{$rejected=$true};if(-not $rejected){throw 'Near-maximum uint64 stale trace accepted'};$script:inputChecks++
Write-Output "PASS synthetic-only Stop input binding checks=$script:inputChecks; no engine/runtime/physical evidence"
