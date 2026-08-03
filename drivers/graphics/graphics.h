#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "stdio.h"
#include "stdint.h"

#ifdef TFT
#include "st7789.h"
#endif
#ifdef HDMI
#include "hdmi.h"
#endif
#ifdef VGA_DRV
#include "vga.h"
#endif
#ifdef TV
#include "tv.h"
#endif
#ifdef SOFTTV
#include "tv-software.h"
#endif

// Video backends (vga/tv/hdmi/tft) define the text-grid dimensions used by
// the draw_text/draw_window prototypes below. The 1bpp HDMI-DVI backend has
// no text overlay, so provide a fallback when no backend defined them.
#ifndef TEXTMODE_COLS
#define TEXTMODE_COLS 100
#endif
#ifndef TEXTMODE_ROWS
#define TEXTMODE_ROWS 37
#endif

#include "font6x8.h"
#include "font8x8.h"
#include "font8x16.h"
enum graphics_mode_t {
    TEXTMODE_DEFAULT,
    GRAPHICSMODE_DEFAULT,
};

void graphics_init();

/* Phase 2: system-clock contract. A backend whose pixel/bit clock is tied to
 * the system clock (HDMI-DVI, RGB-TV) dictates the clock; VGA tolerates any
 * clock (its pixel divider is derived from clk_sys). */
const uint32_t* graphics_get_supported_system_clocks(uint32_t* count);
bool graphics_system_clock_can_change(void);

void graphics_set_mode(enum graphics_mode_t mode);

void graphics_set_buffer(uint8_t* buffer, uint16_t width, uint16_t height);
#ifdef BENCH_EMU
void graphics_set_bench_buffer(uint8_t* buffer, uint16_t width, uint16_t height);
#endif

void graphics_set_offset(int x, int y);

void graphics_set_palette(uint8_t i, uint32_t color);

void graphics_set_textbuffer(uint8_t* buffer);

void graphics_set_bgcolor(uint32_t color888);

void graphics_set_flashmode(bool flash_line, bool flash_frame);

void draw_text(const char string[TEXTMODE_COLS + 1], uint32_t x, uint32_t y, uint8_t color, uint8_t bgcolor);
void draw_window(const char title[TEXTMODE_COLS + 1], uint32_t x, uint32_t y, uint32_t width, uint32_t height);

void clrScr(uint8_t color);

#ifdef __cplusplus
}
#endif
