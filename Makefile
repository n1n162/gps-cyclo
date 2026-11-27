# Tests sur ordinateur du code commun (montees, GPX, positionnement, FIT, noms de cols).
# make test s'appuie sur un jeu fictif genere par tests/make_synthetique.py : aucun fichier personnel n'est necessaire.
# Les cibles ui et nommage utilisent encore tests/data/sortie.gpx (non publie, voir README).
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Icore
BUILD = build
DATA = tests/data
SYN = tests/data/synthetique

.PHONY: all test ui sim nommage map-test gvt-test cols-format-test splash-test clean
all: test

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/test_climb: tests/test_climb.c core/climb.c core/route.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_cols: tests/test_cols.c core/cols.c core/climb.c core/route.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_route: tests/test_route.c core/route.c core/matcher.c core/climb.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_fit: tests/test_fit.c core/fit.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_sim: tests/test_sim.c sim/sim_engine.c core/route.c core/matcher.c core/climb.c | $(BUILD)
	$(CC) $(CFLAGS) -Isim $^ -lm -o $@
$(BUILD)/test_alerts: tests/test_alerts.c core/alerts.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_guidance: tests/test_guidance.c core/guidance.c core/route.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@
$(BUILD)/test_cols_format: tests/test_cols_format.c core/cols.c core/cols.h core/route.c core/climb.c | $(BUILD)
	$(CC) $(CFLAGS) tests/test_cols_format.c core/cols.c core/route.c core/climb.c -lm -o $@

cols-format-test: $(BUILD)/test_cols_format
	$(BUILD)/test_cols_format

$(BUILD)/name_real: tests/name_real.c core/cols.c core/climb.c core/route.c | $(BUILD)
	$(CC) $(CFLAGS) $^ -lm -o $@

$(BUILD)/test_map_data: tests/test_map_data.c core/map_data.c core/map_data.h | $(BUILD)
	$(CC) $(CFLAGS) tests/test_map_data.c core/map_data.c -o $@

$(BUILD)/fixture.gvm: tests/data/fixture_overpass.geojson tools/osm_to_gvm.py | $(BUILD)
	python3 tools/osm_to_gvm.py $< $@

map-test: $(BUILD)/test_map_data $(BUILD)/fixture.gvm
	$(BUILD)/test_map_data $(BUILD)/fixture.gvm

# Carte vectorielle en tuiles GVT1 (docs/carte.md) : donnees synthetiques, decoupage, verification Python, lecteur C.
# Le point (lat 45.20, lon 5.00) est le premier sommet de l'autoroute synthetique.
$(BUILD)/test_gvt: tests/test_gvt.c core/gvt.c core/gvt.h | $(BUILD)
	$(CC) $(CFLAGS) tests/test_gvt.c core/gvt.c -lm -o $@

$(BUILD)/synthetic.geojson: tests/make_gvt_fixture.py | $(BUILD)
	python3 tests/make_gvt_fixture.py $@

$(BUILD)/synthetic.gvt: $(BUILD)/synthetic.geojson tests/geojson_to_gvt_fixture.py
	python3 tests/geojson_to_gvt_fixture.py $< $@

$(BUILD)/test_polyfill: tests/test_polyfill.c core/polyfill.c core/polyfill.h | $(BUILD)
	$(CC) $(CFLAGS) -Icore tests/test_polyfill.c core/polyfill.c -lm -o $@

$(BUILD)/test_linedraw: tests/test_linedraw.c core/linedraw.c core/linedraw.h | $(BUILD)
	$(CC) $(CFLAGS) -Icore tests/test_linedraw.c core/linedraw.c -lm -o $@

$(BUILD)/test_mapview: tests/test_mapview.c core/mapview.c core/mapview.h core/gvt.c core/route.c | $(BUILD)
	$(CC) $(CFLAGS) -Icore tests/test_mapview.c core/mapview.c core/gvt.c core/route.c -lm -o $@

gvt-test: $(BUILD)/test_gvt $(BUILD)/test_mapview $(BUILD)/test_polyfill $(BUILD)/test_linedraw $(BUILD)/synthetic.gvt
	python3 tests/verify_gvt.py $(BUILD)/synthetic.geojson $(BUILD)/synthetic.gvt
	$(BUILD)/test_gvt $(BUILD)/synthetic.gvt 45.20 5.00
	$(BUILD)/test_mapview $(BUILD)/synthetic.gvt
	$(BUILD)/test_polyfill
	$(BUILD)/test_linedraw

$(DATA)/sortie.gpx:
	@echo "Place un GPX de test (avec heures et frequence cardiaque) dans $(DATA)/sortie.gpx"; exit 1

$(DATA)/replay.csv $(DATA)/replay2.csv: $(DATA)/sortie.gpx tests/make_replay.py
	cd $(DATA) && python3 ../make_replay.py sortie.gpx

# Jeu de test fictif (position en plein Atlantique, 66 km, 5 montees) et base de cols fabriquee.
$(SYN)/synthetique.gpx $(SYN)/cols.csv: tests/make_synthetique.py
	mkdir -p $(SYN) && cd $(SYN) && python3 ../../make_synthetique.py

$(SYN)/replay.csv $(SYN)/replay2.csv: $(SYN)/synthetique.gpx tests/make_replay.py
	cd $(SYN) && python3 ../../make_replay.py synthetique.gpx

test: cols-format-test $(BUILD)/test_sim $(BUILD)/test_alerts $(BUILD)/test_guidance $(BUILD)/test_climb $(BUILD)/test_cols $(BUILD)/test_route $(BUILD)/test_fit $(SYN)/replay.csv $(SYN)/replay2.csv $(SYN)/cols.csv
	$(BUILD)/test_alerts
	$(BUILD)/test_guidance
	cd $(SYN) && ../../../$(BUILD)/test_sim synthetique.gpx
	cd $(SYN) && ../../../$(BUILD)/test_climb synthetique.gpx
	cd $(SYN) && ../../../$(BUILD)/test_cols synthetique.gpx cols.csv
	cd $(SYN) && ../../../$(BUILD)/test_route synthetique.gpx
	cd $(SYN) && ../../../$(BUILD)/test_fit
	cd $(SYN) && python3 ../../verify_fit.py

# Simulation d'une sortie sur PC (aperçu des écrans, ouverture automatique de la montée, toucher). Télécharge LVGL 9.2.2.
ui: $(DATA)/replay2.csv
	sh ui/build_preview.sh h && sh ui/build_preview.sh a && sh ui/build_preview.sh b && sh ui/build_preview.sh c && sh ui/build_preview.sh f && sh ui/build_preview.sh g

# Simulateur : une fenetre avec l'appareil et un tableau de commandes. Necessite SDL2 (Mac : brew install sdl2).
# Exemple : make sim GPX=~/Desktop/sortie.gpx COLS=~/Desktop/gps.csv CARTE=~/Documents/carte.gvt
sim:
	@test -n "$(GPX)" || { echo "Usage : make sim GPX=chemin/sortie.gpx [RIDE=chemin/autre_sortie.gpx] [COLS=chemin/gps.csv] [CARTE=chemin/carte.gvt] [OPTS=--zoom 2]"; exit 1; }
	sh sim/build_sim.sh "$(GPX)" $(if $(RIDE),"$(RIDE)") $(if $(COLS),"$(COLS)") $(if $(CARTE),--carte "$(CARTE)") $(OPTS)

# Nommage avec une vraie base de cols (tests/data/gps.csv, export OpenStreetMap converti)
nommage: $(BUILD)/name_real
	cd $(DATA) && ../../$(BUILD)/name_real filtre 0

clean:
	rm -rf $(BUILD) $(DATA)/ride*.fit $(DATA)/replay*.csv $(SYN)


# Tests unitaires du convertisseur d'image de l'ecran de demarrage (necessite Pillow).
splash-test:
	python3 tests/test_splash_to_c.py
