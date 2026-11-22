# Architecture

Statut : brouillon établi à partir du commit `1280ca9`. Les appels entre fonctions ont été relevés dans l'index du dépôt. Le graphe des inclusions est incomplet : les dépendances de `ui/` vers `core/` sont déduites des appels de fonctions et restent à confirmer par lecture du code.

## Principe

Le code métier est écrit en C portable dans `core/`, sans dépendance, pour tourner à l'identique sur PC (tests, simulateur) et sur ESP32-S3 (ESP-IDF). L'interface LVGL 9 est dans `ui/`. Le simulateur `sim/` remplace le matériel par un rejeu de sortie.

## Modules

| Module | Rôle |
|---|---|
| `core/route` | Lecture GPX en flux, rééchantillonnage à pas constant de 10 m |
| `core/matcher` | Position GPS sur la trace, hors parcours, reprise après perte du signal |
| `core/climb` | Détection des montées, catégories, pente par sections |
| `core/cols` | Noms des montées d'après la base locale de cols et de lieux |
| `core/fit` | Enregistrement FIT avec récupération d'un fichier interrompu |
| `core/alerts` | Alertes avec délais, rappels, priorités, acquittement |
| `core/guidance` | Guidage de virage |
| `core/gvt` | Format de carte en tuiles vectorielles GVT1 |
| `core/mapview` | Vue carte : tuiles, cache, orientation |
| `core/polyfill`, `core/linedraw` | Remplissage de polygones et tracé de lignes en RGB565 |
| `core/map_data` | Ancien format de carte GVM1 |
| `core/ble_decode` | Décodage des mesures BLE (fréquence cardiaque, cadence, puissance) |
| `ui/` | Application LVGL 9, portrait 320 x 480, polices avec accents, aperçu PC |
| `sim/` | Simulateur SDL2 avec tableau de commandes et autotest par clics |
| `tools/` | Conversion OpenStreetMap, base de cols, export Strava |
| `tests/` | Tests C et scripts Python de vérification et de génération |

## Dépendances vérifiées

| Constat | Source |
|---|---|
| `core/ble_decode` n'a qu'un appelant : `tests/test_ble.c`. Aucun usage dans `ui/` ni `sim/`. | Index, commit `1280ca9` |
| `core/map_data` (GVM1) n'a qu'un appelant : `tests/test_map_data.c`. Aucun usage dans `ui/` ni `sim/`. | Index, commit `1280ca9` |
| Chaîne GVT1 : `core/gvt` est appelé par `core/mapview`, lui-même appelé par `ui/app.c` (`map_render`), `ui/map_preview.c`, `sim/sim_main.c` et `tests/test_mapview.c`. | Index, commit `1280ca9` |
| `ui/app.c` appelle `core/guidance`, `core/alerts`, `core/climb`, `core/mapview`, `core/polyfill` et `core/linedraw`. | Index, commit `1280ca9` |

Les deux premiers constats indiquent que le décodeur BLE et l'ancien format GVM1 ne sont pas intégrés à l'application. Ce sont des briques prêtes ou des restes, selon la décision à prendre (voir `docs/DECISIONS.md`).

## Organisation de `ui/app.c`

Un seul fichier d'environ 2100 lignes et 121 fonctions. Répartition approximative par plage de lignes, d'après les fonctions indexées :

| Lignes | Contenu |
|---|---|
| 29 à 170 | Couleurs, catégories, aides de création de widgets |
| 174 à 270 | Profil d'altitude des montées |
| 281 à 375 | Flèches de virage |
| 378 à 535 | Barre d'état, champs de données, configuration, zones de FC |
| 535 à 720 | Rappels tactiles, construction et disposition de la page de données |
| 716 à 830 | Rendu et mise à jour de la carte |
| 830 à 1470 | Pages de navigation, altitude, liste et détail des montées, résumé, rafraîchissement |
| 1480 à 1700 | Sélecteur de champs, alertes, réglages de zones de FC |
| 1700 à 2100 | Réglages, thème, accueil, initialisation, mise à jour |

Ce découpage sert de base à la réorganisation prévue dans `docs/ROADMAP.md`.

## Flux de données

    GPX importé -> core/route -> core/matcher <- position GPS
                                    |
                      core/climb -> core/cols (noms) -> ui (page montée)
                                    |
                      core/alerts, core/guidance -> ui (bannières, virages)

    Cartes : OpenStreetMap -> tools/overpass_fetch.py -> tools/osm_to_gvt.py -> fichier .gvt
             .gvt -> core/gvt -> core/mapview (+ polyfill, linedraw) -> ui (carte)

    Enregistrement : mesures -> core/fit -> fichier FIT (export manuel vers Strava)

## Formats de données

| Format | Fichiers | Statut |
|---|---|---|
| GVT1 | `core/gvt.*`, `tools/osm_to_gvt.py`, `docs/carte.md` | Actif, utilisé par l'interface et le simulateur. Cache de tuile de 65 536 octets, plus grande tuile mesurée 18 124 octets (extrait de Grenoble) |
| GVM1 | `core/map_data.*`, `tools/osm_to_gvm.py` | Ancien, conservé provisoirement. Utilisé seulement par son test |
| `gps.csv` | `tools/convertir_cols.py`, `convertir_cols_pbf.py` | Base de cols et lieux issue d'OpenStreetMap. Fichier personnel, jamais versionné |

## Compilation et tests

| Cible | Action | Prérequis |
|---|---|---|
| `make test` | alertes, guidage, simulation, montées, noms, trace, FIT, puis `verify_fit.py` | gcc, make, python3, `garmin-fit-sdk`, `fitdecode`. Aucun fichier personnel : jeu fictif généré par `tests/make_synthetique.py` |
| `make cols-format-test` | format des noms de cols | inclus dans `make test` |
| `make nommage` | noms avec vraie base | `tests/data/gps.csv` et `tests/data/sortie.gpx` |
| `make gvt-test` | format et conversion GVT1, rendu carte | python3 |
| `make map-test` | ancien format GVM1 | |
| `make sim GPX=... COLS=... CARTE=...` | simulateur interactif | SDL2, LVGL |
| `make ui` | aperçu de l'interface | télécharge LVGL 9.2.2, `tests/data/sortie.gpx` |

Le contenu de `core/`, les formats de carte, le simulateur et l'interface sont conçus pour être testés sans matériel. Ces tests ont été rejoués en partie (voir `docs/TESTS.md` et `docs/ETAT_DU_PROJET.md`).

## Dépendances externes

LVGL 9.2.2 (téléchargé par `make ui`), SDL2 (simulateur), `garmin-fit-sdk` et `fitdecode` (Python, vérification FIT). ESP-IDF pour le futur projet matériel, non présent dans le dépôt.

## Dette technique identifiée

Découpage de `ui/app.c`, fusion ou retrait de GVM1, unification des deux `lv_conf.h`, intégration continue. Voir ROADMAP.md.
