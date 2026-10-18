param(
    [Parameter(Mandatory)][string]$ExpectedHead,
    [Parameter(Mandatory)][string]$ExpectedSource,
    [Parameter(Mandatory)][string]$ExpectedDll,
    [string]$EngineRoot='D:\Epic Games\UE_5.7'
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'BoxInputEvidence.ps1')
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$project=Join-Path $repo 'unreal/RedalertProof.uproject'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$dll=Join-Path $repo 'unreal/Binaries/Win64/UnrealEditor-RedalertProof.dll'
if([IO.Path]::GetPathRoot($repo) -ne 'D:\'){throw 'D: isolation required'}
if((& git -C $repo rev-parse HEAD).Trim() -cne $ExpectedHead -or (& git -C $repo rev-parse 'HEAD:unreal/Source').Trim() -cne $ExpectedSource -or [bool](& git -C $repo status --porcelain)){throw 'Frozen clean source required'}
if((Get-FileHash -LiteralPath $dll).Hash -cne $ExpectedDll){throw 'Frozen DLL required'}
$run=Join-Path $repo ('generated/box-input-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
$cache=Join-Path $run 'cache';$temp=Join-Path $run 'temp';$images=Join-Path $run 'images'
New-Item -ItemType Directory -Force -Path $run,$cache,$temp,$images|Out-Null
$log=Join-Path $run 'fixture.log'
$mask=[int64]68;$invoker=[Diagnostics.Process]::GetCurrentProcess();$priorAffinity=$invoker.ProcessorAffinity
if(($priorAffinity.ToInt64() -band $mask) -ne $mask){throw 'CPUs2/6 unavailable'}
function Get-Competing {
    @(Get-CimInstance Win32_Process -ErrorAction Stop|Where-Object {$_.Name -match '^(UnrealEditor(?:-Cmd)?|UnrealBuildTool|UnrealHeaderTool|ShaderCompileWorker|cl|link|clang(?:\+\+)?|lld-link|MSBuild|BuildGraph|AutomationTool|UbaAgent|UbaHost|UbaSessionServer)\.exe$' -or ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool|UnrealHeaderTool|AutomationTool')}|Select-Object ProcessId,Name,CreationDate)
}
$counterPaths=@('\Processor(_Total)\% Processor Time','\Processor(2)\% Processor Time','\Processor(6)\% Processor Time')
$samples=@(Get-Counter $counterPaths -SampleInterval 1 -MaxSamples 3 -ErrorAction Stop|ForEach-Object {@{utc=$_.Timestamp.ToUniversalTime().ToString('o');counters=@($_.CounterSamples|ForEach-Object {@{path=$_.Path;value=$_.CookedValue;status=$_.Status}})}})
$samples|ConvertTo-Json -Depth 7|Set-Content (Join-Path $run 'admission-samples.json') -Encoding utf8
if($samples.Count -ne 3){throw 'Three fresh samples required'}
foreach($sample in $samples){foreach($path in $counterPaths){$matches=@($sample.counters|Where-Object {$_.path.EndsWith($path,[StringComparison]::OrdinalIgnoreCase)});if($matches.Count -ne 1 -or $matches[0].status -ne 0 -or -not [double]::IsFinite($matches[0].value) -or $matches[0].value -lt 0 -or $matches[0].value -gt 80){throw 'CPU admission blocked'}}}
if(@(Get-Competing).Count){throw 'Competing engine/build work blocks allocation'}
$result=[ordered]@{passed=$false;head=$ExpectedHead;sourceTree=$ExpectedSource;dllSHA256=$ExpectedDll;startedUtc=[DateTime]::UtcNow.ToString('o');finishedUtc=$null;runDirectory=$run;error=$null;scenarioDeadlineSeconds=220;totalDeadlineSeconds=240;fps=30;affinityMask=$mask;priority='Normal';physicalInputVerified=$false;realPointerRouteVerified=$false;osFocusCaptureVerified=$false;lostReleaseOccurrenceVerified=$false;population='diagnostic-over-cap';gameplayProof=$false;pollingIsNotContinuousConfinement=$true;policyRepairs=@();cleanupErrors=@();screenshots=@();logSHA256=$null;oracle=$null;owned=@();ownedExits=@();ownedExitConfirmed=$false;remainingCompetingWork=@();storageChecks=0;peakCacheBytes=[int64]0;peakTempBytes=[int64]0}
$prior=@{};$owned=@{};$process=$null;$watch=[Diagnostics.Stopwatch]::StartNew()

try {
    foreach($name in @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')){$prior[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$cache,'Process');[Environment]::SetEnvironmentVariable('UE-SharedDataCachePath','None','Process')
    [Environment]::SetEnvironmentVariable('TEMP',$temp,'Process');[Environment]::SetEnvironmentVariable('TMP',$temp,'Process')
    $invoker.ProcessorAffinity=[IntPtr]$mask
    if(@(Get-Competing).Count){throw 'Competing work appeared before launch'}
    $arguments=@(('"'+$project+'"'),'/Game/CommandProofGenerated/Maps/CommandProof?game=/Script/RedalertProof.ProofBoxInputGameMode','-game','-windowed','-ResX=1280','-ResY=720','-unattended','-nosound','-nosplash','-NoSourceControl','-NoP4','-CoreLimit=2','-ddc=BlockoutDDC','-ProofBoxInputFixture',('-ProofBoxInputOutput="'+$images+'"'),'-ini:GameUserSettings:[/Script/Engine.GameUserSettings]:FrameRateLimit=30','-ini:Engine:[DevOptions.Shaders]:WorkerProcessPriority=0,[ConsoleVariables]:t.MaxFPS=30,[ShaderCompiler]:r.Shaders.Optimize=1',[string]('-abslog="'+$log+'"'))
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory (Join-Path $repo 'unreal') -WindowStyle Hidden -PassThru
    $owned[$process.Id]=@{id=$process.Id;start=$process.StartTime;path=$process.Path;depth=0;process=$process}
    while($true){
        $all=@(Get-CimInstance Win32_Process -ErrorAction Stop)
        for($depth=0;$depth -lt 8;$depth++){foreach($child in $all){
            if($owned.ContainsKey([int]$child.ProcessId) -or -not $owned.ContainsKey([int]$child.ParentProcessId)){continue}
            $parent=$owned[[int]$child.ParentProcessId];$liveParent=Get-Process -Id $parent.id -ErrorAction SilentlyContinue
            if(-not $liveParent -or $liveParent.HasExited -or $liveParent.StartTime -ne $parent.start -or $liveParent.Path -cne $parent.path -or $child.CreationDate -lt $parent.start){continue}
            $live=Get-Process -Id $child.ProcessId -ErrorAction SilentlyContinue
            if(-not $live -or $live.HasExited -or -not $live.Path -or [math]::Abs(($live.StartTime-$child.CreationDate).TotalSeconds) -gt 1){continue}
            $owned[[int]$child.ProcessId]=@{id=[int]$child.ProcessId;start=$live.StartTime;path=$live.Path;depth=([int]$parent.depth+1);parent=$parent.id;process=$live}
        }}
        foreach($record in @($owned.Values)){
            $live=Get-Process -Id $record.id -ErrorAction SilentlyContinue;if(-not $live -or $live.HasExited){continue}
            if($live.StartTime -ne $record.start -or $live.Path -cne $record.path){throw 'Owned process identity changed'}
            if($live.ProcessorAffinity.ToInt64() -ne $mask -or [string]$live.PriorityClass -ne 'Normal'){
                $result.policyRepairs+=@{name=$live.ProcessName;beforeMask=$live.ProcessorAffinity.ToInt64();beforePriority=[string]$live.PriorityClass}
                $live.ProcessorAffinity=[IntPtr]$mask;$live.PriorityClass='Normal'
            }
        }
        foreach($pair in @(@{path=$cache;limit=2GB;field='peakCacheBytes'},@{path=$temp;limit=1GB;field='peakTempBytes'})){
            $bytes=[int64]0;foreach($file in @(Get-ChildItem -LiteralPath $pair.path -File -Force -Recurse -ErrorAction Stop)){$bytes+=$file.Length}
            $result[$pair.field]=[math]::Max([int64]$result[$pair.field],$bytes);if($bytes -gt $pair.limit){throw 'Storage guard exceeded'}
        }
        $result.storageChecks++;$process.Refresh()
        if($process.HasExited){if($watch.Elapsed.TotalSeconds -ge 240){throw 'Completion first observed outside240-second bound'};break}
        if($watch.Elapsed.TotalSeconds -ge 240){throw 'Original240-second total deadline exceeded'}
        Start-Sleep -Milliseconds 500
    }
    if($process.ExitCode -ne 0){throw ('Fixture exit '+$process.ExitCode)}
    $text=Get-Content -LiteralPath $log -Raw;$result.oracle=Check-Oracle $text $images
    foreach($case in @(2,3)){
        $png=Join-Path $images ('box-held-'+$case+'.png');$file=Get-Item -LiteralPath $png
        if($file.Length -lt 45 -or $file.Length -gt 10MB -or $file.LastWriteTimeUtc -lt [DateTime]::Parse($result.startedUtc).ToUniversalTime()){throw 'Missing/stale/oversized outline image'}
        $decoded=Test-BoxInputPng $png
        $result.screenshots+=@{case=$case;path=$png;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $png).Hash;decoded=$decoded}
    }
    $result.passed=$true
} catch {$result.error=$_.Exception.Message}
finally {
    foreach($record in @($owned.Values|Sort-Object depth -Descending)){
        try{
            $pinned=$record.process;$pinned.Refresh()
            if(-not $pinned.HasExited){
                $live=Get-Process -Id $record.id -ErrorAction Stop
                if($live.StartTime -ne $record.start -or $live.Path -cne $record.path){throw 'Cleanup identity changed'}
                $pinned.Kill()
            }
            if(-not $pinned.WaitForExit(5000)){throw 'Original owned process exit not signaled'}
            $result.ownedExits+=@{id=$record.id;start=$record.start;path=$record.path;exitUtc=$pinned.ExitTime.ToUniversalTime().ToString('o');exitCode=$pinned.ExitCode;originalProcessExitSignaled=$true}
        }catch{$result.cleanupErrors+=@($_.Exception.Message);$result.passed=$false}
    }
    $result.owned=@(foreach($record in $owned.Values){@{id=$record.id;start=$record.start;path=$record.path;depth=$record.depth;parent=$record.parent}})
    $result.ownedExitConfirmed=($result.owned.Count -gt 0 -and $result.owned.Count -eq $result.ownedExits.Count -and $result.cleanupErrors.Count -eq 0)
    try{$result.remainingCompetingWork=@(Get-Competing)}catch{$result.cleanupErrors+=@($_.Exception.Message);$result.passed=$false}
    $invoker.ProcessorAffinity=$priorAffinity
    foreach($name in $prior.Keys){[Environment]::SetEnvironmentVariable($name,$prior[$name],'Process')}
    if(Test-Path -LiteralPath $log){$result.logSHA256=(Get-FileHash -LiteralPath $log).Hash}
    $result.finishedUtc=[DateTime]::UtcNow.ToString('o');$result.elapsedSeconds=$watch.Elapsed.TotalSeconds
    if($result.elapsedSeconds -ge 240){$result.passed=$false;if(-not $result.error){$result.error='Acceptance/cleanup exceeded original240-second total bound'}}
    $result|ConvertTo-Json -Depth 12|Set-Content (Join-Path $run 'result.json') -Encoding utf8
}
if(-not $result.passed){throw ($result.error+' See '+$run+'\result.json')}
Write-Output (Join-Path $run 'result.json')
