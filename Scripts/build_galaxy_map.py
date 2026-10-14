"""Builds /Game/Maps/Galaxy: the Terran Accord's strategic map of the Milky Way (the game's start screen).

1 unit = 1 light-year. The galactic centre (Sagittarius A*) is at the origin and Sol sits 26,000 ly out
in the Orion Spur at (-26000, 0, 25). Real objects are placed from their galactic longitude/latitude and
distance; stars within 100 ly of Sol are drawn 30x further out so the local bubble stays selectable
(distances shown in-game are the real ones).

Run headless (script path without spaces):
  UnrealEditor-Cmd.exe DeltaVDominion.uproject -ExecutePythonScript="<path>/build_galaxy_map.py -quit" -nullrhi -unattended
"""

import math
import sys

import unreal

MAP_PATH = "/Game/Maps/Galaxy"
SOL = unreal.Vector(-26000.0, 0.0, 25.0)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lib = unreal.EditorAssetLibrary

STAR = unreal.load_class(None, "/Script/DeltaVDominion.DVStarSystem")
FIELD = unreal.load_class(None, "/Script/DeltaVDominion.DVGalaxyStarfield")
MODE = unreal.load_class(None, "/Script/DeltaVDominion.DVGalaxyGameMode")

ACCORD, CONTESTED, CINDER, UNCHARTED = (unreal.DVControl.ACCORD, unreal.DVControl.CONTESTED,
                                         unreal.DVControl.CINDER, unreal.DVControl.UNCHARTED)


def galactic(d_ly, l_deg, b_deg):
    """Sol-relative galactic coordinates -> map position (x toward the core)."""
    shown = d_ly * 30.0 if d_ly < 100 else d_ly
    l, b = math.radians(l_deg), math.radians(b_deg)
    return unreal.Vector(SOL.x + shown * math.cos(b) * math.cos(l), SOL.y + shown * math.cos(b) * math.sin(l), SOL.z + shown * math.sin(b))


def op(name, location, map_path=""):
    o = unreal.DVOperation()
    o.set_editor_property("name", name)
    o.set_editor_property("location", location)
    o.set_editor_property("map", map_path)
    return o


def system(name, pos, region, distance, control, description, color=(1.0, 0.9, 0.7), size=2.0, ops=(), home=False):
    a = actors.spawn_actor_from_class(STAR, pos, unreal.Rotator())
    a.set_actor_label(name.replace(" ", "_").replace("*", "Star"))
    a.set_folder_path("Systems")
    a.set_editor_property("system_name", name)
    a.set_editor_property("region", region)
    a.set_editor_property("distance_ly", float(distance))
    a.set_editor_property("control", control)
    a.set_editor_property("description", description)
    a.set_editor_property("star_color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    a.set_editor_property("marker_size", size)
    a.set_editor_property("operations", list(ops))
    a.set_editor_property("home_system", home)
    return a


def label(text, pos):
    a = actors.spawn_actor_from_class(STAR, pos, unreal.Rotator())
    a.set_actor_label("Label_" + text.replace(" ", "_"))
    a.set_folder_path("Labels")
    a.set_editor_property("system_name", text)
    a.set_editor_property("region", "label")
    a.set_actor_hidden_in_game(True)
    a.set_editor_property("marker_size", 0.01)
    return a


def arm_point(arm, r, z=0.0):
    phases = [121.0, 211.0, 301.0, 31.0, 166.0]       # must match DVGalaxyStarfield.cpp
    t = math.radians(phases[arm]) + math.log(r / 4000.0) * 3.487
    return unreal.Vector(math.cos(t) * r, math.sin(t) * r, z)


def make_star_material():
    """/Game/Environment/Galaxy/M_Star: unlit, additive glow (stars and the core haze; used by DVGalaxyStarfield)."""
    path = "/Game/Environment/Galaxy/M_Star"
    if lib.does_asset_exist(path):
        if "-rebuild" not in sys.argv:
            return lib.load_asset(path)
        lib.delete_asset(path)
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset("M_Star", "/Game/Environment/Galaxy", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    em = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    em.set_editor_property("parameter_name", "Emissive")
    em.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    # Soft edge: brighter at the centre of each sphere, fading at the silhouette (Fresnel inverted).
    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, -500, 200)
    fres.set_editor_property("exponent", 1.5)
    inv = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -300, 200)
    mel.connect_material_expressions(fres, "", inv, "")
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 100)
    mel.connect_material_expressions(em, "", mul, "A")
    mel.connect_material_expressions(inv, "", mul, "B")
    # Fade out stars close to the camera so zooming into a system isn't a white-out (galaxy-scale stars are ~400 ly wide).
    depth = mel.create_material_expression(mat, unreal.MaterialExpressionPixelDepth, -500, 400)
    sub = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -350, 400)
    sub.set_editor_property("const_b", 300.0)
    mel.connect_material_expressions(depth, "", sub, "A")
    div = mel.create_material_expression(mat, unreal.MaterialExpressionDivide, -250, 400)
    div.set_editor_property("const_b", 1500.0)
    mel.connect_material_expressions(sub, "", div, "A")
    sat = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, -150, 400)
    mel.connect_material_expressions(div, "", sat, "")
    fade = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, 0, 200)
    mel.connect_material_expressions(mul, "", fade, "A")
    mel.connect_material_expressions(sat, "", fade, "B")
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Keep stars a sensible size at every zoom: shrink each instance toward its centre as the camera
    # approaches (factor = clamp(distance / 40000, 0.02, 1)), via world position offset.
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -700, 700)
    op = mel.create_material_expression(mat, unreal.MaterialExpressionObjectPositionWS, -700, 820)
    cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -700, 940)
    rel = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -500, 720)
    mel.connect_material_expressions(wp, "", rel, "A")
    mel.connect_material_expressions(op, "", rel, "B")
    dist = mel.create_material_expression(mat, unreal.MaterialExpressionDistance, -500, 880)
    mel.connect_material_expressions(op, "", dist, "A")
    mel.connect_material_expressions(cam, "", dist, "B")
    k = mel.create_material_expression(mat, unreal.MaterialExpressionDivide, -380, 880)
    k.set_editor_property("const_b", 40000.0)
    mel.connect_material_expressions(dist, "", k, "A")
    clamp = mel.create_material_expression(mat, unreal.MaterialExpressionClamp, -260, 880)
    clamp.set_editor_property("min_default", 0.02)
    clamp.set_editor_property("max_default", 1.0)
    mel.connect_material_expressions(k, "", clamp, "")
    minus1 = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -150, 880)
    minus1.set_editor_property("const_b", -1.0)
    mel.connect_material_expressions(clamp, "", minus1, "A")
    offset = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -50, 760)
    mel.connect_material_expressions(rel, "", offset, "A")
    mel.connect_material_expressions(minus1, "", offset, "B")
    mel.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat


def build():
    make_star_material()
    if lib.does_asset_exist(MAP_PATH):
        levels.load_level(MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors() if not isinstance(a, unreal.WorldSettings)])
    elif not levels.new_level(MAP_PATH):
        raise RuntimeError("Could not create " + MAP_PATH)

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", MODE)

    field = actors.spawn_actor_from_class(FIELD, unreal.Vector(0, 0, 0), unreal.Rotator())
    field.set_actor_label("MilkyWay")

    # --- Home --------------------------------------------------------------------------------
    system("Sol", SOL, "Orion Spur", 0, ACCORD,
           "Gaia, cradle of humanity and capital of the Terran Accord. Mars has fallen to CINDER's foundries; "
           "the TAS Kestrel fights to retake the inner system before the machines reach Gaia itself.",
           (1.0, 0.92, 0.7), 3.0,
           [op("Kestrel (home base)", "TAS Kestrel, hangar deck", "/Game/Maps/Kestrel"),
            op("Breakwater", "Port Halden harbour, Gaia", "/Game/Maps/Breakwater"),
            op("Red Tide", "Tharsis Ridge, Mars", "/Game/Maps/Outpost"),
            op("Glass Ocean", "Conamara Chaos, Europa", "/Game/Maps/Europa"),
            op("Forgefall", "Loki Patera, Io", "/Game/Maps/Io"),
            op("Ghost Signal", "Occator District, Ceres", "/Game/Maps/Ceres"),
            op("Gaia Falls", "Lagos Arcology outskirts, Gaia", "/Game/Maps/Gaia"),
            op("Boot Camp", "Copernicus Range, Luna", "/Game/Maps/BootCamp")], home=True)

    # --- Local bubble (real data; drawn magnified) -------------------------------------------
    system("Alpha Centauri", galactic(4.37, 315.7, -0.7), "Local Bubble", 4.37, CONTESTED,
           "The Accord's forward shipyards at Rigil and the Proxima colony. CINDER-controlled freighters went dark on the approach two days ago.",
           (1.0, 0.85, 0.6), 1.4, [op("Cold Wake", "New Albion Colony, Proxima b", "/Game/Maps/Proxima")])
    system("Barnard's Star", galactic(5.96, 31.0, 14.1), "Local Bubble", 5.96, CINDER,
           "Automated mining relay seized in the first hour of the uprising. Skitter drones roll off its lines by the thousand.",
           (1.0, 0.45, 0.25), 1.0)
    system("Sirius", galactic(8.6, 227.2, -8.9), "Local Bubble", 8.6, ACCORD,
           "Twin-star fleet anchorage. What remains of the Accord's Second Fleet is regrouping here.", (0.75, 0.85, 1.0), 1.6)
    system("Epsilon Eridani", galactic(10.5, 195.8, -48.1), "Local Bubble", 10.5, CONTESTED,
           "Young system wrapped in a debris ring. Hollow Men insurgents hide in the belt and raid Accord convoys.",
           (1.0, 0.7, 0.45), 1.1, [op("Dust Choir", "Eridani outer belt")])
    system("Tau Ceti", galactic(11.9, 173.1, -73.4), "Local Bubble", 11.9, CONTESTED,
           "Agricultural worlds that feed half the Accord. CINDER's harvesters have turned on the fields.", (1.0, 0.85, 0.55), 1.2)
    system("TRAPPIST-1", galactic(40.7, 69.7, -56.6), "Local Bubble", 40.7, UNCHARTED,
           "Seven worlds and one silent research station. Dr. Liang Morrow's old lab sent its last message from here.",
           (1.0, 0.35, 0.2), 1.0, [op("The Architect", "Morrow Station, TRAPPIST-1e")])

    # --- The wider galaxy ----------------------------------------------------------------------
    system("Pleiades", galactic(444, 166.6, -23.5), "Orion Spur", 444, UNCHARTED,
           "The Seven Sisters. Accord survey ships map colony targets among the young blue stars.", (0.6, 0.75, 1.0), 2.4)
    system("Orion Nebula", galactic(1344, 209.0, -19.4), "Orion Spur", 1344, UNCHARTED,
           "The nearest great stellar nursery. Long-range scans show structured signals inside the gas: something is building.",
           (1.0, 0.45, 0.65), 4.0)
    system("Kepler-452", galactic(1800, 74.4, 14.1), "Orion Spur", 1800, UNCHARTED,
           "Gaia's older cousin: a temperate super-Earth. The Accord's long-range colony ark was bound here when the war began.", (1.0, 0.9, 0.65), 1.8)
    system("Crab Nebula", galactic(6500, 184.6, -5.8), "Perseus Arm", 6500, UNCHARTED,
           "Remnant of the supernova of 1054. Its pulsar beats like a clock; CINDER's transmissions sync to it.", (0.7, 0.8, 1.0), 3.2)
    system("Cygnus X-1", galactic(7200, 71.3, 3.1), "Orion Spur", 7200, UNCHARTED,
           "A black hole feeding on a blue supergiant. Ships that chart its accretion disk do not report back.", (0.6, 0.7, 1.0), 2.6)
    system("Eagle Nebula", galactic(7000, 17.0, 0.8), "Sagittarius Arm", 7000, CINDER,
           "The Pillars of Creation. CINDER foundry swarms have been sighted here. That should be impossible without faster-than-light drives.",
           (1.0, 0.5, 0.4), 3.6)
    system("Carina Nebula", galactic(8500, 287.6, -0.6), "Sagittarius Arm", 8500, UNCHARTED,
           "Home of Eta Carinae, a star on the edge of detonation. The richest, most dangerous mining region on any chart.", (1.0, 0.55, 0.7), 3.6)
    system("Omega Centauri", galactic(17090, 309.1, 15.0), "Galactic Halo", 17090, UNCHARTED,
           "Ten million stars in one ancient cluster, possibly the core of a devoured dwarf galaxy.", (1.0, 0.8, 0.5), 3.4)
    system("Sagittarius A*", unreal.Vector(0, 0, 0), "Galactic Core", 26000, UNCHARTED,
           "The supermassive black hole at the heart of the Milky Way. CINDER is not answering Earth. It is answering a signal from here.",
           (1.0, 0.95, 0.85), 5.0, [op("Dominion", "Event horizon, Sagittarius A*")])

    # --- Region labels ------------------------------------------------------------------------
    label("Galactic Core", unreal.Vector(0, 0, 4200))
    label("Perseus Arm", arm_point(0, 38000, 600))
    label("Sagittarius Arm", arm_point(1, 20000, 600))
    label("Norma Arm", arm_point(2, 15000, 600))
    label("Scutum-Centaurus Arm", arm_point(3, 30000, 600))
    label("Orion Spur", unreal.Vector(SOL.x - 2500, SOL.y + 4000, 1400))
    label("Outer Arm", arm_point(1, 47000, 600))

    # --- Look: pure black space, glow from emissive stars ----------------------------------------
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), unreal.Rotator())
    pp.set_actor_label("SpacePost")
    pp.set_editor_property("unbound", True)
    s = pp.settings

    def setpp(name, value):
        try:
            s.set_editor_property("override_" + name, True)
            s.set_editor_property(name, value)
        except Exception as err:
            unreal.log("GALAXY skipped %s (%s)" % (name, err))

    # Locked exposure (min = max) so the galaxy never "adapts" to black space.
    setpp("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM)
    setpp("auto_exposure_min_brightness", 1.5)
    setpp("auto_exposure_max_brightness", 1.5)
    setpp("auto_exposure_bias", 0.0)
    setpp("bloom_intensity", 1.8)
    setpp("bloom_threshold", 0.6)
    setpp("lens_flare_intensity", 0.0)
    setpp("vignette_intensity", 0.6)
    setpp("film_grain_intensity", 0.08)
    setpp("motion_blur_amount", 0.0)
    setpp("ambient_occlusion_intensity", 0.0)
    pp.settings = s

    if not levels.save_current_level():
        raise RuntimeError("Could not save " + MAP_PATH)
    unreal.log("GALAXY OK actors=%d" % len(actors.get_all_level_actors()))


try:
    build()
finally:
    if "-quit" in sys.argv:
        unreal.SystemLibrary.quit_editor()
