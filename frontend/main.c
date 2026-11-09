/* rgarch: libretro frontend with a MinUI-style menu (docs/superpowers/specs/2026-11-08-rgarch-design.md).
 *   rgarch [--resume] <system> <rom>
 * Settings from systems.cfg, the core from RetroArch's core folder. F opens the menu.
 * SIGTERM (power key) writes the auto state and SRAM and exits 0.
 * Env (tests): RGARCH_ROOT prefixes paths, RGARCH_CORE overrides the core file,
 * RGARCH_ERROR_TIMEOUT closes the error page after N seconds, RGARCH_TITLE sets the menu title. */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>
#include "audio.h"
#include "config.h"
#include "core.h"
#include "gameopts.h"
#include "settings.h"
#include "input.h"
#include "menu.h"
#include "osd.h"
#include "paths.h"
#include "state.h"
#include "ui.h"
#include "video.h"

#define FONT "/storage/.config/rglauncher/fonts/BPreplayBold-unhinted.otf"
#define FONT2 "/storage/.config/rglauncher/fonts/VarelaRound-Regular.ttf"
#define SRAM_EVERY_MS 60000

static volatile sig_atomic_t quit_signal;
static void on_term(int s) { (void)s; quit_signal = 1; }

static Core core;
static SystemCfg cfg;
static Ui ui;
static const char *rom, *sysname;
static char title[256];
/* a key combo for the core, sent once the game runs again (down on one frame, up on the next) */
static struct { unsigned code; uint16_t mod; int step; } pending;
/* ScummVM (SCUMM engine): no savestates, but quicksave slots Alt+1..9 / load Ctrl+1..9;
 * menu slots 0..8 map to ScummVM slots 1..9, slot 0 stays ScummVM's own autosave */
static bool kstates;

static void send_key(unsigned code, uint16_t mod) { pending.code = code; pending.mod = mod; pending.step = 0; }

static void on_video(const void *d, unsigned w, unsigned h, size_t pitch) { video_frame(d, w, h, pitch, core.fmt); }
static float aspect(void) { return core.av.geometry.aspect_ratio; }

static const char *root(void) { const char *r = getenv("RGARCH_ROOT"); return r ? r : ""; }

/* systems.cfg, then the player's own choices from user.cfg; -1 without systems.cfg */
static int load_cfg(void) {
    char p[1024];
    snprintf(p, sizeof p, "%s%s", root(), CONFIG_FILE);
    int r = config_load(p, sysname, &cfg);
    snprintf(p, sizeof p, "%s%s", root(), USER_CONFIG_FILE);
    config_merge(p, sysname, &cfg);
    return r;
}

/* Optionen in the menu: picture options are stored per system and apply at once */
static void apply_option(Menu *m) {
    if (m->changed == MO_BRIGHT) { bl_set(m->val[MO_BRIGHT]); return; }
    char p[1024];
    snprintf(p, sizeof p, "%s%s", root(), USER_CONFIG_FILE);
    gameopt_set(p, sysname, (GameOpt)m->changed, m->val[m->changed]);
    load_cfg();
}

/* game name for the menu: RGARCH_TITLE, else the file name without extension */
static void make_title(void) {
    const char *t = getenv("RGARCH_TITLE");
    if (t && *t) { snprintf(title, sizeof title, "%s", t); return; }
    const char *b = strrchr(rom, '/');
    snprintf(title, sizeof title, "%s", b ? b + 1 : rom);
    char *dot = strrchr(title, '.');
    if (dot) *dot = 0;
}

/* "Spiel konnte nicht gestartet werden" until B (or the test timeout) */
static int error_page(void) {
    Uint32 until = getenv("RGARCH_ERROR_TIMEOUT") ? SDL_GetTicks() + 1000 * atoi(getenv("RGARCH_ERROR_TIMEOUT")) : 0;
    for (;;) {
        if (quit_signal) return 0;        /* power key: switch off without waiting */
        input_poll();
        int k;
        while ((k = input_take_press()) >= 0)
            if (k == RETRO_DEVICE_ID_JOYPAD_B || k == RETRO_DEVICE_ID_JOYPAD_A) return 1;
        if (until && SDL_GetTicks() > until) return 1;
        if (ui.ren) {
            ui_set(&ui, UI_BLACK);
            SDL_RenderClear(ui.ren);
            if (ui.big) {
                ui_text(&ui, ui.big, "Spiel konnte nicht", PAD + 24, ui.h / 2 - 40, UI_WHITE, ui.w);
                ui_text(&ui, ui.big, "gestartet werden", PAD + 24, ui.h / 2 + 10, UI_WHITE, ui.w);
                const char *b[][2] = {{"B", "ZURÜCK"}};
                ui_hints(&ui, 0, ui.w - PAD, b, 1);
            }
            video_flip();
        }
        SDL_Delay(16);
    }
}

/* the menu; returns false when the game should end */
static bool run_menu(int *slot) {
    Menu m;
    menu_init(&m, core.can_state || kstates, *slot, core.keyboard != NULL && !core.can_state);
    menu_set_discs(&m, core_disc_count(&core), core_disc_index(&core));
    {
        char u[1024], s[1024];
        snprintf(u, sizeof u, "%s%s", root(), USER_CONFIG_FILE);
        snprintf(s, sizeof s, "%s%s", root(), CONFIG_FILE);
        int b = bl_get();
        int vals[MO_N] = {gameopt_get(u, sysname, GO_SCALE), gameopt_get(u, sysname, GO_FILTER),
                          gameopt_get(u, sysname, GO_FRAME), b > 0 ? b : 5};
        menu_set_options(&m, gameopt_has_frame(s, sysname), vals);
    }
    audio_pause(true);
    input_clear();
    SDL_Texture *pv = NULL;
    int pv_slot = -2;
    bool keep = true;
    for (bool open = true; open && !quit_signal;) {
        if (input_poll()) break;                       /* F again: back to the game */
        int k;
        while (open && (k = input_take_press()) >= 0) {
            switch (menu_key(&m, k)) {
            case M_CONTINUE: open = false; break;
            case M_SAVE: {
                SDL_Surface *s = video_snapshot(320, 240);
                bool ok = kstates ? state_mark(rom, m.slot, s) : state_save(&core, rom, m.slot, s);
                if (ok && kstates) send_key(RETROK_1 + m.slot, RETROKMOD_ALT);
                if (s) SDL_FreeSurface(s);
                if (ok) open = false; else menu_failed(&m, M_SAVE);
                pv_slot = -2;   /* reload the preview */
                break;
            }
            case M_LOAD:
                /* an empty slot does nothing (its preview says "Leer") */
                if (state_exists(rom, m.slot)) {
                    if (kstates) { send_key(RETROK_1 + m.slot, RETROKMOD_CTRL); open = false; }
                    else if (state_load(&core, rom, m.slot)) open = false;
                    else menu_failed(&m, M_LOAD);
                }
                break;
            case M_GAMEMENU:
                /* the game's own save/load menu (ScummVM: Ctrl+F5, works everywhere), after the menu closes */
                send_key(RETROK_F5, RETROKMOD_CTRL);
                open = false;
                break;
            case M_DISC:
                core_disc_next(&core);
                open = false;
                break;
            case M_OPTION: apply_option(&m); break;
            case M_QUIT: open = false; keep = false; break;
            default: break;
            }
        }
        if (!open) break;
        if (menu_on_slot_item(&m) && pv_slot != m.slot) {
            if (pv) SDL_DestroyTexture(pv);
            pv = state_preview(ui.ren, rom, m.slot);
            pv_slot = m.slot;
        }
        video_draw(&cfg, aspect(), true);
        menu_draw(&ui, &m, title, menu_on_slot_item(&m) ? pv : NULL);
        osd_poll();
        osd_draw(&ui);
        video_flip();
    }
    if (pv) SDL_DestroyTexture(pv);
    *slot = m.slot;
    input_clear();
    audio_pause(false);
    return keep;
}

int main(int argc, char **argv) {
    bool resume = argc > 1 && !strcmp(argv[1], "--resume");
    if (argc - resume < 3) {
        fprintf(stderr, "usage: rgarch [--resume] <system> <rom>\n");
        return 2;
    }
    const char *system = sysname = argv[1 + resume];
    rom = argv[2 + resume];
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);

    char path[1024];
    if (load_cfg()) rg_log("no %s%s, using defaults\n", root(), CONFIG_FILE);
    make_title();

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER);
    IMG_Init(IMG_INIT_PNG);
    if (video_open(&ui)) { rg_log("video: %s\n", SDL_GetError()); return 1; }
    if (ui_fonts(&ui, FONT, FONT2)) rg_log("fonts missing\n");
    input_open();

    const char *so = getenv("RGARCH_CORE");
    if (!so) {
        snprintf(path, sizeof path, "%s/storage/.config/retroarch/cores/%s_libretro.so", root(), cfg.core);
        so = path;
    }
    core.on_video = on_video;
    core.on_audio = audio_push;
    core.on_input = input_state;
    if (!*cfg.core && !getenv("RGARCH_CORE")) rg_log("no core for system %s\n", system);
    if ((!*cfg.core && !getenv("RGARCH_CORE")) || core_load(&core, so, rom)) {
        int r = error_page();
        video_close();
        return r;
    }
    input_set_keyboard(core.keyboard);
    kstates = !core.can_state && core.keyboard && !strcmp(system, "scummvm");
    audio_open(core.av.timing.sample_rate);
    state_sram_load(&core, rom);

    int slot = 0;
    Uint32 last_sram = SDL_GetTicks();
    bool first = true, ran = false;   /* ran: the game ran (and a resume, if any, loaded) */
    if (getenv("RGARCH_TEST_TERM_EARLY")) raise(SIGTERM);   /* test: power key during start-up */
    while (!quit_signal) {
        if (input_poll() && !run_menu(&slot)) break;
        Uint32 t_a = SDL_GetTicks();
        if (pending.code && core.keyboard) {
            core.keyboard(pending.step == 0, pending.code, 0, pending.mod);
            if (++pending.step == 2) pending.code = 0;
        }
        core_run(&core);
        Uint32 t_b = SDL_GetTicks();
        if (core.geometry_changed) {   /* SET_SYSTEM_AV_INFO may bring a new sample rate */
            audio_set_rate(core.av.timing.sample_rate);
            core.geometry_changed = false;
        }
        /* a resume that fails to load must not be replaced by this fresh start on exit */
        if (first) {
            if (resume && !state_load(&core, rom, -1)) {
                /* keep an unloadable resume point aside instead of losing it on the next power-off */
                char a[1024], b[1100];
                if (!path_for(P_AUTO, rom, 0, a, sizeof a)) {
                    snprintf(b, sizeof b, "%s.failed", a);
                    rename(a, b);
                    rg_log("resume failed, kept as %s\n", b);
                }
            }
            ran = true;
            first = false;
        }
        video_draw(&cfg, aspect(), false);
        osd_poll();
        osd_draw(&ui);
        video_flip();
        Uint32 t_c = SDL_GetTicks();
        audio_wait();
        if (getenv("RGARCH_FPS_DEBUG")) {
            static Uint32 t0, n, tr, tv;
            Uint32 now = SDL_GetTicks();
            n++; tr += t_b - t_a; tv += t_c - t_b;
            if (!t0) t0 = now;
            if (now - t0 >= 1000) { rg_log("fps %u (run %ums, draw+flip %ums)\n", n, tr, tv); n = 0; t0 = now; tr = tv = 0; }
        }
        if (SDL_GetTicks() - last_sram > SRAM_EVERY_MS) { state_sram_save_async(&core, rom); last_sram = SDL_GetTicks(); }
    }
    /* not before the first frame: that would replace a good resume point with a fresh boot */
    if (quit_signal && core.can_state && ran) state_save(&core, rom, -1, NULL);
    state_sram_wait();
    state_sram_save(&core, rom);
    audio_close();
    core_unload(&core);
    video_close();
    rg_log("exit (%s)\n", quit_signal ? "signal" : "menu");
    return 0;
}
