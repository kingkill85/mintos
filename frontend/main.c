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
#include "input.h"
#include "menu.h"
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
static const char *rom;
static char title[256];
static bool gamemenu_pending;   /* send F5 to the core once the game runs again */

static void on_video(const void *d, unsigned w, unsigned h, size_t pitch) { video_frame(d, w, h, pitch, core.fmt); }
static float aspect(void) { return core.av.geometry.aspect_ratio; }

static const char *root(void) { const char *r = getenv("RGARCH_ROOT"); return r ? r : ""; }

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
    menu_init(&m, core.can_state, *slot, core.keyboard != NULL);
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
                bool ok = state_save(&core, rom, m.slot, s);
                if (s) SDL_FreeSurface(s);
                if (ok) open = false; else menu_failed(&m, M_SAVE);
                pv_slot = -2;   /* reload the preview */
                break;
            }
            case M_LOAD:
                /* an empty slot does nothing (its preview says "Leer") */
                if (state_exists(rom, m.slot)) {
                    if (state_load(&core, rom, m.slot)) open = false; else menu_failed(&m, M_LOAD);
                }
                break;
            case M_GAMEMENU:
                /* the game's own save/load menu (ScummVM: Ctrl+F5, works everywhere), after the menu closes */
                gamemenu_pending = true;
                open = false;
                break;
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
    const char *system = argv[1 + resume];
    rom = argv[2 + resume];
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);

    char path[1024];
    snprintf(path, sizeof path, "%s%s", root(), CONFIG_FILE);
    if (config_load(path, system, &cfg)) rg_log("no %s, using defaults\n", path);
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
    audio_open(core.av.timing.sample_rate);
    state_sram_load(&core, rom);

    int slot = 0;
    Uint32 last_sram = SDL_GetTicks();
    bool first = true, ran = false;   /* ran: the game ran (and a resume was tried) */
    if (getenv("RGARCH_TEST_TERM_EARLY")) raise(SIGTERM);   /* test: power key during start-up */
    while (!quit_signal) {
        if (input_poll() && !run_menu(&slot)) break;
        Uint32 t_a = SDL_GetTicks();
        if (gamemenu_pending && core.keyboard) {
            static int step;
            /* key down on one frame, up on the next */
            core.keyboard(step == 0, RETROK_F5, 0, RETROKMOD_CTRL);   /* ScummVM global menu */
            if (++step == 2) { step = 0; gamemenu_pending = false; }
        }
        core_run(&core);
        Uint32 t_b = SDL_GetTicks();
        if (first && resume) state_load(&core, rom, -1);
        first = false;
        ran = true;
        video_draw(&cfg, aspect(), false);
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
        if (SDL_GetTicks() - last_sram > SRAM_EVERY_MS) { state_sram_save(&core, rom); last_sram = SDL_GetTicks(); }
    }
    /* not before the first frame: that would replace a good resume point with a fresh boot */
    if (quit_signal && core.can_state && ran) state_save(&core, rom, -1, NULL);
    state_sram_save(&core, rom);
    audio_close();
    core_unload(&core);
    video_close();
    rg_log("exit (%s)\n", quit_signal ? "signal" : "menu");
    return 0;
}
