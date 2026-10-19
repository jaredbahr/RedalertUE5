# Pure, fail-closed policy for a future three-process, opt-in AreaGuard run.
# It reads logs only. A passing result requires a human AreaGuard receipt, real
# host Advance transitions, and the same published guard states on both clients.
function Get-AreaGuardObservation {
    param(
        [Parameter(Mandatory)][hashtable]$Logs,
        [Parameter(Mandatory)][string]$RunId,
        [Parameter(Mandatory)][string]$SourceTree,
        [switch]$RequireRuntimeMarkers
    )
    function Require($Condition, [string]$Reason) { if (-not $Condition) { throw "AreaGuard observation: $Reason" } }
    function Int($Value, [long]$Min = 0) {
        return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and
            $Value -isnot [array] -and [double]::IsFinite([double]$Value) -and
            [double]$Value -ge $Min -and [double]$Value -le 2147483647 -and
            [double]$Value -eq [math]::Truncate([double]$Value)
    }
    function Num($Value) {
        return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and
            $Value -isnot [array] -and [double]::IsFinite([double]$Value)
    }
    function JsonRow([string]$Text) {
        Require ($Text.Length -le 65536) 'oversized row'
        $stack = [Collections.Generic.List[object]]::new()
        foreach ($match in [regex]::Matches($Text, '"(?:\\.|[^"\\])*"\s*:|[{}\[\]]')) {
            $token = $match.Value
            if ($token -eq '{') { $stack.Add(@{}) }
            elseif ($token -eq '[') { $stack.Add($null) }
            elseif ($token -eq '}' -or $token -eq ']') {
                Require ($stack.Count -gt 0) 'unbalanced JSON'
                $stack.RemoveAt($stack.Count - 1)
            }
            else {
                Require ($stack.Count -gt 0 -and $null -ne $stack[$stack.Count - 1]) 'key outside object'
                $key = ($token -replace '\s*:$', '' | ConvertFrom-Json)
                Require (-not $stack[$stack.Count - 1].ContainsKey($key)) 'duplicate JSON key'
                $stack[$stack.Count - 1][$key] = $true
            }
        }
        Require ($stack.Count -eq 0) 'unbalanced JSON'
        $result = $Text | ConvertFrom-Json -AsHashtable -Depth 16 -NoEnumerate -ErrorAction Stop
        Require ($result -is [System.Collections.IDictionary]) 'object row required'
        return $result
    }
    function Point($Value) {
        Require ($Value -is [System.Collections.IDictionary] -and $Value.Count -eq 3) 'point shape'
        foreach ($axis in @('x', 'y', 'z')) { Require ((Num $Value[$axis]) -and [math]::Abs([double]$Value[$axis]) -le 10000) "point $axis" }
    }
    function Dist($A, $B) {
        $square = 0.0
        foreach ($axis in @('x', 'y', 'z')) { $square += [math]::Pow([double]$A[$axis] - [double]$B[$axis], 2) }
        return [math]::Sqrt($square)
    }
    function Unit($State, [int]$Id) { return ,@($State.units | Where-Object { $_.id -eq $Id }) }
    function Team($State, [int]$Owner) { return ,@($State.players | Where-Object { $_.owner -eq $Owner }) }
    function State($Value) {
        Require ($Value -is [System.Collections.IDictionary] -and $Value.Count -eq 9) 'state shape'
        Require ((Int $Value.epoch 1) -and (Int $Value.revision) -and $Value.digest -cmatch '^[0-9a-f]{32}$' -and
            $Value.skirmish -is [bool] -and $Value.skirmish -and (Int $Value.phase) -and
            (Int $Value.winner -1) -and $Value.players -is [array] -and
            $Value.units -is [array] -and $Value.resources -is [array] -and
            $Value.units.Count -le 128 -and $Value.resources.Count -le 128) 'state envelope'
        $owners = @{}
        foreach ($player in $Value.players) {
            Require ($player -is [System.Collections.IDictionary] -and $player.Count -eq 4 -and
                (Int $player.owner 1) -and (Int $player.team) -and (Int $player.credits) -and
                (Int $player.gathered) -and -not $owners.ContainsKey([string]$player.owner)) 'player shape or duplicate'
            $owners[[string]$player.owner] = $true
        }
        $ids = @{}
        foreach ($unit in $Value.units) {
            Require ($unit -is [System.Collections.IDictionary] -and $unit.Count -eq 13) 'unit shape'
            Require ((Int $unit.id 1) -and (Int $unit.owner 1) -and (Int $unit.kind) -and
                (Num $unit.hp) -and $unit.hp -ge 0 -and (Int $unit.cargo) -and
                $unit.rally -is [bool] -and $unit.guard -is [bool] -and
                (Int $unit.target -1) -and $unit.returning -is [bool] -and
                -not $ids.ContainsKey([string]$unit.id)) 'unit fields or duplicate'
            Point $unit.position; Point $unit.direction; Point $unit.rallyPoint; Point $unit.post
            $ids[[string]$unit.id] = $true
        }
        $resources = @{}
        foreach ($resource in $Value.resources) {
            Require ($resource -is [System.Collections.IDictionary] -and $resource.Count -eq 4 -and
                (Int $resource.id 1) -and (Int $resource.owner 1) -and (Int $resource.remaining) -and
                -not $resources.ContainsKey([string]$resource.id)) 'resource shape or duplicate'
            Point $resource.position
            $resources[[string]$resource.id] = $true
        }
    }
    function GuardProjection($Value) {
        $units = @($Value.units | Sort-Object id | ForEach-Object {
            [ordered]@{id=$_.id;owner=$_.owner;kind=$_.kind;hp=$_.hp;position=$_.position;
                direction=$_.direction;cargo=$_.cargo;rally=$_.rally;rallyPoint=$_.rallyPoint;
                guard=$_.guard;post=$_.post;target=$_.target;returning=$_.returning}
        })
        $players = @($Value.players | Sort-Object owner | ForEach-Object {
            [ordered]@{owner=$_.owner;team=$_.team;credits=$_.credits;gathered=$_.gathered}
        })
        $resources = @($Value.resources | Sort-Object id | ForEach-Object {
            [ordered]@{id=$_.id;owner=$_.owner;position=$_.position;remaining=$_.remaining}
        })
        return (ConvertTo-Json -InputObject ([ordered]@{phase=$Value.phase;winner=$Value.winner;
            players=$players;units=$units;resources=$resources}) -Depth 8 -Compress)
    }
    Require ($RunId -cmatch '^[0-9a-f]{32}$' -and $SourceTree -cmatch '^[0-9a-f]{40}$') 'run/source argument'
    $snapshots = @{}; $events = @{}
    foreach ($role in @('host', 'rendered', 'observer')) {
        Require ($Logs.ContainsKey($role) -and $Logs[$role] -is [string] -and $Logs[$role].Length -le 32MB) "missing/oversized $role log"
        Require ($Logs[$role] -notmatch 'PROOF_AREA_GUARD_OBSERVATION_ERROR') "$role runtime error"
        Require ($Logs[$role] -notmatch 'PROOF_AREA_GUARD_FIXTURE_ERROR|Fatal error:|Assertion failed:|Unhandled Exception:') "$role fixture/runtime error"
        $viewer = @{host=0;rendered=1;observer=2}[$role]
        $rows = @([regex]::Matches($Logs[$role], 'PROOF_AREA_GUARD_OBSERVATION (\{[^\r\n]*\})') | ForEach-Object { JsonRow $_.Groups[1].Value })
        Require ($rows.Count -eq [regex]::Matches($Logs[$role], 'PROOF_AREA_GUARD_OBSERVATION ').Count) "$role malformed/truncated row"
        Require ($rows.Count -gt 0 -and $rows.Count -le 8192) "$role row count"
        $events[$role] = $rows; $snapshots[$role] = @{}
        $priorRevision = -1
        for ($index = 0; $index -lt $rows.Count; $index++) {
            $row = $rows[$index]
            Require ((Int $row.v 1) -and $row.v -eq 1 -and (Int $row.row 1) -and $row.row -eq $index + 1 -and
                $row.run -ceq $RunId -and $row.source -ceq $SourceTree -and
                $row.role -ceq $(if ($role -eq 'host') { 'host' } else { 'client' }) -and
                (Int $row.viewer) -and $row.viewer -eq $viewer) "$role row envelope"
            Require ($row.event -in @('snapshot', 'command', 'advance')) "$role event"
            Require ($row.Count -eq @{snapshot=8;command=18;advance=11}[$row.event]) "$role event shape"
            if ($role -ne 'host') { Require ($row.event -ceq 'snapshot') "$role unexpected event" }
            if ($row.event -eq 'snapshot') {
                State $row.state
                $key = "$($row.state.epoch):$($row.state.revision)"
                Require (-not $snapshots[$role].ContainsKey($key) -and $row.state.revision -gt $priorRevision) "$role duplicate/regressed snapshot"
                $snapshots[$role][$key] = $row.state
                $priorRevision = $row.state.revision
                $pattern = 'PROOF_SNAPSHOT role=' + $(if ($role -eq 'host') { 'host' } else { 'client' }) +
                    ' owner=' + $viewer + ' epoch=' + $row.state.epoch + ' revision=' + $row.state.revision +
                    ' digest=' + $row.state.digest + '(?![0-9a-f])'
                Require ([regex]::IsMatch($Logs[$role], $pattern)) "$role snapshot lacks ordinary digest log"
            }
            elseif ($row.event -eq 'command') {
                Require ($role -eq 'host' -and (Int $row.owner 1) -and (Int $row.type) -and
                    (Int $row.sequence 1) -and (Int $row.payloadEpoch) -and (Int $row.unit -1) -and
                    (Int $row.target -1) -and $row.accepted -is [bool] -and $row.reason -is [string]) 'command envelope'
                Point $row.destination; State $row.before; State $row.after
                Require ($row.before.epoch -eq $row.after.epoch) 'command epoch changed'
                if (-not $row.accepted) {
                    Require ($row.before.revision -eq $row.after.revision -and $row.before.digest -ceq $row.after.digest) 'rejected command changed state'
                }
            }
            elseif ($row.event -eq 'advance') {
                Require ($role -eq 'host' -and (Num $row.simulatedSeconds) -and $row.simulatedSeconds -ge 0 -and
                    (Num $row.deltaSeconds) -and [math]::Abs($row.deltaSeconds-0.1) -le 0.00000001) 'host clock/step'
                State $row.before; State $row.after
                Require ($row.before.epoch -eq $row.after.epoch -and $row.after.revision -gt $row.before.revision) 'advance chronology'
            }
        }
    }
    $common = @($snapshots.host.Keys | Where-Object {
        $snapshots.rendered.ContainsKey($_) -and $snapshots.observer.ContainsKey($_)
    } | Sort-Object { [int]($_ -split ':')[-1] })
    Require ($common.Count -ge 3) 'no three-role common checkpoints'
    foreach ($key in $common) {
        $hostState = $snapshots.host[$key]
        $projection = GuardProjection $hostState
        foreach ($role in @('rendered', 'observer')) {
            Require ($snapshots[$role][$key].digest -ceq $hostState.digest -and
                (GuardProjection $snapshots[$role][$key]) -ceq $projection) "different common state $key $role"
        }
    }
    $guardCommands = @($events.host | Where-Object { $_.event -eq 'command' -and $_.type -eq 10 -and $_.owner -eq 1 -and $_.accepted })
    Require ($guardCommands.Count -eq 1) 'expected one accepted owner-one AreaGuard command'
    $command = $guardCommands[0]
    $beforeUnit = Unit $command.before $command.unit; $afterUnit = Unit $command.after $command.unit
    Require ($beforeUnit.Count -eq 1 -and $afterUnit.Count -eq 1 -and $beforeUnit[0].owner -eq 1 -and
        $beforeUnit[0].kind -eq 0 -and $beforeUnit[0].hp -gt 0 -and
        $command.reason -ceq 'accepted' -and $command.payloadEpoch -eq $command.after.epoch -and $command.target -eq -1 -and
        (Dist $command.destination @{x=0;y=0;z=0}) -eq 0 -and
        $command.after.revision -gt $command.before.revision -and
        $afterUnit[0].guard -and -not $afterUnit[0].returning -and $afterUnit[0].target -eq -1 -and
        (Dist $afterUnit[0].post $beforeUnit[0].position) -eq 0 -and
        (Dist $afterUnit[0].position $beforeUnit[0].position) -eq 0) 'AreaGuard admission/post'
    $post = $afterUnit[0].post; $unitId = $command.unit
    $advances = @($events.host | Where-Object { $_.event -eq 'advance' -and $_.after.revision -gt $command.after.revision })
    Require ($advances.Count -ge 3) 'missing host advances'
    $acquired = $false; $chased = $false; $returned = $false; $atPost = $false
    $damaged = $false; $escaped = $false; $maxOutbound = 0.0
    $lastClock = -1.0; $returnRevision = -1
    foreach ($advance in $advances) {
        Require ($advance.simulatedSeconds -gt $lastClock) 'host clock regression'
        $lastClock = [double]$advance.simulatedSeconds
        $from = Unit $advance.before $unitId; $to = Unit $advance.after $unitId
        if ($from.Count -ne 1 -or $to.Count -ne 1 -or -not $to[0].guard) { continue }
        Require ((Dist $to[0].post $post) -eq 0 -and (Dist $to[0].position $post) -le 300.0) 'guard post/leash'
        $maxOutbound = [math]::Max($maxOutbound, (Dist $to[0].position $post))
        Require ((Dist $to[0].position $from[0].position) -le 150.0*$advance.deltaSeconds+0.000001) 'extra movement budget'
        if ($from[0].returning -and (Dist $from[0].position $post) -gt 0) {
            Require ($to[0].returning -and $to[0].target -eq -1 -and
                (Dist $to[0].position $post) -le (Dist $from[0].position $post)) 'return interrupted before post'
        }
        if ($to[0].target -gt 0) {
            Require (-not $to[0].returning) 'returning unit has target'
            $target = Unit $advance.after $to[0].target
            $owner = Team $advance.after $to[0].owner
            $foe = if ($target.Count -eq 1) { Team $advance.after $target[0].owner } else { @() }
            Require ($target.Count -eq 1 -and $target[0].hp -gt 0 -and $owner.Count -eq 1 -and
                $foe.Count -eq 1 -and $owner[0].team -ne $foe[0].team -and
                (Dist $target[0].position $post) -le 600.0) 'acquired target outside scan/team'
            $acquired = $true
            if ((Dist $to[0].position $post) -gt 20 -and
                (Dist $to[0].position $post) -gt (Dist $from[0].position $post)) { $chased = $true }
            $priorTarget = Unit $advance.before $to[0].target
            if ($priorTarget.Count -eq 1 -and $target[0].hp -lt $priorTarget[0].hp -and
                (Dist $to[0].position $target[0].position) -le 300.0) { $damaged = $true }
        }
        if ($to[0].returning) {
            Require ($to[0].target -eq -1) 'return target not cleared'
            if ($from[0].target -gt 0) {
                $lostTarget = Unit $advance.after $from[0].target
                Require ($lostTarget.Count -eq 1 -and $lostTarget[0].hp -gt 0 -and
                    (Dist $lostTarget[0].position $post) -gt 600.0) 'return was not caused by observed live target escape'
                $escaped = $true
            }
            if ($returnRevision -lt 0) { $returnRevision = $advance.after.revision }
            if ((Dist $to[0].position $post) -lt (Dist $from[0].position $post)) { $returned = $true }
            if ((Dist $to[0].position $post) -eq 0 -and $advance.after.revision -gt $returnRevision) { $atPost = $true }
        }
    }
    Require ($acquired -and $chased -and $damaged -and $escaped -and $returned -and $atPost -and
        $maxOutbound -ge 299.999) 'missing bounded chase/damage/escape/return/home host progression'
    $commonAcquire = $false; $commonChase = $false; $commonReturn = $false; $commonHome = $false; $commonLeash = $false
    foreach ($key in $common) {
        $state = $snapshots.host[$key]; if ($state.revision -lt $command.after.revision) { continue }
        $match = Unit $state $unitId; if ($match.Count -ne 1) { continue }
        $unit = $match[0]
        if ($unit.guard -and $unit.target -gt 0) { $commonAcquire = $true }
        if ($unit.guard -and $unit.target -gt 0 -and (Dist $unit.position $post) -gt 20) { $commonChase = $true }
        if ($unit.guard -and (Dist $unit.position $post) -ge 299.999) { $commonLeash = $true }
        if ($unit.guard -and $unit.returning -and $unit.target -eq -1 -and (Dist $unit.position $post) -gt 0) { $commonReturn = $true }
        if ($unit.guard -and $unit.returning -and (Dist $unit.position $post) -eq 0) { $commonHome = $true }
    }
    Require ($commonAcquire -and $commonChase -and $commonLeash -and $commonReturn -and $commonHome) 'missing common acquisition/chase/leash/return/home'
    if ($RequireRuntimeMarkers) {
        Require ([regex]::Matches($Logs.rendered,('PROOF_AREA_GUARD_FIXTURE_SEND owner=1 epoch='+$command.after.epoch+
            ' sequence='+$command.sequence+' type=10 unit='+$unitId+' target=-1\b')).Count -eq 1) 'missing/duplicate public RPC send'
        Require ([regex]::Matches($Logs.rendered,('PROOF_RESULT owner=1 sequence='+$command.sequence+' accepted=1 reason=accepted')).Count -eq 1) 'missing/duplicate accepted client receipt'
        Require ([regex]::Matches($Logs.host,('PROOF_AREA_GUARD_FIXTURE_RETREAT target=\d+ guard='+$unitId+'\b')).Count -eq 1) 'missing/duplicate host target retreat'
        foreach ($role in @('host','rendered','observer')) {
            Require ([regex]::Matches($Logs[$role],('PROOF_AREA_GUARD_FIXTURE_READY role='+$role+' status=pass\b')).Count -eq 1) "$role ready marker"
            Require ([regex]::Matches($Logs[$role],('PROOF_AREA_GUARD_FIXTURE_COMPLETE role='+$role+' status=pass\b')).Count -eq 1) "$role complete marker"
        }
    }
    return [ordered]@{passed=$true;epoch=$command.after.epoch;unit=$unitId;post=$post;
        commandSequence=$command.sequence;commonRevisions=$common;maxOutbound=$maxOutbound;escaped=$escaped;
        targetDamaged=$damaged;lastSimulatedSeconds=$lastClock;
        proofScope='synthetic public-command and three-role replicated AreaGuard state; physical G input and performance unverified'}
}
