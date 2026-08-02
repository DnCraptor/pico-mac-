// Thin Macintosh 1bpp HDMI/DVI backend on top of libdvi (Phase 3b).
//
// The Mac framebuffer is 1bpp; we letterbox it (black border) into an 800x600
// DVI frame and TMDS-encode each scanline with libdvi's monochrome path
// (DVI_MONOCHROME_TMDS=1 -> one encoded channel replicated to all 3 lanes).
//
// FIRST CUT for hardware bring-up. The parts most likely to need tuning on a
// real monitor are marked TUNE:
//   TUNE 1 (timing):   HDMI_DVI_TIMING (default 800x600p 60Hz, pixel clk 40MHz).
//   TUNE 2 (pins):     DVI_DEFAULT_SERIAL_CONFIG (set per board in CMake;
//                      murmulator_cfg = pins_tmds{8,10,12}, clk 6, inverted).
//   TUNE 3 (polarity): HDMI_MAC_INVERT (flip if black/white are swapped).
//   TUNE 4 (bit order):-DDVI_1BPP_BIT_REVERSE=1 if 8-pixel groups look mirrored.
#include "graphics.h"
#include "dvi.h"
#include "dvi_timing.h"
#include "common_dvi_pin_configs.h"
#include "tmds_encode.h"
#include "hardware/dma.h"
#include "hardware/structs/bus_ctrl.h"
#include "pico/sync.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>
#include <stdlib.h>

#ifndef HDMI_DVI_TIMING
#define HDMI_DVI_TIMING dvi_timing_800x600p_60hz     // TUNE 1
#endif
#define OUT_W 800
#define OUT_H 600
#define OUT_STRIDE (OUT_W / 8)                        // 100 bytes / scanline

#ifndef HDMI_MAC_INVERT
#define HDMI_MAC_INVERT 1                             // TUNE 3 (Mac 1=black)
#endif

static struct dvi_inst dvi0;
static const uint8_t *s_fb = 0;      // Mac 1bpp framebuffer
static uint16_t s_w = 0, s_h = 0;    // Mac resolution
static uint32_t s_line[OUT_W / 32];  // one 800px 1bpp scanline (100 bytes)
static uint8_t  bit_reverse8[256];   // per-byte MSB<->LSB (encoder is LSB-first)

// Defined in libdvi (dvi.c), default 2 (encode 300 lines, line-double to 600).
// The Mac framebuffer is 342/480 lines tall, which does not fit a 300-line
// encode, so we output the full 600 lines with no doubling. libdvi pulls one
// encoded scanline per DVI_VERTICAL_REPEAT displayed lines, so this MUST match
// the number of lines hdmi_dvi_loop() feeds (OUT_H).
extern uint8_t DVI_VERTICAL_REPEAT;

// Diagnostic halt: blink the on-board LED <code> times, pause, forever.
// (Build with -DBENCH_EMU=OFF so the LED is free for this signal.)
static void __not_in_flash_func(hdmi_dvi_halt)(int code) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
    while (true) {
        for (int i = 0; i < code; ++i) {
#ifdef PICO_DEFAULT_LED_PIN
            gpio_put(PICO_DEFAULT_LED_PIN, 1); sleep_ms(180);
            gpio_put(PICO_DEFAULT_LED_PIN, 0); sleep_ms(180);
#endif
        }
        sleep_ms(900);
    }
}

void graphics_init(void) {
    DVI_VERTICAL_REPEAT = 1;
    for (int i = 0; i < 256; ++i) {
        uint8_t r = 0, v = (uint8_t)i;
        for (int b = 0; b < 8; ++b) { r = (uint8_t)((r << 1) | (v & 1)); v >>= 1; }
        bit_reverse8[i] = r;
    }

    // Verify libdvi can obtain its resources; otherwise dvi_init() would panic
    // (or hang) silently. Mirrors the emu80 reference. Halt codes:
    //   2 blinks = fewer than 6 DMA channels free
    //   3 blinks = fewer than DVI_N_TMDS_BUFFERS TMDS buffers allocatable
    int probe[6], got = 0;
    for (; got < 6; ++got) { probe[got] = dma_claim_unused_channel(false); if (probe[got] < 0) break; }
    for (int i = 0; i < got; ++i) dma_channel_unclaim(probe[i]);
    if (got < 6) hdmi_dvi_halt(2);

    const size_t tmds_bytes = (size_t)(OUT_W / DVI_SYMBOLS_PER_WORD) * sizeof(uint32_t);
    void *pm[DVI_N_TMDS_BUFFERS]; int mg = 0;
    for (; mg < DVI_N_TMDS_BUFFERS; ++mg) { pm[mg] = malloc(tmds_bytes); if (!pm[mg]) break; }
    for (int i = 0; i < mg; ++i) free(pm[i]);
    if (mg < DVI_N_TMDS_BUFFERS) hdmi_dvi_halt(3);

    dvi0.timing  = &HDMI_DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;         // TUNE 2 (from CMake)
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());
    // Give core1 (which produces scanlines) bus priority so core0 RAM traffic
    // can't starve the TMDS output.
    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);
    dvi_get_blank_settings(&dvi0)->top = 0;
    dvi_get_blank_settings(&dvi0)->bottom = 0;
}

void graphics_set_buffer(uint8_t *buffer, uint16_t width, uint16_t height) {
    s_fb = buffer; s_w = width; s_h = height;
}

// Remaining graphics.h contract is not used by the 1bpp HDMI path.
void graphics_set_mode(enum graphics_mode_t m) { (void)m; }
void graphics_set_offset(int x, int y) { (void)x; (void)y; }
void graphics_set_palette(uint8_t i, uint32_t c) { (void)i; (void)c; }
void graphics_set_textbuffer(uint8_t *b) { (void)b; }
void graphics_set_bgcolor(uint32_t c) { (void)c; }
void graphics_set_flashmode(bool fl, bool ff) { (void)fl; (void)ff; }
void clrScr(uint8_t c) { (void)c; }

void __not_in_flash_func(hdmi_dvi_loop)(void) {
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);

    const int      mac_w   = s_w > OUT_W ? OUT_W : s_w;
    const int      mac_h   = s_h > OUT_H ? OUT_H : s_h;
    const int      voff    = (OUT_H - mac_h) / 2;
    const int      hoff_b  = ((OUT_W - mac_w) / 2) / 8;   // byte-aligned for 512/640
    const int      mac_str = mac_w / 8;
    // Encoder shifts LSB-first but Mac is MSB-first -> bit-reverse each byte.
    // Mac bit=1 is black while the encoder maps 1->white -> also invert.
    // Black (border and Mac ink after transforms) is 0x00 in the output buffer.
    const uint8_t  inv = HDMI_MAC_INVERT ? 0xFFu : 0x00u;
    uint8_t       *lb  = (uint8_t *)s_line;

    while (true) {
        for (int y = 0; y < OUT_H; ++y) {
            int my = y - voff;
            if (s_fb && my >= 0 && my < mac_h) {
                const uint8_t *src = s_fb + (unsigned)my * mac_str;
                memset(lb, 0x00, hoff_b);                          // black border
                for (int b = 0; b < mac_str; ++b)
                    lb[hoff_b + b] = bit_reverse8[src[b]] ^ inv;   // reverse + polarity
                memset(lb + hoff_b + mac_str, 0x00,
                       OUT_STRIDE - hoff_b - mac_str);
            } else {
                memset(lb, 0x00, OUT_STRIDE);                      // black
            }
            uint32_t *tmds;
            queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmds);
            tmds_encode_1bpp(s_line, tmds, OUT_W);
            queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmds);
        }
    }
}
