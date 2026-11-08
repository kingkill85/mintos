// uses: frontend/audio.c frontend/ratecontrol.c frontend/core.c frontend/paths.c frontend/archive.c
// libs: $(pkg-config --libs sdl2)
#include <string.h>
#include "check.h"
#include "audio.h"
Uint32 audio_queued(void);
#define SDL_GetQueuedAudioSize_test audio_queued

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

    /* the device only starts once the queue is prefilled (no click/underrun at game start) */
    CHECK_INT(audio_open(48000.0), 0);
    audio_push(buf, 100);
    CHECK_INT(audio_playing(), 0);
    for (int i = 0; i < 20; i++) audio_push(buf, 800);
    CHECK_INT(audio_playing(), 1);
    audio_pause(true);
    CHECK_INT(audio_playing(), 0);
    audio_pause(false);
    audio_push(buf, 100);
    CHECK_INT(audio_playing(), 0);       /* after the menu: prefill again */
    for (int i = 0; i < 20; i++) audio_push(buf, 800);
    CHECK_INT(audio_playing(), 1);
    audio_close();

    /* audio_wait leaves the rate control room up to 2x the target (1.5x: no waiting) */
    CHECK_INT(audio_open(48000.0), 0);
    for (int i = 0; i < 24; i++) audio_push(buf, 192);    /* ~96 ms = 1.5 x 64 ms */
    Uint32 w0 = SDL_GetTicks();
    audio_wait();
    CHECK(SDL_GetTicks() - w0 < 8);
    audio_close();

    /* one huge batch (24 kHz core, 12000 frames -> 24000 output frames) is not cut off */
    CHECK_INT(audio_open(24000.0), 0);
    static int16_t big[12000 * 2];
    audio_push(big, 12000);
    {
        Uint32 q = SDL_GetQueuedAudioSize_test();
        CHECK(q > 23000 * 4);
    }
    audio_close();

    /* the core changes its sample rate (SET_SYSTEM_AV_INFO) */
    CHECK_INT(audio_open(32768.0), 0);
    audio_set_rate(44100.0);
    audio_push(buf, 10);
    CHECK(audio_input_rate() > 43000.0);
    audio_close();

    /* starved queue: the stream must take a LOWER input rate (more output per core sample) */
    CHECK_INT(audio_open(32768.0), 0);
    audio_push(buf, 10);
    SDL_Delay(100);
    audio_push(buf, 10);
    CHECK(audio_input_rate() < 32768.0);
    audio_close();

    /* an empty queue while playing counts as an underrun (logged for the listen test) */
    CHECK_INT(audio_open(48000.0), 0);
    for (int i = 0; i < 10; i++) audio_push(buf, 800);
    int before = audio_underruns();
    SDL_Delay(400);                        /* the dummy device plays the queue empty */
    audio_push(buf, 800);
    CHECK_INT(audio_underruns(), before + 1);
    audio_close();
    TEST_END();
}
