#include "cheevos_store.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* ---- paths ---- */

const char *cs_dir(void) {
    static char buf[1024];
    const char *o = getenv("MINTOS_CHEEVOS_DIR");
    if (o && *o) return o;
    const char *r = getenv("RGARCH_ROOT");
    snprintf(buf, sizeof buf, "%s/storage/.config/mintos/cheevos", r ? r : "");
    return buf;
}

int cs_path(char *out, size_t n, const char *fmt, ...) {
    int k = snprintf(out, n, "%s/", cs_dir());
    if (k < 0 || (size_t)k >= n) return -1;
    va_list ap; va_start(ap, fmt);
    int m = vsnprintf(out + k, n - k, fmt, ap);
    va_end(ap);
    return (m < 0 || (size_t)m >= n - k) ? -1 : 0;
}

/* ---- files ---- */

static void fsync_dir(const char *dir) {
    int d = open(dir, O_RDONLY | O_DIRECTORY);
    if (d < 0) return;
    fsync(d);
    close(d);
}

static void fsync_parent(char *p) {  /* fsync the directory containing file path p */
    char *s = strrchr(p, '/');
    if (!s || s == p) return;
    *s = 0; fsync_dir(p); *s = '/';
}

static void mkdirs(char *p) {  /* mkdir -p of every directory component in p (p is a file path); new dirs are made durable */
    for (char *s = p + 1; *s; s++)
        if (*s == '/') {
            *s = 0;
            if (mkdir(p, 0755) == 0) { fsync_dir(p); fsync_parent(p); }
            *s = '/';
        }
}

int cs_write(const char *rel, const char *data, size_t len) {
    char path[1024], tmp[1030];
    if (cs_path(path, sizeof path, "%s", rel)) return -1;
    if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return -1;
    mkdirs(path);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return -1;
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, data + off, len - off);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) { close(fd); unlink(tmp); return -1; }
        off += w;
    }
    int bad = fsync(fd);
    if (close(fd)) bad = -1;
    if (bad || rename(tmp, path)) { unlink(tmp); return -1; }
    fsync_parent(path);   /* the new dirent must survive power loss too */
    return 0;
}

char *cs_read(const char *rel, size_t *len) {
    char path[1024];
    if (cs_path(path, sizeof path, "%s", rel)) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 4096, n = 0;
    char *b = malloc(cap);
    while (b) {
        size_t r = fread(b + n, 1, cap - n - 1, f);
        n += r;
        if (n < cap - 1) break;
        char *nb = realloc(b, cap *= 2);
        if (!nb) { free(b); b = NULL; }
        else b = nb;
    }
    fclose(f);
    if (!b) return NULL;
    b[n] = 0;
    if (len) *len = n;
    return b;
}

static void chomp_copy(char *dst, size_t n, const char *s, size_t l) {
    if (l >= n) l = n - 1;
    memcpy(dst, s, l); dst[l] = 0;
}

int cs_token(char *user, size_t un, char *token, size_t tn) {
    char *d = cs_read("token", NULL);
    if (!d) return -1;
    char *nl = strchr(d, '\n');
    if (!nl) { free(d); return -1; }
    char *t = nl + 1, *e = strchr(t, '\n');
    size_t tl = e ? (size_t)(e - t) : strlen(t);
    if (nl == d || tl == 0) { free(d); return -1; }
    chomp_copy(user, un, d, nl - d);
    chomp_copy(token, tn, t, tl);
    free(d);
    return 0;
}

/* ---- hash cache ---- */

int cs_hash_lookup(const char *rom, char hash[33], unsigned *game_id) {
    struct stat st;
    if (stat(rom, &st)) return -1;
    char path[1024];
    if (cs_path(path, sizeof path, "hashes.tsv")) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[4096];
    int rc = -1;
    while (rc && fgets(line, sizeof line, f)) {
        char *fld[6]; int nf = 0;
        char *s = line;
        line[strcspn(line, "\r\n")] = 0;
        while (nf < 6) { fld[nf++] = s; char *t = strchr(s, '\t'); if (!t) break; *t = 0; s = t + 1; }
        if (nf < 5 || strcmp(fld[0], rom)) continue;
        if (atoll(fld[1]) != (long long)st.st_size || atoll(fld[2]) != (long long)st.st_mtime) continue;
        if (strlen(fld[3]) != 32) continue;
        memcpy(hash, fld[3], 33);
        if (game_id) *game_id = (unsigned)strtoul(fld[4], NULL, 10);
        rc = 0;
    }
    fclose(f);
    return rc;
}

/* ---- unlock queue ---- */

int cs_queue_add(unsigned game_id, unsigned ach_id, const char *hash, long when) {
    char rel[96], json[256];
    int n = snprintf(json, sizeof json, "{\"g\":%u,\"a\":%u,\"m\":\"%s\",\"t\":%ld}", game_id, ach_id, hash, when);
    if (n < 0 || n >= (int)sizeof json) return -1;
    snprintf(rel, sizeof rel, "queue/%ld-%u.json", when, ach_id);
    if (cs_write(rel, json, n)) return -1;

    char path[1024], ln[64];
    if (cs_path(path, sizeof path, "local/%u.txt", game_id)) return -1;
    mkdirs(path);
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    snprintf(ln, sizeof ln, "%u %ld\n", ach_id, when);
    int rc = (fputs(ln, f) < 0 || fflush(f) || fsync(fileno(f))) ? -1 : 0;
    if (fclose(f)) rc = -1;
    fsync_parent(path);
    return rc;
}

int cs_local_unlocks(unsigned game_id, unsigned *ids, long *when, int max) {
    char path[1024];
    if (cs_path(path, sizeof path, "local/%u.txt", game_id)) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    unsigned a; long t;
    while (n < max && fscanf(f, "%u %ld", &a, &t) == 2) { ids[n] = a; when[n] = t; n++; }
    fclose(f);
    return n;
}
