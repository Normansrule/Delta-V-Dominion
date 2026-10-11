"""Generates equirectangular planet surface textures (Art/Planets/*.png) for the orbital views.

Runs with ordinary Python 3 + numpy + pillow (python -m pip install numpy pillow), outside Unreal:
  python Scripts/make_planet_textures.py
build_kestrel_hub.py imports the PNGs and wraps them on a lat-long sphere. Noise is sampled on the
unit sphere in 3D, so there is no seam at the date line and no pinching at the poles.
"""

import os

import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Art", "Planets")
W, H = 2048, 1024


def sphere_dirs():
    lon = (np.arange(W) + 0.5) / W * 2 * np.pi - np.pi
    lat = np.pi / 2 - (np.arange(H) + 0.5) / H * np.pi
    lon, lat = np.meshgrid(lon, lat)
    return np.stack([np.cos(lat) * np.cos(lon), np.cos(lat) * np.sin(lon), np.sin(lat)], axis=-1), lat, lon


def hash3(ix, iy, iz, seed):
    h = (ix * 374761393 + iy * 668265263 + iz * 2147483647 + seed * 144269504) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0


def value3(p, seed):
    i = np.floor(p).astype(np.int64)
    f = p - i
    u = f * f * (3 - 2 * f)
    out = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (u[..., 0] if dx else 1 - u[..., 0]) * (u[..., 1] if dy else 1 - u[..., 1]) * (u[..., 2] if dz else 1 - u[..., 2])
                out = out + w * hash3(i[..., 0] + dx, i[..., 1] + dy, i[..., 2] + dz, seed)
    return out


def fbm(p, octaves, seed, gain=0.5):
    total, amp, norm = 0.0, 1.0, 0.0
    for o in range(octaves):
        total = total + amp * value3(p * (2 ** o), seed + o * 17)
        norm += amp
        amp *= gain
    return total / norm


def smooth(e0, e1, v):
    t = np.clip((v - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def mars():
    d, lat, lon = sphere_dirs()
    rng = np.random.default_rng(7)
    # Warp the continents a little so they don't look like blobs.
    warp = np.stack([fbm(d * 2.0 + 11, 4, 3), fbm(d * 2.0 + 23, 4, 5), fbm(d * 2.0 + 37, 4, 9)], axis=-1) - 0.5
    cont = fbm(d * 1.6 + warp * 0.9, 6, 1)
    dark = smooth(0.53, 0.68, cont)                              # Syrtis-like dark basalt provinces
    mottling = fbm(d * 9.0, 5, 41)
    fine = fbm(d * 40.0, 4, 77)

    plains = np.array([0.74, 0.40, 0.20])
    dust = np.array([0.86, 0.56, 0.32])
    basalt = np.array([0.30, 0.17, 0.11])
    col = plains[None, None, :] * (1 - mottling[..., None]) + dust[None, None, :] * mottling[..., None]
    col = col * (1 - dark[..., None] * 0.6) + basalt[None, None, :] * (dark[..., None] * 0.6)
    col *= (0.85 + 0.3 * fine)[..., None]

    # A great canyon system near the equator (Valles-style), with ragged walls.
    canyon_lat = 0.12 * np.sin(lon * 3.0) - 0.15
    width = 0.03 + 0.05 * fbm(d * 7.0, 4, 91)
    along = smooth(-1.9, -1.6, lon) * (1 - smooth(-0.6, -0.3, lon))
    canyon = (1 - smooth(0, 1, np.abs(lat - canyon_lat) / width)) * along
    col = col * (1 - canyon[..., None] * 0.4)

    # Craters: dark floors, bright rims, more of them in the southern highlands.
    for _ in range(420):
        z = rng.uniform(-1, 0.7)
        a = rng.uniform(0, 2 * np.pi)
        c = np.array([np.sqrt(1 - z * z) * np.cos(a), np.sqrt(1 - z * z) * np.sin(a), z])
        r = rng.choice([0.008, 0.012, 0.02, 0.035, 0.06], p=[0.45, 0.25, 0.15, 0.1, 0.05])
        ang = np.arccos(np.clip(d @ c, -1, 1))
        floor = 1 - smooth(r * 0.7, r, ang)
        rim = np.exp(-((ang - r) / (r * 0.18)) ** 2)
        col = col * (1 - floor[..., None] * 0.12) + rim[..., None] * 0.035

    # Polar caps with ragged, swirled edges.
    edge = 1.36 + 0.12 * (fbm(d * 5.0, 4, 123) - 0.5)
    cap = smooth(edge, edge + 0.04, np.abs(lat))
    ice = np.array([0.95, 0.94, 0.92])
    col = col * (1 - cap[..., None]) + ice[None, None, :] * cap[..., None]
    return np.clip(col, 0, 1)


def main():
    os.makedirs(OUT, exist_ok=True)
    img = (mars() ** (1 / 1.0) * 255).astype(np.uint8)
    path = os.path.join(OUT, "T_Mars.png")
    Image.fromarray(img, "RGB").save(path)
    print("wrote", os.path.normpath(path))


if __name__ == "__main__":
    main()
