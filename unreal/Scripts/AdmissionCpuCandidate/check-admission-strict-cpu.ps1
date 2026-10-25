param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference = 'Stop'
$root = 'D:\UserTemp\RedalertUE5-timed-production-20261019'
$repo = Join-Path $root 'candidate'
$output = $OutputPath
$expectedHead = '22575d19c0f50805aeacf1d0b1d1b5f2d861d65d'
$expectedTree = '479733a5c279815169df5c51fd0df6fbdd47d7e9'
$expectedSource = '673bb524e9d63f1e783580a218f55e69da28b856'
$names = @('UnrealEditor.exe','UnrealEditor-Cmd.exe','UnrealBuildTool.exe',
    'ShaderCompileWorker.exe','LiveCodingConsole.exe','cl.exe','link.exe')
$head = (& git -C $repo rev-parse HEAD).Trim()
$tree = (& git -C $repo rev-parse 'HEAD^{tree}').Trim()
$source = (& git -C $repo rev-parse 'HEAD:unreal/Source').Trim()
$clean = @(& git -C $repo status --porcelain=v1).Count -eq 0
$before = @(Get-CimInstance Win32_Process | Where-Object {
    $names -contains $_.Name -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool')
})
$gamesBefore = @(Get-CimInstance Win32_Process | Where-Object {
    $_.ExecutablePath -like '*steamapps\common*' -or $_.ExecutablePath -like '*EA Games*' -or
    $_.ExecutablePath -like '*Origin Games*'
})
$memoryKiB = [int64](Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory
$dFree = [int64](Get-PSDrive -Name D).Free
$samples = @()
for ($i = 0; $i -lt 3; $i++) {
    $perf = @(Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor |
        Where-Object { $_.Name -in @('_Total','2','6') })
    $values = @{}
    foreach ($entry in $perf) {
        if ($entry.Name -isnot [string] -or $entry.Name -notin @('_Total','2','6')) { throw 'Invalid CPU instance.' }
        if ($values.ContainsKey($entry.Name)) { throw 'Duplicate CPU instance.' }
        $rawValue = $entry.PercentProcessorTime
        if ($null -eq $rawValue) { throw 'Missing CPU value.' }
        $isNumeric = $rawValue -is [byte] -or $rawValue -is [sbyte] -or
            $rawValue -is [int16] -or $rawValue -is [uint16] -or
            $rawValue -is [int32] -or $rawValue -is [uint32] -or
            $rawValue -is [int64] -or $rawValue -is [uint64] -or
            $rawValue -is [single] -or $rawValue -is [double] -or $rawValue -is [decimal]
        if (-not $isNumeric) { throw 'Invalid CPU value type.' }
        if ($rawValue -lt 0 -or $rawValue -gt 100) { throw 'Invalid raw CPU range.' }
        $cpuValue = [double]$rawValue
        if ([double]::IsNaN($cpuValue) -or [double]::IsInfinity($cpuValue) -or
            $cpuValue -lt 0 -or $cpuValue -gt 100) { throw 'Invalid CPU value.' }
        $values[$entry.Name] = $cpuValue
    }
    if ($values.Count -ne 3) { throw 'Incomplete CPU sample.' }
    $samples += @{utc=[DateTime]::UtcNow.ToString('o');aggregate=$values['_Total'];
        cpu2=$values['2'];cpu6=$values['6']}
    Start-Sleep -Seconds 1
}
$after = @(Get-CimInstance Win32_Process | Where-Object {
    $names -contains $_.Name -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool')
})
$gamesAfter = @(Get-CimInstance Win32_Process | Where-Object {
    $_.ExecutablePath -like '*steamapps\common*' -or $_.ExecutablePath -like '*EA Games*' -or
    $_.ExecutablePath -like '*Origin Games*'
})
$cpuOver = @($samples | Where-Object {
    $_.aggregate -ge 80 -or $_.cpu2 -ge 80 -or $_.cpu6 -ge 80
}).Count -gt 0
$eligible = $head -eq $expectedHead -and $tree -eq $expectedTree -and
    $source -eq $expectedSource -and $clean -and -not $cpuOver -and
    $before.Count -eq 0 -and $after.Count -eq 0 -and
    $gamesBefore.Count -eq 0 -and $gamesAfter.Count -eq 0 -and
    $memoryKiB -ge 8GB/1KB -and $dFree -ge 20GB
$report = [ordered]@{
    utc=[DateTime]::UtcNow.ToString('o');eligible=$eligible
    head=$head;tree=$tree;sourceTree=$source;clean=$clean
    samples=$samples;cpuThresholdPercent=80;mask='0x44';priority='Normal'
    conflictsBefore=$before.Count;conflictsAfter=$after.Count
    gamesBefore=$gamesBefore.Count;gamesAfter=$gamesAfter.Count
    freeMemoryKiB=$memoryKiB;freeDBytes=$dFree
}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $output -Encoding utf8
$report | ConvertTo-Json -Depth 5

