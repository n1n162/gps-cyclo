#!/usr/bin/env python3
"""Verifie un fichier GVT1 contre le GeoJSON dont il vient (decodeur Python independant du code C).

Usage : python3 tests/verify_gvt.py extrait.geojson carte.gvt [--tolerance 8] [--min-size 8]
Controles, pour chaque zoom :
  A. chaque sommet d'origine d'une ligne visible est a moins de tolerance + 1,5 unite d'une ligne decodee de meme classe ;
  B. chaque point decode est a moins de tolerance + 1,5 unite d'une ligne d'origine de meme classe ;
  C. les objets ignores (batiments, points, tags inconnus) et les objets sous le zoom minimal sont absents ;
  D. tailles, ordre de l'index, bornes des coordonnees et nombre de points par ligne ;
  E. surfaces : en un grand nombre de points tires au hasard (dont beaucoup pres des bords), un point est dans une surface
     decodee exactement quand il est dans une surface d'origine de meme classe, sauf a moins de tolerance + 2 unites du bord.
"""
import argparse
import bisect
import math
import random
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
import geojson_to_gvt_fixture as T  # noqa: E402

CELL = 64.0


class SegIndex:
    def __init__(self):
        self.cells = {}

    def add(self, a, b):
        n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / CELL) + 1)
        seen = set()
        for i in range(n + 1):
            t = i / n
            c = (int((a[0] + (b[0] - a[0]) * t) // CELL), int((a[1] + (b[1] - a[1]) * t) // CELL))
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    k = (c[0] + dx, c[1] + dy)
                    if k not in seen:
                        seen.add(k)
                        self.cells.setdefault(k, []).append((a, b))

    def near(self, p, maxd):
        for a, b in self.cells.get((int(p[0] // CELL), int(p[1] // CELL)), ()):
            dx, dy = b[0] - a[0], b[1] - a[1]
            l2 = dx * dx + dy * dy
            t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / l2))
            if math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy) <= maxd:
                return True
        return False


def read_varint(buf, pos):
    r, shift = 0, 0
    while True:
        b = buf[pos]
        pos += 1
        r |= (b & 0x7F) << shift
        if not b & 0x80:
            return r, pos
        shift += 7


def unzigzag(u):
    return (u >> 1) ^ -(u & 1)


def decode_file(path):
    """Retourne (lignes, zmin, zmax, surfaces). lignes[z] = [(classe, [(X, Y)], nb_points)] ;
    surfaces[z] = [(classe, [[(X, Y)]])] en coordonnees monde (unites de tuile)."""
    data = open(path, 'rb').read()
    magic, ver, ext, zmin, zmax, _, n, ioff, *box = T.HEADER.unpack_from(data, 0)
    assert magic == b'GVT1' and ver in (1, 2) and ext == T.EXTENT, 'en-tete invalide'
    assert ioff == T.HEADER_SIZE
    entries, prev = [], -1
    for i in range(n):
        key, off, size = T.INDEX.unpack_from(data, ioff + i * T.INDEX_SIZE)
        assert key > prev, 'index non trie'
        assert off + size <= len(data), 'tuile hors du fichier'
        prev = key
        entries.append((key, off, size))
    tiles = {z: [] for z in range(zmin, zmax + 1)}
    areas = {z: [] for z in range(zmin, zmax + 1)}

    def points(blob, pos, npts, x, y):
        pts, px, py = [], 0, 0
        for i in range(npts):
            a, pos = read_varint(blob, pos)
            b, pos = read_varint(blob, pos)
            a, b = unzigzag(a), unzigzag(b)
            px, py = (a, b) if i == 0 else (px + a, py + b)
            assert -T.BUFFER - 1 <= px <= T.EXTENT + T.BUFFER + 1 and -T.BUFFER - 1 <= py <= T.EXTENT + T.BUFFER + 1
            pts.append((x * T.EXTENT + px, y * T.EXTENT + py))
        return pts, pos

    for key, off, size in entries:
        z, x, y = key >> 32, (key >> 16) & 0xFFFF, key & 0xFFFF
        assert zmin <= z <= zmax and x < (1 << z) and y < (1 << z)
        blob, pos = data[off:off + size], 0
        nf, pos = read_varint(blob, pos)
        assert nf > 0, 'tuile vide dans l index'
        for _ in range(nf):
            cls, flags = blob[pos], blob[pos + 1]
            pos += 2
            if flags == 0:
                assert 1 <= cls <= 9
                npts, pos = read_varint(blob, pos)
                assert 2 <= npts <= T.MAX_POINTS, 'nombre de points hors limites'
                pts, pos = points(blob, pos, npts, x, y)
                tiles[z].append((cls, pts, npts))
            else:
                assert ver == 2 and flags == 1 and 10 <= cls <= 15
                nr, pos = read_varint(blob, pos)
                assert 1 <= nr <= T.MAX_RINGS, 'nombre d anneaux hors limites'
                rings, total = [], 0
                for _r in range(nr):
                    npts, pos = read_varint(blob, pos)
                    assert 3 <= npts <= T.MAX_AREA_POINTS
                    ring, pos = points(blob, pos, npts, x, y)
                    rings.append(ring)
                    total += npts
                assert total <= T.MAX_AREA_POINTS, 'trop de points dans une surface'
                areas[z].append((cls, rings))
        assert pos == len(blob), 'octets en trop dans la tuile'
    return tiles, zmin, zmax, areas


def inside(pt, rings):
    """Remplissage pair-impair."""
    x, y = pt
    c = False
    for r in rings:
        n = len(r)
        for i in range(n):
            x1, y1 = r[i]
            x2, y2 = r[(i + 1) % n]
            if (y1 <= y) != (y2 <= y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
                c = not c
    return c


def dist_to_rings(pt, rings):
    best = 1e18
    for r in rings:
        for i in range(len(r)):
            a, b = r[i], r[(i + 1) % len(r)]
            dx, dy = b[0] - a[0], b[1] - a[1]
            l2 = dx * dx + dy * dy
            t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((pt[0] - a[0]) * dx + (pt[1] - a[1]) * dy) / l2))
            best = min(best, math.hypot(pt[0] - a[0] - t * dx, pt[1] - a[1] - t * dy))
    return best


def check_areas(z, polys, areas_z, tol, min_size, rng):
    """Retourne (points testes, desaccords hors tolerance, surfaces visibles)."""
    dec_by_cls = {}                                  # surfaces decodees par classe, avec leur boite
    for cls, rings in areas_z:
        xs = [p[0] for r in rings for p in r]
        ys = [p[1] for r in rings for p in r]
        dec_by_cls.setdefault(cls, []).append((min(xs), max(xs), min(ys), max(ys), rings))
    orig = []
    for cls, minz, rings in polys:
        if minz > z:
            continue
        w = [[T.world(lon, lat, z) for lon, lat in r] for r in rings]
        xs = [p[0] for r in w for p in r]
        ys = [p[1] for r in w for p in r]
        ext = max(max(xs) - min(xs), max(ys) - min(ys))
        orig.append((cls, min(xs), max(xs), min(ys), max(ys), w, ext))
    tested = bad = 0
    big = [o for o in orig if o[6] >= 2 * min_size]
    for cls, x0, x1, y0, y1, w, ext in big:
        pts = []
        for _ in range(250):
            pts.append((rng.uniform(x0 - 3 * tol, x1 + 3 * tol), rng.uniform(y0 - 3 * tol, y1 + 3 * tol)))
        for _ in range(250):                         # points pres du bord
            r = rng.choice(w)
            a = rng.choice(r)
            ang = rng.uniform(0, 2 * math.pi)
            d = rng.uniform(0, 3 * tol)
            pts.append((a[0] + d * math.cos(ang), a[1] + d * math.sin(ang)))
        for p in pts:
            o_any = any(c == cls and bx0 <= p[0] <= bx1 and by0 <= p[1] <= by1 and inside(p, rr)
                        for c, bx0, bx1, by0, by1, rr, _ in orig)
            o_big = any(c == cls and bx0 <= p[0] <= bx1 and by0 <= p[1] <= by1 and inside(p, rr)
                        for c, bx0, bx1, by0, by1, rr, e in big)
            d_in = any(bx0 <= p[0] <= bx1 and by0 <= p[1] <= by1 and inside(p, rr) for bx0, bx1, by0, by1, rr in dec_by_cls.get(cls, []))
            tested += 1
            if d_in != o_big and o_any == o_big:
                if dist_to_rings(p, w) > tol + 2:
                    bad += 1
    return tested, bad, len(big)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('geojson')
    ap.add_argument('gvt')
    ap.add_argument('--tolerance', type=float, default=8.0)
    ap.add_argument('--min-size', type=float, default=8.0)
    ap.add_argument('--area-tolerance', type=float, default=16.0)
    a = ap.parse_args()
    maxd = a.tolerance + 1.5
    tiles, zmin, zmax, areas = decode_file(a.gvt)
    feats, polys = T.load_features(a.geojson, None)
    rng = random.Random(12345)
    problems = 0
    for z in range(zmin, zmax + 1):
        dec = {}
        for cls, pts, _ in tiles[z]:
            idx = dec.setdefault(cls, SegIndex())
            for p, q in zip(pts, pts[1:]):
                idx.add(p, q)
        orig = {}
        visible, ambiguous = [], 0
        for cls, minz, line in feats:
            if minz > z:
                continue
            w = [T.world(lon, lat, z) for lon, lat in line]
            xs, ys = [p[0] for p in w], [p[1] for p in w]
            ext = max(max(xs) - min(xs), max(ys) - min(ys))
            idx = orig.setdefault(cls, SegIndex())
            for p, q in zip(w, w[1:]):
                idx.add(p, q)
            if ext < a.min_size * 0.5:
                continue                      # certainement ignore
            if ext < a.min_size * 2.0:
                ambiguous += 1                # peut etre garde ou ignore selon la simplification
                continue
            visible.append((cls, w))
        miss = sum(1 for cls, w in visible for p in w if cls not in dec or not dec[cls].near(p, maxd))
        stray = sum(1 for cls, pts, _ in tiles[z] for p in pts if cls not in orig or not orig[cls].near(p, maxd))
        classes = sorted({c for c, _, _ in tiles[z]})
        too_early = [c for c in classes if not any(f[0] == c and f[1] <= z for f in feats)]
        npieces = sum(1 for _, _, n in tiles[z] if n == T.MAX_POINTS)
        tested, bad, nbig = check_areas(z, polys, areas[z], a.area_tolerance, a.min_size, rng)
        acl = sorted({c for c, _ in areas[z]})
        status = 'OK' if not (miss or stray or too_early or bad) else 'ECHEC'
        problems += status != 'OK'
        print('zoom %2d : %5d lignes, classes %s, sommets manquants %d, points hors trace %d, '
              'lignes a %d points %d, ambigus %d ; %d surfaces, classes %s, %d points testes, %d desaccords  %s'
              % (z, len(tiles[z]), classes, miss, stray, T.MAX_POINTS, npieces, ambiguous, len(areas[z]), acl, tested, bad, status))
    if problems:
        sys.exit('VERIFICATION ECHOUEE')
    print('VERIFICATION REUSSIE')


if __name__ == '__main__':
    main()
