# Pure companion policy. The caller must first run Get-RefineryMinerEvidence.
# Published snapshots establish source/authority agreement; only immediate host
# Advance observations establish deposit location. No process or filesystem I/O.
function Get-ReturnCargoEvidence {
    param([Parameter(Mandatory)][string]$Log,
        [Parameter(Mandatory)][hashtable]$ValidatedStateContext,
        [Parameter(Mandatory)]$RefineryEvidence)
    function Require($Condition,[string]$Reason) { if (-not $Condition) { throw "ReturnCargo evidence: $Reason" } }
    function Integer($Value,[long]$Minimum=0,[long]$Maximum=2147483647) {
        return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and $Value -isnot [array] -and
            [double]$Value -ge $Minimum -and [double]$Value -le $Maximum -and [double]$Value -eq [math]::Truncate([double]$Value)
    }
    function Number($Value) { return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and $Value -isnot [array] -and [double]::IsFinite([double]$Value) }
    function Row([string]$Text) {
        Require ($Text.Length -le 8192) 'oversized event'
        # Tokenize complete strings, including values, so braces inside a string
        # cannot change nesting. Reject duplicate/case-colliding decoded keys
        # before ConvertFrom-Json can overwrite them.
        $stack=[Collections.Generic.List[object]]::new()
        foreach ($m in [regex]::Matches($Text,'"(?:\\.|[^"\\])*"\s*:|"(?:\\.|[^"\\])*"|[{}\[\]]')) {
            $t=$m.Value
            if ($t -eq '{') { $stack.Add(@{}) }
            elseif ($t -eq '[') { $stack.Add($null) }
            elseif ($t -in @('}',']')) { Require ($stack.Count -gt 0) 'unbalanced JSON'; $stack.RemoveAt($stack.Count-1) }
            elseif ($t -match ':$') {
                Require ($stack.Count -gt 0 -and $null -ne $stack[$stack.Count-1]) 'key outside object'
                $k=($t -replace '\s*:$','' | ConvertFrom-Json)
                Require (-not $stack[$stack.Count-1].ContainsKey($k)) 'duplicate key'
                $stack[$stack.Count-1][$k]=$true
            }
        }
        Require ($stack.Count -eq 0) 'unbalanced JSON'
        try { $value=$Text | ConvertFrom-Json -Depth 8 -NoEnumerate -ErrorAction Stop }
        catch { throw 'ReturnCargo evidence: malformed JSON' }
        Require ($null -ne $value -and $value -isnot [array] -and $value -isnot [string] -and $value -isnot [ValueType]) 'object required'
        return $value
    }
    function Schema($Object,[string[]]$Fields) {
        $actual=@($Object.PSObject.Properties.Name)
        Require ($actual.Count -eq $Fields.Count) 'event schema field count'
        foreach ($field in $Fields) { Require ($actual -ccontains $field) "missing event field $field" }
    }
    function Units($State,$Id) { return @($State.units | Where-Object id -eq $Id) }
    function Squared($A,$B,[string]$PrefixA='',[string]$PrefixB='') {
        $distance=0.0
        foreach ($axis in @('x','y','z')) {
            $left=if($PrefixA){$PrefixA+$axis.ToUpperInvariant()}else{$axis}
            $right=if($PrefixB){$PrefixB+$axis.ToUpperInvariant()}else{$axis}
            $distance+=[math]::Pow(([double]$A.$left-[double]$B.$right),2)
        }
        return $distance
    }
    function Coordinates($Object,[string[]]$Fields) {
        foreach ($field in $Fields) { Require ((Number $Object.$field) -and [math]::Abs([double]$Object.$field) -le 10000) "invalid coordinate $field" }
    }
    function Accounted($State) {
        $sum=[long]0
        foreach($r in $State.resources){$sum+=$r.remaining}; foreach($u in $State.units){$sum+=$u.cargo}; foreach($p in $State.players){$sum+=$p.gathered}
        return $sum
    }
    Require ($Log.Length -le 24MB -and $Log -notmatch 'PROOF_RETURN_CARGO_ERROR|status=fail') 'runtime failure or log cap'
    Require ($RefineryEvidence.passed -is [bool] -and $RefineryEvidence.passed -and $ValidatedStateContext.maps -is [hashtable] -and
        $ValidatedStateContext.maps.ContainsKey('rendered') -and $ValidatedStateContext.maps.ContainsKey('host')) 'missing validated refinery context'
    $rows=@{PHASE=@();COMMAND=@();TRAVEL=@();DEPOSIT=@();RESUME=@()}
    foreach($m in [regex]::Matches($Log,'PROOF_RETURN_CARGO_([A-Z_]+)([^\r\n]*)')) {
        $kind=$m.Groups[1].Value
        if($kind -ceq 'CAPTURE'){continue}
        Require ($rows.ContainsKey($kind)) 'unknown event kind'
        $rows[$kind]+=,(Row $m.Groups[2].Value)
        Require ($rows[$kind].Count -le 512) 'event count cap'
    }
    Require ($rows.COMMAND.Count+$rows.TRAVEL.Count+$rows.DEPOSIT.Count+$rows.RESUME.Count -le 512) 'combined authoritative row cap'
    $names=@('selected','gather-a','single-return','single-accepted','single-deposited','single-resumed',
        'loaded-only-queued','loaded-only-accepted','loaded-only-deposited','loaded-only-resumed','hold-a','held-a',
        'gather-b','group-queued','group-accepted','group-deposited','group-resumed','restore-a','complete')
    $phases=$rows.PHASE; Require ($phases.Count -eq $names.Count) 'exact phase count'
    $anchor=$phases[0]; $byPhase=@{}; $previousTime=-1.0; $previousRevision=-1
    $phaseFields=@('phase','owner','epoch','revision','a','b','refinery','resource','beforeSequence','nextBefore','nextNow','lastSequence','queue','count','cargoA','cargoB','credits','gathered','elapsed','physicalInputVerified','nativeCallbackInvoked','sharedHandlers','networkTransportVerified')
    for($i=0;$i -lt $names.Count;$i++) {
        $p=$phases[$i]; Schema $p $phaseFields
        Require ($p.phase -ceq $names[$i]) 'phase order'; $byPhase[$p.phase]=$p
        foreach($n in @($phaseFields | Where-Object {$_ -notin @('phase','elapsed')})) { Require (Integer $p.$n) "invalid phase integer $n" }
        foreach($n in @('owner','epoch','a','b','refinery','resource')) { Require ($p.$n -eq $anchor.$n) 'phase identity drift' }
        Require ($p.owner -eq 1 -and $p.epoch -gt 0 -and $p.a -gt 0 -and $p.b -gt $p.a -and $p.refinery -gt 0 -and $p.resource -gt 0) 'phase identity'
        Require ((Number $p.elapsed) -and $p.elapsed -ge 0 -and $p.elapsed -ge $previousTime -and $p.elapsed -le 220 -and $p.revision -ge $previousRevision) 'phase time/revision'
        Require ($p.physicalInputVerified -eq 0 -and $p.nativeCallbackInvoked -eq 0 -and $p.sharedHandlers -eq 1 -and $p.networkTransportVerified -eq 0) 'shared-handler input scope'
        Require ($p.nextBefore -gt 0 -and $p.beforeSequence -lt $p.nextBefore -and $p.nextNow -ge $p.nextBefore -and $p.lastSequence -lt $p.nextNow -and $p.queue -le 2 -and $p.count -le 12) 'phase sequence/selection bounds'
        $key="$($p.epoch):$($p.revision)"
        Require ($ValidatedStateContext.common -contains $key -and $ValidatedStateContext.maps.host.ContainsKey($key) -and $ValidatedStateContext.maps.rendered.ContainsKey($key)) 'phase lacks common authority/source state'
        $s=$ValidatedStateContext.maps.host[$key]
        Require ($s.phase -eq 1 -and $s.winner -eq -1) 'fixture phase outside playing state'
        foreach($id in @($p.a,$p.b,$p.refinery)) {
            $u=@(Units $s $id); $kind=if($id -eq $p.refinery){3}else{1}
            Require ($u.Count -eq 1 -and $u[0].owner -eq 1 -and $u[0].kind -eq $kind -and $u[0].hp -gt 0) 'phase unit identity'
        }
        $a=@(Units $s $p.a)[0]; $b=@(Units $s $p.b)[0]; $player=@($s.players | Where-Object id -eq 1)
        Require ($player.Count -eq 1 -and $p.cargoA -eq $a.cargo -and $p.cargoB -eq $b.cargo -and $p.credits -eq $player[0].credits -and $p.gathered -eq $player[0].gathered) 'phase disagrees with validated state'
        Require (@($s.resources | Where-Object id -eq $p.resource).Count -eq 1) 'phase resource missing'
        $previousTime=[double]$p.elapsed; $previousRevision=[long]$p.revision
    }
    $gatherA=$byPhase['gather-a']
    Require ($gatherA.beforeSequence -eq $anchor.beforeSequence -and $gatherA.nextBefore -eq $anchor.nextBefore -and $gatherA.revision -eq $anchor.revision) 'initial selection/Gather source batch'
    foreach($batch in @(
        @{issue='single-return';observed=@('single-deposited','single-resumed')},
        @{issue='loaded-only-queued';observed=@('loaded-only-deposited','loaded-only-resumed')},
        @{issue='group-queued';observed=@('group-deposited','group-resumed')},
        @{issue='restore-a';observed=@('complete')})) {
        foreach($name in $batch.observed) {
            Require ($byPhase[$name].beforeSequence -eq $byPhase[$batch.issue].beforeSequence -and $byPhase[$name].nextBefore -eq $byPhase[$batch.issue].nextBefore) 'observation command batch drift'
        }
    }
    $start=$ValidatedStateContext.maps.host["$($anchor.epoch):$($anchor.revision)"]
    $conserved=Accounted $start
    Require ($anchor.cargoA -eq 0 -and $anchor.cargoB -eq 0 -and $anchor.count -eq 1 -and $anchor.queue -eq 0 -and $anchor.nextNow -eq $anchor.nextBefore -and $anchor.lastSequence -eq $anchor.beforeSequence) 'unloaded selected start'
    Require (@($start.units | Where-Object {$_.owner -eq 1 -and $_.kind -eq 1 -and $_.hp -gt 0}).Count -eq 2) 'exact existing miner population'
    $grants=@($RefineryEvidence.grants | Where-Object {$_.owner -eq 1 -and $_.epoch -eq $anchor.epoch})
    Require ($grants.Count -eq 1 -and $grants[0].miner -eq $anchor.b -and $grants[0].refinery -eq $anchor.refinery -and $grants[0].resource -eq $anchor.resource) 'real refinery-granted miner provenance'
    $grantRows=@([regex]::Matches($Log,'PROOF_REFINERY_EVENT([^\r\n]*)') | ForEach-Object { Row $_.Groups[1].Value } | Where-Object {$_.event -eq 'grant' -and $_.owner -eq 1 -and $_.epoch -eq $anchor.epoch})
    Require ($grantRows.Count -eq 1 -and $anchor.a -in $grantRows[0].beforeIds -and $anchor.b -notin $grantRows[0].beforeIds) 'starter/granted provenance'
    # Exact fixture miner command order. Global command gaps are permitted only
    # for ordinary non-miner defense/production, never an extra miner command.
    $spec=@(
        @{phase='gather-a';unit=$anchor.a;type=5;target=$anchor.resource;offset=0},
        @{phase='single-return';unit=$anchor.a;type=9;target=$anchor.refinery;offset=0},
        @{phase='loaded-only-queued';unit=$anchor.a;type=9;target=$anchor.refinery;offset=0},
        @{phase='hold-a';unit=$anchor.a;type=6;target=-1;offset=0},
        @{phase='gather-b';unit=$anchor.b;type=5;target=$anchor.resource;offset=0},
        @{phase='group-queued';unit=$anchor.a;type=9;target=$anchor.refinery;offset=0},
        @{phase='group-queued';unit=$anchor.b;type=9;target=$anchor.refinery;offset=1},
        @{phase='restore-a';unit=$anchor.a;type=5;target=$anchor.resource;offset=0})
    $typeNames=@('Move','Attack','Produce','BuildRefinery','BuildFactory','Gather','Stop','SetRally','AttackMove','ReturnCargo')
    $commands=$rows.COMMAND; Require ($commands.Count -ge 10 -and $commands.Count -le 256) 'bounded authoritative command log'
    $commandBySequence=@{}; $lastSequence=0; $lastRevision=-1
    $commandFields=@('owner','epoch','sequence','type','unit','target','accepted','revisionBefore','revision','cargoBefore','cargoAfter','creditsBefore','creditsAfter','gatheredBefore','gatheredAfter','xBefore','yBefore','zBefore','x','y','z','resumeResource','conservedBefore','conservedAfter')
    foreach($c in $commands) {
        Schema $c $commandFields
        foreach($n in @('owner','epoch','sequence','type','accepted','revisionBefore','revision','cargoBefore','cargoAfter','creditsBefore','creditsAfter','gatheredBefore','gatheredAfter','conservedBefore','conservedAfter')) { Require (Integer $c.$n) "invalid command integer $n" }
        foreach($n in @('unit','target','resumeResource')) { Require (Integer $c.$n -1) "invalid command identity $n" }
        Coordinates $c @('xBefore','yBefore','zBefore','x','y','z')
        Require ($c.owner -eq 1 -and $c.epoch -eq $anchor.epoch -and $c.sequence -eq $lastSequence+1 -and $c.type -le 9 -and $c.accepted -le 1 -and $c.revisionBefore -ge $lastRevision -and $c.revision -ge $c.revisionBefore -and $c.cargoBefore -le 50 -and $c.cargoAfter -le 50) 'command identity/chronology'
        $requests=@([regex]::Matches($Log,('PROOF_SKIRMISH_REQUEST owner=1 sequence='+$c.sequence+'(?=\s|$)[^\r\n]*')))
        $expected='PROOF_SKIRMISH_REQUEST owner=1 sequence='+$c.sequence+' type='+$typeNames[$c.type]+' unit='+$c.unit+' target='+$c.target
        Require ($requests.Count -eq 1 -and $requests[0].Value -ceq $expected) 'shared Request identity/count'
        $receipts=@([regex]::Matches($Log,('PROOF_RESULT owner=1 sequence='+$c.sequence+'(?=\s|$)[^\r\n]*')))
        Require ($receipts.Count -eq 1 -and $receipts[0].Value -cmatch ('^PROOF_RESULT owner=1 sequence='+$c.sequence+' accepted='+$c.accepted+' reason=\S+$')) 'receipt identity/count'
        if($c.accepted -eq 1){Require ($receipts[0].Value -ceq ('PROOF_RESULT owner=1 sequence='+$c.sequence+' accepted=1 reason=accepted')) 'accepted receipt reason'}
        $commandBySequence[[string]$c.sequence]=$c; $lastSequence=$c.sequence; $lastRevision=$c.revision
    }
    Require ([regex]::Matches($Log,'PROOF_SKIRMISH_REQUEST owner=1(?=\s|$)').Count -eq $commands.Count -and [regex]::Matches($Log,'PROOF_RESULT owner=1(?=\s|$)').Count -eq $commands.Count) 'orphan Request/receipt'
    $minerCommands=@($commands | Where-Object {$_.unit -in @($anchor.a,$anchor.b)})
    Require ($minerCommands.Count -eq $spec.Count -and @($commands | Where-Object type -eq 9).Count -eq 4) 'exact miner/ReturnCargo command count'
    $forced=@(); $saved=@{}
    for($i=0;$i -lt $spec.Count;$i++) {
        $expected=$spec[$i]; $p=$byPhase[$expected.phase]; $c=$minerCommands[$i]
        Require ($c.sequence -eq $p.nextBefore+$expected.offset -and $c.unit -eq $expected.unit -and $c.type -eq $expected.type -and $c.target -eq $expected.target -and $c.accepted -eq 1 -and $c.revisionBefore -ge $p.revision -and $c.revision -gt $c.revisionBefore) 'fixture command identity/acceptance'
        Require ($c.cargoBefore -eq $c.cargoAfter -and $c.creditsBefore -eq $c.creditsAfter -and $c.gatheredBefore -eq $c.gatheredAfter -and $c.conservedBefore -eq $conserved -and $c.conservedAfter -eq $conserved -and $c.xBefore -eq $c.x -and $c.yBefore -eq $c.y -and $c.zBefore -eq $c.z) 'command granted cargo/credit or teleported'
        if($c.type -eq 5){$saved[[string]$c.unit]=$c.target}
        elseif($c.type -eq 6){$saved[[string]$c.unit]=-1}
        Require ($saved.ContainsKey([string]$c.unit) -and $c.resumeResource -eq $saved[[string]$c.unit]) 'saved Gather command provenance'
        if($c.type -eq 9){Require ($c.cargoBefore -eq $(if($c.unit -eq $anchor.a){$p.cargoA}else{$p.cargoB}) -and $c.cargoBefore -gt 0) 'empty miner ReturnCargo'; $forced+=,$c}
    }
    foreach($name in @('gather-a','single-return','gather-b','restore-a')) {
        $p=$byPhase[$name]; Require ($p.count -eq 1 -and $p.queue -eq 0 -and $p.nextNow -eq $p.nextBefore+1 -and $p.lastSequence -eq $p.nextBefore) 'direct handler sequence/count'
    }
    $hold=$byPhase['hold-a']; Require ($hold.count -eq 1 -and $hold.queue -eq 1 -and $hold.nextNow -eq $hold.nextBefore -and $hold.lastSequence -eq $hold.beforeSequence) 'queued Stop handler sequence/count'
    foreach($batch in @(@{issue='single-return';accepted='single-accepted';count=1},@{issue='loaded-only-queued';accepted='loaded-only-accepted';count=1},@{issue='hold-a';accepted='held-a';count=1},@{issue='group-queued';accepted='group-accepted';count=2})) {
        $issue=$byPhase[$batch.issue]; $accepted=$byPhase[$batch.accepted]
        Require ($accepted.count -eq $(if($batch.issue -in @('loaded-only-queued','group-queued')){2}else{1})) 'accepted selection count'
        Require ($accepted.nextBefore -eq $issue.nextBefore -and $accepted.beforeSequence -eq $issue.beforeSequence -and $accepted.queue -eq 0 -and $accepted.nextNow -eq $issue.nextBefore+$batch.count -and $accepted.lastSequence -eq $accepted.nextNow-1 -and $accepted.revision -ge $commandBySequence[[string]($issue.nextBefore+$batch.count-1)].revision) 'accepted batch receipt/snapshot'
    }
    foreach($name in @('loaded-only-queued','group-queued')) {
        $p=$byPhase[$name]; $count=if($name -eq 'group-queued'){2}else{1}
        Require ($p.count -eq 2 -and $p.queue -eq $count -and $p.nextNow -eq $p.nextBefore -and $p.lastSequence -eq $p.beforeSequence -and $p.cargoA -gt 0 -and $(if($count -eq 1){$p.cargoB -eq 0}else{$p.cargoB -gt 0})) 'group eligibility/unstamped queue'
    }
    Require ($byPhase['single-return'].cargoA -gt 0 -and $byPhase['single-return'].cargoB -eq 0 -and $byPhase['hold-a'].cargoA -gt 0 -and $byPhase['held-a'].cargoA -eq $byPhase['hold-a'].cargoA -and $byPhase['gather-b'].cargoB -eq 0) 'single/held/empty miner phases'
    $heldState=$ValidatedStateContext.maps.host["$($anchor.epoch):$($byPhase['held-a'].revision)"]; $heldA=@(Units $heldState $anchor.a)[0]
    $queuedState=$ValidatedStateContext.maps.host["$($anchor.epoch):$($byPhase['group-queued'].revision)"]; $queuedA=@(Units $queuedState $anchor.a)[0]
    Require ($heldA.cargo -eq $queuedA.cargo -and (Squared $heldA $queuedA) -le 1) 'loaded stopped miner changed before group return'
    Require ($byPhase['complete'].nextNow -eq $byPhase['restore-a'].nextNow -and $byPhase['complete'].lastSequence -eq $byPhase['restore-a'].lastSequence) 'restore/capture sequence barrier'
    Require ($byPhase['complete'].count -eq 2 -and $byPhase['complete'].queue -eq 0 -and $byPhase['complete'].revision -ge $minerCommands[7].revision) 'restore acceptance/capture'
    $travels=$rows.TRAVEL; Require ($travels.Count -eq 4) 'exact forced travel count'
    $travelBySequence=@{}
    foreach($t in $travels) {
        Schema $t @('owner','epoch','revision','sequence','miner','depot','cargoBefore','cargoAfter','xBefore','yBefore','zBefore','x','y','z','depotX','depotY','depotZ')
        foreach($n in @('owner','epoch','revision','sequence','miner','depot','cargoBefore','cargoAfter')){Require (Integer $t.$n) "invalid travel integer $n"}
        Coordinates $t @('xBefore','yBefore','zBefore','x','y','z','depotX','depotY','depotZ')
        $key=[string]$t.sequence; Require ($commandBySequence.ContainsKey($key) -and -not $travelBySequence.ContainsKey($key)) 'travel sequence duplicate/missing'
        $c=$commandBySequence[$key]
        Require ($t.owner -eq 1 -and $t.epoch -eq $anchor.epoch -and $c.type -eq 9 -and $t.miner -eq $c.unit -and $t.depot -eq $c.target -and $t.revision -gt $c.revision -and $t.cargoBefore -eq $c.cargoAfter -and $t.cargoAfter -eq $t.cargoBefore) 'travel identity/cargo'
        $before=[pscustomobject]@{x=$t.xBefore;y=$t.yBefore;z=$t.zBefore}
        Require ((Squared $before $t) -gt 0 -and (Squared $t $t '' 'depot') -gt 1 -and (Squared $t $t '' 'depot') -lt (Squared $before $t '' 'depot')) 'positive loaded travel toward exact refinery'
        $ref=@(Units $start $t.depot)[0]; Require ((Squared $ref $t '' 'depot') -le 0.00000001) 'travel refinery/source disagreement'
        $travelBySequence[$key]=$t
    }
    $deposits=$rows.DEPOSIT; Require ($deposits.Count -ge 4 -and $deposits.Count -le 256) 'bounded deposit observations'
    $forcedDeposits=@{}; $depositKeys=@{}; $boundaries=@{}
    foreach($d in $deposits) {
        Schema $d @('owner','epoch','revision','sequence','resumeResource','miner','depot','before','after','sum','creditsBefore','creditsAfter','gatheredBefore','gatheredAfter','x','y','z','depotX','depotY','depotZ','conservedBefore','conservedAfter','contributors')
        foreach($n in @('owner','epoch','revision','sequence','miner','depot','before','after','sum','creditsBefore','creditsAfter','gatheredBefore','gatheredAfter','conservedBefore','conservedAfter')){Require (Integer $d.$n) "invalid deposit integer $n"}
        Require (Integer $d.resumeResource -1) 'invalid deposit saved resource'
        Coordinates $d @('x','y','z','depotX','depotY','depotZ')
        $key="$($d.revision):$($d.miner)"
        Require ($d.owner -eq 1 -and $d.epoch -eq $anchor.epoch -and $d.miner -in @($anchor.a,$anchor.b) -and $d.depot -eq $anchor.refinery -and -not $depositKeys.ContainsKey($key) -and $d.before -ge 1 -and $d.before -le 50 -and $d.after -eq 0 -and $d.creditsAfter-$d.creditsBefore -eq $d.sum -and $d.gatheredAfter-$d.gatheredBefore -eq $d.sum -and $d.conservedBefore -eq $conserved -and $d.conservedAfter -eq $conserved) 'deposit identity/credit/conservation'
        $depositKeys[$key]=$true
        Require ((Squared $d $d '' 'depot') -le 1) 'deposit outside exact authoritative boundary'
        $ref=@(Units $start $d.depot)[0]; Require ((Squared $ref $d '' 'depot') -le 0.00000001) 'deposit refinery/source disagreement'
        Require ($d.contributors -is [array] -and $d.contributors.Count -ge 1 -and $d.contributors.Count -le 2) 'bounded contributor array'
        $contributors=@{}; $sum=0
        foreach($c in $d.contributors) {
            Schema $c @('miner','before','after','depot','sequence')
            foreach($n in @('miner','before','after','depot','sequence')) { Require (Integer $c.$n) 'invalid contributor integer' }
            Require ($c.miner -in @($anchor.a,$anchor.b) -and $c.depot -eq $anchor.refinery -and $c.before -ge 1 -and $c.before -le 50 -and $c.after -eq 0 -and -not $contributors.ContainsKey([string]$c.miner)) 'invalid/duplicate contributor'
            $contributors[[string]$c.miner]=$c; $sum+=$c.before-$c.after
            if($c.sequence -gt 0){Require ($commandBySequence.ContainsKey([string]$c.sequence) -and $commandBySequence[[string]$c.sequence].type -eq 9 -and $commandBySequence[[string]$c.sequence].unit -eq $c.miner) 'contributor ReturnCargo attribution'}
        }
        Require ($sum -eq $d.sum -and $contributors.ContainsKey([string]$d.miner)) 'deposit contributor sum'
        $mine=$contributors[[string]$d.miner]
        Require ($mine.before -eq $d.before -and $mine.after -eq $d.after -and $mine.sequence -eq $d.sequence) 'tracked contribution'
        $boundary=([ordered]@{sum=$d.sum;creditsBefore=$d.creditsBefore;creditsAfter=$d.creditsAfter;gatheredBefore=$d.gatheredBefore;gatheredAfter=$d.gatheredAfter;contributors=@($d.contributors | Sort-Object miner)} | ConvertTo-Json -Depth 4 -Compress)
        $revision=[string]$d.revision
        if($boundaries.ContainsKey($revision)){Require ($boundaries[$revision] -ceq $boundary) 'contradictory same-step credit boundary'}else{$boundaries[$revision]=$boundary}
        if($d.sequence -gt 0) {
            $seq=[string]$d.sequence
            Require ($commandBySequence.ContainsKey($seq) -and $travelBySequence.ContainsKey($seq) -and -not $forcedDeposits.ContainsKey($seq)) 'missing/duplicate forced deposit'
            $c=$commandBySequence[$seq]
            Require ($c.type -eq 9 -and $d.miner -eq $c.unit -and $d.depot -eq $c.target -and $d.resumeResource -eq $c.resumeResource -and $d.before -eq $c.cargoAfter -and $d.revision -gt $travelBySequence[$seq].revision) 'forced deposit command/travel attribution'
            $forcedDeposits[$seq]=$d
        }
    }
    Require ($forcedDeposits.Count -eq 4) 'exact forced deposit count'
    foreach($d in $deposits){foreach($c in $d.contributors){Require ($depositKeys.ContainsKey("$($d.revision):$($c.miner)")) 'missing contributor deposit observation'}}
    # Every unique Advance credits its contributors once. Shared rows for a
    # simultaneous two-miner boundary are not added twice.
    $priorGathered=-1
    foreach($revision in @($boundaries.Keys | Sort-Object {[long]$_})) {
        $b=$boundaries[$revision] | ConvertFrom-Json
        Require ($b.gatheredBefore -ge $priorGathered) 'reused/overlapping credit boundary'
        $priorGathered=$b.gatheredAfter
    }
    # Bind absolute totals, not only deltas, to every validated common state.
    # A published phase may trail a legitimate ordinary Produce, so equality
    # against only that phase is insufficient. Replay all owner-1 economic
    # boundaries from the validated selected checkpoint; Advance deposits at a
    # shared revision are represented once, followed by any rejected commands.
    $ledger=[Collections.Generic.List[object]]::new(); $mutatingRevisions=@{}
    # Existing frozen model costs; this validator changes no balance values.
    $commandCosts=@{2=50;3=100;4=150}
    foreach($c in $commands) {
        if($c.revision -le $anchor.revision){continue}
        # Spawn commands also bump revision inside AddUnit; no one-step assumption.
        Require ($c.revisionBefore -ge $anchor.revision -and $(if($c.accepted -eq 1){$c.revision -gt $c.revisionBefore}else{$c.revision -eq $c.revisionBefore})) 'command ledger revision step'
        $cost=if($c.accepted -eq 1 -and $commandCosts.ContainsKey([int]$c.type)){$commandCosts[[int]$c.type]}else{0}
        Require ($c.gatheredAfter -eq $c.gatheredBefore -and $c.creditsBefore-$c.creditsAfter -eq $cost) 'command ledger economy mutation'
        if($c.accepted -eq 1) {
            Require (-not $mutatingRevisions.ContainsKey([string]$c.revision)) 'duplicate authoritative mutation revision'
            $mutatingRevisions[[string]$c.revision]=$true
        }
        $ledger.Add([pscustomobject]@{revision=[long]$c.revision;order=1;sequence=[long]$c.sequence;kind='command';row=$c})
    }
    foreach($key in $boundaries.Keys) {
        if([long]$key -le $anchor.revision){continue}
        Require (-not $mutatingRevisions.ContainsKey([string]$key)) 'duplicate authoritative mutation revision'
        $mutatingRevisions[[string]$key]=$true
        $ledger.Add([pscustomobject]@{revision=[long]$key;order=0;sequence=0;kind='deposit';row=($boundaries[$key] | ConvertFrom-Json)})
    }
    foreach($key in $ValidatedStateContext.common) {
        $state=$ValidatedStateContext.maps.host[$key]
        if($state.epoch -ne $anchor.epoch -or $state.revision -le $anchor.revision){continue}
        $player=@($state.players | Where-Object id -eq 1)
        Require ($player.Count -eq 1) 'ledger checkpoint missing owner'
        $ledger.Add([pscustomobject]@{revision=[long]$state.revision;order=2;sequence=0;kind='checkpoint';row=$player[0]})
    }
    $ledgerCredits=[long]$anchor.credits; $ledgerGathered=[long]$anchor.gathered
    foreach($event in @($ledger | Sort-Object revision,order,sequence)) {
        $row=$event.row
        if($event.kind -eq 'checkpoint') {
            Require ($row.credits -eq $ledgerCredits -and $row.gathered -eq $ledgerGathered) 'published checkpoint disagrees with economy ledger'
        } else {
            Require ($row.creditsBefore -eq $ledgerCredits -and $row.gatheredBefore -eq $ledgerGathered) 'disconnected absolute economy boundary'
            $ledgerCredits=[long]$row.creditsAfter; $ledgerGathered=[long]$row.gatheredAfter
        }
    }
    $resumes=$rows.RESUME; Require ($resumes.Count -eq 3) 'exact saved Gather reload count'
    $resumeBySequence=@{}
    foreach($r in $resumes) {
        Schema $r @('owner','epoch','revision','sequence','miner','resource','cargoBefore','cargoAfter','remainingBefore','remainingAfter','x','y','z','resourceX','resourceY','resourceZ')
        foreach($n in @('owner','epoch','revision','sequence','miner','resource','cargoBefore','cargoAfter','remainingBefore','remainingAfter')) { Require (Integer $r.$n) "invalid resume integer $n" }
        Coordinates $r @('x','y','z','resourceX','resourceY','resourceZ')
        $key=[string]$r.sequence
        Require ($forcedDeposits.ContainsKey($key) -and -not $resumeBySequence.ContainsKey($key)) 'resume sequence duplicate/missing'
        $d=$forcedDeposits[$key]
        Require ($r.owner -eq 1 -and $r.epoch -eq $anchor.epoch -and $r.miner -eq $d.miner -and $r.resource -eq $anchor.resource -and $r.resource -eq $d.resumeResource -and $r.revision -gt $d.revision -and $r.cargoBefore -eq 0 -and $r.cargoAfter -gt 0 -and $r.cargoAfter -le 50 -and $r.remainingBefore-$r.remainingAfter -eq $r.cargoAfter) 'saved Gather authoritative reload'
        Require ((Squared $r $r '' 'resource') -le 1) 'reload outside exact resource boundary'
        $ore=@($start.resources | Where-Object id -eq $r.resource)[0]; Require ((Squared $ore $r '' 'resource') -le 0.00000001) 'reload resource/source disagreement'
        Require ($r.remainingBefore -le $ore.remaining) 'reload ore exceeds validated initial source'
        $earlier=$null; $later=$null
        foreach($checkpoint in $ValidatedStateContext.common) {
            $state=$ValidatedStateContext.maps.host[$checkpoint]
            if($state.epoch -ne $anchor.epoch){continue}
            if($state.revision -lt $r.revision -and ($null -eq $earlier -or $state.revision -gt $earlier.revision)){$earlier=$state}
            if($state.revision -ge $r.revision -and ($null -eq $later -or $state.revision -lt $later.revision)){$later=$state}
        }
        Require ($null -ne $earlier -and $null -ne $later) 'reload lacks bracketing source checkpoints'
        $oreEarlier=@($earlier.resources | Where-Object id -eq $r.resource); $oreLater=@($later.resources | Where-Object id -eq $r.resource)
        Require ($oreEarlier.Count -eq 1 -and $oreLater.Count -eq 1 -and $r.remainingBefore -le $oreEarlier[0].remaining -and $r.remainingAfter -ge $oreLater[0].remaining) 'reload absolute ore outside monotone source bounds'
        $exactKey="$($anchor.epoch):$($r.revision)"
        if($ValidatedStateContext.maps.host.ContainsKey($exactKey)) {
            $exact=$ValidatedStateContext.maps.host[$exactKey]
            $oreExact=@($exact.resources | Where-Object id -eq $r.resource); $minerExact=@(Units $exact $r.miner)
            Require ($oreExact.Count -eq 1 -and $minerExact.Count -eq 1 -and $r.remainingAfter -eq $oreExact[0].remaining -and $r.cargoAfter -eq $minerExact[0].cargo) 'reload disagrees with exact authoritative state'
        }
        Require (@($minerCommands | Where-Object {$_.unit -eq $r.miner -and $_.revision -gt $commandBySequence[$key].revision -and $_.revision -le $r.revision}).Count -eq 0) 'intervening command invalidates saved Gather proof'
        $resumeBySequence[$key]=$r
    }
    $cycles=@(
        @{command=$forced[0];deposited='single-deposited';resumed='single-resumed'},
        @{command=$forced[1];deposited='loaded-only-deposited';resumed='loaded-only-resumed'},
        @{command=$forced[2];deposited='group-deposited';resumed=$null},
        @{command=$forced[3];deposited='group-deposited';resumed='group-resumed'})
    $proofs=@()
    foreach($cycle in $cycles) {
        $c=$cycle.command; $key=[string]$c.sequence; $d=$forcedDeposits[$key]; $p=$byPhase[$cycle.deposited]
        $s=$ValidatedStateContext.maps.host["$($p.epoch):$($p.revision)"]; $miner=@(Units $s $c.unit)[0]
        Require ($p.revision -ge $d.revision -and $miner.cargo -eq 0 -and $p.gathered -ge $d.gatheredAfter) 'common post-deposit state'
        if($cycle.resumed) {
            Require ($resumeBySequence.ContainsKey($key)) 'missing saved Gather reload'
            $r=$resumeBySequence[$key]; $p=$byPhase[$cycle.resumed]; $s=$ValidatedStateContext.maps.host["$($p.epoch):$($p.revision)"]; $miner=@(Units $s $c.unit)[0]
            Require ($p.revision -ge $r.revision -and $miner.cargo -eq $r.cargoAfter -and
                @($deposits | Where-Object {$_.miner -eq $r.miner -and $_.revision -gt $r.revision -and $_.revision -le $p.revision}).Count -eq 0) 'common first resumed load state'
        } else { Require ($c.resumeResource -eq -1 -and -not $resumeBySequence.ContainsKey($key)) 'stopped miner must not auto-Gather' }
        $proofs+=@{miner=$c.unit;sequence=$c.sequence;refinery=$c.target;travelRevision=$travelBySequence[$key].revision;depositRevision=$d.revision;resumeResource=$c.resumeResource;resumeRevision=$(if($resumeBySequence.ContainsKey($key)){$resumeBySequence[$key].revision}else{$null})}
    }
    $stopDeposit=$forcedDeposits[[string]$forced[2].sequence]
    foreach($key in $ValidatedStateContext.common) {
        $s=$ValidatedStateContext.maps.host[$key]
        if($s.epoch -ne $anchor.epoch -or $s.revision -lt $stopDeposit.revision -or $s.revision -gt $byPhase['restore-a'].revision){continue}
        $a=@(Units $s $anchor.a)
        Require ($a.Count -eq 1 -and $a[0].cargo -eq 0 -and (Squared $a[0] $stopDeposit) -le 0.00000001) 'stopped miner moved/reloaded before explicit restore'
    }
    $captures=@([regex]::Matches($Log,'PROOF_RETURN_CARGO_CAPTURE[^\r\n]*'))
    Require ($captures.Count -eq 1 -and $captures[0].Value -ceq 'PROOF_RETURN_CARGO_CAPTURE name=03-return-cargo.png selectedCount=2 written=1 sharedHandlers=1 nativeCallbackInvoked=0 physicalInputVerified=0 networkTransportVerified=0') 'capture input scope/count'
    $completions=@([regex]::Matches($Log,'PROOF_SKIRMISH_COMPLETE role=client owner=1[^\r\n]*'))
    Require ($completions.Count -eq 1 -and $completions[0].Value -ceq 'PROOF_SKIRMISH_COMPLETE role=client owner=1 status=pass scenario=Victory winnerTeam=0 built=1 gathered=1 produced=1 botBuilt=1 botProduced=1 botAttacked=1 reason=complete_loop physicalInput=unverified') 'ordinary Victory required'
    return [ordered]@{passed=$true;epoch=$anchor.epoch;miners=@($anchor.a,$anchor.b);refinery=$anchor.refinery;resource=$anchor.resource;returnCommandCount=4;trackedMinerCommandCount=8;sharedHandlers=$true;nativeCallbackInvoked=$false;physicalInputVerified=$false;networkTransportVerified=$false;executionScope='Standalone local-authority shared handlers';phases=$phases;proofs=$proofs;depositBoundary='Immediate authoritative before/after Advance, squared distance <= 1';savedGatherEvidence='Accepted-command provenance plus authoritative reload without intervening miner command';ordinaryVictory=$true}
}
