# Pure fixture-evidence check; no processes or file writes.
function Get-SkirmishOwnershipRetryEvidence {
    param([Parameter(Mandatory)][string]$Log,[switch]$RequireSemanticEvidence)
    if($Log.Length -gt 24MB){throw 'Ownership retry evidence: log too large'}
    if($Log -match 'PROOF_SKIRMISH_OWNERSHIP_REJECTION_ERROR'){throw 'Ownership retry evidence: diagnostic cap exceeded'}
    $diagnostics=@()
    foreach($m in [regex]::Matches($Log,'PROOF_SKIRMISH_OWNERSHIP_REJECTION([^\r\n]*)')){
        $row=$m.Groups[1].Value.Trim()
        if($row -notmatch '^owner=(\d+) sequence=(\d+) unit=(\d+) exists=([01]) unitOwner=(-?\d+) hp=(-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?) revision=(\d+) epoch=(\d+) type=Attack reason=ownership$'){throw 'Ownership retry evidence: malformed host diagnostic'}
        $diagnostics+=@{owner=[long]$Matches[1];sequence=[long]$Matches[2];unit=[long]$Matches[3];exists=[int]$Matches[4];unitOwner=[long]$Matches[5];hp=[double]::Parse($Matches[6],[Globalization.CultureInfo]::InvariantCulture);revision=[long]$Matches[7];epoch=[long]$Matches[8]}
        $d=$diagnostics[-1]
        foreach($key in @('owner','revision')){if($d[$key] -lt 0 -or $d[$key] -gt 2147483647){throw 'Ownership retry evidence: diagnostic integer bounds'}}
        foreach($key in @('unit','sequence','epoch')){if($d[$key] -lt 1 -or $d[$key] -gt 2147483647){throw 'Ownership retry evidence: diagnostic positive integer bounds'}}
        if($d.unitOwner -lt -1 -or $d.unitOwner -gt 2147483647 -or -not [double]::IsFinite($d.hp) -or $d.hp -lt 0 -or $d.hp -gt 300){throw 'Ownership retry evidence: diagnostic unit fields'}
        if($diagnostics.Count -gt 64){throw 'Ownership retry evidence: diagnostic cap exceeded'}
    }
    $result=@();$seen=@{}
    foreach($m in [regex]::Matches($Log,'PROOF_SKIRMISH_RETRY reason=ownership([^\r\n]*)')){
        $row=$m.Groups[1].Value.Trim()
        if($row -notmatch '^unit=(\d+) sequence=(\d+) snapshotRevision=(\d+) replicatedUnit=owned-dead$'){throw 'Ownership retry evidence: malformed retry'}
        $unit=[long]$Matches[1];$sequence=[long]$Matches[2];$revision=[long]$Matches[3]
        if($unit -lt 1 -or $unit -gt 2147483647 -or $sequence -lt 1 -or $sequence -gt 2147483647 -or $revision -gt 2147483647){throw 'Ownership retry evidence: retry integer bounds'}
        $key=('{0}:{1}' -f $unit,$sequence);if($seen.ContainsKey($key)){throw 'Ownership retry evidence: duplicate retry'};$seen[$key]=$true
        $matching=@($diagnostics|Where-Object {$_.unit -eq $unit -and $_.sequence -eq $sequence -and $_.owner -eq 1})
        if($matching.Count -ne 1){throw 'Ownership retry evidence: missing/duplicate matching host diagnostic'}
        $d=$matching[0]
        if($d.exists -ne 1 -or $d.unitOwner -ne $d.owner -or -not [double]::IsFinite($d.hp) -or $d.hp -gt 0 -or $d.epoch -le 0 -or $revision -lt $d.revision){throw 'Ownership retry evidence: retry did not follow validated owned-dead attacker'}
        $pattern=('PROOF_SNAPSHOT role=(host|client) owner=(\d+) epoch='+$d.epoch+' revision='+$revision+' digest=([a-f0-9]{32})\b')
        $rows=@([regex]::Matches($Log,$pattern)|Where-Object {($_.Groups[1].Value -ceq 'host' -and $_.Groups[2].Value -eq '0') -or ($_.Groups[1].Value -ceq 'client' -and $_.Groups[2].Value -eq '1')})
        if(@($rows|Where-Object {$_.Groups[1].Value -ceq 'host'}).Count -lt 1 -or @($rows|Where-Object {$_.Groups[1].Value -ceq 'client'}).Count -lt 1 -or @($rows|ForEach-Object {$_.Groups[3].Value}|Select-Object -Unique).Count -ne 1){throw 'Ownership retry evidence: missing matching ordinary host/client death revision'}
        $dead=@();$live=@()
        if($RequireSemanticEvidence){foreach($s in [regex]::Matches($Log,'PROOF_REFINERY_STATE ([^\r\n]+)')){
            $state=$s.Groups[1].Value|ConvertFrom-Json -Depth 16 -ErrorAction Stop
            if($state.role -cne 'client' -or $state.viewer -ne 1 -or $state.epoch -ne $d.epoch -or $state.phase -ne 1){continue}
            $u=@($state.units|Where-Object {$_.id -eq $unit -and $_.owner -eq 1})
            if($u.Count -ne 1){continue}
            if($state.revision -eq $revision -and $u[0].hp -le 0){$dead+=$state}
            if($state.revision -lt $d.revision -and $u[0].hp -gt 0){$live+=$state}
        }
        }
        if($RequireSemanticEvidence -and ($dead.Count -ne 1 -or $live.Count -lt 1)){throw 'Ownership retry evidence: missing same-epoch playing live/dead client observations'}
        $result+=@{unit=$unit;sequence=$sequence;hostRejectedRevision=$d.revision;clientDeadRevision=$revision;epoch=$d.epoch;hostHitPoints=$d.hp;owner=$d.owner;semanticEvidenceRequired=[bool]$RequireSemanticEvidence}
    }
    return ,$result
}
