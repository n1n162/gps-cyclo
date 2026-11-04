#!/usr/bin/env python3
"""Convertit un fichier OpenStreetMap (.osm.pbf) en fichier de base pour le GPS : type;nom;lat;lon;alt

Usage : python3 convertir_cols.py france-latest.osm.pbf gps.csv [types]

  types (facultatif) : liste separee par des virgules parmi col, lieu, eau, boulangerie, commerce, toilette, velociste,
  ou "tous". Sans cet argument, un menu demande les types a extraire.

Si gps.csv existe deja, les nouveaux points y sont ajoutes (les lignes existantes peuvent avoir un identifiant devant le type
ou des colonnes en plus : ils sont acceptes, mais les colonnes en plus ne sont pas conservees)."""
import math
import os
import re
import sys
import osmium

# Définition des catégories et des tags OSM correspondants
CATEGORIES = {
    "col": {
        "name": "Cols",
        "check": lambda tags: (
            tags.get("mountain_pass") == "yes"
            or tags.get("natural") == "saddle"
        ),
    },
    "lieu": {
        "name": "Lieux / Localités",
        "check": lambda tags: tags.get("place") in {
            "village",
            "hamlet",
            "town",
            "suburb",
            "locality",
            "isolated_dwelling",
        },
    },
    "eau": {
        "name": "Points d'eau potable",
        "check": lambda tags: tags.get("amenity") == "drinking_water",
    },
    "boulangerie": {
        "name": "Boulangeries",
        "check": lambda tags: tags.get("shop") == "bakery",
    },
    "commerce": {
        "name": "Commerces (alimentation, supermarchés...)",
        "check": lambda tags: tags.get("shop")
        in {
            "convenience",
            "supermarket",
            "general",
            "department_store",
            "greengrocer",
            "butcher",
        },
    },
    "toilette": {
        "name": "Toilettes publiques",
        "check": lambda tags: tags.get("amenity") == "toilets",
    },
    "velociste": {
        "name": "Vélostistes (réparation/vente vélo)",
        "check": lambda tags: tags.get("shop") == "bicycle",
    },
}


def parse_ele(v):
  if v is None:
    return ""
  m = re.search(r"-?\d+(?:[.,]\d+)?", str(v).replace(" ", ""))
  if not m:
    return ""
  x = float(m.group(0).replace(",", "."))
  return str(int(round(x))) if -500 < x < 9000 else ""


class OSMHandler(osmium.SimpleHandler):

  def __init__(self, selected_kinds):
    super().__init__()
    self.selected_kinds = selected_kinds
    self.rows = []
    self.skipped = 0

  def node(self, n):
    if not n.tags:                       # la grande majorite des noeuds n'a aucun tag : on sort tout de suite
      return
    if not n.location.valid():
      self.skipped += 1
      return
    tags = {tag.k: tag.v for tag in n.tags}
    name = " ".join((tags.get("name") or "").replace(";", ",").split())      # sans ';' ni retour a la ligne
    if not name:
      self.skipped += 1
      return

    matched_kind = None
    for kind in self.selected_kinds:
      if CATEGORIES[kind]["check"](tags):
        matched_kind = kind
        break

    if matched_kind:
      self.rows.append([
          matched_kind,
          name,
          round(n.location.lat, 6),
          round(n.location.lon, 6),
          parse_ele(tags.get("ele")),
      ])
    else:
      self.skipped += 1


def metres(la1, lo1, la2, lo2):
  dy = (la2 - la1) * 111320.0
  dx = (lo2 - lo1) * 111320.0 * math.cos(math.radians((la1 + la2) / 2))
  return math.hypot(dx, dy)


def main():
  if len(sys.argv) not in (3, 4):
    raise SystemExit(
        "Usage : python3 convertir_cols.py <fichier.osm.pbf> <fichier.csv> [types]"
    )

  pbf_file = sys.argv[1]
  out_file = sys.argv[2]

  keys_list = list(CATEGORIES.keys())
  if len(sys.argv) == 4:                                   # types donnes en argument : pas de menu
    wanted = [w.strip() for w in sys.argv[3].split(",") if w.strip()]
    if wanted == ["tous"]:
      selected_kinds = keys_list
    else:
      unknown = [w for w in wanted if w not in CATEGORIES]
      if unknown or not wanted:
        raise SystemExit(
            "Types inconnus : %s. Types possibles : %s, tous"
            % (", ".join(unknown) or "(aucun)", ", ".join(keys_list))
        )
      selected_kinds = [k for k in keys_list if k in wanted]
  else:
    # --- Menu interactif ---
    print("=== Sélection des types à extraire ===")
    for i, k in enumerate(keys_list, 1):
      print(f"{i}. {CATEGORIES[k]['name']}")
    print(f"{len(keys_list) + 1}. Tous les types")

    choice = input(
        f"Entrez les numéros séparés par des virgules (ex: 1,3,4) ou"
        f" '{len(keys_list) + 1}' pour tout [défaut: tous] : "
    ).strip()

    if not choice or choice == str(len(keys_list) + 1):
      selected_kinds = keys_list
    else:
      selected_kinds = []
      for part in choice.split(","):
        part = part.strip()
        if part.isdigit():
          idx = int(part) - 1
          if 0 <= idx < len(keys_list):
            selected_kinds.append(keys_list[idx])
      if not selected_kinds:
        selected_kinds = keys_list

  print(
      "-> Types sélectionnés :"
      f" {', '.join([CATEGORIES[k]['name'] for k in selected_kinds])}"
  )

  # --- Chargement du fichier existant (mode incrémentiel) ---
  existing_rows = []
  existing_keys = set()
  unreadable = 0
  if os.path.exists(out_file):
    print(
        f"Fichier existant détecté ({out_file}). Chargement des données"
        " actuelles..."
    )
    with open(out_file, "r", encoding="utf-8") as f:
      for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
          continue
        parts = line.split(";")
        # le type est dans l'une des trois premieres colonnes (un identifiant peut le preceder)
        p = next((i for i in range(min(3, len(parts))) if parts[i] in CATEGORIES), None)
        try:
          if p is None or len(parts) < p + 4:
            raise ValueError
          kind, name = parts[p], parts[p + 1]
          lat, lon = float(parts[p + 2]), float(parts[p + 3])
        except ValueError:
          unreadable += 1                       # en-tete, ligne abimee : ignoree
          continue
        alt = parts[p + 4] if len(parts) > p + 4 else ""
        existing_rows.append([kind, name, lat, lon, alt])
        existing_keys.add((kind, name, lat, lon))
    print(f"-> {len(existing_rows)} éléments déjà présents en mémoire.")
    if unreadable:
      print(f"-> {unreadable} lignes illisibles ignorées (elles seront perdues à la réécriture).")

  print("Lecture du fichier OpenStreetMap (.pbf)...")
  handler = OSMHandler(selected_kinds)
  handler.apply_file(pbf_file)

  # --- Traitement des doublons (fusion des cols de même nom à moins de 300 m, y compris avec ceux déjà présents) ---
  cols = [r for r in handler.rows if r[0] == "col"]
  other_rows = [r for r in handler.rows if r[0] != "col"]
  by_name = {}
  for r in existing_rows:
    if r[0] == "col":
      by_name.setdefault(r[1], []).append(r)
  keep, dropped = [], 0
  for r in cols:
    dup = next(
        (k for k in by_name.get(r[1], []) if metres(k[2], k[3], r[2], r[3]) < 300),
        None,
    )
    if dup is None:
      keep.append(r)
      by_name.setdefault(r[1], []).append(r)
    else:
      dropped += 1
      if not dup[4] and r[4]:
        dup[4] = r[4]

  new_parsed = keep + other_rows

  # --- Filtrage pour n'ajouter que les nouveautés ---
  added_count = 0
  truly_new_rows = []
  for r in new_parsed:
    key = (r[0], r[1], r[2], r[3])
    if key not in existing_keys:
      truly_new_rows.append(r)
      existing_keys.add(key)
      added_count += 1

  # Fusion avec l'existant et tri global par coordonnées
  out = existing_rows + truly_new_rows
  out.sort(key=lambda r: (r[2], r[3]))

  # --- Écriture du fichier final ---
  with open(out_file, "w", encoding="utf-8", newline="\n") as f:
    f.write(
        "# type;nom;lat;lon;alt - donnees (c) les contributeurs"
        " d'OpenStreetMap, licence ODbL\n"
    )
    for r in out:
      f.write("%s;%s;%s;%s;%s\n" % tuple(r))

  # Statistiques globales
  stats = {}
  for r in out:
    stats[r[0]] = stats.get(r[0], 0) + 1
  stats_str = ", ".join([f"{v} {k}(s)" for k, v in stats.items()])

  print(
      f"Terminé ! Bilan total : {stats_str} ({added_count} nouveaux"
      f" éléments ajoutés, {dropped} doublons de cols fusionnés,"
      f" {handler.skipped} points ignorés : sans nom ou sans coordonnées)."
  )


if __name__ == "__main__":
  main()
