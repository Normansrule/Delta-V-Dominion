# DELTA-V: DOMINION installer for Windows 10/11 (64-bit).
#
# One line, in PowerShell:
#   irm https://raw.githubusercontent.com/Normansrule/DeltaVDominion/main/Installer/Install-DeltaVDominion.ps1 | iex
#
# Downloads the newest release from GitHub, installs it to %LOCALAPPDATA%\DeltaVDominion,
# installs the Visual C++ / DirectX runtime if needed, and adds Desktop + Start Menu shortcuts.
# Re-run it any time to update. Uninstall: delete that folder and the two shortcuts.

param(
    [string]$Repo = "Normansrule/DeltaVDominion",
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA "DeltaVDominion")
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"   # makes Invoke-WebRequest much faster
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

Write-Host "DELTA-V: DOMINION installer" -ForegroundColor Cyan

$release = Invoke-RestMethod "https://api.github.com/repos/$Repo/releases/latest" -Headers @{ "User-Agent" = "DeltaVDominion-Installer" }
$asset = $release.assets | Where-Object { $_.name -like "DeltaVDominion-Win64*.zip" } | Select-Object -First 1
if (-not $asset) { throw "No Windows build attached to release $($release.tag_name)." }

$zip = Join-Path $env:TEMP $asset.name
Write-Host ("Downloading {0} ({1} MB)..." -f $release.tag_name, [math]::Round($asset.size / 1MB))
Invoke-WebRequest $asset.browser_download_url -OutFile $zip -Headers @{ "User-Agent" = "DeltaVDominion-Installer" }

Write-Host "Installing to $InstallDir..."
if (Test-Path $InstallDir) { Remove-Item -Recurse -Force $InstallDir }
Expand-Archive $zip -DestinationPath $InstallDir
Remove-Item $zip

$prereq = Get-ChildItem $InstallDir -Recurse -Filter "UEPrereqSetup_x64.exe" | Select-Object -First 1
if ($prereq) {
    Write-Host "Installing Visual C++ and DirectX runtimes (skipped if already present)..."
    Start-Process $prereq.FullName -ArgumentList "/quiet /norestart" -Wait
}

$exe = Join-Path $InstallDir "DeltaVDominion.exe"
$shell = New-Object -ComObject WScript.Shell
foreach ($dir in @([Environment]::GetFolderPath("Desktop"), (Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"))) {
    $lnk = $shell.CreateShortcut((Join-Path $dir "DELTA-V Dominion.lnk"))
    $lnk.TargetPath = $exe
    $lnk.WorkingDirectory = $InstallDir
    $lnk.Save()
}

Set-Content (Join-Path $InstallDir "version.txt") $release.tag_name
Write-Host "Installed $($release.tag_name). Launch 'DELTA-V Dominion' from your Desktop or Start Menu." -ForegroundColor Green
