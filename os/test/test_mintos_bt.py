"""Host tests for os/files/usr/local/sbin/mintos-bt: python3 -I os/test/test_mintos_bt.py"""
import importlib.machinery, importlib.util, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
loader = importlib.machinery.SourceFileLoader("mb", os.path.join(HERE, "../files/usr/local/sbin/mintos-bt"))
spec = importlib.util.spec_from_loader("mb", loader)
mb = importlib.util.module_from_spec(spec)
loader.exec_module(mb)
fails = 0
def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL", what)

sample = open(os.path.join(HERE, "btctl-sample.txt")).read()
devs = mb.parse_devices(sample)
check(devs["08:EF:3B:64:7A:34"] == "LG LAC553B(34)", "named device")
check(devs["C0:48:E6:84:EC:C2"] == "[TV] Samsung Q6 Series (55)", "name with brackets")
check(not mb.has_name("62:C0:B2:61:C8:F8", devs["62:C0:B2:61:C8:F8"]), "MAC-only alias is no name")
check(mb.has_name("08:EF:3B:64:7A:34", devs["08:EF:3B:64:7A:34"]), "real name")

info = "Device AA:BB:CC:DD:EE:FF (public)\n\tName: 8BitDo Pro 2\n\tIcon: input-gaming\n\tPaired: yes\n\tConnected: no\n\tUUID: Human Interface Device... (00001124-0000-1000-8000-00805f9b34fb)\n"
i = mb.parse_info(info)
check(i["Icon"] == "input-gaming" and i["Paired"] == "yes" and i["Name"] == "8BitDo Pro 2", "info fields")
check(mb.kind(i) == "input", "gamepad kind")
check(mb.kind({"Icon": "audio-headset"}) == "audio" and mb.kind({"Icon": "audio-headphones"}) == "audio", "audio kind")
check(mb.kind({"UUID": "Audio Sink (0000110b-0000-1000-8000-00805f9b34fb)"}) == "audio", "audio by UUID")
check(mb.kind({}) == "other", "unknown kind")

check(mb.ok("Attempting to pair with AA\n[CHG] Device AA Paired: yes\nPairing successful\n", "pair"), "pair ok")
check(not mb.ok("Failed to pair: org.bluez.Error.AuthenticationFailed\n", "pair"), "pair failed")
check(mb.ok("Attempting to connect to AA\nConnection successful\n", "connect"), "connect ok")
check(mb.is_mac("AA:BB:CC:DD:EE:FF") and not mb.is_mac("AA:BB; rm -rf /"), "mac check")
print("ok" if not fails else f"{fails} failures")
sys.exit(1 if fails else 0)
