# SPDX-FileCopyrightText: 2026 GPS Forge contributors
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS
        "display_qemu.c"
        "display_crowpanel.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_common
    PRIV_REQUIRES
        esp_lcd
        espressif__esp_lcd_qemu_rgb
        esp_lcd_panel_io_additions
        esp_lcd_st7701
        crowpanel_board
)
