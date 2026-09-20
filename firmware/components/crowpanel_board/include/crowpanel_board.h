/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define CROWPANEL_I2C_SDA_GPIO 38
#define CROWPANEL_I2C_SCL_GPIO 39
#define CROWPANEL_ENCODER_A_GPIO 42
#define CROWPANEL_ENCODER_B_GPIO 4
#define CROWPANEL_BACKLIGHT_GPIO 6

#define CROWPANEL_TOUCH_RESET (1u << 0)
#define CROWPANEL_TOUCH_INTERRUPT (1u << 2)
#define CROWPANEL_LCD_POWER (1u << 3)
#define CROWPANEL_LCD_RESET (1u << 4)
#define CROWPANEL_ENCODER_BUTTON (1u << 5)

esp_err_t crowpanel_board_start(void);
esp_err_t crowpanel_expander_set(uint8_t mask, bool high);
esp_err_t crowpanel_expander_read(uint8_t *bits);
esp_err_t crowpanel_touch_read(uint8_t reg, uint8_t *data, size_t length);
esp_err_t crowpanel_input_start(void);
