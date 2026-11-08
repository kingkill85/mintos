/* Gamepad -> RetroPad state for the core, F = menu, keyboard passthrough (ScummVM, MI2 helper). */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "libretro.h"
#include "inputmap.h"

#define PAD_NAME "RG353V Gamepad"

void input_open(void);
bool input_poll(void);            /* pumps events; true when F (GUIDE) was pressed */
int  input_take_press(void);      /* next newly pressed RetroPad button (menu), -1 if none */
void input_clear(void);           /* forget held buttons and queued presses (menu open/close) */
int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id);
void input_set_keyboard(retro_keyboard_event_t cb);
