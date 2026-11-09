#include "mintosd_helpers.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int bt_sink(const char *pcms, char *out, size_t n) {
    static const char suffix[] = "/a2dpsrc/sink";
    const char *p = pcms;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        const char *s = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        size_t len = p - s;
        if (len >= sizeof suffix - 1 && !memcmp(p - (sizeof suffix - 1), suffix, sizeof suffix - 1) && len < n) {
            memcpy(out, s, len);
            out[len] = 0;
            return 1;
        }
    }
    if (n) out[0] = 0;
    return 0;
}

void bt_mac(const char *path, char out[18]) {
    out[0] = 0;
    for (const char *p = strstr(path, "dev_"); p; p = strstr(p + 1, "dev_")) {
        const char *m = p + 4;
        int ok = 1;
        for (int i = 0; i < 17 && ok; i++)
            ok = i % 3 == 2 ? m[i] == '_' : (isdigit((unsigned char)m[i]) || (m[i] >= 'A' && m[i] <= 'F'));
        if (!ok) continue;
        for (int i = 0; i < 17; i++) out[i] = m[i] == '_' ? ':' : m[i];
        out[17] = 0;
        return;
    }
}

void bt_conf(const char *mac, char *out, size_t n) {
    snprintf(out, n, "pcm.!default {\n  type plug\n  slave.pcm \"bluealsa:DEV=%s,PROFILE=a2dp\"\n}\n", mac);
}

int bt_volume_parse(const char *out) {
    const char *p = strstr(out, "Volume:");
    if (!p) return -1;
    for (p += 7; *p && !isdigit((unsigned char)*p); p++) {}
    if (!*p) return -1;
    int v = 0;
    while (isdigit((unsigned char)*p)) v = v * 10 + (*p++ - '0');
    return v;
}

int bt_step(int v, int d) { return clamp(v + 6 * d, 0, 127); }

int bl_step(int raw, int mx) {
    if (mx <= 0) return 10;
    return clamp((raw * 10 + mx / 2) / mx, 1, 10);
}

int bl_raw(int step, int mx) { return mx * clamp(step, 1, 10) / 10; }

int master_pct(const char *out) {
    for (const char *p = strstr(out, "Playback"); p; p = strstr(p + 1, "Playback")) {
        const char *eol = strchr(p, '\n');
        for (const char *b = strchr(p, '['); b && (!eol || b < eol); b = strchr(b + 1, '[')) {
            int v, len;
            if (sscanf(b, "[%d%%]%n", &v, &len) == 1 && len > 0) return v;
        }
    }
    return -1;
}

void osd_line(long long seq, const char *kind, int value, char *out, size_t n) {
    snprintf(out, n, "%lld %s %d\n", seq, kind, clamp(value, 0, 10));
}
