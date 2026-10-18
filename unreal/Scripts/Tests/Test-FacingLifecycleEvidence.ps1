$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\FacingLifecycleEvidence.ps1"
function Row([string]$Role,[int]$Epoch,[int]$Revision,$Direction,$Position=@(0,-300,0)){
    [ordered]@{role=$(if($Role-eq'host'){'host'}else{'client'});viewer=$(if($Role-eq'host'){0}elseif($Role-eq'lead'){1}else{2});epoch=$Epoch;revision=$Revision;digest='0123456789abcdef0123456789abcdef';unit=7;owner=1;kind=1;hp=100;position=$Position;lastTravelDirection=$Direction}
}
function Fixture([string]$Fault=''){
    $logs=@{}; $length=[math]::Sqrt(15.0*15.0+300.0*300.0); $secondDirection=@((-15.0/$length),(-300.0/$length),0.0)
    foreach($role in @('host','lead','rejoin')){
        $lines=[Collections.Generic.List[string]]::new()
        if($role-eq'host'){
            $lines.Add('PROOF_FACING_STATE '+((Row $role 1 10 @(1,0,0))|ConvertTo-Json -Compress))
            $lines.Add('PROOF_FACING_MOVE_ACCEPTED epoch=1 unit=7 sequence=1 revision=9')
            $lines.Add('PROOF_FACING_DISPLACEMENT {"epoch":1,"unit":7,"sequence":1,"beforeRevision":9,"afterRevision":10,"before":[-150,-300,0],"after":[0,-300,0]}')
            $lines.Add('PROOF_FACING_MOVE_ACCEPTED epoch=1 unit=7 sequence=2 revision=11')
            $lines.Add('PROOF_FACING_DISPLACEMENT {"epoch":1,"unit":7,"sequence":2,"beforeRevision":11,"afterRevision":12,"before":[15,0,0],"after":[0,-300,0]}')
            foreach($pair in @(@(12,20),@(20,21),@(21,22))){$lines.Add('PROOF_FACING_DISPLACEMENT '+([ordered]@{epoch=1;unit=7;sequence=2;beforeRevision=$pair[0];afterRevision=$pair[1];before=@(0,-300,0);after=@(0,-300,0)}|ConvertTo-Json -Compress))}
            $lines.Add('PROOF_LIFECYCLE_JOIN owner=2 epoch=1 seeded=0')
            $lines.Add('PROOF_HOST_RESULT owner=1 sequence=1 accepted=1 reason=accepted')
            $lines.Add('PROOF_HOST_RESULT owner=1 sequence=2 accepted=1 reason=accepted')
        }
        if($role-eq'lead'){
            $lines.Add('PROOF_FACING_PREFLIGHT_MOVE step=0 unit=7 sequence=1 x=300 y=0')
            $lines.Add('PROOF_FACING_PREFLIGHT_MOVE step=2 unit=7 sequence=2 x=0 y=-300')
            $lines.Add('PROOF_FACING_PREFLIGHT status=pass unit=7 epoch=1 stationarySeconds=1.5')
        }
        if($Fault-eq'fresh-lost-direction'-and$role-eq'rejoin'){$lines.Add('PROOF_FACING_STATE '+((Row $role 1 19 @(0,0,0))|ConvertTo-Json -Compress))}
        foreach($revision in @(20,21,22)){
            $r=Row $role 1 $revision $secondDirection
            if($Fault-eq'content-disagrees'-and$role-eq'lead'-and$revision-eq21){$r.lastTravelDirection=@(1,0,0)}
            if($Fault-eq'drift'-and$role-eq'rejoin'-and$revision-eq22){$r.position=@(1,-300,0)}
            if($Fault-eq'wrong-owner'-and$role-eq'rejoin'-and$revision-eq22){$r.owner=2}
            if($Fault-eq'nonunit-direction'-and$role-eq'rejoin'-and$revision-eq22){$r.lastTravelDirection=@(0,-2,0)}
            $lines.Add('PROOF_FACING_STATE '+($r|ConvertTo-Json -Compress))
            if($role-eq'host'-and$revision-eq21){
                if($Fault-eq'conflicting-duplicate'){$r.lastTravelDirection=@(1,0,0)}
                $lines.Add('PROOF_FACING_STATE '+($r|ConvertTo-Json -Compress))
            }
        }
        foreach($epoch in @(2,3)){
            $r=Row $role $epoch 4 @(0,0,0) @(-750,-400,0)
            if($Fault-eq'stale-reset'-and$role-eq'rejoin'-and$epoch-eq2){$r.lastTravelDirection=@(0,-1,0)}
            if($Fault-eq'tiny-reset-host'-and$role-eq'host'-and$epoch-eq2){$r.lastTravelDirection=@(0.000001,0,0)}
            if($Fault-eq'tiny-reset-lead'-and$role-eq'lead'-and$epoch-eq2){$r.lastTravelDirection=@(0,-0.000000000001,0)}
            if($Fault-eq'tiny-reset-rejoin'-and$role-eq'rejoin'-and$epoch-eq3){$r.lastTravelDirection=@(0,0,0.000000000001)}
            if($Fault-eq'wrong-reset-role'-and$role-eq'rejoin'-and$epoch-eq2){$r.role='host'}
            if($Fault-eq'wrong-reset-viewer'-and$role-eq'rejoin'-and$epoch-eq2){$r.viewer=0}
            if($Fault-eq'string-reset-viewer'-and$role-eq'rejoin'-and$epoch-eq2){$r.viewer='2'}
            if($Fault-eq'bool-reset-revision'-and$role-eq'rejoin'-and$epoch-eq2){$r.revision=$true}
            $json=$r|ConvertTo-Json -Compress
            if($Fault-eq'duplicate-reset-key'-and$role-eq'rejoin'-and$epoch-eq2){$json=[regex]::Replace($json,'^\{','{"viewer":0,')}
            $lines.Add('PROOF_FACING_STATE '+$json)
        }
        $logs[$role]=$lines-join"`n"
    }
    return $logs
}
$out=[Collections.Generic.List[object]]::new()
$logs=Fixture
if(-not(Get-FacingLateObserverEvidence $logs).passed-or-not(Get-FacingResetEvidence $logs).passed){throw 'Positive fixture failed'}
$out.Add(@{case='positive-late-and-reset';passed=$true})
foreach($fault in @('tiny-reset-host','tiny-reset-lead','tiny-reset-rejoin','fresh-lost-direction','content-disagrees','drift','wrong-owner','nonunit-direction','conflicting-duplicate','stale-reset','wrong-reset-role','wrong-reset-viewer','string-reset-viewer','bool-reset-revision','duplicate-reset-key')){
    $logs=Fixture $fault;$caught=$false;$reason=$null
    try{$null=Get-FacingLateObserverEvidence $logs;$null=Get-FacingResetEvidence $logs}catch{$caught=$true;$reason=$_.Exception.Message}
    if(-not$caught){throw ('Guard missed '+$fault)}
    $out.Add(@{case=$fault;passed=$true;rejection=$reason})
}
$out|ConvertTo-Json -Depth 4|Set-Content (Join-Path ([IO.Path]::GetTempPath()) 'facing-lifecycle-parser-checks.json') -Encoding utf8
$out|ConvertTo-Json -Depth 4

