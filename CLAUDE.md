# Instructions pour Claude (gps-cyclo)

Ce fichier fixe les règles de travail. Il ne contient ni l'état du projet ni son historique : lire `README.md` pour la vue d'ensemble, `docs/ETAT_DU_PROJET.md` pour l'état réel, `docs/DECISIONS.md` pour les décisions, `docs/ARCHITECTURE.md` pour la structure. Ne pas dupliquer leur contenu ici.

## Utilisateur et langue

- Nicolas ne code pas : Claude lit le dépôt et écrit le code. Donner des commandes complètes, à copier telles quelles, et expliquer en français simple.
- Répondre en français. Pas d'emoji, pas de tiret cadratin.
- Réponses concises, raisonnement rigoureux.

## Règles impératives

1. **Périmètre minimal.** Ne modifier que ce que la tâche demande. Ne jamais toucher à plusieurs parties du projet sans nécessité. Un changement logique par commit, documentation et code dans des commits séparés.
2. **Aucune suppression sans validation.** Ne jamais supprimer ni réécrire du code existant sans l'accord explicite de Nicolas. Ce qui doit disparaître est d'abord archivé sous un tag Git.
3. **Toujours une version fonctionnelle.** `main` compile et passe les tests à tout moment. Tout travail se fait sur une branche. Avant une modification risquée, indiquer comment revenir en arrière.
4. **Tester avant de continuer.** Chaque modification est testée avant de passer à la suivante, avec la cible Make concernée. Si l'environnement ne permet pas d'exécuter le test (SDL2, matériel, fichier personnel manquant), le dire explicitement et ne rien conclure.
5. **Aucune annonce sans preuve.** Ne jamais écrire qu'une fonctionnalité fonctionne sans preuve : commande lancée, sortie obtenue, date, commit. Un code qui existe ou une lecture du code ne prouvent pas le fonctionnement.
6. **Aucun commit ni push sans accord.** Demander l'accord de Nicolas avant chaque commit et avant chaque push, à chaque fois, même si un accord a été donné plus tôt.
7. **Ne jamais deviner.** Pas d'API, de version, d'option, de nom de paquet ni de SHA de commit supposés : vérifier dans le code ou la documentation officielle. Pour Strava, ESP-IDF, LVGL, BLE et le format FIT, lire la documentation avant d'écrire du code.

## Niveaux de preuve

Les statuts autorisés dans la documentation sont :

- **Codé** : le code existe.
- **Test présent (non rejoué)** : un test existe et est branché dans une cible Make.
- **Rejoué** : test exécuté avec succès, avec date, commit et environnement consignés.
- **Validé sur matériel** : vérifié sur la carte Waveshare.

Toute modification d'un statut dans `docs/ETAT_DU_PROJET.md` exige la preuve correspondante.

## Données personnelles

Ne jamais ajouter au dépôt : fichiers `.gpx`, `.fit`, `.tcx`, `tests/data/gps.csv`, jetons ou identifiants Strava, fichiers `.env`. Avant tout commit, vérifier `git status` et le contenu du `.gitignore`. Un fichier déjà commité reste dans l'historique : le signaler immédiatement à Nicolas.

## Compiler et tester

    make test          # tests principaux (nécessite tests/data/sortie.gpx, fourni par Nicolas)
    make gvt-test      # carte GVT1 (hors make test)
    make map-test      # ancien format GVM1 (hors make test)
    make sim GPX=sortie.gpx COLS=gps.csv   # simulateur SDL2
    make ui            # aperçu UI (télécharge LVGL 9.2.2)

`make ui` et `make sim` écrivent dans `build/` et téléchargent des dépendances : ne les lancer que si nécessaire et après accord.

## Conventions de code

- `core/` : C portable, sans dépendance, sans allocation dynamique si possible, rien de spécifique au matériel.
- `ui/` : LVGL 9. Ne pas ajouter de logique métier dans `ui/app.c`, la placer dans `core/`.
- Chaque module de `core/` a un test dans `tests/`, branché dans une cible Make.
- Noms de fichiers sans espace ni faute.

## Mise à jour de la documentation

Toute décision technique est ajoutée à `docs/DECISIONS.md` avec sa date. Tout changement d'état est reporté dans `docs/ETAT_DU_PROJET.md`.
