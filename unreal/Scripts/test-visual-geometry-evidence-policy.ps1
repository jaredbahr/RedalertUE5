$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'VisualGeometryEvidence.ps1')
$names=@('03-geometry-harvester-contact.png','04-geometry-harvester-motion-a.png','05-geometry-harvester-motion-b.png','06-geometry-rhino-motion-a.png','07-geometry-rhino-motion-b.png')
function Mesh($Location,$Floor=$false,$Rhino=$false) {
    if($Floor){$min=@(-3000,-3000,-30);$max=@(3000,3000,0);$scale=@(60,60,.3)}
    else{$min=@(($Location[0]-50),($Location[1]-35),$Location[2]);$max=@(($Location[0]+50),($Location[1]+35),($Location[2]+70));$scale=@(1,.7,.7)}
    $componentLocation=@($Location|ForEach-Object {$_});if(-not$Floor){$componentLocation[2]+=35}
    [pscustomobject]@{asset=$(if($Rhino){'/Game/BlockoutGenerated/Meshes/SM_RhinoTank_Blockout.SM_RhinoTank_Blockout'}else{'/Engine/BasicShapes/Cube.Cube'});localMin=@(-50,-50,-50);localMax=@(50,50,50);worldMin=$min;worldMax=$max;location=$componentLocation;scale=$scale;rotationPYR=@(0,0,0);axisX=@(1,0,0);axisY=@(0,1,0);axisZ=@(0,0,1)}
}
function Fixture {
    @(for($i=0;$i-lt5;$i++){$position=@(($i*200),0,0);[pscustomobject]@{capture=$names[$i];stage=$i;unit=$(if($i-lt3){2}else{7});owner=1;kind=$(if($i-lt3){1}else{0});epoch=1;revision=(10+$i);frame=(20+$i);elapsed=(1+$i);modelPosition=$position;component=(Mesh $position -Rhino:($i-ge3));floor=(Mesh @(0,0,-15) -Floor:$true);floorActorLocation=@(0,0,-15);floorActorScale=@(60,60,.3);floorActorRotationPYR=@(0,0,0);cameraLocation=@(-650,0,220);cameraRotationPYR=@(-18.7,0,0);width=1280;height=720;orthoWidth=400;projectedMin=@(400,200);projectedMax=@(880,520);rhinoLoaded=$true;visibleUnitViews=1;otherUnitViewsHidden=$true;hudHidden=$true;modelModified=$false;physicalInputVerified=$false}})
}
function Log($Rows){(@(foreach($r in $Rows){'PROOF_VISUAL_GEOMETRY '+($r|ConvertTo-Json -Depth 8 -Compress);'PROOF_VISUAL_GEOMETRY_RESTORED stage='+$r.stage+' verified=1';'PROOF_VISUAL_GEOMETRY_WRITTEN stage='+($r.stage+1);if($r.stage-eq0){'PROOF_SKIRMISH_REQUEST owner=1 sequence=3 type=Gather unit=2 target=1'};if($r.stage-eq3){'PROOF_SKIRMISH_REQUEST owner=1 sequence=5 type=Attack unit=7 target=8'}})-join"`n")}
$script:checks=0
function Check($Pass,$Name){$script:checks++;if(-not$Pass){throw ('Policy check failed: '+$Name)}}
function Reject($Name,[scriptblock]$Mutation){$rows=Fixture;&$Mutation $rows;$rejected=$false;try{Get-VisualGeometryEvidence -Log (Log $rows)|Out-Null}catch{$rejected=$true};Check $rejected $Name}
$valid=Get-VisualGeometryEvidence -Log (Log (Fixture))
Check $valid.passed 'grounded transformed bounds accepted'
Check ($valid.grounding[0].bottomMinusFloorTopCm-eq0) 'signed contact math'
Check ($valid.motion.Count-eq2-and$valid.motion[0].distanceCm-eq200) 'same-unit measured motion'
$elevated=Fixture;foreach($r in $elevated){$r.modelPosition[2]=75;$r.component.location[2]=110;$r.component.worldMin[2]=75;$r.component.worldMax[2]=145}
Check (Get-VisualGeometryEvidence -Log (Log $elevated)).passed 'nonzero model ground Z preserved without a fixed world height'
$pivot=Fixture;foreach($r in $pivot){$r.component.localMin[2]=-20;$r.component.localMax[2]=80;$r.component.location[2]=14}
Check (Get-VisualGeometryEvidence -Log (Log $pivot)).passed 'off-center local pivot uses actual bottom rather than fixed offset'
$tilted=Fixture;foreach($r in $tilted){$r.component.localMin[0]=-20;$r.component.localMax[0]=80;$r.component.scale=@(2,.5,1.2);$r.component.rotationPYR=@(90,0,0);$r.component.axisX=@(0,0,1);$r.component.axisY=@(0,1,0);$r.component.axisZ=@(-1,0,0);$r.component.location[2]=40;$r.component.worldMin=@(($r.modelPosition[0]-60),-25,0);$r.component.worldMax=@(($r.modelPosition[0]+60),25,200)}
Check (Get-VisualGeometryEvidence -Log (Log $tilted)).passed 'pitched nonuniform scale and shifted pivot have manually known transformed bounds'
Reject 'old clipped center placement' {param($r)foreach($v in $r){$v.component.location[2]=0;$v.component.worldMin[2]=-35;$v.component.worldMax[2]=35}}
Reject 'floating transformed bottom' {param($r)$r[0].component.location[2]+=5;$r[0].component.worldMin[2]+=5;$r[0].component.worldMax[2]+=5}
Reject 'reported bottom forged without transform' {param($r)$r[0].component.worldMin[2]=2}
Reject 'reported top forged without transform' {param($r)$r[0].component.worldMax[2]=72}
Reject 'model ground differs from transformed bottom' {param($r)$r[0].modelPosition[2]=2}
Reject 'model Y moved by presentation' {param($r)$r[0].component.location[1]=2}
Reject 'transformed bounds axis mismatch' {param($r)$r[0].component.axisZ=@(1,0,0)}
Reject 'mesh local pivot ignored' {param($r)$r[0].component.localMin[2]=-20;$r[0].component.localMax[2]=80}
Reject 'wrong same-unit identity' {param($r)$r[2].unit=3}
Reject 'nonfinite bounds' {param($r)$r[0].component.worldMin[2]=[double]::NaN}
Reject 'string bound' {param($r)$r[0].component.worldMin[2]='-35'}
Reject 'inverted bound' {param($r)$r[0].component.worldMin[2]=40}
Reject 'duplicate revision' {param($r)$r[1].revision=$r[0].revision}
Reject 'duplicate frame' {param($r)$r[1].frame=$r[0].frame}
Reject 'wrong viewport' {param($r)$r[0].width=960}
Reject 'physical claim' {param($r)$r[0].physicalInputVerified=$true}
Reject 'numeric boolean' {param($r)$r[0].modelModified=0}
Reject 'other unit visible' {param($r)$r[0].visibleUnitViews=2}
Reject 'hidden HUD missing' {param($r)$r[0].hudHidden=$false}
Reject 'wrong displayed mesh' {param($r)$r[0].component.asset='/Game/Wrong.Wrong'}
Reject 'wrong floor mesh' {param($r)$r[0].floor.asset='/Game/Wrong.Wrong'}
Reject 'component not model position' {param($r)$r[0].component.location[0]=20}
Reject 'zero motion' {param($r)$r[2].component.location=$r[1].component.location;$r[2].modelPosition=$r[1].modelPosition;$r[2].component.worldMin=$r[1].component.worldMin;$r[2].component.worldMax=$r[1].component.worldMax}
Reject 'offscreen bound' {param($r)$r[0].projectedMin[0]=-1}
Reject 'negative scale' {param($r)$r[0].component.scale[2]=-1}
Reject 'wrong stage' {param($r)$r[2].stage=4}
Reject 'string stage' {param($r)$r[0].stage='0'}
Reject 'boolean stage' {param($r)$r[0].stage=$false}
Reject 'negative elapsed' {param($r)$r[0].elapsed=-.5}
Reject 'elapsed outside existing bound' {param($r)$r[4].elapsed=221}
$text=Log (Fixture)
foreach($case in @(@{name='missing restoration';text=($text-replace'PROOF_VISUAL_GEOMETRY_RESTORED stage=2 verified=1','')},@{name='missing write';text=($text-replace'PROOF_VISUAL_GEOMETRY_WRITTEN stage=3','')},@{name='duplicate JSON key';text=($text-replace'"stage":0','"stage":0,"stage":0')},@{name='extra row';text=($text+"`n"+($text-split"`n")[0])})){$rejected=$false;try{Get-VisualGeometryEvidence -Log $case.text|Out-Null}catch{$rejected=$true};Check $rejected $case.name}
foreach($case in @(
    @{name='extra malformed measurement';text=($text+"`nPROOF_VISUAL_GEOMETRY malformed")},
    @{name='restoration trailing junk';text=($text-replace'verified=1','verified=10')},
    @{name='write trailing junk';text=($text-replace'WRITTEN stage=1','WRITTEN stage=1 junk')},
    @{name='cross-kind order';text=($text-replace'RESTORED stage=0 verified=1\nPROOF_VISUAL_GEOMETRY_WRITTEN stage=1',('WRITTEN stage=1'+"`n"+'PROOF_VISUAL_GEOMETRY_RESTORED stage=0 verified=1'))},
    @{name='explicit geometry error';text=($text+"`nPROOF_VISUAL_GEOMETRY_ERROR failed")},
    @{name='explicit scenario failure';text=($text+"`nPROOF_SKIRMISH_COMPLETE role=client status=fail")},
    @{name='oversized log';text=('x'*(24MB+1))},
    @{name='different actual Gather miner';text=($text-replace'type=Gather unit=2','type=Gather unit=11')},
    @{name='missing Gather identity';text=($text-replace'type=Gather','type=Other')},
    @{name='different actual attacker';text=($text-replace'type=Attack unit=7','type=Attack unit=9')}
)){$rejected=$false;try{Get-VisualGeometryEvidence -Log $case.text|Out-Null}catch{$rejected=$true};Check $rejected $case.name}
$moveText=$text-replace'type=Attack unit=7 target=8','type=Move unit=7 target=-1'
Check (Get-VisualGeometryEvidence -Log $moveText).passed 'ordinary Move supports observed Combat motion'
foreach($case in @(
    @{name='different actual mover';text=($moveText-replace'type=Move unit=7','type=Move unit=9')},
    @{name='missing movement command';text=($moveText-replace'type=Move','type=Other')},
    @{name='movement command after motionB';text=(($moveText-replace'PROOF_SKIRMISH_REQUEST owner=1 sequence=5 type=Move unit=7 target=-1','')+"`nPROOF_SKIRMISH_REQUEST owner=1 sequence=5 type=Move unit=7 target=-1")},
    @{name='Move invalid positive target';text=($moveText-replace'target=-1','target=8')},
    @{name='Attack invalid negative target';text=($text-replace'type=Attack unit=7 target=8','type=Attack unit=7 target=-1')},
    @{name='movement wrong owner';text=($moveText-replace'owner=1 sequence=5','owner=2 sequence=5')},
    @{name='movement target suffix';text=($moveText-replace'target=-1','target=-10')}
)){$rejected=$false;try{Get-VisualGeometryEvidence -Log $case.text|Out-Null}catch{$rejected=$true};Check $rejected $case.name}
"visual geometry policy checks=$script:checks failures=0 (synthetic policy only; no UE/pixel evidence)"
