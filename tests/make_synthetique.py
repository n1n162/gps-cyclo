#!/usr/bin/env python3
"""Genere un jeu de test synthetique : aucune donnee personnelle, resultat identique a chaque execution.
Usage (depuis tests/data/synthetique) : python3 ../../make_synthetique.py
  synthetique.gpx : sortie fictive de 66 km avec heures et frequence cardiaque, 5 montees
  cols.csv        : base de cols fabriquee pour tester le nommage des montees (voir tests/test_cols.c)
La position est fictive (plein Atlantique, lat 10, lon -30) : le parcours ne correspond a aucun lieu reel.
Les tests (test_sim, test_climb, test_route, test_fit, test_cols, verify_fit.py) s'appuient sur ce jeu,
pas sur la sortie personnelle de l'utilisateur."""
import math, datetime

LAT0, LON0 = 10.0, -30.0
START = datetime.datetime(2026, 1, 1, 8, 0, 0, tzinfo=datetime.timezone.utc)
M_PAR_DEGRE = 6371008.8 * math.pi / 180.0      # meme rayon terrestre que le code C

# Profil d'altitude : (kilometre, altitude en m). Cinq montees :
#   1 : km 4 a 10,5     (+350 m, 5,4 %)    sommet nomme par un col
#   2 : km 18 a 21      (+150 m, 5,0 %)    sommet nomme par un lieu
#   3 : km 27 a 36      (+540 m, 6,0 %)    sommet nomme par un col
#   4 : km 45 a 47      (+100 m, 5,0 %)    sans nom (le seul col proche est a la mauvaise altitude)
#   5 : km 52 a 58      (+360 m, 6,0 %)    sommet nomme par un lieu
NOEUDS = [(0, 200), (4, 205), (10.5, 555), (16, 260), (18, 262), (21, 412), (25, 215), (27, 220),
          (36, 760), (43, 330), (45, 333), (47, 433), (50, 380), (52, 382), (58, 742), (64, 300), (66, 298)]
SOMMETS_KM = [10.5, 21.0, 36.0, 47.0, 58.0]
L = int(NOEUDS[-1][0] * 1000)                   # longueur du parcours en m


def ele_lineaire(s):
    k = s / 1000.0
    for (k0, e0), (k1, e1) in zip(NOEUDS, NOEUDS[1:]):
        if k0 <= k <= k1:
            return e0 + (e1 - e0) * (k - k0) / (k1 - k0)
    return NOEUDS[-1][1]


# Altitude tous les metres, adoucie sur 301 m pour arrondir les sommets et les fonds de vallee
brut = [ele_lineaire(i) for i in range(L + 1)]
H = 150
ele = []
for i in range(L + 1):
    a, b = max(0, i - H), min(L, i + H)
    ele.append(sum(brut[a:b + 1]) / (b - a + 1))

# Trace : cap qui oscille doucement, sans jamais revenir sur elle-meme
lat, lon = [LAT0], [LON0]
for i in range(1, L + 1):
    cap = math.radians(55 + 30 * math.sin(2 * math.pi * 1.5 * i / L))
    lat.append(lat[-1] + math.cos(cap) / M_PAR_DEGRE)
    lon.append(lon[-1] + math.sin(cap) / (M_PAR_DEGRE * math.cos(math.radians(lat[-1]))))


def interp(tab, s):
    i = min(int(s), L - 1)
    f = s - i
    return tab[i] + (tab[i + 1] - tab[i]) * f


def pente_pct(s):
    i = min(int(s), L - 25)
    return (ele[i + 25] - ele[i]) / 25.0 * 100.0


def vitesse(g):
    return max(2.3, 8.0 - 0.95 * g) if g > 0 else min(13.5, 8.0 - 0.9 * g)


# Un point par seconde
pts = []
s, t = 0.0, 0
while s < L:
    g = pente_pct(s)
    fc = int(round(min(178, max(95, 118 + 4.5 * max(g, 0) + 8 * math.sin(t / 250.0)))))
    pts.append((interp(lat, s), interp(lon, s), interp(ele, s), t, fc))
    s += vitesse(g)
    t += 1
pts.append((lat[L], lon[L], ele[L], t, 120))

with open("synthetique.gpx", "w", encoding="utf-8") as f:
    f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
    f.write('<gpx version="1.1" creator="gps-cyclo tests/make_synthetique.py (donnees fictives)" '
            'xmlns="http://www.topografix.com/GPX/1/1" '
            'xmlns:gpxtpx="http://www.garmin.com/xmlschemas/TrackPointExtension/v1">\n')
    f.write('<metadata><name>Sortie synthetique (donnees fictives)</name></metadata>\n')
    f.write('<trk><name>Sortie synthetique</name><trkseg>\n')
    for la, lo, el, tt, fc in pts:
        h = (START + datetime.timedelta(seconds=tt)).strftime("%Y-%m-%dT%H:%M:%SZ")
        f.write('<trkpt lat="%.7f" lon="%.7f"><ele>%.1f</ele><time>%s</time>'
                '<extensions><gpxtpx:hr>%d</gpxtpx:hr></extensions></trkpt>\n' % (la, lo, el, h, fc))
    f.write('</trkseg></trk>\n</gpx>\n')


def sommet(km):
    s = km * 1000.0
    return interp(lat, s), interp(lon, s), NOEUDS[[k for k, _ in NOEUDS].index(km)][1]


# Base de cols fabriquee. Attendu par tests/test_cols.c, dans l'ordre des montees :
#   1 Col des Essais, 2 Montee de Marnezac, 3 Col de la Pierre Fictive, 4 (sans nom), 5 Montee des Rochettes
s1, s2, s3, s4, s5 = [sommet(k) for k in SOMMETS_KM]
lignes = [
    "type;nom;lat;lon;alt",
    "col;Col des Essais;%.6f;%.6f;%d" % (s1[0], s1[1], s1[2]),
    "lieu;Marnezac;%.6f;%.6f;" % (s2[0] + 0.004, s2[1]),                # a environ 440 m du sommet 2
    "col;Col de la Pierre Fictive;%.6f;%.6f;%d" % (s3[0], s3[1], s3[2] - 3),
    "col;Col Trop Haut;%.6f;%.6f;%d" % (s4[0], s4[1], s4[2] + 200),     # bonne position, 200 m trop haut : refuse
    "lieu;Lointain;%.6f;%.6f;" % (s4[0] + 0.045, s4[1]),                # a environ 5 km du sommet 4 : trop loin
    "lieu;Les Rochettes;%.6f;%.6f;" % (s5[0], s5[1] + 0.007),           # a environ 770 m du sommet 5
    "eau;Fontaine Fictive;%.6f;%.6f;" % (interp(lat, 20000.0), interp(lon, 20000.0)),   # autre type : ignore
]
with open("cols.csv", "w", encoding="utf-8") as f:
    f.write("\n".join(lignes) + "\n")

mont = sum(max(0.0, pts[i + 1][2] - pts[i][2]) for i in range(len(pts) - 1))
print("synthetique.gpx : %d points, %.1f km, %d s, D+ environ %.0f m ; cols.csv : %d lignes"
      % (len(pts), L / 1000.0, pts[-1][3], mont, len(lignes) - 1))
