# Driver stutter: deep dive (2026-10-11)

Binary: `G:\SteamLibrary\steamapps\common\Arma Reforger Experimental\ArmaReforgerSteamDiag.exe`, build 192624,
loaded in IDA. Every address below was read in this session; the labels are mine.

## 1. How the engine syncs a driven car (NwkCarMovementComponent): read from the binary

| Step | Where | What it does |
|---|---|---|
| Driver sends moves | `NwkCarMovementComponent::Rpc_USendMove_S` (string 0x141c602c0) | client simulates locally, sends input + its resulting state per move id |
| Server inserts move into history | sub_140844640 | ordered ring buffer, gaps of 1-3 moves are filled with generated moves (sub_140843850) |
| Server checks one move | sub_140841BE0 -> sub_14083B890 | server runs the SAME inputs through ITS OWN simulation, then compares to the client's state for that move |
| Check, all four must pass | sub_14083B890 | `dist(serverPos, clientPos) < m_fDist` (a1+368); `angle < m_fAngle` (+372); `|serverAngVel| - |clientAngVel| < m_fSpin` (+376); `|serverVel| - |clientVel| < m_fSpeed` (+380). The two speed checks are signed (fail only when the server is faster). Speed-scaled mode: `m_fDist = speed / a1+384`, clamped to [1e-6, 1.5] |
| Approval | sub_140841BE0 | records the approved id. **The server does NOT take the client's state.** Server and driver keep simulating separately |
| Correction | sub_140841BE0 | stores server state, sends correction, then a cooldown `a1+400 = RTT ticks + 6`, capped by dword_142371534 = 60. No new correction during the cooldown |
| Client receives correction | sub_140841A40 -> sub_140843540 | physics body set to the server state for that move id, then sub_140843070 replays every unconfirmed move with single-body steps against the CURRENT world |
| Client visual smoothing | sub_140840DF0 -> sub_140840630 | lerp the visual toward the body at dword_142371528 = 0.35 a frame, if the matrix difference (sum of squared elements) <= dword_142371524 = 10; else snap |
| Client knobs (defaults) | 0x142371520..34 | prediction allowance 0.25 (only drawn in the debug view for cars), lerp limit 10, lerp step 0.35, 0.4, 1.5, correction cooldown cap 60 |
| Diag menu (Diag exe only) | "Vehicle Networking and Reconciliation" | "Debug history" (dword_14248E5B8), "Debug path correction" (qword_14248E470 - logs NVMC SERVER APPROVING / CORRECTING), "C: Debug Lerping" (dword_14248E618), "C: Disable Correction" (dword_14248EDB0). Ids are handed out in startup order (sub_1409F4F90), so script `DiagMenu.SetEngineValue` cannot find them by name |

Consequence (from the code, not yet measured): the driver and server drift apart with nothing pulling
them back until the gap passes the limit. Then the driver resets to the server's past state plus a replay.

## 2. What the logs say (measured, 01:38 run, PeerPlugin1/logs/logs_2026-10-11_01-38-19, NET_DIST 0.25)

Every client jolt line of 0.1 m or more (16 lines). The "jump" is the visual (entity origin) jump in one frame:

| time | dir | jump m | speed m/s | dt s |
|---|---|---|---|---|
| 01:39:26.331 | rev | 0.249 | 0.78 -> 1.37 | 0.0101 |
| 01:39:37.577 | rev | 0.258 | 1.62 -> 1.77 | 0.0116 |
| 01:39:53.996 | fwd | 0.265 | 2.20 -> 2.24 | 0.0122 |
| 01:39:57.565 | fwd | 0.103 | 2.46 -> 2.36 | 0.0108 |
| 01:40:00.705 | rev | 0.251 | 2.64 -> 2.64 | 0.0137 |
| 01:40:09.509 | fwd | 0.244 | 3.51 -> 3.36 | 0.0157 |
| 01:40:14.651 | fwd | 0.211 | 3.70 -> 3.57 | 0.0126 |
| 01:40:16.867 | rev | 0.276 | 2.52 -> 2.84 | 0.0106 |
| 01:40:19.980 | rev | 0.225 | 5.31 -> 5.19 | 0.0090 |
| 01:40:31.002 | fwd | 0.243 | 0.59 -> 0.61 | 0.0147 |
| 01:40:39.450 | rev | 0.225 | 0.95 -> 0.88 | 0.0133 |
| 01:41:21.954 | rev | 0.234 | 0.78 -> 0.68 | 0.0140 |
| 01:41:28.312 | fwd | 0.202 | 2.66 -> 2.60 | 0.0130 |
| 01:41:36.357 | rev | 0.116 | 3.97 -> 3.97 | 0.0446 |
| 01:41:36.367 | rev | 0.142 | 3.97 -> 3.92 | 0.0096 |
| 01:41:45.673 | fwd | 0.218 | 2.54 -> 2.50 | 0.0120 |

13 of the 16 are 0.20-0.28 m, matching NET_DIST 0.25. In the 01:15 run (NET_DIST 0.15) the snaps were 0.11-0.21 m.
So snap size follows the limit, and the snap rate barely changed (about 4.4/min vs 4.6/min). The dt values are
normal frames (0.009-0.016 s), so the snaps are not frame hitches.

Inference (unverified): the visual is NOT smoothed. A 35% lerp would show about 0.09 m on the first frame, not
0.25 m. Either the lerp branch is not taken, or the probe reads the origin after the snap. Probe v2 checks this.

Inference (unverified): the rate stays the same when the limit grows from 0.15 to 0.25 m, so the drift does not
build up slowly. It comes in short episodes (start, stop, reversing, maybe ground events) that push the gap past
either limit in a few ticks. Fewer episodes means fewer snaps; a smaller limit means smaller snaps.

## 3. Candidate causes, ranked by how well they fit (all INFERENCE until probe v2)

1. **Ruts dug at the server's wheel positions.** The rut bowl (0.5 m radius, `1 - k^2/2` falloff) plus berm
   (0.5-0.9 m) sit where the SERVER's wheels were. The driver's wheels run up to NET_DIST off that line, so they
   ride the rut's slope or berm, which the server's wheels do not. The two machines then diverge, and the
   deeper the rut the stronger the push. This fits full depth = 39 snaps vs 3x = 13.
2. **Collider timing.** The server rebuilds colliders as soon as the hold ends; the driver gets heights later
   (send tick + latency + rebuild). A rear wheel can meet a fresh rut on the server and flat ground on the driver.
   This gets worse with speed (wheelbase / speed shrinks).
3. **Vanilla drift.** Starting, stopping and reversing; the user says vanilla drivers stutter too. Vanilla's
   speed-scaled limit is speed/300 (1 cm at 3 m/s), so vanilla corrects constantly but in tiny steps.

## 4. Probe v2 (in code, TEMP)

`Scripts/Game/Mud/MUD_JoltProbe.c` + `MUD_WorldGroundEntity.ProbeGround` + `MUD_FinePatch.m_fColChangeAt`.
- Correction = the physics body (Physics.GetDirectWorldTransform) jumps > 5 cm off its own path. Logged with the
  visual jump, throttle/brake/gear/steer/handbrake, slip, how many wheels are on our ground vs road, and on this
  machine: collider vs newest heights at the wheels, seconds since that collider last changed, dirty patches.
- Trace for 6 frames after each correction: visual vs body offset. If it decays about 35% a frame, the lerp works.
- Each 10 s: driving seconds and corrections per bucket (our ground / road x slow < 2 / mid < 5 / fast m/s).
  **Roads are vanilla terrain, so they are the in-session control.** Also: driving time with a wheel on a lagging
  collider, a fresh collider, or a dirty patch, to compare against the same numbers at correction time.

## 5. Test plan

- Run A (as is, rut speed 3): drive about 3 min on our ground (start, stop, reverse, slow and fast), plus at least
  1 min on a road the same way.
- Run B (control): set `m_fRutSpeed` to 0 (no ruts, same colliders), same driving.
- Read: corrections per minute ours vs road vs run B; at each correction, are the wheels on fresh or lagging
  ground more often than the driving-time share; does the visual lerp happen.

## 6. Run A result (01:58, DS + PeerPlugin1, probe v2, NET_DIST 0.25, rut speed 3) - measured

- Body-jump detector at > 2 m/s is noise: the SERVER (never corrected) logged 385 'corr' lines, 34 of them 0.2-0.35 m;
  the client logged up to 846 per 10 s on a road at speed. Cause (inference): physics steps vs frames move the body along
  its path. Fixed after the run: count the off-path part, plus the along-path part beyond one 1/30 s step.
- Below 2 m/s the noise is ~0: server 0 events in every 10 s window; client events below (whole list, 24 lines).
- Lerp: the visual stays within 1-2 cm of the body in the 6 traced frames after a correction (e.g. 02:01:11.047), so
  NO smoothing happens - each correction is a full one-frame snap of the visual. Proven.

| time | body m | visual m | dir speed m/s | throttle | brake | wheels ours/road | collider vs newest m | collider changed s ago |
|---|---|---|---|---|---|---|---|---|
| 02:01:06.569 | 0.266 | 0.220 | fwd 1.80 | 0.00 | 0.53 | 6/0 | 0.146 | 0.085 |
| 02:01:11.047 | 0.236 | 0.205 | stop 1.09 | 0.00 | 0.49 | 6/0 | 0.298 | 0.125 |
| 02:01:11.197 | 0.259 | 0.228 | stop 1.93 | 0.00 | 0.00 | 6/0 | 0.298 | 0.276 |
| 02:01:11.598 | 0.275 | 0.243 | stop 1.71 | 0.00 | 0.00 | 6/0 | 0.298 | 0.261 |
| 02:01:11.749 | 0.258 | 0.229 | stop 1.88 | 0.00 | 0.00 | 6/0 | 0.298 | 0.413 |
| 02:01:12.250 | 0.273 | 0.244 | stop 1.91 | 0.00 | 0.32 | 6/0 | 0.300 | 0.126 |
| 02:01:12.400 | 0.280 | 0.248 | stop 1.87 | 0.00 | 0.45 | 6/0 | 0.300 | 0.276 |
| 02:01:12.551 | 0.295 | 0.262 | stop 1.77 | 0.00 | 0.66 | 6/0 | 0.300 | 0.426 |
| 02:01:12.749 | 0.272 | 0.242 | stop 1.98 | 0.00 | 0.95 | 6/0 | 0.300 | 0.625 |
| 02:01:12.902 | 0.287 | 0.256 | stop 1.88 | 0.00 | 1.00 | 6/0 | 0.300 | 0.777 |
| 02:01:13.402 | 0.284 | 0.253 | stop 1.90 | 0.23 | 0.00 | 6/0 | 0.300 | 0.439 |
| 02:01:13.565 | 0.284 | 0.250 | stop 1.89 | 0.40 | 0.00 | 6/0 | 0.300 | 0.602 |
| 02:01:13.891 | 0.291 | 0.248 | fwd 1.98 | 0.73 | 0.00 | 6/0 | 0.321 | 0.015 |
| 02:01:14.763 | 0.259 | 0.194 | fwd 1.85 | 1.00 | 0.00 | 5/0 | 0.324 | 0.143 |
| 02:01:14.848 | 0.052 | 0.034 | fwd 1.94 | 1.00 | 0.00 | 5/0 | 0.325 | 0.026 |
| 02:01:21.974 | 0.177 | 0.151 | fwd 1.71 | 0.80 | 0.00 | 6/0 | 0.278 | 0.196 |
| 02:01:22.327 | 0.249 | 0.225 | fwd 1.97 | 0.00 | 0.00 | 6/0 | 0.278 | 0.062 |
| 02:01:23.192 | 0.290 | 0.255 | stop 0.48 | 0.00 | 0.66 | 6/0 | 0.275 | 0.165 |
| 02:01:24.225 | 0.315 | 0.283 | rev 1.21 | 0.47 | 0.00 | 6/0 | 0.280 | 0.157 |
| 02:01:24.535 | 0.199 | 0.166 | rev 1.68 | 1.00 | 0.00 | 6/0 | 0.300 | 0.052 |
| 02:01:24.745 | 0.243 | 0.224 | rev 1.06 | 1.00 | 0.00 | 6/0 | 0.311 | 0.054 |
| 02:01:25.138 | 0.228 | 0.245 | fwd 0.91 | 1.00 | 0.00 | 6/0 | 0.315 | 0.043 |
| 02:01:25.443 | 0.180 | 0.127 | fwd 1.99 | 1.00 | 0.00 | 6/0 | 0.317 | 0.142 |
| 02:03:28.374 | 0.249 | 0.220 | fwd 1.91 | 0.00 | 0.00 | 0/6 | 0.000 | 1000.000 |
| 02:04:00.994 | 0.225 | 0.170 | stop 1.42 | 0.00 | 0.00 | 6/0 | 0.223 | 0.105 |

Count check: 24 rows = 23 on our ground + 1 on road (02:03:28.374, the only vanilla-terrain slow correction in ~13 s of slow road driving).

**Root cause found (code + log):** 02:01:11-13 the truck is parked (x/z fixed at 1373.2/2081.4, y swings 39.40-39.54), the
client corrects every ~150 ms (the engine cooldown) with the collider 0.30 m off the newest heights under all 6 wheels.
Server Wheels() skipped vehicles under 0.2 m/s, so nothing touched their verts and the server's wheel hold ran out 0.15 s
after stopping: its collider dropped into the rut. The driver's TouchLocal() touches at every speed, so the driver kept the
old surface. 0.3 m apart > NET_DIST -> correction -> client pushed into its own ground -> pops up -> correction again.
Fix (2026-10-11 ~02:10, not compiled yet): server touches (no press) the wheels of stopped vehicles that are awake or have a pilot.

## 7. Run B result (02:11, hold fix + along-path probe filter, rut speed 3) - measured

- Script errors: server 0, client 0. Server probe: 0 corrections in the whole run (the noise filter works; the server is never corrected).
- Client (driver), totals summed from the 'MUD corr 10 s' lines:

| run | ours slow | ours mid | ours fast | road fast |
|---|---|---|---|---|
| A 01:58 (old detector, mid/fast = noise) | 19 s / 23 | 55 s / 286 | 61 s / 3316 | 31 s / 1979 |
| B 02:11 (new detector) | 23 s / 3 | 44 s / 6 | 24 s / 3 | 13 s / 1 |

- Slow on our ground: 23 -> 3 (about 9x fewer per second). No parked burst any more (max 2 corrections in one 10 s window).
- All 14 client corrections, whole list (body jump m, speed m/s, collider vs newest m):
  13:42.696 0.264 fwd 1.36 0.148 | 14:07.895 0.273 fwd 3.01 0.130 | 14:12.606 0.249 rev 2.64 0.128 | 14:16.742 0.283 rev 2.98 0.120 |
  14:27.455 0.247 fwd 3.85 0.133 | 14:39.444 0.232 fwd 8.25 0.141 | 14:43.891 0.293 fwd 8.29 0.163 | 14:48.599 0.226 fwd 8.85 road |
  15:01.297 0.227 fwd 11.0 0.197 (3 ours/3 road) | 15:05.900 0.280 fwd 4.95 brake 0.143 | 15:08.568 0.335 rev 1.01 0.165 |
  15:09.169 0.291 rev 0.51 0.164 | 15:22.710 0.264 fwd 2.84 0.143 | 15:35.952 0.271 stop 0.01 0.182
  Count check: 14 = 3 + 6 + 3 (ours buckets) + 1 road fast + 1 (02:15:35.952, below the 'driving' gate so in no bucket).
- What is left: on our ground ~1 correction per 7.6 s vs road 1 in 13 s (small road sample). Each is still a full ~0.25 m snap (= NET_DIST).
  Every one is on dirty patches with the collider 0.12-0.20 m off the newest heights (inference: the ground still changes under the
  wheels at different moments on server and driver while digging; rut speed is 3x for test).

## Robot control test 2026-10-11 10:16-10:19 (Exp 192624, DS + PeerPlugin1, MUD_StutterRobot, cap 0.05)

Same move each run: reverse circle, full brake + full steer, 20 s (sim confirms throttle 1, gear 0 = reverse, steer -1).
Log: PeerPlugin1/logs/logs_2026-10-11_10-15-04/console.log.

| run | where | ground (s ours / road) | corrections |
|-----|-------|------------------------|-------------|
| 1 | dirt <2697,2222> | 19.8 / 0 | 3 |
| 2 | tarmac <892,2623> | 0 / 20.0 | 0 |
| 3 | dirt <1093,2601> | 20.0 / 0 | 3 |
| 4 | dirt, same spot again (already rutted) | 20.0 / 0 | 9 |

Dirt 15 corrections in 59.8 s (3+3+9 = 15), tarmac 0 in 20 s. MEASURED: the snaps come from our ground, not vanilla drift.
Every dirt snap: 6/6 wheels on dirty patches, collider vs newest heights 0.047-0.051 m (= the 0.05 cap),
collider changed 0.01-0.19 s before. Run 4: 8 snaps in 8.5 s, bursts every ~0.5 s from 30.3 s.
Next: same test with m_fRutPhysCap 0 (ruts out of physics, rim plow still in).

## Robot test with m_fRutPhysCap 0, 2026-10-11 10:23-10:25 (Exp 192624)

Log: PeerPlugin1/logs/logs_2026-10-11_10-21-37/console.log. Same move, fresh dirt near <1030,2595>.

| run | ground (s ours / road) | corrections | snap times (s after START) |
|-----|------------------------|-------------|----------------------------|
| 1 (stopped by Space at 19.5 s) | 9.1 / 0 | 1 | 5.1 |
| 2 | 20.0 / 0 | 1 | 7.8 |
| 3 | 20.0 / 0 | 1 | 2.1 (+1 at 20.02, 0.02 s AFTER stop, not counted) |
| 4 | 20.0 / 0 | 2 | 9.9, 19.86 |

Cap 0: 5 counted in 69.0 s (1+1+1+2 = 5) vs cap 0.05: 15 in 59.8 s. About 3.5x fewer. MEASURED.
Every cap-0 snap: collider vs newest heights 0, collider last changed 175-269 s before. So the leftover snaps
are NOT ground changes. Tarmac (vanilla terrain) had 0 in 20 s. Left suspects (inference): our collider
itself (triangle mesh vs heightfield, surface material / grip on loose dirt; most snaps show long slip 0.36-1),
or the input release at run end (2 of 6 snaps within 0.15 s of STOP; tarmac stop had none).

## Robot test, skin only (TEMP_SKIN_ONLY, no patches / ruts / net), 2026-10-11 10:30-10:31 (Exp 192624)

Log: PeerPlugin1/logs/logs_2026-10-11_10-29-20/console.log. Same move, dirt near <1035,2585>. Static coarse skin
collider only (probe "collider changed 1000 s ago" = no patch; it still reports the skin as "ours").

| run | s driving | corrections | snap times (s after START) |
|-----|-----------|-------------|----------------------------|
| 1 | 19.8 | 5 | 2.2, 3.7, 4.3, 10.1, 14.6 (+1 at 21.0 after STOP) |
| 2 | 20.0 | 0 | - (+1 at 20.8 after STOP) |
| 3 | 20.0 | 4 | 2.0, 8.2, 9.9, 14.3 |

9 in 59.8 s (5+0+4 = 9). MEASURED: snaps as often on the never-changing skin as on patches with cap 0
(5 / 69 s). So the leftover snaps are NOT our patches. Left (inference): the skin mesh collider itself, or
plain loose-dirt drift vanilla also has. Next control: same move on unholed vanilla terrain dirt.
Also seen: 3 of the last 4 STOPs were followed by a snap 0.02-1 s later (robot drops all input at once).

## Robot test, VANILLA terrain (GM_Arland.ent, no mud ground, mod loaded), 2026-10-11 10:35-10:36 (Exp 192624)

Log: PeerPlugin1/logs/logs_2026-10-11_10-34-25/console.log. Same move, same spot as the mud runs (~<1050,2600>).

| run | ground | s driving | corrections |
|-----|--------|-----------|-------------|
| 1 | vanilla dirt | 19.5 | 5 (2 of them 0.04 s apart in a 0.1 s frame hitch) |
| 2 | vanilla dirt | 20.0 | 3 |
| 3 | vanilla tarmac <938,2640> | 20.0 | 0 |

## All robot runs, same reverse circle (corrections / s driving)

| setup | corrections | s | per 10 s |
|-------|-------------|---|----------|
| vanilla tarmac (2 runs, 10:18 + 10:36) | 0 | 40.0 | 0 |
| vanilla dirt | 8 | 39.5 | 2.0 |
| our skin only (static) | 9 | 59.8 | 1.5 |
| patches, cap 0 | 5 | 69.0 | 0.7 |
| patches, cap 0.05, fresh dirt (runs 1, 3) | 6 | 39.8 | 1.5 |
| patches, cap 0.05, same rutted spot (run 4) | 9 | 20.0 | 4.5 |

MEASURED: vanilla terrain dirt snaps the driver on this move at the same rate as our ground. The leftover
stutter on loose dirt is vanilla NwkCarMovement behaviour (reverse, full lock, wheel spin), not the mud mod.
Only outlier: cap 0.05 on already rutted ground (bursts every ~0.5 s) - the capped rut part still moving the
collider. Run counts are small (0-9 per run), so the middle rows are within noise of each other.

## Surface noise lead (IDA, ArmaReforgerWorkbenchSteamDiag.exe, Exp 192624, 2026-10-11 10:45)

Vehicle contact surfaces (.vhcsurf, measured from the pak): asphalt has NO NoiseSteer / NoiseFrequency / Roughness
(FrictionTread 0.95); dirt NoiseSteer 0.25, NoiseFrequency 0.1, Roughness 0.12, friction 0.45; grass 0.2/0.09/0.1;
soil 0.3/0.22/0.23; gravel 0.25/0.2/0.15; dirt_road 0.1/0.42/0.04.
Engine (read from IDA): sub_140CA20D0 runs per wheel from sub_140E6F4E0 (wheel stride 0x170). It computes
 k = clamp((speed - 1.111) * 0.1125, 0, 1) * Perlin3D(contactX * f, contactZ * f, 0), f = NoiseFrequency + p1,
 steer noise = k * (NoiseSteer + p1*0.01) * vehicleScaleA, roughness = k * (Roughness + p1*0.01) * vehicleScaleB.
 (sub_1422CD670 = classic 3D Perlin: fade 6t^5-15t^4+10t^3, permutation table dword_143B9D6E0.)
 Steer noise is averaged over the wheels into wheel+0x68; roughness stays per wheel (+0x24 of the contact).
 Speed term: 0 below 1.11 m/s (4 km/h), full at 10 m/s. p1 / scales / "speed" source not yet traced (inference).
Inference, unproven: noise depends on contact position (+ speed), so any small server/driver difference feeds
different steer/bump forces into the two sims - a plausible amplifier that asphalt lacks. Friction 0.45 vs 0.95
(wheel spin) is the other candidate.
TEMP test: Common/Materials/Vehicles/{dirt,soil,grass,grass_rough,gravel,gravel_coarse,dirt_road}.vhcsurf
overridden in this mod with NoiseSteer 0 + Roughness 0 (friction kept). Robot START logs the loaded values.

### Noise test result 10:46-10:47 (vanilla GM_Arland, overrides LIVE: START line "dirt.vhcsurf 0/0 grass.vhcsurf 0/0")
Log PeerPlugin1/logs/logs_2026-10-11_10-46-07. 1 run, same spot: 5 corrections / 19.8 s (vanilla with noise: 5, 3, 5, 2, 4).
MEASURED: NoiseSteer/Roughness 0 does not cut the snaps -> surface noise ruled out (assumes the DS loaded the same
overrides; only the client logs it). Run before (10:43, overrides not yet registered - WB not restarted) = 5, 2, 4.
Next: grip test - soft surfaces FrictionTread 0.95 / NonTread 1 (= asphalt), noise still 0. Most snaps show long slip 0.5-1.

### Grip test result 10:49-10:50 (vanilla GM_Arland, START line "dirt 0/0 grip 0.95, grass 0/0 grip 0.95")
Log PeerPlugin1/logs/logs_2026-10-11_10-49-03. Same spot, same move: 0, 0, 0 corrections in 59.5 s
(19.5 + 20.0 + 20.0). Same surfaces with vanilla grip: 2-5 per 20 s run (5+3+5+2+4+5 = 24 in ~119 s over 6 runs).
MEASURED: the driver snaps on loose ground come from LOW GRIP (FrictionTread 0.45-0.58 -> wheel spin, long slip
0.5-1 at most snaps), not surface noise and not the mud mod. Vanilla engine behaviour.
Script lever found: NwkCarMovementComponent.SetAllowance(speedScaledDistance, distance, angle, linVel, angVel)
- vanilla scripts never call it (grep 2026-10-11). Diag defaults (IDA): Distance 30, Angle 3, LinVel 30, AngVel 25.

## Engine deep dive: server approve / correct (IDA, ArmaReforgerSteamDiag.exe EXPERIMENTAL game, 2026-10-11 11:00)

Read from the binary (decompiled), not run:
- sub_140841BE0 = server processes one client move. Two checks, both must pass or it sends a correction:
  - Engine: sub_14075AA30 - engine starting state, engine state, startup state equal (discrete).
  - Physics: sub_14083B890 - server's own result vs the client's reported result for that move:
    1. position distance < allowance[+368]
    2. rotation angle acos(quat dot) < [+372]
    3. |angVel server| - |angVel client| < [+376] (signed, magnitudes)
    4. |vel server| - |vel client| < [+380] (signed, magnitudes)
  - If [+392] speed scaled: allowance[+368] = clamp(speed / [+384], 0.001, 1.5) each check.
- Defaults (ctor sub_14083A3E0, floats decoded): speedScaled 1, [+384] 300, angle 0.003 rad (0.17 deg),
  angVel 0.25, linVel 0.30. So position allowance = speed / 300 m: 2 m/s -> 6.7 mm, 10 m/s -> 33 mm, stop -> 1 mm.
- Script NwkCarMovementComponent.SetAllowance(speedScaled, distance, angle, linVel, angVel) (native sub_140844470)
  writes [+392], [+368] AND [+384] = distance (with speedScaled the distance arg is the DIVISOR), [+372] angle,
  [+380] linVel, [+376] angVel. Vanilla prefabs leave NwkCarMovementComponent empty; no vanilla script calls it.
- Diag "C: Allowance options" items: S: Enable allowance (default false), S: Speed Scaled (true), S: Distance
  0..1000 default 300, S: Angle default 3, S: Angular Velocity 25, S: Linear Velocity 30 (diag x1000/x100 of the
  real values: inference from 300 / 0.003 / 0.25 / 0.30).
- Correction packs the full sim state (sub_14056DF20): body, engine fields, per wheel 5 floats + 3 floats. So the
  driver's wheel spin IS reset by a correction (no missing-state bug).
- Per-check verbose log lines exist ("NVMC SERVER Process Result m_fServerDistanceAllowance %d" etc.) but only when
  globals qword_14248E470 / qword_14248E5B0 (diag flags) are set.
Inference, unproven: the server demands near bit-exact agreement (6.7 mm at robot speed). On grippy asphalt the two
sims stay together; with wheel spin on low grip, tiny differences grow past the limit -> snap. Lever to test:
SetAllowance on the server with a smaller divisor (bigger tolerance).

### Allowance test 11:00-11:01 - INVALID
0, 0, 0 corrections (3 x 20 s), MUD allowance set on server + client. But every soft .vhcsurf logged
"RESOURCES (E): Failed to open" on client and server: the override files were moved out while Workbench (log from
10:48) still mapped those GUIDs to this mod. So dirt ran with no contact surface (unknown grip) - not comparable.
Redo after a full Workbench restart; the START line must show "dirt.vhcsurf 0.25/0.12 grip 0.45".

### Allowance test 11:05 - INVALID again
0, 0 corrections, but START showed dirt/grass "0/0 grip 0.95": the grip-test overrides were still live. Cause: the
backup copies in mud_backups/ kept their .meta (same GUIDs) inside the addon, so Workbench registered the GUIDs to
them. Fixed 11:07: every backup file renamed *.bak. Gotcha: a backup of a GUID-override inside the addon is still live.

### Allowance test 11:10-11:12 - VALID (vanilla GM_Arland, overrides gone)
START: "allowance test true | dirt.vhcsurf 0.25/0.12 grip 0.55 grass.vhcsurf 0.2/0.1 grip 0.57" (vanilla values).
"MUD allowance: ... divisor 30 angle 0.0175 lin 0.6 ang 0.5" on client (PeerPlugin1 11-08-32) AND server
(server/logs/logs_2026-10-11_11-08-30). Note: the dedicated server log is My Games/server/logs, not Workbench/logs.
| run | corrections | s |
|---|---|---|
| 1 | 3 | 20.0 |
| 2 | 3 | 20.0 |
| 3 | 3 | 20.0 |
| 4 | 2 | 20.0 |
Total 11 in 80 s (3+3+3+2 = 11) = 0.14/s vs vanilla limits 24 in ~119 s = 0.20/s. ~10x looser limits cut snaps only ~30%.
Inference: most snaps are not small drift just over the limit. Either the divergence is large, or a check SetAllowance
does not cover trips (engine/gearbox state compare sub_14075AA30). Next: Diag exe NVMC per-check log
("NVMC SERVER Process Result m_fServer...Allowance %d") via Vehicles > Vehicle Networking and Reconciliation.
Read (IDA sub_140844470, 11:20): SetAllowance stores distance into both +384 (divisor) and +368 (allowance); speedScaled
false -> +368 stays fixed. So the speed-scaled default shrinks to 1 mm near a stop (matches "stutter at the end").
Engine check (sub_14075AA30) compares only engine state / starting state / startup state - not gear; unlikely while driving (inference).
Next test set 11:20: SCALED false, distance 1 m, angle 3.2, lin 100, ang 100 (phys check effectively off).
 Snaps left -> not the phys tolerance (engine check or another path). Snaps gone -> tolerance; then tune back.

### "Phys check off" test 11:22-11:23 (vanilla GM_Arland, vanilla grip 0.55)
"MUD allowance: ... divisor 1 angle 3.2 lin 100 ang 100" (SCALED false = fixed 1 m) on client + server.
| run | corrections | s |
|---|---|---|
| 1 | 4 | 20.0 |
| 2 | 4 | 20.0 |
8 in 40 s = 0.20/s = same as vanilla limits (24/~119 s). MEASURED: loosening the phys tolerance does not remove the
snaps. Either (a) the snaps don't come from the server's phys check (engine check or another path), or (b) the
SetAllowance values are reset after EOnInit (no getter in script to check). No NVMC lines in client or server logs.
Probe "corrected" = body off its expected path by > 5 cm in one frame (MUD_JoltProbe.c:129), any cause.
Driver-only: the driver's machine runs the car sim and gets rewound; passengers only see smoothed server motion.
Next: get the NVMC per-check log. Needs the diag flags on the SERVER process: run Workbench as the host (no
dedicated server) and enable Vehicles > Vehicle Networking and Reconciliation logging there.

### Workbench-as-host test 11:29-11:30 (no dedicated server; server stopped 11:28:07, WB created the player)
User: "it stutters in the workbench test too". Probe: run 1 0 corrections 20.0 s, run 2 0 corrections 20.0 s;
worst frame dt 0.019-0.025 s (no frame hitches). The host drives the authority sim, so no server snap is possible.
MEASURED: what the user sees on the host is NOT a > 5 cm body jump and not a frame hitch. Unresolved: whether it is
the same stutter as on the client (smaller judder the probe threshold misses? camera? wheel/suspension?).
No NVMC lines (diag logging not enabled in this run).

## Raw per-frame log (MUD_FrameLog.c, TEMP, added 2026-10-11 ~11:45)
Goal: find WHAT moves when it stutters, WB host first (no network), then DS client. No thresholds.
- One CSV per robot run, local driver only: $profile:MUD_framelog_<host|client>_run<N>_<tick>.csv
  (path in the 'MUD framelog: run N role ... file ...' log line). Called from MUD_JoltProbe.Tick after the robot.
- Per frame: real tick ms, world ms, dt, speed; physics body pos + ypr; visual (entity transform) pos + ypr;
  visual - body in body space; camera pos + yaw and camera - visual in vehicle space; throttle/brake/steer/gear;
  per wheel contact, contact height in body space, WheelGetPosition y, lat/long slip, rpm.
- Limits: no suspension-compression getter in VehicleWheeledSimulation (API read 11:40), so wheel heights are the
  proxy. Logged from OnPrepareControls (vanilla world): the camera may be one frame behind (order unproven).

### WB-host runs 11:37-11:39 (vanilla GM_Arland, truck 154, dirt ~1040-1060 / 2608-2626, MUD_AllowanceTest phys-off still ON)
Files: ArmaReforgerWorkbench/profile/MUD_framelog_host_run{1_83277,2_162423,3_188363}.csv (run 1 stopped by Space at 7.1 s).
Probe: 0 corrections all 3 runs. Analysis scripts: per-frame deltas, "local median" = 15-frame centred median, moving = speed > 0.5 m/s.
| signal | run 1 (468 fr, 66 fps) | run 2 (1493 fr, 75 fps) | run 3 (1187 fr, 59 fps) |
|---|---|---|---|
| frames with dt > 25 ms | 1 (fr 34, 51 ms, standing) | 0 | 2 (fr 324 52 ms, fr 330 33 ms, moving) |
| physics body step / (speed / 60), p1 / p50 / p99 / max | 0.99 / 1.00 / 1.01 / 1.01 | 0.99 / 1.00 / 1.01 / 1.01 | 0.99 / 1.00 / 2.08 / 3.09 (the hitch frames) |
| frames body did not move (no physics step) | 40 of 235 moving | 290 of 1474 | 23 of 1170 |
| visual step / local median, p5 / p95 | 0.94 / 1.06 | 0.93 / 1.08 | 0.94 / 1.07 |
| frame dt / local median, p5 / p95 | 0.96 / 1.05 | 0.96 / 1.06 | 0.96 / 1.06 |
| corr(visual step jitter, dt jitter) | -0.21 | -0.18 | -0.00 |
| visual speed / local median, p1 / p99 | 0.88 / 1.14 | 0.87 / 1.16 | 0.89 / 1.15 |
| visual pos 2nd difference, p50 / p99 / max (m) | 0.0012 / 0.0042 / 0.0061 | 0.0014 / 0.0049 / 0.0082 | 0.0014 / 0.0056 / 0.0572 (hitch) |
| visual vs body step frames: visual step / median, with step / without | 1.00 / 1.01 | 1.00 / 0.99 | 1.00 / 0.98 |
| camera-vs-car per-frame change p50 (m) | 0.0007 | 0.0009 | 0.0011 |
| camera-vs-car big changes | p99 0.44, max 3.19 | p99 0.08, max 0.20 | p99 0.08, max 0.64 |
| wheel RPM single-step spikes (> 3x both neighbour steps) w0 / w1 / w2 / w3 / w4 / w5 | 64 / 64 / 27 / 9 / 4 / 4 of 195 steps | 436 / 429 / 190 / 95 / 92 / 39 of 1184 | 504 / 498 / 264 / 118 / 62 / 30 of 1149 |
| wheel RPM p50 / p95 abs, w0 / w1 | 36 / 64, 40 / 73 | 38 / 69, 40 / 72 | 37 / 70, 39 / 74 |
Reading (MEASURED unless marked):
- Physics body: fixed 60 Hz steps, each exactly speed/60 long. No body jumps except 2-3 steps in one frame at the
  3 frame hitches (run 3 fr 324/330). Body is NOT the stutter on the host.
- Visual (entity transform): interpolated; per-frame step varies +-6-8 % (p5-p95) and does NOT follow frame dt
  (corr ~0), i.e. ~1.5-2 mm per frame of speed wobble at 2 m/s. Not tied to frames with/without a physics step.
  Whether that is visible is UNRESOLVED (needs a tarmac control run: same numbers there -> not the dirt stutter).
- Camera: the big camera-vs-car changes are smooth 20-30 frame sweeps (run 2 fr 53-80, 1139-1186: mouse/free-look
  or camera swing, monotonic, no sign flips). No single-frame camera jump found.
- Wheels: front wheels w0/w1 alternate RPM between ~2 and ~60-70 on ~37-43 % of physics steps (example run 2
  fr 598-604: w0 2.5, 60.0, 1.4, 1.4, 64.9, 2.0, 70.6; rolling speed at 1.7 m/s is ~30 rpm for r ~0.55 m, inference),
  slips jump 0.0 <-> 1.0 with it. Rear wheels far less. This is a per-step stick-slip of the steered wheels.
  INFERENCE, unproven: (a) if the wheel mesh rotation follows this RPM, the wheels visibly judder on the host;
  (b) a per-step chaotic wheel state is exactly what lets driver and server sims diverge on low grip -> snaps on dirt,
  none on tarmac or grip 0.95. Control needed: the same run on tarmac (expect few or no RPM spikes if this is it).
- No Diag SimState CSV yet (no VehicleWheeledStateDebug folder under any profile, 11:45).

### WB-host tarmac control 11:45 (runs 4-5, runway ~880 / 2606-2626, same robot, same truck 154)
Files: MUD_framelog_host_run4_535075.csv (1475 fr), MUD_framelog_host_run5_558921.csv (1342 fr). Probe 0 corrections.
| signal | dirt runs 2 / 3 (from above) | tarmac run 4 | tarmac run 5 |
|---|---|---|---|
| frames with dt > 25 ms | 0 / 2 | 0 | 1 (fr 536, 36 ms) |
| body step / (speed / 60), p50 / p99 / max | 1.00 / 1.01 / 1.01, 1.00 / 2.08 / 3.09 | 1.00 / 1.00 / 1.97 | 1.00 / 1.00 / 2.98 |
| visual step / local median, p5 / p95 | 0.93 / 1.08, 0.94 / 1.07 | 0.95 / 1.06 | 0.96 / 1.06 |
| visual pos 2nd diff p50 / p99 (m) | 0.0014 / 0.0049, 0.0014 / 0.0056 | 0.0010 / 0.0049 | 0.0010 / 0.0044 |
| wheel RPM single-step spikes w0..w5 | 436/429/190/95/92/39 of 1184, 504/498/264/118/62/30 of 1149 | 0/0/0/0/0/0 of 1182 | 0/0/0/0/0/0 of 1183 |
| w0 RPM p50 / p95 abs | 38 / 69, 37 / 70 | 39 / 41 | 39 / 40 |
Reading:
- MEASURED: the front-wheel stick-slip is dirt-only: 0 RPM spikes on tarmac (RPM steady 34-40) vs ~40 % of steps on dirt.
- MEASURED: visual wobble (+-6 %) and body stepping are the same on tarmac -> not the dirt cause.
- MEASURED (user, 11:47, + data): the host "stutter" was the Diag collider/physics debug draw. The debug shape sits on the
  physics body, which moves in 60 Hz steps (no move on ~20 % of frames at 75 fps: run 2 290 of 1474), while the mesh is
  interpolated -> the debug shape jolts and "catches up" to the mesh. The car itself is smooth on the host.
  So the host has NO real stutter; the real one is client-only (driver snaps, 0.20/s on dirt).
- Fits so far: dirt = stick-slip + snaps; tarmac = no stick-slip + 0 snaps; grip 0.95 = 0 snaps (stick-slip not measured).
  INFERENCE, unproven: the per-step stick-slip makes driver and server sims diverge (chaotic wheel state) -> snaps.
Next: 2 DS-client dirt runs. Check (a) client shows the same stick-slip, (b) whether each probe correction
(PeerPlugin1 console.log 'MUD corr:' time) lands in a stick-slip burst (CSV tick_ms; tick at file open is in the file name,
log time of the 'MUD framelog ... open' line maps the two clocks).
Setup 11:50 for the client runs: CSV gets a probe_corr column (MUD_JoltProbe body jump in m on the frame it counted a
correction, else 0; FrameLog.Tick moved after the probe's vehicle loop). MUD_AllowanceTest.ON = false (vanilla limits).

### DS-client dirt runs 11:53-11:54 (vanilla GM_Arland, vanilla limits, truck 107, dirt ~1034-1057 / 2575-2587)
Files: PeerPlugin1/profile/MUD_framelog_client_run{1_42250,2_68403,3_92777}.csv (1782 / 1940 / 1922 frames).
Log: PeerPlugin1/logs/logs_2026-10-11_11-52-30. Probe: 7 + 7 + 4 = 18 snaps in 60.0 s = 0.30/s (one more after run 2 STOP, not in CSV).
Per snap ("body steps" = body move that frame / (speed/60); "along" = signed move along the last 5 frames' path, + = forward;
"vis/med" = visual move that frame / its 15-frame median; "spikes" = front-wheel RPM spike steps in the previous 30 physics steps,
15/30 = steady stick-slip every other step):
| run | frame | t s | dt | m/s | probe m | off path m (log) | body steps | along m | vis/med | spikes prev 30 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 842 | 8.58 | 0.0128 | 2.06 | 0.240 | 0.039 | 6.25 | -0.212 | 8.2 | 15 |
| 1 | 1066 | 11.25 | 0.0134 | 2.05 | 0.266 | 0.013 | 6.99 | -0.238 | 7.8 | 3 |
| 1 | 1114 | 11.85 | 0.0095 | 1.69 | 0.268 | 0.008 | 8.85 | -0.250 | 8.4 | 6 |
| 1 | 1249 | 13.53 | 0.0103 | 2.22 | 0.323 | 0.017 | 9.25 | +0.342 | 14.8 | 15 |
| 1 | 1403 | 15.43 | 0.0170 | 2.17 | 0.295 | 0.012 | 9.04 | +0.327 | 12.8 | 15 |
| 1 | 1692 | 18.93 | 0.0118 | 1.84 | 0.231 | 0.017 | 6.82 | -0.209 | 8.0 | 15 |
| 1 | 1762 | 19.77 | 0.0122 | 1.64 | 0.267 | 0.009 | 8.92 | -0.244 | 9.6 | 15 |
| 2 | 831 | 8.80 | 0.0105 | 2.28 | 0.327 | 0.017 | 9.15 | +0.348 | 18.6 | 14 |
| 2 | 942 | 9.94 | 0.0103 | 2.16 | 0.299 | 0.007 | 8.88 | +0.319 | 16.0 | 15 |
| 2 | 1089 | 11.44 | 0.0094 | 1.88 | 0.281 | 0.016 | 8.32 | -0.261 | 10.8 | 0 |
| 2 | 1165 | 12.21 | 0.0112 | 1.77 | 0.289 | 0.007 | 9.06 | -0.267 | 10.3 | 0 |
| 2 | 1738 | 17.94 | 0.0103 | 2.08 | 0.230 | 0.015 | 6.07 | -0.210 | 8.5 | 13 |
| 2 | 1786 | 18.43 | 0.0101 | 1.68 | 0.306 | 0.006 | 10.19 | -0.286 | 12.2 | 0 |
| 2 | 1911 | 19.69 | 0.0108 | 1.89 | 0.268 | 0.004 | 7.84 | -0.247 | 9.8 | 4 |
| 3 | 202 | 2.15 | 0.0123 | 2.01 | 0.268 | 0.013 | 7.18 | -0.241 | 8.4 | 0 |
| 3 | 402 | 4.35 | 0.0130 | 2.13 | 0.282 | 0.002 | 8.67 | +0.308 | 13.1 | 15 |
| 3 | 561 | 6.19 | 0.0110 | 2.19 | 0.297 | 0.005 | 8.74 | +0.319 | 14.4 | 15 |
| 3 | 1642 | 17.02 | 0.0100 | 1.75 | 0.266 | 0.005 | 9.75 | +0.283 | 13.7 | 15 |
Checksum: 7 + 7 + 4 = 18 rows. Forward 7, backward 11 (7 + 11 = 18).
Whole-run numbers:
| | run 1 | run 2 | run 3 |
|---|---|---|---|
| physics steps per frame (rounded): 0 / 1 / 2 / 6+ | 413 / 946 / 3 / 7 | 730 / 1169 / 2 / 7 | 711 / 1171 / 2 / 4 |
| non-snap frames with visual move > 1.8x median | 4 of 1369 | 2 of 1908 | 9 of 1888 |
| front-wheel spike rate per physics step | 0.36 | 0.31 | 0.42 |
| share of moving time with >= 10/30 spikes in prev 30 steps | 0.70 (676/961) | 0.58 (685/1182) | 0.82 (975/1183) |
(6+ frames: 7 + 7 + 4 = 18 = the snaps.) Non-snap > 1.8x frames are a 2-step frame after a long frame (e.g. run 2 fr 84 dt 32 ms, fr 85 2 steps).
Reading:
- MEASURED: the client stutter is the snaps and only the snaps. Between them the client is as smooth as the host
  (1 physics step per frame or 0, visual steady). Each snap moves the body 0.21-0.35 m in one frame = 6-10 physics steps of
  travel (100-170 ms at the current speed), almost purely ALONG the path (off path 2-39 mm), 7 forward and 11 backward.
- MEASURED: the visual jumps the same frame by about the same distance (8-19x its normal step; probe visual jump 0.20-0.32 m)
  and the next 5 frames are normal (0.5-1.7x): no smoothing, the snap is shown in full.
- MEASURED: snaps are NOT tied to a stick-slip burst at that moment: 12 of 18 snaps came during a burst (>= 10/30), and the car was
  in a burst 70 % of the moving time overall (2336/3326). 6 snaps came with steady wheels (0-6/30). Stick-slip = dirt-only (host
  tarmac control) still holds at run level, but it is not the per-snap trigger.
- INFERENCE, unproven: a fixed-size jump of 6-10 steps of travel along the path, in both directions, looks like a correction applied
  at the wrong time (state of tick N applied ~6-10 ticks off, or wrong replay count), not a slowly growing position drift: a drift
  over a 6.7 mm limit would give mm-cm snaps. Fits the 11:23 "phys check off" run still snapping at the same rate.
Next options: (1) Diag NVMC per-check logging / SimState CSV on the DS process to see what the server sends and when;
(2) IDA: client side of the correction (reset to server state + replay of unconfirmed moves) - how it picks the tick.

## Engine read: client side of a correction (IDA, Exp 192624 Diag exe, read after the 12:02 runs) - read, not run
- sub_140841A40 = client receives a correction for move N:
  1. sub_140842EC0: drop history moves older than N ("NVMC REMOVE UNTIL ..." lines, gated by "Debug history").
  2. If "C: Disable Correction" (Prediction options, dword_14248EDB0) is on: the correction is ignored entirely.
  3. If the oldest kept move == N: sub_140843540 sets the body to the server transform + sim state (and stores the old visual
     pose in a small ring at +1264 for the lerp), pops N, then sub_140843070 replays every remaining move: re-apply its inputs,
     one physics step per move (only if the move's +136 flag is set), store the new velocities into the move, mark it +3.
  4. If the oldest kept move is NEWER than N: warning "NVMC Huge round trip time warning, Client cannot rewind! Must be snapped!"
     and the server state is applied with NO replay (= the car is put where it was ~RTT ago). Not in any client log so far (grep 12:04).
  "NVMC CLIENT CORRECTING %u" is logged when "Debug path correction" is on, at log level 4: none in console.log (12:04),
  so either the item was not on in the client process or level 4 does not reach console.log (unresolved).
- Replay physics: "Collision prediction" (dword_14248E3E8, options "true,false") picks the step path: default (true) steps the
  body through the world (with collisions); false integrates the body alone. Inference from the two call shapes.
- Diag item map (registration functions read): "Debug history" = dword_14248E5B8, "C: Disable Correction" = dword_14248EDB0,
  "WS of (U) to CSV" = dword_14248E3C0 (writes the CSV from inside the replay loop), "InitState to CSV" = dword_14248E6F8,
  "SimState to CSV" = dword_14248E428 (writes when a move is generated / processed).
- SimState CSV as written (PeerPlugin1/logs/logs_2026-10-11_11-52-30/VehicleWheeledStateDebug_0.txt, checked after 12:02): the header
  lists ~200 columns but each move writes only 11 values (MoveId, 0S/1C, Generated, Corrected, IsSimulating, steer, throttle,
  brake, clutch, gear, handbrake) and no newline - parse as groups of 11. 5158 client moves, ids 20087-25157, 14 id gaps,
  Corrected = 1 on 6. File stopped growing 11:59:56.
INFERENCE, unproven: an along-path jump of 6-10 steps fits a replay that runs a different number of physics steps than the
original prediction did (one step per move with the +136 flag vs the frames/steps the client really ran), or the
"cannot rewind" snap. Cheapest test: "C: Disable Correction" on the client -> if the snaps vanish they are corrections; then
compare replay step count vs real steps.

### DS-client runs 4-10, 11:59-12:02 (log logs_2026-10-11_11-52-30, MUD_framelog_client_run4..10)
| run | start x / z | probe snaps | front spike steps w0 / w1 | max body step (steps) | surface (from stick-slip + position) |
|---|---|---|---|---|---|
| 4 | 1037 / 2580 | 7 | 241 / 240 of 1174 | 10.64 | dirt |
| 5 | 1058 / 2591 | 5 | 465 / 463 of 1180 | 9.88 | dirt |
| 6 | 1036 / 2585 | 1 | 428 / 425 of 1186 | 8.97 | dirt |
| 7 | 951 / 2636 | 0 | 0 / 0 of 1180 | 2.04 | tarmac (inference: no stick-slip) |
| 8 | 936 / 2647 | 0 | 0 / 0 of 1154 | 1.01 | tarmac (inference) |
| 9 | 1037 / 2588 | 0 | 528 / 526 of 1181 | 1.93 | dirt |
| 10 | 1038 / 2611 | 0 | 485 / 480 of 1184 | 2.10 | dirt |
Checksum: snaps 7 + 5 + 1 + 0 + 0 + 0 + 0 = 13. Runs 9-10: dirt, full stick-slip, 0 snaps in 40 s (vanilla dirt rate 0.20-0.30/s
would give ~8-12). User 12:05: these runs had "C: Disable Correction" on (9-10 inferred from the 0 snaps, 11 stated).
SimState file (now 15601 moves, no malformed rows): Corrected = 1 on only 6 moves (ids 21899-21903, twice, before run 4); none in
runs 4-6 although the probe saw 13 snaps -> the client SimState "Corrected" column does not mark these snaps (unresolved why).
Moves per robot run 1396-1848 in 20 s (~70-90/s) vs 59.5 physics steps/s and ~100-110 frames/s: a move is neither a physics
step nor a frame (unresolved).

### "C: Disable Correction" run 11, 12:04 (client diag, dirt 1034 / 2588)
MUD_framelog_client_run11_705325.csv, 2164 frames: probe snaps 0, max body step 2.07 steps (normal 2-step frame), front spike steps
538 / 537 of 1183 (stick-slip as strong as ever).
MEASURED: with the client ignoring corrections, dirt driving has 0 snaps in 60 s (runs 9-11; 9-10 inferred to have it on) while the
wheels stick-slip as before. So the driver stutter IS the client applying server corrections; the stick-slip itself does not jolt.
Confirm runs 12-13 (12:04:50-12:05:37, Disable Correction on, dirt 1032-1041 / 2588-2610): probe 0 + 0 snaps, max body step
1.01 / 1.97 steps, front spike steps 493 / 1184 and 554 / 1183. 'MUD corr:' lines 12:02:20-12:05:53 (log end): 0.
Total with Disable Correction on: runs 9-13 = 5 x 20 s = 100 s on dirt, 0 snaps (vs 31 snaps in runs 1-6, 120 s, with it off:
7 + 7 + 4 + 7 + 5 + 1 = 31).

## Engine read: server side of a correction (IDA, Exp 192624 Diag exe, read after run 13) - read, not run
All functions in the Diag exe; the DS exe was not opened (inference: same code).
- Per tick, server (sub_1408417C0): sub_140841BE0 checks the oldest queued client move, then sub_14083BB00 applies its inputs.
  Empty queue -> "NVMC NO MOVE TO CONSUME" and the car steps with null inputs (sub_14078F200(ctrl, 0)).
- Buffering: a1+488 is set when the queue length > a1+352 (= "S: Replay delay" diag + 2, default 2) and cleared when the queue
  empties. While cleared, no checks run.
- Check order in sub_140841BE0: the server's CURRENT body transform (before this move's inputs are applied) vs the client's
  reported result in the move.
  - Engine check sub_14075AA30 (only if the move carries engine data, move +52 != 0): engine state, engine starting state,
    startup state must equal the server's. Not a physics test.
  - Phys check sub_14083B890: distance (|pos diff| < +368), angle (acos(dot) < +372), |w| server - |w| client < +376, |v|
    server - |v| client < +380. The two speed tests are SIGNED: a server that is slower than the client never fails them.
  - Speed-scaled (+392 true): +368 = |v| / +384 every check, clamped to 0.001..1.5 m.
  - Both pass -> approve, cooldown a1+400 = 0. Else, if cooldown == 0 -> move +3 = 1, store move id (a1+1848), the CURRENT
    server transform (a1+1904) and sim state, send, cooldown = (rtt >> 4) - queue + 6, capped at "S: Max Rtt allowed" + 60.
  - A move with +2 set skips both checks (meaning unresolved).
- SetAllowance native (sub_140844470): divisor -> +384 AND +368, angle -> +372, linear -> +380, angular -> +376, scaled -> +392.
  The only other code reading these names (sub_14083E6C0) is a debug-graph display, not a reset. So the 11:23 values were
  very likely live on the server: fixed 1 m distance, 3.2 rad, 100, 100 (server log 11:22:52 "MUD allowance ... divisor 1",
  1 vehicle only - which vehicle was not checked).
- Script API of NwkCarMovementComponent: SetAllowance, SetPrediction, EnableSimulation, EnableInterpolation, Is*. No script
  setter for replay delay, max rtt or resend rate; those diag values are applied per process in sub_14083ED70.

What this says about the two open questions (inference, unproven):
1. Why corrections still came with the 11:23 limits: with 1 m / 3.2 rad / 100 / 100, the phys check can only fail if the server
   and client are > 1 m apart; the engine check is not about motion. So either the gap really reaches 1 m (server stepping
   without moves while its queue is dry: "NO MOVE TO CONSUME" -> null inputs -> it brakes/coasts and falls behind), or the
   11:23 snaps were not the server phys check at all. Unresolved; the 11:23 snaps were not checked in SimState.
2. Why each snap is 6-10 steps along the path: the server sends its transform as of the moment it CHECKED move N, which is
   before move N's own inputs. If the server's step count and the client's move ids drift apart (queue dry -> extra null-input
   steps, rebuffering -> paused checks), the server state is k steps off the client's state for "move N", and the replay puts
   the client k steps off along its path. Fits the measured along-path, both-direction snaps. Not proven.

Decisive test (needs the server-side diag items, so the server must be the Diag exe):
Workbench as host + PeerPlugin client driving the robot on dirt. On the WB host: "Debug path correction" (qword_14248E470 ->
per-move "Engine %d; Phys %d", per-allowance pass/fail, "SERVER CORRECTING %u") and "SimState to CSV". Then raise
"S: Replay delay" (2 -> 8). If snaps drop with a bigger replay delay, queue starvation is the cause. Earlier level-4 NVMC lines
never reached console.log on the client (unresolved whether level 4 is logged at all).

### Replay-delay test 12:22-12:25 - NOT the planned setup
Workbench started the dedicated server again (12:19:02 "Starting dedicated server: ...ArmaReforgerServerDiag.exe", server log
logs_2026-10-11_12-19-02) + PeerPlugin1 client (log logs_2026-10-11_12-19-04), truck 186, dirt ~1045-1068 / 2579-2599.
| run | start | snaps (robot STOP line) | CSV |
|---|---|---|---|
| 1 | 12:22:48 | 0 | MUD_framelog_client_run1_225206.csv |
| 2 | 12:23:12 | 2 | MUD_framelog_client_run2_249517.csv |
| 3 | 12:24:37 | 3 | MUD_framelog_client_run3_334799.csv |
| 4 | 12:25:01 | 5 | MUD_framelog_client_run4_359160.csv |
Checksum 0 + 2 + 3 + 5 = 10 in 80 s = 0.125/s (vanilla 0.20-0.30/s).
NVMC lines: 0 in the server, WB and client console.log. Replay delay is a SERVER setting (a1+352 gates the server's checks);
set in the client's diag menu it has no effect on the server (inference from the per-process apply in sub_14083ED70). The DS has
no diag menu. Which runs had replay delay 8 was not stated. Result: inconclusive.

### Why 0 NVMC lines so far (IDA read 2026-10-11 ~12:40, Exp 192624 Diag exe, read not run)
- All NVMC lines go through sub_140181170(4, 27, ...) -> sub_1409144D0, which drops any line with level < global 0x1423735FC.
  Its default is 8 ("normal"). sub_1409234C0 sets it from the "-logLevel" launch param: spam 1, verbose 2, debug 4, normal 8,
  warning 16, error 32, fatal 64. Level 4 = debug. So without "-logLevel debug" (or verbose/spam) every NVMC line is dropped,
  whatever the diag menu says. All earlier runs (WB with no args, peer client without -logLevel) explain the 0 lines.
- Gates: "SERVER CORRECTING %u", "SERVER APPROVING %u", "Process Result: Engine %d; Phys %d" = "Debug path correction"
  (qword_14248E470). "NVMC NO MOVE TO CONSUME" (sub_14083BB00) = a DIFFERENT item, "Debug history" (dword_14248E5B8).
- Control for the next run: "SERVER APPROVING" fires on every passed check, so if it shows up the channel works and a 0 for
  CORRECTING / NO MOVE is real.

### WB-host attempt 12:49-12:52 - VOID (robot driven by the host, no client)
WB started 12:39:23 with -logLevel debug (266 "(D)" lines in its console.log: debug level reaches the log). WB play started
12:49:22 "Starting RPL server, listening on 0.0.0.0:2001". The only PeerPlugin1 client (log logs_2026-10-11_12-46-52, also
-logLevel debug) was started BEFORE the WB server and only sent "L2 handshake" to 127.0.0.1:2001 (181 tries) until its log
ends 12:48:26; no client was launched after that (no newer PeerPlugin1 log, last WB "Running command" 12:46:51).
So the 5 robot runs ran on the WB host itself (MUD_framelog_host_run1..5, 12:50:37-12:52:46, truck 157, ~1035-1056 / 2594-2619):
| run | start | probe corrections | NVMC lines |
|---|---|---|---|
| 1 | 12:50:37 | 0 | 0 |
| 2 | 12:51:02 | 0 | 0 |
| 3 | 12:51:26 | 0 | 0 |
| 4 | 12:52:01 | 0 | 0 |
| 5 | 12:52:26 | 0 | 0 |
Checksum corrections 0 x 5 = 0; NVMC lines in the whole WB log: 0. Expected for a host-driven car (the host is the authority,
no client moves to check), so this says nothing about starvation and does NOT test the log channel (no APPROVING either).
Next: WB in play mode first, THEN start the client so it can connect, and drive the robot from the client window.

### WB-host + client session 12:56-12:57 - first real NVMC data (WB log logs_2026-10-11_12-39-23, client logs_2026-10-11_12-54-46)
Setup: WB host (-logLevel debug, diag "Debug path correction" + "Debug history" + "SimState to CSV"), PeerPlugin1 client
(-logLevel debug, started BEFORE pressing Play; Play "Server localhost + PeerTool"). Truck 146, dirt ~1049 / 2595.
Client drove 12:56:22-12:57:13 (server NVMC lines span exactly that: "Processing 0" 12:56:22.133 .. "APPROVING 3079" 12:57:13.010).
Only ONE robot run (12:56:27, Space-stopped at 5.95 s, speed 0.0009 m/s at 3 s = the truck did not move); the rest was manual.
After 12:57:13 the WB host drove (WB probe 'mine 1' 12:57:34+); client probe 'mine 0' snaps after that are a proxy view.
Replay delay changes: none identifiable (no 4-run set) -> the replay-delay comparison was NOT done.

Server NVMC line counts (WB console.log, whole session): NO MOVE TO CONSUME 6 (all 12:56:21.540-.542, before the first
"Processing 0"; 0 while driving), Process Result: Engine/Phys 740, APPROVING 705, CORRECTING 28, Phys 0 lines 0,
"Engine 0; Phys 1" 35. Checksum: 705 + 35 = 740 checks; 35 engine fails - 28 sent = 7 inside the resend cooldown.
Client: CLIENT APPROVING 188, CLIENT CORRECTING 8.
Meaning of the per-field lines (IDA sub_140841BE0): the three engine lines print (client value != server value), so 1 = DIFFERS;
the four allowance lines + "Phys" print 1 = pass. Engine check sub_14075AA30 = exact equality of 3 fields:
move+2 vs ctrl+2153 (engine starting state), move+0 vs ctrl+2151 (engine state), move+4 vs ctrl+2144 (startup state).
| decision | count | engine starting state differs | engine state differs | startup state differs | 4 allowances |
|---|---|---|---|---|---|
| APPROVE, engine fields printed equal | 336 | 0 | 0 | 0 | pass |
| APPROVE, engine fields printed differ (unresolved: check passed anyway) | 6 | 1 | 1 | 0 | pass |
| APPROVE, move without engine data (no engine lines) | 363 | - | - | - | pass |
| CORRECT | 28 | 1 | 1 | 0 | pass |
Checksum 336 + 6 + 363 + 28 = 733 decisions (705 + 28); 740 - 733 = 7 = failed checks inside the cooldown.
Correction bursts (server ids sent -> id the client applied, client time, client probe snap > 5 cm 'mine 1'):
| burst | server CORRECTING ids | client applied | client time | probe snap |
|---|---|---|---|---|
| 1 | 511 513 515 545 547 | 547 | 12:56:31.025 | none (truck stopped, robot) |
| 2 | 1044 1046 1048 | 1048 | 12:56:39.385 | none |
| 3 | 1205 1207 | 1207 | 12:56:41.989 | none |
| 4 | 1629 1631 1633 | 1633 | 12:56:49.049 | none |
| 5 | 1789 1791 1793 1823 1825 | 1825 | 12:56:52.407 | 12:56:52.426 body 0.051 m, gear 2 throttle 1 |
| 6 | 2242 | 2242 | 12:56:59.239 | none |
| 7 | 2380 2382 2384 2414 2416 | 2416 | 12:57:02.171 | none |
| 8 | 2572 2574 2576 2607 | 2607 | 12:57:05.421 | none |
Checksum ids 5+3+2+3+5+1+5+4 = 28 = CORRECTING count. SimState: server rows 740 (= checks; ids step 1 x665, 37 x42, 28 x29),
client rows 3229; inputs (steer/throttle/brake/clutch/gear/hb) identical on both sides at every corrected id checked
(511, 545, 1044, 1205). The SimState has no engine-state column.

MEASURED (this session, 51 s of client driving on dirt, mostly slow manual):
- The queue never ran dry while the client drove (0 NO MOVE lines after 12:56:22). Starvation did not happen here.
- The server's PHYSICS check passed 740 / 740. Every correction (28 / 28) was the ENGINE check: engine state and engine
  starting state differ between the client's move and the server; startup state equal; inputs equal.
NOT shown: that this is also what fails in the earlier DS runs at 0.20-0.30 snaps/s (those had no NVMC log), and why the engine
states differ (value pair not logged). Only 1 of 8 applied corrections gave a probe snap > 5 cm (slow/stopped driving).

### WB-host + client robot runs 13:11-13:13 - snaps counted, NO NVMC (log level lost)
WB restarted 13:08:03 WITHOUT -logLevel debug (process command line has no args; 0 "(D)" lines in WB log logs_2026-10-11_13-08-03).
Client launched 13:09:14 by WB Play (CLI params without -logLevel; 0 "(D)" lines in logs_2026-10-11_13-09-15). So 0 NVMC lines
in both logs is the log filter, not a result. Client drove (robot in the client window), truck 131, dirt ~994-1016 / 2590-2613.
| run | start | robot STOP corrections (client probe) | speed at 3 s | replay delay (per plan, not confirmed) |
|---|---|---|---|---|
| 1 | 13:11:39 | 1 | 2.27 m/s | default |
| 2 | 13:12:08 | 10 | 1.69 m/s | default |
| 3 | 13:12:58 | 1 | 1.94 m/s | 8 |
| 4 | 13:13:24 | 4 | 1.54 m/s | 8 |
Checksum 1 + 10 + 1 + 4 = 16 in 80 s = 0.20/s (= vanilla dirt rate). Default 11 / 40 s vs 8: 5 / 40 s, but run-to-run spread is
large (1 vs 10 at the same setting; 12:25 test 0/2/3/5), so this does NOT separate the settings. Which runs had 8: unconfirmed.

### Robot runs 13:16-13:18 - client debug only (server WB still the 13:08 instance, 0 "(D)" lines)
Client log logs_2026-10-11_13-15-54 (12251 "(D)" lines), truck 102, dirt ~994-1032 / 2583-2603. Server-side NVMC: none (WB not
restarted with -logLevel debug), so WHICH check failed is unknown for these runs. Replay delay per run: not stated.
Client NVMC: APPROVING 1058, CORRECTING 25.
| window | client CORRECTING ids | count | client probe snaps > 5 cm (time, body jump, state) |
|---|---|---|---|
| before run 1 | 550 723 | 2 | - |
| run 1 13:16:46-13:17:06 | 1234 1240 1244 1322 1324 1326 1328 | 7 | 0 (truck not moving at 3 s: throttle 1 brake 1, 0.001 m/s) |
| between | 2540 | 1 | - |
| run 2 13:17:16-13:17:36 | 2894 3305 3746 | 3 | 13:17:17.477 0.73 m (10 ms after 2894), rev 1.0 m/s |
| between | 4160 4527 | 2 | 13:17:45.223 2.41 m off path (10 ms after 4527), stopped |
| run 3 13:17:57-13:18:17 | 5380 5587 5832 6075 6471 | 5 | 13:17:58.901 1.32 m (10 ms after 5380), rev 1.1 m/s |
| run 4 13:18:22-13:18:42 | 6855 7249 7638 7995 | 4 | 13:18:23.487 4.12 m (10 ms after 6855), rev 0.46 m/s |
| after run 4 | 8225 | 1 | 13:18:46.354 2.26 m off path (27 ms after 8225), stopped |
Checksum corrections 2+7+1+3+5+4+2+1 = 25 = CLIENT CORRECTING count; in runs 7+3+5+4 = 19. Probe snaps 5; robot STOP lines say
0+1+1+1 = 3 in runs (+2 outside) = 5.
MEASURED: every probe snap came 10-27 ms after a CLIENT CORRECTING line (5 / 5). 20 / 25 applied corrections gave no snap > 5 cm.
The snaps this time are large (0.73-4.12 m) and come right at robot START (car starts reversing) or right after STOP; the steady
mid-run corrections (~every 4-7 s) give no probe snap. Earlier DS runs had 0.21-0.35 m snaps mid-run. Unresolved why
(inference: the WB host in the background runs its sim differently from a DS; not checked).
WB host frame health in that window (WB 'MUD corr 10 s' lines, which should come every 10 s): 13:16:41 (worst dt 0.1, 492 frames),
13:16:58 (0.1, 590), 13:17:08 (0.019, 744), then NOTHING until 13:17:52 (0.1, 372) and 13:18:47 (0.1, 186), 13:18:57 (0.028, 418).
MEASURED: the WB host's game loop stalled / ran far below normal during runs 2-4 (gaps of 44 s and 55 s between 10 s lines,
worst dt hitting the 0.1 clamp). Inference: WB was unfocused/throttled while the user drove in the client window; the big
0.7-4 m snaps likely come from that, not from the DS-style stutter. A WB host needs to keep full rate for this test.

### Replay-delay test, WB host + client, full NVMC both sides, 13:46-13:48 (WB logs_2026-10-11_13-42-57, client logs_2026-10-11_13-44-24)
WB started by the user with -logLevel debug (real IdentityID), client -logLevel debug, client drove the robot, truck 133,
dirt ~1005-1029 / 2586-2605. User: runs 1-2 "S: Replay delay" 2 (default), runs 3-4 = 8.
Logging control: SERVER APPROVING 3080 lines. Whole-log server counts: checks ("Process Result: Engine") 3617, APPROVING 3080,
CORRECTING 17, NO MOVE TO CONSUME 90, discarding 27. Client: APPROVING 1287, CORRECTING 17.
Allowance lines print 1 = pass / 0 = fail (all 1 in the passing checks). Per window (script runs.py in scratch):
| window | delay | checks | engine fails | phys fails (dist / angle / linear) | SERVER CORRECTING ids | NO MOVE | client CORRECTING | probe snaps > 5 cm (body jump) |
|---|---|---|---|---|---|---|---|---|
| before run 1 | 2 | 97 | 0 | 0 | - | 5 | 0 | 0 |
| run 1 13:46:32-52 | 2 | 147 | 0 | 137 (137 / 137 / 0) | 347 737 1169 | 33 | 3 | 1 (1.37 m at start) |
| between | 2 | 31 | 0 | 31 (31 / 31 / 14) | 1543 | 12 | 1 | 0 |
| run 2 13:46:57-13:47:17 | 2 | 130 | 0 | 130 (130 / 130 / 1) | 1973 2393 2834 | 37 | 3 | 0 |
| between | ? | 523 | 0 | 49 (49 / 49 / 4) | 3085 | 3 | 1 | 0 |
| run 3 13:47:30-50 | 8 | 1027 | 0 | 111 (111 / 70 / 0) | 3800 3986 4081 4654 4690 | 0 | 5 | 5 (1.47, 3.18, 0.28, 0.22, 0.32 m) |
| between | 8 | 277 | 0 | 4 (4 / 0 / 0) | 5238 | 0 | 1 | 1 (0.35 m, stopping) |
| run 4 13:47:57-13:48:18 | 8 | 922 | 0 | 75 (75 / 67 / 0) | 5455 5758 6559 | 0 | 3 | 2 (1.09 at start, 0.30 m) |
| after run 4 (to 13:48:25) | 8 | 333 | 0 | 0 | - | 0 | 0 | 0 |
Checksums (frozen copy of both logs, cut at 13:48:25; the WB log was still growing while first counted): checks
97+147+31+130+523+1027+277+922+333 = 3487 = check lines < 13:48:25; APPROVING 2950 + failed 537 = 3487; phys fails
137+31+130+49+111+4+75 = 537 (engine fails 0); 537 failed -> 17 CORRECTING sent, 520 inside the cooldown;
CORRECTING 3+1+3+1+5+1+3 = 17; NO MOVE 5+33+12+37+3 = 90; client CORRECTING 17 = log; probe snaps 1+5+1+2 = 9 (robot STOP lines: 1+0+5+2 = 8 in runs + 1 between = 9).
Timing: every probe snap came 12-30 ms after a CLIENT CORRECTING line (9 / 9); 8 of 17 corrections gave no snap > 5 cm
(5 of them in the delay-2 window).
WB host health ('MUD corr 10 s' lines should be every 10 s): 13:46:25 (dt max 0.1, 383 fr), then NONE until 13:47:11 (0.1, 325),
13:47:33 (0.1), 13:47:43 (0.1), 13:47:53 (0.049), 13:48:11 (0.1), 13:48:22 (0.1). The host stalled over the whole delay-2 window
(46 s gap = runs 1-2) and ran ~30 fps with 0.1 s frames after. So delay and host stalls are CONFOUNDED.

MEASURED:
- With replay delay 8 there were 0 NO MOVE lines in runs 3-4 and still 8 corrections / 7 snaps in 40 s (0.18 snaps/s, the vanilla
  dirt rate). Queue starvation is NOT needed for the snaps -> the starvation theory as the cause of the stutter is KILLED.
- All 17 corrections in this session were PHYSICS fails (distance + angle; engine check 0 fails). The 12:56 session had 28 / 28
  ENGINE fails and 0 phys fails. Both kinds happen; which one dominates on a real DS is not known.
- Delay-2 window: the server checked only ~7 moves/s (vs ~50/s at delay 8) and nearly every check failed distance; corrections
  are capped by the cooldown (one per ~7 s). Inference: the stalled host + empty queue let server and client drift apart.
NOT shown: the delay-2 vs delay-8 difference on a healthy host. Snaps 0.22-0.35 m mid-run (the DS size) appeared only in runs
3-4; the 1-3 m snaps come at robot start/stop.

### Allowance-test run 13:56-13:57 (MUD_AllowanceTest.ON, fixed 1 m / 3.2 rad / 100 / 100, replay delay 8) - SERVER LIMITS NOT APPLIED
WB logs_2026-10-11_13-42-57 (session from 13:55:48), client logs_2026-10-11_13-55-42. Only 2 robot runs (client log ends 13:57:38,
user said 4). Truck 102 (client id 0x...66), dirt ~1004-1027 / 2592-2606.
| window | checks | engine fails | phys fails (dist / angle / linear) | CORRECTING ids | NO MOVE | client CORR | probe snaps > 5 cm |
|---|---|---|---|---|---|---|---|
| before run 1 (from 13:55:48) | 165 | 0 | 0 | - | 9 | 0 | 0 (1 proxy 'mine 0') |
| run 1 13:56:45-13:57:05 | 132 | 0 | 125 (125 / 125 / 0) | 487 933 1218 | 0 | 3 | 1 (1.50 m at start) |
| between | 36 | 0 | 31 (31 / 31 / 10) | 1746 | 0 | 1 | 0 |
| run 2 13:57:10-13:57:30 | 1069 | 0 | 105 (105 / 69 / 0) | 1978 2110 2702 2768 2925 2981 | 0 | 6 | 6 (2.02, 1.21, 0.38, 0.29, 0.31, 0.31 m) |
| after run 2 to 13:57:40 | 292 | 0 | 65 (65 / 0 / 0) | 3246 3355 | 53 (13:57:37, client leaving) | 2 | 2 (0.21, 0.47 m stopping) |
Checksums: checks 165+132+36+1069+292 = 1694 (= check lines 13:55:48-13:57:40); APPROVING 1368 + failed 326 = 1694; phys fails
125+31+105+65 = 326; CORRECTING 3+1+6+2 = 12 = client CORRECTING 12; snaps 1+6+2 = 9 'mine 1' (+1 proxy = 10 total lines).
Every 'mine 1' probe snap came 13-25 ms after a CLIENT CORRECTING (9 / 9).
MEASURED: the server failed the ANGLE allowance 225 times. With SetAllowance angle 3.2 rad (> pi) that test cannot fail
(acos(dot) <= pi), so the server's truck still had the default limits. Server log has ONE 'MUD allowance' line in the session
(13:56:13, entity 0x...A1); the client set it on truck 102. Inference: modded Vehicle.EOnInit runs before InPlayMode() is true
for world-placed vehicles on the WB host, so only a later-spawned vehicle got it. The 11:23 DS run also logged a single
'MUD allowance' line ("which vehicle was not checked") -> the 11:23 conclusion "1 m limit still snaps" is NOT valid evidence.
WB host health: 10 s lines 13:56:40 -> 13:57:19 (39 s gap, stall during run 1), worst dt 0.1 most windows.
Fix 13:5x: apply SetAllowance from a CallLater after EOnInit (play mode checked when it fires) and log every vehicle.

### Allowance-test rerun 14:11-14:13 (CallLater apply, limits reached the server truck) - WB host too stalled to judge
WB logs_2026-10-11_13-42-57 (session from 14:10:23, Starting RPL server), client logs_2026-10-11_14-10-16 (8925 "(D)" lines).
Server: 1 'EOnInit' + 1 'set divisor 1' line (14:10:31 / 14:10:33, entity 159, play mode 1); client: same for truck 129.
Only ONE vehicle fires the modded Vehicle.EOnInit per machine (the spawned truck; world-placed vehicles never log it) -> the
13:56 'InPlayMode false' inference was wrong; the earlier single line WAS probably the driven truck. Unresolved why 13:56 still
had angle fails then (inference: the 13:56 server line came 30 s before... not checked further).
Replay delay: not stated for this run.
| window | checks | engine fails | phys fails (dist / angle) | CORRECTING ids | NO MOVE | client CORR | probe snaps 'mine 1' |
|---|---|---|---|---|---|---|---|
| before run 1 (from 14:10:23) | 400 | 0 | 9 (9 / 0) | 174 | 9 | 1 | 0 |
| run 1 14:11:19-39 | 455 | 17 | 41 (41 / 0) | 804 806 957 960 1082 1084 1086 1088 1090 1126 1130 1134 1137 1471 | 9 | (see note) | (see note) |
| between | 78 | 0 | 50 (50 / 0) | 1915 2014 2116 2224 | 0 | 4 | 1 (1.06 m) |
| run 2 14:11:48-14:12:08 | 150 | 0 | 132 (132 / 0) | 2434 2500 2538 3021 | 0 | 4 | 1+ (1.35 m at start) |
| between | 42 | 0 | 42 (42 / 0) | 3513 | 0 | 1 | 0 |
| run 3 14:12:15-35 | 138 | 0 | 138 (138 / 0) | 4050 4587 5130 | 0 | 3 | 0 |
| between | 30 | 0 | 30 (30 / 0) | - | 0 | 0 | 0 |
| run 4 14:12:40-14:13:00 | 126 | 0 | 126 (126 / 0) | 5538 6147 | 0 | 2 | 1 (4.90 m at start) |
| after run 4 to 14:13:10 | 345 | 0 | 79 (79 / 0) | 6719 6968 | 0 | 2 | 1 (1.41 m) |
Checksums: checks 400+455+78+150+42+138+30+126+345 = 1764; APPROVING 1100 + failed 664 (engine 17 + phys 647) = 1764;
CORRECTING 1+14+4+4+1+3+0+2+2 = 31; client CORRECTING 26 total (run-1 window cut off in the printout; 31 - 26 = 5 server
corrections the client skipped/merged, inference); probe 'mine 1' snaps 9 total; robot STOP lines 1+3+0+1 = 5 in runs.
MEASURED: with the server truck at a FIXED 1 m distance limit and angle/speed limits off, the distance check still failed
647 times (all of run 3: 138 / 138) = server and client > 1 m apart most of the time.
WB host health: 10 s lines 14:10:33, 14:10:43, 14:11:09 (gap 25 s), 14:11:29, 14:12:22 (gap 54 s), 14:13:10 (gap 48 s), worst dt
0.1 in every window, 162-576 frames per window. The client ran fine (worst dt 0.02-0.04, 455-988 frames / 10 s).
CONCLUSION: while the user drives in the client window the WB host's game loop stalls for tens of seconds, so its car falls metres
behind the client. A WB host cannot answer the allowance question (or any snap-rate question). The DS does not have this
problem (but has no NVMC log). Engine fails appeared again (17, run 1).

### Allowance test, WB window in FRONT during runs, 14:20-14:22 (WB logs_2026-10-11_14-19-10, client logs_2026-10-11_14-19-38)
Limits applied: server 'set divisor 1' 14:20:22 (entity 167), client 14:20:29 (truck 103). Client drove the robot; user brought
the WB window to the front during each run. Replay delay not stated (8 per plan). Run 4 Space-stopped at 4.6 s.
WB host health: 10 s lines every 10-20 s (max gap 20 s, was 25-54 s with the client in front), 358-638 frames / window, worst
dt still 0.1 in 8 of 13 windows. Checks per 20 s run ~1150 (~58 / s, vs 126-150 per run in the stalled 14:11 session).
| window | checks | engine fails | phys fails (all distance > 1 m) | CORRECTING | NO MOVE | client CORR | probe snaps 'mine 1' |
|---|---|---|---|---|---|---|---|
| before run 1 (from 14:19:41) | 638 | 0 | 0 | 0 | 4 | 0 | 0 |
| run 1 14:20:45-14:21:05 | 1156 | 16 | 0 | 14 (1066-1107) | 2 | 2 (1077, 1095) | 0 |
| between | 289 | 0 | 0 | 0 | 4 | 0 | 0 |
| run 2 14:21:13-33 | 1150 | 0 | 19 | 2 | 6 | 2 | 2 (1.20, 0.99 m) + 1 at 0.13 m before the first CORR line |
| between | 126 | 0 | 0 | 0 | 3 | 0 | 0 |
| run 3 14:21:39-59 | 1154 | 0 | 11 | 1 | 4 | 1 | 1 (1.46 m) |
| between | 87 | 0 | 0 | 0 | 0 | 0 | 0 |
| run 4 14:22:04-09 (4.6 s) | 135 | 0 | 13 | 2 | 20 | 2 | 3 (0.18, 1.14, 1.00 m) |
| between | 15 | 0 | 0 | 0 | 2 | 0 | 0 |
| run 5 14:22:12-32 | 1119 | 0 | 29 | 4 | 7 | 4 | 4 (2.08, 1.09, 1.03, 1.05 m) |
| after run 5 to 14:22:40 | 109 | 0 | 0 | 0 | 1 | 0 | 0 |
Checksums: checks 638+1156+289+1150+126+1154+87+135+15+1119+109 = 5978; APPROVING 5890 + failed 88 (engine 16 + phys 72) = 5978;
phys 19+11+13+29 = 72; CORRECTING 14+2+1+2+4 = 23; client CORRECTING 2+2+1+2+4 = 11 (12 of the 14 engine corrections in run 1
were not applied by the client - inference: superseded inside the same burst); snaps 0+3+1+3+4 = 11 lines incl. 0.13 / 0.18 m.
Failed checks come in 9 short bursts (0.1-0.3 s each): 14:20:48 (engine, run 1 +3 s), 14:21:14 (run 2 +1 s), 14:21:29 (run 2
+16 s), 14:21:40 (run 3 +1 s), 14:22:05-06 (run 4 +1-2 s), 14:22:13-15 (run 5 +1-3 s), 14:22:20 (run 5 +8 s).
MEASURED (WB host healthier, limits really applied: 1 m fixed, angle/speed off):
- No angle fails; the remaining physics fails are all distance > 1 m, and they come as sudden bursts, not slow drift.
- 7 of 9 bursts are 1-3 s after a robot START (the truck starting to reverse from a stop); 2 are mid-run.
- The 0.2-0.35 m mid-run snaps of the vanilla-limit runs are gone; what is left is ~1 snap per 20 s run but each 1-2 m.
- Engine-state fails still happen (16, run 1 start) and the client applied 2 of them without a visible snap.
NOT shown: why server and client are suddenly > 1 m apart at the start of reversing (inference: they disagree on WHEN the
gearbox/engine engages reverse - fits the engine-state fails at starts; or the remaining WB frame spikes, dt 0.1). Needs a
healthy server (DS) comparison: same robot, allowance ON vs OFF, snaps per run.

### DS + client, AllowanceTest ON half, 14:32-14:34 (server logs_2026-10-11_14-31-27, client PeerPlugin1 logs_2026-10-11_14-31-29)
Setup: ArmaReforgerServerDiag.exe (-logLevel debug, -maxFPS 30), client -logLevel debug with "Debug path correction" on.
The DS exe cannot log server NVMC checks (strings absent, see mud-gotchas) -> SERVER CORRECTING / engine / phys fails = n/a.
Limits applied: server 'set divisor 1 angle 3.2 lin 100 ang 100' 14:32:40.214 (entity 65, the only EOnInit line), client
14:32:40.276 (truck 110 = 0x...6E, the robot's truck). User did 3 runs (asked for 2). Replay delay = DS default (no diag menu).
Client health: worst dt 0.011-0.046 per 10 s window, 839-1364 frames / 10 s. Server health: no probe on the DS (not logged).
| window | client CORRECTING ids | probe snaps 'mine 1' (body jump, time into run) | robot STOP corrections | speed at 3 s |
|---|---|---|---|---|
| before run 1 | 33 | 0 | - | - |
| run 1 14:33:01.5-21.5 | - | 0 | 0 | 0.0018 m/s (throttle 1 brake 1: late start, moved 18 m by STOP) |
| run 2 14:33:27.6-47.6 | 3280 3882 | 2 (1.029 m at +9.8 s rev 2.16 m/s; 1.029 m at +19.9 s rev 1.71 m/s) | 2 | 2.07 m/s |
| run 3 14:33:52.7-14:34:12.7 | 4799 | 1 (1.030 m at +10.1 s rev 1.85 m/s) | 1 | 1.85 m/s |
| server engine / phys fails, SERVER CORRECTING | n/a | n/a | n/a | n/a |
Checksums: client CORRECTING 1+0+2+1 = 4 = grep count; snaps 0+2+1 = 3 = robot STOP sum 0+2+1 = 3 = 'MUD corr: ' lines 3.
Each snap came 9-10 ms after a CLIENT CORRECTING (3 / 3). ON rate: 3 snaps / 60 s = 0.05 /s (2 valid moving runs: 3 / 40 s = 0.075 /s).
MEASURED: all 3 snaps are 1.029-1.030 m = just over the fixed 1 m limit, mid-run or at stop, none at reverse start.
Inference (not tested): on the DS, server and client drift apart slowly and steadily until the gap crosses the limit; a bigger
limit trades many small snaps for fewer snaps of the limit's size.

### DS + client, AllowanceTest OFF half, 14:37-14:38 (server logs_2026-10-11_14-36-21, client logs_2026-10-11_14-36-23)
Same setup, DS + client restarted after ON -> false. Control: 0 'MUD allowance' lines on server and client, robot START says
"allowance test false" (vanilla limits). Truck 106 (0x...6A). 3 runs. Client health: worst dt 0.012-0.074, 921-1294 frames / 10 s.
| window | client CORRECTING ids | probe snaps 'mine 1' | robot STOP corrections | speed at 3 s |
|---|---|---|---|---|
| before run 1 | 243 | 0 (1 'mine 0' proxy line 14:36:59, 0.60 m, standing, hb 1) | - | - |
| run 1 14:37:07.1-27.1 | - | 0 | 0 | 0.0024 m/s (late start again, moved ~21 m) |
| run 2 14:37:33.1-53.1 | - | 0 | 0 | 1.92 m/s |
| run 3 14:37:57.8-14:38:17.8 | - | 0 | 0 | 1.91 m/s |
| server engine / phys fails, SERVER CORRECTING | n/a | n/a | n/a | n/a |
Checksums: client CORRECTING 1+0+0+0 = 1 = grep count; snaps 0+0+0 = 0 = robot STOP sum 0; 'MUD corr: ' lines 1 (the proxy one).
Client NVMC line types (both halves): only REMOVE UNTIL / CLIENT APPROVING / CLIENT CORRECTING, no LERP lines.

### ON vs OFF result (DS, 14:32-14:38)
| half | runs | s | client CORR in runs | snaps | snap size |
|---|---|---|---|---|---|
| ON (fixed 1 m, angle/speed off) | 3 | 60 | 3 | 3 | 1.029-1.030 m |
| OFF (vanilla) | 3 | 60 | 0 | 0 | - |
Checksum snaps 3 + 0 = 3 = 'mine 1' lines across both client logs.
MEASURED: SetAllowance ON did NOT cut snaps on the DS; in this session OFF had none and ON had 3 limit-sized ones. No snap came
at reverse start on the DS (the 1-3 m reverse-start snaps were only seen on the WB host -> inference: a WB-host artifact).
UNRESOLVED: why OFF gave 0 / 60 s here when earlier DS dirt runs gave 0.125-0.30 /s on the same area (12:22: 10 / 80 s). At
0.2 /s, 0 in 60 s is very unlikely by chance. Changes since then that I can name (not tested): client now runs -logLevel debug
with saved diag items ("Record Moves", "Draw recorded paths" 7, "S: Replay delay" 6, "Debug path correction"...); the earlier
client logs are rotated away, so their settings can't be re-checked. Truck ran on 'road' (not our patches) both halves.

### DS + client, OFF, client diag save REMOVED, 14:44-14:46 (server logs_2026-10-11_14-43-33, client logs_2026-10-11_14-43-35)
PeerPlugin1/profile/diagmenu.save moved to diagmenu.save.bak_1440 before the client start (its items: Record Moves 1, Draw
recorded paths 7, SimState to CSV 1, WS of (U) to CSV 1, InitState to CSV 1, S: Input/Output Diags 4, Debug path correction 1,
Debug history 1, Debug engine flags 1, Draw Phys Comp Debug 1, S: Replay delay 6, Entity diag 1, Height modif 50, AI diag 1).
Control: 0 NVMC lines in the client log (diag off), 0 'MUD allowance' lines, START "allowance test false". Truck 166 (0x...A6).
Client health: worst dt 0.012-0.050, 881-1143 frames / 10 s. Client CORRECTING: n/a (diag off).
| run | window | probe snaps 'mine 1' (body jump, time into run) | robot STOP corrections | speed at 3 s |
|---|---|---|---|---|
| before run 1 | - | 0 (1 'mine 0' proxy 0.06 m, standing) | - | - |
| 1 | 14:44:34.3-54.2 | 2 (0.297 m +9.4 s, 0.287 m +10.1 s) | 2 | 0.0031 m/s (late start) |
| 2 | 14:44:58.6-14:45:18.6 | 2 (0.252 m +8.6 s, 0.291 m +9.2 s) | 2 | 1.82 m/s |
| 3 | 14:45:23.2-43.2 | 4 (0.283 m +2.4 s, 0.278 m +3.9 s, 0.311 m +10.0 s, 0.276 m +12.1 s) | 4 | 1.91 m/s |
Checksum snaps 2+2+4 = 8 = robot STOP sum 2+2+4 = 8 = 'mine 1' lines 8. Rate 8 / 60 s = 0.13 /s, size 0.25-0.31 m
(= the earlier vanilla DS snaps).
MEASURED: same DS, same truck spot, same AllowanceTest OFF: with the client diag save 0 snaps / 60 s, without it 8 / 60 s.
So one (or more) client diag item removes the snaps. Which one: not tested. Candidate by name (inference): "S: Replay delay" 6,
or "Record Moves". The ON-half result (3 x 1.03 m) was also measured WITH the diag save -> ON vs OFF must be redone without it.

### DS + client, OFF, client diag = only "S: Replay delay" 6, 14:49-14:51 (server logs_2026-10-11_14-48-29, client logs_2026-10-11_14-48-30)
Client diagmenu.save held only '"S: Replay delay" 6' at client start (the user restarted anyway; also set it live per plan).
The value in effect is NOT logged (0 NVMC lines, no diag log) -> assumed 6, unverified. Truck 193 (0x...C1).
Client health: worst dt 0.011-0.063, 907-1188 frames / 10 s.
| run | window | probe snaps 'mine 1' | robot STOP corrections | speed at 3 s |
|---|---|---|---|---|
| 1 | 14:49:31.3-34.1 (Space-stopped 2.85 s, did not move) | 0 | 0 | - |
| 2 | 14:49:40.4-14:50:00.4 | 3 (0.250, 0.275, 0.236 m) | 3 | 0.0027 m/s (late start) |
| 3 | 14:50:06.0-26.0 | 6 (0.290, 0.297, 0.230, 0.240, 0.282, 0.284 m) | 6 | 2.04 m/s |
| after run 3 | 14:50:26.1 | 1 (0.283 m, throttle 0 = stopping) | - | - |
| 4 | 14:50:31.0-51.0 | 2 (0.216, 0.265 m) | 2 | 1.82 m/s |
Checksum 'mine 1' lines 3+6+1+2 = 12 = all 'mine 1' lines in the log; robot STOP sum 0+3+6+2 = 11 (+1 after run 3).
In-run rate 11 / 60 s = 0.18 /s (runs 2-4).
MEASURED: replay delay 6 alone does NOT remove the snaps (if the value was active). The suppressing item is another one
from diagmenu.save.bak_1440. Next candidate: "Record Moves" 1.

### Client log 14:48-14:55 runs 5-8 (same client session as the replay-delay runs, settings changed live by the user, NOT recorded)
Client diagmenu.save written 14:49:36 by the client: "S: Replay delay" 4, Entity diag, Height modif 50, AI diag -> the
"replay delay 6" runs above may have run at 4 (unverified). NVMC lines (= "Debug path correction" on) 14:53:22-14:54:41.
| run | start | length | snaps 'mine 1' | robot STOP corrections |
|---|---|---|---|---|
| 5 | 14:52:26 | 20.0 s | 4 (0.28-0.32 m) | 4 |
| 6 | 14:52:56 | 14.6 s (Space) | 5 (0.23-0.37 m) | 5 |
| 7 | 14:53:28 | 16.7 s (Space) | 2 (0.29, 0.26 m) | 2 |
| 8 | 14:54:07 | 20.0 s | 0 (+1 2.72 m at 14:54:29, 2 s after STOP) | 0 |
Checksum 4+5+2+0 = 11 = robot STOP sum. Settings per run unknown -> not used below.

### Control rerun with the ORIGINAL client diag save, 14:57-14:59 (server logs_2026-10-11_14-57-01, client logs_2026-10-11_14-57-02)
diagmenu.save.bak_1440 copied back before the client start (file time 14:56:17, unchanged after). OFF (0 'MUD allowance').
Truck 109 (0x...6D). Client health: worst dt 0.011-0.041, 901-1152 frames / 10 s.
| run | window | client CORRECTING | snaps 'mine 1' | robot STOP corrections | speed at 3 s |
|---|---|---|---|---|---|
| before run 1 | - | 1 (83) | 0 | - | - |
| 1 | 14:57:57-14:58:17 | 0 | 0 | 0 | 0.0012 m/s (late start) |
| 2 | 14:58:21.7-41.7 | 4 (2248 2399 2805 2918) | 4 (0.25-0.31 m) | 4 | 2.00 m/s |
| 3 | 14:58:57.5-14:59:17.5 | 7 (4132-4795) | 7 (0.24-0.31 m) | 7 | 2.15 m/s |
Checksums: client CORRECTING 1+0+4+7 = 12 = grep count; snaps 0+4+7 = 11 = robot STOP sum = 'MUD corr: ' lines 11.
Every snap 8-14 ms after a CLIENT CORRECTING (11 / 11).
MEASURED: with the SAME diag save that gave 0 / 60 s at 14:37, the snaps are back (11 / 60 s). The client diag items do NOT
remove the snaps; the 14:37 OFF session (0 snaps) is an unexplained outlier. Earlier "one diag item removes the snaps" is WRONG.

### Pooled DS dirt results 2026-10-11 14:33-14:59 (sessions with known settings)
| setting | session | runs | s | snaps | snap size |
|---|---|---|---|---|---|
| ON (1 m fixed) | 14:33 | 3 | 60 | 3 | 1.03 m |
| OFF | 14:37 | 3 | 60 | 0 | - |
| OFF, no client diag | 14:44 | 3 | 60 | 8 | 0.25-0.31 m |
| OFF, replay delay 4-6 | 14:49 | 3 (runs 2-4) | 60 | 11 | 0.22-0.30 m |
| OFF, original diag | 14:57 | 3 | 60 | 11 | 0.24-0.31 m |
OFF total 0+8+11+11 = 30 in 240 s = 0.125 /s; ON 3 in 60 s = 0.05 /s. Run-to-run spread is large (one OFF session gave 0).
MEASURED: ON gives fewer snaps in its one session but each is ~4x bigger (1.03 m vs 0.22-0.31 m). Not a clear win; a single
ON session can be an outlier like 14:37 OFF was.

### Same client session, user set client "S: Replay delay" 6 LIVE after run 3 (~14:59:20-15:00:29), runs 4-6
User: "changed it to 6, no stutter". diagmenu.save unchanged (14:56:17, already holds 6 -> so the value loaded at client start
was apparently NOT in effect for runs 1-3, or the menu showed something else; unresolved, ask what the menu showed before).
| run | window | client CORRECTING in run | snaps 'mine 1' | robot STOP corrections | speed at 3 s |
|---|---|---|---|---|---|
| 4 | 15:00:29.0-48.5 (Space 19.5 s) | 0 | 0 | 0 | 1.88 m/s |
| after run 4 | 15:00:50.8 | 1 (10800) | 0 | - | - |
| 5 | 15:01:04.4-24.4 | 0 | 0 | 0 | 1.65 m/s |
| 6 | 15:01:29.2-49.2 | 0 | 0 | 0 | 1.85 m/s |
Checksum: client CORRECTING after 14:59:18 = 1 = grep; snaps 0 = 'MUD corr: ' lines after 14:59:18 0. Client health: worst dt
0.012-0.059, 871-1089 frames / 10 s.
MEASURED (same client, server, truck, settings except the live change): runs 2-3 before = 11 snaps / 40 s, runs 4-6 after =
0 snaps / 59.5 s, and 0 corrections in runs. Fits the 14:37 0-snap session (same save with 6). Contradicts the earlier
inference that replay delay only acts on the server. Inference (not tested): a diag value restored from diagmenu.save is not
applied until it is changed in the menu.

### Replay-delay sweep, same client session, runs 7-16, 15:03-15:08 (client logs_2026-10-11_14-57-02)
User's sequence (live changes in the client menu, times NOT logged): start 8 = stutter (runs 1-3), 6 = none (runs 4-6),
8 = stutter, 2 = none, 6 = none, 8 = none. NOTE: the user says the client STARTED at 8, though diagmenu.save held 6 (file
re-saved 15:07:12, still 6). Unresolved.
| run | start | length | snaps 'mine 1' | robot STOP corr | area x / z | front-left wheel long slip > 0.2 share |
|---|---|---|---|---|---|---|
| 7 | 15:03:11.7 | 12.0 s (Space) | 0 | 0 | 1060-1069 / 2572-2589 | 0.66 |
| 8 | 15:03:42.0 | 20.0 s | 3 | 3 | 1074-1087 / 2595-2618 | 0.46 |
| 9 | 15:04:18.4 | 20.0 s | 3 | 3 | 1084-1097 / 2595-2617 | 0.61 |
| 10 | 15:04:56.9 | 20.0 s | 0 | 0 | 1101-1117 / 2610-2633 | 0.51 |
| 11 | 15:05:20.8 | 20.0 s | 0 | 0 | 1105-1124 / 2610-2631 | 0.47 |
| 12 | 15:05:53.1 | 20.0 s | 0 | 0 | 1100-1118 / 2614-2634 | 0.60 |
| 13 | 15:06:17.2 | 11.7 s (Space) | 0 | 0 | 1119-1123 / 2613-2630 | 0.62 |
| 14 | 15:06:38.7 | 20.0 s | 0 | 0 | 1100-1118 / 2610-2633 | 0.54 |
| 15 | 15:07:28.2 | 20.0 s | 0 | 0 | 1110-1123 / 2611-2633 | 0.50 |
| 16 | 15:07:52.3 | 20.0 s | 0 | 0 | 1100-1113 / 2611-2634 | 0.49 |
Checksum snaps 3+3 = 6 = 'mine 1' lines after 15:01:50 (6) = robot STOP sum 6. Client CORRECTING after 15:01:50: 9 (15:03:35,
15:03:54, 15:03:55, 15:04:01, 15:04:21, 15:04:28, 15:04:31, 15:04:45, 15:04:46); 6 gave a snap 8 ms later.
Slip share for runs 1-6 (original dirt 1029-1069 / 2569-2598): 0.48-0.61 -> the new area slips about as much (surface not logged).
MEASURED: snaps only in runs 1-3 and 8-9 (both "8" periods before the first switch to 2); none in 10 runs after the switch
to 6 (runs 4-6) or after the switch to 2 (runs 10-16), including the final "8" period.
NOT shown: that the replay-delay VALUE decides it (the final 8 had no snaps). Which run had which value: not logged.
Runs 10-16 were ~50 m away from the original spot (different ground, slip similar). Inference options: (a) changing the
setting does something lasting (e.g. resets client move history), (b) session-to-session spread (14:37 also had 0).

### Big replay-delay sweep 15:12-15:20 (server logs_2026-10-11_15-11-36, client logs_2026-10-11_15-11-38) - snaps at ALL values
New DS + client session, OFF, truck 172 (0x...AC). User changed "S: Replay delay" live across many values ("stutter on all
values"); which run had which value: not logged / not stated. Client diagmenu.save re-saved 15:18:27 without a replay delay line.
| run | start | s | snaps 'mine 1' | client CORR | robot STOP corr | snap sizes m |
|---|---|---|---|---|---|---|
| 1 | 15:12:39.0 | 20.0 | 2 | 2 | 2 | 0.23 0.28 |
| 2 | 15:13:03.1 | 20.0 | 2 | 2 | 2 | 0.30 0.29 |
| 3 | 15:13:27.2 | 20.0 | 1 | 1 | 1 | 0.30 |
| 4 | 15:13:51.3 | 20.0 | 3 | 3 | 3 | 0.33 0.31 0.27 |
| 5 | 15:14:33.8 | 20.0 | 4 | 4 | 4 | 0.28 0.27 0.27 0.31 |
| 6 | 15:14:57.9 | 20.0 | 4 | 4 | 4 | 0.28 0.31 0.29 0.33 |
| 7 | 15:15:27.5 | 1.2 | 0 | 0 | 0 | - |
| 8 | 15:15:36.5 | 20.0 | 6 | 6 | 6 | 0.29 0.22 0.22 0.28 0.32 0.27 |
| 9 | 15:16:00.4 | 20.0 | 4 | 4 | 4 | 0.27 0.24 0.29 0.31 |
| 10 | 15:16:36.5 | 20.0 | 2 | 2 | 2 | 0.29 0.31 |
| 11 | 15:17:00.5 | 20.0 | 3 | 3 | 3 | 0.22 0.27 0.27 |
| 12 | 15:17:34.1 | 20.0 | 3 | 3 | 3 | 0.27 0.33 0.30 |
| 13 | 15:17:58.6 | 20.0 | 4 | 4 | 4 | 0.30 0.25 0.35 0.28 |
| 14 | 15:18:36.8 | 20.0 | 6 | 6 | 6 | 0.29 0.28 0.30 0.32 0.31 0.29 |
| 15 | 15:19:02.8 | 20.0 | 4 | 5 | 4 | 0.26 0.30 0.31 0.26 |
| 16 | 15:19:44.7 | 20.0 | 5 | 5 | 5 | 0.28 0.29 0.26 0.29 0.33 |
Checksums: snaps 2+2+1+3+4+4+0+6+4+2+3+3+4+6+4+5 = 53 = robot STOP sum 53; 'mine 1' lines in the log 55 (2 outside runs);
client CORRECTING 58 in the log, 54 inside runs. 53 snaps / 301.3 s = 0.18 /s. Every run > 2 s snapped.
MEASURED: no replay-delay value stopped the snaps in this session -> the replay-delay theory is KILLED. The 0-snap stretches
(14:37 runs 1-3, 15:00 runs 4-6, 15:05-15:08 runs 10-16) are unexplained spread, not the setting. Cause of those quiet
stretches: unresolved (not location: runs 4-6 were on the original dirt).

### IDA read: what "S: Replay delay" writes (2026-10-11 ~15:25, Exp 192624 Diag exe, read not run)
- Diag item qword_14248E7D8, registered in sub_140120FC0 with "0, 300, 2, 1" (inference: min 0, max 300, default 2, step 1).
- Read by 4 virtual functions (one per class, called every tick):
  - sub_14083ED70, sub_14084B610, sub_140868000 (server move processing): if value != 0 -> component WORD +352 = value + 2.
  - sub_14085FEE0 (CLIENT side: its callees draw "Processing Correction", "InvokeCorrection", "Prediction in range",
    "Approval", "No move to consume"): if value != 0 -> component WORD +344 = value + 5.
- So replay delay acts on the client too (earlier "server-only" inference was WRONG), and it is a one-way override: a value of
  0 writes nothing, so the last nonzero value sticks. With the default 2 the diag exe always overrides both fields (server 4,
  client 7); the retail defaults of +352 / +344 were not read.
- Moot for the stutter: the 15:12-15:20 sweep snapped at every value. No script setter found earlier.

### Framelog comparison, quiet vs noisy runs (2026-10-11 ~15:30, script %TEMP%/fl/cmp.py, client CSVs only)
All client framelogs written 14:37-15:26 (DS, AllowanceTest OFF), runs >= 15 s; stats over frames after the first 3 s.
snaps = frames with probe_corr > 0. slip_long = share of frames with any wheel |long slip| > 0.2; rpm_jump = share of
frames where front-left |rpm| changes > 30 in one frame (stick-slip); pitch/roll_sd in degrees.
| CSV written | snaps | s | speed | speed_sd | throttle | dt_max | slip_long | rpm_jump | pitch_sd | roll_sd |
|---|---|---|---|---|---|---|---|---|---|---|
| 14:37:27 | 0 | 20.0 | 1.650 | 0.408 | 0.992 | 0.020 | 0.901 | 0.567 | 0.252 | 0.240 |
| 14:37:53 | 0 | 20.0 | 1.788 | 0.201 | 0.962 | 0.013 | 0.928 | 0.570 | 0.530 | 0.541 |
| 14:38:17 | 0 | 20.0 | 1.793 | 0.225 | 0.887 | 0.014 | 0.837 | 0.467 | 0.314 | 0.226 |
| 14:44:54 | 2 | 20.0 | 1.705 | 0.460 | 0.997 | 0.020 | 0.836 | 0.533 | 0.329 | 0.245 |
| 14:45:18 | 2 | 20.0 | 1.669 | 0.229 | 0.965 | 0.024 | 0.801 | 0.549 | 0.307 | 0.642 |
| 14:45:43 | 4 | 20.0 | 1.848 | 0.161 | 0.951 | 0.033 | 0.889 | 0.517 | 0.348 | 0.271 |
| 14:46:25 | 6 | 20.0 | 1.892 | 0.221 | 0.871 | 0.016 | 0.740 | 0.401 | 0.474 | 0.495 |
| 14:50:00 | 3 | 20.0 | 1.742 | 0.326 | 0.996 | 0.027 | 0.908 | 0.517 | 0.282 | 0.460 |
| 14:50:25 | 6 | 20.0 | 1.868 | 0.238 | 0.913 | 0.017 | 0.710 | 0.418 | 0.350 | 0.455 |
| 14:50:50 | 2 | 20.0 | 1.811 | 0.142 | 0.998 | 0.013 | 0.966 | 0.540 | 0.224 | 0.450 |
| 14:52:45 | 4 | 20.0 | 1.779 | 0.240 | 0.904 | 0.020 | 0.783 | 0.479 | 0.258 | 0.483 |
| 14:53:45 | 2 | 16.7 | 1.696 | 0.198 | 0.966 | 0.015 | 0.883 | 0.538 | 0.540 | 0.392 |
| 14:54:26 | 0 | 20.0 | 1.718 | 0.137 | 0.941 | 0.018 | 0.956 | 0.620 | 0.431 | 0.389 |
| 14:58:16 | 0 | 20.0 | 1.701 | 0.482 | 0.914 | 0.020 | 0.743 | 0.507 | 0.158 | 0.323 |
| 14:58:41 | 4 | 20.0 | 1.907 | 0.188 | 0.860 | 0.019 | 0.797 | 0.455 | 0.754 | 0.469 |
| 14:59:17 | 7 | 20.0 | 1.843 | 0.208 | 0.954 | 0.025 | 0.791 | 0.455 | 0.464 | 0.364 |
| 15:00:48 | 0 | 19.5 | 1.676 | 0.550 | 0.816 | 0.013 | 0.772 | 0.425 | 0.934 | 0.725 |
| 15:01:24 | 0 | 20.0 | 1.831 | 0.177 | 0.972 | 0.014 | 0.909 | 0.524 | 0.450 | 0.491 |
| 15:01:49 | 0 | 20.0 | 1.846 | 0.212 | 0.896 | 0.018 | 0.786 | 0.546 | 0.374 | 0.497 |
| 15:04:01 | 3 | 20.0 | 1.935 | 0.227 | 0.852 | 0.015 | 0.715 | 0.372 | 0.606 | 0.324 |
| 15:04:38 | 3 | 20.0 | 1.751 | 0.260 | 0.977 | 0.012 | 0.813 | 0.482 | 1.091 | 0.668 |
| 15:05:16 | 0 | 20.0 | 1.890 | 0.202 | 0.916 | 0.012 | 0.831 | 0.497 | 0.723 | 0.369 |
| 15:05:40 | 0 | 20.0 | 1.888 | 0.233 | 0.916 | 0.014 | 0.729 | 0.410 | 1.326 | 0.706 |
| 15:06:13 | 0 | 20.0 | 1.668 | 0.181 | 0.943 | 0.013 | 0.899 | 0.570 | 0.318 | 0.423 |
| 15:06:58 | 0 | 20.0 | 1.962 | 0.186 | 0.972 | 0.016 | 0.832 | 0.485 | 0.951 | 0.784 |
| 15:07:48 | 0 | 20.0 | 1.722 | 0.284 | 0.930 | 0.013 | 0.726 | 0.410 | 0.680 | 0.780 |
| 15:08:12 | 0 | 20.0 | 1.999 | 0.172 | 0.948 | 0.022 | 0.733 | 0.436 | 0.835 | 0.427 |
| 15:12:58 | 2 | 20.0 | 1.850 | 0.131 | 0.985 | 0.025 | 0.997 | 0.672 | 0.254 | 0.529 |
| 15:13:23 | 2 | 20.0 | 1.666 | 0.211 | 0.982 | 0.029 | 0.801 | 0.557 | 0.556 | 0.402 |
| 15:13:47 | 1 | 20.0 | 1.864 | 0.120 | 0.977 | 0.018 | 0.997 | 0.628 | 0.268 | 0.518 |
| 15:14:11 | 3 | 20.0 | 1.775 | 0.214 | 0.976 | 0.013 | 0.843 | 0.506 | 0.501 | 0.385 |
| 15:14:53 | 4 | 20.0 | 1.829 | 0.140 | 0.953 | 0.028 | 0.967 | 0.588 | 0.427 | 0.516 |
| 15:15:17 | 4 | 20.0 | 1.817 | 0.182 | 0.956 | 0.012 | 0.906 | 0.519 | 0.405 | 0.345 |
| 15:15:56 | 6 | 20.0 | 1.947 | 0.192 | 0.947 | 0.025 | 0.751 | 0.489 | 0.740 | 0.492 |
| 15:16:20 | 4 | 20.0 | 1.770 | 0.176 | 0.975 | 0.020 | 0.901 | 0.517 | 0.200 | 0.330 |
| 15:16:56 | 2 | 20.0 | 1.883 | 0.180 | 0.938 | 0.018 | 0.868 | 0.564 | 0.755 | 0.549 |
| 15:17:20 | 3 | 20.0 | 1.792 | 0.192 | 0.986 | 0.012 | 0.884 | 0.500 | 0.176 | 0.221 |
| 15:17:54 | 3 | 20.0 | 1.866 | 0.197 | 0.963 | 0.017 | 0.881 | 0.568 | 0.728 | 0.664 |
| 15:18:18 | 4 | 20.0 | 1.681 | 0.219 | 0.958 | 0.012 | 0.817 | 0.517 | 0.599 | 0.599 |
| 15:18:56 | 6 | 20.0 | 1.908 | 0.186 | 0.905 | 0.016 | 0.767 | 0.536 | 0.291 | 0.345 |
| 15:19:22 | 4 | 20.0 | 1.895 | 0.176 | 0.985 | 0.032 | 0.889 | 0.553 | 0.587 | 0.546 |
| 15:20:04 | 5 | 20.0 | 1.793 | 0.196 | 0.937 | 0.034 | 0.871 | 0.565 | 0.368 | 0.319 |
| 15:22:39 | 4 | 17.5 | 1.571 | 0.460 | 0.990 | 0.019 | 0.819 | 0.524 | 0.240 | 0.251 |
| 15:23:11 | 5 | 20.0 | 1.861 | 0.187 | 0.998 | 0.025 | 0.942 | 0.518 | 1.052 | 0.970 |
| 15:24:02 | 4 | 15.0 | 1.970 | 0.217 | 0.917 | 0.015 | 0.601 | 0.365 | 0.214 | 0.231 |
| 15:25:27 | 0 | 20.0 | 1.881 | 0.167 | 0.990 | 0.015 | 0.960 | 0.632 | 0.956 | 1.094 |

Checksum: quiet (0 snaps) 15 + noisy (>= 2) 30 + other (1 snap) 1 = 46 runs; snaps total 114.
| metric | quiet mean [min-max] | noisy mean [min-max] |
|---|---|---|
| speed | 1.801 [1.650-1.999] | 1.811 [1.571-1.970] |
| speed_sd | 0.254 [0.137-0.550] | 0.218 [0.131-0.460] |
| throttle | 0.933 [0.816-0.992] | 0.952 [0.852-0.998] |
| dt_max | 0.016 [0.012-0.022] | 0.020 [0.012-0.034] |
| slip_long | 0.836 [0.726-0.960] | 0.838 [0.601-0.997] |
| rpm_jump | 0.511 [0.410-0.632] | 0.510 [0.365-0.672] |
| pitch_sd | 0.616 [0.158-1.326] | 0.471 [0.176-1.091] |
| roll_sd | 0.534 [0.226-1.094] | 0.447 [0.221-0.970] |
(dt, dt_p99, steer, slip_lat 0 and wheel contact 1.0 were also equal in both groups.)
MEASURED: no client-side column separates quiet from noisy runs - every range overlaps (speed, throttle, frame time, wheel slip,
stick-slip, body pitch/roll). Worst frame dt is a bit higher in noisy runs (mean 0.020 vs 0.016 s) but the ranges overlap.
Server logs (DS, debug level) hold no timing or vehicle lines during runs; only GM editor opens (15:00:49, 15:03:33, 15:04:43,
15:17:23, 15:18:28) - they fall in both quiet and noisy stretches, so not the switch.
Quiet runs come in streaks (14:37 x3, 15:00-15:02 x3, 15:05-15:08 x6 full runs; singles 14:54, 14:58, 15:25), noisy runs too -> inference: some state that lasts
minutes, most likely on the server side (not logged). Next to measure: server frame time + server-side truck state via a TEMP
script line on the DS.

### "C: Disable Correction" toggled on/off by the user, then off: no stutter (client logs_2026-10-11_15-21-39, 15:22-15:30)
Toggle times not logged. Runs 1-9 (15:22:21-15:28:42); snaps ('mine 1') and CLIENT CORRECTING by time:
snaps 21 lines 15:22:31-15:27:53 (each 9-11 ms after a CLIENT CORRECTING; 23 CORRECTING lines, last 15:27:53.161).
After 15:27:53: runs 8 (15:27:58-15:28:18) and 9 (15:28:22-42) and on to log end 15:30:14 -> 0 snaps, 0 CORRECTING,
4115 CLIENT APPROVING (approvals keep flowing, ~1400 NVMC lines / 10 s as before).
IDA read (Exp 192624 Diag exe): "C: Disable Correction" = dword_14248EDB0, a plain bool read LIVE at every received approval /
correction (client handlers sub_140841920 / sub_140841A40 and 2 sibling classes): when true they return early. No latch: set
back to false, corrections are handled normally again. "NVMC CLIENT CORRECTING" is printed AFTER that early return, so with the
flag on a correction would be silent - the client log alone cannot tell "flag still on" from "server sent none".
Inference: the menu shows false and the flag is read live -> this is another quiet stretch like 14:37 / 15:00 / 15:05; the
toggling is NOT shown to cause it (quiet stretches also came without touching it).

### GM-move test 15:34-15:41 (server logs_2026-10-11_15-33-20, client logs_2026-10-11_15-33-21)
User: circled at one spot toggling Disable Correction and replay delay 2-8 -> always stutter; picked the truck up with the GM
editor and moved it -> less (at 6); moved again, set 8 -> no stutter, even back at the old stutter spots; Disable Correction
confirmed off. The server log only says "opened the editor" (no move lines), so move times = editor-open times (inference).
| run | start | start pos x / z | snaps 'mine 1' | robot STOP corr | editor open before it |
|---|---|---|---|---|---|
| 1 | 15:34:33.6 | 1042.8 / 2597.2 | 3 | 3 | spawn 15:34:07 |
| 2 | 15:35:02.9 | 1022.6 / 2585.2 | 5 | 5 | - |
| 3 | 15:35:27.9 | 1043.4 / 2595.0 | 3 | 3 | - |
| 4 | 15:35:57.9 | 1035.9 / 2579.8 | 0 | 0 | - |
| 5 | 15:36:21.7 | 1030.1 / 2602.7 | 0 | 0 | - |
| (between) | 15:36:44-49 | - | 2 (around the move) | - | 15:36:47 |
| 6 | 15:36:56.9 | 1043.5 / 2554.3 | 0 | 0 | 15:36:47 |
| 7 | 15:37:21.0 | 1041.2 / 2577.8 | 2 | 2 | - |
| 8 | 15:37:54.7 | 1072.5 / 2519.6 | 0 | 0 | 15:37:45 |
| 9 | 15:38:19.0 | 1059.5 / 2539.4 | 0 | 0 | - |
| 10 | 15:39:57.6 | 1072.9 / 2520.0 | 0 | 0 | - |
| 11 | 15:40:21.6 | 1063.1 / 2541.2 | 0 | 0 | - |
| (between) | 15:40:44.8 | - | 1 (around the move) | - | 15:40:42 |
| 12 | 15:40:50.3 | 1044.4 / 2574.3 | 0 | 0 | 15:40:42 |
| 13 | 15:41:14.3 | 1044.4 / 2598.1 | 0 | 0 | - |
Checksum: in-run snaps 3+5+3+0+0+0+2+0+0+0+0+0+0 = 13 = robot STOP sum 13; + 4 outside runs (15:34:56 after run 1, 15:36:44, 15:36:49, 15:40:44) = 17 =
'mine 1' lines (+1 'mine 0' proxy line at spawn 15:34:07 = 18 'MUD corr:' lines). Client CORRECTING 20 (last 15:40:46); 0 inside runs 8-13.
Editor opens vs quiet stretches over all sessions:
| session | editor open (move?) | what followed |
|---|---|---|
| 14:57 | 15:00:49 | quiet already from run 4 (15:00:29, before the open) |
| 14:57 | 15:03:33 | noisy (runs 8-9) |
| 14:57 | 15:04:43 | quiet runs 10-16 |
| 15:11 | 15:17:23, 15:18:28 | noisy (runs 12-16) |
| 15:21 | 15:27:14 | noisy (snaps to 15:27:53) |
| 15:21 | 15:27:51 | quiet from 15:27:53 to log end 15:30:14 |
| 15:33 | 15:36:47 | run 6 quiet, run 7 noisy |
| 15:33 | 15:37:45 | quiet runs 8-13, incl. the old stutter spots |
MEASURED: not the place (runs 12-13 quiet at the old spots). A GM move came right before 3 long quiet stretches, but 4 opens were
followed by snaps and 3 quiet stretches began with no open (14:37, 15:00:29, 15:35:58). So the GM move is NOT shown to cause it.
Inference: quiet/noisy is a state that holds for minutes and flips on its own. Candidate (untested): timing phase between the DS
frame (-maxFPS 30 = 33 ms frames, 60 Hz physics) and the client's move stream; a teleport or time could shift it.
Cheap test: DS without -maxFPS 30 (or 60), >= 6 runs.

### Other ways to sync or hide snaps: web + IDA + script API (2026-10-11 ~15:50, Exp 192624 Diag exe, read not run)
Question: keep correction ON (the engine has to keep the server in charge) but hide or avoid the snaps.

Web (2 searches):
- No BI fix or community thread for the DRIVER's own snaps. The BI API docs list SetAllowance and SetPrediction on
  NwkCarMovementComponent with no description of values.
- Workshop mod "Simply Networked" (AlliumFr, 0.0.2, 31.08.2026, https://reforger.armaplatform.com/workshop/6A3E481A621A3742-SimplyNetworked):
  script easing of replicated transforms. Its page talks about OTHER players' movement ("smoother spectator/third-person
  visuals"), not the local driver's prediction corrections. Not tested; APL-ND license (can't copy its code).

IDA, the engine's own visual lerp (sub_140840630, called per frame from a vtable at 0x141c603e8, and once from
sub_140840DF0 at the end of the client correction handler):
- Runs only while +489 is set (set by sub_140840DF0) and the entity has no parent (+96 -> +192 -> +8 null).
- Compares the stored visual matrix (+492..540) to the physics matrix. If the squared difference <= 10.0 AND the
  physics vfunc +808 returns true, it moves the stored matrix 35 % toward physics and writes it to the entity
  (sub_1409B60F0(entity, mat, 0)). Else it stops (+489 = 0); if vfunc +808 is false it writes the physics matrix.
- sub_140840DF0 calls the lerp at once only when vfunc +808 is FALSE.
- Measured earlier (section 6): the visual stays 1-2 cm from the body after a correction, so this lerp has no visible
  effect on our truck. Why (INFERENCE, not proven): the physics step syncs the entity from the body every step, so the
  35 % write is overwritten; or the stored matrix is taken after the rewind. Did not find the +492 writer.
- Its knobs (allowance 10.0, 35 %, 40 % angle) are only reachable through the Diag exe menu, not script or prefab.

Script API (enfusion-mcp api_search):
- CharacterCamera3rdPersonVehicle has OnAfterCameraUpdate(dt, keyframe, inout transformMS[4]) and OnUpdate: script
  can move the camera. So a script can ease the CAMERA over ~0.2 s after a body jump (MUD_JoltProbe already detects
  jumps). Limit (inference): the truck and cockpit still jump on screen; only the world stops jumping. 1st person: the
  camera is tied to the seated character, so either the world or the cockpit jumps.
- No script access to the physics rewind or replay: writing our own vehicle sync is not possible.

Ranked options (none measured yet):
| # | option | what it fixes | cost | status |
|---|---|---|---|---|
| 1 | DS -maxFPS 60 (server runs at the physics rate) | may cut snaps at source | 0 code | next test, >= 6 runs |
| 2 | camera ease after a detected body jump (modded vehicle camera) | hides world jump, 3rd + 1st person | small script | idea, untested |
| 3 | more even dirt grip (asphalt grip on dirt: 0 snaps / 59.5 s, 3 runs, 10:49) | cuts snaps at source on our dirt | surface tuning, changes feel | only 3 runs, and 0-snap quiet stretches were later seen by themselves: needs a >= 6-run recheck |
| 4 | SetAllowance 0.5 m | fewer but bigger snaps | 1 call | 1 m tested: not a clear win |
| 5 | C: Disable Correction for real players | 0 snaps | n/a | NOT shippable: Diag-exe only, and the client would drift from the server |

### DS session 15:46-15:52 (server logs_2026-10-11_15-46-49, client logs_2026-10-11_15-46-49)
Server CLI still has -maxFPS 30 (CLI Params line 15:46:49.463), so this is NOT the 60 fps test. Client diagmenu.save
(15:42) has "S: Replay delay" 4. AllowanceTest false. Vanilla GM_Arland.
| run | start | start pos x/z | snaps (mine 1) in run | robot 'corrections' |
|---|---|---|---|---|
| 1 | 15:47:39.9 | 1053.6 / 2576.3 | 1 | 1 |
| 2 | 15:48:04.1 | 1031.2 / 2569.7 | 2 | 2 |
| 3 | 15:48:56.7 | 1085.4 / 2520.5 | 0 | 0 |
| 4 | 15:49:24.7 | 1062.5 / 2521.5 | 0 | 0 |
| 5 | 15:49:48.7 | 1085.8 / 2517.8 | 0 | 0 |
| 6 | 15:50:35.6 | 1088.2 / 2488.7 | 4 | 4 |
| 7 | 15:51:15.4 | 1076.9 / 2523.8 | 0 | 0 |
| 8 | 15:51:39.4 | 1057.8 / 2535.4 | 0 | 0 |
Checksum: 1+2+0+0+0+4+0+0 = 7 in runs + 3 outside runs (15:50:26.8, 15:51:02.9, 15:51:04.9) = 10 = grep -c 'mine 1'.
'mine 0' 0. CLIENT CORRECTING 12 lines (4970 and 10584 have no snap line). SERVER CORRECTING 0.
Rate in runs: 7 / 160 s = 0.044/s (big sweep 15:12-15:20: 53 / 301.3 s = 0.18/s). Snap size 0.25-0.32 m in reverse, as before.
5 of 8 runs had 0 snaps: another partly quiet session at the same 30 fps.

### Server timing log + PLACE matters (2026-10-11 16:00-16:05, server logs_2026-10-11_15-59-11, client logs_2026-10-11_15-59-13)
New TEMP MUD_ServerLog.c (server, per second while a pilot sits in): every second while driving 30-31 frames,
60-62 EOnSimulate steps (so EOnSimulate = one call per 60 Hz physics step, 0 when the body sleeps), frame gap
31-36 ms (3 one-off windows 18-48 ms), 0 frames without a step while moving. SAME in the noisy runs 2-3
(4 + 2 snaps) and the quiet runs 4-5 (0 + 0). PROVEN: the server's frame/step timing does not explain quiet vs noisy.

User GM-moved the truck between runs 3 and 4 - to the area where every earlier quiet stretch also happened.
All 81 robot runs that ran the full 20 s today, 14:33-16:01 (client logs 14-31-29 .. 15-59-13, 'MUD robot STOP ... (done)'
lines; start = START line position), split by start position:
| start area | runs | snaps | runs with 0 |
|---|---|---|---|
| x 1040-1095, z < 2560 ("south") | 20 | 11 | 16 |
| everywhere else (mostly x 1020-1090, z 2565-2635, "north") | 61 | 133 | 22 |
Checksum: 20 + 61 = 81 runs; 11 + 133 = 144 = sum of all 'corrections' in those STOP lines.
South: 0.55 snaps/run, 15 of the 20 runs start z 2515-2545 and have 1 snap between them; the snaps there come from
starts at 2551 (5), 2489 (4), 2503 (1), 2526 (1). North: 2.2/run, its 0-runs come in time clusters (14:37 x3, 15:01 x2,
15:05-15:08 at x 1104-1119 x6, 15:25-15:26, 15:36-15:41).
CORRECTION to earlier text: "quiet stretches are not tied to location" is WRONG. The truck's location is the
biggest factor found so far; most "quiet after a GM move" cases were moves into the south area.
Framelog per-area (139 client CSVs, /tmp/loc.py, rows with speed > 1): south is BUMPIER, not smoother (height range
0.43 vs 0.27 m, pitch sd 0.81 vs 0.53 deg) and has the same mean |long slip| (0.212 vs 0.198). So not slope or slip
(lateral slip column reads 0 everywhere - getter useless). Open: which surface each area is.
Next probe (added 16:08): MUD_StutterRobot STOP now prints 'MUD robot surfaces run N' = wheel-frames per contact
surface file + FrictionTread + contact entity class.

### Surface per area (client logs_2026-10-11_16-06-32, 16:07-16:09)
The surfaces map was not cleared per run (bug, fixed after), so the logged counts are running totals; per-run = difference:
| run | start x/z | area | snaps | grass (grip 0.57) wheel-frames | dirt (grip 0.55) wheel-frames |
|---|---|---|---|---|---|
| 1 | 1039.8 / 2599.3 | north | 3 | 12725 | 223 |
| 2 | 1016.8 / 2596.4 | north | 3 | 12679 | 167 |
| 3 | 1069.8 / 2535.5 | south | 0 | 12104 | 88 |
| 4 | 1047.2 / 2540.7 | south | 0 | 12960 | 0 |
Checksum: grass 12725+12679+12104+12960 = 50468 = the last running total; dirt 223+167+88+0 = 478 = last total.
All contact entities GenericTerrainEntity. PROVEN: both areas are ~99 % vanilla grass.vhcsurf - surface is not the difference.
Snap positions (139 client framelogs, 261 frames with body jump > 0.15 m, 4 m cells, /tmp/snappos.py): spread over 137
cells, top cell 7 snaps - no single bump; it is an AREA effect. Next probe (16:15): physics entities within 30 m by prefab,
on the client at robot START ('MUD robot nearby') and on the server every 10 s ('MUD srv: ... nearby').

### Nearby physics objects (client logs_2026-10-11_16-10-58, server logs_2026-10-11_16-10-56, 16:11-16:14)
| run | start x/z | area | snaps | client: physics entities within 30 m |
|---|---|---|---|---|
| 1 | 1042.7 / 2603.7 | north | 2 | 0 |
| 2 | 1023.6 / 2592.3 | north | 6 | 0 |
| 3 | 1071.1 / 2531.5 | south | 0 | 4 (juniper bushes) |
| 4 | 1054.1 / 2516.0 | south | 0 | 1 (LightRunway_01_white) |
Server 'nearby' every 10 s: the same lists at the same places (north 0 x5; south 4 junipers, 0, runway light, ...).
PROVEN: the noisy area has NO physics objects within 30 m on either machine; client and server agree -> objects ruled out.
Per-area framelog compare (/tmp/area2.py, 113 north + 25 south runs, rows with speed > 1): wheel contact losses 0 / 0,
share of wheel-frames with |long slip| > 0.5 0.193 / 0.206, rpm change per frame 20.3 / 20.8, speed 1.83 / 1.84 (sd 0.22 /
0.23), contact height sd 0.035 / 0.035, yaw per frame 0.111 / 0.091 deg (north turns ~20 % faster). Only yaw rate differs.
INFERENCE (not proven): terrain SHAPE - north is flat (height range 0.27 m), south slopes/bumps (0.43 m); on flat grass the
truck spins at the grip limit in a tighter circle, which may make client and server physics drift apart more easily.

### SetPrediction(false) test (server logs_2026-10-11_16-23-02, client logs_2026-10-11_16-23-04, 16:23-16:27)
'MUD prediction: ... set OFF' on server (16:23:49.807, id 65) and client (16:23:49.858, id 109 = the robot truck).
| run | start x/z | area | snaps (mine 1 with body jump) |
|---|---|---|---|
| 1 | 1041.9 / 2587.8 | north | 3 (0.26, 0.27, 0.28 m) |
| 2 | 1047.6 / 2546.2 | south | 0 |
Plus user hand driving at 9-13 m/s: 5 'visual only' lines (0.10-0.16 m), 0 body snaps. 'mine 1' lines 8 = 3 + 5;
CLIENT CORRECTING 4 (3 in run 1 + 1 at 1671 after it). User: "driving felt fine".
RESULT: prediction off does NOT stop corrections or snaps (north 3/run, same as the 2.2/run baseline).
IDA (read): script SetPrediction = sub_1408445C0, writes byte +556 = !turnOn. Read only in sub_140843070 and
sub_14084B610 (car; also sub_1408524D0, sub_14086CD00 for the other classes): when +556 is set they SKIP a loop over
the queued moves (apply the move's inputs + physics calls). The client still simulates the truck itself every frame, and
the correction handler is not gated by +556. INFERENCE: the flag turns off move replay, not local simulation, so it
can't give "server-only" driving. MUD_PredictionTest.OFF set back to false.

### KEY: our MUD_Wheels limits were on in EVERY test today. True engine limits test 16:38-16:40
Found 16:35: MUD_Wheels.Register (modded Vehicle.EOnInit, every machine, every wheeled vehicle in play) calls
SetAllowance(false, NET_DIST 0.25, NET_ANGLE 0.05, NET_SPEED 1.0, NET_SPIN 0.8). So every "vanilla limits" /
"AllowanceTest OFF" run in this doc ran with a FIXED 0.25 m limit, not the engine's speed / 300. Snap size has always
matched the limit in force (NET_DIST 0.15 -> 0.11-0.21 m, 0.25 -> 0.20-0.32 m, AllowanceTest 1 m -> 1.03 m).
Test: MUD_Wheels.NET_LIMITS = false (no SetAllowance at all). 'MUD wheels: ... own net limits 0' on server (0x84) and
client (0xAB = truck 171). Server log logs_2026-10-11_16-37-33, client logs_2026-10-11_16-37-35.
| run | start x/z | area | robot corrections | s |
|---|---|---|---|---|
| 1 | 1031.4 / 2595.1 | north | 0 | 20.0 |
| 2 | 1013.4 / 2583.1 | north | 0 | 18.6 (Space) |
| 3 | 1026.5 / 2603.1 | north | 0 | 20.0 |
| 4 | 1014.7 / 2583.1 | north | 0 | 20.0 |
Whole client session: CLIENT CORRECTING 0, 'mine 1' 0. Server: 105 s with a pilot, 76 s at > 1 m/s (robot only).
Same area with our limits: 2.2 snaps/run (61 runs), 0-runs 36 %, but in time clusters - so 4 zeros is strong, not proof.
IDA: the phys check sub_14083B890 has no enable gate; SetAllowance (sub_140844470) only writes the 6 fields, no flag.
Engine defaults are TIGHTER on every axis than ours, so "fewer corrections with tighter limits" is unexplained -
UNRESOLVED. Next: >= 6 more runs + hand driving with NET_LIMITS false, then >= 6 with true in the same session spot.

### OUR ground, engine limits, rut cap removed (DS -server $Testingplanvechmsh:Arlnd.ent, logs_2026-10-11_16-46-50 both)
NET_LIMITS false ('own net limits 0' on server), PhysD = H + Rut (full depth), m_fRutSpeed 3 (TEST), robot circling in
one spot ~1005-1029 / 2586-2601 (carving its own ruts every pass).
| run | robot corrections | body jumps > 0.3 m | biggest m |
|---|---|---|---|
| 1 | 12 | 7 | 0.69 |
| 2 | 4 | 0 | 0.23 |
| 3 | 6 | 2 | 0.68 |
| 4 | 8 (Space at 19.8 s) | 1 | 0.32 |
| 5 | 6 | 5 | 0.72 |
Robot total 12+4+6+8+6 = 36 in ~100 s. Whole session: 'mine 1' 41, CLIENT CORRECTING 218 (most without a > 5 cm jump).
Probe 10 s lines during runs: a wheel on a collider 2+ cm off the newest heights, changed < 0.5 s ago, on a dirty
patch = the WHOLE 10 s in every window - the ground under the truck never stops changing while it circles.
Compare: same robot, engine limits, plain GM_Arland: 0 in 4 runs (16:38). So on our ruts the engine limits give MANY
more and BIGGER snaps (up to 0.72 m) than our old 0.25 m limit gave (~0.27 m). The moving ground is the cause here
(the original 2026-10-10 finding), and the plain-ground snaps were our 0.25 m limit.
