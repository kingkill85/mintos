// uses: frontend/menu.c frontend/gameopts.c frontend/config.c common/ui.c
// libs: $(pkg-config --libs sdl2 SDL2_ttf SDL2_image)
#include <string.h>
#include "check.h"
#include "menu.h"

int main(void) {
    Menu m;
    menu_init(&m, true, 0, false);
    CHECK_INT(m.nitems, 5);
    CHECK_INT(m.items[0], M_CONTINUE); CHECK_INT(m.items[1], M_SAVE);
    CHECK_INT(m.items[2], M_LOAD); CHECK_INT(m.items[3], M_RESET); CHECK_INT(m.items[4], M_QUIT);
    CHECK_INT(m.sel, 0);

    menu_init(&m, false, 0, false);
    CHECK_INT(m.nitems, 3);
    CHECK_INT(m.items[0], M_CONTINUE); CHECK_INT(m.items[1], M_RESET); CHECK_INT(m.items[2], M_QUIT);

    /* no savestates but the game has its own save menu (ScummVM): offer that instead */
    menu_init(&m, false, 0, true);
    CHECK_INT(m.nitems, 4);
    CHECK_INT(m.items[1], M_GAMEMENU); CHECK_INT(m.items[2], M_RESET);
    CHECK_STR(menu_label(M_GAMEMENU), "ScummVM-Menü");
    m.sel = 1;
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A), M_GAMEMENU);

    menu_init(&m, true, 4, false);
    CHECK_INT(m.slot, 4);                                       /* remembers the last slot */
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_UP), M_NONE); /* wraps to the last item */
    CHECK_INT(m.sel, 4);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_DOWN), M_NONE);
    CHECK_INT(m.sel, 0);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_RIGHT), M_NONE);
    CHECK_INT(m.slot, 4);                                       /* slots only on Speichern/Laden */
    menu_key(&m, RETRO_DEVICE_ID_JOYPAD_DOWN);                  /* Speichern */
    menu_key(&m, RETRO_DEVICE_ID_JOYPAD_RIGHT);
    CHECK_INT(m.slot, 5);
    for (int i = 0; i < 4; i++) menu_key(&m, RETRO_DEVICE_ID_JOYPAD_RIGHT);
    CHECK_INT(m.slot, 0);                                       /* 8 -> 0 */
    menu_key(&m, RETRO_DEVICE_ID_JOYPAD_LEFT);
    CHECK_INT(m.slot, 8);
    menu_key(&m, RETRO_DEVICE_ID_JOYPAD_DOWN);                  /* Laden */
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A), M_LOAD);
    CHECK_INT(m.slot, 8);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_B), M_CONTINUE);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_X), M_NONE);
    /* a failed save/load keeps the menu open with a message until the next key */
    menu_init(&m, true, 0, false);
    CHECK(m.msg == NULL);
    menu_failed(&m, M_SAVE);
    CHECK_STR(m.msg, "Speichern fehlgeschlagen");
    menu_failed(&m, M_LOAD);
    CHECK_STR(m.msg, "Laden fehlgeschlagen");
    menu_key(&m, RETRO_DEVICE_ID_JOYPAD_DOWN);
    CHECK(m.msg == NULL);
    /* multi-disc game: "Disc wechseln" before Beenden, label shows the current disc */
    menu_init(&m, true, 0, false);
    menu_set_discs(&m, 3, 0);
    CHECK_INT(m.nitems, 6);
    CHECK_INT(m.items[3], M_RESET); CHECK_INT(m.items[4], M_DISC);
    CHECK_INT(m.items[5], M_QUIT);
    CHECK_STR(menu_item_text(&m, 4), "Disc wechseln (1/3)");
    menu_set_discs(&m, 1, 0);              /* single disc: no item */
    CHECK_INT(m.nitems, 5);
    CHECK_STR(menu_label(M_SAVE), "Speichern");
    CHECK_STR(menu_label(M_QUIT), "Beenden");
    /* Neustart only on a second A in a row; anything else in between disarms it */
    menu_init(&m, true, 0, false);
    m.sel = 3;
    CHECK_STR(menu_label(M_RESET), "Neustart");
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A), M_NONE);
    CHECK(m.msg != NULL);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A), M_RESET);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A), M_NONE);   /* armed again, not reset */
    menu_key(&m, RETRO_DEVICE_ID_JOYPAD_DOWN);
    menu_key(&m, RETRO_DEVICE_ID_JOYPAD_UP);
    CHECK(m.msg == NULL);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A), M_NONE);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_B), M_CONTINUE);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A), M_NONE);   /* B disarmed it */
    TEST_END();
}
