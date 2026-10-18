# Companion to Get-RefineryMinerEvidence: consumes its validated semantic states.
# Pure policy; no process launches or filesystem access.
function Get-GroupGatherEvidence {
    param([Parameter(Mandatory)][string]$Log,[Parameter(Mandatory)][hashtable]$ValidatedStateContext,
        [Parameter(Mandatory)]$RefineryEvidence)
    function Require($Condition,[string]$Reason) { if(-not $Condition){throw "Group Gather evidence: $Reason"} }
    function Integer($Value,[long]$Min=0) {
        return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and $Value -isnot [array] -and
            [double]$Value -ge $Min -and [double]$Value -le 2147483647 -and [double]$Value -eq [math]::Truncate([double]$Value)
    }
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
    function Unit($State,$Id) { return @($State.units|Where-Object id -eq $Id) }
    function DistanceSquared($A,$B) { $v=0.0;foreach($axis in @('x','y','z')){$v+=[math]::Pow(([double]$A.$axis-[double]$B.$axis),2)};return $v }
    Require ($Log.Length -le 24MB -and $Log -notmatch 'PROOF_GROUP_GATHER_ERROR|status=fail') 'runtime failure or log cap'
    Require ($RefineryEvidence.passed -eq $true -and $ValidatedStateContext.maps.ContainsKey('rendered') -and $ValidatedStateContext.maps.ContainsKey('host')) 'missing validated refinery context'
    $phases=@([regex]::Matches($Log,'PROOF_GROUP_GATHER_PHASE([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value})
    Require ($phases.Count -eq 4) 'exactly four phases required'
    $names=@('selected','queued','accepted','complete');$previousTime=-1.0;$previousRevision=-1
    $anchor=$phases[0]
    foreach($i in 0..3) {
        $p=$phases[$i];Require ($p.phase -ceq $names[$i]) 'phase order'
        foreach($n in @('owner','epoch','revision','a','b','resource','beforeSequence','nextBefore','nextNow','lastSequence','queue','count')){Require (Integer $p.$n) "invalid phase integer $n"}
        Require ($null -ne $p.elapsed -and $p.elapsed -isnot [string] -and $p.elapsed -isnot [bool] -and $p.elapsed -isnot [array] -and [double]::IsFinite([double]$p.elapsed) -and $p.elapsed -ge 0 -and $p.elapsed -ge $previousTime -and $p.elapsed -le 220 -and $p.revision -ge $previousRevision) 'phase time/revision'
        foreach($n in @('owner','epoch','a','b','resource','beforeSequence','nextBefore')){Require ($p.$n -eq $anchor.$n) 'phase identity drift'}
        Require ($p.owner -eq 1 -and $p.epoch -gt 0 -and $p.a -gt 0 -and $p.b -gt $p.a -and $p.resource -gt 0 -and $p.count -eq 2 -and $p.nextBefore -eq $p.beforeSequence+1) 'phase identity/selection'
        $key="$($p.epoch):$($p.revision)";Require ($ValidatedStateContext.maps.rendered.ContainsKey($key)) 'phase lacks rendered state'
        $state=$ValidatedStateContext.maps.rendered[$key]
        foreach($id in @($p.a,$p.b)){$u=Unit $state $id;Require ($u.Count -eq 1 -and $u[0].owner -eq 1 -and $u[0].kind -eq 1 -and $u[0].hp -gt 0) 'phase miner identity'}
        Require (@($state.resources|Where-Object id -eq $p.resource).Count -eq 1) 'phase missing resource'
        if($i -lt 2){Require ($p.nextNow -eq $p.nextBefore -and $p.lastSequence -eq $p.beforeSequence -and $p.queue -eq $(if($i -eq 0){0}else{2})) 'enqueue stamped commands or queue mismatch'}
        else {Require ($p.nextNow -eq $p.nextBefore+2 -and $p.lastSequence -eq $p.nextBefore+1 -and $p.queue -eq 0) 'accepted/drained sequence mismatch'}
        $previousTime=[double]$p.elapsed;$previousRevision=[long]$p.revision
    }
    # Queued count is exactly two, distinct from phase index one.
    Require ($phases[1].queue -eq 2) 'queue must contain both miners'
    $commands=@([regex]::Matches($Log,'PROOF_GROUP_GATHER_COMMAND([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value})
    Require ($commands.Count -eq 2) 'exactly two authoritative Gather commands required'
    foreach($i in 0..1) {
        $c=$commands[$i];foreach($n in @('owner','epoch','sequence','unit','resource','accepted','revision')){Require (Integer $c.$n) "invalid command integer $n"}
        $id=if($i -eq 0){$anchor.a}else{$anchor.b}
        Require ($c.owner -eq 1 -and $c.epoch -eq $anchor.epoch -and $c.sequence -eq $anchor.nextBefore+$i -and $c.unit -eq $id -and $c.resource -eq $anchor.resource -and $c.accepted -eq 1 -and $c.revision -ge $phases[1].revision -and $c.revision -le $phases[2].revision) 'command identity/acceptance'
        $requests=@([regex]::Matches($Log,('PROOF_SKIRMISH_REQUEST owner=1 sequence='+$c.sequence+'(?=\s|$)[^\r\n]*')))
        Require ($requests.Count -eq 1 -and $requests[0].Value -ceq ('PROOF_SKIRMISH_REQUEST owner=1 sequence='+$c.sequence+' type=Gather unit='+$id+' target='+$anchor.resource)) 'missing/duplicate/contradictory shared Request record'
    }
    Require ($commands[1].revision -gt $commands[0].revision) 'commands must be ordered'
    $grants=@($RefineryEvidence.grants|Where-Object {$_.owner -eq 1 -and $_.epoch -eq $anchor.epoch})
    Require ($grants.Count -eq 1 -and $grants[0].miner -eq $anchor.b -and $grants[0].resource -eq $anchor.resource -and $grants[0].gatherSequence -eq $anchor.nextBefore+1) 'missing real refinery-granted miner'
    $grantRows=@([regex]::Matches($Log,'PROOF_REFINERY_EVENT([^\r\n]*)')|ForEach-Object{Row $_.Groups[1].Value}|Where-Object {$_.event -eq 'grant' -and $_.owner -eq 1 -and $_.epoch -eq $anchor.epoch})
    Require ($grantRows.Count -eq 1 -and $anchor.a -in $grantRows[0].beforeIds -and $anchor.b -notin $grantRows[0].beforeIds) 'starter/granted provenance'
    $start=$ValidatedStateContext.maps.rendered["$($anchor.epoch):$($anchor.revision)"]
    $proofs=@()
    foreach($id in @($anchor.a,$anchor.b)) {
        $initial=(Unit $start $id)[0];Require ($initial.cargo -eq 0) 'miner started preloaded';$moved=$null;$loaded=$null;$deposited=$null;$prior=$null;$priorPlayer=$null
        foreach($key in $ValidatedStateContext.common) {
            $state=$ValidatedStateContext.maps.host[$key]
            if($state.epoch -ne $anchor.epoch -or $state.revision -lt $anchor.revision -or $state.revision -gt $phases[3].revision){continue}
            $units=Unit $state $id;$players=@($state.players|Where-Object id -eq 1)
            if($units.Count -ne 1 -or $players.Count -ne 1){continue};$u=$units[0];$player=$players[0]
            Require ($u.owner -eq 1 -and $u.kind -eq 1 -and $u.hp -gt 0) 'progress source changed'
            if(-not $moved -and (DistanceSquared $initial $u) -gt 1){$moved=$key}
            if(-not $loaded -and $u.cargo -gt 0){$loaded=$key}
            $depots=@($state.units|Where-Object {$_.owner -eq 1 -and $_.kind -eq 3 -and $_.hp -gt 0 -and (DistanceSquared $_ $u) -le 1})
            if($loaded -and -not $deposited -and $prior -and $prior.cargo -gt 0 -and $u.cargo -eq 0 -and $player.gathered -gt $priorPlayer.gathered -and $depots.Count -ge 1){$deposited=$key}
            $prior=$u;$priorPlayer=$player
        }
        Require ($moved -and $loaded -and $deposited) "missing real movement/load/deposit for miner $id"
        $proofs+=@{miner=$id;moved=$moved;loaded=$loaded;deposited=$deposited}
    }
    $captures=@([regex]::Matches($Log,'PROOF_GROUP_GATHER_CAPTURE[^\r\n]*'))
    Require ($captures.Count -eq 1 -and $captures[0].Value -ceq 'PROOF_GROUP_GATHER_CAPTURE name=03-group-gather.png selectedCount=2 written=1 physicalInputVerified=0') 'missing/duplicate/malformed group capture'
    return [ordered]@{passed=$true;epoch=$anchor.epoch;miners=@($anchor.a,$anchor.b);resource=$anchor.resource;sequences=@($anchor.nextBefore,($anchor.nextBefore+1));phases=$phases;proofs=$proofs;physicalInputVerified=$false}
}
