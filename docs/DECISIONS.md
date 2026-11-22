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
- Vérification de l'historique (20 novembre 2026, `main` à `83d008c`) : les 18 commits des trois branches distantes (`main`, `ecran-demarrage`, `docs/donnees-et-gitignore`) ont été contrôlés par la liste des fichiers touchés. Aucun fichier `.fit`, `.gpx`, `.tcx`, `.csv` ni `.env` n'a jamais été ajouté. Cela remplace le contrôle partiel du 19 novembre (9 commits sur 13). `tests/data/` ne contient que `fixture_overpass.geojson`.
- Risques restants : le `.gitignore` ne protège pas un fichier déjà suivi ni un dossier envoyé à la main par l'interface web ; la branche locale `qwen` n'a jamais été examinée et reste hors dépôt (D14).
- Captures `docs/captures/` (les 19 ouvertes et examinées une par une le 20 novembre 2026) : les deux cartes de démonstration montrent une ville fictive à grille de rues. 7 captures affichent les noms de montées d'une vraie sortie (`trace`, `liste_montees`, `simulateur`, `alerte_montee_proche`, `fin_de_montee`, `montee`, `montee_auto`), ce qui rend la zone de roulage reconnaissable à l'échelle d'une région. Les 12 autres n'ont aucun nom de lieu. Aucune des 19 ne montre de nom de rue, de coordonnées, d'adresse, de nom de personne ni de date (seulement une heure), et les PNG n'ont aucune métadonnée (EXIF). Elles affichent des mesures de sortie : durée, distance, altitude, fréquence cardiaque, cadence, puissance. Dépôt public. Décision de Nicolas le 20 novembre 2026 : captures conservées telles quelles.
- Fichiers `tests/data/ride.fit`, `ride_crash.fit` et `ride_recovered.fit` (vérifié le 20 novembre 2026 par lecture du code) : `make test` les écrivait dans `tests/data/` à partir de `replay2.csv`, calculé par `tests/make_replay.py` depuis `tests/data/sortie.gpx`. C'étaient donc des données d'une vraie sortie (position, heure, fréquence cardiaque). Ils ne sont pas suivis par Git (`git ls-files`), sont couverts par `*.fit` (`git check-ignore`), et `make clean` les supprime avec `replay*.csv`, couvert par `*.csv`.
- Depuis le 21 novembre 2026 (branche `tests/jeu-synthetique`, D15), `make test` n'écrit plus de données personnelles : ses `ride*.fit` et `replay*.csv` sont écrits dans `tests/data/synthetique/` à partir du jeu fictif. `make ui` écrit encore `tests/data/replay2.csv` depuis la sortie personnelle (ignoré par `*.csv`).
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
- Décision : `README.md` et `CLAUDE.md` à la racine, les autres documents de référence dans `docs/`. Documents de suivi : `TESTS.md`, `BUGS_CONNUS.md`, `HISTORIQUE.md`, ajoutés le 20 novembre 2026. Documentations techniques spécifiques (BLE, FIT, GPX, GVT1) créées seulement lorsque leur contenu le justifie.

### D13. Retrait du `Modelfile`
- Date : 20 novembre 2026
- Décision : retirer `Modelfile` de la racine du dépôt (suppression dans la PR qui porte cette décision).
- Contenu : deux lignes, `FROM qwen2.5-coder:14b` et `PARAMETER num_ctx 16384` (configuration Ollama), sans donnée personnelle.
- Constat : ajouté le 13 novembre 2026 dans le commit `dfec269` ("ajout fichier BLE"), avec `CLAUDE.md` et les fichiers BLE. Aucune référence dans le `Makefile`, le code, le `README.md` ni `CLAUDE.md` (recherche du 20 novembre 2026).
- Raison : sans rapport avec le firmware, non utilisé, recréable en deux lignes. Il reste consultable dans l'historique (`dfec269`).
- Suite : la branche locale `qwen` est abandonnée (D14).

### D14. Branche locale `qwen` abandonnée
- Date : 20 novembre 2026
- Décision : abandonner l'essai `qwen` sans examiner la branche. Rien n'est poussé sur GitHub.
- Constat : la branche est absente de GitHub (vérifié avec `git ls-remote`), son contenu n'a jamais été examiné. Elle reste, le cas échéant, sur la machine de Nicolas, hors dépôt. La garder ou la supprimer en local est son choix.
- Précaution : si elle est un jour poussée, contrôler son contenu avant (règle de D04 : aucune donnée personnelle ni jeton).

### D15. Jeu de test fictif pour `make test`
- Date : 21 novembre 2026
- Décision : `make test` s'appuie sur un jeu fictif généré par `tests/make_synthetique.py` (sortie de 66 km en plein Atlantique, 5 montées, base de cols fabriquée), pas sur la sortie personnelle de Nicolas. Le jeu n'est pas versionné : il est généré dans `tests/data/synthetique/` (ignoré par Git).
- Le simulateur n'est pas concerné : il reste utilisable avec n'importe quelle sortie contenant des heures et avec la vraie base `gps.csv` (`make sim GPX=... COLS=...`). `make ui` et `make nommage` gardent leurs fichiers personnels (B10).
- Raison : sur le Mac de Nicolas, `make test` échouait cinq fois (`test_sim`), car les tests exigent une sortie de plus d'une heure, deux montées et 50 km, et attendaient sept noms de montées propres à la sortie de référence (B08). Une base fabriquée permet aussi de vérifier des cas précis : col refusé car trop haut, lieu trop loin, type ignoré.
- Conséquences : `tests/test_cols.c` attend les noms du jeu fictif et reçoit ses fichiers en arguments (B02). `cols_fixture.csv` n'est plus utilisé.
- Un défaut du simulateur découvert grâce à ce jeu est corrigé dans un commit séparé (B09, `e5263a7`). Ce correctif modifie du code existant et a été fait sans accord explicite préalable (règle 2 de `CLAUDE.md`) : il reste à valider par Nicolas.

## Décisions à prendre

| Sujet | Options | Éléments à réunir |
|---|---|---|
| Avenir de GVM1 | Archiver sous tag puis retirer, ou conserver | Seul appelant : `tests/test_map_data.c` (vérifié dans l'index) |
| Décodeur BLE (`core/ble_decode`) | Brancher son test dans une cible Make et l'intégrer à l'application, ou archiver | Appelé seulement par son test. Aucune règle du Makefile ne compile `tests/test_ble.c` (Makefile lu le 19 novembre 2026) |
| Structure de `ui/app.c` | Découper en plusieurs fichiers | Mesure de la taille réelle |
| Branche `ecran-demarrage` | Fusionner, continuer ou archiver sous tag | 1 commit (`b7df9ff`, 15 novembre), 3 fichiers ajoutés (`ui/splash.c`, `ui/splash.h`, `tools/splash_to_c.py`), aucun fichier modifié, non branché dans le Makefile ni dans `ui/app.c`. Fusion sans conflit attendue |
| Jeux de test pour `make ui` et `make nommage` | Les ramener au jeu fictif, ou garder les fichiers personnels | B10 : `ui` n'a pas besoin de la vraie base, `nommage` si |
| Stratégie de nommage | Règle unique pour les fichiers et dossiers | n/a |
