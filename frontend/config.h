/* Per-system settings from /storage/.config/rgarch/systems.cfg. */
#pragma once

typedef enum { SCALE_INTEGER, SCALE_ASPECT, SCALE_FULL, SCALE_CUSTOM } ScaleMode;
typedef enum { FILTER_NEAREST, FILTER_SHARP, FILTER_LINEAR, FILTER_SCANLINES } Filter;

typedef struct {
    char core[64];
    ScaleMode scale;
    int cx, cy, cw, ch;   /* SCALE_CUSTOM rectangle on the 640x480 screen */
    Filter filter;
    char overlay[256];    /* absolute PNG path, "" for none */
} SystemCfg;

#define CONFIG_FILE "/storage/.config/rgarch/systems.cfg"
#define USER_CONFIG_FILE "/storage/.config/rgarch/user.cfg"   /* the player's own choices */
#define OVERLAY_DIR "/storage/.config/retroarch/overlays/"
/* frames to choose per system: "<label> = <png under OVERLAY_DIR> <x> <y> <w> <h>" (the
 * picture's rectangle in the frame's window); user.cfg "frame = <label>|none" picks one */
#define FRAMES_FILE "/storage/.config/rgarch/frames.cfg"

/* Fills *out from [default] then [system]. 0 ok (also when the system has no
 * section), -1 when the file cannot be read. */
int config_load(const char *file, const char *system, SystemCfg *out);

/* Applies only the [system] section of file over *out (user.cfg). "overlay = none" = no frame.
 * -1 when the file cannot be read. */
int config_merge(const char *file, const char *system, SystemCfg *out);
/* value of key in [section] (no [default] fallback); 0 found, -1 not */
int config_get(const char *file, const char *section, const char *key, char *out, int n);
/* sets key in [section] (created at the end if missing); NULL removes it; atomic write */
int config_set(const char *file, const char *section, const char *key, const char *val);
/* calls fn(arg, key, value) for every line of [section], in file order; -1 if unreadable */
int config_each(const char *file, const char *section, void (*fn)(void *, const char *, const char *), void *arg);
/* parses a frames.cfg value "<png> <x> <y> <w> <h>"; 0 ok */
int config_frame_parse(const char *val, char *png, int n, int *x, int *y, int *w, int *h);
/* applies user.cfg's "frame" for system: none = no frame; a label from frames_file = its
 * PNG and picture rectangle (SCALE_CUSTOM); absent = keep systems.cfg's */
void config_frame(const char *frames_file, const char *user_file, const char *system, SystemCfg *c);
