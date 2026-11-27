#!/bin/sh
# Apercu de la page trace avec fond de carte, sans GPX personnel : carte de demonstration (petite ville fictive avec zones
# colorees, themes sombre et clair) et carte de test (la trace suit l'autoroute synthetique). Pour chaque cadre : image avec
# trace et image de la carte seule. Affiche aussi combien de fois la carte est redessinee pendant une sortie simulee.
# Necessite git, gcc, python3 avec Pillow. Captures ecrites dans ui/build/apercu_carte/.
set -e
cd "$(dirname "$0")"
LVGL_TAG=v9.2.2
[ -d lvgl ] || git clone --depth 1 --branch "$LVGL_TAG" https://github.com/lvgl/lvgl.git lvgl
mkdir -p build/obj
if [ ! -f build/liblvgl.a ]; then
  for f in $(find lvgl/src -name '*.c'); do
    o=build/obj/$(echo "$f" | tr '/' '_' | sed 's/\.c$/.o/')
    [ -f "$o" ] || gcc -O1 -w -I. -Ilvgl -DLV_CONF_INCLUDE_SIMPLE -c "$f" -o "$o"
  done
  ar rcs build/liblvgl.a build/obj/*.o
fi
python3 ../tests/make_gvt_demo.py build/demo.geojson
python3 ../tests/geojson_to_gvt_fixture.py build/demo.geojson build/demo.gvt > /dev/null
python3 ../tests/make_gvt_fixture.py build/synthetic.geojson
python3 ../tools/osm_to_gvt.py build/synthetic.geojson build/synthetic.gvt > /dev/null
gcc -O1 -Wall -Wextra -I. -Ilvgl -I../core -DLV_CONF_INCLUDE_SIMPLE app.c map_preview.c \
    ../core/alerts.c ../core/cols.c ../core/climb.c ../core/route.c ../core/matcher.c ../core/guidance.c \
    ../core/gvt.c ../core/mapview.c ../core/polyfill.c ../core/linedraw.c fonts/ui_font_*.c build/liblvgl.a -lm -o build/map_preview
mkdir -p build/apercu_carte
cd build/apercu_carte
rm -f screen_*.rgb565 apercu_*.png
../map_preview ../demo.gvt demo sombre
../map_preview ../demo.gvt demo clair
../map_preview ../synthetic.gvt autoroute sombre
python3 ../../rgb565_to_png.py
