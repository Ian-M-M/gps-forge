# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS
        "gps.c"
        "nmea.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_common
    PRIV_REQUIRES
        controls
        esp_driver_uart
        freertos
)
