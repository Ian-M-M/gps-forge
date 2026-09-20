/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"

#include "controls.h"
#include "crowpanel_board.h"
#include "gps.h"
#include "ui.h"
#include "sdkconfig.h"

void app_main(void)
{
#if CONFIG_GPS_PLATFORM_CROWPANEL
    ESP_ERROR_CHECK(crowpanel_board_start());
#endif
    ESP_ERROR_CHECK(controls_start());
#if CONFIG_GPS_PLATFORM_CROWPANEL
    ESP_ERROR_CHECK(crowpanel_input_start());
#endif
    ESP_ERROR_CHECK(gps_start());
    ESP_ERROR_CHECK(ui_start());
}
