# Synthetic offline parser checks only. No UE, transport or physical input proof.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'RefineryMinerEvidence.ps1')
. (Join-Path $PSScriptRoot 'AttackMoveEvidence.ps1')
function Copy-AM($Value){return ,(ConvertFrom-Json -InputObject (ConvertTo-Json -InputObject $Value -Depth 12) -Depth 12 -NoEnumerate)}
function New-AMContext($States,[int]$Revision,$IDs,[int]$Owner=-1,[int]$Credits=-1) {
    $state=@($States|Where-Object revision -le $Revision|Sort-Object revision|Select-Object -Last 1)
    if($state.Count -eq 0){$state=@($States|Sort-Object revision|Select-Object -First 1)}
    $units=@()
    foreach($id in $IDs){
        $unit=@($state[0].units|Where-Object id -eq $id)
        if($unit.Count -eq 0){$unit=@($States|ForEach-Object{$_.units}|Where-Object id -eq $id|Select-Object -First 1)}
        $units+=,(Copy-AM $unit[0])
    }
    $players=@($state[0].players|ForEach-Object{[pscustomobject]@{id=$_.id;credits=$_.credits;gathered=$_.gathered}})
    if($Owner -gt 0){($players|Where-Object id -eq $Owner).credits=$Credits}
    $jobs=@()
    if($Revision -ge 58 -and $Revision -le 108){$paid=$Revision-58;$jobs=@([pscustomobject][ordered]@{owner=1;sequence=5;factory=13;work=100000*$paid;paid=$paid;held=0})}
    return [pscustomobject][ordered]@{phase=1;jobs=$jobs;units=$units;players=$players}
}
function New-AMUnit([int]$Id,[int]$Owner,[int]$Kind,[double]$X,[double]$Y,[double]$HP=100,[int]$Cargo=0) { return [ordered]@{id=$Id;owner=$Owner;kind=$Kind;x=$X;y=$Y;z=0;hp=$HP;cargo=$Cargo;rally=0;rallyX=0;rallyY=0;rallyZ=0} }
function New-SyntheticAttackMoveLogs {
    $owners=@(100,1,2);$ref=@{100=7;1=9;2=11};$miner=@{100=8;1=10;2=12}
    $refX=@{100=1800;1=-1050;2=-1050};$ys=@{100=0;1=-400;2=600};$spawnX=@{100=2100;1=-750;2=-750};$oreX=@{100=1950;1=-850;2=-850}
    $logs=@{host='';rendered='';observer=''};$states=@()
    foreach($rev in @(24,32,52,60,200,202)+@(208..276|Where-Object{$_%2 -eq 0})) {
        $s=[ordered]@{epoch=1;revision=$rev;digest=('{0:x32}' -f $rev);phase=1;winner=-1;units=@();players=@();resources=@();jobs=@()}
        $s.units+=New-AMUnit 1 1 2 -1400 -400 300;$s.units+=New-AMUnit 2 1 1 -1300 -400
        $s.units+=New-AMUnit 3 2 2 -1400 600 300;$s.units+=New-AMUnit 4 2 1 -1300 600
        $s.units+=New-AMUnit 5 100 2 1400 0 300
        $targetHP=if($rev -lt 214){100}elseif($rev -lt 252){95-2.5*($rev-214)}else{0}
        $s.units+=New-AMUnit 6 100 1 $(if($rev -lt 200){1500}else{-150}) $(if($rev -lt 200){0}else{-700}) $targetHP
        foreach($o in $owners) {
            $cargo=if($rev -eq 32){50}else{0};$x=if($rev -eq 24){$spawnX[$o]}elseif($rev -eq 32){$oreX[$o]}else{$refX[$o]}
            $s.units+=New-AMUnit $ref[$o] $o 3 $refX[$o] $ys[$o] 200
            $s.units+=New-AMUnit $miner[$o] $o 1 $x $ys[$o] 100 $cargo
            $credits=if($rev -lt 52){250}elseif($o -eq 1 -and $rev -eq 60){148}elseif($o -eq 1 -and $rev -ge 200){100}else{300}
            $s.players+=@{id=$o;team=$(if($o -eq 100){1}else{0});credits=$credits;gathered=$(if($rev -ge 52){50}else{0})}
            $s.resources+=@{id=$o;owner=$o;x=$oreX[$o];y=$ys[$o];z=0;remaining=$(if($rev -ge 32){450}else{500})}
        }
        if($rev -ge 60) {
            $s.units+=New-AMUnit 13 1 4 -900 -700 200
            $x=if($rev -le 202){-600}elseif($rev -lt 214){-600+15*($rev-203)}elseif($rev -le 252){-450}elseif($rev -le 258){-450+15*($rev-252)}else{-360}
            if($rev -ge 200){$s.units+=New-AMUnit 14 1 0 $x -700}
            else{$s.jobs=@([ordered]@{owner=1;sequence=5;factory=13;work=200000;paid=2;held=0})}
        }
        $states+=,$s
    }
    # One actual Advance deposits for all three owners. Every observation of
    # revision 49/50 must carry the same complete world, including other owners.
    $depositBefore=New-AMContext $states 52 @(1..12)
    foreach($owner in $owners){
        ($depositBefore.players|Where-Object id -eq $owner).credits=250
        ($depositBefore.players|Where-Object id -eq $owner).gathered=0
        ($depositBefore.units|Where-Object id -eq $miner[$owner]).cargo=50
    }
    $depositAfter=Copy-AM $depositBefore
    foreach($owner in $owners){
        ($depositAfter.players|Where-Object id -eq $owner).credits=300
        ($depositAfter.players|Where-Object id -eq $owner).gathered=50
        ($depositAfter.units|Where-Object id -eq $miner[$owner]).cargo=0
    }
    $beforeIds=@(1..6);$beforeRev=10
    foreach($o in $owners) {
        $g=@{event='grant';epoch=1;owner=$o;sequence=1;refinery=$ref[$o];miner=$miner[$o];beforeRevision=$beforeRev;revision=$beforeRev+3;beforeDigest=('{0:x32}' -f $beforeRev);digest=('{0:x32}' -f ($beforeRev+3));added=2;refineriesAdded=1;minersAdded=1;creditsBefore=350;creditsAfter=250;cost=100;x=$spawnX[$o];y=$ys[$o];z=0;rx=$refX[$o];ry=$ys[$o];rz=0;beforeIds=@($beforeIds);afterIds=@($beforeIds)+@($ref[$o],$miner[$o])}
        $beforeIds=@($g.afterIds);$beforeRev+=3
        $events=@($g,@{event='gather';epoch=1;owner=$o;sequence=2;refinery=$ref[$o];miner=$miner[$o];resource=$o;revision=$(if($o -eq 100){20}elseif($o -eq 1){22}else{23})},@{event='load';epoch=1;owner=$o;refinery=$ref[$o];miner=$miner[$o];resource=$o;revision=30;cargo=50;oreBefore=500;oreAfter=450},@{event='deposit';epoch=1;owner=$o;refinery=$ref[$o];depot=$ref[$o];miner=$miner[$o];beforeRevision=49;revision=50;cargoBefore=50;cargoAfter=0;depositSum=50;creditsBefore=250;creditsAfter=300;gatheredBefore=0;gatheredAfter=50;contributors=@(@{miner=$miner[$o];before=50;after=0;depot=$ref[$o]})})
        foreach($e in $events){
            if($e.event -eq 'deposit'){
                $e.productionPayment=0;$e.productionRefund=0
                $e.productionBefore=Copy-AM $depositBefore
                $e.productionAfter=Copy-AM $depositAfter
            }
            $logs.host+="PROOF_REFINERY_EVENT "+($e|ConvertTo-Json -Depth 12 -Compress)+"`n"}
    }
    foreach($role in @('host','rendered','observer')) {
        foreach($s in $states) {
            $row=[ordered]@{role=$(if($role -eq 'host'){'host'}else{'client'});viewer=$(if($role -eq 'host'){0}elseif($role -eq 'rendered'){1}else{2})}
            foreach($k in $s.Keys){$row[$k]=$s[$k]}
            $logs[$role]+="PROOF_SNAPSHOT role=$($row.role) owner=$($row.viewer) epoch=1 revision=$($s.revision) digest=$($s.digest)`nPROOF_REFINERY_STATE "+($row|ConvertTo-Json -Depth 12 -Compress)+"`n"
            if($role -eq 'host') {
                $time=if($s.revision -le 52){.1*($s.revision-23)}elseif($s.revision -eq 60){3.2}elseif($s.revision -le 202){17.0+.1*($s.revision-200)}elseif($s.revision -le 258){17.2+.1*($s.revision-203)}else{22.7+.1*($s.revision-259)}
                $logs.host+='PROOF_ATTACK_MOVE_CLOCK '+(@{epoch=1;revision=$s.revision;digest=$s.digest;simulatedSeconds=$time}|ConvertTo-Json -Compress)+"`n"
            }
        }
    }
    $commands=@()
    foreach($o in @(1,2,100)) {
        $types=if($o -eq 1){@(3,5,6,4,2,8,6)}elseif($o -eq 2){@(3,5,6)}else{@(3,5,0,6)}
        for($i=0;$i -lt $types.Count;$i++) {
            $seq=$i+1;$type=$types[$i]
            $before=if($o -eq 100){@(10,19,20,52)[$i]}elseif($o -eq 1){@(13,21,53,55,57,202,258)[$i]}else{@(16,22,54)[$i]}
            $rev=$before+$(if($type -eq 3){3}elseif($type -eq 4){2}else{1})
            $unit=if($type -in @(3,4)){-1}elseif($type -eq 2){13}elseif($type -eq 8 -or ($o -eq 1 -and $seq -eq 7)){14}elseif($type -eq 0){6}else{$miner[$o]}
            $ids=if($before -lt 13){@(1..6)}elseif($before -lt 16){@(1..8)}elseif($before -lt 19){@(1..10)}elseif($before -lt 57){@(1..12)}elseif($before -lt 59){@(1..13)}else{@(1..14)}
            $afterIds=if($type -eq 3){@($ids)+@($ref[$o],$miner[$o])}elseif($type -eq 4){@($ids)+13}elseif($type -eq 2){@($ids)}else{@($ids)}
            $credits=if($type -eq 3){350}elseif($before -lt 52){250}elseif($o -eq 1 -and $before -ge 59){100}elseif($type -eq 2){150}else{300}
            $kind=if($unit -eq -1){-1}elseif($unit -eq 13){4}elseif($unit -eq 14){0}else{1}
            $x=if($unit -eq 14){$(if($seq -eq 7){-360}else{-600})}elseif($unit -eq 13){-900}elseif($unit -eq 6){1500}else{$spawnX[$o]}
            $y=if($unit -in @(13,14)){-700}elseif($unit -eq 6){0}else{$ys[$o]}
            $dest=if($type -eq 8){@(600,-700,0)}elseif($type -eq 0){@(-150,-700,0)}elseif($type -eq 4){@(-900,-700,0)}elseif($type -eq 3){@($refX[$o],$ys[$o],0)}else{@(0,0,0)}
            $commands+=,@{owner=$o;epoch=1;sequence=$seq;type=$type;unit=$unit;target=$(if($type -eq 5){$o}else{-1});accepted=1;reason='accepted';beforeRevision=$before;revision=$rev;beforeDigest=('{0:x32}' -f $before);digest=('{0:x32}' -f $rev);destinationX=$dest[0];destinationY=$dest[1];destinationZ=$dest[2];creditsBefore=$credits;creditsAfter=$credits-$(if($type -eq 3){100}elseif($type -eq 4){150}else{0});kind=$kind;sourceOwner=$(if($kind -lt 0){-1}else{$o});x=$x;y=$y;z=0;hp=$(if($kind -eq 4){200}elseif($kind -ge 0){100}else{0});visibleUnchanged=$(if($type -in @(6,8)){1}else{0});beforeIds=@($ids);afterIds=@($afterIds)}
        }
    }
    foreach($c in $commands){
        $c.productionBefore=New-AMContext $states $c.beforeRevision $c.beforeIds $c.owner $c.creditsBefore
        $c.productionAfter=New-AMContext $states $c.revision $c.afterIds $c.owner $c.creditsAfter
    }
    $productionRows=@()
    $productionContext=Copy-AM ($commands|Where-Object{$_.owner -eq 1 -and $_.type -eq 2}).productionAfter
    for($i=1;$i -le 50;$i++){
        $before=Copy-AM $productionContext;$productionContext.jobs[0].work=$i*100000;$productionContext.jobs[0].paid=$i
        ($productionContext.players|Where-Object id -eq 1).credits--
        $revision=58+$i
        if($i -eq 50){$revision++;$productionContext.jobs=@();$productionContext.units+=,(New-AMUnit 14 1 0 -600 -700)}
        $row=@{owner=1;epoch=1;beforeRevision=57+$i;revision=$revision;beforeDigest=('{0:x32}' -f (57+$i));digest=('{0:x32}' -f $revision);simulatedSeconds=3.0+.1*$i;productionPayment=1;productionRefund=0;productionBefore=$before;productionAfter=(Copy-AM $productionContext)}
        $productionRows+=,$row
    }
    $stream=@($commands|ForEach-Object{[pscustomobject]@{marker='COMMAND';row=$_;revision=$_.revision}})+@($productionRows|ForEach-Object{[pscustomobject]@{marker='PRODUCTION';row=$_;revision=$_.revision}})
    foreach($e in ($stream|Sort-Object revision)){$logs.host+='PROOF_ATTACK_MOVE_'+$e.marker+' '+($e.row|ConvertTo-Json -Depth 12 -Compress)+"`n"}
    foreach($p in @(@('moving',208,6),@('firing',228,6),@('resumed',256,6),@('stopped',276,7))) {
        $s=$states|Where-Object revision -eq $p[1];$u=$s.units|Where-Object id -eq 14;$t=$s.units|Where-Object id -eq 6
        $row=@{phase=$p[0];epoch=1;revision=$s.revision;digest=$s.digest;unit=14;target=6;x=$u.x;y=$u.y;z=0;targetHP=$t.hp;elapsed=1+$s.revision/10.0;sequence=$p[2];selected=14;queue=0;nativeRight=0;nativeStop=0}
        $logs.rendered+='PROOF_ATTACK_MOVE_PHASE '+($row|ConvertTo-Json -Compress)+"`n"
    }
    $logs.rendered+="PROOF_SKIRMISH_REQUEST owner=1 sequence=6 type=AttackMove unit=14 target=-1`nPROOF_RESULT owner=1 sequence=6 accepted=1`nPROOF_SKIRMISH_REQUEST owner=1 sequence=7 type=Stop unit=14 target=-1`nPROOF_RESULT owner=1 sequence=7 accepted=1`n"
    foreach($role in @('host','rendered','observer')) {
        $identity=if($role -eq 'observer'){'owner=2'}else{'unit=14 target=6'}
        $logs[$role]+="PROOF_ATTACK_MOVE_READY role=$role status=pass $identity input=synthetic physicalInput=unverified`nPROOF_ATTACK_MOVE_COMPLETE role=$role status=pass input=synthetic physicalInput=unverified`n"
    }
    return $logs
}
function Edit-AMRows($Logs,[string]$Role,[string]$Marker,[scriptblock]$Edit) {
    $Logs[$Role]=($Logs[$Role] -split '\r?\n'|ForEach-Object{if($_.StartsWith($Marker+' ')){$o=$_.Substring($Marker.Length+1)|ConvertFrom-Json -Depth 12;&$Edit $o;$Marker+' '+($o|ConvertTo-Json -Depth 12 -Compress)}else{$_}})-join "`n"
}
$amChecks=0
function Reject-AM([string]$Name,[scriptblock]$Mutate) {
    $logs=New-SyntheticAttackMoveLogs;$before=$logs|ConvertTo-Json -Depth 3 -Compress;&$Mutate $logs
    if(($logs|ConvertTo-Json -Depth 3 -Compress) -ceq $before){throw "No-op synthetic mutation: $Name"}
    $rejected=$false;try{Get-AttackMoveEvidence $logs -RequireComplete|Out-Null}catch{$rejected=$true}
    if(-not $rejected){throw "Accepted invalid synthetic Attack Move: $Name"};$script:amChecks++
}
$valid=Get-AttackMoveEvidence (New-SyntheticAttackMoveLogs) -RequireComplete
if(-not $valid.passed -or $valid.firingHostSeconds -lt 1){throw 'Valid synthetic Attack Move rejected'};$amChecks++
$logs=New-SyntheticAttackMoveLogs;foreach($r in @('host','rendered','observer')){$logs[$r]=$logs[$r] -replace '(?m)^PROOF_ATTACK_MOVE_COMPLETE[^\n]*\n?',''}
if(-not (Get-AttackMoveEvidence $logs).passed){throw 'Pre-release policy rejected'};$amChecks++
Reject-AM 'missing observer' {param($l)$l.Remove('observer')}
Reject-AM 'missing owner2 economy' {param($l)$l.host=$l.host -replace '(?m)^PROOF_REFINERY_EVENT[^\n]*"owner":2[^\n]*\n?',''}
Reject-AM 'missing ready' {param($l)$l.observer=$l.observer -replace '(?m)^PROOF_ATTACK_MOVE_READY[^\n]*\n?',''}
Reject-AM 'missing completion' {param($l)$l.host=$l.host -replace '(?m)^PROOF_ATTACK_MOVE_COMPLETE[^\n]*\n?',''}
Reject-AM 'runtime failure' {param($l)$l.rendered+="`nPROOF_ATTACK_MOVE_ERROR role=rendered reason=failed"}
Reject-AM 'duplicate command' {param($l)$l.host+="`n"+($l.host -split '\n'|Where-Object{$_ -like 'PROOF_ATTACK_MOVE_COMMAND *'}|Select-Object -Last 1)}
Reject-AM 'foreign AM source' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.sourceOwner=2}}}
Reject-AM 'dead AM source' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.hp=0}}}
Reject-AM 'targeted AM envelope' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.target=6}}}
Reject-AM 'AM direct visible mutation' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.visibleUnchanged=0}}}
Reject-AM 'AM cost mutation' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.creditsAfter=99}}}
Reject-AM 'wrong paid production' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 2){$e.creditsAfter=101}}}
Reject-AM 'extra produced unit' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 2){$e.afterIds+=99}}}
Reject-AM 'replayed sequence' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.sequence=5}}}
Reject-AM 'boolean sequence' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.sequence=$true}}}
Reject-AM 'cross epoch' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.epoch=2}}}
Reject-AM 'missing actual host clock' {param($l)$l.host=$l.host -replace '(?m)^PROOF_ATTACK_MOVE_CLOCK[^\n]*\n?',''}
Reject-AM 'duplicate clock' {param($l)$l.host+="`n"+($l.host -split '\n'|Where-Object{$_ -like 'PROOF_ATTACK_MOVE_CLOCK *'}|Select-Object -Last 1)}
Reject-AM 'clock digest mismatch' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_CLOCK {param($e)if($e.revision -eq 228){$e.digest='f'*32}}}
Reject-AM 'forged clock jump' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_CLOCK {param($e)if($e.revision -eq 228){$e.simulatedSeconds+=10}}}
Reject-AM 'insufficient actual firing time/DPS' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_CLOCK {param($e)$e.simulatedSeconds*=.5}}
Reject-AM 'phase not actual snapshot' {param($l)Edit-AMRows $l rendered PROOF_ATTACK_MOVE_PHASE {param($e)if($e.phase -eq 'firing'){$e.x+=10}}}
Reject-AM 'native event incorrectly invoked' {param($l)Edit-AMRows $l rendered PROOF_ATTACK_MOVE_PHASE {param($e)$e.nativeRight=1}}
Reject-AM 'changed selection' {param($l)Edit-AMRows $l rendered PROOF_ATTACK_MOVE_PHASE {param($e)$e.selected=10}}
Reject-AM 'pending extra queue order' {param($l)Edit-AMRows $l rendered PROOF_ATTACK_MOVE_PHASE {param($e)$e.queue=1}}
Reject-AM 'missing resume phase' {param($l)$l.rendered=$l.rendered -replace '(?m)^PROOF_ATTACK_MOVE_PHASE[^\n]*"phase":"resumed"[^\n]*\n?',''}
Reject-AM 'missing shared Request path' {param($l)$l.rendered=$l.rendered.Replace('type=AttackMove','type=Move')}
Reject-AM 'missing client receipt' {param($l)$l.rendered=$l.rendered.Replace('sequence=6 accepted=1','sequence=6 accepted=0')}
Reject-AM 'shared semantic disagreement' {param($l)Edit-AMRows $l observer PROOF_REFINERY_STATE {param($e)if($e.revision -eq 228){($e.units|Where-Object id -eq 14).x+=1}}}
Reject-AM 'duplicate JSON key' {param($l)$l.host=$l.host.Replace('"type":8','"type":8,"type":8')}
Reject-AM 'numeric string sequence' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.sequence='6'}}}
Reject-AM 'array source HP' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.hp=@(100)}}}
Reject-AM 'object source coordinate' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 8){$e.x=@{value=-600}}}}
Reject-AM 'primitive command JSON' {param($l)$rows=$l.host -split '\n';$done=$false;$l.host=($rows|ForEach-Object{if(-not $done -and $_ -like 'PROOF_ATTACK_MOVE_COMMAND *'){$done=$true;'PROOF_ATTACK_MOVE_COMMAND false'}else{$_}})-join "`n"}
Reject-AM 'extra invalid completion' {param($l)$l.host+="`nPROOF_ATTACK_MOVE_COMPLETE role=host status=fail input=synthetic physicalInput=unverified"}
Reject-AM 'extra primitive phase' {param($l)$l.rendered+="`nPROOF_ATTACK_MOVE_PHASE null"}
Reject-AM 'duplicate ready' {param($l)$l.observer+="`nPROOF_ATTACK_MOVE_READY role=observer status=pass owner=2 input=synthetic physicalInput=unverified"}
Reject-AM 'phase digest malformed' {param($l)Edit-AMRows $l rendered PROOF_ATTACK_MOVE_PHASE {param($e)if($e.phase -eq 'firing'){$e.digest='z'*32}}}
Reject-AM 'stopped phase before accepted Stop' {param($l)Edit-AMRows $l rendered PROOF_ATTACK_MOVE_PHASE {param($e)if($e.phase -eq 'stopped'){$e.revision=258;$e.digest=('{0:x32}' -f 258)}}}
Reject-AM 'moving phase before accepted AM' {param($l)Edit-AMRows $l rendered PROOF_ATTACK_MOVE_PHASE {param($e)if($e.phase -eq 'moving'){$e.revision=202;$e.digest=('{0:x32}' -f 202)}}}
Reject-AM 'resume phase after accepted Stop' {param($l)Edit-AMRows $l rendered PROOF_ATTACK_MOVE_PHASE {param($e)if($e.phase -eq 'resumed'){$e.revision=260;$e.digest=('{0:x32}' -f 260)}}}
Reject-AM 'Produce paid progress at start' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_COMMAND {param($e)if($e.type -eq 2){$e.productionAfter.jobs[0].work=100000;$e.productionAfter.jobs[0].paid=1}}}
Reject-AM 'missing staged-payment advance' {param($l)$l.host=$l.host -replace '(?m)^PROOF_ATTACK_MOVE_PRODUCTION[^\n]*"revision":60[^\n]*\n?',''}
Reject-AM 'payment from credits cannot forge work ledger' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_PRODUCTION {param($e)if($e.revision -eq 60){$e.productionPayment=2;($e.productionAfter.players|Where-Object id -eq 1).credits--}}}
Reject-AM 'production held mid-Advance' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_PRODUCTION {param($e)if($e.revision -eq 60){$e.productionAfter.jobs[0].held=1}}}
Reject-AM 'production job sequence replacement' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_PRODUCTION {param($e)if($e.revision -eq 60){$e.productionAfter.jobs[0].sequence=4}}}
Reject-AM 'production live factory refund' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_PRODUCTION {param($e)if($e.revision -eq 60){$e.productionAfter.jobs=@();$e.productionRefund=1;$e.productionPayment=0;($e.productionAfter.players|Where-Object id -eq 1).credits=150}}}
Reject-AM 'production premature cancellation' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_PRODUCTION {param($e)if($e.revision -eq 60){$e.productionAfter.jobs=@()}}}
Reject-AM 'production completion identity replaced' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_PRODUCTION {param($e)if($e.revision -eq 109){($e.productionAfter.units|Where-Object id -eq 14).id=99}}}
Reject-AM 'production completion spawn moved' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_PRODUCTION {param($e)if($e.revision -eq 109){($e.productionAfter.units|Where-Object id -eq 14).x++}}}
Reject-AM 'production unpaid visible progress' {param($l)Edit-AMRows $l host PROOF_REFINERY_STATE {param($e)if($e.revision -eq 60){$e.jobs[0].paid=0}}}
Reject-AM 'production checkpoint jobs disagree across clients' {param($l)Edit-AMRows $l observer PROOF_REFINERY_STATE {param($e)if($e.revision -eq 60){$e.jobs[0].held=1}}}
Reject-AM 'production digest chain replaced' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_PRODUCTION {param($e)if($e.revision -eq 60){$e.beforeDigest='f'*32}}}
Reject-AM 'production clock differs from published clock' {param($l)Edit-AMRows $l host PROOF_ATTACK_MOVE_PRODUCTION {param($e)$e.simulatedSeconds+=.01}}
Reject-AM 'production physically precedes accepted Produce' {param($l)$rows=$l.host -split '\n';$first=@($rows|Where-Object{$_ -like 'PROOF_ATTACK_MOVE_PRODUCTION *'})[0];$l.host=$first+"`n"+(($rows|Where-Object{$_ -cne $first}) -join "`n")}
Reject-AM 'production physically follows Attack Move' {param($l)$rows=$l.host -split '\n';$last=@($rows|Where-Object{$_ -like 'PROOF_ATTACK_MOVE_PRODUCTION *'})[-1];$l.host=(($rows|Where-Object{$_ -cne $last}) -join "`n")+"`n"+$last}
Reject-AM 'paid step physically follows completion' {param($l)$rows=$l.host -split '\n';$first=@($rows|Where-Object{$_ -like 'PROOF_ATTACK_MOVE_PRODUCTION *'})[0];$last=@($rows|Where-Object{$_ -like 'PROOF_ATTACK_MOVE_PRODUCTION *'})[-1];$l.host=(($rows|Where-Object{$_ -cne $first}|ForEach-Object{$_;if($_ -ceq $last){$first}}) -join "`n")}
Reject-AM 'contradictory unpublished simultaneous deposit context' {param($l)Edit-AMRows $l host PROOF_REFINERY_EVENT {param($e)if($e.event -eq 'deposit' -and $e.owner -eq 1){($e.productionBefore.players|Where-Object id -eq 2).credits--;($e.productionAfter.players|Where-Object id -eq 2).credits--}}}
Write-Output ("PASS synthetic-only Attack Move checks=$amChecks; no UE/runtime/native evidence")
