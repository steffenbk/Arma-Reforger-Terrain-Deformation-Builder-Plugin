# Fact check: ruts, craters, trenches, tunnels (2026-10-16)

User: "Don't focus on the house now. Only tunnels, craters, ruts and trenches. Continue finding information and
research, don't guess, fact check everything."
Checks every in-scope claim of research/2026-10-16-ground-options-full-review.md and
research/2026-10-16-architecture-gaps-deep-dive.md, adds new findings, and real-world reference data.

Verdict words: **VERIFIED-CODE** (read in Scripts/ today, line given), **MEASURED** (offline, game formulas:
results/2026-10-16-gap-checks.md, results/2026-10-16-voxel-crater-slope.md), **CORRECTED** (an earlier claim was
wrong or too broad - corrected text given), **ESTIMATE** (arithmetic from measured inputs, not measured itself),
**UNVERIFIED** (needs the game / IDA; test given), **READ** (web search-result text only: this session's network
policy denied community.bistudio.com, reforger.armaplatform.com, arexplorer.zeroy.com, dtic, researchgate, pmc,
globalsecurity, so no page could be opened; each source is the search snippet).

## 1. Summary - what changed after the checks

| # | Finding | Verdict |
|---|---|---|
| F1 | A joining client silently DROPS ops once 20000 are waiting (`s_mPending.Count() >= 20000`, MUD_VoxelProbe.c:973-974); ops apply strictly in seq order (Drain :993); the client asks for JIP once (s_bAskedJip :1030-1035) and the server answers once per controller (MUD_VoxelNet.c m_bMudVoxelJipSent). One dropped op = the joiner's ground stops for the rest of the match. | VERIFIED-CODE (new) |
| F2 | Rut-field cost on clients: no camera-distance gate (VERIFIED-CODE MUD_WorldGround.c:3891-3920 vs the old height path :6999). But "every client builds every rutted patch" was too broad: a NEW or SLEEPING patch is built synchronously at most once per frame (WHEEL_BUILDS_PER_FRAME 1, :117, :3152; ~9-12 ms each MEASURED 2026-10-09); a capped one is skipped then and made later by whatever makes it (InitPatch reads the field, :2977). EXISTING patches are marked and rebuilt with no distance limit. | CORRECTED |
| F3 | Tunnel roof under FLOOR 3 m: 0.84-0.87 m in the sample field (1-2 solid samples), 0.94-1.22 m drawn over the axis; clearance 2.13-2.16 m; no roof holes on flat ground. (Deep dive said "~0.8 m".) | MEASURED |
| F4 | A blast inside a tunnel opens it to the sky: RPG HEAT -> ~4.2 m wide hole, 81 mm-size -> ~8 m. | MEASURED |
| F5 | Crater next to a road: a 0.8-2.0 m drop within 0.5 m of the road edge (58-76 deg); the voxel path has no edge fade (the heightfield faded over 1 m, smoothstep, EdgeLock). | MEASURED + VERIFIED-CODE (new) |
| F6 | Repeated hits stack to FLOOR 3 m: 81 mm reaches 2.88 m at hit 2 and 3.0 m at hit 3 (heightfield cap was 2.39 m). | MEASURED (new) |
| F7 | Trench floors draw 0.25-0.55 m narrower than planned and vary with grid position (1.0 m plan: 0.46-0.72 m); walls draw ~49-57 deg (plan 60). Widths were chosen for the old 0.25 m grid (MUD_TrenchLayout.c WidthOf comment). | MEASURED (new) |
| F8 | Rut walls: profile shape barely matters; depth / radius does. Heavy truck (cap 0.35 m, RUT_R 0.5): p90 46 deg, max 48.5 deg; rut start / end 60 deg. RUT_R 0.6 brings it under 45 deg. The plan's (1-k2)^2 profile was NOT adopted (code: `1 - k*k`, MUD_VoxelProbe.c:2924 CarveFloor) - and would not have fixed it. | MEASURED + VERIFIED-CODE (new) |
| F9 | Real craters are much smaller than ours (section 4). Our 81 mm crater: 1.43 m deep, ~6.3 m across the crest. Measured 120 mm mortar (more explosive than 81 mm): 0.72 m deep, 2.69 m across. | READ |
| F10 | Vanilla vehicles were moved to VehicleWheeledSimulation_SA (Bohemia modding update, READ); vanilla script keeps separate SA / non-SA code paths (READ). Our whole wheel system finds vehicles only via `FindComponent(VehicleWheeledSimulation)` (MUD_Wheels.c:24). Our ruts DO work on vanilla trucks in the user's build (PROVEN, all robot runs), so either the user's build still uses the old class or SA inherits it. | UNVERIFIED - one log line decides |

## 2. Claim-by-claim

### Ruts
| Claim (source doc) | Verdict | Evidence |
|---|---|---|
| Ruts no longer use voxel columns / MAX_COLUMNS (review) | VERIFIED-CODE | Issue skips ConvCols / cap / seed for kind 4 when RUT_FIELD (MUD_VoxelProbe.c:1907-1916); RUT_FIELD true (:276) |
| The rut edge step was removed | VERIFIED-CODE | CarveFloor `fall = 1 - k * k` (MUD_VoxelProbe.c:2924) "fall 0 at the rim, not 0.5" |
| The smooth (1-k2)^2 profile / <= 45 deg target was built | CORRECTED: not built; and MEASURED it would not reach 45 deg at cap 0.35 (45.6); RUT_R 0.6 would (44.3 max) | gap-checks section 1 |
| Rut cap 0.35 m, step 0.08 m | VERIFIED-CODE | MUD_WorldGround.c:1191 depth = min(0.08 + t x 0.025, 0.35); RutWheel(.., m_fRutStep, depth) :1225 |
| Wheel = one centre ray, raw triangle normal | IDA 2026-10-15 item 5 (not re-checked here: no IDA in this session) | research/2026-10-15-ida-rut-questions.md |
| Tracked vehicles make no ruts | VERIFIED-CODE | only VehicleWheeledSimulation is looked up (MUD_Wheels.c:24, MUD_WorldGround.c:1160, :1336); VehicleTrackedSimulation is a separate API class (READ: script API lists GearboxGearsCount for VehicleTrackedSimulation, VehicleWheeledSimulation, VehicleWheeledSimulation_SA_B) |
| Rut-field patches build with no distance gate | VERIFIED-CODE, scope CORRECTED (F2) | |
| ~10 mesh + ~10 collider rebuilds/s per vehicle at 6-7 ms | from LOG (rut README stage 4, SP WB) - not re-measured | docs/features/ruts/README.md |
| "~3 trucks saturate a DS" | ESTIMATE (30 fps DS from MUD_ServerLog 2026-10-11 x 1 collider/frame vs ~10/s per truck); MayRun lets a job wait up to MAX_WAIT frames, so real throughput is lower, not higher | needs the queue-length test |
| Band mesher is on, 3-4x the grid cost | VERIFIED (prefab m_bBandMesher 1; results/2026-10-13-sdf-phase2.md: mesh 2.9-4.7 vs 0.9-1.1 ms, collider 4.2-5.5 vs 1.4-1.6) | |
| Grid path draws the rut field + soil the same way | VERIFIED-CODE | BuildMesh :5832-5845 (FIELD_SOIL_D), BuildCollider :5955-5962 (floor + lock) |

### Craters
| Claim | Verdict | Evidence |
|---|---|---|
| Voxel craters skip jitter / lobes / stack cap / 42 deg rule | VERIFIED-CODE | Crater(): Blast(wp, full * scale) at MUD_WorldGround.c:5427 runs before the jitter / rim / stack code; InArea branch :5369-5381 |
| Steep crest ring from 81 mm up (14-18 % of area > 50 deg) | MEASURED | voxel-crater-slope.md |
| Soldier MaxSlope 50 deg | IDA 2026-10-10 (mud-gotchas line 10), not re-checked | |
| Repeated hits go to 3 m | MEASURED (F6) | |
| Crater at a road edge leaves a cliff | MEASURED + VERIFIED-CODE (F5): Carve skips m_aHard (:2684); hard = within 0.01 m of a non-hole cell (Hard :3451) | |
| Blast in a tunnel cuts to the sky | MEASURED (F4); Crater()'s voxel branch only refuses blasts ABOVE the undug ground (:5374-5378) | |
| No bullet slab / safety plate under voxel cells | VERIFIED-CODE: VOX cells get slot -1 (:5652-5654) and slab + plate are built only for cells with a slot (:5771-5783, band :6352+); bricks are Terrain\|Foliage only (MakeBody) | |
| Bullets pass voxel rims | UNVERIFIED (game test). READ: Reforger penetration compares the projectile's penetration against the hit collider's game material resistance x thickness, and a game material can override thickness; how an exit / thickness is found on a one-sided trimesh is not documented (no source found) | Bohemia wiki Weapon Creation snippet |
| GroundRescue is off on voxel cells | VERIFIED-CODE (:5011-5027, IsVoxCell -> return) - it was made for prone soldiers shoved through thin crater walls (PROVEN 2026-10-10) | |
| Objects over new craters are not woken | VERIFIED-CODE (no SetActive / entity query after GoLive; only MUD_WakePlaced for GM spawns, MUD_VanillaHoleFix.c) - effect UNVERIFIED | |
| Voxel ops ignore the building lock | VERIFIED-CODE (Carve / CarveTunnel / CutTrench / FillTrench skip only m_aHard) - effect UNVERIFIED | |
| Past MAX_COLUMNS craters fall back to the heightfield | VERIFIED-CODE (Issue refuses :1924-1928 -> Blast false -> Crater continues) | |
| ~195 mortar craters fit the column cap | ESTIMATE (column count per crater averaged over 4000 positions; overlapping craters reuse columns, so more fit in one shelled area) | |
| Client memory ~110 KB per column | ESTIMATE from one JIP measurement that counts everything built for the join (jip-size.md: 1182 columns, +127 MB) | |

### Trenches
| Claim | Verdict | Evidence |
|---|---|---|
| Walls 60 deg by design | VERIFIED-CODE (SLOPE_RUN 0.58 -> atan(1/0.58) = 59.9 deg); drawn 49-57 deg median MEASURED | |
| Max 2.5 m, default 1.5 m, 0.25 m per swing | VERIFIED-CODE (MAX_LAYERS 10, DEFAULT_LAYERS 6, LAYER 0.25) | |
| No spoil / parapet | VERIFIED-CODE (CutTrench "Only lowers") | |
| Drawn floor narrower than planned | MEASURED (F7) | |
| Shovel needs a shovel; T (tunnel) needs no tool | VERIFIED-CODE (MUD_Dig.HoldsShovel checks; MUD_RpcAsk_VoxelTunnel checks only on foot, 3 m, rate) | |
| Trench walls can be climbed | UNVERIFIED: walls > MaxSlope 50, so no walking out; Reforger climb / vault limits not found in any source (READ: none) - game test | |

### Tunnels
| Claim | Verdict | Evidence |
|---|---|---|
| Capsule r 0.8, 2.5 m tall, 1.1 m ahead, every 300 ms | VERIFIED-CODE (:313-318) | |
| FLOOR 3 m limits tunnels; DepthBox ignored | VERIFIED-CODE (CarveTunnel floorY = undug - FLOOR :3261; no DepthAt in MUD_VoxelProbe.c) | |
| Roof ~0.8 m | CORRECTED: 0.84-0.87 m field, ~1 m drawn (F3) | |
| No roof / bridge rules exist | VERIFIED-CODE (nothing in CarveTunnel or after it) | |
| Volume per T click | VERIFIED-CODE arithmetic: capsule = pi r^2 x 2h + 4/3 pi r^3 = 1.81 + 2.14 = 3.95 m3 (overlap with the last click not subtracted); a shovel swing = 1 x 1 x 0.25 = 0.25 m3, at most 1 per s (MUD_Dig FREE_LENGTH / FREE_WIDTH / MIN_GAP_MS) | |

### JIP / MP (in scope because ruts fill it)
| Claim | Verdict | Evidence |
|---|---|---|
| JIP sends the whole op log in one server frame | VERIFIED-CODE (MUD_RpcAsk_VoxelJip loops Rpc over all batches) | |
| 20000-op client cap with no recovery | VERIFIED-CODE (F1, new) | |
| 18-31 MB of op text per hour with 10 trucks | ESTIMATE (339 chars per op and 2.9 rut ops/s per truck, both MEASURED: jip-size.md, freeze-test.md) | |
| Only the rut floor is hashed across machines | VERIFIED-CODE (DISP_HASH at CarveFloor :2999 and ClearFloor :3201; heightfield HashLines hash m_aH) | |
| Hosted servers can be Linux | READ: native Linux dedicated server binary (hosting guides; Bohemia wiki lists server app ids) | |

## 3. Withdrawn / narrowed earlier statements
- Review N5 (voxel house desync) - withdrawn earlier today (MP-gated). Out of scope now.
- Deep dive 3.1 "every client builds every rutted patch on the map" -> narrowed to F2.
- Deep dive 3.5 "at most ~0.8 m of roof" -> 0.84-0.87 m field, ~1 m drawn (F3).
- Review section 7 bullet "MatTerrainObject on voxel tops never probed" - stays withdrawn (MAT_TOP, VERIFIED-CODE :351).

## 4. Real-world reference data (READ - search snippets; all primary pages were blocked)

### Craters
| Source | Munition / ground | Apparent diameter | Apparent depth |
|---|---|---|---|
| CRREL Special Report 95-2 (Collins 1995), Eagle River Flats, winter | 81 mm mortar, point-detonating, 3 rounds | ~2.45 x 2.15 m (radius 1.15 m) | 0.16 m, mostly the 0.15 m snow cover |
| same | 81 mm, delay fuze | camouflets (hidden craters) | - |
| CRREL 2007, Eagle River Flats (marsh), June | 120 mm mortar HE, 13 high-order rounds | 2.69 m (radius 1.35 m) | 0.72 m |
| Ukrainian battlefield satellite mapping (ScienceDirect 2023) | mortar | 5-30 m2 area (~2.5-6.2 m across) | - |
| explosion-crater paper (snippet) | general | depth ~ 1/4 of diameter, soil dependent | |
| ARES crater analysis guide | 60 / 81 / 82 mm | soil density has the biggest influence on small-munition craters | |
Ours (CODE + MEASURED): 81 mm M821 0.99 kg TNT -> FullDepth 1.43 m, crest 3.15 m radius (6.3 m across), rim to 4.58 m.
That is ~2x the measured 120 mm depth and ~2.3x its diameter, for a smaller shell. The depth/diameter ratio (0.23)
is in the normal range; the SCALE is high (FullDepth 1.5 x kg^0.6 was tuned to "mortar 1.5 m", a game choice -
MUD_WorldGround.c attribute comment). Not a bug; a calibration the user can decide on.

### Trenches / fighting positions
- ATP 3-21.8 (2024, InfantryDrills snippet): maximum depth armpit deep (if soil permits); parapet retaining walls
  at least 39 in (~1 m) thick (an M4 / M7 length + 6 in).
- Older US field manual (snippet): parapet not more than 50 cm high, at least 90 cm wide; 75 cm thick for larger pits.
- 45 cm (18 in) of earth where no natural parapet exists (civilian guide snippet).
- Ours: no parapet or spoil at all; depth 0.5-2.5 m; soldier exit depends on climbing (untested).
- How much earth stops 5.56 / 7.62: the only figures found were forum posts relaying a "military spec" table with
  unclear units - NOT used.

### Tunnels
- Cu Chi (UNESCO tentative list text): levels at ~4 m, ~6 m and 7-10 m below the surface; passages on average
  62 cm wide x 104 cm high (level 3: 75 x 95 cm); dug in compact laterite clay.
- WWI dugouts (war history overview, period accounts): German dugouts 6-9 ft (1.8-2.7 m) deep in 1915, 25-30 ft
  (7.6-9 m) on the Somme; an "ordinary" Australian dugout had 6-7 ft (~2 m) of earth on it.
- Ours: 2.1 m clearance (walk upright), ~0.85-1 m roof, floor at most 3 m down. Real tunnels are lower / narrower
  and much deeper under cover.

### Ruts
- Vennik et al. 2019 (Soil & Tillage Research 186:120-127): 7 Mg military truck, 7 Estonian soils, ruts after 1 and
  10 passes; deeper ruts on grassland than arable; multipass on grassland not well simulated. Depth values not
  reachable from here.
- Soil-bin study (Sci Rep 2025): first pass = 55 % of the total rut depth (track wheel, 2 kN); increases per pass
  shrink. Different load class - only the TREND transfers.
- Ours: first pass 0.08 m of a 0.35 m cap (23 %), equal steps per pass until the cap.
- Jansson & Johnsson (cited in snippet): tracked machines rut less - 8 tracked passes ~ 2 wheeled passes.

### Reforger engine (READ)
- Character step height / max slope / climb height: no published values found.
- Penetration model: projectile penetration vs game material resistance x collider thickness (thickness overridable
  in the game material); exit handling undocumented.
- Vehicles moved to VehicleWheeledSimulation_SA / SCR_CarControllerComponent_SA (modding update); old names "still
  work if you keep them" but may lose support.

## 5. What could not be checked here, and how to check it
| Item | Why not here | How |
|---|---|---|
| Bullets through voxel rims / parapets | needs the game | SP: prone / standing target in a voxel crater and trench, shooter 50 m (deep dive 4 #2) |
| Soldier can walk / climb out of a mortar crater, a 1.5 m trench | needs the game (step / climb values unpublished) | SP walk test |
| Which wheel sim class 1.9 vanilla trucks carry | needs the game / pak | log line `MUD wheels: <id> registered` on a fresh truck |
| Real throughput of rut-field rebuilds with many trucks | needs MP | dirty-queue log line, DS + 2 peers |
| 20000-op JIP stall | needs MP (or a fake-op switch) | issue 21000 fake rut ops on a local DS, then join; expect 'waiting 10 s for op seq' |
| Primary sources (CRREL tables, field manuals, Bohemia pages) | network policy denied the hosts | allow them in the environment's network settings, or read them locally |
