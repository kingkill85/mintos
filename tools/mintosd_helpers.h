/* Pure helpers of mintosd (parsing, steps), tested on the host by frontend/test/test_mintosd.c. */
#pragma once
#include <stddef.h>

/* the playback PCM (we are the A2DP source) from `bluealsa-cli list-pcms`; 0 if none.
 * Copies it to out (size n). */
int bt_sink(const char *pcms, char *out, size_t n);
/* "AA:BB:CC:DD:EE:FF" from a bluealsa path with dev_AA_BB_..; "" if none */
void bt_mac(const char *path, char out[18]);
/* the ALSA config that makes the headphones the default device */
void bt_conf(const char *mac, char *out, size_t n);
/* "Volume: L: 100 R: 90" -> 100; -1 if missing */
int bt_volume_parse(const char *out);
/* bluealsa volume 0..127 in ~5 % steps */
int bt_step(int v, int d);
/* backlight as common/settings.c: 10 steps, 1 is the dimmest */
int bl_step(int raw, int mx);
int bl_raw(int step, int mx);
/* first "[NN%]" of a Playback line from amixer; -1 if missing */
int master_pct(const char *out);
/* "<seq> <kind> <0..10>\n" for /run/mintos-osd */
void osd_line(long long seq, const char *kind, int value, char *out, size_t n);
