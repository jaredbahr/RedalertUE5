param(
    [string]$EngineRoot = 'D:\Epic Games\UE_5.7',
    [ValidateRange(20000, 60000)][int]$Port = 28777,
    [ValidateRange(35, 90)][int]$TimeoutSeconds = 60
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$repoRoot = (Resolve-Path (Join-Path $projectRoot '..')).Path
$project = Join-Path $projectRoot 'RedalertProof.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if ([IO.Path]::GetPathRoot($repoRoot) -ne 'D:\') { throw 'Use a D: checkout.' }
if (-not (Test-Path -LiteralPath $editor)) { throw "Missing installed editor: $editor" }
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'Content\CommandProofGenerated\Maps\CommandProof.umap'))) { throw 'Generate CommandProof first.' }
# Probe the actual UDP game port without altering firewall or machine settings.
$probe = [Net.Sockets.UdpClient]::new()
try { $probe.Client.ExclusiveAddressUse = $true; $probe.Client.Bind([Net.IPEndPoint]::new([Net.IPAddress]::Loopback,$Port)) }
finally { $probe.Dispose() }
$run = Join-Path $repoRoot ('generated\native-network-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force -Path $run,(Join-Path $run 'cache'),(Join-Path $run 'temp') | Out-Null
$prior = @{}; $processes = [Collections.Generic.List[object]]::new()
$result = [ordered]@{ head=(& git -C $repoRoot rev-parse HEAD).Trim();dirtyWorkingTree=[bool](& git -C $repoRoot status --porcelain); passed=$false; transport='UE native UDP loopback'; internetTest=$false; performanceTest=$false; runDirectory=$run; port=$Port; commonRevisions=@(); error=$null }
$names = @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')
$mask=[int64]0;$count=0;$available=[Diagnostics.Process]::GetCurrentProcess().ProcessorAffinity.ToInt64()
for($cpu=0;$cpu -lt 63 -and $count -lt 2;$cpu++){ $bit=[int64]1 -shl $cpu;if($available -band $bit){$mask=$mask -bor $bit;$count++} }
if($count -ne 2){throw 'Need two available CPUs.'}
$result.cpuPolicy=@{mask=$mask;priority='Normal';polls=0;repairs=0;pollingIsNotHardConfinement=$true}
function Maintain-OwnedPolicy {
    foreach($entry in $processes){
        $live=Get-Process -Id $entry.Process.Id -ErrorAction SilentlyContinue
        if(-not $live -or $live.StartTime -ne $entry.Started -or $live.Path -ne $entry.Path){continue}
        try{
            if($live.ProcessorAffinity.ToInt64() -ne $mask -or $live.PriorityClass -ne 'Normal'){$result.cpuPolicy.repairs++}
            $live.ProcessorAffinity=[IntPtr]$mask;$live.PriorityClass='Normal';$live.Refresh();$result.cpuPolicy.polls++
            if($live.ProcessorAffinity.ToInt64() -ne $mask -or $live.PriorityClass -ne 'Normal'){throw 'Native network CPU policy failed.'}
        }catch{if(-not $entry.Process.HasExited){throw}}
    }
}
$cvars=Join-Path $run 'literal-client.ini'
"[Startup]`nnet.IpConnectionDisableResolution=1`n" | Set-Content $cvars -Encoding ascii
try {
    foreach ($n in $names) { $prior[$n]=[Environment]::GetEnvironmentVariable($n,'Process') }
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $run 'cache'),'Process')
    [Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',(Join-Path $run 'temp'),'Process')
    [Environment]::SetEnvironmentVariable('TMP',(Join-Path $run 'temp'),'Process')
    function Start-Proof([string]$Role,[string]$Destination,[string[]]$Mode) {
        $log = Join-Path $run ($Role+'.log')
        $launchArguments = @(('"'+$project+'"'),$Destination) + $Mode + @('-ProofAutomation','-unattended','-nullrhi','-nosound','-nosplash','-NoSourceControl','-NoP4','-ddc=BlockoutDDC','-CoreLimit=2','-ini:Engine:[AssetRegistry]:EditorGameScansAR=Async,[ConsoleVariables]:t.MaxFPS=30',('-cvarsini="'+$cvars+'"'),('-port='+$Port),('-abslog="'+$log+'"'))
        $p=Start-Process -FilePath $editor -ArgumentList $launchArguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
        $processes.Add([pscustomobject]@{Process=$p; Started=$p.StartTime; Role=$Role; Log=$log; Path=$editor})
        Maintain-OwnedPolicy
    }
    Start-Proof 'host' '/Game/CommandProofGenerated/Maps/CommandProof' @('-server','-MULTIHOME=127.0.0.1')
    $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    # Readiness marker rather than a fixed startup delay.
    do {
        if ($processes[0].Process.HasExited) { throw 'Server exited before readiness.' }
        $hostText = if(Test-Path $processes[0].Log){Get-Content $processes[0].Log -Raw}else{''}
        if ($hostText -match 'PROOF_READY') { break }
        if ([DateTime]::UtcNow -ge $deadline) { throw 'Server readiness timed out.' }
        Maintain-OwnedPolicy
        Start-Sleep -Milliseconds 200
    } while ($true)
    Start-Proof 'client1' ('127.0.0.1:'+$Port) @('-game')
    Start-Proof 'client2' ('127.0.0.1:'+$Port) @('-game')
    while (@($processes | Where-Object {-not $_.Process.HasExited}).Count) {
        if ([DateTime]::UtcNow -ge $deadline) { throw 'Native proof timed out.' }
        Maintain-OwnedPolicy
        Start-Sleep -Milliseconds 250
    }
    $snapshots=@(); $owners=@()
    foreach ($entry in $processes) {
        if ($entry.Process.ExitCode -ne 0) { throw "$($entry.Role) exited $($entry.Process.ExitCode)." }
        $log=Get-Content -LiteralPath $entry.Log -Raw
        if ($log -match 'PROOF_AUTOMATION_COMPLETE[^\r\n]*status=fail') { throw "$($entry.Role) reported failure." }
        $complete=[regex]::Match($log,'PROOF_AUTOMATION_COMPLETE role=(host|client) owner=(\d+) status=pass[^\r\n]*')
        if (-not $complete.Success) { throw "$($entry.Role) missing successful completion." }
        foreach ($flag in @('moved=1','damaged=1','produced=1')) { if (-not $complete.Value.Contains($flag)) { throw "$($entry.Role) missing $flag." } }
        if ($entry.Role -eq 'host') {
            foreach ($field in @('humanAccepted','rejectedOwnership','botAccepted')) { if ($complete.Value -notmatch ($field+'=([1-9]\d*)')) { throw "Host missing positive $field." } }
        } else {
            $owner=[int]$complete.Groups[2].Value; $owners+=$owner
            if (-not $complete.Value.Contains('ack=1')) { throw 'Client missing acknowledgements.' }
            foreach ($seq in 1..4) {
                $accepted=if($seq -eq 4){0}else{1}
                if ($log -notmatch "PROOF_RESULT owner=$owner sequence=$seq accepted=$accepted reason=\S+") { throw "Missing expected client $owner sequence $seq result." }
            }
        }
        $map=@{}
        foreach ($m in [regex]::Matches($log,'PROOF_SNAPSHOT role=(host|client) owner=\d+(?: epoch=\d+)? revision=(\d+) digest=([A-Za-z0-9]+)')) {
            $rev=$m.Groups[2].Value; $digest=$m.Groups[3].Value
            if ($map.ContainsKey($rev) -and $map[$rev] -ne $digest) { throw "Inconsistent revision $rev within $($entry.Role)." }
            $map[$rev]=$digest
        }
        $snapshots+=,$map
    }
    if (($owners | Sort-Object -Unique).Count -ne 2 -or @($owners | Where-Object {$_ -notin 1,2}).Count) { throw 'Expected distinct native player owners 1 and 2.' }
    $common=@($snapshots[0].Keys | Where-Object {$snapshots[1].ContainsKey($_) -and $snapshots[2].ContainsKey($_)} | Sort-Object {[int]$_})
    if ($common.Count -lt 3) { throw 'Need at least three shared snapshot revisions.' }
    foreach ($rev in $common) { if ($snapshots[0][$rev] -ne $snapshots[1][$rev] -or $snapshots[0][$rev] -ne $snapshots[2][$rev]) { throw "Replica digest mismatch at revision $rev." } }
    $result.commonRevisions=$common; $result.passed=$true
} catch { $result.error=$_.Exception.Message }
finally {
    # Only processes started by this run; refuse PID reuse or a changed executable.
    foreach ($entry in $processes) {
        if (-not $entry.Process.HasExited) {
            $live=Get-Process -Id $entry.Process.Id -ErrorAction SilentlyContinue
            if ($live -and $live.StartTime -eq $entry.Started -and $live.Path -eq $entry.Path) {
                $identity=Get-CimInstance Win32_Process -Filter ('ProcessId='+$entry.Process.Id) -ErrorAction SilentlyContinue
                if ($identity -and $identity.CommandLine.Contains($project)) { Stop-Process -Id $entry.Process.Id -Force }
            }
        }
    }
    foreach ($n in $names) { if ($prior.ContainsKey($n)) { [Environment]::SetEnvironmentVariable($n,$prior[$n],'Process') } }
    $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding utf8
}
if (-not $result.passed) { throw "$($result.error) See $run\result.json" }
Write-Output (Join-Path $run 'result.json')
