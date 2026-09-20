/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "display.h"

#include "sdkconfig.h"

#if CONFIG_GPS_PLATFORM_CROWPANEL

#include <stdint.h>
#include <string.h>

#include "crowpanel_board.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_SPI_CS_GPIO 16
#define LCD_SPI_SCLK_GPIO 2
#define LCD_SPI_SDA_GPIO 1
#define LCD_DE_GPIO 40
#define LCD_VSYNC_GPIO 7
#define LCD_HSYNC_GPIO 15
#define LCD_PCLK_GPIO 41

static const char *TAG = "display";
static esp_lcd_panel_handle_t panel;

static const st7701_lcd_init_cmd_t init_commands[] = {
    {0x01, NULL, 0, 0},
    {0xFF, (const uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xCC, (const uint8_t[]){0x10}, 1, 0},
    {0xCD, (const uint8_t[]){0x08}, 1, 0},
    {0xB0, (const uint8_t[]){0x02, 0x13, 0x1B, 0x0D, 0x10, 0x05, 0x08, 0x07, 0x07, 0x24, 0x04, 0x11, 0x0E, 0x2C, 0x33, 0x1D}, 16, 0},
    {0xB1, (const uint8_t[]){0x05, 0x13, 0x1B, 0x0D, 0x11, 0x05, 0x08, 0x07, 0x07, 0x24, 0x04, 0x11, 0x0E, 0x2C, 0x33, 0x1D}, 16, 0},
    {0xFF, (const uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (const uint8_t[]){0x5D}, 1, 0},
    {0xB1, (const uint8_t[]){0x43}, 1, 0},
    {0xB2, (const uint8_t[]){0x81}, 1, 0},
    {0xB3, (const uint8_t[]){0x80}, 1, 0},
    {0xB5, (const uint8_t[]){0x43}, 1, 0},
    {0xB7, (const uint8_t[]){0x85}, 1, 0},
    {0xB8, (const uint8_t[]){0x20}, 1, 0},
    {0xC1, (const uint8_t[]){0x78}, 1, 0},
    {0xC2, (const uint8_t[]){0x78}, 1, 0},
    {0xD0, (const uint8_t[]){0x88}, 1, 0},
    {0xE0, (const uint8_t[]){0x00, 0x00, 0x02}, 3, 0},
    {0xE1, (const uint8_t[]){0x03, 0xA0, 0x00, 0x00, 0x04, 0xA0, 0x00, 0x00, 0x00, 0x20, 0x20}, 11, 0},
    {0xE2, (const uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 13, 0},
    {0xE3, (const uint8_t[]){0x00, 0x00, 0x11, 0x00}, 4, 0},
    {0xE4, (const uint8_t[]){0x22, 0x00}, 2, 0},
    {0xE5, (const uint8_t[]){0x05, 0xEC, 0xA0, 0xA0, 0x07, 0xEE, 0xA0, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 16, 0},
    {0xE6, (const uint8_t[]){0x00, 0x00, 0x11, 0x00}, 4, 0},
    {0xE7, (const uint8_t[]){0x22, 0x00}, 2, 0},
    {0xE8, (const uint8_t[]){0x06, 0xED, 0xA0, 0xA0, 0x08, 0xEF, 0xA0, 0xA0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 16, 0},
    {0xEB, (const uint8_t[]){0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00}, 7, 0},
    {0xED, (const uint8_t[]){0xFF, 0xFF, 0xFF, 0xBA, 0x0A, 0xBF, 0x45, 0xFF, 0xFF, 0x54, 0xFB, 0xA0, 0xAB, 0xFF, 0xFF, 0xFF}, 16, 0},
    {0xEF, (const uint8_t[]){0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F}, 6, 0},
    {0xFF, (const uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (const uint8_t[]){0x08}, 1, 0},
    {0xFF, (const uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x36, (const uint8_t[]){0x00}, 1, 0},
    {0x3A, (const uint8_t[]){0x60}, 1, 0},
    {0x11, NULL, 0, 100},
    {0x29, NULL, 0, 50},
};

esp_err_t display_start(void)
{
    ESP_RETURN_ON_ERROR(crowpanel_board_start(), TAG, "Board I2C failed");
    ESP_RETURN_ON_ERROR(crowpanel_expander_set(CROWPANEL_LCD_POWER, true), TAG, "LCD power failed");
    ESP_RETURN_ON_ERROR(crowpanel_expander_set(CROWPANEL_LCD_RESET, true), TAG, "LCD reset high failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(crowpanel_expander_set(CROWPANEL_LCD_RESET, false), TAG, "LCD reset low failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_RETURN_ON_ERROR(crowpanel_expander_set(CROWPANEL_LCD_RESET, true), TAG, "LCD reset release failed");
    vTaskDelay(pdMS_TO_TICKS(120));

    const spi_line_config_t line_config = {
        .cs_io_type = IO_TYPE_GPIO,
        .cs_gpio_num = LCD_SPI_CS_GPIO,
        .scl_io_type = IO_TYPE_GPIO,
        .scl_gpio_num = LCD_SPI_SCLK_GPIO,
        .sda_io_type = IO_TYPE_GPIO,
        .sda_gpio_num = LCD_SPI_SDA_GPIO,
        .io_expander = NULL,
    };
    const esp_lcd_panel_io_3wire_spi_config_t io_config =
        ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
    esp_lcd_panel_io_handle_t io;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_config, &io), TAG, "LCD command bus failed");

    const esp_lcd_rgb_panel_config_t rgb_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = 16 * 1000 * 1000,
            .h_res = DISPLAY_WIDTH,
            .v_res = DISPLAY_HEIGHT,
            .hsync_pulse_width = 10,
            .hsync_back_porch = 10,
            .hsync_front_porch = 20,
            .vsync_pulse_width = 10,
            .vsync_back_porch = 10,
            .vsync_front_porch = 10,
            .flags.pclk_active_neg = false,
        },
        .data_width = 16,
        .num_fbs = 2,
        .bounce_buffer_size_px = DISPLAY_WIDTH * 20,
        .data_gpio_nums = {46, 3, 8, 18, 17, 14, 13, 12, 11, 10, 9, 5, 45, 48, 47, 21},
        .de_gpio_num = LCD_DE_GPIO,
        .pclk_gpio_num = LCD_PCLK_GPIO,
        .vsync_gpio_num = LCD_VSYNC_GPIO,
        .hsync_gpio_num = LCD_HSYNC_GPIO,
        .flags.fb_in_psram = true,
    };
    st7701_vendor_config_t vendor_config = {
        .init_cmds = init_commands,
        .init_cmds_size = sizeof(init_commands) / sizeof(init_commands[0]),
        .rgb_config = &rgb_config,
        .flags = {.mirror_by_cmd = 1, .enable_io_multiplex = 0},
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7701(io, &panel_config, &panel), TAG, "LCD panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "LCD panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "LCD panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "LCD enable failed");
    gpio_set_direction(CROWPANEL_BACKLIGHT_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CROWPANEL_BACKLIGHT_GPIO, 1);
    ESP_LOGI(TAG, "CrowPanel ST7701 display ready");
    return ESP_OK;
}

esp_err_t display_draw_bitmap(int x_start, int y_start, int x_end, int y_end,
                              const void *pixels)
{
    if (panel == NULL || pixels == NULL) return ESP_ERR_INVALID_STATE;
    return esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, pixels);
}

esp_err_t display_refresh(void)
{
    return panel == NULL ? ESP_ERR_INVALID_STATE : ESP_OK;
}

#endif
