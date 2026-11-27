#!/usr/bin/env python3
"""Convertit des GeoJSON synthétiques en GVT1 uniquement pour les tests et aperçus PC. Ce script n'est pas un outil de production.

Usage :
  python3 tests/geojson_to_gvt_fixture.py fixture.geojson fixture.gvt [--zmin 11] [--zmax 15]
         [--tolerance 8] [--area-tolerance 16] [--min-size 8] [--no-areas] [--bbox ouest,sud,est,nord]

Entrees acceptees (plusieurs fichiers possibles, le dernier argument est la sortie) :
  - GeoJSON (export d'Overpass Turbo) ;
  - JSON de l'API Overpass, requete avec [out:json] et "out geom" (voir tools/overpass_fetch.py).
Un meme objet present dans plusieurs fichiers (identifiant identique) n'est gardé qu'une fois.

Lignes gardees : routes, chemins, cours d'eau, voies ferrees. Surfaces gardees (format version 2) : zones urbaines, forets,
champs et prairies, parcs, roche et glaciers, plans d'eau, pour colorer le fond de la carte (--no-areas pour les omettre).
Bibliotheque standard uniquement. Les tuiles suivent la grille XYZ (Web Mercator), 4096 unites par tuile.
"""
import argparse
import bisect
import json
import math
import os
import struct
import sys
import tempfile

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


def lines_of(geom):
    """Lignes (listes de [lon, lat]) d'une geometrie GeoJSON ; les anneaux de polygones sont des lignes fermees."""
    if not geom:
        return []
    kind, c = geom.get('type'), geom.get('coordinates')
    if kind == 'LineString':
        return [c]
    if kind == 'MultiLineString':
        return list(c)
    if kind == 'Polygon':
        return list(c)
    if kind == 'MultiPolygon':
        return [ring for poly in c for ring in poly]
    return []


def polygons_of(geom):
    """Polygones (liste d'anneaux) d'une geometrie GeoJSON Polygon ou MultiPolygon."""
    if not geom:
        return []
    if geom.get('type') == 'Polygon':
        return [list(geom['coordinates'])]
    if geom.get('type') == 'MultiPolygon':
        return [list(poly) for poly in geom['coordinates']]
    return []


def stitch_rings(pieces):
    """Assemble des morceaux de lignes (membres d'une relation) en anneaux fermes. Un anneau qui ne se ferme pas est ignore."""
    def key(p):
        return (round(p[0] * 1e7), round(p[1] * 1e7))
    segs = [list(p) for p in pieces if len(p) >= 2]
    ends = {}
    for i, sg in enumerate(segs):
        ends.setdefault(key(sg[0]), []).append(i)
        ends.setdefault(key(sg[-1]), []).append(i)
    used = [False] * len(segs)
    rings = []
    for i in range(len(segs)):
        if used[i]:
            continue
        used[i] = True
        ring = list(segs[i])
        while key(ring[0]) != key(ring[-1]):
            k = key(ring[-1])
            nxt = next((j for j in ends.get(k, []) if not used[j]), None)
            if nxt is None:
                ring = None
                break
            used[nxt] = True
            sg = segs[nxt]
            if key(sg[0]) == k:
                ring.extend(sg[1:])
            else:
                ring.extend(reversed(sg[:-1]))
        if ring and len(ring) >= 4:
            rings.append(ring)
    return rings


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


def geojson_items(data):
    """(cle, tags, lignes, polygones) pour chaque objet d'un GeoJSON. Un polygone est une liste d'anneaux."""
    for ft in data.get('features', []):
        props = ft.get('properties') or {}
        key = ft.get('id') or props.get('@id')
        geom = ft.get('geometry')
        yield key, props, lines_of(geom), polygons_of(geom)


def overpass_items(data, stats):
    """(cle, tags, lignes, polygones) pour chaque objet d'un JSON Overpass obtenu avec "out geom"."""
    for el in data.get('elements', []):
        kind = el.get('type')
        if kind == 'way':
            geom = el.get('geometry')
            if not geom:
                stats['sans_geometrie'] += 1
                continue
            line = [(p['lon'], p['lat']) for p in geom]
            closed = len(line) >= 4 and line[0] == line[-1]
            yield ('way', el.get('id')), el.get('tags') or {}, [line], ([[line]] if closed else [])
        elif kind == 'relation':
            pieces = []
            for m in el.get('members', []):
                if m.get('type') == 'way' and m.get('role') in ('outer', 'inner'):
                    if m.get('geometry'):
                        pieces.append([(p['lon'], p['lat']) for p in m['geometry']])
                    else:
                        stats['sans_geometrie'] += 1
            rings = stitch_rings(pieces)
            yield ('relation', el.get('id')), el.get('tags') or {}, pieces, ([rings] if rings else [])


def load_features(paths, bbox, areas=True):
    """Lignes et surfaces classees de un ou plusieurs fichiers (GeoJSON ou JSON Overpass), sans doublons d'identifiant.
    Retourne (lignes, surfaces) : lignes = [(classe, zoom minimal, [(lon, lat), ...])],
    surfaces = [(classe, zoom minimal, [anneau, ...])]."""
    if isinstance(paths, str):
        paths = [paths]
    lines, polys, seen = [], [], set()
    stats = {'sans_geometrie': 0}

    def in_box(pts):
        if not bbox:
            return True
        w, s_, e, n = bbox
        lons = [p[0] for p in pts]
        lats = [p[1] for p in pts]
        return not (max(lons) < w or min(lons) > e or max(lats) < s_ or min(lats) > n)

    for path in paths:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
        if isinstance(data, dict) and 'elements' in data:
            items = overpass_items(data, stats)
        else:
            items = geojson_items(data)
        for key, tags, ls, pgs in items:
            if key is not None:
                if key in seen:
                    continue
                seen.add(key)
            ac = classify_area(tags) if areas else None
            if pgs and ac:
                for rings in pgs:
                    rings = [r for r in rings if len(r) >= 3]
                    if rings and in_box([p for r in rings for p in r]):
                        polys.append((ac[0], ac[1], [[(p[0], p[1]) for p in r] for r in rings]))
                continue
            c = classify(tags)
            if c is None:
                continue
            for line in ls:
                if len(line) >= 2 and in_box(line):
                    lines.append((c[0], c[1], [(p[0], p[1]) for p in line]))
    if not lines and not polys:
        hint = ''
        if stats['sans_geometrie']:
            hint = ' (%d objets sans geometrie : la requete Overpass doit finir par "out geom;")' % stats['sans_geometrie']
        raise SystemExit('aucune ligne exploitable dans ' + ', '.join(paths) + hint)
    return lines, polys


def convert(src, dst, zmin, zmax, tol, min_size, bbox, area_tol=16.0, areas=True):
    lines, polys = load_features(src, bbox, areas)
    lons = [p[0] for _, _, line in lines for p in line] + [p[0] for _, _, rings in polys for r in rings for p in r]
    lats = [p[1] for _, _, line in lines for p in line] + [p[1] for _, _, rings in polys for r in rings for p in r]
    box = [int(round(v * 1e7)) for v in (min(lons), min(lats), max(lons), max(lats))]
    index = []
    stats = {}
    biggest = 0
    too_big = 0
    tmp = tempfile.NamedTemporaryFile(delete=False)
    try:
        pos = 0
        for z in range(zmin, zmax + 1):
            tiles = {}
            nlines = nareas = 0
            for cls, minz, rings in polys:
                if minz > z:
                    continue
                for key, loc in area_tiles(rings, z, area_tol, min_size).items():
                    tiles.setdefault(key, []).append(('A', cls, loc))
                    nareas += 1
            for cls, minz, line in lines:
                if minz > z:
                    continue
                pts = []
                for lon, lat in line:
                    p = world(lon, lat, z)
                    if not pts or pts[-1] != p:
                        pts.append(p)
                if len(pts) < 2:
                    continue
                pts = simplify(pts, tol)
                xs = [p[0] for p in pts]
                ys = [p[1] for p in pts]
                if max(max(xs) - min(xs), max(ys) - min(ys)) < min_size:
                    continue
                for (tx, ty), pieces in tile_polyline(pts, z).items():
                    for piece in pieces:
                        for part in to_local(piece, tx, ty):
                            tiles.setdefault((tx, ty), []).append(('L', cls, part))
                            nlines += 1
            nbytes = 0
            for (tx, ty) in sorted(tiles):
                blob = encode_tile(tiles[(tx, ty)])
                biggest = max(biggest, len(blob))
                too_big += len(blob) > SLOT_BYTES
                tmp.write(blob)
                index.append((key_of(z, tx, ty), pos, len(blob)))
                pos += len(blob)
                nbytes += len(blob)
            stats[z] = (len(tiles), nbytes, nlines, nareas)
            print('  zoom %d traite' % z, file=sys.stderr, flush=True)
        tmp.close()
        base = HEADER_SIZE + INDEX_SIZE * len(index)
        with open(dst, 'wb') as out:
            out.write(HEADER.pack(MAGIC, VERSION, EXTENT, zmin, zmax, 0, len(index), HEADER_SIZE, *box))
            for key, off, size in index:
                out.write(INDEX.pack(key, base + off, size))
            with open(tmp.name, 'rb') as blobs:
                while True:
                    chunk = blobs.read(1 << 20)
                    if not chunk:
                        break
                    out.write(chunk)
    finally:
        if not tmp.closed:
            tmp.close()
        os.unlink(tmp.name)
    return stats, base + pos, biggest, too_big


def parse_bbox(s):
    try:
        v = [float(x) for x in s.split(',')]
        if len(v) != 4 or v[0] >= v[2] or v[1] >= v[3]:
            raise ValueError
        return v
    except ValueError:
        raise argparse.ArgumentTypeError('bbox attendue : ouest,sud,est,nord (degres)')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('files', nargs='+', metavar='fichier',
                   help='un ou plusieurs extraits (GeoJSON ou JSON Overpass), puis le fichier .gvt a produire')
    p.add_argument('--zmin', type=int, default=11)
    p.add_argument('--zmax', type=int, default=15)
    p.add_argument('--tolerance', type=float, default=8.0, help='simplification, en unites de tuile (4096 = une tuile)')
    p.add_argument('--area-tolerance', type=float, default=16.0, help='simplification des surfaces, en unites de tuile')
    p.add_argument('--no-areas', action='store_true', help='ne garder que les lignes (fichier plus petit, fond uni)')
    p.add_argument('--min-size', type=float, default=8.0, help='taille minimale d un objet, en unites de tuile')
    p.add_argument('--bbox', type=parse_bbox)
    a = p.parse_args()
    if not 0 <= a.zmin <= a.zmax <= 16:
        sys.exit('zooms attendus : 0 <= zmin <= zmax <= 16')
    if len(a.files) < 2:
        sys.exit('il faut au moins un extrait et le fichier de sortie')
    *inputs, output = a.files
    stats, total, biggest, too_big = convert(inputs, output, a.zmin, a.zmax, a.tolerance, a.min_size, a.bbox, a.area_tolerance, not a.no_areas)
    for z, (n, b, nl, na) in stats.items():
        print('zoom %2d : %6d tuiles, %10d octets, %7d lignes, %6d surfaces' % (z, n, b, nl, na))
    print('fichier %s : %d octets' % (output, total))
    print('plus grande tuile : %d octets (cache de l\'appareil : %d)' % (biggest, SLOT_BYTES))
    if too_big:
        print('ATTENTION : %d tuile(s) depassent le cache et ne seront pas affichees. Reduire la zone, ou augmenter '
              '--min-size / --tolerance, ou retirer les petits chemins de la requete.' % too_big)
    print('Credit requis sur l appareil : © contributeurs OpenStreetMap (ODbL).')


if __name__ == '__main__':
    main()
