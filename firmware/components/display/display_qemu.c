/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "display.h"

#include "sdkconfig.h"

#if CONFIG_GPS_PLATFORM_QEMU

#include <stdint.h>
#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_qemu_rgb.h"
#include "esp_log.h"

static const char *TAG = "display";

static esp_lcd_panel_handle_t panel;
static uint16_t *framebuffer;

esp_err_t display_start(void)
{
    ESP_LOGI(
        TAG,
        "Creating QEMU RGB display %dx%d RGB565",
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT);

    const esp_lcd_rgb_qemu_config_t config = {
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT,
        .bpp = RGB_QEMU_BPP_16,
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_rgb_qemu(&config, &panel),
        TAG,
        "Failed to create QEMU RGB panel");

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_reset(panel),
        TAG,
        "Failed to reset QEMU RGB panel");

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_init(panel),
        TAG,
        "Failed to initialize QEMU RGB panel");

    void *fb = NULL;
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_qemu_get_frame_buffer(
            panel,
            &fb),
        TAG,
        "Failed to get QEMU framebuffer");

    if (fb == NULL) {
        ESP_LOGE(TAG, "QEMU framebuffer is NULL");
        return ESP_FAIL;
    }
    framebuffer = fb;

    memset(framebuffer, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));

    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_qemu_refresh(panel),
        TAG,
        "Failed to refresh QEMU display");

    ESP_LOGI(TAG, "QEMU display ready");

    return ESP_OK;
}

esp_err_t display_draw_bitmap(int x_start, int y_start, int x_end, int y_end,
                              const void *pixels)
{
    if (panel == NULL || framebuffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (pixels == NULL || x_start < 0 || y_start < 0 ||
        x_start >= x_end || y_start >= y_end ||
        x_end > DISPLAY_WIDTH || y_end > DISPLAY_HEIGHT) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint16_t *source = pixels;
    const int width = x_end - x_start;
    for (int y = y_start; y < y_end; ++y) {
        memcpy(framebuffer + y * DISPLAY_WIDTH + x_start,
               source + (y - y_start) * width,
               width * sizeof(uint16_t));
    }

    return ESP_OK;
}

esp_err_t display_refresh(void)
{
    if (panel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_lcd_rgb_qemu_refresh(panel);
}

#endif
