# Captures README screenshots and GIFs from the game.
# Needs ffmpeg on PATH (winget install Gyan.FFmpeg) and a built editor (Build.bat DeltaVDominionEditor).
#
#   powershell -ExecutionPolicy Bypass -File Scripts\capture_media.ps1
#
# Frames are rendered off-screen by the engine itself ("DVCapture" console command), so nothing
# else on your desktop can end up in the images. Output goes to Docs\media.

param(
    [string]$Engine = "C:\Program Files\Epic Games\UE_5.8",
    [int]$Width = 1280,
    [int]$Height = 720,
    [switch]$GifsOnly,
    [switch]$FirefightOnly
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Project = Join-Path $Root "DeltaVDominion.uproject"
$Out = Join-Path $Root "Docs\media"
$Frames = Join-Path $Root "Saved\Screenshots\DVCapture"
$Editor = Join-Path $Engine "Engine\Binaries\Win64\UnrealEditor.exe"
New-Item -ItemType Directory -Force $Out, $Frames | Out-Null

function Invoke-Capture([string]$ExecCmds, [string]$LastFrame, [string]$Extra = "") {
    Remove-Item -LiteralPath $LastFrame -ErrorAction SilentlyContinue
    $argList = "`"$Project`" /Game/Maps/BootCamp -game -RenderOffScreen -windowed -ResX=$Width -ResY=$Height -nosound -unattended -DVQuitAfterCapture $Extra `"-ExecCmds=$ExecCmds`""
    $proc = Start-Process -FilePath $Editor -ArgumentList $argList -PassThru
    # The game quits itself after capturing, but engine shutdown can be slow; stop it once the last frame is on disk.
    for ($i = 0; $i -lt 600 -and -not (Test-Path -LiteralPath $LastFrame) -and -not $proc.HasExited; $i++) { Start-Sleep 1 }
    Start-Sleep 2
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
    if (-not (Test-Path -LiteralPath $LastFrame)) { throw "Capture produced no frames: $ExecCmds" }
}

function Save-Still([string]$Name, [int]$Camera, [float]$Delay) {
    $cam = if ($Camera -ge 0) { "DVShowcase $Camera, " } else { "" }
    Invoke-Capture "${cam}DVCapture $Name 1 0 $Delay" (Join-Path $Frames "${Name}_0000.png")
    Copy-Item (Join-Path $Frames "${Name}_0000.png") (Join-Path $Out "$Name.png") -Force
}

function Save-Gif([string]$Name, [int]$Camera, [float]$Delay, [int]$Seconds, [int]$Fps = 15, [int]$GifWidth = 640) {
    # Fixed-step simulation: every frame is exactly 1/Fps of game time, however slow the capture.
    $count = $Seconds * $Fps
    Get-ChildItem $Frames -Filter "${Name}_*.png" | Remove-Item
    $last = Join-Path $Frames ("{0}_{1:D4}.png" -f $Name, ($count - 1))
    Invoke-Capture "DVShowcase $Camera, DVCapture $Name $count 0 $Delay" $last "-BENCHMARK -FPS=$Fps"
    $filter = "scale=${GifWidth}:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=96:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle"
    ffmpeg -y -loglevel error -framerate $Fps -i (Join-Path $Frames "${Name}_%04d.png") -vf $filter -loop 0 (Join-Path $Out "$Name.gif")
}

if (-not $GifsOnly -and -not $FirefightOnly) {
    Save-Still "overview" 0 8
    Save-Still "cover_line" 1 14
    Save-Still "enemy_line" 2 16
    Save-Still "first_person" -1 6
}
if (-not $FirefightOnly) { Save-Gif "overwatch_flank" 0 3 12 }
Save-Gif "firefight" 2 3 10   # reverse angle: squad advancing into the Hearthbound guns

Get-ChildItem $Out | Select-Object Name, Length
