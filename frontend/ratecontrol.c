#include "ratecontrol.h"

double rate_adjust(size_t queued, size_t target) {
    double r = 1.0 + 0.005 * ((double)target - (double)queued) / (double)target;
    return r < 0.995 ? 0.995 : r > 1.005 ? 1.005 : r;
}

size_t resample(Resampler *r, const int16_t *in, size_t n, double step, int16_t *out, size_t max_out) {
    if (!n) return 0;
    if (!r->primed) { r->prev[0] = in[0]; r->prev[1] = in[1]; r->primed = 1; r->pos = 1.0; }
    size_t k = 0;
    /* pos is measured in input frames, where index 0 = prev (last frame of the previous call) */
    while (k < max_out && r->pos <= (double)n) {
        size_t i = (size_t)r->pos;
        double t = r->pos - i;
        const int16_t *a = i == 0 ? r->prev : in + 2 * (i - 1), *b = i == n ? a : in + 2 * i;
        for (int c = 0; c < 2; c++) out[2 * k + c] = (int16_t)(a[c] + (b[c] - a[c]) * t);
        k++;
        r->pos += step;
    }
    r->prev[0] = in[2 * (n - 1)];
    r->prev[1] = in[2 * (n - 1) + 1];
    r->pos -= n;
    return k;
}
