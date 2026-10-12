# Captures README screenshots and GIFs from the game.
# Needs ffmpeg on PATH (winget install Gyan.FFmpeg) and a built editor (Build.bat DeltaVDominionEditor).
#
#   powershell -ExecutionPolicy Bypass -File Scripts\capture_media.ps1
#   powershell -ExecutionPolicy Bypass -File Scripts\capture_media.ps1 -Only planet_ceres,kestrel_mars
#
# Frames are rendered off-screen by the engine itself ("DVCapture" console command), so nothing
# else on your desktop can end up in the images. Output goes to Docs\media.

param(
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8",
    [int]$Width = 1280,
    [int]$Height = 720,
    [switch]$GifsOnly,
    [switch]$FirefightOnly,
    [string]$Map = "/Game/Maps/Outpost",
    [string[]]$Only = @()
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Project = Join-Path $Root "DeltaVDominion.uproject"
$Out = Join-Path $Root "Docs\media"
$Frames = Join-Path $Root "Saved\Screenshots\DVCapture"
$Editor = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe"
New-Item -ItemType Directory -Force $Out, $Frames | Out-Null
$Save = Join-Path $Root "Saved\SaveGames\Campaign0.sav"
$SaveBackup = "$Save.capture-backup"
if (Test-Path -LiteralPath $Save) { Copy-Item -LiteralPath $Save $SaveBackup -Force }

function Invoke-Capture([string]$ExecCmds, [string]$LastFrame, [string]$Extra = "", [string]$ShotMap = "") {
    if (-not $ShotMap) { $ShotMap = $Map }
    Remove-Item -LiteralPath $LastFrame -ErrorAction SilentlyContinue
    # Every capture starts from a fresh campaign so the shots are consistent (your save is restored at the end).
    Remove-Item -LiteralPath $Save -ErrorAction SilentlyContinue
    $argList = "`"$Project`" $ShotMap -game -RenderOffScreen -windowed -ResX=$Width -ResY=$Height -nosound -unattended -DVQuitAfterCapture $Extra `"-ExecCmds=$ExecCmds`""
    $proc = Start-Process -FilePath $Editor -ArgumentList $argList -PassThru
    # The game quits itself after capturing, but engine shutdown can be slow; stop it once the last frame is on disk.
    for ($i = 0; $i -lt 600 -and -not (Test-Path -LiteralPath $LastFrame) -and -not $proc.HasExited; $i++) { Start-Sleep 1 }
    Start-Sleep 2
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
    if (-not (Test-Path -LiteralPath $LastFrame)) { throw "Capture produced no frames: $ExecCmds" }
}

function Save-Still([string]$Name, [int]$Camera, [float]$Delay, [int]$ShowUI = 0, [string]$Before = "") {
    if ($Only.Count -gt 0 -and $Only -notcontains $Name) { return }
    $cam = if ($Camera -ge 0) { "DVShowcase $Camera, " } else { "" }
    Invoke-Capture "${cam}${Before}DVCapture $Name 1 0 $Delay $ShowUI" (Join-Path $Frames "${Name}_0000.png")
    Copy-Item (Join-Path $Frames "${Name}_0000.png") (Join-Path $Out "$Name.png") -Force
}

function Save-Gif([string]$Name, [int]$Camera, [float]$Delay, [int]$Seconds, [string]$Before = "", [int]$Fps = 15, [int]$GifWidth = 640) {
    if ($Only.Count -gt 0 -and $Only -notcontains $Name) { return }
    # Fixed-step simulation: every frame is exactly 1/Fps of game time, however slow the capture.
    $count = $Seconds * $Fps
    Get-ChildItem $Frames -Filter "${Name}_*.png" | Remove-Item
    $last = Join-Path $Frames ("{0}_{1:D4}.png" -f $Name, ($count - 1))
    Invoke-Capture "DVShowcase $Camera, ${Before}DVCapture $Name $count 0 $Delay 1" $last "-BENCHMARK -FPS=$Fps"
    $filter = "scale=${GifWidth}:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=96:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle"
    ffmpeg -y -loglevel error -framerate $Fps -i (Join-Path $Frames "${Name}_%04d.png") -vf $filter -loop 0 (Join-Path $Out "$Name.gif")
}

function Save-GalaxyStill([string]$Name, [string]$Cmds, [float]$Delay) {
    if ($Only.Count -gt 0 -and $Only -notcontains $Name) { return }
    Invoke-Capture "${Cmds}DVCapture $Name 1 0 $Delay 1" (Join-Path $Frames "${Name}_0000.png") "" "/Game/Maps/Galaxy"
    Copy-Item (Join-Path $Frames "${Name}_0000.png") (Join-Path $Out "$Name.png") -Force
}

function Save-MapStill([string]$Name, [string]$ShotMap, [string]$Cmds, [float]$Delay, [int]$ShowUI = 0) {
    if ($Only.Count -gt 0 -and $Only -notcontains $Name) { return }
    Invoke-Capture "${Cmds}DVCapture $Name 1 0 $Delay $ShowUI" (Join-Path $Frames "${Name}_0000.png") "" $ShotMap
    Copy-Item (Join-Path $Frames "${Name}_0000.png") (Join-Path $Out "$Name.png") -Force
}

try {
if (-not $GifsOnly -and -not $FirefightOnly) {
    # Planets
    foreach ($planet in "Europa", "Io", "Proxima", "Ceres", "Gaia") {
        Save-MapStill ("planet_" + $planet.ToLower()) "/Game/Maps/$planet" "DVShowcase 0, " 6
    }
    Save-MapStill "gaia_title" "/Game/Maps/Gaia" "" 3 1
    # The Kestrel: hangar deck, war table, vehicles in action
    Save-MapStill "kestrel_hangar" "/Game/Maps/Kestrel" "DVShowcase 0, " 5
    Save-MapStill "kestrel_wartable" "/Game/Maps/Kestrel" "DVShowcase 1, " 4 1
    Save-MapStill "kestrel_mars" "/Game/Maps/Kestrel" "DVShowcase 3, " 5
    Save-MapStill "kestrel_armoury" "/Game/Maps/Kestrel" "DVShowcase 4, " 5
    Save-MapStill "landing_zone" "/Game/Maps/Outpost" "DVShowcase 4, " 3
    Save-MapStill "tank_kestrel" "/Game/Maps/Outpost" "DVBoard tank 1, DVAutopilot 2 0.6 0.1 0 1, " 7 1
    Save-MapStill "vehicle_gunship" "/Game/Maps/Outpost" "DVBoard gunship 1, DVAutopilot 1.5 0.6 0.2 0.6 3, " 7 1
    Save-MapStill "bulwark" "/Game/Maps/Outpost" "DVViewBulwark 520, " 6
    Save-MapStill "supply_drop" "/Game/Maps/Outpost" "DVReadyCallIns, DVSupplyDrop 1 1100, " 4 1
    Save-MapStill "operation_hold" "/Game/Maps/Outpost" "DVSkipStage 2, DVSkipStage 5, " 10 1
    Save-MapStill "extraction" "/Game/Maps/Outpost" "DVSkipStage 2, DVSkipStage 3, DVSkipStage 4, DVShowcase 2, " 13.5 1
    Save-GalaxyStill "galaxy" "" 9                                        # start screen: the Milky Way
    Save-GalaxyStill "galaxy_sol" "DVGalaxyView 1, " 5                    # Sol, Gaia and the local bubble
    Save-GalaxyStill "galaxy_arm" "DVGalaxyView 3, DVSelectSystem Eagle, " 6
    Save-Still "title_card" -1 3 1                    # mission intro: title card + Captain Vance on the radio
    Save-Still "outpost" 0 6                          # clean plate over the approach at dusk
    Save-Still "war_frames" 3 5                       # Hearthbound war-frames holding the bunker
    Save-Still "ridge" 1 6                            # low angle from the boulders
    Save-Still "first_person" -1 14 1                 # combat HUD after the intro
    Save-Still "after_action" 2 18 1 "DVEndMission 0 15, "
}
if (-not $FirefightOnly) { Save-Gif "strafing_run" 0 4.3 4 "DVReadyCallIns, DVStrafeRun 4, " }
if (-not $FirefightOnly) { Save-Gif "orbital_strike" 1 11 7 "DVOrbitalStrike 12, " }
Save-Gif "firefight" 3 4 8                            # tracers across the bunker doorway
}
finally {
    Remove-Item -LiteralPath $Save -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $SaveBackup) { Move-Item -LiteralPath $SaveBackup $Save -Force }
}

Get-ChildItem $Out | Select-Object Name, Length
