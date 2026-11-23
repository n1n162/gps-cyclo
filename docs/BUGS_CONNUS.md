# Bugs et défauts connus

Statut : brouillon. Dernière mise à jour : 22 novembre 2026, `main` à `65ad3c0`.

Colonne "Reproduit" : **oui** si une commande et sa sortie le prouvent, **non** si le défaut est seulement consigné. Les défauts de structure (`ui/app.c` monolithique, deux `lv_conf.h`, code probablement orphelin) restent dans `docs/ETAT_DU_PROJET.md`, section "Problèmes connus".

## Ouverts

| Id | Défaut | Reproduit | Preuve ou source | Remarque |
|---|---|---|---|---|
| B01 | Recherche des cols cassée : depuis la modification de `gps.csv` du 29 octobre 2026 (id OSM conservé via `recup_cols.py`), le simulateur ne trouve plus les cols | non | Consigné dans `docs/ETAT_DU_PROJET.md` le 19 novembre 2026. `gps.csv` indisponible ici | Cause non analysée. `recup_cols.py` n'est pas dans le dépôt, l'analyse est impossible depuis GitHub seul |
| B04 | Le test BLE n'est pas branché dans le Makefile | oui | `Makefile` lu le 19 novembre 2026. Compilé à la main le 20 novembre 2026, il réussit (35 lignes OK) | Le module `core/ble_decode` n'est appelé que par ce test |
| B05 | Nom de fichier fautif : `docs/ecran acceuil.jpg` (espace et faute) | oui | Liste des fichiers du dépôt | À renommer après vérification des références (la branche `ecran-demarrage` l'utilise comme image source) |
| B06 | Aucun `requirements.txt` | oui | Liste des fichiers du dépôt | Les scripts lancés par `gvt-test` et `map-test` n'importent que la bibliothèque standard Python (imports lus le 20 novembre 2026). `tools/strava_upload.py` et `tools/splash_to_c.py` n'ont pas été lus |
| B10 | `make ui` et `make nommage` exigent encore des fichiers personnels (`tests/data/sortie.gpx`, `tests/data/gps.csv`) | oui | `Makefile` : la règle de `sortie.gpx` s'arrête avec un message | Voulu pour `nommage` (vraie base), à revoir pour `ui` |
| B11 | `test_route` et `test_fit` n'ont aucune assertion en C | oui | Lecture de `tests/test_route.c` et `tests/test_fit.c` le 21 novembre 2026 | `test_fit` est vérifié par `verify_fit.py`. `test_route` affiche seulement des mesures |
| B12 | `make test` s'arrête sur une trace Python brute (`ModuleNotFoundError`) quand `garmin-fit-sdk` ou `fitdecode` manque | oui | `make test` sur le Mac de Nicolas, 22 novembre 2026 : `No module named 'garmin_fit_sdk'`, code retour 1. Après `pip install garmin-fit-sdk fitdecode`, `verify_fit.py` va jusqu'au bout | Prérequis documenté dans le README, non vérifié par le Makefile. Un message clair serait un changement de code, à valider |

## Corrigés

| Id | Défaut | Correction |
|---|---|---|
| C01 | `.gitignore` ne couvrait pas les traces GPX, FIT, TCX, le dossier `build/` ni `.env` | Commit `61191c4` (19 novembre 2026), fusionné par la PR #1 (`83d008c`). Historique contrôlé, aucun fichier de données jamais commité (voir `docs/DECISIONS.md`, D04) |
| B02 | `test_cols` plantait (erreur de segmentation, code retour 139) quand `sortie.gpx` ou `cols_fixture.csv` manquait | Commit `6329ee4` : fichiers passés en arguments, message et code 1 si un fichier manque. Reproduit le 20 novembre 2026, rejoué le 21 novembre 2026 |
| B03 | `make test` impossible sur un clone neuf (`sortie.gpx` et `cols_fixture.csv` requis, ce dernier ignoré par `*.csv` et jamais versionné) | Commit `6329ee4` : jeu fictif généré par `tests/make_synthetique.py`. `make test` rejoué sur un clone neuf dans le bac à sable (21 novembre 2026) et sur le Mac de Nicolas (22 novembre 2026), voir `docs/TESTS.md`. Reste B10 pour `ui` et `nommage` |
| B07 | `.gitignore` ne couvrait pas `__pycache__`, l'historique aider (`.aider*`) ni le clone local de LVGL (`sim/lvgl/`) | Commit `02def03`. Constaté avec `git status` après `make gvt-test` (20 novembre 2026) et sur le Mac de Nicolas (21 novembre 2026). Sur le Mac, `git status --short` est vide après `make test` (22 novembre 2026) |
| B08 | Les tests étaient calés sur la sortie de référence : sur une autre sortie, `test_sim` échouait cinq fois et `test_cols` attendait sept noms propres à cette sortie | Commit `6329ee4` (jeu fictif). Échecs constatés sur le Mac de Nicolas le 21 novembre 2026, cause lue dans `tests/test_sim.c` |
| B09 | « Montée précédente » du simulateur restait bloquée sur la même montée après quelques « montée suivante » | Commit `e5263a7`. Reproduit avec un programme de contrôle jetable (position figée à 25 907 m), rejoué après correction (43 940, 25 907, 16 946, 2 925, 0 m), par `test_sim` dans le bac à sable (21 novembre 2026) et sur le Mac (22 novembre 2026). Fusionné par la PR #6 (`65ad3c0`) |

## Règle d'entretien

Un défaut n'est marqué "oui" qu'avec une commande et sa sortie. Un défaut corrigé passe dans "Corrigés" avec le commit, il n'est jamais effacé.
