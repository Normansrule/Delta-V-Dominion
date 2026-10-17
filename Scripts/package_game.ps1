# Builds the downloadable Windows game: cook, pak and zip a build.
#
#   powershell -ExecutionPolicy Bypass -File Scripts\package_game.ps1
#
# Output: Build\Release\DeltaVDominion-Win64.zip (unzip anywhere, run DeltaVDominion.exe).
# The zip includes Unreal's prerequisites installer (VC++ runtime, DirectX) for PCs that lack them.

param(
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8",
    [string]$Config = "Development"   # Development keeps the ~ console (DVShowcase); use Shipping for public releases
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Project = Join-Path $Root "DeltaVDominion.uproject"
$Archive = Join-Path $Root "Build\Release\Staged"
$Zip = Join-Path $Root "Build\Release\DeltaVDominion-Win64.zip"
$Uat = Join-Path $Engine "Engine\Build\BatchFiles\RunUAT.bat"

if (Test-Path $Archive) { Remove-Item -Recurse -Force $Archive }

& $Uat BuildCookRun "-project=$Project" -noP4 -platform=Win64 "-clientconfig=$Config" `
    -build -cook -stage -pak -iostore -compressed -prereqs -nodebuginfo `
    -archive "-archivedirectory=$Archive" -map=/Game/Maps/Galaxy+/Game/Maps/Kestrel+/Game/Maps/Outpost+/Game/Maps/Europa+/Game/Maps/Io+/Game/Maps/Proxima+/Game/Maps/Ceres+/Game/Maps/Gaia+/Game/Maps/Breakwater+/Game/Maps/RooksRest+/Game/Maps/BootCamp -unattended -utf8output
if ($LASTEXITCODE -ne 0) { throw "Packaging failed (exit $LASTEXITCODE). See the log above." }

$Game = Join-Path $Archive "Windows"
Copy-Item (Join-Path $PSScriptRoot "..\Installer\PLAY_ME.txt") $Game -Force
if (Test-Path $Zip) { Remove-Item -Force $Zip }
Compress-Archive -Path (Join-Path $Game "*") -DestinationPath $Zip -CompressionLevel Optimal

$size = [math]::Round((Get-Item $Zip).Length / 1MB)
Write-Host "Packaged $Zip ($size MB)"
