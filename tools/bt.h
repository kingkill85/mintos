/* The parsing of mintos-bt (bluetoothctl output), tested on the host by frontend/test/test_bt.c. */
#pragma once
#include <stddef.h>

typedef struct { char mac[18], name[128]; } BtDev;
/* "Device <mac> <name>" lines (not "(random)" headers) -> devs; returns the count */
int bt_parse_devices(const char *out, BtDev *devs, int max);
int bt_in(const BtDev *devs, int n, const char *mac);
/* a real name, not just the MAC (bluetoothctl shows AA-BB-.. for nameless devices) */
int bt_has_name(const char *mac, const char *name);
/* the first "\t<Key>: <value>" of `bluetoothctl info` into val; UUID: all of them, space-separated */
int bt_info(const char *out, const char *key, char *val, size_t n);
/* "audio", "input" or "other" from the info output */
const char *bt_kind(const char *info);
/* did pair / connect work */
int bt_ok(const char *out, const char *what);
int bt_is_mac(const char *s);
void bt_status_line(int running, const char *show, char *out, size_t n);
