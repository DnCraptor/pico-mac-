// Olimex RP2040-PICO-PC with a Raspberry Pi Pico 2 (RP2350A) - "PCp2" in the
// pico-class kit. Pins as in emu80v4 (PCp2 builds accepted on this board).
//   HDMI   GPIO12..19: clock 12/13, D0 14/15, D2 16/17, D1 18/19, pairs
//          inverted (libdvi Olimex_RP2040_PICO_PC_cfg). The HDMI clock comes
//          from PIO (DVI_USE_PIO_CLOCK): PWM slice 6 of GPIO12/13 is needed
//          by the left audio channel on GPIO28.
//   Sound  PWM audio jack only: left GPIO28, right GPIO27. GPIO26 is DVI_CEC
//          and is not wired to the jack. GPIO23 high keeps the Pico's SMPS
//          out of its noisy power-save mode.
//   PS/2   keyboard on GPIO0 (clock) / GPIO1 (data).
//   SD     SPI0: SCK 6, MOSI 7, MISO 4, CS 22.
//   NES    pad on UEXT: CLK GPIO5 (GPIO8 is the optional PSRAM CS on this
//          board), LAT 9, DATA 20.
#pragma once
#if PICO_RP2350
#include "boards/pico2.h"
#else
#include "boards/pico.h"
#endif

#define PICO_PC 1

#define PS2KBD_GPIO_FIRST 0
#define KBD_CLOCK_PIN PS2KBD_GPIO_FIRST
#define KBD_DATA_PIN  (PS2KBD_GPIO_FIRST + 1)

// The Mac sound is played on BEEPER_PIN and mirrored to BEEPER_PIN_R.
#define PWM_PIN0     28
#define PWM_PIN1     27
#define BEEPER_PIN   28
#define BEEPER_PIN_R 27
#define SMPS_MODE_PIN 23
#define SOUND_FREQUENCY 48000
#define I2S_FREQUENCY   48000

#define SDCARD_PIN_SPI0_CS   22
#define SDCARD_PIN_SPI0_SCK  6
#define SDCARD_PIN_SPI0_MOSI 7
#define SDCARD_PIN_SPI0_MISO 4

// The PSRAM driver headers need these to compile; the emulator does not use
// PSRAM (PSRAM is not defined), so the pins are never touched.
#define PSRAM_SPINLOCK 1
#define PSRAM_ASYNC    1
#define PSRAM_PIN_CS   8
#define PSRAM_PIN_SCK  6
#define PSRAM_PIN_MOSI 7
#define PSRAM_PIN_MISO 4

#define USE_NESPAD     1
#define NES_GPIO_CLK   5
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
