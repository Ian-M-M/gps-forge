/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gps_fix.h"

#define NMEA_MAX_SENTENCE_LEN 128

typedef struct {
    char buffer[NMEA_MAX_SENTENCE_LEN];
    size_t length;
    bool receiving;
} nmea_parser_t;

void nmea_parser_init(nmea_parser_t *parser);

bool nmea_parser_feed(
    nmea_parser_t *parser,
    const uint8_t *data,
    size_t length,
    gps_fix_t *fix
);
