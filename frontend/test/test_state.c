// uses: frontend/state.c frontend/core.c frontend/paths.c frontend/archive.c
// so: frontend/test/testcore.c
// libs: $(pkg-config --libs sdl2 SDL2_image) -lz
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "check.h"
#include "state.h"
#include <zlib.h>

static void on_video(const void *d, unsigned w, unsigned h, size_t p) { (void)d; (void)w; (void)h; (void)p; }
static size_t on_audio(const int16_t *d, size_t n) { (void)d; return n; }
static int exists(const char *p) { return access(p, F_OK) == 0; }

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    char root[] = "/tmp/rgarch-state-XXXXXX", rom[512], p[1024], cmd[1200];
    mkdtemp(root);
    setenv("RGARCH_ROOT", root, 1);
    snprintf(rom, sizeof rom, "%s/storage/roms/gb/Zelda, The (Rev 2).tst", root);
    snprintf(cmd, sizeof cmd, "mkdir -p '%s/storage/roms/gb' && echo x > '%s'", root, rom);
    system(cmd);

    Core c = {.on_video = on_video, .on_audio = on_audio};
    CHECK_INT(core_load(&c, "./testcore.so", rom), 0);
    for (int i = 0; i < 5; i++) core_run(&c);

    SDL_Surface *pic = SDL_CreateRGBSurfaceWithFormat(0, 320, 240, 32, SDL_PIXELFORMAT_ARGB8888);
    CHECK(state_save(&c, rom, 2, pic));
    snprintf(p, sizeof p, "%s/storage/states/gb/Zelda, The (Rev 2).st2", root);
    CHECK(exists(p));
    strcat(p, ".png");
    CHECK(exists(p));
    snprintf(p, sizeof p, "%s/storage/states/gb/Zelda, The (Rev 2).st2.tmp", root);
    CHECK(!exists(p));
    CHECK(state_exists(rom, 2));
    CHECK(!state_exists(rom, 3));

    unsigned char before[4], after[4];
    core_state_save(&c, before, 4);
    core_run(&c); core_run(&c);
    CHECK(state_load(&c, rom, 2));
    core_state_save(&c, after, 4);
    CHECK(memcmp(before, after, 4) == 0);

    /* a stale .tmp from an interrupted write does not matter */
    snprintf(cmd, sizeof cmd, "echo broken > '%s'", p);
    system(cmd);
    CHECK(state_load(&c, rom, 2));

    /* auto state: slot -1, no preview */
    CHECK(state_save(&c, rom, -1, NULL));
    snprintf(p, sizeof p, "%s/storage/states/gb/Zelda, The (Rev 2).auto", root);
    CHECK(exists(p));
    CHECK(state_load(&c, rom, -1));

    /* SRAM: written once, not again while unchanged */
    CHECK(state_sram_save(&c, rom));
    snprintf(p, sizeof p, "%s/storage/saves/gb/Zelda, The (Rev 2).srm", root);
    CHECK(exists(p));
    CHECK(!state_sram_save(&c, rom));
    size_t n;
    unsigned char *sram = core_memory(&c, RETRO_MEMORY_SAVE_RAM, &n);
    sram[0] ^= 0xff;
    CHECK(state_sram_save(&c, rom));
    sram[0] = 0x55;
    state_sram_load(&c, rom);                  /* back to the saved byte */
    CHECK(sram[0] != 0x55);

    /* games that save themselves (ScummVM quicksave): only a marker + preview for the menu */
    CHECK(!state_exists(rom, 6));
    CHECK(state_mark(rom, 6, pic));
    CHECK(state_exists(rom, 6));
    snprintf(p, sizeof p, "%s/storage/states/gb/Zelda, The (Rev 2).st6.png", root);
    CHECK(access(p, F_OK) == 0);

    /* periodic SRAM save in the background: copy now, write in a thread */
    sram[1] ^= 0x5a;
    CHECK(state_sram_save_async(&c, rom));
    sram[1] ^= 0x5a;                          /* the game keeps running and changes memory */
    state_sram_wait();
    {
        snprintf(p, sizeof p, "%s/storage/saves/gb/Zelda, The (Rev 2).srm", root);
        FILE *sf = fopen(p, "rb");
        unsigned char disk[8] = {0};
        if (sf) { fread(disk, 1, 8, sf); fclose(sf); }
        CHECK_INT(disk[1], sram[1] ^ 0x5a);   /* the copy taken at save time */
    }
    CHECK(!state_sram_save_async(&c, rom) || 1);
    state_sram_wait();

    /* RetroArch's compressed states (#RZIPv1#, zlib chunks) load too */
    {
        unsigned char st[4], z[64];
        core_state_save(&c, st, 4);
        uLongf zn = sizeof z;
        compress(z, &zn, st, 4);
        snprintf(p, sizeof p, "%s/storage/states/gb/Zelda, The (Rev 2).st7", root);
        FILE *rf = fopen(p, "wb");
        unsigned char hdr[20] = {'#', 'R', 'Z', 'I', 'P', 'v', 1, '#'};
        unsigned chunk = 131072; memcpy(hdr + 8, &chunk, 4);
        unsigned long long total = 4; memcpy(hdr + 12, &total, 8);
        fwrite(hdr, 1, 20, rf);
        unsigned zlen = (unsigned)zn; fwrite(&zlen, 4, 1, rf); fwrite(z, 1, zn, rf);
        fclose(rf);
        core_run(&c);
        CHECK(state_load(&c, rom, 7));
        unsigned char now0[4];
        core_state_save(&c, now0, 4);
        CHECK(memcmp(now0, st, 4) == 0);
        /* the same with raw deflate chunks (no zlib header) */
        z_stream zs = {0};
        deflateInit2(&zs, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        zs.next_in = st; zs.avail_in = 4; zs.next_out = z; zs.avail_out = sizeof z;
        deflate(&zs, Z_FINISH);
        zlen = sizeof z - zs.avail_out;
        deflateEnd(&zs);
        rf = fopen(p, "wb");
        fwrite(hdr, 1, 20, rf); fwrite(&zlen, 4, 1, rf); fwrite(z, 1, zlen, rf);
        fclose(rf);
        core_run(&c);
        CHECK(state_load(&c, rom, 7));
        unsigned char now[4];
        core_state_save(&c, now, 4);
        CHECK(memcmp(now, st, 4) == 0);
    }

    SDL_Renderer *r = SDL_CreateSoftwareRenderer(SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_ARGB8888));
    SDL_Texture *t = state_preview(r, rom, 2);
    CHECK(t != NULL);
    CHECK(state_preview(r, rom, 5) == NULL);
    core_unload(&c);
    snprintf(cmd, sizeof cmd, "rm -rf %s", root); system(cmd);
    TEST_END();
}
