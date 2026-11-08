// uses: frontend/menu.c frontend/gameopts.c frontend/config.c common/ui.c
// libs: $(pkg-config --libs sdl2 SDL2_ttf SDL2_image)
#include <string.h>
#include "check.h"
#include "menu.h"

int main(void) {
    Menu m;
    menu_init(&m, true, 0, false);
    int vals[MO_N] = {0, 2, 0, 5};
    menu_set_options(&m, true, vals);
    /* "Optionen" before Beenden */
    CHECK_INT(m.nitems, 5);
    CHECK_INT(m.items[3], M_OPTIONS);
    CHECK_INT(m.items[4], M_QUIT);
    CHECK_STR(menu_item_text(&m, 3), "Optionen");

    /* A opens the page: Bild, Filter, Rahmen, Helligkeit */
    m.sel = 3;
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A), M_NONE);
    CHECK(m.page);
    CHECK_INT(m.nopts, 4);
    CHECK_INT(m.opts[0], MO_SCALE); CHECK_INT(m.opts[2], MO_FRAME); CHECK_INT(m.opts[3], MO_BRIGHT);
    CHECK_STR(menu_opt_value(&m, 1), "Scharf");
    CHECK_STR(menu_opt_value(&m, 3), "5/10");

    /* right changes the selected option and reports it */
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_RIGHT), M_OPTION);
    CHECK_INT(m.changed, MO_SCALE);
    CHECK_INT(m.val[MO_SCALE], 1);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_LEFT), M_OPTION);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_LEFT), M_OPTION);
    CHECK_INT(m.val[MO_SCALE], 3);                             /* wraps */
    menu_key(&m, RETRO_DEVICE_ID_JOYPAD_UP);                   /* wraps to Helligkeit */
    CHECK_INT(m.osel, 3);
    for (int i = 0; i < 8; i++) menu_key(&m, RETRO_DEVICE_ID_JOYPAD_RIGHT);
    CHECK_INT(m.val[MO_BRIGHT], 10);                           /* clamped, no wrap */
    for (int i = 0; i < 12; i++) menu_key(&m, RETRO_DEVICE_ID_JOYPAD_LEFT);
    CHECK_INT(m.val[MO_BRIGHT], 1);
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A), M_NONE);  /* A does nothing here */

    /* B goes back to the main list (not to the game) */
    CHECK_INT(menu_key(&m, RETRO_DEVICE_ID_JOYPAD_B), M_NONE);
    CHECK(!m.page);
    CHECK_INT(m.sel, 3);

    /* without a frame in systems.cfg there is no Rahmen line */
    menu_init(&m, false, 0, false);
    menu_set_options(&m, false, vals);
    CHECK_INT(m.items[1], M_OPTIONS);
    m.sel = 1;
    menu_key(&m, RETRO_DEVICE_ID_JOYPAD_A);
    CHECK_INT(m.nopts, 3);
    CHECK_INT(m.opts[2], MO_BRIGHT);
    TEST_END();
}
