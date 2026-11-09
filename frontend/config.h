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
    int crt;              /* systems.cfg crt = yes: TV/arcade, Scanlines allowed */
    int border;           /* drawn frame around the picture: 0 none, else 1 + index in config_border() */
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
/* Drawn frames: rgarch paints them around whatever picture the system has, so they never
 * change its size. frames.cfg uses them as "<label> = @<style>"; every system also gets
 * the two plain ones (CONFIG_PLAIN) at the end of its Rahmen list. */
typedef struct { const char *name; unsigned base, edge, accent; int dots; } BorderStyle;   /* 0xRRGGBB, accent 0 = none */
extern const char *const CONFIG_PLAIN[2];
const BorderStyle *config_border(int border);   /* SystemCfg.border -> style, NULL for 0/unknown */
int config_border_style(const char *label);     /* a CONFIG_PLAIN label -> border, else 0 */
/* after systems.cfg, user.cfg and the frame: Scanlines only on crt systems; a picture whose
 * position belonged to a frame that is off gets centred */
void config_finish(SystemCfg *c, int sw, int sh);
void config_frame(const char *frames_file, const char *user_file, const char *system, SystemCfg *c);
