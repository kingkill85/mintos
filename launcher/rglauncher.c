/* rglauncher: a minimal, MinUI-style game launcher for ROCKNIX on the RG353V.
 *
 * Black screen, white text, the selection is a white pill. The root list shows
 * "Zuletzt gespielt" and every system that has games; A opens, B goes back.
 * Games start through ROCKNIX's runemu.sh (see device/launch.sh). */
#include <SDL.h>
#include <SDL_ttf.h>
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
#define FALLBACK_FONT "/usr/share/fonts/liberation/LiberationSans-Bold.ttf"
#define PSU "/sys/class/power_supply"
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
enum { K_GAME, K_SYSTEM, K_RECENT, K_POWEROFF };
typedef struct { int kind, sys; char label[128]; char path[512]; } Entry;

static Entry games[MAXE];   /* current game list */
static int ngames;
static Entry root[NSYS + 2];
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
}

/* ---------- drawing ---------- */
static SDL_Window *win;
static SDL_Renderer *ren;
static TTF_Font *big, *small, *btn;
static int W = 640, H = 480;

/* MinUI geometry at 640x480 (its 2x scale): padding 20, pill 60, button 44 */
#define PAD 20
#define PILL 60
#define HINT 60
#define BTN 44

static const SDL_Color WHITE = {255, 255, 255, 255}, BLACK = {0, 0, 0, 255},
                       GRAY = {0x99, 0x99, 0x99, 255}, DARK = {0x26, 0x26, 0x26, 255},
                       LIGHT = {0xcc, 0xcc, 0xcc, 255}, MID = {0x80, 0x80, 0x80, 255};

static void set(SDL_Color c) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255); }

/* filled rounded rectangle with fully round ends */
static void pill(int x, int y, int w, int h, SDL_Color c) {
    set(c);
    int r = h / 2;
    for (int dy = 0; dy < h; dy++) {
        float cy = dy + 0.5f - h / 2.0f;
        float dx2 = (float)r * r - cy * cy;
        int inset = dx2 > 0 ? r - (int)(SDL_sqrtf(dx2) + 0.5f) : r;
        SDL_RenderDrawLine(ren, x + inset, y + dy, x + w - 1 - inset, y + dy);
    }
}

static int text_w(TTF_Font *f, const char *s) {
    int w = 0;
    TTF_SizeUTF8(f, s, &w, NULL);
    return w;
}

/* draws s vertically centered on cy, cut with "..." to fit maxw; returns drawn width */
static int text(TTF_Font *f, const char *s, int x, int cy, SDL_Color c, int maxw) {
    char buf[160];
    snprintf(buf, sizeof buf, "%s", s);
    if (text_w(f, buf) > maxw) {
        size_t len = strlen(buf);
        while (len > 1) {
            len--;
            while (len > 1 && (buf[len] & 0xc0) == 0x80) len--;  /* keep UTF-8 intact */
            snprintf(buf + len, sizeof buf - len, "...");
            if (text_w(f, buf) <= maxw) break;
        }
    }
    if (!*buf) return 0;
    SDL_Surface *sf = TTF_RenderUTF8_Blended(f, buf, c);
    if (!sf) return 0;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, sf);
    SDL_Rect dst = {x, cy - sf->h / 2, sf->w, sf->h};
    SDL_RenderCopy(ren, t, NULL, &dst);
    int w = sf->w;
    SDL_DestroyTexture(t);
    SDL_FreeSurface(sf);
    return w;
}

static int read_int(const char *path) {
    FILE *f = fopen(path, "r");
    int v = -1;
    if (f) { if (fscanf(f, "%d", &v) != 1) v = -1; fclose(f); }
    return v;
}

/* first power supply of type Battery ("battery" on ROCKNIX, "rk817-battery" on mainline) */
static const char *battery_dir(void) {
    static char dir[300];
    if (dir[0]) return dir;
    DIR *d = opendir(PSU);
    struct dirent *de;
    while (d && (de = readdir(d))) {
        char p[300], t[16] = "";
        snprintf(p, sizeof p, PSU "/%s/type", de->d_name);
        FILE *f = fopen(p, "r");
        if (!f) continue;
        if (!fgets(t, sizeof t, f)) t[0] = 0;
        fclose(f);
        if (!strncmp(t, "Battery", 7)) { snprintf(dir, sizeof dir, PSU "/%s", de->d_name); break; }
    }
    if (d) closedir(d);
    return dir;
}

static void battery(void) {
    char cap[320], stp[320];
    snprintf(cap, sizeof cap, "%s/capacity", battery_dir());
    snprintf(stp, sizeof stp, "%s/status", battery_dir());
    int pct = read_int(cap);
    if (pct < 0) return;
    char st[32] = "";
    FILE *f = fopen(stp, "r");
    if (f) { if (!fgets(st, sizeof st, f)) st[0] = 0; fclose(f); }
    int charging = !strncmp(st, "Charging", 8);
    int cx = W - PAD - PILL, cy = PAD;
    pill(cx, cy, PILL, PILL, DARK);                       /* circle */
    int bw = 30, bh = 16, x = cx + (PILL - bw - 3) / 2, y = cy + (PILL - bh) / 2;
    set(WHITE);
    for (int i = 0; i < 2; i++) { SDL_Rect r = {x + i, y + i, bw - 2 * i, bh - 2 * i}; SDL_RenderDrawRect(ren, &r); }
    SDL_Rect nub = {x + bw, y + 4, 3, bh - 8};
    SDL_RenderFillRect(ren, &nub);
    int fillw = (bw - 8) * (pct > 100 ? 100 : pct) / 100;
    SDL_Color c = pct <= 10 && !charging ? (SDL_Color){0xff, 0x30, 0x30, 255} : WHITE;
    set(charging ? (SDL_Color){0x40, 0xd0, 0x60, 255} : c);
    SDL_Rect lvl = {x + 4, y + 4, fillw, bh - 8};
    SDL_RenderFillRect(ren, &lvl);
}

/* MinUI hint group: one dark pill holding "[button] WORD" pairs. A button is a light
 * circle with a dark letter, or (long names like POWER) a light pill with gray text.
 * Returns the group's width; drawn right-aligned at `right` or left-aligned at `left`. */
static int hint_group(int left, int right, const char *pairs[][2], int n) {
    int w = 8, bws[4], lws[4];
    for (int i = 0; i < n; i++) {
        int tw = text_w(pairs[i][0][1] ? small : btn, pairs[i][0]);
        bws[i] = pairs[i][0][1] ? tw + 24 : BTN;
        lws[i] = text_w(small, pairs[i][1]);
        w += bws[i] + 10 + lws[i] + 14;
    }
    int x = right ? right - w : left, y = H - PAD - HINT, cy = y + HINT / 2;
    pill(x, y, w, HINT, DARK);
    int cx = x + 8;
    for (int i = 0; i < n; i++) {
        const char *b = pairs[i][0];
        pill(cx, cy - BTN / 2, bws[i], BTN, LIGHT);
        TTF_Font *f = b[1] ? small : btn;
        text(f, b, cx + (bws[i] - text_w(f, b)) / 2, cy, b[1] ? MID : DARK, bws[i]);
        cx += bws[i] + 10;
        text(small, pairs[i][1], cx, cy, WHITE, lws[i]);
        cx += lws[i] + 14;
    }
    return w;
}

static int sel, top;
static int level;          /* 0 = root, 1 = game list */
static int root_sel;
static char title[64];

static void draw(const char *status) {
    set(BLACK);
    SDL_RenderClear(ren);
    battery();

    Entry *list = level ? games : root;
    int n = level ? ngames : nroot;
    int y0 = PAD, rows = (H - y0 - PAD - HINT) / PILL;
    if (sel < top) top = sel;
    if (sel >= top + rows) top = sel - rows + 1;
    if (n == 0) text(big, "Keine Spiele", PAD + 24, y0 + PILL / 2, GRAY, W);
    for (int r = 0; r < rows && top + r < n; r++) {
        int i = top + r, y = y0 + r * PILL;
        int maxw = W - 2 * PAD - 48 - (r == 0 ? PILL + 10 : 0);  /* first row leaves room for the battery */
        Entry *e = &list[i];
        SDL_Color fg = WHITE;
        if (i == sel) {
            int tw = text_w(big, e->label);
            pill(PAD, y, (tw < maxw ? tw : maxw) + 48, PILL, WHITE);
            fg = BLACK;
        }
        text(big, e->label, PAD + 24, y + PILL / 2, fg, maxw);
    }

    if (status) {
        text(small, status, PAD + 4, H - PAD - HINT / 2, WHITE, W - 2 * PAD);
    } else {
        const char *power[][2] = {{"POWER", "AUS"}};
        hint_group(PAD, 0, power, 1);
        Entry *e = n ? &list[sel] : NULL;
        const char *a = !e ? NULL : e->kind == K_GAME ? "STARTEN" : "ÖFFNEN";
        const char *both[][2] = {{"B", "ZURÜCK"}, {"A", a ? a : ""}};
        if (level && a) hint_group(0, W - PAD, both, 2);
        else if (level) hint_group(0, W - PAD, both, 1);
        else if (a) hint_group(0, W - PAD, both + 1, 1);
    }
    SDL_RenderPresent(ren);
}

static int open_video(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) < 0) return -1;
    SDL_ShowCursor(SDL_DISABLE);
    win = SDL_CreateWindow("rglauncher", 0, 0, W, H, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) return -1;
    SDL_GetWindowSize(win, &W, &H);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);
    return ren ? 0 : -1;
}

static void close_video(void) {
    if (!win) return;
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    ren = NULL; win = NULL;
}

static int fonts(void) {
    if (TTF_Init() < 0) return -1;
    /* MinUI at 640x480: large 32, small 24, buttons 28 */
    big = TTF_OpenFont(FONT, 32);
    small = TTF_OpenFont(FONT, 24);
    btn = TTF_OpenFont(FONT, 28);
    int bold = 0;
    if (!big || !small || !btn) {
        big = TTF_OpenFont(FONT2, 30); small = TTF_OpenFont(FONT2, 22); btn = TTF_OpenFont(FONT2, 26);
        bold = 1;
    }
    if (!big || !small) {
        big = TTF_OpenFont(FALLBACK_FONT, 28);
        small = TTF_OpenFont(FALLBACK_FONT, 18);
    }
    if (!big || !small) return -1;
    if (!btn) btn = small;
    if (bold) { TTF_SetFontStyle(big, TTF_STYLE_BOLD); TTF_SetFontStyle(small, TTF_STYLE_BOLD); TTF_SetFontStyle(btn, TTF_STYLE_BOLD); }
    return 0;
}

/* remember where we were so we come back to the same place after a game */
static void save_state(void) {
    FILE *f = fopen(CONF "/state", "w");
    if (!f) return;
    fprintf(f, "%d %d %d\n", level, level ? root_sel : sel, level ? sel : 0);
    fclose(f);
}

static void open_entry(int i);

static void load_state(void) {
    int lv = 0, rs = 0, gs = 0;
    FILE *f = fopen(CONF "/state", "r");
    if (f) { if (fscanf(f, "%d %d %d", &lv, &rs, &gs) != 3) lv = rs = gs = 0; fclose(f); }
    sel = rs < nroot ? rs : 0;
    if (lv == 1 && root[sel].kind != K_POWEROFF) {
        open_entry(sel);
        sel = gs < ngames ? gs : 0;
    }
}

static void open_entry(int i) {
    Entry *e = &root[i];
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
    if (fonts() < 0 || open_video() < 0) { fprintf(stderr, "init: %s\n", SDL_GetError()); return 1; }
    build_root();
    load_state();
    Uint32 held_since = 0, last_rep = 0, last_draw = 0;
    int held = 0, confirm_off = 0;
    const char *status = NULL;
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
            dirty = 1;
            Entry *list = level ? games : root;
            int n = level ? ngames : nroot;
            /* SDL names buttons by position (Xbox layout). The RG353V prints Nintendo
             * labels: its A is the right button (SDL "B"), its B the bottom one (SDL "A"). */
            int press_a = btn == SDL_CONTROLLER_BUTTON_B, press_b = btn == SDL_CONTROLLER_BUTTON_A;
            if (!press_a) { confirm_off = 0; status = NULL; }
            int page = (H - PAD - HINT - 8 - PAD - HINT - 8) / PILL;
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
                if (e->kind == K_POWEROFF) {
                    if (confirm_off) { save_state(); close_video(); if (system("systemctl poweroff") < 0) {} return 0; }
                    confirm_off = 1; status = "Nochmal A zum Ausschalten";
                } else if (e->kind == K_GAME) {
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
        Uint32 now = SDL_GetTicks();
        if (held && now - held_since > 350 && now - last_rep > 70) {
            move(held == SDL_CONTROLLER_BUTTON_DPAD_UP ? -1 : 1, level ? ngames : nroot);
            last_rep = now; dirty = 1;
        }
        if (now - last_draw > 30000) dirty = 1;  /* keep the battery current */
        if (dirty) { draw(status); last_draw = now; }
        SDL_Delay(10);
    }
}
