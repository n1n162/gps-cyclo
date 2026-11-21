# Tests

Statut : brouillon. Dernière mise à jour : 20 novembre 2026, `main` à `b1744a1`.

Ce document explique comment lancer les tests et consigne chaque rejeu avec sa date, son commit et son environnement (règle de `CLAUDE.md`). Le niveau de preuve de chaque fonctionnalité est dans `docs/ETAT_DU_PROJET.md`.

## Commandes

| Commande | Ce qu'elle lance | Fichiers personnels requis |
|---|---|---|
| `make test` | Alertes, guidage, lecture du format de `gps.csv`, moteur du simulateur, montées, noms de cols, trace, FIT, puis `verify_fit.py` | `tests/data/sortie.gpx` pour tout ; `tests/data/cols_fixture.csv` en plus pour `test_cols` |
| `make cols-format-test` | Lecture du format de `gps.csv` | aucun |
| `make gvt-test` | Carte GVT1 (format, lecture, vue, remplissage, tracé de lignes) et conversion Overpass | aucun |
| `make map-test` | Ancien format de carte GVM1 | aucun (`tests/data/fixture_overpass.geojson` est versionné) |
| `make nommage` | Nommage des montées avec une vraie base de cols | `sortie.gpx` et `gps.csv` |
| `make ui` | Six scénarios d'aperçu de l'interface, télécharge LVGL 9.2.2 | `replay2.csv`, généré depuis `sortie.gpx` |
| `make sim GPX=...` | Simulateur interactif, SDL2 requis | un fichier GPX |
| `make clean` | Supprime `build/`, `tests/data/ride*.fit` et `tests/data/replay*.csv` | |

`make ui` et `make sim` téléchargent des dépendances ou ouvrent une fenêtre : ne les lancer qu'avec l'accord de Nicolas (`CLAUDE.md`).

## Inventaire des tests

| Fichier de `tests/` | Module testé | Lancé par | Données nécessaires |
|---|---|---|---|
| `test_alerts.c` | `core/alerts` | `make test` | aucune |
| `test_guidance.c` | `core/guidance` | `make test` | aucune |
| `test_cols_format.c` | `core/cols` (format de `gps.csv`) | `make test`, `make cols-format-test` | aucune |
| `test_climb.c` | `core/climb`, `core/route` | `make test` | `sortie.gpx` |
| `test_cols.c` | `core/cols` (grammaire, nommage) | `make test` | `sortie.gpx`, `cols_fixture.csv` |
| `test_route.c` | `core/route`, `core/matcher` | `make test` | `sortie.gpx`, `replay.csv` |
| `test_sim.c` | `sim/sim_engine` | `make test` | `sortie.gpx` |
| `test_fit.c`, `verify_fit.py` | `core/fit` | `make test` | `replay2.csv` (le test écrit `ride*.fit`) |
| `test_gvt.c`, `test_mapview.c`, `test_polyfill.c`, `test_linedraw.c`, `verify_gvt.py`, `test_overpass.py` | Carte GVT1, rendu, conversion | `make gvt-test` | aucune (données synthétiques de `make_gvt_fixture.py`) |
| `test_map_data.c` | `core/map_data` (GVM1) | `make map-test` | aucune |
| `test_ble.c` | `core/ble_decode` | aucune cible Make | aucune |
| `name_real.c` | Nommage avec la vraie base | `make nommage` | `sortie.gpx`, `gps.csv` |

Scripts de préparation : `make_replay.py` (génère `replay.csv` et `replay2.csv` depuis `sortie.gpx`) et `make_gvt_fixture.py` (génère les données synthétiques). `make_gvt_demo.py` n'est référencé par aucune règle du Makefile.

## Résultats consignés

### Rejeu partiel du 20 novembre 2026

- Environnement : bac à sable Linux de Claude, **pas la machine de Nicolas**. Ubuntu 24.04.5 LTS, gcc 13.3.0, Python 3.12.3, GNU Make 4.3, options `-O2 -Wall -Wextra`, aucun avertissement de compilation.
- Commit : `b1744a1` (`b1744a103750dd631dbb133afaf76a8324c64c78`), clone neuf, aucun fichier personnel.
- Aucun fichier suivi par Git n'a été modifié. Le rejeu a seulement créé `build/` (ignoré par Git) et des dossiers `__pycache__` (non ignorés, voir `docs/BUGS_CONNUS.md`, B07).

| Test | Commande | Résultat |
|---|---|---|
| Format de `gps.csv` | `make cols-format-test` | Réussi (code retour 0) |
| Carte GVT1, rendu, conversion Overpass | `make gvt-test` | Réussi (code retour 0), cinq verdicts de synthèse OK |
| Ancien format GVM1 | `make map-test` | Réussi (code retour 0) |
| Alertes | `make build/test_alerts` puis `build/test_alerts` | Réussi, 48 lignes OK, aucun échec. Ce nombre correspond aux 48 vérifications annoncées dans `docs/avancement.md` |
| Guidage | `make build/test_guidance` puis `build/test_guidance` | Réussi |
| Décodage BLE | Compilation à la main, hors Make : `gcc -O2 -Wall -Wextra -Icore tests/test_ble.c core/ble_decode.c -lm` | Réussi, 35 lignes OK. Aucune règle du Makefile ne lance ce test |
| Noms de cols | `make build/test_cols` puis lancement dans `tests/data` | **Non concluant** : plante (code retour 139, erreur de segmentation) car `sortie.gpx` et `cols_fixture.csv` sont absents. Aucune conclusion sur le code testé |
| Montées, trace, simulateur, FIT | `test_climb`, `test_route`, `test_sim`, `test_fit`, `verify_fit.py` | **Non rejoués** : `sortie.gpx` absent |
| Nommage réel | `make nommage` | **Non rejoué** : `sortie.gpx` et `gps.csv` absents |
| Interface et simulateur | `make ui`, `make sim` | **Non lancés** : téléchargement de LVGL, SDL2 et accord de Nicolas requis |
| Ensemble | `make test` | **Non rejoué** : fichiers personnels absents |

### Rejeu sur la machine de Nicolas

| Date | Commit | Machine | `make test` | `make gvt-test` | `make map-test` | Remarques |
|---|---|---|---|---|---|---|
| à faire | | | | | | Phase 0 de `docs/ROADMAP.md` |

### Comment consigner un rejeu

Une ligne par rejeu : date, commit complet, machine ou bac à sable, versions du compilateur et de Python, commande, code retour, nombre de lignes OK et d'échecs. Ne rien conclure d'un test qui n'a pas pu s'exécuter.

## Constats du rejeu du 20 novembre 2026

1. `test_cols` exige un second fichier personnel, `tests/data/cols_fixture.csv`. Il n'est mentionné que dans `docs/cols.md`, il est ignoré par `*.csv` et il n'a jamais été versionné (aucun commit ne l'a ajouté).
2. `test_cols` ouvre `sortie.gpx` et `cols_fixture.csv` sans contrôler l'échec de l'ouverture (lignes 39 et 46 de `tests/test_cols.c`). Le plantage vient très probablement de là. Cause déduite de la lecture du code, non confirmée par un débogueur (absent du bac à sable).
3. `test_cols` attend sept noms de montées précis (tableau `want`, ligne 52), qui correspondent à la sortie de Nicolas. Le test ne peut pas servir avec d'autres données.
4. `test_ble` réussit quand on le compile à la main, mais aucune règle du Makefile ne le lance.

Suivi dans `docs/BUGS_CONNUS.md` (B02, B03, B04).
