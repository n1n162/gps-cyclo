#!/usr/bin/env python3
"""Tests du format compact GCR1."""
import json
import os
import struct
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, 'tools', 'route_to_gcr1.py')


class Gcr1Tests(unittest.TestCase):
    def test_geojson_round_trip_layout(self):
        route = {
            'properties': {'format': 'GPS-CYCLO-ROUTE-1'},
            'maneuvers': [{'type': 'right', 'distance_from_start_m': 1234,
                           'lat': 45.12, 'lon': 5.34,
                           'from_name': 'Ancienne', 'to_name': 'Nouvelle'}],
        }
        with tempfile.TemporaryDirectory() as tmp:
            source = os.path.join(tmp, 'route.geojson')
            packed = os.path.join(tmp, 'route.gcr1')
            with open(source, 'w', encoding='utf-8') as f:
                json.dump(route, f)
            proc = subprocess.run(['python3', SCRIPT, source, packed],
                                  capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            with open(packed, 'rb') as f:
                data = f.read()

        self.assertEqual(data[:4], b'GCR1')
        count, = struct.unpack_from('<I', data, 4)
        self.assertEqual(count, 1)
        kind, distance, lat, lon = struct.unpack_from('<BIff', data, 8)
        self.assertEqual(kind, 4)
        self.assertEqual(distance, 1234)
        self.assertAlmostEqual(lat, 45.12, places=5)
        self.assertAlmostEqual(lon, 5.34, places=5)
        from_len, to_len = struct.unpack_from('<HH', data, 21)
        self.assertEqual(data[25:25 + from_len], b'Ancienne')
        self.assertEqual(data[25 + from_len:25 + from_len + to_len], b'Nouvelle')
        self.assertEqual(len(data), 25 + from_len + to_len)


if __name__ == '__main__':
    unittest.main()
