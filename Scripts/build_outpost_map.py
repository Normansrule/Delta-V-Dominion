"""Builds /Game/Maps/Outpost: Hearthbound outpost on an arid moon at dusk (milestone 1 combat map).

Everything is generated: Nanite terrain and rocks (GeometryScript), PBR master materials, a fortified
compound kit-bashed from engine meshes, floodlights, and cinematic lighting/post. Re-run any time:
  UnrealEditor-Cmd.exe DeltaVDominion.uproject -ExecutePythonScript="<path>/build_outpost_map.py -quit" -nullrhi -unattended
(the script path must not contain spaces). Pass -rebuild to regenerate the meshes and materials too.

Units are cm. +X points from the player's start (west) toward the compound (east).
"""

import math
import random
import sys

import unreal

MAP_PATH = "/Game/Maps/Outpost"
ENV = "/Game/Environment/Outpost"
REBUILD = "-rebuild" in sys.argv

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
gs = unreal

random.seed(7)


def log(msg):
    unreal.log("OUTPOST " + msg)


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
    ground = (fbm(x / 2400.0, y / 2400.0, 4, 3) - 0.5) * 90.0          # gentle dunes in the arena
    rim = smoothstep(1.0, 1.7, q)
    mesa = ridged(x / 9000.0, y / 9000.0, 4, 11)
    mountains = rim * (900.0 + 2600.0 * mesa) + rim * (fbm(x / 1500.0, y / 1500.0, 5, 5) - 0.5) * 600.0
    return ground + mountains


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
    sand_a = const3(mat, (0.26, 0.155, 0.09), -1200, -560)
    sand_b = const3(mat, (0.085, 0.06, 0.045), -1200, -440)
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
    rock = const3(mat, (0.05, 0.042, 0.036), -1100, 0)
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
    dark = const3(mat, (0.022, 0.02, 0.018), -1000, -450)
    light = const3(mat, (0.12, 0.095, 0.075), -1000, -330)
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
    path = ENV + "/Materials/" + name
    if lib.does_asset_exist(path) and not REBUILD:
        return lib.load_asset(path)
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    mi = tools.create_asset(name, ENV + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
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
    path = ENV + "/Materials/" + name
    if lib.does_asset_exist(path) and not REBUILD:
        return lib.load_asset(path)
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    mi = tools.create_asset(name, ENV + "/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
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

    # --- Terrain and rocks --------------------------------------------------------------
    place(terrain, "MoonTerrain", (0, 0, 0), material=terrain_mat, folder="Terrain")

    # Boulders as cover across the approach, plus big outcrops on the slopes.
    cover_rocks = [(-2600, -1300), (-2300, 900), (-1600, -300), (-900, 1500), (-700, -1600), (-200, 400),
                   (300, -900), (700, 1700), (1100, -2100), (-3300, 300), (-1200, -2600), (200, 2600)]
    for i, (x, y) in enumerate(cover_rocks):
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
    spawn(unreal.PlayerStart, "PlayerStart", (-4300, 0, ground(-4300, 0) + 110))
    soldier = unreal.load_class(None, "/Script/DeltaVDominion.DVSoldierCharacter")
    enemy_spots = [(bx - 900, by - 250), (bx - 900, by + 450), (bx - 1300, by + 1000), (bx - 1200, by - 1100),
                   (bx - 300, by + 1250), (bx - 450, by), (1300, -500), (1400, 800)]
    for i, (x, y) in enumerate(enemy_spots):
        e = spawn(soldier, "Hearthbound_%02d" % i, (x, y, ground(x, y) + 105), rot(yaw=180), "Enemies")
        e.set_editor_property("team_id", 2)

    nav = spawn(unreal.NavMeshBoundsVolume, "NavMeshBounds", (0, 0, 300), folder="Navigation")
    ext = nav.get_actor_bounds(False)[1]
    if ext.x > 1.0:
        nav.set_actor_scale3d(unreal.Vector(5600.0 / ext.x, 4200.0 / ext.y, 1200.0 / ext.z))
    spawn(unreal.RecastNavMesh, "RecastNavMesh-Default", (0, 0, 0), folder="Navigation")

    # Showcase cameras ("DVShowcase N").
    shots = [((-2400, -2900, 700), (2600, 200, 150)),        # 0: overwatch over the approach toward the compound at dusk
             ((-2200, -1500, 220), (2600, 300, 220)),        # 1: low, behind the boulders, looking at the bunker
             ((5600, 2600, 900), (-800, -400, 0)),           # 2: reverse from behind the compound, sun in the lens
             ((bx - 1700, by - 700, 170), (bx - 600, by + 100, 250))]  # 3: close on the bunker doorway
    for i, (eye, target) in enumerate(shots):
        cam = spawn(unreal.CameraActor, "ShowcaseCam_%d" % i, (eye[0], eye[1], eye[2] + ground(eye[0], eye[1])), folder="Cameras")
        cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(eye[0], eye[1], eye[2] + ground(eye[0], eye[1])),
                                                                        unreal.Vector(target[0], target[1], target[2] + ground(target[0], target[1]))), False)
        cam.tags = [unreal.Name("Showcase%d" % i)]
        cc = cam.camera_component
        setp(cc, "field_of_view", 62.0)

    # --- Sky, sun, fog, post ------------------------------------------------------------
    sun = spawn(unreal.DirectionalLight, "Sun", (0, 0, 2000), rot(pitch=-21, yaw=38), "Lighting")
    sl = sun.light_component
    setp(sl, "atmosphere_sun_light", True)
    setp(sl, "intensity", 9.0)
    setp(sl, "light_color", unreal.Color(r=255, g=196, b=150, a=255))
    setp(sl, "enable_light_shaft_bloom", True)
    setp(sl, "bloom_scale", 0.08)
    setp(sl, "volumetric_scattering_intensity", 2.0)
    sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)

    sky_atmo = spawn(unreal.SkyAtmosphere, "SkyAtmosphere", (0, 0, 0), folder="Lighting")
    ac = sky_atmo.get_component_by_class(unreal.SkyAtmosphereComponent)
    setp(ac, "mie_scattering_scale", 0.008)      # a little dust haze over the default Earth-like sky
    setp(ac, "mie_anisotropy", 0.8)

    sky = spawn(unreal.SkyLight, "SkyLight", (0, 0, 800), folder="Lighting")
    setp(sky.light_component, "real_time_capture", True)
    setp(sky.light_component, "intensity", 1.2)
    sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)

    fog = spawn(unreal.ExponentialHeightFog, "HeightFog", (0, 0, -200), folder="Lighting")
    fc = fog.component
    setp(fc, "fog_density", 0.012)
    setp(fc, "fog_height_falloff", 0.2)
    setp(fc, "fog_inscattering_luminance", unreal.LinearColor(0.35, 0.22, 0.16, 1.0))
    setp(fc, "enable_volumetric_fog", True)
    setp(fc, "volumetric_fog_scattering_distribution", 0.75)
    setp(fc, "volumetric_fog_albedo", unreal.Color(r=255, g=220, b=190, a=255))
    setp(fc, "volumetric_fog_extinction_scale", 0.6)
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

    setpp("auto_exposure_bias", -0.2)
    setpp("color_saturation", unreal.Vector4(1.0, 1.0, 1.0, 1.08))
    setpp("color_contrast", unreal.Vector4(1.0, 1.0, 1.0, 1.15))
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

    if not levels.save_current_level():
        raise RuntimeError("Could not save " + MAP_PATH)
    unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "RebuildNavigation")
    log("OK actors=%d" % len(actors.get_all_level_actors()))


try:
    build()
finally:
    if "-quit" in sys.argv:
        unreal.SystemLibrary.quit_editor()
