#include <cstdio>
#include <cstring>
#include <cstdarg>

#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/pio.h>
#include <hardware/i2c.h>
#include <hardware/vreg.h>
#include <hardware/sync.h>
#include <hardware/flash.h>

#include "graphics.h"

#include "audio.h"
#include "ff.h"
#include "psram_spi.h"
#ifdef KBDUSB
    #include "ps2kbd_mrmltr.h"
#else
    #include "ps2.h"
#endif

#if USE_NESPAD
#include "nespad.h"
#endif

#pragma GCC optimize("Ofast")

#define HOME_DIR (char*)"\\mac+"

extern "C" {
#include "disc.h"
#include "umac.h"
#include "kbd.h"
}

extern int cursor_x;
extern int cursor_y;
extern int cursor_button;
int cursor_joy_inc_dec = 1;

// Mac binary data:  disc and ROM images
static const uint8_t __in_flash() __aligned(4096) umac_disc[400 << 10] = {
#include "umac-disc.h"
};
static const uint8_t __in_flash() __aligned(4096) umac_rom[128 << 10] = {
#include "umac-rom.h"
};
static uint8_t umac_ram[RAM_SIZE];

struct semaphore vga_start_semaphore;

static FATFS fs;

struct input_bits_t {
    bool a: true;
    bool b: true;
    bool select: true;
    bool start: true;
    bool right: true;
    bool left: true;
    bool up: true;
    bool down: true;
};

input_bits_t gamepad1_bits = { false, false, false, false, false, false, false, false };
static input_bits_t gamepad2_bits = { false, false, false, false, false, false, false, false };

/* Renderer loop on Pico's second core */
///void repeat_handler(void);

extern "C" bool handleScancode(const uint32_t ps2scancode) {
    #if 0
    if (ps2scancode != 0x45 && ps2scancode != 0x1D && ps2scancode != 0xC5) {
        char tmp1[16];
        snprintf(tmp1, 16, "%08X", ps2scancode);
        OSD::osdCenteredMsg(tmp1, LEVEL_WARN, 500);
    }
    #endif
    static bool pause_detected = false;
    if (pause_detected) {
        pause_detected = false;
        if (ps2scancode == 0x1D) return true; // ignore next byte after 0x45, TODO: split with NumLock
    }
    if ( ((ps2scancode >> 8) & 0xFF) == 0xE0) { // E0 block
        uint8_t cd = ps2scancode & 0xFF;
        bool pressed = cd < 0x80;
        cd &= 0x7F;
        /**
        switch (cd) {
            case 0x5B: kbdPushData(fabgl::VirtualKey::VK_LCTRL, pressed); return true; /// L WIN
            case 0x1D: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_ALTFIRE, pressed);
                kbdPushData(fabgl::VirtualKey::VK_RCTRL, pressed);
                return true;
            }
            case 0x38: kbdPushData(fabgl::VirtualKey::VK_RALT, pressed); return true;
            case 0x5C: {  /// R WIN
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_ALTFIRE, pressed);
                kbdPushData(fabgl::VirtualKey::VK_RCTRL, pressed);
                return true;
            }
            case 0x5D: kbdPushData(fabgl::VirtualKey::VK_F1, pressed); return true; /// MENU
            case 0x37: kbdPushData(fabgl::VirtualKey::VK_PRINTSCREEN, pressed); return true;
            case 0x46: kbdPushData(fabgl::VirtualKey::VK_BREAK, pressed); return true;
            case 0x52: kbdPushData(fabgl::VirtualKey::VK_INSERT, pressed); return true;
            case 0x47: {
                joyPushData(fabgl::VirtualKey::VK_MENU_HOME, pressed);
                kbdPushData(fabgl::VirtualKey::VK_HOME, pressed);
                return true;
            }
            case 0x4F: kbdPushData(fabgl::VirtualKey::VK_END, pressed); return true;
            case 0x49: kbdPushData(fabgl::VirtualKey::VK_PAGEUP, pressed); return true;
            case 0x51: kbdPushData(fabgl::VirtualKey::VK_PAGEDOWN, pressed); return true;
            case 0x53: kbdPushData(fabgl::VirtualKey::VK_DELETE, pressed); return true;
            case 0x48: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_UP, pressed);
                joyPushData(fabgl::VirtualKey::VK_MENU_UP, pressed);
                kbdPushData(fabgl::VirtualKey::VK_UP, pressed);
                return true;
            }
            case 0x50: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
                joyPushData(fabgl::VirtualKey::VK_MENU_DOWN, pressed);
                kbdPushData(fabgl::VirtualKey::VK_DOWN, pressed);
                return true;
            }
            case 0x4B: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_LEFT, pressed);
                joyPushData(fabgl::VirtualKey::VK_MENU_LEFT, pressed);
                kbdPushData(fabgl::VirtualKey::VK_LEFT, pressed);
                return true;
            }
            case 0x4D: {
                if (Config::CursorAsJoy) joyPushData(fabgl::VirtualKey::VK_DPAD_RIGHT, pressed);
                joyPushData(fabgl::VirtualKey::VK_MENU_RIGHT, pressed);
                kbdPushData(fabgl::VirtualKey::VK_RIGHT, pressed);
                return true;
            }
            case 0x35: kbdPushData(fabgl::VirtualKey::VK_SLASH, pressed); return true;
            case 0x1C: { // VK_KP_ENTER
                kbdPushData(Config::rightSpace ? fabgl::VirtualKey::VK_SPACE : fabgl::VirtualKey::VK_RETURN, pressed);
                return true;
            }
        }
        */
        return true;
    }
    uint8_t cd = ps2scancode & 0xFF;
    bool pressed = cd < 0x80;
    cd &= 0x7F;
    /**
    switch (cd) {
        case 0x1E: kbdPushData(fabgl::VirtualKey::VK_A, pressed); return true;
        case 0x30: kbdPushData(fabgl::VirtualKey::VK_B, pressed); return true;
        case 0x2E: kbdPushData(fabgl::VirtualKey::VK_C, pressed); return true;
        case 0x20: kbdPushData(fabgl::VirtualKey::VK_D, pressed); return true;
        case 0x12: kbdPushData(fabgl::VirtualKey::VK_E, pressed); return true;
        case 0x21: kbdPushData(fabgl::VirtualKey::VK_F, pressed); return true;
        case 0x22: kbdPushData(fabgl::VirtualKey::VK_G, pressed); return true;
        case 0x23: kbdPushData(fabgl::VirtualKey::VK_H, pressed); return true;
        case 0x17: kbdPushData(fabgl::VirtualKey::VK_I, pressed); return true;
        case 0x24: kbdPushData(fabgl::VirtualKey::VK_J, pressed); return true;
        case 0x25: kbdPushData(fabgl::VirtualKey::VK_K, pressed); return true;
        case 0x26: kbdPushData(fabgl::VirtualKey::VK_L, pressed); return true;
        case 0x32: kbdPushData(fabgl::VirtualKey::VK_M, pressed); return true;
        case 0x31: kbdPushData(fabgl::VirtualKey::VK_N, pressed); return true;
        case 0x18: kbdPushData(fabgl::VirtualKey::VK_O, pressed); return true;
        case 0x19: kbdPushData(fabgl::VirtualKey::VK_P, pressed); return true;
        case 0x10: kbdPushData(fabgl::VirtualKey::VK_Q, pressed); return true;
        case 0x13: kbdPushData(fabgl::VirtualKey::VK_R, pressed); return true;
        case 0x1F: kbdPushData(fabgl::VirtualKey::VK_S, pressed); return true;
        case 0x14: kbdPushData(fabgl::VirtualKey::VK_T, pressed); return true;
        case 0x16: kbdPushData(fabgl::VirtualKey::VK_U, pressed); return true;
        case 0x2F: kbdPushData(fabgl::VirtualKey::VK_V, pressed); return true;
        case 0x11: kbdPushData(fabgl::VirtualKey::VK_W, pressed); return true;
        case 0x2D: kbdPushData(fabgl::VirtualKey::VK_X, pressed); return true;
        case 0x15: kbdPushData(fabgl::VirtualKey::VK_Y, pressed); return true;
        case 0x2C: kbdPushData(fabgl::VirtualKey::VK_Z, pressed); return true;

        case 0x0B: kbdPushData(fabgl::VirtualKey::VK_0, pressed); return true;
        case 0x02: kbdPushData(fabgl::VirtualKey::VK_1, pressed); return true;
        case 0x03: kbdPushData(fabgl::VirtualKey::VK_2, pressed); return true;
        case 0x04: kbdPushData(fabgl::VirtualKey::VK_3, pressed); return true;
        case 0x05: kbdPushData(fabgl::VirtualKey::VK_4, pressed); return true;
        case 0x06: kbdPushData(fabgl::VirtualKey::VK_5, pressed); return true;
        case 0x07: kbdPushData(fabgl::VirtualKey::VK_6, pressed); return true;
        case 0x08: kbdPushData(fabgl::VirtualKey::VK_7, pressed); return true;
        case 0x09: kbdPushData(fabgl::VirtualKey::VK_8, pressed); return true;
        case 0x0A: kbdPushData(fabgl::VirtualKey::VK_9, pressed); return true;

        case 0x29: kbdPushData(fabgl::VirtualKey::VK_TILDE, pressed); return true;
        case 0x0C: kbdPushData(fabgl::VirtualKey::VK_MINUS, pressed); return true;
        case 0x0D: kbdPushData(fabgl::VirtualKey::VK_EQUALS, pressed); return true;
        case 0x2B: kbdPushData(fabgl::VirtualKey::VK_BACKSLASH, pressed); return true;
        case 0x1A: kbdPushData(fabgl::VirtualKey::VK_LEFTBRACKET, pressed); return true;
        case 0x1B: kbdPushData(fabgl::VirtualKey::VK_RIGHTBRACKET, pressed); return true;
        case 0x27: kbdPushData(fabgl::VirtualKey::VK_SEMICOLON, pressed); return true;
        case 0x28: kbdPushData(fabgl::VirtualKey::VK_QUOTE, pressed); return true;
        case 0x33: kbdPushData(fabgl::VirtualKey::VK_COMMA, pressed); return true;
        case 0x34: kbdPushData(fabgl::VirtualKey::VK_PERIOD, pressed); return true;
        case 0x35: kbdPushData(fabgl::VirtualKey::VK_SLASH, pressed); return true;

        case 0x0E: {
            joyPushData(fabgl::VirtualKey::VK_MENU_BS, pressed);
            kbdPushData(fabgl::VirtualKey::VK_BACKSPACE, pressed);
            return true;
        }
        case 0x39: {
            joyPushData(fabgl::VirtualKey::VK_MENU_ENTER, pressed);
            kbdPushData(fabgl::VirtualKey::VK_SPACE, pressed);
            return true;
        }
        case 0x0F: {
            if (Config::TABasfire1) JPAD(fabgl::VirtualKey::VK_DPAD_FIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_TAB, pressed);
            return true;
        }
        case 0x3A: kbdPushData(fabgl::VirtualKey::VK_CAPSLOCK, pressed); return true; /// TODO: CapsLock
        case 0x2A: kbdPushData(fabgl::VirtualKey::VK_LSHIFT, pressed); return true;
        case 0x1D: kbdPushData(fabgl::VirtualKey::VK_LCTRL, pressed); return true;
        case 0x38: {
            if (Config::CursorAsJoy) JPAD(fabgl::VirtualKey::VK_DPAD_FIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_LALT, pressed);
            return true;
        }
        case 0x36: kbdPushData(fabgl::VirtualKey::VK_RSHIFT, pressed); return true;
        case 0x1C: {
            joyPushData(fabgl::VirtualKey::VK_MENU_ENTER, pressed);
            kbdPushData(fabgl::VirtualKey::VK_RETURN, pressed);
            return true;
        }
        case 0x01: kbdPushData(fabgl::VirtualKey::VK_ESCAPE, pressed); return true;
        case 0x3B: kbdPushData(fabgl::VirtualKey::VK_F1, pressed); return true;
        case 0x3C: kbdPushData(fabgl::VirtualKey::VK_F2, pressed); return true;
        case 0x3D: kbdPushData(fabgl::VirtualKey::VK_F3, pressed); return true;
        case 0x3E: kbdPushData(fabgl::VirtualKey::VK_F4, pressed); return true;
        case 0x3F: kbdPushData(fabgl::VirtualKey::VK_F5, pressed); return true;
        case 0x40: kbdPushData(fabgl::VirtualKey::VK_F6, pressed); return true;
        case 0x41: kbdPushData(fabgl::VirtualKey::VK_F7, pressed); return true;
        case 0x42: kbdPushData(fabgl::VirtualKey::VK_F8, pressed); return true;
        case 0x43: kbdPushData(fabgl::VirtualKey::VK_F9, pressed); return true;
        case 0x44: kbdPushData(fabgl::VirtualKey::VK_F10, pressed); return true;
        case 0x57: kbdPushData(fabgl::VirtualKey::VK_F11, pressed); return true;
        case 0x58: kbdPushData(fabgl::VirtualKey::VK_F12, pressed); return true;

        case 0x46: kbdPushData(fabgl::VirtualKey::VK_SCROLLLOCK, pressed); return true; /// TODO:
        case 0x45: {
            kbdPushData(fabgl::VirtualKey::VK_PAUSE, pressed);
            pause_detected = pressed;
            return true;
        }
        case 0x37: {
            JPAD(fabgl::VirtualKey::VK_DPAD_START, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_MULTIPLY, pressed);
            return true;
        }
        case 0x4A: {
            JPAD(fabgl::VirtualKey::VK_DPAD_SELECT, pressed);
            kbdPushData(fabgl::VirtualKey::VK_MINUS, pressed);
            return true;
        }
        case 0x4E: {
            JPAD(fabgl::VirtualKey::VK_DPAD_FIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_PLUS, pressed);
            return true;
        }
        case 0x53: {
            JPAD(fabgl::VirtualKey::VK_DPAD_FIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_PERIOD, pressed);
            return true;
        }
        case 0x52: {
            JPAD(fabgl::VirtualKey::VK_DPAD_ALTFIRE, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_0, pressed);
            return true;
        }
        case 0x4F: {
            JPAD(fabgl::VirtualKey::VK_DPAD_LEFT, pressed);
            JPAD(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_1, pressed);
            return true;
        }
        case 0x50: {
            JPAD(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_2, pressed);
            return true;
        }
        case 0x51: {
            JPAD(fabgl::VirtualKey::VK_DPAD_RIGHT, pressed);
            JPAD(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_3, pressed);
            return true;
        }
        case 0x4B: {
            JPAD(fabgl::VirtualKey::VK_DPAD_LEFT, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_4, pressed);
            return true;
        }
        case 0x4C: {
            JPAD(fabgl::VirtualKey::VK_DPAD_DOWN, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_5, pressed);
            return true;
        }
        case 0x4D: {
            JPAD(fabgl::VirtualKey::VK_DPAD_RIGHT, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_6, pressed);
            return true;
        }
        case 0x47: {
            JPAD(fabgl::VirtualKey::VK_DPAD_LEFT, pressed);
            JPAD(fabgl::VirtualKey::VK_DPAD_UP, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_7, pressed);
            return true;
        }
        case 0x48: {
            JPAD(fabgl::VirtualKey::VK_DPAD_UP, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_8, pressed);
            return true;
        }
        case 0x49: {
            JPAD(fabgl::VirtualKey::VK_DPAD_RIGHT, pressed);
            JPAD(fabgl::VirtualKey::VK_DPAD_UP, pressed);
            kbdPushData(fabgl::VirtualKey::VK_KP_9, pressed);
            return true;
        }
    }
    */
    return true;
}


#if USE_NESPAD

static void nespad_tick1(void) {
    nespad_read();
    bool a = (nespad_state & DPAD_A) != 0;
    bool up = (nespad_state & DPAD_UP) != 0;
    bool down = (nespad_state & DPAD_DOWN) != 0;
    bool left = (nespad_state & DPAD_LEFT) != 0;
    bool right = (nespad_state & DPAD_RIGHT) != 0;
    if (!gamepad1_bits.a && a) {
        cursor_button = true;
    } else if (gamepad1_bits.a && !a) {
        cursor_button = false;
    }
    if ((gamepad1_bits.up && up) || (gamepad1_bits.down && down) || (gamepad1_bits.left && left) || (gamepad1_bits.right && right)) {
        cursor_joy_inc_dec++;
        if (cursor_joy_inc_dec > 10) cursor_joy_inc_dec = 10;
    } else {
        cursor_joy_inc_dec = 1;
    }
    gamepad1_bits.a = a;
    gamepad1_bits.b = (nespad_state & DPAD_B) != 0;
    gamepad1_bits.start = (nespad_state & DPAD_START) != 0;
    gamepad1_bits.up = up;
    gamepad1_bits.down = down;
    gamepad1_bits.left = left;
    gamepad1_bits.right = right;

    if (gamepad1_bits.up) {
        cursor_y -= cursor_joy_inc_dec;
    }
    if (gamepad1_bits.down) {
        cursor_y += cursor_joy_inc_dec;
    }
    if (gamepad1_bits.left) {
        cursor_x -= cursor_joy_inc_dec;
    }
    if (gamepad1_bits.right) {
        cursor_x += cursor_joy_inc_dec;
    }
}

static void nespad_tick2(void) {
    bool a = (nespad_state2 & DPAD_A) != 0;
    if (!gamepad2_bits.a && a) {
        cursor_button = true;
    } else if (gamepad2_bits.a && !a) {
        cursor_button = false;
    }
    bool up = (nespad_state2 & DPAD_UP) != 0;
    bool down = (nespad_state2 & DPAD_DOWN) != 0;
    bool left = (nespad_state2 & DPAD_LEFT) != 0;
    bool right = (nespad_state2 & DPAD_RIGHT) != 0;
    if ((gamepad2_bits.up && up) || (gamepad2_bits.down && down) || (gamepad2_bits.left && left) || (gamepad2_bits.right && right)) {
        cursor_joy_inc_dec++;
        if (cursor_joy_inc_dec > 10) cursor_joy_inc_dec = 10;
    } else {
        cursor_joy_inc_dec = 1;
    }
    gamepad2_bits.a = a;
    gamepad2_bits.b = (nespad_state2 & DPAD_B) != 0;
    gamepad2_bits.select = (nespad_state2 & DPAD_SELECT) != 0;
    gamepad2_bits.start = (nespad_state2 & DPAD_START) != 0;
    gamepad2_bits.up = up;
    gamepad2_bits.down = down;
    gamepad2_bits.left = left;
    gamepad2_bits.right = right;

    if (gamepad2_bits.up) {
        cursor_y -= cursor_joy_inc_dec;
    }
    if (gamepad2_bits.down) {
        cursor_y += cursor_joy_inc_dec;
    }
    if (gamepad2_bits.left) {
        cursor_x -= cursor_joy_inc_dec;
    }
    if (gamepad2_bits.right) {
        cursor_x += cursor_joy_inc_dec;
    }
}

static void nespad_tick1(void);
static void nespad_tick2(void);
#endif

#ifdef KBDUSB
inline static bool isInReport(hid_keyboard_report_t const *report, const unsigned char keycode) {
    for (unsigned char i: report->keycode) {
        if (i == keycode) {
            return true;
        }
    }
    return false;
}

uint8_t pressed_key[256] = { 0 }; // TODO: optimize it

void __not_in_flash_func(process_kbd_report)(
    hid_keyboard_report_t const *report,
    hid_keyboard_report_t const *prev_report
) {
    for (uint8_t pkc: prev_report->keycode) {
        if (!pkc) continue;
        bool key_still_pressed = false;
        for (uint8_t kc: report->keycode) {
            if (kc == pkc) {
                key_still_pressed = true;
                break;
            }
        }
        if (!key_still_pressed) {
            kbd_queue_push(pressed_key[pkc], false);
            pressed_key[pkc] = 0;
        }
    }
    for (uint8_t kc: report->keycode) {
        if (!kc) continue;
        uint8_t* pk = pressed_key + kc;
        uint8_t hid_code = *pk;
        if (hid_code == 0) { // it was not yet pressed
            hid_code = kc;
            if (hid_code != 0) {
                *pk = hid_code;
                kbd_queue_push(hid_code, true);
            }
        }
    }
}

Ps2Kbd_Mrmltr ps2kbd(
        pio1,
        KBD_CLOCK_PIN,
        process_kbd_report
);
#endif

void __scratch_x("render") render_core() {
    multicore_lockout_victim_init();
    graphics_init();

    graphics_set_buffer(umac_ram + umac_get_fb_offset(), DISP_WIDTH, DISP_HEIGHT);
    graphics_set_bgcolor(0x000000);
    graphics_set_flashmode(false, false);
    sem_acquire_blocking(&vga_start_semaphore);

    uint32_t tickKbdRep1 = time_us_32();
    // 60 FPS loop
#define frame_tick (16666)
    uint64_t tick = time_us_64();
    //bool tick1 = true;
    uint64_t last_input_tick = tick;
    while (true) {
///        pcm_call();
        if (tick >= last_input_tick + frame_tick) {
#ifdef KBDUSB
            ps2kbd.tick();
#endif
#ifdef USE_NESPAD
           // (tick1 ? nespad_tick1 : nespad_tick2)(); // split call for joy1 and 2
           // tick1 = !tick1;
           nespad_tick1();
           nespad_tick2();
#endif
            last_input_tick = tick;
        }
        tick = time_us_64();
        uint32_t tickKbdRep2 = time_us_32();
        if (tickKbdRep2 - tickKbdRep1 > 150000) { // repeat each 150 ms
///            repeat_handler();
            tickKbdRep1 = tickKbdRep2;
        }

#ifdef KBDUSB
        tuh_task();
#endif
        tight_loop_contents();
    }
    __unreachable();
}

#if SOFTTV
typedef struct tv_out_mode_t {
    // double color_freq;
    float color_index;
    COLOR_FREQ_t c_freq;
    enum graphics_mode_t mode_bpp;
    g_out_TV_t tv_system;
    NUM_TV_LINES_t N_lines;
    bool cb_sync_PI_shift_lines;
    bool cb_sync_PI_shift_half_frame;
} tv_out_mode_t;
extern tv_out_mode_t tv_out_mode;

bool color_mode=true;
bool toggle_color() {
    color_mode=!color_mode;
    if(color_mode) {
        tv_out_mode.color_index= 1.0f;
    } else {
        tv_out_mode.color_index= 0.0f;
    }

    return true;
}
#endif

static int      disc_do_read(void *ctx, uint8_t *data, unsigned int offset, unsigned int len)
{
        FIL *fp = (FIL *)ctx;
        f_lseek(fp, offset);
        unsigned int did_read = 0;
        FRESULT fr = f_read(fp, data, len, &did_read);
        if (fr != FR_OK || len != did_read) {
                ///printf("disc: f_read returned %d, read %u (of %u)\n", fr, did_read, len);
                return -1;
        }
        return 0;
}

static int      disc_do_write(void *ctx, uint8_t *data, unsigned int offset, unsigned int len)
{
        FIL *fp = (FIL *)ctx;
        f_lseek(fp, offset);
        unsigned int did_write = 0;
        FRESULT fr = f_write(fp, data, len, &did_write);
        if (fr != FR_OK || len != did_write) {
                ///printf("disc: f_write returned %d, read %u (of %u)\n", fr, did_write, len);
                return -1;
        }
        return 0;
}

static void     disc_setup(disc_descr_t discs[DISC_NUM_DRIVES]) {
    static FIL fd;
    static FATFS fs;
    FRESULT fr = f_mount(&fs, "SD", 1);
    if (fr == FR_OK) {
        fr = f_open(&fd, "/umac0.img", FA_OPEN_EXISTING | FA_READ | FA_WRITE);
    }
    if (fr == FR_OK) {
        discs[0].base = 0; // Means use R/W ops
        discs[0].read_only = false;
        discs[0].size = f_size(&fd);
        discs[0].op_ctx = &fd;
        discs[0].op_read = disc_do_read;
        discs[0].op_write = disc_do_write;
    }
    else {
        /* If we don't find (or look for) an SD-based image, attempt
         * to use in-flash disc image:
         */
        discs[0].base = (uint8_t*)umac_disc;
        discs[0].read_only = 1;
        discs[0].size = sizeof(umac_disc);
    }
}

static int umac_cursor_x = 0;
static int umac_cursor_y = 0;
static int umac_cursor_button = 0;

static void     poll_umac()
{
        static absolute_time_t last_1hz = 0;
        static absolute_time_t last_vsync = 0;
        absolute_time_t now = get_absolute_time();

        umac_loop();

        int64_t p_1hz = absolute_time_diff_us(last_1hz, now);
        int64_t p_vsync = absolute_time_diff_us(last_vsync, now);
        if (p_vsync >= 16667) {
                /* FIXME: Trigger this off actual vsync */
                umac_vsync_event();
                last_vsync = now;
        }
        if (p_1hz >= 1000000) {
                umac_1hz_event();
                last_1hz = now;
        }

        int update = 0;
        int dx = 0;
        int dy = 0;
        int b = umac_cursor_button;
        if (cursor_x != umac_cursor_x) {
                dx = cursor_x - umac_cursor_x;
                umac_cursor_x = cursor_x;
                update = 1;
        }
        if (cursor_y != umac_cursor_y) {
                dy = cursor_y - umac_cursor_y;
                umac_cursor_y = cursor_y;
                update = 1;
        }
        if (cursor_button != umac_cursor_button) {
                b = cursor_button;
                umac_cursor_button = cursor_button;
                update = 1;
        }
        if (update) {
                umac_mouse(dx, -dy, b);
        }

        if (!kbd_queue_empty()) {
                uint16_t k = kbd_queue_pop();
                umac_kbd_event(k & 0xff, !!(k & 0x8000));
        }
}

int main() {
#if !PICO_RP2040
    vreg_set_voltage(VREG_VOLTAGE_1_40);
#else
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
#endif
    sleep_ms(10);
    set_sys_clock_khz(CPU_MHZ * KHZ, true);

#ifdef KBDUSB
    tuh_init(BOARD_TUH_RHPORT);
    ps2kbd.init_gpio();
#else
    keyboard_init();
#endif

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }

#if USE_NESPAD
    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);
#endif
    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_core);
    sem_release(&vga_start_semaphore);

///    init_sound();
///    pcm_setup(SOUND_FREQUENCY, SOUND_FREQUENCY * 2 / 50); // 882 * 2  = 1764
#ifdef PSRAM
    init_psram();
#endif
    // send kbd reset only after initial process passed
#ifndef KBDUSB
    keyboard_send(0xFF);
#endif

    disc_descr_t discs[DISC_NUM_DRIVES] = {0};
    disc_setup(discs);
    umac_init(umac_ram, (void *)umac_rom, discs);

    while (true) {
        poll_umac();
    }
    __unreachable();
}
