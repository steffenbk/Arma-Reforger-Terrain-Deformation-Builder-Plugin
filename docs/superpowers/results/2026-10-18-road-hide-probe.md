# Road hide probe (whole-map cut plan Task 1)

WB log logs_2026-10-18_22-31-10/console.log, lines 947-1199 (23:37:34 - 23:38:18), Exp 1.9 Tools exe.
User: "it seems to work hiding it" / "but you hide the whole road".

| Road (origin) | Bounds size (x, y, z) m | physics | Show(false) | ClearFlags VISIBLE | Seen by user |
|---|---|---|---|---|---|
| <1037.7, 39.0, 2543.2> | 398, 0, 548 | 0 | hides | hides | whole road gone |
| <1699.85, 39.6, 2624.0> | 140, 4, 434 | 0 | hides | hides | whole road gone |
| <1227.1, 39.0, 2395.9> | 78, 0, 58 | 0 | hides | hides | whole road gone |

(3 distinct RoadEntity in 30 probe lines; every 'restored' line put the road back. 'hides' for both columns = the user
saw it vanish; which of the two did it first was not told apart - both were pressed on each road.)

## Facts
- One RoadEntity = one whole road spline (78 m to ~550 m bounds), not short pieces. Hiding is all or nothing per road.
- physics 0 on all three: a road mesh has NO collider of its own. Vehicles drive on the ground under it, so after the
  cut they drive on OUR ground there; a crater under a hidden road is physically right already.
- flags 262178 on all three.
