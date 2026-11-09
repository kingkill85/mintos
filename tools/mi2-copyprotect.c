/* Answer Monkey Island 2's Mix'n'Mojo copy protection automatically.
 *
 * ScummVM keeps the screen (the difficulty choice follows it) but accepts any numbers.
 * Started in the background by launch.sh for the "monkey2" target: watches the panel for
 * the purple protection screen (132,0,181), then presses Return and types 1234 + Return
 * through a temporary uinput keyboard (RetroArch hands keyboard input to the core). */
#include "fb.h"
#include "util.h"
#include <linux/input.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int purple(void) {
    static const unsigned pts[2][2] = { { 20, 100 }, { 620, 400 } };
    static const int want[3] = { 132, 0, 181 };
    Fb fb;
    if (fb_open(&fb)) return 0;
    int ok = 1;
    for (int i = 0; i < 2 && ok; i++) {
        uint8_t c[3];
        if (pts[i][0] >= fb.w || pts[i][1] >= fb.h) { ok = 0; break; }
        fb_rgb(&fb, pts[i][0], pts[i][1], c);
        for (int k = 0; k < 3; k++) ok = ok && abs(c[k] - want[k]) < 12;
    }
    fb_close(&fb);
    return ok;
}

static void press(int ui, int key) { input_tap(ui, key, 150, 400); }

int main(void) {
    const int keys[] = { KEY_1, KEY_2, KEY_3, KEY_4, KEY_ENTER };
    int ui = uinput_keys("mi2-copyprotect", keys, 5);
    if (ui < 0) return 1;
    time_t end = time(NULL) + 90;
    int found = 0;
    while (time(NULL) < end && !(found = purple())) sleep_ms(500);
    if (found) {
        sleep_ms(1000); press(ui, KEY_ENTER);   /* "Drücke Return ..." */
        sleep_ms(3000); press(ui, KEY_1); press(ui, KEY_2); press(ui, KEY_3); press(ui, KEY_4);
        sleep_ms(1000); press(ui, KEY_ENTER);
    }
    sleep_ms(1000);
    close(ui);
    return 0;
}
