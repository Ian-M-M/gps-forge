/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;

    double latitude;
    double longitude;

    float altitude_m;
    float speed_kmh;
    float course_deg;

    float hdop;
    uint8_t satellites;
    uint8_t fix_quality;
} gps_fix_t;
