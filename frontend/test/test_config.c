// uses: frontend/config.c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"
#include "config.h"

static const char *write_cfg(const char *text) {
    static char path[] = "/tmp/rgarch-test-cfg-XXXXXX";
    int fd = mkstemp(path);
    FILE *f = fdopen(fd, "w");
    fputs(text, f);
    fclose(f);
    return path;
}

int main(void) {
    const char *p = write_cfg(
        "# test\n[default]\nscale = integer\nfilter = nearest\n\n"
        "[gbc]\ncore = gambatte\nscale = custom 80 12 480 432\noverlay = rgc/Jeltron_GB-Color.png\n\n"
        "[nes]\nfilter = sharp # comment\nscale=full\n"
        "[x]\noverlay = /abs/frame.png\n");
    SystemCfg s;

    CHECK_INT(config_load(p, "gbc", &s), 0);
    CHECK_STR(s.core, "gambatte");
    CHECK_INT(s.scale, SCALE_CUSTOM);
    CHECK_INT(s.cx, 80); CHECK_INT(s.cy, 12); CHECK_INT(s.cw, 480); CHECK_INT(s.ch, 432);
    CHECK_INT(s.filter, FILTER_NEAREST);
    CHECK_STR(s.overlay, "/storage/.config/retroarch/overlays/rgc/Jeltron_GB-Color.png");

    CHECK_INT(config_load(p, "nes", &s), 0);
    CHECK_INT(s.filter, FILTER_SHARP);
    CHECK_INT(s.scale, SCALE_FULL);
    CHECK_STR(s.core, "");
    CHECK_STR(s.overlay, "");

    CHECK_INT(config_load(p, "x", &s), 0);
    CHECK_STR(s.overlay, "/abs/frame.png");

    CHECK_INT(config_load(p, "unknown", &s), 0);  /* defaults only */
    CHECK_INT(s.scale, SCALE_INTEGER);
    CHECK_STR(s.core, "");

    CHECK_INT(config_load("/nonexistent/systems.cfg", "gb", &s), -1);
    unlink(p);
    TEST_END();
}
