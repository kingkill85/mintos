/* Game picture on the panel: SDL renderer (KMSDRM/GLES2), scaling, sharp filter, overlay. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <SDL.h>
#include "libretro.h"
#include "config.h"
#include "ui.h"

int  video_open(Ui *ui);    /* fullscreen window + renderer into ui->ren/w/h; 0 ok */
void video_close(void);
/* new frame from the core; data NULL = keep the previous frame (dupe) */
void video_frame(const void *data, unsigned w, unsigned h, size_t pitch, enum retro_pixel_format fmt);
/* draws the last frame per s (and s->overlay); dim darkens it for the menu. No present. */
void video_draw(const SystemCfg *s, float aspect, bool dim);
void video_flip(void);
/* the current picture (without overlay) scaled to w x h, caller frees; NULL if none */
SDL_Surface *video_snapshot(int w, int h);
