# ESP32-S3 USB Speaker

Turns an ESP32-S3 into a USB Audio Class 2.0 (UAC2) speaker. Your PC sees it as a standard audio output device — no drivers needed on Windows 10/11, macOS, or Linux.

Audio path: **PC → USB → ESP32-S3 → I2S → DAC → Amp → Speaker**

---

## Hardware Required

| Part | Example |
|------|---------|
| ESP32-S3 dev board | ESP32-S3-DevKitC-1 |
| I2S DAC | PCM5102A module |
| Amp | PAM8403 or MAX98357A (has built-in amp + DAC) |
| Speaker | 4–8 Ω, 3–5 W |

> **If you use a MAX98357A** it has its own I2S DAC + amp in one chip. Wire it directly and skip the PAM8403.

---

## Wiring

### ESP32-S3 → PCM5102A

```
ESP32-S3 GPIO 12  →  PCM5102A BCK  (bit clock)
ESP32-S3 GPIO 13  →  PCM5102A LCK  (word select / LRCLK)
ESP32-S3 GPIO 14  →  PCM5102A DIN  (serial data)
3.3 V             →  PCM5102A VCC  (or 5 V if module has regulator)
GND               →  PCM5102A GND
PCM5102A FLT      →  GND           (normal filter roll-off)
PCM5102A DEMP     →  GND           (de-emphasis off)
PCM5102A XSMT     →  3.3 V         (unmute)
PCM5102A FMT      →  GND           (I2S standard format)
```

PCM5102A AOUT L/R → PAM8403 inputs → speakers

### ESP32-S3 → MAX98357A (all-in-one alternative)

```
ESP32-S3 GPIO 12  →  MAX98357A BCLK
ESP32-S3 GPIO 13  →  MAX98357A LRC
ESP32-S3 GPIO 14  →  MAX98357A DIN
3.3 V / 5 V       →  MAX98357A VIN
GND               →  MAX98357A GND
MAX98357A OUT+/-  →  Speaker
```

### USB (native OTG port)

Use the **native USB port** on your ESP32-S3 board — on the DevKitC-1 this is the port labelled **USB** (not the UART/JTAG port). The pins are internally connected to GPIO 19 (D−) and GPIO 20 (D+); no extra wiring needed.

---

## Pin Changes

To use different GPIO pins, edit the top of `src/i2s_output.h`:

```c
#define I2S_BCK_PIN     12
#define I2S_WS_PIN      13
#define I2S_DATA_PIN    14
```

---

## Build & Flash

This is an **ESP-IDF project** (not Arduino). Use PlatformIO with the ESP-IDF framework, or the ESP-IDF CLI directly.

### PlatformIO (recommended)

```bash
pio run --target upload
pio device monitor
```

### ESP-IDF CLI

```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

---

## Using It

1. Flash the firmware and connect the **native USB port** to your PC.
2. Windows: go to **Sound settings → Output** and select **"ESP32-S3 USB Speaker"**.
3. macOS: **System Settings → Sound → Output**.
4. Linux: `pavucontrol` or `aplay -l` to see the new device.
5. Play audio — it should come out of your speaker.

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Device not detected | Make sure you're plugged into the **native USB** port, not the UART/JTAG port |
| Device detected but no sound | Check I2S pin wiring; confirm DAC is powered and not muted |
| Crackling / glitches | Try increasing `DMA_BUFFER_COUNT` in `i2s_output.h` (e.g. 16) |
| Build fails on `TUD_AUDIO_SPEAKER_STEREO_DESCRIPTOR` | Update TinyUSB — older ESP-IDF versions (<5.x) may need manual TinyUSB update |
| `tusb_init()` fails | Ensure `CONFIG_USB_OTG_SUPPORTED=y` in sdkconfig |

---

## Architecture Notes

- **UAC2** is used (not UAC1) because it's natively supported on modern OSes without drivers and supports higher quality audio.
- Audio data arrives in the **TinyUSB task** via `tud_audio_rx_done_pre_read_cb()` and is written directly to the I2S DMA ring buffer.
- The USB task is pinned to **core 1**; I2S DMA runs on **core 0** to avoid contention.
- The I2S driver's `auto_clear = true` outputs silence if the DMA starves, preventing glitchy noise when the host pauses.
- Sample rate is fixed at **48 kHz** (most compatible). To change it, update `SAMPLE_RATE` in `i2s_output.h` *and* `UAC_SAMPLE_RATE` in `usb_audio.c`.
