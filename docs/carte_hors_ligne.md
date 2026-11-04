# Carte hors ligne — première brique (GVM1)

Le projet introduit un format vectoriel binaire `GVM1` et un convertisseur exécuté sur ordinateur. Cette étape **ne modifie pas encore l'écran de navigation** : elle fournit la base de données compacte et son lecteur C, à intégrer ensuite dans LVGL.

## Format GVM1 v1

Tous les entiers sont little-endian. Les coordonnées sont des degrés signés multipliés par 10 000 000.

- En-tête de 16 octets : `GVM1`, version `uint16` (=1), flags `uint16` (=0), nombre d'objets `uint32`, réservé `uint32`.
- Chaque objet a un en-tête de 20 octets : type `uint8`, flags `uint8`, nombre de points `uint16`, puis min/max latitude et min/max longitude (`int32` chacun).
- Chaque point occupe 8 octets : latitude `int32`, longitude `int32`.
- Types : 1 route, 2 piste cyclable, 3 chemin/sentier, 4 eau, 5 voie ferrée, 6 espace vert, 7 bâtiment, 8 point d'intérêt.

Le lecteur `core/map_data.c` parcourt le fichier en flux. Il ignore les objets hors de la fenêtre géographique sans charger toute la carte en RAM. Le code appelant fournit un tampon de points; les objets dépassant sa capacité sont ignorés. Pour la carte embarquée, le tampon devra être dimensionné et les géométries longues éventuellement simplifiées par le convertisseur.

## Conversion

Exporter une zone OSM/Overpass au format GeoJSON, puis :

```sh
python3 tools/osm_to_gvm.py extract.geojson isere.gvm
# Optionnel : rectangle ouest,sud,est,nord en degrés
python3 tools/osm_to_gvm.py extract.geojson zone.gvm --bbox 5.4,45.3,5.9,45.8
```

Le convertisseur Python n'a pas de dépendance externe. Il reconnaît les routes, pistes cyclables, sentiers, voies d'eau, voies ferrées, espaces verts, bâtiments et certains points d'intérêt. Pour une grande région, il faudra générer des extraits par zone et limiter les objets selon le niveau de zoom afin de garder des temps de lecture fluides.

## Test local

```sh
make map-test
```

Les tests utilisent le petit GeoJSON de fixture du dépôt, sans trace personnelle.

## Attribution

Si les données proviennent d'OpenStreetMap, afficher dans l'appareil ou sa documentation : **© contributeurs OpenStreetMap**, données sous licence **ODbL**. Vérifier les conditions de redistribution de l'extrait utilisé.

## Étape suivante

1. Ajouter latitude/longitude à l'échantillon GNSS de l'application, sans dépendre d'un GPX.
2. Ajouter un rendu vectoriel au fond de la page navigation, derrière la trace et la flèche de position.
3. Faire suivre la carte à la position GPS, avec zoom et mode nord/trace devant.
4. Brancher la lecture de `*.gvm` depuis la microSD SDMMC (GPIO 10/9/11, selon la révision matérielle à confirmer).
