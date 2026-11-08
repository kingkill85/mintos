// uses: frontend/config.c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"
#include "config.h"

static char *slurp(const char *p) {
    static char buf[2048];
    FILE *f = fopen(p, "r");
    size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0;
    if (f) fclose(f);
    buf[n] = 0;
    return buf;
}

int main(void) {
    char sys[] = "/tmp/rgarch-test-sys-XXXXXX", user[] = "/tmp/rgarch-test-user-XXXXXX";
    FILE *f = fdopen(mkstemp(sys), "w");
    fputs("[default]\nscale = integer\n[gbc]\ncore = gambatte\nscale = custom 80 12 480 432\noverlay = rgc/J.png\n[nes]\nscale = full\n", f);
    fclose(f);
    close(mkstemp(user));
    unlink(user);

    /* user overrides: missing file changes nothing */
    SystemCfg s;
    config_load(sys, "gbc", &s);
    CHECK_INT(config_merge(user, "gbc", &s), -1);
    CHECK_INT(s.scale, SCALE_CUSTOM);

    /* set creates the section; merge applies only that system */
    CHECK_INT(config_set(user, "gbc", "overlay", "none"), 0);
    CHECK_INT(config_set(user, "gbc", "filter", "sharp"), 0);
    CHECK_INT(config_set(user, "nes", "filter", "linear"), 0);
    config_load(sys, "gbc", &s);
    CHECK_INT(config_merge(user, "gbc", &s), 0);
    CHECK_STR(s.overlay, "");                   /* "none" = no frame */
    CHECK_INT(s.filter, FILTER_SHARP);
    CHECK_INT(s.scale, SCALE_CUSTOM);           /* untouched key keeps systems.cfg */
    CHECK_STR(s.core, "gambatte");

    /* replacing a key keeps one line; removing it falls back */
    CHECK_INT(config_set(user, "gbc", "filter", "nearest"), 0);
    char v[64];
    CHECK_INT(config_get(user, "gbc", "filter", v, sizeof v), 0);
    CHECK_STR(v, "nearest");
    CHECK(strstr(slurp(user), "sharp") == NULL);
    CHECK_INT(config_set(user, "gbc", "filter", NULL), 0);
    CHECK_INT(config_get(user, "gbc", "filter", v, sizeof v), -1);
    CHECK_INT(config_get(user, "nes", "filter", v, sizeof v), 0);   /* other section intact */
    CHECK_STR(v, "linear");
    CHECK_INT(config_get(user, "gbc", "overlay", v, sizeof v), 0);
    CHECK_STR(v, "none");

    /* config_get reads the shipped file too ([section] lookup only, no defaults) */
    CHECK_INT(config_get(sys, "gbc", "overlay", v, sizeof v), 0);
    CHECK_STR(v, "rgc/J.png");
    CHECK_INT(config_get(sys, "nes", "overlay", v, sizeof v), -1);
    CHECK_INT(config_get(sys, "default", "scale", v, sizeof v), 0);
    CHECK_STR(v, "integer");
    unlink(sys); unlink(user);
    TEST_END();
}
