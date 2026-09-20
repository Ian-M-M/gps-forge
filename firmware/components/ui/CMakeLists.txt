# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS "ui.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES
        controls
        display
        esp_timer
        freertos
        gps
        log
        lvgl
)
