/* Per-system settings from /storage/.config/rgarch/systems.cfg. */
#pragma once

typedef enum { SCALE_INTEGER, SCALE_ASPECT, SCALE_FULL, SCALE_CUSTOM } ScaleMode;
typedef enum { FILTER_NEAREST, FILTER_SHARP, FILTER_LINEAR } Filter;

typedef struct {
    char core[64];
    ScaleMode scale;
    int cx, cy, cw, ch;   /* SCALE_CUSTOM rectangle on the 640x480 screen */
    Filter filter;
    char overlay[256];    /* absolute PNG path, "" for none */
} SystemCfg;

#define CONFIG_FILE "/storage/.config/rgarch/systems.cfg"
#define OVERLAY_DIR "/storage/.config/retroarch/overlays/"

/* Fills *out from [default] then [system]. 0 ok (also when the system has no
 * section), -1 when the file cannot be read. */
int config_load(const char *file, const char *system, SystemCfg *out);
