/* Dynamic audio rate control: nudges the resample ratio so the queue stays at target. */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* multiplier for the core's sample rate, 1 + 0.005 * (target - queued) / target, clamped to [0.995, 1.005] */
double rate_adjust(size_t queued, size_t target);

/* Stereo S16 linear-interpolation resampler with a freely changing ratio and no state
 * resets, so rate control never drops or repeats samples. */
typedef struct { double pos; int16_t prev[2]; int primed; } Resampler;
/* step = input samples per output sample (core_rate / out_rate, times the rate control);
 * returns output frames written (at most max_out) */
size_t resample(Resampler *r, const int16_t *in, size_t n, double step, int16_t *out, size_t max_out);

/* Short linear fade-in after the device (re)starts, so a DC level in the core's first
 * samples (Game Boy) does not click. */
typedef struct { int total, left; } Fade;
void fade_start(Fade *f, int frames);
void fade_apply(Fade *f, int16_t *s, size_t frames);   /* in place, stereo */
