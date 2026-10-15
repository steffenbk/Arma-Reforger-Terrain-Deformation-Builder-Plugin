"""Test agent.js without MCP: attach to MudRunner.exe, print 'ready' and N seconds of samples.
Usage: py test_agent.py [seconds]
"""
import os
import sys
import time

import frida

HERE = os.path.dirname(os.path.abspath(__file__))
secs = float(sys.argv[1]) if len(sys.argv) > 1 else 3.0
got = []


def on_msg(msg, _data):
    if msg.get("type") != "send":
        print("MSG", msg)
        return
    p = msg["payload"]
    if p["type"] == "samples":
        got.extend(p["data"])
        print("samples batch", len(p["data"]), "calls", p["calls"])
    else:
        print(p)


sess = frida.attach("MudRunner.exe")
with open(os.path.join(HERE, "agent.js"), encoding="utf-8") as fh:
    sc = sess.create_script(fh.read())
sc.on("message", on_msg)
sc.load()
time.sleep(secs)
sc.unload()
sess.detach()

last = {}
for s in got:
    last[s["wheel"]] = s
print("total samples", len(got), "wheels", len(last))
for w, s in last.items():
    print(w, {k: (round(v, 3) if isinstance(v, float) else v) for k, v in s.items() if k not in ("wheel", "t")})
