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
