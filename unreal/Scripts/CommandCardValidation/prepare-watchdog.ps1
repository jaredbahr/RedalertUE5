# Capability preparation only, before functional dispatch and after capacity admission.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory,[Parameter(Mandatory)][string]$SourceHead,[Parameter(Mandatory)][string]$SupervisorSourceSHA256,[switch]$CapacityAdmitted)
$ErrorActionPreference='Stop'
if(-not $CapacityAdmitted){throw 'Bounded capability preparation requires current capacity admission.'}
if($PSVersionTable.PSVersion.Major -lt 7 -or ('RA2CommandCardStageSupervisor' -as [type])){throw 'Fresh dedicated PowerShell 7 preparation process required.'}
if($SourceHead -cnotmatch '^[a-f0-9]{40}$' -or $SupervisorSourceSHA256 -cnotmatch '^[a-f0-9]{64}$'){throw 'Exact reviewed source identities required.'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh capability output directory required.'}
$source=Join-Path $PSScriptRoot 'CommandCardStageSupervisor.cs'
$sourceBytes=[IO.File]::ReadAllBytes($source)
if($sourceBytes.Length -gt 262144 -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($sourceBytes)).ToLowerInvariant() -cne $SupervisorSourceSHA256){throw 'Supervisor source pin mismatch.'}
[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
$assembly=Join-Path $OutputDirectory 'CommandCardStageSupervisor.dll'
Add-Type -TypeDefinition ([Text.UTF8Encoding]::new($false,$true).GetString($sourceBytes)) -OutputAssembly $assembly
Add-Type -Path $assembly
$loaded=[RA2CommandCardStageSupervisor].Assembly
if($loaded.Location -ine [IO.Path]::GetFullPath($assembly)){throw 'Prepared assembly identity mismatch.'}
$receipt=[ordered]@{head=$SourceHead;supervisorSourceSha256=$SupervisorSourceSHA256;assemblyPath=$loaded.Location;assemblySha256=(Get-FileHash -LiteralPath $assembly -Algorithm SHA256).Hash.ToLowerInvariant();moduleVersionId=$loaded.ManifestModule.ModuleVersionId.ToString();pwshPath=[Environment]::ProcessPath;acceptanceEvidence=$false}
$path=Join-Path $OutputDirectory 'PREPARED_WATCHDOG.json'
[IO.File]::WriteAllText($path,($receipt|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
$capability=[pscustomobject]$receipt
$capability|Add-Member -NotePropertyName receiptPath -NotePropertyValue $path
$capability|Add-Member -NotePropertyName receiptSha256 -NotePropertyValue (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Output $capability
# Keep this parent process for run-stage.ps1; the guarded controller is fresh.
