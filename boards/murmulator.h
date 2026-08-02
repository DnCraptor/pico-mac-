// Murmulator 1.x (classic MURMULATOR). Pins reproduced verbatim from the
// original pico-mac- default build; the resulting macro set is identical.
#pragma once
#if PICO_RP2350
#include "boards/pico2.h"
#else
#include "boards/pico.h"
#endif

// PS/2 keyboard
#define KBD_CLOCK_PIN 0
#define KBD_DATA_PIN  1

// Sound
#define LOAD_WAV_PIO   22
#define PWM_PIN0       26
#define PWM_PIN1       27
#define BEEPER_PIN     28
#define SOUND_FREQUENCY 50000
#define I2S_FREQUENCY   96000

// SD card (SPI0)
#define SDCARD_PIN_SPI0_CS   5
#define SDCARD_PIN_SPI0_SCK  2
#define SDCARD_PIN_SPI0_MOSI 3
#define SDCARD_PIN_SPI0_MISO 4

// PSRAM (present but unused by the emulator; do NOT define PSRAM here)
#define PSRAM_SPINLOCK 1
#define PSRAM_ASYNC    1
#define PSRAM_PIN_CS   18
#define PSRAM_PIN_SCK  19
#define PSRAM_PIN_MOSI 20
#define PSRAM_PIN_MISO 21

// NES / Dendy gamepad
#define USE_NESPAD    1
#define NES_GPIO_CLK  14
#define NES_GPIO_LAT  15
#define NES_GPIO_DATA 16

// VGA / HDMI 8-pin block base
#define VGA_BASE_PIN  6
#define HDMI_BASE_PIN 6

// TFT
#define TFT_CS_PIN   6
#define TFT_RST_PIN  8
#define TFT_LED_PIN  9
#define TFT_DC_PIN   10
#define TFT_DATA_PIN 12
#define TFT_CLK_PIN  13
