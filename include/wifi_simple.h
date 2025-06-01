#ifndef WIFI_SIMPLE_H
#define WIFI_SIMPLE_H

#include "esp_wifi.h"

// typedef enum {
//     WIFI_MODE_NONE = 0,
//     WIFI_MODE_STA,
//     WIFI_MODE_AP,
//     WIFI_MODE_APSTA
// } wifi_mode_t;

typedef enum {
    WIFI_SCAN_DONE,
    WIFI_SCAN_ERROR
} wifi_scan_event_t;

typedef void (*wifi_scan_cb_t)(wifi_scan_event_t event, wifi_ap_record_t *aps, uint16_t count);

// Simplified initialization
void wifi_init_simple(wifi_mode_t mode, const char *ssid, const char *password);

// Scanning functions
void wifi_start_scan(wifi_scan_cb_t scan_cb);
void wifi_print_scan_results(wifi_ap_record_t *aps, uint16_t count);

// Connection management
void wifi_connect(void);
void wifi_disconnect(void);
bool wifi_is_connected(void);

#endif