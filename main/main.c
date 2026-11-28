#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "gvt.h"
#include "sd_card.h"

#define GVT_FILE_PATH SD_CARD_MOUNT_POINT "/france.gvt"

static const char *TAG = "gps_cyclo";
static gvt_t gvt;

typedef struct {
    FILE *file;
    uint32_t size;
} gvt_file_t;

/* Interface de lecture attendue par le coeur portable GVT1. */
static int gvt_file_read(void *ctx, uint32_t offset, void *dst, uint32_t len)
{
    gvt_file_t *source = (gvt_file_t *)ctx;

    if (source == NULL || source->file == NULL || dst == NULL ||
        offset > source->size || len > source->size - offset ||
        (uint64_t)offset > (uint64_t)LONG_MAX) {
        return -1;
    }

    if (fseek(source->file, (long)offset, SEEK_SET) != 0) {
        return -1;
    }
    return fread(dst, 1, len, source->file) == len ? 0 : -1;
}

void app_main(void)
{
    sdmmc_card_t *card = NULL;
    FILE *file = NULL;

    esp_err_t err = sd_card_mount(&card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Demarrage interrompu : verifier la microSD et son format FAT");
        return;
    }

    file = fopen(GVT_FILE_PATH, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Fichier absent : %s", GVT_FILE_PATH);
        goto cleanup;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        ESP_LOGE(TAG, "Impossible de mesurer le fichier GVT1");
        goto cleanup;
    }

    long file_size = ftell(file);
    if (file_size <= 0 || (uint64_t)file_size > UINT32_MAX) {
        ESP_LOGE(TAG, "Taille GVT1 invalide ou superieure a 4 Go");
        goto cleanup;
    }

    gvt_file_t source = {
        .file = file,
        .size = (uint32_t)file_size,
    };

    if (gvt_open(&gvt, gvt_file_read, &source, source.size) != 0) {
        ESP_LOGE(TAG, "En-tete ou index GVT1 invalide");
        goto cleanup;
    }

    ESP_LOGI(TAG, "GVT1 ouvert : %lu octets, %lu tuiles, zoom %u-%u, version %u",
             (unsigned long)source.size,
             (unsigned long)gvt.n_tiles,
             (unsigned)gvt.zmin,
             (unsigned)gvt.zmax,
             (unsigned)gvt.version);

    /* Lecture de l'index depuis la SD pour valider aussi le chemin de lecture. */
    uint32_t x, y;
    float fx, fy;
    double center_lat = ((double)gvt.south_e7 + (double)gvt.north_e7) / 2.0e7;
    double center_lon = ((double)gvt.west_e7 + (double)gvt.east_e7) / 2.0e7;
    gvt_entry_t entry;

    gvt_tile_of(gvt.zmax, center_lat, center_lon, &x, &y, &fx, &fy);
    int found = gvt_find_entry(&gvt, gvt.zmax, x, y, &entry);
    if (found < 0) {
        ESP_LOGE(TAG, "Erreur de lecture de l'index GVT1 sur microSD");
    } else if (found == 1) {
        ESP_LOGI(TAG, "Index SD lu : tuile %u/%lu/%lu, %lu octets",
                 (unsigned)gvt.zmax, (unsigned long)x, (unsigned long)y,
                 (unsigned long)entry.size);
    } else {
        ESP_LOGW(TAG, "Aucune tuile au centre de l'emprise (le fichier reste ouvert)");
    }

cleanup:
    if (file != NULL) {
        fclose(file);
    }
    if (card != NULL) {
        esp_err_t unmount_err = sd_card_unmount(card);
        if (unmount_err != ESP_OK) {
            ESP_LOGE(TAG, "Erreur au demontage microSD : %s", esp_err_to_name(unmount_err));
        }
    }
}
