/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    CONTROLS_ENCODER,
    CONTROLS_BUTTON,
    CONTROLS_TOUCH,
} controls_event_type_t;

typedef struct {
    controls_event_type_t type;
    int16_t steps;       /* Encoder event: signed detents. */
    bool pressed;        /* Button or touch event. */
    uint16_t x;          /* Touch event: 0..479. */
    uint16_t y;
} controls_event_t;

/* Create the event queue before starting a transport. */
esp_err_t controls_start(void);

/* Read one event. timeout_ms=0 polls; UINT32_MAX waits indefinitely. */
bool controls_next_event(controls_event_t *event, uint32_t timeout_ms);

/* Physical backends use the same queue as the QEMU transport. */
bool controls_push_event(const controls_event_t *event);

/* QEMU transport: feed bytes from the shared UART1 NMEA/control stream. */
void controls_feed_bytes(const uint8_t *bytes, size_t length);
