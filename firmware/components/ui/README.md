<!--
SPDX-FileCopyrightText: 2026 GPS Forge contributors
SPDX-License-Identifier: Apache-2.0
-->

# GPS Forge reference application

This component contains the diagnostic LVGL application used by the v1 QEMU
and CrowPanel profiles. It demonstrates the platform boundary; it is not the
application framework.

An application can replace `ui.c` and keep the platform services unchanged:

- `display_start()`, `display_draw_bitmap()`, and `display_refresh()` for output
- `gps_start()` and `gps_get_latest()` for GPS state
- `controls_start()` and `controls_next_event()` for encoder, button, and touch

The host GPS and controls simulators exercise these same application-facing
services through the QEMU transport.
