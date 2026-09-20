/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 480

esp_err_t display_start(void);

/* Coordinates use an exclusive end, and pixels are RGB565. */
esp_err_t display_draw_bitmap(int x_start, int y_start, int x_end, int y_end,
                              const void *pixels);
esp_err_t display_refresh(void);
