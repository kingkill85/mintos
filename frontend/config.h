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
#define USER_CONFIG_FILE "/storage/.config/rgarch/user.cfg"   /* the player's own choices */
#define OVERLAY_DIR "/storage/.config/retroarch/overlays/"

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
