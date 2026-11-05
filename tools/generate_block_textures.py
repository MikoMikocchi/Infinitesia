#!/usr/bin/env python3
"""Generates the 32x32 block textures into assets/textures/blocks/.

One seamless PNG per fluid, loose ground, ice, rock and built material of assets/matter/matter.xml.
File name = material id (granite.png); a face of its own is <id>_top, <id>_side, <id>_bottom.
Minerals get no tile: a mineral block is drawn in its catalog colour.
Standard library only. Colours are deliberately loud (GregTech style): the catalog
colour is saturated, and near-grey rocks get a hue derived from their id so that every
rock is recognisable at a glance. Output is deterministic.

Usage: python tools/GenerateBlockTextures.py [--sheet preview.png]
"""
import colorsys
import math
import random
import struct
import sys
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

SIZE = 32
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "textures" / "blocks"


# ---------------------------------------------------------------- png / colour helpers
def write_png(path, pixels, width=SIZE, height=SIZE):
    raw = b"".join(
        b"\x00" + bytes(c for px in pixels[y * width:(y + 1) * width] for c in (*px, 255))
        for y in range(height))

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    path.write_bytes(png)


def hex_rgb(value):
    value = value.lstrip("#")
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def clamp8(v):
    return max(0, min(255, int(round(v))))


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def shade(rgb, k):
    return tuple(clamp8(c * k) for c in rgb)


def hsv_rgb(h, s, v):
    return tuple(clamp8(c * 255) for c in colorsys.hsv_to_rgb(h % 1.0, s, v))


def loud(rgb, material_id, keep_dark=False):
    """Pushes a realistic catalog colour towards a distinct, saturated one."""
    h, s, v = colorsys.rgb_to_hsv(*(c / 255 for c in rgb))
    if keep_dark:
        return hsv_rgb(h, min(1.0, s * 1.2 + 0.05), max(v, 0.10))
    if s < 0.16:
        h = (zlib.crc32(material_id.encode()) % 360) / 360.0
        s = 0.34
    else:
        s = min(1.0, s * 1.45 + 0.08)
    return hsv_rgb(h, s, max(0.28, min(0.95, v * 1.05)))


def accent_of(rgb, material_id):
    """A contrasting speckle colour, hue-rotated from the base."""
    h, s, v = colorsys.rgb_to_hsv(*(c / 255 for c in rgb))
    spin = 0.18 + (zlib.crc32(material_id.encode()) % 100) / 100.0 * 0.25
    return hsv_rgb(h + spin, min(1.0, s + 0.25), min(1.0, v * 1.15 + 0.1))


# ------------------------------------------------------------------ seamless noise
def make_noise(rng, cells):
    lattice = [[rng.random() for _ in range(cells)] for _ in range(cells)]

    def sample(x, y):
        fx, fy = x * cells / SIZE, y * cells / SIZE
        ix, iy = int(fx), int(fy)
        tx, ty = fx - ix, fy - iy
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        x0, x1 = ix % cells, (ix + 1) % cells
        y0, y1 = iy % cells, (iy + 1) % cells
        top = lattice[y0][x0] * (1 - tx) + lattice[y0][x1] * tx
        bottom = lattice[y1][x0] * (1 - tx) + lattice[y1][x1] * tx
        return top * (1 - ty) + bottom * ty

    return sample


def wrap_dist2(ax, ay, bx, by):
    dx = abs(ax - bx)
    dy = abs(ay - by)
    dx = min(dx, SIZE - dx)
    dy = min(dy, SIZE - dy)
    return dx * dx + dy * dy


class Canvas:
    def __init__(self, base, material_id):
        self.rng = random.Random(zlib.crc32(material_id.encode()))
        self.base = base
        self.dark = shade(base, 0.62)
        self.light = tuple(clamp8(c * 1.22 + 14) for c in base)
        self.accent = accent_of(base, material_id)
        self.px = [base] * (SIZE * SIZE)

    def put(self, x, y, rgb):
        self.px[(y % SIZE) * SIZE + (x % SIZE)] = tuple(clamp8(c) for c in rgb)

    def tone(self, t):
        return lerp(self.dark, self.light, max(0.0, min(1.0, t)))

    def fill_noise(self, cells=(4, 8), weights=(0.6, 0.4), spread=1.0, bias=0.5):
        layers = [make_noise(self.rng, c) for c in cells]
        for y in range(SIZE):
            for x in range(SIZE):
                n = sum(w * f(x, y) for w, f in zip(weights, layers))
                self.put(x, y, self.tone(bias + (n - 0.5) * spread))

    def speckles(self, count, rgb):
        for _ in range(count):
            self.put(self.rng.randrange(SIZE), self.rng.randrange(SIZE), rgb)

    def blobs(self, count, rmin, rmax, rgb_fn):
        for _ in range(count):
            cx, cy = self.rng.randrange(SIZE), self.rng.randrange(SIZE)
            r = self.rng.uniform(rmin, rmax)
            rgb = rgb_fn()
            for y in range(cy - int(r) - 1, cy + int(r) + 2):
                for x in range(cx - int(r) - 1, cx + int(r) + 2):
                    if wrap_dist2(x % SIZE, y % SIZE, cx, cy) <= r * r:
                        self.put(x, y, rgb)


# ------------------------------------------------------------------------- families
def f_granular(c):
    c.fill_noise((4, 8), (0.5, 0.5), 0.45)
    c.speckles(110, shade(c.base, 0.45))
    c.speckles(90, c.light)
    c.speckles(40, c.accent)


def f_dense(c):
    c.fill_noise((3, 6), (0.7, 0.3), 0.55)
    for _ in range(2):
        x, y = c.rng.randrange(SIZE), c.rng.randrange(SIZE)
        for _ in range(11):
            c.put(x, y, shade(c.base, 0.45))
            x += c.rng.choice((-1, 0, 1, 1))
            y += c.rng.choice((-1, 0, 1))
    c.speckles(25, c.light)


def f_porous(c):
    c.fill_noise((4, 8), (0.5, 0.5), 0.5)
    c.blobs(16, 0.8, 2.0, lambda: shade(c.base, 0.38))
    c.speckles(30, c.accent)


def f_clastic(c, count=9, rmin=3.0, rmax=5.0):
    c.fill_noise((4, 8), (0.5, 0.5), 0.35, 0.3)
    for _ in range(count):
        edge = shade(c.base, 0.5)
        fill = lerp(c.dark, c.light, c.rng.random())
        if c.rng.random() < 0.25:
            fill = c.accent
        cx, cy = c.rng.randrange(SIZE), c.rng.randrange(SIZE)
        r = c.rng.uniform(rmin, rmax)
        for y in range(cy - int(r) - 1, cy + int(r) + 2):
            for x in range(cx - int(r) - 1, cx + int(r) + 2):
                d = wrap_dist2(x % SIZE, y % SIZE, cx, cy)
                if d <= r * r:
                    c.put(x, y, edge if d > (r - 1.2) ** 2 else fill)


def f_layered(c):
    warp = make_noise(c.rng, 4)
    fine = make_noise(c.rng, 16)
    tones = [c.rng.random() for _ in range(9)]
    for y in range(SIZE):
        for x in range(SIZE):
            band = int((y + 3.5 * warp(x, y)) / 3.5) % 9
            t = 0.2 + tones[band] * 0.7 + (fine(x, y) - 0.5) * 0.25
            c.put(x, y, c.tone(t))
    c.speckles(18, c.accent)


def f_banded(c):
    warp = make_noise(c.rng, 4)
    k = c.rng.choice((2, 3, 4))
    for y in range(SIZE):
        for x in range(SIZE):
            phase = (y + 4.0 * warp(x, y) + 0.5 * x) / SIZE * k * 2 * math.pi
            s = math.sin(phase)
            c.put(x, y, c.accent if s > 0.82 else c.tone(0.5 + 0.5 * s))
    c.speckles(30, shade(c.base, 0.5))


def f_schist(c):
    c.fill_noise((4, 8), (0.5, 0.5), 0.4)
    for _ in range(34):
        x, y = c.rng.randrange(SIZE), c.rng.randrange(SIZE)
        rgb = c.accent if c.rng.random() < 0.3 else c.light
        for i in range(c.rng.randint(2, 4)):
            c.put(x + i, y - i // 2, rgb)


def f_crystal(c, edge_light=False):
    seeds = [(c.rng.random() * SIZE, c.rng.random() * SIZE, c.rng.random()) for _ in range(11)]
    for y in range(SIZE):
        for x in range(SIZE):
            ranked = sorted((wrap_dist2(x, y, sx, sy), t) for sx, sy, t in seeds)
            (d1, t1), (d2, _) = ranked[0], ranked[1]
            if math.sqrt(d2) - math.sqrt(d1) < 1.0:
                c.put(x, y, c.light if edge_light else shade(c.base, 0.5))
            else:
                c.put(x, y, c.tone(0.25 + t1 * 0.7))
    c.speckles(10, c.accent)


def f_coal(c):
    c.fill_noise((4, 8), (0.5, 0.5), 0.5, 0.25)
    for _ in range(14):
        x, y = c.rng.randrange(SIZE), c.rng.randrange(SIZE)
        c.put(x, y, c.light)
        c.put(x + 1, y, shade(c.light, 0.8))
    c.speckles(14, c.accent)


def f_soft(c):
    c.fill_noise((6, 12), (0.5, 0.5), 0.3, 0.62)
    c.speckles(40, shade(c.base, 0.7))
    c.speckles(30, c.light)


def f_sand(c):
    c.fill_noise((8, 16), (0.5, 0.5), 0.3)
    c.speckles(120, c.light)
    c.speckles(80, c.dark)


def f_gravel(c):
    f_clastic(c, 16, 1.8, 3.0)


def f_soil(c):
    c.fill_noise((6, 12), (0.5, 0.5), 0.45)
    c.speckles(60, c.dark)
    c.speckles(25, c.light)


def f_fibre(c):
    c.fill_noise((4, 8), (0.5, 0.5), 0.4, 0.4)
    for _ in range(40):
        x, y = c.rng.randrange(SIZE), c.rng.randrange(SIZE)
        rgb = c.light if c.rng.random() < 0.5 else c.dark
        for i in range(c.rng.randint(3, 6)):
            c.put(x + i, y, rgb)


def f_ice(c):
    f_crystal(c, edge_light=True)


def f_wave(c):
    warp = make_noise(c.rng, 4)
    for y in range(SIZE):
        for x in range(SIZE):
            s = math.sin((y + 3.0 * warp(x, y)) / SIZE * 4 * 2 * math.pi)
            c.put(x, y, c.tone(0.5 + 0.35 * s))
    c.speckles(14, c.light)


def f_magma(c):
    f_crystal(c)
    c.speckles(30, (255, 220, 90))


def f_grass_top(c):
    c.fill_noise((6, 12), (0.5, 0.5), 0.45)
    c.speckles(70, c.light)
    c.speckles(40, c.dark)


FAMILIES = {
    "granular": f_granular, "dense": f_dense, "porous": f_porous, "clastic": f_clastic,
    "layered": f_layered, "banded": f_banded, "schist": f_schist, "crystal": f_crystal,
    "coal": f_coal, "soft": f_soft, "sand": f_sand, "gravel": f_gravel, "soil": f_soil,
    "fibre": f_fibre, "ice": f_ice, "wave": f_wave, "magma": f_magma,
}

BY_CLASS = {
    "igneous_intrusive": "granular", "igneous_extrusive": "dense",
    "volcaniclastic": "porous", "sedimentary_clastic": "layered",
    "sedimentary_chemical": "soft", "sedimentary_biogenic": "soft",
    "metamorphic": "banded",
}

BY_ROCK = {
    "obsidian": "coal", "volcanic_breccia": "clastic",
    "hyaloclastite": "clastic", "conglomerate": "clastic",
    "sedimentary_breccia": "clastic", "tillite": "clastic",
    "fault_breccia": "clastic", "fault_gouge": "soil",
    "rock_salt": "crystal", "sylvinite": "crystal", "carnallitite": "crystal",
    "trona_rock": "crystal", "gypsum_rock": "crystal",
    "anhydrite_rock": "crystal", "travertine": "layered",
    "oolitic_limestone": "porous", "chert": "dense", "phosphorite": "clastic",
    "biff": "banded", "oolitic_ironstone": "porous", "manganese_rock": "dense",
    "lignite": "coal", "subbituminous_coal": "coal", "bituminous_coal": "coal",
    "anthracite": "coal", "oil_shale": "layered", "diatomite": "soft",
    "bauxite": "porous", "laterite_crust": "porous", "calcrete": "soft",
    "silcrete": "dense", "slate": "layered", "phyllite": "layered",
    "mica_schist": "schist", "chlorite_schist": "schist",
    "garnet_schist": "schist", "graphite_schist": "schist",
    "greenschist": "schist", "blueschist": "schist", "talc_schist": "schist",
    "marble": "crystal", "quartzite": "crystal", "skarn": "crystal",
    "calc_silicate": "crystal", "hornfels": "dense", "serpentinite": "dense",
    "eclogite": "crystal", "carbonatite": "crystal", "pegmatite": "crystal",
    "kimberlite": "clastic", "pillow_basalt": "clastic",
    "marl": "soft", "chalk": "soft",
}

SOILS = {
    "humus": "soil", "subsoil": "soil", "clay": "soil", "silt": "soil",
    "sand": "sand", "gravel": "gravel", "peat": "fibre", "loess": "soil",
    "till": "gravel", "saprolite": "soil", "laterite": "soil",
    "alluvium": "sand", "scree": "gravel", "glacier": "ice",
    "permafrost": "gravel",
}

# Built materials of assets/matter/matter.xml.
BUILT = {
    "planks": "layered", "brick": "clastic", "glass": "ice",
    "glass_red": "ice", "glass_green": "ice", "glass_blue": "ice",
    "lamp": "crystal",
}

FLUIDS = {"water": "wave", "brine": "wave", "oil": "wave",
          "magma": "magma"}


def render(material_id, family, rgb, keep_dark=False):
    canvas = Canvas(loud(rgb, material_id, keep_dark), material_id)
    FAMILIES[family](canvas)
    return canvas.px


def grass_side(name, green_rgb):
    c = Canvas((139, 94, 60), name)
    f_soil(c)
    edge = make_noise(c.rng, 8)
    green = Canvas(green_rgb, name)
    f_grass_top(green)
    for y in range(SIZE):
        for x in range(SIZE):
            if y < 7 + int(edge(x, 0) * 4):
                c.px[y * SIZE + x] = green.px[y * SIZE + x]
    return c.px


def main():
    sheet_path = None
    if "--sheet" in sys.argv:
        sheet_path = Path(sys.argv[sys.argv.index("--sheet") + 1])
    OUT.mkdir(parents=True, exist_ok=True)
    textures = {}

    materials = ET.parse(ROOT / "assets/matter/matter.xml").getroot()
    for m in materials.findall("matter"):
        key, kind = m.get("id"), m.get("class")
        color = m.get("color")
        if key == "turf":
            # Grass-covered ground: one tile per face (docs/06_BLOCKS.md, section 3.5).
            green = hex_rgb(color)
            top = Canvas(green, "turf_top")
            f_grass_top(top)
            textures["turf_top"] = top.px
            textures["turf_side"] = grass_side("turf_side", green)
            textures["turf_bottom"] = render("turf_bottom", "soil", (139, 94, 60))
        elif kind == "rock":
            family = BY_ROCK.get(key, BY_CLASS[m.find("rock").get("class")])
            textures[key] = render(key, family, hex_rgb(color), keep_dark=family == "coal")
        elif kind == "built":
            textures[key] = render(key, BUILT.get(key, "dense"), hex_rgb(color))
        elif kind in ("loose", "ice", "fluid"):
            family = SOILS.get(key) or FLUIDS.get(key)
            if family:
                textures[key] = render(key, family, hex_rgb(color), keep_dark=key == "oil")

    for name, pixels in textures.items():
        write_png(OUT / f"{name}.png", pixels)
    print(f"wrote {len(textures)} textures to {OUT}")

    if sheet_path:
        per_row, scale = 16, 2
        names = sorted(textures)
        rows = (len(names) + per_row - 1) // per_row
        w, h = per_row * SIZE * scale, rows * SIZE * scale
        grid = [(30, 30, 30)] * (w * h)
        for i, name in enumerate(names):
            ox, oy = (i % per_row) * SIZE * scale, (i // per_row) * SIZE * scale
            for y in range(SIZE * scale):
                for x in range(SIZE * scale):
                    grid[(oy + y) * w + ox + x] = textures[name][(y // scale) * SIZE + x // scale]
        write_png(sheet_path, grid, w, h)
        (sheet_path.with_suffix(".txt")).write_text("\n".join(names), "utf-8")
        print(f"sheet {w}x{h}: {sheet_path} (order in .txt)")


if __name__ == "__main__":
    main()
