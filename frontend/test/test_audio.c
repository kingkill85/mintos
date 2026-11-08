// uses: frontend/audio.c frontend/ratecontrol.c
// libs: $(pkg-config --libs sdl2)
#include <string.h>
#include "check.h"
#include "audio.h"

static int16_t buf[2048 * 2];

int main(void) {
    SDL_Init(SDL_INIT_AUDIO);
    double rates[] = {32768.0, 44100.0, 48000.0};
    double fps[] = {59.73, 50.0, 57.5};
    for (int r = 0; r < 3; r++) {
        CHECK_INT(audio_open(rates[r]), 0);
        size_t per = (size_t)(rates[r] / fps[r]);
        for (int i = 0; i < 60; i++) CHECK_INT(audio_push(buf, per), per);
        audio_pause(true);
        audio_pause(false);
        audio_close();
    }
    CHECK_INT(audio_push(buf, 10), 10);   /* closed: swallow, no crash */
    TEST_END();
}
