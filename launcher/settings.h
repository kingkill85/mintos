/* The "Einstellungen" pages of rglauncher. */
#pragma once
#include "ui.h"

enum { NAV_UP, NAV_DOWN, NAV_LEFT, NAV_RIGHT, NAV_A, NAV_B };

/* systems with games (folder, display name) for "Spiel-Optionen"; strings must stay valid */
void settings_set_systems(const char *const *dirs, const char *const *names, int n);
void settings_open(void);
/* 1 when B left the settings */
int settings_key(int nav);
const char *settings_title(void);
int settings_count(void);
int settings_sel(void);
const char *settings_status(void);   /* "Nochmal A ..." or NULL */
const char *settings_hint_a(void);   /* A hint for the selected item, NULL = none */
void settings_draw(Ui *ui, int y0, int rows);
