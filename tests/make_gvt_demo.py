#!/usr/bin/env python3
"""Genere un GeoJSON de demonstration (petite ville fictive) pour juger la lisibilite des styles de carte.

Usage : python3 tests/make_gvt_demo.py sortie.geojson
Centre : lon 5.60, lat 45.20. Grille de rues, routes principales, rocade, riviere, voie ferree, lac, pistes et chemins.
"""
import json
import math
import sys

LON0, LAT0 = 5.60, 45.20


def pt(dx, dy):
    return [LON0 + dx / (111320.0 * math.cos(math.radians(LAT0))), LAT0 + dy / 110574.0]


def feat(tags, coords, kind='LineString'):
    return {'type': 'Feature', 'properties': tags, 'geometry': {'type': kind, 'coordinates': coords}}


def main():
    out = []
    for i in range(-12, 13):
        kind = 'primary' if i == 0 else ('secondary' if i % 5 == 0 else 'residential')
        out.append(feat({'highway': kind}, [pt(-1500, i * 120), pt(1500, i * 120)]))
        out.append(feat({'highway': kind}, [pt(i * 120, -1500), pt(i * 120, 1500)]))
    out.append(feat({'highway': 'tertiary'}, [pt(1800 * math.cos(a), 1800 * math.sin(a)) for a in [2 * math.pi * k / 120 for k in range(121)]]))
    out.append(feat({'highway': 'primary'}, [pt(-3000, -3000), pt(-1500, -1500)]))
    out.append(feat({'highway': 'primary'}, [pt(1500, 1500), pt(3000, 3000)]))
    river = [pt(-3000 + 50 * k, -800 + 0.22 * 50 * k + 160 * math.sin(k / 9.0)) for k in range(121)]
    out.append(feat({'waterway': 'river'}, river))
    out.append(feat({'highway': 'cycleway'}, [pt(-3000 + 50 * k + 40, -800 + 0.22 * 50 * k + 160 * math.sin(k / 9.0) + 40) for k in range(121)]))
    out.append(feat({'waterway': 'stream'}, [pt(-300 + 25 * k, 2500 - 30 * k + 60 * math.sin(k / 3.0)) for k in range(60)]))
    out.append(feat({'railway': 'rail'}, [pt(-3000, 1100), pt(-800, 1000), pt(600, 1250), pt(3000, 1150)]))
    lac = [pt(1000 + 350 * math.cos(a), -1250 + 250 * math.sin(a)) for a in [2 * math.pi * k / 48 for k in range(48)]]
    lac.append(lac[0])
    out.append(feat({'natural': 'water'}, [lac], 'Polygon'))
    for k in range(14):
        a = 2 * math.pi * k / 14
        out.append(feat({'highway': 'track' if k % 2 else 'path'},
                        [pt((1900 + 25 * j) * math.cos(a + 0.003 * j * j / 10), (1900 + 25 * j) * math.sin(a + 0.003 * j * j / 10)) for j in range(30)]))
    def circle(r, n=96, cx=0.0, cy=0.0, rev=False):
        pts = [pt(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n)]
        pts.append(pts[0])
        return pts[::-1] if rev else pts
    def rect(x0, y0, x1, y1):
        return [pt(x0, y0), pt(x1, y0), pt(x1, y1), pt(x0, y1), pt(x0, y0)]
    # surfaces : ville, couronne de foret avec trous, champs, parcs, roche
    out.append(feat({'landuse': 'residential'}, [rect(-1500, -1500, 1500, 1500)], 'Polygon'))
    out.append(feat({'landuse': 'industrial'}, [rect(700, 1100, 1500, 1500)], 'Polygon'))
    out.append(feat({'landuse': 'forest'}, [circle(3100, 120), circle(2150, 96, rev=True)], 'Polygon'))
    out.append(feat({'natural': 'wood'}, [rect(-2900, 1600, -2000, 2500)], 'Polygon'))
    for i, (x0, y0) in enumerate(((1800, 1800), (-2700, -1100), (2200, -300), (-800, 2200))):
        out.append(feat({'landuse': 'farmland' if i % 2 == 0 else 'meadow'}, [rect(x0, y0, x0 + 700 + 100 * i, y0 + 600)], 'Polygon'))
    for x0, y0 in ((-360, -360), (360, 480), (-840, 600)):
        out.append(feat({'leisure': 'park'}, [rect(x0, y0, x0 + 240, y0 + 240)], 'Polygon'))
    out.append(feat({'natural': 'bare_rock'}, [[pt(-2800 + 500 * math.cos(a) * (1 + 0.3 * math.sin(4 * a)), 2900 + 300 * math.sin(a)) for a in [2 * math.pi * k / 40 for k in range(41)]]], 'Polygon'))
    for k in range(10):
        out.append(feat({'highway': 'footway'}, [pt(-700 + 140 * k, -500), pt(-650 + 140 * k, -250), pt(-700 + 140 * k, 0)]))
    json.dump({'type': 'FeatureCollection', 'features': out}, open(sys.argv[1], 'w'))
    print('%d objets ecrits dans %s' % (len(out), sys.argv[1]))


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main()
