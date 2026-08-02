#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Dedicates the calling core (core1) to DVI output; never returns.
// Call after graphics_init() + graphics_set_buffer().
void hdmi_dvi_loop(void);
// Feed one Mac sound sample (mono duplicated to stereo). Safe to call from core0.
void hdmi_dvi_push_audio_sample(int16_t left, int16_t right);
#ifdef __cplusplus
}
#endif
