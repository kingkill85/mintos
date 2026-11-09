#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>

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
        c->filter = !strcmp(val, "sharp") ? FILTER_SHARP : !strcmp(val, "linear") ? FILTER_LINEAR
                  : !strcmp(val, "scanlines") ? FILTER_SCANLINES : FILTER_NEAREST;
    } else if (!strcmp(key, "scale")) {
        if (!strncmp(val, "custom", 6) && sscanf(val + 6, "%d %d %d %d", &c->cx, &c->cy, &c->cw, &c->ch) == 4)
            c->scale = SCALE_CUSTOM;
        else
            c->scale = !strcmp(val, "aspect") ? SCALE_ASPECT : !strcmp(val, "full") ? SCALE_FULL : SCALE_INTEGER;
    } else if (!strcmp(key, "overlay")) {
        if (!strcmp(val, "none")) { c->overlay[0] = 0; return; }
        snprintf(c->overlay, sizeof c->overlay, "%s%s", *val && *val != '/' ? OVERLAY_DIR : "", val);
    }
}

/* calls fn for every key = value in [want]; -1 when the file cannot be read */
static int each(const char *file, const char *want, void (*fn)(void *, const char *, const char *), void *arg) {
    FILE *f = fopen(file, "r");
    if (!f) return -1;
    char line[512], section[64] = "";
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
        fn(arg, trim(s), trim(eq + 1));
    }
    fclose(f);
    return 0;
}

static void apply_cb(void *c, const char *k, const char *v) { apply(c, k, v); }

int config_load(const char *file, const char *system, SystemCfg *out) {
    memset(out, 0, sizeof *out);
    out->scale = SCALE_INTEGER;
    out->filter = FILTER_NEAREST;
    /* [default] first, then the system's own section wins */
    if (each(file, "default", apply_cb, out)) return -1;
    each(file, system, apply_cb, out);
    return 0;
}

int config_merge(const char *file, const char *system, SystemCfg *out) {
    return each(file, system, apply_cb, out);
}

struct get { const char *key; char *out; int n, found; };
static void get_cb(void *a, const char *k, const char *v) {
    struct get *g = a;
    if (!strcmp(k, g->key)) { snprintf(g->out, g->n, "%s", v); g->found = 1; }
}

int config_get(const char *file, const char *section, const char *key, char *out, int n) {
    struct get g = {key, out, n, 0};
    each(file, section, get_cb, &g);
    return g.found ? 0 : -1;
}

int config_set(const char *file, const char *section, const char *key, const char *val) {
    char tmp[600], line[512], copy[512], cur[64] = "";
    snprintf(tmp, sizeof tmp, "%s.tmp", file);
    FILE *out = fopen(tmp, "w");
    if (!out) return -1;
    FILE *in = fopen(file, "r");
    int seen = 0, done = !val || !*val;
    while (in && fgets(line, sizeof line, in)) {
        snprintf(copy, sizeof copy, "%s", line);
        char *s = trim(copy);
        if (*s == '[') {
            if (!strcmp(cur, section) && !done) { fprintf(out, "%s = %s\n", key, val); done = 1; }
            char *end = strchr(s, ']');
            if (end) { *end = 0; snprintf(cur, sizeof cur, "%s", s + 1); }
            if (!strcmp(cur, section)) seen = 1;
            fputs(line, out);
            continue;
        }
        char *eq = strchr(s, '=');
        if (eq && !strcmp(cur, section)) {
            *eq = 0;
            if (!strcmp(trim(s), key)) continue;   /* replaced below / removed */
        }
        fputs(line, out);
    }
    if (in) fclose(in);
    if (!done) {
        if (!(seen && !strcmp(cur, section))) fprintf(out, "[%s]\n", section);
        fprintf(out, "%s = %s\n", key, val);
    }
    if (fclose(out)) return -1;
    return rename(tmp, file);
}

static const BorderStyle STYLES[] = {
    {"schwarz", 0x171717, 0x3c3c3c, 0, 0},
    {"grau", 0xc6c5cb, 0x8f8d99, 0, 0},
    {"schwarz-rot", 0x141414, 0x3a3a3a, 0xb81d24, 0},
    {"grau-lila", 0xc6c5cb, 0x8f8d99, 0x5b4a8b, 0},
    {"snes-schwarz", 0x171717, 0x3c3c3c, 0, 1},
    {"snes-grau", 0xc6c5cb, 0x8f8d99, 0, 1},
    {"sfc-hell", 0xd9d8dc, 0xa9a7b1, 0, 1},
};
#define NSTYLES (int)(sizeof STYLES / sizeof STYLES[0])
const char *const CONFIG_PLAIN[2] = {"Schlicht schwarz", "Schlicht grau"};

const BorderStyle *config_border(int border) { return border >= 1 && border <= NSTYLES ? &STYLES[border - 1] : NULL; }

static int style_index(const char *name) {
    for (int i = 0; i < NSTYLES; i++) if (!strcmp(name, STYLES[i].name)) return i + 1;
    return 0;
}

int config_border_style(const char *label) {
    for (int i = 0; i < 2; i++) if (!strcmp(label, CONFIG_PLAIN[i])) return i + 1;   /* schwarz, grau */
    return 0;
}

int config_each(const char *file, const char *section, void (*fn)(void *, const char *, const char *), void *arg) {
    return each(file, section, fn, arg);
}

int config_frame_parse(const char *val, char *png, int n, int *x, int *y, int *w, int *h) {
    char p[256];
    if (sscanf(val, "%255s %d %d %d %d", p, x, y, w, h) != 5 || *w <= 0 || *h <= 0) return -1;
    snprintf(png, n, "%s", p);
    return 0;
}

void config_frame(const char *frames_file, const char *user_file, const char *system, SystemCfg *c) {
    char label[96], val[300], png[256];
    if (config_get(user_file, system, "frame", label, sizeof label)) return;
    if (!strcmp(label, "none")) { c->overlay[0] = 0; return; }
    int b = config_border_style(label);
    if (b) { c->overlay[0] = 0; c->border = b; return; }   /* drawn around any picture, keeps its size */
    int x, y, w, h;
    if (config_get(frames_file, system, label, val, sizeof val)) return;   /* unknown: keep the default */
    if (val[0] == '@') {
        if ((b = style_index(val + 1))) { c->overlay[0] = 0; c->border = b; }
        return;
    }
    if (config_frame_parse(val, png, sizeof png, &x, &y, &w, &h)) return;
    snprintf(c->overlay, sizeof c->overlay, "%s%s", OVERLAY_DIR, png);
    c->scale = SCALE_CUSTOM; c->cx = x; c->cy = y; c->cw = w; c->ch = h;
}
