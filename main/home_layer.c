

#include "lvgl.h"
#include <stdio.h>
#include "esp_system.h"
#ifdef ESP_IDF_VERSION
#include "esp_log.h"
#endif

#include "lv_example_pub.h"
#include "mpd.h"
//#include "bg.h"


static bool main_layer_enter_cb(void *layer);
static bool main_layer_exit_cb(void *layer);
static void main_layer_timer_cb(lv_timer_t *tmr);
// UI Callbacks
static void menu_event_cb(lv_event_t *e);
static void play_pause_event_cb(lv_event_t *e);
static void next_event_cb(lv_event_t *e);
static void prev_event_cb(lv_event_t *e);
static void switchmode_event_cb(lv_event_t *e);
static void button_scroll_event_cb(lv_event_t *e);

lv_layer_t home_layer = {
    .lv_obj_name    = "home_layer",
    .lv_obj_parent  = NULL,
    .lv_obj_layer   = NULL,
    .lv_show_layer  = NULL,
    .enter_cb       = main_layer_enter_cb,
    .exit_cb        = main_layer_exit_cb,
    .timer_cb       = main_layer_timer_cb,
};

static time_out_count time_100ms, time_500ms, time_2000ms;


static lv_obj_t *page;
static lv_obj_t *label_name;
static lv_obj_t *label_songname;
static lv_obj_t *label_artist;
static lv_obj_t *label_time;

// BG image
static lv_obj_t *img;

// Buttons for prev, play/pause, next
static lv_obj_t *btn_prev;
static lv_obj_t *btn_play_pause;
static lv_obj_t *btn_next;
static lv_obj_t *btn_switchmode;

static lv_obj_t *label_play_pause;
static lv_obj_t *label_prev;
static lv_obj_t *label_next;
static lv_obj_t *label_switchmode;



mpd_song_t *song_home = NULL;
mpd_status_t* status_home = NULL;
const char* audio_symbols[] = {LV_SYMBOL_VOLUME_MID, LV_SYMBOL_VOLUME_MAX};

// helper functions
inline uint8_t time_get_minutes(uint32_t duration)
{
    return duration / 60;
}

inline uint8_t time_get_seconds(uint32_t duration)
{
    return duration % 60;
}

static void play_pause_event_cb(lv_event_t *e)
{
    // Handle click event
    ESP_LOGI("play_pause_event_cb", "LV_EVENT_CLICKED in play_pause_event_cb");

    status_home = malloc(sizeof(mpd_status_t));
    mpd_get_status(status_home);
    if (status_home->state == MPD_STATE_PLAY) {
        ESP_LOGI("play_pause_event_cb", "Pausing");
        mpd_pause();
        // Set label to paused
        lv_label_set_text(label_name, "Paused");
        lv_label_set_text(label_play_pause, LV_SYMBOL_PLAY);
    } else {
        ESP_LOGI("play_pause_event_cb", "Playing");
        mpd_play();
        // Set label to play
        lv_label_set_text(label_name, "Playing");
        lv_label_set_text(label_play_pause, LV_SYMBOL_PAUSE);
    }
    free(status_home);
}

static void next_event_cb(lv_event_t *e)
{
    // Handle click event
    ESP_LOGI("next_event_cb", "LV_EVENT_CLICKED in next_event_cb");
    mpd_next();
}

static void prev_event_cb(lv_event_t *e)
{
    // Handle click event
    ESP_LOGI("prev_event_cb", "LV_EVENT_CLICKED in prev_event_cb");
    mpd_prev();
}

static void switchmode_event_cb(lv_event_t *e)
{
    // Go to volume layer
    ESP_LOGI("switchmode_event_cb", "LV_EVENT_CLICKED in switchmode_event_cb");
    ui_remove_all_objs_from_encoder_group();
    lv_func_goto_layer(&volume_layer);

}

static void button_scroll_event_cb(lv_event_t *e)
{
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_color_make(0x8e, 0xbf, 0xed)); // Set background color to blue
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER); // Set opacity to fully opaque

    // Handle selected / deselected event
    if (LV_EVENT_FOCUSED == lv_event_get_code(e)) {
        // Set style to selected
        lv_obj_add_style(lv_event_get_target(e), &style_btn, 0);
    } else if (LV_EVENT_DEFOCUSED == lv_event_get_code(e)) {
        // Remove style
        lv_obj_remove_style(lv_event_get_target(e), &style_btn, 0);
    }
}

// Method to update play/pause button
void update_play_pause_button()
{
    status_home = malloc(sizeof(mpd_status_t));
    mpd_get_status(status_home);

    ESP_LOGI("elapsed", "Elapsed: %d", status_home->elapsed);
    ESP_LOGI("duration", "Duration: %d", status_home->duration);

    if (status_home->state == MPD_STATE_PLAY) {
        lv_label_set_text(label_play_pause, LV_SYMBOL_PAUSE);
        lv_label_set_text(label_name, "Playing");
    } else {
        lv_label_set_text(label_play_pause, LV_SYMBOL_PLAY);
        lv_label_set_text(label_name, "Paused");
    }

    // Update time
    char time_str[20];
    snprintf(time_str, sizeof(time_str), "%02d:%02d / %02d:%02d", time_get_minutes(status_home->elapsed), time_get_seconds(status_home->elapsed), time_get_minutes(status_home->duration), time_get_seconds(status_home->duration));
    lv_label_set_text(label_time, time_str);

    // Update audio button symbol by calculating the index of 2 symbols
    uint8_t index = 0;
    if (status_home->volume == 0) {
        lv_label_set_text(label_switchmode, LV_SYMBOL_MUTE);
    } else {
        index = status_home->volume / 50;
        lv_label_set_text(label_switchmode, audio_symbols[index]);
    }
    free(status_home);
}

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
                ESP_LOGI("menu_event_cb", "LV_KEY_RIGHT");
            } else if (LV_KEY_LEFT == key) {
                lv_obj_set_style_text_color(label_name, lv_color_make(0x00, 0xff, 0x00), 0);
                ESP_LOGI("menu_event_cb", "LV_KEY_LEFT");
            }
        }
        feed_clock_time();

    } else if (LV_EVENT_LONG_PRESSED == code) {
        /*forbidden_sec_trigger = true;
        ESP_LOGI("menu_event_cb", "LV_EVENT_LONG_PRESSED");
        lv_indev_wait_release(lv_indev_get_next(NULL));
        ui_remove_all_objs_from_encoder_group();
        lv_func_goto_layer(&volume_layer);*/

        
    } else if (LV_EVENT_CLICKED == code) {
        if(false == forbidden_sec_trigger) {
            // Handle click event
            ESP_LOGI("menu_event_cb", "LV_EVENT_CLICKED");
        } else {
            forbidden_sec_trigger = false;
        }
        feed_clock_time();
    }
}

void ui_menu_init(lv_obj_t *parent)
{
    page = lv_obj_create(parent);
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES);

    // Set background image
    lv_obj_t *img = lv_img_create(page);
    lv_img_set_src(img, &img_main_bg);
    lv_img_set_zoom(img, 512);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);


    label_name = lv_label_create(page);
    lv_label_set_text(label_name, "Paused");
    
    lv_obj_set_style_text_color(label_name, lv_color_hex(COLOUR_WHITE), 0);
    lv_obj_set_width(label_name, 150);
    lv_obj_set_style_text_align(label_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_name, LV_ALIGN_BOTTOM_MID, 0, 5);

    // Display song name and artist in the middle of the screen
    label_songname = lv_label_create(page);
    lv_label_set_text(label_songname, "Song Name");
    lv_obj_set_style_text_font(label_songname, &comicsans, 0);
    lv_obj_set_style_text_color(label_songname, lv_color_hex(COLOUR_WHITE), 0);
    lv_obj_set_style_text_align(label_songname, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_songname, LV_ALIGN_CENTER, 0, -20);

    label_artist = lv_label_create(page);
    lv_label_set_text(label_artist, "Artist");
    lv_obj_set_style_text_color(label_artist, lv_color_hex(COLOUR_WHITE), 0);
    lv_obj_set_style_text_align(label_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_artist, LV_ALIGN_CENTER, 0, 10);

    label_time = lv_label_create(page);
    lv_label_set_text(label_time, "00:00 / 00:00");
    lv_obj_set_style_text_color(label_time, lv_color_hex(COLOUR_WHITE), 0);
    lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 30);

    // Buttons for prev, play/pause, next

    btn_prev = lv_btn_create(page);
    lv_obj_set_size(btn_prev, 30, 30);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 25, -15);
    label_prev = lv_label_create(btn_prev);
    lv_label_set_text(label_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_align(label_prev, LV_TEXT_ALIGN_CENTER, 0);



    btn_play_pause = lv_btn_create(page);
    lv_obj_set_size(btn_play_pause, 30, 30);
    lv_obj_align(btn_play_pause, LV_ALIGN_BOTTOM_MID, -30, -10);
    label_play_pause = lv_label_create(btn_play_pause);
    lv_label_set_text(label_play_pause, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_align(label_play_pause, LV_TEXT_ALIGN_CENTER, 0);


    btn_switchmode = lv_btn_create(page);
    lv_obj_set_size(btn_switchmode, 30, 30);
    lv_obj_align(btn_switchmode, LV_ALIGN_BOTTOM_MID, 30, -10);
    label_switchmode = lv_label_create(btn_switchmode);
    lv_label_set_text(label_switchmode, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_align(label_switchmode, LV_TEXT_ALIGN_CENTER, 0);

    btn_next = lv_btn_create(page);
    lv_obj_set_size(btn_next, 30, 30);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, -25, -15);
    label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_align(label_next, LV_TEXT_ALIGN_CENTER, 0);

    // Groups
    lv_group_add_obj(lv_group_get_default(), btn_prev);
    lv_group_add_obj(lv_group_get_default(), btn_play_pause);
    lv_group_add_obj(lv_group_get_default(), btn_switchmode);
    lv_group_add_obj(lv_group_get_default(), btn_next);


    // callback for play/pause button
    lv_obj_add_event_cb(btn_play_pause, play_pause_event_cb, LV_EVENT_CLICKED, NULL);
    // callback for next button
    lv_obj_add_event_cb(btn_next, next_event_cb, LV_EVENT_CLICKED, NULL);
    // callback for prev button
    lv_obj_add_event_cb(btn_prev, prev_event_cb, LV_EVENT_CLICKED, NULL);
    // callback for switchmode button
    lv_obj_add_event_cb(btn_switchmode, switchmode_event_cb, LV_EVENT_CLICKED, NULL);

    // Scroll event for focused and defocused

    lv_obj_add_event_cb(btn_prev, button_scroll_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(btn_prev, button_scroll_event_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_add_event_cb(btn_play_pause, button_scroll_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(btn_play_pause, button_scroll_event_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_add_event_cb(btn_next, button_scroll_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(btn_next, button_scroll_event_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_add_event_cb(btn_switchmode, button_scroll_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(btn_switchmode, button_scroll_event_cb, LV_EVENT_DEFOCUSED, NULL);

    // Set play/pause button as default
    lv_group_focus_obj(btn_play_pause);

    // update play/pause button
    //update_play_pause_button();
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
    set_time_out(&time_2000ms, 2000);
    feed_clock_time();

    return ret;
}

static bool main_layer_exit_cb(void *layer)
{
    LV_LOG_USER("");
    return true;
}

// Updates ui based on mpd status
static void main_layer_timer_cb(lv_timer_t *tmr)
{
    feed_clock_time();
    

    if(is_time_out(&time_2000ms)) {
        
        ESP_LOGI("main_layer_timer_cb", "Timer callback");

        // Update play/pause button
        update_play_pause_button();

        song_home = malloc(sizeof(mpd_song_t));
        mpd_get_currentsong(song_home);
        ESP_LOGI("main_layer_timer_cb", "Song: %s", song_home->title);
        ESP_LOGI("main_layer_timer_cb", "Artist: %s", song_home->artist);
        ESP_LOGI("main_layer_timer_cb", "file: %s", song_home->file);

        // Update song name and artist
        if(song_home->title[0] == '\0')
        {
            if(song_home->file[0] != '\0')
            {
                lv_label_set_text(label_songname, song_home->file);
            }
            else
            {
                lv_label_set_text(label_songname, "No Song");
            }
        }
        else
        {
            lv_label_set_text(label_songname, song_home->title);
        }

        // Check if artist is empty string
        if(song_home->artist[0] == '\0')
        {
            lv_label_set_text(label_artist, "Unknown Artist");
        }
        else
        {
            lv_label_set_text(label_artist, song_home->artist);
        }

        free(song_home);
    }
}