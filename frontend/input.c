#include "input.h"
#include <stdio.h>
#include <string.h>

/* the built-in pad (rg353-pad's merged uinput device) plus external ones (Bluetooth/USB),
 * all as player 1; the raw built-in devices behind the merged one are left alone */
static SDL_GameController *pads[8];
static int npads;
static uint16_t held;              /* bit per RETRO_DEVICE_ID_JOYPAD_* */
static int16_t axis[4];            /* left x/y, right x/y */
static int presses[16], npress;
static retro_keyboard_event_t keyboard;

static void try_open(int index) {
    if (npads >= 8 || !SDL_IsGameController(index)) return;
    const char *n = SDL_GameControllerNameForIndex(index);
    if (n && (!strcmp(n, "adc-joystick") || !strncmp(n, "gpio-keys", 9) || !strcmp(n, "adc-keys"))) return;
    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
    for (int i = 0; i < npads; i++)
        if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == id) return;   /* already open */
    SDL_GameController *c = SDL_GameControllerOpen(index);
    if (c) { pads[npads++] = c; fprintf(stderr, "controller: %s\n", n ? n : "?"); }
}

static void closed(SDL_JoystickID id) {
    for (int i = 0; i < npads; i++)
        if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == id) {
            SDL_GameControllerClose(pads[i]);
            pads[i] = pads[--npads];
            held = 0;   /* buttons held on the pad that left */
            return;
        }
}

int input_pads(void) { return npads; }

void input_open(void) {
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    for (int i = 0; i < SDL_NumJoysticks(); i++) try_open(i);
}

static void press(int id) {
    if (id < 0) return;
    if (!(held & 1 << id) && npress < 16) presses[npress++] = id;
    held |= 1 << id;
}

/* libretro keycodes equal SDL's for ASCII; map the few non-ASCII ones the cores need */
static unsigned retrok(SDL_Keycode k) {
    if (k < 128) return k;
    if (k >= SDLK_F1 && k <= SDLK_F12) return RETROK_F1 + (k - SDLK_F1);
    switch (k) {
    case SDLK_UP: return RETROK_UP;
    case SDLK_DOWN: return RETROK_DOWN;
    case SDLK_LEFT: return RETROK_LEFT;
    case SDLK_RIGHT: return RETROK_RIGHT;
    default: return RETROK_UNKNOWN;
    }
}

bool input_poll(void) {
    bool menu = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_CONTROLLERDEVICEADDED: try_open(e.cdevice.which); break;
        case SDL_CONTROLLERDEVICEREMOVED: closed(e.cdevice.which); break;
        case SDL_CONTROLLERBUTTONDOWN:
            if (e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE) menu = true;
            else press(map_button(e.cbutton.button));
            break;
        case SDL_CONTROLLERBUTTONUP: {
            int id = map_button(e.cbutton.button);
            if (id >= 0) held &= ~(1 << id);
            break;
        }
        case SDL_CONTROLLERAXISMOTION: {
            int t = map_trigger(e.caxis.axis);
            if (t >= 0) {
                if (e.caxis.value > 16000) press(t); else held &= ~(1 << t);
            } else if (e.caxis.axis <= SDL_CONTROLLER_AXIS_RIGHTY) {
                axis[e.caxis.axis] = e.caxis.value;
            }
            break;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (keyboard && !e.key.repeat)
                keyboard(e.type == SDL_KEYDOWN, retrok(e.key.keysym.sym),
                         e.key.keysym.sym < 128 ? (uint32_t)e.key.keysym.sym : 0, 0);
            break;
        }
    }
    return menu;
}

int input_take_press(void) {
    if (!npress) return -1;
    int id = presses[0];
    memmove(presses, presses + 1, --npress * sizeof *presses);
    return id;
}

void input_clear(void) { held = 0; npress = 0; }

int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0) return 0;
    if ((device & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD)
        return id == RETRO_DEVICE_ID_JOYPAD_MASK ? (int16_t)held : id < 16 ? (held >> id) & 1 : 0;
    if ((device & RETRO_DEVICE_MASK) == RETRO_DEVICE_ANALOG && index < 2 && id < 2)
        return axis[index * 2 + id];
    return 0;
}

void input_set_keyboard(retro_keyboard_event_t cb) { keyboard = cb; }
