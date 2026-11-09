// uses: daemon/helpers.c
/* Host tests for the pure parts of mintosd (daemon/helpers.c). */
#include "check.h"
#include "../../daemon/helpers.h"
#include <string.h>

int main(void) {
    char s[256], mac[18];
    const char *pcms = "/org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsrc/sink\n/org/bluealsa/hci0/dev_11_22_33_44_55_66/a2dpsnk/source\n";
    CHECK(bt_sink(pcms, s, sizeof s));
    CHECK_STR(s, "/org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsrc/sink");
    CHECK(!bt_sink("", s, sizeof s));
    CHECK(!bt_sink("/org/bluealsa/hci0/dev_11_22_33_44_55_66/a2dpsnk/source\n", s, sizeof s));   /* capture only */
    CHECK(!bt_sink("/a2dpsrc/sinkX\n", s, sizeof s));

    bt_mac("/org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsrc/sink", mac);
    CHECK_STR(mac, "AA:BB:CC:DD:EE:FF");
    bt_mac("/org/bluealsa/hci0/nothing", mac);
    CHECK_STR(mac, "");

    bt_conf("AA:BB:CC:DD:EE:FF", s, sizeof s);
    CHECK(strstr(s, "pcm.!default") && strstr(s, "bluealsa:DEV=AA:BB:CC:DD:EE:FF,PROFILE=a2dp"));

    CHECK_INT(bt_volume_parse("Volume: L: 100 R: 90\n"), 100);
    CHECK_INT(bt_volume_parse("Volume: 64\n"), 64);
    CHECK_INT(bt_volume_parse("nonsense"), -1);
    CHECK_INT(bt_step(124, 1), 127);
    CHECK_INT(bt_step(3, -1), 0);
    CHECK_INT(bt_step(60, 1), 66);

    CHECK_INT(bl_step(833, 1666), 5);
    CHECK_INT(bl_step(1666, 1666), 10);
    CHECK_INT(bl_step(0, 1666), 1);
    CHECK_INT(bl_raw(5, 1666), 833);
    CHECK_INT(bl_raw(11, 1666), 1666);
    CHECK_INT(bl_raw(0, 1666), 166);

    CHECK_INT(master_pct("Simple mixer control 'Master',0\n  Limits: Playback 0 - 255 Capture 0 - 255\n"
                         "  Front Left: Playback 40 [63%] [-24.00dB] [on]\n"), 63);
    CHECK_INT(master_pct("nothing"), -1);

    osd_line(42, "vol", 12, s, sizeof s);
    CHECK_STR(s, "42 vol 10\n");
    osd_line(7, "bright", -1, s, sizeof s);
    CHECK_STR(s, "7 bright 0\n");
    TEST_END();
}
