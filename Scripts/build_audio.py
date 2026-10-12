"""Imports Art/Audio/*.wav (made by Scripts/make_sounds.py) into /Game/Audio as SoundWave assets.

Run headless (script path without spaces):
  UnrealEditor-Cmd.exe DeltaVDominion.uproject -ExecutePythonScript="<path>/build_audio.py -quit" -nullrhi -unattended
Ambience and engine loops are flagged as looping. The game loads them by name (see Core/DVFx.h, DVSound).
"""

import glob
import os

import unreal

DEST = "/Game/Audio"
LOOPS = ("SW_AmbWind", "SW_AmbHangar", "SW_EngineGround", "SW_EngineGunship")

tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("AUDIO " + msg)


def main():
    src_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Art", "Audio")
    files = sorted(glob.glob(os.path.join(src_dir, "*.wav")))
    if not files:
        log("no wav files in %s (run: python Scripts/make_sounds.py)" % src_dir)
        return
    tasks = []
    for f in files:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", f)
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    count = 0
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        wave = lib.load_asset(DEST + "/" + name)
        if not wave:
            log("failed " + name)
            continue
        wave.set_editor_property("looping", name in LOOPS)
        lib.save_loaded_asset(wave)
        count += 1
    log("OK imported %d sounds" % count)


main()
