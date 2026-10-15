// Murmulator 2.0 with a Raspberry Pi Pico 2 (RP2350A, 4 MB flash) - "m2p2" in
// the pico-class kit:
//   HDMI/VGA GPIO12..19 (HDMI: clock 12/13, D0 14/15, D1 16/17, D2 18/19).
//   Sound  PWM_Enable jumper: TDA1387 I2S (DIN 9, BCK 10, LRCK 11) or PWM
//          (left GP11, right GP10 - one PWM slice). GPIO23 high keeps the
//          Pico's SMPS out of its noisy power-save mode.
//   PS/2   keyboard: CLK GP2, DATA GP3 (ps2kbd_mrmltr2.pio).
//   SD     SPI0: CS 5, SCK 6, MOSI 7, MISO 4. PSRAM CS 8.
//   NES    pads: CLK 20, LAT 21, DATA 26/27.
#pragma once
#if PICO_RP2350
#include "boards/pico2.h"
#else
#include "boards/pico.h"
#endif

#define PS2KBD_GPIO_FIRST 2
#define KBD_CLOCK_PIN PS2KBD_GPIO_FIRST
#define KBD_DATA_PIN  (PS2KBD_GPIO_FIRST + 1)

// I2S sound (cmake I2S=ON, the m2p2 default)
#define AUDIO_DATA_PIN  9
#define AUDIO_CLOCK_PIN 10
// PWM sound (cmake I2S=OFF). The Mac sound is played on BEEPER_PIN and
// mirrored to BEEPER_PIN_R.
#define PWM_PIN0     10
#define PWM_PIN1     11
#define BEEPER_PIN   11
#define BEEPER_PIN_R 10
#define SMPS_MODE_PIN 23
#define SOUND_FREQUENCY 48000
#define I2S_FREQUENCY   48000

#define SDCARD_PIN_SPI0_CS   5
#define SDCARD_PIN_SPI0_SCK  6
#define SDCARD_PIN_SPI0_MOSI 7
#define SDCARD_PIN_SPI0_MISO 4

#define PSRAM_SPINLOCK 1
#define PSRAM_ASYNC    1
#define PSRAM_PIN_CS   8
#define PSRAM_PIN_SCK  6
#define PSRAM_PIN_MOSI 7
#define PSRAM_PIN_MISO 4

#define USE_NESPAD    1
#define NES_GPIO_CLK  20
#define NES_GPIO_LAT  21
#define NES_GPIO_DATA 26
#define NES_GPIO_DATA2 27

#define VGA_BASE_PIN  12
#define HDMI_BASE_PIN 12

#define TFT_CS_PIN   12
#define TFT_RST_PIN  14
#define TFT_LED_PIN  15
#define TFT_DC_PIN   16
#define TFT_DATA_PIN 18
#define TFT_CLK_PIN  19
