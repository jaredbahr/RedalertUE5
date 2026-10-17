# Pure opt-in Stop evidence policy. No files, processes or clocks are changed.
function Get-StopControlsEvidence {
    param([Parameter(Mandatory)][hashtable]$Logs,[ValidateSet(0,1)][int]$ExpectedNativeStopDelta=0)
    function Require($Condition,[string]$Reason){if(-not $Condition){throw "Stop evidence: $Reason"}}
    function Number($V){return $null -ne $V -and $V -isnot [string] -and $V -isnot [bool] -and $V -isnot [array] -and [double]::IsFinite([double]$V)}
    function Integer($V,[long]$Min=0,[long]$Max=2147483647){return (Number $V) -and [double]$V -ge $Min -and [double]$V -le $Max -and [double]$V -eq [math]::Truncate([double]$V)}
    function Check-Keys($Element) {
        if($Element.ValueKind -eq [System.Text.Json.JsonValueKind]::Object){
            $keys=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            foreach($property in $Element.EnumerateObject()){Require ($keys.Add($property.Name)) 'duplicate JSON key';Check-Keys $property.Value}
        }elseif($Element.ValueKind -eq [System.Text.Json.JsonValueKind]::Array){foreach($item in $Element.EnumerateArray()){Check-Keys $item}}
    }
    function Json([string]$Text) {
        Require ($Text.Length -gt 0 -and $Text.Length -le 32768 -and $Text -notmatch '[^\x00-\x7F]') 'empty, non-ASCII or oversized JSON row'
        $document=$null
        try{$document=[System.Text.Json.JsonDocument]::Parse($Text);Require ($document.RootElement.ValueKind -eq [System.Text.Json.JsonValueKind]::Object) 'JSON object required';Check-Keys $document.RootElement}
        finally{if($document){$document.Dispose()}}
        return ($Text|ConvertFrom-Json -Depth 20 -ErrorAction Stop)
    }
    function Check-State($S) {
        Require ((Integer $S.epoch) -and (Integer $S.revision) -and $S.digest -is [string] -and $S.digest -cmatch '^[0-9a-f]{32}$') 'invalid state identity/digest'
        Require ((Integer $S.skirmish 0 1) -and (Integer $S.phase 0 2) -and (Integer $S.winner -1 1)) 'invalid state phase'
        foreach($field in @('units','players','resources')){Require ($S.$field -is [array] -and $S.$field.Count -le 128) "invalid $field array"}
        $ids=@{}
        foreach($u in $S.units){
            Require ((Integer $u.id 1) -and -not $ids.ContainsKey([long]$u.id) -and (Integer $u.owner) -and (Integer $u.kind 0 4) -and (Integer $u.cargo 0 50)) 'invalid/duplicate unit'
            $ids[[long]$u.id]=$true
            Require (Integer $u.rally 0 1) 'invalid rally flag'
            foreach($k in @('rallyX','rallyY','rallyZ')){Require ((Number $u.$k) -and [math]::Abs([double]$u.$k) -le 10000) 'invalid rally coordinate'}
            Require ($u.rallyZ -eq 0 -and (($u.rally -eq 0 -and $u.rallyX -eq 0 -and $u.rallyY -eq 0) -or ($u.rally -eq 1 -and $u.kind -eq 4 -and $u.hp -gt 0))) 'noncanonical or ineligible rally state'
            foreach($k in @('x','y','z','hp')){Require (Number $u.$k) 'non-finite unit field'}
            Require ($u.hp -ge 0 -and $u.hp -le 300 -and [math]::Abs($u.x) -le 10000 -and [math]::Abs($u.y) -le 10000 -and [math]::Abs($u.z) -le 10000) 'unit outside proof bounds'
        }
        $ids=@{}
        foreach($p in $S.players){Require ((Integer $p.id) -and -not $ids.ContainsKey([long]$p.id) -and (Integer $p.team) -and (Integer $p.credits) -and (Integer $p.gathered)) 'invalid/duplicate player';$ids[[long]$p.id]=$true}
        $ids=@{}
        foreach($r in $S.resources){Require ((Integer $r.id 1) -and -not $ids.ContainsKey([long]$r.id) -and (Integer $r.owner) -and (Integer $r.remaining)) 'invalid/duplicate resource';$ids[[long]$r.id]=$true;foreach($k in @('x','y','z')){Require (Number $r.$k) 'invalid resource position'}}
    }
    function Semantic($S,[switch]$WithoutRevision){
        $units=@($S.units|Sort-Object id|ForEach-Object{[ordered]@{id=[long]$_.id;owner=[long]$_.owner;kind=[long]$_.kind;x=[double]$_.x;y=[double]$_.y;z=[double]$_.z;hp=[double]$_.hp;cargo=[long]$_.cargo;rally=[long]$_.rally;rallyX=[double]$_.rallyX;rallyY=[double]$_.rallyY;rallyZ=[double]$_.rallyZ}})
        $players=@($S.players|Sort-Object id|ForEach-Object{[ordered]@{id=[long]$_.id;team=[long]$_.team;credits=[long]$_.credits;gathered=[long]$_.gathered}})
        $resources=@($S.resources|Sort-Object id|ForEach-Object{[ordered]@{id=[long]$_.id;owner=[long]$_.owner;x=[double]$_.x;y=[double]$_.y;z=[double]$_.z;remaining=[long]$_.remaining}})
        $value=[ordered]@{epoch=[long]$S.epoch;skirmish=[long]$S.skirmish;phase=[long]$S.phase;winner=[long]$S.winner;units=$units;players=$players;resources=$resources}
        if(-not $WithoutRevision){$value.revision=[long]$S.revision;$value.digest=$S.digest}
        return ($value|ConvertTo-Json -Depth 12 -Compress)
    }
    function Unit($S,$Id){return @($S.units|Where-Object id -eq $Id)}
    function Distance($A,$B){return [math]::Sqrt([math]::Pow(([double]$A.x-[double]$B.x),2)+[math]::Pow(([double]$A.y-[double]$B.y),2)+[math]::Pow(([double]$A.z-[double]$B.z),2))}
    function Destination($E){return [pscustomobject]@{x=$E.destinationX;y=$E.destinationY;z=$E.destinationZ}}
    $maps=@{};$counts=@{};$commands=@();$fixtures=@();$acceptedKeys=@{}
    foreach($role in @('host','rendered','observer')){
        Require ($Logs.ContainsKey($role)) "missing role $role"
        $text=[string]$Logs[$role];Require ($text.Length -le 16MB -and $text -notmatch 'PROOF_STOP_ERROR') 'oversized log or runtime evidence error'
        $map=@{};$rows=0;$bytes=0;$lastRevision=-1;$lastSeconds=-1.0
        $normal=@{};$view=if($role -eq 'host'){'host'}else{'client'};$viewer=if($role -eq 'host'){0}elseif($role -eq 'rendered'){1}else{2}
        foreach($n in [regex]::Matches($text,'PROOF_SNAPSHOT role=(host|client) owner=(\d+)(?: epoch=(\d+))? revision=(\d+) digest=([a-f0-9]{32})')){
            if($n.Groups[1].Value -cne $view -or [long]$n.Groups[2].Value -ne $viewer){continue}
            Require (-not $n.Groups[3].Success -or [long]$n.Groups[3].Value -eq 0) 'ordinary snapshot epoch mismatch'
            $key=[long]$n.Groups[4].Value;$digest=$n.Groups[5].Value
            if($normal.ContainsKey($key)){Require ($normal[$key] -ceq $digest) 'contradictory ordinary digest'};$normal[$key]=$digest
        }
        foreach($m in [regex]::Matches($text,'PROOF_STOP_STATE([^\r\n]*)')){
            $row=$m.Groups[1].Value.Trim();$bytes+=$row.Length;$rows++;Require ($rows -le 1024 -and $bytes -le 4MB) 'snapshot evidence cap'
            $s=Json $row;Check-State $s
            $view=if($role -eq 'host'){'host'}else{'client'};$viewer=if($role -eq 'host'){0}elseif($role -eq 'rendered'){1}else{2}
            Require ($s.role -ceq $view -and (Integer $s.viewer) -and $s.viewer -eq $viewer) 'wrong state role/viewer'
            Require ($s.epoch -eq 0 -and $s.skirmish -eq 0 -and $s.phase -eq 0 -and $s.winner -eq -1 -and $s.revision -gt $lastRevision) 'fixture epoch/mode or non-increasing revision'
            if($role -eq 'host'){Require ((Number $s.simulatedSeconds) -and $s.simulatedSeconds -ge $lastSeconds) 'invalid host simulation clock';$lastSeconds=[double]$s.simulatedSeconds}
            Require ($normal.ContainsKey([long]$s.revision) -and $normal[[long]$s.revision] -ceq $s.digest) 'Stop snapshot disagrees with ordinary digest'
            $lastRevision=[long]$s.revision;$map[[long]$s.revision]=$s
        }
        Require ($map.Count -ge 3) "missing state rows in $role"
        $maps[$role]=$map;$counts[$role]=$rows
        $rows=0
        foreach($m in [regex]::Matches($text,'PROOF_STOP_COMMAND([^\r\n]*)')){
            Require ($role -eq 'host') 'command event outside host'
            $row=$m.Groups[1].Value.Trim();$bytes+=$row.Length;$rows++;Require ($rows -le 32 -and $bytes -le 4MB) 'command evidence cap'
            $e=Json $row
            Require ($e.role -ceq 'host' -and (Integer $e.owner) -and (Integer $e.unit 1) -and (Integer $e.sequence 1) -and (Integer $e.epoch) -and $e.epoch -eq 0 -and $e.type -cin @('Move','Stop') -and (Integer $e.accepted 0 1) -and (Integer $e.immutable 0 1) -and $e.immutable -eq 1) 'invalid command envelope'
            Require ((Integer $e.beforeRevision) -and (Integer $e.afterRevision)) 'invalid command revision types'
            foreach($k in @('destinationX','destinationY','destinationZ')){Require (Number $e.$k) 'invalid actual command destination'}
            Check-State $e.before;Check-State $e.after
            Require ($e.before.skirmish -eq 0 -and $e.after.skirmish -eq 0 -and $e.before.phase -eq 0 -and $e.after.phase -eq 0 -and $e.before.winner -eq -1 -and $e.after.winner -eq -1) 'command state outside legacy fixture mode'
            Require ($e.before.epoch -eq $e.epoch -and $e.after.epoch -eq $e.epoch -and $e.beforeRevision -eq $e.before.revision -and $e.afterRevision -eq $e.after.revision -and $e.beforeDigest -ceq $e.before.digest -and $e.afterDigest -ceq $e.after.digest) 'command state identity mismatch'
            Require ((Semantic $e.before -WithoutRevision) -ceq (Semantic $e.after -WithoutRevision)) 'command changed visible gameplay state'
            Require ($e.afterRevision -eq ($e.beforeRevision+$e.accepted)) 'command revision delta'
            if($e.accepted -eq 0){Require ($e.beforeDigest -ceq $e.afterDigest) 'rejection mutated digest'}
            if($e.accepted -eq 1){
                $key=('{0}:{1}:{2}' -f $e.epoch,$e.owner,$e.sequence)
                Require (-not $acceptedKeys.ContainsKey($key)) 'duplicate accepted command sequence'
                $acceptedKeys[$key]=$true
                $u=Unit $e.before $e.unit
                Require ($u.Count -eq 1 -and $u[0].owner -eq $e.owner -and $u[0].kind -in @(0,1) -and $u[0].hp -gt 0) 'accepted command unit is absent, foreign, dead or immobile'
            }
            foreach($snapshot in @($e.before,$e.after)){
                if($maps.host.ContainsKey([long]$snapshot.revision)){Require ((Semantic $snapshot) -ceq (Semantic $maps.host[[long]$snapshot.revision])) 'command snapshot contradicts published host state'}
            }
            $commands+=$e
        }
        foreach($m in [regex]::Matches($text,'PROOF_STOP_FIXTURE([^\r\n]*)')){
            Require ($role -eq 'rendered') 'fixture phase outside rendered client'
            Require ($fixtures.Count -lt 8) 'fixture event cap'
            $e=Json $m.Groups[1].Value.Trim()
            Require ($e.phase -cin @('moving','stopped','resumed') -and (Integer $e.owner) -and $e.owner -eq 1 -and (Integer $e.unit 1) -and (Integer $e.epoch) -and $e.epoch -eq 0 -and (Integer $e.revision) -and (Integer $e.sequence 1) -and (Number $e.elapsed) -and (Integer $e.nativeStopCount) -and $e.nativeStopCount -eq $(if($e.phase -ceq 'moving'){0}else{$ExpectedNativeStopDelta})) 'invalid fixture phase'
            foreach($k in @('x','y','z','destinationX','destinationY','destinationZ')){Require (Number $e.$k) 'invalid fixture position'}
            $fixtures+=$e
        }
    }
    Require ($fixtures.Count -eq 3 -and ($fixtures.phase -join ',') -ceq 'moving,stopped,resumed') 'missing/reordered fixture phases'
    $moving=$fixtures[0];$stopped=$fixtures[1];$resumed=$fixtures[2]
    Require ($moving.unit -eq $stopped.unit -and $moving.unit -eq $resumed.unit -and $moving.sequence -eq 5 -and $stopped.sequence -eq 6 -and $resumed.sequence -eq 7 -and $moving.revision -lt $stopped.revision -and $stopped.revision -lt $resumed.revision -and $moving.elapsed -lt $stopped.elapsed -and $stopped.elapsed -lt $resumed.elapsed) 'phase identity/ordering mismatch'
    $events=@()
    foreach($pair in @(@(5,'Move'),@(6,'Stop'),@(7,'Move'))){
        $match=@($commands|Where-Object {$_.owner -eq 1 -and $_.unit -eq $moving.unit -and $_.sequence -eq $pair[0] -and $_.type -ceq $pair[1] -and $_.accepted -eq 1})
        Require ($match.Count -eq 1) 'missing/duplicate accepted phase command'
        $u=Unit $match[0].before $moving.unit;Require ($u.Count -eq 1 -and $u[0].owner -eq 1 -and $u[0].kind -in @(0,1) -and $u[0].hp -gt 0) 'command unit is not live owned mobile'
        $events+=$match[0]
    }
    $common=@($maps.host.Keys|Where-Object {$maps.rendered.ContainsKey($_) -and $maps.observer.ContainsKey($_)}|Sort-Object)
    foreach($rev in $common){Require ((Semantic $maps.host[$rev]) -ceq (Semantic $maps.rendered[$rev]) -and (Semantic $maps.host[$rev]) -ceq (Semantic $maps.observer[$rev])) 'common snapshot semantic mismatch'}
    function Rendered-Phase($Phase){
        Require ($maps.rendered.ContainsKey([long]$Phase.revision)) 'phase revision absent from actual receiving client'
        $u=Unit $maps.rendered[[long]$Phase.revision] $Phase.unit;Require ($u.Count -eq 1 -and $u[0].owner -eq 1 -and $u[0].kind -in @(0,1) -and $u[0].hp -gt 0 -and (Distance $u[0] $Phase) -le 0.0001) 'phase position disagrees with actual client snapshot'
        return $u[0]
    }
    $movingUnit=Rendered-Phase $moving;$stopUnit=Rendered-Phase $stopped;$resumeUnit=Rendered-Phase $resumed
    Require ((Distance (Destination $moving) (Destination $events[0])) -le 2) 'phase destination disagrees with actual Move command'
    Require ((Distance (Destination $stopped) (Destination $moving)) -le 2) 'stopped phase names a different interrupted destination'
    $destination=[pscustomobject]@{x=$events[0].destinationX;y=$events[0].destinationY;z=$events[0].destinationZ}
    Require ($moving.revision -ge $events[0].afterRevision -and $moving.revision -lt $events[1].afterRevision -and (Distance (Unit $events[0].before $moving.unit)[0] $movingUnit) -gt 50 -and (Distance $movingUnit $destination) -gt 100) 'no interrupted movement before destination'
    Require ($stopped.revision -ge $events[1].afterRevision -and $stopped.revision -lt $events[2].afterRevision -and (Distance $stopUnit (Unit $events[1].after $moving.unit)[0]) -le 0.0001) 'stopped phase not after exact Stop'
    $movingCommon=@($common|Where-Object {$_ -ge $events[0].afterRevision -and $_ -lt $events[1].afterRevision -and (Distance (Unit $maps.host[$_] $moving.unit)[0] (Unit $events[0].before $moving.unit)[0]) -gt 50})
    Require ($movingCommon.Count -gt 0) 'no common replicated movement before Stop'
    $stationary=@($common|Where-Object {$_ -ge $events[1].afterRevision -and $_ -lt $events[2].afterRevision})
    Require ($stationary.Count -ge 3) 'fewer than three common stopped snapshots'
    $first=$maps.host[$stationary[0]];$last=$maps.host[$stationary[-1]]
    Require (($last.simulatedSeconds-$first.simulatedSeconds) -ge (1.0-0.000001)) 'stationary common window shorter than one simulated second'
    $anchor=(Unit $events[1].after $moving.unit)[0]
    foreach($rev in $stationary){
        $u=Unit $maps.host[$rev] $moving.unit;Require ($u.Count -eq 1 -and $u[0].owner -eq 1 -and $u[0].kind -eq $anchor.kind -and $u[0].hp -gt 0 -and $u[0].cargo -eq $anchor.cargo -and (Distance $u[0] $anchor) -le 0.0001) 'stopped unit moved, died or changed cargo'
    }
    Require ((Distance (Destination $resumed) (Destination $events[2])) -le 2) 'resume phase destination disagrees with actual command'
    $resumeDestination=[pscustomobject]@{x=$events[2].destinationX;y=$events[2].destinationY;z=$events[2].destinationZ}
    Require ((Distance $resumeUnit $resumeDestination) -lt (Distance $anchor $resumeDestination)) 'resume movement does not approach actual destination'
    Require ($resumed.revision -ge $events[2].afterRevision -and (Distance $resumeUnit $stopUnit) -gt 50) 'no real movement after resume'
    $resumeCommon=@($common|Where-Object {$_ -ge $events[2].afterRevision -and (Unit $maps.host[$_] $moving.unit).Count -eq 1 -and (Unit $maps.host[$_] $moving.unit)[0].owner -eq 1 -and (Unit $maps.host[$_] $moving.unit)[0].kind -eq $anchor.kind -and (Unit $maps.host[$_] $moving.unit)[0].hp -gt 0 -and (Distance (Unit $maps.host[$_] $moving.unit)[0] $anchor) -gt 50})
    Require ($resumeCommon.Count -gt 0) 'no common replicated movement after resume'
    return [ordered]@{passed=$true;expectedNativeStopDelta=$ExpectedNativeStopDelta;nativeStopCounts=@($fixtures.nativeStopCount);owner=1;unit=$moving.unit;epoch=0;sequences=@(5,6,7);roles=@('host','rendered','observer');rowsByRole=$counts;commonRevisionCount=$common.Count;movingRevision=$moving.revision;stoppedRevision=$stopped.revision;resumedRevision=$resumed.revision;stationaryCommonRevisions=$stationary;stationarySimulationSeconds=($last.simulatedSeconds-$first.simulatedSeconds);physicalInputVerified=$false;internetTest=$false;performanceTest=$false}
}
