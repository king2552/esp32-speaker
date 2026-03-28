#include "i2s_output.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "i2s_output";

static i2s_chan_handle_t tx_chan = NULL;

esp_err_t i2s_output_init(void)
{
    // ── Channel config ────────────────────────────────────────────────────────
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_AUTO,           // pick a free I2S peripheral
        I2S_ROLE_MASTER         // ESP32-S3 is master; DAC is slave
    );
    chan_cfg.dma_desc_num  = DMA_BUFFER_COUNT;
    chan_cfg.dma_frame_num = DMA_BUFFER_FRAMES;
    chan_cfg.auto_clear    = true;   // output silence when DMA starves

    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // ── Standard (Philips I2S) config ─────────────────────────────────────────
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,   // PCM5102A derives MCLK internally
            .bclk = I2S_BCK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = I2S_DATA_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
        return ret;
    }

    ret = i2s_channel_enable(tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "I2S init OK – %d Hz, %d-bit, stereo", SAMPLE_RATE, SAMPLE_BITS);
    ESP_LOGI(TAG, "Pins  BCK=%d  WS=%d  DOUT=%d", I2S_BCK_PIN, I2S_WS_PIN, I2S_DATA_PIN);
    return ESP_OK;
}

esp_err_t i2s_output_write(const void *data, size_t len, size_t *bytes_written)
{
    if (!tx_chan) return ESP_ERR_INVALID_STATE;
    // Timeout = 100 ms; if DMA is backed-up we drop data rather than block USB
    return i2s_channel_write(tx_chan, data, len, bytes_written, pdMS_TO_TICKS(100));
}

void i2s_output_deinit(void)
{
    if (tx_chan) {
        i2s_channel_disable(tx_chan);
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
    }
}
