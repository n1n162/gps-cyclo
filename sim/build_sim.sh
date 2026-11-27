#!/bin/sh
# Simulateur du GPS velo sur ordinateur. Usage : sh sim/build_sim.sh parcours.gpx [sortie.gpx] [cols.csv] [--carte carte.gvt] [--zoom 2]
# Prerequis : gcc ou clang, git, et SDL2 (Mac : brew install sdl2 ; Linux : libsdl2-dev).
set -e
cd "$(dirname "$0")"
HERE=$(pwd)
command -v sdl2-config >/dev/null 2>&1 || { echo "SDL2 est absent. Sur Mac : brew install sdl2"; exit 1; }
LVGL_TAG=v9.2.2
[ -d lvgl ] || git clone --depth 1 --branch "$LVGL_TAG" https://github.com/lvgl/lvgl.git lvgl
mkdir -p build/obj
command -v python3 >/dev/null 2>&1 || { echo "Python 3 est absent."; exit 1; }
python3 -c "import PIL" 2>/dev/null || { echo "Pillow est absent. Installer : python3 -m pip install pillow"; exit 1; }
python3 ../tools/splash_to_c.py "../docs/ecran acceuil.jpg" "../ui/splash_img.c"
SDLC=$(sdl2-config --cflags)
SDLL=$(sdl2-config --libs)
if [ ! -f build/liblvgl.a ]; then
  echo "Compilation de LVGL (une seule fois, environ une minute)..."
  for f in $(find lvgl/src -name '*.c'); do
    o=build/obj/$(echo "$f" | tr '/' '_' | sed 's/\.c$/.o/')
    [ -f "$o" ] || cc -O1 -w -I. -Ilvgl -DLV_CONF_INCLUDE_SIMPLE $SDLC -c "$f" -o "$o"
  done
  ar rcs build/liblvgl.a build/obj/*.o
fi
cc -O1 -Wall -Wextra -I. -Ilvgl -I../core -I../ui -DLV_CONF_INCLUDE_SIMPLE $SDLC \
   sim_main.c sim_engine.c ../ui/app.c ../ui/splash.c ../ui/splash_img.c ../core/alerts.c ../core/cols.c ../core/climb.c ../core/route.c ../core/matcher.c ../core/guidance.c ../core/gvt.c ../core/mapview.c ../core/polyfill.c ../core/linedraw.c \
   ../ui/fonts/ui_font_*.c build/liblvgl.a $SDLL -lm -o build/gps_sim
# les chemins relatifs donnes par l'utilisateur sont relatifs au dossier ou il a lance la commande
cd "$OLDPWD"
exec "$HERE/build/gps_sim" "$@"
