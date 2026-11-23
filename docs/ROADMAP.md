# Feuille de route

Statut : proposition à valider par Nicolas. Les phases sont soumises aux règles de `CLAUDE.md` (un changement à la fois, test avant la suite, accord avant commit). Aucune date : l'ordre compte, pas le calendrier. Une phase ne commence que lorsque la précédente est terminée et vérifiée.

## Phase 0. Remettre le dépôt d'équerre

Objectif : un dépôt propre, reproductible et documenté.

- Vérifier l'état réel de Git : branches, historique, `.gitignore`. Rien ne doit être publié avant cette vérification.
- Contrôler l'absence de GPX, FIT, `gps.csv` ou jeton dans l'historique de toutes les branches.
- Rejouer les tests existants sur une machine équipée, consigner résultat, date, commit et environnement. Fait pour `make test` : bac à sable Linux le 21 novembre 2026, Mac de Nicolas le 22 novembre 2026 (`docs/TESTS.md`). Reste `make gvt-test` et `make map-test` sur le Mac de Nicolas.
- Décider du sort de la branche `ecran-demarrage` (la branche locale `qwen` est abandonnée, voir D14).
- Jeu de test fictif pour que `make test` marche sur un clone neuf : fait le 21 novembre 2026 (D15). Reste à étudier l'extension aux cibles `ui` et `nommage` (B10).
- Ajouter `requirements.txt` et une cible `make test-all` (tests, carte, BLE). Prévoir un message clair quand `garmin-fit-sdk` ou `fitdecode` manque (B12).
- Valider puis publier les huit documents racine.
- Renommer `docs/ecran acceuil.jpg`.

Critère de fin : un clone neuf compile et passe `make test-all` sans fichier personnel, et les premiers résultats rejoués sont consignés. Pour `make test`, ce critère est atteint (bac à sable Linux et Mac de Nicolas, après installation des bibliothèques Python).

## Phase 1. Corriger et consolider sur PC

- Réparer la recherche des cols avec le nouveau `gps.csv` (id OpenStreetMap).
- Brancher le test BLE dans une cible Make.
- Décider du sort de GVM1 et du décodeur BLE, tous deux appelés seulement par leurs tests.
- Intégrer l'écran de démarrage au simulateur et à l'interface.
- Mettre en place une intégration continue minimale.
- Découper `ui/app.c` (environ 2100 lignes), une responsabilité à la fois, avec les tests rejoués au vert à chaque étape.

Critère de fin : tests au vert, aucun problème connu de priorité haute dans ETAT_DU_PROJET.md.

## Phase 2. Premier essai sur matériel

Prérequis : carte Waveshare reçue.

- Créer le projet ESP-IDF et faire démarrer la carte.
- Afficher l'interface LVGL à l'écran, avec le tactile.
- Lire la carte microSD.
- Afficher la carte GVT1 depuis la microSD et mesurer la fluidité.
- Mesurer la température et la consommation.

Critère de fin : les fonctionnalités concernées passent au niveau "validé sur matériel" dans ETAT_DU_PROJET.md.

## Phase 3. Capteurs

- Module GNSS u-blox MAX-M10S : acquisition de position, choix des broches.
- Ceinture cardio en BLE réel (le décodeur existe).
- Capteur de cadence (à choisir et acheter).
- Baromètre I2C (à choisir et acheter).

## Phase 4. Sorties réelles

- Enregistrer une vraie sortie en FIT et la vérifier avec `verify_fit.py`.
- Export manuel vers Strava, vérifié avec la documentation Strava.
- Son et vibration pour les alertes.
- Alertes de zones de fréquence cardiaque.

## Phase 5. Appareil utilisable

- Autonomie, boîtier, fixation, étanchéité.
- Essais en conditions réelles (chaleur, pluie, vibrations).

## Risques à suivre

| Risque | Parade |
|---|---|
| Température ambiante maximale de 65 degrés pour l'ESP32-S3R8 | Mesure en phase 2, ECC de la PSRAM en secours (voir MATERIEL.md) |
| Broches libres insuffisantes pour GNSS et baromètre | Lire la page PinOut du schéma avant d'acheter |
| Dérive de la documentation | Mise à jour d'ETAT_DU_PROJET.md à chaque jalon |
| Fuite de données personnelles | Contrôle de l'historique en phase 0, règle dans CLAUDE.md |
