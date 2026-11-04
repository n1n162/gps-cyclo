#include "map_data.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "ECHEC carte: %s\n", m); return 1; } } while (0)

int main(int argc, char **argv)
{
    map_reader_t r;
    map_feature_t f;
    map_point_t pts[MAP_FEATURE_MAX_POINTS];
    map_bounds_t b = {456000000, 457000000, 57000000, 58000000};
    int n = 0, result;
    CHECK(argc == 2, "usage: test_map_data carte.gvm");
    CHECK(map_reader_open(&r, argv[1]) == 0, "ouverture GVM1");
    CHECK(r.feature_count > 0, "carte non vide");
    while ((result = map_reader_next_visible(&r, &b, &f, pts, MAP_FEATURE_MAX_POINTS)) > 0) {
        CHECK(f.n_points > 0, "géométrie avec points");
        CHECK(f.type >= MAP_FEATURE_ROAD && f.type <= MAP_FEATURE_POI, "type connu");
        for (unsigned i = 0; i < f.n_points; i++) {
            CHECK(pts[i].lat_e7 >= 450000000 && pts[i].lat_e7 <= 460000000, "latitude plausible");
            CHECK(pts[i].lon_e7 >= 50000000 && pts[i].lon_e7 <= 60000000, "longitude plausible");
        }
        n++;
    }
    CHECK(result == 0, "fin de fichier sans erreur");
    CHECK(n > 0, "objets dans la zone visible");
    map_reader_close(&r);
    puts("OK carte : format GVM1, lecture en flux, filtrage géographique");
    return 0;
}
