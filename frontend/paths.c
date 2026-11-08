#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

static const char *root(void) {
    const char *r = getenv("RGARCH_ROOT");
    return r ? r : "";
}

/* the part after "storage/roms/", with an optional RGARCH_ROOT prefix */
static const char *rel(const char *rom) {
    const char *r = root();
    size_t rl = strlen(r);
    if (rl && !strncmp(rom, r, rl)) rom += rl;
    static const char pre[] = "/storage/roms/";
    return strncmp(rom, pre, sizeof pre - 1) ? NULL : rom + sizeof pre - 1;
}

int path_system(const char *rom, char *out, size_t n) {
    const char *p = rel(rom), *slash = p ? strchr(p, '/') : NULL;
    if (!slash) return -1;
    snprintf(out, n, "%.*s", (int)(slash - p), p);
    return 0;
}

int path_for(PathKind k, const char *rom, int slot, char *out, size_t n) {
    char sys[256];
    if (path_system(rom, sys, sizeof sys)) return -1;
    const char *base = strrchr(rom, '/') + 1, *dot = strrchr(base, '.');
    int len = dot ? (int)(dot - base) : (int)strlen(base);
    const char *dir = k == P_SRAM || k == P_RTC ? "saves" : "states";
    char ext[16];
    switch (k) {
    case P_SRAM: snprintf(ext, sizeof ext, "srm"); break;
    case P_RTC: snprintf(ext, sizeof ext, "rtc"); break;
    case P_STATE: snprintf(ext, sizeof ext, "st%d", slot); break;
    case P_PREVIEW: snprintf(ext, sizeof ext, "st%d.png", slot); break;
    case P_AUTO: snprintf(ext, sizeof ext, "auto"); break;
    }
    snprintf(out, n, "%s/storage/%s/%s/%.*s.%s", root(), dir, sys, len, base, ext);
    return 0;
}

int mkdirs_for(const char *file) {
    char p[1024];
    snprintf(p, sizeof p, "%s", file);
    char *s = strrchr(p, '/');
    if (!s) return 0;
    *s = 0;
    for (char *q = p + 1; *q; q++) {
        if (*q != '/') continue;
        *q = 0;
        if (mkdir(p, 0755) && errno != EEXIST) return -1;
        *q = '/';
    }
    return mkdir(p, 0755) && errno != EEXIST ? -1 : 0;
}
