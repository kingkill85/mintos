// uses: frontend/cheevos.c frontend/core.c frontend/paths.c frontend/archive.c common/cheevos_store.c common/settings.c common/osd.c common/ui.c -DRC_DISABLE_LUA -Ithird_party/rcheevos/include -Ithird_party/rcheevos/src third_party/rcheevos/src/*.c third_party/rcheevos/src/*/*.c
// libs: $(pkg-config --libs sdl2 SDL2_ttf)
// so: frontend/test/testcore.c
/* Achievements in rgarch against a fixture cheevos dir (what mintos-cheevos would have stored)
 * and the test core: ram[0] = frame counter, ram[1] = 1. Nothing here touches the network. */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <glob.h>
#include <utime.h>
#include <sys/stat.h>
#include "check.h"
#include "core.h"
#include "cheevos.h"
#include "osd.h"

#define H1 "0123456789abcdef0123456789abcdef"   /* game.tst -> game 9 */
#define H2 "fedcba9876543210fedcba9876543210"   /* rf5.tst  -> game 10 */
#define H4 "00000000000000000000000000000004"   /* empty.tst: game without achievements */
#define H3 "00000000000000000000000000000003"   /* nogame.tst: hash known, game/<hash>.json missing */

static char root[] = "/tmp/rgarch-cheevos-XXXXXX";
static char dir[1024];

static void put(const char *rel, const char *s) {
    char p[1200], cmd[1400];
    snprintf(p, sizeof p, "%s/%s", dir, rel);
    snprintf(cmd, sizeof cmd, "mkdir -p \"$(dirname '%s')\"", p);
    if (system(cmd)) return;
    FILE *f = fopen(p, "w");
    if (f) { fputs(s, f); fclose(f); }
}

static int count(const char *pattern) {   /* files in the cheevos dir matching pattern */
    char p[1200];
    snprintf(p, sizeof p, "%s/%s", dir, pattern);
    glob_t g;
    int n = glob(p, 0, NULL, &g) ? 0 : (int)g.gl_pathc;
    if (n) globfree(&g);
    return n;
}

static void hashline(FILE *f, const char *rom, const char *hash, unsigned gid) {
    struct stat st;
    stat(rom, &st);
    fprintf(f, "%s\t%lld\t%lld\t%s\t%u\t1\n", rom, (long long)st.st_size, (long long)st.st_mtime, hash, gid);
}

static void frames(Core *c, int n) { while (n--) { core_run(c); cheevos_frame(); } }

static const CheevoItem *item(const CheevoItem *it, int n, const char *title) {
    for (int i = 0; i < n; i++) if (!strcmp(it[i].title, title)) return &it[i];
    return NULL;
}

#define ACH(id, mem) "{\"ID\":" #id ",\"Title\":\"Ach" #id "\",\"Description\":\"Desc" #id "\",\"Flags\":3,\"Points\":5," \
    "\"MemAddr\":\"" mem "\",\"Author\":\"A\",\"BadgeName\":\"00" #id "\",\"Created\":1367266583,\"Modified\":1376929305}"
#define GAME(gid, achs) "{\"Success\":true,\"GameId\":" #gid ",\"Title\":\"Test Game\",\"ConsoleId\":7," \
    "\"ImageIconUrl\":\"http://server/Images/1.png\",\"RichPresenceGameId\":" #gid ",\"RichPresencePatch\":\"\",\"Sets\":[{" \
    "\"AchievementSetId\":1,\"GameId\":" #gid ",\"Title\":null,\"Type\":\"core\",\"ImageIconUrl\":\"http://server/Images/1.png\"," \
    "\"Achievements\":[" achs "],\"Leaderboards\":[]}]}"

static void on_video(const void *d, unsigned w, unsigned h, size_t p) { (void)d; (void)w; (void)h; (void)p; }
static size_t on_audio(const int16_t *d, size_t n) { (void)d; return n; }

int main(void) {
    mkdtemp(root);
    setenv("RGARCH_ROOT", root, 1);
    unsetenv("MINTOS_CHEEVOS_DIR");
    snprintf(dir, sizeof dir, "%s/storage/.config/mintos/cheevos", root);
    char rom[1024], rf5[1024], nogame[1024], empty[1024], cmd[4096], p[1200];
    snprintf(rom, sizeof rom, "%s/storage/roms/nes/game.tst", root);
    snprintf(rf5, sizeof rf5, "%s/storage/roms/nes/rf5.tst", root);
    snprintf(empty, sizeof empty, "%s/storage/roms/nes/empty.tst", root);
    snprintf(nogame, sizeof nogame, "%s/storage/roms/nes/nogame.tst", root);
    snprintf(cmd, sizeof cmd, "mkdir -p '%s/storage/roms/nes' '%s' && echo x > '%s' && echo y > '%s' && echo z > '%s' && echo w > '%s'",
             root, dir, rom, rf5, nogame, empty);
    CHECK_INT(system(cmd), 0);

    put("token", "User\nApiToken\n");
    put("login.json", "{\"Success\":true,\"User\":\"User\",\"AvatarUrl\":\"http://server/UserPic/User.png\","
                      "\"Token\":\"ApiToken\",\"Score\":12345,\"Messages\":0}");
    /* 101 fires when the counter reaches 5, 102 never (the counter is 0xff only at frame 255) */
    put("game/" H1 ".json", GAME(9, ACH(101, "0xH0000=5") "," ACH(102, "0xH0000=255")));
    /* RF5: 201 needs ram[1] == 1 on 10 frames */
    put("game/" H2 ".json", GAME(10, ACH(201, "0xH0001=1.10.")));
    snprintf(p, sizeof p, "%s/hashes.tsv", dir);
    FILE *f = fopen(p, "w");
    hashline(f, rom, H1, 9);
    hashline(f, rf5, H2, 10);
    hashline(f, nogame, H3, 11);
    hashline(f, empty, H4, 12);
    fclose(f);

    Core c = {.on_video = on_video, .on_audio = on_audio};
    CHECK_INT(core_load(&c, "./testcore.so", rom), 0);

    /* a game with no core achievements: no toast, no session, no list */
    put("game/" H4 ".json", GAME(12, ""));
    osd_toast("");
    CHECK(!cheevos_start(&c, "nes", empty));
    CHECK_STR(osd_toast_text(), "");
    CheevoItem it[8];
    CHECK_INT(cheevos_list(it, 8), 0);

    /* start: logged in and loaded from the files, toast with the count */
    CHECK(cheevos_start(&c, "nes", rom));
    CHECK_STR(osd_toast_text(), "0/2 Erfolge");
    CHECK_INT(cheevos_list(it, 8), 2);

    /* 5 frames: 101 unlocks -> queued for upload, kept locally, toast */
    frames(&c, 5);
    CHECK_INT(count("queue/*-101.json"), 1);
    CHECK_INT(count("queue/*.json"), 1);
    snprintf(p, sizeof p, "%s/local/9.txt", dir);
    char line[64] = "";
    f = fopen(p, "r");
    if (f) { if (!fgets(line, sizeof line, f)) line[0] = 0; fclose(f); }
    CHECK(strncmp(line, "101 ", 4) == 0);
    CHECK_STR(osd_toast_text(), "Erfolg: Ach101 (5)");
    int n = cheevos_list(it, 8);
    CHECK_INT(n, 2);
    const CheevoItem *a = item(it, n, "Ach101"), *b = item(it, n, "Ach102");
    CHECK(a && a->unlocked && a->points == 5 && !strcmp(a->desc, "Desc101"));
    CHECK(b && !b->unlocked);

    /* restart (offline): the local unlock counts, nothing new is queued */
    cheevos_stop();
    CHECK_INT(cheevos_list(it, 8), 0);
    CHECK(cheevos_start(&c, "nes", rom));
    CHECK_STR(osd_toast_text(), "1/2 Erfolge");
    frames(&c, 10);
    CHECK_INT(count("queue/*.json"), 1);
    cheevos_stop();

    /* unlocks from the server (user/<game>.json, r=unlocks answer) are merged with the local ones */
    put("user/9.json", "{\"Success\":true,\"UserUnlocks\":[101,102]}");
    CHECK(cheevos_start(&c, "nes", rom));
    CHECK_STR(osd_toast_text(), "2/2 Erfolge");
    cheevos_stop();
    put("user/9.json", "{\"Success\":true,\"UserUnlocks\":[102]}");
    CHECK(cheevos_start(&c, "nes", rom));
    CHECK_STR(osd_toast_text(), "2/2 Erfolge");   /* 102 from the server, 101 local */
    cheevos_stop();

    /* RF5: a state load resets the hit counts. 6 frames, load a state from counter 2, reset as
     * main.c does: 4 more frames make 10 in total but must not fire; 10 after the reset do */
    CHECK(cheevos_start(&c, "nes", rf5));
    CHECK_STR(osd_toast_text(), "0/1 Erfolge");
    unsigned char st[16];
    core_reset(&c);
    frames(&c, 2);
    CHECK(core_state_save(&c, st, core_state_size(&c)));
    frames(&c, 4);
    CHECK(core_state_load(&c, st, core_state_size(&c)));
    cheevos_reset();
    frames(&c, 4);
    CHECK_INT(count("queue/*-201.json"), 0);
    frames(&c, 5);
    CHECK_INT(count("queue/*-201.json"), 0);
    frames(&c, 1);
    CHECK_INT(count("queue/*-201.json"), 1);
    cheevos_stop();

    /* RF1: the ROM changed after the last sync (new mtime) -> no hash, no achievements */
    struct utimbuf ut = {1000000000, 1000000000};
    CHECK_INT(utime(rom, &ut), 0);
    CHECK(!cheevos_start(&c, "nes", rom));
    frames(&c, 3);                         /* inactive: no work, no crash */
    CHECK_INT(cheevos_list(it, 8), 0);

    /* a hash without game/<hash>.json, a system without achievements */
    CHECK(!cheevos_start(&c, "nes", nogame));
    CHECK(!cheevos_start(&c, "scummvm", rf5));

    /* switched off in the settings */
    snprintf(p, sizeof p, "%s/storage/.config/mintos/settings.cfg", root);
    f = fopen(p, "w"); fputs("cheevos = 0\n", f); fclose(f);
    CHECK(!cheevos_start(&c, "nes", rf5));
    unlink(p);
    CHECK(cheevos_start(&c, "nes", rf5));   /* on again */
    cheevos_stop();

    /* without a token: false, and frames do nothing */
    snprintf(p, sizeof p, "%s/token", dir);
    unlink(p);
    int q = count("queue/*.json");
    CHECK(!cheevos_start(&c, "nes", rf5));
    core_reset(&c);
    frames(&c, 20);
    cheevos_idle();
    cheevos_reset();
    CHECK_INT(count("queue/*.json"), q);
    cheevos_stop();

    core_unload(&c);
    snprintf(cmd, sizeof cmd, "rm -rf %s", root);
    if (system(cmd)) {}
    TEST_END();
}
