#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static void apply(SystemCfg *c, const char *key, const char *val) {
    if (!strcmp(key, "core")) {
        snprintf(c->core, sizeof c->core, "%s", val);
    } else if (!strcmp(key, "filter")) {
        c->filter = !strcmp(val, "sharp") ? FILTER_SHARP : !strcmp(val, "linear") ? FILTER_LINEAR : FILTER_NEAREST;
    } else if (!strcmp(key, "scale")) {
        if (!strncmp(val, "custom", 6) && sscanf(val + 6, "%d %d %d %d", &c->cx, &c->cy, &c->cw, &c->ch) == 4)
            c->scale = SCALE_CUSTOM;
        else
            c->scale = !strcmp(val, "aspect") ? SCALE_ASPECT : !strcmp(val, "full") ? SCALE_FULL : SCALE_INTEGER;
    } else if (!strcmp(key, "overlay")) {
        snprintf(c->overlay, sizeof c->overlay, "%s%s", *val && *val != '/' ? OVERLAY_DIR : "", val);
    }
}

int config_load(const char *file, const char *system, SystemCfg *out) {
    memset(out, 0, sizeof *out);
    out->scale = SCALE_INTEGER;
    out->filter = FILTER_NEAREST;
    FILE *f = fopen(file, "r");
    if (!f) return -1;
    char line[512], section[64] = "";
    /* two passes: [default] first, then the system's own section wins */
    for (int pass = 0; pass < 2; pass++) {
        const char *want = pass ? system : "default";
        rewind(f);
        while (fgets(line, sizeof line, f)) {
            char *hash = strchr(line, '#');
            if (hash) *hash = 0;
            char *s = trim(line);
            if (*s == '[') {
                char *end = strchr(s, ']');
                if (end) { *end = 0; snprintf(section, sizeof section, "%s", s + 1); }
                continue;
            }
            char *eq = strchr(s, '=');
            if (!eq || strcmp(section, want)) continue;
            *eq = 0;
            apply(out, trim(s), trim(eq + 1));
        }
        section[0] = 0;
    }
    fclose(f);
    return 0;
}
