# MudRunner telemetry MCP

Lets Claude attach to a running MudRunner, read every wheel's mud depth / slip / speed each physics step, record
runs to CSV and drive the truck with scripted key presses. Purpose: measure how MudRunner's sinking feels so the
Reforger mud mod can be tuned to match (docs/superpowers/research/2026-10-15-sinking-deep-dive.md).

## How it works
- `agent.js` (Frida 17) hooks two MudRunner.exe functions, each checked by a 48-byte signature at a fixed RVA
  (falls back to a signature scan after game updates):
  - `sub_140930110(mud, wheel)`, RVA 0x930110 - wheel in mud/water. Runs once per wheel, per mud/water object
    near it, per physics step. Read on leave (depth is written inside).
  - `sub_1409310D0(mud, truck, body)`, RVA 0x9310D0 - truck body in mud/water. Read on enter.
- One row per wheel (max 60 Hz) holding BOTH depths; mud substance values come from the last non-water object.
- `server.py` is a stdio MCP server (mcp 2.x `MCPServer`, falls back to mcp 1.x `FastMCP`).
  Tools: attach, detach, status, live, record_start, record_stop, drive.
  CSVs go to `runs/` (`<name>_<time>.csv` wheels, `<name>_<time>_bodies.csv` truck bodies).
- Test scripts without MCP: `test_agent.py [s]` (attach + print), `probe_record.py [s]` (raw CSV),
  `probe_drive.py '<steps json>' name` (attach + drive + summary).

## Fields
Wheel object (`wheel` arg):
| Offset | CSV | Meaning | Status |
|---|---|---|---|
| +116 | width | tyre width (m) | PROVEN live (0.52 / 0.44) |
| +120 | radius | tyre radius (m) | PROVEN live (0.63 / 0.595) |
| +128 | wheel_fric | wheel SubstanceFriction | PROVEN live (1.2 offroad / 0.75 default, = trucks.xml) |
| +212..220 | x y z | wheel centre (world) | PROVEN live (moves with the truck) |
| +224..232 | fx fy fz | wheel forward axis | PROVEN live (pos change along it = ground_speed) |
| +260 | spin | spin rate (rad/s) | PROVEN live (free wheels: spin x radius = ground_speed) |
| +264 | ground_speed | wheel centre speed along forward axis (m/s) | PROVEN live (within ~3 % of position change, all 10 wheels) |
| +272 | mud_depth | mud depth under the wheel bottom (m) | PROVEN live (0.10-0.28 m in mud, rises when driving in) |
| +276 | water_depth | water depth (m) | read; only 0 seen so far (no deep water tested) |
| +168 | throttle | 0..1 on driven wheels, always 0 on undriven | NOT throttle (DISPROVEN live: ~0.98 on a parked truck with no keys). IDA: > 0.0001 turns on the paddle push. INFERENCE: drive engaged / torque share (user had AWD on; check with AWD off). CSV name kept. |
| +332 | drive | constant per wheel (2355 / 1650) | INFERENCE: unknown constant (paddle factor / 4000 in IDA) |
| +24 -> +560 | vx vy vz | wheel Havok body linear velocity | x/z follow motion but ~15 % low; vy shows +0.4 m/s at rest. Use ground_speed. |

Mud object (`mud` arg): SubstanceFriction +40, LinResistance +44, FullPushDepth +48, PushResistance +52,
water if u32 +60 == 105. PROVEN live: mud 2.2 / 10 / 1.2 / 36 and water 0.4 / 1 / 2 / 1 (= media.xml).

Truck body (`body` = truck ? *(truck+16) : 3rd arg): centre of mass +448..456, linear velocity +560..568.
PROVEN-IDA (the function builds point velocity = lin(+560) + ang(+576) x (p - com(+448))); live: the centre sits
between the truck's wheels and the velocity is 0.00 at rest. Belly depth is NOT stored (only on the stack).
Wheels are grouped to the nearest truck body within 8 m, comparing only body samples taken within 0.3 s of the
wheel sample, then a majority vote per wheel (a body only reports while over mud, so last-seen positions go stale).
PROVEN live 2026-10-15 (run awd_on_mud): the driven 6x6 (tyre r 0.605, w 0.37) reports TWO bodies with different
truck pointers, moving together 1-7 m apart; front 2 wheels -> one body, rear 4 -> the other, every row agreeing.
User 2026-10-15: it was ONE Ural, trailer detached. So one vehicle = two physics bodies (front 2 wheels /
rear 4). INFERENCE: split frame so the chassis can twist.
A "truck" in the output is therefore one physics BODY, not always one whole vehicle.

## Setup (Windows)
```
py -m pip install frida mcp
```
Registered at user scope in `~/.claude.json` (2026-10-15, `claude mcp add -s user mudrunner -- <python> <server.py>`):
```json
"mudrunner": {
  "type": "stdio",
  "command": "C:/Users/Steffen/AppData/Local/Python/pythoncore-3.14-64/python.exe",
  "args": ["C:/Users/Steffen/Documents/My Games/ArmaReforgerWorkbench/addons/Testing plan vech msh/Tools/mudrunner_mcp/server.py"]
}
```
(There was no BeamNG MCP entry in any config, so it sits next to ida-pro-mcp / enfusion-mcp.) New Claude
sessions load it; the current one does not. Single-player game, own copy, no anti-cheat.

## Use
1. Start MudRunner (windowed is fine), load a map, sit in a truck near mud. Release the parking brake.
2. `attach` -> "wheel hook {how: rva}, body hook {how: rva}".
3. `live` while in mud -> trucks + wheels with depth > 0.
4. `drive` with `record` set, e.g. straight into mud: `[{"keys":["w"],"seconds":6},{"keys":[],"seconds":2}]`
5. `record_stop` (or drive's own return) -> per-truck and per-wheel max/mean depth, slip, speed + a 1 s curve.

## Proven live (2026-10-15, frida 17.23.3, mcp 2.3.0, Python 3.14)
- Test condition: all-wheel drive ON for every run (as Arma vehicles, where it can't be switched yet), so
  the numbers compare like-for-like with Reforger. Record AWD on/off in the run name if it ever changes.
- Both hooks install at the RVA (how = rva); samples arrive at ~40 Hz per wheel.
- All 7 tools work through a real MCP stdio client (attach, status, live, record_start/stop, drive, detach).
- awd_on_mud: own truck, 6 s W in mud: both bodies moved 7-11 m, top speed 3.3 m/s, wheel depth up to 0.23 m,
  slip up to 5.7 m/s.
- drive: SendInput scan codes move the truck (W forward ~1.8 m/s in mud, S brakes then reverses). No virtual
  gamepad needed. If the throttle goes to 1.0 but nothing moves, the parking brake is on.
- In mud the driven rear wheels spin (slip up to 4.7 m/s) at 0.15-0.28 m depth while the front axle barely slips.

## Limits / not verified yet
- The hooks only fire for wheels / bodies NEAR a mud or water object - and for EVERY truck there, not only the
  one you drive. A parked truck in mud reports; your truck on grass does not. Check `trucks` / `moved_m`.
- Wheels on hard ground away from mud send
  nothing, so "spin x radius = ground speed on hard ground" can only be checked on free-rolling wheels in mud.
- Speedo vs ground_speed not compared by eye (ground_speed was checked against the real position change instead).
- Deep water (water_depth > 0) not tested yet.
- MudRunner key binds beyond W / S (steer A/D, diff lock, AWD, gears) not tested.
- Belly drag / chassis depth: would need a hook on the per-cell mud query; not done.
