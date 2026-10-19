function Get-FactoryProductionContextKey($S){
    Assert-ProductionContext $S
    $jobs=@($S.jobs|ForEach-Object{[ordered]@{owner=[long]$_.owner;sequence=[long]$_.sequence;factory=[long]$_.factory;work=[long]$_.work;paid=[long]$_.paid;held=[long]$_.held}})
    $units=@($S.units|Sort-Object id|ForEach-Object{[ordered]@{id=[long]$_.id;owner=[long]$_.owner;kind=[long]$_.kind;hp=[double]$_.hp;cargo=[long]$_.cargo;x=[double]$_.x;y=[double]$_.y;z=[double]$_.z;rally=[long]$_.rally;rallyX=[double]$_.rallyX;rallyY=[double]$_.rallyY;rallyZ=[double]$_.rallyZ}})
    $players=@($S.players|Sort-Object id|ForEach-Object{[ordered]@{id=[long]$_.id;credits=[long]$_.credits;gathered=[long]$_.gathered}})
    return ([ordered]@{phase=[long]$S.phase;jobs=$jobs;units=$units;players=$players}|ConvertTo-Json -Depth 12 -Compress)
}

function Assert-FactoryPublishedContext($Event,$Maps) {
    foreach($pair in @(@('beforeRevision','beforeDigest','productionBefore'),@('revision','digest','productionAfter'))){
        $key="$($Event.epoch):$($Event.($pair[0]))"
        foreach($role in @('host','rendered','observer')){
            if($Maps[$role].ContainsKey($key)){
                $state=$Maps[$role][$key]
                if($state.digest -cne $Event.($pair[1]) -or (Get-FactoryProductionContextKey $state) -cne (Get-FactoryProductionContextKey $Event.($pair[2]))){throw "Factory evidence: published context mismatch $role/$key"}
            }
        }
    }
}
function Assert-FactoryEventContext($Event,$Maps) {
    Assert-ProductionContext $Event.productionBefore;Assert-ProductionContext $Event.productionAfter
    $beforePlayer=@($Event.productionBefore.players|Where-Object id -eq $Event.owner);$afterPlayer=@($Event.productionAfter.players|Where-Object id -eq $Event.owner)
    if($beforePlayer.Count -ne 1 -or $afterPlayer.Count -ne 1 -or $beforePlayer[0].credits -ne $Event.creditsBefore -or $afterPlayer[0].credits -ne $Event.creditsAfter){throw 'Factory evidence: context credits disagree with event'}
    foreach($pair in @(@('productionBefore','beforeIds'),@('productionAfter','afterIds'))){
        if((@($Event.($pair[0]).units.id|Sort-Object)-join ',') -cne (@($Event.($pair[1])|Sort-Object)-join ',')){throw 'Factory evidence: context IDs disagree with event'}
    }
    Assert-FactoryPublishedContext $Event $Maps
}
function Assert-FactoryImmediateMutation($Event,$Maps) {
    function Require($Condition,[string]$Reason){if(-not $Condition){throw "Factory command evidence: $Reason"}}
    function Zero($Value){return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and $Value -isnot [array] -and [double]$Value -eq 0}
    Assert-FactoryEventContext $Event $Maps
    Require ($Event.productionBefore.phase -eq 1) 'command outside running match'
    Require ((Zero $Event.productionPayment) -and (Zero $Event.productionRefund)) 'build/rally cannot claim production debit/refund'
    $expected=$Event.productionBefore|ConvertTo-Json -Depth 12 -Compress|ConvertFrom-Json -Depth 12
    if($Event.type -ceq 'BuildFactory'){
        $added=@($Event.productionAfter.units|Where-Object{$_.id -notin $Event.productionBefore.units.id})
        Require ($Event.revision -eq $Event.beforeRevision+2 -and $Event.factory -eq $Event.unit -and $added.Count -eq 1 -and $Event.productionAfter.units.Count -eq $Event.productionBefore.units.Count+1) 'exact factory creation/revision'
        $factory=$added[0]
        Require ($factory.id -eq $Event.unit -and $factory.owner -eq $Event.owner -and $factory.kind -eq 4 -and $factory.hp -eq 200 -and $factory.cargo -eq 0 -and $factory.rally -eq 0 -and $factory.rallyX -eq 0 -and $factory.rallyY -eq 0 -and $factory.rallyZ -eq 0) 'canonical factory creation'
        foreach($axis in @('x','y','z')){Require ($factory.$axis -eq $Event.('spawn'+$axis) -and $Event.('rally'+$axis) -eq $Event.('spawn'+$axis)) 'factory built at requested site'}
        $player=@($expected.players|Where-Object id -eq $Event.owner)[0]
        Require ($player.credits -ge 150) 'factory cost';$player.credits-=150;$expected.units+=,$factory
    }elseif($Event.type -ceq 'SetRally'){
        Require ($Event.revision -eq $Event.beforeRevision+1 -and $Event.unit -eq -1 -and $Event.spawnX -eq 0 -and $Event.spawnY -eq 0 -and $Event.spawnZ -eq 0) 'rally revision/non-creation'
        $factory=@($expected.units|Where-Object id -eq $Event.factory)
        Require ($factory.Count -eq 1 -and $factory[0].owner -eq $Event.owner -and $factory[0].kind -eq 4 -and $factory[0].hp -gt 0) 'rally live owned source'
        $factory[0].rally=1;foreach($axis in @('X','Y','Z')){$factory[0].('rally'+$axis)=$Event.('rally'+$axis)}
    }else{throw 'Factory command evidence: unsupported immediate mutation'}
    Require ((Get-FactoryProductionContextKey $expected) -ceq (Get-FactoryProductionContextKey $Event.productionAfter)) 'unrelated mutation in build/rally'
}

# Pure shared lifecycle policy. Production starts and completions are different
# observations. Every intervening authoritative Advance is checked independently;
# replicated intermediate jobs must agree with all three already-validated roles.
function Get-FactoryTimedProductionLifecycle {
    param($Start,$Complete,[object[]]$Advances,$Maps,[object[]]$Common,[long]$StartOffset,[long]$CompleteOffset,[long[]]$AdvanceOffsets)
    function Require($Condition,[string]$Reason){if(-not $Condition){throw "Timed factory evidence: $Reason"}}
    function Integer($Value,[long]$Min=0,[long]$Max=2147483647){return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and $Value -isnot [array] -and [double]::IsFinite([double]$Value) -and [double]$Value -ge $Min -and [double]$Value -le $Max -and [double]$Value -eq [math]::Truncate([double]$Value)}
    function ContextKey($S){return (Get-FactoryProductionContextKey $S)}
    function JobKey($J){return (@($J.owner,$J.sequence,$J.factory,$J.work,$J.paid,$J.held)-join ':')}
    function Published($E){Assert-FactoryPublishedContext $E $Maps}
    function ContextEnvelope($E){Assert-FactoryEventContext $E $Maps}
    Require ($Start.type -ceq 'Produce' -and $Complete.type -ceq 'Complete') 'missing start/completion'
    foreach($n in @('owner','epoch','sequence','factory')){Require ($Start.$n -eq $Complete.$n) "completion identity $n"}
    Require ($Start.unit -eq -1 -and $Start.revision -eq $Start.beforeRevision+1 -and $Start.creditsBefore -eq $Start.creditsAfter -and $Start.spawnX -eq 0 -and $Start.spawnY -eq 0 -and $Start.spawnZ -eq 0) 'start must only add job'
    Require ((Integer $Start.productionPayment) -and (Integer $Start.productionRefund) -and $Start.productionPayment -eq 0 -and $Start.productionRefund -eq 0) 'start cannot claim payment/refund'
    Require (@($Start.productionBefore.units|Where-Object{$_.owner -eq $Start.owner -and $_.hp -gt 0}).Count -lt 12) 'start live record cap'
    Require ((Integer $Complete.productionPayment) -and (Integer $Complete.productionRefund)) 'completion payment/refund integer'
    ContextEnvelope $Start;ContextEnvelope $Complete
    Require ($Start.productionBefore.phase -eq 1 -and @($Start.productionBefore.jobs|Where-Object owner -eq $Start.owner).Count -eq 0) 'start requires running phase and free owner job'
    $job=@($Start.productionAfter.jobs|Where-Object owner -eq $Start.owner)
    Require ($job.Count -eq 1 -and $job[0].sequence -eq $Start.sequence -and $job[0].factory -eq $Start.factory -and $job[0].work -eq 0 -and $job[0].paid -eq 0 -and $job[0].held -eq 0) 'start job identity/zero ledger'
    $expected=$Start.productionBefore|ConvertTo-Json -Depth 12 -Compress|ConvertFrom-Json -Depth 12
    $expected.jobs=@(@($expected.jobs)+$job[0]|Sort-Object owner)
    Require ((ContextKey $expected) -ceq (ContextKey $Start.productionAfter)) 'start changed other state'
    $factory=@($Start.productionBefore.units|Where-Object id -eq $Start.factory)
    Require ($factory.Count -eq 1 -and $factory[0].owner -eq $Start.owner -and $factory[0].kind -eq 4 -and $factory[0].hp -gt 0 -and $factory[0].rally -eq 1) 'start live rallied source'
    foreach($n in @('rallyX','rallyY','rallyZ')){Require ($Start.$n -eq $factory[0].$n -and $Complete.$n -eq $Start.$n) 'production rally identity'}
    Require ($Advances.Count -ge 50 -and $Advances.Count -le 2200) 'bounded complete timed Advance chain'
    Require ((Integer $Complete.advanceCount 50 2200) -and $Complete.advanceCount -eq $Advances.Count) 'completion Advance count mismatch'
    # These offsets come from regex Match.Index, never from JSON properties.
    Require ($StartOffset -ge 0 -and $CompleteOffset -gt $StartOffset -and $AdvanceOffsets.Count -eq $Advances.Count) 'raw lifecycle interval'
    $previousOffset=$StartOffset
    $last=$Start;$lastJob=$job[0];$commonProgress=@();$paidTotal=0;$seen=@{}
    for($i=0;$i -lt $Advances.Count;$i++){
        $a=$Advances[$i]
        Require ((Integer $a.index 1 2200) -and $a.index -eq $i+1) 'missing/duplicate/reordered Advance index'
        Require ($AdvanceOffsets[$i] -gt $previousOffset -and $AdvanceOffsets[$i] -lt $CompleteOffset) 'Advance outside start/completion raw interval'
        $previousOffset=$AdvanceOffsets[$i]
        foreach($n in @('owner','epoch','sequence','factory','beforeRevision','revision','productionPayment','productionRefund')){Require (Integer $a.$n) "Advance integer $n"}
        foreach($n in @('owner','epoch','sequence','factory')){Require ($a.$n -eq $Start.$n) "Advance identity $n"}
        Require ($a.beforeDigest -cmatch '^[A-Fa-f0-9]{32}$' -and $a.digest -cmatch '^[A-Fa-f0-9]{32}$' -and $a.beforeRevision -ge $last.revision -and $a.revision -gt $a.beforeRevision -and -not $seen.ContainsKey([string]$a.revision)) 'Advance chronology/digest'
        $seen[[string]$a.revision]=$true
        $null=ContextKey $a.productionBefore;$null=ContextKey $a.productionAfter
        $beforeJob=@($a.productionBefore.jobs|Where-Object owner -eq $Start.owner);$afterJob=@($a.productionAfter.jobs|Where-Object owner -eq $Start.owner)
        Require ($beforeJob.Count -eq 1 -and (JobKey $beforeJob[0]) -ceq (JobKey $lastJob)) 'missing/changed intervening job ledger'
        if($a.beforeRevision -eq $last.revision){Require ($a.beforeDigest -ceq $last.digest -and (ContextKey $a.productionBefore) -ceq (ContextKey $last.productionAfter)) 'adjacent Advance context discontinuity'}
        Assert-ProductionAdvance $a.productionBefore $a.productionAfter $a.owner $a.productionPayment $a.productionRefund 100000
        $new=@($a.productionAfter.units|Where-Object{$_.id -notin $a.productionBefore.units.id})
        Require ($a.revision -eq $a.beforeRevision+1+$new.Count -and @($a.productionBefore.units|Where-Object{$_.id -notin $a.productionAfter.units.id}).Count -eq 0 -and @($new|Where-Object kind -ne 0).Count -eq 0) 'Advance exact revision/creation'
        Require ($a.productionRefund -eq 0) 'tracked completed production cannot refund'
        $paidTotal+=$a.productionPayment
        Published $a
        if($i -lt $Advances.Count-1){
            Require ($afterJob.Count -eq 1 -and $afterJob[0].sequence -eq $Start.sequence -and $afterJob[0].factory -eq $Start.factory -and $afterJob[0].held -eq $beforeJob[0].held) 'Advance replaced/removed/held tracked job'
            $lastJob=$afterJob[0]
            $key="$($a.epoch):$($a.revision)"
            if($key -in $Common -and $afterJob[0].work -gt 0 -and $afterJob[0].work -lt 5000000 -and $afterJob[0].paid -gt 0){
                Require (@(@('host','rendered','observer')|Where-Object{-not $Maps[$_].ContainsKey($key)}).Count -eq 0) 'common job missing role'
                $commonProgress+=,$key
            }
        }else{
            Require ($afterJob.Count -eq 0 -and $a.revision -eq $Complete.revision -and $a.beforeRevision -eq $Complete.beforeRevision -and $a.beforeDigest -ceq $Complete.beforeDigest -and $a.digest -ceq $Complete.digest -and $a.productionPayment -eq $Complete.productionPayment -and $a.productionRefund -eq $Complete.productionRefund -and (ContextKey $a.productionBefore) -ceq (ContextKey $Complete.productionBefore) -and (ContextKey $a.productionAfter) -ceq (ContextKey $Complete.productionAfter)) 'completion does not match final Advance'
            $newOwned=@($new|Where-Object owner -eq $Start.owner)
            Require ($newOwned.Count -eq 1 -and $newOwned[0].id -eq $Complete.unit -and $newOwned[0].kind -eq 0 -and $newOwned[0].hp -eq 100 -and $newOwned[0].cargo -eq 0 -and $newOwned[0].rally -eq 0) 'completion exact actual new Combat'
            foreach($axis in @('x','y','z')){Require ($newOwned[0].$axis -eq $Complete.('spawn'+$axis)) 'completion actual spawn'}
        }
        $last=$a
    }
    Require ($paidTotal -eq 50 -and $commonProgress.Count -ge 2) 'missing exact paid ledger or common replicated progress'
    return [ordered]@{commonProgress=$commonProgress;advanceCount=$Advances.Count;paid=$paidTotal;startRevision=$Start.revision;completionRevision=$Complete.revision}
}

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
    function Row([string]$Text,[int]$MaxLength=131072) {
        Require ($Text.Length -le $MaxLength) 'oversized event'
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
    foreach($role in @('host','rendered','observer')){
        Require ($Logs.ContainsKey($role) -and $Logs[$role] -is [string] -and $Logs[$role].Length -le 24MB -and $Logs[$role] -notmatch 'PROOF_FACTORY_RALLY_ERROR') "invalid log/runtime error $role"
        if($role -ne 'host'){Require ($Logs[$role] -notmatch 'PROOF_FACTORY_RALLY_(EVENT|ADVANCE)') 'authoritative row outside host'}
        if($role -ne 'rendered'){Require ($Logs[$role] -notmatch 'PROOF_FACTORY_RALLY_(FIXTURE|CAPTURE)') 'rendered row outside rendered'}
    }
    $eventMatches=@([regex]::Matches($Logs.host,'PROOF_FACTORY_RALLY_EVENT([^\r\n]*)'))
    $events=@($eventMatches|ForEach-Object{Row $_.Groups[1].Value})
    Require ($events.Count -eq 3 -and [regex]::Matches($Logs.host,'PROOF_FACTORY_RALLY_EVENT').Count -eq 3) 'expected rally, start, completion'
    $advanceMatches=@([regex]::Matches($Logs.host,'PROOF_FACTORY_RALLY_ADVANCE([^\r\n]*)'))
    $advances=@($advanceMatches|ForEach-Object{Row $_.Groups[1].Value})
    foreach($event in $events) {
        Require ($event.type -in @('SetRally','Produce','Complete')) 'invalid command type'
        foreach($n in @('owner','epoch','sequence','factory','beforeRevision','revision','creditsBefore','creditsAfter')){Require (Integer $event.$n) "invalid integer $n"}
        Require ($event.owner -eq 1 -and $event.epoch -gt 0 -and $event.sequence -gt 0 -and $event.factory -gt 0) 'invalid event identity'
        Require ((Integer $event.unit -1) -and $event.beforeDigest -match '^[a-fA-F0-9]{32}$' -and $event.digest -match '^[a-fA-F0-9]{32}$') 'invalid event unit/digest'
        foreach($n in @('rallyX','rallyY','rallyZ','spawnX','spawnY','spawnZ')){Require ((Number $event.$n) -and [math]::Abs([double]$event.$n) -le 10000) "invalid coordinate $n"}
        Require ($event.rallyZ -eq 0 -and $event.spawnZ -eq 0) 'non-ground event'
        foreach($n in @('beforeIds','afterIds')) {
            Require ($event.$n -is [array] -and $event.$n.Count -gt 0 -and $event.$n.Count -le 128) 'invalid ID array'
            $seen=@{};foreach($id in $event.$n){Require ((Integer $id 1) -and -not $seen.ContainsKey([string]$id)) 'invalid/duplicate ID';$seen[[string]$id]=$true}
        }
        if($event.type -eq 'SetRally'){Assert-FactoryImmediateMutation $event $maps}
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
    $set=@($events|Where-Object type -eq SetRally);$start=@($events|Where-Object type -eq Produce);$produce=@($events|Where-Object type -eq Complete)
    Require ($set.Count -eq 1 -and $start.Count -eq 1 -and $produce.Count -eq 1 -and $events[1].type -ceq 'Produce' -and $events[2].type -ceq 'Complete') 'missing/duplicate/out-of-order event type';$set=$set[0];$start=$start[0];$produce=$produce[0]
    Require ($events[0].type -eq 'SetRally' -and $set.epoch -eq $produce.epoch -and $set.factory -eq $produce.factory -and $produce.sequence -eq $set.sequence+1) 'event sequence/epoch/factory mismatch'
    Require ($set.unit -eq -1 -and $set.revision -eq $set.beforeRevision+1 -and $set.creditsBefore -eq $set.creditsAfter) 'setting rally mutated cost or revision incorrectly'
    Require ($set.factory -in $set.beforeIds -and $set.factory -in $produce.beforeIds) 'command source absent from ID sets'
    Require ((($set.beforeIds|Sort-Object) -join ',') -ceq (($set.afterIds|Sort-Object) -join ',')) 'setting rally changed unit IDs'
    Require ($produce.unit -gt 0 -and $produce.revision -gt $produce.beforeRevision -and $start.beforeRevision -ge $set.revision) 'timed production chronology'
    $timed=Get-FactoryTimedProductionLifecycle $start $produce $advances $maps $common -StartOffset $eventMatches[1].Index -CompleteOffset $eventMatches[2].Index -AdvanceOffsets @($advanceMatches|ForEach-Object{$_.Index})
    Require ($produce.unit -notin $produce.beforeIds -and $produce.unit -in $produce.afterIds -and @($produce.beforeIds|Where-Object{$_ -notin $produce.afterIds}).Count -eq 0) 'production did not add exact new unit'
    foreach($n in @('rallyX','rallyY','rallyZ')){Require ($set.$n -eq $produce.$n) 'rally changed between setting and production'}
    $factoryKey=$null;$progress=@();$spawn=[pscustomobject]@{x=$produce.spawnX;y=$produce.spawnY;z=$produce.spawnZ}
    $initialDistance=Distance $produce $spawn 'rally'
    foreach($key in $common) {
        $state=$maps.host[$key];if($state.epoch -ne $set.epoch){continue}
        $factory=Unit $state $set.factory
        if($factory.Count -ne 1 -or $factory[0].owner -ne 1 -or $factory[0].kind -ne 4 -or $factory[0].hp -le 0 -or $factory[0].rally -ne 1){continue}
        if($factory[0].rallyX -ne $set.rallyX -or $factory[0].rallyY -ne $set.rallyY -or $factory[0].rallyZ -ne $set.rallyZ){continue}
        if($state.revision -ge $set.revision -and $state.revision -lt $start.revision){$factoryKey=$key}
        $unit=Unit $state $produce.unit
        if($state.revision -gt $produce.revision -and $unit.Count -eq 1 -and $unit[0].owner -eq 1 -and $unit[0].kind -eq 0 -and $unit[0].hp -gt 0 -and $unit[0].rally -eq 0 -and (Distance $unit[0] $spawn) -gt 50 -and (Distance $produce $unit[0] 'rally') -lt $initialDistance){$progress+=,$key}
    }
    Require ($factoryKey -and $progress.Count -ge 2) 'missing common pre-production rally or later actual movement'
    $phases=@([regex]::Matches($Logs.rendered,'PROOF_FACTORY_RALLY_FIXTURE([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value 4096})
    Require ($phases.Count -eq 3) 'expected exactly three shared-handler phases'
    $names=@('rallied','spawned','moved');$previous=-1.0;$native=$null
    for($index=0;$index -lt 3;$index++) {
        $p=$phases[$index];Require ($p.phase -ceq $names[$index]) 'missing/out-of-order phase'
        foreach($n in @('owner','epoch','revision','factory','setSequence','produceSequence','selected','selectionCount','nativeRight')){Require (Integer $p.$n) "invalid phase integer $n"}
        foreach($n in @('x','y','z','rallyX','rallyY','rallyZ','elapsed')){Require (Number $p.$n) "invalid phase number $n"}
        Require ((Integer $p.unit -1) -and $p.owner -eq 1 -and $p.epoch -eq $set.epoch -and $p.factory -eq $set.factory -and $p.selected -eq $set.factory -and $p.selectionCount -eq 1 -and $p.setSequence -eq $set.sequence -and $p.elapsed -ge 0 -and $p.elapsed -gt $previous -and $p.elapsed -le 220) 'phase identity/selection/time'
        if($null -eq $native){$native=$p.nativeRight};Require ($p.nativeRight -eq $native) 'synthetic fixture changed native counter';$previous=$p.elapsed
        foreach($n in @('rallyX','rallyY','rallyZ')){Require ($p.$n -eq $set.$n) 'phase destination mismatch'}
        $key="$($p.epoch):$($p.revision)";Require ($maps.rendered.ContainsKey($key)) 'phase lacks exact rendered state'
        $f=Unit $maps.rendered[$key] $set.factory;Require ($f.Count -eq 1 -and $f[0].rally -eq 1 -and $f[0].rallyX -eq $set.rallyX -and $f[0].rallyY -eq $set.rallyY) 'phase factory disagrees with rendered state'
        if($index -eq 0){Require ($p.unit -eq -1 -and $p.produceSequence -eq 0) 'invalid initial phase'}
        else {$u=Unit $maps.rendered[$key] $produce.unit;Require ($p.unit -eq $produce.unit -and $p.produceSequence -eq $produce.sequence -and $u.Count -eq 1 -and (Distance $p $u[0]) -le 0.00001) 'phase unit disagrees with rendered state'}
    }
    Require ($set.revision -le $phases[0].revision -and $phases[0].revision -lt $start.revision -and
        $produce.revision -le $phases[1].revision -and $phases[1].revision -lt $phases[2].revision) 'shared-handler phase revision chronology'
    Require ($phases[2].elapsed-$phases[1].elapsed -ge 1 -and (Distance $phases[2] $phases[1]) -gt 50) 'no observed shared-handler movement window'
    # The existing host uses Advance(0.1): 150 cm/s permits at most 15 cm per
    # revision increment. Other commands can add revisions, making this a
    # conservative bound, never a substitute for common-state movement above.
    Require ($phases[1].revision -ge $produce.revision -and (Distance $phases[1] $spawn) -le 15*($phases[1].revision-$produce.revision)+0.00001) 'first observed unit cannot follow authoritative spawn'
    $captures=@([regex]::Matches($Logs.rendered,'(?m)PROOF_FACTORY_RALLY_CAPTURE name=03-factory-rally\.png selected=(\d+) written=1 physicalInputVerified=0(?=\r?$)'))
    Require ($captures.Count -eq 1 -and [regex]::Matches($Logs.rendered,'PROOF_FACTORY_RALLY_CAPTURE').Count -eq 1 -and [int]$captures[0].Groups[1].Value -eq $set.factory) 'missing selected-factory capture observation'
    return [ordered]@{passed=$true;factory=$set.factory;unit=$produce.unit;epoch=$set.epoch;setSequence=$set.sequence;produceSequence=$produce.sequence;commonRally=$factoryKey;commonMovement=$progress;timedProduction=$timed;phaseRows=$phases;physicalInputVerified=$false;authoritativeEventPolicy='separate accepted start and exact timed completion, independently reconciled Advance ledgers and common replicated jobs/rally/movement'}
}
