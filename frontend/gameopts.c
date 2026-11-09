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
static bool fr_drawn[MAXFRAMES];   /* drawn around the picture (@style) instead of a PNG */
static int fr_n, fr_default;   /* fr_default: systems.cfg sets an overlay -> "Standard" first */
static bool sys_crt, sys_full;   /* systems.cfg: crt = yes; its Bild fills the screen */

static void add_label(void *a, const char *k, const char *v) {
    (void)a;
    if (fr_n < MAXFRAMES) { fr_drawn[fr_n] = v[0] == '@'; snprintf(fr_label[fr_n++], sizeof fr_label[0], "%s", k); }
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
    for (int i = 0; i < 2 && fr_n < MAXFRAMES; i++) {
        fr_drawn[fr_n] = true;
        snprintf(fr_label[fr_n++], sizeof fr_label[0], "%s", CONFIG_PLAIN[i]);
    }
    sys_crt = !config_get(sys, system, "crt", v, sizeof v) && !strcmp(v, "yes");
    if (config_get(sys, system, "scale", v, sizeof v) && config_get(sys, "default", "scale", v, sizeof v)) v[0] = 0;
    sys_full = !strcmp(v, "full");
}

static int frame_count(void) { return fr_default + 1 + fr_n; }

#define F_SCANLINES 4   /* VAL[GO_FILTER] */
#define S_INTEGER 1     /* VAL[GO_SCALE] */
#define S_FULL 3

const char *gameopt_label(GameOpt o) { return LABEL[o]; }
int gameopt_count(GameOpt o) {
    if (o == GO_FRAME) return frame_count();
    if (o == GO_FILTER && !sys_crt) return F_SCANLINES;   /* Scanlines only for TV/arcade systems */
    return COUNT[o];
}

const char *gameopt_choice(GameOpt o, int i) {
    if (o != GO_FRAME) return i >= 0 && i < COUNT[o] ? TXT[o][i] : "";
    if (fr_default && i == 0) return "Standard";
    if (i == fr_default) return "Aus";
    i -= fr_default + 1;
    return i >= 0 && i < fr_n ? fr_label[i] : "";
}

/* the Rahmen option: every system has at least the drawn frames */
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
    frames_for(user, system);   /* this system's frames and kind, for count/choice */
    if (o == GO_FRAME) return frame_get(user, system);
    char v[64];
    if (config_get(user, system, KEY[o], v, sizeof v)) return 0;
    for (int i = 1; i < gameopt_count(o); i++) if (!strcmp(v, VAL[o][i])) return i;
    return 0;
}

/* what a Rahmen choice is: 'n' none, 'p' a PNG frame (it places the picture), 'd' drawn */
static char frame_kind(int i) {
    if (fr_default && i == 0) return 'p';
    if (i <= fr_default) return 'n';
    i -= fr_default + 1;
    return i < fr_n ? (fr_drawn[i] ? 'd' : 'p') : 'n';
}

static int set_raw(const char *user, const char *system, GameOpt o, int i) {
    if (o == GO_FRAME) return frame_set(user, system, i);
    return config_set(user, system, KEY[o], i > 0 && i < COUNT[o] ? VAL[o][i] : NULL);
}

/* The options depend on each other; a choice adjusts the others so they never contradict:
 *  - Scanlines need Ganzzahlig: choosing them sets Bild to Ganzzahlig, leaving them sets Bild
 *    back to Standard (unless a drawn frame still needs the border); another Bild ends them.
 *  - a PNG frame places the picture in its window: choosing one sets Bild to Standard;
 *    another Bild turns it off.
 *  - a drawn frame needs a border: choosing one when Bild fills the screen sets Ganzzahlig;
 *    Vollbild (or a full-screen Standard) turns it off. */
int gameopt_set(const char *user, const char *system, GameOpt o, int i) {
    int scale = gameopt_get(user, system, GO_SCALE), filter = gameopt_get(user, system, GO_FILTER);
    char kind = frame_kind(gameopt_get(user, system, GO_FRAME));
    int r = set_raw(user, system, o, i);
    if (o == GO_FILTER) {
        if (i == F_SCANLINES && filter != F_SCANLINES) {
            r |= set_raw(user, system, GO_SCALE, S_INTEGER);
            if (kind == 'p') r |= set_raw(user, system, GO_FRAME, fr_default);   /* Aus */
        } else if (filter == F_SCANLINES && i != F_SCANLINES && kind != 'd')
            r |= set_raw(user, system, GO_SCALE, 0);
    } else if (o == GO_SCALE) {
        int full = i == S_FULL || (i == 0 && sys_full);
        if (filter == F_SCANLINES && i != S_INTEGER) r |= set_raw(user, system, GO_FILTER, 0);
        if ((kind == 'p' && i != 0) || (kind == 'd' && full)) r |= set_raw(user, system, GO_FRAME, fr_default);
    } else if (o == GO_FRAME) {
        char k = frame_kind(i);
        if (k == 'p') {
            r |= set_raw(user, system, GO_SCALE, 0);
            if (filter == F_SCANLINES) r |= set_raw(user, system, GO_FILTER, 0);
        } else if (k == 'd' && (scale == S_FULL || (scale == 0 && sys_full)))
            r |= set_raw(user, system, GO_SCALE, S_INTEGER);
    }
    return r;
}
