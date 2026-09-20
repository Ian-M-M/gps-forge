/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "gps_fix.h"


/*
 * Starts the GPS service.
 *
 * QEMU uses UART1 at 9600 8N1 and connects it directly to the simulator
 * chardev. The physical CrowPanel profile selects the UART peripheral and
 * RX GPIO through GPS_UART_PORT and GPS_UART_RX_GPIO.
 */
esp_err_t gps_start(void);

/*
 * Copies the latest known GPS state into 'fix'.
 *
 * Returns false if no RMC/GGA sentence has been successfully
 * parsed yet.
 */
bool gps_get_latest(gps_fix_t *fix);
