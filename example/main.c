#include "wifi_simple.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Simple scan callback
static void my_scan_callback(wifi_scan_event_t event, wifi_ap_record_t *aps, uint16_t count) {
    if (event == WIFI_SCAN_DONE) {
        printf("\nScan completed. Found %d networks:\n", count);
        wifi_print_scan_results(aps, count);
    } else {
        printf("Scan failed!\n");
    }
}

void app_main(void) {
    // 1. Initialize WiFi in STA mode (change to WIFI_MODE_AP or WIFI_MODE_APSTA if needed)
    wifi_init_simple(WIFI_MODE_STA, "RITHY", "12345678");
    
    // 2. Start a scan (optional)
    wifi_start_scan(my_scan_callback);
    
    // 3. Connect to the configured network (for STA mode)
    wifi_connect();
    
    // 4. Check connection status
    vTaskDelay(pdMS_TO_TICKS(3000));
    if (wifi_is_connected()) {
        printf("Successfully connected to WiFi!\n");
    } else {
        printf("Failed to connect to WiFi\n");
    }
    
    // 5. Example usage in AP mode:
    // wifi_init_simple(WIFI_MODE_AP, "my_ap", "ap_password");
    // No need to connect - clients will connect to you
}