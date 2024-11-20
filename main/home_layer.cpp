

#include "lvgl.h"
#include <stdio.h>
#include "esp_system.h"
#ifdef ESP_IDF_VERSION
#include "esp_log.h"
#endif

#include "lv_example_pub.h"


static bool main_layer_enter_cb(void *layer);
static bool main_layer_exit_cb(void *layer);
static void main_layer_timer_cb(lv_timer_t *tmr);
static void menu_event_cb(lv_event_t *e);

lv_layer_t home_layer = {
    .lv_obj_name    = "home_layer",
    .lv_obj_parent  = NULL,
    .lv_obj_layer   = NULL,
    .lv_show_layer  = NULL,
    .enter_cb       = main_layer_enter_cb,
    .exit_cb        = main_layer_exit_cb,
    .timer_cb       = main_layer_timer_cb,
};

static time_out_count time_100ms, time_500ms;
static lv_obj_t *page;
static lv_obj_t *label_name;

static void menu_event_cb(lv_event_t *e)
{
    static uint8_t forbidden_sec_trigger = false;

    lv_event_code_t code = lv_event_get_code(e);

    if (LV_EVENT_FOCUSED == code) {
        lv_group_set_editing(lv_group_get_default(), true);
    } else if (LV_EVENT_KEY == code) {
        uint32_t key = lv_event_get_key(e);
        if (is_time_out(&time_100ms)) {
            // Knob rotate controls
            if (LV_KEY_RIGHT == key) {
                lv_obj_set_style_text_color(label_name, lv_color_make(0x00, 0xff, 0x00), 0);
            } else if (LV_KEY_LEFT == key) {
                lv_obj_set_style_text_color(label_name, lv_color_make(0x00, 0xff, 0x00), 0);
            }
        }
        feed_clock_time();

    } else if (LV_EVENT_CLICKED == code) {
        if (false == forbidden_sec_trigger) {
           // set color to red
            lv_obj_set_style_text_color(label_name, lv_color_make(0xff, 0xff, 0x00), 0);
        } else {
            forbidden_sec_trigger = false;
        }
        feed_clock_time();
    } else if (LV_EVENT_LONG_PRESSED == code) {
        ESP_LOGI("home_layer", "long pressed:");
    }
}

void ui_menu_init(lv_obj_t *parent)
{
    page = lv_obj_create(parent);
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES);

    label_name = lv_label_create(page);
    lv_label_set_text(label_name, "Home");
    // Set text color to red
    lv_obj_set_style_text_color(label_name, lv_color_make(0xff, 0x00, 0x00), 0);
    lv_obj_set_width(label_name, 150);
    lv_obj_set_style_text_align(label_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_name, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_obj_add_event_cb(page, menu_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(page, menu_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(page, menu_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(page, menu_event_cb, LV_EVENT_LONG_PRESSED, NULL);
    
    lv_group_add_obj(lv_group_get_default(), page);
}

static bool main_layer_enter_cb(void *layer)
{
    bool ret = false;

    LV_LOG_USER("");
    lv_layer_t *create_layer = (lv_layer_t *)layer;

    if (NULL == create_layer->lv_obj_layer) {
        ret = true;
        create_layer->lv_obj_layer = lv_obj_create(lv_scr_act());
        lv_obj_set_size(create_layer->lv_obj_layer, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_border_width(create_layer->lv_obj_layer, 0, 0);
        lv_obj_set_style_pad_all(create_layer->lv_obj_layer, 0, 0);

        ui_menu_init(create_layer->lv_obj_layer);
    }
    set_time_out(&time_100ms, 200);
    set_time_out(&time_500ms, 500);
    feed_clock_time();

    return ret;
}

static bool main_layer_exit_cb(void *layer)
{
    LV_LOG_USER("");
    return true;
}

static void main_layer_timer_cb(lv_timer_t *tmr)
{
    return;
}