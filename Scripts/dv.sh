#!/usr/bin/env bash
# DELTA-V: DOMINION helper for an Ubuntu (WSL) terminal on the Windows PC that has Unreal installed.
# Works from any directory. Unreal itself runs on Windows; this script just drives it.
#
#   bash dv.sh <command>
#
#   editor     Open the project in the Unreal Editor (BootCamp map)
#   play       Launch the game standalone in a window
#   watch      Launch the game on the overwatch camera with the squad flanking (spectator view)
#   build      Compile the editor target (run after pulling C++ changes)
#   map        Regenerate the BootCamp grey-box map from Scripts/build_bootcamp_map.py
#   capture    Re-render README screenshots and GIFs into Docs/media
#   package    Build the downloadable Windows game into Build/Release/DeltaVDominion-Win64.zip
#   log        Follow the newest game/editor log
#   status     Show git status, last commits and what has been built
set -euo pipefail

UE_ROOT="${UE_ROOT:-/mnt/c/Program Files/Epic Games/UE_5.8}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${DV_PROJECT_DIR:-$(cd "$SCRIPT_DIR/.." && pwd)}"
UPROJECT="$PROJECT_DIR/DeltaVDominion.uproject"

win() { wslpath -w "$1"; }
editor_exe="$UE_ROOT/Engine/Binaries/Win64/UnrealEditor.exe"
editor_cmd="$UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
build_bat="$(win "$UE_ROOT/Engine/Build/BatchFiles/Build.bat")"

[[ -f "$UPROJECT" ]] || { echo "Can't find $UPROJECT (set DV_PROJECT_DIR)"; exit 1; }
[[ -f "$editor_exe" ]] || { echo "Can't find Unreal at $UE_ROOT (set UE_ROOT)"; exit 1; }

ps() { powershell.exe -NoProfile -ExecutionPolicy Bypass "$@"; }

case "${1:-help}" in
  editor)
    "$editor_exe" "$(win "$UPROJECT")" >/dev/null 2>&1 &
    echo "Unreal Editor is starting (first launch compiles shaders and can take a few minutes)." ;;
  play)
    "$editor_exe" "$(win "$UPROJECT")" /Game/Maps/BootCamp -game -windowed -ResX=1600 -ResY=900 >/dev/null 2>&1 &
    echo "Game is starting in a window. Controls: WASD, mouse, LMB fire, RMB aim, F/G/V/T/X squad orders." ;;
  watch)
    "$editor_exe" "$(win "$UPROJECT")" /Game/Maps/BootCamp -game -windowed -ResX=1600 -ResY=900 "-ExecCmds=DVShowcase 0" >/dev/null 2>&1 &
    echo "Spectator view starting. Type ~ then 'DVShowcase 1' or 'DVShowcase 2' for other cameras, 'DVShowcase -1' to take control." ;;
  build)
    ps -Command "& '$build_bat' DeltaVDominionEditor Win64 Development '-Project=$(win "$UPROJECT")' -WaitMutex; exit \$LASTEXITCODE" ;;
  map)
    tmp="/mnt/c/Users/Public/build_bootcamp_map.py"   # Unreal's -ExecutePythonScript can't take paths with spaces
    cp "$SCRIPT_DIR/build_bootcamp_map.py" "$tmp"
    "$editor_cmd" "$(win "$UPROJECT")" "-ExecutePythonScript=$(win "$tmp" | tr '\\' '/') -quit" -nullrhi -unattended -nosplash -stdout \
      | grep -E "BOOTCAMP|LogPython: (Error|Warning)" || true ;;
  capture)
    ps -File "$(win "$SCRIPT_DIR/capture_media.ps1")" ;;
  package)
    ps -File "$(win "$SCRIPT_DIR/package_game.ps1")" ;;
  log)
    tail -F "$(ls -t "$PROJECT_DIR"/Saved/Logs/*.log | head -1)" ;;
  status)
    command -v git-lfs >/dev/null || { echo "Install Git LFS first: sudo apt install -y git-lfs && git lfs install"; exit 1; }
    git -C "$PROJECT_DIR" status --short --branch
    git -C "$PROJECT_DIR" log --oneline -8
    echo
    ls -la "$PROJECT_DIR/Binaries/Win64/"*.dll 2>/dev/null | awk '{print "built:", $6, $7, $8, $9}' || echo "Editor not built yet: bash dv.sh build"
    ls -la "$PROJECT_DIR/Build/Release/"*.zip 2>/dev/null | awk '{print "package:", $5, "bytes", $9}' || echo "No package yet: bash dv.sh package" ;;
  *)
    awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "${BASH_SOURCE[0]}" ;;
esac
