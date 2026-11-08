#include "viewport.h"

static Rect centred(int sw, int sh, int w, int h) {
    return (Rect){(sw - w) / 2, (sh - h) / 2, w, h};
}

/* largest rect with this aspect that fits the screen */
static Rect fit(int sw, int sh, float aspect) {
    int w = sw, h = (int)(sw / aspect + 0.5f);
    if (h > sh) { h = sh; w = (int)(sh * aspect); }
    return centred(sw, sh, w, h);
}

Rect viewport(const SystemCfg *s, int sw, int sh, int bw, int bh, float aspect) {
    if (aspect <= 0) aspect = (float)bw / bh;
    switch (s->scale) {
    case SCALE_FULL:
        return (Rect){0, 0, sw, sh};
    case SCALE_CUSTOM:
        return (Rect){s->cx, s->cy, s->cw, s->ch};
    case SCALE_ASPECT:
        return fit(sw, sh, aspect);
    case SCALE_INTEGER:
    default: {
        int k = sh / bh;
        if (k < 1) return fit(sw, sh, aspect);
        int h = k * bh, w = (int)(h * aspect);
        if (w > sw) return fit(sw, sh, aspect);
        return centred(sw, sh, w, h);
    }
    }
}
