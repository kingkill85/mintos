/* Einstellungen: one list per page, values on the right, changed with left/right.
 *   Einstellungen: Helligkeit, Lautstärke, WLAN ›, Spiel-Optionen ›, Bildschirm aus, Info ›,
 *                  Neustart, Ausschalten
 *   WLAN: on/off, networks (A connects, X forgets; new networks ask for the password)
 *   Bluetooth: on/off, devices (A pairs+connects / disconnects, X forgets)
 *   Spiel-Optionen: systems › Bild/Filter/Rahmen; Info: read-only lines
 * Slow commands (mintos-wifi) run as background jobs; settings_poll() picks up the result. */
#include "settings.h"
#include "../common/settings.h"
#include "gameopts.h"
#include "job.h"
#include "keyboard.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifndef MINTOS_VERSION
#define MINTOS_VERSION "dev"
#endif

typedef enum { I_BRIGHT, I_VOLUME, I_WIFI, I_GAMEOPTS, I_SCREENOFF, I_INFO, I_REBOOT, I_POWEROFF, I_TEXT, I_SYSTEM,
               I_GAMEOPT, I_WIFI_POWER, I_NET, I_BT, I_BT_POWER, I_DEV } ItemId;
typedef enum { P_MAIN, P_INFO, P_SYSTEMS, P_GAME, P_WIFI, P_BT } Page;
enum { J_STATUS = 1, J_LIST, J_SCAN, J_CONNECT, J_FORGET, J_POWER,
       B_STATUS, B_LIST, B_SCAN, B_CONNECT, B_DISCONNECT, B_FORGET, B_POWER };

typedef struct { ItemId id; char label[64]; char value[96]; int bar, arg, sig; } Item;   /* bar 0..10, sig 0..4, -1 none */

static Page page;
static Item items[64];
static int nitems, sel, main_sel, sys_sel, confirm;
static const char *const *sys_dirs, *const *sys_names;
static int nsys, cur_sys;
static char title[128], msg[128];

/* Wi-Fi as last reported by mintos-wifi */
typedef struct { char name[64], type[8]; int bars, known, connected; } Net;
static Net nets[32];
static int nnets, wifi_powered = -1, scanned, kb_on, net_sel;
static char wifi_ssid[64], wifi_state[24];
static Kb kb;

/* Bluetooth as last reported by mintos-bt */
typedef struct { char mac[18], name[64], kind[8]; int paired, connected; } Dev;
static Dev devs[32];
static int ndevs, bt_powered = -1, bt_scanned;

/* background work: one queued user action (runs first), then whatever the open page needs */
static struct { int kind, has; char cmd[400], input[KB_MAX + 2]; } pend;
static int need_ws, need_wl, need_wscan, need_bs, need_bl, need_bscan;

void settings_set_systems(const char *const *dirs, const char *const *names, int n) {
    sys_dirs = dirs; sys_names = names; nsys = n;
}

/* runs in rgarch (has a section with a core in systems.cfg): only those use the options */
static int in_rgarch(const char *dir) {
    char v[64];
    return !config_get(CONFIG_FILE, dir, "core", v, sizeof v) && *v;
}
static const char *status;

static const int OFF_MIN[] = {0, 1, 3, 5};
static const char *OFF_TXT[] = {"Nie", "1 Min.", "3 Min.", "5 Min."};
#define NOFF 4

static int off_index(void) {
    int m = kv_get_int(SETTINGS_FILE, "screen_off", 3);
    for (int i = 0; i < NOFF; i++) if (OFF_MIN[i] == m) return i;
    return 2;
}

static Item *add(ItemId id, const char *label) {
    Item *it = &items[nitems < 63 ? nitems++ : nitems];
    *it = (Item){.id = id, .bar = -1, .sig = -1};
    snprintf(it->label, sizeof it->label, "%s", label);
    return it;
}

static void battery(char *out, size_t n) {
    FILE *f = fopen("/sys/class/power_supply/rk817-battery/capacity", "r");
    int pct = -1;
    if (f) { if (fscanf(f, "%d", &pct) != 1) pct = -1; fclose(f); }
    char st[32] = "";
    f = fopen("/sys/class/power_supply/rk817-battery/status", "r");
    if (f) { if (!fgets(st, sizeof st, f)) st[0] = 0; fclose(f); }
    if (pct < 0) snprintf(out, n, "?");
    else snprintf(out, n, "%d %%%s", pct, !strncmp(st, "Charging", 8) ? " (lädt)" : "");
}

static void ip(char *out, size_t n) {
    struct ifaddrs *ifa, *a;
    snprintf(out, n, "keine");
    if (getifaddrs(&ifa)) return;
    for (a = ifa; a; a = a->ifa_next)
        if (a->ifa_addr && a->ifa_addr->sa_family == AF_INET && !(a->ifa_flags & IFF_LOOPBACK)) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)a->ifa_addr)->sin_addr, out, n);
            break;
        }
    freeifaddrs(ifa);
}

static void build(void) {
    nitems = 0;
    if (page == P_MAIN) {
        Item *it = add(I_BRIGHT, "Helligkeit");
        it->bar = bl_get();
        it = add(I_VOLUME, "Lautstärke");
        int v = vol_get();
        it->bar = v < 0 ? -1 : (v + 5) / 10;
        it = add(I_WIFI, "WLAN");
        snprintf(it->value, sizeof it->value, "%s", wifi_powered == 0 ? "Aus" : *wifi_ssid ? wifi_ssid
                 : wifi_powered < 0 ? "" : "Nicht verbunden");
        it = add(I_BT, "Bluetooth");
        const char *bc = NULL;
        for (int i = 0; i < ndevs; i++) if (devs[i].connected) { bc = devs[i].name; break; }
        snprintf(it->value, sizeof it->value, "%s", bt_powered == 0 ? "Aus" : bc ? bc : bt_powered < 0 ? "" : "An");
        add(I_GAMEOPTS, "Spiel-Optionen");
        it = add(I_SCREENOFF, "Bildschirm aus");
        snprintf(it->value, sizeof it->value, "%s", OFF_TXT[off_index()]);
        add(I_INFO, "Info");
        add(I_REBOOT, "Neustart");
        add(I_POWEROFF, "Ausschalten");
    } else if (page == P_WIFI) {
        Item *it = add(I_WIFI_POWER, "WLAN");
        snprintf(it->value, sizeof it->value, "%s", wifi_powered ? "An" : "Aus");
        if (wifi_powered) {
            for (int i = 0; i < nnets; i++) {
                it = add(I_NET, nets[i].name);
                it->arg = i;
                it->sig = nets[i].bars;
                snprintf(it->value, sizeof it->value, "%s", nets[i].connected ? "Verbunden" : nets[i].known ? "Gespeichert" : "");
            }
            if (!scanned) add(I_TEXT, "Suche Netze ...");
            else if (!nnets) add(I_TEXT, "Keine Netze gefunden");
        }
    } else if (page == P_BT) {
        Item *it = add(I_BT_POWER, "Bluetooth");
        snprintf(it->value, sizeof it->value, "%s", bt_powered ? "An" : "Aus");
        if (bt_powered) {
            for (int i = 0; i < ndevs; i++) {
                it = add(I_DEV, devs[i].name);
                it->arg = i;
                snprintf(it->value, sizeof it->value, "%s", devs[i].connected ? "Verbunden" : devs[i].paired ? "Gekoppelt"
                         : !strcmp(devs[i].kind, "audio") ? "Audio" : !strcmp(devs[i].kind, "input") ? "Controller" : "");
            }
            add(I_TEXT, bt_scanned ? "Neu suchen" : "Suche Geräte ...");
        }
    } else if (page == P_SYSTEMS) {
        for (int i = 0; i < nsys && nitems < 64; i++)
            if (in_rgarch(sys_dirs[i])) add(I_SYSTEM, sys_names[i])->arg = i;
    } else if (page == P_GAME) {
        const char *d = sys_dirs[cur_sys];
        for (int o = 0; o < GO_N; o++) {
            if (o == GO_FRAME && !gameopt_has_frame(CONFIG_FILE, d)) continue;
            Item *it = add(I_GAMEOPT, gameopt_label(o));
            it->arg = o;
            snprintf(it->value, sizeof it->value, "%s", gameopt_choice(o, gameopt_get(USER_CONFIG_FILE, d, o)));
        }
    } else {
        Item *it = add(I_TEXT, "System");
        snprintf(it->value, sizeof it->value, "MintOS %s", MINTOS_VERSION);
        it = add(I_TEXT, "Akku");
        battery(it->value, sizeof it->value);
        struct statvfs vs;
        it = add(I_TEXT, "Speicher frei");
        if (!statvfs("/storage", &vs))
            snprintf(it->value, sizeof it->value, "%.1f von %.1f GB",
                     (double)vs.f_bavail * vs.f_frsize / 1e9, (double)vs.f_blocks * vs.f_frsize / 1e9);
        it = add(I_TEXT, "IP-Adresse");
        ip(it->value, sizeof it->value);
        struct utsname u;
        it = add(I_TEXT, "Kernel");
        if (!uname(&u)) snprintf(it->value, sizeof it->value, "%s", u.release);
    }
    if (sel >= nitems) sel = nitems - 1;
    if (sel < 0) sel = 0;
}

static void kick(void);

/* a user action: queued, runs as soon as the current job (e.g. an 11 s scan) is done */
static void request(int kind, const char *tool, const char *args, const char *input) {
    pend.kind = kind; pend.has = 1;
    snprintf(pend.cmd, sizeof pend.cmd, "/usr/local/sbin/%s %s", tool, args);
    snprintf(pend.input, sizeof pend.input, "%s", input ? input : "");
    pend.has = input ? 2 : 1;
    kick();
}
static void wifi_job(int kind, const char *args, const char *input) { request(kind, "mintos-wifi", args, input); }
static void bt_job(int kind, const char *args) { request(kind, "mintos-bt", args, NULL); }

static void kick(void) {
    if (job_busy()) return;
    if (pend.has) {
        if (!job_start(pend.kind, pend.cmd, pend.has == 2 ? pend.input : NULL)) {
            memset(pend.input, 0, sizeof pend.input);
            pend.has = 0;
        }
        return;
    }
    const char *w = "/usr/local/sbin/mintos-wifi", *b = "/usr/local/sbin/mintos-bt";
    char cmd[100];
#define RUN(flag, kind, tool, args) if (flag) { snprintf(cmd, sizeof cmd, "%s %s", tool, args); \
        if (!job_start(kind, cmd, NULL)) flag = 0; return; }
    RUN(need_ws, J_STATUS, w, "status");
    RUN(need_bs, B_STATUS, b, "status");
    if (page == P_WIFI && wifi_powered > 0) {
        RUN(need_wl, J_LIST, w, "list --cached");
        RUN(need_wscan, J_SCAN, w, "list");
    }
    if ((page == P_BT || page == P_MAIN) && bt_powered > 0) RUN(need_bl, B_LIST, b, "list");
    if (page == P_BT && bt_powered > 0) RUN(need_bscan, B_SCAN, b, "list --scan");
#undef RUN
}

/* "mac<TAB>name<TAB>kind<TAB>paired<TAB>connected" lines */
static void parse_devs(const char *out) {
    ndevs = 0;
    char line[256];
    for (const char *p = out; *p && ndevs < 32;) {
        size_t n = strcspn(p, "\n");
        snprintf(line, sizeof line, "%.*s", (int)(n < sizeof line ? n : sizeof line - 1), p);
        p += n + (p[n] == '\n');
        char *f[5], *save = NULL, *t = line;
        int nf = 0;
        while (nf < 5 && (f[nf] = strtok_r(t, "\t", &save))) { nf++; t = NULL; }
        if (nf < 5) continue;
        Dev *d = &devs[ndevs++];
        snprintf(d->mac, sizeof d->mac, "%s", f[0]);
        snprintf(d->name, sizeof d->name, "%s", f[1]);
        snprintf(d->kind, sizeof d->kind, "%s", f[2]);
        d->paired = atoi(f[3]); d->connected = atoi(f[4]);
    }
}

/* "name<TAB>type<TAB>bars<TAB>known<TAB>connected" lines */
static void parse_nets(const char *out) {
    nnets = 0;
    char line[256];
    for (const char *p = out; *p && nnets < 32;) {
        size_t n = strcspn(p, "\n");
        snprintf(line, sizeof line, "%.*s", (int)(n < sizeof line ? n : sizeof line - 1), p);
        p += n + (p[n] == '\n');
        char *f[5], *save = NULL, *t = line;
        int nf = 0;
        while (nf < 5 && (f[nf] = strtok_r(t, "\t", &save))) { nf++; t = NULL; }
        if (nf < 5) continue;
        Net *e = &nets[nnets++];
        snprintf(e->name, sizeof e->name, "%s", f[0]);
        snprintf(e->type, sizeof e->type, "%s", f[1]);
        e->bars = atoi(f[2]); e->known = atoi(f[3]); e->connected = atoi(f[4]);
        if (e->connected) snprintf(wifi_ssid, sizeof wifi_ssid, "%s", e->name);
    }
}

static void parse_status(const char *out) {
    const char *p;
    wifi_powered = (p = strstr(out, "powered=")) ? atoi(p + 8) : 0;
    wifi_ssid[0] = wifi_state[0] = 0;
    if ((p = strstr(out, "ssid="))) sscanf(p + 5, "%63[^\n]", wifi_ssid);
    if ((p = strstr(out, "state="))) sscanf(p + 6, "%23[^\n]", wifi_state);
}

/* first line of a command's output as the message */
static void set_msg(const char *out, const char *fallback) {
    snprintf(msg, sizeof msg, "%.*s", (int)strcspn(out, "\n"), out);
    if (!*msg) snprintf(msg, sizeof msg, "%s", fallback);
    status = msg;
}

int settings_poll(void) {
    int kind, rc;
    const char *out;
    if (!job_take(&kind, &rc, &out)) return 0;
    switch (kind) {
    case J_STATUS: parse_status(out); break;
    case J_LIST: case J_SCAN:
        if (rc == 0) parse_nets(out);
        if (kind == J_SCAN) scanned = 1;
        break;
    case J_CONNECT:
        if (rc) set_msg(out, "Verbindung fehlgeschlagen");
        else { snprintf(msg, sizeof msg, "Verbunden"); status = msg; }
        need_ws = need_wl = 1;
        break;
    case J_FORGET: case J_POWER: need_ws = need_wl = 1; break;
    case B_STATUS: { const char *p = strstr(out, "powered="); bt_powered = p ? atoi(p + 8) : 0; break; }
    case B_LIST: case B_SCAN:
        if (rc == 0) parse_devs(out);
        if (kind == B_SCAN) bt_scanned = 1;
        break;
    case B_CONNECT:
        if (rc) set_msg(out, "Verbindung fehlgeschlagen");
        else { snprintf(msg, sizeof msg, "Verbunden"); status = msg; }
        need_bl = 1;
        break;
    case B_DISCONNECT: case B_FORGET: need_bl = 1; break;
    case B_POWER: need_bs = need_bl = 1; break;
    }
    kick();
    build();
    return 1;
}

void settings_open(void) {
    page = P_MAIN; sel = 0; confirm = 0; status = NULL; kb_on = 0;
    need_ws = need_bs = need_bl = 1;   /* main page shows the network and the connected device */
    kick();
    build();
}

const char *settings_title(void) {
    switch (page) {
    case P_INFO: return "Einstellungen › Info";
    case P_SYSTEMS: return "Einstellungen › Spiel-Optionen";
    case P_GAME: snprintf(title, sizeof title, "Spiel-Optionen › %s", sys_names[cur_sys]); return title;
    case P_WIFI:
        if (!kb_on) return "Einstellungen › WLAN";
        snprintf(title, sizeof title, "WLAN › %s", nets[net_sel].name);
        return title;
    case P_BT: return "Einstellungen › Bluetooth";
    default: return "Einstellungen";
    }
}
int settings_count(void) { return kb_on ? 0 : nitems; }
int settings_sel(void) { return sel; }
const char *settings_status(void) { return status; }

void settings_hints(Ui *ui) {
    if (kb_on) {
        const char *h[][2] = {{"A", "TIPPEN"}, {"B", "LÖSCHEN"}, {"START", "FERTIG"}};
        ui_hints(ui, 0, ui->w - PAD, h, 3);
        return;
    }
    const char *power[][2] = {{"POWER", "AUS"}};
    ui_hints(ui, PAD, 0, power, 1);
    Item *it = &items[sel];
    const char *a = NULL;
    switch (it->id) {
    case I_INFO: case I_GAMEOPTS: case I_SYSTEM: case I_WIFI: case I_BT: a = "ÖFFNEN"; break;
    case I_DEV: a = devs[it->arg].connected ? "TRENNEN" : "VERBINDEN"; break;
    case I_TEXT: a = page == P_BT && bt_scanned ? "SUCHEN" : NULL; break;
    case I_REBOOT: case I_POWEROFF: a = "OK"; break;
    case I_NET: a = nets[it->arg].connected ? NULL : "VERBINDEN"; break;
    default: break;
    }
    const char *h[3][2] = {{"B", "ZURÜCK"}};
    int n = 1;
    if ((it->id == I_NET && nets[it->arg].known) || (it->id == I_DEV && devs[it->arg].paired)) {
        h[n][0] = "X"; h[n][1] = "VERGESSEN"; n++;
    }
    if (a) { h[n][0] = "A"; h[n][1] = a; n++; }
    ui_hints(ui, 0, ui->w - PAD, h, n);
}

static void connect_net(int i, const char *pw) {
    char q[200], args[260];
    sh_quote(q, sizeof q, nets[i].name);
    snprintf(args, sizeof args, "connect %s", q);
    snprintf(msg, sizeof msg, "Verbinde mit %s ...", nets[i].name);
    status = msg;
    wifi_job(J_CONNECT, args, pw);
}

static void change(Item *it, int d) {
    switch (it->id) {
    case I_BRIGHT: if (it->bar > 0) bl_set(it->bar + d); break;
    case I_VOLUME: { int v = vol_get(); if (v >= 0) vol_set((v + 5) / 10 * 10 + 10 * d); break; }
    case I_SCREENOFF: {
        char m[8];
        snprintf(m, sizeof m, "%d", OFF_MIN[cycle(off_index(), d, NOFF)]);
        kv_set(SETTINGS_FILE, "screen_off", m);
        break;
    }
    case I_GAMEOPT: {
        const char *dir = sys_dirs[cur_sys];
        int n = gameopt_count(it->arg);
        gameopt_set(USER_CONFIG_FILE, dir, it->arg, cycle(gameopt_get(USER_CONFIG_FILE, dir, it->arg), d, n));
        break;
    }
    case I_BT_POWER:
        kv_set(SETTINGS_FILE, "bt", bt_powered ? "off" : NULL);
        bt_job(B_POWER, bt_powered ? "power off" : "power on");
        bt_powered = !bt_powered;
        ndevs = 0; bt_scanned = 0; need_bscan = bt_powered;
        break;
    case I_WIFI_POWER:
        kv_set(SETTINGS_FILE, "wifi", wifi_powered ? "off" : NULL);
        wifi_job(J_POWER, wifi_powered ? "power off" : "power on", NULL);
        wifi_powered = !wifi_powered;
        nnets = 0; scanned = 0; need_wscan = wifi_powered;
        break;
    default: break;
    }
}

static int kb_nav(int nav) {
    switch (nav) {
    case NAV_UP: return KB_UP; case NAV_DOWN: return KB_DOWN; case NAV_LEFT: return KB_LEFT;
    case NAV_RIGHT: return KB_RIGHT; case NAV_A: return KB_A; case NAV_B: return KB_B;
    case NAV_X: return KB_X; case NAV_Y: return KB_Y; case NAV_START: return KB_START;
    case NAV_SELECT: return KB_SELECT;
    }
    return -1;
}

int settings_key(int nav) {
    if (kb_on) {
        int r = kb_key(&kb, kb_nav(nav));
        if (r == KB_CANCEL) kb_on = 0;
        else if (r == KB_DONE) {
            if (strlen(kb.text) < 8) { snprintf(msg, sizeof msg, "Mindestens 8 Zeichen"); status = msg; return 0; }
            kb_on = 0;
            connect_net(net_sel, kb.text);
            memset(kb.text, 0, sizeof kb.text);
        } else status = NULL;
        build();
        return 0;
    }
    int was_confirm = confirm;
    if (nav != NAV_A && nav != NAV_X) { confirm = 0; status = NULL; }
    switch (nav) {
    case NAV_UP: sel = cycle(sel, -1, nitems); break;
    case NAV_DOWN: sel = cycle(sel, 1, nitems); break;
    case NAV_LEFT: case NAV_RIGHT: change(&items[sel], nav == NAV_LEFT ? -1 : 1); break;
    case NAV_B:
        if (page == P_MAIN) return 1;
        if (page == P_GAME) { page = P_SYSTEMS; sel = sys_sel; }
        else if (page == P_WIFI || page == P_BT) { page = P_MAIN; sel = main_sel; need_ws = need_bs = 1; }
        else { page = P_MAIN; sel = main_sel; }
        break;
    case NAV_A:
        if (items[sel].id == I_INFO) { main_sel = sel; page = P_INFO; sel = 0; }
        else if (items[sel].id == I_GAMEOPTS) { main_sel = sel; page = P_SYSTEMS; sel = 0; }
        else if (items[sel].id == I_WIFI) {
            main_sel = sel; page = P_WIFI; sel = 0; scanned = 0;
            need_ws = need_wl = need_wscan = 1;
        } else if (items[sel].id == I_BT) {
            main_sel = sel; page = P_BT; sel = 0; bt_scanned = 0;
            need_bs = need_bl = need_bscan = 1;
        } else if (items[sel].id == I_WIFI_POWER || items[sel].id == I_BT_POWER) change(&items[sel], 1);
        else if (items[sel].id == I_DEV) {
            Dev *d = &devs[items[sel].arg];
            char args[64];
            snprintf(args, sizeof args, "%s %s", d->connected ? "disconnect" : "connect", d->mac);
            snprintf(msg, sizeof msg, d->connected ? "Trenne %s ..." : "Verbinde mit %s ...", d->name);
            status = msg;
            bt_job(d->connected ? B_DISCONNECT : B_CONNECT, args);
        } else if (items[sel].id == I_TEXT && page == P_BT) {
            bt_scanned = 0; need_bscan = 1;   /* search again */
        } else if (items[sel].id == I_NET) {
            Net *n = &nets[items[sel].arg];
            if (n->connected) break;
            if (!n->known && !strcmp(n->type, "psk")) { net_sel = items[sel].arg; kb_init(&kb); kb_on = 1; status = NULL; }
            else if (!n->known && strcmp(n->type, "open")) { snprintf(msg, sizeof msg, "Dieses Netz wird nicht unterstützt"); status = msg; }
            else connect_net(items[sel].arg, NULL);
        }
        else if (items[sel].id == I_SYSTEM) { sys_sel = sel; cur_sys = items[sel].arg; page = P_GAME; sel = 0; }
        else if (items[sel].id == I_REBOOT || items[sel].id == I_POWEROFF) {
            int off = items[sel].id == I_POWEROFF;
            if (confirm) {
                status = off ? "Wird ausgeschaltet ..." : "Startet neu ...";
                if (system(off ? "systemctl poweroff" : "systemctl reboot")) {}
            } else {
                confirm = 1;
                status = off ? "Nochmal A zum Ausschalten" : "Nochmal A zum Neustarten";
            }
        }
        break;
    case NAV_X:
        if (items[sel].id == I_DEV && devs[items[sel].arg].paired) {
            if (!was_confirm) { confirm = 1; snprintf(msg, sizeof msg, "Nochmal X zum Vergessen"); status = msg; break; }
            confirm = 0;
            char args[64];
            snprintf(args, sizeof args, "forget %s", devs[items[sel].arg].mac);
            bt_job(B_FORGET, args);
            snprintf(msg, sizeof msg, "Vergessen"); status = msg;
        }
        if (items[sel].id == I_NET && nets[items[sel].arg].known) {
            if (!was_confirm) { confirm = 1; snprintf(msg, sizeof msg, "Nochmal X zum Vergessen"); status = msg; break; }
            confirm = 0;
            char q[200], args[260];
            sh_quote(q, sizeof q, nets[items[sel].arg].name);
            snprintf(args, sizeof args, "forget %s", q);
            wifi_job(J_FORGET, args, NULL);
            snprintf(msg, sizeof msg, "Vergessen"); status = msg;
        }
        break;
    }
    kick();
    build();
    return 0;
}

/* label left, value (or a 10-step bar) right; the selection is a full-width white pill */
void settings_draw(Ui *ui, int y0, int rows) {
    static int top;
    if (kb_on) { kb_draw(ui, &kb, "Passwort", y0 - 8); return; }
    if (sel < top) top = sel;
    if (sel >= top + rows) top = sel - rows + 1;
    if (top > nitems - rows) top = nitems - rows > 0 ? nitems - rows : 0;
    for (int r = 0; r < rows && top + r < nitems; r++) {
        Item *it = &items[top + r];
        int y = y0 + r * PILL, cy = y + PILL / 2, on = top + r == sel;
        SDL_Color fg = on ? UI_BLACK : UI_WHITE, dim = on ? UI_DARK : UI_GRAY;
        if (on) ui_pill(ui, PAD, y, ui->w - 2 * PAD, PILL, UI_WHITE);
        int right = ui->w - PAD - 24;
        if (it->sig >= 0) {   /* signal: four rising bars, at the far right */
            int bw = 6, gap = 3, x = right - 4 * bw - 3 * gap;
            for (int i = 0; i < 4; i++) {
                int h = 6 + i * 5;
                SDL_Color c = i < it->sig ? fg : (on ? UI_LIGHT : UI_DARK);
                ui_set(ui, c);
                SDL_Rect r = {x + i * (bw + gap), cy + 10 - h, bw, h};
                SDL_RenderFillRect(ui->ren, &r);
            }
            right = x - 16;
        }
        if (it->bar >= 0) {
            int bw = 14, gap = 5, x = right - 10 * bw - 9 * gap;
            for (int i = 0; i < 10; i++)
                ui_pill(ui, x + i * (bw + gap), cy - 9, bw, 18, i < it->bar ? fg : (on ? UI_LIGHT : UI_DARK));
            right = x - 16;
        } else if (it->value[0]) {
            int maxw = (ui->w - 2 * PAD) / 2 + 40;
            char v[110];
            int lr = it->id == I_SCREENOFF || it->id == I_GAMEOPT || it->id == I_WIFI_POWER || it->id == I_BT_POWER;   /* left/right changes it */
            snprintf(v, sizeof v, on && lr ? "< %s >" : "%s", it->value);
            int vw = ui_text_w(ui->small, v);
            if (vw > maxw) vw = maxw;
            ui_text(ui, ui->small, v, right - vw, cy, lr ? fg : dim, maxw);
            right -= vw + 16;
        } else if (it->id == I_INFO || it->id == I_GAMEOPTS || it->id == I_SYSTEM || it->id == I_WIFI || it->id == I_BT) {
            ui_text(ui, ui->big, "›", right - ui_text_w(ui->big, "›"), cy, dim, 40);
        }
        ui_text(ui, ui->big, it->label, PAD + 24, cy, fg, right - PAD - 24);
    }
}
