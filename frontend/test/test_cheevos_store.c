// uses: common/cheevos_store.c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include "check.h"
#include "cheevos_store.h"

static void put(const char *p, const char *s) { FILE *f = fopen(p, "w"); fputs(s, f); fclose(f); }

int main(void) {
    char tmpl[] = "/tmp/cstoreXXXXXX";
    char *d = mkdtemp(tmpl);
    CHECK(d != NULL);
    setenv("MINTOS_CHEEVOS_DIR", d, 1);
    char p[1024], q[1024];

    CHECK_STR(cs_dir(), d);
    CHECK_INT(cs_path(p, sizeof p, "queue/%d-%u.json", 1000, 77u), 0);
    snprintf(q, sizeof q, "%s/queue/1000-77.json", d); CHECK_STR(p, q);
    char tiny[4]; CHECK_INT(cs_path(tiny, sizeof tiny, "abcdef"), -1);

    /* RGARCH_ROOT fallback */
    unsetenv("MINTOS_CHEEVOS_DIR"); setenv("RGARCH_ROOT", "/tmp/rgr", 1);
    CHECK_STR(cs_dir(), "/tmp/rgr/storage/.config/mintos/cheevos");
    unsetenv("RGARCH_ROOT");
    CHECK_STR(cs_dir(), "/storage/.config/mintos/cheevos");
    setenv("MINTOS_CHEEVOS_DIR", d, 1);

    /* round trip; RF2: creates missing dirs */
    size_t n = 0;
    CHECK_INT(cs_write("a/b/c.txt", "hello", 5), 0);
    char *r = cs_read("a/b/c.txt", &n);
    CHECK(r != NULL);
    if (r) { CHECK_STR(r, "hello"); CHECK_INT(n, 5); free(r); }
    CHECK(cs_read("nope", &n) == NULL);
    cs_path(p, sizeof p, "a/b/c.txt.tmp"); CHECK(access(p, F_OK) != 0);

    /* failed write leaves the old file intact: block the tmp name with a directory */
    mkdir(p, 0755);
    CHECK(cs_write("a/b/c.txt", "NEW", 3) != 0);
    r = cs_read("a/b/c.txt", &n);
    if (r) { CHECK_STR(r, "hello"); free(r); } else CHECK(0);
    rmdir(p);

    /* token */
    char u[64], t[64];
    CHECK_INT(cs_token(u, sizeof u, t, sizeof t), -1);
    CHECK_INT(cs_write("token", "bob\nsecret\n", 11), 0);
    CHECK_INT(cs_token(u, sizeof u, t, sizeof t), 0);
    CHECK_STR(u, "bob"); CHECK_STR(t, "secret");

    /* hash lookup; RF1: mtime change invalidates */
    char rom[1100]; snprintf(rom, sizeof rom, "%s/game.gb", d);
    put(rom, "ROMDATA");
    struct stat st; stat(rom, &st);
    char line[2048];
    snprintf(line, sizeof line, "/other.gb\t1\t1\tffffffffffffffffffffffffffffffff\t9\t1\n"
        "%s\t%lld\t%lld\t0123456789abcdef0123456789abcdef\t4242\t5\n", rom, (long long)st.st_size, (long long)st.st_mtime);
    CHECK_INT(cs_write("hashes.tsv", line, strlen(line)), 0);
    char h[33]; unsigned gid = 0;
    CHECK_INT(cs_hash_lookup(rom, h, &gid), 0);
    CHECK_STR(h, "0123456789abcdef0123456789abcdef"); CHECK_INT(gid, 4242);
    struct utimbuf ut = { st.st_atime, st.st_mtime + 100 };
    utime(rom, &ut);
    CHECK_INT(cs_hash_lookup(rom, h, &gid), -1);
    CHECK_INT(cs_hash_lookup("/no/such.gb", h, &gid), -1);

    /* queue + local unlocks */
    CHECK_INT(cs_queue_add(5, 77, "ab..", 1000), 0);
    r = cs_read("queue/1000-77.json", &n);
    CHECK(r != NULL);
    if (r) { CHECK_STR(r, "{\"g\":5,\"a\":77,\"m\":\"ab..\",\"t\":1000}"); free(r); }
    unsigned ids[8]; long when[8];
    CHECK_INT(cs_local_unlocks(5, ids, when, 8), 1);
    CHECK_INT(ids[0], 77); CHECK_INT(when[0], 1000);
    CHECK_INT(cs_queue_add(5, 78, "ab..", 1001), 0);
    CHECK_INT(cs_local_unlocks(5, ids, when, 8), 2);
    CHECK_INT(cs_local_unlocks(5, ids, when, 1), 1);
    CHECK_INT(cs_local_unlocks(6, ids, when, 8), 0);

    snprintf(p, sizeof p, "rm -rf '%s'", d); if (system(p)) {}
    TEST_END();
}
