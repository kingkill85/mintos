#include "video.h"
#include <math.h>
#include "viewport.h"
#include <SDL_image.h>
#include <string.h>
#include <stdlib.h>

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *frame, *sharp, *overlay;
static int fw, fh, ffmt = -1, sharp_w, sharp_h, sw, sh;
static char overlay_path[256];

int video_open(Ui *ui) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) return -1;
    SDL_ShowCursor(SDL_DISABLE);
    win = SDL_CreateWindow("rgarch", 0, 0, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) return -1;
    SDL_GetWindowSize(win, &sw, &sh);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_TARGETTEXTURE);
    if (!ren) return -1;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    ui->ren = ren; ui->w = sw; ui->h = sh;
    return 0;
}

void video_close(void) {
    if (frame) SDL_DestroyTexture(frame);
    if (sharp) SDL_DestroyTexture(sharp);
    if (overlay) SDL_DestroyTexture(overlay);
    frame = sharp = overlay = NULL;
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    ren = NULL; win = NULL; ffmt = -1;
}

static Uint32 sdl_format(enum retro_pixel_format f) {
    /* XRGB: no alpha channel, X may be 0 */
    return f == RETRO_PIXEL_FORMAT_XRGB8888 ? SDL_PIXELFORMAT_RGB888
         : f == RETRO_PIXEL_FORMAT_RGB565 ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_RGB555;
}

void video_frame(const void *data, unsigned w, unsigned h, size_t pitch, enum retro_pixel_format fmt) {
    if (!data || !ren) return;
    if (!frame || (int)w != fw || (int)h != fh || (int)fmt != ffmt) {
        if (frame) SDL_DestroyTexture(frame);
        frame = SDL_CreateTexture(ren, sdl_format(fmt), SDL_TEXTUREACCESS_STREAMING, w, h);
        fw = w; fh = h; ffmt = fmt;
    }
    SDL_UpdateTexture(frame, NULL, data, (int)pitch);
}

/* sharp: nearest-upscale by an integer factor into a target, then linear to the dest rect */
static SDL_Texture *sharpened(const Rect *d) {
    int k = (d->w + fw - 1) / fw;
    int ky = (d->h + fh - 1) / fh;
    if (ky > k) k = ky;
    if (k < 1) k = 1;
    if (!sharp || sharp_w != fw * k || sharp_h != fh * k) {
        if (sharp) SDL_DestroyTexture(sharp);
        sharp = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, fw * k, fh * k);
        sharp_w = fw * k; sharp_h = fh * k;
        SDL_SetTextureScaleMode(sharp, SDL_ScaleModeLinear);
    }
    SDL_SetTextureScaleMode(frame, SDL_ScaleModeNearest);
    SDL_SetRenderTarget(ren, sharp);
    SDL_RenderCopy(ren, frame, NULL, NULL);
    SDL_SetRenderTarget(ren, NULL);
    return sharp;
}

/* A rect within a pixel of a uniform scale gets that scale exactly, as a fraction: GBA
 * 240x160 in 640x427 is 8/3 = 426.67 high. Rounded to 427, one row in the middle got the
 * wrong height and the LCD grid of the frame (Perfect_GBA) no longer lined up there. */
SDL_FRect video_exact_rect(const SDL_Rect *d, int fw, int fh) {
    SDL_FRect f = {d->x, d->y, d->w, d->h};
    if (fw <= 0 || fh <= 0) return f;
    float hx = (float)fh * d->w / fw, wy = (float)fw * d->h / fh;
    if (fabsf(hx - d->h) < 1.0f && hx != d->h) f.h = hx;
    else if (fabsf(wy - d->w) < 1.0f && wy != d->w) f.w = wy;
    return f;
}

static void draw_frame(const SystemCfg *s, float aspect, const SDL_Rect *dst) {
    SDL_FRect f = video_exact_rect(dst, fw, fh);
    if (s->filter == FILTER_SHARP) {
        SDL_RenderCopyF(ren, sharpened(&(Rect){dst->x, dst->y, dst->w, dst->h}), NULL, &f);
    } else {
        SDL_SetTextureScaleMode(frame, s->filter == FILTER_LINEAR ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
        SDL_RenderCopyF(ren, frame, NULL, &f);
    }
    (void)aspect;
}

void video_draw(const SystemCfg *s, float aspect, bool dim) {
    if (!ren) return;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    if (frame) {
        Rect r = viewport(s, sw, sh, fw, fh, aspect);
        SDL_Rect dst = {r.x, r.y, r.w, r.h};
        draw_frame(s, aspect, &dst);
    }
    if (strcmp(overlay_path, s->overlay)) {
        if (overlay) SDL_DestroyTexture(overlay);
        overlay = *s->overlay ? IMG_LoadTexture(ren, s->overlay) : NULL;
        snprintf(overlay_path, sizeof overlay_path, "%s", s->overlay);
    }
    if (overlay) {
        /* a frame a few pixels off the screen size is drawn 1:1, not stretched: Jeltron_GB-Color
         * is 640x479, stretched to 480 one row doubled and its LCD grid slipped off the game */
        int ow, oh;
        SDL_QueryTexture(overlay, NULL, NULL, &ow, &oh);
        SDL_Rect o = {0, 0, ow, oh};
        SDL_RenderCopy(ren, overlay, NULL, abs(ow - sw) <= 2 && abs(oh - sh) <= 2 ? &o : NULL);
    }
    if (dim) {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 170);
        SDL_RenderFillRect(ren, NULL);
    }
}

void video_flip(void) { if (ren) SDL_RenderPresent(ren); }

SDL_Surface *video_snapshot(int w, int h) {
    if (!frame) return NULL;
    SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!t || !out) { if (t) SDL_DestroyTexture(t); if (out) SDL_FreeSurface(out); return NULL; }
    SDL_SetTextureScaleMode(frame, SDL_ScaleModeLinear);
    SDL_SetRenderTarget(ren, t);
    SDL_RenderCopy(ren, frame, NULL, NULL);
    SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, out->pixels, out->pitch);
    SDL_SetRenderTarget(ren, NULL);
    SDL_DestroyTexture(t);
    return out;
}
