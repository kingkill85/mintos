// uses: tools/wifi.c
/* Host tests for mintos-wifi's logic (tools/wifi.c) on a recorded iwd state (os/test/iwd-objects.tsv). */
#include "check.h"
#include "../../tools/wifi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static Prop props[200];

static int load(const char *path) {
    FILE *f = fopen(path, "r");
    char line[700];
    int n = 0;
    while (f && n < 200 && fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        line[strcspn(line, "\n")] = 0;
        char *fld[4] = { line, 0, 0, 0 };
        for (int i = 1; i < 4; i++) {
            char *t = fld[i - 1] ? strchr(fld[i - 1], '\t') : NULL;
            if (t) { *t = 0; fld[i] = t + 1; }
        }
        if (!fld[3]) continue;
        snprintf(props[n].path, sizeof props[n].path, "%s", fld[0]);
        snprintf(props[n].iface, sizeof props[n].iface, "%s", fld[1]);
        snprintf(props[n].key, sizeof props[n].key, "%s", fld[2]);
        snprintf(props[n].val, sizeof props[n].val, "%s", fld[3]);
        n++;
    }
    if (f) fclose(f);
    return n;
}

int main(void) {
    char path[512], s[512];
    snprintf(path, sizeof path, "%s/os/test/iwd-objects.tsv", getenv("REPO"));
    int n = load(path);
    CHECK(n > 50);
    static WState st, st0;
    wifi_parse(props, n, &st);
    CHECK_STR(st.device, "/net/connman/iwd/0/2");
    CHECK_INT(st.powered, 1);
    CHECK_STR(st.state, "connected");
    CHECK_STR(st.ssid, "Tardis");
    CHECK_INT(st.nnet, 4);
    const WNet *t = wifi_find(&st, "Tardis"), *v = wifi_find(&st, "Vodafone-2008"), *h = wifi_find(&st, "Vodafone Homespot");
    CHECK(t && t->known && t->connected);
    CHECK(wifi_find(&st, "Vodafone-EC94"));
    CHECK(v && !v->known);
    CHECK(h && !strcmp(h->type, "open"));
    CHECK_STR(wifi_known_path(&st, "Tardis"), "/net/connman/iwd/546172646973_psk");

    wifi_parse(props, 0, &st0);
    CHECK(!st0.has_device);
    /* status: device=0 while the chip is not there yet (driver still loading), so the UI does not say "Aus" */
    wifi_status_text(&st0, s, sizeof s);
    CHECK_STR(s, "device=0\npowered=0\nstate=\nssid=");
    wifi_status_text(&st, s, sizeof s);
    CHECK_STR(s, "device=1\npowered=1\nstate=connected\nssid=Tardis");
    /* iwd stopped: switched off in the settings -> "Aus" (device there, not powered); otherwise still starting */
    wifi_offline(1, &st0); wifi_status_text(&st0, s, sizeof s);
    CHECK_STR(s, "device=1\npowered=0\nstate=\nssid=");
    wifi_offline(0, &st0); wifi_status_text(&st0, s, sizeof s);
    CHECK_STR(s, "device=0\npowered=0\nstate=\nssid=");

    CHECK_INT(wifi_bars(-4000), 4); CHECK_INT(wifi_bars(-6500), 3); CHECK_INT(wifi_bars(-7200), 2);
    CHECK_INT(wifi_bars(-8000), 1); CHECK_INT(wifi_bars(-9500), 0);

    WNet nets[3] = { { .name = "a", .signal = -5000 }, { .name = "b", .known = 1, .signal = -8000 },
                     { .name = "c", .connected = 1, .known = 1, .signal = -9000 } };
    wifi_sort(nets, 3);
    CHECK_STR(nets[0].name, "c"); CHECK_STR(nets[1].name, "b"); CHECK_STR(nets[2].name, "a");

    iwd_file("Tardis", "psk", s, sizeof s);              CHECK_STR(s, "Tardis.psk");
    iwd_file("Vodafone Homespot", "open", s, sizeof s);  CHECK_STR(s, "Vodafone Homespot.open");
    iwd_file("Caf\xc3\xa9!", "psk", s, sizeof s);        CHECK_STR(s, "=436166c3a921.psk");

    char dir[] = "/tmp/test_wifiXXXXXX";
    CHECK(mkdtemp(dir));
    CHECK_INT(write_psk(dir, "My Net", "geheim 123", path, sizeof path), 0);
    FILE *f = fopen(path, "r");
    size_t r = f ? fread(s, 1, sizeof s - 1, f) : 0;
    s[r] = 0;
    if (f) fclose(f);
    CHECK_STR(s, "[Security]\nPassphrase=geheim 123\n");
    struct stat sb;
    CHECK(!stat(path, &sb) && (sb.st_mode & 0777) == 0600);
    unlink(path); rmdir(dir);
    TEST_END();
}
