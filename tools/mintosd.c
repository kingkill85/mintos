/* mintosd: the RG353V's input/audio/power helper in one small process (replaces the Python
 * daemons rg353-pad, rg353-audio and rg353-power, ~60 MB RAM).
 *
 * Gamepad: the kernel splits the controls into gpio-keys-control (buttons, D-pad, stick
 *   clicks), adc-keys (the F key, BTN_MODE) and adc-joystick (both sticks). RetroArch only
 *   combines buttons of one device, so they are grabbed and re-emitted as one uinput device
 *   "RG353V Gamepad". adc-joystick reports up as +; gamepads expect up = -: the Y axes are mirrored.
 * Audio (rk817 on mainline): output goes to the speaker, or to the headphones while a plug is in
 *   (mainline leaves "Playback Mux" on HP). The volume buttons (gpio-keys-vol) change "Master" in
 *   5 % steps of the perceptual scale; with F held the brightness in the settings' 10 steps.
 *   Each change goes to /run/mintos-osd, shown by rglauncher and rgarch (common/osd.c).
 *   While Bluetooth headphones (bluealsa) are connected, ALSA's default device points to them
 *   (/run/mintos/asound-bt.conf, loaded by /etc/asound.conf) and the buttons change their volume.
 * Power key (rk805 pwrkey), like Garlic OS: during a game (rgarch or RetroArch) the game is
 *   remembered in /storage/.config/rglauncher/resume, the launcher service is stopped (the game
 *   writes its auto save state on SIGTERM) and the device powers off; rglauncher starts that game
 *   again on boot with RESUME=1. logind's own power key handling is off (logind.conf.d/rg353v.conf).
 */
#include "mintosd_helpers.h"
#include "util.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BITS(n) (((n) + 8 * sizeof(long) - 1) / (8 * sizeof(long)))
#define TEST(b, i) (((b)[(i) / (8 * sizeof(long))] >> ((i) % (8 * sizeof(long)))) & 1)

enum { PAD, JACK, VOL, POWER };   /* what a device is for */
static const char *const PAD_SOURCES[] = { "gpio-keys-control", "adc-keys", "adc-joystick" };
#define MAXDEV 8

static struct { int fd, role; } dev[MAXDEV];
static int ndev, uifd = -1, flip[ABS_CNT];   /* flip[code]: min+max of a mirrored axis */
static char flipped[ABS_CNT];
static atomic_int f_held;   /* set by the pad thread, read by the audio thread */
static char bt_current[256];
static pid_t power_pid;

static const char CURRENT[] = "/run/rglauncher-current", RESUME[] = "/storage/.config/rglauncher/resume";
static const char BT_CONF[] = "/run/mintos/asound-bt.conf", OSD_FILE[] = "/run/mintos-osd";
static const char BACKLIGHT[] = "/sys/class/backlight/backlight";

#define RUN(out, ...) RUN_OUT(out, 5000, __VA_ARGS__)
#define RUNQ(...) RUN_Q(5000, __VA_ARGS__)

static void write_file(const char *path, const char *s) {   /* atomically, via path.tmp */
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fputs(s, f);
    if (fclose(f) == 0) rename(tmp, path);
}

/* ---- audio ---- */

static void route(void) {
    char out[512];
    RUN(out, "amixer", "-c", "rk817int", "cget", "iface=CARD,name=Headphones Jack");
    const char *o = strstr(out, ": values=on") ? "HP" : "SPK";
    RUNQ("amixer", "-c", "rk817int", "-q", "sset", "Playback Mux", (char *)o);
    printf("audio -> %s\n", o);
}

static void osd(const char *kind, int value) {
    struct timespec t;
    char line[64];
    clock_gettime(CLOCK_REALTIME, &t);
    osd_line(t.tv_sec * 1000000000LL + t.tv_nsec, kind, value, line, sizeof line);
    write_file(OSD_FILE, line);
}

static int read_int(const char *dir, const char *name) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "r");
    int v = -1;
    if (f) { if (fscanf(f, "%d", &v) != 1) v = -1; fclose(f); }
    return v;
}

static void brightness(int d) {
    int mx = read_int(BACKLIGHT, "max_brightness"), raw = read_int(BACKLIGHT, "brightness");
    if (mx < 0 || raw < 0) return;
    int s = bl_step(raw, mx) + d;
    s = s < 1 ? 1 : s > 10 ? 10 : s;
    char path[256];
    snprintf(path, sizeof path, "%s/brightness", BACKLIGHT);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d\n", bl_raw(s, mx));
    if (fclose(f) == 0) osd("bright", s);
}

static void volume(int d) {
    char out[1024];
    if (f_held) { brightness(d); return; }
    if (bt_current[0]) {
        RUN(out, "bluealsa-cli", "volume", bt_current);
        int v = bt_volume_parse(out);
        if (v < 0) return;
        char num[16];
        v = bt_step(v, d);
        snprintf(num, sizeof num, "%d", v);
        RUNQ("bluealsa-cli", "volume", bt_current, num);
        osd("vol", (v * 10 + 63) / 127);
        return;
    }
    /* -M: perceptual scale (as alsamixer); the raw one is linear in dB from -95 to 0 dB,
     * so its 50 % (-47 dB) sounded like 10 % */
    RUN(out, "amixer", "-c", "rk817int", "-M", "sset", "Master", d > 0 ? "5%+" : "5%-");
    int v = master_pct(out);
    if (v >= 0) osd("vol", (v + 5) / 10);
}

static void bt_watch(void) {
    char out[2048], sink[256];
    RUN(out, "bluealsa-cli", "list-pcms");
    bt_sink(out, sink, sizeof sink);
    if (!strcmp(sink, bt_current)) return;
    strcpy(bt_current, sink);
    if (sink[0]) {
        char mac[18], conf[256];
        bt_mac(sink, mac);
        bt_conf(mac, conf, sizeof conf);
        mkdir("/run/mintos", 0755);
        write_file(BT_CONF, conf);
        printf("audio -> bluetooth %s\n", mac);
    } else {
        unlink(BT_CONF);
        printf("audio -> device\n");
    }
}

/* ---- power ---- */

static int running(const char *name) {   /* as pgrep -x */
    DIR *d = opendir("/proc");
    struct dirent *e;
    int found = 0;
    while (d && !found && (e = readdir(d))) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char path[300], comm[32] = "";
        snprintf(path, sizeof path, "/proc/%s/comm", e->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (fgets(comm, sizeof comm, f)) comm[strcspn(comm, "\n")] = 0;
        fclose(f);
        found = !strcmp(comm, name);
    }
    if (d) closedir(d);
    return found;
}

static void power_off(void) {   /* in a child: the pad keeps working while the game saves */
    if (power_pid > 0) return;
    power_pid = fork();
    if (power_pid != 0) return;
    if ((running("rgarch") || running("retroarch")) && access(CURRENT, R_OK) == 0) {
        char game[1024] = "";
        FILE *f = fopen(CURRENT, "r");
        if (f) { size_t r = fread(game, 1, sizeof game - 1, f); game[r] = 0; fclose(f); }
        write_file(RESUME, game);
        game[strcspn(game, "\n")] = 0;
        printf("saving %s\n", game);
        fflush(stdout);
    }
    RUNQ("systemctl", "stop", "rglauncher");   /* SIGTERM to the game, waits for it */
    sync();
    RUNQ("systemctl", "poweroff");
    _exit(0);
}

/* ---- devices ---- */

static void emit(int type, int code, int value) { input_emit(uifd, type, code, value); }

static int is_pad_source(const char *name) {
    for (size_t i = 0; i < sizeof PAD_SOURCES / sizeof *PAD_SOURCES; i++)
        if (!strcmp(name, PAD_SOURCES[i])) return 1;
    return 0;
}

static void open_devices(void) {
    for (int i = 0; i < 64 && ndev < MAXDEV; i++) {
        char path[32], name[128] = "";
        snprintf(path, sizeof path, "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        ioctl(fd, EVIOCGNAME(sizeof name - 1), name);
        size_t len = strlen(name);
        int role = is_pad_source(name) ? PAD
                 : !strcmp(name, "gpio-keys-vol") ? VOL
                 : !strcmp(name, "rk805 pwrkey") ? POWER
                 : len >= 10 && !strcmp(name + len - 10, "Headphones") ? JACK : -1;
        if (role < 0) { close(fd); continue; }
        if (role != JACK && ioctl(fd, EVIOCGRAB, 1)) perror(name);
        dev[ndev].fd = fd;
        dev[ndev++].role = role;
        printf("%s: %s\n", path, name);
    }
}

static int create_pad(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { perror("/dev/uinput"); return -1; }
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    int axes = 0;
    for (int i = 0; i < ndev; i++) {
        if (dev[i].role != PAD) continue;
        unsigned long keys[BITS(KEY_CNT)] = { 0 }, abs[BITS(ABS_CNT)] = { 0 };
        ioctl(dev[i].fd, EVIOCGBIT(EV_KEY, sizeof keys), keys);
        ioctl(dev[i].fd, EVIOCGBIT(EV_ABS, sizeof abs), abs);
        for (int k = 0; k < KEY_CNT; k++)
            if (TEST(keys, k)) ioctl(fd, UI_SET_KEYBIT, k);
        for (int a = 0; a < ABS_CNT; a++) {
            if (!TEST(abs, a)) continue;
            struct uinput_abs_setup s = { .code = a };
            if (ioctl(dev[i].fd, EVIOCGABS(a), &s.absinfo)) continue;
            if (!axes++) ioctl(fd, UI_SET_EVBIT, EV_ABS);
            ioctl(fd, UI_SET_ABSBIT, a);
            ioctl(fd, UI_ABS_SETUP, &s);
            if (a == ABS_Y || a == ABS_RY) { flipped[a] = 1; flip[a] = s.absinfo.minimum + s.absinfo.maximum; }
        }
    }
    struct uinput_setup us = { .id = { .bustype = BUS_USB, .vendor = 0x2207, .product = 0x0353, .version = 1 } };
    strcpy(us.name, "RG353V Gamepad");
    if (ioctl(fd, UI_DEV_SETUP, &us) || ioctl(fd, UI_DEV_CREATE)) { perror("uinput create"); close(fd); return -1; }
    return fd;
}

static void handle(int role, const struct input_event *ev) {
    switch (role) {
    case PAD:
        if (ev->type == EV_KEY && ev->code == BTN_MODE) f_held = ev->value != 0;
        if (ev->type == EV_ABS && ev->code < ABS_CNT && flipped[ev->code]) emit(EV_ABS, ev->code, flip[ev->code] - ev->value);
        else if (ev->type == EV_KEY || ev->type == EV_ABS) emit(ev->type, ev->code, ev->value);
        else if (ev->type == EV_SYN) emit(EV_SYN, SYN_REPORT, 0);
        break;
    case JACK:
        if (ev->type == EV_SW && ev->code == SW_HEADPHONE_INSERT) route();
        break;
    case VOL:
        if (ev->type == EV_KEY && (ev->value == 1 || ev->value == 2)) {   /* press or autorepeat */
            if (ev->code == KEY_VOLUMEUP) volume(1);
            else if (ev->code == KEY_VOLUMEDOWN) volume(-1);
        }
        break;
    case POWER:
        if (ev->type == EV_KEY && ev->code == KEY_POWER && ev->value == 1) power_off();
        break;
    }
}

/* poll the devices of one thread: pad forwarding alone in main (never waits for a command),
 * everything else in the second thread (amixer, bluealsa-cli, the Bluetooth check every 2 s) */
static int loop(int pad) {
    struct pollfd pf[MAXDEV];
    int idx[MAXDEV], n = 0;
    for (int i = 0; i < ndev; i++)
        if ((dev[i].role == PAD) == pad) { idx[n] = i; pf[n++] = (struct pollfd){ dev[i].fd, POLLIN, 0 }; }
    struct timespec next = { 0 }, now;
    for (;;) {
        if (!pad) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (now.tv_sec >= next.tv_sec) { bt_watch(); next.tv_sec = now.tv_sec + 2; }
            if (power_pid > 0 && waitpid(power_pid, NULL, WNOHANG) == power_pid) power_pid = 0;
        }
        if (poll(pf, n, pad ? -1 : 1000) < 0 && errno != EINTR) return 1;
        for (int j = 0; j < n; j++) {
            if (pf[j].revents & (POLLERR | POLLHUP | POLLNVAL)) { fprintf(stderr, "device lost\n"); return 1; }
            if (!(pf[j].revents & POLLIN)) continue;
            struct input_event ev[64];
            ssize_t r;
            while ((r = read(pf[j].fd, ev, sizeof ev)) > 0)
                for (size_t k = 0; k < (size_t)r / sizeof *ev; k++) handle(dev[idx[j]].role, &ev[k]);
            if (r < 0 && errno != EAGAIN && errno != EINTR) { perror("read"); return 1; }
        }
    }
}

static void *audio_thread(void *arg) {
    (void)arg;
    route();
    exit(loop(0));   /* a lost device ends the whole daemon; systemd restarts it */
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    open_devices();
    if ((uifd = create_pad()) < 0) return 1;
    pthread_t t;
    if (pthread_create(&t, NULL, audio_thread, NULL)) return 1;
    return loop(1);
}
