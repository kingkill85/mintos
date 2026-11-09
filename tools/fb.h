/* What the panel shows: the framebuffer on the primary plane (Esmart0-win0), found through
 * /sys/kernel/debug/dri/display-subsystem/state (debugfs is mounted if needed), mapped read-only. */
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct { int fd, dfd; const uint8_t *px; size_t size; unsigned w, h, pitch, fmt; } Fb;

int fb_open(Fb *fb);   /* 0 on success */
void fb_close(Fb *fb);
/* XRGB8888 pixel at (x, y) as r, g, b */
static inline void fb_rgb(const Fb *fb, unsigned x, unsigned y, uint8_t rgb[3]) {
    const uint8_t *p = fb->px + (size_t)y * fb->pitch + x * 4;
    rgb[0] = p[2]; rgb[1] = p[1]; rgb[2] = p[0];
}
