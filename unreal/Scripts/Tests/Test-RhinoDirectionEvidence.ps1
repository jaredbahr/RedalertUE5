$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\RhinoDirectionEvidence.ps1"
# Entirely synthetic geometry; no retained runtime evidence or external files.
$template=[pscustomobject]@{capture='';stage=0;unit=13;owner=1;kind=0;epoch=1;revision=1;frame=1;elapsed=1;modelPosition=@(0,0,0);component=@{asset='/Game/BlockoutGenerated/Meshes/SM_RhinoTank_Blockout.SM_RhinoTank_Blockout';localMin=@(-2,-3,0);localMax=@(2,4,2);worldMin=@(0,0,0);worldMax=@(0,0,0);location=@(0,0,0);scale=@(1,1,1);rotationPYR=@(0,0,0);axisX=@(1,0,0);axisY=@(0,1,0);axisZ=@(0,0,1)};floor=@{asset='/Engine/BasicShapes/Cube.Cube';localMin=@(-50,-50,-50);localMax=@(50,50,50);worldMin=@(-3000,-3000,-30);worldMax=@(3000,3000,0);location=@(0,0,-15);scale=@(60,60,.3);rotationPYR=@(0,0,0);axisX=@(1,0,0);axisY=@(0,1,0);axisZ=@(0,0,1)};width=1280;height=720;orthoWidth=750;visibleUnitViews=1;rhinoLoaded=$true;otherUnitViewsHidden=$true;hudHidden=$true;modelModified=$false;physicalInputVerified=$false}
function Fixture([string]$Fault=''){
    $lines=[Collections.Generic.List[string]]::new()
    for($i=0;$i-lt4;$i++){
        $r=$template|ConvertTo-Json -Depth 12|ConvertFrom-Json
        $r.capture=@('08-rhino-direction-first.png','09-rhino-direction-turn.png','10-rhino-stationary-a.png','11-rhino-stationary-b.png')[$i]
        $r.stage=@(1,3,4,5)[$i];$r.revision=200+$i;$r.frame=100+$i;$r.elapsed=@(1,2,3,4.5)[$i]
        $r|Add-Member snapshotDigest '0123456789abcdef0123456789abcdef'
        $r|Add-Member moveSequence $(if($i-eq0){9101}else{9102})
        $r|Add-Member moveReceiptAccepted $true
        $r|Add-Member moveOrigin $(if($i-eq0){@(-1000,-700,0)}else{@(-850,-697.5,0)})
        $r|Add-Member moveGoal $(if($i-eq0){@(-700,-700,0)}else{@(-850,-997.5,0)})
        $r.modelPosition=if($i-eq0){@(-850,-697.5,0)}elseif($i-eq1){@(-842.5,-847.625,0)}else{@(-850,-997.5,0)}
        if($Fault-eq'rejected'-and$i-eq1){$r.moveReceiptAccepted=$false}
        if($Fault-eq'wrong-unit'-and$i-eq1){$r.unit++}
        if($Fault-eq'wrong-heading'-and$i-eq1){$r.modelPosition=@(-700,-700,0)}
        if($Fault-eq'drift'-and$i-eq3){$r.modelPosition[0]+=1}
        if($Fault-eq'short-interval'-and$i-eq3){$r.elapsed=3.5}
        if($Fault-eq'no-revision'-and$i-eq3){$r.revision=202}
        $r.component.location=@($r.modelPosition)
        $delta=if($i-eq0){@(300.0,-5.0)}else{@(-15.0,-299.75)}; $length=[math]::Sqrt($delta[0]*$delta[0]+$delta[1]*$delta[1]); $direction=@(($delta[0]/$length),($delta[1]/$length),0)
        $r|Add-Member authoritativeLastTravelDirection @($direction)
        $r.component.axisY=@($direction)
        $r.component.axisX=@($direction[1],(-$direction[0]),0)
        $minimum=@([double]::PositiveInfinity,[double]::PositiveInfinity,[double]::PositiveInfinity)
        $maximum=@([double]::NegativeInfinity,[double]::NegativeInfinity,[double]::NegativeInfinity)
        for($c=0;$c-lt8;$c++){
            $v=@(0..2|ForEach-Object {if($c-band(1-shl$_)){$r.component.localMax[$_]}else{$r.component.localMin[$_]}})
            for($a=0;$a-lt3;$a++){
                $w=$r.component.location[$a]+$r.component.axisX[$a]*$v[0]*$r.component.scale[0]+$r.component.axisY[$a]*$v[1]*$r.component.scale[1]+$r.component.axisZ[$a]*$v[2]*$r.component.scale[2]
                $minimum[$a]=[math]::Min($minimum[$a],$w);$maximum[$a]=[math]::Max($maximum[$a],$w)
            }
        }
        $r.component.worldMin=$minimum;$r.component.worldMax=$maximum
        if($Fault-eq'wrong-authoritative'-and$i-eq1){$r.authoritativeLastTravelDirection=@(1,0,0)}
        if($Fault-eq'matching-wrong-consumers'-and$i-eq1){$r.authoritativeLastTravelDirection=@(1,0,0);$r.component.axisY=@(1,0,0)}
        if($Fault-eq'wrong-visible-basis'-and$i-eq1){$r.component.axisY=@(0,1,0)}
        if($Fault-eq'forged-bounds'-and$i-eq1){$r.component.worldMax[0]+=1}
        if($i-eq1){switch($Fault){
            'string-unit' {$r.unit=[string]$r.unit}
            'bool-revision' {$r.revision=$true}
            'string-bool' {$r.moveReceiptAccepted='true'}
            'string-elapsed' {$r.elapsed=[string]$r.elapsed}
            'fractional-sequence' {$r.moveSequence=9102.5}
            'zero-epoch' {$r.epoch=0}
            'wrong-digest-type' {$r.snapshotDigest=$true}
        }}
        if($i-lt2){
            $lines.Add(('PROOF_SKIRMISH_REQUEST owner=1 sequence={0} type=Move unit={1} target=-1' -f $r.moveSequence,$r.unit))
            $lines.Add(('PROOF_RHINO_DIRECTION_MOVE stage={0} unit={1} sequence={2} destinationX={3:F9} destinationY={4:F9}' -f (2*$i),$r.unit,$r.moveSequence,$r.moveGoal[0],$r.moveGoal[1]))
        }
        if($Fault-eq'missing-row'-and$i-eq3){continue}
        if($i-lt2){$lines.Add(('PROOF_FACING_MOVE_ACCEPTED epoch=1 unit=13 sequence={0} revision={1}' -f $r.moveSequence,(199+$i)))}
        $before=if($i-eq0){@(-1000,-695,0)}elseif($i-eq1){@(-835,-697.75,0)}elseif($i-eq2){@(-842.5,-847.625,0)}else{@(-850,-997.5,0)}
        $after=if($i-eq0){@(-850,-697.5,0)}elseif($i-eq1){@(-842.5,-847.625,0)}else{@(-850,-997.5,0)}
        $trace=[ordered]@{epoch=1;unit=13;sequence=$(if($i-eq0){9101}else{9102});beforeRevision=199+$i;afterRevision=200+$i;before=$before;after=$after}
        if($Fault-eq'wrong-trace-sequence'-and$i-eq1){$trace.sequence=9999}
        if($Fault-eq'chain-gap'-and$i-eq2){$trace.before[0]+=1}
        if($Fault-eq'trace-string-position'-and$i-eq1){$trace.before[0]='-835'}
        if(-not($Fault-eq'missing-trace'-and$i-eq1)){$lines.Add('PROOF_FACING_DISPLACEMENT '+($trace|ConvertTo-Json -Compress))}
        $json=$r|ConvertTo-Json -Depth 12 -Compress
        if($Fault-eq'duplicate-key'-and$i-eq1){$json=[regex]::Replace($json,'^\{','{"unit":999,')}
        if($Fault-eq'nested-duplicate-key'-and$i-eq1){$json=$json.Replace('"component":{','"component":{"axisY":[0,1,0],')}
        $lines.Add('PROOF_RHINO_DIRECTION '+$json)
        if(-not($Fault-eq'missing-restore'-and$i-eq2)){$lines.Add(('PROOF_RHINO_DIRECTION_RESTORED stage={0} verified=1' -f $r.stage))}
        $write=('PROOF_RHINO_DIRECTION_WRITTEN stage={0}' -f ($r.stage+1))
        if($Fault-eq'trailing-write'-and$i-eq2){$write+=' malformed'}
        $lines.Add($write)
    }
    if($Fault-eq'extra-write'){$lines.Add('PROOF_RHINO_DIRECTION_WRITTEN malformed')}
    if($Fault-eq'failure-marker'){$lines.Add('PROOF_RHINO_DIRECTION_ERROR evidence_failure')}
    return $lines-join"`n"
}
$results=[Collections.Generic.List[object]]::new()
$good=Get-RhinoDirectionEvidence (Fixture)
if(-not$good.passed){throw 'Positive fixture failed'}
$results.Add(@{case='positive';passed=$true})
foreach($fault in @('matching-wrong-consumers','wrong-trace-sequence','chain-gap','trace-string-position','missing-trace','rejected','wrong-unit','wrong-heading','drift','short-interval','no-revision','missing-row','missing-restore','wrong-authoritative','wrong-visible-basis','forged-bounds','string-unit','bool-revision','string-bool','string-elapsed','fractional-sequence','zero-epoch','wrong-digest-type','duplicate-key','nested-duplicate-key','trailing-write','extra-write','failure-marker')){
    $caught=$false;$reason=$null
    try{$null=Get-RhinoDirectionEvidence (Fixture $fault)}catch{$caught=$true;$reason=$_.Exception.Message}
    if(-not$caught){throw ('Guard missed '+$fault)}
    $results.Add(@{case=$fault;passed=$true;rejection=$reason})
}
$results|ConvertTo-Json -Depth 4|Set-Content (Join-Path ([IO.Path]::GetTempPath()) 'rhino-direction-parser-checks.json') -Encoding utf8
$results|ConvertTo-Json -Depth 4


