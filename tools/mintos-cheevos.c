/* RetroAchievements sync for the MintOS settings and the wifi trigger.
 *   mintos-cheevos login <user>   password on stdin; prints ok | Falsches Passwort | Kein Netz
 *   mintos-cheevos logout
 *   mintos-cheevos sync           upload queued unlocks, hash ROMs, fetch game data;
 *                                 prints "<n> Spiele, <m> Erfolge hochgeladen" | Neu anmelden | Kein Netz
 * exit: 0 ok, 1 wrong password / token invalid, 2 no network. Data: $MINTOS_CHEEVOS_DIR or /storage/.config/mintos/cheevos. */
#include "cheevos_sync.h"
#include "cheevos_store.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>

void rg_log(const char *fmt, ...) {   /* archive.c logs through this */
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static int usage(void) {
    puts("mintos-cheevos login <user> | logout | sync");
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "logout") && argc == 2) return cs_logout();
    if (!strcmp(argv[1], "login") && argc == 3) {
        char pw[256] = "";
        if (!fgets(pw, sizeof pw, stdin)) pw[0] = 0;
        pw[strcspn(pw, "\r\n")] = 0;
        int r = cs_login(cs_http_post, argv[2], pw);
        memset(pw, 0, sizeof pw);
        puts(r == 0 ? "ok" : r == 1 ? "Falsches Passwort" : "Kein Netz");
        return r == 0 ? 0 : r == 1 ? 1 : 2;
    }
    if (!strcmp(argv[1], "sync") && argc == 2) {
        /* own scratch dir: archive_prepare must not clobber a running game's /tmp/rgarch-rom */
        mkdir("/tmp/mintos-cheevos", 0755);
        setenv("TMPDIR", "/tmp/mintos-cheevos", 1);
        char lock[1024];
        if (!cs_path(lock, sizeof lock, "sync.lock") && (access(lock, F_OK) == 0 || cs_write("sync.lock", "", 0) == 0)) {   /* never replaced afterwards: the lock is on this inode */
            int fd = open(lock, O_RDONLY);
            if (fd >= 0) flock(fd, LOCK_EX);   /* one sync at a time; released when we exit */
        }
        CsReport r;
        int rc = cs_sync(cs_http_post, "/storage/roms", &r);
        if (rc == 0) { printf("%d Spiele, %d Erfolge hochgeladen\n", r.games, r.uploaded); return 0; }
        puts(rc == 1 ? "Neu anmelden" : "Kein Netz");
        return rc == 1 ? 1 : 2;
    }
    return usage();
}
