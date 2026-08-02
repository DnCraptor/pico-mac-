// Waveshare RP2040-PiZero. Pin values from emu80@760330a (HW-unverified on pico-mac).
// RP2040 board: build with -DPICO_PLATFORM=rp2040.
#pragma once
#if PICO_RP2350
#include "boards/pico2.h"
#else
#include "boards/pico.h"
#endif

#define KBD_CLOCK_PIN 0
#define KBD_DATA_PIN  1

#define PWM_PIN0   14
#define PWM_PIN1   15
#define BEEPER_PIN 0
#define SOUND_FREQUENCY 48000
#define I2S_FREQUENCY   48000

#define SDCARD_PIN_SPI0_CS   21
#define SDCARD_PIN_SPI0_SCK  18
#define SDCARD_PIN_SPI0_MOSI 19
#define SDCARD_PIN_SPI0_MISO 20

#define USE_NESPAD     1
#define NES_GPIO_CLK   7
#define NES_GPIO_LAT   8
#define NES_GPIO_DATA  9
#define NES_GPIO_DATA2 10

#define VGA_BASE_PIN  22
#define HDMI_BASE_PIN 22

#define TFT_CS_PIN   22
#define TFT_RST_PIN  24
#define TFT_LED_PIN  25
#define TFT_DC_PIN   26
#define TFT_DATA_PIN 28
#define TFT_CLK_PIN  29
