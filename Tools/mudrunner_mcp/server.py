"""MudRunner telemetry MCP (stdio).

Attaches to a running MudRunner.exe with Frida, hooks the per-wheel mud function (agent.js) and gives
Claude tools to record runs, read live wheel state and drive the truck with scripted key presses.
Runs are saved as CSV in ./runs. Setup: README.md.
"""
import bisect
import csv
import ctypes
import math
import os
import threading
import time
from collections import defaultdict, deque
from ctypes import wintypes

import frida
try:                                   # mcp 2.x renamed FastMCP to MCPServer
    from mcp.server.mcpserver import MCPServer as FastMCP
except ImportError:                    # mcp 1.x
    from mcp.server.fastmcp import FastMCP

HERE = os.path.dirname(os.path.abspath(__file__))
RUNS = os.path.join(HERE, "runs")
PROCESS = "MudRunner.exe"
FIELDS = ["t", "wheel", "mud", "water", "sub_fric", "lin_res", "full_push", "push_res", "width", "radius",
          "wheel_fric", "x", "y", "z", "fx", "fy", "fz", "spin", "ground_speed", "throttle", "drive",
          "mud_depth", "water_depth", "vx", "vy", "vz", "truck"]
BODY_FIELDS = ["t", "body", "truck", "mud", "water", "cx", "cy", "cz", "vx", "vy", "vz"]
TRUCK_RADIUS = 8.0   # a wheel belongs to the nearest truck body within this horizontal distance (m)
MATCH_MS = 300       # only compare a wheel with body samples taken this close in time

mcp = FastMCP("mudrunner")


class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.session = None
        self.script = None
        self.ready = None
        self.body_ready = None
        self.error = None
        self.calls = 0
        self.latest = {}                     # wheel -> last sample
        self.bodies = {}                     # truck body -> last sample
        self.recent = deque(maxlen=6000)     # ~ last seconds, all wheels
        self.rec_rows = None
        self.rec_bodies = None
        self.rec_name = None
        self.rec_t0 = None


S = State()


def _on_message(msg, _data):
    if msg.get("type") != "send":
        with S.lock:
            S.error = str(msg)
        return
    p = msg["payload"]
    with S.lock:
        if p["type"] == "ready":
            S.ready = p
        elif p["type"] == "body_ready":
            S.body_ready = p
        elif p["type"] == "error":
            S.error = p["msg"]
        elif p["type"] == "samples":
            S.calls = p["calls"]
            for s in p["data"]:
                S.latest[s["wheel"]] = s
                S.recent.append(s)
                if S.rec_rows is not None:
                    S.rec_rows.append(s)
            for b in p.get("bodies", []):
                S.bodies[b["body"]] = b
                if S.rec_bodies is not None:
                    S.rec_bodies.append(b)


@mcp.tool()
def attach() -> str:
    """Attach to the running MudRunner.exe and hook the wheel mud function. Start the game and load a map first."""
    if S.session:
        return "already attached: " + str(S.ready)
    S.ready = S.body_ready = S.error = None
    S.session = frida.attach(PROCESS)
    with open(os.path.join(HERE, "agent.js"), encoding="utf-8") as fh:
        S.script = S.session.create_script(fh.read())
    S.script.on("message", _on_message)
    S.script.load()
    for _ in range(50):
        if S.ready or S.error:
            break
        time.sleep(0.1)
    if S.error:
        return "attached but hook failed: " + S.error
    return "attached, wheel hook %s, body hook %s" % (S.ready, S.body_ready)


@mcp.tool()
def detach() -> str:
    """Remove the hook and detach from MudRunner."""
    if not S.session:
        return "not attached"
    try:
        S.script.unload()
        S.session.detach()
    finally:
        S.session = S.script = None
    return "detached"


@mcp.tool()
def status() -> dict:
    """Hook state, total hook calls and how many wheels have reported."""
    with S.lock:
        return {"attached": bool(S.session), "hook": S.ready, "error": S.error, "calls": S.calls,
                "wheels": len(S.latest), "trucks": len(S.bodies), "body_hook": S.body_ready,
                "recording": S.rec_name}


def _truck_of(w, bodies):
    """Nearest truck body (horizontal) within TRUCK_RADIUS, or '' (no body in mud yet / too far)."""
    best, bd = "", TRUCK_RADIUS
    for b in bodies:
        d = math.hypot(w["x"] - b["cx"], w["z"] - b["cz"])
        if d < bd:
            best, bd = b["body"], d
    return best


def _slip(s):
    rim = abs(s["spin"]) * s["radius"]
    return rim - abs(s["ground_speed"])


@mcp.tool()
def live() -> dict:
    """Latest sample per wheel and per truck body (only ones the game ran the mud functions for in the last 2 s;
    wheels on hard ground away from mud/water do not report).
    depth = how far the wheel bottom is under the mud surface (m); water = 1 if a water object is near the wheel;
    slip = rim speed - ground speed (m/s). trucks: chassis centre of mass + velocity, wheels grouped by nearest."""
    now = time.time() * 1000
    out = []
    with S.lock:
        bodies = [b for b in S.bodies.values() if now - b["t"] <= MATCH_MS]
        for w, s in S.latest.items():
            if now - s["t"] > 2000:
                continue
            out.append({"wheel": w, "truck": _truck_of(s, bodies), "water": s["water"], "depth": round(s["mud_depth"], 3),
                        "water_depth": round(s["water_depth"], 3), "radius": s["radius"],
                        "width": s["width"], "ground_speed": round(s["ground_speed"], 2),
                        "slip": round(_slip(s), 2), "pos": [round(s["x"], 2), round(s["y"], 2), round(s["z"], 2)],
                        "mud": {"friction": s["sub_fric"], "lin_res": s["lin_res"], "full_push": s["full_push"],
                                "push_res": s["push_res"]}})
    trucks = [{"truck": b["body"], "com": [round(b["cx"], 2), round(b["cy"], 2), round(b["cz"], 2)],
               "speed": round(math.hypot(b["vx"], b["vy"], b["vz"]), 2),
               "vel": [round(b["vx"], 2), round(b["vy"], 2), round(b["vz"], 2)]} for b in bodies]
    return {"trucks": trucks, "wheels": out}


@mcp.tool()
def record_start(name: str) -> str:
    """Start recording every wheel sample. Call record_stop to save the CSV and get a summary."""
    with S.lock:
        S.rec_rows = []
        S.rec_bodies = []
        S.rec_name = name
        S.rec_t0 = time.time()
    return "recording '" + name + "'"


def _group(rows, brows):
    """Set r["truck"] per wheel row from body samples taken within MATCH_MS of it, then give every row of a
    wheel the body most of its rows matched (a body only reports while it is over mud, so last-seen positions
    go stale)."""
    per = defaultdict(list)
    for b in sorted(brows, key=lambda b: b["t"]):
        per[b["body"]].append(b)
    ts = {k: [b["t"] for b in v] for k, v in per.items()}
    votes = defaultdict(lambda: defaultdict(int))
    for r in rows:
        near = []
        for k, v in per.items():
            i = bisect.bisect_left(ts[k], r["t"])
            cand = [v[j] for j in (i - 1, i) if 0 <= j < len(v) and abs(v[j]["t"] - r["t"]) <= MATCH_MS]
            if cand:
                near.append(min(cand, key=lambda b: abs(b["t"] - r["t"])))
        r["truck"] = _truck_of(r, near)
        if r["truck"]:
            votes[r["wheel"]][r["truck"]] += 1
    for r in rows:
        v = votes.get(r["wheel"])
        r["truck"] = max(v, key=v.get) if v else ""


def _summary(rows, brows):
    _group(rows, brows)
    by = defaultdict(list)
    for r in rows:
        by[r["wheel"]].append(r)
    wheels = []
    for w, rs in by.items():
        rs.sort(key=lambda r: r["t"])
        d = [r["mud_depth"] for r in rs]
        wd = [r["water_depth"] for r in rs]
        sl = [_slip(r) for r in rs]
        gs = [abs(r["ground_speed"]) for r in rs]
        in_mud = [r for r in rs if r["mud_depth"] >= 0.025]
        wheels.append({
            "wheel": w, "truck": rs[-1]["truck"], "samples": len(rs), "seconds": round((rs[-1]["t"] - rs[0]["t"]) / 1000, 2),
            "radius": rs[0]["radius"], "width": rs[0]["width"],
            "depth_max": round(max(d), 3), "depth_mean": round(sum(d) / len(d), 3),
            "water_depth_max": round(max(wd), 3),
            "share_in_mud": round(len(in_mud) / len(rs), 2),
            "slip_mean": round(sum(sl) / len(sl), 2), "slip_max": round(max(sl), 2),
            "ground_speed_first": round(gs[0], 2), "ground_speed_last": round(gs[-1], 2),
            "ground_speed_mean": round(sum(gs) / len(gs), 2),
        })
    # 1 s depth / speed curve, mean over wheels
    curve = []
    if rows:
        t0 = min(r["t"] for r in rows)
        sec = defaultdict(list)
        for r in rows:
            sec[int((r["t"] - t0) // 1000)].append(r)
        for k in sorted(sec):
            rs = sec[k]
            curve.append({"s": k, "depth": round(sum(r["mud_depth"] for r in rs) / len(rs), 3),
                          "ground_speed": round(sum(abs(r["ground_speed"]) for r in rs) / len(rs), 2),
                          "slip": round(sum(_slip(r) for r in rs) / len(rs), 2)})
    trucks = []
    bb = defaultdict(list)
    for b in brows:
        bb[b["body"]].append(b)
    for k, bs in bb.items():
        bs.sort(key=lambda b: b["t"])
        sp = [math.hypot(b["vx"], b["vy"], b["vz"]) for b in bs]
        trucks.append({"truck": k, "samples": len(bs), "com_y_first": round(bs[0]["cy"], 3),
                       "com_y_last": round(bs[-1]["cy"], 3), "com_y_min": round(min(b["cy"] for b in bs), 3),
                       "speed_mean": round(sum(sp) / len(sp), 2), "speed_max": round(max(sp), 2),
                       "moved_m": round(math.hypot(bs[-1]["cx"] - bs[0]["cx"], bs[-1]["cz"] - bs[0]["cz"]), 2)})
    return {"trucks": trucks, "wheels": wheels, "per_second": curve}


@mcp.tool()
def record_stop() -> dict:
    """Stop recording, write runs/<name>_<time>.csv (wheels) + _bodies.csv (truck bodies) and return a per-truck
    and per-wheel summary plus a 1 s curve."""
    with S.lock:
        rows, brows, name = S.rec_rows, S.rec_bodies, S.rec_name
        S.rec_rows = S.rec_bodies = S.rec_name = None
    if rows is None:
        return {"error": "not recording"}
    os.makedirs(RUNS, exist_ok=True)
    out = _summary(rows, brows or [])
    base = os.path.join(RUNS, "%s_%s" % (name, time.strftime("%Y%m%d-%H%M%S")))
    path = base + ".csv"
    with open(path, "w", newline="", encoding="utf-8") as fh:
        wr = csv.DictWriter(fh, fieldnames=FIELDS, extrasaction="ignore")
        wr.writeheader()
        wr.writerows(rows)
    with open(base + "_bodies.csv", "w", newline="", encoding="utf-8") as fh:
        wr = csv.DictWriter(fh, fieldnames=BODY_FIELDS, extrasaction="ignore")
        wr.writeheader()
        wr.writerows(brows or [])
    out["csv"] = path
    out["bodies_csv"] = base + "_bodies.csv"
    out["rows"] = len(rows)
    return out


# ---------------------------------------------------------------- input (SendInput, scan codes)
SC = {"w": 0x11, "a": 0x1E, "s": 0x1F, "d": 0x20, "q": 0x10, "e": 0x12, "r": 0x13, "f": 0x21, "g": 0x22,
      "c": 0x2E, "x": 0x2D, "z": 0x2C, "space": 0x39, "shift": 0x2A, "ctrl": 0x1D, "tab": 0x0F,
      "up": 0xC8, "down": 0xD0, "left": 0xCB, "right": 0xCD, "1": 0x02, "2": 0x03, "3": 0x04}
EXT = {"up", "down", "left", "right"}
KEYEVENTF_SCANCODE, KEYEVENTF_KEYUP, KEYEVENTF_EXTENDEDKEY = 0x0008, 0x0002, 0x0001


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wintypes.WORD), ("wScan", wintypes.WORD), ("dwFlags", wintypes.DWORD),
                ("time", wintypes.DWORD), ("dwExtraInfo", ctypes.c_size_t)]


class _I(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT), ("pad", ctypes.c_byte * 32)]


class INPUT(ctypes.Structure):
    _fields_ = [("type", wintypes.DWORD), ("u", _I)]


def _key(name, up):
    sc = SC[name] & 0xFF
    flags = KEYEVENTF_SCANCODE | (KEYEVENTF_KEYUP if up else 0) | (KEYEVENTF_EXTENDEDKEY if name in EXT else 0)
    inp = INPUT(type=1, u=_I(ki=KEYBDINPUT(0, sc, flags, 0, 0)))
    ctypes.windll.user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))


def _focus():
    u = ctypes.windll.user32
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(hwnd, _):
        n = u.GetWindowTextLengthW(hwnd)
        if n and u.IsWindowVisible(hwnd):
            b = ctypes.create_unicode_buffer(n + 1)
            u.GetWindowTextW(hwnd, b, n + 1)
            if "mudrunner" in b.value.lower() or "spintires" in b.value.lower():
                found.append(hwnd)
        return True
    u.EnumWindows(cb, 0)
    if found:
        u.SetForegroundWindow(found[0])
        time.sleep(0.3)
    return bool(found)


@mcp.tool()
def drive(steps: list, record: str = "") -> dict:
    """Drive with held keys. steps = [{"keys": ["w","d"], "seconds": 3}, {"keys": [], "seconds": 1}, ...].
    Keys: w a s d q e r f g c x z space shift ctrl tab up down left right 1 2 3 (MudRunner default binds:
    w/s throttle-brake-reverse, a/d steer - check the game's controls). record = name to record the drive."""
    if not _focus():
        return {"error": "MudRunner window not found"}
    if record:
        record_start(record)
    held = set()
    try:
        for st in steps:
            want = set(k.lower() for k in st.get("keys", []))
            for k in held - want:
                _key(k, True)
            for k in want - held:
                _key(k, False)
            held = want
            time.sleep(float(st.get("seconds", 1)))
    finally:
        for k in held:
            _key(k, True)
    if record:
        return record_stop()
    return {"done": len(steps)}


if __name__ == "__main__":
    mcp.run()
