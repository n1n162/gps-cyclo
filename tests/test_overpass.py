#!/usr/bin/env python3
"""Teste tools/overpass_fetch.py et la lecture du JSON Overpass par tools/osm_to_gvt.py, sans acces au reseau :
un faux serveur Overpass tourne en local. Usage : python3 tests/test_overpass.py
Le vrai serveur Overpass n'est PAS contacte : ce test verifie notre cote (requete, decoupage, reprises, fusion,
conversion), pas le comportement du serveur."""
import filecmp
import http.server
import json
import math
import os
import subprocess
import sys
import tempfile
import threading
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
sys.path.insert(0, os.path.join(ROOT, 'tools'))
sys.path.insert(0, HERE)
import osm_to_gvt as T  # noqa: E402
import overpass_fetch as F  # noqa: E402
import verify_gvt as V  # noqa: E402

fails = 0


def check(cond, msg):
    global fails
    print('  %-78s %s' % (msg, 'OK' if cond else 'ECHEC'))
    if not cond:
        fails += 1


def to_overpass(geojson):
    """Meme contenu qu'un GeoJSON, mis en forme comme la reponse de l'API Overpass avec "out geom"."""
    els, nid = [], 1000
    for ft in geojson['features']:
        g, tags = ft['geometry'], ft['properties']
        if g['type'] == 'Point':
            els.append({'type': 'node', 'id': nid, 'lat': g['coordinates'][1], 'lon': g['coordinates'][0], 'tags': tags})
            nid += 1
            continue
        if g['type'] == 'Polygon' and len(g['coordinates']) > 1:      # polygone avec trous : relation, anneau exterieur en deux morceaux
            outer, inners = g['coordinates'][0], g['coordinates'][1:]
            cut = len(outer) // 2
            geo = lambda pts: [{'lat': p[1], 'lon': p[0]} for p in pts]
            members = [{'type': 'way', 'ref': nid + 1, 'role': 'outer', 'geometry': geo(outer[:cut + 1])},
                       {'type': 'way', 'ref': nid + 2, 'role': 'outer', 'geometry': geo(outer[cut:])}]
            members += [{'type': 'way', 'ref': nid + 3 + i, 'role': 'inner', 'geometry': geo(r)} for i, r in enumerate(inners)]
            els.append({'type': 'relation', 'id': nid, 'tags': dict(tags, type='multipolygon'), 'members': members})
            nid += 10
            continue
        for line in T.lines_of(g):
            els.append({'type': 'way', 'id': nid, 'tags': tags, 'geometry': [{'lat': p[1], 'lon': p[0]} for p in line]})
            nid += 1
    return {'version': 0.6, 'generator': 'faux serveur de test', 'elements': els}


class Fake(http.server.BaseHTTPRequestHandler):
    payload = b'{}'
    mode = 'normal'
    requests = []
    failures_left = 0

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get('Content-Length', 0))).decode('utf-8')
        Fake.requests.append((body, self.headers.get('User-Agent')))
        if Fake.failures_left > 0:
            Fake.failures_left -= 1
            self.send_response(429)
            self.end_headers()
            return
        out = Fake.payload
        if Fake.mode == 'remark':
            out = json.dumps({'elements': [], 'remark': 'runtime error: Query timed out'}).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(out)

    def log_message(self, *a):
        pass


def run(*args):
    return subprocess.run([sys.executable] + list(args), capture_output=True, text=True)


def main():
    tmp = tempfile.mkdtemp()
    gj = os.path.join(tmp, 'synthetic.geojson')
    run(os.path.join(HERE, 'make_gvt_fixture.py'), gj)
    data = json.load(open(gj))
    ov = to_overpass(data)
    Fake.payload = json.dumps(ov).encode()
    srv = http.server.HTTPServer(('127.0.0.1', 0), Fake)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = 'http://127.0.0.1:%d/api/interpreter' % srv.server_address[1]

    # 1. la requete
    q = F.build_query(45.1, 5.2, 45.3, 5.5)
    check('[out:json]' in q and q.rstrip().endswith('out geom;'), 'requete : sortie JSON et "out geom"')
    check(q.count('(45.100000,5.200000,45.300000,5.500000)') == 9, 'requete : boite sud,ouest,nord,est sur les 9 filtres')
    check(all(k in q for k in list(T.HIGHWAY) + list(T.WATERWAY) + list(T.RAILWAY) + list(T.AREA_LANDUSE) + list(T.AREA_NATURAL) + list(T.AREA_LEISURE)),
          'requete : toutes les valeurs de tags du convertisseur (lignes et surfaces)')
    check('relation[natural~' in q and 'relation[landuse~' in q and 'relation[leisure~' in q, 'requete : surfaces aussi en relations (forets, lacs)')
    q2 = F.build_query(45.1, 5.2, 45.3, 5.5, areas=False)
    check('landuse' not in q2 and 'relation' not in q2 and q2.count('(45.100000') == 3, 'requete sans surfaces : lignes seulement')

    # 2. le decoupage en cellules
    bbox = (5.0, 45.0, 5.35, 45.2)
    grid = F.cells(bbox, 10.0)
    rows = max(r for r, _, _ in grid) + 1
    cols = max(c for _, c, _ in grid) + 1
    mid = math.radians(45.1)
    check(rows == math.ceil(0.2 / (10 / 110.574)) and cols == math.ceil(0.35 / (10 / (111.320 * math.cos(mid)))),
          'cellules : %d lignes x %d colonnes attendues pour 10 km' % (rows, cols))
    area = sum((n - s) * (e - w) for _, _, (s, w, n, e) in grid)
    check(abs(area - 0.2 * 0.35) < 1e-9, 'cellules : couvrent exactement la zone, sans recouvrement')
    check(min(s for _, _, (s, w, n, e) in grid) == 45.0 and max(e for _, _, (s, w, n, e) in grid) == 5.35, 'cellules : bords de la zone respectes')
    check(len(F.cells((5.0, 45.0, 5.01, 45.01), 10.0)) == 1, 'cellules : une zone minuscule donne une cellule')

    # 2bis. la zone d'un GPX : points, marge, plusieurs fichiers, fichiers invalides
    g1, g2 = os.path.join(tmp, 'a.gpx'), os.path.join(tmp, 'b.gpx')
    open(g1, 'w').write('<?xml version="1.0"?><gpx xmlns="http://www.topografix.com/GPX/1/1"><trk><trkseg>'
                        '<trkpt lat="45.60" lon="5.40"/><trkpt lat="45.62" lon="5.45"/></trkseg></trk>'
                        '<rte><rtept lat="45.58" lon="5.43"/></rte><wpt lat="10" lon="10"/></gpx>')
    open(g2, 'w').write('<gpx><trk><trkseg><trkpt lat="45.70" lon="5.30"/></trkseg></trk></gpx>')     # sans espace de noms
    bb = F.gpx_bbox([g1], 3.0)
    check(abs(bb[1] - (45.58 - 3 / 110.574)) < 1e-9 and abs(bb[3] - (45.62 + 3 / 110.574)) < 1e-9, 'GPX : marge de 3 km en latitude, points rtept inclus, wpt ignores')
    check(bb[0] < 5.40 - 0.03 and bb[2] > 5.45 + 0.03, 'GPX : marge en longitude (plus large qu\'en latitude)')
    bb2 = F.gpx_bbox([g1, g2], 0.0)
    check(bb2 == (5.30, 45.58, 5.45, 45.70), 'GPX : plusieurs fichiers, avec ou sans espace de noms')
    bad = os.path.join(tmp, 'vide.gpx')
    open(bad, 'w').write('<gpx><wpt lat="1" lon="2"/></gpx>')
    r = run(os.path.join(ROOT, 'tools', 'overpass_fetch.py'), '--gpx', bad, '--out-dir', os.path.join(tmp, 'e0'))
    check(r.returncode != 0 and 'aucun point' in (r.stderr + r.stdout), 'GPX sans point de trace : message clair')
    r = run(os.path.join(ROOT, 'tools', 'overpass_fetch.py'), '--out-dir', os.path.join(tmp, 'e0'))
    check(r.returncode != 0 and '--bbox' in (r.stderr + r.stdout), 'ni --bbox ni --gpx : refuse')
    r = run(os.path.join(ROOT, 'tools', 'overpass_fetch.py'), '--bbox', '5,45,5.1,45.1', '--gpx', g1, '--out-dir', os.path.join(tmp, 'e0'))
    check(r.returncode != 0, '--bbox et --gpx ensemble : refuse')

    # 3. telechargement : un 429 puis la reponse, pause nulle
    out = os.path.join(tmp, 'extrait')
    Fake.requests.clear()
    Fake.failures_left = 1
    r = run(os.path.join(ROOT, 'tools', 'overpass_fetch.py'), '--bbox', '5.0,45.0,5.2,45.15', '--out-dir', out, '--cell-km', '10',
            '--server', url, '--pause', '0', '--backoff', '0')
    files = sorted(os.listdir(out)) if os.path.isdir(out) else []
    ncell = len(F.cells((5.0, 45.0, 5.2, 45.15), 10.0))
    check(r.returncode == 0 and len(files) == ncell, 'telechargement : %d cellules ecrites (code %d)' % (len(files), r.returncode))
    check(len(Fake.requests) == ncell + 1, 'telechargement : une seule requete en plus pour le 429 (%d au total)' % len(Fake.requests))
    body = urllib.parse.parse_qs(Fake.requests[-1][0])
    check('data' in body and 'out geom;' in body['data'][0], 'telechargement : requete envoyee en POST, champ data')
    check(all(ua and 'gps-velo' in ua for _, ua in Fake.requests), 'telechargement : User-Agent identifie')
    check(not any(f.endswith('.part') for f in files), 'telechargement : aucun fichier partiel laisse')
    before = len(Fake.requests)
    run(os.path.join(ROOT, 'tools', 'overpass_fetch.py'), '--bbox', '5.0,45.0,5.2,45.15', '--out-dir', out, '--cell-km', '10',
        '--server', url, '--pause', '0', '--backoff', '0')
    check(len(Fake.requests) == before, 'reprise : les cellules deja presentes ne sont pas retelechargees')

    out2 = os.path.join(tmp, 'extrait_gpx')
    Fake.requests.clear()
    r = run(os.path.join(ROOT, 'tools', 'overpass_fetch.py'), '--gpx', g1, '--margin-km', '1', '--out-dir', out2, '--cell-km', '50',
            '--server', url, '--pause', '0', '--backoff', '0')
    q = urllib.parse.parse_qs(Fake.requests[0][0])['data'][0] if Fake.requests else ''
    box = F.gpx_bbox([g1], 1.0)
    check(r.returncode == 0 and len(Fake.requests) == 1 and '(%.6f,%.6f,%.6f,%.6f)' % (box[1], box[0], box[3], box[2]) in q,
          'telechargement par GPX : une cellule, requete sur la zone du GPX plus la marge')

    # 4. conversion du JSON Overpass, doublons entre cellules fusionnes, resultat identique au GeoJSON
    g_json, g_geo = os.path.join(tmp, 'depuis_json.gvt'), os.path.join(tmp, 'depuis_geojson.gvt')
    r1 = run(os.path.join(ROOT, 'tools', 'osm_to_gvt.py'), *[os.path.join(out, f) for f in files], g_json)
    r2 = run(os.path.join(ROOT, 'tools', 'osm_to_gvt.py'), gj, g_geo)
    check(r1.returncode == 0 and r2.returncode == 0, 'conversion : JSON Overpass et GeoJSON convertis')
    check(filecmp.cmp(g_json, g_geo, shallow=False), 'conversion : %d fichiers identiques (doublons fusionnes) = meme carte que le GeoJSON' % len(files))
    r3 = run(os.path.join(HERE, 'verify_gvt.py'), gj, g_json)
    check(r3.returncode == 0 and 'REUSSIE' in r3.stdout, 'conversion : verification independante contre le GeoJSON d\'origine')

    # 5. lac en relation (membres outer), ways sans geometrie, signalement d'une remarque du serveur
    lake = [[5.40 + 0.01 * math.cos(a), 45.40 + 0.007 * math.sin(a)] for a in [2 * math.pi * i / 40 for i in range(41)]]
    rel = {'elements': [{'type': 'relation', 'id': 7, 'tags': {'type': 'multipolygon', 'natural': 'water'}, 'members': [
        {'type': 'way', 'ref': 1, 'role': 'outer', 'geometry': [{'lat': p[1], 'lon': p[0]} for p in lake[:21]]},
        {'type': 'way', 'ref': 2, 'role': 'outer', 'geometry': [{'lat': p[1], 'lon': p[0]} for p in lake[20:]]}]}]}
    pr = os.path.join(tmp, 'relation.json')
    json.dump(rel, open(pr, 'w'))
    g_rel = os.path.join(tmp, 'relation.gvt')
    r = run(os.path.join(ROOT, 'tools', 'osm_to_gvt.py'), pr, g_rel)
    tiles, zmin, zmax, areas = V.decode_file(g_rel) if r.returncode == 0 else ({}, 0, 0, {})
    check(r.returncode == 0 and any(cls == T.WATER for z in areas for cls, _ in areas[z]), 'relation : le lac (membres outer) devient une surface d eau')
    nogeom = os.path.join(tmp, 'sans_geometrie.json')
    json.dump({'elements': [{'type': 'way', 'id': 1, 'tags': {'highway': 'primary'}, 'nodes': [1, 2]}]}, open(nogeom, 'w'))
    r = run(os.path.join(ROOT, 'tools', 'osm_to_gvt.py'), nogeom, os.path.join(tmp, 'x.gvt'))
    check(r.returncode != 0 and 'out geom' in (r.stderr + r.stdout), 'ways sans geometrie : message qui explique "out geom"')
    Fake.mode = 'remark'
    r = run(os.path.join(ROOT, 'tools', 'overpass_fetch.py'), '--bbox', '5.0,45.0,5.05,45.05', '--out-dir', os.path.join(tmp, 'e2'),
            '--server', url, '--pause', '0', '--backoff', '0')
    check(r.returncode != 0 and 'timed out' in (r.stderr + r.stdout) and not os.listdir(os.path.join(tmp, 'e2')),
          'remarque du serveur : arret avec le message du serveur, rien d\'ecrit')
    Fake.mode = 'normal'
    r = run(os.path.join(ROOT, 'tools', 'overpass_fetch.py'), '--bbox', '4.0,44.0,6.5,46.0', '--out-dir', os.path.join(tmp, 'e3'), '--server', url)
    check(r.returncode != 0 and '--yes' in (r.stderr + r.stdout), 'grande zone : demande une confirmation (--yes) avant de lancer beaucoup de requetes')
    srv.shutdown()
    print('OK Overpass : requete, cellules, reprise, fusion, conversion' if not fails else '%d ECHEC(S)' % fails)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
