// uses: frontend/viewport.c
#include <string.h>
#include "check.h"
#include "viewport.h"

static void check_rect(Rect r, int x, int y, int w, int h, int line) {
    if (r.x != x || r.y != y || r.w != w || r.h != h) {
        check_failures++;
        fprintf(stderr, "line %d: got {%d,%d,%d,%d}, expected {%d,%d,%d,%d}\n", line, r.x, r.y, r.w, r.h, x, y, w, h);
    }
}
#define RECT(r, x, y, w, h) check_rect(r, x, y, w, h, __LINE__)

int main(void) {
    SystemCfg s = {0};
    s.scale = SCALE_INTEGER;
    RECT(viewport(&s, 640, 480, 160, 144, 10.0f / 9.0f), 80, 24, 480, 432);   /* gb */
    RECT(viewport(&s, 640, 480, 602, 224, 4.0f / 3.0f), 21, 16, 597, 448);   /* nes with NTSC filter width */
    RECT(viewport(&s, 640, 480, 160, 144, 0), 80, 24, 480, 432);              /* aspect 0 = w/h (160/144 = 10/9) */
    RECT(viewport(&s, 640, 480, 640, 480, 4.0f / 3.0f), 0, 0, 640, 480);      /* factor 1 */
    RECT(viewport(&s, 640, 480, 320, 600, 0), 192, 0, 256, 480);  /* taller than the screen: no integer factor, fit by aspect */
    s.scale = SCALE_FULL;
    RECT(viewport(&s, 640, 480, 256, 224, 4.0f / 3.0f), 0, 0, 640, 480);
    s.scale = SCALE_ASPECT;
    RECT(viewport(&s, 640, 480, 320, 240, 4.0f / 3.0f), 0, 0, 640, 480);      /* psx */
    RECT(viewport(&s, 640, 480, 224, 288, 7.0f / 9.0f), 133, 0, 373, 480);    /* vertical arcade */
    RECT(viewport(&s, 640, 480, 320, 200, 1.6f), 0, 40, 640, 400);            /* scummvm 16:10 */
    s.scale = SCALE_CUSTOM; s.cx = 80; s.cy = 12; s.cw = 480; s.ch = 432;
    RECT(viewport(&s, 640, 480, 160, 144, 10.0f / 9.0f), 80, 12, 480, 432);   /* gbc */
    TEST_END();
}
