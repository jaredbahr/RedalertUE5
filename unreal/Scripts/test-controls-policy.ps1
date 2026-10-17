# Pure helper tests: no UE launch, real process mutation, CIM query or deletion.
$ErrorActionPreference='Stop'
$tokens=$null; $errors=$null
$runner=Join-Path $PSScriptRoot 'test-controls-proof.ps1'
$ast=[System.Management.Automation.Language.Parser]::ParseFile($runner,[ref]$tokens,[ref]$errors)
if($errors.Count) { throw 'Runner parse failed.' }
foreach($name in @('Get-OwnedLiveProcess','Apply-OwnedCpuPolicy','Maintain-OwnedCpuPolicy','Stop-OwnedEntry','Stop-OwnedProcesses','Get-PakCacheEvidence')) {
    $definition=$ast.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
    if(-not $definition) { throw "Missing helper $name" }
    . ([scriptblock]::Create($definition.Extent.Text))
}
$fixtureAffinity=[uint64]3
$repoRoot='D:\Mock\RedalertProof'
$workerExe='D:\Mock\Engine\Binaries\Win64\ShaderCompileWorker.exe'
$watch=[Diagnostics.Stopwatch]::StartNew()
$start=[DateTime]::UtcNow.AddSeconds(-20)
function New-MockProcess([int]$ProcessId,[string]$Path,[DateTime]$Started) {
    $mock=[pscustomobject]@{Id=$ProcessId;Path=$Path;StartTime=$Started;HasExited=$false;ProcessorAffinity=[IntPtr]255;PriorityClass=[Diagnostics.ProcessPriorityClass]::BelowNormal}
    $mock | Add-Member -MemberType ScriptMethod -Name Refresh -Value {}
    return $mock
}
$parent=New-MockProcess 100 'D:\Mock\Engine\Binaries\Win64\UnrealEditor.exe' $start
$child=New-MockProcess 101 $workerExe $start.AddSeconds(1)
$foreign=New-MockProcess 102 $workerExe $start.AddSeconds(2)
$mockProcesses=@{100=$parent;101=$child;102=$foreign}
$mockMetadata=@(
    [pscustomobject]@{ProcessId=101;ParentProcessId=100;CreationDate=$child.StartTime;CommandLine='ShaderCompileWorker D:\Mock\RedalertProof\generated\temp'},
    [pscustomobject]@{ProcessId=102;ParentProcessId=999;CreationDate=$foreign.StartTime;CommandLine='ShaderCompileWorker D:\OtherProject\temp'}
)
# Local mocks shadow OS readers only inside this test process.
function Get-Process { [CmdletBinding()]param([int]$Id) return $mockProcesses[$Id] }
function Get-CimInstance { [CmdletBinding()]param([string]$ClassName,[string]$Filter,[int]$OperationTimeoutSec) return $mockMetadata }
$processes=[Collections.Generic.List[object]]::new()
$processes.Add([pscustomobject]@{Process=$parent;Started=$start;Role='warmup';Path=$parent.Path})
$ownedWorkers=[Collections.Generic.List[object]]::new()
$result=@{cpuPolicy=@{lastPollElapsedSeconds=-1;pollCount=0;events=@();eventsTruncated=$false;discoveryError=$null;processes=@()}}
Maintain-OwnedCpuPolicy -Force
if($parent.ProcessorAffinity.ToInt64() -ne 3 -or $child.ProcessorAffinity.ToInt64() -ne 3 -or $child.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::Normal) { throw 'Owned policy was not applied.' }
if($foreign.ProcessorAffinity.ToInt64() -ne 255 -or $foreign.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::BelowNormal -or $ownedWorkers.Count -ne 1) { throw 'Foreign worker was touched/claimed.' }
$parent.ProcessorAffinity=[IntPtr]255; $child.ProcessorAffinity=[IntPtr]255
Maintain-OwnedCpuPolicy -Force
if($parent.ProcessorAffinity.ToInt64() -ne 3 -or $child.ProcessorAffinity.ToInt64() -ne 3 -or $result.cpuPolicy.events.Count -ne 4) { throw 'Affinity reset was not repaired/recorded.' }
$replacement=New-MockProcess 100 $parent.Path $start.AddSeconds(15)
$mockProcesses[100]=$replacement
Maintain-OwnedCpuPolicy -Force
if($replacement.ProcessorAffinity.ToInt64() -ne 255 -or $replacement.PriorityClass -ne [Diagnostics.ProcessPriorityClass]::BelowNormal) { throw 'Reused PID was mutated.' }
# Metadata access failure must fail closed, preserving an explicit error.
$mockProcesses[100]=$parent
function Get-CimInstance { [CmdletBinding()]param([string]$ClassName,[string]$Filter,[int]$OperationTimeoutSec) throw 'Mock metadata unavailable' }
$failedClosed=$false
try { Maintain-OwnedCpuPolicy -Force } catch { $failedClosed=$_.Exception.Message -like 'Cannot verify shader-worker ownership*' }
if(-not $failedClosed -or $result.cpuPolicy.discoveryError -ne 'Mock metadata unavailable') { throw 'Metadata failure did not fail closed.' }
$result.cleanup=@{errors=@();ignoredExitRaces=0}; $result.passed=$true; $result.error=$null
$stopCalls=[Collections.Generic.List[int]]::new()
function Stop-Process { [CmdletBinding()]param([int]$Id,[switch]$Force) $stopCalls.Add($Id); if($Id -eq 100){throw 'Mock denied stop'}; $mockProcesses.Remove($Id) }
$restorationRan=$false; $resultWriteRan=$false
try { Stop-OwnedProcesses } finally { try { $restorationRan=$true } finally { $resultWriteRan=$true } }
if(($stopCalls -join ',') -ne '100,101' -or $result.cleanup.errors.Count -ne 1 -or $result.passed -or -not $restorationRan -or -not $resultWriteRan) { throw 'Cleanup failure blocked remaining cleanup/restoration/evidence.' }
# Exit after identity check but before stop is harmless and must be distinguished.
$mockProcesses[101]=$child
function Stop-Process { [CmdletBinding()]param([int]$Id,[switch]$Force) $mockProcesses.Remove($Id); throw 'Mock already exited' }
Stop-OwnedEntry $ownedWorkers[0]
if($result.cleanup.ignoredExitRaces -ne 1 -or $result.cleanup.errors.Count -ne 1) { throw 'Normal cleanup exit race was misreported.' }
$cacheSample=@'
LogDerivedDataCache: Verbose: ../../../Engine/DerivedDataCache/Compressed.ddp: Cache hit for GlobalShaderMap/key from 'GlobalShaderMap Fixture'
LogDerivedDataCache: Verbose: D:/UE/Engine/DerivedDataCache/Compressed.ddp: Cache hit for MaterialShaderMap/key from 'Material Fixture'
LogDerivedDataCache: Verbose: D:/UE/Engine/DerivedDataCache/DDC.ddp: Cache hit for Other/key from 'Texture Fixture'
LogDerivedDataCache: Verbose: D:/OtherProject/Cache.ddp: Cache hit for GlobalShaderMap/key from 'Foreign Fixture'
LogDerivedDataCache: Verbose: D:/UE/Engine/DerivedDataCache/Compressed.ddp: Cache hit for LegacyTEXTURE/key from '/Engine/EngineMaterials/Texture Fixture'
LogDerivedDataCache: D:/UE/Engine/DerivedDataCache/Compressed.ddp opened for reading
'@
$cacheEvidence=Get-PakCacheEvidence $cacheSample
if($cacheEvidence.allDdpHitCount -ne 5 -or $cacheEvidence.enginePakHitCount -ne 4 -or $cacheEvidence.globalShaderHitCount -ne 1 -or $cacheEvidence.materialRelatedHitCount -ne 2 -or $cacheEvidence.otherEnginePakHitCount -ne 1) { throw 'Pak hit diagnostics counted opens/foreign paths or misclassified examples.' }
[pscustomobject]@{passed=$true;tests=@('owned_parent_and_child_policy','foreign_worker_untouched','affinity_reset_repaired_and_recorded','reused_pid_untouched','metadata_failure_closed','cleanup_failure_continues_parent_first_and_restores','cleanup_exit_race_ignored','actual_pak_hit_diagnostics_exclude_open_and_foreign_path');realProcessesMutated=$false;ueLaunched=$false} | ConvertTo-Json
