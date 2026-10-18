# Isolated diagnostic contract. No engine launch, mutation, or implicit old-observer fallback.
function Assert-AutoGather($Condition,[string]$Message) { if (-not $Condition) { throw "AutoGather: $Message" } }
function Test-AutoInteger($Value,[long]$Min=0,[long]$Max=2147483647) {
    return (($Value -is [int] -or $Value -is [long]) -and $Value -ge $Min -and $Value -le $Max)
}
function Test-AutoNumber($Value) {
    return (($Value -is [int] -or $Value -is [long] -or $Value -is [double]) -and [double]::IsFinite([double]$Value))
}
function Assert-AutoFields($Object,[string[]]$Names) {
    Assert-AutoGather ($Object -is [System.Collections.IDictionary]) 'expected object'
    Assert-AutoGather ($Object.Count -eq $Names.Count) 'unexpected or missing fields'
    foreach($name in $Names) { Assert-AutoGather (@($Object.Keys) -ccontains $name) "missing field $name" }
}
function ConvertTo-AutoG17([double]$Value) {
    $text=$Value.ToString('G17',[Globalization.CultureInfo]::InvariantCulture).ToLowerInvariant()
    # C printf uses at least two exponent digits; .NET may emit three.
    if($text -match '^(.*e[+-])0+(\d{2,})$') { $text=$Matches[1]+$Matches[2] }
    return $text
}
function Get-AutoSnapshotDigest($S) {
    $c='revision:{0};epoch:{1};skirmish:{2};phase:{3};winner:{4};' -f $S.revision,$S.epoch,[int][bool]$S.skirmish,$S.phase,$S.winner
    foreach($p in $S.players) { $c+='p:{0},{1},{2},{3};' -f $p.id,$p.credits,$p.team,$p.gathered }
    foreach($u in $S.units) {
        $c+='u:{0},{1},{2},{3},{4},{5},{6},{7};' -f $u.id,$u.owner,(ConvertTo-AutoG17 $u.x),(ConvertTo-AutoG17 $u.y),(ConvertTo-AutoG17 $u.z),(ConvertTo-AutoG17 $u.hp),$u.kind,$u.cargo
        if($u.rally -or $u.rx -ne 0 -or $u.ry -ne 0 -or $u.rz -ne 0) { $c+='q:{0},{1},{2},{3},{4};' -f $u.id,[int][bool]$u.rally,(ConvertTo-AutoG17 $u.rx),(ConvertTo-AutoG17 $u.ry),(ConvertTo-AutoG17 $u.rz) }
        if($u.dx -ne 0 -or $u.dy -ne 0 -or $u.dz -ne 0) { $c+='d:{0},{1},{2},{3};' -f $u.id,(ConvertTo-AutoG17 $u.dx),(ConvertTo-AutoG17 $u.dy),(ConvertTo-AutoG17 $u.dz) }
    }
    foreach($r in $S.resources) { $c+='r:{0},{1},{2},{3},{4},{5};' -f $r.id,$r.owner,(ConvertTo-AutoG17 $r.x),(ConvertTo-AutoG17 $r.y),(ConvertTo-AutoG17 $r.z),$r.remaining }
    return [Convert]::ToHexString([Security.Cryptography.MD5]::HashData([Text.Encoding]::ASCII.GetBytes($c)))
}
function ConvertTo-AutoSnapshot($S) {
    Assert-AutoFields $S @('revision','epoch','skirmish','phase','winner','digest','players','units','resources')
    Assert-AutoGather ((Test-AutoInteger $S.revision) -and (Test-AutoInteger $S.epoch 1) -and $S.skirmish -is [bool] -and $S.skirmish -and (Test-AutoInteger $S.phase 0 2) -and (Test-AutoInteger $S.winner -1 100) -and $S.digest -cmatch '^[A-Fa-f0-9]{32}$') 'snapshot header'
    $out=[ordered]@{revision=$S.revision;epoch=$S.epoch;skirmish=$S.skirmish;phase=$S.phase;winner=$S.winner;digest=$S.digest.ToUpperInvariant();players=@();units=@();resources=@()}
    foreach($collection in @('players','units','resources')) {
        Assert-AutoGather ($S[$collection] -is [array]) 'snapshot collection must be array'
        $cap=@{players=8;units=128;resources=32}[$collection]
        Assert-AutoGather ($S[$collection].Count -le $cap) 'snapshot collection cap'
        $last=0
        foreach($r in $S[$collection]) {
            $names=switch($collection) { players {@('id','credits','team','gathered')} units {@('id','owner','x','y','z','dx','dy','dz','hp','kind','cargo','rally','rx','ry','rz')} resources {@('id','owner','x','y','z','remaining')} }
            Assert-AutoFields $r $names
            Assert-AutoGather ((Test-AutoInteger $r.id 1) -and $r.id -gt $last) "record IDs must be unique ascending positive integers: $collection/$($r.id)/prior=$last"; $last=$r.id
            $v=[ordered]@{};foreach($name in $names){$v[$name]=$r[$name]}
            if($collection -eq 'players') {
                Assert-AutoGather ((Test-AutoInteger $r.credits) -and (Test-AutoInteger $r.team 0 100) -and (Test-AutoInteger $r.gathered)) 'player fields'
            } else {
                Assert-AutoGather (Test-AutoInteger $r.owner 1) 'record owner'
                $coordinates=if($collection -eq 'units'){@('x','y','z','dx','dy','dz','rx','ry','rz','hp')}else{@('x','y','z')}
                foreach($axis in $coordinates) { Assert-AutoGather ((Test-AutoNumber $r[$axis]) -and [math]::Abs([double]$r[$axis]) -le 10000) "finite bounded $axis"; $v[$axis]=[double]$r[$axis] }
                if($collection -eq 'units') {
                    Assert-AutoGather ((Test-AutoInteger $r.kind 0 4) -and (Test-AutoInteger $r.cargo 0 50) -and $r.hp -ge 0 -and $r.hp -le 300 -and $r.rally -is [bool]) 'unit fields'
                    Assert-AutoGather ($r.rz -eq 0 -and (($r.rally -and $r.kind -eq 4 -and $r.hp -gt 0) -or (-not $r.rally -and $r.rx -eq 0 -and $r.ry -eq 0))) 'canonical rally'
                    $direction=[double]$r.dx*$r.dx+[double]$r.dy*$r.dy
                    Assert-AutoGather ($r.dz -eq 0 -and ($direction -eq 0 -or [math]::Abs($direction-1) -le 1e-12)) 'canonical last travel direction'
                } else { Assert-AutoGather (Test-AutoInteger $r.remaining) 'resource remaining' }
            }
            $out[$collection]+=,$v
        }
    }
    Assert-AutoGather ((Get-AutoSnapshotDigest $out) -ceq $out.digest) 'full snapshot digest mismatch'
    return $out
}
function Get-AutoSemantic($S) { return ConvertTo-Json -InputObject $S -Depth 20 -Compress }
function Get-AutoUnit($S,$Id) { return @($S.units | Where-Object id -eq $Id)[0] }
function Get-AutoPlayer($S,$Id) { return @($S.players | Where-Object id -eq $Id)[0] }
function Get-AutoResource($S,$Id) { return @($S.resources | Where-Object id -eq $Id)[0] }
function Get-AutoDistance($A,$B) { return [math]::Pow(($A.x-$B.x),2)+[math]::Pow(($A.y-$B.y),2)+[math]::Pow(($A.z-$B.z),2) }
function Get-AutoOreTotal($S) { $n=[long]0; foreach($r in $S.resources){$n+=$r.remaining};foreach($u in $S.units){$n+=$u.cargo};foreach($p in $S.players){$n+=$p.gathered};return $n }
function Get-AutoDepot($S,$Miner) { return @($S.units | Where-Object { $_.kind -eq 3 -and $_.owner -eq $Miner.owner -and $_.hp -gt 0 -and (Get-AutoDistance $_ $Miner) -le 1 })[0] }
function Assert-AutoJsonElement($Element,[int]$Depth=0) {
    Assert-AutoGather ($Depth -le 24) 'JSON depth'
    if($Element.ValueKind -eq [Text.Json.JsonValueKind]::Object) {
        $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach($p in $Element.EnumerateObject()) { Assert-AutoGather ($seen.Add($p.Name)) 'duplicate JSON property'; Assert-AutoJsonElement $p.Value ($Depth+1) }
    } elseif($Element.ValueKind -eq [Text.Json.JsonValueKind]::Array) {
        Assert-AutoGather ($Element.GetArrayLength() -le 128) 'JSON array cap'
        foreach($item in $Element.EnumerateArray()){Assert-AutoJsonElement $item ($Depth+1)}
    }
}
function Get-RefineryAutoGatherEvidence {
    param([hashtable]$Logs,[string]$RunId,[string]$SourceTree)
    Assert-AutoGather ($RunId -cmatch '^[a-f0-9]{32}$' -and $SourceTree -cmatch '^[a-f0-9]{40}$') 'expected source/run binding'
    Assert-AutoGather ($Logs.Count -eq 3 -and $Logs.ContainsKey('host') -and $Logs.ContainsKey('rendered') -and $Logs.ContainsKey('observer')) 'exact three roles required'
    $maps=@{}; $streams=@{}; $counts=@{}
    foreach($role in @('host','rendered','observer')) {
        $viewer=@{host=0;rendered=1;observer=2}[$role];$view=if($viewer -eq 0){'host'}else{'client'}
        $text=[string]$Logs[$role]
        Assert-AutoGather ($text.Length -le 24MB -and $text -notmatch 'PROOF_AUTO_GATHER_ERROR|Fatal error:|Assertion failed:') "bounded error-free $role log"
        $matches=[regex]::Matches($text,'(?m)^[^\r\n]*?PROOF_AUTO_GATHER (?<json>\{[^\r\n]*\})\r?$')
        Assert-AutoGather ($matches.Count -gt 1 -and $matches.Count -le 4096 -and $matches.Count -eq [regex]::Matches($text,'PROOF_AUTO_GATHER ').Count) 'missing/malformed/truncated stream record'
        $records=@();$map=@{};$ordinal=0;$bytes=0;$snapshotCount=0;$epoch=0;$finished=$false;$lastRevision=-1
        $normal=@{}
        foreach($m in [regex]::Matches($text,'PROOF_SNAPSHOT role=(host|client|observer) owner=(\d+) epoch=(\d+) revision=(\d+) digest=([A-Za-z0-9]+)')) {
            if($m.Groups[1].Value -ne $view -or [int]$m.Groups[2].Value -ne $viewer){continue}
            $key=$m.Groups[3].Value+':'+$m.Groups[4].Value;$digest=$m.Groups[5].Value.ToUpperInvariant()
            Assert-AutoGather (-not $normal.ContainsKey($key) -or $normal[$key] -ceq $digest) 'contradictory ordinary digest';$normal[$key]=$digest
        }
        foreach($m in $matches) {
            Assert-AutoGather (-not $finished) 'record after finish'
            $json=$m.Groups['json'].Value;$bytes+=$json.Length
            Assert-AutoGather ($json.Length -le 65536 -and $bytes -le 16MB) 'stream byte cap'
            $document=[Text.Json.JsonDocument]::Parse($json)
            try { Assert-AutoJsonElement $document.RootElement } finally { $document.Dispose() }
            $r=ConvertFrom-Json -InputObject $json -AsHashtable -Depth 24
            $base=@('v','run','source','role','viewer','row','event','epoch')
            $extra=switch($r.event) { snapshot {@('state')} command {@('owner','type','unit','target','sequence','payloadEpoch','cx','cy','cz','accepted','grant','before','after')} {$_ -in @('travel','load','return','deposit')} {@('owner','miner','refinery','resource','before','after')} finish {@('passed','records','snapshots')} default {throw 'AutoGather: unknown stream event'} }
            Assert-AutoFields $r ($base+$extra)
            Assert-AutoGather ((Test-AutoInteger $r.v 1 1) -and $r.run -ceq $RunId -and $r.source -ceq $SourceTree -and $r.role -ceq $view -and (Test-AutoInteger $r.viewer $viewer $viewer) -and (Test-AutoInteger $r.row 1) -and $r.row -eq (++$ordinal) -and (Test-AutoInteger $r.epoch 1)) 'stream identity/order'
            if($epoch -eq 0){$epoch=$r.epoch}; Assert-AutoGather ($r.epoch -eq $epoch) 'epoch changed within fixture'
            if($r.event -eq 'snapshot') {
                $s=ConvertTo-AutoSnapshot $r.state;$r.state=$s;$snapshotCount++
                Assert-AutoGather ($s.epoch -eq $epoch -and $s.revision -gt $lastRevision -and $snapshotCount -le 1536) 'snapshot epoch/revision/cap';$lastRevision=$s.revision
                $key="$epoch`:$($s.revision)"
                Assert-AutoGather ($normal.ContainsKey($key) -and $normal[$key] -ceq $s.digest -and -not $map.ContainsKey($key)) 'ordinary snapshot binding';$map[$key]=$s
            } elseif($r.event -eq 'finish') {
                Assert-AutoGather ($r.passed -is [bool] -and $r.passed -and (Test-AutoInteger $r.records 1) -and $r.records -eq $ordinal -and (Test-AutoInteger $r.snapshots 1) -and $r.snapshots -eq $snapshotCount) 'incomplete stream finish'
                $finished=$true
            } else {
                Assert-AutoGather ($viewer -eq 0) 'authoritative event outside host'
                $r.before=ConvertTo-AutoSnapshot $r.before;$r.after=ConvertTo-AutoSnapshot $r.after
                Assert-AutoGather ($r.before.epoch -eq $epoch -and $r.after.epoch -eq $epoch -and $r.after.revision -ge $r.before.revision) 'event trusted epoch/revision'
            }
            $records+=,$r
        }
        Assert-AutoGather ($finished -and $map.Count -ge 4) 'missing stream finish or snapshots'
        $last=$records[-2]; Assert-AutoGather ($last.event -eq 'snapshot' -and $last.state.phase -eq 2) 'finish must follow published terminal snapshot'
        $streams[$role]=$records;$maps[$role]=$map;$counts[$role]=$ordinal
    }
    $common=@($maps.host.Keys | Where-Object { $maps.rendered.ContainsKey($_) -and $maps.observer.ContainsKey($_) } | Sort-Object {[int]($_.Split(':')[0])},{[int]($_.Split(':')[1])})
    Assert-AutoGather ($common.Count -ge 4) 'insufficient common published revisions'
    foreach($key in $common) { foreach($role in @('rendered','observer')) { Assert-AutoGather ((Get-AutoSemantic $maps.host[$key]) -ceq (Get-AutoSemantic $maps[$role][$key])) 'common full snapshot differs' } }
    $grants=@{};$usedIds=@{};$proofs=@()
    foreach($r in $streams.host) {
        if($r.event -eq 'command') {
            Assert-AutoGather ((Test-AutoInteger $r.owner 1) -and (Test-AutoInteger $r.type 0 9) -and (Test-AutoInteger $r.unit -1) -and (Test-AutoInteger $r.target -1) -and (Test-AutoInteger $r.sequence 1) -and (Test-AutoInteger $r.payloadEpoch) -and $r.accepted -is [bool]) 'command fields'
            foreach($axis in @('cx','cy','cz')) { Assert-AutoGather (Test-AutoNumber $r[$axis]) 'command destination' }
            foreach($g in $grants.Values) { Assert-AutoGather (-not ($r.accepted -and $r.unit -eq $g.miner -and -not $g.phases.ContainsKey('deposit'))) 'accepted tracked miner override' }
            if(-not $r.accepted){Assert-AutoGather ((Get-AutoSemantic $r.before) -ceq (Get-AutoSemantic $r.after)) 'rejected command mutation'}
            if($r.accepted){Assert-AutoGather ($r.payloadEpoch -eq $r.epoch) 'accepted stale command'}
            if(-not ($r.accepted -and $r.type -eq 3 -and $r.owner -in @(1,2))) { Assert-AutoGather ($null -eq $r.grant) 'orphan grant';continue }
            Assert-AutoGather (-not $grants.ContainsKey([string]$r.owner)) 'duplicate owner grant'
            $g=$r.grant; Assert-AutoFields $g @('miner','refinery','resource','orderType','orderTarget','returning','resume','ox','oy','oz')
            foreach($id in @('miner','refinery','resource')) { Assert-AutoGather (Test-AutoInteger $g[$id] 1) 'grant identity' }
            Assert-AutoGather ($g.miner -ne $g.refinery -and -not $usedIds.ContainsKey([string]$g.miner) -and -not $usedIds.ContainsKey([string]$g.refinery)) 'reused grant ID'
            $usedIds[[string]$g.miner]=$true;$usedIds[[string]$g.refinery]=$true
            $before=$r.before;$after=$r.after;$miner=Get-AutoUnit $after $g.miner;$ref=Get-AutoUnit $after $g.refinery
            Assert-AutoGather ($null -eq (Get-AutoUnit $before $g.miner) -and $null -eq (Get-AutoUnit $before $g.refinery) -and $null -ne $miner -and $null -ne $ref -and $miner.owner -eq $r.owner -and $ref.owner -eq $r.owner -and $miner.kind -eq 1 -and $ref.kind -eq 3 -and $miner.hp -eq 100 -and $ref.hp -eq 200 -and $miner.cargo -eq 0 -and $ref.cargo -eq 0 -and $miner.dx -eq 0 -and $miner.dy -eq 0 -and $miner.dz -eq 0 -and -not $miner.rally -and -not $ref.rally) 'new pair state'
            Assert-AutoGather ($ref.x -eq $r.cx -and $ref.y -eq $r.cy -and $ref.z -eq $r.cz -and (Get-AutoDistance $miner $ref) -ge 67600) 'build/spawn geometry'
            foreach($u in $before.units) { if($u.hp -gt 0){Assert-AutoGather ((Get-AutoDistance $u $miner) -ge 67600) 'spawn separation'} }
            $expected=ConvertFrom-Json (Get-AutoSemantic $before) -AsHashtable -Depth 20
            $p=Get-AutoPlayer $expected $r.owner; Assert-AutoGather ($null -ne $p -and $p.credits -ge 100) 'build payer';$p.credits-=100
            $expected.units=@((@($expected.units)+@($miner,$ref)) | Sort-Object {[long]$_.id});$expected.revision+=3;$expected.digest=Get-AutoSnapshotDigest $expected
            Assert-AutoGather ((Get-AutoSemantic (ConvertTo-AutoSnapshot $expected)) -ceq (Get-AutoSemantic $after)) 'exact build transaction/preserved state'
            $nearest=$null;$distance=0.0
            foreach($ore in $after.resources) { if($ore.remaining -le 0){continue};$d=Get-AutoDistance $miner $ore;if($null -eq $nearest -or $d -lt $distance -or ($d -eq $distance -and $ore.id -lt $nearest.id)){$nearest=$ore;$distance=$d} }
            Assert-AutoGather ($null -ne $nearest -and $nearest.id -eq $g.resource -and (Test-AutoInteger $g.orderType 5 5) -and (Test-AutoInteger $g.orderTarget 1) -and $g.orderTarget -eq $nearest.id -and $g.returning -is [bool] -and -not $g.returning -and (Test-AutoInteger $g.resume -1 -1)) 'actual initial order/independent target'
            foreach($axis in @('ox','oy','oz')){Assert-AutoGather ((Test-AutoNumber $g[$axis]) -and $g[$axis] -eq 0) 'initial order destination'}
            $g.owner=$r.owner;$g.epoch=$r.epoch;$g.sequence=$r.sequence;$g.built=$after;$g.phases=@{};$g.buildRow=$r.row
            $grants[[string]$r.owner]=$g
        } elseif($r.event -in @('travel','load','return','deposit')) {
            Assert-AutoGather ((Test-AutoInteger $r.owner 1 2) -and $grants.ContainsKey([string]$r.owner)) 'orphan trip phase'
            $g=$grants[[string]$r.owner]
            Assert-AutoGather ((Test-AutoInteger $r.miner 1) -and (Test-AutoInteger $r.refinery 1) -and (Test-AutoInteger $r.resource 1) -and $r.miner -eq $g.miner -and $r.refinery -eq $g.refinery -and $r.resource -eq $g.resource -and $r.epoch -eq $g.epoch -and -not $g.phases.ContainsKey($r.event)) 'phase identity/uniqueness'
            $phaseIndex=@('travel','load','return','deposit').IndexOf([string]$r.event)
            Assert-AutoGather ($g.phases.Count -eq $phaseIndex -and $r.before.revision -ge $g.built.revision -and $r.after.revision -gt $r.before.revision -and (Get-AutoOreTotal $r.before) -eq (Get-AutoOreTotal $r.after)) 'phase order/conservation'
            if($phaseIndex -gt 0){$previous=$g.phases[@('travel','load','return','deposit')[$phaseIndex-1]];Assert-AutoGather ($r.before.revision -ge $previous.before.revision -and $r.after.revision -ge $previous.after.revision) 'phase revision regression'}
            $a=Get-AutoUnit $r.before $g.miner;$b=Get-AutoUnit $r.after $g.miner;$oreA=Get-AutoResource $r.before $g.resource;$oreB=Get-AutoResource $r.after $g.resource
            Assert-AutoGather ($null -ne $a -and $null -ne $b -and $a.owner -eq $g.owner -and $b.owner -eq $g.owner -and $a.kind -eq 1 -and $b.kind -eq 1 -and $a.hp -gt 0 -and $b.hp -gt 0 -and $null -ne $oreA -and $null -ne $oreB) 'phase live identities'
            if($r.event -in @('travel','return')) {
                $moveX=$b.x-$a.x;$moveY=$b.y-$a.y;$length=[math]::Sqrt($moveX*$moveX+$moveY*$moveY)
                Assert-AutoGather ($length -gt 0 -and [math]::Abs($b.dx-$moveX/$length) -le 1e-12 -and [math]::Abs($b.dy-$moveY/$length) -le 1e-12) 'last direction must describe actual travel'
            }
            switch($r.event) {
                travel { Assert-AutoGather ($a.cargo -eq 0 -and $b.cargo -eq 0 -and (Get-AutoDistance $a $b) -gt 0 -and (Get-AutoDistance $b $oreB) -lt (Get-AutoDistance $a $oreB)) 'actual outward travel' }
                load { Assert-AutoGather ($a.cargo -eq 0 -and $b.cargo -gt 0 -and $oreA.remaining-$oreB.remaining -ge $b.cargo -and (Get-AutoDistance $b $oreB) -le 1) 'actual attributed load' }
                return { $nearA=[double]::PositiveInfinity;$nearB=$nearA;foreach($u in $r.after.units){if($u.owner -eq $g.owner -and $u.kind -eq 3 -and $u.hp -gt 0){$nearA=[math]::Min($nearA,(Get-AutoDistance $a $u));$nearB=[math]::Min($nearB,(Get-AutoDistance $b $u))}};Assert-AutoGather ($a.cargo -gt 0 -and (Get-AutoDistance $a $b) -gt 0 -and $nearB -lt $nearA) 'actual loaded return travel' }
                deposit {
                    $sum=0;$contributors=@()
                    foreach($u in $r.before.units){$v=Get-AutoUnit $r.after $u.id;if($u.owner -eq $g.owner -and $u.kind -eq 1 -and $u.hp -gt 0 -and $u.cargo -gt 0 -and $null -ne $v -and $v.owner -eq $g.owner -and $v.kind -eq 1 -and $v.hp -gt 0 -and $v.cargo -lt $u.cargo -and $null -ne (Get-AutoDepot $r.after $v)){$sum+=$u.cargo-$v.cargo;$contributors+=,@{miner=$u.id;before=$u.cargo;after=$v.cargo;depot=(Get-AutoDepot $r.after $v).id}}}
                    $p=Get-AutoPlayer $r.before $g.owner;$q=Get-AutoPlayer $r.after $g.owner
                    Assert-AutoGather ($a.cargo -gt 0 -and $b.cargo -eq 0 -and $null -ne (Get-AutoDepot $r.after $b) -and $null -ne $p -and $null -ne $q -and $q.credits-$p.credits -eq $sum -and $q.gathered-$p.gathered -eq $sum -and $sum -ge $a.cargo -and @($contributors|Where-Object miner -eq $g.miner).Count -eq 1) 'actual deposit and all contributors'
                    $g.contributors=$contributors
                }
            }
            $g.phases[$r.event]=$r
        }
    }
    Assert-AutoGather ($grants.Count -eq 2 -and $grants.ContainsKey('1') -and $grants.ContainsKey('2')) 'both human automatic grants required'
    foreach($owner in @('1','2')) {
        $g=$grants[$owner];Assert-AutoGather ($g.phases.Count -eq 4) 'incomplete automatic trip'
        $role=if($owner -eq '1'){'rendered'}else{'observer'}
        $send=[regex]::Matches([string]$Logs[$role],'PROOF_AUTO_GATHER_SEND owner=(\d+) epoch=(\d+) sequence=(\d+) type=(\d+)')
        $receipt=[regex]::Matches([string]$Logs[$role],'PROOF_AUTO_GATHER_RECEIPT owner=(\d+) epoch=(\d+) sequence=(\d+) accepted=1')
        Assert-AutoGather ($send.Count -eq 1 -and $receipt.Count -eq 1 -and [int]$send[0].Groups[1].Value -eq $g.owner -and [int]$receipt[0].Groups[1].Value -eq $g.owner -and [int]$send[0].Groups[2].Value -eq $g.epoch -and [int]$receipt[0].Groups[2].Value -eq $g.epoch -and [int]$send[0].Groups[3].Value -eq $g.sequence -and [int]$receipt[0].Groups[3].Value -eq $g.sequence -and [int]$send[0].Groups[4].Value -eq 3) 'client build send/accepted receipt correlation'
        Assert-AutoGather ($send.Count -eq [regex]::Matches([string]$Logs[$role],'PROOF_AUTO_GATHER_SEND').Count -and $receipt.Count -eq [regex]::Matches([string]$Logs[$role],'PROOF_AUTO_GATHER_RECEIPT').Count -and $send[0].Index -lt $receipt[0].Index -and $receipt[0].Index -lt ([string]$Logs[$role]).LastIndexOf('PROOF_AUTO_GATHER ')) 'client send/receipt stream order'
        $check=@{};$spawn=Get-AutoUnit $g.built $g.miner
        foreach($key in $common) {
            $s=$maps.host[$key];if($s.epoch -ne $g.epoch -or $s.revision -lt $g.built.revision){continue};$u=Get-AutoUnit $s $g.miner;$p=Get-AutoPlayer $s $g.owner;$ore=Get-AutoResource $s $g.resource
            if($null -eq $u -or $u.owner -ne $g.owner -or $u.kind -ne 1 -or $u.hp -le 0 -or $null -eq $p -or $null -eq $ore){continue}
            if(-not $check.ContainsKey('identity')){$check.identity=$key}
            if(-not $check.ContainsKey('travel') -and $s.revision -ge $g.phases.travel.after.revision -and $s.revision -lt $g.phases.load.after.revision -and $u.cargo -eq 0 -and (Get-AutoDistance $u $spawn) -gt 0 -and (Get-AutoDistance $u $ore) -lt (Get-AutoDistance $spawn $ore)){$check.travel=$key}
            if(-not $check.ContainsKey('loaded') -and $s.revision -ge $g.phases.load.after.revision -and $s.revision -lt $g.phases.deposit.after.revision -and $u.cargo -eq (Get-AutoUnit $g.phases.load.after $g.miner).cargo -and $ore.remaining -le (Get-AutoResource $g.phases.load.after $g.resource).remaining){$check.loaded=$key}
            if(-not $check.ContainsKey('deposited') -and $s.revision -ge $g.phases.deposit.after.revision -and $u.cargo -eq 0 -and $p.gathered -ge (Get-AutoPlayer $g.phases.deposit.after $g.owner).gathered -and $null -ne (Get-AutoDepot $s $u)){$check.deposited=$key}
        }
        Assert-AutoGather ($check.Count -eq 4) 'missing actual receiving-client common identity/travel/load/deposit checkpoints'
        $proofs+=,@{owner=$g.owner;epoch=$g.epoch;buildSequence=$g.sequence;miner=$g.miner;refinery=$g.refinery;resource=$g.resource;checkpoints=$check;contributors=$g.contributors}
    }
    return [ordered]@{passed=$true;run=$RunId;sourceTree=$SourceTree;rowsByRole=$counts;commonEpochRevisions=$common;automaticTrips=$proofs;physicalInputVerified=$false;internetTest=$false;performanceTest=$false;privateOrderReplicated=$false}
}
