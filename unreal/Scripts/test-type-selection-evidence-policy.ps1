# Synthetic offline policy checks only. This does not launch Unreal or verify T hardware input.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'TypeSelectionControlsEvidence.ps1')
function New-TypeLogs {
    $logs=@{host='';rendered='';observer=''}
    for($r=10;$r -le 18;$r++){
        $digest=('{0:x32}' -f $r)
        foreach($role in @('host','rendered','observer')){$view=if($role -eq 'host'){'host'}else{'client'};$owner=if($role -eq 'host'){0}elseif($role -eq 'rendered'){1}else{2};$logs[$role]+="PROOF_SNAPSHOT role=$view owner=$owner epoch=0 revision=$r digest=$digest`n"}
        $units=@(@{id=1;owner=1;kind=0;hp=100;cargo=0;x=-700;y=-450;z=0},@{id=2;owner=1;kind=0;hp=100;cargo=0;x=-600;y=0;z=0})
        $clock=@{epoch=0;revision=$r;digest=$digest;simulatedSeconds=($r/5.0);owner1Submissions=4;units=$units}
        $logs.host+='PROOF_TYPE_SELECTION_CLOCK '+($clock|ConvertTo-Json -Compress -Depth 8)+"`n"
        $phases=if($r -eq 10){@('before','after')}else{@('observed')}
        foreach($phase in $phases){
            $projected=@($units|ForEach-Object{$v=@{};foreach($k in $_.Keys){$v[$k]=$_[$k]};$v.screenX=500;$v.screenY=400;$v})
            $selected=if($phase -eq 'before'){@(1)}else{@(1,2)}
            $frame=@{phase=$phase;epoch=0;revision=$r;digest=$digest;actionBeforeDigest=$(if($phase -eq 'after'){$digest}else{''});owner=1;width=1280;height=720;primary=1;nativeT=0;sequence=4;queue=0;selected=@($selected);expected=@(1,2);group9=@(1);groupPrimary=1;units=$projected}
            $logs.rendered+='PROOF_TYPE_SELECTION_FRAME '+($frame|ConvertTo-Json -Compress -Depth 8)+"`n"
        }
    }
    $logs.rendered+="PROOF_TYPE_SELECTION_COMPLETE status=pass inputSource=sharedHandler nativeCallbackInvoked=0 physicalInputVerified=0`n"
    return $logs
}
function Edit-TypeRow($Logs,[string]$Role,[string]$Marker,[scriptblock]$Edit){
    $changed=0
    $Logs[$Role]=[regex]::Replace($Logs[$Role],($Marker+'([^\r\n]*)'),{param($m) $row=$m.Groups[1].Value.Trim()|ConvertFrom-Json -Depth 12;& $Edit $row;$changed++;return $Marker+' '+($row|ConvertTo-Json -Depth 12 -Compress)}.GetNewClosure())
}
$checks=0
$valid=Get-TypeSelectionControlsEvidence (New-TypeLogs)
if(-not $valid.passed -or $valid.commonRevisions.Count -lt 3 -or $valid.hostSimulationSeconds -lt 1 -or $valid.nativeCallbackInvoked -or $valid.physicalInputVerified){throw 'Valid synthetic evidence rejected/false scope'};$checks++
function Reject-Type([string]$Name,[scriptblock]$Mutation){$l=New-TypeLogs;& $Mutation $l;$rejected=$false;try{$null=Get-TypeSelectionControlsEvidence $l}catch{$rejected=$true};if(-not $rejected){throw "Synthetic negative accepted: $Name"};$script:checks++}
Reject-Type 'missing observer' {param($l)$l.Remove('observer')}
Reject-Type 'missing clock' {param($l)$l.host=[regex]::Replace($l.host,'PROOF_TYPE_SELECTION_CLOCK[^\r\n]*','')}
Reject-Type 'wrong selected IDs' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)if($r.phase -eq 'after'){$r.selected=@(1)}}}
Reject-Type 'native callback called' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)$r.nativeT=1}}
Reject-Type 'new local sequence' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)$r.sequence=5}}
Reject-Type 'queued command' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)$r.queue=1}}
Reject-Type 'host ingress changed' {param($l)Edit-TypeRow $l host PROOF_TYPE_SELECTION_CLOCK {param($r)$r.owner1Submissions=5}}
Reject-Type 'stationarity violation' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)if($r.phase -eq 'observed'){$r.units[0].x+=1}}}
Reject-Type 'foreign unit' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)$r.units[0].owner=2}}
Reject-Type 'projection boundary' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)$r.units[0].screenX=1280}}
Reject-Type 'wrong group fingerprint' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)$r.group9=@(2)}}
Reject-Type 'action mutated digest' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)if($r.phase -eq 'after'){$r.actionBeforeDigest='f'*32}}}
Reject-Type 'short actual sim window' {param($l)Edit-TypeRow $l host PROOF_TYPE_SELECTION_CLOCK {param($r)$r.simulatedSeconds=$r.revision/100.0}}
Reject-Type 'observer mismatch' {param($l)$l.observer=$l.observer.Replace('digest=00000000000000000000000000000012','digest=ffffffffffffffffffffffffffffffff').Replace('digest=00000000000000000000000000000011','digest=ffffffffffffffffffffffffffffffff').Replace('digest=00000000000000000000000000000010','digest=ffffffffffffffffffffffffffffffff').Replace('digest=0000000000000000000000000000000f','digest=ffffffffffffffffffffffffffffffff').Replace('digest=0000000000000000000000000000000e','digest=ffffffffffffffffffffffffffffffff').Replace('digest=0000000000000000000000000000000d','digest=ffffffffffffffffffffffffffffffff').Replace('digest=0000000000000000000000000000000c','digest=ffffffffffffffffffffffffffffffff').Replace('digest=0000000000000000000000000000000b','digest=ffffffffffffffffffffffffffffffff')}
Reject-Type 'duplicate JSON key' {param($l)$l.rendered=$l.rendered.Replace('"nativeT":0','"nativeT":0,"nativeT":0')}
Reject-Type 'numeric string' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)$r.nativeT='0'}}
Reject-Type 'producer error' {param($l)$l.host+="PROOF_TYPE_SELECTION_ERROR overflow`n"}
Reject-Type 'false physical scope' {param($l)$l.rendered=$l.rendered.Replace('physicalInputVerified=0','physicalInputVerified=1')}
Reject-Type 'duplicate frame' {param($l)$row=[regex]::Match($l.rendered,'PROOF_TYPE_SELECTION_FRAME[^\r\n]*"phase":"observed"[^\r\n]*').Value;if(-not $row){$row=([regex]::Matches($l.rendered,'PROOF_TYPE_SELECTION_FRAME[^\r\n]*')|Select-Object -Last 1).Value};$l.rendered+=$row+"`n"}
Reject-Type 'observations precede action' {param($l)Edit-TypeRow $l rendered PROOF_TYPE_SELECTION_FRAME {param($r)if($r.phase -eq 'after'){$r.revision=18;$r.digest=('{0:x32}' -f 18);$r.actionBeforeDigest=$r.digest}}}
Write-Output "PASS $checks synthetic Type selection evidence policy checks; no UE/native/physical input evidence."
