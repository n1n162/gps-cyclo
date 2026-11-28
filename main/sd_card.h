#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_err.h"
#include "sdmmc_cmd.h"

#define SD_CARD_MOUNT_POINT "/sdcard"

esp_err_t sd_card_mount(sdmmc_card_t **out_card);
esp_err_t sd_card_unmount(sdmmc_card_t *card);

#endif
