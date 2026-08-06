/*
 * MOS 6522 VIA emulation for umac.
 *
 * The external API stays compatible with the original umac VIA module.
 * Internally this models the register/port/interrupt/timer semantics needed
 * by the compact Macintosh instead of treating the VIA as a register bag.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "via.h"

#ifdef PICO
#include "pico.h"
#define VIA_FAST_FUNC(x) __not_in_flash_func(x)
#else
#define VIA_FAST_FUNC(x) x
#endif

#ifdef DEBUG
#define VDBG(...) printf(__VA_ARGS__)
#else
#define VDBG(...) do { } while (0)
#endif

#define VIA_RB      0
#define VIA_RA      1
#define VIA_DDRB    2
#define VIA_DDRA    3
#define VIA_T1CL    4
#define VIA_T1CH    5
#define VIA_T1LL    6
#define VIA_T1LH    7
#define VIA_T2CL    8
#define VIA_T2CH    9
#define VIA_SR      10
#define VIA_ACR     11
#define VIA_PCR     12
#define VIA_IFR     13
#define VIA_IER     14
#define VIA_RA_ALT  15

/* IFR/IER bit assignments. */
#define VIA_IRQ_CA2 0x01
#define VIA_IRQ_CA1 0x02
#define VIA_IRQ_SR  0x04
#define VIA_IRQ_CB2 0x08
#define VIA_IRQ_CB1 0x10
#define VIA_IRQ_T2  0x20
#define VIA_IRQ_T1  0x40

#define VIA_CLOCK_NUM  2448u
#define VIA_CLOCK_DEN  3125u

/* Mini vMac machine wiring for Macintosh 128K/512K/Plus.
 *
 * Port A: only A7 is a true input. A0..A6 are output-capable machine wires;
 * when their DDR bit is clear they assume the hardware float value 0xF7.
 * In particular A3/SoundBuffer floats low, selecting the alternate sound
 * page until System explicitly drives the line.
 *
 * Port B: B0, B3..B6 are inputs; B0..B2 and B7 are output-capable.
 * Non-input pins float high. */
#define VIA_ORA_CAN_IN   0x80u
#define VIA_ORA_CAN_OUT  0x7Fu
#define VIA_ORA_FLOAT    0xF7u
#define VIA_ORB_CAN_IN   0x79u
#define VIA_ORB_CAN_OUT  0x87u
#define VIA_ORB_FLOAT    0xFFu

static uint8_t via_regs[16];
static struct via_cb via_callbacks;
static uint8_t irq_flags;
static uint8_t irq_enable;
static int irq_level;

static uint32_t t1_counter;
static uint16_t t1_latch;
static uint32_t t2_counter;
static int t1_running;
static int t2_running;
static int t1_fired_once;
static uint8_t t1_pb7;
static uint32_t clock_frac;

static int sr_tx_pending;
static uint8_t t2_pb6_last;

/* Current logic levels on the four VIA control inputs.  The compact Mac
 * normally keeps these high and presents active-low pulses, but PCR can
 * select the opposite edge. */
static uint8_t ca1_level;
static uint8_t ca2_level;
static uint8_t cb1_level;
static uint8_t cb2_level;

/* Physical output levels for CA2/CB2.  Input modes leave the corresponding
 * line under external control.  Output modes 4..7 implement handshake,
 * pulse, forced-low and forced-high semantics. */
static uint8_t ca2_output;
static uint8_t cb2_output;
static uint8_t ca2_handshake_low;
static uint8_t cb2_handshake_low;
static uint8_t ca2_pulse_cycles;
static uint8_t cb2_pulse_cycles;

/* 6522 input latches. ACR bit 0 enables Port A latching on CA1;
 * ACR bit 1 enables Port B latching on CB1. Only input-configured pins use
 * the latch; output pins always reflect ORA/ORB (or T1 on PB7). */
static uint8_t pa_input_latch;
static uint8_t pb_input_latch;
static uint8_t pa_latch_valid;
static uint8_t pb_latch_valid;

/* Last physical port levels delivered to machine callbacks.  Mini vMac
 * notifies attached devices only when an actual output pin changes; caching
 * here prevents DDR/ACR writes from producing duplicate overlay/sound events. */
static uint8_t notified_port_a;
static uint8_t notified_port_b;
static uint8_t notified_port_a_valid;
static uint8_t notified_port_b_valid;

/* BENCH_EMU diagnostics retained for the platform logger. */

static uint8_t sample_port_a_inputs(void)
{
        uint8_t external = via_callbacks.ra_in ? via_callbacks.ra_in() : 0xff;
        return (uint8_t)((external & VIA_ORA_CAN_IN) |
                         (VIA_ORA_FLOAT & (uint8_t)~VIA_ORA_CAN_IN));
}

static uint8_t sample_port_b_inputs(void)
{
        uint8_t external = via_callbacks.rb_in ? via_callbacks.rb_in() : 0xff;
        return (uint8_t)((external & VIA_ORB_CAN_IN) |
                         (VIA_ORB_FLOAT & (uint8_t)~VIA_ORB_CAN_IN));
}

static uint8_t read_port_a(void)
{
        uint8_t pins = ((via_regs[VIA_ACR] & 0x01) && pa_latch_valid)
                ? pa_input_latch : sample_port_a_inputs();
        uint8_t ddr = (uint8_t)(via_regs[VIA_DDRA] & VIA_ORA_CAN_OUT);
        return (uint8_t)((via_regs[VIA_RA] & ddr) | (pins & (uint8_t)~ddr));
}

static uint8_t read_port_b(void)
{
        uint8_t pins = ((via_regs[VIA_ACR] & 0x02) && pb_latch_valid)
                ? pb_input_latch : sample_port_b_inputs();
        uint8_t ddr = (uint8_t)(via_regs[VIA_DDRB] & VIA_ORB_CAN_OUT);
        uint8_t value = (uint8_t)((via_regs[VIA_RB] & ddr) |
                                  (pins & (uint8_t)~ddr));

        if ((ddr & 0x80) && (via_regs[VIA_ACR] & 0x80))
                value = (uint8_t)((value & 0x7f) | (t1_pb7 ? 0x80 : 0));
        return value;
}

static void notify_port_a(void)
{
        uint8_t value = read_port_a();
        if (!notified_port_a_valid || value != notified_port_a) {
                notified_port_a = value;
                notified_port_a_valid = 1;
                if (via_callbacks.ra_change)
                        via_callbacks.ra_change(value);
        }
}

static void notify_port_b(void)
{
        uint8_t value = read_port_b();
        if (!notified_port_b_valid || value != notified_port_b) {
                notified_port_b = value;
                notified_port_b_valid = 1;
                if (via_callbacks.rb_change)
                        via_callbacks.rb_change(value);
        }
}

static void assess_irq(void)
{
        int level = (irq_flags & irq_enable & 0x7f) != 0;
        if (level != irq_level) {
                irq_level = level;
                if (via_callbacks.irq_set)
                        via_callbacks.irq_set(level);
        }
}

static void set_ifr(uint8_t bits)
{
        irq_flags |= bits & 0x7f;
        assess_irq();
}

static void clear_ifr(uint8_t bits)
{
        irq_flags &= (uint8_t)~bits;
        assess_irq();
}

static uint8_t get_ifr(void)
{
        uint8_t value = irq_flags & 0x7f;
        if (value & irq_enable)
                value |= 0x80;
        return value;
}

static uint32_t timer_reload_value(uint16_t latch)
{
        /* Mini vMac stores the programmed 16-bit latch directly.  A zero
         * latch represents the full 65536-count interval after wraparound. */
        return latch ? (uint32_t)latch : 0x10000u;
}

static unsigned ca2_mode(void)
{
        return (via_regs[VIA_PCR] >> 1) & 7u;
}

static unsigned cb2_mode(void)
{
        return (via_regs[VIA_PCR] >> 5) & 7u;
}

static unsigned sr_mode(void);

static void notify_ca2_output(uint8_t level)
{
        level = (uint8_t)(level != 0);
        if (ca2_output != level) {
                ca2_output = level;
                if (via_callbacks.ca2_change)
                        via_callbacks.ca2_change(level);
        }
}

static void notify_cb2_output(uint8_t level)
{
        level = (uint8_t)(level != 0);
        if (cb2_output != level) {
                cb2_output = level;
                if (via_callbacks.cb2_change)
                        via_callbacks.cb2_change(level);
        }
}

static void refresh_ca2_output(void)
{
        switch (ca2_mode()) {
        case 4: notify_ca2_output(!ca2_handshake_low); break;
        case 5: notify_ca2_output(ca2_pulse_cycles == 0); break;
        case 6: notify_ca2_output(0); break;
        case 7: notify_ca2_output(1); break;
        default: break;
        }
}

static void refresh_cb2_output(void)
{
        /* Shift-register output modes own CB2 while active.  The current
         * keyboard protocol needs the mode-6 attention low level and the
         * external shift-out idle-high state. */
        switch (sr_mode()) {
        case 4:
        case 5:
        case 7:
                notify_cb2_output(1);
                return;
        case 6:
                notify_cb2_output(via_regs[VIA_SR] != 0);
                return;
        default:
                break;
        }

        switch (cb2_mode()) {
        case 4: notify_cb2_output(!cb2_handshake_low); break;
        case 5: notify_cb2_output(cb2_pulse_cycles == 0); break;
        case 6: notify_cb2_output(0); break;
        case 7: notify_cb2_output(1); break;
        default: break;
        }
}

static void port_a_output_handshake(void)
{
        unsigned mode = ca2_mode();
        if (mode == 4u) {
                ca2_handshake_low = 1;
                refresh_ca2_output();
        } else if (mode == 5u) {
                ca2_pulse_cycles = 1;
                refresh_ca2_output();
        }
}

static void port_b_output_handshake(void)
{
        unsigned mode = cb2_mode();
        if (mode == 4u) {
                cb2_handshake_low = 1;
                refresh_cb2_output();
        } else if (mode == 5u) {
                cb2_pulse_cycles = 1;
                refresh_cb2_output();
        }
}

static int ca2_cleared_by_port_access(void)
{
        /* Match Mini vMac VIAEMDEV: only PCR mode 0 is acknowledged by an
         * ORA access. Modes 1..3 are independent/edge-controlled inputs. */
        return ca2_mode() == 0u;
}

static int cb2_cleared_by_port_access(void)
{
        /* Match Mini vMac VIAEMDEV: only PCR mode 0 is acknowledged by an
         * ORB access. */
        return cb2_mode() == 0u;
}

static void ack_port_a(void)
{
        clear_ifr(VIA_IRQ_CA1);
        if (ca2_cleared_by_port_access())
                clear_ifr(VIA_IRQ_CA2);
        port_a_output_handshake();
}

static void ack_port_b(void)
{
        clear_ifr(VIA_IRQ_CB1);
        if (cb2_cleared_by_port_access())
                clear_ifr(VIA_IRQ_CB2);
        port_b_output_handshake();
}

static unsigned sr_mode(void)
{
        return (via_regs[VIA_ACR] >> 2) & 7u;
}

static void sr_write(uint8_t data)
{
        via_regs[VIA_SR] = data;
        clear_ifr(VIA_IRQ_SR);
        refresh_cb2_output();

        switch (sr_mode()) {
        case 6: /* shift out under phi2 clock */
                /* The compact Macintosh writes zero here to pull the
                 * keyboard data line low. Mini vMac deliberately does not
                 * raise SR IRQ for this attention pulse. */
                if (data == 0)
                        via_regs[VIA_SR] = 0;
                break;
        case 7: /* external-clock shift out */
                /* External hardware supplies eight CB1 clocks. Preserve
                 * umac's pacing contract: expose completion immediately,
                 * but defer the keyboard callback until the SR IRQ is
                 * acknowledged by the guest. */
                sr_tx_pending = data;
                set_ifr(VIA_IRQ_SR | VIA_IRQ_CB1);
                break;
        default:
                break;
        }
}

static void sr_acknowledged(void)
{
        if (sr_tx_pending >= 0) {
                uint8_t value = (uint8_t)sr_tx_pending;
                sr_tx_pending = -1;
                if (via_callbacks.sr_tx)
                        via_callbacks.sr_tx(value);
        }
}

void via_init(struct via_cb *cb)
{
        memset(via_regs, 0, sizeof(via_regs));
        memset(&via_callbacks, 0, sizeof(via_callbacks));
        if (cb)
                via_callbacks = *cb;

        /* 6522 output latches reset to zero.  The Macintosh ROM overlay is
         * nevertheless asserted while DDRA4 is input because the machine's
         * Port A float value has A4 high (VIA_ORA_FLOAT = 0xF7).  Keeping the
         * latch itself at zero matches Mini vMac and prevents a stale high
         * from being driven when System later changes DDRA. */
        via_regs[VIA_RA] = 0x00;
        irq_flags = 0;
        irq_enable = 0;
        irq_level = 0;
        t1_counter = 0;
        t1_latch = 0;
        t2_counter = 0;
        t1_running = t2_running = 0;
        t1_fired_once = 0;
        t1_pb7 = 1;
        clock_frac = 0;
        sr_tx_pending = -1;
        pa_input_latch = sample_port_a_inputs();
        pb_input_latch = sample_port_b_inputs();
        pa_latch_valid = 0;
        pb_latch_valid = 0;
        notified_port_a = notified_port_b = 0;
        notified_port_a_valid = notified_port_b_valid = 0;
        t2_pb6_last = (uint8_t)((read_port_b() >> 6) & 1u);
        ca1_level = ca2_level = cb1_level = cb2_level = 1;
        ca2_output = cb2_output = 1;
        ca2_handshake_low = cb2_handshake_low = 0;
        ca2_pulse_cycles = cb2_pulse_cycles = 0;

        /* Publish reset pin levels and explicitly deassert IRQ.  This keeps
         * reinitialisation deterministic even when the previous instance
         * ended with an asserted interrupt or different DDR ownership. */
        if (via_callbacks.irq_set)
                via_callbacks.irq_set(0);
        notify_port_a();
        notify_port_b();
        refresh_ca2_output();
        refresh_cb2_output();
}

void via_write(unsigned int address, uint8_t data)
{
        unsigned r = (address >> 9) & 0x0f;

        switch (r) {
        case VIA_RB:
                via_regs[VIA_RB] = data;
                ack_port_b();
                notify_port_b();
                break;
        case VIA_RA:
                via_regs[VIA_RA] = data;
                ack_port_a();
                notify_port_a();
                break;
        case VIA_RA_ALT:
                /* Mini vMac treats both ORA register aliases identically for
                 * interrupt acknowledgement; the alternate address only
                 * suppresses the CA2 output handshake on real hardware. */
                via_regs[VIA_RA] = data;
                clear_ifr(VIA_IRQ_CA1);
                if (ca2_cleared_by_port_access())
                        clear_ifr(VIA_IRQ_CA2);
                notify_port_a();
                break;
        case VIA_DDRB:
                via_regs[VIA_DDRB] = data;
                notify_port_b();
                break;
        case VIA_DDRA:
                via_regs[VIA_DDRA] = data;
                notify_port_a();
                break;
        case VIA_T1CL:
        case VIA_T1LL:
                t1_latch = (uint16_t)((t1_latch & 0xff00) | data);
                via_regs[VIA_T1LL] = data;
                break;
        case VIA_T1LH:
                t1_latch = (uint16_t)((t1_latch & 0x00ff) |
                                      ((uint16_t)data << 8));
                via_regs[VIA_T1LH] = data;
                break;
        case VIA_T1CH:
                t1_latch = (uint16_t)((t1_latch & 0x00ff) |
                                      ((uint16_t)data << 8));
                via_regs[VIA_T1LH] = data;
                /* A 6522 period includes the reload and underflow cycles. */
                t1_counter = timer_reload_value(t1_latch);
                t1_running = 1;
                t1_fired_once = 0;
                clear_ifr(VIA_IRQ_T1);
                if ((via_regs[VIA_DDRB] & 0x80) &&
                    (via_regs[VIA_ACR] & 0x80)) {
                        t1_pb7 = 0;
                        notify_port_b();
                }
                break;
        case VIA_T2CL:
                via_regs[VIA_T2CL] = data;
                break;
        case VIA_T2CH:
                via_regs[VIA_T2CH] = data;
                t2_counter = timer_reload_value((uint16_t)(via_regs[VIA_T2CL] |
                                        ((uint16_t)data << 8)));
                t2_running = 1;
                clear_ifr(VIA_IRQ_T2);
                break;
        case VIA_SR:
                sr_write(data);
                break;
        case VIA_ACR: {
                uint8_t old = via_regs[VIA_ACR];
                via_regs[VIA_ACR] = data;
                /* Ownership of PB7 may move between ORB and Timer 1. */
                if ((old ^ data) & 0x80)
                        notify_port_b();
                /* Entering T2 pulse-counting mode establishes a fresh PB6
                 * edge baseline; it must not consume a phantom transition. */
                if ((old ^ data) & 0x20)
                        t2_pb6_last = (uint8_t)((read_port_b() >> 6) & 1u);
                /* Disabling a latch immediately returns the port to live
                 * input sampling. A future CA1/CB1 edge captures a fresh
                 * value when the latch is enabled again. */
                if (!(data & 0x01))
                        pa_latch_valid = 0;
                if (!(data & 0x02))
                        pb_latch_valid = 0;
                if ((old ^ data) & 0x01)
                        notify_port_a();
                if ((old ^ data) & 0x02)
                        notify_port_b();
                if ((old ^ data) & 0x1c)
                        refresh_cb2_output();
                break;
        }
        case VIA_PCR: {
                unsigned old_ca2 = ca2_mode();
                unsigned old_cb2 = cb2_mode();
                via_regs[VIA_PCR] = data;
                /* CA2/CB2 output modes cannot leave stale input IRQs set. */
                if (ca2_mode() >= 4u)
                        clear_ifr(VIA_IRQ_CA2);
                if (cb2_mode() >= 4u)
                        clear_ifr(VIA_IRQ_CB2);
                if (old_ca2 != ca2_mode()) {
                        ca2_handshake_low = 0;
                        ca2_pulse_cycles = 0;
                }
                if (old_cb2 != cb2_mode()) {
                        cb2_handshake_low = 0;
                        cb2_pulse_cycles = 0;
                }
                refresh_ca2_output();
                refresh_cb2_output();
                break;
        }
        case VIA_IFR: {
                uint8_t acknowledged = irq_flags & data;
                clear_ifr(data & 0x7f);
                if (acknowledged & VIA_IRQ_SR)
                        sr_acknowledged();
                break;
        }
        case VIA_IER:
                if (data & 0x80)
                        irq_enable |= data & 0x7f;
                else
                        irq_enable &= (uint8_t)~(data & 0x7f);
                assess_irq();
                break;
        default:
                break;
        }
}

uint8_t via_read(unsigned int address)
{
        unsigned r = (address >> 9) & 0x0f;
        uint8_t value;

        switch (r) {
        case VIA_RB:
                value = read_port_b();
                ack_port_b();
                return value;
        case VIA_RA:
                value = read_port_a();
                ack_port_a();
                return value;
        case VIA_RA_ALT:
                value = read_port_a();
                clear_ifr(VIA_IRQ_CA1);
                if (ca2_cleared_by_port_access())
                        clear_ifr(VIA_IRQ_CA2);
                return value;
        case VIA_T1CL:
                clear_ifr(VIA_IRQ_T1);
                return (uint8_t)(t1_counter & 0xffffu);
        case VIA_T1CH:
                return (uint8_t)((t1_counter >> 8) & 0xffu);
        case VIA_T1LL:
                return (uint8_t)t1_latch;
        case VIA_T1LH:
                return (uint8_t)(t1_latch >> 8);
        case VIA_T2CL:
                clear_ifr(VIA_IRQ_T2);
                return (uint8_t)(t2_counter & 0xffffu);
        case VIA_T2CH:
                return (uint8_t)((t2_counter >> 8) & 0xffu);
        case VIA_SR:
                value = via_regs[VIA_SR];
                clear_ifr(VIA_IRQ_SR);
                sr_acknowledged();
                return value;
        case VIA_IFR:
                return get_ifr();
        case VIA_IER:
                return (uint8_t)(0x80 | irq_enable);
        default:
                return via_regs[r];
        }
}

uint8_t via_read_ifr(void)
{
        return get_ifr();
}

static void timer1_underflow(void)
{
        set_ifr(VIA_IRQ_T1);

        if ((via_regs[VIA_DDRB] & 0x80) &&
            (via_regs[VIA_ACR] & 0x80)) {
                if (via_regs[VIA_ACR] & 0x40)
                        t1_pb7 ^= 1;
                else
                        t1_pb7 = 1;
                notify_port_b();
        }

        if (via_regs[VIA_ACR] & 0x40) {
                t1_counter = timer_reload_value(t1_latch);
        } else {
                t1_running = 0;
                t1_fired_once = 1;
        }
}

static void VIA_FAST_FUNC(advance_timer1)(uint32_t cycles)
{
        while (cycles && t1_running) {
                if (cycles < t1_counter) {
                        t1_counter -= (uint32_t)cycles;
                        return;
                }
                cycles -= t1_counter;
                timer1_underflow();
        }
}

static void timer2_decrement(uint32_t amount)
{
        if (!t2_running || amount == 0)
                return;

        if (amount < t2_counter) {
                t2_counter -= amount;
                return;
        }

        /* Timer 2 is one-shot. After underflow its visible counter wraps,
         * but no further interrupt is generated until software reloads it. */
        t2_counter = (uint32_t)(0x10000u -
                ((amount - t2_counter) & 0xffffu));
        t2_running = 0;
        set_ifr(VIA_IRQ_T2);
}

static void sample_timer2_pb6(void)
{
        uint8_t pb6 = (uint8_t)((read_port_b() >> 6) & 1u);
        if ((via_regs[VIA_ACR] & 0x20) && t2_pb6_last && !pb6)
                timer2_decrement(1);
        t2_pb6_last = pb6;
}

static void VIA_FAST_FUNC(advance_control_outputs)(uint32_t cycles)
{
        if (ca2_pulse_cycles) {
                if (cycles >= ca2_pulse_cycles)
                        ca2_pulse_cycles = 0;
                else
                        ca2_pulse_cycles -= (uint8_t)cycles;
                refresh_ca2_output();
        }
        if (cb2_pulse_cycles) {
                if (cycles >= cb2_pulse_cycles)
                        cb2_pulse_cycles = 0;
                else
                        cb2_pulse_cycles -= (uint8_t)cycles;
                refresh_cb2_output();
        }
}

static void VIA_FAST_FUNC(advance_timers)(uint32_t cycles)
{
        uint8_t acr = via_regs[VIA_ACR];

        if (t1_running)
                advance_timer1(cycles);
        if (t2_running && !(acr & 0x20))
                timer2_decrement(cycles);
        if (ca2_pulse_cycles || cb2_pulse_cycles)
                advance_control_outputs(cycles);
}

void VIA_FAST_FUNC(via_tick)(uint32_t elapsed_us)
{
        /* umac_loop() advances by a fixed, small quantum.  Passing elapsed
         * time directly removes the 64-bit absolute-time subtraction and the
         * rollover/recovery branches from every quantum.  2448/3125 is the
         * exact reduced VIA-clock conversion. */
        uint32_t scaled = elapsed_us * VIA_CLOCK_NUM + clock_frac;
        uint32_t cycles = scaled / VIA_CLOCK_DEN;
        clock_frac = scaled - cycles * VIA_CLOCK_DEN;

        if (cycles)
                advance_timers(cycles);

        /* PB6 is relevant only in Timer 2 pulse-counting mode. */
        if (via_regs[VIA_ACR] & 0x20)
                sample_timer2_pb6();
}

static int selected_edge(uint8_t old_level, uint8_t new_level,
                         int positive_edge)
{
        if (old_level == new_level)
                return 0;
        return positive_edge ? (!old_level && new_level)
                             : (old_level && !new_level);
}

void via_set_ca1(int level)
{
        uint8_t new_level = (uint8_t)(level != 0);
        if (selected_edge(ca1_level, new_level,
                          (via_regs[VIA_PCR] & 0x01) != 0)) {
                if (via_regs[VIA_ACR] & 0x01) {
                        pa_input_latch = sample_port_a_inputs();
                        pa_latch_valid = 1;
                        notify_port_a();
                }
                set_ifr(VIA_IRQ_CA1);
                if (ca2_mode() == 4u && ca2_handshake_low) {
                        ca2_handshake_low = 0;
                        refresh_ca2_output();
                }
        }
        ca1_level = new_level;
}

void via_set_ca2(int level)
{
        uint8_t new_level = (uint8_t)(level != 0);
        unsigned mode = ca2_mode();

        /* Modes 0..3 are inputs.  0/1 select a negative edge and 2/3 a
         * positive edge; odd modes are independent only in how ORA access
         * acknowledges the flag, not in edge detection. */
        if (mode < 4u &&
            selected_edge(ca2_level, new_level, mode >= 2u))
                set_ifr(VIA_IRQ_CA2);
        ca2_level = new_level;
}

void via_set_cb1(int level)
{
        uint8_t new_level = (uint8_t)(level != 0);
        if (selected_edge(cb1_level, new_level,
                          (via_regs[VIA_PCR] & 0x10) != 0)) {
                if (via_regs[VIA_ACR] & 0x02) {
                        pb_input_latch = sample_port_b_inputs();
                        pb_latch_valid = 1;
                        notify_port_b();
                }
                set_ifr(VIA_IRQ_CB1);
                if (cb2_mode() == 4u && cb2_handshake_low) {
                        cb2_handshake_low = 0;
                        refresh_cb2_output();
                }
        }
        cb1_level = new_level;
}

void via_set_cb2(int level)
{
        uint8_t new_level = (uint8_t)(level != 0);
        unsigned mode = cb2_mode();
        if (mode < 4u &&
            selected_edge(cb2_level, new_level, mode >= 2u))
                set_ifr(VIA_IRQ_CB2);
        cb2_level = new_level;
}

static void pulse_selected_input(void (*set_line)(int), int positive_edge)
{
        /* The legacy umac interface reports an already-qualified event,
         * rather than a persistent pin level.  Recreate a complete edge so
         * repeated calls remain observable regardless of the previous pin
         * state. */
        set_line(positive_edge ? 0 : 1);
        set_line(positive_edge ? 1 : 0);
}

void via_caX_event(int ca)
{
        /* Preserve umac's historical numbering: 1=one-second/CA2,
         * 2=vertical blank/CA1. */
        if (ca == 1) {
                unsigned mode = ca2_mode();
                if (mode < 4u)
                        pulse_selected_input(via_set_ca2, mode >= 2u);
        } else if (ca == 2) {
                pulse_selected_input(via_set_ca1,
                        (via_regs[VIA_PCR] & 0x01) != 0);
        }
}

void via_sr_rx(uint8_t value)
{
        /* External hardware has supplied eight CB1 clocks and eight bits on
         * CB2. Mini vMac raises both the SR-complete and CB1 flags. */
        if (sr_mode() == 3u) {
                if (via_regs[VIA_ACR] & 0x02) {
                        pb_input_latch = sample_port_b_inputs();
                        pb_latch_valid = 1;
                        notify_port_b();
                }
                via_regs[VIA_SR] = value;
                set_ifr(VIA_IRQ_SR | VIA_IRQ_CB1);
        }
}

uint8_t via_get_ra(void)
{
        return read_port_a();
}

uint8_t via_get_rb(void)
{
        return read_port_b();
}

uint8_t via_get_ca2(void)
{
        return ca2_mode() >= 4u ? ca2_output : ca2_level;
}

uint8_t via_get_cb2(void)
{
        return (sr_mode() >= 4u) || cb2_mode() >= 4u
                ? cb2_output : cb2_level;
}

/* Mini vMac's classic sound device uses Timer 1 as a speaker inversion
 * source only when both continuous mode (ACR6) and PB7 timer output (ACR7)
 * are enabled.  Return the programmed latch exactly as VIA1_GetT1InvertTime
 * does; zero means that the classic PCM path is not being inverted. */
uint16_t via_get_t1_invert_time(void)
{
        return (via_regs[VIA_ACR] & 0xc0u) == 0xc0u ? t1_latch : 0u;
}
