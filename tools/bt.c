#include "bt.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int mac_at(const char *s, int upper_only) {
    for (int i = 0; i < 17; i++) {
        char c = s[i];
        if (i % 3 == 2) { if (c != ':') return 0; }
        else if (!(isdigit((unsigned char)c) || (c >= 'A' && c <= 'F') || (!upper_only && c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

int bt_parse_devices(const char *out, BtDev *devs, int max) {
    int n = 0;
    for (const char *p = out; *p; ) {
        size_t len = strcspn(p, "\n");
        if (len > 24 && !strncmp(p, "Device ", 7) && mac_at(p + 7, 1) && p[24] == ' ' && p[25] != '(') {
            char mac[18];
            memcpy(mac, p + 7, 17); mac[17] = 0;
            int i = 0;
            while (i < n && strcmp(devs[i].mac, mac)) i++;
            if (i == n && n < max) n++;
            if (i < n) {
                snprintf(devs[i].mac, sizeof devs[i].mac, "%s", mac);
                snprintf(devs[i].name, sizeof devs[i].name, "%.*s", (int)(len - 25), p + 25);
            }
        }
        p += len + (p[len] == '\n');
    }
    return n;
}

int bt_in(const BtDev *devs, int n, const char *mac) {
    for (int i = 0; i < n; i++) if (!strcmp(devs[i].mac, mac)) return 1;
    return 0;
}

int bt_has_name(const char *mac, const char *name) {
    char dashed[18];
    snprintf(dashed, sizeof dashed, "%s", mac);
    for (char *c = dashed; *c; c++) if (*c == ':') *c = '-';
    return strcmp(name, dashed) && strcmp(name, mac);
}

int bt_info(const char *out, const char *key, char *val, size_t n) {
    size_t kl = strlen(key), o = 0;
    int found = 0, all = !strcmp(key, "UUID");
    if (n) val[0] = 0;
    for (const char *p = out; *p; ) {
        size_t len = strcspn(p, "\n");
        if (p[0] == '\t' && len > kl + 3 && !strncmp(p + 1, key, kl) && p[1 + kl] == ':' && p[2 + kl] == ' ') {
            int vl = (int)(len - kl - 3);
            if (o + 1 < n) o += snprintf(val + o, n - o, "%s%.*s", found ? " " : "", vl, p + kl + 3);
            if (o >= n) o = n - 1;
            found = 1;
            if (!all) break;
        }
        p += len + (p[len] == '\n');
    }
    return found;
}

const char *bt_kind(const char *info) {
    static const char *const audio[] = { "0000110b", "0000110a", "00001108", "0000111e" };   /* A2DP sink/source, headset, handsfree */
    static const char *const input[] = { "00001124", "00001812" };                           /* HID, HID over GATT */
    char icon[64], uuid[2048];
    bt_info(info, "Icon", icon, sizeof icon);
    bt_info(info, "UUID", uuid, sizeof uuid);
    for (char *c = uuid; *c; c++) *c = tolower((unsigned char)*c);
    int a = !strncmp(icon, "audio", 5), in = !strncmp(icon, "input", 5);
    for (size_t i = 0; i < 4; i++) a = a || strstr(uuid, audio[i]);
    if (a) return "audio";
    for (size_t i = 0; i < 2; i++) in = in || strstr(uuid, input[i]);
    return in ? "input" : "other";
}

int bt_ok(const char *out, const char *what) {
    if (!strcmp(what, "pair")) return strstr(out, "Pairing successful") || strstr(out, "AlreadyExists");
    return strstr(out, "Connection successful") != NULL;
}

int bt_is_mac(const char *s) { return s && strlen(s) == 17 && mac_at(s, 0); }

void bt_status_line(int running, const char *show, char *out, size_t n) {
    snprintf(out, n, "powered=%d", running && strstr(show, "Powered: yes") != NULL);
}
