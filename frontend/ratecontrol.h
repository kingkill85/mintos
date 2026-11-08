/* Dynamic audio rate control: nudges the resample ratio so the queue stays at target. */
#pragma once
#include <stddef.h>

/* multiplier for the core's sample rate, 1 + 0.005 * (target - queued) / target, clamped to [0.995, 1.005] */
double rate_adjust(size_t queued, size_t target);
