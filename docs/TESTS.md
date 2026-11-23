# Tests

Statut : mise à jour le 23 novembre 2026. Les résultats sont consignés avec la date, le commit, la machine et la commande.

## Commandes

| Commande | Ce qu'elle lance | Données personnelles |
|---|---|---|
| `make test` | Alertes, guidage, format des cols, simulateur, montées, trace et FIT | aucune |
| `make gcr1-test` | Lecteur C GCR1, conversion route vers GCR1, GPX vers OSM | aucune |
| `make cols-format-test` | Lecture du format `gps.csv` | aucune |
| `make gvt-test` | Carte GVT1, rendu et conversion Overpass | aucune |
| `make map-test` | Ancien format GVM1 | aucune |
| `make nommage` | Nommage avec une vraie base | `sortie.gpx`, `gps.csv` |
| `make ui` | Aperçu LVGL | `sortie.gpx` |
| `make sim GPX=...` | Simulateur interactif | GPX avec heures |
| `make clean` | Supprime les fichiers générés | aucune |

## Jeu fictif

`make test` génère `tests/data/synthetique/` : 66 km, 13 938 points à 1 Hz, cinq montées et une base de cols synthétique. Aucun fichier personnel n'est nécessaire.

## Inventaire GCR1

| Fichier | Module testé | Cible |
|---|---|---|
| `tests/test_osm_guidance.c` | `core/osm_guidance` | `make gcr1-test` |
| `tests/test_route_to_gcr1.py` | `tools/route_to_gcr1.py` | `make gcr1-test` |
| `tests/test_gpx_to_osm_route.py` | `tools/gpx_to_osm_route.py` | `make gcr1-test` |

Voir [GUIDAGE_OSM.md](GUIDAGE_OSM.md) pour le format GCR1 et son pipeline.

## Rejeu GCR1 sur le Mac de Nicolas

- Date : 23 novembre 2026
- Machine : MacBook Air de Nicolas, macOS 26.6.2, Apple clang 21.0.0, Python 3.9.6
- Branche : `feat/true-guidance`
- Commit initial : `876910847fe139e5cf8cec9c9957f9a20dbc67a1` pour les premiers tests
- Correctif de map-matching : `c9f5f9d37cb2010b9b6815a9ef95f64264586770`
- Documentation : `9402bf15ebee86aa9690f575313de1e56bab15f2` puis mise à jour suivante

| Commande | Résultat |
|---|---|
| `make gcr1-test` | Réussi : lecteur C OK, route vers GCR1 OK, trois tests GPX/OSM OK |
| `make test` | Réussi : suite complète du jeu fictif OK |

Ces rejoues portent sur le code des outils et du lecteur C. Ils ne constituent pas une validation sur l'ESP32-S3.

## Inventaire principal

| Fichier | Module | Cible |
|---|---|---|
| `test_alerts.c` | `core/alerts` | `make test` |
| `test_guidance.c` | `core/guidance` | `make test` |
| `test_cols_format.c` | `core/cols` | `make test` |
| `test_climb.c`, `test_cols.c`, `test_route.c` | Montées, cols, trace | `make test` |
| `test_sim.c` | Simulateur | `make test` |
| `test_fit.c`, `verify_fit.py` | FIT | `make test` |
| `test_gvt.c`, `test_mapview.c`, `test_polyfill.c`, `test_linedraw.c` | Carte | `make gvt-test` |
| `test_map_data.c` | Ancien GVM1 | `make map-test` |
| `test_ble.c` | BLE | aucune cible Make |

## Consigner un rejeu

Une entrée doit contenir : date, commit complet, machine ou bac à sable, versions du compilateur et de Python, commande, code retour, nombre d'échecs et éventuels écarts connus.
