#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

int bl_raw(int step, int max) {
    if (step < 1) step = 1;
    if (step > 10) step = 10;
    return max * step / 10;
}

int bl_step(int raw, int max) {
    if (max <= 0) return 10;
    int s = (raw * 10 + max / 2) / max;
    return s < 1 ? 1 : s > 10 ? 10 : s;
}

static int read_int(const char *p) {
    FILE *f = fopen(p, "r");
    int v = -1;
    if (f) { if (fscanf(f, "%d", &v) != 1) v = -1; fclose(f); }
    return v;
}

static void write_int(const char *p, int v) {
    FILE *f = fopen(p, "w");
    if (f) { fprintf(f, "%d\n", v); fclose(f); }
}

int bl_get(void) {
    int max = read_int(BACKLIGHT "/max_brightness"), cur = read_int(BACKLIGHT "/brightness");
    return max > 0 && cur >= 0 ? bl_step(cur, max) : -1;
}

void bl_set(int step) {
    int max = read_int(BACKLIGHT "/max_brightness");
    if (max > 0) write_int(BACKLIGHT "/brightness", bl_raw(step, max));
}

void bl_power(int on) { write_int(BACKLIGHT "/bl_power", on ? 0 : 4); }

int vol_get(void) {
    FILE *p = popen("amixer -M -c rk817int sget Master 2>/dev/null", "r");
    if (!p) return -1;
    char line[256];
    int v = -1;
    while (fgets(line, sizeof line, p)) {
        char *b = strchr(line, '[');
        if (b && v < 0 && strstr(line, "Playback")) v = atoi(b + 1);
    }
    pclose(p);
    return v;
}

void vol_set(int pct) {
    char cmd[96];
    snprintf(cmd, sizeof cmd, "amixer -q -M -c rk817int sset Master %d%% >/dev/null 2>&1", pct < 0 ? 0 : pct > 100 ? 100 : pct);
    if (system(cmd)) {}
}

/* the value of "key = value" if line holds key, else NULL */
static char *match(char *line, const char *key) {
    char *s = line;
    while (isspace((unsigned char)*s)) s++;
    size_t n = strlen(key);
    if (strncmp(s, key, n)) return NULL;
    s += n;
    while (*s == ' ' || *s == '\t') s++;
    if (*s != '=') return NULL;
    s++;
    while (*s == ' ' || *s == '\t') s++;
    s[strcspn(s, "\r\n")] = 0;
    return s;
}

int kv_get(const char *file, const char *key, char *out, size_t n) {
    FILE *f = fopen(file, "r");
    if (!f) return -1;
    char line[512];
    int r = -1;
    while (fgets(line, sizeof line, f)) {
        char *v = match(line, key);
        if (v) { snprintf(out, n, "%s", v); r = 0; }
    }
    fclose(f);
    return r;
}

int kv_get_int(const char *file, const char *key, int def) {
    char v[32];
    return kv_get(file, key, v, sizeof v) ? def : atoi(v);
}

int kv_set(const char *file, const char *key, const char *val) {
    char tmp[600], line[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", file);
    char dir[600];
    snprintf(dir, sizeof dir, "%s", file);
    char *slash = strrchr(dir, '/');
    if (slash && slash != dir) { *slash = 0; mkdir(dir, 0755); }
    FILE *out = fopen(tmp, "w");
    if (!out) return -1;
    FILE *in = fopen(file, "r");
    if (in) {
        while (fgets(line, sizeof line, in)) {
            char copy[512];
            snprintf(copy, sizeof copy, "%s", line);
            if (!match(copy, key)) fputs(line, out);
        }
        fclose(in);
    }
    if (val && *val) fprintf(out, "%s = %s\n", key, val);
    if (fclose(out)) return -1;
    return rename(tmp, file);
}

int cycle(int i, int d, int n) { return n > 0 ? ((i + d) % n + n) % n : 0; }
