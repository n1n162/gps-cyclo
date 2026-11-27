# Écran de démarrage

Le composant LVGL est intégré au simulateur SDL dans cette branche. L'intégration au firmware ESP-IDF et le test sur la carte ESP32-S3 restent à faire.

## Composants
- `tools/splash_to_c.py` recadre l'image en 320 × 480 et la convertit en RGB565 avec tramage.
- `ui/splash.c` et `ui/splash.h` construisent l'écran LVGL, animent la barre de progression, gèrent le fondu et la suppression différée.
- `ui/splash_img.c` sera généré à partir de `docs/ecran acceuil.jpg` par le script de conversion. Ce fichier généré n'est pas versionné.

## Générer l'image
Installer Pillow puis lancer :

    python3 -m pip install pillow
    python3 tools/splash_to_c.py "docs/ecran acceuil.jpg" ui/splash_img.c

Une prévisualisation peut être produite avec l'option `--preview aperçu.png`.

## Test du convertisseur
Le test utilise une image synthétique et ne nécessite ni GPX ni LVGL :

    python3 tests/test_splash_to_c.py

## Intégration restante
Le script `sim/build_sim.sh` génère l'image avec Pillow et compile le composant dans le simulateur. Au lancement normal, le splash reste affiché 2,5 secondes ou jusqu'à un clic ; le rejeu de la sortie est en pause pendant ce temps. L'autotest `--selftest` saute le splash pour conserver des tests déterministes. Pour ESP-IDF, ajouter les sources au composant firmware et appeler `ui_splash_show()` après l'initialisation de l'accueil. Le test sur la carte ESP32-S3 reste à faire.
