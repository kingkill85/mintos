#include "util.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
}

int run_cmd(char *const argv[], const char *input, char *out, size_t n, int timeout_ms) {
    int op[2], ip[2] = { -1, -1 };
    if (out && n) out[0] = 0;
    if (pipe(op)) return -1;
    if (input && pipe(ip)) { close(op[0]); close(op[1]); return -1; }
    pid_t pid = fork();
    if (pid < 0) { close(op[0]); close(op[1]); if (input) { close(ip[0]); close(ip[1]); } return -1; }
    if (pid == 0) {
        int null = open("/dev/null", O_RDWR);
        dup2(input ? ip[0] : null, 0); dup2(op[1], 1); dup2(null, 2);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(op[1]);
    if (input) {   /* small (a passphrase): fits the pipe buffer */
        close(ip[0]);
        if (write(ip[1], input, strlen(input)) < 0) {}
        close(ip[1]);
    }
    size_t got = 0;
    int timed_out = 0;
    long long end = now_ms() + timeout_ms;
    for (;;) {
        int left = (int)(end - now_ms());
        struct pollfd pf = { op[0], POLLIN, 0 };
        if (left <= 0 || poll(&pf, 1, left) <= 0) { kill(pid, SIGKILL); timed_out = 1; break; }
        char buf[4096];
        ssize_t r = read(op[0], buf, sizeof buf);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        if (out && got + 1 < n) {
            size_t c = (size_t)r < n - 1 - got ? (size_t)r : n - 1 - got;
            memcpy(out + got, buf, c);
            got += c;
            out[got] = 0;
        }
    }
    close(op[0]);
    int st;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    return !timed_out && WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

void sleep_ms(int ms) {
    struct timespec t = { ms / 1000, (ms % 1000) * 1000000L };
    while (nanosleep(&t, &t) && errno == EINTR) {}
}

int setting_off(const char *path, const char *key) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[256];
    int off = 0;
    size_t kl = strlen(key);
    while (!off && fgets(line, sizeof line, f)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (strncmp(p, key, kl)) continue;
        p += kl;
        while (isspace((unsigned char)*p)) p++;
        if (*p++ != '=') continue;
        while (isspace((unsigned char)*p)) p++;
        off = !strncmp(p, "off", 3);
    }
    fclose(f);
    return off;
}

int input_find(const char *name, int flags) {
    for (int i = 0; i < 64; i++) {
        char path[32], nm[128] = "";
        snprintf(path, sizeof path, "/dev/input/event%d", i);
        int fd = open(path, flags | O_CLOEXEC);
        if (fd < 0) continue;
        ioctl(fd, EVIOCGNAME(sizeof nm - 1), nm);
        if (!strcmp(nm, name)) return fd;
        close(fd);
    }
    return -1;
}

int uinput_keys(const char *name, const int *codes, int n) {
    int fd = open("/dev/uinput", O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    for (int i = 0; i < n; i++) ioctl(fd, UI_SET_KEYBIT, codes[i]);
    struct uinput_setup us = { .id = { .bustype = BUS_USB, .vendor = 1, .product = 1, .version = 1 } };
    snprintf(us.name, sizeof us.name, "%s", name);
    if (ioctl(fd, UI_DEV_SETUP, &us) || ioctl(fd, UI_DEV_CREATE)) { close(fd); return -1; }
    return fd;
}

void input_emit(int fd, int type, int code, int value) {
    struct input_event ev = { .type = type, .code = code, .value = value };
    if (write(fd, &ev, sizeof ev) < 0) {}
}

void input_tap(int fd, int code, int hold_ms, int after_ms) {
    input_emit(fd, EV_KEY, code, 1); input_emit(fd, EV_SYN, SYN_REPORT, 0);
    sleep_ms(hold_ms);
    input_emit(fd, EV_KEY, code, 0); input_emit(fd, EV_SYN, SYN_REPORT, 0);
    sleep_ms(after_ms);
}
