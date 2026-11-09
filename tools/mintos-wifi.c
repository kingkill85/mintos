/* Wi-Fi for the MintOS settings, over iwd's D-Bus API (sd-bus).
 *   mintos-wifi status            device=0|1 (0: chip not there yet), powered=0|1, state=..., ssid=...
 *   mintos-wifi list [--cached]   scan (unless --cached), then "name<TAB>type<TAB>bars 0-4<TAB>known<TAB>connected" per network
 *   mintos-wifi connect <ssid>    passphrase (if needed) on stdin; exit 0 when connected
 *   mintos-wifi forget <ssid>
 *   mintos-wifi power on|off      off: stops iwd and unloads the driver (RAM, battery); on: back + wifi-check
 * A new network's passphrase goes into /var/lib/iwd/<name>.psk (iwd picks it up), so no agent is needed. */
#include "util.h"
#include "wifi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#define SVC "net.connman.iwd"
static const char SETTINGS[] = "/storage/.config/mintos/settings.cfg", IWD_DIR[] = "/var/lib/iwd";

static sd_bus *bus;
static char errmsg[256];
static Prop props[400];
static WState st;

static int fail(int r, const sd_bus_error *e) {
    if (e && sd_bus_error_is_set(e)) snprintf(errmsg, sizeof errmsg, "%s: %s", e->name, e->message ? e->message : "");
    else snprintf(errmsg, sizeof errmsg, "%s", strerror(r < 0 ? -r : r));
    return -1;
}

static int call(const char *path, const char *iface, const char *method, int timeout_s, sd_bus_message **reply) {
    sd_bus_message *m = NULL;
    sd_bus_error e = SD_BUS_ERROR_NULL;
    int r = sd_bus_message_new_method_call(bus, &m, SVC, path, iface, method);
    if (r >= 0) r = sd_bus_call(bus, m, (uint64_t)timeout_s * 1000000, &e, reply);
    sd_bus_message_unref(m);
    if (r < 0) { fail(r, &e); sd_bus_error_free(&e); return -1; }
    sd_bus_error_free(&e);
    return 0;
}

/* one variant as text: basic types only (others are left out) */
static int variant(sd_bus_message *m, char *val, size_t n) {
    char type;
    const char *sig;
    int r = sd_bus_message_peek_type(m, &type, &sig);
    if (r < 0) return r;
    if ((r = sd_bus_message_enter_container(m, 'v', sig)) < 0) return r;
    union { const char *s; int b; uint8_t y; int16_t n; uint16_t q; int32_t i; uint32_t u; int64_t x; uint64_t t; double d; } v;
    int basic = 1;
    switch (sig[0] && !sig[1] ? sig[0] : 0) {
    case 's': case 'o': case 'g': r = sd_bus_message_read_basic(m, sig[0], &v.s); if (r >= 0) snprintf(val, n, "%s", v.s); break;
    case 'b': r = sd_bus_message_read_basic(m, 'b', &v.b); if (r >= 0) snprintf(val, n, "%d", v.b != 0); break;
    case 'y': r = sd_bus_message_read_basic(m, 'y', &v.y); if (r >= 0) snprintf(val, n, "%u", v.y); break;
    case 'n': r = sd_bus_message_read_basic(m, 'n', &v.n); if (r >= 0) snprintf(val, n, "%d", v.n); break;
    case 'q': r = sd_bus_message_read_basic(m, 'q', &v.q); if (r >= 0) snprintf(val, n, "%u", v.q); break;
    case 'i': r = sd_bus_message_read_basic(m, 'i', &v.i); if (r >= 0) snprintf(val, n, "%d", v.i); break;
    case 'u': r = sd_bus_message_read_basic(m, 'u', &v.u); if (r >= 0) snprintf(val, n, "%u", v.u); break;
    case 'x': r = sd_bus_message_read_basic(m, 'x', &v.x); if (r >= 0) snprintf(val, n, "%lld", (long long)v.x); break;
    case 't': r = sd_bus_message_read_basic(m, 't', &v.t); if (r >= 0) snprintf(val, n, "%llu", (unsigned long long)v.t); break;
    default: basic = 0; r = sd_bus_message_skip(m, sig);
    }
    if (r < 0) return r;
    if ((r = sd_bus_message_exit_container(m)) < 0) return r;
    return basic;
}

/* GetManagedObjects (a{oa{sa{sv}}}) flattened into props; the state parsed from it */
static int state(void) {
    sd_bus_message *m = NULL;
    if (call("/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", 10, &m)) return -1;
    int n = 0, r;
    const char *path, *iface, *key;
#define TRY(x) if ((r = (x)) < 0) goto bad
    TRY(sd_bus_message_enter_container(m, 'a', "{oa{sa{sv}}}"));
    while ((r = sd_bus_message_enter_container(m, 'e', "oa{sa{sv}}")) > 0) {
        TRY(sd_bus_message_read_basic(m, 'o', &path));
        TRY(sd_bus_message_enter_container(m, 'a', "{sa{sv}}"));
        while ((r = sd_bus_message_enter_container(m, 'e', "sa{sv}")) > 0) {
            TRY(sd_bus_message_read_basic(m, 's', &iface));
            if (n < 400) {
                snprintf(props[n].path, sizeof props[n].path, "%s", path);
                snprintf(props[n].iface, sizeof props[n].iface, "%s", iface);
                props[n].key[0] = props[n].val[0] = 0;
                n++;
            }
            TRY(sd_bus_message_enter_container(m, 'a', "{sv}"));
            while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
                TRY(sd_bus_message_read_basic(m, 's', &key));
                char val[200] = "";
                TRY(variant(m, val, sizeof val));
                if (r > 0 && n < 400) {
                    snprintf(props[n].path, sizeof props[n].path, "%s", path);
                    snprintf(props[n].iface, sizeof props[n].iface, "%s", iface);
                    snprintf(props[n].key, sizeof props[n].key, "%s", key);
                    snprintf(props[n].val, sizeof props[n].val, "%s", val);
                    n++;
                }
                TRY(sd_bus_message_exit_container(m));
            }
            TRY(r);
            TRY(sd_bus_message_exit_container(m));
            TRY(sd_bus_message_exit_container(m));
        }
        TRY(r);
        TRY(sd_bus_message_exit_container(m));
        TRY(sd_bus_message_exit_container(m));
    }
    TRY(r);
#undef TRY
    sd_bus_message_unref(m);
    wifi_parse(props, n, &st);
    return 0;
bad:
    sd_bus_message_unref(m);
    return fail(r, NULL);
}

static int scan(void) {
    sd_bus_message *m = NULL;
    if (!call(st.device, "net.connman.iwd.Station", "Scan", 10, &m)) sd_bus_message_unref(m);
    /* else: already scanning, or busy connecting */
    for (int i = 0; i < 40; i++) {
        sleep_ms(250);
        sd_bus_error e = SD_BUS_ERROR_NULL;
        int scanning = 0, r = sd_bus_get_property_trivial(bus, SVC, st.device, "net.connman.iwd.Station", "Scanning", &e, 'b', &scanning);
        if (r < 0) { fail(r, &e); sd_bus_error_free(&e); return -1; }
        if (!scanning) break;
    }
    return 0;
}

static int cmd_list(int cached) {
    if (state()) return -1;
    if (!st.has_device || !st.powered) return 1;
    if (!cached && (scan() || state())) return -1;
    sd_bus_message *m = NULL;
    if (call(st.device, "net.connman.iwd.Station", "GetOrderedNetworks", 10, &m)) return -1;
    const char *path;
    int16_t sig;
    int r = sd_bus_message_enter_container(m, 'a', "(on)");
    while (r >= 0 && (r = sd_bus_message_read(m, "(on)", &path, &sig)) > 0)
        for (int i = 0; i < st.nnet; i++)
            if (!strcmp(st.nets[i].path, path)) st.nets[i].signal = sig;
    sd_bus_message_unref(m);
    if (r < 0) return fail(r, NULL);
    wifi_sort(st.nets, st.nnet);
    for (int i = 0; i < st.nnet; i++) {
        WNet *w = &st.nets[i];
        for (char *c = w->name; *c; c++) if (*c == '\t') *c = ' ';
        printf("%s\t%s\t%d\t%d\t%d\n", w->name, w->type, wifi_bars(w->signal), w->known, w->connected);
    }
    return 0;
}

static int forget(const char *path) {
    sd_bus_message *m = NULL;
    if (!path || call(path, "net.connman.iwd.KnownNetwork", "Forget", 10, &m)) return -1;
    sd_bus_message_unref(m);
    return 0;
}

static int cmd_connect(const char *name) {
    if (state()) return -1;
    const WNet *w = wifi_find(&st, name);
    if (!w) { puts("Netz nicht gefunden"); return 1; }
    char npath[160], created[512] = "";
    snprintf(npath, sizeof npath, "%s", w->path);
    if (!strcmp(w->type, "psk") && !w->known) {
        char pw[256] = "";
        if (!fgets(pw, sizeof pw, stdin)) pw[0] = 0;
        pw[strcspn(pw, "\n")] = 0;
        if (strlen(pw) < 8) { puts("Passwort zu kurz"); return 1; }
        if (write_psk(IWD_DIR, name, pw, created, sizeof created)) { snprintf(errmsg, sizeof errmsg, "%s nicht schreibbar", created); return -1; }
        memset(pw, 0, sizeof pw);
        sleep_ms(1000);   /* iwd notices the new file */
    }
    sd_bus_message *m = NULL;
    if (!call(npath, "net.connman.iwd.Network", "Connect", 30, &m)) { sd_bus_message_unref(m); return 0; }
    /* iwd answers a wrong passphrase with net.connman.iwd.Failed (or Aborted) */
    int wrong = strstr(errmsg, ".Failed") || strstr(errmsg, ".Aborted");
    if (created[0]) {
        if (!state()) forget(wifi_known_path(&st, name));
        unlink(created);
    }
    puts(wrong ? "Falsches Passwort" : "Verbindung fehlgeschlagen");
    return 1;
}

static int cmd_power(int on) {
    if (on) {
        RUN_Q(30000, "modprobe", "rtw88_8821cs");
        RUN_Q(30000, "systemctl", "start", "iwd");
        RUN_Q(30000, "systemctl", "restart", "--no-block", "wifi-check");   /* reloads the driver if needed */
    } else {
        RUN_Q(30000, "systemctl", "stop", "wifi-check", "iwd");
        RUN_Q(30000, "modprobe", "-r", "rtw88_8821cs", "rtw88_8821c", "rtw88_sdio", "rtw88_core", "mac80211", "cfg80211");
    }
    return 0;
}

static int usage(void) {
    puts("mintos-wifi status | list [--cached] | connect <ssid> | forget <ssid> | power on|off");
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    const char *cmd = argv[1];
    if (!strcmp(cmd, "power") && argc == 3) return cmd_power(!strcmp(argv[2], "on"));
    int r = sd_bus_open_system(&bus);
    if (!strcmp(cmd, "status") && argc == 2) {
        char out[512];
        if (RUN_Q(10000, "systemctl", "is-active", "--quiet", "iwd") != 0) wifi_offline(setting_off(SETTINGS, "wifi"), &st);
        else if (r < 0 || state()) wifi_offline(0, &st);   /* iwd starting */
        wifi_status_text(&st, out, sizeof out);
        puts(out);
        return 0;
    }
    if (r < 0) { fail(r, NULL); printf("Fehler: %s\n", errmsg); return 1; }
    if (!strcmp(cmd, "list")) r = cmd_list(argc > 2 && !strcmp(argv[2], "--cached"));
    else if (!strcmp(cmd, "connect") && argc == 3) r = cmd_connect(argv[2]);
    else if (!strcmp(cmd, "forget") && argc == 3) {
        if (state()) r = -1;
        else { const char *p = wifi_known_path(&st, argv[2]); r = !p ? 1 : forget(p); }
    } else return usage();
    if (r < 0) { printf("Fehler: %s\n", errmsg); return 1; }
    return r;
}
