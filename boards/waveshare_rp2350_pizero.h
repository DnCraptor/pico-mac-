// Waveshare RP2350-PiZero (RP2350B, 16 MB flash) - "z0p2" in the pico-class
// kit. Pins as in the other z0p2 builds of the kit (MOS2, pico-nes, emu80v4):
//   HDMI   GPIO32..39: D2 32/33, D1 34/35, D0 36/37, clock 38/39, pairs not
//          inverted (libdvi waveshare_rp2350_pizero_hdmi_cfg). PIO0 reaches
//          them through GPIO base 16 (set in drivers/hdmi-dvi before dvi_init).
//   Sound  I2S module (PCM5102A) on PIO1: DIN GP10, BCK GP11, LRCK GP12.
//   PS/2   keyboard: CLK GP2, DATA GP3 (ps2kbd_mrmltr2.pio).
//   SD     SPI1: SCK 30, MOSI 31, MISO 40, CS 43.
//   NES    pad: CLK GP4, LAT GP5, DATA GP7.
// Do not include boards/pico2.h: it makes the SDK treat the chip as RP2350A,
// and GPIO32..39 (HDMI) are then not usable.
#pragma once

#define ZERO2 1

#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

#define SDCARD_SPI_BUS       spi1
#define SDCARD_PIN_SPI0_CS   43
#define SDCARD_PIN_SPI0_SCK  30
#define SDCARD_PIN_SPI0_MOSI 31
#define SDCARD_PIN_SPI0_MISO 40

#define PS2KBD_GPIO_FIRST 2
#define KBD_CLOCK_PIN PS2KBD_GPIO_FIRST
#define KBD_DATA_PIN  (PS2KBD_GPIO_FIRST + 1)

// I2S sound (cmake I2S=ON, the z0p2 default)
#define AUDIO_DATA_PIN  10
#define AUDIO_CLOCK_PIN 11
// PWM sound on the same pins (cmake I2S=OFF)
#define PWM_PIN0     10
#define PWM_PIN1     11
#define BEEPER_PIN   10
#define BEEPER_PIN_R 11
#define SOUND_FREQUENCY 48000
#define I2S_FREQUENCY   48000

// The PSRAM driver headers need these to compile; the emulator does not use
// PSRAM (PSRAM is not defined), so the pins are never touched.
#define PSRAM_SPINLOCK 1
#define PSRAM_ASYNC    1
#define PSRAM_PIN_CS   47
#define PSRAM_PIN_SCK  30
#define PSRAM_PIN_MOSI 31
#define PSRAM_PIN_MISO 40

#define USE_NESPAD     1
#define NES_GPIO_CLK   4
#define NES_GPIO_LAT   5
#define NES_GPIO_DATA  7

#define HDMI_BASE_PIN 32
