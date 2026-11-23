# Guidage OSM et format GCR1

## Statut

Le branchement GPX vers OpenStreetMap et le format de guidage compact GCR1 sont implémentés côté outils et code commun. Les tests ont été rejoués sur le Mac de Nicolas le 23 novembre 2026 avec le commit de cette branche.

Cette fonctionnalité prépare le guidage hors ligne : elle n'est pas encore chargée par `ui/app.c` ni reliée à la position GPS en temps réel sur le matériel.

## Pipeline

1. Télécharger ou fournir un extrait OpenStreetMap en JSON.
2. Faire correspondre un parcours GPX au graphe routier.
3. Produire une route GeoJSON au format `GPS-CYCLO-ROUTE-1`.
4. Compacter cette route dans le format `GCR1` pour la microSD.
5. Lire ce fichier avec le lecteur C commun `core/osm_guidance.c`.

```text
GPX + JSON OSM
  -> tools/gpx_to_osm_route.py
  -> route GeoJSON GPS-CYCLO-ROUTE-1
  -> tools/route_to_gcr1.py
  -> fichier GCR1
  -> core/osm_guidance.c
```

## Scripts

| Fichier | Rôle |
|---|---|
| `tools/gpx_to_osm_route.py` | Calage du GPX sur un graphe OSM connecté, contrôle des sens uniques et génération des intersections |
| `tools/route_to_gcr1.py` | Conversion d'une GeoJSON GPS-CYCLO-ROUTE-1 vers GCR1 |
| `core/osm_guidance.h`, `core/osm_guidance.c` | Chargement et parcours des manoeuvres GCR1 en C |
| `tests/test_gpx_to_osm_route.py` | Droite, composants déconnectés et sens unique |
| `tests/test_route_to_gcr1.py` | Mise en forme binaire GCR1 |
| `tests/test_osm_guidance.c` | Lecture C, comptage et sélection temporelle |

## Utilisation

Pour obtenir une route OSM depuis un GPX :

```sh
python3 tools/gpx_to_osm_route.py sortie.gpx osm.json --out route.geojson
```

Pour produire le fichier compact :

```sh
python3 tools/route_to_gcr1.py route.geojson route.gcr1
```

Les scripts exigent uniquement la bibliothèque standard Python.

## Format GCR1

Petit-boutiste :

| Décalage | Taille | Champ |
|---|---:|---|
| 0 | 4 | signature `GCR1` |
| 4 | 4 | nombre de manoeuvres (`uint32`) |

Chaque manoeuvre commence par :

| Décalage relatif | Taille | Champ |
|---|---:|---|
| 0 | 1 | type (`uint8`) |
| 1 | 4 | distance depuis le départ en mètres (`uint32`) |
| 5 | 4 | latitude en float32 |
| 9 | 4 | longitude en float32 |
| 13 | 2 | longueur du nom de départ UTF-8 (`uint16`) |
| 15 | 2 | longueur du nom d'arrivée UTF-8 (`uint16`) |
| 17 | variable | nom de départ puis nom d'arrivée |

Types actuels : ligne droite, léger gauche, gauche, léger droite, droite, demi-tour, rond-point, intersection et arrivée.

## Tests

```sh
make gcr1-test
make test
```

`make gcr1-test` lance :

- le lecteur C GCR1 ;
- le test de conversion route GeoJSON vers GCR1 ;
- les trois tests GPX vers route OSM.

Les deux commandes ont été réussies sur le Mac de Nicolas le 23 novembre 2026.

## Limites actuelles

- Le map-matching suppose des données OSM géométriques utilisables et des ancres à moins de 20 m par défaut dans les tests.
- Les noms sont tronqués à 63 octets lors de la conversion UTF-8.
- Le format ne contient pas encore les arcs complets de toutes les routes ni une topologie embarquée plus riche.
- Aucun affichage, chargement microSD ou mise à jour avec la position GPS n'est encore intégré à `ui/app.c`.
