/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nmea.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static size_t sentence(char *out, size_t capacity, const char *payload)
{
    uint8_t checksum = 0;
    for (const char *p = payload; *p; ++p) {
        checksum ^= (uint8_t)*p;
    }
    int length = snprintf(out, capacity, "$%s*%02X\r\n", payload, checksum);
    return length > 0 && (size_t)length < capacity ? (size_t)length : 0;
}

static bool feed(nmea_parser_t *parser, gps_fix_t *fix, const char *text, size_t length)
{
    return nmea_parser_feed(parser, (const uint8_t *)text, length, fix);
}

static bool same_fix(const gps_fix_t *a, const gps_fix_t *b)
{
    return a->valid == b->valid &&
           a->latitude == b->latitude && a->longitude == b->longitude &&
           a->altitude_m == b->altitude_m && a->speed_kmh == b->speed_kmh &&
           a->course_deg == b->course_deg && a->hdop == b->hdop &&
           a->satellites == b->satellites && a->fix_quality == b->fix_quality;
}

int main(void)
{
    const char *rmc_payload =
        "GPRMC,123519.00,A,4121.9720,N,00207.0140,E,10.0,45.0,190926,,,A";
    const char *gga_payload =
        "GPGGA,123519.00,4121.9720,N,00207.0140,E,1,08,0.9,20.0,M,0.0,M,,";
    nmea_parser_t parser;
    gps_fix_t fix = {0};
    char rmc[160];
    char gga[160];
    size_t rmc_length = sentence(rmc, sizeof(rmc), rmc_payload);
    size_t gga_length = sentence(gga, sizeof(gga), gga_payload);
    CHECK(rmc_length && gga_length);

    nmea_parser_init(&parser);
    CHECK(!feed(&parser, &fix, rmc, 12));
    CHECK(feed(&parser, &fix, rmc + 12, rmc_length - 12));
    CHECK(fix.valid && fabs(fix.latitude - 41.3662) < 0.00001);
    CHECK(fabs(fix.longitude - 2.1169) < 0.00001);
    CHECK(fabsf(fix.speed_kmh - 18.52f) < 0.01f);

    char combined[320];
    memcpy(combined, rmc, rmc_length);
    memcpy(combined + rmc_length, gga, gga_length);
    CHECK(feed(&parser, &fix, combined, rmc_length + gga_length));
    CHECK(fix.fix_quality == 1 && fix.satellites == 8);
    CHECK(fabsf(fix.altitude_m - 20.0f) < 0.01f);

    gps_fix_t previous = fix;
    rmc[8] ^= 1;  /* Corrupt payload without updating checksum. */
    CHECK(!feed(&parser, &fix, rmc, rmc_length));
    CHECK(same_fix(&fix, &previous));
    rmc[8] ^= 1;

    char trailing[160];
    memcpy(trailing, rmc, rmc_length);
    trailing[rmc_length - 2] = 'X';
    trailing[rmc_length - 1] = '\n';
    CHECK(!feed(&parser, &fix, trailing, rmc_length));
    CHECK(same_fix(&fix, &previous));

    CHECK(!feed(&parser, &fix, "$GPRMC,missing-bytes", 20));
    CHECK(feed(&parser, &fix, gga, gga_length));

    char too_long[180];
    too_long[0] = '$';
    memset(too_long + 1, 'x', sizeof(too_long) - 2);
    too_long[sizeof(too_long) - 1] = '\n';
    CHECK(!feed(&parser, &fix, too_long, sizeof(too_long)));
    CHECK(feed(&parser, &fix, rmc, rmc_length));

    previous = fix;
    char malformed[160];
    size_t length = sentence(malformed, sizeof(malformed),
        "GPRMC,123519.00,A,nan,N,00207.0140,E,10.0,45.0,190926,,,A");
    CHECK(length && !feed(&parser, &fix, malformed, length));
    CHECK(same_fix(&fix, &previous));

    length = sentence(malformed, sizeof(malformed),
        "GPRMC,123519.00,A,9121.9720,N,00207.0140,E,10.0,45.0,190926,,,A");
    CHECK(length && !feed(&parser, &fix, malformed, length));

    length = sentence(malformed, sizeof(malformed),
        "GPRMC,123519.00,A,4121.9720,E,00207.0140,E,10.0,45.0,190926,,,A");
    CHECK(length && !feed(&parser, &fix, malformed, length));

    length = sentence(malformed, sizeof(malformed),
        "GPRMC,123519.00,A,4121.9720,N,00207.0140,E,nan,45.0,190926,,,A");
    CHECK(length && !feed(&parser, &fix, malformed, length));

    length = sentence(malformed, sizeof(malformed),
        "GPGGA,123519.00,4121.9720,N,nan,E,1,08,0.9,20.0,M,0.0,M,,");
    CHECK(length && !feed(&parser, &fix, malformed, length));

    length = sentence(malformed, sizeof(malformed),
        "GPGGA,123519.00,4121.9720,N,00207.0140,E,1,08,nan,20.0,M,0.0,M,,");
    CHECK(length && !feed(&parser, &fix, malformed, length));
    CHECK(same_fix(&fix, &previous));

    length = sentence(malformed, sizeof(malformed),
        "GPRMC,123519.00,V,,,,,,,190926,,,N");
    CHECK(length && feed(&parser, &fix, malformed, length));
    CHECK(!fix.valid);

    length = sentence(malformed, sizeof(malformed),
        "GPGGA,123519.00,,,,,0,00,99.9,,M,0.0,M,,");
    CHECK(length && feed(&parser, &fix, malformed, length));
    CHECK(!fix.valid && fix.fix_quality == 0);

    puts("NMEA host parser checks passed");
    return 0;
}
