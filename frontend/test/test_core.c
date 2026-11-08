// uses: frontend/core.c frontend/paths.c frontend/archive.c
// so: frontend/test/testcore.c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"
#include "core.h"

static int frames, last_w, last_h;
static size_t audio_frames;
static void on_video(const void *d, unsigned w, unsigned h, size_t pitch) { (void)d; (void)pitch; frames++; last_w = w; last_h = h; }
static size_t on_audio(const int16_t *d, size_t n) { (void)d; audio_frames += n; return n; }
static int16_t on_input(unsigned p, unsigned d, unsigned i, unsigned id) { (void)p; (void)d; (void)i; (void)id; return 0; }

int main(void) {
    char root[] = "/tmp/rgarch-core-XXXXXX";
    mkdtemp(root);
    setenv("RGARCH_ROOT", root, 1);
    char rom[1024], cmd[2048], opt[1024];
    snprintf(rom, sizeof rom, "%s/storage/roms/test/game.tst", root);
    snprintf(cmd, sizeof cmd, "mkdir -p %s/storage/roms/test && echo x > %s", root, rom);
    system(cmd);

    Core c = {.on_video = on_video, .on_audio = on_audio, .on_input = on_input};
    CHECK_INT(core_load(&c, "./testcore.so", rom), 0);
    CHECK_STR(c.info.library_name, "testcore");
    /* the save directory exists before the core writes its own files there */
    snprintf(opt, sizeof opt, "%s/storage/saves/test", root);
    CHECK(access(opt, F_OK) == 0);
    CHECK_INT(c.fmt, RETRO_PIXEL_FORMAT_XRGB8888);
    CHECK(c.can_state);
    CHECK_INT((int)c.av.geometry.base_width, 160);
    for (int i = 0; i < 10; i++) core_run(&c);
    CHECK_INT(frames, 10); CHECK_INT(last_w, 160); CHECK_INT(last_h, 144);
    CHECK_INT(audio_frames, 8000);

    /* savestate round trip */
    size_t n = core_state_size(&c);
    CHECK_INT(n, 4);
    unsigned char buf[16];
    CHECK(core_state_save(&c, buf, n));
    core_run(&c); core_run(&c);
    unsigned char after[16];
    core_state_save(&c, after, n);
    CHECK(memcmp(buf, after, n) != 0);
    CHECK(core_state_load(&c, buf, n));
    core_state_save(&c, after, n);
    CHECK(memcmp(buf, after, n) == 0);

    size_t sz = 0;
    CHECK(core_memory(&c, RETRO_MEMORY_SAVE_RAM, &sz) != NULL);
    CHECK_INT(sz, 8);
    core_unload(&c);

    /* options file written with the default value */
    snprintf(opt, sizeof opt, "%s/storage/.config/retroarch/config/testcore/testcore.opt", root);
    FILE *f = fopen(opt, "r");
    CHECK(f != NULL);
    char line[256] = "";
    if (f) { fgets(line, sizeof line, f); fclose(f); }
    CHECK_STR(line, "test_colour = \"red\"\n");

    /* a value from the .opt file is used on the next load */
    f = fopen(opt, "w"); fputs("test_colour = \"blue\"\n", f); fclose(f);
    Core c2 = {.on_video = on_video, .on_audio = on_audio, .on_input = on_input};
    CHECK_INT(core_load(&c2, "./testcore.so", rom), 0);
    CHECK_STR(core_option(&c2, "test_colour"), "blue");
    core_unload(&c2);

    /* zipped ROM for a core without zip support: extracted and loaded */
    char zcmd[2048], zrom[1024];
    snprintf(zrom, sizeof zrom, "%s/storage/roms/test/game.zip", root);
    snprintf(zcmd, sizeof zcmd, "cd %s/storage/roms/test && zip -q game.zip game.tst", root);
    CHECK_INT(system(zcmd), 0);
    setenv("TMPDIR", root, 1);
    Core c4 = {.on_video = on_video, .on_audio = on_audio, .on_input = on_input};
    CHECK_INT(core_load(&c4, "./testcore.so", zrom), 0);
    core_unload(&c4);

    Core c3 = {0};
    CHECK_INT(core_load(&c3, "./missing.so", rom), -1);
    snprintf(cmd, sizeof cmd, "rm -rf %s", root); system(cmd);
    TEST_END();
}
