// uses: frontend/video.c frontend/viewport.c
// libs: $(pkg-config --libs sdl2 SDL2_image)
#include <string.h>
#include <stdint.h>
#include "check.h"
#include "video.h"

int main(void) {
    Ui ui = {0};
    CHECK_INT(video_open(&ui), 0);
    CHECK(ui.ren != NULL);
    CHECK_INT(ui.w > 0, 1);
    static uint32_t px32[256 * 224];
    static uint16_t px16[256 * 224];
    for (int i = 0; i < 256 * 224; i++) { px32[i] = 0x00ff0000; px16[i] = 0xf800; }
    SystemCfg s = {0};
    s.scale = SCALE_FULL; s.filter = FILTER_SHARP;
    enum retro_pixel_format fmts[] = {RETRO_PIXEL_FORMAT_XRGB8888, RETRO_PIXEL_FORMAT_RGB565, RETRO_PIXEL_FORMAT_0RGB1555};
    for (int i = 0; i < 3; i++) {
        if (fmts[i] == RETRO_PIXEL_FORMAT_XRGB8888) video_frame(px32, 256, 224, 256 * 4, fmts[i]);
        else video_frame(px16, 256, 224, 256 * 2, fmts[i]);
        video_draw(&s, 4.0f / 3.0f, false);
        video_flip();
        CHECK_STR(SDL_GetError(), "");
    }
    /* XRGB8888 with X = 0 (as cores write it) must not come out transparent/black */
    video_frame(px32, 256, 224, 256 * 4, RETRO_PIXEL_FORMAT_XRGB8888);
    SDL_Surface *red = video_snapshot(16, 16);
    CHECK(red != NULL);
    if (red) {
        Uint32 v = ((Uint32 *)red->pixels)[8 * 16 + 8];
        CHECK_INT((v >> 16) & 0xff, 0xff);
        SDL_FreeSurface(red);
    }

    /* geometry change mid-game: a wider frame must replace the texture */
    video_frame(px32, 128, 112, 128 * 4, RETRO_PIXEL_FORMAT_XRGB8888);
    s.filter = FILTER_NEAREST; s.scale = SCALE_INTEGER;
    video_draw(&s, 0, true);
    video_flip();
    SDL_Surface *snap = video_snapshot(320, 240);
    CHECK(snap != NULL);
    if (snap) { CHECK_INT(snap->w, 320); CHECK_INT(snap->h, 240); SDL_FreeSurface(snap); }
    /* NULL data = duplicate frame: keep the old one, no crash */
    video_frame(NULL, 128, 112, 0, RETRO_PIXEL_FORMAT_XRGB8888);
    video_draw(&s, 0, false);
    video_close();
    TEST_END();
}
