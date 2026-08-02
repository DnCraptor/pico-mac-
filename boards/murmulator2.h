// Murmulator 2.0. Pins reproduced verbatim from the original pico-mac- MURM2 block.
#pragma once
#if PICO_RP2350
#include "boards/pico2.h"
#else
#include "boards/pico.h"
#endif

#define KBD_CLOCK_PIN 2
#define KBD_DATA_PIN  3

#define LOAD_WAV_PIO   22
#define BEEPER_PIN     9
#define PWM_PIN0       10
#define PWM_PIN1       11
#define SOUND_FREQUENCY 50000

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

#define VGA_BASE_PIN  12
#define HDMI_BASE_PIN 12

#define TFT_CS_PIN   12
#define TFT_RST_PIN  14
#define TFT_LED_PIN  15
#define TFT_DC_PIN   16
#define TFT_DATA_PIN 18
#define TFT_CLK_PIN  19
