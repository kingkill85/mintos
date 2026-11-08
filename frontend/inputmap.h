/* SDL GameController -> libretro RetroPad (Nintendo layout), pure table. */
#pragma once
#include <SDL.h>
#include "libretro.h"

int map_button(SDL_GameControllerButton b);  /* RETRO_DEVICE_ID_JOYPAD_* or -1 (GUIDE = menu) */
int map_trigger(SDL_GameControllerAxis a);   /* JOYPAD_L2/R2 for the trigger axes, else -1 */
