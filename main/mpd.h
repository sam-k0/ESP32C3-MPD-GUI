#pragma once

#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "esp_system.h" //esp_init funtions esp_err_t 
#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "esp_event.h" //for wifi event


#define WIFI_SSID "Bagger 244"
#define WIFI_PASSWORD "am0gu$$y69"
#define MPD_HOST "192.168.178.83"
#define MPD_PORT 6600

static char mpd_resp_buf[512];
// Define states as integer constants
#define MPD_STATE_STOP  0
#define MPD_STATE_PLAY  1
#define MPD_STATE_PAUSE 2

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char* WIFI_TAG = "wifi station";
static int s_retry_num = 0;

typedef struct {
    int8_t volume;         // Volume level (0-100, fits in int8_t)
    bool repeat;           // Repeat mode (0 or 1)
    bool random;           // Random mode (0 or 1)
    bool single;           // Single mode (0 or 1)
    bool consume;          // Consume mode (0 or 1)
    uint16_t playlist;     // Current playlist ID (unsigned, typical range fits in uint16_t)
    uint16_t playlistlength; // Number of tracks in the playlist (unsigned)
    uint8_t state;         // Playback state (0=stop, 1=play, 2=pause)
    uint16_t song;         // Current song index (unsigned)
    uint32_t elapsed;      // Elapsed time in seconds (unsigned, fits in uint32_t)
    uint16_t bitrate;      // Current bitrate in kbps (unsigned)
    uint32_t duration;     // Duration of the current song in seconds
} mpd_status_t;

// Currentsong
typedef struct {
    char title[128];      // Song title
    char artist[128];     // Artist name
    char album[128];      // Album name // unused
    char file[256];       // File path
    // unused
    uint32_t duration;    // Duration in seconds
    uint16_t position;    // Position in the playlist
    uint16_t id;          // Song ID
} mpd_song_t;

#ifdef __cplusplus
extern "C" {
#endif



static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

void wifi_init_sta(void);

// Returns the number of bytes read from the socket
int send_mpd_cmd(int sock, const char *cmd, char* resp, size_t resp_size);

// Returns the socket file descriptor
int connect_mpd(const char* ip_addr, uint16_t port);

// Returns the number of bytes read from the socket
// This calls connect_mpd, send_mpd_cmd, and close
// So all we have to do is read the response from the buffer
int connect_send_close(const char* ip_addr, uint16_t port, const char *cmd, char* resp, size_t resp_size);

// Wrapper for the MPD status command
bool parse_mpd_status(const char *response, mpd_status_t *status);
void mpd_get_status(mpd_status_t* status);

// Wrapper for the MPD currentsong command
void parse_mpd_currentsong(const char *response, mpd_song_t *song);
void mpd_get_currentsong(mpd_song_t *song);

// Simple commands
int mpd_set_volume(int volume);
int mpd_play();
int mpd_pause();
int mpd_next();
int mpd_prev();



#ifdef __cplusplus
}
#endif
