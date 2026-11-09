/* On-screen display for volume and brightness changes, the same in rglauncher and rgarch.
 * mintosd writes OSD_FILE ("<seq> vol|bright <0..10>") on every change by the volume
 * keys (F + volume = brightness); the programs that own the display poll it and draw a bar. */
#pragma once
#include "ui.h"

#define OSD_FILE "/run/mintos-osd"
#define OSD_MS 1500

int  osd_poll(void);        /* 1 when the overlay appeared, changed or went away: redraw */
void osd_draw(Ui *ui);      /* draws the overlay while it is visible */

/* a line of text (achievements) shown for OSD_MS * 2 by osd_poll / osd_draw; a new one replaces it */
void osd_toast(const char *text);
const char *osd_toast_text(void);   /* the visible toast, "" when none */

/* F opens a menu on release, unless F + volume changed the brightness meanwhile:
 * osd_mark() on F down, osd_used_since_mark() on F up */
void osd_mark(void);
int  osd_used_since_mark(void);

/* parse "<seq> <kind> <value>"; 0 on success (kind: 'v' volume, 'b' brightness) */
int  osd_parse(const char *s, long long *seq, char *kind, int *value);
