#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "i2s_output.h"
#include "usb_audio.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 USB Speaker starting...");
    ESP_LOGI(TAG, "Audio: %d Hz / %d-bit / stereo", SAMPLE_RATE, SAMPLE_BITS);

    // 1. Bring up I2S → DAC first so the callback can write immediately
    ESP_ERROR_CHECK(i2s_output_init());

    // 2. Start TinyUSB UAC2 device – host will enumerate and see a speaker
    ESP_ERROR_CHECK(usb_audio_init());

    ESP_LOGI(TAG, "Ready. Plug in USB and set 'ESP32-S3 USB Speaker' as your audio output.");

    // Main task has nothing left to do; let FreeRTOS run the USB + I2S tasks
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Heartbeat – system running");
    }
}
