# État du projet

Statut : brouillon. Référence : commit `ea7a114` de `main` (19 novembre 2026). L'analyse du code porte sur `1280ca9` ; les commits suivants ne modifient que des fichiers de documentation.

Dernière vérification : 19 novembre 2026, par Claude, en lisant GitHub (commits, branches, `.gitignore`, `Makefile`, listes de fichiers). Aucun test n'a été rejoué.

## Niveaux de preuve

- **Codé** : le code existe.
- **Test présent (non rejoué)** : un test existe et est branché dans une cible Make. Il n'a pas été exécuté lors de l'audit.
- **Rejoué** : test exécuté avec succès, résultat consigné avec date, commit et environnement.
- **Validé sur matériel** : vérifié sur la carte Waveshare.

À ce jour, aucune fonctionnalité n'est au niveau "Rejoué" ni "Validé sur matériel". L'ancien README affirmait que le code commun était "testé sur ordinateur" (octobre 2026) : cette affirmation est historique, elle n'a pas été confirmée par une exécution récente.

## Fonctionnalités

| Fonctionnalité | Module | Niveau actuel | Remarque |
|---|---|---|---|
| Lecture GPX en flux, pas de 10 m | `core/route` | Test présent | `make test` |
| Positionnement sur la trace, hors parcours | `core/matcher` | Test présent (indirect) | Pas de test dédié, appelé par `test_route` et `test_sim` |
| Détection des montées, catégories, pente | `core/climb` | Test présent | `make test` |
| Noms de montées | `core/cols` | Test présent | Recherche cassée avec le nouveau `gps.csv` |
| Enregistrement FIT et reprise | `core/fit` | Test présent | Vérification externe par `verify_fit.py` |
| Alertes | `core/alerts` | Test présent | `make test` |
| Guidage de virage | `core/guidance` | Test présent | Utilisé par `ui/app.c` |
| Carte GVT1 : format, lecture | `core/gvt` | Test présent | `make gvt-test`, hors `make test` |
| Rendu carte | `core/mapview`, `polyfill`, `linedraw` | Test présent | Utilisé par `ui/app.c`, `ui/map_preview.c`, `sim/sim_main.c` |
| Conversion OSM vers GVT1 | `tools/osm_to_gvt.py` | Test présent | Extrait de Grenoble converti par Nicolas sur son Mac, non rejoué |
| Ancien format GVM1 | `core/map_data` | Test présent | Seul appelant : `tests/test_map_data.c`. Aucun usage dans `ui/` ni `sim/` |
| Décodage BLE (FC, cadence, puissance) | `core/ble_decode` | Codé | `tests/test_ble.c` existe mais aucune règle du Makefile ne le compile (vérifié le 19 novembre 2026). Seul appelant : ce test. Aucun usage dans `ui/` ni `sim/` |
| Interface LVGL (pages, montée auto, champs, alertes, carte, thème, réglages) | `ui/` | Codé | Autotest du simulateur, non rejoué |
| Zones de fréquence cardiaque | `ui/app.c` | Codé (à clarifier) | Réglage des seuils présent dans l'interface. L'ancien README les classait "à faire" |
| Sauvegarde de la configuration | `ui/app.c` | Codé (à clarifier) | Encodage et décodage de la configuration présents. Lieu de stockage non vu |
| Simulateur interactif | `sim/` | Codé | SDL2 requis, non rejoué |
| Export Strava par API | `tools/strava_upload.py` | Codé | Choix retenu : export manuel du FIT |
| Écran de démarrage | `ui/splash.c`, `ui/splash.h`, `tools/splash_to_c.py` | Codé (branche `ecran-demarrage`) | Non fusionné dans `main`. Aucune modification du Makefile ni de `ui/app.c` : non branché. L'image source est `docs/ecran acceuil.jpg` |
| Projet ESP-IDF, GNSS, carte SD, BLE réel, son, vibration, carte sur matériel | n/a | Non développé | |

## Branches (19 novembre 2026)

| Branche | État |
|---|---|
| `main` | `ea7a114`. 13 commits. Contient la documentation de ce chantier |
| `ecran-demarrage` | Part de `1280ca9`. 1 commit (`b7df9ff`, 15 novembre) qui ajoute 3 fichiers et n'en modifie aucun. En retard de 5 commits de documentation sur `main`. Fusion sans conflit attendue (fichiers distincts) |
| `docs/donnees-et-gitignore` | Correction du `.gitignore` et mise à jour des documents. En attente de relecture |
| `qwen` | Absente de GitHub, donc locale ou supprimée. Non examinée. Le fichier `Modelfile` de `main` (`qwen2.5-coder:14b`) suggère un essai local avec Ollama |

## Problèmes connus

1. **Recherche des cols cassée** : depuis la modification de `gps.csv` du 29 octobre (conservation de l'id OSM via `recup_cols.py`), le simulateur ne trouve plus les cols. Cause non analysée. Le script `recup_cols.py` n'est pas dans le dépôt (`tools/` vérifié le 19 novembre 2026) : l'analyse est impossible depuis GitHub seul.
2. **Dépôt non reproductible** : `make test` exige `tests/data/sortie.gpx`. Un clone neuf ne peut pas lancer les tests sans fichier personnel.
3. **Tests dispersés** : `gvt-test` et `map-test` ne sont pas dans `make test`. Le Makefile ne contient aucune règle pour le test BLE.
4. **Code orphelin probable** : `core/ble_decode` et `core/map_data` ne sont appelés que par leurs tests.
5. **`ui/app.c` monolithique** : environ 2100 lignes et 121 fonctions pour au moins douze responsabilités (widgets, profil, flèches de virage, configuration, zones de FC, pages de données, rendu de carte, navigation, altitude, montées, alertes, réglages).
6. **Deux `lv_conf.h`** (`ui/` et `sim/`) : risque de divergence.
7. **Nom de fichier fautif** : `docs/ecran acceuil.jpg`.
8. **Aucun `requirements.txt`** pour les dépendances Python.

## Points non vérifiés

- Protection des données : le `.gitignore` a été lu le 19 novembre 2026 et ne protégeait pas les GPX, FIT et TCX (corrigé dans la branche `docs/donnees-et-gitignore`). L'historique de `main` est contrôlé sur 9 commits sur 13 (voir `docs/DECISIONS.md`, D04). Les images de `docs/captures/` et les fichiers `tests/data/ride*.fit` générés par `make test` n'ont pas été examinés.
- Contenu de la branche locale `qwen`.
- Contenu des documents de `docs/` antérieurs à ce chantier (`avancement.md`, `carte.md`, `carte_hors_ligne.md`, `carte_waveshare.md`, `cols.md`, `export_strava.md`, `simulateur.md`) : listés, non lus.
- Scripts `.sh` (`sim/build_sim.sh`, `ui/build_preview.sh`, `ui/build_map_preview.sh`) : non lus.
- Graphe des inclusions : incomplet dans l'index, les dépendances de `ui/` vers `core/` sont déduites des appels de fonctions.

## Mise à jour

Un niveau n'est relevé que sur preuve : test rejoué (commande, sortie, date, commit, environnement) ou essai sur matériel. Mettre à jour la date et le commit en tête de fichier.
