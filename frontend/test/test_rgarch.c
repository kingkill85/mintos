// uses:
// so: frontend/test/testcore.c
// bin: rgarch-host frontend/main.c frontend/core.c frontend/paths.c frontend/config.c frontend/viewport.c frontend/video.c frontend/audio.c frontend/ratecontrol.c frontend/input.c frontend/inputmap.c frontend/state.c frontend/menu.c frontend/archive.c frontend/gameopts.c common/ui.c common/settings.c common/osd.c frontend/cheevos.c common/cheevos_store.c -DRC_DISABLE_LUA -Ithird_party/rcheevos/include -Ithird_party/rcheevos/src third_party/rcheevos/src/*.c third_party/rcheevos/src/*/*.c
/* Runs a host build of rgarch with the test core (SDL dummy drivers). */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include "check.h"

static char root[] = "/tmp/rgarch-main-XXXXXX";

static const char *err_timeout = "1", *term_early = NULL, *sysname = "test";

static pid_t start(const char *rom, const char *extra) {
    pid_t pid = fork();
    if (pid == 0) {
        char core[1024];
        snprintf(core, sizeof core, "%s/testcore.so", getcwd(NULL, 0));
        setenv("RGARCH_ROOT", root, 1);
        setenv("RGARCH_CORE", core, 1);
        setenv("RGARCH_ERROR_TIMEOUT", err_timeout, 1);
        if (term_early) setenv("RGARCH_TEST_TERM_EARLY", "1", 1);
        if (extra) execl("./rgarch-host", "rgarch", extra, sysname, rom, (char *)NULL);
        else execl("./rgarch-host", "rgarch", sysname, rom, (char *)NULL);
        _exit(127);
    }
    return pid;
}

static int wait_exit(pid_t pid) {
    int st = 0;
    for (int i = 0; i < 100; i++) { if (waitpid(pid, &st, WNOHANG) == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : -1; usleep(50000); }
    kill(pid, SIGKILL); waitpid(pid, &st, 0);
    return -2;   /* hung */
}

int main(void) {
    mkdtemp(root);
    char rom[512], p[1024], cmd[4096];
    snprintf(rom, sizeof rom, "%s/storage/roms/test/Game (USA).tst", root);
    snprintf(cmd, sizeof cmd, "mkdir -p '%s/storage/roms/test' '%s/storage/.config/rgarch' && echo x > '%s' && "
             "printf '[test]\\ncore = testcore\\nscale = integer\\n' > '%s/storage/.config/rgarch/systems.cfg'", root, root, rom, root);
    system(cmd);

    /* SIGTERM (power key): auto state + SRAM, exit 0 */
    pid_t pid = start(rom, NULL);
    usleep(1000000);
    kill(pid, SIGTERM);
    CHECK_INT(wait_exit(pid), 0);
    snprintf(p, sizeof p, "%s/storage/states/test/Game (USA).auto", root);
    CHECK(access(p, F_OK) == 0);
    snprintf(p, sizeof p, "%s/storage/saves/test/Game (USA).srm", root);
    CHECK(access(p, F_OK) == 0);

    /* --resume loads the auto state and keeps running */
    pid = start(rom, "--resume");
    usleep(800000);
    kill(pid, SIGTERM);
    CHECK_INT(wait_exit(pid), 0);
    snprintf(cmd, sizeof cmd, "grep -q 'load state -1: ok' '%s/var/log/rgarch.log'", root);
    CHECK_INT(system(cmd), 0);

    /* a plain start continues too (after Beenden): the auto state loads without --resume */
    pid = start(rom, NULL);
    usleep(800000);
    kill(pid, SIGTERM);
    CHECK_INT(wait_exit(pid), 0);
    snprintf(cmd, sizeof cmd, "grep -q 'load state -1: ok' '%s/var/log/rgarch.log'", root);   /* log is per run */
    CHECK_INT(system(cmd), 0);

    /* --fresh (X in the game list) ignores the auto state */
    pid = start(rom, "--fresh");
    usleep(800000);
    kill(pid, SIGTERM);
    CHECK_INT(wait_exit(pid), 0);
    snprintf(cmd, sizeof cmd, "grep -q 'load state -1' '%s/var/log/rgarch.log'", root);
    CHECK(system(cmd) != 0);

    /* missing ROM: error page, then exit 1 (timeout instead of a B press) */
    pid = start("/storage/roms/test/missing.tst", NULL);
    CHECK_INT(wait_exit(pid), 1);

    /* power key on the error page: quits at once instead of waiting for the timeout */
    err_timeout = "30";
    pid = start("/storage/roms/test/missing.tst", NULL);
    usleep(700000);
    kill(pid, SIGTERM);
    int t0 = (int)time(NULL);
    CHECK_INT(wait_exit(pid), 0);
    CHECK((int)time(NULL) - t0 < 3);
    err_timeout = "1";

    /* SIGTERM before the first frame of a --resume start keeps the old auto state */
    snprintf(p, sizeof p, "%s/storage/states/test/Game (USA).auto", root);
    FILE *af = fopen(p, "wb"); fputs("OLDSTATE", af); fclose(af);
    term_early = "1";
    pid = start(rom, "--resume");
    CHECK_INT(wait_exit(pid), 0);
    term_early = NULL;
    char got[16] = "";
    af = fopen(p, "rb"); if (af) { fread(got, 1, 8, af); fclose(af); }
    CHECK_STR(got, "OLDSTATE");

    /* a --resume whose load fails must not overwrite the auto state on exit */
    af = fopen(p, "wb"); fputs("BAD", af); fclose(af);      /* too short: load fails */
    pid = start(rom, "--resume");
    usleep(800000);
    kill(pid, SIGTERM);
    CHECK_INT(wait_exit(pid), 0);
    memset(got, 0, sizeof got);
    char failed[1100];
    snprintf(failed, sizeof failed, "%s.failed", p);
    af = fopen(failed, "rb"); if (af) { fread(got, 1, 8, af); fclose(af); }
    CHECK_STR(got, "BAD");                     /* kept aside */
    CHECK(access(p, F_OK) == 0);               /* and the power key works again */

    /* achievements: a synced game starts them; a resume load resets them (no trigger from old hit counts) */
    char nrom[512], cd[600];
    snprintf(nrom, sizeof nrom, "%s/storage/roms/nes/Game.tst", root);
    snprintf(cd, sizeof cd, "%s/storage/.config/mintos/cheevos", root);
    snprintf(cmd, sizeof cmd, "mkdir -p '%s/storage/roms/nes' '%s/game' && echo x > '%s' && printf '[nes]\\ncore = testcore\\n' >> '%s/storage/.config/rgarch/systems.cfg' && "
             "printf 'User\\nApiToken\\n' > '%s/token' && "
             "printf '{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":1}' > '%s/login.json' && "
             "printf '%%s\\t%%s\\t%%s\\t0123456789abcdef0123456789abcdef\\t9\\t1\\n' '%s' $(stat -c '%%s %%Y' '%s') > '%s/hashes.tsv' && "
             "printf '{\"Success\":true,\"GameId\":9,\"Title\":\"T\",\"ConsoleId\":7,\"ImageIconUrl\":\"\",\"RichPresenceGameId\":9,\"RichPresencePatch\":\"\",\"Sets\":[{\"AchievementSetId\":1,\"GameId\":9,\"Title\":null,\"Type\":\"core\",\"ImageIconUrl\":\"\","
             "\"Achievements\":[{\"ID\":101,\"Title\":\"A\",\"Description\":\"D\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"0xH0002=1\",\"Author\":\"A\",\"BadgeName\":\"00101\",\"Created\":1,\"Modified\":1}],\"Leaderboards\":[]}]}' "
             "> '%s/game/0123456789abcdef0123456789abcdef.json'",
             root, cd, nrom, root, cd, cd, nrom, nrom, cd, cd);
    CHECK_INT(system(cmd), 0);
    sysname = "nes";
    pid = start(nrom, NULL);
    usleep(800000);
    kill(pid, SIGTERM);
    CHECK_INT(wait_exit(pid), 0);
    snprintf(cmd, sizeof cmd, "grep -q 'cheevos: game 9 .*: 0/1 Erfolge' '%s/var/log/rgarch.log'", root);
    CHECK_INT(system(cmd), 0);
    pid = start(nrom, "--resume");
    usleep(800000);
    kill(pid, SIGTERM);
    CHECK_INT(wait_exit(pid), 0);
    snprintf(cmd, sizeof cmd, "sed -n '/load state -1: ok/,$p' '%s/var/log/rgarch.log' | grep -q 'cheevos reset'", root);
    CHECK_INT(system(cmd), 0);
    sysname = "test";

    /* usage */
    pid = fork();
    if (!pid) { execl("./rgarch-host", "rgarch", (char *)NULL); _exit(127); }
    CHECK_INT(wait_exit(pid), 2);
    snprintf(cmd, sizeof cmd, "rm -rf %s", root); system(cmd);
    TEST_END();
}
