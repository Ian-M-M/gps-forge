/*
 * SPDX-FileCopyrightText: 2026 GPS Forge contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui.h"

#include <stdio.h>

#include "controls.h"
#include "display.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gps.h"
#include "lvgl.h"

#define DRAW_BUFFER_LINES 40
#define LVGL_TICK_MS 5

_Static_assert(LV_COLOR_DEPTH == 16, "QEMU display requires RGB565 LVGL color depth");

static const char *TAG = "ui";
static lv_color_t draw_pixels[DISPLAY_WIDTH * DRAW_BUFFER_LINES];
static lv_disp_draw_buf_t draw_buffer;
static lv_disp_drv_t display_driver;
static bool first_frame_flushed;
static lv_obj_t *fix_status;
static lv_obj_t *latitude_label;
static lv_obj_t *longitude_label;
static lv_obj_t *details_label;
static lv_obj_t *encoder_label;
static lv_obj_t *touch_label;
static int32_t encoder_position;
static bool button_pressed;
static bool touch_pressed;
static uint16_t touch_x;
static uint16_t touch_y;

static void flush_display(lv_disp_drv_t *driver, const lv_area_t *area,
                          lv_color_t *pixels)
{
    ESP_ERROR_CHECK(display_draw_bitmap(area->x1, area->y1,
                                        area->x2 + 1, area->y2 + 1, pixels));
    if (lv_disp_flush_is_last(driver)) {
        ESP_ERROR_CHECK(display_refresh());
        if (!first_frame_flushed) {
            first_frame_flushed = true;
            ESP_LOGI(TAG, "First LVGL frame flushed to QEMU display");
        }
    }
    lv_disp_flush_ready(driver);
}

static void advance_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_MS);
}

static void lvgl_task(void *arg)
{
    while (true) {
        uint32_t delay_ms = lv_timer_handler();
        if (delay_ms < LVGL_TICK_MS) {
            delay_ms = LVGL_TICK_MS;
        } else if (delay_ms > 100) {
            delay_ms = 100;
        }

        TickType_t ticks = pdMS_TO_TICKS(delay_ms);
        vTaskDelay(ticks > 0 ? ticks : 1);
    }
}

static void update_gps(lv_timer_t *timer)
{
    (void)timer;

    gps_fix_t fix;
    const bool has_fix = gps_get_latest(&fix);
    if (!has_fix || !fix.valid) {
        lv_label_set_text(fix_status, has_fix ? "NO GPS FIX" : "WAITING FOR GPS");
        lv_obj_set_style_text_color(fix_status, lv_color_hex(0xFFD080), LV_PART_MAIN);
        lv_label_set_text(latitude_label, "LAT  --");
        lv_label_set_text(longitude_label, "LON  --");
        lv_label_set_text(details_label, "SAT --   SPEED --");
        return;
    }

    char line[48];
    lv_label_set_text(fix_status, "GPS FIX");
    lv_obj_set_style_text_color(fix_status, lv_color_hex(0x7DE0C3), LV_PART_MAIN);

    snprintf(line, sizeof(line), "LAT  %.5f", fix.latitude);
    lv_label_set_text(latitude_label, line);
    snprintf(line, sizeof(line), "LON  %.5f", fix.longitude);
    lv_label_set_text(longitude_label, line);
    snprintf(line, sizeof(line), "%u SAT   %.1f km/h", fix.satellites, fix.speed_kmh);
    lv_label_set_text(details_label, line);
}

static void update_controls(lv_timer_t *timer)
{
    (void)timer;

    controls_event_t event;
    bool encoder_changed = false;
    bool touch_changed = false;
    while (controls_next_event(&event, 0)) {
        switch (event.type) {
        case CONTROLS_ENCODER:
            encoder_position += event.steps;
            encoder_changed = true;
            break;
        case CONTROLS_BUTTON:
            button_pressed = event.pressed;
            encoder_changed = true;
            break;
        case CONTROLS_TOUCH:
            touch_pressed = event.pressed;
            touch_x = event.x;
            touch_y = event.y;
            touch_changed = true;
            break;
        }
    }

    char line[48];
    if (encoder_changed) {
        snprintf(line, sizeof(line), "ENC %+ld   BTN %s",
                 (long)encoder_position, button_pressed ? "DOWN" : "UP");
        lv_label_set_text(encoder_label, line);
    }
    if (touch_changed) {
        if (touch_pressed) {
            snprintf(line, sizeof(line), "TOUCH %u,%u", touch_x, touch_y);
            lv_label_set_text(touch_label, line);
        } else {
            lv_label_set_text(touch_label, "TOUCH --");
        }
    }
}

static void create_start_screen(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dial = lv_obj_create(screen);
    lv_obj_set_size(dial, DISPLAY_WIDTH - 8, DISPLAY_HEIGHT - 8);
    lv_obj_center(dial);
    lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dial, lv_color_hex(0x102638), LV_PART_MAIN);
    lv_obj_set_style_border_color(dial, lv_color_hex(0x45B8C8), LV_PART_MAIN);
    lv_obj_set_style_border_width(dial, 3, LV_PART_MAIN);
    lv_obj_clear_flag(dial, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(dial);
    lv_label_set_text(title, "GPS FORGE");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -116);

    fix_status = lv_label_create(dial);
    lv_label_set_text(fix_status, "WAITING FOR GPS");
    lv_obj_align(fix_status, LV_ALIGN_CENTER, 0, -64);

    latitude_label = lv_label_create(dial);
    lv_label_set_text(latitude_label, "LAT  --");
    lv_obj_set_style_text_color(latitude_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(latitude_label, LV_ALIGN_CENTER, 0, -18);

    longitude_label = lv_label_create(dial);
    lv_label_set_text(longitude_label, "LON  --");
    lv_obj_set_style_text_color(longitude_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(longitude_label, LV_ALIGN_CENTER, 0, 12);

    details_label = lv_label_create(dial);
    lv_label_set_text(details_label, "SAT --   SPEED --");
    lv_obj_set_style_text_color(details_label, lv_color_hex(0x9FD4DF), LV_PART_MAIN);
    lv_obj_align(details_label, LV_ALIGN_CENTER, 0, 66);

    encoder_label = lv_label_create(dial);
    lv_label_set_text(encoder_label, "ENC +0   BTN UP");
    lv_obj_set_style_text_color(encoder_label, lv_color_hex(0x9FD4DF), LV_PART_MAIN);
    lv_obj_align(encoder_label, LV_ALIGN_CENTER, 0, 115);

    touch_label = lv_label_create(dial);
    lv_label_set_text(touch_label, "TOUCH --");
    lv_obj_set_style_text_color(touch_label, lv_color_hex(0x9FD4DF), LV_PART_MAIN);
    lv_obj_align(touch_label, LV_ALIGN_CENTER, 0, 145);

    update_gps(NULL);
}

esp_err_t ui_start(void)
{
    ESP_RETURN_ON_ERROR(display_start(), TAG, "Failed to start display");

    lv_init();
    lv_disp_draw_buf_init(&draw_buffer, draw_pixels, NULL,
                          DISPLAY_WIDTH * DRAW_BUFFER_LINES);

    lv_disp_drv_init(&display_driver);
    display_driver.hor_res = DISPLAY_WIDTH;
    display_driver.ver_res = DISPLAY_HEIGHT;
    display_driver.flush_cb = flush_display;
    display_driver.draw_buf = &draw_buffer;
    if (lv_disp_drv_register(&display_driver) == NULL) {
        return ESP_FAIL;
    }

    create_start_screen();
    if (lv_timer_create(update_gps, 1000, NULL) == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (lv_timer_create(update_controls, 50, NULL) == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const esp_timer_create_args_t tick_args = {
        .callback = advance_lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &tick_timer), TAG,
                        "Failed to create LVGL tick timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(tick_timer, LVGL_TICK_MS * 1000),
                        TAG, "Failed to start LVGL tick timer");

    if (xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 2, NULL) != pdPASS) {
        esp_timer_stop(tick_timer);
        esp_timer_delete(tick_timer);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "GPS LVGL screen ready");
    return ESP_OK;
}
