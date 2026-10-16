param([string]$EngineRoot='D:\Epic Games\UE_5.7',[ValidateRange(60,1800)][int]$SessionSeconds=900)
& (Join-Path $PSScriptRoot 'start-local-interactive.ps1') -EngineRoot $EngineRoot -SessionSeconds $SessionSeconds -Skirmish
