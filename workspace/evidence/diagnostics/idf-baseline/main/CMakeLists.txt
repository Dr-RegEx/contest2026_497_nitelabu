idf_component_register(SRCS "baseline.c" INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event esp_driver_uart lwip)
