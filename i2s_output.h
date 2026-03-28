#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// ─── Pin Configuration ────────────────────────────────────────────────────────
// Adjust these to match your wiring to the DAC (e.g. PCM5102A, MAX98357A)
#define I2S_BCK_PIN     14   // Bit clock
#define I2S_WS_PIN      42   // Word select / LRCLK
#define I2S_DATA_PIN    41   // Serial data out (to DAC DIN)

// ─── Audio Format ─────────────────────────────────────────────────────────────
// Must match what USB Audio descriptor advertises
#define SAMPLE_RATE     48000
#define SAMPLE_BITS     16
#define NUM_CHANNELS    2

// DMA ring buffer: number of DMA descriptors × bytes per descriptor
// 8 × 512 = 4096 bytes of DMA buffering (~21ms @ 48kHz stereo 16-bit)
#define DMA_BUFFER_COUNT    8
#define DMA_BUFFER_FRAMES   128    // frames per DMA buffer (128 * 4 bytes = 512)

esp_err_t i2s_output_init(void);
esp_err_t i2s_output_write(const void *data, size_t len, size_t *bytes_written);
void      i2s_output_deinit(void);
