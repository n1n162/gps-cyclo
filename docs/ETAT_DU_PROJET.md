# État du projet

Dernière mise à jour : 23 novembre 2026.

## Niveaux de preuve

- **Codé** : le code existe.
- **Test présent (non rejoué)** : un test existe mais n'a pas été exécuté dans l'environnement actuel.
- **Rejoué** : test exécuté avec succès, avec commande, date, commit et environnement consignés.
- **Validé sur matériel** : vérifié sur la carte Waveshare.

## Fonctionnalités

| Fonctionnalité | Module | Niveau actuel | Remarque |
|---|---|---|---|
| Lecture GPX en flux | `core/route` | Rejoué | Jeu fictif sur Mac et Linux |
| Positionnement sur la trace | `core/matcher` | Rejoué | Jeu fictif sur Mac et Linux |
| Détection des montées | `core/climb` | Rejoué | Cinq montées détectées sur le jeu fictif |
| Noms de montées | `core/cols` | Rejoué | Quatre montées nommées sur cinq dans le jeu fictif |
| Enregistrement FIT | `core/fit` | Rejoué | Relu par Garmin SDK et `fitdecode` |
| Alertes | `core/alerts` | Rejoué | Suite passée sur Mac |
| Guidage de virage | `core/guidance` | Rejoué | Utilisé par `ui/app.c` |
| Carte GVT1 | `core/gvt` | Rejoué | `make gvt-test` sur bac à sable Linux |
| Rendu carte | `core/mapview`, `polyfill`, `linedraw` | Rejoué | Hors matériel |
| Conversion OSM vers GVT1 | `tools/osm_to_gvt.py` | Rejoué | Données synthétiques |
| Map-matching GPX vers OSM | `tools/gpx_to_osm_route.py` | Rejoué | Trois tests GPX/OSM passés sur Mac le 23 novembre 2026 |
| Conversion vers GCR1 | `tools/route_to_gcr1.py` | Rejoué | Test Python passé sur Mac |
| Lecture GCR1 | `core/osm_guidance` | Rejoué | Test C passé sur Mac |
| Guidage GCR1 dans l'interface | `ui/app.c` | Codé | Aucun chargement GCR1 ni mise à jour depuis la position GPS |
| Décodage BLE | `core/ble_decode` | Codé | Test présent, aucune cible Make |
| Interface LVGL | `ui/` | Codé | Aperçus PC ; matériel non validé |
| Simulateur interactif | `sim/` | Moteur rejoué | Fenêtre SDL2 non relancée sur toutes les machines |
| Projet ESP-IDF, GNSS, carte SD, BLE réel | n/a | Non développé | |

## Dernière vérification GCR1

- Date : 23 novembre 2026
- Machine : MacBook Air de Nicolas, macOS 26.6.2, Apple clang 21.0.0, Python 3.9.6
- Branche : `feat/true-guidance`
- `make gcr1-test` : réussi
- `make test` : réussi
- Résultat : le GPX peut être apparié à un graphe OSM, converti en GCR1 et lu par le lecteur C.
- Limite : aucune intégration à l'interface ou au matériel.

## Problèmes connus

- Le guidage GCR1 n'est pas chargé par l'application ni relié à la position GPS.
- `make ui` et `make nommage` exigent des fichiers personnels.
- `gvt-test`, `map-test` et `gcr1-test` restent distincts de certaines cibles historiques.
- `core/ble_decode` n'est pas lancé par une cible Make.
- `ui/app.c` reste monolithique.
- Deux `lv_conf.h` peuvent diverger.
- Aucun `requirements.txt` Python n'est présent.
- Le test FIT peut manquer de dépendances Python locales.

## Mise à jour

N'élever un niveau qu'après une preuve : test rejoué avec date, commit et environnement, ou essai sur matériel.
