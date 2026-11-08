// uses: common/settings.c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"
#include "settings.h"

int main(void) {
    /* brightness: 10 steps, never 0 (black panel), round trip */
    CHECK_INT(bl_raw(10, 1666), 1666);
    CHECK_INT(bl_raw(1, 1666), 166);
    CHECK_INT(bl_raw(0, 1666), 166);     /* clamped */
    CHECK_INT(bl_raw(11, 1666), 1666);
    CHECK_INT(bl_step(833, 1666), 5);
    CHECK_INT(bl_step(1666, 1666), 10);
    CHECK_INT(bl_step(1, 1666), 1);
    for (int s = 1; s <= 10; s++) CHECK_INT(bl_step(bl_raw(s, 1666), 1666), s);

    /* key = value file: missing file/key gives the default, set keeps other keys */
    char path[] = "/tmp/mintos-test-kv-XXXXXX";
    close(mkstemp(path));
    unlink(path);
    CHECK_INT(kv_get_int(path, "screen_off", 3), 3);
    CHECK_INT(kv_set(path, "screen_off", "5"), 0);
    CHECK_INT(kv_set(path, "other", "x y"), 0);
    CHECK_INT(kv_get_int(path, "screen_off", 3), 5);
    CHECK_INT(kv_set(path, "screen_off", "0"), 0);
    CHECK_INT(kv_get_int(path, "screen_off", 3), 0);
    char v[32];
    CHECK_INT(kv_get(path, "other", v, sizeof v), 0);
    CHECK_STR(v, "x y");
    CHECK_INT(kv_get(path, "nope", v, sizeof v), -1);
    /* a key that is a prefix of another must not match it */
    CHECK_INT(kv_set(path, "screen", "9"), 0);
    CHECK_INT(kv_get_int(path, "screen_off", 3), 0);
    CHECK_INT(kv_get_int(path, "screen", 3), 9);
    /* empty value removes the key */
    CHECK_INT(kv_set(path, "other", NULL), 0);
    CHECK_INT(kv_get(path, "other", v, sizeof v), -1);
    unlink(path);

    /* cycling a choice list left/right wraps */
    CHECK_INT(cycle(0, -1, 4), 3);
    CHECK_INT(cycle(3, 1, 4), 0);
    CHECK_INT(cycle(1, 1, 4), 2);
    TEST_END();
}
