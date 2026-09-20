/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "controls.h"

#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define CONTROLS_QUEUE_LENGTH 64
#define CONTROLS_LINE_LENGTH 64

static const char *TAG = "controls";
static QueueHandle_t event_queue;
static char line[CONTROLS_LINE_LENGTH];
static size_t line_length;
static bool collecting;
static bool overflow;

static bool parse_line(const char *text, controls_event_t *event)
{
    int a;
    int b;
    int c;
    char extra;

    // Field widths keep malformed long numbers from overflowing an int.
    if (sscanf(text, "E %4d %c", &a, &extra) == 1 && a >= -127 && a <= 127 && a != 0) {
        *event = (controls_event_t){.type = CONTROLS_ENCODER, .steps = a};
        return true;
    }
    if (sscanf(text, "B %1d %c", &a, &extra) == 1 && (a == 0 || a == 1)) {
        *event = (controls_event_t){.type = CONTROLS_BUTTON, .pressed = a};
        return true;
    }
    if (sscanf(text, "T %1d %3d %3d %c", &a, &b, &c, &extra) == 3 &&
        (a == 0 || a == 1) && b >= 0 && b < 480 && c >= 0 && c < 480) {
        *event = (controls_event_t){
            .type = CONTROLS_TOUCH, .pressed = a, .x = b, .y = c,
        };
        return true;
    }
    return false;
}

esp_err_t controls_start(void)
{
    if (event_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    event_queue = xQueueCreate(CONTROLS_QUEUE_LENGTH, sizeof(controls_event_t));
    if (event_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Virtual controls queue ready");
    return ESP_OK;
}

bool controls_next_event(controls_event_t *event, uint32_t timeout_ms)
{
    if (event == NULL || event_queue == NULL) {
        return false;
    }

    TickType_t ticks = timeout_ms == UINT32_MAX
                       ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(event_queue, event, ticks) == pdTRUE;
}

bool controls_push_event(const controls_event_t *event)
{
    if (event == NULL || event_queue == NULL) {
        return false;
    }
    if (xQueueSend(event_queue, event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Input queue full; event dropped");
        return false;
    }
    return true;
}

void controls_feed_bytes(const uint8_t *bytes, size_t length)
{
    if (bytes == NULL || event_queue == NULL) {
        return;
    }

    for (size_t i = 0; i < length; ++i) {
        char ch = bytes[i];
        if (ch == '!') {
            collecting = true;
            overflow = false;
            line_length = 0;
            continue;
        }
        if (!collecting) {
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            if (!overflow && line_length > 0) {
                line[line_length] = '\0';
                controls_event_t event;
                if (parse_line(line, &event)) {
                    controls_push_event(&event);
                } else {
                    ESP_LOGW(TAG, "Invalid input command: %s", line);
                }
            }
            collecting = false;
            line_length = 0;
            overflow = false;
            continue;
        }
        if (!overflow) {
            if (line_length < sizeof(line) - 1) {
                line[line_length++] = ch;
            } else {
                overflow = true;
                ESP_LOGW(TAG, "Input command too long");
            }
        }
    }
}
