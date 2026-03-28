idf_component_register(
    SRCS
        "main.c"
        "usb_audio.c"
        "i2s_output.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        usb
        log
        freertos
        esp_common
)
