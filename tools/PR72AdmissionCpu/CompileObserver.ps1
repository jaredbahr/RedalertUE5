[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourceReviewSHA256,[Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Probe.Common.ps1')
$consoleTypeReady=$false
try {
Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class PR72AdmissionCompileOwnConsole {
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool FreeConsole();
}
'@
$consoleTypeReady=$true
$bindings=Assert-ProbeSource $SourceReviewSHA256 -OwnedPayload
$expected='D:\UserTemp\PR72-Admission-CPU-Attribution-20261025\capability'
if([IO.Path]::GetFullPath($OutputDirectory) -ine $expected -or (Test-Path -LiteralPath $OutputDirectory)){throw 'Fresh exact diagnostic capability directory required.'}
if('RA2AdmissionCpuSupervisor' -as [type]){throw 'Fresh PowerShell preparation process required.'}
$source=Join-Path $PSScriptRoot 'AdmissionCpuSupervisor.cs'
$raw=[IO.File]::ReadAllBytes($source)
if($raw.Length -gt 262144 -or (Get-ProbeHash $source) -cne $bindings.observer.sha256){throw 'Exact reviewed observer source required.'}
[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
$assembly=Join-Path $OutputDirectory 'AdmissionCpuSupervisor.dll'
Add-Type -TypeDefinition ([Text.UTF8Encoding]::new($false,$true).GetString($raw)) -OutputAssembly $assembly
Add-Type -LiteralPath $assembly
$loaded=[RA2AdmissionCpuSupervisor].Assembly
if($loaded.Location -ine [IO.Path]::GetFullPath($assembly)){throw 'Compiled observer assembly location mismatch.'}
$receipt=[ordered]@{scope='PR72-admission-cpu-capability';className='RA2AdmissionCpuSupervisor';sourceBindingsSha256=Get-ProbeHash 'D:\UserTemp\PR72-Admission-CPU-Attribution-20261025\PROBE_SOURCE_BINDINGS.json';sourceReviewSha256=$SourceReviewSHA256;observerSourceSha256=$bindings.observer.sha256;assemblyPath=$loaded.Location.Replace('\','/');assemblySha256=Get-ProbeHash $assembly;moduleVersionId=$loaded.ManifestModule.ModuleVersionId.ToString();pwshPath=[Environment]::ProcessPath.Replace('\','/');acceptanceEvidence=$false;runtimeAcceptance=$false}
$path=Join-Path $OutputDirectory 'PREPARED_OBSERVER.json'
$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($receipt|ConvertTo-Json))
if($bytes.Length -gt 4096){throw 'Capability receipt cap.'}
$stream=[IO.FileStream]::new($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
$receipt|ConvertTo-Json -Depth 5
} finally {
 if($consoleTypeReady -and -not [PR72AdmissionCompileOwnConsole]::FreeConsole()){throw 'Preparation caller own-console release failed.'}
}
