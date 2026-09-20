<#
.SYNOPSIS
    Builds the FactoryGame Development Editor target (Win64) from the CLI,
    without requiring Visual Studio.

.DESCRIPTION
    Resolves the custom Unreal Engine install from FactoryGame.uproject's
    EngineAssociation via the per-user engine registration registry key,
    then invokes the engine's own Build.bat (UnrealBuildTool) with the
    exact arguments Visual Studio's generated project files use for the
    "Development Editor" configuration. AIMod (and any other enabled
    Mods/GameFeatures plugin) is compiled as part of this target because
    it is discovered as an enabled Game Feature plugin, not because it is
    named explicitly here.

    This script is a thin wrapper around UnrealBuildTool; it does not
    replace or reimplement it. Build.bat streams UBT's own stdout/stderr
    directly to the console and this script exits with UBT's actual exit
    code, so errors are never swallowed.

.PARAMETER Target
    UBT target name. Defaults to FactoryEditor (the Development Editor
    target that pulls in FactoryGame + all enabled Mods/GameFeatures
    plugins, including AIMod).

.PARAMETER Platform
    Build platform. Defaults to Win64.

.PARAMETER Configuration
    Build configuration. Defaults to Development.

.PARAMETER Architecture
    Target architecture. Defaults to x64.

.PARAMETER EnginePath
    Explicit path to the Unreal Engine root (the directory containing
    Engine\Build\BatchFiles\Build.bat). If omitted, it is resolved from
    FactoryGame.uproject's EngineAssociation via
    HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds.

.EXAMPLE
    .\tools\build-editor.ps1

.EXAMPLE
    .\tools\build-editor.ps1 -EnginePath "F:\Claude\Unreal Engine - CSS"
#>
[CmdletBinding()]
param(
    [string]$Target = "FactoryEditor",
    [string]$Platform = "Win64",
    [string]$Configuration = "Development",
    [string]$Architecture = "x64",
    [string]$EnginePath
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$UProjectPath = Join-Path $RepoRoot "FactoryGame.uproject"

if (-not (Test-Path $UProjectPath)) {
    throw "Could not find FactoryGame.uproject at '$UProjectPath'."
}

if (-not $EnginePath) {
    $UProject = Get-Content -Raw $UProjectPath | ConvertFrom-Json
    $EngineAssociation = $UProject.EngineAssociation
    if (-not $EngineAssociation) {
        throw "FactoryGame.uproject has no EngineAssociation set. Pass -EnginePath explicitly."
    }

    $RegPath = "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds"
    $RegValue = $null
    if (Test-Path $RegPath) {
        $RegValue = (Get-ItemProperty -Path $RegPath -Name $EngineAssociation -ErrorAction SilentlyContinue).$EngineAssociation
    }
    if (-not $RegValue) {
        throw "Could not resolve engine association '$EngineAssociation' from '$RegPath'. Register the engine (Epic Games Launcher / GenerateProjectFiles) or pass -EnginePath explicitly."
    }
    $EnginePath = $RegValue
}

$BuildBat = Join-Path $EnginePath "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path $BuildBat)) {
    throw "Build.bat not found at '$BuildBat'. Check -EnginePath / the engine registration."
}

Write-Host "Engine:        $EnginePath"
Write-Host "Build.bat:     $BuildBat"
Write-Host "Target:        $Target"
Write-Host "Platform:      $Platform"
Write-Host "Configuration: $Configuration"
Write-Host "Architecture:  $Architecture"
Write-Host "Project:       $UProjectPath"
Write-Host ""

& $BuildBat $Target $Platform $Configuration "-Project=`"$UProjectPath`"" -WaitMutex "-architecture=$Architecture"
$ExitCode = $LASTEXITCODE

Write-Host ""
if ($ExitCode -ne 0) {
    Write-Host "Build FAILED with exit code $ExitCode." -ForegroundColor Red
} else {
    Write-Host "Build succeeded." -ForegroundColor Green
}

exit $ExitCode
