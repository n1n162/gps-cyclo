#!/usr/bin/env python3
"""Pack GPS-CYCLO-ROUTE-1 GeoJSON into the compact GCR1 format."""
import argparse
import json
import struct

TYPE = {
    'straight': 0,
    'slight_left': 1,
    'left': 2,
    'slight_right': 3,
    'right': 4,
    'u_turn': 5,
    'roundabout': 6,
    'intersection': 7,
    'arrival': 8,
}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('route')
    ap.add_argument('output')
    args = ap.parse_args()

    with open(args.route, encoding='utf-8') as f:
        route = json.load(f)
    if route.get('properties', {}).get('format') != 'GPS-CYCLO-ROUTE-1':
        ap.error('format d’itinéraire inattendu')

    maneuvers = route.get('maneuvers', [])
    out = bytearray(b'GCR1')
    out += struct.pack('<I', len(maneuvers))

    for maneuver in maneuvers:
        kind = TYPE.get(maneuver.get('type'), 7)
        distance = max(0, int(round(maneuver.get('distance_from_start_m', 0))))
        lat = float(maneuver.get('lat', 0.0))
        lon = float(maneuver.get('lon', 0.0))
        from_name = str(maneuver.get('from_name', '')).encode('utf-8')[:63]
        to_name = str(maneuver.get('to_name', '')).encode('utf-8')[:63]

        out += struct.pack('<BIff', kind, distance, lat, lon)
        out += struct.pack('<HH', len(from_name), len(to_name))
        out += from_name + to_name

    with open(args.output, 'wb') as f:
        f.write(out)
    print('GCR1 écrit : %s (%d manoeuvres)' % (args.output, len(maneuvers)))


if __name__ == '__main__':
    main()
