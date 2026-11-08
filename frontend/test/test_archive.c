// uses: frontend/archive.c frontend/core.c frontend/paths.c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"
#include "archive.h"

int main(void) {
    char dir[] = "/tmp/rgarch-arc-XXXXXX", cmd[2048], out[1024], zip[1024], sz[1024];
    mkdtemp(dir);
    setenv("TMPDIR", dir, 1);
    snprintf(cmd, sizeof cmd, "cd %s && echo ROM > 'Game (USA).tst' && echo r > readme.txt && "
             "zip -q game.zip readme.txt 'Game (USA).tst' && 7z a -bd -y game.7z 'Game (USA).tst' >/dev/null && echo junk > bad.zip", dir);
    CHECK_INT(system(cmd), 0);
    snprintf(zip, sizeof zip, "%s/game.zip", dir);
    snprintf(sz, sizeof sz, "%s/game.7z", dir);

    /* the core takes zips itself: path unchanged */
    CHECK_INT(archive_prepare(zip, "zip|tst", out, sizeof out), 0);
    CHECK_STR(out, zip);
    /* plain ROM: unchanged */
    CHECK_INT(archive_prepare("/x/a.tst", "tst", out, sizeof out), 0);
    CHECK_STR(out, "/x/a.tst");
    /* the core does not take zips: extract the file with a valid extension */
    CHECK_INT(archive_prepare(zip, "tst", out, sizeof out), 0);
    CHECK(strstr(out, "Game (USA).tst") != NULL);
    CHECK(access(out, R_OK) == 0);
    CHECK_INT(archive_prepare(sz, "tst|bin", out, sizeof out), 0);
    CHECK(strstr(out, "Game (USA).tst") != NULL);
    /* no matching file / broken archive */
    CHECK_INT(archive_prepare(zip, "gba", out, sizeof out), -1);
    snprintf(zip, sizeof zip, "%s/bad.zip", dir);
    CHECK_INT(archive_prepare(zip, "tst", out, sizeof out), -1);
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir); system(cmd);
    TEST_END();
}
