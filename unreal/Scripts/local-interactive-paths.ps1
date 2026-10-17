# Local-only configuration. Do not publish resolved machine paths or raw receipts.
function Resolve-ProofInteractivePaths {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$RepoRoot,
        [string]$EngineRoot = $env:UE_ENGINE_ROOT,
        [string]$StorageRoot = $env:RA2_STORAGE_ROOT
    )

    function Assert-ProofNoReparseAncestor([string]$Path, [string]$Label, [bool]$Directory=$true) {
        $current = [IO.Path]::GetFullPath($Path)
        $requested = $current
        while ($true) {
            $lookupErrors = @()
            $item = Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue -ErrorVariable lookupErrors
            if (@($lookupErrors | Where-Object { $_.CategoryInfo.Category -ne [Management.Automation.ErrorCategory]::ObjectNotFound }).Count -gt 0) {
                throw "Cannot inspect $Label path."
            }
            if ($null -ne $item) { break }
            $parent = [IO.Directory]::GetParent($current)
            if ($null -eq $parent -or $parent.FullName -eq $current) { throw "Cannot validate $Label directory." }
            $current = $parent.FullName
        }
        if ($current -eq $requested -and $Directory -and -not $item.PSIsContainer) {
            throw "$Label must be a directory."
        }
        while ($null -ne $item) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Reparse points are unsupported in $Label paths; use a direct local directory."
            }
            if ($item -is [IO.FileInfo]) { $item = $item.Directory }
            else { $item = $item.Parent }
        }
    }

    function Resolve-ProofLocalDirectory([string]$Value, [string]$Label) {
        if ([string]::IsNullOrWhiteSpace($Value) -or $Value -notmatch '^[A-Za-z]:[\\/]') {
            throw "Configure $Label as an absolute local directory."
        }
        $item = Get-Item -LiteralPath $Value -Force -ErrorAction Stop
        if (-not $item.PSIsContainer -or $item.PSProvider.Name -ne 'FileSystem') {
            throw "$Label must identify an existing filesystem directory."
        }
        Assert-ProofNoReparseAncestor $item.FullName $Label
        return $item.FullName
    }

    $storage = Resolve-ProofLocalDirectory $StorageRoot 'StorageRoot / RA2_STORAGE_ROOT'
    $repo = Resolve-ProofLocalDirectory $RepoRoot 'RepoRoot'
    $systemVolume = [IO.Path]::GetPathRoot([Environment]::GetFolderPath([Environment+SpecialFolder]::Windows))
    if (-not $systemVolume -or [IO.Path]::GetPathRoot($storage).Equals($systemVolume, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'StorageRoot must be on a non-system volume.'
    }
    $prefix = $storage.TrimEnd([char[]]@([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)) + [IO.Path]::DirectorySeparatorChar
    if (-not $repo.Equals($storage, [StringComparison]::OrdinalIgnoreCase) -and -not $repo.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The checkout must be inside the configured StorageRoot.'
    }
    $engine = Resolve-ProofLocalDirectory $EngineRoot 'EngineRoot / UE_ENGINE_ROOT'
    $editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
    Assert-ProofNoReparseAncestor $editor 'engine executable' $false
    if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw 'EngineRoot does not contain the required editor executable.' }
    $project = Join-Path $repo 'unreal'
    $project = Resolve-ProofLocalDirectory $project 'project'
    Assert-ProofNoReparseAncestor (Join-Path $repo 'generated') 'generated output'
    Assert-ProofNoReparseAncestor (Join-Path $repo 'generated/controls-cache') 'interactive cache'
    return [pscustomobject]@{RepoRoot=$repo;StorageRoot=$storage;EngineRoot=$engine;ProjectRoot=$project;Editor=$editor}
}
