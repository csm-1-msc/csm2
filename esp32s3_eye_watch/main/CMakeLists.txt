idf_component_register(SRCS "gravity_control.c" "main.c" "lvgl_demo_ui.c"
                    "qma6100p/qma6100p.c"
                    "wifi_connect.c"
                    INCLUDE_DIRS "." "." "qma6100p/include"
                    PRIV_REQUIRES esp_wifi esp_netif nvs_flash esp_timer esp_lcd esp_lvgl_port button driver)
