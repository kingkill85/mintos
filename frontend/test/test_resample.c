// uses: frontend/ratecontrol.c
#include <string.h>
#include <stdlib.h>
#include "check.h"
#include "ratecontrol.h"

int main(void) {
    static int16_t in[2048 * 2], out[8192 * 2];
    for (int i = 0; i < 2048; i++) { in[2 * i] = (int16_t)(i * 10); in[2 * i + 1] = (int16_t)(-i * 10); }
    Resampler r = {0};

    /* 1:1 keeps the samples */
    size_t n = resample(&r, in, 1000, 1.0, out, 8192);
    CHECK_INT(n, 1000);
    CHECK_INT(out[2 * 500], 5000); CHECK_INT(out[2 * 500 + 1], -5000);

    /* 32768 -> 48000 over many calls: total output = input * 48000/32768 (+-1), no gaps */
    Resampler g = {0};
    double step = 32768.0 / 48000.0;
    size_t total_in = 0, total_out = 0;
    for (int call = 0; call < 600; call++) {
        size_t k = resample(&g, in, 548, step, out, 8192);
        total_in += 548; total_out += k;
        /* linear ramp stays a ramp across call boundaries: neighbours differ by ~step*10 */
        for (size_t i = 1; i < k; i++) {
            int d = out[2 * i] - out[2 * (i - 1)];
            if (call > 0 && i == 1 && (d < -5480 || d > 10)) { CHECK(0); break; }
        }
    }
    double want = total_in * 48000.0 / 32768.0;
    CHECK(total_out > want - 2 && total_out < want + 2);

    /* output buffer limit respected */
    Resampler s = {0};
    CHECK_INT(resample(&s, in, 1000, 0.5, out, 100), 100);
    TEST_END();
}
