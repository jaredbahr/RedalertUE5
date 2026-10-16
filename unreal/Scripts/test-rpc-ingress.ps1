param([string]$EngineRoot='D:\Epic Games\UE_5.7',[ValidateRange(20000,60000)][int]$Port=28979,[ValidateRange(30,90)][int]$TimeoutSeconds=90)
$ErrorActionPreference='Stop'
$projectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot=(Resolve-Path (Join-Path $projectRoot '..')).Path
$project=Join-Path $projectRoot 'RedalertProof.uproject'
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\'){throw 'Use a D: checkout.'}
if(-not(Test-Path $editor) -or -not(Test-Path (Join-Path $projectRoot 'Content\CommandProofGenerated\Maps\CommandProof.umap'))){throw 'Installed engine and existing generated map required.'}
$probe=[Net.Sockets.UdpClient]::new()
try{$probe.Client.ExclusiveAddressUse=$true;$probe.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Loopback,$Port))}finally{$probe.Dispose()}
$run=Join-Path $repoRoot ('generated\rpc-ingress-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$cache=Join-Path $repoRoot 'generated\rpc-cache'
$temp=Join-Path $run 'temp'
New-Item -ItemType Directory -Force -Path $run,$cache,$temp | Out-Null
$release=Join-Path $run 'healthy-release'
$cvars=Join-Path $run 'literal-client.ini'
"[Startup]`nnet.IpConnectionDisableResolution=1`n" | Set-Content $cvars -Encoding ascii
$watch=[Diagnostics.Stopwatch]::StartNew()
$mask=[int64]0;$count=0;$available=[Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64()
for($cpu=0;$cpu -lt 63 -and $count -lt 2;$cpu++){ $bit=[int64]1 -shl $cpu;if($available -band $bit){$mask=$mask -bor $bit;$count++} }
if($count -ne 2){throw 'Need two available CPUs.'}
$result=[ordered]@{passed=$false;head=(& git -C $repoRoot rev-parse HEAD).Trim();dirtyWorkingTree=[bool](& git -C $repoRoot status --porcelain);startedUtc=[DateTime]::UtcNow.ToString('o');elapsedSeconds=0;timeoutSeconds=$TimeoutSeconds;transport='UE UDP loopback';internetTest=$false;performanceTest=$false;runDirectory=$run;admitted=0;limitWarnings=0;healthyOwner=0;floodOwner=0;cpuPolicy=[ordered]@{mask=$mask;priority='Normal';polls=0;repairs=0;pollingIsNotHardConfinement=$true;processes=@()};storage=[ordered]@{softPolling=$true;peakCacheBytes=[int64]0;peakTempBytes=[int64]0};cleanupErrors=@();error=$null}
$entries=[Collections.Generic.List[object]]::new();$prior=@{};$lastStorage=-10.0
function Get-Owned($entry){$p=Get-Process -Id $entry.Process.Id -ErrorAction SilentlyContinue;if($p -and $p.StartTime -eq $entry.Started -and $p.Path -eq $editor){return $p};return $null}
function Maintain {
    foreach($entry in $entries){
        $p=Get-Owned $entry;if(-not $p){continue}
        try{
            if($p.ProcessorAffinity.ToInt64() -ne $mask -or $p.PriorityClass -ne 'Normal'){$result.cpuPolicy.repairs++}
            $p.ProcessorAffinity=[IntPtr]$mask;$p.PriorityClass='Normal';$p.Refresh()
            if($p.ProcessorAffinity.ToInt64() -ne $mask -or $p.PriorityClass -ne 'Normal'){throw 'CPU policy verification failed.'}
            $result.cpuPolicy.polls++
            $entry.PeakWS=[math]::Max([int64]$entry.PeakWS,[int64]$p.PeakWorkingSet64);$entry.CPU=$p.CPU
        }catch{if(Get-Owned $entry){throw}}
    }
    if($watch.Elapsed.TotalSeconds - $script:lastStorage -ge 10){
        $script:lastStorage=$watch.Elapsed.TotalSeconds
        $cacheBytes=[int64](Get-ChildItem $cache -Recurse -File | Measure-Object Length -Sum).Sum
        $tempBytes=[int64](Get-ChildItem $temp -Recurse -File | Measure-Object Length -Sum).Sum
        $result.storage.peakCacheBytes=[math]::Max($result.storage.peakCacheBytes,$cacheBytes)
        $result.storage.peakTempBytes=[math]::Max($result.storage.peakTempBytes,$tempBytes)
        if($cacheBytes -gt 2048MB -or $tempBytes -gt 1024MB){throw 'D: cache/temp soft guard exceeded.'}
    }
    if($watch.Elapsed.TotalSeconds -ge $TimeoutSeconds){throw 'Ingress fixture timed out.'}
}
function Start-Role([string]$role,[string]$destination,[string[]]$mode){
    $log=Join-Path $run ($role+'.log')
    $launchArguments=@(('"'+$project+'"'),$destination)+$mode+@('-ProofIngressFixture','-nullrhi','-unattended','-nosound','-nosplash','-NoSourceControl','-NoP4','-ddc=BlockoutDDC','-CoreLimit=2','-ini:Engine:[AssetRegistry]:EditorGameScansAR=Async,[ConsoleVariables]:t.MaxFPS=30',('-port='+$Port),('-abslog="'+$log+'"'))
    if($role -ne 'host'){$launchArguments+=('-cvarsini="'+$cvars+'"')}
    $p=Start-Process $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    $entry=[pscustomobject]@{Process=$p;Started=$p.StartTime;Role=$role;Log=$log;PeakWS=[int64]0;CPU=0.0}
    $entries.Add($entry);Maintain
    return $entry
}
function Read-Log($entry){if(Test-Path $entry.Log){return Get-Content $entry.Log -Raw};return ''}
function Wait-Marker($entry,[string]$pattern){
    do{Maintain;$log=Read-Log $entry;if($log -match $pattern){return};if($entry.Process.HasExited){throw "$($entry.Role) exited before $pattern"};Start-Sleep -Milliseconds 200}while($true)
}
try{
    foreach($name in @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')){$prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$cache,'Process');[Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process');$env:TEMP=$temp;$env:TMP=$temp
    $serverEntry=Start-Role 'host' '/Game/CommandProofGenerated/Maps/CommandProof' @('-server','-MULTIHOME=127.0.0.1')
    Wait-Marker $serverEntry 'PROOF_READY'
    $healthy=Start-Role 'healthy' ('127.0.0.1:'+$Port) @('-game','-ProofIngressHealthy',('-ProofIngressRelease="'+$release+'"'))
    Wait-Marker $healthy 'PROOF_RESULT owner=1 sequence=1 accepted=1 reason=accepted'
    $flood=Start-Role 'flood' ('127.0.0.1:'+$Port) @('-game','-ProofIngressFlood')
    Wait-Marker $serverEntry 'PROOF_INGRESS_CLOSED owner=2 closed=1 admitted=\d+ elapsed=[\d.]+'
    Wait-Marker $flood 'PROOF_INGRESS_CLIENT_CLOSED expected=1 '
    # Only release healthy command 2 AFTER actual server close and client loss evidence.
    [DateTime]::UtcNow.ToString('o') | Set-Content $release
    Wait-Marker $healthy 'PROOF_INGRESS_HEALTHY_COMPLETE owner=1 status=pass moved=1 produced=1 credits=150'
    while(-not $healthy.Process.HasExited -or -not $flood.Process.HasExited){Maintain;Start-Sleep -Milliseconds 200}
    if($healthy.Process.ExitCode -ne 0 -or $flood.Process.ExitCode -ne 0){throw 'Fixture client nonzero exit.'}
    $hostText=Read-Log $serverEntry;$healthyText=Read-Log $healthy;$floodText=Read-Log $flood
    $limits=[regex]::Matches($hostText,'PROOF_INGRESS_LIMIT owner=2 action=close-connection')
    $closed=[regex]::Matches($hostText,'PROOF_INGRESS_CLOSED owner=2 closed=1 admitted=(\d+) elapsed=([\d.]+)')
    if($limits.Count -ne 1 -or $closed.Count -ne 1){throw 'Expected one owning-connection close and one warning.'}
    $admitted=[int]$closed[0].Groups[1].Value;$span=[double]::Parse($closed[0].Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture)
    $receipts=[regex]::Matches($hostText,'PROOF_HOST_RESULT owner=2 sequence=\d+ accepted=0 reason=malformed')
    if($admitted -lt 64 -or $admitted -ge 128 -or $admitted -gt (64+[math]::Floor(32*$span)+1) -or $receipts.Count -ne $admitted){throw 'Ingress work exceeded its measured token budget or receipt count.'}
    $after=$hostText.Substring($closed[0].Index+$closed[0].Length)
    if($after -match 'PROOF_HOST_RESULT owner=2 ' -or $after -notmatch 'PROOF_HOST_RESULT owner=1 sequence=2 accepted=1 reason=accepted'){throw 'Post-close isolation/healthy service failed.'}
    if($healthyText -notmatch 'PROOF_RESULT owner=1 sequence=2 accepted=1 reason=accepted' -or $floodText -notmatch 'PROOF_INGRESS_FLOOD_SENT owner=2 requests=128'){throw 'Missing true RPC send or healthy receipt evidence.'}
    foreach($text in @($healthyText,$floodText)){if($text -notmatch 'Set CVar \[\[net.IpConnectionDisableResolution:1\]\]'){throw 'Missing literal loopback resolver startup setting.'}}
    if($hostText -match 'Reliable buffer overflow|Assertion failed|Fatal error'){throw 'Transport overflow/crash is not ingress-budget proof.'}
    $result.admitted=$admitted;$result.limitWarnings=$limits.Count;$result.healthyOwner=1;$result.floodOwner=2;$result.passed=$true
}catch{$result.error=$_.Exception.Message}
finally{
    foreach($entry in $entries){try{$p=Get-Owned $entry;if($p){Stop-Process -Id $p.Id -Force -ErrorAction Stop}}catch{$result.cleanupErrors+=$_.Exception.Message;$result.passed=$false}}
    foreach($name in $prior.Keys){[Environment]::SetEnvironmentVariable($name,$prior[$name],'Process')}
    $result.cpuPolicy.processes=@($entries | ForEach-Object {@{role=$_.Role;pid=$_.Process.Id;startUtc=$_.Started.ToUniversalTime().ToString('o');peakWorkingSetBytes=$_.PeakWS;lastCpuSeconds=$_.CPU}})
    $result.elapsedSeconds=[math]::Round($watch.Elapsed.TotalSeconds,3);$result.finishedUtc=[DateTime]::UtcNow.ToString('o')
    $result | ConvertTo-Json -Depth 7 | Set-Content (Join-Path $run 'result.json')
}
if(-not $result.passed){throw "$($result.error) See $run\result.json"}
Write-Output (Join-Path $run 'result.json')
