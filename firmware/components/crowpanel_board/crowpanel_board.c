/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "crowpanel_board.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "controls.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define PCF8574_ADDRESS 0x21
#define CST826_ADDRESS 0x15
#define I2C_TIMEOUT_MS 100

static const char *TAG = "crowpanel_board";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t expander;
static i2c_master_dev_handle_t touch;
static SemaphoreHandle_t expander_mutex;
static uint8_t expander_output = 0xff;

static void input_task(void *arg)
{
    (void)arg;
    static const int8_t quadrature[] = {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0,
    };
    uint8_t previous_state = (uint8_t)((gpio_get_level(CROWPANEL_ENCODER_A_GPIO) << 1) |
                                       gpio_get_level(CROWPANEL_ENCODER_B_GPIO));
    uint8_t last_button = 1;
    bool last_touched = false;

    while (true) {
        const uint8_t state = (uint8_t)((gpio_get_level(CROWPANEL_ENCODER_A_GPIO) << 1) |
                                        gpio_get_level(CROWPANEL_ENCODER_B_GPIO));
        const int8_t step = quadrature[(previous_state << 2) | state];
        previous_state = state;
        if (step != 0) {
            const controls_event_t event = {
                .type = CONTROLS_ENCODER,
                .steps = step,
            };
            controls_push_event(&event);
        }

        uint8_t expander_bits;
        if (crowpanel_expander_read(&expander_bits) == ESP_OK) {
            const uint8_t button = (expander_bits & CROWPANEL_ENCODER_BUTTON) != 0;
            if (button != last_button) {
                const controls_event_t event = {
                    .type = CONTROLS_BUTTON,
                    .pressed = !button,
                };
                controls_push_event(&event);
                last_button = button;
            }
        }

        uint8_t touch_data[7];
        bool touched = false;
        uint16_t x = 0;
        uint16_t y = 0;
        if (crowpanel_touch_read(0x02, touch_data, sizeof(touch_data)) == ESP_OK &&
            touch_data[0] > 0 && touch_data[0] <= 1) {
            touched = true;
            x = (uint16_t)(((touch_data[1] & 0x0f) << 8) | touch_data[2]);
            y = (uint16_t)(((touch_data[3] & 0x0f) << 8) | touch_data[4]);
            if (x >= 480) x = 479;
            if (y >= 480) y = 479;
        }
        if (touched || last_touched) {
            const controls_event_t event = {
                .type = CONTROLS_TOUCH,
                .pressed = touched,
                .x = x,
                .y = y,
            };
            controls_push_event(&event);
        }
        last_touched = touched;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t crowpanel_board_start(void)
{
    if (bus != NULL) {
        return ESP_OK;
    }

    expander_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(expander_mutex, ESP_ERR_NO_MEM, TAG, "Mutex allocation failed");

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CROWPANEL_I2C_SDA_GPIO,
        .scl_io_num = CROWPANEL_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "I2C bus failed");

    const i2c_device_config_t expander_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF8574_ADDRESS,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &expander_config, &expander),
                        TAG, "PCF8574 device failed");

    const i2c_device_config_t touch_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CST826_ADDRESS,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &touch_config, &touch),
                        TAG, "CST826 device failed");

    // PCF8574 high bits release the quasi-bidirectional pins for inputs.
    ESP_RETURN_ON_ERROR(i2c_master_transmit(expander, &expander_output, 1, I2C_TIMEOUT_MS),
                        TAG, "PCF8574 initial write failed");
    ESP_LOGI(TAG, "CrowPanel I2C bus and PCF8574 ready");
    return ESP_OK;
}

esp_err_t crowpanel_expander_set(uint8_t mask, bool high)
{
    if (expander == NULL || expander_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(expander_mutex, portMAX_DELAY);
    const uint8_t next = high ? (expander_output | mask) : (expander_output & ~mask);
    esp_err_t err = i2c_master_transmit(expander, &next, 1, I2C_TIMEOUT_MS);
    if (err == ESP_OK) {
        expander_output = next;
    }
    xSemaphoreGive(expander_mutex);
    return err;
}

esp_err_t crowpanel_expander_read(uint8_t *bits)
{
    if (expander == NULL || bits == NULL || expander_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(expander_mutex, portMAX_DELAY);
    esp_err_t err = i2c_master_receive(expander, bits, 1, I2C_TIMEOUT_MS);
    xSemaphoreGive(expander_mutex);
    return err;
}

esp_err_t crowpanel_touch_read(uint8_t reg, uint8_t *data, size_t length)
{
    if (touch == NULL || data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_transmit_receive(touch, &reg, 1, data, length, I2C_TIMEOUT_MS);
}

esp_err_t crowpanel_input_start(void)
{
    if (bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    gpio_config_t encoder_config = {
        .pin_bit_mask = (1ULL << CROWPANEL_ENCODER_A_GPIO) |
                        (1ULL << CROWPANEL_ENCODER_B_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&encoder_config), TAG, "Encoder GPIO setup failed");
    BaseType_t created = xTaskCreate(input_task, "crowpanel_input", 3072, NULL, 6, NULL);
    return created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
