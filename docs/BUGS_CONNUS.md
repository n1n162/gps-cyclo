# Bugs et défauts connus

Statut : brouillon. Dernière mise à jour : 20 novembre 2026, `main` à `b1744a1`.

Colonne "Reproduit" : **oui** si une commande et sa sortie le prouvent, **non** si le défaut est seulement consigné. Les défauts de structure (`ui/app.c` monolithique, deux `lv_conf.h`, code probablement orphelin) restent dans `docs/ETAT_DU_PROJET.md`, section "Problèmes connus".

## Ouverts

| Id | Défaut | Reproduit | Preuve ou source | Remarque |
|---|---|---|---|---|
| B01 | Recherche des cols cassée : depuis la modification de `gps.csv` du 29 octobre 2026 (id OSM conservé via `recup_cols.py`), le simulateur ne trouve plus les cols | non | Consigné dans `docs/ETAT_DU_PROJET.md` le 19 novembre 2026. `gps.csv` indisponible ici | Cause non analysée. `recup_cols.py` n'est pas dans le dépôt, l'analyse est impossible depuis GitHub seul |
| B02 | `test_cols` plante au lieu d'afficher un message quand `sortie.gpx` ou `cols_fixture.csv` manque | oui | Rejeu du 20 novembre 2026 : erreur de segmentation, code retour 139. Voir `docs/TESTS.md` | Cause déduite : `fopen` non contrôlé, lignes 39 et 46 de `tests/test_cols.c`. Non confirmée par débogueur |
| B03 | `make test` est impossible sur un clone neuf | oui | `Makefile` : la règle de `sortie.gpx` s'arrête avec un message. Rejeu du 20 novembre 2026 : `cols_fixture.csv` est aussi nécessaire et aucune règle ni message ne le signale | `cols_fixture.csv` est ignoré par `*.csv` et jamais versionné. Prévu en phase 0 : GPX et fixture synthétiques versionnés |
| B04 | Le test BLE n'est pas branché dans le Makefile | oui | `Makefile` lu le 19 novembre 2026. Compilé à la main le 20 novembre 2026, il réussit (35 lignes OK) | Le module `core/ble_decode` n'est appelé que par ce test |
| B05 | Nom de fichier fautif : `docs/ecran acceuil.jpg` (espace et faute) | oui | Liste des fichiers du dépôt | À renommer après vérification des références (la branche `ecran-demarrage` l'utilise comme image source) |
| B06 | Aucun `requirements.txt` | oui | Liste des fichiers du dépôt | Les scripts lancés par `gvt-test` et `map-test` n'importent que la bibliothèque standard Python (imports lus le 20 novembre 2026). `tools/strava_upload.py` et `tools/splash_to_c.py` n'ont pas été lus |
| B07 | `.gitignore` ne couvre pas `__pycache__` | oui | Après `make gvt-test` le 20 novembre 2026, `git status` liste `tests/__pycache__/` et `tools/__pycache__/` comme fichiers non suivis | Fichiers générés sans valeur, à ne pas envoyer. Ajouter `__pycache__/` au `.gitignore` |

## Corrigés

| Id | Défaut | Correction |
|---|---|---|
| C01 | `.gitignore` ne couvrait pas les traces GPX, FIT, TCX, le dossier `build/` ni `.env` | Commit `61191c4` (19 novembre 2026), fusionné par la PR #1 (`83d008c`). Historique contrôlé, aucun fichier de données jamais commité (voir `docs/DECISIONS.md`, D04) |

## Règle d'entretien

Un défaut n'est marqué "oui" qu'avec une commande et sa sortie. Un défaut corrigé passe dans "Corrigés" avec le commit, il n'est jamais effacé.
