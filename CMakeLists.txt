idf_component_register(SRCS "wifi_simple.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi esp_event nvs_flash esp_netif
                    )
