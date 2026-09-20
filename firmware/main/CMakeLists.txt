# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS "firmware.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        controls
        crowpanel_board
        gps
        ui
)
