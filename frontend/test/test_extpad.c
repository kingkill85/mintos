// uses: frontend/input.c frontend/inputmap.c common/osd.c common/ui.c
// libs: $(pkg-config --libs sdl2 SDL2_ttf)
/* An external (Bluetooth) controller is used next to the built-in pad, as player 1. */
#include <string.h>
#include "check.h"
#include "input.h"

int main(void) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
    SDL_VirtualJoystickDesc d;
    memset(&d, 0, sizeof d);
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = 6; d.nbuttons = 15;
    d.name = "8BitDo Pro 2";
    int dev = SDL_JoystickAttachVirtualEx(&d);
    CHECK(dev >= 0);
    CHECK(SDL_IsGameController(dev));
    input_open();
    CHECK_INT(input_pads(), 1);                 /* opened although it is not "RG353V Gamepad" */

    /* its buttons reach the core: press A (SDL "B" = right button) on the virtual pad */
    SDL_Joystick *j = SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(dev));
    CHECK(j != NULL);
    SDL_JoystickSetVirtualButton(j, SDL_CONTROLLER_BUTTON_B, 1);
    SDL_JoystickUpdate();
    SDL_GameControllerUpdate();
    input_poll();
    CHECK_INT(input_state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A), 1);

    /* a controller connected while the game runs is picked up too */
    d.name = "Xbox Wireless Controller";
    int dev2 = SDL_JoystickAttachVirtualEx(&d);
    CHECK(dev2 >= 0);
    input_poll();
    CHECK_INT(input_pads(), 2);

    /* switched off: its slot is freed */
    SDL_JoystickDetachVirtual(dev2);
    input_poll();
    CHECK_INT(input_pads(), 1);
    TEST_END();
}
