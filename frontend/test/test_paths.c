// uses: frontend/paths.c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "check.h"
#include "paths.h"

int main(void) {
    char out[1024];
    const char *z = "/storage/roms/gbc/Legend of Zelda, The - Link's Awakening DX (USA, Europe) (Rev 2).gbc";
    const char *zn = "Legend of Zelda, The - Link's Awakening DX (USA, Europe) (Rev 2)";
    char want[1024];

    CHECK_INT(path_for(P_SRAM, z, 0, out, sizeof out), 0);
    snprintf(want, sizeof want, "/storage/saves/gbc/%s.srm", zn); CHECK_STR(out, want);
    path_for(P_RTC, z, 0, out, sizeof out);
    snprintf(want, sizeof want, "/storage/saves/gbc/%s.rtc", zn); CHECK_STR(out, want);
    path_for(P_STATE, z, 3, out, sizeof out);
    snprintf(want, sizeof want, "/storage/states/gbc/%s.st3", zn); CHECK_STR(out, want);
    path_for(P_PREVIEW, z, 3, out, sizeof out);
    snprintf(want, sizeof want, "/storage/states/gbc/%s.st3.png", zn); CHECK_STR(out, want);
    path_for(P_AUTO, z, 0, out, sizeof out);
    snprintf(want, sizeof want, "/storage/states/gbc/%s.auto", zn); CHECK_STR(out, want);

    const char *m2 = "/storage/roms/scummvm/monkey2-de/monkey2.scummvm";
    CHECK_INT(path_system(m2, out, sizeof out), 0); CHECK_STR(out, "scummvm");
    path_for(P_SRAM, m2, 0, out, sizeof out); CHECK_STR(out, "/storage/saves/scummvm/monkey2.srm");

    CHECK_INT(path_for(P_SRAM, "/tmp/x.gb", 0, out, sizeof out), -1);
    path_for(P_SRAM, "/storage/roms/gbc/Pok\xc3\xa9mon Gelb.gbc", 0, out, sizeof out);
    CHECK_STR(out, "/storage/saves/gbc/Pok\xc3\xa9mon Gelb.srm");
    path_for(P_SRAM, "/storage/roms/gb/noext", 0, out, sizeof out);
    CHECK_STR(out, "/storage/saves/gb/noext.srm");

    /* RGARCH_ROOT moves everything (tests, Task 9) */
    setenv("RGARCH_ROOT", "/tmp/rgr", 1);
    path_for(P_STATE, "/storage/roms/gb/A.gb", 1, out, sizeof out);
    CHECK_STR(out, "/tmp/rgr/storage/states/gb/A.st1");
    unsetenv("RGARCH_ROOT");

    /* mkdirs_for creates the parent directories */
    char tmpl[] = "/tmp/rgarch-mk-XXXXXX";
    char *d = mkdtemp(tmpl);
    snprintf(want, sizeof want, "%s/a/b/c.srm", d);
    CHECK_INT(mkdirs_for(want), 0);
    snprintf(out, sizeof out, "%s/a/b", d);
    struct stat st;
    CHECK(stat(out, &st) == 0 && S_ISDIR(st.st_mode));
    TEST_END();
}
