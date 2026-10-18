# Pure acceptance-parser tests. Synthetic validated-state contexts exercise this
# helper only; the actual runner separately validates raw three-role semantics.
# Never launches UE and never represents engine or physical-input acceptance.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'FactoryProductionEvidence.ps1')
function New-FactoryProductionPolicyFixture {
    $logs=@{host='';rendered='';observer=''};$maps=@{host=@{};rendered=@{};observer=@{}};$common=@()
    $baseIds=@(1,10,20);$events=@();$phases=@()
    for($i=0;$i -lt 4;$i++) {
        $build=$i -lt 2;$before=@(1);if($i -gt 0){$before+=10};if($i -gt 1){$before+=20}
        $e=@{type=$(if($build){'BuildFactory'}else{'SetRally'});owner=1;epoch=1;sequence=$i+1;factory=$(if($i -in @(0,2)){10}else{20});unit=$(if($build){if($i -eq 0){10}else{20}}else{-1});beforeRevision=@(1,5,9,12)[$i];revision=@(3,7,10,13)[$i];beforeDigest=('a'*32);digest=('b'*32);creditsBefore=900;creditsAfter=$(if($build){750}else{900});rallyX=$(if($i -in @(0,2)){-600}else{-950});rallyY=$(if($i -in @(0,2)){-350}else{350});rallyZ=0;spawnX=0;spawnY=0;spawnZ=0;beforeIds=$before;afterIds=$(if($build){@($before)+$(if($i -eq 0){10}else{20})}else{@($before)});eligible=$(if($i -eq 0){@()}elseif($i -eq 1){@(10)}else{@(10,20)})}
        $e.eligible=@();if($i -eq 1){$e.eligible=@(10)}elseif($i -gt 1){$e.eligible=@(10,20)}
        $events+=,$e
    }
    function Add-State([int]$Revision,[int[]]$Ids,[int]$NewId,[double]$X,[int]$Credits) {
        $units=@(@{id=1;owner=1;kind=2;hp=300;x=0;y=0;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0},
            @{id=10;owner=1;kind=4;hp=200;x=0;y=-350;z=0;rally=1;rallyX=-600;rallyY=-350;rallyZ=0},
            @{id=20;owner=1;kind=4;hp=200;x=-350;y=0;z=0;rally=1;rallyX=-950;rallyY=350;rallyZ=0})
        foreach($id in $Ids){if($id -in @(1,10,20)){continue};$units+=@{id=$id;owner=1;kind=0;hp=100;x=$(if($id -eq $NewId){$X}else{0});y=0;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0}}
        $s=@{epoch=1;revision=$Revision;digest=('{0:x32}' -f $Revision);units=$units;players=@(@{id=1;credits=$Credits})}
        $s=$s|ConvertTo-Json -Depth 8 -Compress|ConvertFrom-Json -Depth 8
        $key="1:$Revision";foreach($role in @('host','rendered','observer')){$maps[$role][$key]=$s};$script:policyCommon+=,$key
    }
    $script:policyCommon=@();Add-State 20 $baseIds -1 0 600
    for($case=0;$case -lt 3;$case++) {
        $before=@($baseIds);for($j=0;$j -lt $case;$j++){$before+=100+$j}
        $after=@($before)+(100+$case);$revision=32+$case*30;$beforeRevision=$revision-2;$factory=if($case -eq 1){20}else{10};$set=$events[$(if($case -eq 1){3}else{2})]
        $e=@{type='Produce';owner=1;epoch=1;sequence=5+$case;factory=$factory;unit=100+$case;beforeRevision=$beforeRevision;revision=$revision;beforeDigest=('c'*32);digest=('{0:x32}' -f $revision);creditsBefore=600-$case*50;creditsAfter=550-$case*50;rallyX=$set.rallyX;rallyY=$set.rallyY;rallyZ=0;spawnX=300;spawnY=0;spawnZ=0;beforeIds=$before;afterIds=$after;eligible=@(10,20)}
        $events+=,$e
        Add-State ($beforeRevision-1) $before -1 0 $e.creditsBefore
        Add-State $revision $after $e.unit 300 $e.creditsAfter
        Add-State ($revision+4) $after $e.unit 240 $e.creditsAfter
        Add-State ($revision+8) $after $e.unit 220 $e.creditsAfter
        for($j=0;$j -lt 3;$j++) {
            $request=$j -eq 0
            $phases+=,@{phase=@('requested','spawned','moved')[$j];case=$case;owner=1;epoch=1;revision=@(($beforeRevision-1),$revision,($revision+8))[$j];first=10;second=20;factory=$factory;unit=$(if($request){-1}else{$e.unit});sequence=$e.sequence;requestSelected=$(if($case -eq 0){-1}elseif($case -eq 1){20}else{1});requestCount=$(if($case -eq 0){0}else{1});nativeProduce=0;nativeRight=0;receipt=$(if($request){0}else{1});receiptSequence=$(if($request){0}else{$e.sequence});receiptEpoch=$(if($request){0}else{1});x=@(0,300,220)[$j];y=0;z=0;elapsed=1+$case*5+@(0,1,2.2)[$j]}
        }
    }
    foreach($e in $events){$logs.host+="PROOF_FACTORY_PRODUCTION_EVENT "+($e|ConvertTo-Json -Depth 8 -Compress)+"`n"}
    foreach($p in $phases){$logs.rendered+="PROOF_FACTORY_PRODUCTION_PHASE "+($p|ConvertTo-Json -Compress)+"`n"}
    for($i=0;$i -lt 3;$i++){$logs.rendered+="PROOF_FACTORY_PRODUCTION_CAPTURE name=0$($i+3)-factory-production.png case=$i written=1 physicalInputVerified=0`n"}
    return @{Logs=$logs;Context=@{maps=$maps;common=$script:policyCommon;roles=@('host','rendered','observer')}}
}
function Edit-ProductionRows($F,[string]$Role,[string]$Marker,[scriptblock]$Edit) {
    $F.Logs[$Role]=($F.Logs[$Role] -split '\r?\n'|ForEach-Object{
        if($_.StartsWith($Marker+' ')){$o=$_.Substring($Marker.Length+1)|ConvertFrom-Json -Depth 16;& $Edit $o;$Marker+' '+($o|ConvertTo-Json -Depth 16 -Compress)}else{$_}
    }) -join "`n"
}
$checks=0
function Reject-Production([string]$Name,[scriptblock]$Mutate) {
    $f=New-FactoryProductionPolicyFixture;$before=$f|ConvertTo-Json -Depth 25 -Compress;& $Mutate $f
    if(($f|ConvertTo-Json -Depth 25 -Compress) -ceq $before){throw "No-op mutation: $Name"}
    $rejected=$false;try{Get-FactoryProductionEvidence -Logs $f.Logs -ValidatedStateContext $f.Context|Out-Null}catch{$rejected=$true}
    if(-not $rejected){throw "Accepted invalid production evidence: $Name"};$script:checks++
}
$f=New-FactoryProductionPolicyFixture
$valid=Get-FactoryProductionEvidence -Logs $f.Logs -ValidatedStateContext $f.Context
if(-not $valid.passed -or $valid.cases.Count -ne 3){throw 'Rejected valid three-case synthetic fixture'};$checks++
Reject-Production 'wrong fallback factory' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.sequence -eq 5){$e.factory=20}}}
Reject-Production 'selected second ignored' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.sequence -eq 6){$e.factory=10}}}
Reject-Production 'eligible order reversed' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Produce'){$e.eligible=@(20,10)}}}
Reject-Production 'wrong cost' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)$e.creditsAfter++}}
Reject-Production 'wrong revision' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)$e.revision++}}
Reject-Production 'wrong new unit' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Produce'){$e.unit=999}}}
Reject-Production 'extra new unit' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Produce'){$e.afterIds+=999}}}
Reject-Production 'receipt sequence' {param($f)Edit-ProductionRows $f rendered PROOF_FACTORY_PRODUCTION_PHASE {param($p)if($p.phase -ne 'requested'){$p.receiptSequence++}}}
Reject-Production 'receipt epoch' {param($f)Edit-ProductionRows $f rendered PROOF_FACTORY_PRODUCTION_PHASE {param($p)if($p.phase -ne 'requested'){$p.receiptEpoch++}}}
Reject-Production 'receipt rejected' {param($f)Edit-ProductionRows $f rendered PROOF_FACTORY_PRODUCTION_PHASE {param($p)$p.receipt=0}}
Reject-Production 'negative time' {param($f)Edit-ProductionRows $f rendered PROOF_FACTORY_PRODUCTION_PHASE {param($p)if($p.case -eq 0 -and $p.phase -eq 'requested'){$p.elapsed=-.5}}}
Reject-Production 'changed native counter' {param($f)Edit-ProductionRows $f rendered PROOF_FACTORY_PRODUCTION_PHASE {param($p)if($p.case -eq 2){$p.nativeProduce=1}}}
Reject-Production 'wrong empty selection' {param($f)Edit-ProductionRows $f rendered PROOF_FACTORY_PRODUCTION_PHASE {param($p)if($p.case -eq 0){$p.requestCount=1;$p.requestSelected=10}}}
Reject-Production 'wrong second selection' {param($f)Edit-ProductionRows $f rendered PROOF_FACTORY_PRODUCTION_PHASE {param($p)if($p.case -eq 1){$p.requestSelected=10}}}
Reject-Production 'fake local movement' {param($f)Edit-ProductionRows $f rendered PROOF_FACTORY_PRODUCTION_PHASE {param($p)if($p.phase -eq 'moved'){$p.x--}}}
Reject-Production 'wrong authoritative spawn' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Produce'){$e.spawnX=9000}}}
Reject-Production 'missing common movement' {param($f)$f.Context.common=@($f.Context.common|Where-Object{$_ -notin @('1:36','1:40')})}
Reject-Production 'missing common pre-rally' {param($f)$f.Context.common=@($f.Context.common|Where-Object{[int]($_.Split(':')[1]) -gt 32})}
Reject-Production 'missing observer map' {param($f)$f.Context.maps.Remove('observer')}
Reject-Production 'missing observer log' {param($f)$f.Logs.Remove('observer')}
Reject-Production 'invalid log type' {param($f)$f.Logs.observer=@('text')}
Reject-Production 'event in rendered' {param($f)$f.Logs.rendered+='PROOF_FACTORY_PRODUCTION_EVENT {}'}
Reject-Production 'phase in observer' {param($f)$f.Logs.observer+='PROOF_FACTORY_PRODUCTION_PHASE {}'}
Reject-Production 'extra malformed capture' {param($f)$f.Logs.rendered+='PROOF_FACTORY_PRODUCTION_CAPTURE malformed'}
Reject-Production 'trailing capture junk' {param($f)$f.Logs.rendered=$f.Logs.rendered.Replace('physicalInputVerified=0','physicalInputVerified=0 junk')}
Reject-Production 'missing capture' {param($f)$f.Logs.rendered=$f.Logs.rendered -replace '(?m)^PROOF_FACTORY_PRODUCTION_CAPTURE[^\n]*case=1[^\n]*\n?',''}
Reject-Production 'duplicate event key' {param($f)$f.Logs.host=$f.Logs.host.Replace('"type":"Produce"','"type":"Produce","type":"Produce"')}
Reject-Production 'event boolean owner' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)$e.owner=$true}}
Reject-Production 'event string sequence' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)$e.sequence=[string]$e.sequence}}
Write-Output "Factory production evidence policy: $checks checks passed (synthetic parser fixtures only)."