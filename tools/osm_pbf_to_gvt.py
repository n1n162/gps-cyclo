#!/usr/bin/env python3
"""Convertit un fichier OpenStreetMap .osm.pbf en carte vectorielle GVT1 v2.

Usage :
  python3 tools/osm_pbf_to_gvt.py france-latest.osm.pbf france.gvt
  python3 tools/osm_pbf_to_gvt.py france-latest.osm.pbf france.gvt --zmin 11 --zmax 15

Les lignes et surfaces sont extraites en flux avec pyosmium et stockées temporairement
dans SQLite. Les noms de villes restent dans la couche GVL1 produite séparément par
tools/osm_pbf_to_gvl.py.

Dépendance : python3 -m pip install osmium
"""

import argparse
import bisect
import math
import os
import sqlite3
import struct
import sys
import tempfile

try:
    import osmium
except ImportError:
    raise SystemExit("Module osmium absent. Installer avec : python3 -m pip install osmium")

MAGIC = b'GVT1'
VERSION = 2            # 1 : lignes seulement ; 2 : ajoute les surfaces
EXTENT = 4096
BUFFER = 64
MAX_POINTS = 1024
MAX_AREA_POINTS = 4096  # points d'une surface dans une tuile (tous anneaux)
MAX_RINGS = 64
SLOT_BYTES = 65536     # taille d'un emplacement du cache de l'appareil (MAPVIEW_SLOT_BYTES dans core/mapview.h)
HEADER = struct.Struct('<4sHHBBHIIiiii')   # 36 octets
INDEX = struct.Struct('<QII')              # 16 octets : cle, decalage, taille
HEADER_SIZE = HEADER.size
INDEX_SIZE = INDEX.size

# Classes de rendu (stables, lues par le code C).
MAJOR, SECONDARY, LOCAL, STREET, CYCLEWAY, TRACK, RIVER, LAKE, RAIL = range(1, 10)
# Surfaces (classes 10 a 15)
URBAN, FOREST, FIELD, PARK, ROCK, WATER = range(10, 16)

# valeur de tag -> (classe, zoom minimal d'affichage)
HIGHWAY = {
    'motorway': (MAJOR, 11), 'trunk': (MAJOR, 11), 'primary': (MAJOR, 11),
    'motorway_link': (MAJOR, 12), 'trunk_link': (MAJOR, 12), 'primary_link': (MAJOR, 12),
    'secondary': (SECONDARY, 12), 'secondary_link': (SECONDARY, 13),
    'tertiary': (LOCAL, 13), 'tertiary_link': (LOCAL, 14),
    'unclassified': (STREET, 13), 'residential': (STREET, 14), 'living_street': (STREET, 14),
    'road': (STREET, 14), 'service': (STREET, 15),
    'cycleway': (CYCLEWAY, 13),
    'track': (TRACK, 14), 'path': (TRACK, 14), 'bridleway': (TRACK, 14),
    'footway': (TRACK, 15), 'steps': (TRACK, 15), 'pedestrian': (TRACK, 15),
}
WATERWAY = {'river': (RIVER, 12), 'canal': (RIVER, 12), 'stream': (RIVER, 14)}
RAILWAY = {'rail': (RAIL, 12), 'narrow_gauge': (RAIL, 13), 'light_rail': (RAIL, 13),
           'tram': (RAIL, 14), 'funicular': (RAIL, 14)}


AREA_LANDUSE = {'residential': (URBAN, 11), 'commercial': (URBAN, 11), 'retail': (URBAN, 11), 'industrial': (URBAN, 11),
                'forest': (FOREST, 11), 'farmland': (FIELD, 12), 'meadow': (FIELD, 12), 'grass': (FIELD, 13),
                'orchard': (FIELD, 13), 'vineyard': (FIELD, 13), 'allotments': (FIELD, 14), 'village_green': (FIELD, 13)}
AREA_NATURAL = {'wood': (FOREST, 11), 'scrub': (FIELD, 12), 'grassland': (FIELD, 12), 'heath': (FIELD, 12),
                'bare_rock': (ROCK, 11), 'scree': (ROCK, 11), 'glacier': (ROCK, 11), 'water': (WATER, 11)}
AREA_LEISURE = {'park': (PARK, 12), 'garden': (PARK, 13), 'recreation_ground': (PARK, 13), 'golf_course': (PARK, 13)}


def classify_area(tags):
    """Retourne (classe de surface, zoom minimal) ou None."""
    for key, table in (('landuse', AREA_LANDUSE), ('natural', AREA_NATURAL), ('leisure', AREA_LEISURE)):
        if key in tags and tags[key] in table:
            return table[tags[key]]
    return None


def classify(tags):
    """Retourne (classe, zoom minimal) ou None si l'objet n'est pas dessine."""
    if 'highway' in tags:
        return HIGHWAY.get(tags['highway'])
    if 'waterway' in tags:
        return WATERWAY.get(tags['waterway'])
    if tags.get('natural') in ('water', 'coastline'):
        return (LAKE, 12)
    if 'railway' in tags:
        return RAILWAY.get(tags['railway'])
    return None


def world(lon, lat, z):
    """Coordonnees monde au zoom z, en unites de tuile (4096 par tuile)."""
    n = (1 << z) * EXTENT
    lat = max(min(lat, 85.0511287798), -85.0511287798)
    s = math.sin(math.radians(lat))
    x = (lon + 180.0) / 360.0 * n
    y = (0.5 - math.log((1.0 + s) / (1.0 - s)) / (4.0 * math.pi)) * n
    return x, y


def simplify(pts, tol):
    """Douglas-Peucker iteratif ; les extremites sont toujours gardees."""
    n = len(pts)
    if n < 3:
        return pts
    keep = [False] * n
    keep[0] = keep[-1] = True
    stack = [(0, n - 1)]
    tol2 = tol * tol
    while stack:
        a, b = stack.pop()
        ax, ay = pts[a]
        bx, by = pts[b]
        dx, dy = bx - ax, by - ay
        l2 = dx * dx + dy * dy
        best, bi = -1.0, -1
        for i in range(a + 1, b):
            px, py = pts[i]
            if l2 == 0.0:
                d2 = (px - ax) ** 2 + (py - ay) ** 2
            else:
                t = ((px - ax) * dx + (py - ay) * dy) / l2
                t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
                d2 = (px - ax - t * dx) ** 2 + (py - ay - t * dy) ** 2
            if d2 > best:
                best, bi = d2, i
        if best > tol2:
            keep[bi] = True
            stack.append((a, bi))
            stack.append((bi, b))
    return [p for p, k in zip(pts, keep) if k]


def clip_segment(ax, ay, bx, by, x0, y0, x1, y1):
    """Liang-Barsky. Retourne (x0, y0, x1, y1, debut_intact, fin_intacte) ou None."""
    dx, dy = bx - ax, by - ay
    t0, t1 = 0.0, 1.0
    for p, q in ((-dx, ax - x0), (dx, x1 - ax), (-dy, ay - y0), (dy, y1 - ay)):
        if p == 0.0:
            if q < 0.0:
                return None
        else:
            r = q / p
            if p < 0.0:
                if r > t1:
                    return None
                if r > t0:
                    t0 = r
            else:
                if r < t0:
                    return None
                if r < t1:
                    t1 = r
    return (ax + t0 * dx, ay + t0 * dy, ax + t1 * dx, ay + t1 * dy, t0 == 0.0, t1 == 1.0)


def tile_polyline(poly, z):
    """Decoupe une polyligne (coordonnees monde) en morceaux par tuile : {(tx, ty): [[(x, y), ...], ...]}."""
    last = (1 << z) - 1
    state, pieces = {}, {}
    for i in range(len(poly) - 1):
        ax, ay = poly[i]
        bx, by = poly[i + 1]
        tx0 = max(0, int(math.floor((min(ax, bx) - BUFFER) / EXTENT)))
        tx1 = min(last, int(math.floor((max(ax, bx) + BUFFER) / EXTENT)))
        ty0 = max(0, int(math.floor((min(ay, by) - BUFFER) / EXTENT)))
        ty1 = min(last, int(math.floor((max(ay, by) + BUFFER) / EXTENT)))
        for ty in range(ty0, ty1 + 1):
            for tx in range(tx0, tx1 + 1):
                r = clip_segment(ax, ay, bx, by, tx * EXTENT - BUFFER, ty * EXTENT - BUFFER,
                                 (tx + 1) * EXTENT + BUFFER, (ty + 1) * EXTENT + BUFFER)
                if r is None:
                    continue
                cx0, cy0, cx1, cy1, s_in, e_in = r
                st = state.get((tx, ty))
                if st is not None and st[1] == i - 1 and st[2] and s_in:
                    st[0].append((cx1, cy1))
                    st[1], st[2] = i, e_in
                else:
                    if st is not None:
                        pieces.setdefault((tx, ty), []).append(st[0])
                    state[(tx, ty)] = [[(cx0, cy0), (cx1, cy1)], i, e_in]
    for key, st in state.items():
        pieces.setdefault(key, []).append(st[0])
    return pieces


def clip_ring(pts, x0, y0, x1, y1):
    """Sutherland-Hodgman : anneau (liste de points) decoupe par un rectangle. Les aretes confondues le long du bord
    s'annulent au remplissage pair-impair."""
    out = pts
    for axis, bound, keep_low in ((0, x0, False), (0, x1, True), (1, y0, False), (1, y1, True)):
        if not out:
            break
        inp, out = out, []
        prev = inp[-1]
        for cur in inp:
            cin = (cur[axis] <= bound) if keep_low else (cur[axis] >= bound)
            pin = (prev[axis] <= bound) if keep_low else (prev[axis] >= bound)
            if cin != pin:
                t = (bound - prev[axis]) / (cur[axis] - prev[axis])
                out.append((prev[0] + t * (cur[0] - prev[0]), prev[1] + t * (cur[1] - prev[1])))
            if cin:
                out.append(cur)
            prev = cur
    return out


def ring_area2(r):
    return sum(r[i][0] * r[(i + 1) % len(r)][1] - r[(i + 1) % len(r)][0] * r[i][1] for i in range(len(r)))


def fit_rings(rings):
    """Reduit les anneaux d'une surface pour qu'elle tienne dans les limites de l'appareil."""
    rings = sorted(rings, key=lambda r: -abs(ring_area2(r)))[:MAX_RINGS]
    tol = 2.0
    while rings and sum(len(r) for r in rings) > MAX_AREA_POINTS:
        tol *= 1.5
        new = []
        for r in rings:
            sm = simplify([(float(x), float(y)) for x, y in r] + [(float(r[0][0]), float(r[0][1]))], tol)[:-1]
            if len(sm) >= 3:
                new.append([(int(round(x)), int(round(y))) for x, y in sm])
        rings = new
    return rings


def area_tiles(rings, z, tol, min_size):
    """Decoupe une surface (anneaux lon/lat) en morceaux par tuile : {(tx, ty): [anneau en entiers locaux, ...]}.
    Tuiles de bord : decoupe de chaque anneau. Tuiles entierement a l'interieur : un rectangle plein."""
    wr = []
    for ring in rings:
        pts = []
        for lon, lat in ring:
            p = world(lon, lat, z)
            if not pts or pts[-1] != p:
                pts.append(p)
        if len(pts) > 1 and pts[0] == pts[-1]:
            pts.pop()
        if len(pts) < 3:
            continue
        sm = simplify(pts + [pts[0]], tol)[:-1]
        if len(sm) >= 3:
            wr.append(sm)
    if not wr:
        return {}
    xs = [p[0] for r in wr for p in r]
    ys = [p[1] for r in wr for p in r]
    if max(max(xs) - min(xs), max(ys) - min(ys)) < min_size:
        return {}
    last = (1 << z) - 1
    tx0 = max(0, int(math.floor((min(xs) - BUFFER) / EXTENT)))
    tx1 = min(last, int(math.floor((max(xs) + BUFFER) / EXTENT)))
    ty0 = max(0, int(math.floor((min(ys) - BUFFER) / EXTENT)))
    ty1 = min(last, int(math.floor((max(ys) + BUFFER) / EXTENT)))
    boundary = set()
    for r in wr:
        for i in range(len(r)):
            ax, ay = r[i]
            bx, by = r[(i + 1) % len(r)]
            for ty in range(max(ty0, int(math.floor((min(ay, by) - BUFFER) / EXTENT))), min(ty1, int(math.floor((max(ay, by) + BUFFER) / EXTENT))) + 1):
                for tx in range(max(tx0, int(math.floor((min(ax, bx) - BUFFER) / EXTENT))), min(tx1, int(math.floor((max(ax, bx) + BUFFER) / EXTENT))) + 1):
                    if (tx, ty) not in boundary and clip_segment(ax, ay, bx, by, tx * EXTENT - BUFFER, ty * EXTENT - BUFFER,
                                                                  (tx + 1) * EXTENT + BUFFER, (ty + 1) * EXTENT + BUFFER) is not None:
                        boundary.add((tx, ty))
    out = {}
    for (tx, ty) in boundary:
        rect = (tx * EXTENT - BUFFER, ty * EXTENT - BUFFER, (tx + 1) * EXTENT + BUFFER, (ty + 1) * EXTENT + BUFFER)
        loc = []
        for r in wr:
            c = clip_ring(r, *rect)
            if len(c) < 3:
                continue
            pts = []
            for x, y in c:
                p = (int(round(x - tx * EXTENT)), int(round(y - ty * EXTENT)))
                if not pts or pts[-1] != p:
                    pts.append(p)
            if len(pts) > 1 and pts[0] == pts[-1]:
                pts.pop()
            if len(pts) >= 3 and abs(ring_area2(pts)) >= 2:
                loc.append(pts)
        if loc:
            out[(tx, ty)] = loc
    full = [(-BUFFER, -BUFFER), (EXTENT + BUFFER, -BUFFER), (EXTENT + BUFFER, EXTENT + BUFFER), (-BUFFER, EXTENT + BUFFER)]
    for ty in range(ty0, ty1 + 1):
        cand = [tx for tx in range(tx0, tx1 + 1) if (tx, ty) not in boundary]
        if not cand:
            continue
        y = (ty + 0.5) * EXTENT
        crossings = []
        for r in wr:
            for i in range(len(r)):
                x1, y1 = r[i]
                x2, y2 = r[(i + 1) % len(r)]
                if (y1 <= y) != (y2 <= y):
                    crossings.append(x1 + (y - y1) * (x2 - x1) / (y2 - y1))
        crossings.sort()
        for tx in cand:
            if bisect.bisect_left(crossings, (tx + 0.5) * EXTENT) % 2 == 1:
                out[(tx, ty)] = [list(full)]
    fitted = {k: fit_rings(v) for k, v in out.items()}
    return {k: v for k, v in fitted.items() if v}


def to_local(piece, tx, ty):
    """Passe en entiers locaux a la tuile, retire les doublons consecutifs, decoupe en morceaux de MAX_POINTS."""
    pts = []
    for x, y in piece:
        p = (int(round(x - tx * EXTENT)), int(round(y - ty * EXTENT)))
        if not pts or pts[-1] != p:
            pts.append(p)
    out, k = [], 0
    while len(pts) - k >= 2:
        out.append(pts[k:k + MAX_POINTS])
        if k + MAX_POINTS >= len(pts):
            break
        k += MAX_POINTS - 1
    return out


def varint(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def zigzag(v):
    return (v << 1) if v >= 0 else (((-v) << 1) - 1)


def encode_points(pts):
    out = bytearray()
    px = py = 0
    for i, (x, y) in enumerate(pts):
        if i == 0:
            out += varint(zigzag(x)) + varint(zigzag(y))
        else:
            out += varint(zigzag(x - px)) + varint(zigzag(y - py))
        px, py = x, y
    return bytes(out)


def encode_tile(features):
    """features : [('L', classe, points) ou ('A', classe, anneaux), ...] ; tri par classe pour un ordre stable."""
    features = sorted(features, key=lambda f: f[1])
    out = bytearray(varint(len(features)))
    for kind, cls, geom in features:
        if kind == 'L':
            out += bytes((cls, 0)) + varint(len(geom)) + encode_points(geom)
        else:
            out += bytes((cls, 1)) + varint(len(geom))
            for ring in geom:
                out += varint(len(ring)) + encode_points(ring)
    return bytes(out)


def key_of(z, x, y):
    return (z << 32) | (x << 16) | y


def ring_coords(ring):
    pts = []
    for node in ring:
        try:
            p = (float(node.lon), float(node.lat))
        except (AttributeError, TypeError, ValueError):
            continue
        if not pts or pts[-1] != p:
            pts.append(p)
    if len(pts) >= 3:
        if pts[0] != pts[-1]:
            pts.append(pts[0])
        return pts
    return []


class PBFToGVT(osmium.SimpleHandler):
    def __init__(self, db, zmin, zmax, tolerance, area_tolerance, min_size, areas):
        super().__init__()
        self.db = db
        self.zmin = zmin
        self.zmax = zmax
        self.tolerance = tolerance
        self.area_tolerance = area_tolerance
        self.min_size = min_size
        self.areas = areas
        self.bounds = [180.0, 90.0, -180.0, -90.0]
        self.counts = {"ways_lignes": 0, "surfaces": 0, "objets_ignores": 0, "aires_osm": 0, "aires_non_classees": 0, "aires_sans_anneau": 0}
        self.inserted = 0

    def update_bounds(self, points):
        for lon, lat in points:
            self.bounds[0] = min(self.bounds[0], lon)
            self.bounds[1] = min(self.bounds[1], lat)
            self.bounds[2] = max(self.bounds[2], lon)
            self.bounds[3] = max(self.bounds[3], lat)

    def add_feature(self, kind, cls, geom, minzoom):
        # Chaque enregistrement est projeté et découpé séparément par niveau de zoom.
        for z in range(max(self.zmin, minzoom), self.zmax + 1):
            if kind == "L":
                pts = []
                for lon, lat in geom:
                    p = world(lon, lat, z)
                    if not pts or pts[-1] != p:
                        pts.append(p)
                if len(pts) < 2:
                    continue
                pts = simplify(pts, self.tolerance)
                if not pts:
                    continue
                xs, ys = [p[0] for p in pts], [p[1] for p in pts]
                if max(max(xs)-min(xs), max(ys)-min(ys)) < self.min_size:
                    continue
                pieces_by_tile = tile_polyline(pts, z)
                for (tx, ty), pieces in pieces_by_tile.items():
                    for piece in pieces:
                        for part in to_local(piece, tx, ty):
                            if len(part) >= 2:
                                self.store(z, tx, ty, ("L", cls, part))
            else:
                for (tx, ty), rings in area_tiles(
                    geom, z, self.area_tolerance, self.min_size
                ).items():
                    if rings:
                        self.store(z, tx, ty, ("A", cls, rings))

    def store(self, z, tx, ty, feature):
        # encode_tile([feature]) commence par le varint du nombre (1, donc un octet).
        encoded = encode_tile([feature])
        payload = encoded[1:]
        key = key_of(z, tx, ty)
        self.db.execute(
            "INSERT INTO records(tile_key, payload) VALUES (?, ?)",
            (key, sqlite3.Binary(payload))
        )
        self.inserted += 1
        if self.inserted % 20000 == 0:
            self.db.commit()
            print(f"  {self.inserted:,} fragments de tuiles indexés…", file=sys.stderr, flush=True)

    def way(self, way):
        tags = dict(way.tags)
        line = []
        try:
            for n in way.nodes:
                if not n.location.valid():
                    return
                line.append((float(n.lon), float(n.lat)))
        except (osmium.InvalidLocationError, AttributeError):
            return
        if len(line) < 2:
            return
        self.update_bounds(line)
        classification = classify(tags)
        if classification:
            self.counts["ways_lignes"] += 1
            self.add_feature("L", classification[0], line, classification[1])
        elif not classify_area(tags):
            self.counts["objets_ignores"] += 1

    def area(self, area):
        if not self.areas:
            return
        self.counts["aires_osm"] += 1
        tags = dict(area.tags)
        classification = classify_area(tags)
        if not classification:
            self.counts["aires_non_classees"] += 1
            return
        rings = []
        try:
            for ring in area.outer_rings():
                pts = ring_coords(ring)
                if pts:
                    rings.append(pts)
            for ring in area.inner_rings():
                pts = ring_coords(ring)
                if pts:
                    rings.append(pts)
        except Exception as exc:
            print(f"  surface ignorée ({exc})", file=sys.stderr)
            return
        if not rings:
            self.counts["aires_sans_anneau"] += 1
            return
        self.update_bounds([p for r in rings for p in r])
        self.counts["surfaces"] += 1
        self.add_feature("A", classification[0], rings, classification[1])


def write_gvt(db, destination, bounds, zmin, zmax):
    db.commit()
    cursor = db.execute(
        "SELECT tile_key, payload FROM records ORDER BY tile_key"
    )
    index_entries = []
    blob_tmp = tempfile.NamedTemporaryFile(prefix="gvt_blobs_", delete=False)
    blob_path = blob_tmp.name
    pos = 0
    biggest = 0
    too_big = 0
    tile_count = 0
    try:
        current_key = None
        payloads = []
        def flush_tile(key, records):
            nonlocal pos, biggest, too_big, tile_count
            if key is None:
                return
            blob = varint(len(records)) + b"".join(records)
            size = len(blob)
            biggest = max(biggest, size)
            if size > SLOT_BYTES:
                too_big += 1
            blob_tmp.write(blob)
            index_entries.append((key, pos, size))
            pos += size
            tile_count += 1

        for key, payload in cursor:
            if current_key is None:
                current_key = key
            if key != current_key:
                flush_tile(current_key, payloads)
                current_key, payloads = key, []
            payloads.append(bytes(payload))
        flush_tile(current_key, payloads)
        blob_tmp.close()

        minlon, minlat, maxlon, maxlat = bounds
        bbox = [int(round(v * 1e7)) for v in (minlon, minlat, maxlon, maxlat)]
        base = HEADER_SIZE + INDEX_SIZE * len(index_entries)
        with open(destination, "wb") as out:
            out.write(HEADER.pack(
                MAGIC, VERSION, EXTENT, zmin, zmax, 0,
                len(index_entries), HEADER_SIZE, *bbox
            ))
            for key, offset, size in index_entries:
                out.write(INDEX.pack(key, base + offset, size))
            with open(blob_path, "rb") as src:
                while True:
                    chunk = src.read(1024 * 1024)
                    if not chunk:
                        break
                    out.write(chunk)
        return tile_count, base + pos, biggest, too_big
    finally:
        if not blob_tmp.closed:
            blob_tmp.close()
        try:
            os.unlink(blob_path)
        except OSError:
            pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pbf", help="fichier source .osm.pbf")
    parser.add_argument("output", help="fichier GVT1 de sortie")
    parser.add_argument("--zmin", type=int, default=11)
    parser.add_argument("--zmax", type=int, default=15)
    parser.add_argument("--tolerance", type=float, default=8.0)
    parser.add_argument("--area-tolerance", type=float, default=16.0)
    parser.add_argument("--min-size", type=float, default=8.0)
    parser.add_argument("--no-areas", action="store_true")
    parser.add_argument("--temp-dir", help="dossier pour la base SQLite temporaire")
    args = parser.parse_args()

    if not 0 <= args.zmin <= args.zmax <= 16:
        parser.error("zooms attendus : 0 <= zmin <= zmax <= 16")
    if not os.path.isfile(args.pbf):
        parser.error(f"fichier PBF introuvable : {args.pbf}")

    fd, db_path = tempfile.mkstemp(prefix="osm_gvt_", suffix=".sqlite", dir=args.temp_dir)
    os.close(fd)
    db = sqlite3.connect(db_path)
    try:
        db.execute("PRAGMA journal_mode=OFF")
        db.execute("PRAGMA synchronous=OFF")
        db.execute("PRAGMA temp_store=FILE")
        db.execute("PRAGMA cache_size=-65536")  # environ 64 Mio
        db.execute("CREATE TABLE records (tile_key INTEGER NOT NULL, payload BLOB NOT NULL)")
        db.execute("CREATE INDEX records_key ON records(tile_key)")

        handler = PBFToGVT(
            db, args.zmin, args.zmax, args.tolerance,
            args.area_tolerance, args.min_size, not args.no_areas
        )
        print("Lecture du PBF et génération des fragments GVT1…", flush=True)
        handler.apply_file(args.pbf, locations=True)
        db.commit()

        if handler.inserted == 0:
            raise SystemExit("Aucune géométrie exploitable n'a été trouvée.")
        print("Assemblage du fichier GVT1…", flush=True)
        tiles, total, biggest, too_big = write_gvt(
            db, args.output, handler.bounds, args.zmin, args.zmax
        )

        print("\nBilan")
        print(f"  Ways linéaires classés : {handler.counts['ways_lignes']:,}")
        print(f"  Objets surfaciques OSM : {handler.counts['aires_osm']:,}")
        print(f"  Surfaces non classées  : {handler.counts['aires_non_classees']:,}")
        print(f"  Surfaces sans anneau   : {handler.counts['aires_sans_anneau']:,}")
        print(f"  Surfaces classées      : {handler.counts['surfaces']:,}")
        print(f"  Fragments indexés      : {handler.inserted:,}")
        print(f"  Tuiles produites       : {tiles:,}")
        print(f"  Taille GVT1            : {total:,} octets ({total / 1e9:.2f} Go)")
        print(f"  Plus grande tuile      : {biggest:,} octets")
        print(f"  Tuiles > cache 65 536  : {too_big:,}")
        print(f"  Emprise OSM            : {handler.bounds}")
        if too_big:
            print("ATTENTION : les tuiles dépassant 65 536 octets risquent de ne pas être affichées par le GPS.")
            print("Augmenter --tolerance ou --min-size, ou réduire le zoom maximal.")
        if not args.no_areas and handler.counts["surfaces"] == 0:
            print("ATTENTION : aucune surface GVT1 n'a été produite.")
            print("Vérifier les compteurs d'aires OSM ci-dessus et les tags présents dans le PBF.")
        print("Crédit requis : © contributeurs OpenStreetMap (ODbL).")
    finally:
        db.close()
        try:
            os.unlink(db_path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
