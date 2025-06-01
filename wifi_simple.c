#include "wifi_simple.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi_simple";

// Internal state
static wifi_scan_cb_t user_scan_cb = NULL;
static bool connected = false;
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;

// Event handlers
static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data);
static void wifi_scan_event_handler(void* arg, esp_event_base_t event_base, 
                                  int32_t event_id, void* event_data);

// Helper function
static const char* auth_mode_str(wifi_auth_mode_t authmode) {
    switch (authmode) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default: return "UNKNOWN";
    }
}

void wifi_init_simple(wifi_mode_t mode, const char *ssid, const char *password) {
    // Validate input parameters
    if (ssid == NULL || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID cannot be empty");
        return;
    }

    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP network interface
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create network interfaces based on mode
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
        sta_netif = esp_netif_create_default_wifi_sta();
        if (!sta_netif) {
            ESP_LOGE(TAG, "Failed to create STA interface");
            return;
        }
    }

    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        ap_netif = esp_netif_create_default_wifi_ap();
        if (!ap_netif) {
            ESP_LOGE(TAG, "Failed to create AP interface");
            return;
        }
    }

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                     &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                     &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, 
                     &wifi_scan_event_handler, NULL, NULL));

    // Set WiFi mode before configuration
    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));

    // Configure WiFi based on mode
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
        wifi_config_t wifi_config = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    }

    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        wifi_config_t wifi_config = {
            .ap = {
                .ssid_len = strlen(ssid),
                .channel = 1,
                .max_connection = 4,
                .authmode = strlen(password) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
            },
        };
        strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
        strncpy((char*)wifi_config.ap.password, password, sizeof(wifi_config.ap.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    }

    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi initialized in mode %d", mode);
}

void wifi_start_scan(wifi_scan_cb_t scan_cb) {
    user_scan_cb = scan_cb;
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };
    
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(err));
        if (user_scan_cb) {
            user_scan_cb(WIFI_SCAN_ERROR, NULL, 0);
        }
    }
}

void wifi_print_scan_results(wifi_ap_record_t *aps, uint16_t count) {
    if (aps == NULL || count == 0) {
        printf("No networks found\n");
        return;
    }

    printf("\nFound %d networks:\n", count);
    printf("==================================================\n");
    printf("               SSID              | RSSI | Channel | Auth Mode \n");
    printf("--------------------------------|------|---------|----------\n");
    
    for (int i = 0; i < count; i++) {
        printf("%32s | %4d | %7d | %s\n", 
              aps[i].ssid, 
              aps[i].rssi,
              aps[i].primary,
              auth_mode_str(aps[i].authmode));
    }
    printf("==================================================\n");
}

void wifi_connect(void) {
    if (sta_netif == NULL) {
        ESP_LOGE(TAG, "Not in STA mode - cannot connect");
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void wifi_disconnect(void) {
    if (sta_netif == NULL) {
        ESP_LOGE(TAG, "Not in STA mode - cannot disconnect");
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    connected = false;
}

bool wifi_is_connected(void) {
    return connected;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                             int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA started");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Connected to AP");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Disconnected from AP");
                connected = false;
                // Optional: Attempt to reconnect
                esp_wifi_connect();
                break;
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP started");
                break;
            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "AP stopped");
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        connected = true;
    }
}

static void wifi_scan_event_handler(void* arg, esp_event_base_t event_base, 
                                  int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        
        wifi_ap_record_t *ap_records = NULL;
        if (ap_count > 0) {
            ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
            if (ap_records) {
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
            } else {
                ESP_LOGE(TAG, "Failed to allocate memory for AP records");
            }
        }

        if (user_scan_cb) {
            user_scan_cb(ap_records ? WIFI_SCAN_DONE : WIFI_SCAN_ERROR, 
                        ap_records, ap_records ? ap_count : 0);
        }
        
        if (ap_records) {
            free(ap_records);
        }
    }
}