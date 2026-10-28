#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$RunDirectory,
    [ValidateRange(1,10)][int]$MaxGaps = 3,
    [string]$ProcessTimeZoneId = ''
)
$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
$root = (Resolve-Path -LiteralPath $RunDirectory).Path
$maxBytes = 16MB

# Hash the same bytes that are parsed. Never follow paths supplied by result.json.
function Read-Evidence([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return @{path=$Path;missing=$true;error='missing-file';sha256=$null;lines=@()} }
    $stream = [IO.File]::OpenRead($Path)
    try {
        if ($stream.Length -gt $maxBytes) { return @{path=$Path;missing=$false;error='file-exceeds-16MiB';sha256=$null;lines=@()} }
        $memory = [IO.MemoryStream]::new()
        try {
            $buffer = [byte[]]::new(8192)
            $count = 0
            while (($read = $stream.Read($buffer,0,$buffer.Length)) -gt 0) {
                $count += $read
                if ($count -gt $maxBytes) { return @{path=$Path;missing=$false;error='file-exceeds-16MiB';sha256=$null;lines=@()} }
                $memory.Write($buffer,0,$read)
            }
            $bytes = $memory.ToArray()
        } finally { $memory.Dispose() }
    } finally { $stream.Dispose() }
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $hash = [BitConverter]::ToString($hasher.ComputeHash($bytes)).Replace('-','').ToLowerInvariant() } finally { $hasher.Dispose() }
    $text = [Text.UTF8Encoding]::new($false,$true).GetString($bytes).TrimStart([char]0xFEFF)
    return @{path=$Path;missing=$false;error=$null;sha256=$hash;bytes=$bytes.Length;text=$text;lines=@($text -split '\r?\n')}
}

function Get-Provenance($Evidence) {
    return [ordered]@{path=$Evidence.path;sha256=$Evidence.sha256;bytes=$Evidence.bytes;missing=$Evidence.missing;error=$Evidence.error}
}

function Get-Stamp([string]$Line) {
    if ($Line -match '^\[(?<stamp>\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]') {
        $time = [datetime]::MinValue
        if ([datetime]::TryParseExact($Matches.stamp,'yyyy.MM.dd-HH.mm.ss:fff',$culture,[Globalization.DateTimeStyles]::None,[ref]$time)) {
            return [datetimeoffset]::new([datetime]::SpecifyKind($time,[DateTimeKind]::Utc))
        }
    }
    return $null
}

function Find-Marker($Evidence,[string]$Pattern,$NotBefore) {
    for ($index=0; $index -lt $Evidence.lines.Count; $index++) {
        $line = $Evidence.lines[$index]
        if ($line -match $Pattern) {
            $stamp = Get-Stamp $line
            if ($stamp -and (-not $NotBefore -or $stamp -ge $NotBefore)) {
                return [ordered]@{utc=$stamp.ToString('o');line=($index+1);path=$Evidence.path;text=$line}
            }
        }
    }
    return $null
}

function Get-ProcessStart([string]$Raw) {
    if (-not $Raw) { return @{raw=$Raw;utc=$null;basis=$null;error='missing-process-start'} }
    # TryParse can infer today's date or midnight for incomplete values. Require
    # the runner's complete ISO date/time before applying timezone normalization.
    if ($Raw -notmatch '^\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})?$') {
        return @{raw=$Raw;utc=$null;basis=$null;error='invalid-process-start'}
    }
    $parsed = [datetimeoffset]::MinValue
    if ($Raw -match '(Z|[+-]\d{2}:\d{2})$') {
        if ([datetimeoffset]::TryParse($Raw,$culture,[Globalization.DateTimeStyles]::None,[ref]$parsed)) {
            return @{raw=$Raw;utc=$parsed.ToUniversalTime().ToString('o');basis='explicit-offset';error=$null}
        }
        return @{raw=$Raw;utc=$null;basis=$null;error='invalid-process-start'}
    }
    if (-not $ProcessTimeZoneId) { return @{raw=$Raw;utc=$null;basis=$null;error='unqualified-process-time-requires-zone'} }
    $local = [datetime]::MinValue
    if (-not [datetime]::TryParse($Raw,$culture,[Globalization.DateTimeStyles]::None,[ref]$local)) { return @{raw=$Raw;utc=$null;basis=$null;error='invalid-process-start'} }
    $local = [datetime]::SpecifyKind($local,[DateTimeKind]::Unspecified)
    $zone = [TimeZoneInfo]::FindSystemTimeZoneById($ProcessTimeZoneId)
    if ($zone.IsInvalidTime($local) -or $zone.IsAmbiguousTime($local)) { return @{raw=$Raw;utc=$null;basis=$ProcessTimeZoneId;error='invalid-or-ambiguous-local-process-time'} }
    $utc = [TimeZoneInfo]::ConvertTimeToUtc($local,$zone)
    return @{raw=$Raw;utc=$utc.ToString('o');basis=('explicit-zone:'+ $ProcessTimeZoneId);error=$null}
}

function Get-Interval($From,$To) {
    if (-not $From -or -not $To) { return $null }
    $seconds = ([datetimeoffset]::Parse($To,$culture)-[datetimeoffset]::Parse($From,$culture)).TotalSeconds
    if ($seconds -lt 0) { return $null }
    return [math]::Round($seconds,3)
}

$resultEvidence = Read-Evidence (Join-Path $root 'result.json')
if ($resultEvidence.error) { throw "Cannot read result evidence: $($resultEvidence.error)" }
# JsonDocument preserves timestamp strings independently of PowerShell's JSON date conversions.
$document = [System.Text.Json.JsonDocument]::Parse([string]$resultEvidence.text)
try {
    $json = $document.RootElement
    $processArray = $json.GetProperty('processes')
    if ($processArray.GetArrayLength() -gt 16) { throw 'At most 16 process records are supported.' }
    $hostEvidence = Read-Evidence (Join-Path $root 'host.log')
    $roles = @()
    foreach ($process in $processArray.EnumerateArray()) {
        $role = $process.GetProperty('role').GetString()
        if ($role -notmatch '^[A-Za-z0-9_-]{1,64}$') { throw 'Unsafe or unsupported role filename.' }
        if (@($roles | Where-Object {$_.role -eq $role}).Count) { throw "Duplicate process role: $role" }
        $startProperty = [System.Text.Json.JsonElement]::new()
        $rawStart = if ($process.TryGetProperty('startUtc',[ref]$startProperty)) { $startProperty.GetString() } else { '' }
        $start = Get-ProcessStart $rawStart
        $evidence = if ($role -eq 'host') { $hostEvidence } else { Read-Evidence (Join-Path $root ($role+'.log')) }
        $first = Find-Marker $evidence '.' $null
        $init = Find-Marker $evidence 'LogLoad: \(Engine Initialization\) Total time:' $null
        $endpointPattern = switch ($role) {
            'host' { 'PROOF_LIFECYCLE_READY epoch=1(?:\s|$)' }
            'lead' { 'PROOF_LIFECYCLE_JOIN owner=1 epoch=1 seeded=1(?:\s|$)' }
            'owner2' { 'PROOF_LIFECYCLE_JOIN owner=2 epoch=1 seeded=1(?:\s|$)' }
            'rejoin' { 'PROOF_LIFECYCLE_JOIN owner=2 epoch=1 seeded=0(?:\s|$)' }
            'third' { 'PROOF_LIFECYCLE_REJECT reason=full(?:\s|$)' }
            default { '' }
        }
        $endpoint = if ($endpointPattern) { Find-Marker $hostEvidence $endpointPattern $(if ($start.utc) {[datetimeoffset]::Parse($start.utc,$culture)} else {$null}) } else { $null }
        $gaps = @()
        $timestampRegressions = 0
        if ($init) {
            $previous = $null
            for ($index=0; $index -lt $init.line; $index++) {
                $time = Get-Stamp $evidence.lines[$index]
                if ($time) {
                    if ($previous) {
                        $delta = ($time-$previous.time).TotalSeconds
                        if ($delta -lt 0) { $timestampRegressions++ }
                        elseif ($delta -gt 0) {
                            $gaps += [ordered]@{seconds=[math]::Round($delta,3);beforeLine=$previous.line;afterLine=($index+1);beforeUtc=$previous.time.ToString('o');afterUtc=$time.ToString('o');beforeText=$previous.text;afterText=$evidence.lines[$index]}
                            $gaps = @($gaps | Sort-Object @{Expression={[double]$_.seconds};Descending=$true},@{Expression={[int]$_.beforeLine};Descending=$false} | Select-Object -First $MaxGaps)
                        }
                    }
                    $previous = @{time=$time;line=($index+1);text=$evidence.lines[$index]}
                }
            }
        }
        $missing = @()
        if ($start.error) { $missing += $start.error }
        if ($evidence.error) { $missing += $evidence.error }
        if (-not $first) { $missing += 'first-dated-log' }
        if (-not $init) { $missing += 'engine-init-complete'; $missing += 'pre-init-gap-analysis-unavailable' }
        if (-not $endpoint) { $missing += 'role-ready-join-or-rejection' }
        $roles += [ordered]@{
            role=$role;processStart=$start;log=(Get-Provenance $evidence);firstDatedLog=$first;engineInitComplete=$init
            endpointKind=$(if(-not $endpointPattern){'unknown'}elseif($role -eq 'host'){'ready'}elseif($role -eq 'third'){'rejected-full'}else{'join'})
            endpoint=$endpoint;missingFields=$missing;timestampRegressions=$timestampRegressions
            seconds=[ordered]@{processToFirstLog=(Get-Interval $start.utc $first.utc);firstLogToInit=(Get-Interval $first.utc $init.utc);processToInit=(Get-Interval $start.utc $init.utc);initToEndpoint=(Get-Interval $init.utc $endpoint.utc);processToEndpoint=(Get-Interval $start.utc $endpoint.utc)}
            preInitGaps=@($gaps | Sort-Object @{Expression={[double]$_.seconds};Descending=$true},@{Expression={[int]$_.beforeLine};Descending=$false} | Select-Object -First $MaxGaps)
        }
    }
    [ordered]@{schemaVersion=1;runDirectory=$root;result=(Get-Provenance $resultEvidence);hostEndpointLog=(Get-Provenance $hostEvidence);timestampBasis='Unreal log timestamps are UTC; process timestamps require explicit offset or supplied zone';maxRankedGaps=$MaxGaps;roles=$roles;limitations=@('Wall-clock log intervals are observations, not subsystem execution times or causal evidence.','Gap ranking stops at the first engine-initialization completion line; missing completion disables gap analysis.','Host readiness can precede the engine-init completion message; negative intervals are null.','This summary does not evaluate lifecycle acceptance or alter a retained result.')} | ConvertTo-Json -Depth 12
} finally { $document.Dispose() }
