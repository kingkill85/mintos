/* Where the game picture goes on the screen (pure maths, no SDL). */
#pragma once
#include "config.h"

typedef struct { int x, y, w, h; } Rect;

/* screen sw x sh; core picture bw x bh with display aspect (0 = bw/bh) */
Rect viewport(const SystemCfg *s, int sw, int sh, int bw, int bh, float aspect);
