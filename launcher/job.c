#include "job.h"
#include <SDL.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static char input_path[64];

static char cmdline[1200], out[16384];
static int kind, rc, has_input;
static SDL_atomic_t state;   /* 0 idle, 1 running, 2 finished */

static int run(void *arg) {
    (void)arg;
    FILE *p = popen(cmdline, "r");
    size_t n = 0;
    if (p) {
        size_t r;
        while (n < sizeof out - 1 && (r = fread(out + n, 1, sizeof out - 1 - n, p)) > 0) n += r;
        int st = pclose(p);
        rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    } else rc = -1;
    out[n] = 0;
    if (has_input) unlink(input_path);
    SDL_AtomicSet(&state, 2);
    return 0;
}

int job_start(int k, const char *cmd, const char *input) {
    if (SDL_AtomicGet(&state) == 1) return -1;
    has_input = input != NULL;
    if (input) {
        /* /run is a tmpfs (the passphrase never reaches the SD card); /tmp on a dev machine */
        snprintf(input_path, sizeof input_path, "%s/mintos-job-input", access("/run", W_OK) ? "/tmp" : "/run");
        int fd = open(input_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) return -1;
        if (write(fd, input, strlen(input)) < 0 || write(fd, "\n", 1) < 0) {}
        close(fd);
        snprintf(cmdline, sizeof cmdline, "%s < %s 2>&1", cmd, input_path);
    } else {
        snprintf(cmdline, sizeof cmdline, "%s 2>&1", cmd);
    }
    kind = k;
    SDL_AtomicSet(&state, 1);
    SDL_Thread *t = SDL_CreateThread(run, "job", NULL);
    if (!t) { SDL_AtomicSet(&state, 0); return -1; }
    SDL_DetachThread(t);
    return 0;
}

int job_busy(void) { return SDL_AtomicGet(&state) == 1; }
int job_kind(void) { return kind; }

int job_take(int *k, int *r, const char **o) {
    if (SDL_AtomicGet(&state) != 2) return 0;
    SDL_AtomicSet(&state, 0);
    *k = kind; *r = rc; *o = out;
    return 1;
}

void sh_quote(char *dst, int n, const char *s) {
    int j = 0;
    if (n < 3) { if (n) *dst = 0; return; }
    dst[j++] = '\'';
    for (; *s && j < n - 6; s++) {
        if (*s == '\'') { memcpy(dst + j, "'\\''", 4); j += 4; }
        else dst[j++] = *s;
    }
    dst[j++] = '\'';
    dst[j] = 0;
}
