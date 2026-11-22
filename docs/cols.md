# Base de cols pour nommer les montées

## Fichier `cols.csv` (à copier à la racine de la carte SD)
Une ligne par entrée, séparateur `;`, UTF-8 :

    type;nom;lat;lon;alt
    col;Col du Mont Tournier;45.634886;5.709346;821
    lieu;Burcin;45.457859;5.450367;

`type` vaut `col` ou `lieu` ; `alt` peut être vide. Les lignes qui commencent par `#` sont ignorées.

## Règles de correspondance (réglables dans `cols_cfg_t`)
1. Col : le plus proche du sommet de la montée, à moins de 300 m et à moins de 40 m d'altitude d'écart. Les sources d'altitude diffèrent de 15 à 20 m, d'où cette tolérance.
2. Sinon, lieu : le plus proche du **sommet**, à moins de 1500 m, donnant "Montée du Collet" (avec du, de la, des, de l', d'). Le réglage `place_mode` permet aussi le lieu du départ ("Montée du Revel") ou les deux ("Le Revel > Le Collet").
3. Sinon, pas de nom : l'écran affiche "Montée 3".
4. Pour économiser la mémoire, `cols_filter_route` ne garde que les entrées à moins de 3 km du parcours (1 113 entrées au lieu de 48 033 sur la sortie de test).

## Construire la base à partir d'OpenStreetMap (une seule fois)
1. Ouvre Overpass Turbo, zoome sur la région voulue (par exemple un département à la fois si la zone est grande).
2. Colle cette requête et lance-la :

       [out:json][timeout:180];
       (
         node["mountain_pass"="yes"]["name"]({{bbox}});
         node["natural"="saddle"]["name"]({{bbox}});
         node["place"~"^(village|hamlet|town|suburb|locality)$"]["name"]({{bbox}});
       );
       out body;

3. Exporte le résultat en GeoJSON.
4. Convertis : `python3 convertir_cols.py export.geojson cols.csv` (ou envoie-moi l'export).

La structure de la requête vient d'un exemple publié sur le forum d'OpenStreetMap, mais je n'ai pas pu l'exécuter moi-même : vérifie le nombre de cols obtenu.

### Autre méthode : à partir d'un fichier `.osm.pbf` (France entière, plusieurs types)

`tools/convertir_cols_pbf.py` lit directement un fichier `.osm.pbf` (par exemple `france-latest.osm.pbf` de Geofabrik) avec la bibliothèque `osmium` (`pip install osmium`), sans passer par Overpass.

```
python3 tools/convertir_cols_pbf.py france-latest.osm.pbf gps.csv            # menu : on choisit les types
python3 tools/convertir_cols_pbf.py france-latest.osm.pbf gps.csv col,lieu   # sans menu
```

Types possibles : `col`, `lieu`, `eau` (points d'eau potable), `boulangerie`, `commerce`, `toilette`, `velociste`, ou `tous`. Seuls `col` et `lieu` servent à nommer les montées ; le simulateur et le lecteur reconnaissent les autres (ils sont comptés comme « autres points d'intérêt ») mais ne les utilisent pas encore. Seuls les points (nœuds) nommés sont extraits : un commerce dessiné comme un bâtiment n'apparaît pas.

Si `gps.csv` existe déjà, les nouveaux points y sont ajoutés (mode incrémental) : un col de même nom à moins de 300 m d'un col déjà présent n'est pas dupliqué (l'altitude manquante est complétée). Les lignes existantes peuvent avoir un identifiant devant le type ; l'identifiant et les colonnes en plus ne sont pas conservés à la réécriture, et les lignes illisibles sont signalées puis perdues.

Mesures sur un fichier de test de 800 000 nœuds : 3,2 s, contre 10,7 s pour la version sans sortie anticipée des nœuds sans tag (les temps sur la France entière ne sont pas mesurés). Les tests ont utilisé un petit fichier `.pbf` construit pour l'occasion, pas un vrai extrait.

## À savoir
- `tests/data/synthetique/cols.csv` est une base de test fabriquée par `tests/make_synthetique.py` (données fictives), utilisée seulement par `test_cols.c`. La vraie base est ton export (`gps.csv`) : `make nommage` et le simulateur l'utilisent.
- Un petit col absent d'OpenStreetMap restera sans nom.
- Données © les contributeurs d'OpenStreetMap, licence ODbL : le crédit doit apparaître dans l'appareil ou sa documentation.
