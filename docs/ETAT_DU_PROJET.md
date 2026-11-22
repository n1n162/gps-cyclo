# État du projet

Statut : brouillon. Référence : commit `ea7a114` de `main` (19 novembre 2026). L'analyse du code porte sur `1280ca9` ; les commits de `main` jusqu'à `44f51bc` ne modifient que de la documentation, le `.gitignore` et la suppression du `Modelfile`. La branche `tests/jeu-synthetique` modifie aussi `sim/sim_engine.c`, le `Makefile` et `tests/test_cols.c`.

Dernière vérification : 19 novembre 2026, par Claude, en lisant GitHub (commits, branches, `.gitignore`, `Makefile`, listes de fichiers). Aucun test n'a été rejoué.

Complément du 20 novembre 2026 (`main` à `83d008c`) : historique des trois branches distantes, captures de `docs/captures/` et fichiers `tests/data/ride*.fit` examinés (voir `docs/DECISIONS.md`, D04). Rejeu partiel dans un bac à sable Linux (`b1744a1`).

Complément du 21 novembre 2026 : `make test` complet sur un jeu de test fictif dans un bac à sable Linux (branche `tests/jeu-synthetique`, code au commit `02def03`), et `make test` partiel sur le Mac de Nicolas (`44f51bc`, arrêté à `test_sim` sur sa propre sortie). Voir `docs/TESTS.md`.

## Niveaux de preuve

- **Codé** : le code existe.
- **Test présent (non rejoué)** : un test existe et est branché dans une cible Make. Il n'a pas été exécuté lors de l'audit.
- **Rejoué** : test exécuté avec succès, résultat consigné avec date, commit et environnement.
- **Validé sur matériel** : vérifié sur la carte Waveshare.

À ce jour, des modules sont au niveau "Rejoué" : dans un bac à sable Linux (le jeu fictif pour `make test`), et pour trois d'entre eux aussi sur le Mac de Nicolas (21 novembre 2026). Aucune fonctionnalité n'est au niveau "Validé sur matériel". Le code de la branche `tests/jeu-synthetique` n'a pas encore été rejoué sur le Mac. L'ancien README affirmait que le code commun était "testé sur ordinateur" (octobre 2026) : cette affirmation est maintenant confirmée pour les modules ci-dessous, sauf indication contraire.

## Fonctionnalités

| Fonctionnalité | Module | Niveau actuel | Remarque |
|---|---|---|---|
| Lecture GPX en flux, pas de 10 m | `core/route` | Rejoué (jeu fictif, bac à sable Linux, 21 novembre 2026) | `make test` : 5 montées sur 66,0 km. `test_route` n'a aucune assertion (B11) |
| Positionnement sur la trace, hors parcours | `core/matcher` | Rejoué (jeu fictif, bac à sable Linux, 21 novembre 2026) | `test_sim` : écart moyen 1,3 m, maximal 5,3 m, panne "hors parcours" OK. Pas de test dédié |
| Détection des montées, catégories, pente | `core/climb` | Rejoué (jeu fictif, bac à sable Linux, 21 novembre 2026) | `test_cols` exige 5 montées détectées sur le jeu fictif |
| Noms de montées | `core/cols` | Rejoué (jeu fictif, bac à sable Linux, 21 novembre 2026). Format de `gps.csv` rejoué aussi sur le Mac | 4 montées nommées sur 5 comme attendu. Recherche cassée avec le nouveau `gps.csv` (B01), non reproduite |
| Enregistrement FIT et reprise | `core/fit` | Rejoué (jeu fictif, bac à sable Linux, 21 novembre 2026) | Relu par deux décodeurs indépendants, 0 erreur. Aucun essai avec un appareil réel |
| Alertes | `core/alerts` | Rejoué (Mac de Nicolas et bac à sable Linux, 21 novembre 2026) | 48 vérifications OK |
| Guidage de virage | `core/guidance` | Rejoué (Mac de Nicolas et bac à sable Linux, 21 novembre 2026) | `test_guidance` OK. Utilisé par `ui/app.c` |
| Carte GVT1 : format, lecture | `core/gvt` | Rejoué (bac à sable Linux, 20 novembre 2026) | `make gvt-test`, hors `make test` |
| Rendu carte | `core/mapview`, `polyfill`, `linedraw` | Rejoué (bac à sable Linux, 20 novembre 2026) | Dans `make gvt-test`. Utilisé par `ui/app.c`, `ui/map_preview.c`, `sim/sim_main.c` |
| Conversion OSM vers GVT1 | `tools/osm_to_gvt.py` | Rejoué sur données synthétiques (bac à sable Linux, 20 novembre 2026) | Extrait de Grenoble converti par Nicolas sur son Mac, non rejoué |
| Ancien format GVM1 | `core/map_data` | Rejoué (bac à sable Linux, 20 novembre 2026) | `make map-test`. Seul appelant : `tests/test_map_data.c`. Aucun usage dans `ui/` ni `sim/` |
| Décodage BLE (FC, cadence, puissance) | `core/ble_decode` | Codé | `tests/test_ble.c` existe mais aucune règle du Makefile ne le compile (vérifié le 19 novembre 2026). Seul appelant : ce test. Aucun usage dans `ui/` ni `sim/`. Test compilé et passé à la main le 20 novembre 2026 (35 lignes OK), hors Make |
| Interface LVGL (pages, montée auto, champs, alertes, carte, thème, réglages) | `ui/` | Codé | Autotest du simulateur, non rejoué |
| Zones de fréquence cardiaque | `ui/app.c` | Codé (à clarifier) | Réglage des seuils présent dans l'interface. L'ancien README les classait "à faire" |
| Sauvegarde de la configuration | `ui/app.c` | Codé (à clarifier) | Encodage et décodage de la configuration présents. Lieu de stockage non vu |
| Simulateur interactif | `sim/` | Moteur rejoué (jeu fictif, bac à sable Linux, 21 novembre 2026). Fenêtre : codé | 19 vérifications OK sur le moteur. Fenêtre SDL2 non lancée. Défaut de "montée précédente" corrigé (B09) |
| Export Strava par API | `tools/strava_upload.py` | Codé | Choix retenu : export manuel du FIT |
| Écran de démarrage | `ui/splash.c`, `ui/splash.h`, `tools/splash_to_c.py` | Codé (branche `ecran-demarrage`) | Non fusionné dans `main`. Aucune modification du Makefile ni de `ui/app.c` : non branché. L'image source est `docs/ecran acceuil.jpg` |
| Projet ESP-IDF, GNSS, carte SD, BLE réel, son, vibration, carte sur matériel | n/a | Non développé | |

## Branches (21 novembre 2026)

| Branche | État |
|---|---|
| `main` | `44f51bc`, 27 commits. Contient la PR #5 (TESTS, BUGS_CONNUS, HISTORIQUE) |
| `tests/jeu-synthetique` | Part de `44f51bc`. Code : `e5263a7` (simulateur, B09), `6329ee4` (jeu fictif, B02, B03, B08), `02def03` (`.gitignore`, B07). Documentation ensuite. En attente de revue |
| `ecran-demarrage` | Part de `1280ca9`. 1 commit (`b7df9ff`, 15 novembre) qui ajoute 3 fichiers (`tools/splash_to_c.py`, `ui/splash.c`, `ui/splash.h`) et n'en modifie aucun. En retard de 19 commits sur `main` au 21 novembre 2026. Fusion sans conflit attendue (fichiers distincts) |
| `docs/donnees-et-gitignore`, `docs/maj-documentation`, `docs/captures-verifiees`, `chore/retirer-modelfile`, `docs/suivi-tests-bugs-historique` | Branches fusionnées dans `main` (PR #1 à #5). Elles existent encore sur GitHub |
| `qwen` | Absente de GitHub. Abandonnée sur décision de Nicolas le 20 novembre 2026, jamais examinée (voir `docs/DECISIONS.md`, D14) |

## Problèmes connus

Suivi détaillé : `docs/BUGS_CONNUS.md`.

1. **Recherche des cols cassée** : depuis la modification de `gps.csv` du 29 octobre (conservation de l'id OSM via `recup_cols.py`), le simulateur ne trouve plus les cols. Cause non analysée. Le script `recup_cols.py` n'est pas dans le dépôt (`tools/` vérifié le 19 novembre 2026) : l'analyse est impossible depuis GitHub seul.
2. **`make ui` et `make nommage` exigent des fichiers personnels** (B10) : `tests/data/sortie.gpx` et `gps.csv`. `make test` n'en exige plus depuis le jeu fictif (21 novembre 2026).
3. **Tests dispersés** : `gvt-test` et `map-test` ne sont pas dans `make test`. Le Makefile ne contient aucune règle pour le test BLE (B04).
4. **Code orphelin probable** : `core/ble_decode` et `core/map_data` ne sont appelés que par leurs tests.
5. **`ui/app.c` monolithique** : environ 2100 lignes et 121 fonctions pour au moins douze responsabilités (widgets, profil, flèches de virage, configuration, zones de FC, pages de données, rendu de carte, navigation, altitude, montées, alertes, réglages).
6. **Deux `lv_conf.h`** (`ui/` et `sim/`) : risque de divergence.
7. **Nom de fichier fautif** : `docs/ecran acceuil.jpg` (B05).
8. **Aucun `requirements.txt`** pour les dépendances Python (B06).
9. **`test_route` et `test_fit` n'ont aucune assertion en C** (B11).

## Points non vérifiés

- Protection des données : `.gitignore` corrigé et fusionné (PR #1), étendu sur la branche `tests/jeu-synthetique` (B07). Historique des trois branches distantes contrôlé en entier le 20 novembre 2026 : 18 commits, aucun fichier de données. Depuis le jeu fictif, `make test` n'écrit plus de données personnelles (D15). Captures de `docs/captures/` : les 19 examinées une par une, conservées telles quelles par décision de Nicolas (D04).
- Documents de `docs/` antérieurs à ce chantier : `avancement.md` lu le 20 novembre 2026 (état au 13 octobre 2026, affirmations historiques rejouées en partie seulement) ; `cols.md` et `simulateur.md` lus le 21 novembre 2026 (une correction chacun) ; `carte.md`, `carte_hors_ligne.md`, `carte_waveshare.md`, `export_strava.md` listés, non lus.
- Tests non rejoués : `make nommage`, `make ui`, `make sim` (fenêtre SDL2), `make test` avec le jeu fictif sur le Mac de Nicolas, `make gvt-test` et `make map-test` sur le Mac. Les tests d'origine sur la sortie de référence d'environ 103 km n'ont pas été rejoués.
- Simulateur : l'arrêt avec "Sortie illisible" quand le GPX n'a pas d'heures est lu dans `sim/sim_main.c`, non lancé.
- Scripts `.sh` (`sim/build_sim.sh`, `ui/build_preview.sh`, `ui/build_map_preview.sh`) : non lus.
- Graphe des inclusions : incomplet dans l'index, les dépendances de `ui/` vers `core/` sont déduites des appels de fonctions.

## Mise à jour

Un niveau n'est relevé que sur preuve : test rejoué (commande, sortie, date, commit, environnement) ou essai sur matériel. Mettre à jour la date et le commit en tête de fichier.
