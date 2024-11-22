

#include "lvgl.h"
#include <stdio.h>
#include "esp_system.h"
#ifdef ESP_IDF_VERSION
#include "esp_log.h"
#endif

#include "lv_example_pub.h"
#include "mpd.h"


static bool main_layer_enter_cb(void *layer);
static bool main_layer_exit_cb(void *layer);
static void main_layer_timer_cb(lv_timer_t *tmr);
// UI Callbacks
static void menu_event_cb(lv_event_t *e);
static void play_pause_event_cb(lv_event_t *e);
static void next_event_cb(lv_event_t *e);
static void prev_event_cb(lv_event_t *e);
static void switchmode_event_cb(lv_event_t *e);

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
// Buttons for prev, play/pause, next
static lv_obj_t *btn_prev;
static lv_obj_t *btn_play_pause;
static lv_obj_t *btn_next;
static lv_obj_t *btn_switchmode;

static lv_obj_t *label_play_pause;
static lv_obj_t *label_prev;
static lv_obj_t *label_next;
static lv_obj_t *label_switchmode;


static void play_pause_event_cb(lv_event_t *e)
{
    // Handle click event
    ESP_LOGI("play_pause_event_cb", "LV_EVENT_CLICKED in play_pause_event_cb");
    mpd_status_t status = mpd_get_status();
    if (status.state == MPD_STATE_PLAY) {
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
    //ui_remove_all_objs_from_encoder_group();
    //lv_func_goto_layer(&volume_layer);

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

    label_name = lv_label_create(page);
    lv_label_set_text(label_name, "Paused");
    // Set text color to red
    lv_obj_set_style_text_color(label_name, lv_color_make(0xff, 0x00, 0x00), 0);
    lv_obj_set_width(label_name, 150);
    lv_obj_set_style_text_align(label_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_name, LV_ALIGN_BOTTOM_MID, 0, 5);

    // Display song name and artist in the middle of the screen
    label_songname = lv_label_create(page);
    lv_label_set_text(label_songname, "Song Name");
    lv_obj_set_style_text_font(label_songname, &comicsans, 0);
    lv_obj_set_style_text_color(label_songname, lv_color_make(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_text_align(label_songname, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_songname, LV_ALIGN_CENTER, 0, -20);

    label_artist = lv_label_create(page);
    lv_label_set_text(label_artist, "Artist");
    lv_obj_set_style_text_color(label_artist, lv_color_make(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_text_align(label_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_artist, LV_ALIGN_CENTER, 0, 10);

    // Buttons for prev, play/pause, next
    btn_prev = lv_btn_create(page);
    lv_obj_set_size(btn_prev, 30, 30);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 25, -15);
    label_prev = lv_label_create(btn_prev);
    lv_label_set_text(label_prev, LV_SYMBOL_PREV);
    // set label centered to the button
    lv_obj_set_style_text_align(label_prev, LV_TEXT_ALIGN_CENTER, 0);
    
    btn_play_pause = lv_btn_create(page);
    lv_obj_set_size(btn_play_pause, 30, 30);
    lv_obj_align(btn_play_pause, LV_ALIGN_BOTTOM_MID, 0, -10);
    label_play_pause = lv_label_create(btn_play_pause);
    lv_label_set_text(label_play_pause, LV_SYMBOL_PLAY);

    btn_next = lv_btn_create(page);
    lv_obj_set_size(btn_next, 30, 30);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, -25, -15);
    label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, LV_SYMBOL_NEXT);

    btn_switchmode = lv_btn_create(page);
    lv_obj_set_size(btn_switchmode, 30, 30);
    lv_obj_align(btn_switchmode, LV_ALIGN_TOP_RIGHT, -25, 15);
    label_switchmode = lv_label_create(btn_switchmode);
    lv_label_set_text(label_switchmode, LV_SYMBOL_REFRESH);



    // Callbacks for knob rotate controls
    /*lv_obj_add_event_cb(page, menu_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(page, menu_event_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(page, menu_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(page, menu_event_cb, LV_EVENT_LONG_PRESSED, NULL);*/
    
    //lv_group_add_obj(lv_group_get_default(), page);
    lv_group_add_obj(lv_group_get_default(), btn_prev);
    lv_group_add_obj(lv_group_get_default(), btn_play_pause);
    lv_group_add_obj(lv_group_get_default(), btn_next);
    lv_group_add_obj(lv_group_get_default(), btn_switchmode);

    // callback for play/pause button
    lv_obj_add_event_cb(btn_play_pause, play_pause_event_cb, LV_EVENT_CLICKED, NULL);
    // callback for next button
    lv_obj_add_event_cb(btn_next, next_event_cb, LV_EVENT_CLICKED, NULL);
    // callback for prev button
    lv_obj_add_event_cb(btn_prev, prev_event_cb, LV_EVENT_CLICKED, NULL);
    // callback for switchmode button
    lv_obj_add_event_cb(btn_switchmode, switchmode_event_cb, LV_EVENT_CLICKED, NULL);
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

        mpd_status_t status = mpd_get_status();
        // Print status
        /*ESP_LOGI("MPD_PARSER", "Parsed Status - Volume: %d, Repeat: %d, Random: %d, Single: %d, Consume: %d, Playlist: %hu, Playlist Length: %hu, State: %d, Song: %hu, Elapsed: %u, Bitrate: %hu",
             status.volume, status.repeat, status.random, status.single, status.consume, status.playlist, status.playlistlength, status.state, status.song, status.elapsed, status.bitrate);
        */
        // update play/pause button
        if (status.state == MPD_STATE_PLAY) {
            lv_label_set_text(label_play_pause, LV_SYMBOL_PAUSE);
            lv_label_set_text(label_name, "Playing");
        } else {
            lv_label_set_text(label_play_pause, LV_SYMBOL_PLAY);
            lv_label_set_text(label_name, "Paused");
        }
    
    



        // Update the song name and artist

        //connect_send_close(MPD_HOST, MPD_PORT, "currentsong\n", mpd_resp_buf, sizeof(mpd_resp_buf));
        //mpd_song_t song = mpd_get_currentsong();
        //lv_label_set_text(label_songname, song.title);
        //lv_label_set_text(label_artist, song.artist);

        //const char* test_string = "file: Camellia-Feelin_Sky-WPNUBDRoxgy7W48GBXZ.mp3\nLast-Modified: 2024-11-20T16:30:21Z\nFormat: 44100:24:2\nArtist: Camellia\nTitle: Feelin Sky\nAlbum: Feelin Sky\nTime: 362\nduration: 362.213\nPos: 0\nId: 13\nOK\n";

        /*mpd_song_t song;
        parse_mpd_currentsong(test_string, &song);
        //song = mpd_get_currentsong();
       ESP_LOGI("MPD_PARSER", "Parsed Song - Title: %s, Artist: %s, Album: %s, File: %s, Duration: %f, Pos: %hu, Id: %hu",
             song.title, song.artist, song.album, song.file, song.duration, song.position, song.id);*/
    }
}