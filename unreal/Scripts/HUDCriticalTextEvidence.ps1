function Get-HUDCriticalTextEvidence([string]$Log, [string[]]$Names) {
    if (-not $Names -or @($Names | Select-Object -Unique).Count -ne $Names.Count) { throw 'Need distinct requested capture names.' }
    $pattern='(?m)PROOF_HUD_CRITICAL_CAPTURE name=(?<name>\S+) group=(?<group>economy|selection) variant=(?<variant>-?\d+) complete=(?<complete>[01]) width=(?<width>\S+) available=(?<available>\S+) height=(?<height>\S+) maxheight=(?<maxheight>\S+)(?=\r?$)'
    $markers=[regex]::Matches($Log,$pattern)
    $expected=$Names.Count*2
    if ($markers.Count -ne $expected -or [regex]::Matches($Log,'PROOF_HUD_CRITICAL_CAPTURE').Count -ne $expected) { throw 'Need exactly two complete critical text observations per capture.' }
    foreach($name in $Names) {
        foreach($group in @('economy','selection')) {
            $matching=@($markers | Where-Object {$_.Groups['name'].Value -ceq $name -and $_.Groups['group'].Value -ceq $group})
            if($matching.Count -ne 1) { throw 'Missing or repeated critical text group.' }
            $m=$matching[0];$variant=0
            if(-not [int]::TryParse($m.Groups['variant'].Value,[ref]$variant) -or $variant -lt 0 -or $variant -gt 1 -or $m.Groups['complete'].Value -ne '1') { throw 'Critical text overflow or invalid variant.' }
            $r=[ordered]@{name=$name;group=$group;variant=$variant;complete=$true}
            foreach($field in @('width','available','height','maxheight')) {
                $value=0.0
                if(-not [double]::TryParse($m.Groups[$field].Value,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$value) -or [double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -le 0) { throw 'Invalid critical text measurement.' }
                $r[$field]=$value
            }
            if($r.width -gt $r.available -or $r.height -gt $r.maxheight) { throw 'Critical text exceeds measured bounds.' }
            [pscustomobject]$r
        }
    }
}