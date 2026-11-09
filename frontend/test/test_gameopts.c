// uses: frontend/config.c frontend/gameopts.c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"
#include "gameopts.h"

int main(void) {
    char sys[] = "/tmp/rgarch-test-sys-XXXXXX", user[] = "/tmp/rgarch-test-user-XXXXXX";
    FILE *f = fdopen(mkstemp(sys), "w");
    fputs("[default]\nscale = integer\n[gbc]\nscale = custom 80 12 480 432\noverlay = rgc/J.png\n[nes]\nscale = full\n", f);
    fclose(f);
    close(mkstemp(user));
    unlink(user);

    CHECK_STR(gameopt_label(GO_SCALE), "Bild");
    CHECK_STR(gameopt_label(GO_FILTER), "Filter");
    CHECK_STR(gameopt_label(GO_FRAME), "Rahmen");
    CHECK_INT(gameopt_count(GO_SCALE), 4);
    CHECK_STR(gameopt_choice(GO_SCALE, 0), "Standard");
    CHECK_STR(gameopt_choice(GO_SCALE, 3), "Vollbild");
    CHECK_STR(gameopt_choice(GO_FILTER, 2), "Scharf");
    CHECK_STR(gameopt_choice(GO_FRAME, 1), "Aus");

    CHECK(gameopt_has_frame(sys, "gbc"));
    CHECK(!gameopt_has_frame(sys, "nes"));

    /* nothing set = Standard; set writes user.cfg, Standard removes the key again */
    CHECK_INT(gameopt_get(user, "gbc", GO_SCALE), 0);
    CHECK_INT(gameopt_set(user, "gbc", GO_SCALE, 2), 0);
    CHECK_INT(gameopt_set(user, "gbc", GO_FRAME, 1), 0);
    CHECK_INT(gameopt_get(user, "gbc", GO_SCALE), 2);
    CHECK_INT(gameopt_get(user, "gbc", GO_FRAME), 1);
    SystemCfg c;
    config_load(sys, "gbc", &c);
    config_merge(user, "gbc", &c);
    CHECK_INT(c.scale, SCALE_ASPECT);
    CHECK_STR(c.overlay, "");
    CHECK_INT(gameopt_set(user, "gbc", GO_SCALE, 0), 0);
    config_load(sys, "gbc", &c);
    config_merge(user, "gbc", &c);
    CHECK_INT(c.scale, SCALE_CUSTOM);
    CHECK_INT(gameopt_get(user, "gbc", GO_SCALE), 0);
    /* an unknown value in user.cfg reads as Standard */
    config_set(user, "gbc", "filter", "weird");
    CHECK_INT(gameopt_get(user, "gbc", GO_FILTER), 0);
    /* Scanlines set Bild to Ganzzahlig; leaving them sets it back to Standard */
    CHECK_INT(gameopt_set(user, "snes", GO_FILTER, 4), 0);
    CHECK_STR(gameopt_choice(GO_FILTER, 4), "Scanlines");
    CHECK_INT(gameopt_get(user, "snes", GO_SCALE), 1);
    CHECK_INT(gameopt_set(user, "snes", GO_SCALE, 3), 0);          /* changing Bild by hand afterwards sticks */
    CHECK_INT(gameopt_set(user, "snes", GO_FILTER, 4), 0);         /* same filter again: Bild untouched */
    CHECK_INT(gameopt_get(user, "snes", GO_SCALE), 3);
    CHECK_INT(gameopt_set(user, "snes", GO_FILTER, 2), 0);
    CHECK_INT(gameopt_get(user, "snes", GO_SCALE), 0);
    CHECK_INT(gameopt_get(user, "snes", GO_FILTER), 2);
    unlink(sys); unlink(user);
    TEST_END();
}
