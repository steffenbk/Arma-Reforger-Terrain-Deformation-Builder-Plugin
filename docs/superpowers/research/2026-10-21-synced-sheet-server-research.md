# Synced sheet on a real server - far pop-in, sync memory, per-client op cost (research, 2026-10-21)

Prompt: docs/superpowers/prompts/2026-10-21-research-synced-sheet-popin.md (branch claude/upbeat-lovelace-q0w022,
commit 3ab5bb8). Code read on that commit. Source data: results/2026-10-21-mud-lab-stage3-measure.md sec 12.
Labels: PROVEN-CODE (file:line), MEASURED (log / doc), [repo] (repo doc claim), INFERENCE. Nothing here was run.

## Short answers

**1. Is the ring the pop, and what is outside it?** Yes, confirmed from the code.
- The sheet tile list is every covered tile whose middle lies within `m_fRadBuild` of the camera (QueueNear,
  MUD_MudSheet.c:5696-5785; SheetWanted 5306 returns true for every covered tile when SPARSE is false, PatchLiftOn
  5289). `m_fRadBuild` = 320 / speed clamped to 20..40 m (RadSpeed 5794-5795): 32 m at 10 m/s, 21.5 m at 14.9 m/s.
  That matches the logged 40 / 29.7 / 27.2 / 25.3 / 20 (MEASURED). Builds start with the tile nearest a point 1.5 s
  ahead of the camera (AHEAD_S 225).
- Meshes go past RADIUS + RADIUS_HYST = 46 m (DropFar 5801-5803), but only once there are more than MESH_CAP 600 stored
  meshes, farthest first (5836-5853). 600 tiles x 9 m2 = 5400 m2, about a 41 m disc, so in practice old tracks
  behind you drop at about 46 m too (INFERENCE). When you come back to your own loop, its tiles are rebuilt at 20-40 m.
  That is the pop (PROVEN-CODE + MEASURED 'to build 0' = built on time, at the edge).
- Outside the ring there is no sheet tile, so `Covers` is false and the LAZY_RUT_MESH patch mesh shows
  (MUD_WorldGround.c:4867-4881, SheetShow). That patch is hard ground + rut floor. **A worn-only track shows nothing
  there.** WEAR_PASSES 1 means pass 1 writes no floor, no FloorDirty and no patch rebuild (MUD_VoxelProbe.c:283-289,
  WearPass 5052). A track driven 2+ times shows a 3 cm groove per pass (RUT_PASS_MAX 0.03, :275), with soil from
  6 cm (FIELD_SOIL_D, MUD_WorldGround.c:92). Client patches draw it out to m_fClientBuildRadius = 300 m (:221).
- A second part of the pop (INFERENCE): the untouched sheet top is THICK + LIFT = 0.16 m above the hard ground
  (MUD_MudSheet.c:596, :115). With SPARSE false the patch is not lifted, so at the ring the whole surface steps up
  16 cm at the same moment the track appears. A fully pressed track (D ~ 0) sits at patch level. In other words,
  the near track's relief comes from raising everything around it, and that raised surface ends at the ring. This is
  the same "driven / undriven height step" the mud-sheet README still lists as open.

**2. Far look options:** see the table. Recommended: **A, the far press**. Patch meshes outside the drawn sheet are
pressed down by the sync layer's own depth, so the groove is already there when the ring arrives.

**3. Freeing or capping m_mTiles.** Deleting tiles one machine at a time is NOT safe. A tile's existence is state:
LimitDrag, the slump and the hashes read only existing tiles (SnapParts comment, MUD_MudSync.c:1749-1750), and
every machine must run every op on the same set. Two safe steps:
- **(a) Lossless compaction.** P / PT / PV are only read under `PV == veh && time - PT <= PASS_GAP_T`
  (:912-913, :1009-1012). A new pass overwrites P (:1010). So once the latest op time is more than 4 s past a tile's
  last stamp, those three arrays can never change a future op (PROVEN-CODE; op time is server GetTickCount / 100,
  monotonic within one server run, INFERENCE). Two changes make dropping them exact:
  - Hash and snapshot them as their defaults when dead, judged at the op's (or snapshot's) own op time. That covers
    the per-op hash, LayerHash :2066 and SnapParts / RunToks.
  - After that, any machine can locally pack an idle tile to D + B and Ou + Ov: two ints a cell, each half the
    QUANT_F int (16 bits), exact via CellQ / FromQ, the same path the snapshot uses. That is 3.2 KB instead of
    11.2 KB (-71 %), less if only cells off their default are stored.
  - Range check needed: D above 0.655 m or drag outside +-0.327 m does not fit 16 bits. The fallback keeps the full
    tile.
- **(b) Synced forget, reset on touch.** Each tile keeps `m_iTouchT`, the op time of its last write (stamp, slump or
  clear; a clear op uses the last rut op's time). A tile untouched for longer than FORGET_T (for example 20 min of op
  time) counts as absent for every reader at the current op time:
  - TileOf(make) resets it to fresh, TileOf(no make) returns null, and the hash and SnapParts skip it.
  - Every machine then sees the same thing at the same seq without scanning, so any machine can physically free
    expired tiles whenever it likes.
  - Expiry must never drop bare cells. Keep a 400-bit bare mask per tile forever (13 ints), or craters get
    re-covered with mud (Stage 4 behaviour, [repo] README).
  - The drawn sheet needs a slow local sweep to redraw tiles that expired near the camera (look only).
- **What the server must keep:** all live tiles, compacted; the bare masks forever; the latest op time. The
  snapshot needs that op time too, and LoadSnap must restore it. Without it a joiner's expiry clock differs from
  everyone else's.
- **Why it matters (INFERENCE from MEASURED 702 tiles in ~75 s, ~750 m):** about 0.94 tiles per metre of fresh track.
  That is about 380 MB per truck-hour of fresh offroad driving at 10 m/s, uncompacted, on the server AND on every
  client. Compaction (a) alone gives about 110 MB; (b) bounds it to the traffic of the last FORGET_T. J2 snapshot
  size and joiner load time grow the same way (measure: 'MUD sheet sync: snapshot at seq ... bytes').

**4. Can a client skip or defer far ops?** Not by simply deferring them. An op's sink and slump read the data ground
at its own seq. Submit flushes all open sync work before the next op changes the ground (MUD_VoxelProbe.c:4071,
MUD_MudSync.c:314-326). A far truck's next rut op changes the ground right next to its last one. So "store now,
press later" reads a different ground and gives different mud: no longer exact (PROVEN-CODE for the order rule,
INFERENCE for the divergence).
- **The exact way is region gating with a per-region snapshot** (the J2 joiner path made local):
  - Split the world into 48 m blocks. A client holds the blocks within R_HOLD (for example 150 m) of its camera and
    vehicle.
  - An op whose box lies wholly in held blocks runs as today.
  - An op that touches an unheld block is skipped. It only marks the blocks it touched as dirty, held ones
    included: a held block next to it is then stale and gets dropped.
  - When the camera comes near a dirty or stale block, the client asks the server for that block, plus a one-tile
    margin, as SnapParts items at the server's current seq S'. It installs them and skips that block's ops <= S',
    as s_iSnapTo does now (:229-230), per block.
  - The server keeps the whole layer (SERVER_LAYER already does).
  - Clients pay only for ops near them, and free far blocks completely.
- **Cost today (INFERENCE from MEASURED sync 0.51 ms/frame at 91 fps):**
  - About 46 ms per second per offroad truck, or about 11 ms per op at the ~4 ops/s per truck the code comment
    gives (:316-317). Far ops skip the drawn-side steps (JOB_SHEET / JOB_REPLAY) and are cheaper. Unmeasured;
    guess 50-100 %.
  - 10 trucks offroad: 2-7 ms/frame at 60 fps on every client, and spiky. Ops from several trucks arrive closer
    together than one op's work, so Flush runs them in one frame (the code says so, :316-317).
  - 30 trucks: 6-20 ms/frame. At the top of that range the work exceeds real time and the client cannot keep up.
  - The server pays all of it on every machine size (DS sync-layer cost NOT measured yet; the hosted run had a
    client log only).
  - With gating, a client pays only for the trucks within R_HOLD.

## Problem 1 - options ranked

Look columns are what the player sees of a track outside the ring. ms = extra client script per frame (INFERENCE,
from the 2026-10-09 patch bench: mesh + skin cover ~2.8 ms, setup 2.9, index lists 2.5; MUD_WorldGround.c:215
attribute text; tile build ~2.9 ms DS 14:31).

| # | Option | How | 50 m | 100 m | 200 m | ms / frame | Memory | MP risk | Cannot fix | Measure first |
|---|---|---|---|---|---|---|---|---|---|---|
| A | **Far press on the patch mesh** (D2 PressAt idea, depth from the sync layer, not wear) | Mesh only, no collider, not J2. Outside drawn sheet tiles a patch vertex drops by (THICK - D_sync) x lock (min of the sync cells within half a patch cell); the existing reveal turns it to soil from FIELD_SOIL_D. Sync tiles with D < THICK mark the patches over them "far dirty"; FieldDue makes patches there out to the client build radius; rebuild at most once per 2 s per patch, RENDER_GATE, 1 per frame. Under the sheet the patch is always lower than the sheet, so no z-fight and no second surface owner (lesson 7) | groove + soil line, 0.25 m grid (blocky edge) | brown double line | thin line (no patch past 300 m) | ~0.05 per truck in view (1-2 rebuilds/s), plus first patch make ~8 ms each at 1 a frame | patches along every track (now only cut ones); per-patch size not measured | none (client look only) | no flow / ridges far; feet and wheels on a far track float up to 15 cm over the drawn groove (collider unchanged); the 16 cm ring step stays | patch makes + rebuilds per 10 s, 'MUD world' budget line, look at 50 / 100 / 200 m |
| B | Keep own tracks longer: MESH_CAP 600 -> 1500 | one const | full sheet out to wherever you drove last | same | same | ~0 build; render of up to 2.5x tile meshes | 2.5x stored meshes | none | fixes only coming back to your own loop; others' tracks still pop | meshes stored vs fps on a standalone run |
| C | Track ribbon mesh | quads along each op segment at contact y + 0.5 cm, dark dirt emat, merged into one MeshObject per 64 m block, rebuilt <= every 2 s | dark stripes | dark stripes (colour reads well far) | stripes | small (few k verts / block) | small | none | only 1 cm between the patch (ground) and the pressed sheet (ground + D + LIFT): z-fight with the patch far away or poking through the sheet near; no depth | WB look at 100 m for z-fight |
| D | Slide-in at the ring edge (no alpha on script meshes) | a new stored tile starts THICK + LIFT lower and moves up over 0.3-0.5 s with SetOrigin; the tile is its own entity, no collider (COLLIDE false), so no rebuild. Morph by rebuilds = 3-5x build cost, rejected | - | - | - | ~0 (4-5 tiles moving) | 0 | none | softens the 16 cm step only; the track still appears at the ring. Untested: z-fight with a half-covered patch during the rise | WB look |
| E | Coarse far mesh per 32-64 m block | one low-detail mesh from the sync depth | blob at a 1 m grid | blob | blob | 0.5 m grid = 16.6k verts = ~110 ms a build at 6.6 us/vert (scaled from 2.9 ms / 441 verts); 1 m grid ~28 ms and the 0.4 m track aliases away | 1 mesh / block | none | two owners of one surface with the patches (lesson 7) | not worth a test before A |
| F | SPEED_RADIUS off | ring stays 40 m at speed | - | - | - | ~+1.3 at 15 m/s (demand model ~5.65 x speed tiles/s at 40 m, :228-230: 44 -> 85 builds/s at 2.9 ms), near the ~1 build/frame ceiling | +tiles | none | moves the pop from 20-30 m to 40 m only | 'to build' backlog at 15 m/s |
| G | Decals / texture swap per segment | - | - | - | - | - | - | - | blocked: World.CreateDecal gives null on our script meshes (MEASURED, mud-gotchas :452); a per-segment texture swap is what A already does via the reveal | - |

Variant of A rejected: lifting every offroad patch 16 cm except under the track would match the sheet top exactly
(SPARSE A2 + LiftAt :7007 with depth). It costs a rebuild of every offroad patch in view, adds skin / patch seams,
and grass covers lifted ruts (LOOK_LIFT_TEST note, MUD_MudSheet.c:250, research/2026-10-19-ruts-invisible.md).

## Recommended plans

**Problem 1: A, far press (plus B as a free knob).** Build behind one const `FAR_PRESS`, client only:
- LiftAt / PressAt gets a far-press branch for patches that `Covers` says are not under a drawn tile.
- A per-patch D grid is read once per build straight from the sync tiles. Do not call Get per vertex: 1089 vertices
  x 4-9 cells is too slow.
- The sync job's JOB_WRITE marks "far dirty" patches.
- FieldDue treats tiles with a sync tile as rut patches, so worn-only tracks get a patch.
- Keep D, the slide-in, for later, if the 16 cm step still shows after A.

**Problem 2:**
1. Measure the server layer cost (below) before building anything.
2. Lossless compaction (3a): canonical dead pass state in the hashes and snapshot, then local packing of idle
   tiles. Done in one build, hash lines re-baselined.
3. Synced forget (3b): reset on touch, bare masks kept, latest op time in the snapshot.
4. Region gating (4) once more than about 5 trucks are expected. It is the only fix for the per-client cost, and it
   also gives clients real memory freeing.

## Smallest tests

Workbench look run (function only, one session):
1. FAR_PRESS on, MESH_CAP 600. Drive a 3-pass loop offroad: 1 pass on part of it, 3 passes on another part.
2. Free camera at 50 / 100 / 200 m: is the 1-pass track visible as a line? Is the 3-pass track soil coloured?
   Screenshot each.
3. Drive back into the loop at 15 m/s. Does the groove continue across the ring edge? Is the remaining pop only the
   16 cm surface step plus finer detail?
4. Stand a soldier prone on a far track and check the float from 40 m.
5. Log: 0 script errors; 'MUD world' budget line, patches made / rebuilt per 10 s.
6. MESH_CAP 1500 for the second half of the session: are revisit pops gone?

Dedicated server, or standalone on the hosted server (cost; at least 6 runs per setting, per CLAUDE.md):
1. DS with SYNC on, two robot trucks plus one human client far from them (more than 300 m).
2. Server log: 'MUD mud sheet sync' 5 s lines (ms per phase, ops, flushes) = the server layer cost per truck. Not
   measured yet.
3. Far client log: the same line. This is the cost of ops you will never see (the case region gating removes), and
   'MUD prof' sync ms against the trucks' count.
4. Same session, FAR_PRESS on vs off on the client: MUD ms and fps.
5. After 10 min: 'tiles touched' and snapshot bytes. Then a late joiner: snapshot load ms and a layer hash match.
   This is the baseline for 3a / 3b.
