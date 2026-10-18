$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'VisualGeometryEvidence.ps1')
$names=@('03-geometry-harvester-contact.png','04-geometry-harvester-motion-a.png','05-geometry-harvester-motion-b.png','06-geometry-rhino-motion-a.png','07-geometry-rhino-motion-b.png')
function Mesh($Location,$Floor=$false,$Rhino=$false) {
    if($Floor){$min=@(-3000,-3000,-30);$max=@(3000,3000,0);$scale=@(60,60,.3)}
    else{$min=@(($Location[0]-50),-35,-35);$max=@(($Location[0]+50),35,35);$scale=@(1,.7,.7)}
    [pscustomobject]@{asset=$(if($Rhino){'/Game/BlockoutGenerated/Meshes/SM_RhinoTank_Blockout.SM_RhinoTank_Blockout'}else{'/Engine/BasicShapes/Cube.Cube'});localMin=@(-50,-50,-50);localMax=@(50,50,50);worldMin=$min;worldMax=$max;location=@($Location|ForEach-Object {$_});scale=$scale;rotationPYR=@(0,0,0);axisX=@(1,0,0);axisY=@(0,1,0);axisZ=@(0,0,1)}
}
function Fixture {
    @(for($i=0;$i-lt5;$i++){$position=@(($i*200),0,0);[pscustomobject]@{capture=$names[$i];stage=$i;unit=$(if($i-lt3){2}else{7});owner=1;kind=$(if($i-lt3){1}else{0});epoch=1;revision=(10+$i);frame=(20+$i);elapsed=(1+$i);modelPosition=$position;component=(Mesh $position -Rhino:($i-ge3));floor=(Mesh @(0,0,-15) -Floor:$true);floorActorLocation=@(0,0,-15);floorActorScale=@(60,60,.3);floorActorRotationPYR=@(0,0,0);cameraLocation=@(-650,0,220);cameraRotationPYR=@(-18.7,0,0);width=1280;height=720;orthoWidth=400;projectedMin=@(400,200);projectedMax=@(880,520);rhinoLoaded=$true;visibleUnitViews=1;otherUnitViewsHidden=$true;hudHidden=$true;modelModified=$false;physicalInputVerified=$false}})
}
function Log($Rows){(@(foreach($r in $Rows){'PROOF_VISUAL_GEOMETRY '+($r|ConvertTo-Json -Depth 8 -Compress);'PROOF_VISUAL_GEOMETRY_RESTORED stage='+$r.stage+' verified=1';'PROOF_VISUAL_GEOMETRY_WRITTEN stage='+($r.stage+1)})-join"`n")}
$script:checks=0
function Check($Pass,$Name){$script:checks++;if(-not$Pass){throw ('Policy check failed: '+$Name)}}
function Reject($Name,[scriptblock]$Mutation){$rows=Fixture;&$Mutation $rows;$rejected=$false;try{Get-VisualGeometryEvidence -Log (Log $rows)|Out-Null}catch{$rejected=$true};Check $rejected $Name}
$valid=Get-VisualGeometryEvidence -Log (Log (Fixture))
Check $valid.passed 'valid measured clipping is evidence, not a required outcome'
Check ($valid.grounding[0].bottomMinusFloorTopCm-eq-35) 'signed contact math'
Check ($valid.motion.Count-eq2-and$valid.motion[0].distanceCm-eq200) 'same-unit measured motion'
$clear=Fixture;foreach($r in $clear){$r.component.worldMin[2]=0;$r.component.worldMax[2]=70}
$unclipped=Get-VisualGeometryEvidence -Log (Log $clear)
Check ($unclipped.passed-and$unclipped.grounding[0].bottomMinusFloorTopCm-eq0) 'no clipping also accepted; no assumed defect'
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
Reject 'zero motion' {param($r)$r[2].component.location=$r[1].component.location;$r[2].modelPosition=$r[1].modelPosition}
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
    @{name='oversized log';text=('x'*(24MB+1))}
)){$rejected=$false;try{Get-VisualGeometryEvidence -Log $case.text|Out-Null}catch{$rejected=$true};Check $rejected $case.name}
"visual geometry policy checks=$script:checks failures=0 (synthetic policy only; no UE/pixel evidence)"
