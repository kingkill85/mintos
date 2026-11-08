/* Einstellungen: one list per page, values on the right, changed with left/right.
 *   Einstellungen: Helligkeit, Lautstärke, Bildschirm aus, Info ›, Neustart, Ausschalten
 *   Info: read-only lines */
#include "settings.h"
#include "../common/settings.h"
#include "gameopts.h"
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

typedef enum { I_BRIGHT, I_VOLUME, I_GAMEOPTS, I_SCREENOFF, I_INFO, I_REBOOT, I_POWEROFF, I_TEXT, I_SYSTEM, I_GAMEOPT } ItemId;
typedef enum { P_MAIN, P_INFO, P_SYSTEMS, P_GAME } Page;

typedef struct { ItemId id; char label[64]; char value[96]; int bar, arg; } Item;   /* bar: 0..10, -1 none */

static Page page;
static Item items[64];
static int nitems, sel, main_sel, sys_sel, confirm;
static const char *const *sys_dirs, *const *sys_names;
static int nsys, cur_sys;
static char title[128];

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
    Item *it = &items[nitems++];
    *it = (Item){.id = id, .bar = -1};
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
        add(I_GAMEOPTS, "Spiel-Optionen");
        it = add(I_SCREENOFF, "Bildschirm aus");
        snprintf(it->value, sizeof it->value, "%s", OFF_TXT[off_index()]);
        add(I_INFO, "Info");
        add(I_REBOOT, "Neustart");
        add(I_POWEROFF, "Ausschalten");
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

void settings_open(void) { page = P_MAIN; sel = 0; confirm = 0; status = NULL; build(); }

const char *settings_title(void) {
    switch (page) {
    case P_INFO: return "Einstellungen › Info";
    case P_SYSTEMS: return "Einstellungen › Spiel-Optionen";
    case P_GAME: snprintf(title, sizeof title, "Spiel-Optionen › %s", sys_names[cur_sys]); return title;
    default: return "Einstellungen";
    }
}
int settings_count(void) { return nitems; }
int settings_sel(void) { return sel; }
const char *settings_status(void) { return status; }

const char *settings_hint_a(void) {
    switch (items[sel].id) {
    case I_INFO: case I_GAMEOPTS: case I_SYSTEM: return "ÖFFNEN";
    case I_REBOOT: case I_POWEROFF: return "OK";
    default: return NULL;
    }
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
    default: break;
    }
}

int settings_key(int nav) {
    if (nav != NAV_A) { confirm = 0; status = NULL; }
    switch (nav) {
    case NAV_UP: sel = cycle(sel, -1, nitems); break;
    case NAV_DOWN: sel = cycle(sel, 1, nitems); break;
    case NAV_LEFT: case NAV_RIGHT: change(&items[sel], nav == NAV_LEFT ? -1 : 1); break;
    case NAV_B:
        if (page == P_MAIN) return 1;
        if (page == P_GAME) { page = P_SYSTEMS; sel = sys_sel; }
        else { page = P_MAIN; sel = main_sel; }
        break;
    case NAV_A:
        if (items[sel].id == I_INFO) { main_sel = sel; page = P_INFO; sel = 0; }
        else if (items[sel].id == I_GAMEOPTS) { main_sel = sel; page = P_SYSTEMS; sel = 0; }
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
    }
    build();
    return 0;
}

/* label left, value (or a 10-step bar) right; the selection is a full-width white pill */
void settings_draw(Ui *ui, int y0, int rows) {
    static int top;
    if (sel < top) top = sel;
    if (sel >= top + rows) top = sel - rows + 1;
    if (top > nitems - rows) top = nitems - rows > 0 ? nitems - rows : 0;
    for (int r = 0; r < rows && top + r < nitems; r++) {
        Item *it = &items[top + r];
        int y = y0 + r * PILL, cy = y + PILL / 2, on = top + r == sel;
        SDL_Color fg = on ? UI_BLACK : UI_WHITE, dim = on ? UI_DARK : UI_GRAY;
        if (on) ui_pill(ui, PAD, y, ui->w - 2 * PAD, PILL, UI_WHITE);
        int right = ui->w - PAD - 24;
        if (it->bar >= 0) {
            int bw = 14, gap = 5, x = right - 10 * bw - 9 * gap;
            for (int i = 0; i < 10; i++)
                ui_pill(ui, x + i * (bw + gap), cy - 9, bw, 18, i < it->bar ? fg : (on ? UI_LIGHT : UI_DARK));
            right = x - 16;
        } else if (it->value[0]) {
            int maxw = (ui->w - 2 * PAD) / 2 + 40;
            char v[110];
            snprintf(v, sizeof v, on && it->id != I_TEXT ? "< %s >" : "%s", it->value);
            int vw = ui_text_w(ui->small, v);
            if (vw > maxw) vw = maxw;
            ui_text(ui, ui->small, v, right - vw, cy, it->id == I_TEXT ? dim : fg, maxw);
            right -= vw + 16;
        } else if (it->id == I_INFO || it->id == I_GAMEOPTS || it->id == I_SYSTEM) {
            ui_text(ui, ui->big, "›", right - ui_text_w(ui->big, "›"), cy, dim, 40);
        }
        ui_text(ui, ui->big, it->label, PAD + 24, cy, fg, right - PAD - 24);
    }
}
