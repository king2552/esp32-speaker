#pragma once

#include "esp_err.h"

// Initialise TinyUSB with a UAC2 Audio Class descriptor and start the USB task.
// Call after i2s_output_init().
esp_err_t usb_audio_init(void);
