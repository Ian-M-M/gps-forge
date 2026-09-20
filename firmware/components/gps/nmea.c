/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nmea.h"

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NMEA_MAX_FIELDS 20

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    c = (char)toupper((unsigned char)c);

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

static bool nmea_checksum_valid(const char *sentence)
{
    if (sentence == NULL || sentence[0] != '$') {
        return false;
    }

    const char *checksum_separator = strchr(sentence, '*');

    if (checksum_separator == NULL) {
        return false;
    }

    if (checksum_separator[1] == '\0' ||
        checksum_separator[2] == '\0') {
        return false;
    }

    const int high = hex_value(checksum_separator[1]);
    const int low = hex_value(checksum_separator[2]);

    if (high < 0 || low < 0) {
        return false;
    }

    const char *ending = checksum_separator + 3;
    if (!((ending[0] == '\n' && ending[1] == '\0') ||
          (ending[0] == '\r' && ending[1] == '\n' && ending[2] == '\0'))) {
        return false;
    }

    uint8_t calculated = 0;

    for (const char *p = sentence + 1;
         p < checksum_separator;
         ++p) {
        calculated ^= (uint8_t)*p;
    }

    const uint8_t received =
        (uint8_t)((high << 4) | low);

    return calculated == received;
}

static size_t split_fields(
    char *sentence,
    char **fields,
    size_t max_fields)
{
    size_t count = 0;
    char *field = sentence;

    while (count < max_fields) {
        fields[count++] = field;

        char *comma = strchr(field, ',');

        if (comma == NULL) {
            break;
        }

        *comma = '\0';
        field = comma + 1;
    }

    return count;
}

static bool parse_double(const char *str, double *value)
{
    if (str == NULL || str[0] == '\0' || value == NULL) {
        return false;
    }

    char *end;
    const double parsed = strtod(str, &end);

    if (end == str || *end != '\0' || !isfinite(parsed)) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool parse_float(const char *str, float *value)
{
    if (str == NULL || str[0] == '\0' || value == NULL) {
        return false;
    }

    char *end;
    const float parsed = strtof(str, &end);

    if (end == str || *end != '\0' || !isfinite(parsed)) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool parse_u8(const char *str, uint8_t *value)
{
    if (str == NULL || str[0] == '\0' || value == NULL) {
        return false;
    }

    char *end;
    const unsigned long parsed = strtoul(str, &end, 10);

    if (end == str || *end != '\0' || parsed > UINT8_MAX) {
        return false;
    }

    *value = (uint8_t)parsed;
    return true;
}

static bool parse_coordinate(
    const char *coordinate,
    const char *hemisphere,
    bool latitude,
    double *result)
{
    if (coordinate == NULL ||
        hemisphere == NULL ||
        result == NULL ||
        hemisphere[0] == '\0' || hemisphere[1] != '\0') {
        return false;
    }

    double raw;

    if (!parse_double(coordinate, &raw) || raw < 0.0 ||
        raw > (latitude ? 9000.0 : 18000.0)) {
        return false;
    }

    /*
     * NMEA representation:
     *
     * Latitude:  ddmm.mmmm
     * Longitude: dddmm.mmmm
     *
     * The same calculation works for both.
     */
    const int degrees = (int)(raw / 100.0);
    const double minutes = raw - ((double)degrees * 100.0);

    if (minutes < 0.0 || minutes >= 60.0 ||
        (degrees == (latitude ? 90 : 180) && minutes != 0.0)) {
        return false;
    }

    double decimal =
        (double)degrees + (minutes / 60.0);

    switch (hemisphere[0]) {
    case 'N':
        if (!latitude) return false;
        break;
    case 'E':
        if (latitude) return false;
        break;

    case 'S':
        if (!latitude) return false;
        decimal = -decimal;
        break;
    case 'W':
        if (latitude) return false;
        decimal = -decimal;
        break;

    default:
        return false;
    }

    *result = decimal;
    return true;
}

static bool sentence_type_is(
    const char *type,
    const char *expected)
{
    if (type == NULL || expected == NULL) {
        return false;
    }

    const size_t length = strlen(type);

    /*
     * Don't depend on the talker ID.
     *
     * Accept:
     *   GPRMC
     *   GNRMC
     *   GLRMC
     *   ...
     */
    return length >= 3 &&
           strcmp(type + length - 3, expected) == 0;
}

static bool parse_rmc(
    char **fields,
    size_t count,
    gps_fix_t *fix)
{
    /*
     * RMC:
     *
     * 0  GPRMC
     * 1  UTC
     * 2  status: A/V
     * 3  latitude
     * 4  N/S
     * 5  longitude
     * 6  E/W
     * 7  speed in knots
     * 8  course
     * 9  date
     */

    if (count < 10 || fix == NULL) {
        return false;
    }

    if ((fields[2][0] != 'A' && fields[2][0] != 'V') || fields[2][1] != '\0') {
        return false;
    }

    fix->valid = fields[2][0] == 'A';

    /*
     * A V (invalid) RMC may legitimately contain empty position
     * fields. In that case status itself was still parsed correctly.
     */
    if (!fix->valid) {
        return true;
    }

    double latitude;
    double longitude;

    if (!parse_coordinate(fields[3], fields[4], true, &latitude) ||
        !parse_coordinate(fields[5], fields[6], false, &longitude)) {
        return false;
    }

    fix->latitude = latitude;
    fix->longitude = longitude;

    if (fields[7][0] != '\0') {
        float speed_knots;
        if (!parse_float(fields[7], &speed_knots) || speed_knots < 0.0f) {
            return false;
        }
        fix->speed_kmh = speed_knots * 1.852f;
    }

    if (fields[8][0] != '\0') {
        float course;
        if (!parse_float(fields[8], &course) || course < 0.0f || course > 360.0f) {
            return false;
        }
        fix->course_deg = course;
    }

    return true;
}

static bool parse_gga(
    char **fields,
    size_t count,
    gps_fix_t *fix)
{
    /*
     * GGA:
     *
     * 0  GPGGA
     * 1  UTC
     * 2  latitude
     * 3  N/S
     * 4  longitude
     * 5  E/W
     * 6  fix quality
     * 7  satellites
     * 8  HDOP
     * 9  altitude
     * 10 altitude unit
     */

    if (count < 10 || fix == NULL) {
        return false;
    }

    uint8_t quality;

    if (!parse_u8(fields[6], &quality)) {
        return false;
    }

    fix->fix_quality = quality;
    fix->valid = quality != 0;

    /*
     * quality == 0 means no fix. Position fields can be empty.
     */
    if (!fix->valid) {
        return true;
    }

    double latitude;
    double longitude;

    if (!parse_coordinate(fields[2], fields[3], true, &latitude) ||
        !parse_coordinate(fields[4], fields[5], false, &longitude)) {
        return false;
    }

    fix->latitude = latitude;
    fix->longitude = longitude;

    if (fields[7][0] != '\0') {
        uint8_t satellites;
        if (!parse_u8(fields[7], &satellites)) {
            return false;
        }
        fix->satellites = satellites;
    }

    if (fields[8][0] != '\0') {
        float hdop;
        if (!parse_float(fields[8], &hdop) || hdop < 0.0f) {
            return false;
        }
        fix->hdop = hdop;
    }

    if (fields[9][0] != '\0') {
        float altitude;
        if (!parse_float(fields[9], &altitude)) {
            return false;
        }
        fix->altitude_m = altitude;
    }

    return true;
}

static bool parse_sentence(
    const char *sentence,
    gps_fix_t *fix)
{
    if (sentence == NULL || fix == NULL) {
        return false;
    }

    const char *checksum_separator = strchr(sentence, '*');

    if (checksum_separator == NULL) {
        return false;
    }

    /*
     * Copy only the NMEA payload. Remove '$' and '*HH'.
     */
    char payload[NMEA_MAX_SENTENCE_LEN];

    const size_t payload_length =
        (size_t)(checksum_separator - (sentence + 1));

    if (payload_length >= sizeof(payload)) {
        return false;
    }

    memcpy(payload, sentence + 1, payload_length);
    payload[payload_length] = '\0';

    char *fields[NMEA_MAX_FIELDS];

    const size_t field_count =
        split_fields(payload, fields, NMEA_MAX_FIELDS);

    if (field_count == 0) {
        return false;
    }

    gps_fix_t candidate = *fix;

    if (sentence_type_is(fields[0], "RMC")) {
        if (parse_rmc(fields, field_count, &candidate)) {
            *fix = candidate;
            return true;
        }
    } else if (sentence_type_is(fields[0], "GGA")) {
        if (parse_gga(fields, field_count, &candidate)) {
            *fix = candidate;
            return true;
        }
    }

    /*
     * Valid NMEA sentence, but not one we're interested in.
     */
    return false;
}

void nmea_parser_init(nmea_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }

    memset(parser, 0, sizeof(*parser));
}

bool nmea_parser_feed(
    nmea_parser_t *parser,
    const uint8_t *data,
    size_t length,
    gps_fix_t *fix)
{
    if (parser == NULL ||
        data == NULL ||
        fix == NULL) {
        return false;
    }

    bool fix_updated = false;

    for (size_t i = 0; i < length; ++i) {
        const char c = (char)data[i];

        /*
         * '$' always starts a new sentence. This also
         * re-synchronizes us after truncated/corrupt input.
         */
        if (c == '$') {
            parser->length = 0;
            parser->receiving = true;
            parser->buffer[parser->length++] = c;
            continue;
        }

        if (!parser->receiving) {
            continue;
        }

        /*
         * Keep one byte available for '\0'.
         */
        if (parser->length >= NMEA_MAX_SENTENCE_LEN - 1) {
            parser->length = 0;
            parser->receiving = false;
            continue;
        }

        parser->buffer[parser->length++] = c;

        if (c != '\n') {
            continue;
        }

        parser->buffer[parser->length] = '\0';

        if (nmea_checksum_valid(parser->buffer) &&
            parse_sentence(parser->buffer, fix)) {
            fix_updated = true;
        }

        parser->length = 0;
        parser->receiving = false;
    }

    return fix_updated;
}
