// uses:
// so: frontend/test/testcore.c
// bin: rgarch-host frontend/main.c frontend/core.c frontend/paths.c frontend/config.c frontend/viewport.c frontend/video.c frontend/audio.c frontend/ratecontrol.c frontend/input.c frontend/inputmap.c frontend/state.c frontend/menu.c common/ui.c
/* Runs a host build of rgarch with the test core (SDL dummy drivers). */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "check.h"

static char root[] = "/tmp/rgarch-main-XXXXXX";

static pid_t start(const char *rom, const char *extra) {
    pid_t pid = fork();
    if (pid == 0) {
        char core[1024];
        snprintf(core, sizeof core, "%s/testcore.so", getcwd(NULL, 0));
        setenv("RGARCH_ROOT", root, 1);
        setenv("RGARCH_CORE", core, 1);
        setenv("RGARCH_ERROR_TIMEOUT", "1", 1);
        if (extra) execl("./rgarch-host", "rgarch", extra, "test", rom, (char *)NULL);
        else execl("./rgarch-host", "rgarch", "test", rom, (char *)NULL);
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
    char rom[512], p[1024], cmd[1400];
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

    /* missing ROM: error page, then exit 1 (timeout instead of a B press) */
    pid = start("/storage/roms/test/missing.tst", NULL);
    CHECK_INT(wait_exit(pid), 1);

    /* usage */
    pid = fork();
    if (!pid) { execl("./rgarch-host", "rgarch", (char *)NULL); _exit(127); }
    CHECK_INT(wait_exit(pid), 2);
    snprintf(cmd, sizeof cmd, "rm -rf %s", root); system(cmd);
    TEST_END();
}
