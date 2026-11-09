#include "fb.h"
#include <drm/drm.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <unistd.h>

static const char STATE[] = "/sys/kernel/debug/dri/display-subsystem/state";

static int fb_id(void) {
    if (access("/sys/kernel/debug/dri", F_OK)) mount("none", "/sys/kernel/debug", "debugfs", 0, NULL);
    FILE *f = fopen(STATE, "r");
    if (!f) return -1;
    static char s[1 << 16];
    size_t n = fread(s, 1, sizeof s - 1, f);
    s[n] = 0;
    fclose(f);
    const char *p = strstr(s, "Esmart0-win0\n\tcrtc=");
    if (!p) return -1;
    p = strchr(p + 19, '\n');
    if (!p || strncmp(p, "\n\tfb=", 5)) return -1;
    return atoi(p + 5);
}

static int open_card(void) {   /* the rockchip-drm card */
    for (int i = 0; i < 8; i++) {
        char link[64], target[PATH_MAX], dev[32];
        snprintf(link, sizeof link, "/sys/class/drm/card%d/device/driver", i);
        if (!realpath(link, target) || strcmp(basename(target), "rockchip-drm")) continue;
        snprintf(dev, sizeof dev, "/dev/dri/card%d", i);
        return open(dev, O_RDWR | O_CLOEXEC);
    }
    return -1;
}

int fb_open(Fb *fb) {
    memset(fb, 0, sizeof *fb);
    fb->fd = fb->dfd = -1;
    int id = fb_id();
    if (id <= 0 || (fb->fd = open_card()) < 0) return -1;
    struct drm_mode_fb_cmd2 c = { .fb_id = id };
    if (ioctl(fb->fd, DRM_IOCTL_MODE_GETFB2, &c) || !c.handles[0]) { fb_close(fb); return -1; }
    struct drm_prime_handle ph = { .handle = c.handles[0], .flags = DRM_CLOEXEC | DRM_RDWR, .fd = -1 };
    if (ioctl(fb->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &ph)) { fb_close(fb); return -1; }
    fb->dfd = ph.fd;
    fb->w = c.width; fb->h = c.height; fb->pitch = c.pitches[0]; fb->fmt = c.pixel_format;
    fb->size = (size_t)fb->pitch * fb->h;
    void *m = mmap(NULL, fb->size, PROT_READ, MAP_SHARED, fb->dfd, 0);
    if (m == MAP_FAILED) { fb_close(fb); return -1; }
    fb->px = m;
    return 0;
}

void fb_close(Fb *fb) {
    if (fb->px) munmap((void *)fb->px, fb->size);
    if (fb->dfd >= 0) close(fb->dfd);
    if (fb->fd >= 0) close(fb->fd);
    fb->px = NULL; fb->dfd = fb->fd = -1;
}
