# Pure strict evidence policy. Does not launch processes, mutate state or release the fixture.
function Get-AttackMoveEvidence {
    param([Parameter(Mandatory)][hashtable]$Logs,[switch]$RequireComplete)
    function Require($Value,[string]$Why) { if(-not $Value){throw "Attack Move evidence: $Why"} }
    function Integer($V,[long]$Min=0,[long]$Max=2147483647) { return $null -ne $V -and $V -isnot [string] -and $V -isnot [bool] -and $V -isnot [array] -and [double]::IsFinite([double]$V) -and [double]$V -ge $Min -and [double]$V -le $Max -and [double]$V -eq [math]::Truncate([double]$V) }
    function Number($V) { return $null -ne $V -and $V -isnot [string] -and $V -isnot [bool] -and $V -isnot [array] -and [double]::IsFinite([double]$V) }
    function Row([string]$Text) {
        Require ($Text.Length -le 8192) 'oversized JSON observation'
        $stack=[Collections.Generic.List[object]]::new()
        foreach($m in [regex]::Matches($Text,'"(?:\\.|[^"\\])*"\s*:|[{}\[\]]')) {
            $t=$m.Value
            if($t -eq '{'){$stack.Add(@{})}elseif($t -eq '['){$stack.Add($null)}
            elseif($t -in @('}',']')){Require ($stack.Count -gt 0) 'unbalanced JSON';$stack.RemoveAt($stack.Count-1)}
            else{Require ($stack.Count -gt 0 -and $null -ne $stack[$stack.Count-1]) 'key outside object';$key=($t -replace '\s*:$',''|ConvertFrom-Json);Require (-not $stack[$stack.Count-1].ContainsKey($key)) 'duplicate JSON key';$stack[$stack.Count-1][$key]=$true}
        }
        Require ($stack.Count -eq 0) 'unbalanced JSON'
        $o=$Text|ConvertFrom-Json -Depth 12 -NoEnumerate -ErrorAction Stop
        Require ($null -ne $o -and $o -isnot [array] -and $o -isnot [string] -and $o -isnot [ValueType]) 'object required';return $o
    }
    function Unit($S,$Id) { return @($S.units|Where-Object id -eq $Id) }
    function Distance($A,$B,[string]$Prefix='') { $sum=0.0;foreach($axis in @('x','y','z')){$n=$Prefix+$axis;$sum+=[math]::Pow([double]$A.$n-[double]$B.$axis,2)};return [math]::Sqrt($sum) }
    function Point($E,[string]$Prefix='') {return [pscustomobject]@{x=$E.($Prefix+'X');y=$E.($Prefix+'Y');z=$E.($Prefix+'Z')}}
    $context=@{}
    $refinery=Get-RefineryMinerEvidence -Logs $Logs -Scenario Coop -ValidatedStateContext $context
    $maps=$context.maps;$common=$context.common
    Require ($null -ne $maps -and $common.Count -ge 3) 'validated three-role semantic context required'
    foreach($role in @('host','rendered','observer')) {
        Require ($Logs[$role] -notmatch 'PROOF_ATTACK_MOVE_ERROR') "runtime error $role"
        Require ([regex]::Matches($Logs[$role],('PROOF_ATTACK_MOVE_READY role='+$role+' status=pass\b[^\r\n]*input=synthetic physicalInput=unverified')).Count -eq 1) "missing/duplicate ready $role"
        Require ([regex]::Matches($Logs[$role],'PROOF_ATTACK_MOVE_READY').Count -eq 1) "unexpected ready $role"
        Require ([regex]::Matches($Logs[$role],'PROOF_ATTACK_MOVE_COMPLETE').Count -le 1) "extra completion $role"
        if($RequireComplete){Require ([regex]::Matches($Logs[$role],('PROOF_ATTACK_MOVE_COMPLETE role='+$role+' status=pass input=synthetic physicalInput=unverified')).Count -eq 1) "missing/duplicate completion $role"}
    }
    $events=@([regex]::Matches($Logs.host,'PROOF_ATTACK_MOVE_COMMAND([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value})
    Require ($events.Count -eq 14 -and $events.Count -le 32) 'expected exactly fourteen ordinary setup/AM/Stop commands'
    $expected=@{1=@(3,5,6,4,2,8,6);2=@(3,5,6);100=@(3,5,0,6)}
    foreach($e in $events) {
        foreach($n in @('owner','epoch','sequence','type','beforeRevision','revision','creditsBefore','creditsAfter','accepted','visibleUnchanged')){Require (Integer $e.$n) "invalid event integer $n"}
        foreach($n in @('unit','target','kind','sourceOwner')){Require (Integer $e.$n -1) "invalid signed event integer $n"}
        foreach($n in @('x','y','z','hp','destinationX','destinationY','destinationZ')){Require (Number $e.$n) "invalid event number $n"}
        Require ($e.owner -in @(1,2,100) -and $e.epoch -eq 1 -and $e.sequence -gt 0 -and $e.accepted -eq 1 -and $e.reason -ceq 'accepted') 'wrong event owner/epoch/receipt'
        Require ($e.beforeDigest -cmatch '^[a-fA-F0-9]{32}$' -and $e.digest -cmatch '^[a-fA-F0-9]{32}$' -and $e.revision -gt $e.beforeRevision) 'event digest/revision'
        foreach($n in @('beforeIds','afterIds')) {
            Require ($e.$n -is [array] -and $e.$n.Count -gt 0 -and $e.$n.Count -le 128) 'invalid ID set'
            $seen=@{};foreach($id in $e.$n){Require ((Integer $id 1) -and -not $seen.ContainsKey([string]$id)) 'duplicate/invalid unit ID';$seen[[string]$id]=$true}
        }
        foreach($pair in @(@('beforeRevision','beforeDigest'),@('revision','digest'))) {
            $key="$($e.epoch):$($e.($pair[0]))"
            if($maps.host.ContainsKey($key)){
                $state=$maps.host[$key];$ids=if($pair[0] -eq 'beforeRevision'){$e.beforeIds}else{$e.afterIds}
                $credits=if($pair[0] -eq 'beforeRevision'){$e.creditsBefore}else{$e.creditsAfter}
                Require ($state.digest -ceq $e.($pair[1]) -and (($state.units.id|Sort-Object)-join ',') -ceq (($ids|Sort-Object)-join ',')) 'published command checkpoint mismatch'
                $player=@($state.players|Where-Object id -eq $e.owner);Require ($player.Count -eq 1 -and $player[0].credits -eq $credits) 'published command credits mismatch'
            }
        }
    }
    foreach($owner in @(1,2,100)) {
        $commands=@($events|Where-Object owner -eq $owner)
        Require ($commands.Count -eq $expected[$owner].Count) "wrong command count owner $owner"
        for($i=0;$i -lt $commands.Count;$i++){Require ($commands[$i].sequence -eq $i+1 -and $commands[$i].type -eq $expected[$owner][$i]) "wrong ordered command owner $owner"}
    }
    $human=@($events|Where-Object owner -eq 1);$produce=$human[4];$am=$human[5];$stop=$human[6]
    $botMove=@($events|Where-Object{$_.owner -eq 100 -and $_.type -eq 0})[0];$target=$botMove.unit
    Require ($target -gt 0 -and $botMove.kind -eq 1 -and $botMove.sourceOwner -eq 100 -and $botMove.destinationX -eq -150 -and $botMove.destinationY -eq -700 -and $botMove.destinationZ -eq 0) 'ordinary stationary target placement'
    Require ($am.unit -gt 0 -and $stop.unit -eq $am.unit -and $am.kind -eq 0 -and $stop.kind -eq 0 -and $am.sourceOwner -eq 1 -and $stop.sourceOwner -eq 1 -and $am.hp -gt 0 -and $stop.hp -gt 0) 'AM/Stop ownership, live combat identity'
    Require ($am.target -eq -1 -and $stop.target -eq -1 -and [math]::Abs($am.destinationX-600) -le 2 -and [math]::Abs($am.destinationY+700) -le 2 -and $am.destinationZ -eq 0) 'ground Attack Move payload'
    foreach($e in @($am,$stop)) {
        Require ($e.visibleUnchanged -eq 1 -and $e.revision -eq $e.beforeRevision+1 -and $e.creditsBefore -eq $e.creditsAfter -and (($e.beforeIds|Sort-Object)-join ',') -ceq (($e.afterIds|Sort-Object)-join ',')) 'AM/Stop changed visible state, resources or IDs'
    }
    Require ($produce.kind -eq 4 -and $produce.sourceOwner -eq 1 -and $produce.hp -gt 0 -and $produce.creditsBefore-$produce.creditsAfter -eq 50 -and $produce.revision -eq $produce.beforeRevision+2) 'paid production contract'
    $added=@($produce.afterIds|Where-Object{$_ -notin $produce.beforeIds})
    Require ($added.Count -eq 1 -and $added[0] -eq $am.unit -and @($produce.beforeIds|Where-Object{$_ -notin $produce.afterIds}).Count -eq 0) 'production did not add exact Attack Move unit'
    Require ($produce.revision -lt $am.revision -and $am.revision -lt $stop.revision) 'command chronology'
    $origin=[pscustomobject]@{x=$am.x;y=$am.y;z=$am.z};$destination=Point $am 'destination';$stopPoint=[pscustomobject]@{x=$stop.x;y=$stop.y;z=$stop.z}
    Require ([math]::Abs($am.x-($produce.x+300)) -le .001 -and [math]::Abs($am.y-$produce.y) -le .001 -and $am.z -eq $produce.z) 'combat source does not match ordinary first production spawn'
    Require ((Distance $stopPoint $destination) -gt 100) 'Stop must override an unfinished route'
    foreach($seq in @(6,7)) {
        $type=if($seq -eq 6){'AttackMove'}else{'Stop'}
        Require ([regex]::Matches($Logs.rendered,("PROOF_SKIRMISH_REQUEST owner=1 sequence=$seq type=$type unit=$($am.unit) target=-1\b")).Count -eq 1) 'missing shared Request/queue route'
        Require ($Logs.rendered -match ("PROOF_RESULT owner=1 sequence=$seq accepted=1\b")) 'missing actual accepted client receipt'
    }
    $clock=@{};$priorTime=-1.0;$priorRevision=-1
    $clockRows=@([regex]::Matches($Logs.host,'PROOF_ATTACK_MOVE_CLOCK([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value})
    Require ($clockRows.Count -ge 3 -and $clockRows.Count -le 1024) 'missing/excess actual host clock rows'
    foreach($c in $clockRows) {
        Require ((Integer $c.epoch 1) -and $c.epoch -eq 1 -and (Integer $c.revision 1) -and (Number $c.simulatedSeconds) -and $c.simulatedSeconds -ge 0) 'invalid clock envelope'
        $key="$($c.epoch):$($c.revision)"
        Require (-not $clock.ContainsKey($key) -and $maps.host.ContainsKey($key) -and $maps.host[$key].digest -ceq $c.digest) 'clock not bound to exact published host state'
        Require ($c.revision -gt $priorRevision -and $c.simulatedSeconds -ge $priorTime) 'clock/revision regression'
        if($priorRevision -ge 0){Require ($c.simulatedSeconds-$priorTime -le .10000001*($c.revision-$priorRevision)+.000001) 'clock exceeds actual bounded advance envelope'}
        $clock[$key]=[double]$c.simulatedSeconds;$priorTime=[double]$c.simulatedSeconds;$priorRevision=[int]$c.revision
    }
    foreach($key in $maps.host.Keys){Require ($clock.ContainsKey($key)) 'published host state missing actual clock'}
    $phases=@([regex]::Matches($Logs.rendered,'PROOF_ATTACK_MOVE_PHASE([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value})
    Require ($phases.Count -eq 4) 'expected exactly four measured shared-handler phases'
    $phaseNames=@('moving','firing','resumed','stopped');$lastRevision=0;$lastElapsed=-1.0
    for($i=0;$i -lt 4;$i++) {
        $p=$phases[$i]
        foreach($n in @('epoch','revision','unit','target','sequence','selected','queue','nativeRight','nativeStop')){Require (Integer $p.$n) "invalid phase integer $n"}
        foreach($n in @('x','y','z','targetHP','elapsed')){Require (Number $p.$n) "invalid phase number $n"}
        Require ($p.phase -ceq $phaseNames[$i] -and $p.epoch -eq 1 -and $p.unit -eq $am.unit -and $p.target -eq $target -and $p.selected -eq $am.unit -and $p.queue -eq 0 -and $p.nativeRight -eq 0 -and $p.nativeStop -eq 0) 'phase identity/selection/native counter'
        Require ($p.sequence -eq $(if($i -eq 3){7}else{6}) -and $p.revision -gt $lastRevision -and $p.elapsed -gt $lastElapsed -and $p.elapsed -le 55) 'phase sequence/revision/time chronology'
        $key="$($p.epoch):$($p.revision)";Require ($maps.rendered.ContainsKey($key)) 'phase not actually received by rendered client'
        $state=$maps.rendered[$key];$u=Unit $state $am.unit;$t=Unit $state $target
        Require ($state.digest -ceq $p.digest -and $u.Count -eq 1 -and $t.Count -eq 1 -and (Distance $p $u[0]) -le .00001 -and $t[0].hp -eq $p.targetHP) 'phase differs from actual received semantic state'
        $lastRevision=$p.revision;$lastElapsed=$p.elapsed
    }
    Require ($am.revision -le $phases[0].revision -and $phases[2].revision -lt $stop.revision -and $stop.revision -le $phases[3].revision) 'phase lies outside accepted AM/Stop command window'
    $pausePoint=$phases[1]
    Require ((Distance $phases[0] $origin) -gt 50 -and $phases[0].targetHP -eq 100 -and $phases[1].targetHP -gt 0 -and $phases[1].targetHP -lt 100 -and $phases[2].targetHP -eq 0 -and $phases[3].targetHP -eq 0) 'travel/firing/death phases'
    Require ((Distance $phases[2] $pausePoint) -gt 50 -and (Distance $phases[2] $destination) -lt (Distance $pausePoint $destination) -and (Distance $phases[3] $stopPoint) -le .0001) 'resume direction or Stop position'
    $travel=@();$pause=@();$dead=@();$resume=@();$stopped=@();$spawn=@()
    foreach($key in $common) {
        $state=$maps.host[$key];if($state.epoch -ne 1){continue}
        Require ($clock.ContainsKey($key)) 'common state missing host clock'
        $u=Unit $state $am.unit;$t=Unit $state $target
        if($u.Count -ne 1 -or $t.Count -ne 1){continue};$u=$u[0];$t=$t[0]
        Require ($u.owner -eq 1 -and $u.kind -eq 0 -and $u.hp -gt 0 -and $t.owner -eq 100 -and $t.kind -eq 1) 'common ownership/role/liveness'
        if($state.revision -ge $produce.revision -and $state.revision -le $am.beforeRevision -and (Distance $u $origin) -le .0001){$spawn+=,$key}
        if($state.revision -lt $am.revision){continue}
        Require ($state.phase -eq 1 -and $state.winner -eq -1 -and $t.x -eq -150 -and $t.y -eq -700 -and $t.z -eq 0) 'unexpected terminal or moving target'
        $record=[pscustomobject]@{key=$key;revision=$state.revision;time=$clock[$key];hp=$t.hp}
        if($state.revision -lt $stop.revision -and $t.hp -eq 100 -and (Distance $u $origin) -gt 50 -and (Distance $u $t) -gt 300){$travel+=,$record}
        if($state.revision -lt $stop.revision -and $t.hp -gt 0 -and $t.hp -lt 100 -and (Distance $u $pausePoint) -le .0001 -and (Distance $u $t) -le 300){$pause+=,$record}
        if($t.hp -eq 0){$dead+=,$record}
        if($state.revision -lt $stop.revision -and $t.hp -eq 0 -and (Distance $u $pausePoint) -gt 50 -and (Distance $u $destination) -lt (Distance $pausePoint $destination)){$resume+=,$record}
        if($state.revision -ge $stop.revision -and $t.hp -eq 0 -and (Distance $u $stopPoint) -le .0001){$stopped+=,$record}
    }
    Require ($spawn.Count -ge 1 -and $travel.Count -ge 1 -and $pause.Count -ge 3 -and $dead.Count -ge 1 -and $resume.Count -ge 1 -and $stopped.Count -ge 3) 'missing common spawn/travel/pause/death/resume/Stop states'
    $pause=@($pause|Sort-Object revision);$stopped=@($stopped|Sort-Object revision)
    $span=$pause[-1].time-$pause[0].time;$damage=$pause[0].hp-$pause[-1].hp
    Require ($span -ge 1 -and [math]::Abs($damage-25*$span) -le .01 -and $damage -gt 0) 'stationary firing lacks one actual host second at current DPS'
    Require ($stopped[-1].time-$stopped[0].time -ge 1) 'Stop lacks one actual host second of common stationary state'
    Require ($travel[0].revision -lt $pause[0].revision -and $pause[-1].revision -lt $resume[0].revision -and $resume[0].revision -lt $stop.revision) 'common state chronology'
    return [ordered]@{passed=$true;refineryEvidence=$refinery;epoch=1;unit=$am.unit;target=$target;attackMoveSequence=6;stopSequence=7;commonTravel=$travel;commonPause=$pause;commonResume=$resume;commonStopped=$stopped;firingHostSeconds=$span;firingDamage=$damage;phaseRows=$phases;physicalInputVerified=$false;internetTest=$false;performanceTest=$false;proofScope='synthetic shared handlers; three-role visible-state equality and actual-host-clock travel/fire/death/resume/Stop'}
}
