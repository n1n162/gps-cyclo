# Tests

Statut : brouillon. Dernière mise à jour : 24 novembre 2026, branche `feat/ble-make-test`.

Ce document explique comment lancer les tests et consigne chaque rejeu avec sa date, son commit et son environnement (règle de `CLAUDE.md`). Le niveau de preuve de chaque fonctionnalité est dans `docs/ETAT_DU_PROJET.md`.

## Commandes

| Commande | Ce qu'elle lance | Fichiers personnels requis |
|---|---|---|
| `make test` | Alertes, guidage, décodage BLE, format de `gps.csv`, moteur du simulateur, montées, noms de cols, trace, FIT, puis `verify_fit.py`, sur un jeu fictif généré | aucun |
| `make ble-test` | Décodage des trames BLE fréquence cardiaque, CSC et puissance | aucun |
| `make cols-format-test` | Lecture du format de `gps.csv` | aucun |
| `make gvt-test` | Carte GVT1 (format, lecture, vue, remplissage, tracé de lignes) et conversion Overpass | aucun |
| `make map-test` | Ancien format de carte GVM1 | aucun |
| `make nommage` | Nommage des montées avec une vraie base de cols | `tests/data/sortie.gpx` et `tests/data/gps.csv` |
| `make ui` | Six scénarios d'aperçu de l'interface | `tests/data/replay2.csv`, généré depuis `tests/data/sortie.gpx` |
| `make sim GPX=...` | Simulateur interactif, SDL2 requis | un GPX avec heures |
| `make clean` | Supprime les fichiers générés | aucun |

## Inventaire des tests

| Fichier de `tests/` | Module testé | Lancé par | Données utilisées |
|---|---|---|---|
| `test_ble.c` | `core/ble_decode` | `make ble-test`, `make test` | aucune |
| Les autres fichiers C | Autres modules | `make test` ou leurs cibles dédiées | selon les inventaires précédents |

## Constats

- `test_ble` est maintenant compilé et lancé par une cible Make dédiée.
- Le test reste indépendant du matériel : il vérifie uniquement le décodage des trames BLE standard.
