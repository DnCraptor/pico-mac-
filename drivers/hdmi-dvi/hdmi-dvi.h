#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Dedicates the calling core (core1) to DVI output; never returns.
// Call after graphics_init() + graphics_set_buffer().
void hdmi_dvi_loop(void);
#ifdef __cplusplus
}
#endif
