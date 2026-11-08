/* Saves (/storage/saves/<sys>) and states with previews (/storage/states/<sys>).
 * Files are written to <file>.tmp, synced and renamed, so a power cut never leaves a half file. */
#pragma once
#include <stdbool.h>
#include <SDL.h>
#include "core.h"

void state_sram_load(Core *c, const char *rom);
bool state_sram_save(Core *c, const char *rom);   /* false when unchanged since load/last save */
/* same, but only copies the memory now and writes it in a background thread (periodic save,
 * no hitch on a slow card); false when unchanged or a write is still running */
bool state_sram_save_async(Core *c, const char *rom);
void state_sram_wait(void);                        /* wait for a background write */
/* slot 0..8, or -1 = auto state; preview (may be NULL) saved as PNG next to the slot */
bool state_save(Core *c, const char *rom, int slot, SDL_Surface *preview);
bool state_load(Core *c, const char *rom, int slot);
/* for games that keep their own saves (ScummVM quicksave slots): marker file + preview, so
 * the menu shows the slot as used */
bool state_mark(const char *rom, int slot, SDL_Surface *preview);
bool state_exists(const char *rom, int slot);
SDL_Texture *state_preview(SDL_Renderer *r, const char *rom, int slot);   /* NULL if none */
