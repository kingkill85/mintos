#include "archive.h"
#include "core.h"   /* rg_log */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

static int has_ext(const char *list, const char *ext) {
    size_t n = strlen(ext);
    for (const char *p = list; p && *p;) {
        const char *bar = strchr(p, '|');
        size_t len = bar ? (size_t)(bar - p) : strlen(p);
        if (len == n && !strncasecmp(p, ext, n)) return 1;
        p = bar ? bar + 1 : NULL;
    }
    return 0;
}

/* runs argv without a shell (file names may contain anything) */
static int run(char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        execvp(argv[0], argv);
        _exit(127);
    }
    int st = 0;
    if (pid < 0 || waitpid(pid, &st, 0) < 0) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

int archive_prepare(const char *rom, const char *valid, char *out, size_t n) {
    const char *dot = strrchr(rom, '.');
    const char *ext = dot ? dot + 1 : "";
    int zip = !strcasecmp(ext, "zip"), sz = !strcasecmp(ext, "7z");
    if ((!zip && !sz) || has_ext(valid, ext)) { snprintf(out, n, "%s", rom); return 0; }

    const char *tmp = getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp";
    char dir[512], opt[600];
    snprintf(dir, sizeof dir, "%s/rgarch-rom", tmp);
    char *rm[] = {"rm", "-rf", dir, NULL};
    run(rm);
    snprintf(opt, sizeof opt, "-o%s", dir);
    char *unzip[] = {"unzip", "-qq", "-o", (char *)rom, "-d", dir, NULL};
    char *seven[] = {"7z", "x", "-y", "-bd", opt, (char *)rom, NULL};
    if (run(zip ? unzip : seven)) { rg_log("cannot extract %s\n", rom); return -1; }

    DIR *d = opendir(dir);
    struct dirent *de;
    int found = 0;
    while (d && (de = readdir(d))) {
        const char *e = strrchr(de->d_name, '.');
        if (de->d_name[0] != '.' && e && has_ext(valid, e + 1)) {
            snprintf(out, n, "%s/%s", dir, de->d_name);
            found = 1;
            break;
        }
    }
    if (d) closedir(d);
    if (!found) rg_log("%s: no file for this core inside\n", rom);
    return found ? 0 : -1;
}
