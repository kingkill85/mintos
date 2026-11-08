/* Sound: core samples -> SDL_AudioStream (resampling, dynamic rate control) -> 48 kHz queue.
 * audio_wait() paces emulation on the queue fill (target 64 ms). */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <SDL.h>

int  audio_open(double core_rate);                 /* 0 ok */
size_t audio_push(const int16_t *s, size_t frames); /* interleaved stereo; returns frames */
void audio_wait(void);
void audio_pause(bool on);
void audio_close(void);
