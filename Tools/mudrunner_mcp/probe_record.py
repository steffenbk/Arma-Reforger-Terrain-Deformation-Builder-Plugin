"""Record N seconds of raw agent samples to runs/probe_<time>.csv (no MCP). Usage: py probe_record.py 40"""
import csv
import os
import sys
import time

import frida

HERE = os.path.dirname(os.path.abspath(__file__))
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
rows = []
info = []


def on_msg(msg, _d):
    if msg.get("type") != "send":
        info.append(str(msg))
        return
    p = msg["payload"]
    if p["type"] == "samples":
        rows.extend(p["data"])
    else:
        info.append(str(p))


sess = frida.attach("MudRunner.exe")
with open(os.path.join(HERE, "agent.js"), encoding="utf-8") as fh:
    sc = sess.create_script(fh.read())
sc.on("message", on_msg)
sc.load()
print("\n".join(info), flush=True)
time.sleep(secs)
sc.unload()
sess.detach()
os.makedirs(os.path.join(HERE, "runs"), exist_ok=True)
out = os.path.join(HERE, "runs", "probe_%s.csv" % time.strftime("%Y%m%d-%H%M%S"))
keys = list(rows[0].keys()) if rows else []
with open(out, "w", newline="", encoding="utf-8") as fh:
    w = csv.DictWriter(fh, fieldnames=keys)
    w.writeheader()
    w.writerows(rows)
print(len(rows), "rows ->", out)
