#!/usr/bin/env python3
"""Genere un GeoJSON synthetique (deterministe) pour tester tools/osm_to_gvt.py.

Usage : python3 tests/make_gvt_fixture.py sortie.geojson
Le point de depart de l'autoroute (lon 5.00, lat 45.20) sert de repere au test C (voir Makefile).
"""
import json
import math
import sys


def feat(tags, coords, kind='LineString'):
    return {'type': 'Feature', 'properties': tags, 'geometry': {'type': kind, 'coordinates': coords}}


def metres(lon, lat, dx, dy):
    """Deplace un point de dx metres vers l'est et dy metres vers le nord."""
    return [lon + dx / (111320.0 * math.cos(math.radians(lat))), lat + dy / 110574.0]


def tile_edge_lon(z, lon):
    n = 1 << z
    x = math.floor((lon + 180.0) / 360.0 * n)
    return x / n * 360.0 - 180.0


def tile_center(z, lon, lat):
    """Centre (lon, lat) de la tuile du zoom z qui contient le point."""
    n = 1 << z
    x = math.floor((lon + 180.0) / 360.0 * n)
    sn = math.sin(math.radians(lat))
    y = math.floor((0.5 - math.log((1.0 + sn) / (1.0 - sn)) / (4.0 * math.pi)) * n)
    return ((x + 0.5) / n * 360.0 - 180.0,
            math.degrees(math.atan(math.sinh(math.pi * (1.0 - 2.0 * (y + 0.5) / n)))))


def main():
    out = []
    # 1. autoroute en diagonale, un peu sinueuse (traverse beaucoup de tuiles)
    moto = [[5.00 + 0.12 * i / 40, 45.20 + 0.08 * i / 40 + 0.0006 * math.sin(i)] for i in range(41)]
    moto[0] = [5.00, 45.20]
    out.append(feat({'highway': 'motorway', 'ref': 'A48'}, moto))
    # 2. route secondaire exactement sur un bord de tuile du zoom 15
    edge = tile_edge_lon(15, 5.50)
    out.append(feat({'highway': 'secondary'}, [[edge, 45.45], [edge, 45.46], [edge, 45.47]]))
    # 3. route tertiaire, puis piste cyclable
    out.append(feat({'highway': 'tertiary'}, [metres(5.25, 45.25, 100 * i, 30 * math.cos(i / 3.0)) for i in range(40)]))
    out.append(feat({'highway': 'cycleway'}, [metres(5.30, 45.30, 120 * i, 40 * math.sin(i / 2.0)) for i in range(30)]))
    # 4. serpentin dense, entierement dans une seule tuile (force le decoupage en morceaux de 1024 points au zoom 15)
    cx, cy = tile_center(15, 5.60, 45.40)
    base = metres(cx, cy, -200, -200)          # bloc de 400 m centre dans une tuile du zoom 15
    serp = []
    for row in range(80):
        for k in range(100):
            x = 400.0 * k / 99 if row % 2 == 0 else 400.0 * (99 - k) / 99
            serp.append(metres(base[0], base[1], x, row * 5.0 + (4.0 if k % 2 else 0.0)))
    out.append(feat({'highway': 'path'}, serp))
    # 5. lac (anneau ferme) et petit etang
    lac = [metres(5.45, 45.45, 800 * math.cos(a), 800 * math.sin(a)) for a in [2 * math.pi * i / 60 for i in range(60)]]
    lac.append(lac[0])
    out.append(feat({'natural': 'water'}, [lac], 'Polygon'))
    etang = [metres(5.40, 45.35, 3 * math.cos(a), 3 * math.sin(a)) for a in [2 * math.pi * i / 12 for i in range(12)]]
    etang.append(etang[0])
    out.append(feat({'natural': 'water'}, [etang], 'Polygon'))
    # 6. riviere sinueuse
    out.append(feat({'waterway': 'river'}, [metres(5.20, 45.60, 150 * i, 300 * math.sin(i / 8.0)) for i in range(200)]))
    # 7. voie ferree : tres long segment droit (2 sommets) puis coude
    out.append(feat({'railway': 'rail'}, [[5.10, 45.10], [5.35, 45.15], [5.40, 45.30]]))
    # 8. route de service (zoom 15 seulement) et chemin pietonnier
    out.append(feat({'highway': 'service'}, [metres(5.60, 45.41, 30 * i, 20 * math.sin(i)) for i in range(25)]))
    # 8bis. surfaces : grande foret avec une clairiere (des centaines de tuiles au zoom 15, tuiles entierement a l'interieur),
    #       zone urbaine concave, champ aligne sur une tuile, parc, roche, plan d'eau dont un trou (ile)
    def square(lon, lat, half_km, n=1):
        return [metres(lon, lat, dx * half_km * 1000, dy * half_km * 1000) for dx, dy in ((-1, -1), (1, -1), (1, 1), (-1, 1), (-1, -1))]
    out.append(feat({'landuse': 'forest'}, [square(5.80, 45.60, 6.0), square(5.80, 45.60, 1.6)[::-1]], 'Polygon'))
    star = []
    for k in range(24):
        r = 1400 if k % 2 == 0 else 600
        a = 2 * math.pi * k / 24
        star.append(metres(5.06, 45.24, r * math.cos(a), r * math.sin(a)))
    star.append(star[0])
    out.append(feat({'landuse': 'residential'}, [star], 'Polygon'))
    n15 = 1 << 15
    fx0, fx1 = 16900 / n15 * 360.0 - 180.0, 16902 / n15 * 360.0 - 180.0
    def lat_of(ty):
        return math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * ty / n15))))
    out.append(feat({'landuse': 'farmland'}, [[[fx0, lat_of(11760)], [fx1, lat_of(11760)], [fx1, lat_of(11762)], [fx0, lat_of(11762)], [fx0, lat_of(11760)]]], 'Polygon'))
    out.append(feat({'leisure': 'park'}, [[metres(5.50, 45.30, 700 * math.cos(a) * (1 + 0.25 * math.sin(5 * a)), 500 * math.sin(a) * (1 + 0.25 * math.sin(5 * a)))
                                             for a in [2 * math.pi * k / 40 for k in range(41)]]], 'Polygon'))
    out.append(feat({'natural': 'bare_rock'}, [[metres(5.90, 45.30, 1500 * math.cos(a) * (1 + 0.4 * math.sin(3 * a)), 900 * math.sin(a))
                                                  for a in [2 * math.pi * k / 60 for k in range(61)]]], 'Polygon'))
    pond = [metres(5.70, 45.55, 900 * math.cos(a), 600 * math.sin(a)) for a in [2 * math.pi * k / 48 for k in range(49)]]
    isle = [metres(5.70, 45.55, 250 * math.cos(a), 150 * math.sin(a)) for a in [2 * math.pi * k / 24 for k in range(24, -1, -1)]]
    out.append(feat({'natural': 'water'}, [pond, isle], 'Polygon'))
    out.append(feat({'natural': 'coastline'}, [metres(5.95, 45.15, 400 * i, 120 * math.sin(i / 3.0)) for i in range(40)]))
    # 9. objets ignores : batiment, point, ligne sans classe
    out.append(feat({'building': 'yes'}, [[[5.5, 45.5], [5.5001, 45.5], [5.5001, 45.5001], [5.5, 45.5]]], 'Polygon'))
    out.append(feat({'place': 'village', 'name': 'Test'}, [5.5, 45.5], 'Point'))
    out.append(feat({'highway': 'proposed'}, [[5.5, 45.5], [5.6, 45.6]]))
    json.dump({'type': 'FeatureCollection', 'generator': 'fixture synthetique GVT1', 'features': out},
              open(sys.argv[1], 'w'))
    print('%d objets ecrits dans %s' % (len(out), sys.argv[1]))


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main()
