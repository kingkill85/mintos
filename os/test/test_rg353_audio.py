"""Host tests for the Bluetooth part of os/files/usr/local/sbin/rg353-audio (no evdev needed)."""
import importlib.machinery, importlib.util, os, sys, types

HERE = os.path.dirname(os.path.abspath(__file__))
sys.modules.setdefault("evdev", types.SimpleNamespace(ecodes=types.SimpleNamespace()))   # host has no evdev
loader = importlib.machinery.SourceFileLoader("ra", os.path.join(HERE, "../files/usr/local/sbin/rg353-audio"))
spec = importlib.util.spec_from_loader("ra", loader)
ra = importlib.util.module_from_spec(spec)
loader.exec_module(ra)
fails = 0
def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL", what)

pcms = "/org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsrc/sink\n/org/bluealsa/hci0/dev_11_22_33_44_55_66/a2dpsnk/source\n"
check(ra.bt_sink(pcms) == "/org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsrc/sink", "playback pcm picked")
check(ra.bt_sink("") is None, "none connected")
check(ra.bt_sink("/org/bluealsa/hci0/dev_11_22_33_44_55_66/a2dpsnk/source\n") is None, "capture only is ignored")
check(ra.bt_mac("/org/bluealsa/hci0/dev_AA_BB_CC_DD_EE_FF/a2dpsrc/sink") == "AA:BB:CC:DD:EE:FF", "mac")
conf = ra.bt_conf("AA:BB:CC:DD:EE:FF")
check('pcm.!default' in conf and 'bluealsa:DEV=AA:BB:CC:DD:EE:FF,PROFILE=a2dp' in conf, "alsa conf")
check(ra.bt_volume_parse("Volume: L: 100 R: 90\n") == 100 and ra.bt_volume_parse("Volume: 64\n") == 64, "volume parse")
check(ra.bt_volume_parse("nonsense") is None, "volume parse fail")
check(ra.step(124, +1) == 127 and ra.step(3, -1) == 0 and ra.step(60, +1) == 66, "volume steps (0..127, ~5 %)")
check(ra.bl_step(833, 1666) == 5 and ra.bl_step(1666, 1666) == 10 and ra.bl_step(0, 1666) == 1, "brightness step as common/settings.c")
check(ra.bl_raw(5, 1666) == 833 and ra.bl_raw(11, 1666) == 1666 and ra.bl_raw(0, 1666) == 166, "brightness raw clamped to 1..10")
check(ra.master_pct("Simple mixer control 'Master',0\n  Front Left: Playback 40 [63%] [-24.00dB] [on]\n") == 63, "master percent")
check(ra.master_pct("nothing") is None, "master percent missing")
print("ok" if not fails else f"{fails} failures")
sys.exit(1 if fails else 0)
