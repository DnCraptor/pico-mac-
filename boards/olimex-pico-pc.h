// Olimex RP2040-PICO-PC. Pin values from emu80@760330a (HW-unverified on pico-mac).
// RP2040 board: build with -DPICO_PLATFORM=rp2040. Sound is PWM-only.
#pragma once
#if PICO_RP2350
#include "boards/pico2.h"
#else
#include "boards/pico.h"
#endif

#define KBD_CLOCK_PIN 0
#define KBD_DATA_PIN  1

#define PWM_PIN0   26
#define PWM_PIN1   27
#define BEEPER_PIN 0
#define SOUND_FREQUENCY 48000
#define I2S_FREQUENCY   48000

#define SDCARD_PIN_SPI0_CS   22
#define SDCARD_PIN_SPI0_SCK  6
#define SDCARD_PIN_SPI0_MOSI 7
#define SDCARD_PIN_SPI0_MISO 4

#define USE_NESPAD     1
#define NES_GPIO_CLK   8
#define NES_GPIO_LAT   9
#define NES_GPIO_DATA  20
#define NES_GPIO_DATA2 21

#define VGA_BASE_PIN  12
#define HDMI_BASE_PIN 12

#define TFT_CS_PIN   12
#define TFT_RST_PIN  14
#define TFT_LED_PIN  15
#define TFT_DC_PIN   16
#define TFT_DATA_PIN 18
#define TFT_CLK_PIN  19
