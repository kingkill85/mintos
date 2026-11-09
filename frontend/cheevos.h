/* RetroAchievements in rgarch, offline and softcore only. rgarch never talks to the network:
 * mintos-cheevos (tools/cheevos_sync.c) stores the server's answers in the cheevos dir
 * (common/cheevos_store.h), and rc_client's server calls are answered from those files;
 * unlocks go to the queue that the sync tool uploads later. */
#pragma once
#include <stdbool.h>
#include "core.h"

/* false (and no per-frame work) with "cheevos = 0" in settings.cfg, for systems without
 * achievements, without a token, without a hash for this ROM file (as it is now) or without
 * game/<hash>.json. Otherwise logs in and loads the game from the files (no waiting) and
 * shows the toast "x/y Erfolge". */
bool cheevos_start(Core *c, const char *system, const char *rom);
void cheevos_frame(void);   /* after every core_run: rc_client_do_frame; no-op when inactive */
void cheevos_idle(void);    /* while the game is paused (menu): rc_client_idle */
void cheevos_reset(void);   /* after a state load or a core reset: no triggers from old hit counts */
void cheevos_stop(void);    /* before core_unload */

typedef struct { char title[64], desc[128]; unsigned points; bool unlocked; } CheevoItem;
int cheevos_list(CheevoItem *out, int max);   /* the game's achievements; 0 when inactive */
