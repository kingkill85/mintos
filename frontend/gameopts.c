#include "gameopts.h"
#include <string.h>

static const char *KEY[GO_N] = {"scale", "filter", "overlay"};
static const char *LABEL[GO_N] = {"Bild", "Filter", "Rahmen"};
static const char *VAL[GO_N][4] = {
    {NULL, "integer", "aspect", "full"},
    {NULL, "nearest", "sharp", "linear"},
    {NULL, "none"},
};
static const char *TXT[GO_N][4] = {
    {"Standard", "Ganzzahlig", "Seitenverhältnis", "Vollbild"},
    {"Standard", "Pixel", "Scharf", "Weich"},
    {"An", "Aus"},
};
static const int COUNT[GO_N] = {4, 4, 2};

const char *gameopt_label(GameOpt o) { return LABEL[o]; }
int gameopt_count(GameOpt o) { return COUNT[o]; }
const char *gameopt_choice(GameOpt o, int i) { return i >= 0 && i < COUNT[o] ? TXT[o][i] : ""; }

bool gameopt_has_frame(const char *syscfg, const char *system) {
    char v[256];
    return !config_get(syscfg, system, "overlay", v, sizeof v) && *v;
}

int gameopt_get(const char *user, const char *system, GameOpt o) {
    char v[64];
    if (config_get(user, system, KEY[o], v, sizeof v)) return 0;
    for (int i = 1; i < COUNT[o]; i++) if (!strcmp(v, VAL[o][i])) return i;
    return 0;
}

int gameopt_set(const char *user, const char *system, GameOpt o, int i) {
    return config_set(user, system, KEY[o], i > 0 && i < COUNT[o] ? VAL[o][i] : NULL);
}
