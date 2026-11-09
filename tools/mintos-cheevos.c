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

/* archive.c logs through this. Silent unless MINTOS_CHEEVOS_DEBUG is set: job.c runs us with 2>&1 and the
 * settings show the first output line, which must be our result line. */
void rg_log(const char *fmt, ...) {
    if (!getenv("MINTOS_CHEEVOS_DEBUG")) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

/* login, logout and sync share one lock (blocking), so an account switch cannot race a running sync.
 * The lock file is created in place and never replaced, so it stays on one inode; released on exit. 0 ok. */
static int take_lock(void) {
    char lock[1024], dir[1024];
    snprintf(dir, sizeof dir, "%s/", cs_dir());   /* mkdir -p of the data dir */
    for (char *s = dir + 1; *s; s++) if (*s == '/') { *s = 0; mkdir(dir, 0755); *s = '/'; }
    int fd = cs_path(lock, sizeof lock, "sync.lock") ? -1 : open(lock, O_RDWR | O_CREAT, 0600);
    if (fd < 0 || flock(fd, LOCK_EX)) { puts("Fehler: Sperre"); return -1; }
    return 0;
}

static int usage(void) {
    puts("mintos-cheevos login <user> | logout | sync");
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "logout") && argc == 2) return take_lock() ? 1 : cs_logout();
    if (!strcmp(argv[1], "login") && argc == 3) {
        char pw[256] = "";
        if (take_lock()) return 1;
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
        if (take_lock()) return 1;   /* one sync at a time */
        CsReport r;
        int rc = cs_sync(cs_http_post, "/storage/roms", &r);
        if (rc == 0) { printf("%d Spiele, %d Erfolge hochgeladen\n", r.games, r.uploaded); return 0; }
        puts(rc == 1 ? "Neu anmelden" : "Kein Netz");
        return rc == 1 ? 1 : 2;
    }
    return usage();
}
