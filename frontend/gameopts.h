/* The player's per-system picture options ("Spiel-Optionen"), stored in user.cfg over
 * systems.cfg. Choice 0 is always "Standard" = the shipped value from systems.cfg. */
#pragma once
#include <stdbool.h>
#include "config.h"

typedef enum { GO_SCALE, GO_FILTER, GO_FRAME, GO_N } GameOpt;

const char *gameopt_label(GameOpt o);
int gameopt_count(GameOpt o);
const char *gameopt_choice(GameOpt o, int i);
bool gameopt_has_frame(const char *syscfg, const char *system);   /* systems.cfg sets an overlay */
int gameopt_get(const char *user, const char *system, GameOpt o);
int gameopt_set(const char *user, const char *system, GameOpt o, int i);
