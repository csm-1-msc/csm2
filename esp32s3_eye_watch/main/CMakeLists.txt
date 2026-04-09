idf_component_register(SRCS "main.c" "lvgl_demo_ui.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_wifi esp_netif nvs_flash esp_timer esp_lcd esp_lvgl_port button)
