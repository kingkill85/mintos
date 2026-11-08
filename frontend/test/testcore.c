/* Minimal libretro core for rgarch tests: 160x144 XRGB8888 solid frames whose colour
 * depends on a frame counter and the option test_colour (red|blue), 800 stereo samples
 * per frame at 48 kHz, 8 bytes SRAM, serialize = the 4-byte counter. */
#include <string.h>
#include <stdint.h>
#include "libretro.h"

static retro_environment_t env;
static retro_video_refresh_t video;
static retro_audio_sample_batch_t audio;
static retro_audio_sample_t audio1;
static unsigned disk_index, disk_ejected;
static bool d_set_eject(bool e) { disk_ejected = e; return true; }
static bool d_get_eject(void) { return disk_ejected; }
static unsigned d_get_index(void) { return disk_index; }
static bool d_set_index(unsigned i) { if (!disk_ejected || i > 1) return false; disk_index = i; return true; }
static unsigned d_get_num(void) { return 2; }
static bool d_replace(unsigned i, const struct retro_game_info *g) { (void)i; (void)g; return false; }
static bool d_add(void) { return false; }
static retro_input_poll_t poll;
static uint32_t counter, frame[160 * 144];
static uint8_t sram[8];
static int16_t samples[800 * 2];

void retro_set_environment(retro_environment_t cb) {
    env = cb;
    static struct retro_core_option_v2_definition defs[] = {
        {"test_colour", "Colour", NULL, NULL, NULL, NULL, {{"red", NULL}, {"blue", NULL}, {NULL, NULL}}, "red"},
        {"test_single", "Single samples", NULL, NULL, NULL, NULL, {{"no", NULL}, {"yes", NULL}, {NULL, NULL}}, "no"},
        {NULL, NULL, NULL, NULL, NULL, NULL, {{NULL, NULL}}, NULL}};
    static struct retro_core_options_v2 opts = {NULL, defs};
    env(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &opts);
    static struct retro_disk_control_callback dc = {d_set_eject, d_get_eject, d_get_index, d_set_index, d_get_num, d_replace, d_add};
    env(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &dc);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio1 = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll = cb; }
void retro_set_input_state(retro_input_state_t cb) { (void)cb; }
void retro_init(void) {}
void retro_deinit(void) {}
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_get_system_info(struct retro_system_info *i) {
    memset(i, 0, sizeof *i);
    i->library_name = "testcore"; i->library_version = "1"; i->valid_extensions = "tst";
}
void retro_get_system_av_info(struct retro_system_av_info *i) {
    memset(i, 0, sizeof *i);
    i->geometry.base_width = 160; i->geometry.base_height = 144;
    i->geometry.max_width = 160; i->geometry.max_height = 144; i->geometry.aspect_ratio = 10.0f / 9.0f;
    i->timing.fps = 60.0; i->timing.sample_rate = 48000.0;
}
void retro_set_controller_port_device(unsigned p, unsigned d) { (void)p; (void)d; }
void retro_reset(void) { counter = 0; }
void retro_run(void) {
    poll();
    struct retro_variable v = {"test_colour", NULL};
    int blue = env(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value && !strcmp(v.value, "blue");
    uint32_t c = (blue ? 0x0000ff : 0xff0000) | (counter & 0xff) << 8;
    for (int i = 0; i < 160 * 144; i++) frame[i] = c;
    video(frame, 160, 144, 160 * 4);
    struct retro_variable sv = {"test_single", NULL};
    if (env(RETRO_ENVIRONMENT_GET_VARIABLE, &sv) && sv.value && !strcmp(sv.value, "yes"))
        for (int i = 0; i < 800; i++) audio1(0, 0);
    else
        audio(samples, 800);
    counter++;
}
size_t retro_serialize_size(void) { return 4; }
bool retro_serialize(void *d, size_t n) { if (n < 4) return false; memcpy(d, &counter, 4); return true; }
bool retro_unserialize(const void *d, size_t n) { if (n < 4) return false; memcpy(&counter, d, 4); return true; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned i, bool e, const char *c) { (void)i; (void)e; (void)c; }
bool retro_load_game(const struct retro_game_info *g) {
    /* like Genesis Plus GX for cartridges: drop the disk interface again with NULL */
    if (g && g->path && strstr(g->path, "nodisk")) env(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, NULL);
    /* like real cores without zip support: archive bytes are not a ROM */
    if (g && g->data && g->size >= 2 && !memcmp(g->data, "PK", 2)) return false;
    enum retro_pixel_format f = RETRO_PIXEL_FORMAT_XRGB8888;
    return env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &f);
}
bool retro_load_game_special(unsigned t, const struct retro_game_info *g, size_t n) { (void)t; (void)g; (void)n; return false; }
void retro_unload_game(void) {}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
void *retro_get_memory_data(unsigned id) { return id == RETRO_MEMORY_SAVE_RAM ? sram : NULL; }
size_t retro_get_memory_size(unsigned id) { return id == RETRO_MEMORY_SAVE_RAM ? sizeof sram : 0; }
