"""Host tests for os/files/usr/local/sbin/mintos-wifi: python3 -I os/test/test_mintos_wifi.py"""
import importlib.machinery, importlib.util, json, os, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
loader = importlib.machinery.SourceFileLoader("mw", os.path.join(HERE, "../files/usr/local/sbin/mintos-wifi"))
spec = importlib.util.spec_from_loader("mw", loader)
mw = importlib.util.module_from_spec(spec)
loader.exec_module(mw)

objs = json.load(open(os.path.join(HERE, "iwd-objects.json")))["data"][0]
fails = 0
def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL", what)

st = mw.parse(objs)
check(st["device"] == "/net/connman/iwd/0/2", "device path")
check(st["powered"] is True, "powered")
check(st["state"] == "connected", "state")
check(st["ssid"] == "Tardis", "connected ssid")
nets = {n["name"]: n for n in st["networks"]}
check(set(nets) == {"Tardis", "Vodafone-EC94", "Vodafone-2008", "Vodafone Homespot"}, "network names")
check(nets["Tardis"]["known"] and nets["Tardis"]["connected"], "Tardis known+connected")
check(not nets["Vodafone-2008"]["known"], "unknown network")
check(nets["Vodafone Homespot"]["type"] == "open", "open type")
check(mw.parse({})["device"] is None, "no iwd objects")

# signal (100 * dBm) -> 0..4 bars
check(mw.bars(-4000) == 4 and mw.bars(-6500) == 3 and mw.bars(-7200) == 2 and mw.bars(-8000) == 1 and mw.bars(-9500) == 0, "bars")

# iwd file names: plain SSIDs as is, others hex with '='
check(mw.iwd_file("Tardis", "psk") == "Tardis.psk", "plain name")
check(mw.iwd_file("Vodafone Homespot", "open") == "Vodafone Homespot.open", "space allowed")
check(mw.iwd_file("Café!", "psk") == "=" + "Café!".encode().hex() + ".psk", "hex name")

# writing a passphrase file
d = tempfile.mkdtemp()
p = mw.write_psk(d, "My Net", "geheim 123")
check(open(p).read() == "[Security]\nPassphrase=geheim 123\n", "psk content")
check(oct(os.stat(p).st_mode & 0o777) == "0o600", "psk mode")

print("ok" if not fails else f"{fails} failures")
sys.exit(1 if fails else 0)
