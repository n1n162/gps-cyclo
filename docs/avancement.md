# Avancement (13 octobre 2026)

## Les cinq exigences
- **Profil des montées type ClimbPro** : fait. Détection avec fusion et recul du départ, catégories Garmin, noms de cols, écrans dédiés. Vérifié sur une sortie de 103 km, comparé à Strava et au profil officiel du Mont Tournier.
- **Enregistrement de la séance** : écriture FIT faite et relue sans erreur par le décodeur officiel Garmin et par fitdecode, avec récupération après coupure. L'acquisition GNSS reste à faire.
- **Export Strava** : script d'envoi par l'API écrit (`tools/strava_upload.py`, mode d'emploi dans `docs/export_strava.md`), testé contre un faux serveur. Le premier envoi réel chez Strava reste à faire. L'envoi direct depuis la carte n'est pas écrit.
- **Guidage sur carte** : trace GPX faite (chargement, positionnement, erreur moyenne de 3 m en simulation). Il manque le fond de carte et les indications de virage.
- **Capteurs FC et cadence** : pas commencé. L'exemple Espressif adapté à une ceinture cardio est repéré.

## Interface
Application LVGL en portrait 320 x 480, mise à jour en direct (aucun objet recréé : 249 objets du début à la fin d'une sortie simulée). La page de montée s'ouvre seule au début de la montée, une fenêtre de fin affiche durée, distance, pente, dénivelé, vitesse et fréquence cardiaque moyennes, puis disparaît ; l'utilisateur peut la fermer ou quitter la page sans être ramené de force. Testé en simulation avec un doigt virtuel (`make ui`, trois scénarios).

## Champs personnalisables
17 champs au choix (temps, heure, distance, vitesse et sa moyenne et son maximum, fréquence cardiaque et sa moyenne, cadence, puissance, dénivelé, altitude, pente, reste, arrivée estimée, prochaine montée, batterie). Un appui long sur un champ ouvre le sélecteur ; on peut aussi passer à 4, 6 ou 8 champs, le profil des prochains kilomètres prenant la place libérée. La configuration s'encode en texte (`v1;8;0,2,6,...`) : l'action `config_changed` est appelée à chaque changement, il reste à brancher l'enregistrement sur la carte (SD ou mémoire flash). Testé en simulation (`make ui`, scénario f).

## Alertes
Six alertes : hors parcours, batterie faible (20 %) puis critique (10 %), fréquence cardiaque trop élevée (désactivée par défaut, limite réglable), signal GPS perdu, ceinture cardio perdue, montée proche (annonce à 300 m). Chaque alerte attend quelques secondes avant de se déclencher, ne s'efface qu'après un retour à la normale, se rappelle toutes les 30 s si on l'ignore (hors parcours, fréquence cardiaque) et s'acquitte en touchant la bannière. Les alertes d'information disparaissent seules après 6 s, les avertissements après 10 s, les alertes critiques restent. Les réglages s'ouvrent par un appui long sur la barre d'état et s'enregistrent sous forme de texte (`a1;110111;180;20;10`, action `alerts_changed`). L'action `alert` signale chaque nouvelle alerte : c'est là que le bip ou la vibration seront branchés. Testé en moteur seul (48 vérifications, `tests/test_alerts.c`) et dans la simulation de sortie (`make ui`, scénario g).

## Simulateur
Une fenêtre avec l'appareil et un tableau de commandes : lecture d'une sortie à x1, x10, x60 ou x300, saut avant la montée précédente ou suivante, pannes (hors parcours, GPS, ceinture, FC trop haute, batterie). Moteur de rejeu testé (`make test`), simulateur complet vérifié par autotest avec de vrais clics de souris sous Linux. Pas encore essayé sur macOS. Mode d'emploi : `docs/simulateur.md`.

## Points de vigilance
- Tout est validé sur ordinateur uniquement.
- Les broches libres de l'en-tête de la carte pour le GNSS ne sont pas confirmées (tableau en image sur la page Waveshare).
- Compatibilité de l'interface LVGL 9 avec `esp_lvgl_port` : à confirmer à la compilation sur la carte.
- Toucher sous la pluie ou avec des gants, et lisibilité au soleil : à tester sur le vélo.
- L'altitude diffère de 15 à 20 m selon les sources (dénivelé et noms de cols).
- Les noms de montées viennent du lieu OpenStreetMap le plus proche du sommet : pas toujours l'usage local.

## Prochaines étapes
1. Commander le matériel (carte, GNSS, baromètre, batterie).
2. Projet ESP-IDF : écran et tactile, puis GNSS, enregistrement FIT sur SD, ceinture cardio.
3. Son ou vibration pour les alertes, une fois la carte en main.
4. Fond de carte hors ligne.
