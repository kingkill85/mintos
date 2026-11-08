/* MinUI in-game menu: Weiter / Speichern / Laden / Beenden, 9 slots with preview. */
#pragma once
#include <stdbool.h>
#include "ui.h"
#include "libretro.h"

typedef enum { M_NONE, M_CONTINUE, M_SAVE, M_LOAD, M_GAMEMENU, M_DISC, M_QUIT } MenuAction;

typedef struct {
    int sel, slot, nitems;
    bool can_state;
    MenuAction items[6];
    const char *msg;   /* "Speichern fehlgeschlagen" etc., cleared by the next key */
    unsigned discs, disc;   /* multi-disc games */
    char text[48];
} Menu;

#define MENU_SLOTS 9

/* game_menu: no savestates, but the game has its own save/load menu (ScummVM, F5) */
void menu_init(Menu *m, bool can_state, int last_slot, bool game_menu);
/* RetroPad button pressed: action on A (the selected item), M_CONTINUE on B, else M_NONE */
MenuAction menu_key(Menu *m, int retro_id);
const char *menu_label(MenuAction a);
void menu_failed(Menu *m, MenuAction a);
/* adds/removes "Disc wechseln (i/n)" before Beenden (only for n > 1) */
void menu_set_discs(Menu *m, unsigned count, unsigned index);
const char *menu_item_text(Menu *m, int i);   /* label of item i as shown */  /* show "<Speichern|Laden> fehlgeschlagen" */
bool menu_on_slot_item(const Menu *m);    /* Speichern or Laden selected */
/* draws over whatever is on screen (the dimmed game); preview NULL = empty slot */
void menu_draw(Ui *ui, Menu *m, const char *title, SDL_Texture *preview);
