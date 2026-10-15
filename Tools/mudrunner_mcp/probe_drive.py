"""Attach, then drive with server.drive (SendInput) while recording. No MCP client needed.
Usage: py probe_drive.py '[{"keys":["w"],"seconds":5},{"keys":[],"seconds":2}]' name
"""
import json
import sys

import server

steps = json.loads(sys.argv[1])
name = sys.argv[2] if len(sys.argv) > 2 else "probe_drive"
print(server.attach())
res = server.drive(steps, record=name)
print(server.detach())
for t in res.get("trucks", []):
    print(t)
for w in res.get("wheels", []):
    print(w)
for c in res.get("per_second", []):
    print(c)
print(res.get("csv"), res.get("rows"), res.get("error"))
