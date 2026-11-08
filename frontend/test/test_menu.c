// uses: frontend/menu.c common/ui.c
// libs: $(pkg-config --libs sdl2 SDL2_ttf SDL2_image)
#include <string.h>
#include "check.h"
#include "menu.h"

int main(void) {
    Menu m;
    menu_init(&m, true, 0);
    CHECK_INT(m.nitems, 4);
    CHECK_INT(m.items[0], M_CONTINUE); CHECK_INT(m.items[1], M_SAVE);
    CHECK_INT(m.items[2], M_LOAD); CHECK_INT(m.items[3], M_QUIT);
    CHECK_INT(m.sel, 0);

    menu_init(&m, false, 0);
    CHECK_INT(m.nitems, 2);
    CHECK_INT(m.items[0], M_CONTINUE); CHECK_INT(m.items[1], M_QUIT);

    menu_init(&m, true, 4);
    CHECK_INT(m.slot, 4);                                       /* remembers the last slot */
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_UP), M_NONE); /* wraps to the last item */
    CHECK_INT(m.sel, 3);
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
    CHECK_STR(menu_label(M_SAVE), "Speichern");
    CHECK_STR(menu_label(M_QUIT), "Beenden");
    TEST_END();
}
