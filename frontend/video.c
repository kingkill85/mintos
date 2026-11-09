#include "video.h"
#include <math.h>
#include "viewport.h"
#include <SDL_image.h>
#include <string.h>
#include <stdlib.h>

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *frame, *sharp, *overlay, *shade;   /* shade: drawn frame gradient */
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
    if (shade) SDL_DestroyTexture(shade);
    frame = sharp = overlay = shade = NULL;
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

/* CRT look: a dark line under every game row (the gap between a CRT's lines). Even lines
 * need a whole number of screen rows per game row, so the picture is scaled vertically by
 * an integer (SNES 224 rows: 2x = 448, 16 px border top and bottom) and the width by the
 * same factor, keeping the proportions (SNES full screen: 597x448, still 4:3). */
#define SCANLINE_ALPHA 90

static SDL_FRect scanline_rect(SDL_FRect f) {
    int k = fh > 0 ? (int)(f.h / fh + 0.01f) : 0;
    if (k < 2) return f;   /* below 2 rows per game row there is no room for lines */
    float h = (float)k * fh, w = floorf(f.w * h / f.h + 0.5f);
    f.x = floorf(f.x + (f.w - w) / 2);
    f.y = floorf(f.y + (f.h - h) / 2);
    f.w = w;
    f.h = h;
    return f;
}

static void scanlines(const SDL_FRect *f) {
    int k = fh > 0 ? (int)(f->h / fh + 0.01f) : 0;
    if (k < 2) return;
    int t = (k + 1) / 3;   /* 2-3 rows: 1 dark, 4-5: 2 */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, SCANLINE_ALPHA);
    for (int i = 0; i < fh; i++) {
        SDL_FRect r = {f->x, f->y + (float)(i + 1) * k - t, f->w, (float)t};
        SDL_RenderFillRectF(ren, &r);
    }
}

/* the picture's rectangle as drawn (exact scale, scanlines' integer height) */
static SDL_FRect picture_rect(const SystemCfg *s, const SDL_Rect *dst) {
    SDL_FRect f = video_exact_rect(dst, fw, fh);
    if (s->filter == FILTER_SCANLINES) f = scanline_rect(f);
    return f;
}

/* drawn frame (config_border): the border gets the style's colour with light from the top,
 * an edge line around the picture, an optional accent line and the SNES button dots */
static void set_rgb(unsigned c, Uint8 a) { SDL_SetRenderDrawColor(ren, c >> 16 & 255, c >> 8 & 255, c & 255, a); }

static void border_back(const BorderStyle *b) {
    set_rgb(b->base, 255);
    SDL_RenderClear(ren);
    if (!shade) {   /* 1x64 gradient: white 10 % at the top to black 18 % at the bottom */
        shade = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, 64);
        if (!shade) return;
        Uint32 px[64];
        for (int i = 0; i < 64; i++) {
            float t = i / 63.0f, a = t < 0.36f ? 0.10f * (1 - t / 0.36f) : 0.18f * (t - 0.36f) / 0.64f;
            px[i] = (Uint32)(a * 255) << 24 | (t < 0.36f ? 0xffffff : 0);
        }
        SDL_UpdateTexture(shade, NULL, px, sizeof px[0]);
        SDL_SetTextureBlendMode(shade, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(shade, SDL_ScaleModeLinear);
    }
    SDL_RenderCopy(ren, shade, NULL, NULL);
}

static void ring(const SDL_FRect *f, int from, int to) {
    for (int i = from; i <= to; i++) {
        SDL_FRect r = {f->x - i, f->y - i, f->w + 2 * i, f->h + 2 * i};
        SDL_RenderDrawRectF(ren, &r);
    }
}

static void dot(float cx, float cy, float r, unsigned c) {
    set_rgb(c, 255);
    for (float y = -r; y <= r; y += 1) {
        float hw = sqrtf(r * r - y * y);
        SDL_RenderDrawLineF(ren, cx - hw, cy + y, cx + hw, cy + y);
    }
}

static void border_edge(const BorderStyle *b, const SDL_FRect *f) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    set_rgb(b->edge, 255);
    ring(f, 1, 3);
    float room = f->x < f->y ? f->x : f->y;
    if (b->accent && room >= 9) { set_rgb(b->accent, 255); ring(f, 6, 7); }
    if (b->dots && f->x >= 14) {   /* the four button colours, centred in the left border */
        float cx = f->x / 2, cy = f->y + f->h / 2, d = 5;
        dot(cx + d, cy, 3, 0xe4202b); dot(cx, cy + d, 3, 0xf6c400);
        dot(cx - d, cy, 3, 0x1e9a46); dot(cx, cy - d, 3, 0x2b6bd6);
    }
}

static void draw_frame(const SystemCfg *s, float aspect, const SDL_Rect *dst) {
    SDL_FRect f = picture_rect(s, dst);
    if (s->filter == FILTER_SHARP || s->filter == FILTER_SCANLINES) {
        SDL_RenderCopyF(ren, sharpened(&(Rect){(int)f.x, (int)f.y, (int)f.w, (int)f.h}), NULL, &f);
        if (s->filter == FILTER_SCANLINES) scanlines(&f);
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
    SDL_Rect pic = {0, 0, 0, 0};   /* the game picture on screen, for the overlay opacity */
    if (frame) {
        Rect r = viewport(s, sw, sh, fw, fh, aspect);
        SDL_Rect dst = {r.x, r.y, r.w, r.h};
        SDL_FRect f = picture_rect(s, &dst);
        pic = (SDL_Rect){(int)f.x, (int)f.y, (int)f.w, (int)f.h};
        /* a drawn frame needs a border of at least 8 px somewhere */
        const BorderStyle *bs = config_border(s->border);
        if (bs && f.x < 8 && f.y < 8) bs = NULL;   /* no room for a frame */
        if (bs) border_back(bs);
        draw_frame(s, aspect, &dst);
        if (bs) border_edge(bs, &f);
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
        int one = abs(ow - sw) <= 2 && abs(oh - sh) <= 2;
        SDL_Rect o = {0, 0, ow, oh};
        SDL_SetTextureAlphaMod(overlay, 255);
        int weak = s->opacity > 0 && s->opacity < 100;
        if ((!weak && !s->cut) || pic.w <= 0) {
            SDL_RenderCopy(ren, overlay, NULL, one ? &o : NULL);
        } else {
            /* overlay_opacity weakens only the LCD grid over the picture; the frame around it
             * stays solid (a see-through frame showed its light inner edge as a grey ring) */
            int W = one ? ow : sw, H = one ? oh : sh, c = s->cut;   /* overlay_cut: ring left out */
            SDL_Rect in = {pic.x - c, pic.y - c, pic.w + 2 * c, pic.h + 2 * c};
            SDL_Rect part[5] = {
                {0, 0, W, in.y}, {0, in.y + in.h, W, H - in.y - in.h},
                {0, in.y, in.x, in.h}, {in.x + in.w, in.y, W - in.x - in.w, in.h}, pic};
            for (int i = 0; i < 5; i++) {
                SDL_Rect d = part[i], src = d;
                if (d.w <= 0 || d.h <= 0) continue;
                if (!one) src = (SDL_Rect){d.x * ow / sw, d.y * oh / sh, d.w * ow / sw, d.h * oh / sh};
                if (i == 4) SDL_SetTextureAlphaMod(overlay, weak ? s->opacity * 255 / 100 : 255);
                SDL_RenderCopy(ren, overlay, &src, &d);
            }
            SDL_SetTextureAlphaMod(overlay, 255);
        }
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
