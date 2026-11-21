# État du projet

Statut : brouillon. Référence : commit `ea7a114` de `main` (19 novembre 2026). L'analyse du code porte sur `1280ca9` ; les commits suivants ne modifient que des fichiers de documentation et le `.gitignore`.

Dernière vérification : 19 novembre 2026, par Claude, en lisant GitHub (commits, branches, `.gitignore`, `Makefile`, listes de fichiers). Aucun test n'a été rejoué.

Complément du 20 novembre 2026 (`main` à `83d008c`) : historique des trois branches distantes, captures de `docs/captures/` et fichiers `tests/data/ride*.fit` examinés (voir `docs/DECISIONS.md`, D04). Aucun test n'a été rejoué à cette étape.

Rejeu partiel du 20 novembre 2026 (`main` à `b1744a1`) dans un bac à sable Linux de Claude, pas sur la machine de Nicolas : voir `docs/TESTS.md`. `make test` complet reste non rejoué (fichiers personnels absents).

## Niveaux de preuve

- **Codé** : le code existe.
- **Test présent (non rejoué)** : un test existe et est branché dans une cible Make. Il n'a pas été exécuté lors de l'audit.
- **Rejoué** : test exécuté avec succès, résultat consigné avec date, commit et environnement.
- **Validé sur matériel** : vérifié sur la carte Waveshare.

À ce jour, des modules sont au niveau "Rejoué" grâce à un rejeu partiel du 20 novembre 2026 dans un bac à sable Linux, pas sur la machine de Nicolas (voir `docs/TESTS.md`). Aucune fonctionnalité n'est au niveau "Validé sur matériel". L'ancien README affirmait que le code commun était "testé sur ordinateur" (octobre 2026) : cette affirmation n'est confirmée que partiellement.

## Fonctionnalités

| Fonctionnalité | Module | Niveau actuel | Remarque |
|---|---|---|---|
| Lecture GPX en flux, pas de 10 m | `core/route` | Test présent | `make test` |
| Positionnement sur la trace, hors parcours | `core/matcher` | Test présent (indirect) | Pas de test dédié, appelé par `test_route` et `test_sim` |
| Détection des montées, catégories, pente | `core/climb` | Test présent | `make test` |
| Noms de montées | `core/cols` | Format de `gps.csv` rejoué, nommage : test présent | `cols-format-test` rejoué le 20 novembre 2026. `test_cols` non concluant (fichiers personnels absents). Recherche cassée avec le nouveau `gps.csv` |
| Enregistrement FIT et reprise | `core/fit` | Test présent | Vérification externe par `verify_fit.py` |
| Alertes | `core/alerts` | Rejoué (bac à sable Linux, 20 novembre 2026) | `make test`. 48 vérifications OK, voir `docs/TESTS.md` |
| Guidage de virage | `core/guidance` | Rejoué (bac à sable Linux, 20 novembre 2026) | `test_guidance` OK. Utilisé par `ui/app.c` |
| Carte GVT1 : format, lecture | `core/gvt` | Rejoué (bac à sable Linux, 20 novembre 2026) | `make gvt-test`, hors `make test` |
| Rendu carte | `core/mapview`, `polyfill`, `linedraw` | Rejoué (bac à sable Linux, 20 novembre 2026) | Dans `make gvt-test`. Utilisé par `ui/app.c`, `ui/map_preview.c`, `sim/sim_main.c` |
| Conversion OSM vers GVT1 | `tools/osm_to_gvt.py` | Rejoué sur données synthétiques (bac à sable Linux, 20 novembre 2026) | Extrait de Grenoble converti par Nicolas sur son Mac, non rejoué |
| Ancien format GVM1 | `core/map_data` | Rejoué (bac à sable Linux, 20 novembre 2026) | `make map-test`. Seul appelant : `tests/test_map_data.c`. Aucun usage dans `ui/` ni `sim/` |
| Décodage BLE (FC, cadence, puissance) | `core/ble_decode` | Codé | `tests/test_ble.c` existe mais aucune règle du Makefile ne le compile (vérifié le 19 novembre 2026). Seul appelant : ce test. Aucun usage dans `ui/` ni `sim/`. Test compilé et passé à la main le 20 novembre 2026 (35 lignes OK), hors Make |
| Interface LVGL (pages, montée auto, champs, alertes, carte, thème, réglages) | `ui/` | Codé | Autotest du simulateur, non rejoué |
| Zones de fréquence cardiaque | `ui/app.c` | Codé (à clarifier) | Réglage des seuils présent dans l'interface. L'ancien README les classait "à faire" |
| Sauvegarde de la configuration | `ui/app.c` | Codé (à clarifier) | Encodage et décodage de la configuration présents. Lieu de stockage non vu |
| Simulateur interactif | `sim/` | Codé | SDL2 requis, non rejoué |
| Export Strava par API | `tools/strava_upload.py` | Codé | Choix retenu : export manuel du FIT |
| Écran de démarrage | `ui/splash.c`, `ui/splash.h`, `tools/splash_to_c.py` | Codé (branche `ecran-demarrage`) | Non fusionné dans `main`. Aucune modification du Makefile ni de `ui/app.c` : non branché. L'image source est `docs/ecran acceuil.jpg` |
| Projet ESP-IDF, GNSS, carte SD, BLE réel, son, vibration, carte sur matériel | n/a | Non développé | |

## Branches (20 novembre 2026)

| Branche | État |
|---|---|
| `main` | Audité à `83d008c`. Depuis : PR #2 et #3 (documentation) et PR #4 (retrait du `Modelfile` et documentation). Aucun autre fichier modifié |
| `ecran-demarrage` | Part de `1280ca9`. 1 commit (`b7df9ff`, 15 novembre) qui ajoute 3 fichiers (`tools/splash_to_c.py`, `ui/splash.c`, `ui/splash.h`) et n'en modifie aucun. En retard de 16 commits sur `main` au 20 novembre 2026. Fusion sans conflit attendue (fichiers distincts) |
| `docs/donnees-et-gitignore`, `docs/maj-documentation`, `docs/captures-verifiees`, `chore/retirer-modelfile` | Branches fusionnées dans `main` (PR #1 à #4). Elles existent encore sur GitHub |
| `qwen` | Absente de GitHub. Abandonnée sur décision de Nicolas le 20 novembre 2026, jamais examinée (voir `docs/DECISIONS.md`, D14) |

## Problèmes connus

Suivi détaillé : `docs/BUGS_CONNUS.md`.

1. **Recherche des cols cassée** : depuis la modification de `gps.csv` du 29 octobre (conservation de l'id OSM via `recup_cols.py`), le simulateur ne trouve plus les cols. Cause non analysée. Le script `recup_cols.py` n'est pas dans le dépôt (`tools/` vérifié le 19 novembre 2026) : l'analyse est impossible depuis GitHub seul.
2. **Dépôt non reproductible** : `make test` exige `tests/data/sortie.gpx` et, pour `test_cols`, `tests/data/cols_fixture.csv` (constaté le 20 novembre 2026). Un clone neuf ne peut pas lancer les tests sans fichiers personnels.
3. **Tests dispersés** : `gvt-test` et `map-test` ne sont pas dans `make test`. Le Makefile ne contient aucune règle pour le test BLE.
4. **Code orphelin probable** : `core/ble_decode` et `core/map_data` ne sont appelés que par leurs tests.
5. **`ui/app.c` monolithique** : environ 2100 lignes et 121 fonctions pour au moins douze responsabilités (widgets, profil, flèches de virage, configuration, zones de FC, pages de données, rendu de carte, navigation, altitude, montées, alertes, réglages).
6. **Deux `lv_conf.h`** (`ui/` et `sim/`) : risque de divergence.
7. **Nom de fichier fautif** : `docs/ecran acceuil.jpg`.
8. **Aucun `requirements.txt`** pour les dépendances Python.
9. **`test_cols` plante sans ses fichiers** : erreur de segmentation au lieu d'un message (rejeu du 20 novembre 2026, `docs/BUGS_CONNUS.md`, B02).

## Points non vérifiés

- Protection des données : `.gitignore` corrigé et fusionné (PR #1). Historique des trois branches distantes contrôlé en entier le 20 novembre 2026 : 18 commits, aucun fichier de données. Fichiers `tests/data/ride*.fit` : issus d'une vraie sortie, non suivis et ignorés par Git, `make test` non rejoué. Captures de `docs/captures/` : les 19 examinées une par une, conservées telles quelles par décision de Nicolas (D04).
- Contenu des documents de `docs/` antérieurs à ce chantier : `avancement.md` lu le 20 novembre 2026 (état au 13 octobre 2026, affirmations historiques rejouées en partie seulement) ; `carte.md`, `carte_hors_ligne.md`, `carte_waveshare.md`, `cols.md`, `export_strava.md`, `simulateur.md` listés, non lus (une ligne de `cols.md` consultée).
- Tests non rejoués : `test_climb`, `test_route`, `test_sim`, `test_fit`, `verify_fit.py`, `test_cols` (fichiers personnels absents), `make nommage`, `make ui`, `make sim` (voir `docs/TESTS.md`).
- Scripts `.sh` (`sim/build_sim.sh`, `ui/build_preview.sh`, `ui/build_map_preview.sh`) : non lus.
- Graphe des inclusions : incomplet dans l'index, les dépendances de `ui/` vers `core/` sont déduites des appels de fonctions.

## Mise à jour

Un niveau n'est relevé que sur preuve : test rejoué (commande, sortie, date, commit, environnement) ou essai sur matériel. Mettre à jour la date et le commit en tête de fichier.
