/* Bluetooth for the MintOS settings, through bluetoothctl.
 *   mintos-bt status              powered=0|1
 *   mintos-bt list [--scan]       "mac<TAB>name<TAB>audio|input|other<TAB>paired<TAB>connected" per device
 *                                 (connected and paired first; devices without a name only when paired)
 *   mintos-bt connect <mac>       pair (no PIN, "just works") + trust + connect; exit 0 when connected
 *   mintos-bt disconnect <mac>
 *   mintos-bt forget <mac>
 *   mintos-bt power on|off        off: stops bluetoothd + bluealsa and unloads the driver; on: back
 *   mintos-bt boot                apply settings.cfg "bt = off" at start-up
 * Discovery runs inside bluetoothctl: BlueZ stops a scan when the client that started it leaves. */
#include "bt.h"
#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char SETTINGS[] = "/storage/.config/mintos/settings.cfg";
static char out[1 << 16];

/* bluetoothctl [--timeout t] args...; its output in out */
static const char *ctl(int timeout, const char *a1, const char *a2, const char *a3, const char *a4) {
    char t[16];
    const char *argv[9];
    int n = 0;
    argv[n++] = "bluetoothctl";
    if (timeout) { snprintf(t, sizeof t, "%d", timeout); argv[n++] = "--timeout"; argv[n++] = t; }
    const char *a[] = { a1, a2, a3, a4 };
    for (int i = 0; i < 4 && a[i]; i++) argv[n++] = a[i];
    argv[n] = NULL;
    run_cmd((char **)argv, NULL, out, sizeof out, ((timeout ? timeout : 10) + 15) * 1000);
    return out;
}

static int active(void) { return RUN_Q(10000, "systemctl", "is-active", "--quiet", "bluetooth") == 0; }

typedef struct { int unconnected, unpaired; char lower[128]; BtDev d; const char *kind; } Row;

static int row_cmp(const void *a, const void *b) {
    const Row *x = a, *y = b;
    if (x->unconnected != y->unconnected) return x->unconnected - y->unconnected;
    if (x->unpaired != y->unpaired) return x->unpaired - y->unpaired;
    int c = strcmp(x->lower, y->lower);
    return c ? c : strcmp(x->d.mac, y->d.mac);
}

static int cmd_list(int scan) {
    static BtDev all[128], paired[128], connected[128];
    static Row rows[128];
    if (scan) ctl(8, "scan", "on", NULL, NULL);
    int na = bt_parse_devices(ctl(0, "devices", NULL, NULL, NULL), all, 128);
    int np = bt_parse_devices(ctl(0, "devices", "Paired", NULL, NULL), paired, 128);
    int nc = bt_parse_devices(ctl(0, "devices", "Connected", NULL, NULL), connected, 128);
    int nr = 0;
    for (int i = 0; i < na; i++) {
        int p = bt_in(paired, np, all[i].mac);
        if (!bt_has_name(all[i].mac, all[i].name) && !p) continue;
        Row *r = &rows[nr++];
        r->d = all[i];
        r->unpaired = !p;
        r->unconnected = !bt_in(connected, nc, all[i].mac);
        snprintf(r->lower, sizeof r->lower, "%s", all[i].name);
        for (char *c = r->lower; *c; c++) *c = tolower((unsigned char)*c);
        r->kind = bt_kind(ctl(0, "info", all[i].mac, NULL, NULL));
    }
    qsort(rows, nr, sizeof *rows, row_cmp);
    for (int i = 0; i < nr; i++) {
        for (char *c = rows[i].d.name; *c; c++) if (*c == '\t') *c = ' ';
        printf("%s\t%s\t%s\t%d\t%d\n", rows[i].d.mac, rows[i].d.name, rows[i].kind, !rows[i].unpaired, !rows[i].unconnected);
    }
    return 0;
}

static int cmd_connect(const char *mac) {
    char paired[16];
    bt_info(ctl(0, "info", mac, NULL, NULL), "Paired", paired, sizeof paired);
    if (strcmp(paired, "yes")) {
        ctl(0, "pairable", "on", NULL, NULL);
        if (!bt_ok(ctl(25, "--agent", "NoInputNoOutput", "pair", mac), "pair")) { puts("Koppeln fehlgeschlagen"); return 1; }
    }
    ctl(0, "trust", mac, NULL, NULL);
    if (!bt_ok(ctl(15, "connect", mac, NULL, NULL), "connect")) { puts("Verbindung fehlgeschlagen"); return 1; }
    return 0;
}

/* bluetoothd sees hci0 only once the firmware is loaded (~1 s after the driver) */
static void wait_controller(void) {
    for (int i = 0; i < 20; i++) {
        if (strstr(ctl(0, "show", NULL, NULL, NULL), "Powered:")) break;
        sleep_ms(500);
    }
}

static int power(int on) {
    if (on) {
        RUN_Q(30000, "modprobe", "hci_uart");
        RUN_Q(30000, "systemctl", "start", "bluetooth", "bluealsa");
        wait_controller();
        return strstr(ctl(0, "power", "on", NULL, NULL), "succeeded") ? 0 : 1;
    }
    if (active()) ctl(0, "power", "off", NULL, NULL);
    RUN_Q(30000, "systemctl", "stop", "bluealsa", "bluetooth");
    RUN_Q(30000, "modprobe", "-r", "hidp", "uhid", "hci_uart", "btrtl", "bluetooth");
    return 0;
}

static int usage(void) {
    puts("mintos-bt status | list [--scan] | connect|disconnect|forget <mac> | power on|off | boot");
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    const char *cmd = argv[1];
    if (!strcmp(cmd, "status") && argc == 2) {
        char line[32];
        int running = active();
        bt_status_line(running, running ? ctl(0, "show", NULL, NULL, NULL) : "", line, sizeof line);
        puts(line);
        return 0;
    }
    if (!strcmp(cmd, "boot") && argc == 2) {
        /* unloading the driver during the firmware upload makes it time out (hci0 tx timeout) */
        if (setting_off(SETTINGS, "bt")) { if (active()) wait_controller(); power(0); }
        return 0;
    }
    if (!strcmp(cmd, "power") && argc == 3) return power(!strcmp(argv[2], "on"));
    if (!active()) { puts("Bluetooth ist aus"); return 1; }   /* bluetoothctl would hang without bluetoothd */
    if (!strcmp(cmd, "list")) return cmd_list(argc > 2 && !strcmp(argv[2], "--scan"));
    if (argc == 3 && bt_is_mac(argv[2])) {
        if (!strcmp(cmd, "connect")) return cmd_connect(argv[2]);
        if (!strcmp(cmd, "disconnect")) return strstr(ctl(0, "disconnect", argv[2], NULL, NULL), "Successful") ? 0 : 1;
        if (!strcmp(cmd, "forget")) return strstr(ctl(0, "remove", argv[2], NULL, NULL), "removed") ? 0 : 1;
    }
    return usage();
}
