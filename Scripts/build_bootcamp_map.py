"""Builds /Game/Maps/BootCamp: a grey-box firing range for the milestone 1 slice.

Run from the editor (Tools > Execute Python Script) or headless (quits when done; the
script path must not contain spaces, so copy it somewhere like C:/Temp first):
  UnrealEditor-Cmd.exe DeltaVDominion.uproject -ExecutePythonScript="<path>/build_bootcamp_map.py -quit" -nullrhi -unattended

Layout (units are cm, +X points downrange):
  - 60 m x 40 m floor with 4 m perimeter walls
  - Player start at the west end; the game mode spawns your squad behind you
  - A field of low crates (crouch-height cover) and tall walls (lean/peek cover)
  - Hearthbound enemies (TeamId 2) dug in at the east end, facing the player
  - Lumen-friendly sky: sun, atmosphere, sky light, height fog
  - NavMeshBoundsVolume over the whole range (navmesh builds at runtime)
"""

import sys

import unreal

MAP_PATH = "/Game/Maps/BootCamp"

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

CUBE = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")  # 100 cm, pivot at centre
GRID = unreal.load_asset("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")
SOLDIER = unreal.load_class(None, "/Script/DeltaVDominion.DVSoldierCharacter")


def make_greybox_materials():
    """/Game/Dev/M_GreyBox (Color, Emissive, Roughness params) plus a few tinted instances.
    The C++ placeholder soldier bodies, rifles and tracers also use M_GreyBox."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary
    lib = unreal.EditorAssetLibrary
    path = "/Game/Dev/M_GreyBox"
    if lib.does_asset_exist(path):
        mat = lib.load_asset(path)
    else:
        mat = tools.create_asset("M_GreyBox", "/Game/Dev", unreal.Material, unreal.MaterialFactoryNew())
        color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -100)
        color.set_editor_property("parameter_name", "Color")
        color.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
        mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        emissive = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 150)
        emissive.set_editor_property("parameter_name", "Emissive")
        emissive.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
        mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 350)
        rough.set_editor_property("parameter_name", "Roughness")
        rough.set_editor_property("default_value", 0.7)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(mat)
        lib.save_loaded_asset(mat)

    def instance(name, rgb, roughness=0.7):
        ipath = "/Game/Dev/" + name
        if lib.does_asset_exist(ipath):
            mi = lib.load_asset(ipath)
        else:
            mi = tools.create_asset(name, "/Game/Dev", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", mat)
        mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
        lib.save_loaded_asset(mi)
        return mi

    return {
        "crate": instance("MI_Crate", (0.07, 0.08, 0.035)),        # olive drab
        "wall": instance("MI_Concrete", (0.18, 0.18, 0.17), 0.9),
        "ground": instance("MI_Ground", (0.16, 0.14, 0.11), 1.0),  # dusty earth outside the range
        "barrier": instance("MI_Barrier", (0.55, 0.42, 0.08)),    # hazard yellow
    }


def block(label, centre, size, material=GRID, folder="Geometry"):
    """Box of `size` (cm) whose centre is at `centre`."""
    actor = actors.spawn_actor_from_object(CUBE, unreal.Vector(*centre), unreal.Rotator(0, 0, 0))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    actor.static_mesh_component.set_material(0, material)
    return actor


def spawn(cls, label, location, rotation=(0, 0, 0), folder="Gameplay"):
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(*rotation))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def build():
    # Rebuild in place if the map exists (it may be the open startup map, so it can't be deleted).
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not levels.load_level(MAP_PATH):
            raise RuntimeError("Could not load " + MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors() if not isinstance(a, unreal.WorldSettings)])
    elif not levels.new_level(MAP_PATH):
        raise RuntimeError("Could not create " + MAP_PATH)

    mats = make_greybox_materials()

    # --- Ground and perimeter -------------------------------------------------------------
    block("Floor", (0, 0, -25), (6000, 4000, 50))
    block("Outskirts", (0, 0, -60), (60000, 60000, 50), mats["ground"])
    block("Wall_North", (0, 2025, 200), (6100, 50, 400), mats["wall"])
    block("Wall_South", (0, -2025, 200), (6100, 50, 400), mats["wall"])
    block("Wall_West", (-3025, 0, 200), (50, 4000, 400), mats["wall"])
    block("Wall_East", (3025, 0, 200), (50, 4000, 400), mats["wall"])

    # --- Cover ----------------------------------------------------------------------------
    # Low crates (~1 m): crouch behind, stand to fire over. Bots prefer these.
    low_cover = [
        (-1600, -900), (-1600, 700), (-900, 0), (-700, -1400), (-400, 1300),
        (0, -600), (300, 800), (700, -1200), (900, 300), (1500, -500),
        (1500, 1100), (2100, 0), (2200, -1300), (2300, 1400),
    ]
    for i, (x, y) in enumerate(low_cover):
        block("Crate_%02d" % i, (x, y, 55), (120, 220, 110), mats["crate"], "Cover")

    # Tall walls (3 m): full cover, lean (Q/E) to peek.
    tall_cover = [(-1200, -300), (-200, 500), (500, -300), (1100, 1400), (1200, -1700), (1900, 600)]
    for i, (x, y) in enumerate(tall_cover):
        block("CoverWall_%02d" % i, (x, y, 150), (40, 400, 300), mats["wall"], "Cover")

    # --- Gameplay -------------------------------------------------------------------------
    spawn(unreal.PlayerStart, "PlayerStart", (-2250, 0, 100))  # squad forms up to 5 m behind

    # Hearthbound fire team, east end, behind the last line of cover, facing west.
    enemy_spots = [(2350, -150), (2400, 1250), (2450, -1150), (1700, -650), (1700, 950)]
    for i, (x, y) in enumerate(enemy_spots):
        enemy = spawn(SOLDIER, "Hearthbound_%02d" % i, (x, y, 100), (0, 180, 0), "Enemies")
        enemy.set_editor_property("team_id", 2)

    # Showcase cameras for captures ("DVShowcase N" in the console).
    shots = [
        ((-3600, -2900, 2200), (600, 0, 0)),       # Showcase0: high overwatch of the whole range
        ((-1500, -1700, 260), (2300, 200, 120)),   # Showcase1: low, over the crates toward the enemy
        ((2900, 1900, 900), (-500, -300, 0)),      # Showcase2: reverse angle from behind the Hearthbound
    ]
    for i, (eye, target) in enumerate(shots):
        cam = spawn(unreal.CameraActor, "ShowcaseCam_%d" % i, eye, folder="Cameras")
        cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*eye), unreal.Vector(*target)), False)
        cam.tags = [unreal.Name("Showcase%d" % i)]
        cam.camera_component.set_editor_property("field_of_view", 70.0)

    nav = spawn(unreal.NavMeshBoundsVolume, "NavMeshBounds", (0, 0, 150), folder="Navigation")
    extent = nav.get_actor_bounds(False)[1]
    if extent.x > 1.0:
        nav.set_actor_scale3d(unreal.Vector(3100.0 / extent.x, 2100.0 / extent.y, 400.0 / extent.z))
    else:
        unreal.log_warning("NavMeshBoundsVolume spawned without a brush; add one by hand.")

    # A scripted volume doesn't always trigger the editor to create nav data, and without a
    # RecastNavMesh actor the game has nothing to generate into (AI then can't path at all).
    if not any(isinstance(a, unreal.RecastNavMesh) for a in actors.get_all_level_actors()):
        spawn(unreal.RecastNavMesh, "RecastNavMesh-Default", (0, 0, 0), folder="Navigation")
    unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "RebuildNavigation")

    # --- Lighting (fully dynamic: Lumen + virtual shadow maps) ----------------------------
    sun = spawn(unreal.DirectionalLight, "Sun", (0, 0, 1000), (-40, -35, 0), "Lighting")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_editor_property("intensity", 8.0)
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.SkyAtmosphere, "SkyAtmosphere", (0, 0, 0), folder="Lighting")
    sky = spawn(unreal.SkyLight, "SkyLight", (0, 0, 500), folder="Lighting")
    sky.light_component.set_editor_property("real_time_capture", True)
    sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.ExponentialHeightFog, "HeightFog", (0, 0, 0), folder="Lighting")
    spawn(unreal.VolumetricCloud, "Clouds", (0, 0, 0), folder="Lighting")

    if not levels.save_current_level():
        raise RuntimeError("Could not save " + MAP_PATH)

    final_extent = nav.get_actor_bounds(False)[1]
    unreal.log("BOOTCAMP_OK nav extent=%s actors=%d" % (final_extent, len(actors.get_all_level_actors())))


try:
    build()
finally:
    if "-quit" in sys.argv:
        unreal.SystemLibrary.quit_editor()
