// uses:
/* Drives os/files/usr/local/sbin/rg353-migrate-saves on a temp tree (ROOT=...). */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"

static char root[] = "/tmp/rgarch-mig-XXXXXX";
static void touch(const char *rel) { char c[2048]; snprintf(c, sizeof c, "mkdir -p \"$(dirname '%s/%s')\" && echo x > '%s/%s'", root, rel, root, rel); system(c); }
static int exists(const char *rel) { char p[1024]; snprintf(p, sizeof p, "%s/%s", root, rel); return access(p, F_OK) == 0; }

int main(void) {
    mkdtemp(root);
    touch("storage/roms/gbc/Zelda (Rev 2).gbc");
    touch("storage/roms/gbc/Zelda (Rev 2).srm");
    touch("storage/roms/gbc/Zelda (Rev 2).rtc");
    touch("storage/roms/gbc/Zelda (Rev 2).state");
    touch("storage/roms/gbc/Zelda (Rev 2).state3");
    touch("storage/roms/gbc/Zelda (Rev 2).state3.png");
    touch("storage/roms/gbc/Zelda (Rev 2).state.auto");
    touch("storage/roms/gb/Mario.srm");
    touch("storage/saves/gb/Mario.srm");   /* already there: keep, do not overwrite */
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "ROOT=%s sh %s/os/files/usr/local/sbin/rg353-migrate-saves >/dev/null", root, getenv("REPO"));
    CHECK_INT(system(cmd), 0);
    CHECK(exists("storage/saves/gbc/Zelda (Rev 2).srm"));
    CHECK(exists("storage/saves/gbc/Zelda (Rev 2).rtc"));
    CHECK(exists("storage/states/gbc/Zelda (Rev 2).st0"));
    CHECK(exists("storage/states/gbc/Zelda (Rev 2).st3"));
    CHECK(exists("storage/states/gbc/Zelda (Rev 2).st3.png"));
    CHECK(exists("storage/states/gbc/Zelda (Rev 2).auto"));
    CHECK(!exists("storage/roms/gbc/Zelda (Rev 2).srm"));
    CHECK(exists("storage/roms/gbc/Zelda (Rev 2).gbc"));
    CHECK(exists("storage/roms/gb/Mario.srm"));            /* skipped: target existed */
    CHECK_INT(system(cmd), 0);                             /* idempotent */
    snprintf(cmd, sizeof cmd, "rm -rf %s", root); system(cmd);
    TEST_END();
}
