/* The logic of mintos-wifi without D-Bus, tested on the host by frontend/test/test_wifi.c. */
#pragma once
#include <stddef.h>

/* iwd's GetManagedObjects flattened: one row per property, plus one with key "" per interface */
typedef struct { char path[160], iface[48], key[32], val[200]; } Prop;

typedef struct { char path[160], name[128], type[16]; int known, connected, signal; } WNet;
typedef struct {
    int has_device, powered;
    char device[160], state[32], ssid[128];
    WNet nets[64];
    int nnet;
    struct { char name[128], path[160]; } known[64];
    int nknown;
} WState;

void wifi_parse(const Prop *props, int n, WState *st);
/* status while iwd is not running: switched off in the settings ("Aus"), or still coming up */
void wifi_offline(int switched_off, WState *st);
void wifi_status_text(const WState *st, char *out, size_t n);
/* signal (100 * dBm) -> 0..4 bars */
int wifi_bars(int signal);
/* connected first, then known, then by signal */
void wifi_sort(WNet *nets, int n);
const WNet *wifi_find(const WState *st, const char *name);
const char *wifi_known_path(const WState *st, const char *name);
/* iwd's file name: plain SSIDs as is, others "=" + hex */
void iwd_file(const char *name, const char *type, char *out, size_t n);
/* writes dir/<name>.psk (mode 600) for iwd; path into out. 0 on success */
int write_psk(const char *dir, const char *name, const char *passphrase, char *out, size_t n);
