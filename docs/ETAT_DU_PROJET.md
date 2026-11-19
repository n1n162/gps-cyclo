# État du projet

Statut : brouillon. Référence : commit `1280ca9` de `main` (index du 14 novembre 2026).

Dernière vérification : à renseigner (date, commit, personne).

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
| Décodage BLE (FC, cadence, puissance) | `core/ble_decode` | Codé | Test présent dans `tests/test_ble.c`, non branché dans une cible Make connue. Seul appelant : ce test. Aucun usage dans `ui/` ni `sim/` |
| Interface LVGL (pages, montée auto, champs, alertes, carte, thème, réglages) | `ui/` | Codé | Autotest du simulateur, non rejoué |
| Zones de fréquence cardiaque | `ui/app.c` | Codé (à clarifier) | Réglage des seuils présent dans l'interface. L'ancien README les classait "à faire" |
| Sauvegarde de la configuration | `ui/app.c` | Codé (à clarifier) | Encodage et décodage de la configuration présents. Lieu de stockage non vu |
| Simulateur interactif | `sim/` | Codé | SDL2 requis, non rejoué |
| Export Strava par API | `tools/strava_upload.py` | Codé | Choix retenu : export manuel du FIT |
| Écran de démarrage | `docs/ecran acceuil.jpg` | Image ajoutée | Intégration à l'application à faire |
| Projet ESP-IDF, GNSS, carte SD, BLE réel, son, vibration, carte sur matériel | n/a | Non développé | |

## Problèmes connus

1. **Recherche des cols cassée** : depuis la modification de `gps.csv` du 29 octobre (conservation de l'id OSM via `recup_cols.py`), le simulateur ne trouve plus les cols. Cause non analysée.
2. **Dépôt non reproductible** : `make test` exige `tests/data/sortie.gpx`. Un clone neuf ne peut pas lancer les tests sans fichier personnel.
3. **Tests dispersés** : `gvt-test` et `map-test` ne sont pas dans `make test`. Le test BLE n'est dans aucune cible connue.
4. **Code orphelin probable** : `core/ble_decode` et `core/map_data` ne sont appelés que par leurs tests.
5. **`ui/app.c` monolithique** : environ 2100 lignes et 121 fonctions pour au moins douze responsabilités (widgets, profil, flèches de virage, configuration, zones de FC, pages de données, rendu de carte, navigation, altitude, montées, alertes, réglages).
6. **Deux `lv_conf.h`** (`ui/` et `sim/`) : risque de divergence.
7. **Nom de fichier fautif** : `docs/ecran acceuil.jpg`.
8. **Aucun `requirements.txt`** pour les dépendances Python.

## Points non vérifiés

- Protection des données : le `.gitignore` a été lu le 19 novembre 2026 et ne protégeait pas les GPX, FIT et TCX (corrigé dans la branche `docs/donnees-et-gitignore`). L'historique de `main` est contrôlé sur 9 commits sur 13 (voir `docs/DECISIONS.md`, D04). Les images de `docs/captures/` et les fichiers `tests/data/ride*.fit` générés par `make test` n'ont pas été examinés.
- Contenu des branches `ecran-demarrage` et `qwen`.
- Présence de `recup_cols.py` dans le dépôt.
- Lignes de règles du Makefile non lues (génération des données de rejeu, éventuelle règle du test BLE).
- Documents `docs/` autres que le README.
- Graphe des inclusions : incomplet dans l'index, les dépendances de `ui/` vers `core/` sont déduites des appels de fonctions.

## Mise à jour

Un niveau n'est relevé que sur preuve : test rejoué (commande, sortie, date, commit, environnement) ou essai sur matériel. Mettre à jour la date et le commit en tête de fichier.
