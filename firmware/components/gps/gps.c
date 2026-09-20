/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gps.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/uart.h"
#include "sdkconfig.h"
#include "controls.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "nmea.h"

#if CONFIG_GPS_PLATFORM_CROWPANEL
#define GPS_UART ((uart_port_t)CONFIG_GPS_UART_PORT)
#else
#define GPS_UART UART_NUM_1
#endif
#define GPS_BAUD_RATE 9600
#define GPS_RX_BUFFER_SIZE 2048
#define GPS_READ_BUFFER_SIZE 256

#define GPS_TASK_STACK_SIZE 4096
#define GPS_TASK_PRIORITY 5

static const char *TAG = "gps";

static gps_fix_t latest_fix;
static bool latest_fix_available;

static SemaphoreHandle_t fix_mutex;

static void gps_task(void *arg)
{
    (void)arg;

    nmea_parser_t parser;
    gps_fix_t working_fix = {0};

    nmea_parser_init(&parser);

    uint8_t data[GPS_READ_BUFFER_SIZE];

    while (true) {
        const int len = uart_read_bytes(
            GPS_UART,
            data,
            sizeof(data),
            pdMS_TO_TICKS(1000));

        if (len <= 0) {
            continue;
        }

        controls_feed_bytes(data, (size_t)len);

        if (!nmea_parser_feed(
                &parser,
                data,
                (size_t)len,
                &working_fix)) {
            continue;
        }

        if (xSemaphoreTake(fix_mutex, portMAX_DELAY) == pdTRUE) {
            latest_fix = working_fix;
            latest_fix_available = true;

            xSemaphoreGive(fix_mutex);
        }

    }
}

esp_err_t gps_start(void)
{
    fix_mutex = xSemaphoreCreateMutex();

    if (fix_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const uart_config_t config = {
        .baud_rate = GPS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(
        GPS_UART,
        GPS_RX_BUFFER_SIZE,
        0,
        0,
        NULL,
        0);

    if (err != ESP_OK) {
        vSemaphoreDelete(fix_mutex);
        fix_mutex = NULL;
        return err;
    }

    err = uart_param_config(GPS_UART, &config);

    if (err != ESP_OK) {
        uart_driver_delete(GPS_UART);

        vSemaphoreDelete(fix_mutex);
        fix_mutex = NULL;

        return err;
    }

#if CONFIG_GPS_PLATFORM_CROWPANEL
    #if CONFIG_GPS_UART_RX_GPIO >= 0
    err = uart_set_pin(GPS_UART, UART_PIN_NO_CHANGE, CONFIG_GPS_UART_RX_GPIO,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        uart_driver_delete(GPS_UART);
        vSemaphoreDelete(fix_mutex);
        fix_mutex = NULL;
        return err;
    }
    #else
    ESP_LOGW(TAG, "Physical GPS RX GPIO is unset; configure GPS_UART_RX_GPIO");
    #endif
#endif

    BaseType_t task_created = xTaskCreate(
        gps_task,
        "gps",
        GPS_TASK_STACK_SIZE,
        NULL,
        GPS_TASK_PRIORITY,
        NULL);

    if (task_created != pdPASS) {
        uart_driver_delete(GPS_UART);

        vSemaphoreDelete(fix_mutex);
        fix_mutex = NULL;

        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(
        TAG,
        "GPS service started: UART%d @ %d baud",
        (int)GPS_UART,
        GPS_BAUD_RATE);

    return ESP_OK;
}

bool gps_get_latest(gps_fix_t *fix)
{
    if (fix == NULL || fix_mutex == NULL) {
        return false;
    }

    bool available = false;

    if (xSemaphoreTake(fix_mutex, portMAX_DELAY) == pdTRUE) {
        if (latest_fix_available) {
            *fix = latest_fix;
            available = true;
        }

        xSemaphoreGive(fix_mutex);
    }

    return available;
}
