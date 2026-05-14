/*
 * wifi_connect.c - WiFi 连接模块
 * @brief 提供 WiFi 连接和 NTP 时间同步功能
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_connect";

// Event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static bool s_wifi_initialized = false;
static bool s_ntp_synced = false;

// Default NTP servers - using multiple reliable servers
#define NTP_SERVER_1 "ntp.ali.com"
#define NTP_SERVER_2 "ntp.tencent.com"
#define NTP_SERVER_3 "pool.ntp.org"

// Default timezone for China (CST = China Standard Time, UTC+8)
#define DEFAULT_TIMEZONE "CST-8"

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (!s_ntp_synced) {
            // Retry connection if NTP not synced yet
            esp_wifi_connect();
        }
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_connect_init(void)
{
    if (s_wifi_initialized) {
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();
    
    // Initialize non-volatile storage for Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP network interface
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // Set WiFi to station mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    s_wifi_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized");
    
    return ESP_OK;
}

esp_err_t wifi_connect(const char *ssid, const char *password)
{
    if (!s_wifi_initialized) {
        ESP_LOGE(TAG, "WiFi not initialized, call wifi_connect_init() first");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = password ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection - increased timeout to 30 seconds for classroom WiFi
    ESP_LOGI(TAG, "Waiting for WiFi connection (30s timeout)...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(30000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi successfully!");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi (WIFI_FAIL_BIT set)");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Connection timeout (30s elapsed)");
        return ESP_FAIL;
    }
}

void wifi_disconnect(void)
{
    if (s_wifi_initialized) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        ESP_LOGI(TAG, "WiFi disconnected");
    }
}

bool wifi_is_connected(void)
{
    if (!s_wifi_initialized) {
        return false;
    }
    
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return true;
    }
    return false;
}

const char* wifi_get_ip(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return NULL;
    
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip) == ESP_OK) {
        snprintf(buf, buf_size, IPSTR, IP2STR(&ip.ip));
        return buf;
    }
    strcpy(buf, "N/A");
    return buf;
}

// NTP query using UDP - direct NTP packet exchange
// Returns timestamp on success, 0 on failure
static time_t query_ntp_server(const char *server_name)
{
    ESP_LOGI(TAG, "Querying NTP server: %s", server_name);
    
    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    
    // Resolve server name
    int ret = getaddrinfo(server_name, "123", &hints, &res);
    if (ret != 0 || res == NULL) {
        ESP_LOGW(TAG, "Failed to resolve NTP server: %s", server_name);
        return 0;
    }
    
    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGW(TAG, "Failed to create socket");
        freeaddrinfo(res);
        return 0;
    }
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // Connect to server
    ret = connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (ret < 0) {
        ESP_LOGW(TAG, "Failed to connect to NTP server");
        close(sock);
        return 0;
    }
    
    // NTP packet structure (48 bytes)
    uint8_t ntp_packet[48] = {0};
    ntp_packet[0] = 0x1B; // LI=0, Version=4, Mode=3 (client)
    
    // Send NTP request
    ret = send(sock, ntp_packet, sizeof(ntp_packet), 0);
    if (ret < 0) {
        ESP_LOGW(TAG, "Failed to send NTP request");
        close(sock);
        return 0;
    }
    
    // Receive NTP response
    ret = recv(sock, ntp_packet, sizeof(ntp_packet), 0);
    close(sock);
    if (ret < 0) {
        ESP_LOGW(TAG, "Failed to receive NTP response (timeout?)");
        return 0;
    }
    
    // Extract timestamp from NTP response
    // NTP timestamp is at bytes 40-43 (transmit timestamp)
    uint8_t *ts = &ntp_packet[40];
    uint32_t ntp_time = (ts[0] << 24) | (ts[1] << 16) | (ts[2] << 8) | ts[3];
    
    // Convert NTP time to Unix time (NTP epoch is 1900, Unix epoch is 1970)
    // NTP epoch offset: 2208988800 seconds
    time_t unix_time = ntp_time - 2208988800;
    
    ESP_LOGI(TAG, "NTP server %s returned timestamp: %" PRIu32 " -> Unix: %" PRId64, server_name, ntp_time, (int64_t)unix_time);
    
    return unix_time;
}

// Try to sync time using direct NTP query
static bool sync_time_direct(void)
{
    const char *servers[] = {NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3, "time.windows.com", "time.nist.gov"};
    int num_servers = sizeof(servers) / sizeof(servers[0]);
    
    for (int i = 0; i < num_servers; i++) {
        time_t ts = query_ntp_server(servers[i]);
        if (ts > 1577836800) {  // After 2020-01-01
            ESP_LOGI(TAG, "Successfully synced time from %s: %" PRId64, servers[i], (int64_t)ts);
            
            // Set system time
            struct timeval tv;
            tv.tv_sec = ts;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
            
            s_ntp_synced = true;
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));  // Wait before trying next server
    }
    
    return false;
}

// SNTP time synchronization callback
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "===== SNTP CALLBACK TRIGGERED! =====");
    s_ntp_synced = true;
    
    // Log current time
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm);
    ESP_LOGI(TAG, "Current time: %s", timebuf);
    ESP_LOGI(TAG, "===== SNTP CALLBACK COMPLETED =====");
}

// SNTP server response callback (not used in ESP-IDF 5.x)

// NTP sync task - waits for sync and periodically re-syncs
static void ntp_sync_task(void *arg)
{
    ESP_LOGI(TAG, "NTP sync task started");
    
    // Wait for SNTP sync with longer timeout (60 seconds)
    // SNTP is more reliable than direct UDP query for most networks
    ESP_LOGI(TAG, "Waiting for SNTP sync (60s timeout)...");
    int timeout_ticks = 600; // 60 seconds
    while (!s_ntp_synced && timeout_ticks > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout_ticks--;
        if (timeout_ticks % 50 == 0) {  // Log every 5 seconds
            ESP_LOGI(TAG, "Waiting for SNTP sync... %ds remaining", timeout_ticks / 10);
        }
    }
    
    if (!s_ntp_synced) {
        ESP_LOGW(TAG, "SNTP sync timeout (60s), trying direct NTP query as fallback...");
        
        // Try direct NTP query as fallback
        if (sync_time_direct()) {
            ESP_LOGI(TAG, "Direct NTP query successful as fallback!");
        } else {
            ESP_LOGE(TAG, "All NTP sync methods failed!");
        }
    } else {
        ESP_LOGI(TAG, "SNTP sync completed successfully");
    }
    
    // Periodic re-sync every 10 minutes (more frequent for better accuracy)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000));
        
        if (s_wifi_initialized && wifi_is_connected()) {
            ESP_LOGI(TAG, "Re-syncing time with NTP servers...");
            
            // Use SNTP for re-sync
            sntp_restart();
            vTaskDelay(pdMS_TO_TICKS(5000));
            
            // Log current time
            time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            char timebuf[64];
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm);
            ESP_LOGI(TAG, "Re-synced time: %s", timebuf);
        } else {
            ESP_LOGW(TAG, "WiFi not connected, skipping re-sync");
        }
    }
}

esp_err_t ntp_init(const char *timezone)
{
    ESP_LOGI(TAG, "=== ntp_init() STARTED ===");
    
    // Set timezone
    if (timezone == NULL) {
        timezone = DEFAULT_TIMEZONE;
    }
    setenv("TZ", timezone, 1);
    tzset();
    
    ESP_LOGI(TAG, "Timezone set to: %s", timezone);

    // Initialize SNTP (primary method)
    ESP_LOGI(TAG, "Initializing SNTP with servers: %s, %s, %s", NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    
    // Set operating mode to POLL (standard NTP client mode)
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    ESP_LOGI(TAG, "SNTP operating mode set to POLL");
    
    // Configure NTP servers
    esp_sntp_setservername(0, NTP_SERVER_1);
    ESP_LOGI(TAG, "NTP server 0: %s", NTP_SERVER_1);
    esp_sntp_setservername(1, NTP_SERVER_2);
    ESP_LOGI(TAG, "NTP server 1: %s", NTP_SERVER_2);
    esp_sntp_setservername(2, NTP_SERVER_3);
    ESP_LOGI(TAG, "NTP server 2: %s", NTP_SERVER_3);
    
    // Set time sync notification callback
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    ESP_LOGI(TAG, "Time sync notification callback set");
    
    // Use shorter sync interval initially for faster sync
    esp_sntp_set_sync_interval(1000);  // Start with 1 second interval
    ESP_LOGI(TAG, "Sync interval set to 1s");
    
    // Initialize SNTP (returns void in ESP-IDF 5.x)
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP initialized successfully");
    
    // Log SNTP status
    ESP_LOGI(TAG, "SNTP operating mode: POLL, servers configured");
    
    // Check available heap before creating task
    ESP_LOGI(TAG, "Free heap before task creation: %lu bytes", (unsigned long)esp_get_free_heap_size());
    
    // Start NTP sync task with higher stack size and priority
    ESP_LOGI(TAG, "Attempting to create NTP sync task...");
    BaseType_t task_created = xTaskCreate(ntp_sync_task, "ntp_sync", 12288, NULL, 7, NULL);
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "=== FAILED to create NTP sync task! Return code: %d ===", task_created);
        ESP_LOGE(TAG, "Free heap after failed task creation: %lu bytes", (unsigned long)esp_get_free_heap_size());
    } else {
        ESP_LOGI(TAG, "=== NTP sync task created successfully ===");
        ESP_LOGI(TAG, "Free heap after task creation: %lu bytes", (unsigned long)esp_get_free_heap_size());
    }
    
    ESP_LOGI(TAG, "=== ntp_init() COMPLETED ===");
    
    return ESP_OK;
}

bool ntp_get_time(struct tm *timeinfo)
{
    if (timeinfo == NULL) {
        return false;
    }
    
    // Check if we have a valid time (not epoch 1970)
    time_t now = time(NULL);
    
    // Check if time is valid (after 2020)
    if (now < 1577836800) {  // Before 2020-01-01, likely not synced
        ESP_LOGW(TAG, "Time not synced yet, current timestamp: %lld", (long long)now);
        localtime_r(&now, timeinfo);
        return false;
    }
    
    localtime_r(&now, timeinfo);
    ESP_LOGI(TAG, "Returning valid time: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return true;
}

bool ntp_wait_for_sync(long timeout_ms)
{
    ESP_LOGI(TAG, "Waiting for NTP sync with timeout %ldms...", timeout_ms);
    int timeout_ticks = timeout_ms / 100;
    while (!s_ntp_synced && timeout_ticks > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout_ticks--;
        if (timeout_ticks % 50 == 0) {
            ESP_LOGI(TAG, "Waiting for NTP sync... %ds remaining", timeout_ticks / 10);
        }
    }
    bool result = s_ntp_synced;
    ESP_LOGI(TAG, "NTP sync %s", result ? "successful" : "timeout");
    return result;
}

// Get current time as timestamp
time_t ntp_get_timestamp(void)
{
    return time(NULL);
}