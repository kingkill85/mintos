// uses: frontend/input.c frontend/inputmap.c common/osd.c common/ui.c
// libs: $(pkg-config --libs sdl2 SDL2_ttf)
#include <string.h>
#include "check.h"
#include "input.h"

static int keys;
static void kb(bool down, unsigned code, uint32_t ch, uint16_t mod) { (void)down; (void)code; (void)ch; (void)mod; keys++; }

int main(void) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    input_open();
    CHECK(!input_poll());
    CHECK_INT(input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A), 0);
    CHECK_INT(input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK), 0);
    CHECK_INT(input_state(1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A), 0);   /* port 2: nothing */
    CHECK_INT(input_take_press(), -1);

    /* synthetic controller events take the same path as real ones */
    SDL_Event e;
    memset(&e, 0, sizeof e); e.type = SDL_CONTROLLERBUTTONDOWN;
    e.cbutton.button = SDL_CONTROLLER_BUTTON_B;       /* right button = RetroPad A */
    SDL_PushEvent(&e);
    CHECK(!input_poll());
    CHECK_INT(input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A), 1);
    CHECK_INT(input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK), 1 << RETRO_DEVICE_ID_JOYPAD_A);
    CHECK_INT(input_take_press(), RETRO_DEVICE_ID_JOYPAD_A);
    CHECK_INT(input_take_press(), -1);
    e.type = SDL_CONTROLLERBUTTONUP; SDL_PushEvent(&e);
    input_poll();
    CHECK_INT(input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A), 0);

    e.type = SDL_CONTROLLERBUTTONDOWN; e.cbutton.button = SDL_CONTROLLER_BUTTON_GUIDE; SDL_PushEvent(&e);
    CHECK(!input_poll());                                   /* F down: nothing yet (F + volume = brightness) */
    e.type = SDL_CONTROLLERBUTTONUP; SDL_PushEvent(&e);
    CHECK(input_poll());                                    /* F released without volume -> menu */

    SDL_Event a;
    memset(&a, 0, sizeof a); a.type = SDL_CONTROLLERAXISMOTION;
    a.caxis.axis = SDL_CONTROLLER_AXIS_LEFTX; a.caxis.value = 20000; SDL_PushEvent(&a);
    a.caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERLEFT; a.caxis.value = 32767; SDL_PushEvent(&a);
    input_poll();
    CHECK_INT(input_state(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X), 20000);
    CHECK_INT(input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2), 1);

    input_set_keyboard(kb);
    SDL_Event k;
    memset(&k, 0, sizeof k); k.type = SDL_KEYDOWN;
    k.key.keysym.sym = SDLK_RETURN; SDL_PushEvent(&k);
    input_poll();
    CHECK_INT(keys, 1);
    TEST_END();
}
