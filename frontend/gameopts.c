#include "gameopts.h"
#include <stdio.h>
#include <string.h>

static const char *KEY[GO_N] = {"scale", "filter", "overlay"};
static const char *LABEL[GO_N] = {"Bild", "Filter", "Rahmen"};
static const char *VAL[GO_N][5] = {
    {NULL, "integer", "aspect", "full"},
    {NULL, "nearest", "sharp", "linear", "scanlines"},
    {NULL, "none"},
};
static const char *TXT[GO_N][5] = {
    {"Standard", "Ganzzahlig", "Seitenverhältnis", "Vollbild"},
    {"Standard", "Pixel", "Scharf", "Weich", "Scanlines"},
    {"An", "Aus"},
};
static const int COUNT[GO_N] = {4, 5, 2};

/* Rahmen: [Standard (systems.cfg's frame), ] Aus, then the labels of frames.cfg [system].
 * The list belongs to one system; it is re-read (small files) from the directory of the cfg
 * file passed in, and gameopt_count/choice use the last one read. */
#define MAXFRAMES 24
static char fr_label[MAXFRAMES][96];
static int fr_n, fr_default;   /* fr_default: systems.cfg sets an overlay -> "Standard" first */

static void add_label(void *a, const char *k, const char *v) {
    (void)a; (void)v;
    if (fr_n < MAXFRAMES) snprintf(fr_label[fr_n++], sizeof fr_label[0], "%s", k);
}

/* any cfg path in the rgarch directory (systems.cfg or user.cfg) -> that directory's files */
static void frames_for(const char *cfg, const char *system) {
    const char *slash = strrchr(cfg, '/');
    int dl = slash ? (int)(slash - cfg + 1) : 0;
    char dir[1024], sys[1100], frames[1100], v[256];
    snprintf(dir, sizeof dir, "%.*s", dl, cfg);
    snprintf(sys, sizeof sys, "%ssystems.cfg", dir);
    snprintf(frames, sizeof frames, "%sframes.cfg", dir);
    fr_n = 0;
    fr_default = !config_get(sys, system, "overlay", v, sizeof v) && *v && strcmp(v, "none");
    config_each(frames, system, add_label, NULL);
}

static int frame_count(void) { return fr_default + 1 + fr_n; }

const char *gameopt_label(GameOpt o) { return LABEL[o]; }
int gameopt_count(GameOpt o) { return o == GO_FRAME ? frame_count() : COUNT[o]; }

const char *gameopt_choice(GameOpt o, int i) {
    if (o != GO_FRAME) return i >= 0 && i < COUNT[o] ? TXT[o][i] : "";
    if (fr_default && i == 0) return "Standard";
    if (i == fr_default) return "Aus";
    i -= fr_default + 1;
    return i >= 0 && i < fr_n ? fr_label[i] : "";
}

/* the Rahmen option is offered when there is something to choose */
bool gameopt_has_frame(const char *syscfg, const char *system) {
    frames_for(syscfg, system);
    return fr_default || fr_n > 0;
}

static int frame_get(const char *user, const char *system) {
    frames_for(user, system);
    char v[96];
    if (config_get(user, system, "frame", v, sizeof v)) {
        /* older user.cfg: "overlay = none" meant Aus */
        if (!config_get(user, system, "overlay", v, sizeof v) && !strcmp(v, "none")) return fr_default;
        return 0;
    }
    if (!strcmp(v, "none")) return fr_default;
    for (int i = 0; i < fr_n; i++) if (!strcmp(v, fr_label[i])) return fr_default + 1 + i;
    return 0;
}

static int frame_set(const char *user, const char *system, int i) {
    frames_for(user, system);
    int r = config_set(user, system, "overlay", NULL);
    if (i <= 0 || (i == fr_default && !fr_default)) return r | config_set(user, system, "frame", NULL);
    if (i == fr_default) return r | config_set(user, system, "frame", "none");
    i -= fr_default + 1;
    if (i >= fr_n) return -1;
    return r | config_set(user, system, "frame", fr_label[i]);
}

int gameopt_get(const char *user, const char *system, GameOpt o) {
    if (o == GO_FRAME) return frame_get(user, system);
    char v[64];
    if (config_get(user, system, KEY[o], v, sizeof v)) return 0;
    for (int i = 1; i < COUNT[o]; i++) if (!strcmp(v, VAL[o][i])) return i;
    return 0;
}

#define F_SCANLINES 4   /* VAL[GO_FILTER] */
#define S_INTEGER 1     /* VAL[GO_SCALE] */

/* Scanlines need whole screen rows per game row: choosing them also sets Bild to Ganzzahlig,
 * leaving them sets Bild back to Standard */
int gameopt_set(const char *user, const char *system, GameOpt o, int i) {
    if (o == GO_FRAME) return frame_set(user, system, i);
    int was = o == GO_FILTER ? gameopt_get(user, system, GO_FILTER) : -1;
    int r = config_set(user, system, KEY[o], i > 0 && i < COUNT[o] ? VAL[o][i] : NULL);
    if (o == GO_FILTER && i == F_SCANLINES && was != F_SCANLINES)
        r |= config_set(user, system, KEY[GO_SCALE], VAL[GO_SCALE][S_INTEGER]);
    else if (o == GO_FILTER && was == F_SCANLINES && i != F_SCANLINES)
        r |= config_set(user, system, KEY[GO_SCALE], NULL);
    return r;
}
