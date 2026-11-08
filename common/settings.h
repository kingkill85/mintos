/* MintOS settings helpers shared by rglauncher and rgarch (no SDL). */
#pragma once
#include <stddef.h>

#define SETTINGS_FILE "/storage/.config/mintos/settings.cfg"
#define BACKLIGHT "/sys/class/backlight/backlight"

/* brightness in 10 steps; step 1 is the dimmest that still shows a picture */
int bl_raw(int step, int max);
int bl_step(int raw, int max);
int bl_get(void);                 /* current step, -1 without a backlight */
void bl_set(int step);            /* systemd-backlight keeps it across reboots */
void bl_power(int on);            /* panel on/off (screen-off timer) */

int vol_get(void);                /* Master in %, -1 on error */
void vol_set(int pct);

/* "key = value" lines; kv_set with NULL/"" removes the key; writes are atomic */
int kv_get(const char *file, const char *key, char *out, size_t n);
int kv_get_int(const char *file, const char *key, int def);
int kv_set(const char *file, const char *key, const char *val);

int cycle(int i, int d, int n);   /* (i + d) mod n */
