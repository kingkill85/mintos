#include "audio.h"
#include "ratecontrol.h"
#include "core.h"   /* rg_log */
#include <stdlib.h>

#define OUT_RATE 48000
#define TARGET (OUT_RATE * 4 * 64 / 1000)   /* 64 ms of S16 stereo */

static SDL_AudioDeviceID dev;
static Resampler rs;
static double core_rate, cur_rate;
static int underruns;
static bool paused, primed, playing;   /* playing: device started after the prefill */
static int16_t outbuf[8192 * 2];

int audio_open(double rate) {
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return -1;
    SDL_AudioSpec want = {.freq = OUT_RATE, .format = AUDIO_S16SYS, .channels = 2, .samples = 512}, have;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) return -1;
    core_rate = cur_rate = rate;
    rs = (Resampler){0};
    playing = false;            /* started in audio_push once TARGET is queued */
    return 0;
}

size_t audio_push(const int16_t *s, size_t frames) {
    if (!dev) return frames;
    Uint32 q = SDL_GetQueuedAudioSize(dev);
    if (q == 0 && playing && !paused) {
        underruns++;
        rg_log("audio underrun %d\n", underruns);
    }
    /* queue low -> rate_adjust > 1 -> treat the core as slower, so each core sample
     * yields more output (and the other way round) */
    cur_rate = core_rate / rate_adjust(q, TARGET);
    size_t n = resample(&rs, s, frames, cur_rate / OUT_RATE, outbuf, sizeof outbuf / 4);
    SDL_QueueAudio(dev, outbuf, (Uint32)(n * 4));
    primed = true;
    if (!playing && !paused && SDL_GetQueuedAudioSize(dev) >= TARGET) {
        SDL_PauseAudioDevice(dev, 0);
        playing = true;
    }
    return frames;
}

void audio_wait(void) {
    if (!dev) return;
    /* up to 2x TARGET: rate_adjust reaches its -0.5 % there, enough for 59.73 Hz on a 60 Hz panel */
    for (int i = 0; i < 100 && SDL_GetQueuedAudioSize(dev) > 2 * TARGET; i++) SDL_Delay(1);
}

void audio_pause(bool on) {
    if (!dev) return;
    paused = on;
    /* pause now; un-pausing waits for the prefill in audio_push */
    SDL_PauseAudioDevice(dev, 1);
    playing = false;
    if (on) { SDL_ClearQueuedAudio(dev); primed = false; }
}

void audio_close(void) {
    if (dev) SDL_CloseAudioDevice(dev);
    dev = 0; primed = playing = false;
}

int audio_underruns(void) { return underruns; }
double audio_input_rate(void) { return cur_rate; }
int audio_playing(void) { return playing; }
