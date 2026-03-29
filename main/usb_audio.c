/*
 * usb_audio.c
 *
 * Implements a USB Audio Class 2.0 (UAC2) speaker using TinyUSB.
 *
 * The USB descriptor advertises:
 *   - 1 audio streaming interface (speaker only, no mic)
 *   - 48 kHz, 16-bit, stereo PCM
 *
 * Received audio data is forwarded to i2s_output_write() from the TinyUSB
 * audio callback which runs in the TinyUSB task context.
 *
 * Wiring reminder
 * ───────────────
 *  ESP32-S3 USB D-  →  USB connector D-  (GPIO 19 on most DevKitC-1 boards)
 *  ESP32-S3 USB D+  →  USB connector D+  (GPIO 20)
 *  These are the native USB OTG pins – do NOT use the JTAG USB port.
 */

#include "usb_audio.h"
#include "i2s_output.h"

#include "esp_private/usb_phy.h"
#include "esp_tinyusb.h"
#include "tinyusb_net.h"
#include "class/audio/audio.h"
#include "device/usbd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "usb_audio";

// ─── Descriptor constants ─────────────────────────────────────────────────────

#define USB_VID         0x303A   // Espressif VID (fine for personal use)
#define USB_PID         0x4001   // arbitrary product ID
#define USB_BCD         0x0200   // USB 2.0

// Interface numbers
#define ITF_NUM_AUDIO_CONTROL   0
#define ITF_NUM_AUDIO_STREAMING 1
#define ITF_NUM_TOTAL           2

// Endpoint
#define EPNUM_AUDIO_OUT 0x01     // OUT EP 1

// String indices
#define STRID_LANGID    0
#define STRID_MANUF     1
#define STRID_PRODUCT   2
#define STRID_SERIAL    3

// Audio format constants matching i2s_output.h
#define UAC_SAMPLE_RATE  48000u
#define UAC_CHANNELS     2
#define UAC_BITS         16
#define UAC_BYTES        (UAC_BITS / 8)

// Max packet size for one USB frame (1 ms) at 48 kHz stereo 16-bit = 192 bytes
// Add a bit of headroom for 48.something kHz rounding
#define UAC_EP_SIZE      (UAC_SAMPLE_RATE / 1000 * UAC_CHANNELS * UAC_BYTES + UAC_CHANNELS * UAC_BYTES)

// ─── Device descriptor ────────────────────────────────────────────────────────

static const tusb_desc_device_t device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUF,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1,
};

// ─── Configuration descriptor ─────────────────────────────────────────────────
//
// UAC2 topology:
//
//  [USB Host OUT EP] ──→ [Input Terminal: USB streaming] ──→ [Feature Unit: volume/mute]
//                                                          ──→ [Output Terminal: speaker]
//
// We use the TinyUSB UAC2 helper macros. The descriptor is built inline so the
// compiler knows the total length for bConfigurationValue.

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_AUDIO_SPEAKER_STEREO_DESC_LEN)

// Clock source entity ID = 4, Input terminal = 1, Feature unit = 2, Output terminal = 3
static const uint8_t config_desc[] = {
    // Configuration
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // UAC2 speaker (stereo, 16-bit, 48 kHz)
    TUD_AUDIO_SPEAKER_STEREO_DESCRIPTOR(UAC_BITS, UAC_EP_SIZE)
};

// ─── String descriptors ───────────────────────────────────────────────────────

static const char *string_desc[] = {
    (const char[]){0x09, 0x04},   // [0] supported language: English (0x0409)
    "Espressif",                   // [1] manufacturer
    "ESP32-S3 USB Speaker",        // [2] product
    "000001",                      // [3] serial
};

// ─── TinyUSB callbacks ────────────────────────────────────────────────────────

// Called every USB frame; audio data arrives here
bool tud_audio_rx_done_pre_read_cb(uint8_t rhport,
                                   uint16_t n_bytes_received,
                                   uint8_t func_id,
                                   uint8_t ep_out,
                                   uint8_t cur_alt_setting)
{
    (void)rhport; (void)func_id; (void)ep_out; (void)cur_alt_setting;

    // Scratch buffer sized for one full USB frame plus a little headroom
    static uint8_t buf[UAC_EP_SIZE + 8];
    uint16_t avail = tud_audio_available();
    if (avail == 0) return true;

    uint16_t rd = tud_audio_read(buf, (avail < sizeof(buf)) ? avail : sizeof(buf));
    if (rd == 0) return true;

    size_t written = 0;
    esp_err_t err = i2s_output_write(buf, rd, &written);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "i2s write err %s", esp_err_to_name(err));
    }
    return true;
}

// Volume / mute control – accept everything, ignore for now
bool tud_audio_set_req_entity_cb(uint8_t rhport,
                                 tusb_control_request_t const *p_request,
                                 uint8_t *pBuff)
{
    (void)rhport; (void)p_request; (void)pBuff;
    return true;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport,
                                 tusb_control_request_t const *p_request)
{
    (void)rhport; (void)p_request;
    return false;   // let TinyUSB reply with a stall so the host stops asking
}

// ─── TinyUSB descriptor callbacks (required by tusb_config.h) ────────────────

const uint8_t *tud_descriptor_device_cb(void)
{
    return (const uint8_t *)&device_desc;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return config_desc;
}

static uint16_t desc_str_buf[32];

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&desc_str_buf[1], string_desc[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc) / sizeof(string_desc[0])) return NULL;
        const char *str = string_desc[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) {
            desc_str_buf[1 + i] = str[i];
        }
    }
    desc_str_buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return desc_str_buf;
}

// ─── TinyUSB task ─────────────────────────────────────────────────────────────

static void usb_device_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "USB task running on core %d", xPortGetCoreID());
    while (1) {
        tud_task();   // drives TinyUSB state machine
    }
}

// ─── Public init ──────────────────────────────────────────────────────────────

esp_err_t usb_audio_init(void)
{
    ESP_LOGI(TAG, "Initialising TinyUSB UAC2 speaker");

    if (!tusb_init()) {
        ESP_LOGE(TAG, "tusb_init() failed");
        return ESP_FAIL;
    }

    // Pin the USB task to core 1 so I2S DMA (core 0) isn't interrupted
    BaseType_t ret = xTaskCreatePinnedToCore(
        usb_device_task, "usbd", 4096, NULL, configMAX_PRIORITIES - 1, NULL, 1
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create USB task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UAC2 speaker ready – 48 kHz / 16-bit / stereo");
    return ESP_OK;
}
