# Architecture

Statut : mis à jour le 23 novembre 2026 pour le guidage OSM GCR1.

## Principe

Le code métier est écrit en C portable dans `core/`, sans dépendance, pour tourner à l'identique sur PC (tests, simulateur) et sur ESP32-S3 (ESP-IDF). L'interface LVGL 9 est dans `ui/`. Le simulateur reprend les données GPX dans `sim/`.

## Modules

| Module | Rôle |
|---|---|
| `core/route` | Lecture GPX en flux, rééchantillonnage à pas constant |
| `core/matcher` | Position GPS sur la trace, hors parcours, reprise après perte du signal |
| `core/climb` | Détection des montées, catégories, pente par sections |
| `core/cols` | Noms des montées d'après la base locale |
| `core/fit` | Enregistrement FIT avec récupération d'un fichier interrompu |
| `core/alerts` | Alertes avec délais, rappels, priorités, acquittement |
| `core/guidance` | Guidage de virage sans données OSM embarquées |
| `core/osm_guidance` | Lecture C des manoeuvres GCR1 |
| `core/gvt` | Format de carte en tuiles vectorielles GVT1 |
| `core/mapview` | Vue carte : tuiles, cache, orientation |
| `core/polyfill`, `core/linedraw` | Rendu de surfaces et lignes RGB565 |
| `core/map_data` | Ancien format de carte GVM1 |
| `core/ble_decode` | Décodage des mesures BLE |
| `ui/` | Application LVGL 9 |
| `sim/` | Simulateur SDL2 |
| `tools/gpx_to_osm_route.py` | Correspondance GPX vers un graphe OSM |
| `tools/route_to_gcr1.py` | Conversion GeoJSON vers GCR1 |
| `tools/osm_to_gvt.py` | Conversion OSM vers GVT1 |
| `tests/` | Tests C et scripts Python |

## Flux de données

```text
GPX importé -> core/route -> core/matcher <- position GPS
                               |
                 core/climb -> core/cols -> ui (montées)
                               |
                 core/alerts, core/guidance -> ui (virages)

Guidage : GPX + OSM
  -> gpx_to_osm_route.py
  -> route GPS-CYCLO-ROUTE-1
  -> route_to_gcr1.py
  -> GCR1
  -> core/osm_guidance

Carte : OSM -> overpass_fetch.py -> osm_to_gvt.py -> GVT1
  -> core/gvt -> core/mapview -> ui/simulateur

Mesures -> core/fit -> fichier FIT
```

## Formats de données

| Format | Fichiers | Statut |
|---|---|---|
| GVT1 | `core/gvt.*`, `tools/osm_to_gvt.py`, `docs/carte.md` | Actif dans l'interface et le simulateur |
| GCR1 | `core/osm_guidance.*`, `tools/gpx_to_osm_route.py`, `tools/route_to_gcr1.py`, `docs/GUIDAGE_OSM.md` | Implémenté et testé sur PC ; pas encore chargé par `ui/app.c` |
| GVM1 | `core/map_data.*`, `tools/osm_to_gvm.py` | Ancien, conservé provisoirement |
| `gps.csv` | `tools/convertir_cols.py`, `convertir_cols_pbf.py` | Base locale de cols |

## Compilation et tests

| Cible | Action |
|---|---|
| `make test` | Suite principale sur les données fictives |
| `make gcr1-test` | Lecture C GCR1, conversion route et map-matching GPX/OSM |
| `make cols-format-test` | Format des noms de cols |
| `make gvt-test` | Carte GVT1, rendu et conversion Overpass |
| `make map-test` | Ancien format GVM1 |
| `make sim GPX=... COLS=... CARTE=...` | Simulateur interactif |
| `make ui` | Aperçu de l'interface |

Détails : [GUIDAGE_OSM.md](GUIDAGE_OSM.md).
