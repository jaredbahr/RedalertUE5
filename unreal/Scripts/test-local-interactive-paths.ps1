# Local filesystem/policy tests only. The fake editor is inert and never executed.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'local-interactive-paths.ps1')
. (Join-Path $PSScriptRoot 'interactive-viewport.ps1')
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$fixture=Join-Path $repo ('generated/tool-root-policy-'+[Guid]::NewGuid().ToString('N'))
$checks=0;$junctions=New-Object Collections.Generic.List[string]
$priorEngine=$env:UE_ENGINE_ROOT;$priorStorage=$env:RA2_STORAGE_ROOT
function Check([bool]$Condition,[string]$Name){if(-not $Condition){throw "Policy check failed: $Name"};$script:checks++}
function Reject([string]$Name,[scriptblock]$Action){$rejected=$false;try{&$Action|Out-Null}catch{$rejected=$true};Check $rejected $Name}
function Make-Directory([string]$Path){[IO.Directory]::CreateDirectory($Path)|Out-Null;return $Path}
function Make-Engine([string]$Path){$bin=Make-Directory (Join-Path $Path 'Engine/Binaries/Win64');[IO.File]::WriteAllBytes((Join-Path $bin 'UnrealEditor.exe'),[Text.Encoding]::UTF8.GetBytes('INERT POLICY FIXTURE - DO NOT EXECUTE'));return $Path}
function Make-Repo([string]$Path){Make-Directory (Join-Path $Path 'unreal')|Out-Null;return $Path}
function Make-Junction([string]$Path,[string]$Target){New-Item -ItemType Junction -Path $Path -Target $Target -ErrorAction Stop|Out-Null;$script:junctions.Add($Path)}
function Remove-Junction([string]$Path){
    $full=[IO.Path]::GetFullPath($Path)
    if(-not $full.StartsWith(([IO.Path]::GetFullPath($fixture)+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Refusing junction cleanup outside fixture'}
    if(-not([IO.File]::GetAttributes($full) -band [IO.FileAttributes]::ReparsePoint)){throw 'Expected fixture junction'}
    # Nonrecursive RemoveDirectory unlinks even a dangling junction; PS5 Remove-Item prompts here.
    [IO.Directory]::Delete($full)
    $null=$script:junctions.Remove($Path)
}
try{
    foreach($preset in @(@('1280x720',1280,720),@('960x540',960,540))){
        $view=Get-ProofInteractiveViewport -Viewport $preset[0]
        Check ($view.Name -ceq $preset[0] -and $view.Width -eq $preset[1] -and $view.Height -eq $preset[2]) 'viewport dimensions match supported preset'
        Check ($view.ResX -ceq ('-ResX='+$preset[1]) -and $view.ResY -ceq ('-ResY='+$preset[2])) 'viewport yields exactly two numeric engine arguments'
    }
    Check ((Get-ProofInteractiveViewport).Name -ceq '1280x720') 'default viewport stays 1280x720'
    Check ((Get-ProofInteractiveViewport -Viewport '960X540').Name -ceq '960x540') 'viewport casing normalized'
    foreach($invalid in @('',' ','1920x1080','960x540 -ExecCmds=Quit','1280x720;Quit')){
        Reject 'unsupported viewport rejected' {Get-ProofInteractiveViewport -Viewport $invalid}
        foreach($launcher in @('start-local-interactive.ps1','start-small-match.ps1')){
            $script:forbidden=0;$failed=$false
            try { & {
                function Resolve-ProofInteractivePaths {$script:forbidden++;throw 'Unexpected path resolution'}
                function Start-Process {$script:forbidden++;throw 'Unexpected process launch'}
                function Start-Sleep {$script:forbidden++;throw 'Unexpected delay'}
                & (Join-Path $PSScriptRoot $launcher) -Viewport $invalid
            } | Out-Null } catch { $failed=$true }
            Check ($failed -and $script:forbidden -eq 0) 'launcher rejects unsupported viewport before side effects'
        }
    }
    $storage=Make-Directory (Join-Path $fixture 'designated storage')
    $workspace=Make-Repo (Join-Path $storage 'checkout with spaces')
    $engine=Make-Engine (Join-Path $fixture 'installed engine with spaces')
    $systemRoot=[IO.Path]::GetPathRoot([Environment]::GetFolderPath([Environment+SpecialFolder]::Windows))
    $valid=Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $storage
    Check ($valid.RepoRoot -eq $workspace -and $valid.EngineRoot -eq $engine -and $valid.StorageRoot -eq $storage) 'explicit configured roots'
    Check ($valid.Editor -eq (Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe')) 'editor path composition and spaces'
    Check (-not(Test-Path -LiteralPath (Join-Path $workspace 'generated'))) 'resolution does not create output'
    $env:UE_ENGINE_ROOT=$engine;$env:RA2_STORAGE_ROOT=$storage
    Check ((Resolve-ProofInteractivePaths -RepoRoot $workspace).Editor -eq $valid.Editor) 'process environment defaults'
    $env:UE_ENGINE_ROOT='invalid';$env:RA2_STORAGE_ROOT='invalid'
    Check ((Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $storage).Editor -eq $valid.Editor) 'explicit roots override environment'
    Check ((Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot ($storage+[IO.Path]::DirectorySeparatorChar)).RepoRoot -eq $workspace) 'trailing separator'
    Check ((Resolve-ProofInteractivePaths -RepoRoot $workspace.ToUpperInvariant() -EngineRoot $engine -StorageRoot $storage.ToUpperInvariant()).Editor -eq $valid.Editor) 'case-insensitive filesystem containment'
    $dotWorkspace=Join-Path $storage 'unused/../checkout with spaces'
    Make-Directory (Join-Path $storage 'unused')|Out-Null
    Check ((Resolve-ProofInteractivePaths -RepoRoot $dotWorkspace -EngineRoot $engine -StorageRoot $storage).RepoRoot -eq $workspace) 'normalized contained path'
    Reject 'missing environment roots' {Resolve-ProofInteractivePaths -RepoRoot $workspace}
    Reject 'empty engine root' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot '' -StorageRoot $storage}
    Reject 'whitespace storage root' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot ' '}
    Reject 'relative storage root' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot '.'}
    Reject 'relative repository root' {Resolve-ProofInteractivePaths -RepoRoot '.' -EngineRoot $engine -StorageRoot $storage}
    Reject 'UNC engine root rejected before network access' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot '\\invalid-policy-host\share' -StorageRoot $storage}
    $volumeRelative=[IO.Path]::GetPathRoot($storage).TrimEnd([IO.Path]::DirectorySeparatorChar)+'relative'
    Reject 'volume-relative engine root' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $volumeRelative -StorageRoot $storage}
    Reject 'system volume storage' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $systemRoot}
    $outside=Make-Repo (Join-Path $fixture 'outside')
    Reject 'checkout outside designated root' {Resolve-ProofInteractivePaths -RepoRoot $outside -EngineRoot $engine -StorageRoot $storage}
    $sibling=Make-Repo ($storage+'-sibling')
    Reject 'sibling prefix is not containment' {Resolve-ProofInteractivePaths -RepoRoot $sibling -EngineRoot $engine -StorageRoot $storage}
    $inert=Join-Path $fixture 'inert.file';[IO.File]::WriteAllText($inert,'fixture')
    Reject 'engine root is a file' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $inert -StorageRoot $storage}
    Reject 'storage root is a file' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $inert}
    Reject 'repository root is a file' {Resolve-ProofInteractivePaths -RepoRoot $inert -EngineRoot $engine -StorageRoot $storage}
    Reject 'nonexistent engine root' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot (Join-Path $fixture 'missing') -StorageRoot $storage}
    $noEditor=Make-Directory (Join-Path $fixture 'engine without editor')
    Reject 'required editor missing' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $noEditor -StorageRoot $storage}
    $directoryEditor=Make-Directory (Join-Path $fixture 'directory editor')
    Make-Directory (Join-Path $directoryEditor 'Engine/Binaries/Win64/UnrealEditor.exe')|Out-Null
    Reject 'editor must be a file' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $directoryEditor -StorageRoot $storage}
    $noProject=Make-Directory (Join-Path $storage 'missing project')
    Reject 'project directory missing' {Resolve-ProofInteractivePaths -RepoRoot $noProject -EngineRoot $engine -StorageRoot $storage}
    $generated=Join-Path $workspace 'generated'
    [IO.File]::WriteAllText($generated,'not a directory')
    Reject 'generated output is file' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $storage}
    Remove-Item -LiteralPath $generated -Force
    Make-Directory $generated|Out-Null
    $cache=Join-Path $generated 'controls-cache';[IO.File]::WriteAllText($cache,'not a directory')
    Reject 'interactive cache is file' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $storage}
    Remove-Item -LiteralPath $cache -Force
    $storageLink=Join-Path $fixture 'storage link';Make-Junction $storageLink $storage
    Reject 'storage reparse directory' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $storageLink}
    Remove-Junction $storageLink
    $repoLink=Join-Path $storage 'repo link';Make-Junction $repoLink $workspace
    Reject 'repository reparse directory' {Resolve-ProofInteractivePaths -RepoRoot $repoLink -EngineRoot $engine -StorageRoot $storage}
    Remove-Junction $repoLink
    $engineLink=Join-Path $fixture 'engine link';Make-Junction $engineLink $engine
    Reject 'engine root reparse directory' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engineLink -StorageRoot $storage}
    Remove-Junction $engineLink
    $nestedEngine=Make-Directory (Join-Path $fixture 'nested engine');Make-Directory (Join-Path $nestedEngine 'Engine')|Out-Null
    $binaryLink=Join-Path $nestedEngine 'Engine/Binaries';Make-Junction $binaryLink (Join-Path $engine 'Engine/Binaries')
    Reject 'editor FileInfo ancestors include reparse parent' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $nestedEngine -StorageRoot $storage}
    Remove-Junction $binaryLink
    $targetOutput=Make-Directory (Join-Path $fixture 'output target')
    Remove-Item -LiteralPath $generated -Force
    Make-Junction $generated $targetOutput
    Reject 'generated output junction' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $storage}
    Remove-Junction $generated
    Make-Directory $generated|Out-Null;Make-Junction $cache $targetOutput
    Reject 'interactive cache junction' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $storage}
    Remove-Junction $cache
    # Exercise the actual small-match wrapper against an inert parameter-recording sibling.
    $wrapperRoot=Make-Directory (Join-Path $fixture 'wrapper fixture')
    $wrapper=Join-Path $wrapperRoot 'start-small-match.ps1'
    [IO.File]::WriteAllText($wrapper,[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'start-small-match.ps1')))
    $stub=@'
param([string]$EngineRoot,[int]$SessionSeconds,[switch]$Skirmish,[switch]$CameraPan,[switch]$CameraEdgePan,[string]$StorageRoot,[string]$Viewport)
[pscustomobject]@{EngineRoot=$EngineRoot;StorageRoot=$StorageRoot;SessionSeconds=$SessionSeconds;Skirmish=[bool]$Skirmish;CameraPan=[bool]$CameraPan;CameraEdgePan=[bool]$CameraEdgePan;Viewport=$Viewport}
'@
    [IO.File]::WriteAllText((Join-Path $wrapperRoot 'start-local-interactive.ps1'),$stub)
    $forwarded=& $wrapper $engine 900 -StorageRoot $storage -CameraPan -CameraEdgePan
    Check ($forwarded.EngineRoot -eq $engine -and $forwarded.StorageRoot -eq $storage -and $forwarded.SessionSeconds -eq 900 -and $forwarded.Skirmish -and $forwarded.CameraPan -and $forwarded.CameraEdgePan) 'small match positional compatibility and named forwarding'
    Check ($forwarded.Viewport -ceq '1280x720') 'small match preserves default viewport'
    $forwarded=& $wrapper $engine 180 -StorageRoot $storage -Viewport '960x540'
    Check ($forwarded.Viewport -ceq '960x540' -and $forwarded.SessionSeconds -eq 180) 'small match forwards smaller viewport and existing session limit'
    foreach($name in @('start-local-interactive.ps1','start-small-match.ps1')){
        $message=''
        try{& (Join-Path $PSScriptRoot $name) $engine 900 -StorageRoot $systemRoot|Out-Null}catch{$message=$_.Exception.Message}
        Check ($message -ceq 'StorageRoot must be on a non-system volume.') "$name preserves positional engine and session arguments"
    }
    Check (Test-Path -LiteralPath $targetOutput -PathType Container) 'junction checks leave target intact'
    $danglingTarget=Make-Directory (Join-Path $fixture 'removed junction target')
    Remove-Item -LiteralPath $generated -Force
    Make-Junction $generated $danglingTarget
    [IO.Directory]::Delete($danglingTarget)
    Reject 'dangling generated junction' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $storage}
    Remove-Junction $generated
    Make-Directory $generated|Out-Null;Make-Directory $danglingTarget|Out-Null;Make-Junction $cache $danglingTarget
    [IO.Directory]::Delete($danglingTarget)
    Reject 'dangling cache junction' {Resolve-ProofInteractivePaths -RepoRoot $workspace -EngineRoot $engine -StorageRoot $storage}
    Remove-Junction $cache
    # Invalid launcher calls must stop before any output creation, sleep, process launch or environment mutation.
    foreach($name in @('start-local-interactive.ps1','start-small-match.ps1')){
        $script:forbidden=0;$before=@{}
        foreach($key in @('UE-LocalDataCachePath','UE-SharedDataCachePath','TEMP','TMP')){$before[$key]=[Environment]::GetEnvironmentVariable($key,'Process')}
        $failed=$false
        try{
            & {
                function New-Item {$script:forbidden++;throw 'Unexpected output creation'}
                function Start-Process {$script:forbidden++;throw 'Unexpected process launch'}
                function Start-Sleep {$script:forbidden++;throw 'Unexpected delay'}
                & (Join-Path $PSScriptRoot $name) -EngineRoot $engine -StorageRoot $systemRoot -SessionSeconds 60
            }|Out-Null
        }catch{$failed=$true}
        Check ($failed -and $script:forbidden -eq 0) "$name fails before side effects"
        foreach($key in $before.Keys){Check ([Environment]::GetEnvironmentVariable($key,'Process') -ceq $before[$key]) "$name preserves process environment"}
    }
    $baseline=(& git -C $repo show 'c152ec0:unreal/Scripts/start-local-interactive.ps1') -join "`n"
    if($LASTEXITCODE -ne 0){throw 'Missing source baseline for launcher-tail check'}
    $current=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'start-local-interactive.ps1')).Replace("`r`n","`n").TrimEnd("`n")
    $marker='$head=('
    $expectedTail=$baseline.Substring($baseline.IndexOf($marker)).Replace("'-ResX=1280','-ResY=720'",'$viewportConfig.ResX,$viewportConfig.ResY').Replace("mode='standalone local practice';runDirectory=",'mode=''standalone local practice'';requestedViewport=$viewportConfig.Name;runDirectory=')
    Check ($current.Substring($current.IndexOf($marker)) -ceq $expectedTail) 'launcher tail changes only viewport arguments and requested-viewport metadata; all existing limits and lifecycle code retained'
    foreach($name in @('local-interactive-paths.ps1','interactive-viewport.ps1','start-local-interactive.ps1','start-small-match.ps1')){
        $tokens=$null;$errors=$null
        [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name),[ref]$tokens,[ref]$errors)|Out-Null
        Check ($errors.Count -eq 0) "$name syntax"
    }
    Write-Output "PASS local interactive path checks=$checks; inert fixtures only; no engine/runtime/physical evidence"
}finally{
    $env:UE_ENGINE_ROOT=$priorEngine;$env:RA2_STORAGE_ROOT=$priorStorage
    foreach($link in @($junctions)){Remove-Junction $link}
    $resolvedFixture=[IO.Path]::GetFullPath($fixture)
    $allowedPrefix=[IO.Path]::GetFullPath((Join-Path $repo 'generated'))+[IO.Path]::DirectorySeparatorChar
    if(-not $resolvedFixture.StartsWith($allowedPrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Refusing fixture cleanup outside this checkout'}
    if(Test-Path -LiteralPath $resolvedFixture){Remove-Item -LiteralPath $resolvedFixture -Recurse -Force}
}
