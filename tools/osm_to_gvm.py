#!/usr/bin/env python3
"""Convert an Overpass/OSM GeoJSON extract to a compact GVM1 offline map.

Usage: python3 tools/osm_to_gvm.py extract.geojson map.gvm [--bbox west,south,east,north]
Coordinates are stored as signed degrees * 1e7. All integers are little-endian.
The converter intentionally uses only Python's standard library.
"""
import argparse
import json
import math
import struct
import sys
from pathlib import Path

MAGIC = b'GVM1'
VERSION = 1
HEADER = struct.Struct('<4sHHII')       # magic, version, flags, feature_count, reserved
RECORD = struct.Struct('<BBHiiii')       # type, flags, n_points, minlat,maxlat,minlon,maxlon
POINT = struct.Struct('<ii')             # lat_e7, lon_e7

# Stable feature classes consumed by the embedded renderer.
ROAD = 1
CYCLE = 2
PATH = 3
WATER = 4
RAIL = 5
GREEN = 6
BUILDING = 7
POI = 8


def classify(tags):
    highway = tags.get('highway', '')
    if highway in ('cycleway',):
        return CYCLE
    if highway in ('path', 'footway', 'track', 'bridleway', 'steps', 'pedestrian'):
        return PATH
    if highway in ('motorway', 'motorway_link', 'trunk', 'trunk_link', 'primary',
                   'primary_link', 'secondary', 'secondary_link', 'tertiary',
                   'tertiary_link', 'unclassified', 'residential', 'living_street',
                   'service', 'road'):
        return ROAD
    if 'waterway' in tags or tags.get('natural') in ('water', 'coastline'):
        return WATER
    if 'railway' in tags:
        return RAIL
    if tags.get('landuse') in ('forest', 'grass', 'meadow', 'orchard', 'vineyard') or tags.get('natural') in ('wood', 'scrub', 'heath', 'grassland'):
        return GREEN
    if 'building' in tags:
        return BUILDING
    if any(k in tags for k in ('mountain_pass', 'amenity', 'shop', 'tourism', 'natural', 'historic', 'place')):
        return POI
    return 0


def in_bbox(coords, bbox):
    if bbox is None:
        return True
    west, south, east, north = bbox
    return west <= coords[0] <= east and south <= coords[1] <= north


def flatten_geometry(geom):
    if not geom:
        return []
    kind, c = geom.get('type'), geom.get('coordinates')
    if kind == 'Point':
        return [(c, True)]
    if kind == 'MultiPoint':
        return [(p, True) for p in c]
    if kind == 'LineString':
        return [(c, False)]
    if kind == 'MultiLineString':
        return [(line, False) for line in c]
    if kind == 'Polygon':
        return [(ring, False) for ring in c]
    if kind == 'MultiPolygon':
        return [(ring, False) for poly in c for ring in poly]
    return []


def convert(src, dst, bbox=None):
    data = json.loads(Path(src).read_text(encoding='utf-8'))
    features = []
    for feature in data.get('features', []):
        props = feature.get('properties') or {}
        tags = props.get('tags') if isinstance(props.get('tags'), dict) else props
        kind = classify(tags)
        if not kind:
            continue
        for coords, is_point in flatten_geometry(feature.get('geometry')):
            if not coords:
                continue
            if is_point:
                if not in_bbox(coords, bbox):
                    continue
                pts = [(coords[1], coords[0])]
                record_type = POI
            else:
                # Keep complete ways when any point is inside the requested rectangle. This
                # preserves geometry continuity; the device performs the final viewport clip.
                if bbox and not any(in_bbox(p, bbox) for p in coords):
                    # A long segment can cross the bbox with both endpoints outside.
                    west, south, east, north = bbox
                    if not any(west <= p[0] <= east for p in coords):
                        continue
                    if not any(south <= p[1] <= north for p in coords):
                        continue
                pts = [(p[1], p[0]) for p in coords if len(p) >= 2 and
                       math.isfinite(p[0]) and math.isfinite(p[1])]
                record_type = kind
            if not pts or len(pts) > 65535:
                continue
            lat = [round(p[0] * 1e7) for p in pts]
            lon = [round(p[1] * 1e7) for p in pts]
            features.append((record_type, pts, min(lat), max(lat), min(lon), max(lon)))

    # A deterministic record order makes generated files easy to compare in git/tests.
    features.sort(key=lambda f: (f[0], f[2], f[4], len(f[1])))
    with open(dst, 'wb') as out:
        out.write(HEADER.pack(MAGIC, VERSION, 0, len(features), 0))
        for kind, pts, minlat, maxlat, minlon, maxlon in features:
            out.write(RECORD.pack(kind, 0, len(pts), minlat, maxlat, minlon, maxlon))
            for lat, lon in pts:
                out.write(POINT.pack(round(lat * 1e7), round(lon * 1e7)))
    return len(features), sum(len(f[1]) for f in features)


def parse_bbox(s):
    try:
        v = [float(x) for x in s.split(',')]
        if len(v) != 4 or not (-180 <= v[0] <= v[2] <= 180 and -90 <= v[1] <= v[3] <= 90):
            raise ValueError
        return tuple(v)
    except ValueError:
        raise argparse.ArgumentTypeError('bbox attendue : ouest,sud,est,nord (degrés)')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('geojson', help='export GeoJSON d’Overpass ou GeoJSON OSM')
    p.add_argument('output', help='fichier cartographique .gvm à créer')
    p.add_argument('--bbox', type=parse_bbox, help='rectangle optionnel : ouest,sud,est,nord')
    a = p.parse_args()
    try:
        n, pts = convert(a.geojson, a.output, a.bbox)
    except (OSError, json.JSONDecodeError, KeyError, struct.error) as e:
        print(f'Erreur : {e}', file=sys.stderr)
        return 1
    print(f'Carte créée : {a.output} — {n} objets, {pts} points, {Path(a.output).stat().st_size:,} octets')
    print('Crédit requis sur l’appareil : © contributeurs OpenStreetMap (ODbL).')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
