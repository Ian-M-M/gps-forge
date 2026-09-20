# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS "controls.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES freertos log
)
