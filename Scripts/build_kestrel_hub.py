"""Builds /Game/Maps/Kestrel: the hangar deck of the TAS Kestrel, Delta Squadron's home base.

Walk the deck with your squad, look out of the open hangar mouth at Mars below, use the war table (Z) to
open the galaxy map and pick the next operation, and visit the memorial wall. The gunships, tank and LTV
parked on the deck are real vehicles you can board.

Run headless (script path without spaces):
  UnrealEditor-Cmd.exe DeltaVDominion.uproject -ExecutePythonScript="<path>/build_kestrel_hub.py -quit" -nullrhi -unattended
Units are cm. The hangar mouth is at -X, open to space.
"""

import math
import random
import sys

import unreal

MAP_PATH = "/Game/Maps/Kestrel"
SHARED = "/Game/Environment/Outpost/Materials/"

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
random.seed(11)

CHAMFER = None
SIZE = None


def log(msg):
    unreal.log("KESTREL " + msg)


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def setp(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as err:
        log("skipped %s.%s (%s)" % (type(obj).__name__, name, err))


def mat(name):
    return lib.load_asset(SHARED + name)


def spawn(cls, label, loc, rotation=None, folder="Hub"):
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), rotation or rot())
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def block(label, centre, size, material, yaw=0.0, folder="Hangar", mesh=None):
    m = mesh or CHAMFER
    b = m.get_bounds()
    ext = (b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2)
    a = actors.spawn_actor_from_object(m, unreal.Vector(0, 0, 0), rot(yaw=yaw))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.set_actor_scale3d(unreal.Vector(size[0] / ext[0], size[1] / ext[1], size[2] / ext[2]))
    comp = a.static_mesh_component
    for i in range(comp.get_num_materials()):
        comp.set_material(i, material)
    origin, _ = a.get_actor_bounds(False)
    loc = a.get_actor_location()
    a.set_actor_location(unreal.Vector(centre[0] + loc.x - origin.x, centre[1] + loc.y - origin.y, centre[2] + loc.z - origin.z), False, False)
    return a


def expr(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def const3(m, rgb, x, y):
    return expr(m, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def lerp(m, a, b, alpha, x, y, alpha_pin=""):
    l = expr(m, unreal.MaterialExpressionLinearInterpolate, x, y)
    mel.connect_material_expressions(a, "", l, "A")
    mel.connect_material_expressions(b, "", l, "B")
    mel.connect_material_expressions(alpha, alpha_pin, l, "Alpha")
    return l


def import_texture(png, dest, name):
    """Imports Art/Planets/<png> (made by Scripts/make_planet_textures.py) as a texture asset."""
    import os
    src = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Art", "Planets", png)
    if not os.path.exists(src):
        log("missing %s (run: python Scripts/make_planet_textures.py)" % src)
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])
    return lib.load_asset(dest + "/" + name)


def make_mars_material():
    """Mars from orbit: a generated equirectangular albedo (plains, dark basalt provinces, a great canyon,
    craters and polar caps) wrapped on a lat-long sphere, plus a thin dusty limb glow."""
    path = "/Game/Environment/Kestrel/M_PlanetMars"
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    albedo = import_texture("T_Mars.png", "/Game/Environment/Kestrel", "T_Mars")
    m = tools.create_asset("M_PlanetMars", "/Game/Environment/Kestrel", unreal.Material, unreal.MaterialFactoryNew())
    if albedo:
        t = expr(m, unreal.MaterialExpressionTextureSample, -900, 0, texture=albedo)
        mel.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(m, unreal.MaterialExpressionConstant, -700, 250, r=0.95)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    fres = expr(m, unreal.MaterialExpressionFresnel, -1000, 500, exponent=5.0, base_reflect_fraction=0.0)
    limb = const3(m, (1.2, 0.55, 0.3), -1000, 650)
    glow = expr(m, unreal.MaterialExpressionMultiply, -800, 550)
    mel.connect_material_expressions(fres, "", glow, "A")
    mel.connect_material_expressions(limb, "", glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    lib.save_loaded_asset(m)
    return m


def smooth_sphere():
    """A dense UV sphere (radius 50, like /Engine/BasicShapes/Sphere) so the planet's limb is round, not faceted."""
    path = "/Game/Environment/Kestrel/SM_PlanetSphere"
    if lib.does_asset_exist(path):
        return lib.load_asset(path)
    gs = unreal
    dm = unreal.DynamicMesh()
    gs.GeometryScript_Primitives.append_sphere_lat_long(dm, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), 50.0, 128, 192)
    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.set_editor_property("enable_nanite", False)
    opts.set_editor_property("enable_collision", False)
    mesh, _ = gs.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm, path, opts)
    lib.save_loaded_asset(mesh)
    return mesh


def light(label, loc, color, candelas, radius, spot=False, pitch=-90.0):
    cls = unreal.SpotLight if spot else unreal.PointLight
    a = spawn(cls, label, loc, rot(pitch=pitch) if spot else None, "Lights")
    lc = a.spot_light_component if spot else a.point_light_component
    setp(lc, "intensity_units", unreal.LightUnits.CANDELAS)
    setp(lc, "intensity", candelas)
    setp(lc, "light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    setp(lc, "attenuation_radius", radius)
    if spot:
        setp(lc, "outer_cone_angle", 55.0)
        setp(lc, "inner_cone_angle", 30.0)
    return a


def build():
    global CHAMFER
    CHAMFER = lib.load_asset("/Game/LevelPrototyping/Meshes/SM_ChamferCube")
    cyl = lib.load_asset("/Engine/BasicShapes/Cylinder")
    sphere = lib.load_asset("/Engine/BasicShapes/Sphere")
    deck, steel, hull = mat("MI_Concrete_Dark"), mat("MI_Gunmetal"), mat("MI_Container_Navy")
    olive, rust, hazard = mat("MI_Container_Olive"), mat("MI_Container_Rust"), mat("MI_Hazard")
    lamp, cyan, red = mat("MI_Lamp"), mat("MI_Strip_Cyan"), mat("MI_Strip_Red")
    amber, mars = mat("MI_Neon_Amber"), mat("MI_GasGiant")

    if lib.does_asset_exist(MAP_PATH):
        levels.load_level(MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors() if not isinstance(a, unreal.WorldSettings)])
    elif not levels.new_level(MAP_PATH):
        raise RuntimeError("Could not create " + MAP_PATH)

    L, W, H = 12000.0, 6000.0, 2500.0

    # --- Shell: deck, walls with structural ribs, ceiling with light bays, aft doors ------------------
    block("Deck", (0, 0, -20), (L, W, 40), deck)
    for lane in (-1200, 0, 1200):
        block("Lane_%d" % lane, (-500, lane, 2), (L - 1500, 18, 4), cyan)
    for i in range(12):
        block("Hazard_N%d" % i, (-5400 + i * 1000, W / 2 - 300, 2), (500, 60, 4), hazard)
        block("Hazard_S%d" % i, (-5400 + i * 1000, -W / 2 + 300, 2), (500, 60, 4), hazard)
    for side, y in (("N", W / 2), ("S", -W / 2)):
        block("Wall_" + side, (0, y + (40 if y > 0 else -40), H / 2), (L, 80, H), hull)
        for i in range(13):
            x = -6000 + i * 1000
            block("Rib_%s%d" % (side, i), (x, y - (90 if y > 0 else -90), H / 2), (120, 180, H), steel)
            block("RibLight_%s%d" % (side, i), (x, y - (182 if y > 0 else -182), 1500), (40, 6, 600), cyan)
        # Catwalk with railing at 9 m.
        cy = y - (380 if y > 0 else -380)
        block("Catwalk_" + side, (0, cy, 900), (L, 360, 30), steel)
        block("Rail_" + side, (0, cy - (170 if y > 0 else -170), 1010), (L, 8, 8), steel)
        for i in range(24):
            block("Post_%s%d" % (side, i), (-5750 + i * 500, cy - (170 if y > 0 else -170), 960), (8, 8, 100), steel)
            block("Strut_%s%d" % (side, i), (-5750 + i * 500, cy, 450), (30, 30, 900), steel)
    block("Ceiling", (0, 0, H + 30), (L, W + 160, 60), hull)
    for i in range(6):
        for j in (-1, 1):
            x, y = -5000 + i * 2000, j * 1500
            block("LightBay_%d_%d" % (i, j), (x, y, H - 10), (900, 260, 20), lamp)
            light("BayLight_%d_%d" % (i, j), (x, y, H - 120), (255, 236, 210), 3500.0, 3200.0, spot=True)
    block("AftDoors", (L / 2 + 40, 0, H / 2), (80, W, H), steel)
    block("AftDoorSeam", (L / 2 - 4, 0, H / 2), (10, 30, H - 200), red)
    for k in range(5):
        block("AftDoorBand%d" % k, (L / 2 - 4, 0, 300 + k * 450), (10, W - 400, 30), hazard)

    # --- Hangar mouth: open to space with an energy frame -----------------------------------------
    mx = -L / 2
    block("MouthTop", (mx, 0, H - 150), (300, W, 300), steel)
    block("MouthL", (mx, W / 2 - 200, H / 2), (300, 400, H), steel)
    block("MouthR", (mx, -W / 2 + 200, H / 2), (300, 400, H), steel)
    block("MouthGlowTop", (mx - 160, 0, H - 300), (20, W - 800, 30), cyan)
    block("MouthGlowL", (mx - 160, W / 2 - 420, H / 2), (20, 30, H - 400), cyan)
    block("MouthGlowR", (mx - 160, -W / 2 + 420, H / 2), (20, 30, H - 400), cyan)
    block("LaunchLip", (mx - 600, 0, -20), (1200, W - 800, 40), deck)
    for i in range(6):
        block("LaunchLight%d" % i, (mx - 1100 + i * 200, 0, 4), (40, W - 1200, 6), amber)

    # Mars below and the sun beyond, seen through the mouth.
    r = 3000000.0
    planet = actors.spawn_actor_from_object(smooth_sphere(), unreal.Vector(-9000000.0, 1000000.0, -2600000.0), rot(pitch=25))
    planet.set_actor_label("Mars")
    planet.set_folder_path("Space")
    planet.set_actor_scale3d(unreal.Vector(r / 50.0, r / 50.0, r / 50.0))
    planet.static_mesh_component.set_material(0, make_mars_material())
    # Phobos, a lumpy little moon hanging off Mars' limb.
    phobos = actors.spawn_actor_from_object(sphere, unreal.Vector(-2500000.0, -1100000.0, -250000.0), rot(pitch=20, yaw=40))
    phobos.set_actor_label("Phobos")
    phobos.set_folder_path("Space")
    phobos.set_actor_scale3d(unreal.Vector(1100.0, 800.0, 700.0))
    phobos.static_mesh_component.set_material(0, mat("MI_Concrete_Dark"))
    sun = spawn(unreal.DirectionalLight, "Sun", (0, 0, 4000), rot(pitch=-28, yaw=130), "Lighting")   # from behind and to one side: Mars shows a terminator
    setp(sun.light_component, "intensity", 4.0)
    setp(sun.light_component, "light_color", unreal.Color(r=255, g=240, b=220, a=255))
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky = spawn(unreal.SkyLight, "SkyLight", (0, 0, 1000), folder="Lighting")
    setp(sky.light_component, "real_time_capture", False)   # no sky to capture in space
    setp(sky.light_component, "intensity", 0.6)
    sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)

    # --- Gantry crane over the deck ------------------------------------------------------------------
    block("Gantry", (1500, 0, 2150), (300, W - 200, 200), steel)
    block("GantryTrolley", (1500, 600, 2000), (400, 400, 160), hazard)
    block("GantryCable", (1500, 600, 1500), (10, 10, 900), steel)
    block("GantryHook", (1500, 600, 1020), (120, 120, 120), steel)

    # --- Cargo, fuel and munitions --------------------------------------------------------------------
    for i in range(14):
        x, y = random.uniform(-4500, 4500), random.choice((-1, 1)) * random.uniform(2000, 2500)
        sx, sy, sz = random.choice(((240, 160, 120), (200, 200, 200), (600, 244, 260)))
        block("Crate_%02d" % i, (x, y, sz / 2), (sx, sy, sz), random.choice((olive, rust, steel)), random.uniform(-10, 10), "Cargo")
    for i in range(4):
        x = 3600 + i * 380
        block("FuelTank_%d" % i, (x, 2500, 220), (300, 300, 440), hazard, folder="Cargo", mesh=cyl)
    for i in range(8):
        block("MunitionRack_%d" % i, (-4400 + i * 320, -2450, 120), (260, 120, 240), steel, folder="Cargo")
        block("Munition_%d" % i, (-4400 + i * 320, -2450, 260), (220, 40, 40), red, folder="Cargo")

    # --- Vehicles on the deck (real, boardable) --------------------------------------------------------
    gunship = unreal.load_class(None, "/Script/DeltaVDominion.DVGunship")
    tank = unreal.load_class(None, "/Script/DeltaVDominion.DVTank")
    ltv = unreal.load_class(None, "/Script/DeltaVDominion.DVGroundVehicle")
    for i, y in enumerate((-1500, 1500)):
        block("Pad_%d" % i, (-3000, y, 5), (1500, 1500, 10), deck, folder="Vehicles")
        block("PadRing_%d" % i, (-3000, y, 12), (1300, 1300, 4), cyan, folder="Vehicles", mesh=cyl)
        spawn(gunship, "Raven_%d" % i, (-3000, y, 190), rot(yaw=180), "Vehicles")
    spawn(tank, "Sabre", (3000, -1700, 140), rot(yaw=180), "Vehicles")
    spawn(ltv, "Jackal", (3000, 1700, 120), rot(yaw=200), "Vehicles")

    # --- War table, memorial wall, armoury ----------------------------------------------------------
    inter = unreal.load_class(None, "/Script/DeltaVDominion.DVInteractable")
    table = spawn(inter, "WarTable", (0, 0, 0), folder="Stations")
    table.set_editor_property("interaction", unreal.DVInteraction.WAR_TABLE)
    table.set_editor_property("prompt", "Open the war table (galaxy map)")
    block("MemorialWall", (1500, -W / 2 + 140, 700), (2400, 40, 1200), steel, folder="Stations")
    block("MemorialPlaque", (1500, -W / 2 + 165, 700), (2200, 10, 1000), amber, folder="Stations")
    block("MemorialFlagBar", (1500, -W / 2 + 175, 1380), (2400, 20, 60), red, folder="Stations")
    memorial = spawn(inter, "Memorial", (1500, -W / 2 + 600, 0), folder="Stations")
    memorial.set_editor_property("interaction", unreal.DVInteraction.MEMORIAL)
    memorial.set_editor_property("prompt", "Memorial wall and roster")
    armoury = spawn(inter, "Armoury", (-1000, W / 2 - 600, 0), folder="Stations")
    armoury.set_editor_property("interaction", unreal.DVInteraction.ARMOURY)
    armoury.set_editor_property("prompt", "Armoury: take the next weapon off the rack")
    guns = [lib.load_asset("/Game/Weapons/Rifle/Meshes/SM_Rifle"), lib.load_asset("/Game/Weapons/Pistol/Meshes/SM_Pistol"),
            lib.load_asset("/Game/Weapons/GrenadeLauncher/Meshes/SM_GrenadeLauncher")]
    for i in range(6):
        x = -1900 + i * 360
        block("WeaponRack_%d" % i, (x, W / 2 - 200, 140), (300, 40, 280), steel, folder="Stations")
        block("RackGlow_%d" % i, (x, W / 2 - 225, 250), (260, 6, 20), mat("MI_Strip_Cyan"), folder="Stations")
        for k in range(2):
            gun = guns[(i + k) % len(guns)]
            if gun:
                g = actors.spawn_actor_from_object(gun, unreal.Vector(x - 60 + k * 120, W / 2 - 240, 120 + k * 80), rot(pitch=80, yaw=90))
                g.set_actor_label("RackGun_%d_%d" % (i, k))
                g.set_folder_path("Stations")
                g.set_actor_scale3d(unreal.Vector(2.0, 2.0, 2.0))
    light("ArmouryLight", (-1000, W / 2 - 700, 900), (190, 255, 210), 2500.0, 2200.0, spot=True, pitch=-60.0)

    # --- Gameplay -------------------------------------------------------------------------------------
    spawn(unreal.PlayerStart, "PlayerStart", (1600, 0, 110), rot(yaw=180), "Gameplay")
    nav = spawn(unreal.NavMeshBoundsVolume, "NavMeshBounds", (0, 0, 300), folder="Navigation")
    ext = nav.get_actor_bounds(False)[1]
    if ext.x > 1.0:
        nav.set_actor_scale3d(unreal.Vector(5800.0 / ext.x, 2800.0 / ext.y, 600.0 / ext.z))
    spawn(unreal.RecastNavMesh, "RecastNavMesh-Default", (0, 0, 0), folder="Navigation")

    shots = [((4800, 1800, 1300), (-6000, -400, 600)),    # 0: down the deck toward the mouth and Mars
             ((900, 700, 260), (0, 0, 150)),               # 1: the war table
             ((-5600, -1800, 300), (2000, 600, 500)),      # 2: from the mouth looking aboard
             ((-7800, 0, 1300), (-90000, 10000, -26000)),  # 3: out of the mouth at Mars
             ((-2500, W / 2 - 1300, 330), (-1200, W / 2 - 150, 170))]   # 4: the armoury racks
    for i, (eye, target) in enumerate(shots):
        cam = spawn(unreal.CameraActor, "ShowcaseCam_%d" % i, eye, folder="Cameras")
        cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*eye), unreal.Vector(*target)), False)
        cam.tags = [unreal.Name("Showcase%d" % i)]
        setp(cam.camera_component, "field_of_view", 70.0)

    pp = spawn(unreal.PostProcessVolume, "GlobalPost", (0, 0, 0), folder="Lighting")
    pp.set_editor_property("unbound", True)
    s = pp.settings
    for name, value in (("auto_exposure_bias", 0.4), ("bloom_intensity", 0.8), ("vignette_intensity", 0.45),
                        ("film_grain_intensity", 0.08), ("ambient_occlusion_intensity", 0.7),
                        ("color_contrast", unreal.Vector4(1.0, 1.0, 1.0, 1.1))):
        try:
            s.set_editor_property("override_" + name, True)
            s.set_editor_property(name, value)
        except Exception as err:
            log("post skipped %s (%s)" % (name, err))
    pp.settings = s

    if not levels.save_current_level():
        raise RuntimeError("Could not save " + MAP_PATH)
    unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "RebuildNavigation")
    log("OK actors=%d" % len(actors.get_all_level_actors()))


try:
    build()
finally:
    if "-quit" in sys.argv:
        unreal.SystemLibrary.quit_editor()
