"""Builds the playable planets: one map per biome, each a Hearthbound strongpoint to assault.

  mars     /Game/Maps/Outpost   Red Tide       Tharsis Ridge at dusk (rust dunes, mesas)
  europa   /Game/Maps/Europa    Glass Ocean    Jupiter's ice moon: blue ice cliffs, black sky, Jupiter overhead
  io       /Game/Maps/Io        Forgefall      Volcanic moon: black basalt, sulphur haze, rivers of lava
  proxima  /Game/Maps/Proxima   Cold Wake      Proxima b colony: red-dwarf light over forested hills
  ceres    /Game/Maps/Ceres     Ghost Signal   Night mining town on Ceres, drowned in neon
  breakwater /Game/Maps/Breakwater Breakwater  Port Halden on Gaia at dawn: a harbour strait, docks, transports, a missile boat
  gaia     /Game/Maps/Gaia      Gaia Falls     Earth: green hills outside the capital, skyline on the horizon

Everything is generated: Nanite terrain and rocks (GeometryScript), PBR materials, a fortified compound
kit-bashed from engine meshes, the Accord landing zone with vehicles, the Kestrel overhead, biome extras,
and cinematic lighting/post. Run headless (script path without spaces):
  UnrealEditor-Cmd.exe DeltaVDominion.uproject -ExecutePythonScript="<path>/build_planet_map.py -biome=all -quit" -nullrhi -unattended
-biome=<name> builds one planet; -rebuild regenerates meshes and materials.

Units are cm. +X points from the player's start (west) toward the compound (east).
"""

import math
import random
import sys

import unreal

REBUILD = "-rebuild" in sys.argv
SHARED = "/Game/Environment/Outpost"     # shared structure materials (also used by the C++ vehicle kit)

# Per-planet look and layout. Colours are linear RGB.
BIOMES = {
    "mars": dict(map="/Game/Maps/Outpost", env="/Game/Environment/Outpost", seed=0, dunes=90.0, rim=900.0, mesa=2600.0,
                 sand_a=(0.26, 0.155, 0.09), sand_b=(0.085, 0.06, 0.045), slope_rock=(0.05, 0.042, 0.036),
                 rock_dark=(0.022, 0.02, 0.018), rock_light=(0.12, 0.095, 0.075),
                 sun=(-21, 38), sun_color=(255, 196, 150), sun_lux=9.0, atmosphere=True, mie=0.008, sky_light=1.2,
                 fog=0.012, fog_color=(0.35, 0.22, 0.16), clouds=True, saturation=1.08, contrast=1.15, exposure=-0.2,
                 extras=[]),
    "europa": dict(map="/Game/Maps/Europa", env="/Game/Environment/Planets/Europa", seed=21, dunes=60.0, rim=1400.0, mesa=3600.0,
                   sand_a=(0.62, 0.7, 0.78), sand_b=(0.34, 0.42, 0.52), slope_rock=(0.16, 0.24, 0.32),
                   rock_dark=(0.1, 0.15, 0.2), rock_light=(0.55, 0.66, 0.78),
                   sun=(-12, 150), sun_color=(230, 238, 255), sun_lux=6.0, atmosphere=False, mie=0.0, sky_light=0.8,
                   fog=0.004, fog_color=(0.25, 0.35, 0.5), clouds=False, saturation=0.95, contrast=1.2, exposure=0.2,
                   extras=["gas_giant"]),
    "io": dict(map="/Game/Maps/Io", env="/Game/Environment/Planets/Io", seed=31, dunes=110.0, rim=1000.0, mesa=2200.0,
               sand_a=(0.42, 0.36, 0.07), sand_b=(0.06, 0.055, 0.05), slope_rock=(0.025, 0.022, 0.02),
               rock_dark=(0.018, 0.016, 0.015), rock_light=(0.09, 0.075, 0.05),
               sun=(-30, 60), sun_color=(255, 232, 196), sun_lux=7.0, atmosphere=True, mie=0.03, sky_light=1.0,
               fog=0.03, fog_color=(0.55, 0.32, 0.1), clouds=False, saturation=1.15, contrast=1.2, exposure=0.0,
               extras=["lava", "gas_giant"]),
    "proxima": dict(map="/Game/Maps/Proxima", env="/Game/Environment/Planets/Proxima", seed=41, dunes=160.0, rim=800.0, mesa=2000.0,
                    sand_a=(0.11, 0.17, 0.05), sand_b=(0.06, 0.09, 0.035), slope_rock=(0.09, 0.08, 0.07),
                    rock_dark=(0.045, 0.045, 0.04), rock_light=(0.2, 0.19, 0.17),
                    sun=(-32, 20), sun_color=(255, 135, 85), sun_lux=7.5, atmosphere=True, mie=0.02, sky_light=1.1,
                    fog=0.02, fog_color=(0.35, 0.2, 0.18), clouds=True, saturation=1.05, contrast=1.1, exposure=0.0,
                    extras=["trees"]),
    "ceres": dict(map="/Game/Maps/Ceres", env="/Game/Environment/Planets/Ceres", seed=51, dunes=70.0, rim=1200.0, mesa=3000.0,
                  sand_a=(0.16, 0.16, 0.17), sand_b=(0.06, 0.06, 0.065), slope_rock=(0.03, 0.03, 0.035),
                  rock_dark=(0.02, 0.02, 0.022), rock_light=(0.12, 0.12, 0.13),
                  sun=(-8, 200), sun_color=(170, 185, 255), sun_lux=1.2, atmosphere=False, mie=0.0, sky_light=0.4,
                  fog=0.004, fog_color=(0.05, 0.07, 0.15), clouds=False, saturation=1.0, contrast=1.15, exposure=0.5,
                  extras=["neon"]),
    "gaia": dict(map="/Game/Maps/Gaia", env="/Game/Environment/Planets/Gaia", seed=61, dunes=80.0, rim=600.0, mesa=1800.0,
                 sand_a=(0.13, 0.17, 0.06), sand_b=(0.15, 0.115, 0.075), slope_rock=(0.12, 0.11, 0.1),
                 rock_dark=(0.05, 0.05, 0.048), rock_light=(0.22, 0.21, 0.2),
                 sun=(-40, 30), sun_color=(255, 245, 230), sun_lux=10.0, atmosphere=True, mie=0.004, sky_light=1.2,
                 fog=0.01, fog_color=(0.4, 0.45, 0.55), clouds=True, saturation=1.05, contrast=1.1, exposure=0.0,
                 extras=["city", "trees"]),
    "breakwater": dict(map="/Game/Maps/Breakwater", env="/Game/Environment/Planets/Breakwater", seed=71, dunes=60.0, rim=700.0, mesa=1600.0,
                       sand_a=(0.32, 0.29, 0.22), sand_b=(0.16, 0.15, 0.12), slope_rock=(0.13, 0.12, 0.11),
                       rock_dark=(0.06, 0.06, 0.06), rock_light=(0.24, 0.23, 0.21),
                       sun=(-9, 95), sun_color=(255, 196, 140), sun_lux=8.0, atmosphere=True, mie=0.012, sky_light=1.1,
                       fog=0.008, fog_color=(0.42, 0.44, 0.48), clouds=True, saturation=1.05, contrast=1.1, exposure=-0.6,
                       extras=["harbour", "city"]),
    # Rook's Rest: a Free Ports market town on the Tharsis rim at golden hour. No enemies; a hub between operations.
    "rooksrest": dict(map="/Game/Maps/RooksRest", env="/Game/Environment/Planets/RooksRest", seed=81, dunes=70.0, rim=1100.0, mesa=3000.0,
                      sand_a=(0.3, 0.17, 0.09), sand_b=(0.1, 0.065, 0.045), slope_rock=(0.06, 0.045, 0.036),
                      rock_dark=(0.025, 0.02, 0.018), rock_light=(0.14, 0.1, 0.075),
                      sun=(-8, 200), sun_color=(255, 170, 110), sun_lux=7.0, atmosphere=True, mie=0.012, sky_light=1.1,
                      fog=0.006, fog_color=(0.42, 0.24, 0.15), clouds=True, saturation=1.1, contrast=1.12, exposure=-0.1,
                      extras=["town"]),
    # Ironstride: Arcadia Planitia, an open plain under a dust-storm sky, big enough for a 30 m walker to stride a loop.
    "ironstride": dict(map="/Game/Maps/Ironstride", env="/Game/Environment/Planets/Ironstride", seed=91, dunes=60.0, rim=900.0, mesa=2400.0,
                       sand_a=(0.28, 0.15, 0.08), sand_b=(0.11, 0.07, 0.05), slope_rock=(0.06, 0.045, 0.035),
                       rock_dark=(0.025, 0.02, 0.018), rock_light=(0.13, 0.095, 0.07),
                       sun=(-34, 60), sun_color=(255, 205, 165), sun_lux=7.5, atmosphere=True, mie=0.03, sky_light=1.2,
                       fog=0.02, fog_color=(0.45, 0.27, 0.17), clouds=True, saturation=1.05, contrast=1.12, exposure=-0.2,
                       extras=["walker"]),
    # Brasshaven (Wolf 359 b): a brick-and-brass city in a smoky canyon, cut off from the Lattice for eighty years.
    "brasshaven": dict(map="/Game/Maps/Brasshaven", env="/Game/Environment/Planets/Brasshaven", seed=101, dunes=40.0, rim=2400.0, mesa=5200.0,
                       sand_a=(0.13, 0.085, 0.055), sand_b=(0.06, 0.042, 0.032), slope_rock=(0.2, 0.085, 0.045),
                       rock_dark=(0.06, 0.03, 0.02), rock_light=(0.24, 0.12, 0.07),
                       sun=(-14, 235), sun_color=(255, 178, 110), sun_lux=6.5, atmosphere=True, mie=0.045, sky_light=1.0,
                       fog=0.028, fog_color=(0.5, 0.31, 0.16), clouds=True, saturation=1.02, contrast=1.15, exposure=-0.15,
                       extras=["brass"]),
}
B = BIOMES["mars"]
MAP_PATH = B["map"]
ENV = B["env"]


def use_biome(name):
    global B, MAP_PATH, ENV
    B = BIOMES[name]
    MAP_PATH = B["map"]
    ENV = B["env"]
    random.seed(7 + B["seed"])

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
gs = unreal

random.seed(7)


def log(msg):
    unreal.log("PLANET " + msg)


def setp(obj, name, value):
    """set_editor_property that logs instead of failing when a property was renamed in this engine version."""
    try:
        obj.set_editor_property(name, value)
    except Exception as err:
        log("skipped %s.%s (%s)" % (type(obj).__name__, name, err))


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


# --------------------------------------------------------------------------------------------
# Noise
# --------------------------------------------------------------------------------------------

def _hash(ix, iy, seed):
    h = (ix * 374761393 + iy * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def value_noise(x, y, seed=0):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = _hash(ix, iy, seed), _hash(ix + 1, iy, seed)
    c, d = _hash(ix, iy + 1, seed), _hash(ix + 1, iy + 1, seed)
    return (a + (b - a) * sx) + ((c + (d - c) * sx) - (a + (b - a) * sx)) * sy


def fbm(x, y, octaves=5, seed=0):
    total, amp, freq, norm = 0.0, 1.0, 1.0, 0.0
    for o in range(octaves):
        total += amp * value_noise(x * freq, y * freq, seed + o * 17)
        norm += amp
        amp *= 0.5
        freq *= 2.03
    return total / norm


def ridged(x, y, octaves=4, seed=0):
    total, amp, freq, norm = 0.0, 1.0, 1.0, 0.0
    for o in range(octaves):
        n = 1.0 - abs(value_noise(x * freq, y * freq, seed + o * 31) * 2.0 - 1.0)
        total += amp * n * n
        norm += amp
        amp *= 0.5
        freq *= 2.1
    return total / norm


def smoothstep(e0, e1, v):
    t = max(0.0, min(1.0, (v - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def terrain_height(x, y):
    """Flat-ish battlefield in an oval basin, ringed by ridged mesas."""
    q = math.sqrt((x / 5600.0) ** 2 + (y / 4300.0) ** 2)
    sd = B["seed"]
    ground = (fbm(x / 2400.0, y / 2400.0, 4, 3 + sd) - 0.5) * B["dunes"]          # gentle relief in the arena
    rim = smoothstep(1.0, 1.7, q)
    mesa = ridged(x / 9000.0, y / 9000.0, 4, 11 + sd)
    mountains = rim * (B["rim"] + B["mesa"] * mesa) + rim * (fbm(x / 1500.0, y / 1500.0, 5, 5 + sd) - 0.5) * 600.0
    h = ground + mountains
    if "harbour" in B["extras"]:
        # A deep strait runs north to south through the arena: the harbour, open to the sea at both ends.
        h -= 780.0 * (1.0 - smoothstep(850.0, 1250.0, abs(x)))
    if "brass" in B["extras"]:
        # The city floor is level; the canyon walls rise sheer past the last streets.
        h *= smoothstep(5200.0, 6800.0, math.sqrt(x * x + y * y * 1.5))
    if "town" in B["extras"]:
        # The town sits on a levelled plaza; the land rises back into dunes and mesas past the last buildings.
        h *= smoothstep(4400.0, 6000.0, math.sqrt(x * x + y * y * 1.6))
    if "lava" in B["extras"]:
        # Two meandering channels cut across the arena flanks; the lava plane fills them.
        for offset in (-2600.0, 2900.0):
            centre = offset + math.sin(x / 1700.0 + offset) * 700.0
            d = abs(y - centre)
            h -= 380.0 * (1.0 - smoothstep(250.0, 650.0, d)) * (1.0 - smoothstep(4800.0, 6200.0, abs(x)))
    return h


# --------------------------------------------------------------------------------------------
# Materials
# --------------------------------------------------------------------------------------------

def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def const3(mat, rgb, x, y):
    return expr(mat, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def world_uv(mat, scale, x, y):
    """WorldPosition.xy / scale, as a UV."""
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, x, y)
    mask = expr(mat, unreal.MaterialExpressionComponentMask, x + 150, y, r=True, g=True, b=False, a=False)
    mel.connect_material_expressions(wp, "", mask, "")
    div = expr(mat, unreal.MaterialExpressionDivide, x + 300, y, const_b=scale)
    mel.connect_material_expressions(mask, "", div, "A")
    return div


def tex(mat, path, uv, x, y, normal=False):
    t = expr(mat, unreal.MaterialExpressionTextureSample, x, y, texture=lib.load_asset(path))
    if normal:
        t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_expressions(uv, "", t, "UVs")
    return t


def new_material(name):
    path = ENV + "/Materials/" + name
    if lib.does_asset_exist(path):
        if not REBUILD:
            return lib.load_asset(path), False
        lib.delete_asset(path)
    return tools.create_asset(name, ENV + "/Materials", unreal.Material, unreal.MaterialFactoryNew()), True


def make_terrain_material():
    mat, fresh = new_material("M_MoonGround")
    if not fresh:
        return mat
    # Macro colour: two dusts blended by large-scale noise, then darkened rock on steep slopes.
    wp_macro = expr(mat, unreal.MaterialExpressionWorldPosition, -1600, -300)
    big = expr(mat, unreal.MaterialExpressionNoise, -1300, -300, scale=0.0009, levels=5, output_min=0.0, output_max=1.0,
               noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU, turbulence=False)
    mel.connect_material_expressions(wp_macro, "", big, "Position")
    sand_a = const3(mat, B["sand_a"], -1200, -560)
    sand_b = const3(mat, B["sand_b"], -1200, -440)
    l1 = expr(mat, unreal.MaterialExpressionLinearInterpolate, -900, -480)
    mel.connect_material_expressions(sand_a, "", l1, "A")
    mel.connect_material_expressions(sand_b, "", l1, "B")
    contrast = expr(mat, unreal.MaterialExpressionPower, -1000, -300, const_exponent=1.6)
    mel.connect_material_expressions(big, "", contrast, "Base")
    mel.connect_material_expressions(contrast, "", l1, "Alpha")

    normal_ws = expr(mat, unreal.MaterialExpressionVertexNormalWS, -1600, 100)
    up = expr(mat, unreal.MaterialExpressionComponentMask, -1450, 100, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(normal_ws, "", up, "")
    slope = expr(mat, unreal.MaterialExpressionPower, -1300, 100, const_exponent=6.0)
    mel.connect_material_expressions(up, "", slope, "Base")
    rock = const3(mat, B["slope_rock"], -1100, 0)
    l2 = expr(mat, unreal.MaterialExpressionLinearInterpolate, -700, -300)
    mel.connect_material_expressions(rock, "", l2, "A")
    mel.connect_material_expressions(l1, "", l2, "B")
    mel.connect_material_expressions(slope, "", l2, "Alpha")

    # Fine detail: concrete grain modulates albedo slightly.
    fine_uv = world_uv(mat, 220.0, -1600, 400)
    fine = tex(mat, "/Game/Building/Textures/T_PolishedConcrete01_BC", fine_uv, -1200, 400)
    detail = expr(mat, unreal.MaterialExpressionMultiply, -500, -200)
    mel.connect_material_expressions(l2, "", detail, "A")
    lift = expr(mat, unreal.MaterialExpressionAdd, -800, 400, const_b=0.55)
    mel.connect_material_expressions(fine, "R", lift, "A")
    mel.connect_material_expressions(lift, "", detail, "B")
    mel.connect_material_property(detail, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Normals: two world-aligned layers.
    n_uv = world_uv(mat, 160.0, -1600, 700)
    n1 = tex(mat, "/Game/Building/Textures/T_Noise01_N", n_uv, -1200, 700, normal=True)
    n_uv2 = world_uv(mat, 45.0, -1600, 950)
    n2 = tex(mat, "/Game/Building/Textures/T_FineConcrete01_N", n_uv2, -1200, 950, normal=True)
    blend = expr(mat, unreal.MaterialExpressionAdd, -800, 800)
    mel.connect_material_expressions(n1, "RGB", blend, "A")
    mel.connect_material_expressions(n2, "RGB", blend, "B")
    flat = expr(mat, unreal.MaterialExpressionMultiply, -600, 800)
    mel.connect_material_expressions(blend, "", flat, "A")
    flat.set_editor_property("const_b", 0.6)
    mel.connect_material_property(flat, "", unreal.MaterialProperty.MP_NORMAL)

    rough = expr(mat, unreal.MaterialExpressionConstant, -500, 300, r=0.92)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat


def make_rock_material():
    mat, fresh = new_material("M_Rock")
    if not fresh:
        return mat
    uv = world_uv(mat, 380.0, -1400, -200)
    grain = tex(mat, "/Game/Building/Textures/T_PolishedConcrete01_BC", uv, -1000, -200)
    dark = const3(mat, B["rock_dark"], -1000, -450)
    light = const3(mat, B["rock_light"], -1000, -330)
    l = expr(mat, unreal.MaterialExpressionLinearInterpolate, -700, -350)
    mel.connect_material_expressions(dark, "", l, "A")
    mel.connect_material_expressions(light, "", l, "B")
    mel.connect_material_expressions(grain, "R", l, "Alpha")
    mel.connect_material_property(l, "", unreal.MaterialProperty.MP_BASE_COLOR)
    n_uv = world_uv(mat, 45.0, -1400, 200)
    n = tex(mat, "/Game/Building/Textures/T_Noise01_N", n_uv, -1000, 200, normal=True)
    mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    rough = expr(mat, unreal.MaterialExpressionConstant, -700, 0, r=0.8)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat


def tinted(parent_path, name, tint, metallic=None, roughness=None, uv_scale=None):
    path = SHARED + "/Materials/" + name
    if lib.does_asset_exist(path) and not REBUILD:
        return lib.load_asset(path)
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    mi = tools.create_asset(name, SHARED + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", lib.load_asset(parent_path))
    mel.set_material_instance_vector_parameter_value(mi, "TintColor", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    if metallic is not None:
        mel.set_material_instance_scalar_parameter_value(mi, "MetallicAmount", metallic)
    if roughness is not None:
        mel.set_material_instance_scalar_parameter_value(mi, "RoughnessAmount", roughness)
    if uv_scale is not None:
        mel.set_material_instance_vector_parameter_value(mi, "UVOffsetAndScale", unreal.LinearColor(0, 0, uv_scale, uv_scale))
    lib.save_loaded_asset(mi)
    return mi


def glow(name, color, strength):
    path = SHARED + "/Materials/" + name
    if lib.does_asset_exist(path) and not REBUILD:
        return lib.load_asset(path)
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    mi = tools.create_asset(name, SHARED + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", lib.load_asset("/Game/Dev/M_GreyBox"))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
    mel.set_material_instance_vector_parameter_value(mi, "Emissive", unreal.LinearColor(color[0] * strength, color[1] * strength, color[2] * strength, 1.0))
    lib.save_loaded_asset(mi)
    return mi


# --------------------------------------------------------------------------------------------
# Meshes (GeometryScript)
# --------------------------------------------------------------------------------------------

def save_mesh(dm, name, nanite=True):
    path = ENV + "/Meshes/" + name
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    opts.set_editor_property("enable_nanite", nanite)
    opts.set_editor_property("enable_collision", True)
    mesh, outcome = gs.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm, path, opts)
    if mesh is None:
        raise RuntimeError("Failed to create " + path)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    lib.save_loaded_asset(mesh)
    return mesh


def recompute(dm):
    gs.GeometryScript_Normals.recompute_normals(dm, unreal.GeometryScriptCalculateNormalsOptions())


def make_terrain():
    path = ENV + "/Meshes/SM_MoonTerrain"
    if lib.does_asset_exist(path) and not REBUILD:
        return lib.load_asset(path)
    dm = unreal.DynamicMesh()
    size, steps = 60000.0, 300
    gs.GeometryScript_Primitives.append_rectangle_xy(dm, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), size, size, steps, steps)
    _, positions, _ = gs.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)
    pts = list(gs.GeometryScript_List.convert_vector_list_to_array(positions))
    for i, p in enumerate(pts):
        pts[i] = unreal.Vector(p.x, p.y, terrain_height(p.x, p.y))
    gs.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(dm, gs.GeometryScript_List.convert_array_to_vector_list(pts))
    recompute(dm)
    plane = unreal.Transform(scale=unreal.Vector(400.0, 400.0, 1.0))
    gs.GeometryScript_UVs.set_mesh_u_vs_from_planar_projection(dm, 0, plane, unreal.GeometryScriptMeshSelection())
    log("terrain verts %d" % len(pts))
    return save_mesh(dm, "SM_MoonTerrain")


def make_rock(index):
    name = "SM_Rock_%02d" % index
    path = ENV + "/Meshes/" + name
    if lib.does_asset_exist(path) and not REBUILD:
        return lib.load_asset(path)
    dm = unreal.DynamicMesh()
    gs.GeometryScript_Primitives.append_sphere_lat_long(dm, unreal.GeometryScriptPrimitiveOptions(), unreal.Transform(), 100.0, 40, 56)
    _, positions, _ = gs.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)
    pts = list(gs.GeometryScript_List.convert_vector_list_to_array(positions))
    seed = 100 + index * 13
    rng = random.Random(seed)
    # A convex, faceted core (the intersection of random half-spaces) reads as fractured stone,
    # then two noise layers break up the flat faces.
    planes = []
    for k in range(14):
        z = rng.uniform(-0.4, 1.0)
        a = rng.uniform(0, 2 * math.pi)
        r = math.sqrt(max(0.0, 1 - z * z))
        planes.append(((r * math.cos(a), r * math.sin(a), z), rng.uniform(0.62, 0.95)))
    planes.append(((0.0, 0.0, -1.0), 0.35))          # flat base
    stretch = (rng.uniform(1.0, 1.6), rng.uniform(0.8, 1.2), rng.uniform(0.55, 0.9))
    for i, p in enumerate(pts):
        length = math.sqrt(p.x * p.x + p.y * p.y + p.z * p.z) or 1.0
        nx, ny, nz = p.x / length, p.y / length, p.z / length
        r = 1.0
        for (px, py, pz), d in planes:
            dp = nx * px + ny * py + nz * pz
            if dp > 1e-3:
                r = min(r, d / dp)
        n = (fbm(nx * 3.0 + seed, ny * 3.0 + nz * 2.0, 3, seed) - 0.5) * 0.12 + (ridged(nx * 9.0 + nz * 3.0, ny * 9.0 - nz * 2.0, 2, seed + 1) - 0.5) * 0.05
        r = 100.0 * r * (1.0 + n)
        pts[i] = unreal.Vector(nx * r * stretch[0], ny * r * stretch[1], nz * r * stretch[2])
    gs.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(dm, gs.GeometryScript_List.convert_array_to_vector_list(pts))
    recompute(dm)
    gs.GeometryScript_UVs.set_mesh_u_vs_from_box_projection(dm, 0, unreal.Transform(scale=unreal.Vector(2.0, 2.0, 2.0)), unreal.GeometryScriptMeshSelection())
    return save_mesh(dm, name)


# --------------------------------------------------------------------------------------------
# Placement helpers
# --------------------------------------------------------------------------------------------

CHAMFER = None
CHAMFER_SIZE = None


def place(mesh, label, location, rotation=None, scale=(1, 1, 1), material=None, folder="Environment"):
    a = actors.spawn_actor_from_object(mesh, unreal.Vector(*location), rotation or rot())
    a.set_actor_label(label)
    a.set_folder_path(folder)
    a.set_actor_scale3d(unreal.Vector(*scale))
    if material:
        comp = a.static_mesh_component
        for i in range(comp.get_num_materials()):
            comp.set_material(i, material)
    return a


def block(label, centre, size, material, yaw=0.0, folder="Compound"):
    """Chamfered box with its centre at `centre` and the given size in cm."""
    sx, sy, sz = size[0] / CHAMFER_SIZE.x, size[1] / CHAMFER_SIZE.y, size[2] / CHAMFER_SIZE.z
    a = place(CHAMFER, label, (0, 0, 0), rot(yaw=yaw), (sx, sy, sz), material, folder)
    # Re-centre whatever the mesh pivot is.
    origin, extent = a.get_actor_bounds(False)
    loc = a.get_actor_location()
    a.set_actor_location(unreal.Vector(centre[0] + loc.x - origin.x, centre[1] + loc.y - origin.y, centre[2] + loc.z - origin.z), False, False)
    return a


def ground(x, y):
    return terrain_height(x, y)


def spawn(cls, label, location, rotation=None, folder="Gameplay"):
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*location), rotation or rot())
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def floodlight(label, x, y, yaw, mats, height=650.0):
    gz = ground(x, y)
    block(label + "_Pole", (x, y, gz + height / 2), (22, 22, height), mats["gunmetal"], folder="Lights")
    block(label + "_Head", (x, y, gz + height), (60, 120, 35), mats["gunmetal"], yaw, folder="Lights")
    block(label + "_Lens", (x + math.cos(math.radians(yaw)) * 32, y + math.sin(math.radians(yaw)) * 32, gz + height - 12), (6, 100, 20), mats["lamp"], yaw, folder="Lights")
    s = spawn(unreal.SpotLight, label, (x, y, gz + height - 20), rot(pitch=-32, yaw=yaw), "Lights")
    lc = s.spot_light_component
    setp(lc, "intensity_units", unreal.LightUnits.CANDELAS)
    setp(lc, "intensity", 9000.0)
    setp(lc, "light_color", unreal.Color(r=255, g=205, b=150, a=255))
    setp(lc, "attenuation_radius", 5500.0)
    setp(lc, "outer_cone_angle", 34.0)
    setp(lc, "inner_cone_angle", 18.0)
    setp(lc, "volumetric_scattering_intensity", 3.0)
    setp(lc, "mobility", unreal.ComponentMobility.MOVABLE)


def container(label, x, y, yaw, material, mats, stacked=False):
    gz = ground(x, y)
    z = gz + 130 + (260 if stacked else 0)
    block(label, (x, y, z), (600, 244, 260), material, yaw, folder="Containers")
    # Corrugation ribs and door frame give the silhouette some detail.
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for i in range(-5, 6):
        ox, oy = c * i * 52, s * i * 52
        block(label + "_RibA%d" % (i + 5), (x + ox - s * 124, y + oy + c * 124, z), (14, 6, 240), material, yaw, folder="Containers")
        block(label + "_RibB%d" % (i + 5), (x + ox + s * 124, y + oy - c * 124, z), (14, 6, 240), material, yaw, folder="Containers")
    block(label + "_Door", (x + c * 302, y + s * 302, z), (8, 236, 252), mats["gunmetal"], yaw, folder="Containers")


def jersey(label, x, y, yaw, mats):
    gz = ground(x, y)
    block(label, (x, y, gz + 45), (300, 60, 90), mats["concrete"], yaw, folder="Barriers")
    block(label + "_Top", (x, y, gz + 100), (300, 30, 20), mats["concrete"], yaw, folder="Barriers")
    block(label + "_Stripe", (x, y, gz + 70), (302, 62, 12), mats["hazard"], yaw, folder="Barriers")


# --------------------------------------------------------------------------------------------
# Biome extras
# --------------------------------------------------------------------------------------------

def clear_of_gameplay(x, y):
    """Keep scatter away from the player start, landing zone and compound."""
    cameras = ((-2400, -2900), (-2200, -1500), (5600, 2600), (1300, -700), (-2500, 1900))   # keep showcase views clear
    if any((x - cx) ** 2 + (y - cy) ** 2 < 1800 ** 2 for cx, cy in cameras):
        return False
    return not ((-6200 < x < -2600 and -1600 < y < 3400) or (1400 < x < 4600 and -2000 < y < 2200))


WATER_Z = -300.0


def make_water_material():
    mat, fresh = new_material("M_HarbourWater")
    if not fresh:
        return mat
    # Deep grey-green water, glassy, with two drifting layers of ripples and paler water at grazing angles.
    deep = const3(mat, (0.008, 0.03, 0.035), -900, -400)
    shallow = const3(mat, (0.05, 0.12, 0.13), -900, -280)
    fres = expr(mat, unreal.MaterialExpressionFresnel, -900, -160, exponent=4.0, base_reflect_fraction=0.04)
    col = expr(mat, unreal.MaterialExpressionLinearInterpolate, -600, -300)
    mel.connect_material_expressions(deep, "", col, "A")
    mel.connect_material_expressions(shallow, "", col, "B")
    mel.connect_material_expressions(fres, "", col, "Alpha")
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(mat, unreal.MaterialExpressionConstant, -600, 0, r=0.04)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = expr(mat, unreal.MaterialExpressionConstant, -600, 80, r=1.0)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    normals = []
    for i, (scale, sx, sy) in enumerate(((900.0, 0.02, 0.012), (260.0, -0.035, 0.02))):
        uv = world_uv(mat, scale, -1600, 300 + i * 300)
        pan = expr(mat, unreal.MaterialExpressionPanner, -1200, 300 + i * 300, speed_x=sx, speed_y=sy)
        mel.connect_material_expressions(uv, "", pan, "Coordinate")
        normals.append(tex(mat, "/Game/Building/Textures/T_Noise01_N", pan, -900, 300 + i * 300, normal=True))
    add = expr(mat, unreal.MaterialExpressionAdd, -600, 400)
    mel.connect_material_expressions(normals[0], "RGB", add, "A")
    mel.connect_material_expressions(normals[1], "RGB", add, "B")
    flat = expr(mat, unreal.MaterialExpressionMultiply, -400, 400, const_b=0.5)
    mel.connect_material_expressions(add, "", flat, "A")
    mel.connect_material_property(flat, "", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat


def marker(label, location, tag):
    m = spawn(unreal.TargetPoint, label, location, folder="Objectives")
    m.tags = [unreal.Name(tag)]
    return m


def build_harbour(mats):
    """Port Halden: the strait, a blocked bridge, the docks and cranes, the transports, the boat and the air defence."""
    water = place(lib.load_asset("/Engine/BasicShapes/Plane"), "HarbourWater", (0, 0, WATER_Z), rot(), (40.0, 700.0, 1.0), make_water_material(), "Harbour")
    wc = water.static_mesh_component
    wc.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    wc.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    wc.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY, unreal.CollisionResponseType.ECR_BLOCK)   # hoverbikes skim it
    water.tags = [unreal.Name("DV_Water")]

    # Quay walls along both banks.
    for side in (-1, 1):
        for i in range(12):
            y = -5500 + i * 1000
            block("Quay_%s%02d" % ("W" if side < 0 else "E", i), (side * 1000, y, WATER_Z + 180), (220, 1000, 520), mats["concrete"], folder="Harbour")
            block("QuayEdge_%s%02d" % ("W" if side < 0 else "E", i), (side * 890, y, WATER_Z + 445), (20, 1000, 14), mats["hazard"], folder="Harbour")

    # The bridge to the north, blocked by a CINDER barricade of stacked containers and a wrecked transporter.
    by_ = 2600
    block("Bridge_Deck", (0, by_, 30), (3000, 700, 60), mats["concrete_dark"], folder="Bridge")
    for x in range(-1300, 1400, 650):
        block("Bridge_Pier_%d" % x, (x, by_, -350), (160, 400, 700), mats["concrete"], folder="Bridge")
    block("Bridge_RailN", (0, by_ + 340, 90), (3000, 20, 60), mats["gunmetal"], folder="Bridge")
    block("Bridge_RailS", (0, by_ - 340, 90), (3000, 20, 60), mats["gunmetal"], folder="Bridge")
    for k in range(3):
        block("Barricade_%d" % k, (-150 + k * 160, by_, 60 + 260 * k + 130), (600, 244, 260), [mats["rust"], mats["navy"], mats["olive"]][k], 85 + k * 8, folder="Bridge")
    block("Wreck_Truck", (300, by_ - 120, 160), (700, 260, 200), mats["gunmetal"], 30, folder="Bridge")
    block("Barricade_Sign", (-150, by_, 900), (40, 600, 120), mats["strip_red"], folder="Bridge")

    # Docks on the east bank: piers, gantry cranes, container stacks.
    for i, y in enumerate((-2800, -600, 1400)):
        block("Pier_%d" % i, (700, y, WATER_Z + 330), (500, 260, 40), mats["concrete_dark"], folder="Docks")
        for k in range(3):
            block("PierPost_%d_%d" % (i, k), (520 + k * 170, y, WATER_Z + 150), (40, 40, 360), mats["concrete"], folder="Docks")
    for i, y in enumerate((-2000, 400)):
        cx = 1450
        for lx, ly in ((-200, -400), (200, -400), (-200, 400), (200, 400)):
            block("Crane_%d_Leg%d%d" % (i, lx, ly), (cx + lx, y + ly, ground(cx + lx, y + ly) + 1100), (60, 60, 2200), mats["hazard"], folder="Cranes")
        block("Crane_%d_Beam" % i, (cx, y, ground(cx, y) + 2250), (500, 900, 120), mats["hazard"], folder="Cranes")
        block("Crane_%d_Boom" % i, (cx - 1400, y, ground(cx, y) + 2350), (3400, 160, 100), mats["hazard"], folder="Cranes")
        block("Crane_%d_Cab" % i, (cx, y - 300, ground(cx, y) + 2050), (220, 220, 200), mats["gunmetal"], folder="Cranes")
        block("Crane_%d_Lamp" % i, (cx - 3000, y, ground(cx, y) + 2290), (40, 40, 40), mats["strip_red"], folder="Cranes")
    for i in range(10):
        x, y = random.uniform(1700, 2400), random.uniform(-3200, 2000)
        if abs(y + 2000) < 600 or abs(y - 400) < 600:
            continue
        container("DockBox_%d" % i, x, y, random.choice((0, 90)), random.choice((mats["rust"], mats["navy"], mats["olive"])), mats, stacked=random.random() < 0.4)

    # Civilian transports moored in the strait, bows to the sea (south).
    transport = unreal.load_class(None, "/Script/DeltaVDominion.DVTransportShip")
    for i, y in enumerate((300, -5200)):
        spawn(transport, "Transport_%d" % i, (420, y, WATER_Z - 20), rot(yaw=-90), "Harbour")

    # The Marlin at the west slip, nose to the east bank.
    block("Slip", (-650, -3000, WATER_Z + 60), (500, 900, 30), mats["concrete_dark"], folder="Harbour")
    boat = unreal.load_class(None, "/Script/DeltaVDominion.DVBoat")
    spawn(boat, "Marlin", (-350, -3000, WATER_Z + 60), rot(yaw=20), "Vehicles")   # clear water south of the moored transport

    # Air-defence controller on the north-west headland: radar, launcher racks, a control shack.
    ax, ay = -2000, 3300   # north of the bridge on the west bank, out of rifle range of the drop
    az = ground(ax, ay)
    block("AA_Pad", (ax, ay, az + 15), (1500, 1300, 30), mats["concrete_dark"], folder="AirDefence")
    block("AA_Shack", (ax - 400, ay - 300, az + 160), (400, 300, 300), mats["concrete"], folder="AirDefence")
    block("AA_ShackStrip", (ax - 190, ay - 300, az + 250), (6, 260, 10), mats["strip_red"], folder="AirDefence")
    for k in range(4):
        block("AA_RadarLeg%d" % k, (ax + 250 + (k % 2) * 120, ay + 250 + (k // 2) * 120, az + 600), (24, 24, 1200), mats["gunmetal"], folder="AirDefence")
    block("AA_Radar", (ax + 310, ay + 310, az + 1250), (60, 520, 300), mats["gunmetal"], 35, folder="AirDefence")
    block("AA_RadarGlow", (ax + 340, ay + 290, az + 1250), (8, 420, 20), mats["strip_red"], 35, folder="AirDefence")
    for k in range(2):
        rx = ax - 200 + k * 500
        block("AA_Launcher%d" % k, (rx, ay + 350, az + 160), (320, 220, 200), mats["olive"], folder="AirDefence")
        for t in range(4):
            block("AA_Tube%d_%d" % (k, t), (rx - 90 + t * 60, ay + 350, az + 330), (40, 200, 40), mats["gunmetal"], folder="AirDefence")
    marker("Objective_AirDefence", (ax, ay, az + 20), "DV_Obj_1")
    soldier = unreal.load_class(None, "/Script/DeltaVDominion.DVSoldierCharacter")
    for i, (dx, dy) in enumerate(((-500, 300), (300, -400), (600, 200), (-100, 600), (-700, -100))):
        e = spawn(soldier, "AA_Guard_%02d" % i, (ax + dx, ay + dy, ground(ax + dx, ay + dy) + 105), rot(yaw=200), "Enemies")
        e.set_editor_property("team_id", 2)

    # The dock control key sits in the east compound's bunker; the Raven comes in to a pad south of it.
    marker("Objective_DockKey", (3000, 0, ground(3000, 0) + 20), "DV_Obj_2")
    lx, ly = 3900, -2700
    lz = ground(lx, ly)
    block("Extract_Pad", (lx, ly, lz + 15), (1400, 1400, 30), mats["concrete_dark"], folder="Harbour")
    marker("Extraction_Pad", (lx, ly, lz + 30), "DV_LZ")

    # Harbour overlook for trailers ("DVShowcase 5") and a low shot along the water ("DVShowcase 6").
    for i, (eye, target) in enumerate((((-2600, -4200, 1700), (600, 600, -300)), ((-700, -3800, -150), (300, 1800, -100)))):
        cam = spawn(unreal.CameraActor, "ShowcaseCam_%d" % (5 + i), eye, folder="Cameras")
        cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*eye), unreal.Vector(*target)), False)
        cam.tags = [unreal.Name("Showcase%d" % (5 + i))]
        setp(cam.camera_component, "field_of_view", 70.0)


def build_extras(mats, rocks, rock_mat):
    extras = B["extras"]
    if "harbour" in extras:
        build_harbour(mats)
    if "lava" in extras:
        lava = place(CHAMFER, "LavaSea", (0, 0, -260), rot(), (60000 / CHAMFER_SIZE.x, 60000 / CHAMFER_SIZE.y, 20 / CHAMFER_SIZE.z), mats["lava"], "Lava")
        for i in range(18):
            x = -5000 + i * 600
            for offset in (-2600.0, 2900.0):
                y = offset + math.sin(x / 1700.0 + offset) * 700.0
                l = spawn(unreal.PointLight, "LavaGlow_%d_%d" % (i, int(offset)), (x, y, -150), folder="Lava")
                lc = l.point_light_component
                setp(lc, "intensity_units", unreal.LightUnits.CANDELAS)
                setp(lc, "intensity", 2500.0)
                setp(lc, "light_color", unreal.Color(r=255, g=90, b=20, a=255))
                setp(lc, "attenuation_radius", 1600.0)
                setp(lc, "cast_shadows", False)
    if "gas_giant" in extras:
        # A gas giant hanging in the sky (Jupiter seen from its moons): body plus darker bands.
        sphere = lib.load_asset("/Engine/BasicShapes/Sphere")
        cx, cy, cz, r = 900000.0, -1400000.0, 700000.0, 560000.0
        place(sphere, "GasGiant", (cx, cy, cz), rot(roll=18), (r / 50.0, r / 50.0, r / 50.0), mats["giant"], "Sky")
        for i, dz in enumerate((-0.35, -0.12, 0.08, 0.3)):
            band_r = r * math.sqrt(max(0.0, 1.0 - dz * dz)) * 1.004
            place(sphere, "GasGiantBand%d" % i, (cx, cy, cz + dz * r), rot(roll=18), (band_r / 50.0, band_r / 50.0, r * 0.07 / 50.0), mats["giant_band"], "Sky")
    if "trees" in extras:
        tree = lib.load_asset("/Game/ArchVis/SampleScene/Tree/HillTree_02")
        if tree:
            count = 0
            for i in range(600):
                x, y = random.uniform(-14000, 14000), random.uniform(-11000, 11000)
                q = math.sqrt((x / 5600.0) ** 2 + (y / 4300.0) ** 2)
                if q < 0.55 or not clear_of_gameplay(x, y) or (q < 1.0 and random.random() < 0.75):
                    continue
                s = random.uniform(0.8, 1.5)
                place(tree, "Tree_%03d" % count, (x, y, ground(x, y) - 20), rot(yaw=random.uniform(0, 360)), (s, s, s * random.uniform(0.9, 1.2)), None, "Trees")
                count += 1
                if count >= 220:
                    break
            log("trees %d" % count)
    if "neon" in extras:
        # Cyberpunk mining town: rigs, stacked habitat blocks and neon signage, lit for the night.
        # Mostly sodium-amber and cold cyan work lights; magenta is an accent on a few signs, not the whole town.
        neon = [mats["sign_amber"], mats["sign_amber"], mats["sign_cyan"], mats["sign_cyan"], mats["sign_pink"]]
        windows = mats["windows_dim"]
        for i in range(26):
            ang = random.uniform(0, 2 * math.pi)
            q = random.uniform(1.05, 1.6)
            x, y = math.cos(ang) * 5600 * q, math.sin(ang) * 4300 * q
            gz = ground(x, y)
            w, d, h = random.uniform(600, 1400), random.uniform(600, 1400), random.uniform(1200, 4200)
            yaw = random.uniform(0, 90)
            block("Hab_%02d" % i, (x, y, gz + h / 2), (w, d, h), mats["concrete_dark"], yaw, folder="Town")
            for k in range(int(h // 500)):
                # Lit window bands on most floors, a neon trim every third floor.
                strip = random.choice(neon) if k % 3 == 2 else windows
                block("HabStrip_%02d_%d" % (i, k), (x, y, gz + 300 + k * 500), (w + 12, d + 12, 18), strip, yaw, folder="Town")
            if random.random() < 0.45:
                block("HabSign_%02d" % i, (x, y, gz + h + 260), (w * 0.7, 40, 380), random.choice(neon), yaw, folder="Town")
        for i in range(10):
            x, y = random.uniform(-4000, 4500), random.choice((-1, 1)) * random.uniform(2400, 3600)
            if not clear_of_gameplay(x, y):
                continue
            gz = ground(x, y)
            for k, (ox, oy) in enumerate([(-150, -150), (150, -150), (-150, 150), (150, 150)]):
                block("Rig_%d_Leg%d" % (i, k), (x + ox, y + oy, gz + 900), (30, 30, 1800), mats["gunmetal"], folder="Rigs")
            block("Rig_%d_Head" % i, (x, y, gz + 1850), (420, 420, 180), mats["gunmetal"], folder="Rigs")
            block("Rig_%d_Ring" % i, (x, y, gz + 1700), (440, 440, 20), mats["sign_amber"], folder="Rigs")
            l = spawn(unreal.PointLight, "RigLight_%d" % i, (x, y, gz + 1650), folder="Rigs")
            lc = l.point_light_component
            setp(lc, "intensity_units", unreal.LightUnits.CANDELAS)
            setp(lc, "intensity", 1600.0)
            setp(lc, "light_color", random.choice([unreal.Color(r=255, g=170, b=70, a=255), unreal.Color(r=255, g=170, b=70, a=255), unreal.Color(r=60, g=200, b=255, a=255)]))
            setp(lc, "attenuation_radius", 3500.0)
    if "city" in extras:
        # The capital's skyline beyond the hills: towers with lit window bands and aircraft beacons.
        for i in range(70):
            ang = random.uniform(-0.9, 0.9) + (math.pi if random.random() < 0.3 else 0.0)
            dist = random.uniform(16000, 28000)
            x, y = math.cos(ang) * dist, math.sin(ang) * dist
            gz = ground(x, y)
            w, d, h = random.uniform(900, 2600), random.uniform(900, 2600), random.uniform(4000, 22000)
            yaw = random.uniform(-15, 15)
            block("Tower_%02d" % i, (x, y, gz + h / 2), (w, d, h), mats["concrete_dark"] if "harbour" in extras else mats["concrete"], yaw, folder="City")
            for k in range(int(h // 900)):
                block("TowerWin_%02d_%d" % (i, k), (x, y, gz + 450 + k * 900), (w + 10, d + 10, 120), mats["windows_dim"] if "harbour" in extras else mats["windows"], yaw, folder="City")
            block("TowerBeacon_%02d" % i, (x, y, gz + h + 60), (80, 80, 80), mats["strip_red"], folder="City")



# --------------------------------------------------------------------------------------------
# Rook's Rest (hub town)
# --------------------------------------------------------------------------------------------

def town_light(label, loc, color, candelas, radius):
    l = spawn(unreal.PointLight, label, loc, folder="Lights")
    lc = l.point_light_component
    setp(lc, "intensity_units", unreal.LightUnits.CANDELAS)
    setp(lc, "intensity", candelas)
    setp(lc, "light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    setp(lc, "attenuation_radius", radius)
    setp(lc, "cast_shadows", False)
    return l


def townsperson(label, loc, yaw, name, trade, wanders=True, radius=1400.0):
    folk = spawn(unreal.load_class(None, "/Script/DeltaVDominion.DVTownsfolk"), label, loc, rot(yaw=yaw), "Townsfolk")
    folk.set_editor_property("display_name", name)
    folk.set_editor_property("trade", trade)
    folk.set_editor_property("wanders", wanders)
    folk.set_editor_property("wander_radius", radius)
    return folk


def stall(i, x, y, facing, canvas, mats, vendor):
    """A market stall facing the street: four poles, a canvas awning, a counter of goods; the vendor stands behind it."""
    gz = ground(x, y)
    c, s_ = math.cos(math.radians(facing)), math.sin(math.radians(facing))
    for k, (ox, oy) in enumerate([(-180, -130), (180, -130), (-180, 130), (180, 130)]):
        rx, ry = ox * -s_ + oy * c, ox * c + oy * s_
        block("Stall%d_Pole%d" % (i, k), (x + rx, y + ry, gz + 140), (10, 10, 280), mats["gunmetal"], folder="Market")
    block("Stall%d_Awning" % i, (x, y, gz + 285), (300, 420, 14), canvas, facing, folder="Market")
    block("Stall%d_Valance" % i, (x + c * 150, y + s_ * 150, gz + 260), (6, 420, 50), canvas, facing, folder="Market")
    block("Stall%d_Counter" % i, (x + c * 90, y + s_ * 90, gz + 50), (70, 380, 100), mats["adobe_dark"], facing, folder="Market")
    for k in range(4):
        goods = random.choice([mats["rust"], mats["navy"], mats["olive"], mats["hazard"], mats["sign_amber"]])
        off = -140 + k * 95
        block("Stall%d_Goods%d" % (i, k), (x + c * 90 - s_ * off, y + s_ * 90 + c * off, gz + 118), (40, 50, 36), goods,
              facing + random.uniform(-20, 20), folder="Market")
    for k in range(3):
        off = -150 + k * 150
        block("Stall%d_Bulb%d" % (i, k), (x + c * 140 - s_ * off, y + s_ * 140 + c * off, gz + 255), (12, 12, 12), mats["lamp"], folder="Market")
    town_light("Stall%d_Light" % i, (x + c * 60, y + s_ * 60, gz + 240), (255, 190, 120), 250.0, 700.0)
    townsperson("Vendor_%d" % i, (x - c * 60, y - s_ * 60, gz + 100), facing, vendor, 0, wanders=False)


def town_house(label, x, y, w, d, h, mats, sign=None):
    """Adobe-and-scrap habitat: thick walls, a lit doorway and windows on the street side, rooftop clutter, maybe a neon sign."""
    gz = ground(x, y)
    block(label, (x, y, gz + h / 2), (w, d, h), random.choice([mats["adobe"], mats["adobe"], mats["adobe_dark"]]), folder="Town")
    block(label + "_Parapet", (x, y, gz + h + 15), (w + 20, d + 20, 30), mats["adobe_dark"], folder="Town")
    face = -1 if y > 0 else 1
    fy = y + face * (d / 2 + 2)
    block(label + "_Door", (x, fy, gz + 120), (130, 6, 230), mats["windows_dim"], folder="Town")
    for k in range(int(w // 400)):
        wx = x - w / 2 + 200 + k * 400
        if abs(wx - x) > 120:
            block(label + "_Win%d" % k, (wx, fy, gz + (460 if h > 600 else 210)), (90, 6, 70), mats["windows_dim"], folder="Town")
    for k in range(random.randint(1, 3)):
        block(label + "_Roof%d" % k, (x + random.uniform(-w / 3, w / 3), y + random.uniform(-d / 3, d / 3), gz + h + 60),
              (random.uniform(80, 200), random.uniform(80, 200), random.uniform(60, 140)), random.choice([mats["gunmetal"], mats["rust"]]),
              random.uniform(0, 90), folder="Town")
    if sign is not None:
        block(label + "_Sign", (x + random.uniform(-w / 4, w / 4), y + face * (d / 2 + 40), gz + h - 120), (220, 14, 90), sign, folder="Town")
    town_light(label + "_DoorLight", (x, fy + face * 120, gz + 260), (255, 175, 100), 150.0, 600.0)


def build_town(mats):
    """Rook's Rest: one main street (along X) of stalls, homes and the Rusty Rook cantina; shuttle pad west, garage east."""
    inter = unreal.load_class(None, "/Script/DeltaVDominion.DVInteractable")
    cyl = lib.load_asset("/Engine/BasicShapes/Cylinder")
    canvases = [mats["canvas_red"], mats["canvas_teal"], mats["canvas_sand"], mats["canvas_red"]]
    signs = [mats["sign_amber"], mats["sign_cyan"], mats["sign_pink"], mats["sign_amber"]]

    # Street: packed earth with salvaged deck plates pressed into it.
    block("MainStreet", (0, 0, ground(0, 0) - 5), (6400, 900, 20), mats["street"], folder="Street")
    for i in range(16):
        block("StreetPlate_%d" % i, (-3000 + i * 400, random.uniform(-40, 40), 3), (300, 160, 6), mats["adobe_dark"], random.uniform(-6, 6), folder="Street")

    # The Rusty Rook: open front, bar and bottles inside, balcony and a big amber sign over the street.
    cx, cy = 0.0, 1150.0
    W, D, H = 1600.0, 900.0, 720.0
    adobe, dark = mats["adobe"], mats["adobe_dark"]
    block("Rook_Back", (cx, cy + D / 2, H / 2), (W, 60, H), adobe, folder="Cantina")
    block("Rook_West", (cx - W / 2, cy, H / 2), (60, D, H), adobe, folder="Cantina")
    block("Rook_East", (cx + W / 2, cy, H / 2), (60, D, H), adobe, folder="Cantina")
    block("Rook_FrontL", (cx - W / 2 + 300, cy - D / 2, H / 2), (600, 60, H), adobe, folder="Cantina")
    block("Rook_FrontR", (cx + W / 2 - 300, cy - D / 2, H / 2), (600, 60, H), adobe, folder="Cantina")
    block("Rook_Lintel", (cx, cy - D / 2, 300 + (H - 300) / 2), (400, 60, H - 300), adobe, folder="Cantina")
    block("Rook_Roof", (cx, cy, H + 20), (W + 120, D + 120, 40), dark, folder="Cantina")
    block("Rook_Floor", (cx, cy, 4), (W - 60, D - 60, 8), mats["wood"], folder="Cantina")
    block("Rook_Balcony", (cx, cy - D / 2 - 160, 380), (W, 320, 20), dark, folder="Cantina")
    for k in range(5):
        block("Rook_BalconyPost%d" % k, (cx - W / 2 + 40 + k * (W - 80) / 4, cy - D / 2 - 300, 190), (24, 24, 380), mats["gunmetal"], folder="Cantina")
    # The sign: a dark board with a neon outline and glowing letter strokes, not a light box.
    sy = cy - D / 2 - 330
    block("Rook_SignBoard", (cx, sy + 12, 520), (900, 14, 170), mats["wood"], folder="Cantina")
    for k, (dx, dz, w, h) in enumerate([(0, 80, 880, 8), (0, -80, 880, 8), (-440, 0, 8, 168), (440, 0, 8, 168)]):
        block("Rook_SignEdge%d" % k, (cx + dx, sy, 520 + dz), (w, 10, h), mats["sign_amber"], folder="Cantina")
    for k in range(9):
        lx = cx - 340 + k * 85
        block("Rook_SignStroke%d" % k, (lx, sy, 520), (14, 10, 90), mats["sign_amber"], folder="Cantina")
        if k % 2 == 0:
            block("Rook_SignBar%d" % k, (lx + 18, sy, 520 + random.choice((-40, 0, 40))), (44, 10, 12), mats["sign_amber"], folder="Cantina")
    block("Rook_SignAccent", (cx + 520, cy - D / 2 - 330, 520), (90, 18, 90), mats["sign_pink"], 45, folder="Cantina")
    block("Rook_Bar", (cx, cy + D / 2 - 220, 55), (1000, 90, 110), mats["wood"], folder="Cantina")
    block("Rook_BarTop", (cx, cy + D / 2 - 220, 114), (1040, 110, 8), mats["gunmetal"], folder="Cantina")
    block("Rook_Shelf", (cx, cy + D / 2 - 50, 200), (1000, 40, 220), mats["wood"], folder="Cantina")
    for k in range(14):
        block("Rook_Bottle%d" % k, (cx - 460 + k * 70, cy + D / 2 - 75, 250 + (k % 2) * 70), (14, 14, 40),
              random.choice([mats["sign_amber"], mats["sign_cyan"], mats["windows"]]), folder="Cantina")
    for k in range(4):
        tx, ty = cx - 500 + k * 330, cy - 100
        place(cyl, "Rook_Table%d" % k, (tx, ty, 38), None, (1.4, 1.4, 0.75), mats["wood"], "Cantina")
        town_light("Rook_Lamp%d" % k, (tx, ty, 420), (255, 170, 90), 280.0, 900.0)
    for k in range(6):
        place(cyl, "Rook_Stool%d" % k, (cx - 400 + k * 160, cy + D / 2 - 330, 23), None, (0.5, 0.5, 0.45), mats["gunmetal"], "Cantina")
    town_light("Rook_Neon", (cx, cy - D / 2 - 420, 500), (255, 150, 60), 600.0, 1500.0)
    for k, name in enumerate(["Drifter", "Off-shift miner", "Ports pilot", "Ice hauler"]):
        townsperson("Patron_%d" % k, (cx - 450 + k * 300, cy + 60 + (k % 2) * 120, 100), random.uniform(0, 360), name, 2, wanders=k % 2 == 0, radius=500.0)

    # Mags Okafor's job board outside the cantina door, and Mags herself.
    board = spawn(inter, "ContractBoard", (cx - 520, cy - D / 2 - 520, 0), folder="Stations")
    board.set_editor_property("interaction", unreal.DVInteraction.CONTRACT_BOARD)
    board.set_editor_property("prompt", "Mags Okafor's job board: take a contract")
    board.set_editor_property("reach", 420.0)
    block("Board_Panel", (cx - 520, cy - D / 2 - 380, 230), (420, 20, 260), mats["wood"], folder="Stations")
    for k in range(6):
        block("Board_Note%d" % k, (cx - 680 + (k % 3) * 160, cy - D / 2 - 392, 180 + (k // 3) * 110), (110, 4, 80),
              mats["sign_amber"] if k == 1 else mats["canvas_sand"], random.uniform(-6, 6), folder="Stations")
    townsperson("Mags_Okafor", (cx - 800, cy - D / 2 - 560, 100), -60, "Mags Okafor, Free Ports fixer", 1, wanders=False)

    # North and south rows of homes and workshops, stalls in front of them.
    north = [(-2400, 800, 650), (-1500, 700, 500), (1400, 800, 700), (2300, 700, 500)]
    south = [(-2300, 900, 550), (-1200, 800, 750), (0, 900, 600), (1200, 800, 500)]
    for i, (x, w, h) in enumerate(north):
        town_house("House_N%d" % i, x, 1150, w, 600, h, mats, sign=signs[i % 4] if i % 2 == 0 else None)
    for i, (x, w, h) in enumerate(south):
        town_house("House_S%d" % i, x, -1150, w, 600, h, mats, sign=signs[(i + 1) % 4] if i % 2 == 1 else None)
    vendors = ["Noodle cart", "Coolant and water", "Spare parts", "Rations trader", "Charm seller", "Scrap dealer"]
    stalls = [(-2100, 560, -90), (-1200, 560, -90), (1300, 560, -90), (-1700, -560, 90), (-500, -560, 90), (1000, -560, 90)]
    for i, (x, y, facing) in enumerate(stalls):
        stall(i, x, y, facing, canvases[i % 4], mats, vendors[i])

    # Juno's garage at the east end: open bay toward the street, skiff bikes parked outside.
    gx, gy = 2900, -1300
    block("Garage_Back", (gx, gy - 400, 300), (1200, 60, 600), mats["rust"], folder="Garage")
    block("Garage_SideW", (gx - 600, gy, 300), (60, 800, 600), mats["rust"], folder="Garage")
    block("Garage_SideE", (gx + 600, gy, 300), (60, 800, 600), mats["rust"], folder="Garage")
    block("Garage_Roof", (gx, gy, 620), (1300, 900, 40), mats["gunmetal"], folder="Garage")
    block("Garage_Sign", (gx, gy + 420, 560), (600, 14, 70), mats["sign_cyan"], folder="Garage")
    for k in range(3):
        block("Garage_Bench%d" % k, (gx - 350 + k * 350, gy - 300, 50), (260, 80, 100), mats["gunmetal"], folder="Garage")
    town_light("Garage_Light", (gx, gy, 560), (190, 220, 255), 400.0, 1400.0)
    townsperson("Mechanic", (gx - 200, gy - 150, 100), 90, "Juno Vasquez, mechanic", 3, wanders=False)

    # Skyline: water tower and comms mast.
    wx, wy = -1800, 2200
    for k, (ox, oy) in enumerate([(-150, -150), (150, -150), (-150, 150), (150, 150)]):
        block("WaterTower_Leg%d" % k, (wx + ox, wy + oy, 600), (30, 30, 1200), mats["gunmetal"], folder="Skyline")
    place(cyl, "WaterTower_Tank", (wx, wy, 1350), None, (5.0, 5.0, 4.0), mats["rust"], "Skyline")
    block("WaterTower_Band", (wx, wy, 1450), (510, 510, 30), mats["sign_amber"], folder="Skyline")
    mx, my = 1900, 2300
    for h in range(0, 2400, 300):
        block("Mast_Seg%d" % h, (mx, my, h + 150), (60 - h / 80.0, 60 - h / 80.0, 300), mats["gunmetal"], folder="Skyline")
    block("Mast_Beacon", (mx, my, 2440), (40, 40, 40), mats["strip_red"], folder="Skyline")

    # String lights across the street.
    for i, x in enumerate((-2950, 2950)):
        block("Pole_%d_A" % i, (x, -470, 300), (24, 24, 600), mats["gunmetal"], folder="Street")
        block("Pole_%d_B" % i, (x, 470, 300), (24, 24, 600), mats["gunmetal"], folder="Street")
    for i in range(9):
        x0 = -2600 + i * 650
        # A sagging cable from rooftop to rooftop with small bulbs hanging off it.
        yaw = math.degrees(math.atan2(920, 140))
        for k in range(10):
            t = k / 9.0
            z = 560 - math.sin(t * math.pi) * 90
            block("Garland_%d_%d" % (i, k), (x0 + (t - 0.5) * 140, -460 + t * 920, z - 10), (8, 8, 9),
                  random.choice([mats["lamp"], mats["sign_amber"], mats["sign_pink"]]), folder="Lights")
            if k < 9:
                t2 = (k + 0.5) / 9.0
                block("GarlandCable_%d_%d" % (i, k), (x0 + (t2 - 0.5) * 140, -460 + t2 * 920, 560 - math.sin(t2 * math.pi) * 90), (104, 2, 2),
                      mats["gunmetal"], yaw, folder="Lights")
        if i % 2 == 0:
            town_light("GarlandLight_%d" % i, (x0, 0, 470), (255, 180, 110), 350.0, 1200.0)

    # The people of Rook's Rest, out on the street.
    walkers = [(1, "Dockhand"), (1, "Freight loader"), (2, "Drifter"), (2, "Prospector"), (2, "Pilgrim"),
               (4, "Colony kid"), (4, "Colony kid"), (1, "Courier"), (2, "Refugee from Ceres")]
    for i, (trade, name) in enumerate(walkers):
        townsperson("Walker_%d" % i, (random.uniform(-2400, 2400), random.uniform(-300, 300), 100), random.uniform(0, 360), name, trade, radius=1800.0)

    # The shuttle home: the Raven on the pad takes Delta back up to the Kestrel.
    shuttle = spawn(inter, "ShuttleHome", (-3000, -700, 0), folder="Stations")
    shuttle.set_editor_property("interaction", unreal.DVInteraction.SHUTTLE)
    shuttle.set_editor_property("prompt", "Shuttle up to the Kestrel")
    shuttle.set_editor_property("destination", "/Game/Maps/Kestrel")
    log("town built")


# --------------------------------------------------------------------------------------------
# Brasshaven
# --------------------------------------------------------------------------------------------

def tube(label, a, b, dia, material, folder):
    """A pipe from a to b (straight, any direction) out of the engine cylinder."""
    cyl = lib.load_asset("/Engine/BasicShapes/Cylinder")
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2)
    r = unreal.MathLibrary.make_rot_from_z(unreal.Vector(dx, dy, dz))   # the cylinder's axis is +Z: point it along a->b
    place(cyl, label, mid, r, (dia / 100.0, dia / 100.0, length / 100.0), material, folder)


def gas_lamp(label, x, y, mats):
    gz = ground(x, y)
    block(label + "_Post", (x, y, gz + 210), (16, 16, 420), mats["soot"], folder="Lamps")
    block(label + "_Arm", (x, y, gz + 420), (60, 8, 8), mats["soot"], folder="Lamps")
    block(label + "_Head", (x, y, gz + 405), (34, 34, 44), mats["gaslamp"], folder="Lamps")
    block(label + "_Cap", (x, y, gz + 435), (46, 46, 10), mats["brass"], folder="Lamps")
    town_light(label + "_Light", (x, y, gz + 380), (255, 165, 80), 260.0, 900.0)


def brass_house(label, x, y, w, d, h, mats):
    """A narrow brick townhouse: brass cornice, a pitched slate roof, chimney, warm windows on the street side."""
    cube = lib.load_asset("/Engine/BasicShapes/Cube")
    gz = ground(x, y)
    block(label, (x, y, gz + h / 2), (w, d, h), random.choice([mats["brick"], mats["brick"], mats["brick_dark"]]), folder="Houses")
    block(label + "_Cornice", (x, y, gz + h + 12), (w + 30, d + 30, 24), mats["brass"], folder="Houses")
    pitch = 32.0
    run = d / 2.0 + 20
    slab = run / math.cos(math.radians(pitch))
    for side in (-1, 1):
        place(cube, label + "_Roof%d" % side, (x, y + side * run / 2, gz + h + 24 + run / 2 * math.tan(math.radians(pitch))),
              rot(roll=-side * pitch), ((w + 50) / 100.0, slab / 100.0, 0.18), mats["soot"], "Houses")
    cx = x + random.uniform(-w / 3, w / 3)
    block(label + "_Chimney", (cx, y, gz + h + 260), (90, 90, 420), mats["brick_dark"], folder="Houses")
    block(label + "_ChimneyCap", (cx, y, gz + h + 475), (110, 110, 18), mats["copper"], folder="Houses")
    face = -1 if y > 0 else 1
    fy = y + face * (d / 2 + 2)
    block(label + "_Door", (x, fy, gz + 130), (120, 6, 250), mats["wood"], folder="Houses")
    block(label + "_Fanlight", (x, fy, gz + 280), (120, 6, 30), mats["windows_dim"], folder="Houses")
    floors = int((h - 120) // 300)
    for f in range(floors):
        for k in range(int(w // 260)):
            wx = x - w / 2 + 130 + k * 260
            if f == 0 and abs(wx - x) < 120:
                continue
            block(label + "_Win%d_%d" % (f, k), (wx, fy, gz + 200 + f * 300), (80, 6, 140),
                  mats["windows"] if random.random() < 0.55 else mats["windows_dim"], folder="Houses")
            block(label + "_Sill%d_%d" % (f, k), (wx, fy + face * 6, gz + 125 + f * 300), (100, 14, 10), mats["brass"], folder="Houses")


def build_brasshaven(mats):
    """The Valve Quarter: a cobbled street of brick townhouses, the clock tower, the Steamheart foundry, a moored airship."""
    cyl = lib.load_asset("/Engine/BasicShapes/Cylinder")
    sphere = lib.load_asset("/Engine/BasicShapes/Sphere")
    cone = lib.load_asset("/Engine/BasicShapes/Cone")

    # Street and square.
    block("Cobbles", (-500, 0, ground(-500, 0) - 4), (6400, 1500, 20), mats["brick_dark"], folder="Street")
    block("FoundryYard", (2300, 0, ground(2300, 0) - 3), (1600, 2600, 18), mats["concrete_dark"], folder="Street")
    for i in range(14):
        block("Kerb_N_%d" % i, (-3200 + i * 420, 740, ground(-3200 + i * 420, 740) + 8), (400, 30, 18), mats["brass"], folder="Street")
        block("Kerb_S_%d" % i, (-3200 + i * 420, -740, ground(-3200 + i * 420, -740) + 8), (400, 30, 18), mats["brass"], folder="Street")

    # Two rows of townhouses facing the street.
    xs = [-2900, -2000, -1100, -200, 700]
    for i, x in enumerate(xs):
        brass_house("House_N%d" % i, x, 1250, random.choice([700, 800, 860]), 700, random.choice([720, 900, 1020, 1200]), mats)
        brass_house("House_S%d" % i, x + 300, -1250, random.choice([700, 800, 860]), 700, random.choice([720, 900, 1020, 1200]), mats)

    # Tube-mail: copper pipes along both rows, and crossings over the street with brass junction drums.
    for side in (-1, 1):
        y = side * 880
        tube("TubeMail_Row%d" % side, (-3300, y, 560), (1300, y, 560), 24, mats["copper"], "TubeMail")
    for i, x in enumerate((-2500, -1300, -100, 1000)):
        tube("TubeMail_Cross%d" % i, (x, -880, 660), (x, 880, 660), 20, mats["copper"], "TubeMail")
        for side in (-1, 1):
            place(cyl, "TubeMail_Drum%d_%d" % (i, side), (x, side * 880, 610), None, (0.6, 0.6, 1.2), mats["brass"], "TubeMail")
            tube("TubeMail_Riser%d_%d" % (i, side), (x, side * 880, 560), (x, side * 880, 660), 20, mats["copper"], "TubeMail")

    # Gas lamps down both kerbs.
    for i, x in enumerate(range(-3100, 1500, 650)):
        gas_lamp("Lamp_N%d" % i, x, 640, mats)
        gas_lamp("Lamp_S%d" % i, x + 325, -640, mats)

    # The clock tower over the north row.
    tx, ty = -600, 2350
    tz = ground(tx, ty)
    block("ClockTower", (tx, ty, tz + 1300), (720, 720, 2600), mats["brick"], folder="ClockTower")
    for k in range(4):
        block("ClockTower_Band%d" % k, (tx, ty, tz + 500 + k * 600), (750, 750, 30), mats["brass"], folder="ClockTower")
    place(cyl, "ClockFace", (tx, ty - 365, tz + 2200), rot(roll=90), (5.2, 5.2, 0.12), mats["clockface"], "ClockTower")
    place(cyl, "ClockRim", (tx, ty - 360, tz + 2200), rot(roll=90), (5.6, 5.6, 0.1), mats["brass"], "ClockTower")
    block("ClockHand_H", (tx + 60, ty - 375, tz + 2240), (150, 8, 18), mats["soot"], 0, folder="ClockTower")
    block("ClockHand_M", (tx, ty - 380, tz + 2300), (14, 8, 210), mats["soot"], folder="ClockTower")
    place(cone, "ClockSpire", (tx, ty, tz + 2600 + 450), None, (8.0, 8.0, 9.0), mats["copper"], "ClockTower")
    town_light("ClockGlow", (tx, ty - 600, tz + 2200), (255, 210, 150), 500.0, 1500.0)

    # The Steamheart: a brick foundry hall with a great gear over its doors and three smoking stacks.
    hx, hy = 3700, 0
    hz = ground(hx, hy)
    W, D, H = 1800.0, 1800.0, 1100.0
    block("Steamheart_Back", (hx + W / 2, hy, hz + H / 2), (60, D, H), mats["brick"], folder="Steamheart")
    block("Steamheart_N", (hx, hy + D / 2, hz + H / 2), (W, 60, H), mats["brick"], folder="Steamheart")
    block("Steamheart_S", (hx, hy - D / 2, hz + H / 2), (W, 60, H), mats["brick"], folder="Steamheart")
    block("Steamheart_FrontN", (hx - W / 2, hy + 600, hz + H / 2), (60, 600, H), mats["brick"], folder="Steamheart")
    block("Steamheart_FrontS", (hx - W / 2, hy - 600, hz + H / 2), (60, 600, H), mats["brick"], folder="Steamheart")
    block("Steamheart_Lintel", (hx - W / 2, hy, hz + 800 + (H - 800) / 2), (60, 600, H - 800), mats["brick_dark"], folder="Steamheart")
    block("Steamheart_Roof", (hx, hy, hz + H + 20), (W + 100, D + 100, 40), mats["soot"], folder="Steamheart")
    block("Steamheart_Cornice", (hx - W / 2 - 20, hy, hz + H - 20), (40, D + 60, 50), mats["brass"], folder="Steamheart")
    block("Steamheart_Glow", (hx + W / 2 - 60, hy, hz + 300), (20, 900, 500), mats["ember"], folder="Steamheart")
    town_light("Steamheart_Furnace", (hx, hy, hz + 400), (255, 120, 40), 1500.0, 2600.0)
    gx, gz = hx - W / 2 - 50, hz + 950
    place(cyl, "Gear_Disc", (gx, hy, gz), rot(pitch=90), (6.5, 6.5, 0.4), mats["brass"], "Steamheart")
    place(cyl, "Gear_Hub", (gx - 25, hy, gz), rot(pitch=90), (1.6, 1.6, 0.6), mats["copper"], "Steamheart")
    for k in range(14):
        a = k * 2 * math.pi / 14
        place(lib.load_asset("/Engine/BasicShapes/Cube"), "Gear_Tooth%d" % k, (gx, hy + math.cos(a) * 360, gz + math.sin(a) * 360),
              rot(roll=-math.degrees(a)), (0.36, 0.7, 0.6), mats["brass"], "Steamheart")
    for k, y in enumerate((-500, 0, 500)):
        sx = hx + 400
        place(cyl, "Stack%d" % k, (sx, y, hz + H + 1300), None, (2.6, 2.6, 26.0), mats["soot"], "Steamheart")
        for r in range(3):
            place(cyl, "Stack%d_Ring%d" % (k, r), (sx, y, hz + H + 400 + r * 700), None, (2.9, 2.9, 0.25), mats["brass"], "Steamheart")
        block("Stack%d_Ember" % k, (sx, y, hz + H + 2620), (180, 180, 20), mats["ember"], folder="Steamheart")
    # Pipes from the hall out to the manifold in the yard.
    tube("FeedPipe_N", (hx - W / 2, 400, 380), (2600, 400, 380), 40, mats["copper"], "Steamheart")
    tube("FeedPipe_S", (hx - W / 2, -400, 380), (2600, -400, 380), 40, mats["copper"], "Steamheart")
    spawn(unreal.load_class(None, "/Script/DeltaVDominion.DVPressureManifold"), "SteamheartManifold", (2250, 0, ground(2250, 0)), rot(yaw=0), "Gameplay")

    # A Guild airship at its mooring mast, and a bigger one far off in the haze.
    ax, ay, az = -1400, -3000, 2500
    place(sphere, "Airship_Envelope", (ax, ay, az), None, (32.0, 9.0, 9.0), mats["canvas_cream"], "Airship")
    for k in range(7):
        place(cyl, "Airship_Rib%d" % k, (ax - 1200 + k * 400, ay, az), rot(pitch=90),
              (9.2 * math.sqrt(max(0.05, 1 - ((k - 3) / 3.6) ** 2)), 9.2 * math.sqrt(max(0.05, 1 - ((k - 3) / 3.6) ** 2)), 0.12), mats["brass"], "Airship")
    block("Airship_Gondola", (ax, ay, az - 620), (1100, 300, 230), mats["wood"], folder="Airship")
    block("Airship_GondolaWin", (ax, ay - 152, az - 600), (900, 6, 70), mats["windows"], folder="Airship")
    for side in (-1, 1):
        tube("Airship_Strut%d" % side, (ax, ay + side * 120, az - 500), (ax, ay + side * 300, az - 300), 14, mats["copper"], "Airship")
        place(cyl, "Airship_Prop%d" % side, (ax - 700, ay + side * 260, az - 560), rot(pitch=90), (0.5, 0.5, 0.6), mats["brass"], "Airship")
        block("Airship_Blade%d" % side, (ax - 735, ay + side * 260, az - 560), (8, 30, 320), mats["wood"], 0, folder="Airship")
    for k, (fy, fz) in enumerate([(0, 900), (900, 0), (-900, 0)]):
        block("Airship_Fin%d" % k, (ax + 1500, ay + fy * 0.6, az + fz * 0.6), (400, 20 if fz else 500, 500 if fz else 20), mats["brass"], folder="Airship")
    mx, my = ax - 2050, ay
    for k, (ox, oy) in enumerate([(-90, -90), (90, -90), (-90, 90), (90, 90)]):
        block("Mast_Leg%d" % k, (mx + ox, my + oy, ground(mx, my) + az / 2), (20, 20, az), mats["soot"], folder="Airship")
    for h in range(300, int(az), 400):
        block("Mast_Brace%d" % h, (mx, my, ground(mx, my) + h), (200, 200, 12), mats["brass"], folder="Airship")
    tube("Mast_Cable", (mx, my, ground(mx, my) + az), (ax - 1550, ay, az), 10, mats["soot"], "Airship")
    place(sphere, "FarAirship", (-9000, 11000, 5200), rot(yaw=30), (60.0, 16.0, 16.0), mats["canvas_cream"], "Airship")
    block("FarAirship_Gondola", (-9000, 11000, 4100), (1800, 500, 380), mats["wood"], 30, folder="Airship")

    # Cover in the square: crates, barrels, handcarts and low brick walls.
    spots = [(-2600, -300), (-2200, 350), (-1600, -150), (-900, 400), (-500, -350), (100, 250), (500, -300), (1300, 150),
             (1600, -600), (1700, 700), (-300, 600), (-1300, -550)]
    for i, (x, y) in enumerate(spots):
        gz = ground(x, y)
        kind = i % 4
        if kind == 0:
            block("Crate_%d" % i, (x, y, gz + 60), (120, 120, 120), mats["wood"], random.uniform(0, 90), folder="Cover")
            block("Crate_%d_Top" % i, (x + 20, y - 10, gz + 165), (90, 90, 90), mats["wood"], random.uniform(0, 90), folder="Cover")
        elif kind == 1:
            for k in range(3):
                place(cyl, "Barrel_%d_%d" % (i, k), (x + (k - 1) * 75, y + random.uniform(-15, 15), gz + 55), None, (0.65, 0.65, 1.1), mats["copper"], "Cover")
        elif kind == 2:
            block("Wall_%d" % i, (x, y, gz + 60), (420, 50, 120), mats["brick_dark"], random.choice([0, 90]), folder="Cover")
        else:
            block("Cart_%d" % i, (x, y, gz + 80), (260, 140, 70), mats["wood"], random.uniform(-30, 30), folder="Cover")
            for side in (-1, 1):
                place(cyl, "Cart_%d_Wheel%d" % (i, side), (x, y + side * 80, gz + 50), rot(roll=90), (1.0, 1.0, 0.08), mats["brass"], "Cover")

    # The people of the Valve Quarter, and the Guildmaster at the foundry gate.
    folk = [("Lamplighter", 1, "Eighty years without a ship from the sky, and the first one brings soldiers."),
            ("Guild apprentice", 3, "Don't touch the constables' keys. They're wound to the Guild's own pattern."),
            ("Tube-mail runner", 1, "Three hundred stations and not one wire. Nothing out there can listen to a pipe."),
            ("Boilermaker", 3, "Can you hear the Steamheart? She's singing too high. That's never good."),
            ("Glassblower", 0, "Your helmet lenses are very fine work. Who blew them? A machine? Oh."),
            ("Schoolmistress", 2, "Inside, children. The soldiers are only passing through."),
            ("Guild child", 4, "Is it true there are cities with no gears in them at all?"),
            ("Coal porter", 1, "My father said the Lattice went quiet and we were lucky. I believe him now."),
            ("Pensioner", 2, "I remember the last ship. I was six. It was smaller than yours."),
            ("Clocksmith", 3, "Somebody punched those cards by hand. Good hand, too. That's what frightens me.")]
    for i, (name, trade, line) in enumerate(folk):
        p = townsperson("Citizen_%d" % i, (random.uniform(-2800, 600), random.uniform(-500, 500), ground(0, 0) + 100), random.uniform(0, 360), name, trade, radius=1500.0)
        p.set_editor_property("custom_line", line)
    g = townsperson("Guildmaster_Ottilie", (1950, 700, ground(1950, 700) + 100), -150, "Guildmaster Ottilie Cogsworth-Vane", 0, wanders=False)
    g.set_editor_property("custom_line", "The Steamheart is the last forge in the sky that the Static cannot reach. Keep your hands off anything with a chip.")
    log("brasshaven built")


# --------------------------------------------------------------------------------------------
# Build
# --------------------------------------------------------------------------------------------

def build():
    global CHAMFER, CHAMFER_SIZE
    CHAMFER = lib.load_asset("/Game/LevelPrototyping/Meshes/SM_ChamferCube")
    b = CHAMFER.get_bounds()
    CHAMFER_SIZE = unreal.Vector(b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2)

    if lib.does_asset_exist(MAP_PATH):
        levels.load_level("/Game/Maps/BootCamp")      # release the old map's assets so they can be rebuilt
    terrain_mat = make_terrain_material()
    rock_mat = make_rock_material()
    mats = {
        "concrete": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Concrete_Bunker", (0.42, 0.40, 0.37), roughness=0.85),
        "concrete_dark": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Concrete_Dark", (0.16, 0.16, 0.16), roughness=0.9),
        "gunmetal": tinted("/Game/Building/Materials/MI_StdOpaque_Brushed", "MI_Gunmetal", (0.07, 0.075, 0.08), metallic=1.0, roughness=0.42),
        "rust": tinted("/Game/Building/Materials/MI_StdOpaque_Brushed", "MI_Container_Rust", (0.30, 0.07, 0.035), metallic=0.6, roughness=0.6),
        "navy": tinted("/Game/Building/Materials/MI_StdOpaque_Brushed", "MI_Container_Navy", (0.03, 0.06, 0.13), metallic=0.6, roughness=0.55),
        "olive": tinted("/Game/Building/Materials/MI_StdOpaque_Brushed", "MI_Container_Olive", (0.08, 0.09, 0.04), metallic=0.5, roughness=0.6),
        "hazard": tinted("/Game/Building/Materials/MI_StdOpaque_Brushed", "MI_Hazard", (0.75, 0.42, 0.02), metallic=0.2, roughness=0.5),
        "lamp": glow("MI_Lamp", (1.0, 0.8, 0.55), 40.0),
        "strip_red": glow("MI_Strip_Red", (1.0, 0.12, 0.05), 25.0),
        "strip_cyan": glow("MI_Strip_Cyan", (0.1, 0.7, 1.0), 20.0),
        "lava": glow("MI_Lava", (1.0, 0.32, 0.04), 60.0),
        "neon_pink": glow("MI_Neon_Pink", (1.0, 0.1, 0.6), 45.0),
        "neon_cyan": glow("MI_Neon_Cyan", (0.05, 0.9, 1.0), 45.0),
        "neon_amber": glow("MI_Neon_Amber", (1.0, 0.55, 0.05), 40.0),
        "sign_pink": glow("MI_Sign_Pink", (1.0, 0.15, 0.55), 9.0),
        "sign_cyan": glow("MI_Sign_Cyan", (0.1, 0.8, 1.0), 8.0),
        "sign_amber": glow("MI_Sign_Amber", (1.0, 0.55, 0.12), 10.0),
        "windows_dim": glow("MI_Windows_Dim", (1.0, 0.8, 0.55), 3.0),
        "windows": glow("MI_Windows", (1.0, 0.85, 0.6), 12.0),
        "giant": glow("MI_GasGiant", (0.85, 0.6, 0.38), 3.0),
        "giant_band": glow("MI_GasGiantBand", (0.55, 0.32, 0.18), 2.0),
        "adobe": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Adobe", (0.46, 0.3, 0.19), roughness=0.95),
        "adobe_dark": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Adobe_Dark", (0.26, 0.17, 0.11), roughness=0.95),
        "street": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_PackedEarth", (0.2, 0.12, 0.07), roughness=1.0),
        "wood": tinted("/Game/Building/Materials/MI_StdOpaque_Brushed", "MI_OldWood", (0.16, 0.08, 0.035), metallic=0.0, roughness=0.8),
        "canvas_red": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Canvas_Red", (0.45, 0.07, 0.04), roughness=0.9),
        "canvas_teal": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Canvas_Teal", (0.04, 0.22, 0.22), roughness=0.9),
        "canvas_sand": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Canvas_Sand", (0.55, 0.42, 0.24), roughness=0.9),
        "brass": tinted("/Game/Building/Materials/MI_StdOpaque_Brushed", "MI_Brass", (0.55, 0.36, 0.12), metallic=1.0, roughness=0.3),
        "copper": tinted("/Game/Building/Materials/MI_StdOpaque_Brushed", "MI_Copper", (0.6, 0.22, 0.1), metallic=1.0, roughness=0.38),
        "brick": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Brick", (0.24, 0.075, 0.045), roughness=0.92),
        "brick_dark": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Brick_Dark", (0.11, 0.04, 0.028), roughness=0.95),
        "soot": tinted("/Game/Building/Materials/MI_StdOpaque_Brushed", "MI_Soot", (0.03, 0.028, 0.026), metallic=0.4, roughness=0.75),
        "canvas_cream": tinted("/Game/Building/Materials/MI_StdOpaque_Concrete", "MI_Canvas_Cream", (0.6, 0.52, 0.38), roughness=0.85),
        "gaslamp": glow("MI_GasLamp", (1.0, 0.62, 0.25), 30.0),
        "clockface": glow("MI_ClockFace", (0.95, 0.88, 0.7), 2.5),
        "ember": glow("MI_Ember", (1.0, 0.35, 0.08), 25.0),
    }
    terrain = make_terrain()
    rocks = [make_rock(i) for i in range(6)]

    # Rebuild in place when the map exists (deleting a map package that was loaded this session fails).
    if lib.does_asset_exist(MAP_PATH):
        if not levels.load_level(MAP_PATH):
            raise RuntimeError("Could not load " + MAP_PATH)
        actors.destroy_actors([a for a in actors.get_all_level_actors() if not isinstance(a, unreal.WorldSettings)])
    elif not levels.new_level(MAP_PATH):
        raise RuntimeError("Could not create " + MAP_PATH)

    town = "town" in B["extras"]
    brass = "brass" in B["extras"]

    # --- Terrain and rocks --------------------------------------------------------------
    place(terrain, "MoonTerrain", (0, 0, 0), material=terrain_mat, folder="Terrain")
    if "harbour" not in B["extras"]:
        # Plains out to the horizon around the 600 m terrain tile, so the land doesn't end when a Raven climbs.
        edge, far, top = 29500.0, 1500000.0, 400.0
        for name, cx, cy, sx, sy in (("N", 0, (edge + far) / 2, 2 * far, far - edge), ("S", 0, -(edge + far) / 2, 2 * far, far - edge),
                                     ("E", (edge + far) / 2, 0, far - edge, 2 * edge), ("W", -(edge + far) / 2, 0, far - edge, 2 * edge)):
            # A plain cube: the chamfered block's bevel would grow kilometres wide at this scale.
            place(lib.load_asset("/Engine/BasicShapes/Cube"), "Plains_" + name, (cx, cy, top - 200), None, (sx / 100.0, sy / 100.0, 4.0), terrain_mat, "Terrain")

    # Boulders as cover across the approach, plus big outcrops on the slopes.
    cover_rocks = [(-2600, -1300), (-2300, 900), (-1600, -300), (-900, 1500), (-700, -1600), (-200, 400),
                   (300, -900), (700, 1700), (1100, -2100), (-3300, 300), (-1200, -2600), (200, 2600)]
    for i, (x, y) in enumerate([] if town or brass else cover_rocks):
        s = random.uniform(1.1, 1.8)
        place(random.choice(rocks), "CoverRock_%02d" % i, (x, y, ground(x, y) - 10), rot(yaw=random.uniform(0, 360)),
              (s * random.uniform(0.9, 1.4), s, s * random.uniform(0.8, 1.1)), rock_mat, "Rocks")
    for i in range(90):
        ang = random.uniform(0, 2 * math.pi)
        q = random.uniform(1.0, 1.9)
        x, y = math.cos(ang) * 5600 * q, math.sin(ang) * 4300 * q
        s = random.uniform(2.5, 6.0)
        # Tall, half-buried spires and crags read as eroded rock instead of floating slabs.
        place(random.choice(rocks), "Outcrop_%02d" % i, (x, y, ground(x, y) - 30 * s), rot(pitch=random.uniform(-8, 8), yaw=random.uniform(0, 360), roll=random.uniform(-8, 8)),
              (s * random.uniform(0.7, 1.1), s * random.uniform(0.7, 1.1), s * random.uniform(1.2, 2.4)), rock_mat, "Rocks")

    # --- Hearthbound compound (east) ----------------------------------------------------
    bx, by = 3000.0, 0.0
    if not town and not brass and "walker" not in B["extras"]:   # the Ironstride's loop needs the plain clear
        bz = ground(bx, by)
        # Bunker: thick walls with a doorway facing west, roof slab with overhang, glowing strips.
        block("Bunker_Floor", (bx, by, bz + 10), (1400, 1000, 30), mats["concrete_dark"])
        block("Bunker_WallN", (bx, by + 480, bz + 220), (1400, 60, 440), mats["concrete"])
        block("Bunker_WallS", (bx, by - 480, bz + 220), (1400, 60, 440), mats["concrete"])
        block("Bunker_WallE", (bx + 680, by, bz + 220), (60, 1000, 440), mats["concrete"])
        block("Bunker_WallW_A", (bx - 680, by + 330, bz + 220), (60, 340, 440), mats["concrete"])
        block("Bunker_WallW_B", (bx - 680, by - 330, bz + 220), (60, 340, 440), mats["concrete"])
        block("Bunker_Lintel", (bx - 680, by, bz + 400), (60, 320, 80), mats["concrete"])
        block("Bunker_Roof", (bx, by, bz + 470), (1600, 1200, 60), mats["concrete_dark"])
        block("Bunker_Strip_W", (bx - 712, by, bz + 352), (6, 300, 10), mats["strip_red"])
        block("Bunker_Strip_N", (bx, by + 512, bz + 420), (1300, 6, 8), mats["strip_red"])
        block("Bunker_Vent", (bx + 300, by - 200, bz + 540), (300, 260, 80), mats["gunmetal"])
        inner = spawn(unreal.PointLight, "Bunker_Light", (bx, by, bz + 380), folder="Lights")
        setp(inner.point_light_component, "intensity_units", unreal.LightUnits.CANDELAS)
        setp(inner.point_light_component, "intensity", 600.0)
        setp(inner.point_light_component, "light_color", unreal.Color(r=255, g=60, b=40, a=255))
        setp(inner.point_light_component, "attenuation_radius", 1400.0)

        # Comms tower with a blinking-red top light.
        tx, ty = bx + 1100, by + 900
        tz = ground(tx, ty)
        for i, (ox, oy) in enumerate([(-80, -80), (80, -80), (-80, 80), (80, 80)]):
            block("Tower_Leg%d" % i, (tx + ox, ty + oy, tz + 900), (18, 18, 1800), mats["gunmetal"], folder="Tower")
        for h in range(200, 1800, 300):
            block("Tower_Brace%d" % h, (tx, ty, tz + h), (180, 180, 10), mats["gunmetal"], folder="Tower")
        block("Tower_Dish", (tx, ty, tz + 1850), (260, 40, 160), mats["gunmetal"], 30, folder="Tower")
        block("Tower_Beacon", (tx, ty, tz + 1950), (30, 30, 30), mats["strip_red"], folder="Tower")
        beacon = spawn(unreal.PointLight, "Tower_BeaconLight", (tx, ty, tz + 1960), folder="Lights")
        setp(beacon.point_light_component, "intensity_units", unreal.LightUnits.CANDELAS)
        setp(beacon.point_light_component, "intensity", 400.0)
        setp(beacon.point_light_component, "light_color", unreal.Color(r=255, g=30, b=20, a=255))
        setp(beacon.point_light_component, "attenuation_radius", 900.0)

        # Containers, barriers and sandbag lines around the compound and across no-man's land.
        container("Container_A", bx - 1500, by + 1300, 75, mats["rust"], mats)
        container("Container_B", bx - 1400, by - 1500, 100, mats["navy"], mats)
        container("Container_C", bx + 200, by - 1400, 0, mats["olive"], mats)
        container("Container_D", bx + 200, by - 1400, 0, mats["rust"], mats, stacked=True)
        container("Container_E", bx - 300, by + 1500, 10, mats["navy"], mats)
        container("Container_F", -800, -500, 35, mats["olive"], mats)
        container("Container_G", 600, 1100, -20, mats["rust"], mats)
        for i, (x, y, yaw) in enumerate([(bx - 1100, by - 300, 90), (bx - 1100, by + 400, 90), (bx - 2000, by + 100, 80),
                                         (1200, -700, 60), (1300, 600, 110), (-300, 1100, 0), (-1900, -800, 20), (200, -2000, 70)]):
            jersey("Barrier_%02d" % i, x, y, yaw, mats)

        floodlight("Flood_N", bx - 900, by + 900, 200, mats)
        floodlight("Flood_S", bx - 900, by - 900, 160, mats)
        floodlight("Flood_W", -200, 2300, 250, mats)

    # --- Gameplay -----------------------------------------------------------------------
    start = (-2900, 300) if town else (-4300, 0)
    spawn(unreal.PlayerStart, "PlayerStart", (start[0], start[1], ground(*start) + 110), rot(yaw=0))
    if not town:
        soldier = unreal.load_class(None, "/Script/DeltaVDominion.DVSoldierCharacter")
        enemy_spots = [(bx - 900, by - 250), (bx - 900, by + 450), (bx - 1300, by + 1000), (bx - 1200, by - 1100),
                       (bx - 300, by + 1250), (bx - 450, by), (1300, -500), (1400, 800)]
        if brass:
            # The Guild's clockwork constables, rewound to shoot uniforms: around the square and the Steamheart's yard.
            enemy_spots = [(2300, -650), (2400, 750), (1700, 1050), (1800, -1050), (900, -400), (1000, 600), (-200, 850), (0, -800)]
        for i, (x, y) in enumerate(enemy_spots):
            e = spawn(soldier, ("Constable_%02d" if brass else "Hearthbound_%02d") % i, (x, y, ground(x, y) + 105), rot(yaw=180), "Enemies")
            e.set_editor_property("team_id", 2)
            if brass:
                e.set_editor_property("clockwork", True)

        # --- Free Ports salvage contracts: optional, off the main path ------------------------------------
        salvage = unreal.load_class(None, "/Script/DeltaVDominion.DVSalvage")
        placed = 0
        for i in range(60):
            ang = random.uniform(0, 2 * math.pi)
            q = random.uniform(0.45, 0.85)
            x, y = math.cos(ang) * 5600 * q, math.sin(ang) * 4300 * q
            if not clear_of_gameplay(x, y) or ("harbour" in B["extras"] and abs(x) < 1500):
                continue
            spawn(salvage, "Salvage_%d" % placed, (x, y, ground(x, y) + 5), rot(yaw=random.uniform(0, 360)), "Contracts")
            placed += 1
            if placed >= 3:
                break
        log("salvage contracts %d" % placed)

    # --- Accord vehicles at the landing zone, and the Kestrel overhead ------------------------------
    ltv = unreal.load_class(None, "/Script/DeltaVDominion.DVGroundVehicle")
    tank = unreal.load_class(None, "/Script/DeltaVDominion.DVTank")
    gunship = unreal.load_class(None, "/Script/DeltaVDominion.DVGunship")
    kestrel = unreal.load_class(None, "/Script/DeltaVDominion.DVKestrel")
    if not town:
        spawn(ltv, "LTV_Jackal", (-3700, 950, ground(-3700, 950) + 120), rot(yaw=10), "Vehicles")
        if not brass:   # the Guild allows no armour in its streets
            spawn(tank, "Tank_Sabre", (-3800, -1050, ground(-3800, -1050) + 140), rot(yaw=-5), "Vehicles")
    bike = unreal.load_class(None, "/Script/DeltaVDominion.DVHoverbike")
    for i, (bx, by) in enumerate([(2900, -650), (2600, -650)] if town else [(-3300, 300), (-3300, -300)]):
        spawn(bike, "Hoverbike_%d" % i, (bx, by, ground(bx, by) + 160), rot(yaw=8), "Vehicles")
    px, py = (-3600, -1300) if town else (-5000, 2300)
    pz = ground(px, py)
    block("LZ_Pad", (px, py, pz + 15), (1600, 1600, 40), mats["concrete_dark"], folder="Vehicles")
    for i, (ox, oy) in enumerate([(-740, -740), (740, -740), (-740, 740), (740, 740)]):
        block("LZ_Light%d" % i, (px + ox, py + oy, pz + 40), (40, 40, 20), mats["strip_cyan"], folder="Vehicles")
    spawn(gunship, "Gunship_Raven", (px, py, pz + 200), rot(yaw=90 if town else 30), "Vehicles")
    spawn(kestrel, "TAS_Kestrel", (230000, 70000, 50000), rot(yaw=110), "Vehicles")   # 2.4 km east, 500 m up: on the horizon over the compound

    nav = spawn(unreal.NavMeshBoundsVolume, "NavMeshBounds", (0, 0, 300), folder="Navigation")
    ext = nav.get_actor_bounds(False)[1]
    if ext.x > 1.0:
        nav.set_actor_scale3d(unreal.Vector(5600.0 / ext.x, 4200.0 / ext.y, 1200.0 / ext.z))
    spawn(unreal.RecastNavMesh, "RecastNavMesh-Default", (0, 0, 0), folder="Navigation")

    # Showcase cameras ("DVShowcase N").
    shots = [((-3300, 0, 320), (2400, 0, 250)),                # 0: down Rook's Rest main street at golden hour
             ((750, -380, 200), (-150, 900, 330)),             # 1: the Rusty Rook cantina and the job board
             ((4600, 3400, 1200), (-700, -400, 100)),          # 2: the town from the dunes, sun behind the camera
             ((900, -150, 170), (1500, -700, 180)),            # 3: a market stall up close
             ((-3000, -2300, 260), (-3600, -1300, 220))] if town else [
             ((-3300, 0, 320), (2400, 0, 420)),               # 0: down the Valve Quarter's main street toward the Steamheart
             ((1550, -520, 190), (2250, 0, 260)),             # 1: the pressure manifold in the foundry yard
             ((-4800, -4600, 1900), (0, -600, 900)),          # 2: over the rooftops, the moored airship in the haze
             ((-1850, -300, 200), (-600, 2300, 1300)),        # 3: gas lamps and the clock tower
             ((1100, 1000, 200), (3600, 0, 1700))] if brass else [((-2400, -2900, 700), (2600, 200, 150)),        # 0: overwatch over the approach toward the compound at dusk
             ((-2200, -1500, 220), (2600, 300, 220)),        # 1: low, behind the boulders, looking at the bunker
             ((5600, 2600, 900), (-800, -400, 0)),           # 2: reverse from behind the compound, sun in the lens
             ((bx - 1700, by - 700, 170), (bx - 600, by + 100, 250)),  # 3: close on the bunker doorway
             ((-2500, 1900, 170), (-4200, 300, 140))]          # 4: the landing zone: LTV, tank and gunship up close
    for i, (eye, target) in enumerate(shots):
        cam = spawn(unreal.CameraActor, "ShowcaseCam_%d" % i, (eye[0], eye[1], eye[2] + ground(eye[0], eye[1])), folder="Cameras")
        cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(eye[0], eye[1], eye[2] + ground(eye[0], eye[1])),
                                                                        unreal.Vector(target[0], target[1], target[2] + ground(target[0], target[1]))), False)
        cam.tags = [unreal.Name("Showcase%d" % i)]
        cc = cam.camera_component
        setp(cc, "field_of_view", 62.0)

    # --- Sky, sun, fog, post ------------------------------------------------------------
    sun = spawn(unreal.DirectionalLight, "Sun", (0, 0, 2000), rot(pitch=B["sun"][0], yaw=B["sun"][1]), "Lighting")
    sl = sun.light_component
    setp(sl, "atmosphere_sun_light", True)
    setp(sl, "intensity", B["sun_lux"])
    setp(sl, "light_color", unreal.Color(r=B["sun_color"][0], g=B["sun_color"][1], b=B["sun_color"][2], a=255))
    setp(sl, "enable_light_shaft_bloom", True)
    setp(sl, "bloom_scale", 0.08)
    setp(sl, "volumetric_scattering_intensity", 2.0)
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)

    if B["atmosphere"]:
        sky_atmo = spawn(unreal.SkyAtmosphere, "SkyAtmosphere", (0, 0, 0), folder="Lighting")
        ac = sky_atmo.get_component_by_class(unreal.SkyAtmosphereComponent)
        setp(ac, "mie_scattering_scale", B["mie"])      # dust / haze level
        setp(ac, "mie_anisotropy", 0.8)

    sky = spawn(unreal.SkyLight, "SkyLight", (0, 0, 800), folder="Lighting")
    setp(sky.light_component, "real_time_capture", B["atmosphere"] or B["clouds"])   # real-time capture needs a sky to capture
    setp(sky.light_component, "intensity", B["sky_light"])
    sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)

    fog = spawn(unreal.ExponentialHeightFog, "HeightFog", (0, 0, -200), folder="Lighting")
    fc = fog.component
    setp(fc, "fog_density", B["fog"])
    setp(fc, "fog_height_falloff", 0.2)
    setp(fc, "fog_inscattering_luminance", unreal.LinearColor(B["fog_color"][0], B["fog_color"][1], B["fog_color"][2], 1.0))
    setp(fc, "enable_volumetric_fog", True)
    setp(fc, "volumetric_fog_scattering_distribution", 0.75)
    setp(fc, "volumetric_fog_albedo", unreal.Color(r=255, g=220, b=190, a=255))
    setp(fc, "volumetric_fog_extinction_scale", 0.6)
    if B["clouds"]:
        spawn(unreal.VolumetricCloud, "Clouds", (0, 0, 0), folder="Lighting")

    pp = spawn(unreal.PostProcessVolume, "GlobalPost", (0, 0, 0), folder="Lighting")
    pp.set_editor_property("unbound", True)
    s = pp.settings

    def setpp(name, value):
        try:
            s.set_editor_property("override_" + name, True)
            s.set_editor_property(name, value)
        except Exception as err:   # property names drift between engine versions; skip rather than fail
            log("post setting skipped: %s (%s)" % (name, err))

    setpp("auto_exposure_bias", B["exposure"])
    setpp("color_saturation", unreal.Vector4(1.0, 1.0, 1.0, B["saturation"]))
    setpp("color_contrast", unreal.Vector4(1.0, 1.0, 1.0, B["contrast"]))
    setpp("bloom_intensity", 0.9)
    setpp("bloom_threshold", 1.2)
    setpp("lens_flare_intensity", 0.15)
    setpp("vignette_intensity", 0.5)
    setpp("film_grain_intensity", 0.12)
    setpp("scene_fringe_intensity", 0.25)
    setpp("motion_blur_amount", 0.35)
    setpp("ambient_occlusion_intensity", 0.6)
    setpp("lumen_final_gather_quality", 2.0)
    setpp("lumen_reflection_quality", 2.0)
    setpp("lumen_scene_lighting_quality", 2.0)
    setpp("lumen_ray_lighting_mode", unreal.LumenRayLightingModeOverride.HIT_LIGHTING_FOR_REFLECTIONS)
    pp.settings = s

    build_extras(mats, rocks, rock_mat)
    if brass:
        build_brasshaven(mats)
    if town:
        build_town(mats)
    if "walker" in B["extras"]:
        # The Ironstride strides a loop around the centre of the plain (see ADVWalker).
        spawn(unreal.load_class(None, "/Script/DeltaVDominion.DVWalker"), "Ironstride", (0, 0, ground(0, 0) + 2200), rot(), "Gameplay")

    if not levels.save_current_level():
        raise RuntimeError("Could not save " + MAP_PATH)
    unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "RebuildNavigation")
    log("OK actors=%d" % len(actors.get_all_level_actors()))


def selected_biomes():
    for a in sys.argv:
        if a.startswith("-biome="):
            name = a.split("=", 1)[1]
            return list(BIOMES.keys()) if name == "all" else [name]
    return ["mars"]


try:
    for biome in selected_biomes():
        use_biome(biome)
        log("building %s -> %s" % (biome, MAP_PATH))
        build()
finally:
    if "-quit" in sys.argv:
        unreal.SystemLibrary.quit_editor()
