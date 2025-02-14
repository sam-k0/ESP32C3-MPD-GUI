

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
//static void menu_event_cb(lv_event_t *e);
static void play_pause_event_cb(lv_event_t *e);
static void next_event_cb(lv_event_t *e);
static void prev_event_cb(lv_event_t *e);
static void switchmode_event_cb(lv_event_t *e);
static void button_scroll_event_cb(lv_event_t *e);
static void volume_arc_event_cb(lv_event_t *e);

void switch_mode();

// Tasks
void fetch_currentsong_info_task(void *pvParameter);
void fetch_status_info_task(void *pvParameter);
void update_song_info(void *pvParameter);
void update_ui_from_status(void *pvParameter);


lv_layer_t home_layer = {
    .lv_obj_name    = "home_layer",
    .lv_obj_parent  = NULL,
    .lv_obj_layer   = NULL,
    .lv_show_layer  = NULL,
    .enter_cb       = main_layer_enter_cb,
    .exit_cb        = main_layer_exit_cb,
    .timer_cb       = main_layer_timer_cb,
};


// Main control elements
static lv_obj_t *page;
static lv_obj_t *label_status;
static lv_obj_t *label_songname;
static lv_obj_t *label_artist;
static lv_obj_t *label_time;
static lv_obj_t *label_volume;
static lv_obj_t *bar_song_progress;

// BG image
static lv_obj_t *img;

// Buttons for prev, play/pause, next
static lv_obj_t *btn_prev;
static lv_obj_t *btn_play_pause;
static lv_obj_t *btn_next;
static lv_obj_t *btn_switchmode;
static lv_obj_t *volume_arc;
// Labels for buttons
static lv_obj_t *label_play_pause;
static lv_obj_t *label_prev;
static lv_obj_t *label_next;
static lv_obj_t *label_switchmode;
// Local variables
uint8_t mode = 0;
uint8_t volume_idle_counter = 5;
const char* audio_symbols[] = {LV_SYMBOL_VOLUME_MID, LV_SYMBOL_VOLUME_MAX, LV_SYMBOL_VOLUME_MAX};

// helper functions
inline uint8_t time_get_minutes(uint32_t duration)
{
    return duration / 60;
}

inline uint8_t time_get_seconds(uint32_t duration)
{
    return duration % 60;
}

void print_memory_info()
{
    // Get free heap memory
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI("MEM", "Free heap memory: %d bytes", free_heap);

    // Get minimum free stack space for the current task
    UBaseType_t min_free_stack = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("MEM", "Minimum free stack space: %d bytes", min_free_stack * sizeof(StackType_t));
}


// Callbacks
static void play_pause_event_cb(lv_event_t *e)
{
    // Handle click event
    ESP_LOGI("play_pause_event_cb", "LV_EVENT_CLICKED in play_pause_event_cb");

    mpd_status_t status;
    mpd_get_status(&status);
    if (status.state == MPD_STATE_PLAY) {
        ESP_LOGI("play_pause_event_cb", "Pausing");
        mpd_pause();
        // Set label to paused
        lv_label_set_text(label_status, "Paused");
        lv_label_set_text(label_play_pause, LV_SYMBOL_PLAY);
    } else {
        ESP_LOGI("play_pause_event_cb", "Playing");
        mpd_play();
        // Set label to play
        lv_label_set_text(label_status, "Playing");
        lv_label_set_text(label_play_pause, LV_SYMBOL_PAUSE);
    }
    //free(currstatus);
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
    switch_mode();
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

static void volume_arc_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint8_t value = lv_arc_get_value(volume_arc);

    if (LV_EVENT_KEY == code) // scroll?
    {   
        volume_idle_counter = 0; // reset idle counter to not interrupt user
        uint32_t key = lv_event_get_key(e);
        if (LV_KEY_RIGHT == key) {
            ESP_LOGI("volume_arc_event_cb", "LV_KEY_RIGHT: %d", value);
            mpd_set_volume(value*10);
        } else if (LV_KEY_LEFT == key) {
            ESP_LOGI("volume_arc_event_cb", "LV_KEY_LEFT: %d", value);
            mpd_set_volume(value*10);
        }

        // Update volume label
        char volume_str[5];
        snprintf(volume_str, sizeof(volume_str), "%d", value*10);
        lv_label_set_text(label_volume, volume_str);

    } else if (LV_EVENT_CLICKED == code) {
    // Handle click event
        ESP_LOGI("volume_arc_event_cb", "LV_EVENT_CLICKED in volume_arc_event_cb");
        switch_mode();
    }
}



void switch_mode()
{
    if(mode == 0) // Music control mode, switch to volume control mode
    {
        mode = 1;
        // Remove everything from default group
        ui_remove_all_objs_from_encoder_group();
        // Only have volume arc in the group
        lv_group_add_obj(lv_group_get_default(), volume_arc);
        // Set it to visible and focus on it
        lv_obj_clear_flag(volume_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(label_volume, LV_OBJ_FLAG_HIDDEN);
        lv_group_focus_obj(volume_arc);
        lv_group_set_editing(lv_group_get_default(), true);
    }
    else if(mode == 1) // Volume control mode, switch to music control mode
    {
        mode = 0;
        // Remove everything from default group
        ui_remove_all_objs_from_encoder_group();
        // Add all buttons to the group
        lv_group_add_obj(lv_group_get_default(), btn_prev);
        lv_group_add_obj(lv_group_get_default(), btn_play_pause);
        lv_group_add_obj(lv_group_get_default(), btn_switchmode);
        lv_group_add_obj(lv_group_get_default(), btn_next);
        // Make volume arc invisible and focus on play/pause button
        lv_obj_add_flag(volume_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label_volume, LV_OBJ_FLAG_HIDDEN);
        lv_group_focus_obj(btn_play_pause);
    }
}

// tasks
// Task to fetch song info and status
void fetch_currentsong_info_task(void *pvParameters)
{
    // Sleep for a while before fetching
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        mpd_song_t song;
        mpd_get_currentsong(&song);
        
        print_memory_info();
        lv_async_call(update_song_info, &song);

        // Delay for a while before fetching again
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

void fetch_status_info_task(void *pvParameters)
{
    // Sleep for a while before fetching
    vTaskDelay(pdMS_TO_TICKS(3000)); 

    while (1) {
        mpd_status_t status;
        mpd_get_status(&status);

        lv_async_call(update_ui_from_status, &status);

        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

// Updates UI and frees the parameter mem
void update_song_info(void *param)
{
    mpd_song_t* song_ref = (mpd_song_t*)param;
    
    // Only update song name if it hasn't changed
    if (strcmp(lv_label_get_text(label_songname), song_ref->title) != 0) {
        ESP_LOGI("main_layer_timer_cb", "Updating song name");
        // Update song name and artist
        if (song_ref->title[0] == '\0') {
            if (song_ref->file[0] != '\0') {
                lv_label_set_text(label_songname, song_ref->file);
            } else {
                lv_label_set_text(label_songname, "No song playing...");
            }
        } else {
            lv_label_set_text(label_songname, song_ref->title);
        }
    }

    // Check if artist is empty string
    if (song_ref->artist[0] == '\0') {
        lv_label_set_text(label_artist, "Unknown Artist");
    } else {
        lv_label_set_text(label_artist, song_ref->artist);
    }
}

// Method to update play/pause button
void update_ui_from_status(void *pvParameter)
{
    mpd_status_t* my_status= (mpd_status_t*)pvParameter;

    if (my_status->state == MPD_STATE_PLAY) {
        lv_label_set_text(label_play_pause, LV_SYMBOL_PAUSE);
        lv_label_set_text(label_status, "Playing");
    } else if (my_status->state == MPD_STATE_PAUSE){
        lv_label_set_text(label_play_pause, LV_SYMBOL_PLAY);
        lv_label_set_text(label_status, "Paused");
    } else {
        lv_label_set_text(label_play_pause, LV_SYMBOL_STOP);
        lv_label_set_text(label_status, "Stopped");
    }

    // Update time
    char time_str[20];
    snprintf(time_str, sizeof(time_str), "%02d:%02d / %02d:%02d", time_get_minutes(my_status->elapsed), time_get_seconds(my_status->elapsed), time_get_minutes(my_status->duration), time_get_seconds(my_status->duration));
    lv_label_set_text(label_time, time_str);

    // Update progress bar
    if (my_status->duration != 0) {
        lv_bar_set_range(bar_song_progress, 0, my_status->duration);
        lv_bar_set_value(bar_song_progress, my_status->elapsed, LV_ANIM_ON);
    } else {
        lv_bar_set_value(bar_song_progress, 0, LV_ANIM_ON);
    }


    // Update audio button symbol by calculating the index of 2 symbols
    uint8_t index = 0;
    if (my_status->volume == 0) {
        lv_label_set_text(label_switchmode, LV_SYMBOL_MUTE);
    } else {
        index = my_status->volume / 50;
        lv_label_set_text(label_switchmode, audio_symbols[index]);
    }
    // Update volume arc
    uint8_t overflow = my_status->volume % 10;
    uint8_t target = my_status->volume - overflow;
    if (target != 0)
    {
        target = target / 10; // calculate the value for the arc

        // clamp the value to a range between 0 and 10
        // This hopefully fixes a crash

        if (target > 10) {
            target = 10;
        } else if (target < 0) {
            target = 0;
        }
    }

    if (volume_idle_counter >= 5) {
        lv_arc_set_value(volume_arc, target);
    }else {
        volume_idle_counter++;
    }
    // Update volume label
    char volume_str[5];
    snprintf(volume_str, sizeof(volume_str), "%d", my_status->volume);
    lv_label_set_text(label_volume, volume_str);
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


    label_status = lv_label_create(page);
    lv_label_set_text(label_status, "Paused");
    lv_obj_set_style_text_color(label_status, lv_color_hex(COLOUR_WHITE), 0);
    lv_obj_set_width(label_status, 150);
    lv_obj_set_style_text_align(label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_status, LV_ALIGN_BOTTOM_MID, 0, 5);

    // Display song name and artist in the middle of the screen
    label_songname = lv_label_create(page);
    lv_label_set_text(label_songname, "Connecting to MPD...");
    lv_obj_set_style_text_font(label_songname, &comicsans, 0);
    lv_obj_set_style_text_color(label_songname, lv_color_hex(COLOUR_WHITE), 0);
    lv_obj_set_style_text_align(label_songname, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label_songname, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label_songname, LV_HOR_RES - 6);
    lv_obj_align(label_songname, LV_ALIGN_CENTER, 0, -20);

    label_artist = lv_label_create(page);
    lv_label_set_text(label_artist, "Make sure MPD is running");
    lv_obj_set_style_text_color(label_artist, lv_color_hex(COLOUR_WHITE), 0);
    lv_obj_set_style_text_align(label_artist, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label_artist, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label_artist, LV_HOR_RES - 6);
    lv_obj_align(label_artist, LV_ALIGN_CENTER, 0, 10);

    label_time = lv_label_create(page);
    lv_label_set_text(label_time, "00:00 / 00:00");
    lv_obj_set_style_text_color(label_time, lv_color_hex(COLOUR_WHITE), 0);
    lv_obj_set_style_text_align(label_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, 30);

    bar_song_progress = lv_bar_create(page);
    lv_obj_set_size(bar_song_progress, LV_HOR_RES - 40, 3);
    lv_obj_align(bar_song_progress, LV_ALIGN_CENTER, 0, 40);
    lv_bar_set_range(bar_song_progress, 0, 100);
    lv_bar_set_value(bar_song_progress, 1, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_song_progress, lv_color_hex(COLOUR_WHITE), LV_PART_MAIN); // Background color
    lv_obj_set_style_bg_color(bar_song_progress, lv_color_hex(0x6aaff7), LV_PART_INDICATOR); // Progress color


    // Buttons for prev, play/pause, next
    btn_prev = lv_btn_create(page);
    lv_obj_set_size(btn_prev, 30, 30);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 25, -15);
    label_prev = lv_label_create(btn_prev);
    lv_label_set_text(label_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_align(label_prev, LV_TEXT_ALIGN_LEFT, 0);

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

    // Arc for volume control
    volume_arc = lv_arc_create(page);
    lv_obj_set_size(volume_arc, LV_HOR_RES - 40, LV_VER_RES - 40);
    lv_arc_set_rotation(volume_arc, 180 + (180 - 120) / 2);
    lv_arc_set_bg_angles(volume_arc, 0, 120);
    lv_arc_set_value(volume_arc, 5);
    lv_arc_set_range(volume_arc, 0, 10);
    lv_obj_set_style_arc_width(volume_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(volume_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(volume_arc, lv_color_hex(COLOUR_WHITE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(volume_arc, lv_color_hex(COLOUR_WHITE), LV_PART_INDICATOR);
    lv_obj_align(volume_arc, LV_ALIGN_TOP_MID, 0, -5);
    lv_obj_add_flag(volume_arc, LV_OBJ_FLAG_HIDDEN);

    // Label for volume
    label_volume = lv_label_create(page);
    lv_label_set_text(label_volume, "50");
    lv_obj_set_style_text_color(label_volume, lv_color_hex(COLOUR_WHITE), 0);
    lv_obj_set_style_text_align(label_volume, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_volume, &comicsans, 0);
    lv_obj_align(label_volume, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_flag(label_volume, LV_OBJ_FLAG_HIDDEN);
    
    // Groups
    lv_group_add_obj(lv_group_get_default(), btn_prev);
    lv_group_add_obj(lv_group_get_default(), btn_play_pause);
    lv_group_add_obj(lv_group_get_default(), btn_switchmode);
    lv_group_add_obj(lv_group_get_default(), btn_next);


    // callback for play/pause button
    lv_obj_add_event_cb(btn_play_pause, play_pause_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_next, next_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_prev, prev_event_cb, LV_EVENT_CLICKED, NULL);
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

    // Volume arc events
    lv_obj_add_event_cb(volume_arc, volume_arc_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(volume_arc, volume_arc_event_cb, LV_EVENT_KEY, NULL);

    // Set play/pause button as default
    lv_group_focus_obj(btn_play_pause);

    // Start tasks
    xTaskCreate(fetch_currentsong_info_task, "fetch_currentsong_info_task", 4096, NULL, 5, NULL);
    xTaskCreate(fetch_status_info_task, "fetch_status_info_task", 4096, NULL, 5, NULL);
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
    feed_clock_time();
}