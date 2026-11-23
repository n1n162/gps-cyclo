#!/usr/bin/env python3
"""Map-match a GPX to a connected OSM bicycle-road graph.

Input OSM files are Overpass JSON with way geometry (``out body geom`` is
recommended; geometry-only files are also accepted, using coordinates as node
identifiers). This is an offline preprocessing tool, not an on-device router.
"""
import argparse
import heapq
import json
import math
import xml.etree.ElementTree as ET

EARTH_R = 6371008.8
SKIP_HIGHWAYS = {'motorway', 'motorway_link', 'trunk', 'trunk_link', 'construction', 'proposed'}


def hav(a, b):
    p1, p2 = math.radians(a[0]), math.radians(b[0])
    dp, dl = p2 - p1, math.radians(b[1] - a[1])
    h = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * EARTH_R * math.asin(min(1.0, math.sqrt(h)))


def local_xy(point, origin):
    return (math.radians(point[1] - origin[1]) * EARTH_R * math.cos(math.radians(origin[0])),
            math.radians(point[0] - origin[0]) * EARTH_R)


def project_to_segment(p, a, b):
    ax, ay = local_xy(a, p)
    bx, by = local_xy(b, p)
    dx, dy = bx - ax, by - ay
    den = dx * dx + dy * dy
    t = 0.0 if den == 0 else max(0.0, min(1.0, -(ax * dx + ay * dy) / den))
    q = (a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]))
    return hav(p, q), t, q


def read_gpx(path):
    root = ET.parse(path).getroot()
    pts = []
    for el in root.iter():
        if el.tag.rsplit('}', 1)[-1] not in ('trkpt', 'rtept'):
            continue
        try:
            pts.append((float(el.attrib['lat']), float(el.attrib['lon'])))
        except (KeyError, ValueError):
            continue
    if len(pts) < 2:
        raise ValueError('GPX must contain at least two track or route points')
    return pts


def resample(points, spacing=80.0):
    """Keep the GPX order and create anchors at a bounded distance interval."""
    out = [points[0]]
    carry = 0.0
    for a, b in zip(points, points[1:]):
        length = hav(a, b)
        if length == 0:
            continue
        d = spacing - carry
        while d < length:
            t = d / length
            out.append((a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1])))
            d += spacing
        carry = (carry + length) % spacing
        out.append(b)
    return out


def node_key(node, point):
    if node is not None:
        return ('id', int(node))
    return ('xy', round(point[0], 7), round(point[1], 7))


def load_graph(paths):
    graph, arcs = {}, []
    seen = set()
    for path in paths:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
        for el in data.get('elements', []):
            if el.get('type') != 'way' or not el.get('tags', {}).get('highway'):
                continue
            if el.get('id') in seen:
                continue
            seen.add(el.get('id'))
            tags = el.get('tags', {})
            highway = tags.get('highway', '')
            if highway in SKIP_HIGHWAYS or tags.get('access') in ('no', 'private') or tags.get('bicycle') == 'no':
                continue
            geom = el.get('geometry') or []
            if len(geom) < 2:
                continue
            coords = [(float(p['lat']), float(p['lon'])) for p in geom]
            node_ids = el.get('nodes')
            if not isinstance(node_ids, list) or len(node_ids) != len(coords):
                node_ids = [None] * len(coords)
            name = tags.get('name', '')
            oneway = tags.get('oneway') in ('yes', '1', 'true', '-1')
            reverse_only = tags.get('oneway') == '-1'
            for i, (a, b) in enumerate(zip(coords, coords[1:])):
                u, v = node_key(node_ids[i], a), node_key(node_ids[i + 1], b)
                length = hav(a, b)
                if length <= 0.01:
                    continue
                if not reverse_only:
                    arc = {'u': u, 'v': v, 'a': a, 'b': b, 'len': length, 'way': el['id'], 'name': name}
                    arcs.append(arc)
                    graph.setdefault(u, []).append(arc)
                if not oneway or reverse_only:
                    arc = {'u': v, 'v': u, 'a': b, 'b': a, 'len': length, 'way': el['id'], 'name': name}
                    arcs.append(arc)
                    graph.setdefault(v, []).append(arc)
    if not arcs:
        raise ValueError('No usable OSM highway ways found')
    return graph, arcs


def candidates(point, arcs, max_snap, limit=8):
    found = []
    for arc in arcs:
        dist, t, q = project_to_segment(point, arc['a'], arc['b'])
        if dist <= max_snap:
            found.append({'arc': arc, 't': t, 'point': q, 'snap': dist})
    found.sort(key=lambda c: c['snap'])
    return found[:limit]


def shortest(graph, start, goal, limit):
    if start == goal:
        return 0.0, []
    dist = {start: 0.0}
    prev = {}
    heap = [(0.0, start)]
    while heap:
        cost, node = heapq.heappop(heap)
        if cost != dist.get(node):
            continue
        if cost > limit:
            break
        if node == goal:
            path = []
            while node != start:
                parent, arc = prev[node]
                path.append(arc)
                node = parent
            path.reverse()
            return cost, path
        for arc in graph.get(node, ()):
            nd = cost + arc['len']
            if nd <= limit and nd < dist.get(arc['v'], float('inf')):
                dist[arc['v']] = nd
                prev[arc['v']] = (node, arc)
                heapq.heappush(heap, (nd, arc['v']))
    return None


def connect(graph, left, right, expected):
    """Return a directed, connected network path between two edge projections."""
    a, b = left['arc'], right['arc']
    if a is b and right['t'] >= left['t']:
        return (right['t'] - left['t']) * a['len'], []
    # The route leaves the first directed edge at its end and joins the next
    # directed edge at its start. Bound detours to avoid unrelated graph loops.
    middle = shortest(graph, a['v'], b['u'], max(500.0, expected * 4.0 + 100.0))
    if middle is None:
        return None
    cost, path = middle
    total = (1.0 - left['t']) * a['len'] + cost + right['t'] * b['len']
    return total, path


def route_match(anchors, graph, arcs, max_snap):
    choices = [candidates(p, arcs, max_snap) for p in anchors]
    if any(not c for c in choices):
        bad = next(i for i, c in enumerate(choices) if not c)
        raise ValueError('No OSM road within %.1f m of GPX anchor %d/%d' % (max_snap, bad + 1, len(anchors)))
    costs = [c['snap'] ** 2 for c in choices[0]]
    parents = []
    links = []
    for i in range(1, len(choices)):
        expected = hav(anchors[i - 1], anchors[i])
        row_costs, row_parents, row_links = [], [], []
        for right in choices[i]:
            best_cost, best_parent, best_link = float('inf'), -1, None
            for j, left in enumerate(choices[i - 1]):
                link = connect(graph, left, right, expected)
                if link is None:
                    continue
                length, path = link
                # Preserve GPX progression while preferring close snaps and
                # rejecting implausible graph detours or discontinuities.
                score = costs[j] + right['snap'] ** 2 + 0.25 * (length - expected) ** 2
                if score < best_cost:
                    best_cost, best_parent, best_link = score, j, (length, path)
            row_costs.append(best_cost)
            row_parents.append(best_parent)
            row_links.append(best_link)
        if min(row_costs) == float('inf'):
            raise ValueError('OSM graph is disconnected between GPX anchors %d and %d' % (i, i + 1))
        costs, parents = row_costs, parents + [row_parents]
        links.append(row_links)
    idx = min(range(len(costs)), key=costs.__getitem__)
    selected = [None] * len(choices)
    selected[-1] = idx
    for i in range(len(choices) - 1, 0, -1):
        selected[i - 1] = parents[i - 1][selected[i]]
    return choices, selected, links


def route_geometry(choices, selected, links):
    coords, path_arcs = [], []
    first = choices[0][selected[0]]
    coords.append(first['point'])
    for i in range(1, len(choices)):
        left = choices[i - 1][selected[i - 1]]
        right = choices[i][selected[i]]
        coords.append(left['arc']['b'])
        path_arcs.extend(links[i - 1][selected[i]][1])
        for arc in links[i - 1][selected[i]][1]:
            coords.append(arc['b'])
        coords.append(right['arc']['a'])
        coords.append(right['point'])
    # Collapse adjacent duplicate coordinates while retaining order.
    compact = []
    for p in coords:
        if not compact or hav(compact[-1], p) > 0.5:
            compact.append(p)
    return compact


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('gpx')
    ap.add_argument('osm', nargs='+', help='Overpass JSON extracts')
    ap.add_argument('--out', required=True, help='output route GeoJSON')
    ap.add_argument('--max-snap-m', type=float, default=30.0)
    ap.add_argument('--spacing-m', type=float, default=80.0, help='GPX anchor interval')
    args = ap.parse_args()
    if args.max_snap_m <= 0 or args.spacing_m <= 0:
        ap.error('--max-snap-m and --spacing-m must be positive')
    try:
        original = read_gpx(args.gpx)
        anchors = resample(original, args.spacing_m)
        graph, arcs = load_graph(args.osm)
        choices, selected, links = route_match(anchors, graph, arcs, args.max_snap_m)
        coords = route_geometry(choices, selected, links)
    except (OSError, ValueError, json.JSONDecodeError, ET.ParseError) as e:
        ap.exit(2, 'gpx_to_osm_route: %s\n' % e)
    maneuvers = []
    prev = None
    distance = 0.0
    for i in range(1, len(coords)):
        distance += hav(coords[i - 1], coords[i])
    # The route geometry is topologically connected; maneuver generation uses
    # changes between selected OSM way IDs, not turns inferred from the raw GPX.
    chosen_arcs = [choices[i][selected[i]]['arc'] for i in range(len(choices))]
    progress = 0.0
    for i, arc in enumerate(chosen_arcs):
        if prev is not None and arc['way'] != prev['way']:
            point = choices[i][selected[i]]['point']
            maneuvers.append({'type': 'intersection', 'lat': point[0], 'lon': point[1],
                              'from_way': prev['way'], 'to_way': arc['way'],
                              'from_name': prev['name'], 'to_name': arc['name'],
                              'distance_from_start_m': round(progress, 1)})
        if i:
            progress += links[i - 1][selected[i]][0]
        prev = arc
    output = {
        'type': 'FeatureCollection',
        'properties': {'format': 'GPS-CYCLO-ROUTE-1', 'source_gpx': args.gpx,
                       'anchors': len(anchors), 'matched_anchors': len(selected),
                       'network_distance_m': round(distance, 1)},
        'features': [{'type': 'Feature', 'properties': {'type': 'matched_route'},
                      'geometry': {'type': 'LineString', 'coordinates': [[lon, lat] for lat, lon in coords]}}],
        'maneuvers': maneuvers,
    }
    with open(args.out, 'w', encoding='utf-8') as f:
        json.dump(output, f, ensure_ascii=False, indent=2)
    print('Wrote connected OSM-matched route: %s (%d anchors, %d intersections)' %
          (args.out, len(anchors), len(maneuvers)))


if __name__ == '__main__':
    main()
