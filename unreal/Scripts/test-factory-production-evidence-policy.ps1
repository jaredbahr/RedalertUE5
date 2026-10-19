# Pure acceptance-parser tests. Synthetic validated-state contexts exercise this
# helper only; the actual runner separately validates raw three-role semantics.
# Never launches UE and never represents engine or physical-input acceptance.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RefineryMinerEvidence.ps1')
. (Join-Path $PSScriptRoot 'FactoryProductionEvidence.ps1')
function New-FactoryProductionPolicyFixture {
    $logs=@{host='';rendered='';observer=''};$maps=@{host=@{};rendered=@{};observer=@{}};$events=@();$phases=@();$advances=@()
    function Copy($O){return ($O|ConvertTo-Json -Depth 16 -Compress|ConvertFrom-Json -Depth 16)}
    function Digest([int]$R){return ('{0:x32}' -f $R)}
    function Add-State([int]$Revision,$Context){
        $s=Copy $Context;$s|Add-Member -NotePropertyName epoch -NotePropertyValue 1;$s|Add-Member -NotePropertyName revision -NotePropertyValue $Revision;$s|Add-Member -NotePropertyName digest -NotePropertyValue (Digest $Revision)
        $key="1:$Revision";foreach($role in @('host','rendered','observer')){$maps[$role][$key]=Copy $s};$script:policyCommon+=,$key
    }
    $current=Copy @{phase=1;jobs=@();players=@(@{id=1;credits=900;gathered=0});units=@(
        @{id=1;owner=1;kind=2;hp=300;cargo=0;x=0;y=0;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0})}
    $script:policyCommon=@()
    for($i=0;$i -lt 4;$i++) {
        $build=$i -lt 2;$before=Copy $current;$factory=if($i -in @(0,2)){10}else{20}
        $beforeRevision=@(1,5,9,12)[$i];$revision=@(3,7,10,13)[$i]
        if($build){
            $spawnX=if($i -eq 0){0}else{-350};$spawnY=if($i -eq 0){-350}else{0}
            $current.units+=,[pscustomobject]@{id=$factory;owner=1;kind=4;hp=200;cargo=0;x=$spawnX;y=$spawnY;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0};$current.players[0].credits-=150
            $rallyX=$spawnX;$rallyY=$spawnY
        }else{
            $spawnX=0;$spawnY=0;$rallyX=if($i -eq 2){-600}else{-950};$rallyY=if($i -eq 2){-350}else{350}
            $changed=@($current.units|Where-Object id -eq $factory)[0];$changed.rally=1;$changed.rallyX=$rallyX;$changed.rallyY=$rallyY
        }
        $e=@{type=$(if($build){'BuildFactory'}else{'SetRally'});owner=1;epoch=1;sequence=$i+1;factory=$factory;unit=$(if($build){$factory}else{-1});beforeRevision=$beforeRevision;revision=$revision;beforeDigest=(Digest $beforeRevision);digest=(Digest $revision);creditsBefore=$before.players[0].credits;creditsAfter=$current.players[0].credits;rallyX=$rallyX;rallyY=$rallyY;rallyZ=0;spawnX=$spawnX;spawnY=$spawnY;spawnZ=0;beforeIds=@($before.units.id);afterIds=@($current.units.id);eligible=@($before.units|Where-Object{$_.owner -eq 1 -and $_.kind -eq 4 -and $_.hp -gt 0}|ForEach-Object{$_.id});productionBefore=$before;productionAfter=(Copy $current);productionPayment=0;productionRefund=0}
        $events+=,$e;Add-State $beforeRevision $before;Add-State $revision $current
    }
    Add-State 20 $current
    for($case=0;$case -lt 3;$case++) {
        $startRev=31+$case*80;$factory=if($case -eq 1){20}else{10};$set=$events[$(if($case -eq 1){3}else{2})];$seq=5+$case;$unit=100+$case
        $start=@{type='Produce';owner=1;epoch=1;sequence=$seq;factory=$factory;unit=-1;beforeRevision=$startRev-1;revision=$startRev;beforeDigest=(Digest ($startRev-1));digest=(Digest $startRev);creditsBefore=$current.players[0].credits;creditsAfter=$current.players[0].credits;rallyX=$set.rallyX;rallyY=$set.rallyY;rallyZ=0;spawnX=0;spawnY=0;spawnZ=0;beforeIds=@($current.units.id);afterIds=@($current.units.id);eligible=@(10,20);productionBefore=(Copy $current);productionPayment=0;productionRefund=0}
        Add-State ($startRev-2) $current
        $current.jobs=@([pscustomobject]@{owner=1;sequence=$seq;factory=$factory;work=0;paid=0;held=0});$start.productionAfter=Copy $current;$events+=,$start
        Add-State $startRev $current;$revision=$startRev
        $spawnX=@(300,-650,-300)[$case];$spawnY=@(-350,0,-350)[$case]
        for($step=1;$step -le 50;$step++) {
            $before=Copy $current;$beforeRev=$revision;$current.players[0].credits--
            if($step -lt 50){$current.jobs[0].work=$step*100000;$current.jobs[0].paid=$step;$revision++}
            else{$current.jobs=@();$current.units+=,[pscustomobject]@{id=$unit;owner=1;kind=0;hp=100;cargo=0;x=$spawnX;y=$spawnY;z=0;rally=0;rallyX=0;rallyY=0;rallyZ=0};$revision+=2}
            $advance=@{index=$step;owner=1;epoch=1;sequence=$seq;factory=$factory;beforeRevision=$beforeRev;revision=$revision;beforeDigest=(Digest $beforeRev);digest=(Digest $revision);productionBefore=$before;productionAfter=(Copy $current);productionPayment=1;productionRefund=0}
            $advances+=,$advance
            if($step -in @(10,25,50)){Add-State $revision $current}
        }
        $complete=@{type='Complete';advanceCount=50;owner=1;epoch=1;sequence=$seq;factory=$factory;unit=$unit;beforeRevision=$advance.beforeRevision;revision=$revision;beforeDigest=$advance.beforeDigest;digest=$advance.digest;creditsBefore=$advance.productionBefore.players[0].credits;creditsAfter=$current.players[0].credits;rallyX=$set.rallyX;rallyY=$set.rallyY;rallyZ=0;spawnX=$spawnX;spawnY=$spawnY;spawnZ=0;beforeIds=@($advance.productionBefore.units.id);afterIds=@($current.units.id);eligible=@(10,20);productionBefore=$advance.productionBefore;productionAfter=$advance.productionAfter;productionPayment=1;productionRefund=0};$events+=,$complete
        $current.units[-1].x=$spawnX-60;Add-State ($revision+4) $current
        $current.units[-1].x=$spawnX-80;Add-State ($revision+8) $current
        for($j=0;$j -lt 3;$j++) {
            $request=$j -eq 0
            $phases+=,@{phase=@('requested','spawned','moved')[$j];case=$case;owner=1;epoch=1;revision=@(($startRev-2),$revision,($revision+8))[$j];first=10;second=20;factory=$factory;unit=$(if($request){-1}else{$unit});sequence=$seq;requestSelected=$(if($case -eq 0){-1}elseif($case -eq 1){20}else{1});requestCount=$(if($case -eq 0){0}else{1});nativeProduce=0;nativeRight=0;receipt=$(if($request){0}else{1});receiptSequence=$(if($request){0}else{$seq});receiptEpoch=$(if($request){0}else{1});x=@(0,$spawnX,($spawnX-80))[$j];y=$(if($request){0}else{$spawnY});z=0;elapsed=1+$case*10+@(0,5,6.2)[$j]}
        }
    }
    foreach($e in $events){
        if($e.type -eq 'Complete'){foreach($a in @($advances|Where-Object sequence -eq $e.sequence)){$logs.host+="PROOF_FACTORY_PRODUCTION_ADVANCE "+($a|ConvertTo-Json -Depth 16 -Compress)+"`n"}}
        $logs.host+="PROOF_FACTORY_PRODUCTION_EVENT "+($e|ConvertTo-Json -Depth 16 -Compress)+"`n"
    }
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
Reject-Production 'wrong authoritative spawn' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete'){$e.spawnX=9000}}}
Reject-Production 'missing common movement' {param($f)$f.Context.common=@($f.Context.common|Where-Object{$_ -notin @('1:86','1:90')})}
Reject-Production 'missing common pre-rally' {param($f)$f.Context.common=@($f.Context.common|Where-Object{[int]($_.Split(':')[1]) -gt 31})}
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

Reject-Production 'completion sequence spliced' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete' -and $e.sequence -eq 5){$e.sequence=6}}}
Reject-Production 'completion epoch spliced' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete'){$e.epoch=2}}}
Reject-Production 'completion factory spliced' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete' -and $e.sequence -eq 5){$e.factory=20}}}
Reject-Production 'completion refunds spent credits' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete'){$e.productionRefund=50}}}
Reject-Production 'completion payment numeric string' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete'){$e.productionPayment='1'}}}
Reject-Production 'start prematurely paid' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Produce'){$e.productionAfter.jobs[0].paid=1;$e.productionAfter.jobs[0].work=100000}}}
Reject-Production 'start is held' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Produce'){$e.productionAfter.jobs[0].held=1}}}
Reject-Production 'start spends credits' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Produce'){$e.productionAfter.players[0].credits--}}}
Reject-Production 'start phantom retained job' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Produce'){$e.productionBefore.jobs=$e.productionAfter.jobs}}}
Reject-Production 'Advance paid progress inflated' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.revision -eq 32){$a.productionAfter.jobs[0].work=200000;$a.productionAfter.jobs[0].paid=2}}}
Reject-Production 'Advance underpayment' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.revision -eq 32){$a.productionPayment=0}}}
Reject-Production 'Advance refunds while progressing' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.revision -eq 32){$a.productionRefund=1}}}
Reject-Production 'Advance introduces hold' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.revision -eq 32){$a.productionAfter.jobs[0].held=1}}}
Reject-Production 'Advance changed start sequence' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.revision -eq 32){$a.productionAfter.jobs[0].sequence=6}}}
Reject-Production 'Advance cross epoch' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.revision -eq 32){$a.epoch=2}}}
Reject-Production 'Advance missing jobs schema' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.revision -eq 32){$a.productionBefore.PSObject.Properties.Remove('jobs')}}}
Reject-Production 'Advance duplicate key' {param($f)$f.Logs.host=$f.Logs.host.Replace('"productionPayment":1','"productionPayment":1,"productionPayment":1')}
Reject-Production 'missing paid Advance' {param($f)$f.Logs.host=$f.Logs.host -replace '(?m)^PROOF_FACTORY_PRODUCTION_ADVANCE[^\n]*"revision":32[,}][^\n]*\n?',''}
Reject-Production 'duplicate Advance' {param($f)$f.Logs.host+="`n"+($f.Logs.host -split '\n'|Where-Object{$_ -like 'PROOF_FACTORY_PRODUCTION_ADVANCE *'}|Select-Object -First 1)}
Reject-Production 'Advance outside host' {param($f)$f.Logs.observer+='PROOF_FACTORY_PRODUCTION_ADVANCE {}'}
Reject-Production 'missing common job progress' {param($f)$f.Context.common=@($f.Context.common|Where-Object{$_ -notin @('1:41','1:56')})}
Reject-Production 'observer job differs with same digest' {param($f)$f.Context.maps.observer['1:41'].jobs[0].paid=11;$f.Context.maps.observer['1:41'].jobs[0].work=1100000}
Reject-Production 'extra completion unit' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete'){$e.afterIds+=999}}}
Reject-Production 'duplicate completion event' {param($f)$f.Logs.host+="`n"+($f.Logs.host -split '\n'|Where-Object{$_ -like 'PROOF_FACTORY_PRODUCTION_EVENT *' -and $_ -match '"type":"Complete"'}|Select-Object -First 1)}
Reject-Production 'missing observer common job row' {param($f)$f.Context.maps.observer.Remove('1:41')}
Reject-Production 'oversized unchanged phase' {param($f)Edit-ProductionRows $f rendered PROOF_FACTORY_PRODUCTION_PHASE {param($p)$p|Add-Member -NotePropertyName padding -NotePropertyValue ('x'*4096)}}

Reject-Production 'Advance index duplicated without duplicate row' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.sequence -eq 5 -and $a.index -eq 2){$a.index=1}}}
Reject-Production 'Advance index string' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.sequence -eq 5 -and $a.index -eq 1){$a.index='1'}}}
Reject-Production 'Advance index missing' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_ADVANCE {param($a)if($a.sequence -eq 5 -and $a.index -eq 1){$a.PSObject.Properties.Remove('index')}}}
Reject-Production 'completion false Advance count' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete'){$e.advanceCount++}}}
Reject-Production 'completion missing Advance count' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete'){$e.PSObject.Properties.Remove('advanceCount')}}}
Reject-Production 'completion Advance count string' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'Complete'){$e.advanceCount='50'}}}
Reject-Production 'raw Advance rows reordered' {param($f)
    $lines=@($f.Logs.host -split '\r?\n');$positions=@(0..($lines.Count-1)|Where-Object{$lines[$_].StartsWith('PROOF_FACTORY_PRODUCTION_ADVANCE ')})
    $saved=$lines[$positions[0]];$lines[$positions[0]]=$lines[$positions[1]];$lines[$positions[1]]=$saved;$f.Logs.host=$lines -join "`n"
}
Reject-Production 'last Advance occurs after Complete' {param($f)
    $lines=@($f.Logs.host -split '\r?\n');$position=@(0..($lines.Count-1)|Where-Object{$lines[$_].StartsWith('PROOF_FACTORY_PRODUCTION_ADVANCE ') -and $lines[$_] -match '"index":50[,}]'})[0]
    $saved=$lines[$position];$f.Logs.host=(@(for($i=0;$i -lt $lines.Count;$i++){if($i -ne $position){$lines[$i]}})+$saved) -join "`n"
}
Reject-Production 'first Advance occurs before Produce' {param($f)
    $lines=@($f.Logs.host -split '\r?\n');$position=@(0..($lines.Count-1)|Where-Object{$lines[$_].StartsWith('PROOF_FACTORY_PRODUCTION_ADVANCE ')})[0]
    $saved=$lines[$position];$f.Logs.host=(@($saved)+@(for($i=0;$i -lt $lines.Count;$i++){if($i -ne $position){$lines[$i]}})) -join "`n"
}

Reject-Production 'build missing before context' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'BuildFactory'){$e.PSObject.Properties.Remove('productionBefore')}}}
Reject-Production 'build missing after context' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'BuildFactory'){$e.PSObject.Properties.Remove('productionAfter')}}}
Reject-Production 'rally missing before context' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'SetRally'){$e.PSObject.Properties.Remove('productionBefore')}}}
Reject-Production 'rally missing after context' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'SetRally'){$e.PSObject.Properties.Remove('productionAfter')}}}
Reject-Production 'build falsely claims production refund' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'BuildFactory'){$e.productionRefund=1}}}
Reject-Production 'rally falsely claims production debit' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'SetRally'){$e.productionPayment=1}}}
Reject-Production 'build missing zero payment' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'BuildFactory'){$e.PSObject.Properties.Remove('productionPayment')}}}
Reject-Production 'rally boolean zero refund' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'SetRally'){$e.productionRefund=$false}}}
Reject-Production 'build context credits disagree' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'BuildFactory'){$e.productionAfter.players[0].credits++}}}
Reject-Production 'rally context IDs disagree' {param($f)Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.type -eq 'SetRally'){$e.productionAfter.units[0].id=999}}}
Reject-Production 'build self-consistent unrelated HQ damage' {param($f)
    Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.sequence -eq 1){($e.productionAfter.units|Where-Object id -eq 1).hp=299}}
    foreach($role in @('host','rendered','observer')){($f.Context.maps[$role]['1:3'].units|Where-Object id -eq 1).hp=299}
}
Reject-Production 'rally self-consistent unrelated factory movement' {param($f)
    Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.sequence -eq 3){($e.productionAfter.units|Where-Object id -eq 20).x=-349}}
    foreach($role in @('host','rendered','observer')){($f.Context.maps[$role]['1:10'].units|Where-Object id -eq 20).x=-349}
}
Reject-Production 'build self-consistent unexpected production job' {param($f)
    Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.sequence -eq 1){$e.productionAfter.jobs=@([pscustomobject]@{owner=1;sequence=99;factory=10;work=0;paid=0;held=0})}}
    foreach($role in @('host','rendered','observer')){$f.Context.maps[$role]['1:3'].jobs=@([pscustomobject]@{owner=1;sequence=99;factory=10;work=0;paid=0;held=0})}
}
Reject-Production 'build self-consistent wrong new factory health' {param($f)
    Edit-ProductionRows $f host PROOF_FACTORY_PRODUCTION_EVENT {param($e)if($e.sequence -eq 1){($e.productionAfter.units|Where-Object id -eq 10).hp=201}}
    foreach($role in @('host','rendered','observer')){($f.Context.maps[$role]['1:3'].units|Where-Object id -eq 10).hp=201}
}
Reject-Production 'build published context mismatches only observer' {param($f)($f.Context.maps.observer['1:3'].units|Where-Object id -eq 1).hp=299}
Write-Output "Factory production evidence policy: $checks checks passed (synthetic parser fixtures only)."