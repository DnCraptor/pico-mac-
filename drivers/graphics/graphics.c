#include "graphics.h"
#include <string.h>

/* Phase 2: system-clock contract (see graphics.h). */
const uint32_t* graphics_get_supported_system_clocks(uint32_t* count) {
#ifdef HDMI_DVI
    static const uint32_t clocks[] = { 400 };            /* 800x600@60: TMDS bit = sysclk */
#elif defined(PICO_RP2040)
    static const uint32_t clocks[] = { 378, 400, 408 };
#else
    static const uint32_t clocks[] = { 378, 400, 440, 480 };
#endif
    if (count) *count = sizeof(clocks) / sizeof(clocks[0]);
    return clocks;
}

bool graphics_system_clock_can_change(void) {
#if defined(HDMI_DVI) || defined(RGB_TV)
    return false;   /* pixel/bit clock is tied to the system clock */
#else
    return true;    /* VGA derives its pixel clock from clk_sys */
#endif
}
/**
void draw_text(const char string[TEXTMODE_COLS + 1], uint32_t x, uint32_t y, uint8_t color, uint8_t bgcolor) {
if (!text_buffer) return;
    uint8_t* t_buf = text_buffer + TEXTMODE_COLS * 2 * y + 2 * x;
    for (int xi = TEXTMODE_COLS * 2; xi--;) {
        if (!*string) break;
        *t_buf++ = *string++;
        *t_buf++ = bgcolor << 4 | color & 0xF;
    }
}
*/
void draw_window(const char title[TEXTMODE_COLS + 1], uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    char line[width + 1];
    memset(line, 0, sizeof line);
    width--;
    height--;
    // Рисуем рамки

    memset(line, 0xCD, width); // ═══


    line[0] = 0xC9; // ╔
    line[width] = 0xBB; // ╗
    draw_text(line, x, y, 11, 1);

    line[0] = 0xC8; // ╚
    line[width] = 0xBC; //  ╝
    draw_text(line, x, height + y, 11, 1);

    memset(line, ' ', width);
    line[0] = line[width] = 0xBA;

    for (int i = 1; i < height; i++) {
        draw_text(line, x, y + i, 11, 1);
    }

    snprintf(line, width - 1, " %s ", title);
    draw_text(line, x + (width - strlen(line)) / 2, y, 14, 3);
}
