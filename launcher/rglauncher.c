/* rglauncher: a minimal, MinUI-style game launcher for ROCKNIX on the RG353V.
 *
 * Black screen, white text, the selection is a white pill. The root list shows
 * "Zuletzt gespielt" and every system that has games; A opens, B goes back.
 * Games start through ROCKNIX's runemu.sh (see device/launch.sh). */
#include <SDL.h>
#include <SDL_ttf.h>
#include "ui.h"
#include "settings.h"
#include "../common/settings.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define ROMS "/storage/roms"
#define CONF "/storage/.config/rglauncher"
#define FONT CONF "/fonts/BPreplayBold-unhinted.otf"   /* MinUI's font */
#define FONT2 CONF "/fonts/VarelaRound-Regular.ttf"
#define MAXE 2048
#define MAXRECENT 12

typedef struct { const char *dir, *name, *exts; } Sys;
static const Sys SYSTEMS[] = {
    {"gb", "Game Boy", ".gb.zip.7z"},
    {"gbc", "Game Boy Color", ".gbc.gb.zip.7z"},
    {"gba", "Game Boy Advance", ".gba.zip.7z"},
    {"nes", "NES", ".nes.zip.7z.fds"},
    {"snes", "Super Nintendo", ".sfc.smc.zip.7z"},
    {"mastersystem", "Master System", ".sms.zip.7z"},
    {"gamegear", "Game Gear", ".gg.zip.7z"},
    {"megadrive", "Mega Drive", ".md.bin.gen.smd.zip.7z"},
    {"genesis", "Genesis", ".md.bin.gen.smd.zip.7z"},
    {"megadrive-japan", "Mega Drive (Japan)", ".md.bin.gen.smd.zip.7z"},
    {"sg-1000", "SG-1000", ".sg.zip.7z"},
    {"segacd", "Sega CD", ".chd.cue.iso.m3u"},
    {"megacd", "Mega-CD", ".chd.cue.iso.m3u"},
    {"sega32x", "32X", ".32x.zip.7z"},
    {"famicom", "Famicom", ".nes.zip.7z"},
    {"fds", "Famicom Disk System", ".fds.zip.7z"},
    {"sfc", "Super Famicom", ".sfc.smc.zip.7z"},
    {"pcengine", "PC Engine", ".pce.zip.7z"},
    {"tg16", "TurboGrafx-16", ".pce.zip.7z"},
    {"pcenginecd", "PC Engine CD", ".chd.cue.m3u"},
    {"tg16cd", "TurboGrafx-CD", ".chd.cue.m3u"},
    {"ngp", "Neo Geo Pocket", ".ngp.zip.7z"},
    {"ngpc", "Neo Geo Pocket Color", ".ngc.ngp.zip.7z"},
    {"wonderswan", "WonderSwan", ".ws.zip.7z"},
    {"wonderswancolor", "WonderSwan Color", ".wsc.ws.zip.7z"},
    {"atari2600", "Atari 2600", ".a26.bin.zip.7z"},
    {"atari7800", "Atari 7800", ".a78.bin.zip.7z"},
    {"atarilynx", "Atari Lynx", ".lnx.zip.7z"},
    {"virtualboy", "Virtual Boy", ".vb.zip.7z"},
    {"arcade", "Arcade", ".zip.7z"},
    {"fbneo", "FinalBurn Neo", ".zip.7z"},
    {"neogeo", "Neo Geo", ".zip.7z"},
    {"cps1", "CPS-1", ".zip.7z"},
    {"cps2", "CPS-2", ".zip.7z"},
    {"cps3", "CPS-3", ".zip.7z"},
    {"psx", "PlayStation", ".cue.chd.pbp.m3u"},
    {"n64", "Nintendo 64", ".n64.z64.v64.zip.7z"},
    {"nds", "Nintendo DS", ".nds.zip.7z"},
    {"psp", "PSP", ".iso.cso.chd"},
    {"dreamcast", "Dreamcast", ".chd.cdi.gdi.m3u"},
    {"scummvm", "ScummVM", ".scummvm"},
};
#define NSYS (int)(sizeof SYSTEMS / sizeof *SYSTEMS)

/* ---------- entries ---------- */
enum { K_GAME, K_SYSTEM, K_RECENT };
typedef struct { int kind, sys; char label[128]; char path[512]; } Entry;

static Entry games[MAXE];   /* current game list */
static int ngames;
static Entry root[NSYS + 3];
static int nroot;

static int has_ext(const char *f, const char *exts) {
    const char *dot = strrchr(f, '.');
    if (!dot || dot == f) return 0;
    size_t len = strlen(dot);
    for (const char *p = exts; *p == '.';) {
        const char *next = strchr(p + 1, '.');
        size_t l = next ? (size_t)(next - p) : strlen(p);
        if (l == len && !strncasecmp(p, dot, l)) return 1;
        if (!next) break;
        p = next;
    }
    return 0;
}

static void pretty(char *out, size_t sz, const char *path) {
    const char *file = strrchr(path, '/');
    snprintf(out, sz, "%s", file ? file + 1 : path);
    char *dot = strrchr(out, '.');
    if (dot && dot != out) *dot = 0;
    char *tag = strstr(out, " (");  /* "Name (USA, Europe) (Rev 1)" -> "Name" */
    if (tag && tag != out) *tag = 0;
    for (char *p = out; *p; p++) if (*p == '_') *p = ' ';
}

static int cmp_label(const void *a, const void *b) {
    return strcasecmp(((const Entry *)a)->label, ((const Entry *)b)->label);
}

static int scan_dir(const char *dir, int sys, int depth, Entry *out, int n, int max) {
    DIR *d = opendir(dir);
    if (!d) return n;
    struct dirent *de;
    while ((de = readdir(d)) && n < max) {
        if (de->d_name[0] == '.') continue;
        char p[512];
        snprintf(p, sizeof p, "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(p, &st)) continue;
        if (S_ISDIR(st.st_mode)) {
            /* ScummVM game folders hold data plus the core's <id>.scummvm: list only the top */
            if (depth < 2 && strcmp(SYSTEMS[sys].dir, "scummvm") && strcasecmp(de->d_name, "images") && strcasecmp(de->d_name, "videos")
                && strcasecmp(de->d_name, "manuals"))
                n = scan_dir(p, sys, depth + 1, out, n, max);
        } else if (has_ext(de->d_name, SYSTEMS[sys].exts)) {
            Entry *e = &out[n++];
            e->kind = K_GAME; e->sys = sys;
            pretty(e->label, sizeof e->label, de->d_name);
            snprintf(e->path, sizeof e->path, "%s", p);
        }
    }
    closedir(d);
    return n;
}

static int count_games(int sys) {
    static Entry tmp[MAXE];
    char dir[256];
    snprintf(dir, sizeof dir, ROMS "/%s", SYSTEMS[sys].dir);
    return scan_dir(dir, sys, 0, tmp, 0, MAXE);
}

static void load_system(int sys) {
    char dir[256];
    snprintf(dir, sizeof dir, ROMS "/%s", SYSTEMS[sys].dir);
    ngames = scan_dir(dir, sys, 0, games, 0, MAXE);
    qsort(games, ngames, sizeof *games, cmp_label);
}

/* recent: one "sys<TAB>path" per line, newest first */
static void load_recent(void) {
    ngames = 0;
    FILE *f = fopen(CONF "/recent", "r");
    if (!f) return;
    char line[600];
    while (ngames < MAXRECENT && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        char *tab = strchr(line, '\t');
        if (!tab) continue;
        *tab = 0;
        struct stat st;
        if (stat(tab + 1, &st)) continue;
        for (int s = 0; s < NSYS; s++)
            if (!strcmp(SYSTEMS[s].dir, line)) {
                Entry *e = &games[ngames++];
                e->kind = K_GAME; e->sys = s;
                snprintf(e->path, sizeof e->path, "%s", tab + 1);
                pretty(e->label, sizeof e->label, e->path);
                break;
            }
    }
    fclose(f);
}

static void add_recent(const Entry *g) {
    char old[MAXRECENT][600];
    int n = 0;
    FILE *f = fopen(CONF "/recent", "r");
    if (f) {
        while (n < MAXRECENT && fgets(old[n], sizeof old[n], f)) n++;
        fclose(f);
    }
    f = fopen(CONF "/recent", "w");
    if (!f) return;
    char mine[600];
    snprintf(mine, sizeof mine, "%s\t%s\n", SYSTEMS[g->sys].dir, g->path);
    fputs(mine, f);
    for (int i = 0, w = 1; i < n && w < MAXRECENT; i++)
        if (strcmp(old[i], mine)) { fputs(old[i], f); w++; }
    fclose(f);
}

static void build_root(void) {
    nroot = 0;
    struct stat st;
    if (!stat(CONF "/recent", &st) && st.st_size > 0) {
        root[nroot].kind = K_RECENT; root[nroot].sys = -1;
        snprintf(root[nroot].label, sizeof root[nroot].label, "Zuletzt gespielt");
        nroot++;
    }
    for (int s = 0; s < NSYS; s++)
        if (count_games(s)) {
            root[nroot].kind = K_SYSTEM; root[nroot].sys = s;
            snprintf(root[nroot].label, sizeof root[nroot].label, "%s", SYSTEMS[s].name);
            nroot++;
        }
    static const char *dirs[NSYS], *names[NSYS];
    int ns = 0;
    for (int i = 0; i < nroot; i++)
        if (root[i].kind == K_SYSTEM) { dirs[ns] = SYSTEMS[root[i].sys].dir; names[ns] = SYSTEMS[root[i].sys].name; ns++; }
    settings_set_systems(dirs, names, ns);
}

/* ---------- drawing ---------- */
static SDL_Window *win;

static Ui ui = {.w = 640, .h = 480};

static int sel, top;
static int level;          /* 0 = root, 1 = game list */
static int root_sel;
static char title[64];
static int in_settings;   /* the F key shows the settings pages over the list */
static int saved_level, saved_sel, saved_top;   /* where F was pressed */

/* header: where we are (dark pill, top left), position "3/41", battery */
static void draw_header(const char *where, int pos, int n) {
    ui_battery(&ui);
    int right = ui.w - PAD - HINT - 14;
    if (n > 0) {
        char c[24];
        snprintf(c, sizeof c, "%d/%d", pos + 1, n);
        int cw = ui_text_w(ui.btn, c);
        ui_text(&ui, ui.btn, c, right - cw, PAD + HINT / 2, UI_GRAY, cw);
        right -= cw + 16;
    }
    ui_title(&ui, where, right - PAD);
}

#define LIST_Y (PAD + HINT + 16)
#define ROWS ((ui.h - LIST_Y - PAD - HINT - 10) / PILL)

static void draw(const char *status) {
    ui_set(&ui, UI_BLACK);
    SDL_RenderClear(ui.ren);

    const char *a = NULL;
    if (in_settings) {
        draw_header(settings_title(), settings_sel(), settings_count());
        settings_draw(&ui, LIST_Y, ROWS);
        if (!status) status = settings_status();
        if (!status) { settings_hints(&ui); SDL_RenderPresent(ui.ren); return; }
    } else {
        Entry *list = level ? games : root;
        int n = level ? ngames : nroot;
        int y0 = LIST_Y, rows = ROWS;
        draw_header(level ? title : "MintOS", sel, n);
        if (sel < top) top = sel;
        if (sel >= top + rows) top = sel - rows + 1;
        if (n == 0) ui_text(&ui, ui.big, "Keine Spiele", PAD + 24, y0 + PILL / 2, UI_GRAY, ui.w);
        for (int r = 0; r < rows && top + r < n; r++) {
            int i = top + r, y = y0 + r * PILL;
            int maxw = ui.w - 2 * PAD - 48;
            Entry *e = &list[i];
            SDL_Color fg = UI_WHITE;
            if (i == sel) {
                int tw = ui_text_w(ui.big, e->label);
                ui_pill(&ui, PAD, y, (tw < maxw ? tw : maxw) + 48, PILL, UI_WHITE);
                fg = UI_BLACK;
            }
            ui_text(&ui, ui.big, e->label, PAD + 24, y + PILL / 2, fg, maxw);
        }
        Entry *e = n ? &list[sel] : NULL;
        a = !e ? NULL : e->kind == K_GAME ? "STARTEN" : "ÖFFNEN";
    }

    if (status) {
        ui_text(&ui, ui.small, status, PAD + 4, ui.h - PAD - HINT / 2, UI_WHITE, ui.w - 2 * PAD);
    } else {
        const char *power[][2] = {{"POWER", "AUS"}};
        ui_hints(&ui, PAD, 0, power, 1);
        const char *both[][2] = {{"B", "ZURÜCK"}, {"A", a ? a : ""}};
        if (level && a) ui_hints(&ui, 0, ui.w - PAD, both, 2);
        else if (level) ui_hints(&ui, 0, ui.w - PAD, both, 1);
        else if (a) ui_hints(&ui, 0, ui.w - PAD, both + 1, 1);
    }
    SDL_RenderPresent(ui.ren);
}

static int open_video(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) return -1;
    SDL_ShowCursor(SDL_DISABLE);
    win = SDL_CreateWindow("rglauncher", 0, 0, ui.w, ui.h, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) return -1;
    SDL_GetWindowSize(win, &ui.w, &ui.h);
    ui.ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ui.ren) ui.ren = SDL_CreateRenderer(win, -1, 0);
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);
    return ui.ren ? 0 : -1;
}

static void close_video(void) {
    if (!win) return;
    SDL_DestroyRenderer(ui.ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    ui.ren = NULL; win = NULL;
}

/* remember where we were so we come back to the same place after a game; the marker in
 * /run (empty after every boot) tells a return from a game from a fresh start, which
 * begins at the top of the root list */
#define FROM_GAME "/run/rglauncher-from-game"
static void save_state(void) {
    FILE *f = fopen(CONF "/state", "w");
    if (!f) return;
    fprintf(f, "%d %d %d\n", level, level ? root_sel : sel, level ? sel : 0);
    fclose(f);
}

static void open_entry(int i);

static void load_state(void) {
    int lv = 0, rs = 0, gs = 0;
    if (unlink(FROM_GAME)) { sel = 0; return; }   /* fresh start: root list, top */
    FILE *f = fopen(CONF "/state", "r");
    if (f) { if (fscanf(f, "%d %d %d", &lv, &rs, &gs) != 3) lv = rs = gs = 0; fclose(f); }
    sel = rs < nroot ? rs : 0;
    if (lv == 1 && nroot) {
        open_entry(sel);
        sel = gs < ngames ? gs : 0;
    }
}

static void open_entry(int i) {
    Entry *e = &root[i];
    root_sel = i;
    level = 1;
    if (e->kind == K_RECENT) { load_recent(); snprintf(title, sizeof title, "Zuletzt gespielt"); }
    else { load_system(e->sys); snprintf(title, sizeof title, "%s", e->label); }
    root_sel = i;
    level = 1;
    sel = top = 0;
}

/* Hand the display to the game for good: replace this process with launch.sh.
 * Re-initialising SDL's KMSDRM video after RetroArch left a black screen, so instead
 * systemd restarts the launcher when the game exits and load_state() restores the list. */
static void launch(Entry *g) {
    add_recent(g);
    if (level && root[root_sel].kind == K_RECENT) sel = 0;  /* the game is now first in the list */
    if (win) save_state();  /* not on resume at boot: the list was never loaded */
    int m = open(FROM_GAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (m >= 0) close(m);
    close_video();
    int fd = open("/var/log/rglauncher-game.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }
    /* exec directly, no shell: game names may contain quotes */
    execl(CONF "/launch.sh", "launch.sh", g->path, SYSTEMS[g->sys].dir, (char *)NULL);
    _exit(127);
}

static void move(int d, int n) {
    if (n == 0) return;
    sel = (sel + d + n) % n;
}

/* Garlic-style resume: rg353-power wrote CONF/resume ("<system>\t<rom>") when the power key
 * switched the device off during a game. Start that game again; RESUME=1 makes launch.sh
 * load RetroArch's auto save state. The file is removed first so a crash cannot loop. */
static void resume_game(void) {
    char line[700], *tab;
    FILE *f = fopen(CONF "/resume", "r");
    if (!f) return;
    int ok = fgets(line, sizeof line, f) != NULL;
    fclose(f);
    unlink(CONF "/resume");
    if (!ok || !(tab = strchr(line, '\t'))) return;
    *tab = 0;
    line[strcspn(tab + 1, "\n") + (tab + 1 - line)] = 0;
    for (int s = 0; s < NSYS; s++) {
        if (strcmp(SYSTEMS[s].dir, line)) continue;
        Entry e = {.kind = K_GAME, .sys = s};
        snprintf(e.path, sizeof e.path, "%s", tab + 1);
        snprintf(e.label, sizeof e.label, "%s", tab + 1);
        if (access(e.path, R_OK)) return;
        setenv("RESUME", "1", 1);
        launch(&e);
    }
}

int main(void) {
    mkdir(CONF, 0755);
    /* before touching the display: opening and at once closing KMS left the panel black */
    resume_game();
    if (ui_fonts(&ui, FONT, FONT2) < 0 || open_video() < 0) { fprintf(stderr, "init: %s\n", SDL_GetError()); return 1; }
    build_root();
    load_state();
    Uint32 held_since = 0, last_rep = 0, last_draw = 0, last_input = SDL_GetTicks();
    int held = 0, screen_off = 0;
    const char *status = NULL;
    bl_power(1);
    /* With KMSDRM the first presented frame after taking over the display (boot, or
     * after RetroArch exits) often never reaches the panel, so the list stayed black
     * until the 30 s battery redraw. Present a few frames to be sure one sticks. */
    for (int i = 0; i < 4; i++) { draw(NULL); SDL_Delay(50); }
    for (;;) {
        SDL_Event ev;
        int dirty = 0;
        while (SDL_PollEvent(&ev)) {
            int btn = -1;
            if (ev.type == SDL_QUIT) return 0;
            if (ev.type == SDL_CONTROLLERDEVICEADDED) SDL_GameControllerOpen(ev.cdevice.which);
            if (ev.type == SDL_CONTROLLERBUTTONDOWN) btn = ev.cbutton.button;
            if (ev.type == SDL_CONTROLLERBUTTONUP) { if (ev.cbutton.button == held) held = 0; continue; }
            if (ev.type == SDL_CONTROLLERAXISMOTION && ev.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                int v = ev.caxis.value;
                if (v > 20000 && held != SDL_CONTROLLER_BUTTON_DPAD_DOWN) btn = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
                else if (v < -20000 && held != SDL_CONTROLLER_BUTTON_DPAD_UP) btn = SDL_CONTROLLER_BUTTON_DPAD_UP;
                else if (v > -8000 && v < 8000) held = 0;
            }
            if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                switch (ev.key.keysym.sym) {
                case SDLK_UP: btn = SDL_CONTROLLER_BUTTON_DPAD_UP; break;
                case SDLK_DOWN: btn = SDL_CONTROLLER_BUTTON_DPAD_DOWN; break;
                case SDLK_LEFT: btn = SDL_CONTROLLER_BUTTON_DPAD_LEFT; break;
                case SDLK_RIGHT: btn = SDL_CONTROLLER_BUTTON_DPAD_RIGHT; break;
                case SDLK_RETURN: btn = SDL_CONTROLLER_BUTTON_B; break;
                case SDLK_BACKSPACE: case SDLK_ESCAPE: btn = SDL_CONTROLLER_BUTTON_A; break;
                }
            }
            if (ev.type == SDL_KEYUP) held = 0;
            if (btn < 0) continue;
            last_input = SDL_GetTicks();
            if (screen_off) { screen_off = 0; bl_power(1); held = 0; dirty = 1; continue; }   /* the waking key does nothing else */
            dirty = 1;
            /* SDL names buttons by position (Xbox layout). The RG353V prints Nintendo
             * labels: its A is the right button (SDL "B"), its B the bottom one (SDL "A"). */
            int press_a = btn == SDL_CONTROLLER_BUTTON_B, press_b = btn == SDL_CONTROLLER_BUTTON_A;
            int nav = btn == SDL_CONTROLLER_BUTTON_DPAD_UP ? NAV_UP : btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN ? NAV_DOWN
                    : btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? NAV_LEFT : btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ? NAV_RIGHT
                    : press_a ? NAV_A : press_b ? NAV_B : btn == SDL_CONTROLLER_BUTTON_X ? NAV_X
                    : btn == SDL_CONTROLLER_BUTTON_Y ? NAV_Y : btn == SDL_CONTROLLER_BUTTON_START ? NAV_START
                    : btn == SDL_CONTROLLER_BUTTON_BACK ? NAV_SELECT : -1;
            /* F (SDL "guide") opens the settings from anywhere in the list and closes them again */
            int leave = 0;
            if (btn == SDL_CONTROLLER_BUTTON_GUIDE) {
                if (!in_settings) {
                    saved_level = level; saved_sel = sel; saved_top = top;
                    in_settings = 1; settings_open();
                    continue;
                }
                leave = 1;
            }
            if (in_settings) {
                if (leave || (nav >= 0 && settings_key(nav))) {
                    in_settings = 0; build_root();
                    level = saved_level; sel = saved_sel; top = saved_top;
                    int n = level ? ngames : nroot;
                    if (sel >= n) sel = n ? n - 1 : 0;
                    if (!level && root_sel >= nroot) root_sel = 0;
                }
                if (nav >= NAV_UP && nav <= NAV_RIGHT) { held = btn; held_since = last_rep = SDL_GetTicks(); }
                continue;
            }
            Entry *list = level ? games : root;
            int n = level ? ngames : nroot;
            status = NULL;
            int page = ROWS;
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) move(-1, n);
            else if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) move(1, n);
            else if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT || btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                sel -= page; if (sel < 0) sel = 0;
            } else if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                sel += page; if (sel > n - 1) sel = n - 1; if (sel < 0) sel = 0;
            } else if (press_b && level) {
                level = 0; build_root();
                sel = root_sel < nroot ? root_sel : 0; top = 0;
            } else if (press_a && n) {
                Entry *e = &list[sel];
                if (e->kind == K_GAME) {
                    draw("Startet ...");
                    launch(e);
                } else {
                    open_entry(sel);
                }
            }
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP || btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                held = btn; held_since = last_rep = SDL_GetTicks();
            }
        }
        if (in_settings && settings_poll()) dirty = 1;
        Uint32 now = SDL_GetTicks();
        if (held && now - held_since > 350 && now - last_rep > (in_settings ? 120 : 70)) {
            if (in_settings)
                settings_key(held == SDL_CONTROLLER_BUTTON_DPAD_UP ? NAV_UP : held == SDL_CONTROLLER_BUTTON_DPAD_DOWN ? NAV_DOWN
                             : held == SDL_CONTROLLER_BUTTON_DPAD_LEFT ? NAV_LEFT : NAV_RIGHT);
            else
                move(held == SDL_CONTROLLER_BUTTON_DPAD_UP ? -1 : 1, level ? ngames : nroot);
            last_rep = now; dirty = 1;
        }
        /* screen off after the chosen idle time (settings: "Bildschirm aus") */
        static int off_min = -1;
        static Uint32 last_cfg;
        if (off_min < 0 || now - last_cfg > 1000) { off_min = kv_get_int(SETTINGS_FILE, "screen_off", 3); last_cfg = now; }
        if (!screen_off && off_min > 0 && now - last_input > (Uint32)off_min * 60000) { screen_off = 1; bl_power(0); }
        if (now - last_draw > 30000) dirty = 1;  /* keep the battery current */
        if (dirty && !screen_off) { draw(status); last_draw = now; }
        SDL_Delay(10);
    }
}
