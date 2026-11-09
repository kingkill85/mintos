/* Screenshot of what the panel shows: fbgrab [out.png] (root, needs debugfs). */
#include "fb.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static void chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n) {
    uint32_t len = htonl(n), crc = crc32(crc32(0, (const Bytef *)type, 4), d, n);
    fwrite(&len, 4, 1, f);
    fwrite(type, 1, 4, f);
    if (n) fwrite(d, 1, n, f);
    crc = htonl(crc);
    fwrite(&crc, 4, 1, f);
}

int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "/tmp/screen.png";
    Fb fb;
    if (fb_open(&fb)) { fprintf(stderr, "fbgrab: no framebuffer on the primary plane\n"); return 1; }
    size_t rowlen = 1 + (size_t)fb.w * 3, rawlen = rowlen * fb.h;
    uint8_t *raw = malloc(rawlen);
    uLongf zlen = compressBound(rawlen);
    uint8_t *z = malloc(zlen);
    if (!raw || !z) return 1;
    for (unsigned y = 0; y < fb.h; y++) {
        uint8_t *r = raw + y * rowlen;
        *r++ = 0;   /* filter: none */
        for (unsigned x = 0; x < fb.w; x++, r += 3) fb_rgb(&fb, x, y, r);
    }
    if (compress(z, &zlen, raw, rawlen) != Z_OK) return 1;
    FILE *f = fopen(out, "wb");
    if (!f) { perror(out); return 1; }
    uint8_t ihdr[13];
    uint32_t w = htonl(fb.w), h = htonl(fb.h);
    memcpy(ihdr, &w, 4); memcpy(ihdr + 4, &h, 4);
    memcpy(ihdr + 8, (uint8_t[]){ 8, 2, 0, 0, 0 }, 5);   /* 8 bit RGB */
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, zlen);
    chunk(f, "IEND", NULL, 0);
    if (fclose(f)) { perror(out); return 1; }
    size_t nonzero = 0;
    for (size_t i = 0; i < fb.size; i += 97) nonzero += fb.px[i] != 0;
    printf("%u %u 0x%x nonzero bytes: %zu\n", fb.w, fb.h, fb.fmt, nonzero);
    fb_close(&fb);
    return 0;
}
