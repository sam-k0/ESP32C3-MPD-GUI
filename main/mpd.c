
#include "mpd.h"
#include <stdio.h> //for basic printf commands
#include <string.h> //for handling strings
#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "esp_system.h" //esp_init funtions esp_err_t 
#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "esp_log.h" //for showing logs
#include "esp_event.h" //for wifi event
#include "nvs_flash.h" //non volatile storage
#include "lwip/err.h" //light weight ip packets error handling
#include "lwip/sys.h" //system applications for light weight ip apps
#include "lwip/sockets.h" //sockets for lwip
#include "lwip/inet.h" //internet operations for lwip


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect(); // Connect on start event
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
        if (s_retry_num < 5) 
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
        } else 
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG,"connect to the AP fail\n");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        }

    };
    // copy the ssid and password to the wifi_config
    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char *)wifi_config.sta.password, WIFI_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASSWORD);
    } else {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
    }
}

int connect_mpd(const char* ip_addr, uint16_t port)
{
    int sock = -1; // socket file descriptor
    struct sockaddr_in dest_addr; // server address

    // Create a socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        ESP_LOGE("MPD", "Socket creation failed");
        return -1;
    }

    // Set the dest address
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(ip_addr);
    dest_addr.sin_port = htons(port);

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) 
    {
        ESP_LOGE("MPD", "Connection to server failed");
        return -1;
    }

    return sock;
}

int send_mpd_cmd(int sock, const char *cmd, char* resp, size_t resp_size)
{
    ESP_LOGI("MPD", "Sending command: %s", cmd);
    int bytes_sent = send(sock, cmd, strlen(cmd), 0);
    if (bytes_sent < 0) 
    {
        ESP_LOGE("MPD", "Failed to send command");
        return -1;
    }

    int bytes_recv = recv(sock, resp, resp_size-1, 0); // -1 to leave room for null terminator
    if (bytes_recv < 0) 
    {
        ESP_LOGE("MPD", "Failed to receive response");
        return -1;
    }

    resp[bytes_recv] = '\0'; // null terminate the response

    //ESP_LOGI("MPD", "Response: %s", resp);
    return bytes_recv; // return the number of bytes received
}

// Connect to the MPD server, send a command, and close the connection
int connect_send_close(const char* ip_addr, uint16_t port, const char *cmd, char* resp, size_t resp_size)
{
    int sock = connect_mpd(ip_addr, port);
    if (sock < 0) 
    {
        return -1;
    }

    // Discard the greeting message
    char temp_buf[256]; // Temporary buffer for the greeting message
    int greeting_recv = recv(sock, temp_buf, sizeof(temp_buf) - 1, 0);
    if (greeting_recv <= 0) 
    {
        ESP_LOGE("MPD", "Failed to receive greeting from MPD");
        close(sock);
        return -1;
    }
    temp_buf[greeting_recv] = '\0';
    ESP_LOGI("MPD", "Greeting: %s", temp_buf);

    int bytes_recv = send_mpd_cmd(sock, cmd, resp, resp_size);
    close(sock);
    return bytes_recv;
}

bool parse_mpd_status(const char *response, mpd_status_t *status) {
    if (response == NULL || status == NULL) {
        ESP_LOGE("MPD_PARSER", "Invalid arguments to parse_mpd_status.");
        return false;
    }
    memset(status, 0, sizeof(mpd_status_t));

    char *line = strtok((char *)response, "\n");
    while (line != NULL) {
        if (sscanf(line, "volume: %hhd", &status->volume) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed volume: %d", status->volume);
        } else if (sscanf(line, "repeat: %hhd", (int8_t *)&status->repeat) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed repeat: %d", status->repeat);
        } else if (sscanf(line, "random: %hhd", (int8_t *)&status->random) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed random: %d", status->random);
        } else if (sscanf(line, "single: %hhd", (int8_t *)&status->single) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed single: %d", status->single);
        } else if (sscanf(line, "consume: %hhd", (int8_t *)&status->consume) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed consume: %d", status->consume);
        } else if (sscanf(line, "playlist: %hu", &status->playlist) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed playlist: %d", status->playlist);
        } else if (sscanf(line, "playlistlength: %hu", &status->playlistlength) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed playlist length: %d", status->playlistlength);
        } else if (strncmp(line, "state: play", 11) == 0) {
            status->state = MPD_STATE_PLAY;
            ESP_LOGI("MPD_PARSER", "Parsed state: play");
        } else if (strncmp(line, "state: pause", 12) == 0) {
            status->state = MPD_STATE_PAUSE;
            ESP_LOGI("MPD_PARSER", "Parsed state: pause");
        } else if (strncmp(line, "state: stop", 11) == 0) {
            status->state = MPD_STATE_STOP;
            ESP_LOGI("MPD_PARSER", "Parsed state: stop");
        } else if (sscanf(line, "song: %hu", &status->song) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed song: %d", status->song);
        } else if (sscanf(line, "elapsed: %u", &status->elapsed) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed elapsed: %d", status->elapsed);
        } else if (sscanf(line, "bitrate: %hu", &status->bitrate) == 1) {
            ESP_LOGI("MPD_PARSER", "Parsed bitrate: %d", status->bitrate);
        }

        line = strtok(NULL, "\n");
    }
    return true;
}

mpd_status_t mpd_get_status()
{
    mpd_status_t status;
    memset(&status, 0, sizeof(mpd_status_t));
    connect_send_close(MPD_HOST, MPD_PORT, "status\n", mpd_resp_buf, sizeof(mpd_resp_buf));
    parse_mpd_status(mpd_resp_buf, &status);
    return status;
}

// Parse the current song response
void parse_mpd_currentsong(const char *response, mpd_song_t *song)
{
    if (response == NULL || song == NULL) {
        ESP_LOGE("MPD_PARSER", "Invalid arguments to parse_mpd_currentsong.");
        return;
    }

    // Log the full response for debugging
    ESP_LOGI("MPD_PARSER", "Response:\n%s", response);

    // Copy the response into a mutable buffer to avoid modifying the original string
    char response_copy[512]; // Ensure this buffer is large enough to hold the response
    strncpy(response_copy, response, sizeof(response_copy) - 1);
    response_copy[sizeof(response_copy) - 1] = '\0'; // Null-terminate to be safe

    // Use strtok to split the response into lines based on the newline character
    char *line = strtok(response_copy, "\n");

    // Parse the response line by line
    while (line != NULL) {
        ESP_LOGI("MPD_PARSER", "Parsing line: %s", line);

        // Parse Title
        if (strncmp(line, "Title: ", 7) == 0) {
            strncpy(song->title, line + 7, sizeof(song->title) - 1);
            song->title[sizeof(song->title) - 1] = '\0';  // Ensure null termination
        }
        // Parse Artist
        else if (strncmp(line, "Artist: ", 8) == 0) {
            strncpy(song->artist, line + 8, sizeof(song->artist) - 1);
            song->artist[sizeof(song->artist) - 1] = '\0';  // Ensure null termination
        }
        // Parse Album
        else if (strncmp(line, "Album: ", 7) == 0) {
            strncpy(song->album, line + 7, sizeof(song->album) - 1);
            song->album[sizeof(song->album) - 1] = '\0';  // Ensure null termination
        }
        // Parse File path
        else if (strncmp(line, "file: ", 6) == 0) {
            strncpy(song->file, line + 6, sizeof(song->file) - 1);
            song->file[sizeof(song->file) - 1] = '\0';  // Ensure null termination
        }
        // Parse Duration (floating point)
        else if (sscanf(line, "duration: %f", &song->duration) == 1) {
            // Duration parsed successfully
        }
        // Parse Position (integer)
        else if (sscanf(line, "Pos: %hu", &song->position) == 1) {
            // Position parsed successfully
        }
        // Parse ID (integer)
        else if (sscanf(line, "Id: %hu", &song->id) == 1) {
            // ID parsed successfully
        }

        // Get the next line by calling strtok with NULL
        line = strtok(NULL, "\n");
    }

    // Print the final parsed song info for debugging
    ESP_LOGI("MPD_PARSER", "Parsed Song - Title: %s, Artist: %s, Album: %s, File: %s, Duration: %f, Pos: %hu, Id: %hu",
             song->title, song->artist, song->album, song->file, song->duration, song->position, song->id);
}


mpd_song_t mpd_get_currentsong()
{
    mpd_song_t song;
    memset(&song, 0, sizeof(mpd_song_t));
    connect_send_close(MPD_HOST, MPD_PORT, "currentsong\n", mpd_resp_buf, sizeof(mpd_resp_buf));
    parse_mpd_currentsong(mpd_resp_buf, &song);
    return song;
}

int mpd_set_volume(int volume)
{
    char cmd[32]; // format buffer
    snprintf(cmd, sizeof(cmd), "setvol %d\n", volume); // format the command
    return connect_send_close(MPD_HOST, MPD_PORT, cmd, mpd_resp_buf, sizeof(mpd_resp_buf));
}

int mpd_next()
{
    return connect_send_close(MPD_HOST, MPD_PORT, "next\n", mpd_resp_buf, sizeof(mpd_resp_buf));
}

int mpd_prev()
{
    return connect_send_close(MPD_HOST, MPD_PORT, "previous\n", mpd_resp_buf, sizeof(mpd_resp_buf));
}

int mpd_play()
{
    return connect_send_close(MPD_HOST, MPD_PORT, "play\n", mpd_resp_buf, sizeof(mpd_resp_buf));
}

int mpd_pause()
{
    return connect_send_close(MPD_HOST, MPD_PORT, "pause\n", mpd_resp_buf, sizeof(mpd_resp_buf));
}

