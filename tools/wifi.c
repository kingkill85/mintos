#include "wifi.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CP(dst, src) snprintf(dst, sizeof dst, "%s", src)

static const char *prop(const Prop *p, int n, const char *path, const char *iface, const char *key) {
    for (int i = 0; i < n; i++)
        if (!strcmp(p[i].path, path) && !strcmp(p[i].iface, iface) && !strcmp(p[i].key, key)) return p[i].val;
    return NULL;
}

static WNet *net_of(WState *st, const char *path) {
    for (int i = 0; i < st->nnet; i++)
        if (!strcmp(st->nets[i].path, path)) return &st->nets[i];
    if (st->nnet == (int)(sizeof st->nets / sizeof *st->nets)) return NULL;
    WNet *w = &st->nets[st->nnet++];
    memset(w, 0, sizeof *w);
    CP(w->path, path);
    w->signal = -10000;
    return w;
}

void wifi_parse(const Prop *p, int n, WState *st) {
    memset(st, 0, sizeof *st);
    for (int i = 0; i < n; i++) {
        if (!strcmp(p[i].iface, "net.connman.iwd.KnownNetwork") && !strcmp(p[i].key, "Name")
            && st->nknown < (int)(sizeof st->known / sizeof *st->known)) {
            CP(st->known[st->nknown].name, p[i].val);
            CP(st->known[st->nknown++].path, p[i].path);
        }
        /* the device: the first path (sorted) with a Device interface */
        if (!strcmp(p[i].iface, "net.connman.iwd.Device") && !p[i].key[0]
            && (!st->has_device || strcmp(p[i].path, st->device) < 0)) {
            st->has_device = 1;
            CP(st->device, p[i].path);
        }
        if (!strcmp(p[i].iface, "net.connman.iwd.Network")) {
            WNet *w = net_of(st, p[i].path);
            if (!w) continue;
            if (!strcmp(p[i].key, "Name")) CP(w->name, p[i].val);
            else if (!strcmp(p[i].key, "Type")) CP(w->type, p[i].val);
            else if (!strcmp(p[i].key, "KnownNetwork")) w->known = 1;
            else if (!strcmp(p[i].key, "Connected")) w->connected = atoi(p[i].val) != 0;
        }
    }
    if (st->has_device) {
        const char *v = prop(p, n, st->device, "net.connman.iwd.Device", "Powered");
        st->powered = v && atoi(v);
        v = prop(p, n, st->device, "net.connman.iwd.Station", "State");
        if (v) CP(st->state, v);
    }
    for (int i = 0; i < st->nnet; i++)
        if (st->nets[i].connected) CP(st->ssid, st->nets[i].name);
}

void wifi_offline(int switched_off, WState *st) {
    memset(st, 0, sizeof *st);
    if (switched_off) { st->has_device = 1; CP(st->device, "off"); }
}

void wifi_status_text(const WState *st, char *out, size_t n) {
    snprintf(out, n, "device=%d\npowered=%d\nstate=%s\nssid=%s", st->has_device, st->powered, st->state, st->ssid);
}

int wifi_bars(int signal) {
    return signal >= -6000 ? 4 : signal >= -7000 ? 3 : signal >= -7800 ? 2 : signal >= -8800 ? 1 : 0;
}

static int net_cmp(const void *a, const void *b) {
    const WNet *x = a, *y = b;
    if (x->connected != y->connected) return y->connected - x->connected;
    if (x->known != y->known) return y->known - x->known;
    return y->signal - x->signal;
}

void wifi_sort(WNet *nets, int n) { qsort(nets, n, sizeof *nets, net_cmp); }

const WNet *wifi_find(const WState *st, const char *name) {
    for (int i = 0; i < st->nnet; i++)
        if (!strcmp(st->nets[i].name, name)) return &st->nets[i];
    return NULL;
}

const char *wifi_known_path(const WState *st, const char *name) {
    for (int i = 0; i < st->nknown; i++)
        if (!strcmp(st->known[i].name, name)) return st->known[i].path;
    return NULL;
}

void iwd_file(const char *name, const char *type, char *out, size_t n) {
    int plain = *name != 0;
    for (const char *c = name; *c && plain; c++)
        plain = (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9')
             || *c == ' ' || *c == '_' || *c == '-';
    if (plain) { snprintf(out, n, "%s.%s", name, type); return; }
    size_t o = 0;
    if (o + 1 < n) out[o++] = '=';
    for (const unsigned char *c = (const unsigned char *)name; *c && o + 2 < n; c++)
        o += snprintf(out + o, n - o, "%02x", *c);
    snprintf(out + o, n - o, ".%s", type);
}

int write_psk(const char *dir, const char *name, const char *passphrase, char *out, size_t n) {
    char file[300], tmp[600];
    iwd_file(name, "psk", file, sizeof file);
    snprintf(out, n, "%s/%s", dir, file);
    snprintf(tmp, sizeof tmp, "%s.tmp", out);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return -1;
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); return -1; }
    fprintf(f, "[Security]\nPassphrase=%s\n", passphrase);
    if (fclose(f)) return -1;
    return rename(tmp, out);
}
