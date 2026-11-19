# Décisions techniques

Statut : brouillon. Les décisions ci-dessous viennent des déclarations de Nicolas (octobre et novembre 2026) et du README. Les dates marquées "à préciser" doivent être confirmées avec l'historique Git.

Format : une décision par entrée, jamais réécrite. Pour changer d'avis, ajouter une nouvelle entrée qui remplace l'ancienne.

## Décisions prises

### D01. Matériel du prototype : Waveshare ESP32-S3-Touch-LCD-3.5
- Date : octobre 2026 
- Décision : carte tactile 3,5 pouces 320 x 480 avec 8 Mo de PSRAM.
- Raison : écran tactile demandé, priorité au détail de la carte.

### D02. Code métier en C portable dans `core/`
- Date : octobre 2026 
- Décision : logique en C sans dépendance, testée sur PC, destinée à ESP-IDF. Interface en LVGL 9 (version 9.2.2), simulation sur PC avec SDL2.
- Raison : tester sans matériel.

### D03. Base de cols locale
- Date : octobre 2026 
- Décision : base locale issue d'OpenStreetMap (`gps.csv`), pas de requête OSM à chaque import de parcours.
- Nom d'une montée : nom du col, sinon nom du lieu au sommet.

### D04. Données personnelles hors du dépôt
- Date : octobre 2026 
- Décision : aucun GPX, FIT ni trace dans le dépôt.
- Raison : une sortie commence et finit souvent au domicile.
- Constat du 19 novembre 2026 : l'ancien README annonçait que `.gitignore` excluait `*.gpx` et `*.fit`. C'était inexact : le fichier ne contenait que `.DS_Store`, `venv/`, `*.pbf` et `*.csv`.
- Mise en oeuvre : `*.gpx`, `*.fit`, `*.tcx`, `build/` et `.env` ajoutés au `.gitignore` (branche `docs/donnees-et-gitignore`).
- Vérification de l'historique (19 novembre 2026, `main` à `ea7a114`) : aucun GPX, FIT, CSV ni jeton dans les 9 commits contrôlés en détail sur 13, dont le commit initial. `tests/data/sortie.gpx` et `tests/data/gps.csv` n'ont jamais été commités. `tests/data/` ne contient que `fixture_overpass.geojson`. Les 4 autres commits sont des modifications de fichiers de documentation, non détaillées.
- Risques restants : le `.gitignore` ne protège pas un fichier déjà suivi ni un dossier envoyé à la main par l'interface web ; les images de `docs/captures/` n'ont pas été examinées (elles peuvent montrer un parcours réel) ; l'origine des données des `tests/data/ride*.fit` générés par `make test` n'est pas vérifiée ; la branche locale `qwen` n'a pas été examinée.
- Règle : avant tout envoi par l'interface web, vérifier que le dossier ne contient ni GPX, FIT ni `gps.csv`.

### D05. Carte hors ligne : repartir de zéro
- Date : 23 octobre 2026
- Décision : abandon de la première approche (format GVM1 et lecteur en flux). Couverture visée : tout le département de l'Isère.

### D06. Format de carte GVT1
- Date : 24 octobre 2026
- Décision : tuiles vectorielles GVT1 (`core/gvt.*`, `tools/osm_to_gvt.py`, `docs/carte.md`). L'ancien format GVM1 est conservé provisoirement.
- Sujet ouvert : date de retrait de GVM1 (voir "Décisions à prendre").

### D07. Rendu de carte
- Dates : 25 et 26 octobre 2026
- Décisions : fond coloré selon la zone (ville, forêt) plutôt que noir ; carte mise en cache dans une image et redessinée seulement si nécessaire ; thème clair par défaut, choix clair/sombre dans les réglages.
- Orientation : cap en haut, plus un mode parcours libre sans trace.

### D08. Export Strava manuel
- Date : 14 novembre 2026
- Décision : export manuel du fichier FIT. Les fichiers d'export par API sont conservés dans le dépôt (`tools/strava_upload.py`, `docs/export_strava.md`).
- Règle : lire la documentation Strava avant tout code d'API.

### D09. Dépôt et outils
- Dates : 13 et 14 novembre 2026
- Décisions : dépôt de travail `gps-cyclo` ; Graphify branché pour l'indexation du code.

### D10. Écran de démarrage
- Date : 14 novembre 2026
- Décision : l'image `docs/ecran acceuil.jpg` devient l'écran de démarrage, aussi intégrée au simulateur. Le fichier sera renommé sans espace ni faute.
- Avancement au 19 novembre 2026 : le module LVGL (`ui/splash.c`, `ui/splash.h`) et le convertisseur d'image (`tools/splash_to_c.py`) existent sur la branche `ecran-demarrage`, non fusionnée. L'intégration au simulateur et à `ui/app.c` n'est pas faite.

### D11. Règles de travail de Claude
- Date : 19 novembre 2026
- Décision : périmètre minimal, aucune suppression sans validation, version fonctionnelle toujours conservée, test avant chaque étape suivante, aucune annonce sans preuve, aucun commit ni push sans accord. Texte de référence : `CLAUDE.md`.
- Raison : Claude réalise l'essentiel du développement, Nicolas ne code pas.

### D12. Organisation documentaire
- Date : 19 novembre 2026 (proposée, à valider)
- Décision : `README.md` et `CLAUDE.md` à la racine, les autres documents de référence dans `docs/`. Documents de suivi à ajouter ensuite : `TESTS.md`, `BUGS_CONNUS.md`, `HISTORIQUE.md`. Documentations techniques spécifiques (BLE, FIT, GPX, GVT1) créées seulement lorsque leur contenu le justifie.

## Décisions à prendre

| Sujet | Options | Éléments à réunir |
|---|---|---|
| Avenir de GVM1 | Archiver sous tag puis retirer, ou conserver | Seul appelant : `tests/test_map_data.c` (vérifié dans l'index) |
| Décodeur BLE (`core/ble_decode`) | Brancher son test dans une cible Make et l'intégrer à l'application, ou archiver | Appelé seulement par son test. Aucune règle du Makefile ne compile `tests/test_ble.c` (Makefile lu le 19 novembre 2026) |
| Structure de `ui/app.c` | Découper en plusieurs fichiers | Mesure de la taille réelle |
| Branche `ecran-demarrage` | Fusionner, continuer ou archiver sous tag | 1 commit (`b7df9ff`, 15 novembre), 3 fichiers ajoutés (`ui/splash.c`, `ui/splash.h`, `tools/splash_to_c.py`), aucun fichier modifié, non branché dans le Makefile ni dans `ui/app.c`. Fusion sans conflit attendue |
| Branche locale `qwen` | Pousser sur GitHub pour examen, ou abandonner | Absente de GitHub, état local inconnu. Le `Modelfile` de `main` (`qwen2.5-coder:14b`) suggère un essai avec Ollama. À décider : garder ou non le `Modelfile` |
| Jeu de test public | GPX synthétique versionné | Choix du parcours |
| Stratégie de nommage | Règle unique pour les fichiers et dossiers | n/a |
