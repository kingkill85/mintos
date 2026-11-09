// uses: tools/cheevos_sync.c common/cheevos_store.c frontend/archive.c -DRC_DISABLE_LUA -Ithird_party/rcheevos/include -Ithird_party/rcheevos/src third_party/rcheevos/src/*.c third_party/rcheevos/src/*/*.c
/* Host tests for mintos-cheevos' logic (tools/cheevos_sync.c) with a fake server answering from os/test/cheevos/. */
#include "check.h"
#include "../../tools/cheevos_sync.h"
#include "cheevos_store.h"
#include "rc_consoles.h"
#include <ctype.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define HASH "0829f71740aab1ab98b33eae21dee122"

static int offline;            /* the whole network is down */
static int hl_mode;            /* hashlibrary: 0 normal, 1 Success:false, 2 garbage */
static int unlocks_bad;        /* unlocks answer 200 with an expired_token Code */
static const char *award_mode; /* ok | already | badtoken | rejected */
static char calls[64][24];     /* r= values seen, in order */
static int ncalls;
static char last_award[512];

static char *load(const char *name) {
    char p[512];
    snprintf(p, sizeof p, "%s/os/test/cheevos/%s", getenv("REPO"), name);
    FILE *f = fopen(p, "rb");
    if (!f) { fprintf(stderr, "missing fixture %s\n", p); exit(2); }
    char *b = calloc(1, 8192);
    if (fread(b, 1, 8191, f) == 0) exit(2);
    fclose(f);
    return b;
}

static int fake(const char *url, const char *post, char **body, int *status) {
    (void)url;
    const char *r = strstr(post, "r=");
    CHECK(r != NULL);
    if (!r) return -1;
    char name[24] = "";
    sscanf(r + 2, "%23[a-z0-9]", name);
    if (ncalls < 64) snprintf(calls[ncalls++], sizeof calls[0], "%s", name);
    if (offline) return -1;
    *status = 200;
    if (!strcmp(name, "login2")) {
        if (strstr(post, "&p=") && !strstr(post, "&p=secret")) {
            *body = strdup("{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\",\"Code\":\"invalid_credentials\",\"Status\":401}");
            *status = 401;
        } else if (strstr(post, "u=Other")) *body = strdup("{\"Success\":true,\"User\":\"Other\",\"Token\":\"TOK999\",\"Score\":1,\"SoftcoreScore\":1,\"Messages\":0}");
        else *body = load("login2.json");
    } else if (!strcmp(name, "hashlibrary")) {
        if (hl_mode == 1) *body = strdup("{\"Success\":false,\"Error\":\"nope\"}");
        else if (hl_mode == 2) *body = strdup("<html>captive portal</html>");
        else *body = strstr(post, "c=7") ? load("hashlibrary-7.json") : strdup("{\"Success\":true,\"MD5List\":[]}");
        char *hp = strstr(*body, HASH);   /* the server may answer with upper-case hashes: compared case-insensitively */
        for (int i = 0; hp && i < 32; i++) hp[i] = (char)toupper((unsigned char)hp[i]);
    }
    else if (!strcmp(name, "achievementsets")) *body = load("achievementsets.json");
    else if (!strcmp(name, "unlocks")) *body = load(unlocks_bad ? "unlocks-badtoken.json" : "unlocks.json");
    else if (!strcmp(name, "startsession")) *body = strdup("{\"Success\":true}");
    else if (!strcmp(name, "awardachievement")) {
        snprintf(last_award, sizeof last_award, "%s", post);
        if (!strcmp(award_mode, "ok")) *body = load("award-ok.json");
        else if (!strcmp(award_mode, "already")) *body = load("award-already.json");
        else if (!strcmp(award_mode, "badtoken")) { *body = load("award-badtoken.json"); *status = 401; }
        else if (!strcmp(award_mode, "badtoken200")) *body = load("award-badtoken.json");   /* no HTTP 401: only the Code says so */
        else if (!strcmp(award_mode, "429")) { *body = strdup("{\"Error\":\"Too Many Requests\"}"); *status = 429; }
        else *body = strdup("{\"Success\":false,\"Error\":\"Unknown achievement.\"}");
    } else *body = strdup("{\"Success\":true}");
    return 0;
}

static int called(const char *n) { int c = 0; for (int i = 0; i < ncalls; i++) c += !strcmp(calls[i], n); return c; }
static int exists(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static int exists(const char *fmt, ...) {
    char rel[256], p[1024];
    va_list ap; va_start(ap, fmt); vsnprintf(rel, sizeof rel, fmt, ap); va_end(ap);
    cs_path(p, sizeof p, "%s", rel);
    return access(p, F_OK) == 0;
}
static void put(const char *path, const void *d, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(d, 1, n, f) != n) exit(2);
    fclose(f);
}
static void sh(const char *fmt, const char *a) {
    char c[2048]; snprintf(c, sizeof c, fmt, a);
    if (system(c)) fprintf(stderr, "failed: %s\n", c);
}

int main(void) {
    char root[] = "/tmp/cs-sync-XXXXXX", p[1024], dir[600];
    if (!mkdtemp(root)) return 2;
    snprintf(dir, sizeof dir, "%s/c", root);
    setenv("MINTOS_CHEEVOS_DIR", dir, 1);
    snprintf(p, sizeof p, "%s/tmp", root);
    mkdir(p, 0755);
    setenv("TMPDIR", p, 1);
    static unsigned char nes[16 + 8192];
    memcpy(nes, "NES\x1a", 4);
    char roms[700], rom[1100];
    snprintf(roms, sizeof roms, "%s/roms", root);
    snprintf(p, sizeof p, "%s/roms/nes", root); sh("mkdir -p '%s'", p);
    snprintf(rom, sizeof rom, "%s/a.nes", p);
    put(rom, nes, sizeof nes);

    /* systems */
    CHECK(cs_system("megacd") && cs_system("megacd")->console == RC_CONSOLE_SEGA_CD);
    CHECK(cs_system("scummvm") == NULL);
    CHECK(cs_system("nonsense") == NULL);
    CHECK(cs_system("famicom") && cs_system("famicom")->console == RC_CONSOLE_NINTENDO);
    CHECK(cs_system("fbneo") && cs_system("fbneo")->console == RC_CONSOLE_ARCADE);
    CHECK(cs_system("sg-1000") && cs_system("sg-1000")->console == RC_CONSOLE_SG1000);
    CHECK(cs_system("nes") && strstr(cs_system("nes")->exts, "nes"));

    /* login */
    offline = 1;
    CHECK_INT(cs_login(fake, "Tester", "secret"), -1);
    CHECK(!exists("token"));
    offline = 0;
    CHECK_INT(cs_login(fake, "Tester", "wrong"), 1);
    CHECK(!exists("token"));
    CHECK_INT(cs_login(fake, "Tester", "secret"), 0);
    cs_path(p, sizeof p, "token");
    struct stat st;
    CHECK(stat(p, &st) == 0 && (st.st_mode & 0777) == 0600);
    char u[64], t[64];
    CHECK_INT(cs_token(u, sizeof u, t, sizeof t), 0);
    CHECK_STR(u, "Tester"); CHECK_STR(t, "TOK123");
    CHECK(exists("login.json"));
    snprintf(p, sizeof p, "grep -rq secret '%s'", dir);
    CHECK(system(p) != 0);   /* password nowhere on disk */

    /* first sync: hash, game data, session */
    CsReport r;
    award_mode = "ok";
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(r.games, 1); CHECK_INT(r.hashed, 1); CHECK_INT(r.uploaded, 0); CHECK_INT(r.token_invalid, 0);
    CHECK(exists("game/%s.json", HASH));
    CHECK(exists("user/1234.json"));
    char h[33]; unsigned gid = 0;
    CHECK_INT(cs_hash_lookup(rom, h, &gid), 0);
    CHECK_STR(h, HASH); CHECK_INT(gid, 1234);
    CHECK_INT(called("hashlibrary"), 1);
    CHECK_INT(called("achievementsets"), 1);
    CHECK_INT(called("unlocks"), 1);
    CHECK_INT(called("startsession"), 0);   /* never: it would mark the library as played */

    /* second sync: nothing to hash, no new hashlibrary, game data still fresh */
    ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(r.hashed, 0); CHECK_INT(r.games, 1);
    CHECK_INT(called("hashlibrary"), 0);
    CHECK_INT(called("achievementsets"), 0);

    /* a stored game file that is not an achievementsets answer is fetched again although it is fresh */
    cs_path(p, sizeof p, "game/%s.json", HASH);
    put(p, "<html>captive portal</html>", 27);
    ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(called("achievementsets"), 1);
    { char *g = cs_read("game/" HASH ".json", NULL); CHECK(g && strstr(g, "\"Sets\"")); free(g); }
    ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(called("achievementsets"), 0);   /* valid again: fresh */

    /* queue: ok */
    long now = time(NULL);
    cs_queue_add(1234, 101, HASH, now - 50);
    cs_queue_add(1234, 102, HASH, now - 40);   /* stays locally: the server's session list lacks it */
    unsigned ids[8]; long when[8];
    CHECK_INT(cs_local_unlocks(1234, ids, when, 8), 2);
    ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(r.uploaded, 2); CHECK_INT(r.failed, 0);
    CHECK(!exists("queue/%ld-101.json", now - 50));
    CHECK_INT(called("awardachievement"), 2);
    {
        const char *z = strstr(last_award, "&o=");
        CHECK(z != NULL);
        long s = z ? atol(z + 3) : -1;
        CHECK(s >= 40 && s <= 60);
        CHECK(strstr(last_award, "&h=0") != NULL);   /* softcore */
    }
    /* entries the fresh session already lists (101) leave local/, others stay */
    CHECK_INT(cs_local_unlocks(1234, ids, when, 8), 1);
    CHECK_INT(ids[0], 102);

    /* queue: already awarded (RF4) is done, not failed; not counted as uploaded */
    award_mode = "already";
    cs_queue_add(1234, 102, HASH, now - 30);
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK(!exists("queue/%ld-102.json", now - 30));
    CHECK(!exists("queue/failed/%ld-102.json", now - 30));
    CHECK_INT(r.uploaded, 0); CHECK_INT(r.failed, 0);

    /* queue: rejected for another reason -> failed/ */
    award_mode = "rejected";
    cs_queue_add(1234, 102, HASH, now - 20);
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK(!exists("queue/%ld-102.json", now - 20));
    CHECK(exists("queue/failed/%ld-102.json", now - 20));
    CHECK_INT(r.failed, 1);

    /* stale temp files and failed/ are never uploaded */
    cs_write("queue/5-1.json.tmp", "{\"g\":1234,\"a\":1,\"m\":\"" HASH "\",\"t\":5}", 56);
    ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(called("awardachievement"), 0);
    CHECK(exists("queue/5-1.json.tmp"));
    cs_path(p, sizeof p, "queue/5-1.json.tmp"); unlink(p);

    /* transport error: file kept, nothing failed, sync reports no network */
    award_mode = "ok";
    cs_queue_add(1234, 101, HASH, now - 10);
    offline = 1;
    CHECK_INT(cs_sync(fake, roms, &r), -1);
    CHECK(exists("queue/%ld-101.json", now - 10));
    CHECK_INT(r.failed, 0);
    offline = 0;

    /* invalid token with a full queue (RF3) */
    award_mode = "badtoken";
    ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 1);
    CHECK(exists("queue/%ld-101.json", now - 10));
    CHECK(exists("token.invalid"));
    CHECK_INT(r.token_invalid, 1);
    CHECK_INT(called("hashlibrary") + called("achievementsets") + called("startsession") + called("login2"), 0);
    /* after logging in again the next sync uploads it */
    award_mode = "ok";
    CHECK_INT(cs_login(fake, "Tester", "secret"), 0);
    CHECK(!exists("token.invalid"));
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(r.uploaded, 1);
    CHECK(!exists("queue/%ld-101.json", now - 10));

    /* HTTP 429 is "try later", never "uploaded": file kept even though the body has no Success */
    award_mode = "429";
    cs_queue_add(1234, 101, HASH, now - 9);
    CHECK_INT(cs_sync(fake, roms, &r), -1);
    CHECK(exists("queue/%ld-101.json", now - 9));
    CHECK_INT(r.uploaded, 0); CHECK_INT(r.failed, 0);

    /* expired token announced only by the body's Code (HTTP 200) is still RF3 */
    award_mode = "badtoken200";
    ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 1);
    CHECK(exists("queue/%ld-101.json", now - 9));
    CHECK(exists("token.invalid"));
    CHECK_INT(called("hashlibrary"), 0);
    award_mode = "ok";
    CHECK_INT(cs_login(fake, "Tester", "secret"), 0);
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK(!exists("queue/%ld-101.json", now - 9));
    /* same for the unlocks request */
    unlocks_bad = 1;
    CHECK_INT(cs_sync(fake, roms, &r), 1);
    CHECK(exists("token.invalid"));
    unlocks_bad = 0;
    CHECK_INT(cs_login(fake, "Tester", "secret"), 0);

    /* a new ROM in a subfolder is found, a stray .txt is not hashed; an unreachable server means no hashing at all */
    snprintf(p, sizeof p, "%s/roms/nes/sub", root); sh("mkdir -p '%s'", p);
    nes[200] = 7;
    snprintf(rom, sizeof rom, "%s/b.nes", p);
    put(rom, nes, sizeof nes);
    nes[200] = 0;
    snprintf(p, sizeof p, "%s/roms/nes/readme.txt", root);
    put(p, "hello", 5);
    offline = 1;
    CHECK_INT(cs_sync(fake, roms, &r), -1);
    CHECK_INT(r.hashed, 0);
    offline = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(r.hashed, 1);
    CHECK(cs_hash_lookup(rom, h, &gid) == 0);
    {
        char *tsv = cs_read("hashes.tsv", NULL);
        CHECK(tsv && !strstr(tsv, "readme.txt") && strstr(tsv, "sub/b.nes"));
        free(tsv);
    }

    /* one hashlibrary request per console and sync, even when it fails; garbage = no network */
    for (int i = 1; i <= 2; i++) {
        char nm[1100]; nes[300] = i;
        snprintf(nm, sizeof nm, "%s/roms/nes/c%d.nes", root, i);
        put(nm, nes, sizeof nes);
    }
    nes[300] = 0;
    hl_mode = 1; ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(called("hashlibrary"), 1);
    hl_mode = 2; ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), -1);
    CHECK_INT(called("hashlibrary"), 1);
    hl_mode = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 0);

    /* ROM changed -> hashed again */
    nes[100] = 1;
    put(rom, nes, sizeof nes);
    struct timespec ts[2] = {{0, UTIME_OMIT}, {now + 100, 0}};
    utimensat(AT_FDCWD, rom, ts, 0);
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(r.hashed, 1);
    CHECK(cs_hash_lookup(rom, h, &gid) == 0 && gid == 0);   /* unknown to the server now */

    /* zipped ROMs are extracted (non-arcade) in $TMPDIR, arcade zips are hashed by name */
    nes[100] = 0;
    put(rom, nes, sizeof nes);
    snprintf(p, sizeof p, "%s/roms/nes", root);
    sh("cd '%s' && zip -q z.zip a.nes && rm a.nes", p);
    snprintf(p, sizeof p, "%s/roms/arcade", root); sh("mkdir -p '%s'", p);
    snprintf(rom, sizeof rom, "%s/pacman.zip", p);
    put(rom, "PK", 2);
    ncalls = 0;
    CHECK_INT(cs_sync(fake, roms, &r), 0);
    CHECK_INT(r.games, 1);
    snprintf(p, sizeof p, "%s/roms/nes/z.zip", root);
    CHECK(cs_hash_lookup(p, h, &gid) == 0 && !strcmp(h, HASH) && gid == 1234);
    CHECK(cs_hash_lookup(rom, h, &gid) == 0 && strlen(h) == 32 && gid == 0);
    CHECK_INT(called("hashlibrary"), 2);   /* NES (7) and ARCADE (27) */
    snprintf(p, sizeof p, "%s/tmp/rgarch-rom", root);
    CHECK(access(p, F_OK) != 0);   /* whole extraction dir gone */

    /* logout */
    /* logout keeps queue/ and local/, drops user/; the same account's login keeps them, another account's drops them */
    cs_queue_add(1234, 101, HASH, now - 5);
    CHECK(exists("user/1234.json") && exists("local/1234.txt"));
    CHECK_INT(cs_logout(), 0);
    CHECK(!exists("token") && !exists("login.json") && !exists("user"));
    CHECK(exists("queue/%ld-101.json", now - 5) && exists("local/1234.txt"));
    CHECK_INT(cs_login(fake, "tester", "secret"), 0);   /* case-insensitive same user */
    CHECK(exists("queue/%ld-101.json", now - 5) && exists("local/1234.txt"));
    CHECK_INT(cs_logout(), 0);
    CHECK_INT(cs_login(fake, "Other", "secret"), 0);
    CHECK(!exists("queue/%ld-101.json", now - 5) && exists("queue/failed/%ld-101.json", now - 5));
    CHECK(!exists("local") && !exists("user"));
    { char *lu = cs_read("last_user", NULL); CHECK(lu && !strncmp(lu, "Other", 5)); free(lu); }
    CHECK_INT(cs_logout(), 0);

    sh("rm -rf '%s'", root);
    TEST_END();
}

void rg_log(const char *fmt, ...) { (void)fmt; }
