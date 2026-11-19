# Cahier des charges

Statut : brouillon construit à partir des demandes de Nicolas (octobre et novembre 2026) et du README. Les lignes marquées "proposé" n'ont pas été validées.

## Objectif

Concevoir un compteur GPS vélo avec des fonctions comparables aux meilleurs appareils (Garmin, Wahoo, COROS), fabriqué à partir d'une carte ESP32-S3 avec écran tactile.

## Exigences

Priorité : P1 indispensable pour la première version, P2 important, P3 souhaitable.

| Id | Exigence | Priorité | Origine |
|---|---|---|---|
| E01 | Enregistrer la sortie au format FIT, avec reprise après interruption | P1 | demandé |
| E02 | Importer une trace GPX et guider sur la carte | P1 | demandé |
| E03 | Afficher une carte détaillée, orientation cap en haut | P1 | demandé |
| E04 | Mode parcours libre sans trace, avec carte affichée | P1 | demandé |
| E05 | Profil des montées type ClimbPro (pourcentage, distance restante) | P1 | demandé |
| E06 | Donner un nom à chaque montée : nom du col, sinon nom du lieu au sommet | P1 | demandé |
| E07 | Compatibilité capteurs de fréquence cardiaque et de cadence (BLE) | P1 | demandé |
| E08 | Export vers Strava | P1 | demandé, choix : export manuel du FIT |
| E09 | Écran couleur tactile 3,5 pouces, 320 x 480 | P1 | demandé |
| E10 | Carte hors ligne couvrant le département de l'Isère | P1 | demandé |
| E11 | Thème de carte clair par défaut, clair/sombre au choix dans les réglages | P2 | demandé |
| E12 | Fond de carte coloré selon la zone (ville, forêt) | P2 | demandé |
| E13 | Alertes : hors parcours, batterie, fréquence cardiaque, perte GPS, perte ceinture, montée proche | P2 | codé |
| E14 | Champs de données personnalisables | P2 | codé |
| E15 | Écran de démarrage personnalisé | P3 | demandé |
| E16 | Son et vibration pour les alertes | P3 | proposé |
| E17 | Alertes de zones de fréquence cardiaque détaillées | P3 | proposé |
| E18 | Autonomie cible, étanchéité, résistance à la chaleur | à définir | proposé |

## Contraintes

- Base de cols locale (pas de requête OpenStreetMap pendant l'import d'un parcours).
- Aucune donnée personnelle dans le dépôt public.
- Code commun testable sur PC sans matériel.

## Hors périmètre actuel

Navigation avec calcul d'itinéraire, connexion Wi-Fi à Strava, capteurs de puissance (voir ROADMAP.md pour l'ordre de traitement).

## À définir avec Nicolas

Autonomie visée, boîtier et fixation sur le vélo, type exact de capteur de cadence, besoin de capteur de puissance.
