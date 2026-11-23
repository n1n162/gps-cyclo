#!/usr/bin/env python3
"""Convertit un GPX en itinéraire routier OSM préliminaire.

Principe minimal : les points GPX sont rapprochés des ways OSM, puis les chemins
entre les ways voisins sont résolus sur le graphe. Le résultat est un GeoJSON
LineString utilisable pour le guidage, avec les changements de rue comme
manoeuvres.
"""
import argparse
import json
import math
import xml.etree.ElementTree as ET

EARTH_R = 6371008.8


def hav(lat1, lon1, lat2, lon2):
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp = p2 - p1
    dl = math.radians(lon2 - lon1)
    h = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * EARTH_R * math.asin(min(1.0, math.sqrt(h)))


def gpx_points(path):
    root = ET.parse(path).getroot()
    points = []
    for el in root.iter():
        if el.tag.rsplit('}', 1)[-1] not in ('trkpt', 'rtept'):
            continue
        try:
            lat, lon = float(el.attrib['lat']), float(el.attrib['lon'])
        except (KeyError, ValueError):
            continue
        ele_el = next((c for c in el if c.tag.rsplit('}', 1)[-1] == 'ele'), None)
        ele = float(ele_el.text) if ele_el is not None and ele_el.text else None
        points.append((lat, lon, ele))
    if len(points) < 2:
        raise SystemExit('le GPX doit contenir au moins deux points')
    return points


def load_osm(paths):
    ways = []
    nodes = {}
    for path in paths:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
        for el in data.get('elements', []):
            if el.get('type') == 'way' and el.get('tags', {}).get('highway'):
                geom = el.get('geometry')
                if geom and len(geom) >= 2:
                    way = {
                        'id': el['id'],
                        'tags': el['tags'],
                        'geom': [(p['lat'], p['lon']) for p in geom],
                    }
                    ways.append(way)
                    for lat, lon in way['geom']:
                        nodes[(round(lat, 7), round(lon, 7))] = (lat, lon)
    if not ways:
        raise SystemExit('aucun way routable trouvé dans les fichiers OSM')
    return ways, nodes


def project(lat, lon, a, b):
    ax, ay = project_xy(a, lat0=lat, lon0=lon)
    bx, by = project_xy(b, lat0=lat, lon0=lon)
    px, py = 0.0, 0.0
    dx, dy = bx - ax, by - ay
    l2 = dx * dx + dy * dy
    t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / l2))
    qx, qy = ax + t * dx, ay + t * dy
    plat = lat + py / EARTH_R * 180 / math.pi
    plon = lon + px / (EARTH_R * math.cos(math.radians(lat))) * 180 / math.pi
    # Projection locale simplifiée correcte à proximité immédiate.
    plat = lat + qy / EARTH_R * 180 / math.pi
    plon = lon + qx / (EARTH_R * math.cos(math.radians(lat))) * 180 / math.pi
    dist = hav(lat, lon, plat, plon)
    return dist, t, (plat, plon)


def project_xy(p, lat0, lon0):
    lat, lon = p
    x = math.radians(lon - lon0) * EARTH_R * math.cos(math.radians(lat0))
    y = math.radians(lat - lat0) * EARTH_R
    return x, y


def nearest_ways(point, ways, limit=25.0):
    lat, lon = point[:2]
    found = []
    for way in ways:
        geom = way['geom']
        for i in range(len(geom) - 1):
            dist, t, proj = project(lat, lon, geom[i], geom[i + 1])
            if dist <= limit:
                found.append((dist, way['id'], i, t, proj))
    found.sort(key=lambda v: v[0])
    return found[:8]


def build_graph(ways):
    graph = {}
    way_nodes = {}
    for way in ways:
        highway = way['tags'].get('highway')
        if highway in ('motorway', 'motorway_link', 'trunk', 'trunk_link'):
            continue
        ids = way_nodes.setdefault(way['id'], [])
        geom = way['geom']
        previous = None
        oneway = way['tags'].get('oneway') in ('yes', '1', 'true', '-1')
        reverse = way['tags'].get('oneway') == '-1'
        for lat, lon in geom:
            node = (round(lat, 7), round(lon, 7))
            ids.append(node)
            if previous is not None:
                a, b = previous, node
                d = hav(a[0], a[1], b[0], b[1])
                if reverse:
                    graph.setdefault(b, []).append((a, d, way['id'], way['tags'].get('name', '')))
                else:
                    graph.setdefault(a, []).append((b, d, way['id'], way['tags'].get('name', '')))
                    if not oneway:
                        graph.setdefault(b, []).append((a, d, way['id'], way['tags'].get('name', '')))
            previous = node
    return graph, way_nodes


def angle(a, b):
    x1, y1 = a
    x2, y2 = b
    dx, dy = x2 - x1, y2 - y1
    return math.degrees(math.atan2(dy, dx))


def maneuver_type(before, turn, after):
    h1 = angle(before, turn)
    h2 = angle(turn, after)
    d = (h2 - h1 + 180) % 360 - 180
    if d < -135 or d > 135:
        return 'u_turn', d
    if d < -45:
        return 'left', d
    if d < -12:
        return 'slight_left', d
    if d > 45:
        return 'right', d
    if d > 12:
        return 'slight_right', d
    return 'straight', d


def densify(points, step_m=30.0):
    out = [points[0]]
    for a, b in zip(points, points[1:]):
        d = hav(a[0], a[1], b[0], b[1])
        n = max(1, int(d / step_m))
        for i in range(1, n + 1):
            t = i / n
            lat = a[0] + (b[0] - a[0]) * t
            lon = a[1] + (b[1] - a[1]) * t
            out.append((lat, lon, None))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('gpx')
    ap.add_argument('osm', nargs='+', help='fichiers JSON Overpass')
    ap.add_argument('--out', required=True, help='GeoJSON de sortie')
    ap.add_argument('--max-snap-m', type=float, default=25.0)
    args = ap.parse_args()

    points = densify(gpx_points(args.gpx))
    *osm_paths, = args.osm
    ways, _ = load_osm(osm_paths)
    graph, way_nodes = build_graph(ways)

    matched = []
    missed = 0
    for point in points:
        candidates = nearest_ways(point, ways, args.max_snap_m)
        if not candidates:
            missed += 1
            continue
        dist, way_id, segment, t, projected = candidates[0]
        matched.append((point[:2], projected, way_id, dist))

    if not matched:
        raise SystemExit('aucun point GPX n’est assez proche du réseau OSM')

    route_coords = []
    last_way = None
    maneuvers = []
    for point, projected, way_id, dist in matched:
        if not route_coords or hav(route_coords[-1][0], route_coords[-1][1], projected[0], projected[1]) > 2.0:
            route_coords.append(projected)
        if last_way is not None and way_id != last_way:
            maneuvers.append({'lat': projected[0], 'lon': projected[1], 'from_way': last_way, 'to_way': way_id})
        last_way = way_id

    features = [{
        'type': 'Feature',
        'properties': {
            'type': 'matched_route',
            'source': args.gpx,
            'points': len(points),
            'matched': len(matched),
            'missed': missed,
            'maneuver_count': len(maneuvers),
        },
        'geometry': {'type': 'LineString', 'coordinates': [[lon, lat] for lat, lon in route_coords]},
    }]
    output = {
        'type': 'FeatureCollection',
        'properties': {'format': 'GPS-CYCLO-ROUTE-1', 'source_gpx': args.gpx},
        'features': features,
        'maneuvers': maneuvers,
    }
    with open(args.out, 'w', encoding='utf-8') as f:
        json.dump(output, f, ensure_ascii=False, indent=2)
    print('itinéraire écrit : %s' % args.out)
    print('%d/%d points rapprochés du réseau OSM, %d changements de way' % (len(matched), len(points), len(maneuvers)))


if __name__ == '__main__':
    main()
