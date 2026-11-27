# Écran de démarrage

Le composant d'écran de démarrage LVGL est ajouté dans cette branche. L'intégration au simulateur et au firmware ESP-IDF reste à effectuer et à valider avant de considérer la fonctionnalité terminée.

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
Avant de l'utiliser dans le simulateur, ajouter le composant et l'image générée aux sources de compilation, puis intégrer le déroulement du splash dans la boucle SDL en préservant les fonctionnalités actuelles du simulateur. Pour ESP-IDF, ajouter les sources au composant firmware et appeler `ui_splash_show()` après l'initialisation de l'accueil. Le test sur la carte ESP32-S3 reste à faire.
