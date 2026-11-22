# Tests

Statut : brouillon. Dernière mise à jour : 21 novembre 2026, branche `tests/jeu-synthetique` (code au commit `02def03`), base `main` à `44f51bc`.

Ce document explique comment lancer les tests et consigne chaque rejeu avec sa date, son commit et son environnement (règle de `CLAUDE.md`). Le niveau de preuve de chaque fonctionnalité est dans `docs/ETAT_DU_PROJET.md`.

## Commandes

| Commande | Ce qu'elle lance | Fichiers personnels requis |
|---|---|---|
| `make test` | Alertes, guidage, format de `gps.csv`, moteur du simulateur, montées, noms de cols, trace, FIT, puis `verify_fit.py`, sur un jeu fictif généré | aucun (python3, `garmin-fit-sdk` et `fitdecode` requis) |
| `make cols-format-test` | Lecture du format de `gps.csv` | aucun |
| `make gvt-test` | Carte GVT1 (format, lecture, vue, remplissage, tracé de lignes) et conversion Overpass | aucun |
| `make map-test` | Ancien format de carte GVM1 | aucun (`tests/data/fixture_overpass.geojson` est versionné) |
| `make nommage` | Nommage des montées avec une vraie base de cols | `tests/data/sortie.gpx` et `tests/data/gps.csv` |
| `make ui` | Six scénarios d'aperçu de l'interface, télécharge LVGL 9.2.2 | `tests/data/replay2.csv`, généré depuis `tests/data/sortie.gpx` |
| `make sim GPX=...` | Simulateur interactif, SDL2 requis | un GPX avec heures (voir `docs/simulateur.md`) |
| `make clean` | Supprime `build/`, `tests/data/ride*.fit`, `tests/data/replay*.csv` et `tests/data/synthetique/` | |

`make ui` et `make sim` téléchargent des dépendances ou ouvrent une fenêtre : ne les lancer qu'avec l'accord de Nicolas (`CLAUDE.md`).

## Jeu de test fictif

`make test` génère `tests/data/synthetique/` avec `tests/make_synthetique.py` (résultat identique à chaque exécution, environ 4 Mo, ignoré par Git). Aucune donnée personnelle : la position est fictive (plein Atlantique, lat 10, lon -30).

- `synthetique.gpx` : 13 938 points à 1 Hz, 66 km, 13 937 s, avec heures et fréquence cardiaque.
- `cols.csv` : base fabriquée de 7 lignes de données, dont 6 entrées chargées (la ligne de type `eau` est comptée à part, selon `docs/cols.md`).

| Montée | Détectée (km) | Nom attendu | Ce que ça vérifie |
|---|---|---|---|
| 1 | 3,4 à 10,5 | Col des Essais | col accepté |
| 2 | 17,4 à 21,0 | Montée de Marnezac | lieu à environ 440 m du sommet |
| 3 | 26,4 à 36,0 | Col de la Pierre Fictive | col accepté avec 3 m d'écart d'altitude |
| 4 | 44,4 à 47,0 | (sans nom) | col à 200 m trop haut refusé, lieu à 5 km trop loin |
| 5 | 51,4 à 58,0 | Montée des Rochettes | lieu à environ 770 m du sommet |

Pourquoi ce jeu : les anciens tests s'appuyaient sur la sortie personnelle de référence de Nicolas (environ 103 km, au moins deux montées). `test_sim` exige une sortie de plus d'une heure avec deux montées et plus de 50 km, et `test_cols` attendait sept noms de montées propres à cette sortie (voir `docs/DECISIONS.md`, D15).

## Inventaire des tests

| Fichier de `tests/` | Module testé | Lancé par | Données utilisées |
|---|---|---|---|
| `test_alerts.c` | `core/alerts` | `make test` | aucune |
| `test_guidance.c` | `core/guidance` | `make test` | aucune |
| `test_cols_format.c` | `core/cols` (format de `gps.csv`) | `make test`, `make cols-format-test` | aucune |
| `test_climb.c` | `core/climb`, `core/route` | `make test` | jeu fictif |
| `test_cols.c` | `core/cols` (grammaire, nommage) | `make test` | jeu fictif (`synthetique.gpx`, `cols.csv`) |
| `test_route.c` | `core/route`, `core/matcher` | `make test` | jeu fictif (`replay.csv`) |
| `test_sim.c` | `sim/sim_engine` | `make test` | jeu fictif |
| `test_fit.c`, `verify_fit.py` | `core/fit` | `make test` | jeu fictif (`replay2.csv`, le test écrit `ride*.fit`) |
| `test_gvt.c`, `test_mapview.c`, `test_polyfill.c`, `test_linedraw.c`, `verify_gvt.py`, `test_overpass.py` | Carte GVT1, rendu, conversion | `make gvt-test` | données synthétiques de `make_gvt_fixture.py` |
| `test_map_data.c` | `core/map_data` (GVM1) | `make map-test` | `fixture_overpass.geojson` |
| `test_ble.c` | `core/ble_decode` | aucune cible Make | aucune |
| `name_real.c` | Nommage avec la vraie base | `make nommage` | `sortie.gpx`, `gps.csv` personnels |

Scripts de préparation : `make_synthetique.py` (jeu fictif), `make_replay.py` (génère `replay.csv` et `replay2.csv` depuis un GPX) et `make_gvt_fixture.py` (données de carte synthétiques). `make_gvt_demo.py` n'est référencé par aucune règle du Makefile.

`test_route` et `test_fit` n'ont aucune assertion en C : `test_route` affiche des mesures, `test_fit` écrit les fichiers que `verify_fit.py` vérifie. `test_climb` exige seulement au moins une montée.

## Résultats consignés

### `make test` complet sur le jeu fictif, 21 novembre 2026

- Environnement : bac à sable Linux de Claude, **pas la machine de Nicolas**. Ubuntu 24.04.5 LTS, gcc 13.3.0, Python 3.12.3, GNU Make 4.3, `garmin-fit-sdk` 21.222.0, `fitdecode` 0.11.0, options `-O2 -Wall -Wextra`.
- Commit : `02def03` (branche `tests/jeu-synthetique`), clone neuf, aucun fichier personnel. Après le test, `git status` est vide : les fichiers générés sont ignorés.

| Test | Résultat |
|---|---|
| `make test` | Code retour 0, 0 avertissement de compilation, 0 échec |
| Format de `gps.csv` | 13 vérifications OK |
| Alertes | 48 lignes OK, tous les tests réussis |
| Guidage | OK |
| Moteur du simulateur | 19 lignes OK. Position sur le parcours : écart moyen 1,3 m, maximal 5,3 m (seuils 8 m et 60 m). Montées suivante et précédente OK, aller à 50 km OK |
| Noms de cols | 14 lignes OK (8 de grammaire, 1 de lecture CSV, 5 montées). 5 montées détectées, 4 nommées, 1 sans nom comme attendu |
| Montées et trace | `test_climb` : 5 montées sur 66,0 km. `test_route` : exécuté, sans assertion |
| FIT | `ride.fit` : 13 839 enregistrements, 346 433 octets. Relu par le SDK Garmin (`is_fit` vrai, intégrité vraie, 0 erreur) et par `fitdecode` (CRC vérifié) : écarts maximaux nuls sur le temps, la distance (0,00 m) et la FC, 0,10 m sur l'altitude. Récupération après coupure : 6 989 points sur 7 000 écrits, 6 989 enregistrements relus |

### `make test` sur le Mac de Nicolas, 21 novembre 2026 (avant le jeu fictif)

- Environnement : MacBook Air de Nicolas, macOS 26.6.2 (25G83), Apple clang 21.0.0. Version de Python non relevée. `main` à `44f51bc`.
- Données : `sortie.gpx` personnel (3 466 points, 17,8 km, FC présente), `gps.csv`, pas de `cols_fixture.csv`.

| Test | Résultat |
|---|---|
| Format de `gps.csv` | 13 vérifications OK |
| Alertes | Tous les tests réussis |
| Guidage | OK |
| Moteur du simulateur | **5 échecs** : chargement (3 466 points, 3 465 s), montée suivante (position 0 m, départ de la montée 1 à 410 m), montée 2 inexistante, montée précédente, aller à 50 km. `make` s'arrête (code 1) |
| Montées, noms de cols, trace, FIT | **Non exécutés** : `make` s'est arrêté avant |

Cause des 5 échecs, lue dans `tests/test_sim.c` (lignes 29 à 59) : le test exige une sortie de plus de 3 600 s, au moins deux montées dont la première démarre après 500 m, et 50 km de parcours. La sortie de Nicolas dure 3 465 s et fait 17,8 km. Ce n'est pas un défaut du code. Non confirmé en rejouant la sortie de référence.

### Autres cibles, rejeu du 20 novembre 2026 (`b1744a1`, bac à sable Linux, mêmes versions d'outils)

| Test | Commande | Résultat |
|---|---|---|
| Carte GVT1, rendu, conversion Overpass | `make gvt-test` | Réussi (code retour 0), cinq verdicts de synthèse OK |
| Ancien format GVM1 | `make map-test` | Réussi (code retour 0) |
| Décodage BLE | Compilation à la main : `gcc -O2 -Wall -Wextra -Icore tests/test_ble.c core/ble_decode.c -lm` | Réussi, 35 lignes OK. Aucune règle du Makefile ne lance ce test |
| Nommage réel, interface, simulateur | `make nommage`, `make ui`, `make sim` | **Non lancés** : fichiers personnels, téléchargement de LVGL, SDL2 et accord de Nicolas requis |

### Rejeu sur la machine de Nicolas

| Date | Commit | Machine | `make test` | `make gvt-test` | `make map-test` | Remarques |
|---|---|---|---|---|---|---|
| 21 novembre 2026 | `44f51bc` | MacBook Air, macOS 26.6.2, Apple clang 21.0.0 | arrêté à `test_sim` (5 échecs, données) | à faire | à faire | avant le jeu fictif |
| à faire | | | | | | `make test` avec le jeu fictif, après fusion |

### Comment consigner un rejeu

Une ligne par rejeu : date, commit complet, machine ou bac à sable, versions du compilateur et de Python, commande, code retour, nombre de lignes OK et d'échecs. Ne rien conclure d'un test qui n'a pas pu s'exécuter.

## Constats du 21 novembre 2026

1. Les tests étaient calés sur la sortie de référence : sur la sortie de Nicolas, `test_sim` échouait cinq fois et `test_cols` exigeait un fichier absent. Corrigé par le jeu fictif (`docs/BUGS_CONNUS.md`, B03 et B08).
2. « Montée précédente » restait bloquée : `sim_seek_pos` se place sur le premier point à la distance demandée, jusqu'à un pas plus loin (5 à 7 m ici), et `sim_next_climb` ne tolérait que 1 m. Constaté avec un programme de contrôle jetable : la position restait à 25 907 m à chaque appui. Après correction : 43 940 m, 25 907 m, 16 946 m, 2 925 m puis 0 m. Défaut du code, présent avec n'importe quel GPX (B09, commit `e5263a7`).
3. `test_cols` plantait quand ses fichiers manquaient. Il affiche maintenant un message et rend le code 1 (B02).
4. `test_ble` réussit quand on le compile à la main, mais aucune règle du Makefile ne le lance (B04).
