/*
 * Cursor acceleration for mouse emulation (NumPad, NES pads, XInput pads).
 *
 * The step is in Mac pixels per input tick.  Input ticks run every 25 ms
 * (40 Hz) for the NumPad and the NES pads; XInput steps once per report.
 *
 *   - A short press moves the cursor by CURSOR_STEP_MIN pixels, so single
 *     pixels can still be reached.
 *   - After CURSOR_STEP_HOLD ticks of holding (75 ms) the step grows by
 *     CURSOR_STEP_GROW every tick up to CURSOR_STEP_MAX.
 *
 * With these values the cursor crosses the 512-pixel Mac screen in about
 * 1.3 s of holding (16 px per tick, 640 px/s, is reached after ~0.85 s).
 * The old ramp (start 1, +0.1 per tick, up to 30) needed about 2.3 s for
 * the same distance, and it reached its top speed only after 7 s.
 */
#ifndef CURSOR_ACCEL_H
#define CURSOR_ACCEL_H

#define CURSOR_STEP_MIN  1.0f
#define CURSOR_STEP_MAX 16.0f
#define CURSOR_STEP_GROW 0.5f
#define CURSOR_STEP_HOLD 3

typedef struct {
    float step;
    int held_ticks;
} cursor_accel_t;

/* Call once per input tick; returns the step (whole pixels) to move by. */
static inline int cursor_accel_step(cursor_accel_t *a, bool still_held)
{
    if (!still_held) {
        a->step = CURSOR_STEP_MIN;
        a->held_ticks = 0;
    } else if (++a->held_ticks > CURSOR_STEP_HOLD) {
        a->step += CURSOR_STEP_GROW;
        if (a->step > CURSOR_STEP_MAX) a->step = CURSOR_STEP_MAX;
    }
    return (int)a->step;
}

#endif
