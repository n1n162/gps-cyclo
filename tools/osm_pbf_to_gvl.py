#!/usr/bin/env python3
"""
Extrait les noms de villes, bourgs, villages et hameaux d'un fichier OSM PBF
et produit une couche de libelles GVL1, independante du format de carte GVT1.

Dependance :
    python3 -m pip install osmium

Utilisation :
    python3 osm_pbf_to_gvl.py france-latest.osm.pbf france_labels.gvl

Le fichier GVL1 contient un index de tuiles XYZ et, pour chaque tuile,
des libelles UTF-8 avec leurs coordonnees locales (0..4096).
Il ne modifie ni le fichier GVT1 ni le depot GitHub.

Niveaux d'affichage proposes :
    place=city      zoom 11+
    place=town      zoom 12+
    place=village   zoom 13+
    place=hamlet    zoom 15+

Attention : ce programme genere uniquement les noms de lieux. Il faut ensuite
un lecteur GVL1 et le rendu du texte dans LVGL pour les afficher sur l'ecran.
"""

import argparse
import math
import os
import struct
import sys
import tempfile

try:
    import osmium
except ImportError:
    raise SystemExit(
        "Dependance manquante : installe pyosmium avec "
        "'python3 -m pip install osmium'"
    )

MAGIC = b"GVL1"
VERSION = 1
EXTENT = 4096
HEADER = struct.Struct("<4sHHBBHII")
INDEX = struct.Struct("<QII")
RECORD = struct.Struct("<H H B")  # x local, y local, rang
PLACE_RULES = {
    "city": (11, 0),
    "town": (12, 1),
    "village": (13, 2),
    "hamlet": (15, 3),
}
MAX_NAME_BYTES = 255


def world_tile(lon, lat, zoom):
    """Retourne (tuile_x, tuile_y, x_local, y_local) en Web Mercator XYZ."""
    n = 1 << zoom
    lat = max(-85.0511287798, min(85.0511287798, lat))
    x = (lon + 180.0) / 360.0 * n
    s = math.sin(math.radians(lat))
    y = (0.5 - math.log((1.0 + s) / (1.0 - s)) / (4.0 * math.pi)) * n

    tx = max(0, min(n - 1, int(math.floor(x))))
    ty = max(0, min(n - 1, int(math.floor(y))))
    lx = max(0, min(EXTENT - 1, int((x - tx) * EXTENT)))
    ly = max(0, min(EXTENT - 1, int((y - ty) * EXTENT)))
    return tx, ty, lx, ly


def tile_key(zoom, x, y):
    # Meme cle que le format GVT1 : zoom dans les 8 bits de poids fort.
    return (zoom << 32) | (x << 16) | y


class PlaceHandler(osmium.SimpleHandler):
    def __init__(self, zmin, zmax, bbox):
        super().__init__()
        self.zmin = zmin
        self.zmax = zmax
        self.bbox = bbox
        self.tiles = {}
        self.seen = set()
        self.counts = {k: 0 for k in PLACE_RULES}

    def node(self, node):
        tags = node.tags
        place = tags.get("place")
        rule = PLACE_RULES.get(place)
        name = tags.get("name")
        if rule is None or not name or not node.location.valid():
            return

        # Evite les doublons eventuels par identifiant OSM.
        if node.id in self.seen:
            return
        self.seen.add(node.id)

        lon = node.location.lon
        lat = node.location.lat
        if self.bbox:
            west, south, east, north = self.bbox
            if not (west <= lon <= east and south <= lat <= north):
                return

        min_zoom, rank = rule
        self.counts[place] += 1
        for z in range(max(self.zmin, min_zoom), self.zmax + 1):
            tx, ty, lx, ly = world_tile(lon, lat, z)
            key = tile_key(z, tx, ty)
            encoded = name.encode("utf-8")[:MAX_NAME_BYTES]
            # Ne jamais couper au milieu d'un caractere UTF-8.
            encoded = encoded.decode("utf-8", errors="ignore").encode("utf-8")
            self.tiles.setdefault(key, []).append((lx, ly, rank, encoded))


def parse_bbox(value):
    try:
        vals = [float(v) for v in value.split(",")]
        if len(vals) != 4:
            raise ValueError
        west, south, east, north = vals
        if west >= east or south >= north:
            raise ValueError
        return vals
    except ValueError:
        raise argparse.ArgumentTypeError(
            "bbox attendue : ouest,sud,est,nord (degres)"
        )


def write_gvl(path, tiles, zmin, zmax):
    """Ecrit un GVL1 : entete, index trie, puis blocs de tuiles."""
    ordered = []
    for key in sorted(tiles):
        records = sorted(tiles[key], key=lambda r: (r[2], r[1], r[0], r[3]))
        blob = bytearray(struct.pack("<H", len(records)))
        for x, y, rank, name in records:
            blob += RECORD.pack(x, y, rank)
            blob += struct.pack("<B", len(name))
            blob += name
        ordered.append((key, bytes(blob)))

    header_size = HEADER.size
    index_size = INDEX.size
    data_offset = header_size + index_size * len(ordered)
    offset = 0

    with open(path, "wb") as out:
        out.write(HEADER.pack(
            MAGIC, VERSION, EXTENT, zmin, zmax, 0,
            len(ordered), header_size
        ))
        for key, blob in ordered:
            out.write(INDEX.pack(key, data_offset + offset, len(blob)))
            offset += len(blob)
        for _, blob in ordered:
            out.write(blob)

    return data_offset + offset, max((len(b) for _, b in ordered), default=0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_pbf", help="Fichier OSM .pbf source")
    parser.add_argument("output_gvl", help="Fichier de sortie .gvl")
    parser.add_argument("--zmin", type=int, default=11)
    parser.add_argument("--zmax", type=int, default=15)
    parser.add_argument("--bbox", type=parse_bbox,
                        help="ouest,sud,est,nord en degres")
    args = parser.parse_args()

    if not os.path.isfile(args.input_pbf):
        parser.error("fichier PBF introuvable : " + args.input_pbf)
    if not (0 <= args.zmin <= args.zmax <= 16):
        parser.error("zooms attendus : 0 <= zmin <= zmax <= 16")

    handler = PlaceHandler(args.zmin, args.zmax, args.bbox)
    print("Lecture du PBF et extraction des lieux...", file=sys.stderr)
    handler.apply_file(args.input_pbf, locations=True)

    if not handler.tiles:
        raise SystemExit("Aucun nom de lieu exploitable trouve.")

    total, biggest = write_gvl(
        args.output_gvl, handler.tiles, args.zmin, args.zmax
    )

    print("Lieux extraits :")
    for place in PLACE_RULES:
        print("  {:8s} : {}".format(place, handler.counts[place]))
    print("Tuiles avec libelles : {}".format(len(handler.tiles)))
    print("Fichier : {} ({} octets)".format(args.output_gvl, total))
    print("Plus gros bloc de tuile : {} octets".format(biggest))
    print("Format : GVL1 version 1 ; noms UTF-8.")
    print("Note : le rendu LVGL et le lecteur GVL1 restent a integrer.")


if __name__ == "__main__":
    main()
