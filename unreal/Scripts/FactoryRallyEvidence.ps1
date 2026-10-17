# Pure opt-in policy; all published state validation is shared with the refinery
# evidence policy. Immediate command revisions are authoritative observations,
# not a claim that every command revision was replicated.
function Get-FactoryRallyEvidence {
    param([Parameter(Mandatory)][hashtable]$Logs)
    function Require($Condition,[string]$Reason) { if(-not $Condition){throw "Factory rally evidence: $Reason"} }
    function Integer($Value,[long]$Min=0,[long]$Max=2147483647) {
        return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and $Value -isnot [array] -and [double]$Value -ge $Min -and [double]$Value -le $Max -and [double]$Value -eq [math]::Truncate([double]$Value)
    }
    function Number($Value) { return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and $Value -isnot [array] -and [double]::IsFinite([double]$Value) }
    function Row([string]$Text) {
        Require ($Text.Length -le 4096) 'oversized event'
        $stack=[Collections.Generic.List[object]]::new()
        foreach($m in [regex]::Matches($Text,'"(?:\\.|[^"\\])*"\s*:|[{}\[\]]')) {
            $t=$m.Value
            if($t -eq '{'){$stack.Add(@{})} elseif($t -eq '['){$stack.Add($null)}
            elseif($t -in @('}',']')){Require ($stack.Count -gt 0) 'unbalanced JSON';$stack.RemoveAt($stack.Count-1)}
            else {Require ($stack.Count -gt 0 -and $null -ne $stack[$stack.Count-1]) 'key outside object';$k=($t -replace '\s*:$',''|ConvertFrom-Json);Require (-not $stack[$stack.Count-1].ContainsKey($k)) 'duplicate key';$stack[$stack.Count-1][$k]=$true}
        }
        Require ($stack.Count -eq 0) 'unbalanced JSON'
        $value=$Text|ConvertFrom-Json -Depth 8 -NoEnumerate -ErrorAction Stop
        Require ($null -ne $value -and $value -isnot [array] -and $value -isnot [string] -and $value -isnot [ValueType]) 'object required'
        return $value
    }
    function Distance($A,$B,[string]$Prefix='') {
        $sum=0.0;foreach($axis in @('X','Y','Z')){$n=$Prefix+$axis;$d=[double]$A.$n-[double]$B.($axis.ToLowerInvariant());$sum+=$d*$d};return [math]::Sqrt($sum)
    }
    function Unit($S,$Id) {return @($S.units|Where-Object id -eq $Id)}
    $context=@{}
    $refinery=Get-RefineryMinerEvidence -Logs $Logs -Scenario Coop -ValidatedStateContext $context
    $maps=$context.maps;$common=$context.common
    Require ($null -ne $maps -and $common.Count -ge 3) 'missing validated shared state'
    foreach($role in @('host','rendered','observer')){Require ($Logs[$role] -notmatch 'PROOF_FACTORY_RALLY_ERROR') "runtime error $role"}
    $events=@([regex]::Matches($Logs.host,'PROOF_FACTORY_RALLY_EVENT([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value})
    Require ($events.Count -eq 2) 'expected exactly two authoritative events'
    foreach($event in $events) {
        Require ($event.type -in @('SetRally','Produce')) 'invalid command type'
        foreach($n in @('owner','epoch','sequence','factory','beforeRevision','revision','creditsBefore','creditsAfter')){Require (Integer $event.$n) "invalid integer $n"}
        Require ($event.owner -eq 1 -and $event.epoch -gt 0 -and $event.sequence -gt 0 -and $event.factory -gt 0) 'invalid event identity'
        Require ((Integer $event.unit -1) -and $event.beforeDigest -match '^[a-fA-F0-9]{32}$' -and $event.digest -match '^[a-fA-F0-9]{32}$') 'invalid event unit/digest'
        foreach($n in @('rallyX','rallyY','rallyZ','spawnX','spawnY','spawnZ')){Require ((Number $event.$n) -and [math]::Abs([double]$event.$n) -le 10000) "invalid coordinate $n"}
        Require ($event.rallyZ -eq 0 -and $event.spawnZ -eq 0) 'non-ground event'
        foreach($n in @('beforeIds','afterIds')) {
            Require ($event.$n -is [array] -and $event.$n.Count -gt 0 -and $event.$n.Count -le 128) 'invalid ID array'
            $seen=@{};foreach($id in $event.$n){Require ((Integer $id 1) -and -not $seen.ContainsKey([string]$id)) 'invalid/duplicate ID';$seen[[string]$id]=$true}
        }
        foreach($pair in @(@('beforeRevision','beforeDigest'),@('revision','digest'))) {
            $key="$($event.epoch):$($event.($pair[0]))"
            if($maps.host.ContainsKey($key)){
                $published=$maps.host[$key]
                Require ($published.digest -ceq $event.($pair[1])) 'published command checkpoint digest mismatch'
                $before=$pair[0] -eq 'beforeRevision';$ids=if($before){$event.beforeIds}else{$event.afterIds};$credits=if($before){$event.creditsBefore}else{$event.creditsAfter}
                Require ((($published.units.id|Sort-Object) -join ',') -ceq (($ids|Sort-Object) -join ',')) 'published command ID set mismatch'
                $player=@($published.players|Where-Object id -eq 1);Require ($player.Count -eq 1 -and $player[0].credits -eq $credits) 'published command credits mismatch'
            }
        }
    }
    $set=@($events|Where-Object type -eq SetRally);$produce=@($events|Where-Object type -eq Produce)
    Require ($set.Count -eq 1 -and $produce.Count -eq 1) 'missing/duplicate event type';$set=$set[0];$produce=$produce[0]
    Require ($events[0].type -eq 'SetRally' -and $set.epoch -eq $produce.epoch -and $set.factory -eq $produce.factory -and $produce.sequence -eq $set.sequence+1) 'event sequence/epoch/factory mismatch'
    Require ($set.unit -eq -1 -and $set.revision -eq $set.beforeRevision+1 -and $set.creditsBefore -eq $set.creditsAfter) 'setting rally mutated cost or revision incorrectly'
    Require ($set.factory -in $set.beforeIds -and $set.factory -in $produce.beforeIds) 'command source absent from ID sets'
    Require ((($set.beforeIds|Sort-Object) -join ',') -ceq (($set.afterIds|Sort-Object) -join ',')) 'setting rally changed unit IDs'
    Require ($produce.unit -gt 0 -and $produce.revision -eq $produce.beforeRevision+2 -and $produce.beforeRevision -ge $set.revision -and $produce.creditsBefore-$produce.creditsAfter -eq 50) 'paid production contract'
    Require ($produce.unit -notin $produce.beforeIds -and @($produce.afterIds|Where-Object{$_ -notin $produce.beforeIds}).Count -eq 1 -and $produce.unit -in $produce.afterIds -and @($produce.beforeIds|Where-Object{$_ -notin $produce.afterIds}).Count -eq 0) 'production did not add exact new unit'
    foreach($n in @('rallyX','rallyY','rallyZ')){Require ($set.$n -eq $produce.$n) 'rally changed between setting and production'}
    $factoryKey=$null;$progress=@();$spawn=[pscustomobject]@{x=$produce.spawnX;y=$produce.spawnY;z=$produce.spawnZ}
    $initialDistance=Distance $produce $spawn 'rally'
    foreach($key in $common) {
        $state=$maps.host[$key];if($state.epoch -ne $set.epoch){continue}
        $factory=Unit $state $set.factory
        if($factory.Count -ne 1 -or $factory[0].owner -ne 1 -or $factory[0].kind -ne 4 -or $factory[0].hp -le 0 -or $factory[0].rally -ne 1){continue}
        if($factory[0].rallyX -ne $set.rallyX -or $factory[0].rallyY -ne $set.rallyY -or $factory[0].rallyZ -ne $set.rallyZ){continue}
        if($state.revision -ge $set.revision -and $state.revision -lt $produce.revision){$factoryKey=$key}
        $unit=Unit $state $produce.unit
        if($state.revision -gt $produce.revision -and $unit.Count -eq 1 -and $unit[0].owner -eq 1 -and $unit[0].kind -eq 0 -and $unit[0].hp -gt 0 -and $unit[0].rally -eq 0 -and (Distance $unit[0] $spawn) -gt 50 -and (Distance $produce $unit[0] 'rally') -lt $initialDistance){$progress+=,$key}
    }
    Require ($factoryKey -and $progress.Count -ge 2) 'missing common pre-production rally or later actual movement'
    $phases=@([regex]::Matches($Logs.rendered,'PROOF_FACTORY_RALLY_FIXTURE([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value})
    Require ($phases.Count -eq 3) 'expected exactly three shared-handler phases'
    $names=@('rallied','spawned','moved');$previous=-1.0;$native=$null
    for($index=0;$index -lt 3;$index++) {
        $p=$phases[$index];Require ($p.phase -ceq $names[$index]) 'missing/out-of-order phase'
        foreach($n in @('owner','epoch','revision','factory','setSequence','produceSequence','selected','selectionCount','nativeRight')){Require (Integer $p.$n) "invalid phase integer $n"}
        foreach($n in @('x','y','z','rallyX','rallyY','rallyZ','elapsed')){Require (Number $p.$n) "invalid phase number $n"}
        Require ((Integer $p.unit -1) -and $p.owner -eq 1 -and $p.epoch -eq $set.epoch -and $p.factory -eq $set.factory -and $p.selected -eq $set.factory -and $p.selectionCount -eq 1 -and $p.setSequence -eq $set.sequence -and $p.elapsed -gt $previous -and $p.elapsed -le 220) 'phase identity/selection/time'
        if($null -eq $native){$native=$p.nativeRight};Require ($p.nativeRight -eq $native) 'synthetic fixture changed native counter';$previous=$p.elapsed
        foreach($n in @('rallyX','rallyY','rallyZ')){Require ($p.$n -eq $set.$n) 'phase destination mismatch'}
        $key="$($p.epoch):$($p.revision)";Require ($maps.rendered.ContainsKey($key)) 'phase lacks exact rendered state'
        $f=Unit $maps.rendered[$key] $set.factory;Require ($f.Count -eq 1 -and $f[0].rally -eq 1 -and $f[0].rallyX -eq $set.rallyX -and $f[0].rallyY -eq $set.rallyY) 'phase factory disagrees with rendered state'
        if($index -eq 0){Require ($p.unit -eq -1 -and $p.produceSequence -eq 0) 'invalid initial phase'}
        else {$u=Unit $maps.rendered[$key] $produce.unit;Require ($p.unit -eq $produce.unit -and $p.produceSequence -eq $produce.sequence -and $u.Count -eq 1 -and (Distance $p $u[0]) -le 0.00001) 'phase unit disagrees with rendered state'}
    }
    Require ($set.revision -le $phases[0].revision -and $phases[0].revision -lt $produce.revision -and
        $produce.revision -le $phases[1].revision -and $phases[1].revision -lt $phases[2].revision) 'shared-handler phase revision chronology'
    Require ($phases[2].elapsed-$phases[1].elapsed -ge 1 -and (Distance $phases[2] $phases[1]) -gt 50) 'no observed shared-handler movement window'
    # The existing host uses Advance(0.1): 150 cm/s permits at most 15 cm per
    # revision increment. Other commands can add revisions, making this a
    # conservative bound, never a substitute for common-state movement above.
    Require ($phases[1].revision -ge $produce.revision -and (Distance $phases[1] $spawn) -le 15*($phases[1].revision-$produce.revision)+0.00001) 'first observed unit cannot follow authoritative spawn'
    $captures=@([regex]::Matches($Logs.rendered,'PROOF_FACTORY_RALLY_CAPTURE name=03-factory-rally\.png selected=(\d+) written=1 physicalInputVerified=0'))
    Require ($captures.Count -eq 1 -and [int]$captures[0].Groups[1].Value -eq $set.factory) 'missing selected-factory capture observation'
    return [ordered]@{passed=$true;factory=$set.factory;unit=$produce.unit;epoch=$set.epoch;setSequence=$set.sequence;produceSequence=$produce.sequence;commonRally=$factoryKey;commonMovement=$progress;phaseRows=$phases;physicalInputVerified=$false;authoritativeEventPolicy='bounded source-validated immediate Submit changes; later published rally/movement states match all three roles'}
}
