#!/usr/bin/env python3
"""Integration tests for the GPX-to-OSM route matcher."""
import json
import math
import os
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, 'tools', 'gpx_to_osm_route.py')


def distance_m(a, b):
    radius = 6371008.8
    p1, p2 = math.radians(a[1]), math.radians(b[1])
    dp, dl = p2 - p1, math.radians(b[0] - a[0])
    h = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * radius * math.asin(min(1.0, math.sqrt(h)))


class GpxToOsmRouteTests(unittest.TestCase):
    def run_converter(self, points, ways, expected_success=True):
        with tempfile.TemporaryDirectory() as tmp:
            gpx_path = os.path.join(tmp, 'route.gpx')
            osm_path = os.path.join(tmp, 'osm.json')
            out_path = os.path.join(tmp, 'matched.geojson')
            track = ''.join('<trkpt lat="%.7f" lon="%.7f"/>' % p for p in points)
            with open(gpx_path, 'w', encoding='utf-8') as f:
                f.write('<gpx><trk><trkseg>%s</trkseg></trk></gpx>' % track)
            with open(osm_path, 'w', encoding='utf-8') as f:
                json.dump({'elements': ways}, f)
            proc = subprocess.run(
                ['python3', SCRIPT, gpx_path, osm_path, '--out', out_path,
                 '--spacing-m', '1000', '--max-snap-m', '20'],
                text=True, capture_output=True)
            if expected_success:
                self.assertEqual(proc.returncode, 0, proc.stderr)
                with open(out_path, encoding='utf-8') as f:
                    return json.load(f)
            self.assertNotEqual(proc.returncode, 0, proc.stdout + proc.stderr)
            return None

    def test_straight_way_does_not_create_detour_or_backtrack(self):
        a, b = (45.0, 5.0), (45.001, 5.0)
        result = self.run_converter([a, b], [{
            'type': 'way', 'id': 1, 'nodes': [101, 102],
            'tags': {'highway': 'residential', 'name': 'Rue droite'},
            'geometry': [{'lat': a[0], 'lon': a[1]}, {'lat': b[0], 'lon': b[1]}],
        }])
        coords = result['features'][0]['geometry']['coordinates']
        self.assertLess(distance_m((coords[0][1], coords[0][0]), a), 1.0)
        self.assertLess(distance_m((coords[-1][1], coords[-1][0]), b), 1.0)
        length = sum(distance_m((p[1], p[0]), (q[1], q[0])) for p, q in zip(coords, coords[1:]))
        self.assertLess(length, distance_m(a, b) * 1.1)

    def test_unconnected_osm_components_are_rejected(self):
        a, b = (45.0, 5.0), (45.001, 5.0)
        mid_a, mid_b = (45.00045, 5.0), (45.00055, 5.0)
        ways = [
            {'type': 'way', 'id': 1, 'nodes': [1, 2],
             'tags': {'highway': 'residential'},
             'geometry': [{'lat': a[0], 'lon': a[1]}, {'lat': mid_a[0], 'lon': mid_a[1]}]},
            {'type': 'way', 'id': 2, 'nodes': [3, 4],
             'tags': {'highway': 'residential'},
             'geometry': [{'lat': mid_b[0], 'lon': mid_b[1]}, {'lat': b[0], 'lon': b[1]}]},
        ]
        self.run_converter([a, b], ways, expected_success=False)

    def test_oneway_direction_is_respected(self):
        a, b = (45.0, 5.0), (45.001, 5.0)
        way = {'type': 'way', 'id': 1, 'nodes': [1, 2],
               'tags': {'highway': 'residential', 'oneway': 'yes'},
               'geometry': [{'lat': b[0], 'lon': b[1]}, {'lat': a[0], 'lon': a[1]}]}
        self.run_converter([a, b], [way], expected_success=False)


if __name__ == '__main__':
    unittest.main()
