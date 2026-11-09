// uses: tools/bt.c
/* Host tests for mintos-bt's parsing (tools/bt.c) on recorded bluetoothctl output (os/test/btctl-sample.txt). */
#include "check.h"
#include "../../tools/bt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char sample[1 << 16];

static const char *name_of(const BtDev *d, int n, const char *mac) {
    for (int i = 0; i < n; i++) if (!strcmp(d[i].mac, mac)) return d[i].name;
    return "";
}

int main(void) {
    char path[512], s[256];
    snprintf(path, sizeof path, "%s/os/test/btctl-sample.txt", getenv("REPO"));
    FILE *f = fopen(path, "r");
    size_t r = f ? fread(sample, 1, sizeof sample - 1, f) : 0;
    sample[r] = 0;
    if (f) fclose(f);

    static BtDev d[64];
    int n = bt_parse_devices(sample, d, 64);
    CHECK(n >= 16);
    CHECK_STR(name_of(d, n, "08:EF:3B:64:7A:34"), "LG LAC553B(34)");
    CHECK_STR(name_of(d, n, "C0:48:E6:84:EC:C2"), "[TV] Samsung Q6 Series (55)");
    CHECK_STR(name_of(d, n, "BE:16:FC:51:04:A4"), "MELK-OA21P  A4");
    CHECK(!bt_has_name("62:C0:B2:61:C8:F8", name_of(d, n, "62:C0:B2:61:C8:F8")));   /* MAC-only alias */
    CHECK(bt_has_name("08:EF:3B:64:7A:34", name_of(d, n, "08:EF:3B:64:7A:34")));
    CHECK(bt_in(d, n, "08:EF:3B:64:7A:34") && !bt_in(d, n, "00:00:00:00:00:00"));

    const char *info = "Device AA:BB:CC:DD:EE:FF (public)\n\tName: 8BitDo Pro 2\n\tIcon: input-gaming\n\tPaired: yes\n"
                       "\tConnected: no\n\tUUID: Human Interface Device... (00001124-0000-1000-8000-00805f9b34fb)\n"
                       "\tManufacturerData.Key: 0x0075 (117)\n";
    CHECK(bt_info(info, "Icon", s, sizeof s)); CHECK_STR(s, "input-gaming");
    CHECK(bt_info(info, "Paired", s, sizeof s)); CHECK_STR(s, "yes");
    CHECK(bt_info(info, "Name", s, sizeof s)); CHECK_STR(s, "8BitDo Pro 2");
    CHECK(!bt_info(info, "Key", s, sizeof s));
    CHECK_STR(bt_kind(info), "input");
    CHECK_STR(bt_kind("\tIcon: audio-headset\n"), "audio");
    CHECK_STR(bt_kind("\tIcon: audio-headphones\n"), "audio");
    CHECK_STR(bt_kind("\tUUID: Audio Sink (0000110B-0000-1000-8000-00805f9b34fb)\n"), "audio");
    CHECK_STR(bt_kind(""), "other");

    CHECK(bt_ok("Attempting to pair with AA\n[CHG] Device AA Paired: yes\nPairing successful\n", "pair"));
    CHECK(!bt_ok("Failed to pair: org.bluez.Error.AuthenticationFailed\n", "pair"));
    CHECK(bt_ok("Attempting to connect to AA\nConnection successful\n", "connect"));
    CHECK(bt_is_mac("AA:BB:CC:DD:EE:FF") && bt_is_mac("aa:bb:cc:dd:ee:ff"));
    CHECK(!bt_is_mac("AA:BB; rm -rf /") && !bt_is_mac("AA:BB:CC:DD:EE:FF0"));
    /* bluetoothd stopped (switched off): status must not ask bluetoothctl (it would hang) */
    bt_status_line(0, "", s, sizeof s);                    CHECK_STR(s, "powered=0");
    bt_status_line(1, "\tPowered: yes\n", s, sizeof s);    CHECK_STR(s, "powered=1");
    bt_status_line(1, "\tPowered: no\n", s, sizeof s);     CHECK_STR(s, "powered=0");
    TEST_END();
}
