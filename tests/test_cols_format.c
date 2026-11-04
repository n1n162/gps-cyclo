/* Test de la lecture de gps.csv (core/cols.c) : format d'origine, colonnes supplementaires (identifiant OpenStreetMap),
 * en-tete qui nomme les colonnes, formats non reconnus signales. Sans fichier de donnees. */
#include "cols.h"
#include <stdio.h>
#include <string.h>

static int fails;
typedef struct { const char *s; size_t pos; } mem_t;
static size_t rd_mem(void *ctx, char *buf, size_t max)
{
    mem_t *m = ctx;
    size_t left = strlen(m->s) - m->pos, k = left < max ? left : max;
    memcpy(buf, m->s + m->pos, k);
    m->pos += k;
    return k;
}

typedef struct { const char *name, *csv; int n, rejected; const char *first; int ele; } tcase_t;

int main(void)
{
    static const tcase_t cases[] = {
        {"format d'origine avec en-tete", "type;nom;lat;lon;alt\ncol;Col de la Croix-Haute;44.6;5.7;1179\nlieu;Saint-Etienne;45.1;5.2;\n", 2, 0, "Col de la Croix-Haute", 1179},
        {"fins de ligne Windows", "type;nom;lat;lon;alt\r\ncol;Col A;45.0;5.0;900\r\n", 1, 0, "Col A", 900},
        {"identifiant OpenStreetMap en fin de ligne", "col;Col B;45.1;5.1;1200;123456789\n", 1, 0, "Col B", 1200},
        {"identifiant devant le type, sans en-tete", "123456789;col;Col C;45.2;5.2;1300\n", 1, 0, "Col C", 1300},
        {"identifiant devant, avec en-tete", "id;type;nom;lat;lon;alt\n42;col;Col D;45.3;5.3;1400\n", 1, 0, "Col D", 1400},
        {"identifiant au milieu, en-tete qui nomme les colonnes", "type;nom;id;lat;lon;alt\ncol;Col E;987;45.4;5.4;1500\n", 1, 0, "Col E", 1500},
        {"autres noms de colonnes", "osm_id;type;name;latitude;longitude;ele\n7;lieu;Lieu F;45.5;5.5;600\n", 1, 0, "Lieu F", 600},
        {"altitude absente", "col;Col G;45.6;5.6;\ncol;Col H;45.7;5.7\n", 2, 0, "Col G", COLS_NO_ELE},
        {"commentaire, ligne parasite et ligne sans coordonnees", "# commentaire\nn'importe quoi\ncol;Sans coordonnees;;;\ncol;Col I;45.8;5.8;100\n", 1, 1, "Col I", 100},
        {"identifiant entre le nom et la latitude, sans en-tete : non devinable, signale", "col;Col J;555;45.9;5.9;1000\n", 0, 1, NULL, 0},
        {"type inconnu : signale", "route;Route K;45.0;5.0;100\n", 0, 1, NULL, 0},
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        cols_entry_t e[8];
        char pool[512];
        cols_db_t d;
        mem_t m = { cases[i].csv, 0 };
        int n, ok;
        cols_init(&d, e, 8, pool, sizeof pool);
        n = cols_load(rd_mem, &m, &d);
        ok = n == cases[i].n && d.rejected == cases[i].rejected &&
             (!cases[i].first || (n > 0 && strcmp(cols_name(&d, 0), cases[i].first) == 0 && d.e[0].ele == cases[i].ele));
        printf("  %-76s %s (%d entrees, %d non reconnues)\n", cases[i].name, ok ? "OK" : "ECHEC", n, d.rejected);
        fails += !ok;
    }
    {   /* autres types du script convertir_cols.py : reconnus, ignores, jamais signales comme format inconnu */
        static const char *csv = "eau;Fontaine;45.1;5.1;\nboulangerie;Boulangerie Dupont;45.2;5.2;\ncommerce;Super U;45.3;5.3;\n"
                                 "toilette;WC du lac;45.4;5.4;\nvelociste;Velo Plus;45.5;5.5;\n123;eau;Fontaine avec id devant;45.6;5.6;\n"
                                 "col;Col M;45.7;5.7;900\nroute;Type inconnu;45.8;5.8;\n";
        cols_entry_t e[8];
        char pool[512];
        cols_db_t d;
        mem_t m = { csv, 0 };
        int ok;
        cols_init(&d, e, 8, pool, sizeof pool);
        cols_load(rd_mem, &m, &d);
        ok = d.n == 1 && d.other == 6 && d.rejected == 1;
        printf("  autres types reconnus : %d col, %d autres, %d non reconnue (type inconnu) %s\n", d.n, d.other, d.rejected, ok ? "OK" : "ECHEC");
        fails += !ok;
    }
    {   /* coordonnees lues au bon endroit quand une colonne s'ajoute devant */
        cols_entry_t e[2];
        char pool[128];
        cols_db_t d;
        mem_t m = { "99;col;Col L;45.123;5.456;777\n", 0 };
        cols_init(&d, e, 2, pool, sizeof pool);
        cols_load(rd_mem, &m, &d);
        printf("  coordonnees lues avec une colonne devant : %.3f %.3f %s\n", (double)d.e[0].lat, (double)d.e[0].lon,
               (d.e[0].lat > 45.122f && d.e[0].lat < 45.124f && d.e[0].lon > 5.455f && d.e[0].lon < 5.457f) ? "OK" : "ECHEC");
        fails += !(d.e[0].lat > 45.122f && d.e[0].lat < 45.124f && d.e[0].lon > 5.455f && d.e[0].lon < 5.457f);
    }
    if (fails) { printf("%d ECHEC(S)\n", fails); return 1; }
    printf("OK lecture de gps.csv : format d'origine, colonnes supplementaires, en-tetes, formats non reconnus signales\n");
    return 0;
}
