#include "audio.h"
#include "ratecontrol.h"

#define OUT_RATE 48000
#define TARGET (OUT_RATE * 4 * 64 / 1000)   /* 64 ms of S16 stereo */

static SDL_AudioDeviceID dev;
static SDL_AudioStream *stream;
static double core_rate, cur_rate;

static void drain(void) {
    char tmp[8192];
    int n;
    while ((n = SDL_AudioStreamGet(stream, tmp, sizeof tmp)) > 0) SDL_QueueAudio(dev, tmp, n);
}

static int make_stream(double rate) {
    if (stream) { SDL_AudioStreamFlush(stream); drain(); SDL_FreeAudioStream(stream); }
    stream = SDL_NewAudioStream(AUDIO_S16SYS, 2, (int)(rate + 0.5), AUDIO_S16SYS, 2, OUT_RATE);
    cur_rate = rate;
    return stream ? 0 : -1;
}

int audio_open(double rate) {
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return -1;
    SDL_AudioSpec want = {.freq = OUT_RATE, .format = AUDIO_S16SYS, .channels = 2, .samples = 512}, have;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) return -1;
    core_rate = rate;
    if (make_stream(rate)) return -1;
    SDL_PauseAudioDevice(dev, 0);
    return 0;
}

size_t audio_push(const int16_t *s, size_t frames) {
    if (!dev || !stream) return frames;
    double want = core_rate * rate_adjust(SDL_GetQueuedAudioSize(dev), TARGET);
    if (want > cur_rate * 1.001 || want < cur_rate * 0.999) make_stream(want);
    SDL_AudioStreamPut(stream, s, (int)(frames * 4));
    drain();
    return frames;
}

void audio_wait(void) {
    if (!dev) return;
    for (int i = 0; i < 100 && SDL_GetQueuedAudioSize(dev) > TARGET; i++) SDL_Delay(1);
}

void audio_pause(bool on) {
    if (!dev) return;
    SDL_PauseAudioDevice(dev, on);
    if (on) SDL_ClearQueuedAudio(dev);
}

void audio_close(void) {
    if (stream) SDL_FreeAudioStream(stream);
    if (dev) SDL_CloseAudioDevice(dev);
    stream = NULL; dev = 0;
}
