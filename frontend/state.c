#include "state.h"
#include "paths.h"
#include <SDL_image.h>
#include <zlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

static void *saved[2];          /* copies of SRAM/RTC as last written, to detect changes */
static size_t saved_n[2];
static const unsigned mem_id[2] = {RETRO_MEMORY_SAVE_RAM, RETRO_MEMORY_RTC};
static const PathKind mem_path[2] = {P_SRAM, P_RTC};

static bool write_atomic(const char *path, const void *data, size_t n) {
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (mkdirs_for(path)) return false;
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    bool ok = write(fd, data, n) == (ssize_t)n && fsync(fd) == 0;
    close(fd);
    if (ok) ok = rename(tmp, path) == 0;
    if (!ok) { unlink(tmp); rg_log("cannot write %s\n", path); }
    return ok;
}

static void *read_all(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    void *b = len > 0 ? malloc(len) : NULL;
    *n = b ? fread(b, 1, len, f) : 0;
    fclose(f);
    return b;
}

static void remember(int i, const void *d, size_t n) {
    free(saved[i]);
    saved[i] = malloc(n);
    memcpy(saved[i], d, n);
    saved_n[i] = n;
}

void state_sram_load(Core *c, const char *rom) {
    for (int i = 0; i < 2; i++) {
        size_t n, fn;
        void *mem = core_memory(c, mem_id[i], &n);
        if (!mem) continue;
        char p[1024];
        if (path_for(mem_path[i], rom, 0, p, sizeof p)) continue;
        void *d = read_all(p, &fn);
        if (d) memcpy(mem, d, fn < n ? fn : n);
        free(d);
        remember(i, mem, n);
    }
}

bool state_sram_save(Core *c, const char *rom) {
    bool wrote = false;
    for (int i = 0; i < 2; i++) {
        size_t n;
        void *mem = core_memory(c, mem_id[i], &n);
        char p[1024];
        if (!mem || path_for(mem_path[i], rom, 0, p, sizeof p)) continue;
        /* write when changed, or when there is no file yet (like RetroArch) */
        if (saved[i] && saved_n[i] == n && !memcmp(saved[i], mem, n) && access(p, F_OK) == 0) continue;
        if (!write_atomic(p, mem, n)) continue;
        remember(i, mem, n);
        wrote = true;
    }
    return wrote;
}

static void save_preview(const char *rom, int slot, SDL_Surface *preview);

static int state_path(const char *rom, int slot, char *p, size_t n) {
    return slot < 0 ? path_for(P_AUTO, rom, 0, p, n) : path_for(P_STATE, rom, slot, p, n);
}

bool state_save(Core *c, const char *rom, int slot, SDL_Surface *preview) {
    char p[1024];
    size_t n = core_state_size(c);
    if (!n || state_path(rom, slot, p, sizeof p)) return false;
    void *b = malloc(n);
    bool ok = b && core_state_save(c, b, n) && write_atomic(p, b, n);
    free(b);
    if (ok) save_preview(rom, slot, preview);
    rg_log("save state %d: %s\n", slot, ok ? "ok" : "failed");
    return ok;
}

static void save_preview(const char *rom, int slot, SDL_Surface *preview) {
    char pp[1024], tmp[1100];
    if (!preview || slot < 0 || path_for(P_PREVIEW, rom, slot, pp, sizeof pp)) return;
    snprintf(tmp, sizeof tmp, "%s.tmp", pp);
    if (IMG_SavePNG(preview, tmp) == 0) rename(tmp, pp);
}

bool state_mark(const char *rom, int slot, SDL_Surface *preview) {
    char p[1024];
    if (slot < 0 || state_path(rom, slot, p, sizeof p)) return false;
    static const char note[] = "saved by the game itself (ScummVM slot)\n";
    if (!write_atomic(p, note, sizeof note - 1)) return false;
    save_preview(rom, slot, preview);
    return true;
}

/* RetroArch's compressed state: "#RZIPv1#", u32 chunk size, u64 total, then
 * (u32 length + zlib data) per chunk. Returns the plain bytes or NULL. */
static void *unrzip(const unsigned char *d, size_t n, size_t *out_n) {
    if (n < 20 || memcmp(d, "#RZIPv", 6)) return NULL;
    uint64_t total;
    memcpy(&total, d + 12, 8);
    unsigned char *out = malloc(total ? total : 1);
    size_t pos = 20, got = 0;
    while (out && pos + 4 <= n && got < total) {
        uint32_t len;
        memcpy(&len, d + pos, 4);
        pos += 4;
        if (pos + len > n) break;
        uLongf room = total - got;
        if (uncompress(out + got, &room, d + pos, len) != Z_OK) {
            /* some RetroArch versions write raw deflate without the zlib header */
            z_stream zs = {0};
            zs.next_in = (Bytef *)(d + pos); zs.avail_in = len;
            zs.next_out = out + got; zs.avail_out = total - got;
            int ok = inflateInit2(&zs, -15) == Z_OK && inflate(&zs, Z_FINISH) == Z_STREAM_END;
            room = (total - got) - zs.avail_out;
            inflateEnd(&zs);
            if (!ok) break;
        }
        got += room;
        pos += len;
    }
    if (!out || got != total) { free(out); return NULL; }
    *out_n = got;
    return out;
}

bool state_load(Core *c, const char *rom, int slot) {
    char p[1024];
    size_t n;
    if (state_path(rom, slot, p, sizeof p)) return false;
    void *b = read_all(p, &n);
    if (b && n >= 6 && !memcmp(b, "#RZIPv", 6)) {   /* written by RetroArch with compression */
        size_t pn;
        void *plain = unrzip(b, n, &pn);
        free(b);
        b = plain;
        n = plain ? pn : 0;
    }
    bool ok = b && core_state_load(c, b, n);
    free(b);
    rg_log("load state %d: %s\n", slot, ok ? "ok" : "failed");
    return ok;
}

bool state_exists(const char *rom, int slot) {
    char p[1024];
    return !state_path(rom, slot, p, sizeof p) && access(p, F_OK) == 0;
}

SDL_Texture *state_preview(SDL_Renderer *r, const char *rom, int slot) {
    char p[1024];
    if (path_for(P_PREVIEW, rom, slot, p, sizeof p) || access(p, F_OK)) return NULL;
    return IMG_LoadTexture(r, p);
}

/* ---------- background SRAM write ---------- */
typedef struct { char path[1024]; void *data; size_t n; } Job;
static Job jobs[2];
static int njobs;
static SDL_Thread *writer;
static SDL_atomic_t busy;   /* 1 while the thread writes */

static int write_jobs(void *arg) {
    (void)arg;
    for (int i = 0; i < njobs; i++) { write_atomic(jobs[i].path, jobs[i].data, jobs[i].n); free(jobs[i].data); }
    SDL_AtomicSet(&busy, 0);
    return 0;
}

void state_sram_wait(void) {
    if (writer) { SDL_WaitThread(writer, NULL); writer = NULL; }
}

bool state_sram_save_async(Core *c, const char *rom) {
    if (writer) {
        if (SDL_AtomicGet(&busy)) return false;   /* still writing: next time */
        state_sram_wait();
    }
    njobs = 0;
    for (int i = 0; i < 2; i++) {
        size_t n;
        void *mem = core_memory(c, mem_id[i], &n);
        char p[1024];
        if (!mem || path_for(mem_path[i], rom, 0, p, sizeof p)) continue;
        if (saved[i] && saved_n[i] == n && !memcmp(saved[i], mem, n) && access(p, F_OK) == 0) continue;
        snprintf(jobs[njobs].path, sizeof jobs[njobs].path, "%s", p);
        jobs[njobs].data = malloc(n);
        memcpy(jobs[njobs].data, mem, n);
        jobs[njobs].n = n;
        njobs++;
        remember(i, mem, n);
    }
    if (!njobs) return false;
    SDL_AtomicSet(&busy, 1);
    writer = SDL_CreateThread(write_jobs, "sram", NULL);
    if (!writer) write_jobs(NULL);
    return true;
}
