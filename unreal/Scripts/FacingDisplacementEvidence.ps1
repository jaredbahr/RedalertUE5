# Diagnostic oracle: only actual authoritative positions around real Advance.
# Expected direction never reads LastTravelDirection, destination or visual axes.
function Get-FacingDisplacementTrace([string]$Log) {
    if($Log.Length-gt24MB-or$Log-match'PROOF_FACING_DISPLACEMENT_ERROR'){throw 'Displacement trace failed or over bound'}
    function Keys($E){
        if($E.ValueKind-eq[Text.Json.JsonValueKind]::Object){$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase);foreach($p in $E.EnumerateObject()){if(-not$seen.Add($p.Name)){throw 'Duplicate displacement key'};Keys $p.Value}}
        elseif($E.ValueKind-eq[Text.Json.JsonValueKind]::Array){foreach($v in $E.EnumerateArray()){Keys $v}}
    }
    $matches=[regex]::Matches($Log,'(?m)^.*PROOF_FACING_DISPLACEMENT (\{[^\r\n]*\})\r?$')
    if($matches.Count-eq0-or$matches.Count-gt4096-or$matches.Count-ne[regex]::Matches($Log,'PROOF_FACING_DISPLACEMENT(?=\s|$)').Count){throw 'Incomplete displacement trace'}
    $accepted=[regex]::Matches($Log,'(?m)^.*PROOF_FACING_MOVE_ACCEPTED epoch=(\d+) unit=(\d+) sequence=(\d+) revision=(\d+)\r?$')
    if($accepted.Count-ne[regex]::Matches($Log,'PROOF_FACING_MOVE_ACCEPTED').Count){throw 'Incomplete accepted Move trace'}
    $commands=@{};$chains=@{}
    foreach($a in $accepted){$key=($a.Groups[1].Value+':'+$a.Groups[2].Value+':'+$a.Groups[3].Value);if($commands.ContainsKey($key)){throw 'Duplicate accepted Move'};$commands[$key]=$a}
    $rows=@(foreach($m in $matches){
        $doc=[Text.Json.JsonDocument]::Parse($m.Groups[1].Value);try{Keys $doc.RootElement}finally{$doc.Dispose()}
        $r=$m.Groups[1].Value|ConvertFrom-Json -ErrorAction Stop
        foreach($p in @('epoch','unit','sequence','beforeRevision','afterRevision')){
            $v=$r.$p
            if($null-eq$v-or$v-is[string]-or$v-is[bool]-or$v-is[array]-or-not[double]::IsFinite([double]$v)-or$v-lt1-or$v-gt[int]::MaxValue-or$v-ne[math]::Truncate([double]$v)){throw 'Displacement integer required'}
        }
        foreach($p in @('before','after')){if($r.$p-isnot[array]-or$r.$p.Count-ne3-or@($r.$p|Where-Object {$null-eq$_-or$_-is[string]-or$_-is[bool]-or-not[double]::IsFinite([double]$_)}).Count){throw 'Displacement finite position required'}}
        $key=([string]$r.epoch+':'+$r.unit+':'+$r.sequence)
        if(-not$commands.ContainsKey($key)){throw 'Displacement lacks accepted Move'}
        $a=$commands[$key]
        if($a.Index-ge$m.Index-or$r.beforeRevision-lt[int]$a.Groups[4].Value-or$r.afterRevision-le$r.beforeRevision){throw 'Displacement acceptance/revision order'}
        if($chains.ContainsKey($key)){
            $prior=$chains[$key]
            if($r.beforeRevision-lt$prior.afterRevision){throw 'Displacement revision regression'}
            for($i=0;$i-lt3;$i++){if($r.before[$i]-ne$prior.after[$i]){throw 'Displacement position chain gap'}}
        }
        $chains[$key]=$r
        $r
    })
    return $rows
}
function Get-FacingDisplacementAt {
    param($Trace,[int]$Epoch,[int]$Unit,[int]$Sequence,[int]$Revision,$Position)
    $steps=@($Trace|Where-Object {$_.epoch-eq$Epoch-and$_.unit-eq$Unit-and$_.sequence-eq$Sequence-and$_.afterRevision-le$Revision})
    if(-not$steps.Count){throw 'No actual Advance for observed command'}
    $last=$steps[-1]
    for($i=0;$i-lt3;$i++){if([math]::Abs($last.after[$i]-$Position[$i])-gt0.01){throw 'Observed position not bound to actual Advance'}}
    $direction=$null;$pair=$null;$travel=0.0
    foreach($s in $steps){
        $dx=[double]$s.after[0]-[double]$s.before[0];$dy=[double]$s.after[1]-[double]$s.before[1]
        if($s.after[2]-ne$s.before[2]){throw 'Nonhorizontal actual movement'}
        $scale=[math]::Max([math]::Abs($dx),[math]::Abs($dy))
        if($scale-eq0){continue}
        $x=$dx/$scale;$y=$dy/$scale;$length=[math]::Sqrt($x*$x+$y*$y)
        $direction=@(($x/$length),($y/$length),0.0);$pair=$s;$travel+=$scale*$length
    }
    if($null-eq$direction){throw 'No nonzero actual displacement'}
    return [ordered]@{direction=$direction;lastNonzeroAdvance=$pair;lastAdvance=$last;actualTravelCm=$travel}
}
