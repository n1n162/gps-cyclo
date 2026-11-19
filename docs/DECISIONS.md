# Décisions techniques

Statut : brouillon. Les décisions ci-dessous viennent des déclarations de Nicolas (octobre et novembre 2026) et du README. Les dates marquées "à préciser" doivent être confirmées avec l'historique Git.

Format : une décision par entrée, jamais réécrite. Pour changer d'avis, ajouter une nouvelle entrée qui remplace l'ancienne.

## Décisions prises

### D01. Matériel du prototype : Waveshare ESP32-S3-Touch-LCD-3.5
- Date : octobre 2026 (à préciser)
- Décision : carte tactile 3,5 pouces 320 x 480 avec 8 Mo de PSRAM.
- Raison : écran tactile demandé, priorité au détail de la carte.
- Remplace : essais prévus sur ESP32 DevKit C, sans PSRAM.

### D02. Code métier en C portable dans `core/`
- Date : octobre 2026 (à préciser)
- Décision : logique en C sans dépendance, testée sur PC, destinée à ESP-IDF. Interface en LVGL 9 (version 9.2.2), simulation sur PC avec SDL2.
- Raison : tester sans matériel.

### D03. Base de cols locale
- Date : octobre 2026 (à préciser)
- Décision : base locale issue d'OpenStreetMap (`gps.csv`), pas de requête OSM à chaque import de parcours.
- Nom d'une montée : nom du col, sinon nom du lieu au sommet.

### D04. Données personnelles hors du dépôt
- Date : octobre 2026 (à préciser)
- Décision : aucun GPX, FIT ni trace dans le dépôt.
- Raison : une sortie commence et finit souvent au domicile.
- Mise en oeuvre annoncée : l'ancien README indiquait que `.gitignore` exclut `*.gpx` et `*.fit`.
- Statut de la vérification : non vérifié. Le contenu du `.gitignore` et l'historique Git n'ont jamais été examinés. La règle est une intention, pas un fait établi, jusqu'à preuve.
- Condition avant toute publication sur GitHub : lire le `.gitignore`, contrôler l'historique de toutes les branches (GPX, FIT, TCX, `gps.csv`, jetons), et corriger avant de publier si nécessaire.

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
| Décodeur BLE (`core/ble_decode`) | Brancher son test dans une cible Make et l'intégrer à l'application, ou archiver | Appelé seulement par son test. Lire les règles du Makefile |
| Structure de `ui/app.c` | Découper en plusieurs fichiers | Mesure de la taille réelle |
| Branches `ecran-demarrage` et `qwen` | Fusionner, continuer, archiver | Origine et contenu de chaque branche |
| Jeu de test public | GPX synthétique versionné | Choix du parcours |
| Stratégie de nommage | Règle unique pour les fichiers et dossiers | n/a |
