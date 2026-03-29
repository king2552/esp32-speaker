#pragma once

// ─── TinyUSB core ────────────────────────────────────────────────────────────
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED
#define CFG_TUSB_OS             OPT_OS_FREERTOS
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN      __attribute__((aligned(4)))

// Debug: set to 2 for verbose TinyUSB logs, 0 for none
#define CFG_TUSB_DEBUG          0

// ─── Device ───────────────────────────────────────────────────────────────────
#define CFG_TUD_ENDPOINT0_SIZE  64

// ─── Audio Class ─────────────────────────────────────────────────────────────
#define CFG_TUD_AUDIO           1       // 1 audio function

// Speaker only (no mic), so only OUT direction
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN               TUD_AUDIO_SPEAKER_STEREO_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT               1
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ            64

// EP OUT FIFO (at least 1 ms of audio at 48 kHz stereo 16-bit = 192 bytes)
// Use 2× for safety
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX          400
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ       400

// No IN endpoint (microphone disabled)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX           0
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ        0

// Disable MIDI, CDC, MSC, HID, vendor
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_HID             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0
