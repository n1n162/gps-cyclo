#include "sd_card.h"

#include <stdio.h>

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

#define SD_PIN_CMD GPIO_NUM_10
#define SD_PIN_D0  GPIO_NUM_9
#define SD_PIN_CLK GPIO_NUM_11

static const char *TAG = "sd_card";

esp_err_t sd_card_mount(sdmmc_card_t **out_card)
{
    if (out_card == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_card = NULL;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = SD_PIN_CLK;
    slot.cmd = SD_PIN_CMD;
    slot.d0 = SD_PIN_D0;
    slot.d1 = GPIO_NUM_NC;
    slot.d2 = GPIO_NUM_NC;
    slot.d3 = GPIO_NUM_NC;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };

    esp_err_t err = esp_vfs_fat_sdmmc_mount(
        SD_CARD_MOUNT_POINT, &host, &slot, &mount_config, out_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Impossible de monter la microSD (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "microSD montee");
    sdmmc_card_print_info(stdout, *out_card);
    return ESP_OK;
}

esp_err_t sd_card_unmount(sdmmc_card_t *card)
{
    if (card == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_vfs_fat_sdcard_unmount(SD_CARD_MOUNT_POINT, card);
}
