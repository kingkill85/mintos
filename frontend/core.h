/* libretro host: loads a core, answers its environment calls, keeps its options
 * in RetroArch's .opt file (/storage/.config/retroarch/config/<lib>/<lib>.opt). */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "libretro.h"

typedef struct {
    struct retro_system_info info;
    struct retro_system_av_info av;
    enum retro_pixel_format fmt;
    bool can_state;          /* serialize size > 0 after load */
    bool geometry_changed;   /* set by SET_GEOMETRY / SET_SYSTEM_AV_INFO, cleared by the caller */
    void (*on_video)(const void *data, unsigned w, unsigned h, size_t pitch);
    size_t (*on_audio)(const int16_t *data, size_t frames);
    int16_t (*on_input)(unsigned port, unsigned device, unsigned index, unsigned id);
    retro_keyboard_event_t keyboard;   /* set by the core, may be NULL */
    void *dl;                          /* private */
} Core;

int  core_load(Core *c, const char *so_path, const char *rom);   /* 0 ok, -1 (logged) */
void core_run(Core *c);
void core_reset(Core *c);
void core_unload(Core *c);
size_t core_state_size(Core *c);
bool core_state_save(Core *c, void *buf, size_t n);
bool core_state_load(Core *c, const void *buf, size_t n);
void *core_memory(Core *c, unsigned id, size_t *size);   /* RETRO_MEMORY_SAVE_RAM / _RTC / _SYSTEM_RAM */
/* the core's SET_MEMORY_MAPS (descriptors copied), NULL if the core set none; valid until core_unload */
const struct retro_memory_map *core_memory_map(Core *c);
const char *core_option(Core *c, const char *key);       /* current value or NULL */
/* multi-disc games (disk control interface); count 0 = not supported */
unsigned core_disc_count(Core *c);
unsigned core_disc_index(Core *c);
bool core_disc_next(Core *c);   /* eject, select the next image (wraps), close */

/* log to /var/log/rgarch.log (RGARCH_ROOT-prefixed), also used by the other modules */
void rg_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
