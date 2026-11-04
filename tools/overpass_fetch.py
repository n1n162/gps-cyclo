#!/usr/bin/env python3
"""Telecharge un extrait OpenStreetMap depuis l'API Overpass, en ligne de commande, par cellules.

Usage :
  python3 tools/overpass_fetch.py --bbox ouest,sud,est,nord --out-dir extrait [--cell-km 10]
  python3 tools/overpass_fetch.py --gpx parcours.gpx [autre.gpx ...] --out-dir extrait [--margin-km 3]
  python3 tools/osm_to_gvt.py extrait/*.json carte.gvt

Avec --gpx, la zone est celle des traces (points trkpt et rtept) agrandie de --margin-km de chaque cote : la carte couvre
ainsi exactement le parcours.

La zone est decoupee en cellules (10 km de cote par defaut) : une requete par cellule, une apres l'autre, avec une pause
entre deux requetes. Chaque cellule est ecrite dans out-dir/extrait_LIGNE_COLONNE.json ; les fichiers deja presents sont
gardes (relancer la commande reprend ou elle s'est arretee). Les objets qui touchent deux cellules apparaissent dans les
deux fichiers : osm_to_gvt.py les fusionne par identifiant.

La requete est celle de docs/carte.md (routes, chemins, cours d'eau, voies ferrees, et les surfaces : zones urbaines, forets,
champs, parcs, roche, plans d'eau), avec "out geom". --no-areas omet les surfaces (telechargement bien plus leger).
Bibliotheque standard uniquement.
"""
import argparse
import json
import math
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import osm_to_gvt as T  # noqa: E402

DEFAULT_SERVER = 'https://overpass-api.de/api/interpreter'
USER_AGENT = 'gps-velo-diy/1.0 (compteur GPS velo personnel)'
MANY_CELLS = 50


def build_query(south, west, north, east, timeout=180, areas=True):
    """Requete Overpass QL pour une cellule. L'ordre de la boite est sud, ouest, nord, est."""
    box = '(%.6f,%.6f,%.6f,%.6f)' % (south, west, north, east)
    rx = lambda table: '|'.join(sorted(table))
    clauses = ['  way[highway~"^(%s)$"]%s;' % (rx(T.HIGHWAY), box),
               '  way[waterway~"^(%s)$"]%s;' % (rx(T.WATERWAY), box),
               '  way[railway~"^(%s)$"]%s;' % (rx(T.RAILWAY), box)]
    if areas:
        for key, table in (('landuse', T.AREA_LANDUSE), ('natural', T.AREA_NATURAL), ('leisure', T.AREA_LEISURE)):
            for kind in ('way', 'relation'):          # les grandes surfaces (forets, lacs) sont souvent des relations
                clauses.append('  %s[%s~"^(%s)$"]%s;' % (kind, key, rx(table), box))
    return '[out:json][timeout:%d];\n(\n%s\n);\nout geom;\n' % (timeout, '\n'.join(clauses))


def gpx_bbox(paths, margin_km):
    """(ouest, sud, est, nord) des points de trace de un ou plusieurs fichiers GPX, agrandi de margin_km."""
    lats, lons = [], []
    for path in paths:
        try:
            root = ET.parse(path).getroot()
        except (ET.ParseError, OSError) as e:
            raise SystemExit('GPX illisible : %s (%s)' % (path, e))
        for el in root.iter():
            if el.tag.rsplit('}', 1)[-1] in ('trkpt', 'rtept'):
                try:
                    lats.append(float(el.attrib['lat']))
                    lons.append(float(el.attrib['lon']))
                except (KeyError, ValueError):
                    continue
    if not lats:
        raise SystemExit('aucun point de trace (trkpt ou rtept) dans : ' + ', '.join(paths))
    south, north, west, east = min(lats), max(lats), min(lons), max(lons)
    mid = math.radians((south + north) / 2.0)
    dlat = margin_km / 110.574
    dlon = margin_km / (111.320 * math.cos(mid))
    return (west - dlon, south - dlat, east + dlon, north + dlat)


def cells(bbox, cell_km):
    """Cellules (ligne, colonne, (sud, ouest, nord, est)) qui couvrent la boite sans trou ni recouvrement."""
    west, south, east, north = bbox
    mid = math.radians((south + north) / 2.0)
    dlat = cell_km / 110.574
    dlon = cell_km / (111.320 * math.cos(mid))
    rows = max(1, int(math.ceil((north - south) / dlat - 1e-9)))
    cols = max(1, int(math.ceil((east - west) / dlon - 1e-9)))
    out = []
    for r in range(rows):
        for c in range(cols):
            s = south + (north - south) * r / rows
            n = south + (north - south) * (r + 1) / rows
            w = west + (east - west) * c / cols
            e = west + (east - west) * (c + 1) / cols
            out.append((r, c, (s, w, n, e)))
    return out


def fetch(server, query, http_timeout, retries, backoff):
    """Envoie la requete. Reessaie sur 429 (trop de requetes), 504 (serveur charge) et erreurs reseau."""
    body = urllib.parse.urlencode({'data': query}).encode('utf-8')
    last = None
    for attempt in range(1, retries + 1):
        req = urllib.request.Request(server, data=body, headers={'User-Agent': USER_AGENT})
        try:
            with urllib.request.urlopen(req, timeout=http_timeout) as resp:
                raw = resp.read()
            data = json.loads(raw.decode('utf-8'))
            if not isinstance(data, dict) or 'elements' not in data:
                raise SystemExit('reponse inattendue du serveur (pas de liste "elements")')
            if data.get('remark'):
                raise SystemExit('le serveur signale : %s\nReduire --cell-km ou retirer les petits chemins de la requete.' % data['remark'])
            return raw, len(data['elements'])
        except urllib.error.HTTPError as e:
            if e.code not in (429, 504):
                raise SystemExit('erreur HTTP %d du serveur %s' % (e.code, server))
            last = 'HTTP %d' % e.code
        except (urllib.error.URLError, TimeoutError, ConnectionError, json.JSONDecodeError) as e:
            last = str(e)
        if attempt < retries:
            wait = backoff * attempt
            print('  echec (%s), nouvel essai dans %g s (%d/%d)' % (last, wait, attempt, retries - 1), flush=True)
            time.sleep(wait)
    raise SystemExit('abandon apres %d essais (%s)' % (retries, last))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--bbox', type=T.parse_bbox, help='ouest,sud,est,nord en degres decimaux')
    ap.add_argument('--gpx', nargs='+', metavar='FICHIER', help='au lieu de --bbox : la zone des traces de ces fichiers GPX')
    ap.add_argument('--margin-km', type=float, default=3.0, help='marge autour des traces GPX (defaut 3)')
    ap.add_argument('--out-dir', required=True)
    ap.add_argument('--cell-km', type=float, default=10.0, help='cote d\'une cellule (defaut 10)')
    ap.add_argument('--server', default=DEFAULT_SERVER)
    ap.add_argument('--pause', type=float, default=5.0, help='secondes entre deux requetes (defaut 5)')
    ap.add_argument('--retries', type=int, default=4, help='essais par cellule (defaut 4)')
    ap.add_argument('--backoff', type=float, default=30.0, help='attente de base entre essais, en secondes (defaut 30)')
    ap.add_argument('--timeout', type=float, default=240.0, help='delai HTTP en secondes (defaut 240)')
    ap.add_argument('--no-areas', action='store_true', help='ne telecharger que les lignes (pas de fond colore)')
    ap.add_argument('--force', action='store_true', help='retelecharger les cellules deja presentes')
    ap.add_argument('--yes', action='store_true', help='accepter plus de %d requetes' % MANY_CELLS)
    a = ap.parse_args()
    if a.cell_km <= 0:
        sys.exit('--cell-km doit etre positif')
    if (a.bbox is None) == (a.gpx is None):
        sys.exit('donner exactement une des options --bbox ou --gpx')
    if a.margin_km < 0:
        sys.exit('--margin-km ne peut pas etre negatif')
    if a.gpx:
        a.bbox = gpx_bbox(a.gpx, a.margin_km)
        print('zone des traces avec %g km de marge : --bbox %.4f,%.4f,%.4f,%.4f' % ((a.margin_km,) + a.bbox))
    grid = cells(a.bbox, a.cell_km)
    print('%d cellule(s) de %g km environ' % (len(grid), a.cell_km))
    if len(grid) > MANY_CELLS and not a.yes:
        sys.exit('Cela fait beaucoup de requetes sur un serveur public (%d). Pour une region entiere, un fichier .osm.pbf '
                 'telecharge d\'un seul bloc est plus adapte. Relancer avec --yes pour continuer quand meme.' % len(grid))
    os.makedirs(a.out_dir, exist_ok=True)
    done = 0
    for n, (r, c, (s, w, no, e)) in enumerate(grid, 1):
        path = os.path.join(a.out_dir, 'extrait_%02d_%02d.json' % (r, c))
        if os.path.exists(path) and os.path.getsize(path) > 0 and not a.force:
            print('[%d/%d] %s : deja present' % (n, len(grid), os.path.basename(path)))
            continue
        if done:
            time.sleep(a.pause)
        raw, count = fetch(a.server, build_query(s, w, no, e, areas=not a.no_areas), a.timeout, a.retries, a.backoff)
        tmp = path + '.part'
        with open(tmp, 'wb') as f:
            f.write(raw)
        os.replace(tmp, path)
        done += 1
        print('[%d/%d] %s : %d objets, %d octets' % (n, len(grid), os.path.basename(path), count, len(raw)), flush=True)
    print('Termine. Convertir : python3 tools/osm_to_gvt.py %s/*.json carte.gvt' % a.out_dir)
    print('Donnees © contributeurs OpenStreetMap (ODbL).')


if __name__ == '__main__':
    main()
