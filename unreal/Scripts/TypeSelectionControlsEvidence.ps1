# Pure offline policy for the opt-in rendered shared-T handler fixture.
function Get-TypeSelectionControlsEvidence {
    param([Parameter(Mandatory)][hashtable]$Logs)
    function Need($Condition,[string]$Why){if(-not $Condition){throw "Type selection evidence: $Why"}}
    function Num($V){return $null -ne $V -and $V -isnot [string] -and $V -isnot [bool] -and $V -isnot [array] -and [double]::IsFinite([double]$V)}
    function Int($V,[long]$Min=0,[long]$Max=2147483647){return (Num $V) -and [double]$V -ge $Min -and [double]$V -le $Max -and [double]$V -eq [math]::Truncate([double]$V)}
    function Keys($E){
        if($E.ValueKind -eq [System.Text.Json.JsonValueKind]::Object){$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);foreach($p in $E.EnumerateObject()){Need ($seen.Add($p.Name)) 'duplicate JSON key';Keys $p.Value}}
        elseif($E.ValueKind -eq [System.Text.Json.JsonValueKind]::Array){foreach($v in $E.EnumerateArray()){Keys $v}}
    }
    function Json([string]$S){
        Need ($S.Length -gt 0 -and $S.Length -le 32768 -and $S -notmatch '[^\x00-\x7F]') 'invalid JSON row size/encoding'
        $d=$null;try{$d=[System.Text.Json.JsonDocument]::Parse($S);Need ($d.RootElement.ValueKind -eq [System.Text.Json.JsonValueKind]::Object) 'object required';Keys $d.RootElement}finally{if($d){$d.Dispose()}}
        return ($S|ConvertFrom-Json -Depth 12 -ErrorAction Stop)
    }
    function Identity($R){Need ((Int $R.epoch) -and $R.epoch -eq 0 -and (Int $R.revision) -and $R.digest -is [string] -and $R.digest -cmatch '^[a-f0-9]{32}$') 'invalid epoch/revision/digest'}
    function Ids($A){Need ($A -is [array] -and $A.Count -le 12) 'invalid ID array';$last=0;foreach($id in $A){Need ((Int $id 1) -and $id -gt $last) 'invalid/nonunique/unsorted IDs';$last=$id}}
    function Units($A){
        Need ($A -is [array] -and $A.Count -le 128) 'invalid unit array';$seen=@{}
        foreach($u in $A){Need ((Int $u.id 1) -and -not $seen.ContainsKey([long]$u.id) -and (Int $u.owner) -and (Int $u.kind 0 4) -and (Int $u.cargo 0 50)) 'invalid/duplicate unit identity';$seen[[long]$u.id]=$true
            foreach($f in @('hp','x','y','z')){Need (Num $u.$f) 'invalid unit numeric field'}
            Need ($u.hp -ge 0 -and $u.hp -le 300 -and [math]::Abs($u.x) -le 10000 -and [math]::Abs($u.y) -le 10000 -and [math]::Abs($u.z) -le 10000) 'unit outside bounds'
        }
    }
    function SameIds($A,$B){return ($A -join ',') -ceq ($B -join ',')}
    function SameUnit($A,$B){foreach($f in @('id','owner','kind','hp','cargo','x','y','z')){if($A.$f -ne $B.$f){return $false}};return $true}
    $normal=@{};$clocks=@{};$frames=@();$clockLast=-1;$secondsLast=-1.0
    foreach($role in @('host','rendered','observer')){
        Need ($Logs.ContainsKey($role) -and $Logs[$role] -is [string]) "missing role $role"
        $text=$Logs[$role];Need ($text.Length -le 16MB -and $text -notmatch 'PROOF_TYPE_SELECTION_ERROR') 'oversized log or producer error'
        $view=if($role -eq 'host'){'host'}else{'client'};$owner=if($role -eq 'host'){0}elseif($role -eq 'rendered'){1}else{2};$map=@{}
        foreach($m in [regex]::Matches($text,'PROOF_SNAPSHOT role=(host|client) owner=(\d+)(?: epoch=(\d+))? revision=(\d+) digest=([a-f0-9]{32})')){
            if($m.Groups[1].Value -cne $view -or [long]$m.Groups[2].Value -ne $owner){continue}
            Need (-not $m.Groups[3].Success -or [long]$m.Groups[3].Value -eq 0) 'ordinary epoch mismatch'
            $r=[long]$m.Groups[4].Value;$digest=$m.Groups[5].Value
            if($map.ContainsKey($r)){Need ($map[$r] -ceq $digest) 'contradictory ordinary digest'};$map[$r]=$digest
        }
        Need ($map.Count -ge 3) "missing ordinary snapshots $role";$normal[$role]=$map
        $rows=0;$bytes=0
        foreach($m in [regex]::Matches($text,'PROOF_TYPE_SELECTION_CLOCK([^\r\n]*)')){
            Need ($role -eq 'host') 'clock outside host';$rows++;$bytes+=$m.Length;Need ($rows -le 1024 -and $bytes -le 4MB) 'clock cap'
            $c=Json $m.Groups[1].Value.Trim();Identity $c;Units $c.units
            Need ((Num $c.simulatedSeconds) -and $c.simulatedSeconds -ge 0 -and $c.simulatedSeconds -le 60 -and $c.simulatedSeconds -ge $secondsLast -and $c.revision -gt $clockLast -and (Int $c.owner1Submissions 0 128)) 'clock/counter ordering or bounds'
            Need ($map.ContainsKey([long]$c.revision) -and $map[[long]$c.revision] -ceq $c.digest) 'clock ordinary digest mismatch'
            $clocks[[long]$c.revision]=$c;$clockLast=[long]$c.revision;$secondsLast=[double]$c.simulatedSeconds
        }
        $rows=0;$bytes=0
        foreach($m in [regex]::Matches($text,'PROOF_TYPE_SELECTION_FRAME([^\r\n]*)')){
            Need ($role -eq 'rendered') 'frame outside rendered role';$rows++;$bytes+=$m.Length;Need ($rows -le 1024 -and $bytes -le 4MB) 'frame cap'
            $f=Json $m.Groups[1].Value.Trim();Identity $f;Ids $f.selected;Ids $f.expected;Ids $f.group9;Units $f.units
            Need ($f.phase -cin @('before','after','observed') -and (Int $f.owner) -and $f.owner -eq 1 -and (Int $f.width) -and $f.width -eq 1280 -and (Int $f.height) -and $f.height -eq 720 -and (Int $f.primary 1) -and (Int $f.groupPrimary 1)) 'invalid frame envelope'
            Need ((Int $f.nativeT) -and $f.nativeT -eq 0 -and (Int $f.sequence) -and $f.sequence -eq 4 -and (Int $f.queue) -and $f.queue -eq 0) 'native callback, sequence or queue changed'
            Need ($f.expected.Count -eq 2 -and $f.units.Count -eq 2 -and (SameIds @($f.units.id|Sort-Object) $f.expected)) 'expected real two-unit set missing'
            foreach($u in $f.units){Need ($u.owner -eq 1 -and $u.kind -eq 0 -and $u.hp -gt 0 -and (Num $u.screenX) -and (Num $u.screenY) -and $u.screenX -ge 0 -and $u.screenX -lt $f.width -and $u.screenY -ge 0 -and $u.screenY -lt $f.height) 'unit not live owned combat with visible projected center'}
            Need ($map.ContainsKey([long]$f.revision) -and $map[[long]$f.revision] -ceq $f.digest) 'frame ordinary digest mismatch'
            $frames+=,$f
        }
        $complete=[regex]::Matches($text,'PROOF_TYPE_SELECTION_COMPLETE([^\r\n]*)')
        if($role -eq 'rendered'){Need ($complete.Count -eq 1 -and $complete[0].Groups[1].Value.Trim() -ceq 'status=pass inputSource=sharedHandler nativeCallbackInvoked=0 physicalInputVerified=0') 'missing/extraneous or false-scope completion'}else{Need ($complete.Count -eq 0) 'completion outside rendered role'}
    }
    Need ($frames.Count -ge 5 -and $frames[0].phase -ceq 'before' -and $frames[1].phase -ceq 'after') 'before/after phases missing or reordered'
    $before=$frames[0];$after=$frames[1];$anchor=[long]$before.primary
    Need ($before.revision -le $after.revision) 'action precedes initial selection'
    Need ($before.selected.Count -eq 1 -and $before.selected[0] -eq $anchor -and $before.expected -contains $anchor -and (SameIds $before.group9 @($anchor)) -and $before.groupPrimary -eq $anchor) 'invalid initial anchor/group'
    Need ($after.actionBeforeDigest -is [string] -and $after.actionBeforeDigest -ceq $after.digest) 'shared handler mutated snapshot digest'
    $common=@();$seen=@{};$lastRev=-1
    foreach($f in $frames){
        Need ((SameIds $f.expected $before.expected) -and $f.primary -eq $anchor -and (SameIds $f.group9 $before.group9) -and $f.groupPrimary -eq $anchor) 'candidate/primary/group changed'
        foreach($u in $f.units){$old=@($before.units|Where-Object id -eq $u.id)[0];foreach($k in @('x','y','z','cargo')){Need ($u.$k -eq $old.$k) 'unit moved or cargo changed during selection'}}
        if($f.phase -cne 'before'){Need (SameIds $f.selected $before.expected) 'selection is not exact expected pair'}
        if($f.phase -ceq 'observed'){Need ($f.revision -ge $after.revision -and $f.revision -gt $lastRev -and -not $seen.ContainsKey([long]$f.revision)) 'duplicate/regressing observed revision';$seen[[long]$f.revision]=$true;$lastRev=[long]$f.revision}
        else{Need (($f -eq $before) -or ($f -eq $after)) 'duplicate before/after phase'}
        Need ($clocks.ContainsKey([long]$f.revision)) 'missing authoritative frame snapshot'
        $c=$clocks[[long]$f.revision]
        Need ($c.digest -ceq $f.digest -and $c.owner1Submissions -eq 4) 'host command ingress or digest changed'
        foreach($u in $f.units){$hostUnits=@($c.units|Where-Object id -eq $u.id);Need ($hostUnits.Count -eq 1 -and (SameUnit $hostUnits[0] $u)) 'host unit semantics differ from rendered snapshot'}
        if($f.phase -cne 'observed'){continue}
        if($normal.observer.ContainsKey([long]$f.revision) -and $normal.observer[[long]$f.revision] -ceq $f.digest){$common+=,$c}
    }
    Need ($common.Count -ge 3) 'need three common observed host/rendered/observer revisions'
    $span=[double]$common[-1].simulatedSeconds-[double]$common[0].simulatedSeconds
    Need ($span -ge 1.0) 'need one actual host simulation second without new commands or movement'
    # Immediate action must also have an authoritative published state, not merely a local claimed digest.
    Need ($clocks.ContainsKey([long]$after.revision)) 'missing authoritative action snapshot'
    return [ordered]@{passed=$true;inputSource='sharedHandler';nativeCallbackInvoked=$false;physicalInputVerified=$false;osWindowFocusVerified=$false;selectedIds=@($before.expected);primary=$anchor;commonRevisions=@($common.revision);hostSimulationSeconds=$span;hostOwner1Submissions=4;clockRows=$clocks.Count;frameRows=$frames.Count}
}
