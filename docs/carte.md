# Carte hors ligne : tuiles vectorielles GVT1

Cette conception utilise exclusivement les fichiers OpenStreetMap .osm.pbf comme source de la carte. Le convertisseur PBF produit GVT1 en flux, avec stockage temporaire SQLite. Le format historique GVM1 est indépendant et n'est pas utilisé par cette carte.

## Principe

Une carte est un fichier unique sur la microSD. Il contient des **tuiles** de la grille XYZ (Web Mercator), chacune avec les lignes et les surfaces à dessiner à un zoom donné. L'appareil ne lit que les tuiles visibles. Pour une carte nationale, l'index peut rester sur la microSD et être recherché par dichotomie, sans le charger en PSRAM. La carte est dessinée dans une image en mémoire, qui n'est refaite que quand c'est nécessaire.

Contenu (format version 2) :
- **lignes** : routes, chemins, cours d'eau, voies ferrées ;
- **surfaces** : zones urbaines, forêts, champs et prairies, parcs, roche et glaciers, plans d'eau. Elles colorent le fond de la carte. Pas de noms ni de points d'intérêt.

Les fichiers de la version 1 (lignes seules, comme `carte_grenoble.gvt` du premier essai) restent lisibles : le fond est alors uni.

## Niveaux, classes et couleurs

Zooms stockés : 11 à 15 (réglable à la conversion). Au zoom 15, une tuile fait environ 0,86 km de côté à la latitude de l'Isère et 4096 unités, soit environ 0,21 m par unité.

Lignes (classes 1 à 9) :

| Classe | Contenu OSM | Zoom minimal |
|---|---|---|
| 1 | motorway, trunk, primary | 11 (les `_link` : 12) |
| 2 | secondary | 12 (`_link` : 13) |
| 3 | tertiary | 13 (`_link` : 14) |
| 4 | unclassified | 13 |
| 4 | residential, living_street, road | 14 |
| 4 | service | 15 |
| 5 | cycleway | 13 |
| 6 | track, path, bridleway | 14 |
| 6 | footway, steps, pedestrian | 15 |
| 7 | waterway river, canal | 12 (stream : 14) |
| 8 | natural water ou coastline en ligne ouverte | 12 |
| 9 | railway rail | 12 (narrow_gauge, light_rail : 13 ; tram, funicular : 14) |

Surfaces (classes 10 à 15), de la plus basse à la plus haute dans l'ordre de dessin :

| Classe | Contenu OSM | Zoom minimal |
|---|---|---|
| 12 champs | landuse farmland, meadow (12), grass, orchard, vineyard, village_green (13), allotments (14) ; natural scrub, grassland, heath (12) | 12 à 14 |
| 11 forêts | landuse forest ; natural wood | 11 |
| 14 roche | natural bare_rock, scree, glacier | 11 |
| 10 urbain | landuse residential, commercial, retail, industrial | 11 |
| 13 parcs | leisure park (12), garden, recreation_ground, golf_course (13) | 12 à 13 |
| 15 eau | natural water (polygones, y compris les lacs en relations) | 11 |

Là où aucune surface n'existe, le fond est la couleur de base du thème.

Deux thèmes de couleurs (`ui_app_set_map_theme`) : 0 sombre coloré (fond 0x1B2229, forêts vert foncé, eau bleu profond, routes claires) et 1 clair (fond 0xF2EFE9 comme la carte OpenStreetMap, forêts vert tendre, routes orange, jaune et gris, pistes cyclables violettes pour ne pas les confondre avec la trace bleue). L'échelle et la mention OpenStreetMap changent de couleur avec le thème. Les captures `docs/captures/carte_demo_sombre.png` et `carte_demo_clair.png` montrent les deux sur une petite ville fictive. Le thème par défaut est le **clair**. Il se choisit dans l'écran RÉGLAGES de la page d'accueil (boutons SOMBRE et CLAIR, `docs/captures/reglages_carte.png`) et s'enregistre à part de la configuration des champs, sous la forme `m1;0` (sombre) ou `m1;1` (clair) : `ui_map_cfg_encode` et `ui_map_cfg_decode`, hook `ui_hooks.map_cfg_changed` appelé à chaque changement, `ui_app_set_map_cfg` pour appliquer un réglage relu au démarrage. L'écriture et la relecture sur la microSD ou la mémoire flash restent à brancher sur l'appareil (le simulateur affiche `CONFIG CARTE  m1;1`).

Un objet plus petit que `--min-size` unités (8 par défaut, soit environ une demi-largeur de pixel) est supprimé à ce zoom. Les lignes sont simplifiées par Douglas-Peucker avec une tolérance de `--tolerance` unités (8), les surfaces avec `--area-tolerance` (16).

## Format du fichier (petit-boutiste)

En-tête, 36 octets :

| Décalage | Taille | Champ |
|---|---|---|
| 0 | 4 | `GVT1` |
| 4 | 2 | version (2 ; la version 1 n'a pas de surfaces) |
| 6 | 2 | étendue de tuile (4096) |
| 8 | 1 | zoom minimal |
| 9 | 1 | zoom maximal |
| 10 | 2 | réservé |
| 12 | 4 | nombre de tuiles |
| 16 | 4 | décalage de l'index (36) |
| 20 | 16 | emprise : ouest, sud, est, nord, en degrés x 1e7 (int32) |

Index, 16 octets par tuile, trié par clé croissante : clé (uint64) = `zoom << 32 | x << 16 | y`, décalage absolu (uint32), taille (uint32).

Tuile : nombre d'enregistrements (varint), puis pour chacun : classe (1 octet), type (1 octet : 0 ligne, 1 surface), puis
- ligne : nombre de points (varint, 2 à 1024), puis les points ;
- surface : nombre d'anneaux (varint, 1 à 64), puis pour chaque anneau son nombre de points (varint, au moins 3) et ses points. Au plus 4096 points par surface dans une tuile. Remplissage pair-impair : un anneau à l'intérieur d'un autre est un trou, et le dernier point n'est pas répété.

Les points sont des entiers relatifs à la tuile : le premier en absolu, les suivants en différence avec le précédent, chaque valeur en zigzag + varint. Les coordonnées vont de -64 à 4160 (marge de découpe de 64 unités, pour éviter les trous aux bords). Les enregistrements sont triés par classe croissante. Une ligne de plus de 1024 points est coupée en morceaux qui se recouvrent d'un point. Une tuile entièrement à l'intérieur d'une forêt contient un simple rectangle plein.

### Intégrer le choix du thème dans une page de réglages existante

Par défaut, le bouton RÉGLAGES de la page d'accueil ouvre un écran de l'application qui ne contient que le choix du thème (`build_settings_screen` dans `ui/app.c`). Deux façons de ne pas remplacer une page de réglages que tu as déjà :

- compiler avec `-DUI_HOME_SETTINGS_SCREEN=0` : le bouton RÉGLAGES garde alors exactement son comportement d'origine, et l'application ne crée aucun écran de réglages ;
- appeler `ui_app_add_theme_picker(&app, parent, x, y, largeur)` dans ta propre page : elle ajoute l'étiquette FOND DE CARTE et les deux boutons SOMBRE / CLAIR dans `parent`, sans toucher au reste, et retourne la hauteur occupée (92 pixels). À appeler une seule fois.

Dans les deux cas, le hook `ui_hooks.map_cfg_changed` signale chaque changement, et `ui_app_set_map_cfg` applique un réglage relu au démarrage.

## Fichiers

| Fichier | Rôle |
|---|---|
| `tools/osm_pbf_to_gvt.py` | convertisseur unique .osm.pbf vers GVT1 ; lecture Pyosmium, stockage temporaire SQLite |
| `tools/osm_pbf_to_gvl.py` | extraction des noms de villes et villages depuis le même PBF vers GVL1 |
| `core/gvt.h`, `core/gvt.c` | lecteur C : en-tête, index, recherche d'une tuile, décodage des lignes et des surfaces |
| `core/mapview.h`, `core/mapview.c` | vue carte sans LVGL : niveau de zoom, tuiles à lire, passage tuile vers pixels, cache compact et cache optionnel des grosses tuiles |
| `core/polyfill.h`, `core/polyfill.c` | remplissage de surfaces (pair-impair) dans un tampon RGB565 |
| `core/linedraw.h`, `core/linedraw.c` | segments épais lissés à bouts arrondis dans un tampon RGB565 |
| `ui/app.c`, `ui/app.h` | image de la carte dans la page trace : `ui_app_set_map`, `ui_app_set_map_theme` |
| `tests/make_gvt_fixture.py`, `tests/make_gvt_demo.py` | données synthétiques (lignes et surfaces) ; petite ville fictive |
| `tests/verify_gvt.py`, `tests/test_gvt.c`, `tests/test_mapview.c`, `tests/test_polyfill.c`, `tests/test_linedraw.c` | tests du format, du rendu et des données synthétiques |
| `ui/map_preview.c`, `ui/build_map_preview.sh` | aperçu de la page trace avec fond de carte, sans GPX personnel |
| `make gvt-test` | enchaîne tous les tests de la carte |

## Utilisation

Côté programme hôte (ordinateur ou ESP32-S3) :

1. Fournir une fonction de lecture sur le fichier de la carte, déterminer sa taille (en octets, fichier limité à 4 Go par les offsets GVT1), puis appeler `gvt_open(&g, read, ctx, taille)`. L'ouverture refuse un en-tête incomplet ou un index qui dépasse la taille réelle du fichier. Chaque entrée d'index est contrôlée avant utilisation : clé/zoom/coordonnées cohérents, tuile non vide, données situées après l'index et entièrement contenues dans le fichier.
2. Pour une petite carte, `gvt_load_index` peut charger l'index en RAM (16 octets par tuile) et accélérer les recherches. Pour une carte nationale, ne pas charger cet index : `gvt_find_entry` et `mapview_get` font la recherche dichotomique dans le fichier, avec un cache LRU fixe de 4 blocs contigus de 512 octets (2 Ko au total). Chaque bloc contient 32 entrees ; les lectures SD sont donc regroupees et les blocs reutilises entre les recherches.
3. Fournir `MAPVIEW_SLOTS` x `MAPVIEW_SLOT_BYTES` octets (16 x 64 Ko = 1 Mo) au cache courant, avec `mapview_init`.
4. Facultatif : fournir un second tampon pour les grosses tuiles avec `mapview_set_big_cache(&mv, mem, taille_emplacement, nombre_emplacements)`. Ce cache LRU est séparé du cache compact et peut garder plusieurs grosses tuiles simultanément. Le code accepte jusqu'à `MAPVIEW_BIG_SLOTS_MAX` emplacements ; la mémoire est entièrement fournie par l'appelant. Sans ce second cache, les tuiles dépassant 64 Ko restent refusées.
5. Fournir `UI_MAP_BUF_BYTES` octets (341 Ko) pour l'image de la carte : `ui_app_set_map(&app, &mv, canvas)`. `ui_app_set_map(&app, NULL, NULL)` enlève la carte. Sans carte, la page trace est inchangée.


Mémoire de travail à prévoir en PSRAM : image de la carte 341 Ko, cache compact 1 Mo, environ 88 Ko pour le remplissage des surfaces (`polyfill.c`) et 48 Ko de tampons de décodage dans `ui/app.c`, auxquels s'ajoute le cache optionnel des grosses tuiles selon sa configuration. Le simulateur PC utilise provisoirement 4 emplacements de 256 Ko (1 Mo) pour tester ce second niveau ; ce dimensionnement n'est pas encore retenu pour l'ESP32-S3. Il sera fixé après mesure des tailles aux zooms 11 à 15.

La carte est centrée sur le même point que la trace (le point du parcours le plus proche de la position), avec la même échelle et la même rotation (nord en haut ou trace devant). Elle n'a donc pas besoin de latitude et longitude dans `ui_sample_t`.

Dessin : fond, puis surfaces dans l'ordre du tableau ci-dessus, puis lignes en trois passes (eaux et voies ferrées, puis chemins et petites routes, puis routes principales). Les segments de moins de 1,5 pixel et ceux hors de l'image sont ignorés. L'image fait 32 pixels de plus que l'écran de chaque côté. Elle est **redessinée seulement** si l'échelle change, si le cap tourne de plus de 1,5 degré, ou si la position sort de la marge ; sinon elle est simplement décalée. Mesuré en simulation, sur la carte de Grenoble, pour 5 minutes à 6 m/s : nord en haut, 32 redessins à 250 m de demi-largeur, 14 à 600 m, 5 à 1,5 km ; trace devant, 35, 18 et 13. Avant le cache, la carte était redessinée toutes les secondes (300 fois).

LVGL n'est pas utilisé pour les lignes de la carte : sa file de tâches de dessin garde chaque ligne en mémoire (plusieurs milliers de segments épuisent les 256 Ko du tas de LVGL) et a fini par ne plus rendre la main sur un enchaînement de lignes sur un canevas, alors que la même ligne seule passait.

## Vérification sur ordinateur

- `make gvt-test` :
  - fichier produit contre les données d'origine (sommets, points hors trace, et pour les surfaces des milliers de points tirés au hasard, dont beaucoup près des bords : un point est dans une surface décodée exactement quand il est dans la surface d'origine, sauf à moins de tolérance + 2 unités du bord) ; trois bugs injectés volontairement dans le convertisseur sont tous détectés ;
  - lecteur C, tuiles tronquées, tampon trop petit ;
  - alignement carte/trace (écart maximal 0,05 pixel), zoom, couverture des tuiles pour toutes les rotations, cache ;
  - remplissage et lignes : plus de 4 millions de pixels de remplissage et 12 millions de pixels de lignes comparés à une référence indépendante, aucun écart hors égalités d'arrondi sur une arête ;
  - conversion des fixtures GeoJSON synthétiques par un outil réservé aux tests ; cette cible ne valide pas le lecteur PBF Pyosmium, qui doit être rejoué avec un vrai fichier .osm.pbf.
- `sh ui/build_map_preview.sh` : rend la page trace avec LVGL 9.2.2 (thèmes sombre et clair, avec et sans la trace), compte les redessins pendant une sortie simulée, et vérifie le réglage du thème (clair par défaut, enregistrement, refus de 8 enregistrements invalides, parcours de l'écran RÉGLAGES jusqu'au retour à l'accueil).
- `sh sim/build_sim.sh parcours.gpx [sortie.gpx] --carte carte.gvt` : le simulateur affiche la carte sur la page trace (deuxième page). Au démarrage, il indique quelle part du parcours est couverte par la carte (un point tous les 500 m) et avertit si moins de la moitié l'est : sans carte sous le parcours, la page trace n'a qu'un fond uni. Avec `--selftest`, une vérification de plus contrôle que la page trace dessine des routes de la carte, sans erreur de lecture : elle échoue si la carte ne couvre pas le parcours.

## Produire la carte depuis un fichier PBF

La source officielle de la carte est un fichier OpenStreetMap au format .osm.pbf. Pour la France, télécharger le fichier France depuis Geofabrik (par exemple france-latest.osm.pbf) puis lancer :

```bash
python3 -m pip install osmium
python3 tools/osm_pbf_to_gvt.py france-latest.osm.pbf france.gvt
```

Le convertisseur traite le fichier localement : aucune requête Overpass ni connexion réseau n'est nécessaire pendant la conversion. Il lit les objets OSM avec Pyosmium, découpe les lignes et les surfaces dans les tuiles GVT1 et utilise SQLite comme stockage temporaire pour limiter la mémoire. Les fragments sont d'abord insérés sans index secondaire ; l'index SQLite est créé après l'import du PBF. Lors de l'assemblage, le GVT1 final est écrit directement : aucun fichier temporaire `gvt_blobs_*` ne duplique les données de la carte. L'index GVT1 est réservé au début du fichier puis rempli progressivement.

Options utiles :

```bash
python3 tools/osm_pbf_to_gvt.py france-latest.osm.pbf france.gvt --zmin 11 --zmax 15
python3 tools/osm_pbf_to_gvt.py extrait-regional.osm.pbf isere.gvt
```

Les paramètres de simplification sont également disponibles : --tolerance, --area-tolerance, --min-size, --no-areas et --temp-dir. Par défaut, les surfaces sont incluses : forêts, zones urbaines, champs, parcs, roche et eau.

Les noms de villes et villages ne sont pas inclus dans GVT1. Ils sont extraits séparément du même PBF :

```bash
python3 tools/osm_pbf_to_gvl.py france-latest.osm.pbf france.gvl
```

Le format GVL1 est actuellement produit, mais son lecteur et le rendu des libellés dans LVGL restent à intégrer.

Pour couvrir un parcours, utiliser le PBF régional ou national qui contient la zone souhaitée. Le simulateur ne télécharge pas les données : il vérifie uniquement si le fichier GVT1 fourni couvre le parcours.

Mention obligatoire pour les données OpenStreetMap : © contributeurs OpenStreetMap, licence ODbL (l'appareil l'affiche déjà sous la carte).

## Taille pour l'Isère (estimation)

Avec une surface d'environ 7 400 km2 (valeur à confirmer), il y a au plus 13 300 tuiles pour les zooms 11 à 15 (9 965 au zoom 15). L'index fait donc au plus environ 207 Ko.

Mesuré sur le centre de Grenoble (boîte 5.67,45.14,5.78,45.23) avec les surfaces : 666 tuiles, 1,54 Mo (1,07 Mo pour les seules lignes), plus grande tuile 26,9 Ko sur 64 Ko de cache, soit environ 2,3 Ko par tuile en moyenne. Si toutes les tuiles de l'Isère lui ressemblaient, la carte ferait au plus environ 31 Mo. Le centre de Grenoble est l'une des zones les plus chargées du département, mais ses surfaces (zones urbaines, forêts des pentes, rivière) ne sont pas représentatives des zones agricoles ou de haute montagne.

## Limites connues

- Les surfaces ont été vérifiées automatiquement sur des données synthétiques, et visuellement sur un extrait réel de Grenoble (forêts, zones urbaines, rivière, parcs reconnaissables, pas de trou aux bords de tuiles). Un anneau de relation qui ne se ferme pas (par exemple une forêt coupée par la limite de la zone téléchargée) est ignoré : sur l'extrait de Grenoble, une partie des forêts des pentes peut donc manquer.
- Le convertisseur PBF utilise Pyosmium et SQLite pour traiter les grands fichiers sans charger toutes les géométries en mémoire. Le temps de conversion et l'espace temporaire disque dépendent de la taille du PBF et des niveaux de zoom.
- Le rendu n'a été essayé que sur ordinateur. Le temps d'un dessin complet de la carte sur l'ESP32-S3 n'est pas mesuré : sur le PC de développement, un dessin complet de la carte de Grenoble avec surfaces (jusqu'à 7 800 segments) prend 5 à 17 ms.
- Un cap qui change de moins de 1,5 degré entre deux dessins laisse un décalage de quelques pixels aux bords de l'écran entre la carte et la trace ; la carte est redessinée dès que ce seuil est dépassé.
- La lecture d'une tuile sur la microSD se fait pendant le dessin : une tuile lente bloque l'affichage. En mode sans index RAM, chaque recherche dichotomique effectue plusieurs petites lectures dans l'index du fichier ; le lecteur utilise maintenant un cache LRU de 4 blocs de 512 octets (2 Ko), ce qui regroupe les lectures et reutilise les blocs lors des recherches suivantes. Les tuiles de plus de 64 Ko utilisent le cache optionnel des grosses tuiles ; celles qui dépassent la capacité configurée sont refusées (compteur `too_big`).
- La fonction de lecture de la microSD pour la carte Waveshare n'est pas écrite.
