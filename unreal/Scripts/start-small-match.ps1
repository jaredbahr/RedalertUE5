param([string]$EngineRoot=$env:UE_ENGINE_ROOT,[ValidateRange(60,1800)][int]$SessionSeconds=900,[switch]$CameraPan,[switch]$CameraEdgePan,[string]$StorageRoot=$env:RA2_STORAGE_ROOT)
& (Join-Path $PSScriptRoot 'start-local-interactive.ps1') -EngineRoot $EngineRoot -StorageRoot $StorageRoot -SessionSeconds $SessionSeconds -Skirmish -CameraPan:$CameraPan -CameraEdgePan:$CameraEdgePan
