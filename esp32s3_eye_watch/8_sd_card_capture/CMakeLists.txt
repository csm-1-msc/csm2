idf_component_register(
    SRCS "sd_card_capture.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_timer
        sdmmc
        bsp_driver
        lvgl
        esp_lcd
    NAME sd_card_capture
)
