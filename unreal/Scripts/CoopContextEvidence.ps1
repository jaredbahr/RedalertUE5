# Pure, opt-in policy. The caller first validates shared refinery states and rally evidence.
function Get-CoopContextEvidence {
    param([Parameter(Mandatory)][hashtable]$Logs,[Parameter(Mandatory)][hashtable]$ValidatedStateContext,[Parameter(Mandatory)]$RallyEvidence)
    function Require($Ok,[string]$Why){if(-not $Ok){throw "Coop context evidence: $Why"}}
    function Num($V){return $null -ne $V -and $V -isnot [string] -and $V -isnot [bool] -and $V -isnot [array] -and [double]::IsFinite([double]$V)}
    function Int($V,[long]$Min=0,[long]$Max=2147483647){return (Num $V) -and $V -ge $Min -and $V -le $Max -and $V -eq [math]::Truncate([double]$V)}
    function Row([string]$Text,[string[]]$Fields){
        Require ($Text.Length -gt 0 -and $Text.Length -le 4096 -and $Text -notmatch '[^\x00-\x7F]') 'empty/oversized/non-ASCII JSON'
        $doc=$null
        try{
            $doc=[System.Text.Json.JsonDocument]::Parse($Text);$root=$doc.RootElement
            Require ($root.ValueKind -eq [System.Text.Json.JsonValueKind]::Object) 'JSON object required'
            $keys=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            foreach($p in $root.EnumerateObject()){Require ($keys.Add($p.Name) -and $p.Name -cin $Fields) 'duplicate/unknown key'}
            Require ($keys.Count -eq $Fields.Count) 'missing key'
            return ($Text|ConvertFrom-Json -Depth 4 -ErrorAction Stop)
        }finally{if($doc){$doc.Dispose()}}
    }
    function Unit($State,$Id){$found=@($State.units|Where-Object id -eq $Id);Require ($found.Count -eq 1 -and $found[0].hp -gt 0) 'missing/dead phase unit';return $found[0]}
    function Team($State,$Owner){$found=@($State.players|Where-Object id -eq $Owner);Require ($found.Count -eq 1 -and (Int $found[0].team)) 'unknown team';return $found[0].team}
    function Dist($A,$B){return [math]::Sqrt([math]::Pow(([double]$A.x-$B.x),2)+[math]::Pow(([double]$A.y-$B.y),2)+[math]::Pow(([double]$A.z-$B.z),2))}
    $maps=$ValidatedStateContext.maps;$common=@($ValidatedStateContext.common)
    Require ($null -ne $maps -and $common.Count -ge 3 -and $RallyEvidence.passed -is [bool] -and $RallyEvidence.passed) 'validated states/rally required'
    foreach($role in @('host','rendered','observer')){Require ($Logs.ContainsKey($role) -and $null -ne $maps[$role] -and ([string]$Logs[$role]).Length -le 24MB -and $Logs[$role] -notmatch 'PROOF_COOP_CONTEXT_ERROR') 'missing role/map or runtime error'}
    $commands=@();$phases=@();$seen=@{};$lastCommandRevision=-1
    $commandFields=@('owner','epoch','sequence','type','unit','target','x','y','z','accepted','revision')
    $phaseFields=@('phase','epoch','revision','beforeSequence','sequence','unit','secondary','target','x','y','z','nativeRight','elapsed')
    foreach($role in @('host','rendered','observer')){
        foreach($m in [regex]::Matches($Logs[$role],'PROOF_COOP_CONTEXT_COMMAND([^\r\n]*)')){
            Require ($role -ceq 'host' -and $commands.Count -lt 256) 'command role/cap'
            $e=Row $m.Groups[1].Value.Trim() $commandFields
            foreach($k in @('owner','epoch','sequence','type','unit','revision','accepted')){Require (Int $e.$k) "invalid command $k"}
            Require ($e.owner -eq 1 -and $e.epoch -gt 0 -and $e.sequence -gt 0 -and $e.type -le 8 -and (Int $e.target -1) -and $e.accepted -le 1 -and $e.revision -ge $lastCommandRevision) 'command identity/type/order'
            foreach($k in @('x','y','z')){Require ((Num $e.$k) -and [math]::Abs($e.$k) -le 10000) 'invalid command destination'}
            $lastCommandRevision=$e.revision
            if($e.accepted){$key="$($e.epoch):$($e.sequence)";Require (-not $seen.ContainsKey($key)) 'duplicate accepted sequence';$seen[$key]=$true}
            $commands+=$e
        }
        foreach($m in [regex]::Matches($Logs[$role],'PROOF_COOP_CONTEXT_PHASE([^\r\n]*)')){
            Require ($role -ceq 'rendered' -and $phases.Count -lt 10) 'phase role/cap'
            $p=Row $m.Groups[1].Value.Trim() $phaseFields
            Require ($p.phase -is [string]) 'phase name required'
            foreach($k in @('epoch','revision','beforeSequence','sequence','unit','nativeRight')){Require (Int $p.$k) "invalid phase $k"}
            Require ($p.epoch -gt 0 -and $p.unit -gt 0 -and (Int $p.secondary -1) -and (Int $p.target 1) -and $p.nativeRight -eq 0 -and (Num $p.elapsed) -and $p.elapsed -ge 0 -and $p.elapsed -le 220) 'phase identity/native/time'
            foreach($k in @('x','y','z')){Require ((Num $p.$k) -and [math]::Abs($p.$k) -le 10000) 'invalid phase destination'}
            $phases+=$p
        }
    }
    $names=@('factory_hostile_rejected','factory_allied_rally','single_ally_issued','single_ally_moved','group_ally_issued','group_ally_moved','single_enemy_issued','group_enemy_issued','resource_issued','context_complete')
    Require ($phases.Count -eq 10 -and ($phases.phase -join ',') -ceq ($names -join ',')) 'missing/reordered phases'
    $epoch=$phases[0].epoch;$lastRevision=-1;$lastElapsed=-1.0
    for($index=0;$index -lt 10;$index++){
        $p=$phases[$index];$key="$epoch`:$($p.revision)"
        Require ($p.epoch -eq $epoch -and $p.revision -ge $lastRevision -and $p.elapsed -ge $lastElapsed -and $maps.rendered.ContainsKey($key)) 'phase chronology/state'
        $s=$maps.rendered[$key];$u=Unit $s $p.unit
        Require ($u.owner -eq 1 -and $s.epoch -eq $epoch -and $s.phase -eq 1) 'phase unit/playing state'
        $ownTeam=Team $s 1
        if($index -in @(0,1)){Require ($u.kind -eq 4 -and $p.unit -eq $RallyEvidence.factory -and $p.secondary -eq -1) 'factory identity'}
        elseif($index -in @(8,9)){Require ($u.kind -eq 1 -and $p.secondary -eq -1) 'resource actor';$r=@($s.resources|Where-Object id -eq $p.target);Require ($r.Count -eq 1 -and ($r[0].remaining -gt 0 -or $u.cargo -gt 0)) 'missing/depleted resource';Require ((Dist $p $r[0]) -le 2) 'projected click does not match resource position'}
        else{Require ($u.kind -eq 0) 'combat actor';if($index -in @(4,5,7)){$other=Unit $s $p.secondary;Require ($other.owner -eq 1 -and $other.kind -eq 1 -and $other.id -ne $u.id) 'group harvester'}else{Require ($p.secondary -eq -1) 'single selection'}}
        if($index -lt 8){
            $target=Unit $s $p.target;Require ($target.kind -eq 2 -and $target.owner -ne 1) 'HQ target'
            Require ((Dist $p $target) -le 2) 'projected click does not match HQ position'
            $targetTeam=Team $s $target.owner
            if($index -in @(1,2,3,4,5)){Require ($targetTeam -eq $ownTeam) 'ally target is not allied'}else{Require ($targetTeam -ne $ownTeam) 'hostile target is not hostile'}
        }
        $lastRevision=$p.revision;$lastElapsed=$p.elapsed
    }
    Require ($phases[0].revision -eq $phases[1].revision -and $phases[0].elapsed -eq $phases[1].elapsed) 'factory negative and rally were not synchronous'
    foreach($index in 2..7){Require ($phases[$index].unit -eq $RallyEvidence.unit) 'context combat differs from produced rally unit'}
    Require ($phases[4].secondary -eq $phases[7].secondary -and $phases[8].unit -eq $phases[4].secondary) 'group/resource harvester changed'
    foreach($index in 2..5){Require ($phases[$index].target -eq $phases[1].target) 'allied HQ changed'}
    foreach($index in @(6,7)){Require ($phases[$index].target -eq $phases[0].target) 'hostile HQ changed'}
    Require ($RallyEvidence.epoch -eq $epoch -and $phases[0].sequence -eq $phases[0].beforeSequence -and $phases[1].beforeSequence -eq $phases[0].sequence -and $phases[1].sequence -eq $phases[1].beforeSequence+1 -and $phases[1].sequence -eq $RallyEvidence.setSequence) 'factory sequence/rally binding'
    $rally=$RallyEvidence.phaseRows[0]
    Require ((Dist $phases[1] ([pscustomobject]@{x=$rally.rallyX;y=$rally.rallyY;z=$rally.rallyZ})) -le 0.00001) 'allied rally destination mismatch'
    function Action($Phase,[int]$Count){
        $rows=@($commands|Where-Object {$_.epoch -eq $epoch -and $_.sequence -gt $Phase.beforeSequence -and $_.sequence -le $Phase.beforeSequence+$Count})
        Require ($rows.Count -eq $Count -and @($rows|Where-Object accepted -ne 1).Count -eq 0) 'missing/rejected/extra action commands'
        foreach($row in $rows){Require ($row.revision -gt $Phase.revision -and $row.revision -le $phases[9].revision) 'command outside observed action/completion interval'}
        $ordered=@($rows|Sort-Object sequence)
        for($j=0;$j -lt $Count;$j++){Require ($ordered[$j].sequence -eq $Phase.beforeSequence+$j+1) 'action sequence gap'}
        return ,$ordered
    }
    function SameAction($A,$B){foreach($k in @('beforeSequence','unit','secondary','target')){Require ($A.$k -eq $B.$k) 'action identity changed'};Require ((Dist $A $B) -le 0.00001) 'action destination changed'}
    SameAction $phases[2] $phases[3];SameAction $phases[4] $phases[5];SameAction $phases[8] $phases[9]
    Require ($phases[2].sequence -eq $phases[2].beforeSequence+1 -and $phases[3].sequence -eq $phases[2].sequence -and $phases[4].sequence -eq $phases[4].beforeSequence -and $phases[5].sequence -eq $phases[4].beforeSequence+2 -and $phases[6].sequence -eq $phases[6].beforeSequence+1 -and $phases[7].sequence -eq $phases[7].beforeSequence -and $phases[8].sequence -eq $phases[8].beforeSequence+1 -and $phases[9].sequence -eq $phases[8].sequence) 'issued/moved sequence mismatch'
    Require ($phases[4].beforeSequence -eq $phases[3].sequence -and $phases[6].beforeSequence -eq $phases[5].sequence -and $phases[7].beforeSequence -eq $phases[6].sequence -and $phases[8].beforeSequence -eq $phases[7].beforeSequence+1) 'context action continuity'
    $set=Action $phases[1] 1;Require ($set[0].type -eq 7 -and $set[0].unit -eq $phases[1].unit -and $set[0].target -eq -1 -and (Dist $set[0] $phases[1]) -le 0.00001) 'allied factory did not SetRally'
    Require ($phases[2].beforeSequence -eq $RallyEvidence.produceSequence) 'post-rally action sequence gap'
    $single=Action $phases[2] 1;Require ($single[0].type -eq 0 -and $single[0].unit -eq $phases[2].unit -and $single[0].target -eq -1 -and (Dist $single[0] $phases[2]) -le 0.00001) 'allied single did not Move'
    Require ($single[0].revision -le $phases[3].revision) 'single Move receipt after moved phase'
    $group=Action $phases[4] 2;$source=$maps.rendered["$epoch`:$($phases[4].revision)"]
    $actors=@((Unit $source $phases[4].unit),(Unit $source $phases[4].secondary))|Sort-Object id
    $center=[pscustomobject]@{x=($actors[0].x+$actors[1].x)/2;y=($actors[0].y+$actors[1].y)/2;z=($actors[0].z+$actors[1].z)/2}
    for($j=0;$j -lt 2;$j++){
        $dest=[pscustomobject]@{x=$phases[4].x+$actors[$j].x-$center.x;y=$phases[4].y+$actors[$j].y-$center.y;z=$phases[4].z+$actors[$j].z-$center.z}
        Require ($group[$j].type -eq 0 -and $group[$j].unit -eq $actors[$j].id -and $group[$j].target -eq -1 -and (Dist $group[$j] $dest) -le 0.00001) 'allied group did not preserve formation Move'
        Require ($group[$j].revision -le $phases[5].revision) 'group Move receipt after moved phase'
    }
    foreach($index in @(6,7)){$attack=Action $phases[$index] 1;Require ($attack[0].type -eq 1 -and $attack[0].unit -eq $phases[$index].unit -and $attack[0].target -eq $phases[$index].target -and $attack[0].x -eq 0 -and $attack[0].y -eq 0 -and $attack[0].z -eq 0 -and $attack[0].revision -le $phases[$index+1].revision) 'hostile command was not canonical Combat Attack before next action'}
    $gather=Action $phases[8] 1;Require ($gather[0].type -eq 5 -and $gather[0].unit -eq $phases[8].unit -and $gather[0].target -eq $phases[8].target -and $gather[0].x -eq 0 -and $gather[0].y -eq 0 -and $gather[0].z -eq 0 -and $gather[0].revision -le $phases[9].revision -and $phases[9].elapsed-$phases[8].elapsed -ge .5) 'resource Gather/completion missing'
    function Movement($Issued,$Moved,$Next,$Events){
        Require ($Moved.elapsed-$Issued.elapsed -ge 1) 'movement window too short'
        $source=$maps.rendered["$epoch`:$($Issued.revision)"];$proof=@()
        foreach($key in $common){
            $s=$maps.host[$key]
            if($s.epoch -ne $epoch -or $s.revision -lt ($Events|Measure-Object revision -Maximum).Maximum -or $s.revision -gt $Moved.revision -or $s.revision -ge $Next.revision){continue}
            $ok=$true
            foreach($event in $Events){$origin=Unit $source $event.unit;$live=Unit $s $event.unit;Require ($live.owner -eq 1 -and $live.kind -eq $origin.kind) 'movement actor ownership/kind changed';if((Dist $origin $live) -le 50 -or (Dist $live $event) -ge (Dist $origin $event)){$ok=$false}}
            if($ok){$proof+=$key}
        }
        Require ($proof.Count -ge 2) 'missing common three-role movement toward command'
        foreach($event in $Events){$origin=Unit $source $event.unit;$end=Unit $maps.rendered["$epoch`:$($Moved.revision)"] $event.unit;Require ((Dist $origin $end) -gt 50 -and (Dist $end $event) -lt (Dist $origin $event)) 'moved phase lacks actual progress'}
        return ,$proof
    }
    $singleMovement=Movement $phases[2] $phases[3] $phases[4] $single
    $groupMovement=Movement $phases[4] $phases[5] $phases[6] $group
    Require (@($commands|Where-Object {$_.epoch -ne $epoch}).Count -eq 0) 'cross epoch command trace'
    return [ordered]@{passed=$true;epoch=$epoch;phases=$phases;commandCount=$commands.Count;singleCommonMovement=$singleMovement;groupCommonMovement=$groupMovement;inputSource='sharedActionHandler';pointerSource='projectedSynthetic';nativeRight=0;physicalInputVerified=$false;osWindowFocusVerified=$false}
}
