#!/usr/bin/env python3
# Convertit un extrait OSM PBF en GVT1 v2 en utilisant SQLite comme stockage temporaire.
# À placer dans le même dossier que osm_to_gvt.py.
#
# Dépendances :
#   python3 -m pip install osmium
#
# Usage :
#   python3 osm_pbf_to_gvt.py france-latest.osm.pbf france.gvt
#   python3 osm_pbf_to_gvt.py france-latest.osm.pbf france.gvt --zmin 11 --zmax 15
#
# Le fichier GVT1 est compatible avec le format du convertisseur osm_to_gvt.py fourni.
# Les noms de villes restent dans le fichier GVL1 généré par osm_pbf_to_gvl.py.

import argparse
import importlib.util
import os
import sqlite3
import struct
import sys
import tempfile
from pathlib import Path

try:
    import osmium
except ImportError:
    raise SystemExit("Module osmium absent. Installer avec : python3 -m pip install osmium")

HERE = Path(__file__).resolve().parent
CONVERTER = HERE / "osm_to_gvt.py"
if not CONVERTER.exists():
    raise SystemExit("osm_to_gvt.py doit être dans le même dossier que ce script.")

spec = importlib.util.spec_from_file_location("osm_to_gvt", CONVERTER)
gvt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gvt)

HEADER = gvt.HEADER
INDEX = gvt.INDEX
MAGIC = gvt.MAGIC
VERSION = gvt.VERSION
EXTENT = gvt.EXTENT
SLOT_BYTES = gvt.SLOT_BYTES


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
        self.counts = {"ways_lignes": 0, "surfaces": 0, "objets_ignores": 0}
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
                    p = gvt.world(lon, lat, z)
                    if not pts or pts[-1] != p:
                        pts.append(p)
                if len(pts) < 2:
                    continue
                pts = gvt.simplify(pts, self.tolerance)
                if not pts:
                    continue
                xs, ys = [p[0] for p in pts], [p[1] for p in pts]
                if max(max(xs)-min(xs), max(ys)-min(ys)) < self.min_size:
                    continue
                pieces_by_tile = gvt.tile_polyline(pts, z)
                for (tx, ty), pieces in pieces_by_tile.items():
                    for piece in pieces:
                        for part in gvt.to_local(piece, tx, ty):
                            if len(part) >= 2:
                                self.store(z, tx, ty, ("L", cls, part))
            else:
                for (tx, ty), rings in gvt.area_tiles(
                    geom, z, self.area_tolerance, self.min_size
                ).items():
                    if rings:
                        self.store(z, tx, ty, ("A", cls, rings))

    def store(self, z, tx, ty, feature):
        # encode_tile([feature]) commence par le varint du nombre (1, donc un octet).
        encoded = gvt.encode_tile([feature])
        payload = encoded[1:]
        key = gvt.key_of(z, tx, ty)
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
        classification = gvt.classify(tags)
        if classification:
            self.counts["ways_lignes"] += 1
            self.add_feature("L", classification[0], line, classification[1])
        elif not gvt.classify_area(tags):
            self.counts["objets_ignores"] += 1

    def area(self, area):
        if not self.areas:
            return
        tags = dict(area.tags)
        classification = gvt.classify_area(tags)
        if not classification:
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
            blob = gvt.varint(len(records)) + b"".join(records)
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
        base = gvt.HEADER_SIZE + gvt.INDEX_SIZE * len(index_entries)
        with open(destination, "wb") as out:
            out.write(HEADER.pack(
                MAGIC, VERSION, EXTENT, zmin, zmax, 0,
                len(index_entries), gvt.HEADER_SIZE, *bbox
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
        print("Crédit requis : © contributeurs OpenStreetMap (ODbL).")
    finally:
        db.close()
        try:
            os.unlink(db_path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
