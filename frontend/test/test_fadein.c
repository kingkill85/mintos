// uses: frontend/ratecontrol.c
#include <string.h>
#include <stdlib.h>
#include "check.h"
#include "ratecontrol.h"

int main(void) {
    static int16_t buf[4000 * 2];
    for (int i = 0; i < 4000 * 2; i++) buf[i] = 10000;   /* a DC level: the click source */
    Fade f;
    fade_start(&f, 1440);                                  /* 30 ms at 48 kHz */
    fade_apply(&f, buf, 1000);
    CHECK_INT(buf[0], 0);                                  /* starts silent */
    CHECK(buf[2 * 500] > 3000 && buf[2 * 500] < 4000);     /* ~ 500/1440 of the level */
    CHECK(buf[2 * 999] < 10000);
    fade_apply(&f, buf + 2000, 1000);
    CHECK_INT(buf[2000 + 2 * 999], 10000);                 /* done after 1440 frames */
    CHECK(f.left == 0);
    /* no fade running: untouched */
    buf[0] = 1234;
    fade_apply(&f, buf, 1);
    CHECK_INT(buf[0], 1234);
    TEST_END();
}
