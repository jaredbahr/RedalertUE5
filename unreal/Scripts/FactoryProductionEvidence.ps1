# Opt-in three-role timed production routing policy. Immediate command states
# need not all be published; intermediate replicated job progress is required.
. (Join-Path $PSScriptRoot 'FactoryRallyEvidence.ps1')
function Get-FactoryProductionEvidence {
    param([Parameter(Mandatory)][hashtable]$Logs,[Parameter(Mandatory)][hashtable]$ValidatedStateContext)
    function Require($Condition,[string]$Reason) { if(-not $Condition){throw "Factory production evidence: $Reason"} }
    function Integer($Value,[long]$Min=0) { return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and $Value -isnot [array] -and [double]::IsFinite([double]$Value) -and [double]$Value -ge $Min -and [double]$Value -le 2147483647 -and [double]$Value -eq [math]::Truncate([double]$Value) }
    function Number($Value) { return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and $Value -isnot [array] -and [double]::IsFinite([double]$Value) }
    function Row([string]$Text,[int]$MaxLength=131072) {
        if(-not ($Text.Length -le $MaxLength)){throw 'Factory production evidence: oversized row'}
        $stack=[Collections.Generic.List[object]]::new()
        # Cache only pure key-token decoding in this bounded row. Object and
        # duplicate checks still run for every occurrence; rows share no cache.
        $decodedJsonKeyTokens=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
        foreach($m in [regex]::Matches($Text,'"(?:\\.|[^"\\])*"\s*:|[{}\[\]]')) {
            $t=$m.Value
            if($t -eq '{'){$stack.Add(@{})} elseif($t -eq '['){$stack.Add($null)}
            elseif($t -in @('}',']')){if(-not ($stack.Count -gt 0)){throw 'Factory production evidence: unbalanced JSON'};$stack.RemoveAt($stack.Count-1)}
            else {if(-not ($stack.Count -gt 0 -and $null -ne $stack[$stack.Count-1])){throw 'Factory production evidence: key outside object'};if(-not $decodedJsonKeyTokens.ContainsKey($t)){$decodedJsonKeyTokens[$t]=($t -replace '\s*:$',''|ConvertFrom-Json)};$k=$decodedJsonKeyTokens[$t];if(-not (-not $stack[$stack.Count-1].ContainsKey($k))){throw 'Factory production evidence: duplicate key'};$stack[$stack.Count-1][$k]=$true}
        }
        if(-not ($stack.Count -eq 0)){throw 'Factory production evidence: unbalanced JSON'}
        $v=$Text|ConvertFrom-Json -Depth 8 -NoEnumerate -ErrorAction Stop
        if(-not ($null -ne $v -and $v -isnot [array] -and $v -isnot [string] -and $v -isnot [ValueType])){throw 'Factory production evidence: object required'}
        return $v
    }
    function Distance($A,$B,[string]$Prefix='') {
        $sum=0.0;foreach($axis in @('X','Y','Z')){$n=$Prefix+$axis;$d=[double]$A.$n-[double]$B.($axis.ToLowerInvariant());$sum+=$d*$d};return [math]::Sqrt($sum)
    }
    function Unit($S,$Id) { return @($S.units|Where-Object id -eq $Id) }
    foreach($role in @('host','rendered','observer')) {
        Require ($Logs.ContainsKey($role) -and $Logs[$role] -is [string] -and $Logs[$role].Length -le 24MB) "missing/invalid/big log $role"
        if($role -ne 'host'){Require ($Logs[$role] -notmatch 'PROOF_FACTORY_PRODUCTION_(EVENT|ADVANCE)') 'event outside host'}
        if($role -ne 'rendered'){Require ($Logs[$role] -notmatch 'PROOF_FACTORY_PRODUCTION_(PHASE|CAPTURE)') 'phase/capture outside rendered'}
    }
    Require ([regex]::Matches($Logs.host,'PROOF_FACTORY_PRODUCTION_EVENT').Count -eq 10) 'event marker count'
    Require ([regex]::Matches($Logs.rendered,'PROOF_FACTORY_PRODUCTION_PHASE').Count -eq 9) 'phase marker count'
    Require ([regex]::Matches($Logs.rendered,'(?m)PROOF_FACTORY_PRODUCTION_CAPTURE').Count -eq 3) 'capture marker count'
    $maps=$ValidatedStateContext.maps;$common=$ValidatedStateContext.common
    Require ($null -ne $maps -and $common.Count -ge 3 -and $maps.ContainsKey('observer')) 'need validated three-role state'
    foreach($role in @('host','rendered','observer')) { Require ($Logs[$role] -notmatch 'PROOF_FACTORY_PRODUCTION_ERROR') "runtime error $role" }
    $eventMatches=@([regex]::Matches($Logs.host,'PROOF_FACTORY_PRODUCTION_EVENT([^\r\n]*)'))
    $events=@($eventMatches|ForEach-Object{Row $_.Groups[1].Value})
    Require ($events.Count -eq 10) 'expected builds/rallies and three separate start/completion pairs'
    $advanceMatches=@([regex]::Matches($Logs.host,'PROOF_FACTORY_PRODUCTION_ADVANCE([^\r\n]*)'))
    $advances=@($advanceMatches|ForEach-Object{Row $_.Groups[1].Value})
    Require ($advances.Count -ge 150 -and $advances.Count -le 6600) 'Advance row bound'
    $types=@('BuildFactory','BuildFactory','SetRally','SetRally','Produce','Complete','Produce','Complete','Produce','Complete')
    $priorSequence=0;$priorRevision=0;$epoch=0
    for($i=0;$i -lt 10;$i++) {
        $e=$events[$i];Require ($e.type -ceq $types[$i]) 'event order/type'
        foreach($n in @('owner','epoch','sequence','factory','beforeRevision','revision','creditsBefore','creditsAfter')){Require (Integer $e.$n) "event integer $n"}
        Require ($e.owner -eq 1 -and $e.epoch -gt 0 -and $e.factory -gt 0 -and (($e.type -eq 'Complete' -and $e.sequence -eq $priorSequence) -or ($e.type -ne 'Complete' -and $e.sequence -gt $priorSequence)) -and $e.beforeRevision -ge $priorRevision) 'event chronology/identity'
        if($epoch -eq 0){$epoch=$e.epoch};Require ($e.epoch -eq $epoch) 'event epoch changed'
        $priorSequence=$e.sequence;$priorRevision=$e.revision
        Require ((Integer $e.unit -1) -and $e.beforeDigest -cmatch '^[A-Fa-f0-9]{32}$' -and $e.digest -cmatch '^[A-Fa-f0-9]{32}$') 'unit/digest'
        foreach($n in @('rallyX','rallyY','rallyZ','spawnX','spawnY','spawnZ')){Require ((Number $e.$n) -and [math]::Abs([double]$e.$n) -le 10000) "coordinate $n"}
        Require ($e.rallyZ -eq 0 -and $e.spawnZ -eq 0) 'ground coordinates'
        foreach($n in @('beforeIds','afterIds','eligible')) {
            Require ($e.$n -is [array] -and $e.$n.Count -le 128) 'ID array'
            $seen=@{};foreach($id in $e.$n){Require ((Integer $id 1) -and -not $seen.ContainsKey([string]$id)) 'duplicate/invalid ID';$seen[[string]$id]=$true}
        }
        Require ($e.beforeIds.Count -gt 0 -and $e.afterIds.Count -gt 0) 'empty snapshots'
        $cost=if($e.type -eq 'BuildFactory'){150}else{0}
        $delta=if($e.type -eq 'BuildFactory'){2}else{1}
        if($e.type -ne 'Complete'){Require ($e.creditsBefore-$e.creditsAfter -eq $cost -and $e.revision -eq $e.beforeRevision+$delta) 'cost/revision'}
        else{Require ($e.revision -gt $e.beforeRevision -and (Integer $e.productionPayment) -and (Integer $e.productionRefund)) 'completion revision/ledger'}
        $added=@($e.afterIds|Where-Object{$_ -notin $e.beforeIds});$removed=@($e.beforeIds|Where-Object{$_ -notin $e.afterIds})
        Require ($removed.Count -eq 0) 'removed ID'
        if($e.type -in @('SetRally','Produce')){Require ($e.unit -eq -1 -and $added.Count -eq 0 -and $e.factory -in $e.beforeIds) 'rally creation/source'}
        elseif($e.type -eq 'BuildFactory'){Require ($e.unit -gt 0 -and $added.Count -eq 1 -and $added[0] -eq $e.unit) 'paid exact added unit'}
        else {Require ($e.unit -gt 0 -and $e.unit -in $added) 'completion must add new unit'}
        if($e.type -in @('BuildFactory','SetRally')){Assert-FactoryImmediateMutation $e $maps}
        foreach($pair in @(@('beforeRevision','beforeDigest'),@('revision','digest'))) {
            $key="$epoch`:$($e.($pair[0]))"
            if($maps.host.ContainsKey($key)){
                $s=$maps.host[$key];Require ($s.digest -ceq $e.($pair[1])) 'published command digest'
                $before=$pair[0] -eq 'beforeRevision';$ids=if($before){$e.beforeIds}else{$e.afterIds};$credits=if($before){$e.creditsBefore}else{$e.creditsAfter}
                Require ((($s.units.id|Sort-Object) -join ',') -ceq (($ids|Sort-Object) -join ',')) 'published command ID set'
                $p=@($s.players|Where-Object id -eq 1);Require ($p.Count -eq 1 -and $p[0].credits -eq $credits) 'published command credit'
            }
        }
    }
    $first=$events[0].unit;$second=$events[1].unit
    Require ($first -ne $second -and $events[0].factory -eq $first -and $events[1].factory -eq $second -and
        $events[2].factory -eq $first -and $events[3].factory -eq $second) 'paid factory/rally identity'
    Require ($events[2].rallyX -ne $events[3].rallyX -or $events[2].rallyY -ne $events[3].rallyY) 'rallies must differ'
    $phases=@([regex]::Matches($Logs.rendered,'PROOF_FACTORY_PRODUCTION_PHASE([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value 4096})
    Require ($phases.Count -eq 9) 'expected three phases per case'
    $names=@('requested','spawned','moved');$nativeProduce=$null;$nativeRight=$null;$previousElapsed=-1.0;$results=@()
    for($case=0;$case -lt 3;$case++) {
        $start=$events[4+$case*2];$e=$events[5+$case*2];$set=$events[$(if($case -eq 1){3}else{2})];$factory=if($case -eq 1){$second}else{$first}
        Require ($start.factory -eq $factory -and ($start.eligible -join ',') -ceq "$first,$second" -and $start.factory -in $start.beforeIds -and ($e.eligible -join ',') -ceq "$first,$second") 'routing/eligible array order'
        $chain=@();$offsets=@()
        for($rowIndex=0;$rowIndex -lt $advances.Count;$rowIndex++){
            if($advances[$rowIndex].sequence -eq $start.sequence){$chain+=,$advances[$rowIndex];$offsets+=,$advanceMatches[$rowIndex].Index}
        }
        $timed=Get-FactoryTimedProductionLifecycle $start $e $chain $maps $common -StartOffset $eventMatches[4+$case*2].Index -CompleteOffset $eventMatches[5+$case*2].Index -AdvanceOffsets $offsets
        foreach($n in @('rallyX','rallyY','rallyZ')){Require ($e.$n -eq $set.$n) 'chosen rally mismatch'}
        $rows=@($phases[($case*3)..($case*3+2)])
        for($j=0;$j -lt 3;$j++) {
            $p=$rows[$j];Require ($p.phase -ceq $names[$j]) 'phase order'
            foreach($n in @('case','owner','epoch','revision','first','second','factory','sequence','requestCount','nativeProduce','nativeRight','receipt','receiptSequence','receiptEpoch')){Require (Integer $p.$n) "phase integer $n"}
            Require ((Integer $p.unit -1) -and (Integer $p.requestSelected -1)) 'phase unit/selection'
            foreach($n in @('x','y','z','elapsed')){Require (Number $p.$n) "phase coordinate/time $n"}
            Require ($p.case -eq $case -and $p.owner -eq 1 -and $p.epoch -eq $epoch -and $p.first -eq $first -and $p.second -eq $second -and $p.factory -eq $factory -and $p.sequence -eq $e.sequence -and $p.elapsed -ge 0 -and $p.elapsed -gt $previousElapsed -and $p.elapsed -le 220) 'phase identity/time'
            $previousElapsed=$p.elapsed
            if($null -eq $nativeProduce){$nativeProduce=$p.nativeProduce;$nativeRight=$p.nativeRight}
            Require ($p.nativeProduce -eq $nativeProduce -and $p.nativeRight -eq $nativeRight) 'synthetic fixture changed native counters'
            $key="$epoch`:$($p.revision)";Require ($maps.rendered.ContainsKey($key)) 'phase missing exact rendered revision'
            $s=$maps.rendered[$key]
            $eligible=@($s.units|Where-Object{$_.owner -eq 1 -and $_.kind -eq 4 -and $_.hp -gt 0})
            Require (($eligible.id -join ',') -ceq "$first,$second") 'phase does not contain both ordered live factories'
            if($case -eq 0){Require ($p.requestSelected -eq -1 -and $p.requestCount -eq 0) 'no selection case'}
            elseif($case -eq 1){Require ($p.requestSelected -eq $second -and $p.requestCount -eq 1) 'selected second case'}
            else {$u=Unit $s $p.requestSelected;Require ($p.requestCount -eq 1 -and $u.Count -eq 1 -and $u[0].owner -eq 1 -and $u[0].kind -eq 2 -and $u[0].hp -gt 0) 'non-factory HQ case'}
            if($j -eq 0){Require ($p.unit -eq -1 -and $p.revision -le $start.beforeRevision) 'request chronology'}
            else {
                $u=Unit $s $e.unit
                Require ($p.unit -eq $e.unit -and $p.receipt -eq 1 -and $p.receiptSequence -eq $e.sequence -and $p.receiptEpoch -eq $epoch -and $u.Count -eq 1 -and $u[0].owner -eq 1 -and $u[0].kind -eq 0 -and $u[0].hp -gt 0 -and (Distance $p $u[0]) -le .00001) 'actual receipt/spawn phase'
            }
        }
        Require ($rows[1].revision -ge $e.revision -and $rows[2].revision -gt $rows[1].revision -and $rows[2].elapsed-$rows[1].elapsed -ge 1 -and (Distance $rows[2] $rows[1]) -gt 50) 'movement window'
        $spawn=[pscustomobject]@{x=$e.spawnX;y=$e.spawnY;z=$e.spawnZ};$initialDistance=Distance $e $spawn 'rally'
        Require ((Distance $rows[1] $spawn) -le 15*($rows[1].revision-$e.revision)+.00001) 'spawn follow-up bound'
        $pre=@();$progress=@()
        foreach($key in $common) {
            $s=$maps.host[$key];if($s.epoch -ne $epoch){continue}
            $ok=$true
            foreach($r in @($events[2],$events[3])){
                $f=Unit $s $r.factory
                if($f.Count -ne 1 -or $f[0].owner -ne 1 -or $f[0].kind -ne 4 -or $f[0].hp -le 0 -or $f[0].rally -ne 1 -or $f[0].rallyX -ne $r.rallyX -or $f[0].rallyY -ne $r.rallyY -or $f[0].rallyZ -ne $r.rallyZ){$ok=$false}
            }
            if(-not $ok){continue}
            if($s.revision -ge $events[3].revision -and $s.revision -lt $start.revision){$pre+=,$key}
            if($s.revision -le $rows[1].revision -or $s.revision -gt $rows[2].revision){continue}
            $u=Unit $s $e.unit
            if($u.Count -eq 1 -and $u[0].owner -eq 1 -and $u[0].kind -eq 0 -and $u[0].hp -gt 0 -and $u[0].rally -eq 0 -and (Distance $u[0] $spawn) -gt 50 -and (Distance $e $u[0] 'rally') -lt $initialDistance){$progress+=,$key}
        }
        Require ($pre.Count -ge 1 -and $progress.Count -ge 2) 'missing common pre-production rallies or later movement across all three roles'
        $results+=@{case=$case;factory=$factory;unit=$e.unit;sequence=$e.sequence;commonRally=$pre;commonMovement=$progress;timedProduction=$timed;phases=$rows}
    }
    Require (@($advances|Where-Object{$_.sequence -notin @($events|Where-Object type -eq Produce).sequence}).Count -eq 0) 'orphan Advance'
    $captures=@([regex]::Matches($Logs.rendered,'(?m)PROOF_FACTORY_PRODUCTION_CAPTURE name=(\d\d)-factory-production\.png case=(\d) written=1 physicalInputVerified=0(?=\r?$)'))
    Require ($captures.Count -eq 3) 'three completed captures required'
    for($i=0;$i -lt 3;$i++){Require ([int]$captures[$i].Groups[1].Value -eq $i+3 -and [int]$captures[$i].Groups[2].Value -eq $i) 'capture case/order'}
    return [ordered]@{passed=$true;first=$first;second=$second;epoch=$epoch;cases=$results;authoritativeEvents=$events;physicalInputVerified=$false;scope='actual shared Produce handler and loopback request/receipt/timed job/debit/completion/rally snapshots across three roles; no physical P or internet claim'}
}
