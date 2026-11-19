# Matériel

Statut : brouillon. Sources : schéma Waveshare ESP32-S3-Touch-LCD-3.5 Rev 2.0 et fiche technique ESP32-S3 v2.2 (fichiers du projet). Les informations non confirmées par ces documents sont marquées "à vérifier". Aucun composant n'a été commandé ni testé à ce jour d'après Nicolas.

## Carte retenue

Waveshare ESP32-S3-Touch-LCD-3.5 : écran tactile 3,5 pouces, 320 x 480.

Composants relevés sur le schéma :

| Fonction | Composant |
|---|---|
| Microcontrôleur | ESP32-S3R8 (8 Mo de PSRAM octale dans le boîtier, pas de flash intégrée) |
| Flash externe | XM25QH128DHIQT (128 Mbit, soit 16 Mo) |
| Écran et tactile | Dalle HXR35014C30 avec module tactile. Contrôleurs ST7796 et FT6336 d'après les notes de Nicolas : à vérifier |
| Gestion d'alimentation et charge LiPo | AXP2101, avec entrée NTC |
| Horloge temps réel | PCF85063ATL |
| Capteur de mouvement | QMI8658 (accéléromètre et gyroscope) |
| Expandeur d'entrées-sorties | TCA9554PWR (8 lignes EXIO) |
| Audio | Codec ES8311, amplificateur NS4150B, microphone |
| Carte microSD | Lecteur TF-07F |
| Connecteurs | USB Type-C (USB natif de l'ESP32-S3), batterie (SH1.0 2 broches et PH1.25 2 broches), caméra 24 broches, antenne U.FL, rangée de broches 2 x 16 au pas de 2,54 mm |
| Boutons | K1, K2, K3 (rôle de chacun à vérifier sur le schéma) |

## Contraintes issues de la fiche technique

- **Température** : la variante ESP32-S3R8 est donnée pour -40 à 65 degrés Celsius de température ambiante. Avec l'ECC de la PSRAM activé, la limite passe à 85 degrés, au prix de 1/16 de PSRAM utilisable. Point à traiter pour un appareil exposé au soleil sur un guidon.
- **Broches réservées** : avec une PSRAM octale, GPIO33 à GPIO37 sont utilisées par la PSRAM, et GPIO26 à GPIO32 par la mémoire flash et PSRAM. Ne pas les utiliser pour autre chose.
- **Broches de démarrage (strapping)** : GPIO0, GPIO3, GPIO45, GPIO46 imposent des niveaux au démarrage.
- **USB** : GPIO19 et GPIO20 sont l'USB natif. GPIO43 et GPIO44 sont l'UART0 (console).
- **Alimentation des broches** : 3,3 V.

## Capteurs à ajouter

| Capteur | Choix | Remarque |
|---|---|---|
| GNSS | u-blox MAX-M10S (visé) | Nécessite un UART libre et une alimentation 3,3 V. Choix des broches à faire, voir ci-dessous |
| Baromètre | I2C, modèle non choisi | Plusieurs périphériques I2C existent déjà sur la carte : vérifier les adresses et le bus |
| Ceinture cardio, capteur de cadence | BLE | Ceinture cardio disponible. Capteur de cadence non acheté |
| Batterie | LiPo | Capacité à choisir selon l'autonomie visée |

## Questions ouvertes

1. Quelles broches de la rangée 2 x 16 sont réellement libres pour l'UART du GNSS et pour le bus du baromètre ? À lire dans la page "PinOut" du schéma.
2. Le microcontrôleur tolère-t-il l'enveloppe thermique réelle d'un guidon au soleil ? Mesure à prévoir.
3. Autonomie réelle avec écran allumé, GNSS et BLE actifs : non mesurée.
4. Boîtier et fixation : non étudiés.

## Prototype actuel

Nicolas possède aussi une carte ESP32 DevKit C (ESP32-WROOM-32, sans PSRAM, ce n'est pas un S3). Elle ne convient pas à la cible finale (carte hors ligne, interface LVGL), mais peut servir à des essais de BLE ou de GNSS.
