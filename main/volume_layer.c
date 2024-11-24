#include "lvgl.h"
#include <stdio.h>
#include "esp_system.h"
#ifdef ESP_IDF_VERSION
#include "esp_log.h"
#endif

#include "lv_example_pub.h"
#include "mpd.h"

static bool volume_layer_enter_cb(void *layer);
static bool volume_layer_exit_cb(void *layer);
static void volume_layer_timer_cb(lv_timer_t *tmr);
// UI Callbacks
static void volume_roller_event_cb(lv_event_t *e);

lv_layer_t volume_layer = {
    .lv_obj_name    = "volume_layer",
    .lv_obj_parent  = NULL,
    .lv_obj_layer   = NULL,
    .lv_show_layer  = NULL,
    .enter_cb       = volume_layer_enter_cb,
    .exit_cb        = volume_layer_exit_cb,
    .timer_cb       = volume_layer_timer_cb,
};

static time_out_count time_2000ms;
static lv_obj_t *page;
static lv_obj_t *label_name;
static lv_obj_t *roller_volume;

static uint8_t idle_counter = 0;
mpd_status_t* status_volume = NULL;


static void volume_roller_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (LV_EVENT_VALUE_CHANGED == code) {
        // Get the current value of the roller
        uint16_t value = lv_roller_get_selected(roller_volume);
        ESP_LOGI("volume_roller_event_cb", "Volume: %d", value);
        mpd_set_volume(value*10);
    } else if (LV_EVENT_CLICKED == code) {
        // goto home layer
        ESP_LOGI("volume_roller_event_cb", "Clicked event: Returning to home layer");
        lv_indev_wait_release(lv_indev_get_next(NULL));
        ui_remove_all_objs_from_encoder_group();
        lv_func_goto_layer(&home_layer);
    } else if (LV_EVENT_KEY == code) { // reset idle counter
        uint32_t key = lv_event_get_key(e);
        if (LV_KEY_RIGHT == key) {
            idle_counter = 0;
            ESP_LOGI("volume_roller_event_cb", "LV_KEY_RIGHT");
        } else if (LV_KEY_LEFT == key) {
            idle_counter = 0;
            ESP_LOGI("volume_roller_event_cb", "LV_KEY_LEFT");
        }
    }
}


void ui_volume_init(lv_obj_t *parent)
{
    page = lv_obj_create(parent);
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES);

    // Set background image
    lv_obj_t *img = lv_img_create(page);
    lv_img_set_src(img, &img_main_bg);
    lv_img_set_zoom(img, 512);
    lv_obj_align(img, LV_ALIGN_CENTER, 0,0);

    label_name = lv_label_create(page);
    lv_label_set_text(label_name, "Volume");
    // Set text color to red
    lv_obj_set_style_text_color(label_name, lv_color_make(0xff, 0x00, 0x00), 0);
    lv_obj_set_width(label_name, 150);
    lv_obj_set_style_text_align(label_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_name, LV_ALIGN_CENTER, 0, 0);

    // style for the roller
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_bg_opa(&style, LV_OPA_0);
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_border_width(&style, 0);
    lv_style_set_pad_all(&style, 0);
    lv_obj_add_style(lv_scr_act(), &style, 0);

    // Roller for volume control
    roller_volume = lv_roller_create(page);
    lv_obj_add_style(roller_volume, &style, 0);
    lv_obj_set_style_text_line_space(roller_volume, 40, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller_volume, LV_OPA_TRANSP, LV_PART_SELECTED);

    lv_roller_set_options(roller_volume, "0\n10\n20\n30\n40\n50\n60\n70\n80\n90\n100", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller_volume, 3);
    lv_obj_align(roller_volume, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(roller_volume, volume_roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(roller_volume, volume_roller_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(roller_volume, volume_roller_event_cb, LV_EVENT_KEY, NULL);

    lv_group_add_obj(lv_group_get_default(), roller_volume);
    // Select the roller by default
    lv_group_focus_obj(roller_volume);

    // Get the current volume from MPD
    status_volume = malloc(sizeof(mpd_status_t));
    mpd_get_status(status_volume);

    uint8_t overflow = status_volume->volume % 10;
    uint8_t target = status_volume->volume - overflow;

    free(status_volume);

    if (target != 0){
        target = target / 10; // calculate the value for the roller
    }

    // set the roller to the closest value
    lv_roller_set_selected(roller_volume, target, LV_ANIM_OFF);
}

static bool volume_layer_enter_cb(void *layer)
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

        ui_volume_init(create_layer->lv_obj_layer);
    }
    set_time_out(&time_2000ms, 2000); // set timers
    feed_clock_time();

    return ret;
}

static bool volume_layer_exit_cb(void *layer)
{
    LV_LOG_USER("");
    return true;
}

static void volume_layer_timer_cb(lv_timer_t *tmr)
{
    feed_clock_time();
    if(is_time_out(&time_2000ms)) {
        status_volume = malloc(sizeof(mpd_status_t));
        mpd_get_status(status_volume); // Get volume from MPD
        uint8_t overflow = status_volume->volume % 10;
        uint8_t target = status_volume->volume - overflow;
        free(status_volume);

        if (target != 0){
            target = target / 10; // calculate the value for the roller
        }

        if (idle_counter < 5)
        {
            idle_counter++;
        }

        // set the roller to the closest value
        if (idle_counter >= 5) {
            ESP_LOGI("volume_layer_timer_cb", "Setting volume to %d as user is idle", target);
            lv_roller_set_selected(roller_volume, target, LV_ANIM_ON);
        }
    }

    return;
}