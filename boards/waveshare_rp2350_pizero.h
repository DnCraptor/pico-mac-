// Waveshare RP2350-PiZero. Pin values from emu80@760330a (HW-unverified on pico-mac).
// HDMI-oriented board (no VGA pin block). SD is on SPI1.
#pragma once
#include "boards/pico2.h"

#define SDCARD_SPI_BUS       spi1
#define SDCARD_PIN_SPI0_CS   43
#define SDCARD_PIN_SPI0_SCK  30
#define SDCARD_PIN_SPI0_MOSI 31
#define SDCARD_PIN_SPI0_MISO 40

// GP2 = CLOCK, GP3 = DATA; PIO program waits for clock on GP2
#define PS2KBD_REVERSED_PINS 1
#define KBD_CLOCK_PIN 2
#define KBD_DATA_PIN  3

#define PWM_PIN0   10
#define PWM_PIN1   11
#define BEEPER_PIN 0
#define SOUND_FREQUENCY 48000
#define I2S_FREQUENCY   48000

#define USE_NESPAD     1
#define NES_GPIO_CLK   4
#define NES_GPIO_LAT   5
#define NES_GPIO_DATA  7

#define HDMI_BASE_PIN 32
