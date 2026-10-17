# Pure, opt-in log policy. Does not launch processes or read/write files.
function Get-RefineryMinerEvidence {
    param([Parameter(Mandatory)][hashtable]$Logs,[ValidateSet('Victory','Defeat','Coop')][string]$Scenario)
    function Require($Condition,[string]$Reason) { if (-not $Condition) { throw "Refinery/miner evidence: $Reason" } }
    function Integer($Value,[long]$Minimum=0,[long]$Maximum=2147483647) {
        return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and [double]$Value -ge $Minimum -and [double]$Value -le $Maximum -and [double]$Value -eq [math]::Truncate([double]$Value)
    }
    function Number($Value) { return $null -ne $Value -and $Value -isnot [string] -and $Value -isnot [bool] -and [double]::IsFinite([double]$Value) }
    function JsonRow([string]$Text) {
        Require ($Text.Length -le 32768) 'JSON row exceeds byte cap'
        # Schema keys contain only ASCII names, so reject repeated names at each object
        # using the token reader before ConvertFrom-Json can silently overwrite them.
        $stack=[Collections.Generic.List[object]]::new()
        foreach($m in [regex]::Matches($Text,'"(?:\\.|[^"\\])*"\s*:|[{}\[\]]')) {
            $t=$m.Value
            if($t -eq '{') { $stack.Add(@{}) }
            elseif($t -eq '[') { $stack.Add($null) }
            elseif($t -in @('}',']')) { Require ($stack.Count -gt 0) 'unbalanced JSON'; $stack.RemoveAt($stack.Count-1) }
            else { Require ($stack.Count -gt 0 -and $null -ne $stack[$stack.Count-1]) 'JSON key outside object'; $k=($t -replace '\s*:$','' | ConvertFrom-Json); Require (-not $stack[$stack.Count-1].ContainsKey($k)) 'duplicate JSON key'; $stack[$stack.Count-1][$k]=$true }
        }
        Require ($stack.Count -eq 0) 'unbalanced JSON'
        try { $o=$Text | ConvertFrom-Json -Depth 16 -ErrorAction Stop } catch { throw 'Refinery/miner evidence: malformed JSON' }
        Require ($null -ne $o -and $o -isnot [array]) 'JSON object required'; return $o
    }
    function Unit($S,$Id) { return @($S.units | Where-Object {$_.id -eq $Id}) }
    function Player($S,$Id) { return @($S.players | Where-Object {$_.id -eq $Id}) }
    function Semantic($S) { return ([ordered]@{epoch=$S.epoch;revision=$S.revision;digest=$S.digest;phase=$S.phase;winner=$S.winner;units=$S.units;players=$S.players;resources=$S.resources} | ConvertTo-Json -Depth 12 -Compress) }
    $roles=if($Scenario -eq 'Coop'){@('host','rendered','observer')}else{@('host','rendered')}
    $owners=if($Scenario -eq 'Coop'){@(1,2,100)}else{@(1,100)}
    $maps=@{}; $counts=@{}; $events=@()
    foreach($role in $roles) {
        Require ($Logs.ContainsKey($role)) "missing role $role"
        $text=[string]$Logs[$role]; Require ($text.Length -le 24MB) 'log exceeds bounded input'; Require ($text -notmatch 'PROOF_REFINERY_ERROR') "runtime failure in $role"
        $view=if($role -eq 'host'){'host'}else{'client'}; $viewer=if($role -eq 'host'){0}elseif($role -eq 'observer'){2}else{1}
        $normal=@{}
        foreach($m in [regex]::Matches($text,'PROOF_SNAPSHOT role=(host|client|observer) owner=(\d+) epoch=(\d+) revision=(\d+) digest=([A-Za-z0-9]+)')) {
            if($m.Groups[1].Value -ne $view -or [int]$m.Groups[2].Value -ne $viewer) { continue }
            $key=$m.Groups[3].Value+':'+$m.Groups[4].Value; $digest=$m.Groups[5].Value
            if($normal.ContainsKey($key)) { Require ($normal[$key] -eq $digest) 'contradictory ordinary digest' }; $normal[$key]=$digest
        }
        $map=@{}; $bytes=0; $rows=0
        foreach($m in [regex]::Matches($text,'PROOF_REFINERY_STATE([^\r\n]*)')) {
            $s=JsonRow $m.Groups[1].Value
            Require ($s.role -in @('host','client') -and (Integer $s.viewer 0 2) -and (($s.role -eq 'host' -and $s.viewer -eq 0) -or ($s.role -eq 'client' -and $s.viewer -in @(1,2)))) 'invalid snapshot role'
            if($s.role -ne $view -or $s.viewer -ne $viewer) { continue }
            $rows++; $bytes+=$m.Groups[1].Length
            Require ($rows -le 1536 -and $bytes -le 8MB) 'snapshot cap exceeded'
            Require ((Integer $s.epoch 1) -and (Integer $s.revision) -and $s.digest -match '^[A-Fa-f0-9]{32}$' -and (Integer $s.phase 0 2) -and (Integer $s.winner -1 100)) 'invalid snapshot header'
            Require ($s.units -is [array] -and $s.players -is [array] -and $s.resources -is [array]) 'snapshot collections must be arrays'
            Require (@($s.units).Count -le 128 -and @($s.players).Count -le 8 -and @($s.resources).Count -le 32) 'snapshot record cap'
            foreach($collection in @('units','players','resources')) {
                $ids=@{}
                foreach($record in $s.$collection) {
                    Require ((Integer $record.id 1) -and -not $ids.ContainsKey([string]$record.id)) 'duplicate/invalid record ID'; $ids[[string]$record.id]=$true
                    if($collection -eq 'players') { Require ((Integer $record.credits) -and (Integer $record.gathered) -and (Integer $record.team 0 100)) 'invalid player'; continue }
                    Require (Integer $record.owner 1) 'invalid owner'
                    foreach($axis in @('x','y','z')) { Require ($null -ne $record.$axis -and (Number $record.$axis) -and [math]::Abs([double]$record.$axis) -le 10000) 'nonfinite/out of world position' }
                    if($collection -eq 'units') { Require ((Integer $record.kind 0 4) -and (Integer $record.cargo 0 50) -and (Number $record.hp) -and $record.hp -ge 0 -and $record.hp -le 300) 'invalid unit' }
                    else { Require (Integer $record.remaining) 'invalid resource' }
                }
            }
            $key="$($s.epoch):$($s.revision)"
            Require ($normal.ContainsKey($key) -and $normal[$key] -eq $s.digest) "semantic row lacks ordinary matching digest $role/$key"
            Require (-not $map.ContainsKey($key)) "duplicate semantic key $role/$key"; $map[$key]=$s
        }
        Require ($map.Count -gt 0) "missing semantic rows $role"; $maps[$role]=$map; $counts[$role]=$rows
        if($role -eq 'host') { foreach($m in [regex]::Matches($text,'PROOF_REFINERY_EVENT([^\r\n]*)')) { $events+=,(JsonRow $m.Groups[1].Value) }; Require ($events.Count -le 160) 'event cap' }
    }
    $common=@($maps.host.Keys | Where-Object { $key=$_; @($roles | Where-Object {-not $maps[$_].ContainsKey($key)}).Count -eq 0 } | Sort-Object {[int]($_.Split(':')[0])},{[int]($_.Split(':')[1])})
    Require ($common.Count -ge 3) 'fewer than three common epoch/revision checkpoints'
    foreach($key in $common) { foreach($role in $roles) { Require ((Semantic $maps.host[$key]) -ceq (Semantic $maps[$role][$key])) "common digest or semantic state differs $role/$key" } }
    foreach($e in $events) {
        Require ($e.event -in @('grant','gather','load','deposit','rejected') -and (Integer $e.epoch 1) -and $e.owner -in $owners -and (Integer $e.owner 1)) 'invalid event header'
        $fields = switch($e.event) {
            grant { @('sequence','beforeRevision','revision','added','refineriesAdded','minersAdded','creditsBefore','creditsAfter','cost') }
            gather { @('sequence','revision','resource') }
            load { @('revision','resource','cargo','oreBefore','oreAfter') }
            deposit { @('revision','depot','cargoBefore','cargoAfter','depositSum','creditsBefore','creditsAfter','gatheredBefore','gatheredAfter') }
            rejected { @('sequence') }
        }
        foreach($field in $fields) { Require (Integer $e.$field) "non-integer event field $field" }
        if($e.event -eq 'rejected') { Require ($e.beforeDigest -cmatch '^[A-Fa-f0-9]{32}$' -and $e.beforeDigest -ceq $e.afterDigest -and (Integer $e.sequence 1)) 'rejected build mutated state'; continue }
        Require ((Integer $e.refinery 1) -and (Integer $e.miner 1) -and (Integer $e.revision)) 'invalid event identity'
    }
    $grants=@($events | Where-Object event -eq grant); Require ($grants.Count -ge $owners.Count -and $grants.Count -le 32) 'missing/excess grants'
    $seen=@{}; $ids=@{}; $proofs=@()
    foreach($g in $grants) {
        $identity="$($g.epoch):$($g.owner):$($g.sequence)"
        Require ((Integer $g.sequence 1) -and -not $seen.ContainsKey($identity)) 'duplicate grant transaction'; $seen[$identity]=$true
        foreach($id in @($g.refinery,$g.miner)) { $k="$($g.epoch):$id"; Require (-not $ids.ContainsKey($k)) 'duplicate grant ID'; $ids[$k]=$true }
        Require ($g.added -eq 2 -and $g.refineriesAdded -eq 1 -and $g.minersAdded -eq 1 -and $g.cost -eq 100 -and (Integer $g.creditsBefore 100) -and (Integer $g.creditsAfter) -and $g.creditsBefore-$g.creditsAfter -eq 100) 'grant count/cost'
        Require ((Integer $g.beforeRevision) -and $g.revision -gt $g.beforeRevision -and $g.beforeDigest -match '^[A-Fa-f0-9]{32}$' -and $g.digest -match '^[A-Fa-f0-9]{32}$' -and $g.beforeDigest -cne $g.digest) 'grant revision/digest'
        $before=@{};$after=@{}
        Require ($g.beforeIds -is [array] -and $g.afterIds -is [array] -and $g.beforeIds.Count -le 126 -and $g.afterIds.Count -le 128) 'missing/bounded grant ID sets'
        foreach($id in $g.beforeIds){Require ((Integer $id 1) -and -not $before.ContainsKey([string]$id)) 'invalid before grant IDs';$before[[string]$id]=$true}
        foreach($id in $g.afterIds){Require ((Integer $id 1) -and -not $after.ContainsKey([string]$id)) 'invalid after grant IDs';$after[[string]$id]=$true}
        $added=@($after.Keys|Where-Object {-not $before.ContainsKey($_)})
        Require ($after.Count -eq $before.Count+2 -and @($before.Keys|Where-Object {-not $after.ContainsKey($_)}).Count -eq 0 -and $added.Count -eq 2 -and $added -contains [string]$g.refinery -and $added -contains [string]$g.miner) 'grant exact newly added pair'
        $distance=0.0
        foreach($axis in @('x','y','z')) { $ra='r'+$axis; foreach($field in @($axis,$ra)) { Require ((Number $g.$field) -and [math]::Abs([double]$g.$field) -le 10000) 'grant spawn' }; $distance+=[math]::Pow(($g.$axis-$g.$ra),2) }; Require ($distance -ge 67600) 'overlapping grant spawn'
        $related=@($events | Where-Object {$_.event -in @('gather','load','deposit') -and $_.epoch -eq $g.epoch -and $_.owner -eq $g.owner -and $_.miner -eq $g.miner -and $_.refinery -eq $g.refinery})
        $a=@($related | Where-Object event -eq gather); $l=@($related | Where-Object event -eq load); $d=@($related | Where-Object event -eq deposit)
        Require ($a.Count -eq 1 -and $l.Count -eq 1 -and $d.Count -eq 1) 'missing/duplicate accepted Gather/load/deposit'
        $a=$a[0];$l=$l[0];$d=$d[0]
        Require ((Integer $a.sequence 1) -and $a.sequence -gt $g.sequence -and $a.revision -gt $g.revision -and (Integer $a.resource 1) -and $l.resource -eq $a.resource -and $l.revision -ge $a.revision -and $d.revision -gt $l.revision) 'Gather identity/order'
        Require ((Integer $l.cargo 1 50) -and (Integer $l.oreBefore 1) -and (Integer $l.oreAfter) -and $l.oreBefore-$l.oreAfter -ge $l.cargo) 'load ore/cargo'
        Require ($d.cargoBefore -eq $l.cargo -and $d.cargoAfter -eq 0 -and (Integer $d.depot 1) -and (Integer $d.depositSum 1) -and (Integer $d.creditsBefore) -and (Integer $d.creditsAfter) -and (Integer $d.gatheredBefore) -and (Integer $d.gatheredAfter) -and $d.creditsAfter-$d.creditsBefore -eq $d.depositSum -and $d.gatheredAfter-$d.gatheredBefore -eq $d.depositSum) 'deposit credits/delivery'
        Require ($d.contributors -is [array] -and $d.contributors.Count -ge 1 -and $d.contributors.Count -le 12) 'contributors must be bounded array'
        $contributors=@{};$sum=0
        foreach($c in $d.contributors) { Require ((Integer $c.miner 1) -and (Integer $c.depot 1) -and (Integer $c.before 1 50) -and (Integer $c.after 0 50) -and $c.before -gt $c.after -and -not $contributors.ContainsKey([string]$c.miner)) 'deposit contributors';$contributors[[string]$c.miner]=$c;$sum+=$c.before-$c.after }
        Require ($sum -eq $d.depositSum -and $contributors.ContainsKey([string]$g.miner) -and $contributors[[string]$g.miner].before -eq $d.cargoBefore -and $contributors[[string]$g.miner].after -eq 0 -and $contributors[[string]$g.miner].depot -eq $d.depot) 'tracked miner contribution'
        $identityKey=$null;$loadKey=$null;$depositKey=$null
        foreach($key in $common) {
            $s=$maps.host[$key]; if($s.epoch -ne $g.epoch -or $s.revision -lt $g.revision) { continue }
            $miner=@(Unit $s $g.miner);$ref=@(Unit $s $g.refinery);$p=@(Player $s $g.owner)
            if($miner.Count -ne 1 -or $ref.Count -ne 1 -or $p.Count -ne 1 -or $miner[0].owner -ne $g.owner -or $ref[0].owner -ne $g.owner -or $miner[0].kind -ne 1 -or $ref[0].kind -ne 3 -or $miner[0].hp -le 0 -or $ref[0].hp -le 0) { continue }
            if(-not $identityKey){$identityKey=$key}
            $ore=@($s.resources | Where-Object id -eq $a.resource)
            if(-not $loadKey -and $s.revision -ge $l.revision -and $s.revision -lt $d.revision -and $miner[0].cargo -eq $l.cargo -and $ore.Count -eq 1 -and $ore[0].remaining -le $l.oreAfter){$loadKey=$key}
            $contributorIdentities=$true
            foreach($c in $d.contributors) {
                $cm=@(Unit $s $c.miner);$cr=@(Unit $s $c.depot)
                if($cm.Count -ne 1 -or $cr.Count -ne 1 -or $cm[0].owner -ne $g.owner -or $cm[0].kind -ne 1 -or $cr[0].owner -ne $g.owner -or $cr[0].kind -ne 3){$contributorIdentities=$false}
            }
            $depot=@(Unit $s $d.depot)
            if($contributorIdentities -and -not $depositKey -and $s.revision -ge $d.revision -and $miner[0].cargo -eq 0 -and $p[0].gathered -ge $d.gatheredAfter -and $depot.Count -eq 1 -and $depot[0].owner -eq $g.owner -and $depot[0].kind -eq 3 -and $depot[0].hp -gt 0){$depositKey=$key}
        }
        Require ($identityKey -and $loadKey -and $depositKey) "missing common identity/load/deposit state for owner $($g.owner) miner $($g.miner)"
        $proofs+=@{epoch=$g.epoch;owner=$g.owner;sequence=$g.sequence;refinery=$g.refinery;miner=$g.miner;gatherSequence=$a.sequence;resource=$a.resource;depositSum=$d.depositSum;contributors=$d.contributors;checkpoints=@{identity=$identityKey;loaded=$loadKey;deposited=$depositKey};digests=@{identity=$maps.host[$identityKey].digest;loaded=$maps.host[$loadKey].digest;deposited=$maps.host[$depositKey].digest}}
    }
    foreach($owner in $owners) { Require (@($grants | Where-Object owner -eq $owner).Count -ge 1) "missing owner $owner grant" }
    Require (@($events | Where-Object event -in @('gather','load','deposit')).Count -eq 3*$grants.Count) 'orphan phase event'
    return [ordered]@{passed=$true;requiredRoles=$roles;requiredOwners=$owners;rowsByRole=$counts;commonEpochRevisions=$common;grants=$proofs;physicalInputVerified=$false;internetTest=$false;performanceTest=$false;phasePolicy='common published identity, loaded cargo, and persistent post-deposit empty cargo/delivery total; immediate credit/ore attribution comes from authoritative bounded events'}
}
