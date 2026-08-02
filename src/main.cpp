#include <cstdio>
#include <cstring>
#include <cstdarg>

#ifdef PICO_RP2350
#include <hardware/regs/qmi.h>
#include <hardware/structs/qmi.h>
#endif

#include <hardware/watchdog.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <hardware/pio.h>
#include <hardware/i2c.h>
#include <hardware/vreg.h>
#include <hardware/sync.h>
#include <hardware/flash.h>
#include <hardware/pwm.h>

#include "graphics.h"
#ifdef HDMI_DVI
#include "hdmi-dvi.h"
#endif

#include "audio.h"
#include <hardware/dma.h>
#include <hardware/irq.h>
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

/* Phase 1: core voltage is selectable via CMake (default 1.60V). */
#ifndef VREG_VOLTAGE_SEL
#define VREG_VOLTAGE_SEL VREG_VOLTAGE_1_60
#endif

extern "C" {
#include "disc.h"
#include "umac.h"
#include "kbd.h"
}

extern volatile int cursor_x;
extern volatile int cursor_y;
extern volatile int cursor_button;

// Mac binary data:  disc and ROM images
static const uint8_t __in_flash() __aligned(4096) umac_disc[400 << 10] = {
#include "umac-disc.h"
};
static const uint8_t __in_flash() __aligned(4096) umac_rom[128 << 10] = {
#if UMAC_MEMSIZE==208
    #if USE_VGA_RES
        #include "umac-rom-208-vga.h"
    #else
        #ifdef HDMI
            #include "umac-rom-208-320.h"
        #else
            #include "umac-rom-208-orig.h"
        #endif
    #endif
#else
    #if USE_VGA_RES
        #include "umac-rom-464-vga.h"
    #else
        #include "umac-rom-464-orig.h"
    #endif
#endif
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

uint8_t pressed_key[256] = { 0 };

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
    static float cursor_joy_inc_dec = 1;
    nespad_read();
    bool a = (nespad_state & DPAD_A) || pressed_key[HID_KEY_KEYPAD_ENTER];
    bool up = (nespad_state & DPAD_UP) || pressed_key[HID_KEY_KEYPAD_8];
    bool down = (nespad_state & DPAD_DOWN) || pressed_key[HID_KEY_KEYPAD_5] || pressed_key[HID_KEY_KEYPAD_2];
    bool left = (nespad_state & DPAD_LEFT) || pressed_key[HID_KEY_KEYPAD_4];
    bool right = (nespad_state & DPAD_RIGHT) || pressed_key[HID_KEY_KEYPAD_6];
    if (!gamepad1_bits.a && a) {
        cursor_button = true;
    } else if (gamepad1_bits.a && !a) {
        cursor_button = false;
    }
    if ((gamepad1_bits.up && up) || (gamepad1_bits.down && down) || (gamepad1_bits.left && left) || (gamepad1_bits.right && right)) {
        cursor_joy_inc_dec += 0.1;
        if (cursor_joy_inc_dec > 30) cursor_joy_inc_dec = 30;
    } else {
        cursor_joy_inc_dec = 1;
    }
    gamepad1_bits.a = a;
    gamepad1_bits.b = (nespad_state & DPAD_B) != 0;
    //gamepad1_bits.start =
    if ((nespad_state & DPAD_START) && (nespad_state & DPAD_SELECT)) {
        watchdog_enable(10, true);
        while (true) sleep_ms(10);
    }
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
    static float cursor_joy_inc_dec = 1;
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
        cursor_joy_inc_dec += 0.1;
        if (cursor_joy_inc_dec > 30) cursor_joy_inc_dec = 30;
    } else {
        cursor_joy_inc_dec = 1;
    }
    gamepad2_bits.a = a;
    gamepad2_bits.b = (nespad_state2 & DPAD_B) != 0;
//    gamepad2_bits.select = (nespad_state2 & DPAD_SELECT) != 0;
//    gamepad2_bits.start = (nespad_state2 & DPAD_START) != 0;
    if ((nespad_state2 & DPAD_START) && (nespad_state2 & DPAD_SELECT)) {
        watchdog_enable(10, true);
        while (true) sleep_ms(10);
    }
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
    if (pressed_key[HID_KEY_CONTROL_LEFT] && pressed_key[HID_KEY_ALT_LEFT] && pressed_key[HID_KEY_DELETE]) {
        watchdog_enable(10, true);
        while (true) sleep_ms(10);
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

#ifdef HDMI_DVI
    /* Phase 3b: dedicate core1 to DVI scanline generation; never returns.
     * Input (PS/2, NES, USB) does not run in this mode yet -- moving it to
     * core0 is Phase 3c. This first cut is for verifying HDMI video output. */
    hdmi_dvi_loop();
#endif

    uint32_t tickKbdRep1 = time_us_32();
    // 40 FPS loop
#define frame_tick (25000)
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
        tuh_task();   /* drains USB events; IRQ itself is on core0 (see main) */
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

/* ---------------------------------------------------------------------------
 * Mac 128K/512K sound output
 *
 * The Mac hardware reads the sound buffer directly (DMA-like) at ~22 kHz.
 * ---------------------------------------------------------------------------
 */
#define MAC_SOUNDBASE_ADDR1   0x027A
#define MAC_SOUNDBASE_ADDR2   0x027E
#define MAC_SOUND_BUF_SAMPLES  370u
volatile static uint32_t snd_sample_idx = 0;
static repeating_timer_t m_timer = { 0 };

extern "C" void __not_in_flash_func(v_sync)(void) {
    snd_sample_idx = 16;
}

/* Calling ~22255 times per second by timer
 * Reads the active Mac sound buffer via SoundBase global, scales by volume,
 * and updates the PWM duty cycle on BEEPER_PIN.
 */
static bool __not_in_flash_func(timer_callback)(repeating_timer_t *rt) {
    /* sndres: VIA RB bit 7 -- when 0 the Mac hardware reset the speaker sound generation */
    if (!(via_get_rb() & 0x80)) {
//        pwm_set_gpio_level(BEEPER_PIN, 0);
//        snd_sample_idx = 0;
#ifdef HDMI_DVI
        hdmi_dvi_push_audio_sample(0, 0);   // keep the HDMI ring fed with silence
#endif
        return true;
    }

    uint8_t ra = via_get_ra();
    // Apple Macintosh Hardware Memory Map (1983, Twiggy / early Mac docs)
    // PA3  → /SND PG2   (Sound page select)
    // active - inverted
    // Sound buffers (Inside Macintosh, top-of-RAM relative):
    //   main = MemTop - 0x0300, alternate = MemTop - 0x5F00.
    // (The snd-branch used 0x5C00, the main<->alt distance, as a top offset,
    //  which is 0x300 too high and read past the alternate buffer -> silence
    //  whenever the app page-flipped to the alt buffer via PA3.)
    uint32_t snd_base = (ra & 0b01000) ? RAM_SIZE - 0x0300 : RAM_SIZE - 0x5F00;

    uint32_t idx  = snd_sample_idx++ % MAC_SOUND_BUF_SAMPLES;
    uint32_t addr = snd_base + idx * 2;
    uint8_t sample = RAM_RD8(CLAMP_RAM_ADDR(addr));

    /* Volume: VIA RA[2:0] = 0 (mute) .. 7 (full). Scale sample linearly. */
    // Apple Macintosh Hardware Memory Map (1983)
    // PA2  → SV2   (Sound Volume bit 2)
    // PA1  → SV1   (Sound Volume bit 1)
    // PA0  → SV0   (Sound Volume bit 0)
//    uint8_t volume = (ra & 0x07);
//    uint8_t level = sample >> (8 - volume);

    pwm_set_gpio_level(BEEPER_PIN, sample); // level);
#ifdef HDMI_DVI
    // 8-bit unsigned (centered ~128) -> signed 16-bit, mono to both channels.
    int16_t s16 = (int16_t)(((int)sample - 128) << 8);
    hdmi_dvi_push_audio_sample(s16, s16);
#endif
    return true;
}


static void     poll_umac()
{
        static absolute_time_t last_1hz = 0;
        static absolute_time_t last_vsync = 0;
        absolute_time_t now = get_absolute_time();

#ifdef HDMI_DVI
        /* Phase 3c: in HDMI-DVI mode core1 is fully dedicated to TMDS generation
         * (hdmi_dvi_loop() never returns), so input is serviced here on core0.
         * The USB host IRQ is already on core0 (Phase 1b), so tuh_task() belongs
         * here too. Same cadence as the VGA render_core loop: USB ~1 kHz, PS/2 +
         * NES ~40 Hz. In VGA builds this block is compiled out and input stays on
         * core1 as before. */
        {
                static absolute_time_t last_usb = 0, last_in = 0;
#ifdef KBDUSB
                if (absolute_time_diff_us(last_usb, now) >= 1000) { tuh_task(); last_usb = now; }
#endif
                if (absolute_time_diff_us(last_in, now) >= 25000) {
#ifdef KBDUSB
                        ps2kbd.tick();
#endif
#ifdef USE_NESPAD
                        nespad_tick1();
                        nespad_tick2();
#endif
                        last_in = now;
                }
        }
#endif

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

#ifndef PICO_RP2040
void __not_in_flash() flash_timings() {
        const int max_flash_freq = 88 * MHZ;
        const int clock_hz = CPU_MHZ * MHZ;
        int divisor = (clock_hz + max_flash_freq - 1) / max_flash_freq;
        if (divisor == 1 && clock_hz > 100000000) {
            divisor = 2;
        }
        int rxdelay = divisor;
        if (clock_hz / divisor > 100000000) {
            rxdelay += 1;
        }
        qmi_hw->m[0].timing = 0x60007000 |
                            rxdelay << QMI_M0_TIMING_RXDELAY_LSB |
                            divisor << QMI_M0_TIMING_CLKDIV_LSB;
}
#endif

#ifdef BENCH_EMU
/* Phase 0: on-screen emulation-speed HUD (no UART; UART pins are unavailable).
 *
 * global_time_us advances by exactly one UMAC_EXECLOOP_QUANTUM per umac_loop()
 * regardless of how long that loop really took, so the ratio of emulated time
 * to wall-clock time is a direct measure of headroom:
 *   speed = 1.000 -> exactly real time (a real 7.8336 MHz 68000)
 *   speed > 1     -> emulator has spare capacity
 *   speed < 1     -> emulator runs slower than a real Mac
 * Effective m68k clock = 7.8336 MHz * speed.
 *
 * The readout is blitted straight into the 1bpp Mac framebuffer as a small HUD
 * in the top-left corner (white text on a black box). Framebuffer polarity:
 * bit=1 -> black, MSB = leftmost pixel (see drivers/vga-nextgen/vga.c).
 *
 * HUD legend (two lines):
 *   S x.xxx  Q<quantum> C<sysclk>   speed multiple, quantum(us), CPU MHz
 *   M xx.x                          effective m68k MHz
 */
static const uint8_t bench_font[17][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x00,0x00,0x00,0x60,0x60,0x00}, // '.'
    {0x70,0x88,0x98,0xA8,0xC8,0x88,0x70,0x00}, // '0'
    {0x20,0x60,0x20,0x20,0x20,0x20,0x70,0x00}, // '1'
    {0x70,0x88,0x08,0x10,0x20,0x40,0xF8,0x00}, // '2'
    {0xF0,0x08,0x10,0x30,0x08,0x88,0x70,0x00}, // '3'
    {0x10,0x30,0x50,0x90,0xF8,0x10,0x10,0x00}, // '4'
    {0xF8,0x80,0xF0,0x08,0x08,0x88,0x70,0x00}, // '5'
    {0x30,0x40,0x80,0xF0,0x88,0x88,0x70,0x00}, // '6'
    {0xF8,0x08,0x10,0x20,0x40,0x40,0x40,0x00}, // '7'
    {0x70,0x88,0x88,0x70,0x88,0x88,0x70,0x00}, // '8'
    {0x70,0x88,0x88,0x78,0x08,0x10,0x60,0x00}, // '9'
    {0x78,0x80,0x80,0x70,0x08,0x08,0xF0,0x00}, // 'S'
    {0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88,0x00}, // 'M'
    {0x70,0x88,0x88,0x88,0xA8,0x90,0x68,0x00}, // 'Q'
    {0x70,0x88,0x80,0x80,0x80,0x88,0x70,0x00}, // 'C'
    {0x88,0x50,0x20,0x50,0x88,0x88,0x88,0x00}, // 'X'
};
static int bench_glyph_index(char c) {
    switch (c) {
        case '.': return 1;
        case '0': return 2;  case '1': return 3;  case '2': return 4;
        case '3': return 5;  case '4': return 6;  case '5': return 7;
        case '6': return 8;  case '7': return 9;  case '8': return 10;
        case '9': return 11; case 'S': return 12; case 'M': return 13;
        case 'Q': return 14; case 'C': return 15; case 'X': return 16;
        default:  return 0;  /* space */
    }
}
static inline void bench_setpx(uint8_t *fb, int stride, int x, int y, bool white) {
    uint8_t *b = &fb[(size_t)y * stride + (x >> 3)];
    uint8_t m = 0x80 >> (x & 7);
    if (white) *b &= ~m; else *b |= m;   /* white=0(clear), black=1(set) */
}
static void bench_fillbox(uint8_t *fb, int stride, int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            bench_setpx(fb, stride, x, y, false); /* black */
}
static void bench_puts(uint8_t *fb, int stride, int px, int py, const char *s) {
    for (; *s; ++s, px += 6) {
        const uint8_t *g = bench_font[bench_glyph_index(*s)];
        for (int row = 0; row < 8; ++row) {
            uint8_t bits = g[row];
            for (int col = 0; col < 6; ++col)
                if (bits & (0x80 >> col))
                    bench_setpx(fb, stride, px + col, py + row, true); /* white ink */
        }
    }
}
static char *bench_u2s(char *p, unsigned v) {
    char t[10]; int n = 0;
    if (!v) { *p++ = '0'; return p; }
    while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) *p++ = t[--n];
    return p;
}
static void bench_frame(uint8_t *fb, int stride, int w, int h) {
    for (int x = 0; x < w; ++x) { bench_setpx(fb, stride, x, 0, false); bench_setpx(fb, stride, x, h - 1, false); }
    for (int y = 0; y < h; ++y) { bench_setpx(fb, stride, 0, y, false); bench_setpx(fb, stride, w - 1, y, false); }
}
static void bench_draw(uint8_t *fb, int stride, const char *l1, const char *l2) {
    bench_frame(fb, stride, DISP_WIDTH, DISP_HEIGHT);
    bench_fillbox(fb, stride, 0, 0, 132, 21);
    bench_puts(fb, stride, 2, 2,  l1);
    bench_puts(fb, stride, 2, 12, l2);
}
static void bench_run()
{
    const uint64_t MAC_HZ = 7833600ULL; /* 68000 nominal clock */
    uint8_t *fb = umac_ram + umac_get_fb_offset();
    const int stride = DISP_WIDTH / 8;
    uint64_t last_real = time_us_64();
    uint64_t last_emu  = umac_get_global_time_us();
    uint64_t last_draw = 0;
    char line1[40] = "S----";
    char line2[24] = "M----";
    bench_draw(fb, stride, line1, line2);   /* show HUD within the first frame */
    while (true) {
        poll_umac();
        uint64_t now = time_us_64();
        if (now - last_real >= 1000000ULL) {
            uint64_t d_emu  = umac_get_global_time_us() - last_emu;
            uint64_t d_real = now - last_real;
            unsigned sm = (unsigned)((d_emu * 1000ULL) / d_real);          /* speed x1000 */
            unsigned mt = (unsigned)(((MAC_HZ * d_emu) / d_real) / 100000ULL); /* MHz x10 */
            char *p = line1;
            *p++ = 'S'; p = bench_u2s(p, sm / 1000); *p++ = '.';
            unsigned f = sm % 1000;
            *p++ = (char)('0' + f / 100); *p++ = (char)('0' + (f / 10) % 10); *p++ = (char)('0' + f % 10);
            *p++ = 'X'; *p++ = ' ';
            *p++ = 'Q'; p = bench_u2s(p, (unsigned)umac_get_execloop_quantum()); *p++ = ' ';
            *p++ = 'C'; p = bench_u2s(p, (unsigned)CPU_MHZ); *p = 0;
            char *q = line2;
            *q++ = 'M'; q = bench_u2s(q, mt / 10); *q++ = '.'; *q++ = (char)('0' + mt % 10); *q = 0;
            last_real = now; last_emu += d_emu;
        }
        if (now - last_draw >= 33000ULL) {   /* ~30 Hz redraw keeps HUD visible */
            bench_draw(fb, stride, line1, line2);
            last_draw = now;
        }
    }
}
#endif

int main() {
#if !PICO_RP2040
    vreg_disable_voltage_limit();
    vreg_set_voltage(VREG_VOLTAGE_SEL);
    flash_timings();
#else
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
#endif
    sleep_ms(100);
    /* Phase 2: the video backend may require a specific system clock (e.g.
     * HDMI-DVI needs 400 MHz for its TMDS bit clock). VGA can run at any clock,
     * so it honours CPU_MHZ. */
    {
        uint32_t nclk = 0;
        const uint32_t *clks = graphics_get_supported_system_clocks(&nclk);
        uint32_t target_mhz = (graphics_system_clock_can_change() || nclk == 0)
                                  ? (uint32_t)CPU_MHZ
                                  : clks[0];
        set_sys_clock_khz(target_mhz * KHZ, true);
    }

#ifdef KBDUSB
    /* USB host is initialised on core0 so its IRQ does NOT run on core1, which
     * generates the time-critical VGA signal (a USB IRQ there tears sync).
     * tuh_task() runs on core1; consolidating both onto one core is a Phase 1b
     * task (input-driver reconciliation with the Vector reference). */
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

    /* PWM for Mac sound: updates at ~21.7 kHz */
    {
        pwm_config _pwm_cfg = pwm_get_default_config();
        gpio_set_function(BEEPER_PIN, GPIO_FUNC_PWM);
        pwm_config_set_clkdiv(&_pwm_cfg, 1.0f);
        pwm_config_set_wrap(&_pwm_cfg, 0xFF);
        pwm_init(pwm_gpio_to_slice_num(BEEPER_PIN), &_pwm_cfg, true);
        pwm_set_gpio_level(BEEPER_PIN, 0);
    	add_repeating_timer_us(-1000000 / 22255, timer_callback, NULL, &m_timer);
    }

#ifdef BENCH_EMU
    bench_run();   /* never returns */
#else
    while (true) {
        poll_umac();
    }
#endif
    __unreachable();
}
