# Historique

Statut : brouillon. Dernière mise à jour : 21 novembre 2026, `main` à `44f51bc` (27 commits), plus la branche `tests/jeu-synthetique`.

Sources : `git log` des branches distantes (`main`, `ecran-demarrage` et les branches de documentation) et décisions de `docs/DECISIONS.md`. Chaque événement Git porte son commit.

## Avant le dépôt Git

Le dépôt commence le 4 novembre 2026 par un import de 96 fichiers (33 615 lignes). Le travail antérieur n'a pas d'historique Git. Il n'en reste que deux traces écrites : `docs/avancement.md` (état au 13 octobre 2026) et, dans `docs/ETAT_DU_PROJET.md`, la modification de `gps.csv` du 29 octobre 2026. Les affirmations d'`avancement.md` (essais sur une sortie de 103 km, tests, autotest du simulateur) sont historiques : elles n'ont été rejouées que partiellement (voir `docs/TESTS.md`).

## Chronologie Git

| Date | Commit | Événement |
|---|---|---|
| 4 novembre 2026 | `1586f19` | Commit initial : import de 96 fichiers |
| 4 novembre 2026 | `ff46c38` | `.gitignore` initial (données et environnement virtuel) |
| 13 novembre 2026 | `dfec269` | Décodage BLE (`core/ble_decode`, `tests/test_ble.c`), `CLAUDE.md` et un `Modelfile` |
| 14 novembre 2026 | `a48e457`, `c56b6d6`, `dbf5036`, `8bbb0a1` | Export Strava : script d'envoi du FIT par l'API et documentation. L'API demande un abonnement : export manuel retenu, script gardé |
| 14 novembre 2026 | `1280ca9` | Ajout de l'image `docs/ecran acceuil.jpg` |
| 15 novembre 2026 | `b7df9ff` | Écran de démarrage (`ui/splash`, `tools/splash_to_c.py`), sur la branche `ecran-demarrage`, non fusionné |
| 19 novembre 2026 | `47930bd`, `81dc2b0`, `066542e`, `5658a88`, `ea7a114` | Refonte de la documentation : README, `CLAUDE.md`, `DECISIONS.md` et documents ajoutés par envoi |
| 19 novembre 2026 | `61191c4` | `.gitignore` étendu : GPX, FIT, TCX, `build/`, `.env` |
| 19 novembre 2026 | `a801f7b`, `a0461ce` | Données personnelles consignées (D04), état vérifié |
| 19 novembre 2026 | `83d008c` | PR #1 fusionnée |
| 20 novembre 2026 | `877c64c`, `1e5f209` | PR #2 : mise à jour de la documentation |
| 20 novembre 2026 | `97c79df`, `7f25ca0` | PR #3 : captures de `docs/captures/` vérifiées |
| 20 novembre 2026 | `e77d051`, `33e90a7`, `b1744a1` | PR #4 : `Modelfile` supprimé, documentation mise à jour |
| 21 novembre 2026 | `34cd4a3`, `17e66d4`, `44f51bc` | PR #5 : `TESTS.md`, `BUGS_CONNUS.md` et `HISTORIQUE.md` ajoutés, documents alignés |
| 21 novembre 2026 | `e5263a7` | Branche `tests/jeu-synthetique` : correctif de « montée précédente » du simulateur (B09) |
| 21 novembre 2026 | `6329ee4` | Branche `tests/jeu-synthetique` : jeu de test fictif pour `make test` (D15, B02, B03, B08) |
| 21 novembre 2026 | `02def03` | Branche `tests/jeu-synthetique` : `.gitignore` étendu (B07) |
| 21 novembre 2026 | `2fcd58e` | Branche `tests/jeu-synthetique` : README, `CLAUDE.md`, `ARCHITECTURE.md`, `cols.md`, `simulateur.md` alignés |
| 21 novembre 2026 | commit de documentation suivant, même branche | `TESTS.md`, `BUGS_CONNUS.md`, `HISTORIQUE.md`, `ETAT_DU_PROJET.md`, `DECISIONS.md` (D15), `ROADMAP.md` |

## Remise en ordre du dépôt (19 et 21 novembre 2026)

- 19 novembre : règles de travail (D11) et organisation documentaire (D12). État audité en lecture seule sur GitHub.
- 19 et 20 novembre : protection des données (D04). `.gitignore` corrigé, historique contrôlé sur 18 commits des trois branches distantes de l'époque (aucun fichier de données), fichiers `ride*.fit` vérifiés, 19 captures examinées une par une et conservées.
- 20 novembre : `Modelfile` retiré (D13), branche locale `qwen` abandonnée sans examen (D14).
- 20 novembre : premier rejeu partiel des tests dans un bac à sable Linux (`docs/TESTS.md`).
- 21 novembre : `make test` lancé sur le Mac de Nicolas : 5 échecs de `test_sim` dus à sa sortie, qui révèlent que les tests sont calés sur la sortie de référence. Décision : jeu de test fictif pour les tests, simulateur inchangé (D15). Un défaut du simulateur est trouvé et corrigé (B09).
- 21 novembre : premier `make test` complet au vert (bac à sable, jeu fictif, clone neuf).

## Suite

Les prochaines étapes sont dans `docs/ROADMAP.md`. Ajouter ici une ligne par fusion ou par décision, avec le commit.
