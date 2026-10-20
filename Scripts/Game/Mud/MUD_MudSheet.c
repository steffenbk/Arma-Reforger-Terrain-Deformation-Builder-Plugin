//! TEST 2026-10-14 v2 (MudRunner way, docs/superpowers/research/2026-10-14-mudrunner-mud-article.md): visual-only
//! mud sheet around the local player's vehicle (no collider, nothing sent; every machine except a dedicated server
//! runs its own). A world-aligned grid (N x N, CELL m) follows the vehicle by whole cells. Per vertex: mud height d
//! over the ground g, and a texture drag (Ou, Ov in m). Each tyre step, like MudRunner's primitives:
//!  PRESS (min): d = 0 under the tyre (the real ground and its rut show there), texture dragged along.
//!  PUSH (max): a ridge each side and a bow wave ahead, height H from speed and sink (no volume kept),
//!   texture dragged outward / forward.
//! Mud slumps downhill where steeper than REPOSE (volume kept). Only quads with mud are drawn (rest degenerate);
//! vertices without mud sit under the ground. Ground heights from traces down (vehicles and characters skipped).
//! v3 (user sketch 2026-10-14 21:15): a SOFT DIRT LAYER, THICK deep, lies on the hard (voxel / heightfield) ground
//! over the whole sheet; tyres press through it to the hard ground (they look sunk in), ridges THICK + H beside.
//! It fades into the ground over the last EDGE m of the sheet. Whole sheet drawn, indices only rebuilt when the
//! traced set changes. v1 (volume-kept berms) = checkpoint v047. Prints 'MUD mud sheet:' every 5 s.
//! TRAIL (2026-10-14 22:45, user: "the mud is only one tile so it quickly disappears"; v053 = before): the sheet
//! shifts by whole TILE_M tiles on a world grid; a tile a wheel touched stays behind as a frozen trail tile (its data
//! kept, mesh still drawn; since MUD ZONES: no limit, zones bound it) and comes back to life when the sheet covers it again.
//! MUD ZONES (2026-10-14 23:10, user pick B; plan docs/superpowers/plans/2026-10-14-mud-zones-plan.md; v055 = before):
//! mud only where a placed MUD_MudZoneEntity says so. Every zone tile is built at load (one per frame) and stored;
//! the live window takes them out while it covers them and puts them back after. Window tiles outside every zone
//! are hidden and give no drag. A tile fades only on sides where the next tile is not a zone tile. No trail limit.
class MUD_MudSheet
{
	// PARKED 2026-10-15 17:05 (user: ruts tank fps, jitter on a DS; checkpoint v083 = last sheet state). See
	// docs/features/mud-sheet/README.md. true = mud sheet + ONE GRID back on.
	// SYNCED SHEET Stage 1 (user go 2026-10-17, v177 = before): back as a LOOK-ONLY layer - ONE_GRID, COLLIDE and
	// DRAG_TEST off; the wheels and feet stand on the felt ground (heightfield + rut field) under the mud.
	// PARKED AGAIN 2026-10-18 (user: 'park the mud zone, focus on voxel ruts'; v206 = last sheet state, step fix
	// LIFT_BY_DRAWN built but the user still saw the step / pop). Lessons: docs/features/mud-sheet/README.md top.
	// MUD LAB (2026-10-20, prompts/2026-10-20-local-mud-lab-stage1.md): runtime switch, only MUD_MudLab turns it on, as
	// SHEET LOCAL (SYNC false): client-only, no sync layer, the deposit band (tiles only where wheels went). Off = parked.
	static bool ENABLED = false;
	// SYNCED SHEET Stage 2 (2026-10-18, v179 = before): the drawn mud = the sync layer (MUD_MudSync: pressed from the
	// replicated rut ops, the same on every machine) + this machine's own stamps at the tyres it sees (prediction),
	// each dropped when the op holding it comes (SyncChanged) or it expires (ExpirePreds). No local slump (the sync
	// layer slumps per op). false = Stage 1 (local stamps + slump only).
	static bool SYNC = true;                    // MUD LAB: false while sheet local runs (LabSet), back to true when off
	static const float COVER_R = 0.3;           // SYNC: an own stamp whose middle is this close (m, x z) to an op segment is that op's
	static const float PRED_GONE_S = 5;         // SYNC: an own stamp no op took goes after this (s) once its vehicle is gone or PRED_FAR m away
	static const float PRED_FAR = 10;           //   (RUT_BEHIND: a parked truck's last segments are never cut, so no op ever takes them)
	static const float PRED_MAX_S = 60;         // SYNC: ... and after this in any case
	static const float PRED_PAD = 1.25;         // SYNC: m a stamp reaches past its segment (HALF_MAX ridge band + NOSE + FRONT_LEN)
	static const int PRED_CAP = 4000;           // SYNC: most own stamps held (oldest go first)
	static const int EXPIRE_BOX = 160;          // SYNC: cells a side of one restore box when expired stamps go
	static const int REPLAY_CHUNK = 16;         // SYNC op split (2026-10-18): own stamps replayed per MUD_MudSync step
	static const int REPLAY_HOLD_MS = 500;      // SYNC: an unfinished replay stops holding the upload after this
	static const bool FOLLOW = true;           // live window under the vehicle (wheel marks + drag); false = only the zone tiles built at load show. Off 23:35 -> back on 23:42 (user: mud not pressed)
	// user 2026-10-14 22:55: MUD_SheetMud.emat (ST_Mud_01) showed white; DirtCover_01 is the proven voxel dirt
	static const ResourceName MAT = "{6EBF7ADA9FD582F7}Assets/Structures/Military/Fortifications/DirtMounds/Data/DirtCover_01.emat";   // = MUD_VoxelProbe.MAT_DIRT
	static const int N = 81;                    // vertices per side (= the holder class sizes)
	static const int NN = 6561;
	static const int QUADS = 6400;              // (N - 1)^2
	static const int TILES = 4;                 // tiles per side: one mesh per tile (the whole 6561-vert sheet as ONE
	                                            // mesh never drew, a 4-vert quad did - 2026-10-14 21:35 test)
	static const int TQ = 20;                   // quads per tile side ((N - 1) / TILES)
	static const int TV = 21;                   // verts per tile side (= MUD_MudSheetTile sizes)
	static const float CELL = 0.15;             // m: sheet 12 m
	static const float SIZE = 12;               // (N - 1) * CELL: UV 0..1 over the sheet (tiling in the emat)
	static const float RECENTER = 1.8;          // m off the middle before the sheet shifts (by whole tiles)
	static const float TILE_M = 3;              // TQ * CELL: tile size, the world grid the sheet shifts on
	static const int UV_TILES = 4;              // SIZE / TILE_M: UVs repeat every 4 tiles (12 m, as before)
	static const float HALF = 0.18;             // tyre half width (m)
	static const float BAND = 0.32;             // ridge width beside the tyre (m); 0.4 until 2026-10-15 (HEAVY: narrower = chunkier ridge)
	static const float FRONT = 0.35;            // (old flat bow wave, unused since ARCH 2026-10-15)
	static const float NOSE = 0.2;              // tyre footprint ahead of the contact point (m): pressed, the arch hugs it
	static const float ARCH_H = 0.8;            // arch round the tyre front: x the side ridge height
	// BOX RUTS (user 2026-10-15: 'all one width', 'more square shaped', 'more narrow pushing in front'). BOX = true:
	// the footprint is a box (tyre width x NOSE), not a round capsule; the side ridges stand up steep right at the rut
	// wall (flat top RIDGE_TOP of the band, then fall off); the push ahead is only as wide as the tyre and FRONT_LEN
	// long (no arch round the sides). Tyre half width = wheel radius x HALF_PER_R, clamped (the script API has no tyre
	// width, only WheelGetRadius - INFERENCE: tyre width ~0.75 x radius on the vanilla trucks/cars). false = old capsule.
	static const bool BOX = true;
	static const float HALF_PER_R = 0.38;
	static const float HALF_MIN = 0.11;
	static const float HALF_MAX = 0.28;
	static const float RIDGE_TOP = 0.35;         // share of the ridge band at full height next to the wall
	static const float FRONT_LEN = 0.22;         // m: push ahead of the footprint
	// SOFT RUTS (user 2026-10-15: ruts full width from the first pass, square edges, pops to dirt):
	// WIDEN: the footprint is W_FIRST x the tyre on fresh mud and grows to full width as the mud under the tyre
	// gets pressed (pass-start depth at the contact: THICK = first pass, 0 = pressed out).
	// SOFT_WALL: the press fades out over this share of the half width toward the side, and the side ridge rises
	// from the wall (sine) instead of standing straight up - a round shoulder, not a box.
	// DIRT_NOISE: a quad turns dirt once its corners changed more than TOUCH + up to DIRT_NOISE m (a fixed
	// random share per world quad), so the dirt edge is ragged, not a ruler line along the box.
	static const bool SOFT = true;
	static const float W_FIRST = 0.55;
	static const float SOFT_WALL = 0.35;
	static const float DIRT_NOISE = 0.04;
	static const float DRAG_TOUCH = 0.03;        // SOFT: texture drag shorter than this (m) does not make dirt
	// SMOOTH_EDGE (user 2026-10-15: dirt / grass edge steps along the 0.15 m quads): each vertex gets a dirt score q
	// (1 = on the edge, from TOUCH + noise, DRAG_TOUCH and Bare as DirtQuad); a quad with dirt and clean corners is
	// cut along q = 1 (per triangle, new vertices on its edges, shared with the next quad), so the edge is a smooth
	// line through the quads. Mesh only: the collider keeps the plain quads. Caps fall back to whole quads.
	static const bool SMOOTH_EDGE = true;
	static const int EDGE_VCAP = 1024;          // = tv size (441 grid + cuts + 320 skirt)
	static const int EDGE_ICAP = 2880;          // = tixTop size; tix has + 480 for the skirt
	static const float H0 = 0.05;               // ridge height at walking pace (m)
	static const float H_SPEED = 0.012;         // + per m/s
	static const float H_SINK = 0.8;            // + per m the tyre sits under the ground beside it
	static const float HMAX = 0.28;
	static const float DRAG = 0.5;              // texture drag per m of tyre travel (m)
	static const float DRAG_MAX = 0.3;          // most drag a vertex can build up (m); 0.6 until 2026-10-15 (stretched up to 6x over 0.15 m cells)
	// UV STRETCH (user 2026-10-15 'textures extremely distorted'): two neighbour vertices may differ in drag by at most
	// DRAG_LIM (m), so a 0.15 m quad stretches at most (CELL + DRAG_LIM) / CELL = 1.4x. 0 = no limit (old look).
	static const float DRAG_LIM = 0.06;
	// GRADUAL PRESS (user 2026-10-15: the sheet went to full depth on the first pass): each vehicle pass presses the mud
	// under the tyre PRESS_STEP thinner (THICK / PRESS_STEP passes to the hard ground) and the side ridges grow by the
	// same share of their full height. A cell counts a new pass when another vehicle touches it, or the same one after
	// PASS_GAP s (a 6x6 is one pass, not three). false = old behaviour (full press at once).
	static const bool GRADUAL = true;
	static const float PRESS_STEP = 0.05;
	static const float PASS_GAP = 4;
	static const float EPS = 0.004;             // thinner than this: no mud drawn
	static const float LIFT = 0.01;             // mud surface above ground + d
	static const float SINK = 0.06;             // a vertex without mud sits this far under the ground
	static const float REPOSE = 1.3;            // steepest mud slope that stays (rise per run); 0.9 until 2026-10-15 (HEAVY: user 'too soft looking')
	static const float SLUMP = 0.25;            // share of the excess that flows per Slump call (10 Hz since MUD ZONES; was 0.15 per frame); 0.4 until 2026-10-15 (HEAVY)
	// COST 2026-10-14 22:55 (driving: 5-6 ms/frame, ~3000 traces/s): only every 2nd cell each way is traced (0.3 m),
	// the cells between are filled from those; re-traces 40 -> 4 per frame (ground changes are rare: digs, craters)
	static const int NEW_TRACES = 150;          // first ground traces per frame (cells never traced)
	static const int OLD_TRACES = 4;            // re-traces per frame (round robin over the traced cells)
	static const float REGROUND_MAX = 100;      // m: biggest rut op box FloorChanged re-reads (synced sheet Stage 1)
	static const float MOVED = 0.08;           // ground under a mud cell moved more than this (m, trench / fill / crater): mud gone
	// COLLIDE (user 2026-10-15 "we just walk inside of it"): each mud tile gets a static trimesh on CharCollide
	// (0x80000) = solid for Character + CharacterAI only (gproj matrix, mud-gotchas 2026-10-13). NOT FireGeo: every
	// ground read of ours is a Projectile trace (GroundDown, Trace, MUD_Surface.Y ...) and would read the mud top as
	// ground. The wheel raycast (VehicleCast) does not hit CharCollide; the Wheel preset geoms MAY touch it (unproven).
	static const bool COLLIDE = false;           // PERF A/B 2026-10-15: off gave the same fps (38-48 driving). Synced sheet: off (look only)
	// CharCollide + Foliage: Foliage (0x40000) is the ExcludePhysicsLayer of CompartmentAccess_base.conf - without it
	// get in / out is "Obstructed" on the mud (user 2026-10-15; same fix as mud-gotchas 2026-10-08 10:50)
	static const int COLL_LAYER = 0x80000 | 0x40000;
	static const float COLL_S = 1.0;            // a live (window) tile rebuilds its collider at most once per this (s)
	static const ResourceName COLL_MAT = "{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat";   // = MUD_VoxelProbe.GAMEMAT
	static const float BLAST_CLEAR = 1.0;       // x crater crest radius with no mud left (was 0.8 + fade to 1.0)
	static const float JOB_DELAY = 0.1;        // s after a voxel go-live before the mud re-reads the ground there
	// BLAST NOW (user 2026-10-15: the sheet faded away tile by tile, ~0.3 s after the crater showed): the blast takes the
	// mud off in the go-live frame itself (no traces needed); the ground re-read follows as before. Around the crater
	// the re-read traces EVERY cell (not every 2nd) so the thin mud on the rim follows the voxel shape (seam).
	static const bool BLAST_NOW = true;
	// RIM BARE (user 2026-10-15: mud still seemed to go slowly + a step at the crater): the thin mud BlastKeep left on the
	// rim sat at the OLD ground height, hidden under the raised voxel rim, then popped up onto it ~0.35 s later in the
	// redo. true = no mud anywhere on the rim (out to BlastReach = the voxel rim's outer foot): one change, in the
	// go-live frame; the mud past the foot meets the old ground height there, so no step. false = thin mud on the rim.
	static const bool RIM_BARE = true;
	// MERGE (user 2026-10-15 'layered like a cake', docs/superpowers/research/2026-10-15-sheet-voxel-merge.md):
	// TAPER: the mud thins to the ground over this many m toward a blown-off / dug (Bare) cell, as EDGE at open tile
	// sides, instead of a ~0.2 m drop in one cell (0 = old step). Since 2026-10-15 17:00 (user: 'curve down, not a
	// straight layer edge') counted across tile borders (TaperField reads the 8 neighbour tiles, TAPER_PAD cells
	// deep) and shaped by TAPER_CURVE: a sine shoulder (steep at the bare edge, flat on top) instead of a straight ramp.
	static const float TAPER = 0.75;
	static const int TAPER_PAD = 6;             // cells read past each tile side (>= TAPER / CELL + 1)
	static const int TAPER_W = 33;              // = TV + 2 * TAPER_PAD
	static const bool TAPER_CURVE = true;
	// BUILD ONCE (user 2026-10-15: 'a little laggy when it explodes on a tunnel'; LOG one blast = sheet builds 37-45 ms
	// + DigNow 58-81 ms in two frames, mostly the same stored tiles twice): stored tiles changed by an op / DigNow go
	// into m_aOneBuild (once each). A 6 ms/frame budget looked like 'loading weirdly, not instant' (user 16:46) ->
	// all are built in one go: crater / rut ops in TakeOp, trench ops next Frame, sheet-and-voxel ops in FlushNow.
	// SYNC: an op that goes to the sheet AND the voxels (tunnel / zone edge) is held (m_aPend) and applied to the
	// sheet in the voxels' go-live (FlushNow), so both halves show in the same frame (user shot: half a crater).
	static const int PEND_MAX_MS = 1500;        // a held op whose voxels never go live is applied after this
	// PEND_STEP_MS (user 2026-10-15: a 60-op trench beside a tunnel froze ~0.7 s, LOG ApplyPending 285 ms in the
	// go-live frame): held ops are applied this many ms per frame (at least one); DigNow waits until all are in.
	static const int PEND_STEP_MS = 8;
	// DIG NOW (user 2026-10-15: trench mud 'loading away', not as instant as the voxels): in the go-live frame every
	// mud vertex in the change box reads the new ground from the voxel field (MUD_VoxelProbe.SurfaceAt, no traces);
	// moved more than MOVED -> mud off + new height, tile rebuilt now. The traced redo still follows and refines.
	// Only for non-crater ops (craters: BLAST_NOW). false = old way (mud goes ~0.1-0.2 s after the voxels).
	static const bool DIG_NOW = true;
	// (user 2026-10-15 'a little choppy': LOG a 72-op trench ran DigNow 72x, go-live frame 185 ms. Now the boxes are
	// collected and FlushNow does each vertex + tile ONCE per go-live.)
	// Since 2026-10-15 ~14:15 (user: crater edges 'tails load again', a floating tile): craters go through DigNow too
	// (every vertex in the box takes the field height, not only moved ones), BlastNow only changes data and FlushNow
	// builds each tile once, and the traced redo 0.1-0.3 s later is skipped (TRACE_REDO false) - one change, one frame.
	static const bool TRACE_REDO = false;
	static const float FIELD_EPS = 0.005;       // m: a smaller height change from the field is ignored
	// HOLE_SKIP: a quad touching a Bare cell whose corners differ more than this in height (m) is not drawn - the
	// mesh no longer stretches from the mud down into a dug hole / crater; the voxel wall shows there (0 = off).
	static const float HOLE_SKIP = 0.2;
	// ONE GRID proof (user 2026-10-15, plan docs/superpowers/plans/2026-10-15-one-grid-proof.md, checkpoint v072 =
	// before): in mud zones the sheet IS the ground. Once all zone tiles are built, the skin cells they fully cover go
	// VOX (our patch mesh + collider out); the sheet draws bare cells at ground height, its collider is the ground
	// (no mud) on the ground layer, and crater (0) / rut (4) ops in a zone change the sheet's ground (TakeOp), no
	// voxels. SP only (the sheet is client-only). false = voxel + sheet as before.
	static const bool ONE_GRID = false;         // synced sheet 2026-10-18: off - every op goes to voxels / rut field (one ground, the same on the DS)
	static const float ONE_DIP = 0.03;          // m: the tile side toward ground that is not ours dips this far under
	static const float ONE_FLOOR = 3;           // = MUD_VoxelProbe.FLOOR
	static const float ONE_FUNNEL = 0.5;        // = MUD_VoxelProbe.FUNNEL
	static const float ONE_RUT_R = 0.5;         // = MUD_VoxelProbe.RUT_R
	static const float ONE_RUT_ON = 0.6;        // = MUD_VoxelProbe.RUT_ON
	// ONE_GRID ground rut shape (SOFT): cosine profile (0 at the edge, no step) and radius ONE_W0 x ONE_RUT_R on
	// undug ground, full radius once the rut is m_fB deep (wheel height against the undug ground).
	static const float ONE_W0 = 0.5;
	// VOXEL HOLES (user 2026-10-15: tunnels broken under ONE_GRID - the sheet followed the dug ground into the tunnel
	// mouth and its ground collider plugged it): a vertex whose ground a voxel op (tunnel, trench, shovel, fill) moved
	// more than MOVED gets m_aBare 2 = voxel owned. Quads touching one draw nothing and collide nothing: the voxel
	// bricks (walls, tunnel, their collider) show through. Its ground height stays (the sheet edge stays on top).
	static const int BARE_VOX = 2;
	static const float IN_VOX = 0.3;         // TakeOp: a crater hit this far under the sheet ground = in a tunnel (voxels)
	// SKIRT (DS test 2026-10-15: sky seen through 3 m tile borders - the live tile near the truck and its stored
	// neighbour disagree on the edge height, nothing is under the sheet any more): every tile hangs a dirt curtain
	// this deep under its border, facing out. Render only (the collider is the ground, the same on both tiles).
	static const float ONE_SKIRT = 0.45;
	// SKIRT_ALL (2026-10-18, lazy A/B DS 13:38: see-through gaps only with the patch meshes hidden): the ONE_SKIRT curtain
	// also when ONE_GRID is off (it is off since the synced sheet) - in the zone the sheet is the only drawn ground again
	static const bool SKIRT_ALL = true;
	static const int BUILD_MS = 8;              // zone tiles built per frame at load: until this many ms are used (~0.8 ms a tile)
	// CAMERA RADIUS (synced sheet Stage 3, user 2026-10-18: 40 m; v182 = before): a zone tile has a mesh only while its
	// middle is within RADIUS m (x z) of the camera - built on approach, nearest first, RAD_BUILD_MS a frame; past
	// RADIUS + RADIUS_HYST the mesh goes. The mud itself lives in the sync layer (touched tiles only); a dropped tile
	// keeps its own data only when it has bare cells (craters, digs: Stage 4 clears those from ops). No pre-build of
	// every zone tile at load. ONE_GRID needs every tile built: it keeps the old pre-build.
	static const bool RADIUS_ON = true;
	static const float RADIUS = 40;
	static const float RADIUS_HYST = 6;         // m past RADIUS before a mesh goes (no build / drop flicker at the edge)
	static const float RADIUS_EVERY = 0.25;     // s between radius checks
	// sheet perf fix 5 MESH KEEP (2026-10-18, WB 11:00 circle drive: 190-300 meshes dropped per 10 s and the same tiles
	// built again, ~750 builds per 10 s at ~2.5 ms; v189 = before): a built mesh stays until more than MESH_CAP stored
	// meshes exist; then the farthest past RADIUS + RADIUS_HYST go first. 0 = drop every mesh past the radius, as before.
	// The test zone has 462 tiles, so nothing drops there.
	static const int MESH_CAP = 600;
	// sheet perf fix 3 (2026-10-18; v188 = before, was 4): 2 ms a frame - fewer approach builds per frame while driving
	static const int RAD_BUILD_MS = 2;          // ms of tile builds per frame on approach (at least one; ~2.9 ms a tile, DS 14:31)
	// BUILD AHEAD (2026-10-18, research/2026-10-18-all-offroad-sheet-deep-dive.md fix 2): QueueNear orders by distance to
	// a point AHEAD_S s ahead of the camera (max AHEAD_MAX m) instead of square rings round the camera
	static const float AHEAD_S = 1.5;
	static const float AHEAD_MAX = 20;
	// SPEED RADIUS (fix 4): build radius = SPEED_RADIUS_K / speed (m/s), clamped RADIUS_MIN..RADIUS. Demand measured
	// ~5.65 x speed tiles/s at 40 m; one build a frame ~50 / s -> K 320 (8 m/s still 40 m, 16 m/s 20 m). Built meshes still
	// stay to RADIUS + RADIUS_HYST (DropFar unchanged). false = RADIUS always.
	// SPARSE SHEET (option A + A2, user 2026-10-18 'do A with A2', research/2026-10-18-fixes-2-4-and-brainstorm.md sec 5):
	// radius tiles are built only where the sync layer has mud data (MUD_MudSync.AnyTileNear = touched ground); untouched
	// covered ground shows the ground patches, their MESH drawn PatchLift() higher (MUD_WorldGroundEntity, A2; colliders
	// unchanged) so it meets the untouched sheet top. Patches over a sync tile stay unlifted (the sheet draws there).
	// Needs SYNC + ALL_OFFROAD. false = every covered tile gets a sheet tile (H3, v202 = before).
	static const bool SPARSE = false;   // WHOLE-MAP TEST (every offroad tile in RADIUS) - set back to true before commit
	static const float PATCH_GAP = 0.03;        // m a lifted patch stays under the untouched sheet top (no z-fight)
	// LIFT BY DRAWN (step fix, user 2026-10-18 'tiles pop in, height difference driven / not driven'): a patch is drawn
	// unlifted only under a tile whose sheet mesh is ON SCREEN (live slot or stored, flag m_iUnlift / m_aSlotUnlift),
	// told after the mesh is up; a dropped stored mesh stays visible until it is deleted (1 s) so the patch re-lifts
	// under it first. Lift = untouched sheet top - LIFT_UNDER (was THICK - PATCH_GAP = 4 cm under it). LiftAt: a vertex
	// drops only when every tile within one patch cell of it is drawn (no slope outside the sheet edge).
	// false = A2 as v204 (unlift over sync tiles + the whole live window)
	static const bool LIFT_BY_DRAWN = true;
	static const float LIFT_UNDER = 0.01;
	// TEMP LOOK LIFT TEST (user 2026-10-19, research/2026-10-19-sheet-redesign.md sec 6, v242 = before): clients draw
	// the coarse skin hole cells and every ground patch mesh LOOK_LIFT_M higher (mesh only, colliders untouched; works
	// with the sheet off). Skin: 0 at any vertex touching kept terrain or a voxel cell; patch: times its lock. Look at
	// grass, holed props, road / building edges, patch-skin seam, crater rim, a prone soldier. Remove after the test.
	static const bool LOOK_LIFT_TEST = false;   // off 2026-10-19: grass covers every lifted rut (research/2026-10-19-ruts-invisible.md)
	static const float LOOK_LIFT_M = 0.16;
	static const bool SPEED_RADIUS = true;
	static const float SPEED_RADIUS_K = 320;
	static const float RADIUS_MIN = 20;
	static const float LOG10_S = 10;            // TEST_KIT: s between 'MUD sheet:' Stage 3 lines
	// LAZY RUT MESH (Stage 3, user 2026-10-18 'hide it under the sheet, colliders stay immediate'): where the sheet
	// draws over a whole ground patch (Covers), MUD_WorldGroundEntity holds that patch's mesh rebuilds and hides its
	// stale mesh (an old mesh is higher than the pressed sheet and would poke through); it shows + rebuilds it once
	// the sheet no longer covers it. Colliders rebuild as before. false = patch meshes as before.
	static const bool LAZY_RUT_MESH = true;
	// TEMP LAZY A/B (2026-10-18, user: see-through gaps at the rut sides while driving): LAZY_AB_S > 0 = lazy rut mesh on
	// and off in turns of this many s (a hint says which). Off = the patch meshes under the sheet show + rebuild again:
	// gaps gone when off = the sheet itself leaves the holes. 0 = LAZY_RUT_MESH as set.
	static const float LAZY_AB_S = 0;     // A/B DS 13:38 done: gaps only with lazy ON -> skirts + gap probe
	// EASE DROP (2026-10-18, user: 'it loads and then sinks down', hide it): when a rut op's floor cut lowers the ground
	// under a live window cell by more than EASE_MIN, the drawn ground slides down over EASE_S s (smoothstep) instead
	// of jumping. Look only (COLLIDE false, the sync layer never reads m_aG). Stored tiles still jump. 0 = off.
	static const float EASE_S = 1.0;
	static const float EASE_MIN = 0.01;
	// RE-GROUND EXACT (2026-10-18): FloorChanged re-reads only sheet cells within REGROUND_R m of a changed floor sample
	// (the floor is read between samples 0.25 m apart). false = the whole op box as before.
	static const bool REGROUND_EXACT = true;
	static const float REGROUND_R = 0.26;
	protected int m_iReCells10;                 // log: cells re-read / ops in the 10 s 'MUD sheet gaps' line
	protected int m_iReOps10;
	protected int m_iLookCuts10;                // H2: look rut cells re-read / LookChanged calls (same line)
	protected int m_iLookCells10;
	protected static bool s_bLazyOn = true;
	protected static int s_iLazyPhase = -1;
	// SHEET LOCAL + DEPOSIT BAND (MUD LAB stage 1 part B, deep dive research/2026-10-20-mud-sheet-deep-dive.md sec 3):
	// mud thickness = band(distance to the nearest cell a stamp wrote); band 1 within BAND_IN, a smooth fall to 0 at
	// BAND_IN + BAND_FADE; at band 0 the surface sits BAND_UNDER under the ground (no step at the band edge). Tiles are
	// born on the first stamp that reaches them (with the band margin); untouched ground has no tile.
	static float BAND_IN = 0.4;                 // m (MUD_MudLab presets set it)
	static float BAND_FADE = 0.8;               // m
	static const float BAND_UNDER = 0.01;       // m the surface sits under the ground at band 0
	static const int BAND_W = 49;               // TV + 2 x the biggest margin (ceil(2.0 / CELL) = 14)
	static bool TOP_UV64 = true;                // sheet local: the MAT_TOP section uses the 64 m skin chunk UV frame
	static const float TOP_UV_M = 64;           // = MUD_VoxelProbe.TOP_TILE (grass top UV span)
	static const int LAB_PRESETS = 4;           // BAND_IN 0.4 / 0.8 x BAND_FADE 0.8 / 1.2
	// MUD LAB part C cost levers (deep dive research/2026-10-20-mud-sheet-deep-dive.md sec 2): each its own switch,
	// default off. TEST_KIT numpad 0 while sheet local runs (MUD_LabSheetLocal.Tick) cycles the sets
	// none -> all -> L1 -> L2 -> L3 -> L6 -> L8 -> none; sheet local off (or a new play) = none.
	static bool LEV_L1_CARRY = false;           // L1: slot dirty flags move with their tiles in Follow (no blanket dirty)
	static bool LEV_L2_DRAG = false;            // L2: LimitDrag once per upload over the stamped cells + 1
	static bool LEV_L3_SLUMP = false;           // L3: slump only changed cells + 1 ring, one pass at L3_FACTOR
	static bool LEV_L6_EASE = false;            // L6: EaseTick at upload rate, not every frame
	static bool LEV_L8_VEH = false;             // L8: near vehicles only, cached sims, map by vehicle, pruned
	static const int LEVER_SETS = 6;
	static const float L3_FACTOR = 0.6;
	static const float VEH_NEAR_M = 10;         // L8: a vehicle origin this far outside the window still stamps
	static const float VEH_PRUNE_S = 5;         // L8: dead / far vehicle entries go this often ...
	static const float VEH_PRUNE_M = 50;        // ... when this far outside the near box
	protected static int s_iLeverSet;
	protected bool m_bSlotsKeep;                // L1: UpdateSlots leaves the dirty flags (Follow carried them)
	protected ref MUD_MudSheetCells m_Drag = new MUD_MudSheetCells();     // L2: cells stamps wrote since the last upload
	protected ref MUD_MudSheetCells m_Chg = new MUD_MudSheetCells();      // L3: cells changed since the last slump
	protected int m_iSlumpGen;                  // L3: work set mark (m_Chg.m_aGen)
	protected float m_fNextEase;                // L6
	protected ref map<Vehicle, ref MUD_MudSheetVeh> m_mVeh = new map<Vehicle, ref MUD_MudSheetVeh>();   // L8
	protected ref array<Vehicle> m_aNearVeh = {};                        // L8: this frame's near vehicles ...
	protected ref array<VehicleWheeledSimulation> m_aNearSim = {};       // ... and their sims
	protected float m_fVehPrune;
	// MUD LAB part D (SHEET LOCAL only): far data + mesh radius
	static const int TILE_DATA_CAP = 20000;     // data tiles kept (full stores + compact); the oldest compact ones go first
	static const float MESH_R = 70;             // meshes only for tiles whose middle is within this of the camera
	static const float MESH_DROP = 10;          // a mesh past MESH_R + this is deleted at once, its data compacted
	static const int MESH_MS = 2;               // ms of radius builds per frame (at least one tile)
	static const float FAR_R = 40;              // a stored tile built past this is built at 11 x 11, never upgraded
	static const float FAR_H = 0.3;             // compact mud height range 0..FAR_H m (1 byte)
	protected ref map<int, ref MUD_MudSheetFar> m_mFar = new map<int, ref MUD_MudSheetFar>();
	protected ref array<int> m_aFarOrder = {};  // compact keys, oldest first
	protected MUD_MudSheetFar m_TchFar;         // TouchStore's cache: the compact tile of m_iTchKey
	protected int m_iFarIn;                     // 10 s: tiles compacted / expanded / dropped by the cap / far-ring builds
	protected int m_iFarOut;
	protected int m_iFarCap;
	protected int m_iFarLow;
	// MUD LAB part E (SHEET LOCAL only): remote trucks from the rut ops ("sync-lite", MUD_MudSync.RutOp -> OpStamps)
	static const float OP_SPEED = 5;            // m/s: an op has no speed with the op tag off
	static const float OP_RADIUS = 0.5;         // m: wheel radius for TyreHalf (no tyre in the op)
	static const int OP_VEH = -1;               // GRADUAL pass vehicle of op stamps (own vehicles are >= 1)
	static const float OWN_S = 20;              // s an own stamp covers op segments (the op's round trip, generous)
	static const float OWN_PRUNE_S = 5;
	static const float OP_EDGE_M = 1.5;         // m: a segment this far inside the window goes through Stamp
	protected ref map<int, float> m_mOwn = new map<int, float>();     // own stamp cells (OwnKey) -> time
	protected float m_fOwnPrune;
	protected int m_iOpKey = -1;                // OpTile's one-tile cache
	protected bool m_bOpMake;
	protected MUD_MudSheetTileData m_OpD;
	protected ref array<int> m_aOpShown = {};   // shown stored tiles this op changed
	protected int m_iOpStamps;                  // 10 s
	protected int m_iOpSkipOwn;
	protected int m_iOpMs;
	protected int m_iOpClears;
	// MUD LAB EDGE AUDIT (sheet local meter, user 2026-10-20 DS screenshot: a striped wall along straight tile borders
	// = the ONE_SKIRT curtain of a raised tile with nothing drawn beside it): each build keeps its border verts, the 10 s
	// line says what is beside every raised one (ok / lower neighbour / no neighbour and why)
	static const bool EDGE_AUDIT = true;
	static const float EDGE_RAISED = 0.03;      // m over the ground: a border vert this high shows its skirt if open
	static const float EDGE_STEP = 0.03;        // m: drawn neighbour this much lower = a visible step
	protected ref map<int, ref array<float>> m_mEdgeRec = new map<int, ref array<float>>();   // TrailKey -> 12 x TV
	protected static int s_iLabPreset;
	// MUD LAB STAGE 2 (prompt prompts/2026-10-20-local-mud-lab-stage2.md, research/2026-10-20-sheet-local-edges-craters.md;
	// checkpoint v255 = before): sheet local fixes, each its own switch, all default off. TEST_KIT numpad * (KC_MULTIPLY)
	// while sheet local runs cycles the fix sets (FixApply); sheet local off (or a new play) = none.
	// part A: tile-edge wall
	static bool EDGE_COBUILD_PICK = false;      // A2: dirty band / stamp slots beside a picked wheel slot build with it
	static bool EDGE_COBUILD = false;           // A3: a build whose border differs from the tile beside builds that one too
	static bool SKIRT_APRON = false;            // A4: skirt bottom verts go out ~45 deg down to under the ground, own UVs
	static bool EDGE_OPEN_FADE = false;         // A8: a side whose neighbour is not drawn (and not built this upload) fades over EDGE
	static bool EDGE_DRAPE = false;             // A7 (user idea 2026-10-20): skirt curves over and down as a quarter round (wins over A4)
	static bool BAND_BORN_FRONT = false;        // A5: a tile born / re-banded outside the window builds before the radius list
	static const float EDGE_CO_STEP = 0.03;     // A3: m between our border vert and the neighbour's = co-build (EDGE_STEP)
	static const int CO_MAX = 4;                // A3: stored neighbours built at once per upload; the rest are queued
	// part B: crater delay (O1a / O1b / O1c of the research)
	static bool CRATER_TRACE_NOW = false;       // B2: a now-slot not traced yet is traced / filled and built in the go-live
	static bool CRATER_TAPER_NOW = false;       // B3: the hole's taper tiles build in the go-live frame (cap below)
	static bool CRATER_EASE_DROP = false;       // B4: a dig drops the running ground ease of every vertex it writes
	static const int TAPER_NOW_MAX = 12;        // B3: at most this many taper tiles ...
	static const int TAPER_NOW_MS = 15;         // ... and this many ms in the go-live frame
	// part C: cost
	static bool BAND_INCR = false;              // C1: band from a kept distance field, updated only round new touched cells
	static bool BAND_MICRO = false;             // C2: BandTile reads the field directly, no Math.Min / Math.Cos
	static bool BUILD_SKIP_SAME = false;        // C3: a rebuild that moves no vert 3 mm (classes same) keeps the old mesh
	static bool UV64_FAST = false;              // C4: TOP_UV64 UVs into their own array (no x1e6 round trip via cix)
	static bool BAND_STORED_HOLD = false;       // C5: band-only stored rebuilds obey SLOT_MIN_S
	static const int LUT_N = 514;               // C1 / C2: band by distance, 32 steps a cell over 16 cells (+2)
	protected static float s_aBandLut[514];
	protected static bool s_bLutOk;
	protected static int s_iDistGen;            // C1: a new value = every kept distance is stale (fix set, preset)
	protected int m_iDistGen = -1;
	protected ref MUD_MudSheetDist m_DistF;     // C1: window distances (Follow swaps them with m_DistT)
	protected ref MUD_MudSheetDist m_DistT;
	protected int m_aSlotDist[16];              // C1: 1 = the slot's window distances are current
	protected ref array<int> m_aNewGi = {};     // C1: world cells touched since the last BandIncr
	protected ref array<int> m_aNewGk = {};
	protected ref array<int> m_aNewV = {};      // C1: scratch, their window index
	protected bool m_bIncrMark;                 // C1: BandMark from a window stamp - live slots only get born
	protected bool m_bBenchDone;                // C1: the TEST_KIT bench ran (once per instance)
	protected int m_iIncrRuns;                  // C1 meters (10 s): BandIncr passes, box cells, slots dirtied, BandTiles
	protected int m_iIncrCells;
	protected int m_iIncrDirty;
	protected int m_iIncrFull;
	protected int m_aSlotSig1[16];              // C3: class signature of the live slot's last mesh (0 = unknown)
	protected int m_aSlotSig2[16];
	protected bool m_bSkipSame;                 // C3: BuildTileMesh kept the old mesh (m_SkipRes)
	protected ref Resource m_SkipRes;
	protected int m_iLowWas;                    // C3: d.m_iLow before this build
	protected int m_iSkipSame;                  // C3 meter (10 s): builds that kept the old mesh
	protected ref map<int, float> m_mStoredAt = new map<int, float>();   // C5: stored tile key -> its last build time
	protected int m_iBandHeld;                  // C5 meter (10 s): band-only stored builds put off
	static const int FIX_SETS = 10;
	protected static int s_iFixSet;
	// part A meters (10 s, LocalLog) and state
	protected int m_iCoPick;                    // A2: slots picked beside a wheel slot
	protected int m_iCoLive;                    // A3: live neighbours built in the same upload / next upload
	protected int m_iCoStored;                  // A3: stored neighbours built now
	protected int m_iCoCapped;                  // A3: stored neighbours queued (over CO_MAX)
	protected int m_iCoStoredUp;                // A3: stored co-builds since the last upload (CO_MAX)
	protected int m_iBornFront;                 // A5: front builds
	protected bool m_bCoBuilding;               // A3: a co-build runs (it triggers no co-builds: no ping-pong)
	protected bool m_bInUpload;                 // A3: Upload's slot loop runs (live neighbours join it)
	protected bool m_bCoAny;                    // A3: some slot got m_aCoLive in this upload
	protected int m_aCoLive[16];                // A3: 1 = build in this upload (co-build)
	protected int m_aCoNext[16];                // A3: 1 = build at the next upload (co-build)
	protected int m_aWheelSlot[16];             // A2: 1 = a wheel contact lies in the slot (this upload)
	protected ref array<int> m_aFrontTx = {};   // A5: tiles built before the radius list
	protected ref array<int> m_aFrontTz = {};
	protected int m_iSkirtFrom;                 // A4: first skirt vert of the mesh being built
	protected int m_iSkirtTix;                  // A7: first skirt index (tix) of the mesh being built
	protected int m_iOpenFade;                  // A8 meter (10 s): tile sides faded toward a neighbour not drawn
	protected int m_iDrapeCut;                  // A7 meter (10 s): builds whose drape stopped at the vert / index cap
	// builds by reason (MUD LAB part B step 3, every 10 s in Log10): what a BuildTile call was for
	static const int BW_WHEEL = 0;
	static const int BW_OLDEST = 1;
	static const int BW_NEAREST = 2;
	static const int BW_MESHLESS = 3;
	static const int BW_FREEZE = 4;
	static const int BW_RESTORE = 5;
	static const int BW_SLUMP = 6;
	static const int BW_BAND = 7;
	static const int BW_FIT = 8;                // picked because no more than UPLOAD_SLOTS slots were dirty
	static const int BW_HOLE = 9;               // crater / dig: built in the blast frame (DoDigNow)
	static const int BW_OP = 10;                // MUD LAB part E: a remote truck's rut op (OpStamps)
	static const int BW_COBUILD = 11;           // MUD LAB stage 2 part A: built beside a tile (EDGE_COBUILD_PICK / EDGE_COBUILD)
	static const int BW_N = 12;
	// slot dirt sources (m_aSlotWhy bits): which marks made a live slot dirty since its last build
	static const int WHY_STAMP = 1;
	static const int WHY_SLUMP = 2;
	static const int WHY_BAND = 4;
	static const int WHY_OTHER = 8;
	protected static int s_iSlumpMsAll;         // running totals (MUD_MudLab 10 s deltas)
	protected static int s_iBandMsAll;
	protected bool m_bLocal;                    // this instance runs sheet local (ENABLED and not SYNC at its start)
	protected ref map<int, int> m_mBorn = new map<int, int>();       // TrailKey -> 1: tile born (a stamp reached it)
	protected ref array<int> m_aBandQ = {};     // stored tiles whose band must be computed again
	protected ref map<int, int> m_mQWhy = new map<int, int>();       // QueueBuild key -> BW_* (none = restore)
	protected int m_aSlotBand[16];              // 1 = live slot's band must be computed again
	protected int m_aSlotWhy[16];               // WHY_* bits since the slot's last build
	protected int m_aPickWhy[16];               // BW_* of this upload's pick
	protected int m_aSlotNow[16];               // 1 = hole in this slot: build it in the blast frame (DoDigNow)
	protected int m_iMarkWhy = 8;               // WHY_* MarkRect adds (WHY_OTHER unless a caller sets it)
	protected int m_iBuildWhy = 5;              // BW_* the next BuildTile counts (BW_RESTORE unless a caller sets it)
	protected int m_aBuildsBy[12];              // builds by reason, this 10 s
	protected bool m_bMarkNow;                  // MarkRect also flags m_aSlotNow (hole marks in a go-live)
	protected bool m_bQueueFront;               // QueueBuild puts the key at the FRONT of m_aOneBuild (DigNow taper neighbours)
	protected int m_iWouldSkip;                 // builds whose drawn heights changed less than WOULD_SKIP_M
	static const float WOULD_SKIP_M = 0.003;
	protected int m_iTouchCells;                // cells touched (all time)
	protected bool m_bTouchNew;                 // the stamp running now touched a new cell
	protected int m_iBornWait;                  // log: uploads a born slot waited for its traces (10 s)
	protected int m_iTchKey = -1;               // TouchAt cache
	protected MUD_MudSheetStore m_TchSt;
	static const float UPLOAD_EVERY = 0.1;      // s between mesh rebuilds (10 Hz, as the KB mud tile)
	// S1 perf 2026-10-18 (research/2026-10-18-mud-zone-rut-perf.md; WB 10:24 run: ~67 live slot builds/s x 1.5 ms =
	// most of the sheet's 'upload' 600 ms per 5 s): at most UPLOAD_SLOTS mud slots rebuild per upload - the ones
	// nearest a vehicle, plus the longest-waiting one; the rest stay dirty for the next upload (look only). A slot
	// without a mesh (just entered the window) always builds. 0 = every dirty slot, as before (v187).
	static const int UPLOAD_SLOTS = 3;
	// WHEEL TILES FIRST (DS 2026-10-18 12:15, research/2026-10-18-sheet-alternatives-and-perf.md sec 13: own stamp -> drawn
	// tile max 0.9-9 s, user screenshot = no mud under the front wheel, then it pops in): every dirty zone slot with a wheel
	// contact in it builds at each upload, on top of UPLOAD_SLOTS (at most this many; 0 = off = before, v193)
	static const int WHEEL_SLOTS_MAX = 6;
	// D1 2026-10-18 (A2 DS 15:26: upload 1.2-2.0 ms a frame, wheel slots rebuilt at every 0.1 s upload): a zone slot that
	// already has a mesh rebuilds at most every SLOT_MIN_S (wheel slots too); new slots always build. 0 = off (v203)
	static const float SLOT_MIN_S = 0.25;
	// D1 fix (DS 15:37, user: 'a wave behind us'): with wheel slots held too, own wheel stamp -> drawn tile went 80 -> 250 ms
	// avg (0.4 -> 1-2.5 m behind the wheel, ruts came in 0.25 s chunks). true = a slot with a wheel contact skips the hold
	static const bool SLOT_MIN_WHEEL_FREE = true;
	// sheet perf fix 2 (2026-10-18, MUD_Prof WB 10:48: 'sh stored' worst 13-32 ms in one frame): stored tile rebuilds
	// (sync restore, trench / fill) stop once STORED_MS is used in a frame (at least one build); the rest wait for the
	// next frame. 0 = all at once, as before (v188).
	static const int STORED_MS = 3;
	// sheet cost A5 (2026-10-19, prompts/2026-10-19-local-sheet-cost-followup-v2.md Part A; v239 = before): a stored tile
	// rebuild (FlushOneBuild) waits while the tile was not drawn last frame (IEntity.IsRendered, SET_RENDERED set at
	// spawn), as MUD_WorldGroundEntity.RenderWait does for patches. Only tiles with a mesh that are shown; always built
	// within STORED_GATE_NEAR m of the camera. Live window slots never wait (they are not built here). false = off.
	static const bool STORED_RENDERED_GATE = false;
	static const float STORED_GATE_NEAR = 15;
	// FREEZE TEST (TEMP 2026-10-15): mesh rebuilds (Upload, incl. colliders) paused every other FREEZE_CYCLE s; all
	// script work still runs and the tiles stay drawn. PERF line splits fps live vs frozen: frozen much higher =
	// rebuild cost, same = cost of drawing the mud tiles (DYNAMIC_VERTEXES is no way out, mud-gotchas 2026-10-15)
	static const bool FREEZE_TEST = false;
	static const float FREEZE_CYCLE = 10;
	static const bool DEBUG_SHOW = false;        // TEMP 2026-10-14 control: proven brick material (MAT_DIRT), sheet flat 3 m over the vehicle origin
	static const bool DEBUG_TELEPORT = false;    // TEMP 2026-10-14: after the 3rd upload, freeze the sheet and put the player in its middle
	// TERRAIN LOOK (user 2026-10-15, as the voxel tops): quads whose mud nobody changed draw with MAT_TOP (terrain
	// material); pressed / pushed / dragged / blasted quads with MAT (dirt). Two sections, two emats (same emat on two
	// sections crashes - mud-gotchas 2026-10-14 G2).
	static const bool TOP_LOOK = true;
	// D2 2026-10-18: the TEMP GAP PROBE costs an array + a map entry per tile build; off unless gaps are hunted again
	static const bool GAP_PROBE = false;
	// PAPER TEST 1 (2026-10-18, research/2026-10-18-mud-mesh-cost-research-findings.md): BuildTileMesh fast paths, same
	// mesh (untouched cells skip the dirt score maths, a tile with no open side skips the edge fade, bare flags read once
	// per vertex). s_bFastBuild switches them at run time; the TEST KIT bench A/Bs old vs fast on one live tile
	protected static bool s_bFastBuild = true;
	static const float BENCH_EVERY_S = 30;      // TEST KIT bench: BENCH_N old + BENCH_N fast builds of one live tile; 0 = off
	static const int BENCH_N = 16;
	protected float m_fBenchAt = -1;
	protected int m_iBenchOldMs;
	protected int m_iBenchFastMs;
	protected int m_iBenchRuns;
	// PAPER TEST 2: a stored tile further than LOD_M from the camera builds at 2 x CELL (0.3 m, no cuts); Radius queues a
	// full rebuild once the camera is within LOD_M - LOD_BACK. false = off (v204)
	static const bool LOD_ON = true;
	static const float LOD_M = 20;
	static const float LOD_BACK = 3;
	protected vector m_vLodCam;
	protected bool m_bLodCam;
	protected int m_iLodLow;                    // log: low builds per 10 s
	protected int m_iLodUp;                     // log: low tiles queued for a full rebuild per 10 s
	protected int m_iLiftTold;                  // log: LIFT BY DRAWN patch tells per 10 s
	protected int m_iLowNow;                    // low tiles stored now (LodUpgrade skips its loop at 0; recounted every 10 s)
	static const ResourceName MAT_TOP = "{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat";   // = MUD_VoxelProbe.MAT_TOP
	static const float TOUCH = 0.01;            // m off THICK (or any texture drag, or blasted) = a dirt quad
	static const float THICK = 0.15;            // soft dirt layer on the hard ground (m)
	// MUD DRAG TEST (2026-10-14, SP only; shape from MudRunner sub_140930110, research/2026-10-14-mudrunner-ida.md):
	// the wheels stand on the hard ground, so they already look sunk THICK; the mud only adds drag by submerged area
	// (front: tyre width x depth, side: pi x radius x depth), speed above 1 m/s counted as v^0.75, and less engine.
	static const bool DRAG_TEST = false;         // PERF test 2026-10-15: off gave the same fps in mud (39-49). Synced sheet: off (no forces)
	static const float SLOW = 0.4;              // m/s2 slowing per (m/s)^0.75 with all wheels in fresh THICK mud
	static const float SLOW_MAX = 4;            // most slowing the mud can give the whole vehicle (m/s2)
	static const float DRAG_MIN_DEPTH = 0.025;  // MudRunner: no mud force below this depth
	static const float DRAG_LOOK = 0.3;         // depth read this far ahead of the contact (fresh mud, not own rut)
	static const float EFF_LOSS = 0.35;         // gearbox efficiency x (1 - EFF_LOSS x mud share of the wheels)
	static const float EDGE = 1.0;              // m at an open tile side where the layer fades into the ground (was 1.5)

	protected static ref MUD_MudSheet s_Inst;
	// OFF TEST (TEMP 2026-10-15): no mud sheet at all (no tiles, ruts, drag, colliders), only an fps line every 5 s
	// ('MUD mud sheet PERF OFF') - control run on the same loop as the FREEZE test (docs/superpowers/results/2026-10-15-freeze-test.md)
	static const bool OFF_TEST = false;
	protected static float s_fOffT = -1;
	protected static float s_fOffPrev = -1;
	protected static int s_iOffN;
	protected static float s_fOffDtMax;
	protected static float s_fOffSpeed;

	protected ref MUD_MudSheetTileData m_TD = new MUD_MudSheetTileData();     // scratch: one live tile's data
	protected ref map<int, ref MUD_MudSheetStore> m_mTrail = new map<int, ref MUD_MudSheetStore>();
	protected ref array<int> m_aTrailOrder = {};                         // trail keys, oldest first
	protected ref array<ref MUD_MudSheetStore> m_aDying = {};            // dropped trail tiles: hidden, deleted 1 s later
	protected int m_iBuildQuads;
	protected ref TraceParam m_OurTrace;
	// colliders: parallel lists, one entry per tile entity with a body (geom destroyed after the body, mud-gotchas 12:29)
	protected ref array<IEntity> m_aCollEnt = {};
	protected ref array<PhysicsGeom> m_aCollGeom = {};
	protected ref array<float> m_aCollAt = {};          // time of the last build
	protected ref array<int> m_aCollPend = {};          // 1 = mesh changed since, rebuild when COLL_S is up
	protected float m_fCollCheck;
	protected float m_fStatPrevT = -1;                  // PERF A/B 2026-10-15: real frame times for the log line
	protected float m_fStatDtMax;
	protected float m_fStatSpeed;                       // highest vehicle speed in the window (m/s)
	protected float m_fFrzT;                            // FREEZE TEST: seconds and frames frozen / live in the window
	protected int m_iFrzN;
	protected float m_fLiveT;
	protected int m_iLiveN;
	protected static bool s_bTraceHidden;             // MUD_VehicleAccess: colliders not traceable (get in / out)
	protected int m_iCollBuilt;                         // since the last log line
	protected int m_iCollMs;
	protected int m_iCollHeld;
	// int, not bool: a member 'bool x[16]' read back true for slots outside every zone (DIAG 2026-10-15 00:23,
	// window at x 1044 "16 of 16 in a zone", zones end x 1029; int copy 8, bool copy 16) - mud-gotchas 2026-10-15
	protected int m_aSlotZone[16];              // 1 = live window slot is a zone tile (TILES x TILES)
	protected int m_aSlotDirty[16];             // 1 = live window slot needs a new mesh
	protected int m_aSlotReady[16];             // LAZY RUT MESH: 1 = live slot's mesh is up with every cell traced (Covers)
	protected int m_aSlotWait[16];              // S1: uploads this dirty slot was passed over (longest-waiting gets one build)
	protected int m_iUpSkipped;                 // S1 log: slot builds put off to a later upload, per 5 s
	protected int m_aPick[16];                  // S1: 1 = Upload builds this dirty slot now (UploadPick); 2 = D1 held
	protected int m_aSlotUnlift[16];            // LIFT BY DRAWN: 1 = patches were told this slot's mesh is on screen
	protected int m_aSlotBuiltMs[16];           // D1: m_fLast (ms) of the slot's last build (int: member float arrays untested)
	protected int m_iUpThrottled;               // D1 log: dirty slot passes held by SLOT_MIN_S, per 10 s
	protected static int s_iCoverGen;           // LAZY RUT MESH: +1 whenever sheet cover may have shrunk (CoverGen)
	protected static ref array<int> s_aCoverNo = {0, 0, 0, 0, 0, 0}; // R5 log (CoverWhy): Covers answers - 0 yes, 1 too big, 2 zone edge, 3 bare near,
	                                            // 4 live slot not ready, 5 stored tile without its mesh
	protected bool m_bZonesQueued;
	protected ref array<int> m_aBuildTx = {};   // zone tiles still to build at load
	protected ref array<int> m_aBuildTz = {};
	protected int m_iZoneTiles;
	protected ref array<MUD_MudZoneEntity> m_aZones = {};
	protected float m_fNextRad;                 // CAMERA RADIUS: time of the next check
	protected vector m_vRadPrev;                // SPEED RADIUS: camera at the last check
	protected float m_fRadPrevT;                // SPEED RADIUS: time of the last check (0 = none yet)
	protected vector m_vRadVel;                 // SPEED RADIUS: smoothed camera velocity x z (m/s)
	protected float m_fRadBuild;                // SPEED RADIUS: build radius now (0 = RADIUS)
	protected int m_iRadQueued;                 // log: build list size at the last radius check
	// TAPER trim (Stage 3): tiles a voxel change may have bared (TrailKey -> 1, never cleared); TaperField skips a tile
	// with none of these round it. m_bBareAll: a change box too big to mark - every tile tapers (old way)
	protected ref map<int, int> m_mBareTiles = new map<int, int>();
	protected bool m_bBareAll;
	// Stage 3 log (TEST_KIT, LOG10_S): tile builds, their ms, own stamps replayed, meshes dropped
	protected float m_fSt10T = -1;
	protected int m_iSt10Builds;
	protected int m_iSt10BuildMs;
	protected int m_iSt10BuildMax;
	// TEMP GAP PROBE (2026-10-18): live builds that left quads undrawn for an untraced corner, those quads; max height
	// gap (m) between a built tile's border and its neighbour's last-built border (-10000 = untraced vertex)
	protected int m_iGapLiveBuilds;
	protected int m_iGapBuildsUnt;
	protected int m_iGapUntraced;
	protected int m_iGapUntracedMax;
	protected int m_iGapUntNow;     // untraced quads of the build running now (was a local: 64-locals limit)
	protected float m_fGapEdgeMax;
	protected int m_iGapEdgeOver;               // builds with an edge gap > 2 cm
	protected ref map<int, ref array<float>> m_mGapEdge = new map<int, ref array<float>>();   // TrailKey -> S N W E border y (4 x TV)
	protected int m_iSt10Replay;
	protected int m_iSt10Drops;
	// voxel ground changes (GroundChanged): per change its x, z box, blast middle and radius (0 = no blast)
	protected ref array<vector> m_aChgLo = {};
	protected ref array<vector> m_aChgHi = {};
	protected ref array<vector> m_aChgWp = {};
	protected ref array<float> m_aChgR = {};
	protected ref array<int> m_aJobTx = {};     // mud tiles still to redo, with their change
	protected ref array<int> m_aJobTz = {};
	protected ref array<int> m_aJobC = {};
	protected float m_fJobsAt;                  // no redo before this time (new brick bodies in first)
	protected int m_iJobT0;
	protected int m_iJobLive;
	protected int m_iJobStored;
	protected int m_iJobTraces;
	protected int m_iJobBare;
	protected int m_iStatHoles;                 // ground reads on hole cells (our ground) since the last log line
	protected float m_fNextSlump;
	protected ref MUD_MudSheetField m_F = new MUD_MudSheetField();
	protected ref MUD_MudSheetField m_T = new MUD_MudSheetField();    // scratch: shift copy, slump deltas (m_aD)
	protected ref array<ref MUD_MudSheetVeh> m_aVeh = {};
	// GRADUAL: per touched cell (key PassKey) the time of the last stamp, the vehicle (MUD_MudSheetVeh.m_iId) and the mud
	// depth when its current pass began (arch / ridge pushes earlier in the same pass don't count)
	protected ref map<int, float> m_mPassT = new map<int, float>();
	protected ref map<int, int> m_mPassV = new map<int, int>();
	protected ref map<int, float> m_mPassD = new map<int, float>();
	protected float m_fPassPruneT;
	protected int m_iNextVehId;
	protected ref Resource m_Res;
	protected IEntity m_Ent;                    // tile 0 (logs)
	protected ref array<IEntity> m_aTileEnt = {};
	protected ref array<ref Resource> m_aTileRes = {};

	protected int m_iX0;                        // sheet corner in world cells (x = m_iX0 * CELL)
	protected int m_iZ0;
	protected bool m_bPlaced;
	protected float m_fEstY;                    // ground guess (last wheel contact)
	protected bool m_bTeleported;               // DEBUG_TELEPORT done: the sheet no longer follows
	protected float m_fDebugY;                  // DEBUG_SHOW: vehicle origin y + 3 at placement
	protected float m_fVehY;                    // last followed vehicle origin y (log)
	protected ref Resource m_QuadRes;           // DEBUG_SHOW control quad (kept alive)
	protected IEntity m_QuadEnt;
	protected float m_fOrgY;                    // entity origin height (at the ground, not y 0: see Upload check 2026-10-14)
	protected int m_iBoxI0;                     // cells with mud (+1)
	protected int m_iBoxI1 = -1;
	protected int m_iBoxK0;
	protected int m_iBoxK1 = -1;
	protected int m_iRobin;
	// PERF 2026-10-15 (user: ~10-15 fps lost driving in mud; collider + drag A/B: not them): no work off the zones,
	// slump box = cells that moved last pass (+1), tiles marked only where the mud really moved
	protected bool m_bAnyZone;                  // a window slot is a zone tile
	protected int m_iScan;                      // Grounds first-trace cursor (one pass over the sheet per UpdateSlots)
	static const float SLUMP_SHOW = 0.001;      // a slump change smaller than this (m) does not rebuild the tile
	protected bool m_bDirty;
	protected float m_fNextUp;
	protected bool m_bIdxDirty;                 // traced set changed: rebuild the index buffer
	protected int m_iUntraced = NN;             // cells with no ground yet (0: Grounds skips the scan)
	protected float m_fLast = -1;
	protected ref map<int, ref MUD_MudSheetEase> m_mEase = new map<int, ref MUD_MudSheetEase>();   // EASE DROP: PassKey -> slide
	protected int m_iAbPhase = -1;              // TEST KIT render A/B: 0 = stored meshes shown, 1 = hidden
	protected float m_fAbNext;                  // next re-apply of the hide (new builds show themselves)
	protected ref array<IEntity> m_aAbHidden = {};   // meshes RenderAB hid (only these are shown again)
	protected float m_fStatT;
	protected int m_iStatFrames;
	protected int m_iStatMs;
	protected int m_iStatMax;
	protected int m_iStatTraces;
	protected int m_iStatQuads;
	protected int m_iStatUploads;
	protected int m_aStatSec[5];                // ms per step this log window: follow, wheels, slump, grounds, upload
	protected float m_fStatH;
	protected float m_fStatDragA;               // most mud slowing in this log window (m/s2)
	protected float m_fStatMud;                 // highest mud share of a vehicle's wheels (0..1)
	protected float m_fStatEff = 1;             // lowest gearbox factor set
	protected float m_fStatSink;                // sum of raw sink (traced side ground - wheel contact y), unclamped
	protected int m_iStatSinkN;
	protected float m_fStatSinkMin = 1000;
	protected float m_fStatSinkMax = -1000;
	// SYNC (Stage 2): own stamps waiting for their op, oldest first; the replay clip (window cells); log counts
	protected ref array<ref MUD_MudSheetPred> m_aPred = {};
	protected float m_fNextExpire;
	protected bool m_bClip;
	protected int m_iClipI0;
	protected int m_iClipI1;
	protected int m_iClipK0;
	protected int m_iClipK1;
	// SYNC op split: own stamps still to replay after an op's restore (SyncBegin -> SyncReplay chunks), its world cell
	// box, and the upload hold while they run (no frame shows the box without them)
	protected ref array<ref MUD_MudSheetPred> m_aReplay = {};
	protected int m_iReplayAt;
	protected int m_iRpGi0;
	protected int m_iRpGi1;
	protected int m_iRpGk0;
	protected int m_iRpGk1;
	protected bool m_bReplayHold;
	protected int m_iReplayHoldAt;              // System.GetTickCount when the hold began
	// TEMP delay meter (2026-10-18, user: ruts show a little after the wheel), per Log10 window
	protected ref map<int, float> m_mDlStampT = new map<int, float>();   // world tile key (tx * 20000 + tz) -> time of its first undrawn own wheel stamp
	protected ref map<int, float> m_mDlSpeed = new map<int, float>();    // ... that stamp's wheel speed (m/s)
	protected float m_fDlSum;                   // own wheel stamp -> its live tile mesh drawn (s)
	protected float m_fDlMax;
	protected float m_fDlDist;                  // that delay x the wheel speed (m)
	protected int m_iDlN;
	protected int m_iDlDue;                     // frames an upload was due but held by an op's replay
	protected int m_iDlHolds;                   // replay holds ended (done or stuck), their ms
	protected int m_iDlHoldMs;
	protected int m_iDlHoldMax;
	protected int m_iDlStuck;                   // ... of them let go by REPLAY_HOLD_MS
	protected int m_iDlWheelPicks;              // slots WHEEL TILES FIRST picked
	protected int m_iStatCovered;
	protected int m_iStatExpired;
	protected int m_iStatReplayed;
	protected int m_iStatResync;
	protected int m_iStatResyncMs;
	protected int m_iStatStoredRe;
	protected int m_iSt10StoredHeld;            // A5: stored builds held (not rendered), tile-frames per 10 s
	protected ref array<int> m_aHeldBuild = {};  // A5: keys held this FlushOneBuild (queued again at its end)

	//------------------------------------------------------------------------------------------------
	//! OFF_TEST: fps, worst frame and top vehicle speed per 5 s, nothing else
	protected static void OffLog(float time)
	{
		if (s_fOffT < 0 || time < s_fOffPrev)
		{
			s_fOffT = time;     // first frame, or a new play
			s_fOffPrev = -1;
			s_iOffN = 0;
			s_fOffDtMax = 0;
			s_fOffSpeed = 0;
		}
		if (s_fOffPrev >= 0)
			s_fOffDtMax = Math.Max(s_fOffDtMax, time - s_fOffPrev);
		s_fOffPrev = time;
		s_iOffN++;
		foreach (Vehicle v : MUD_Wheels.GetAll())
		{
			if (!v)
				continue;
			Physics ph = v.GetPhysics();
			if (ph)
				s_fOffSpeed = Math.Max(s_fOffSpeed, ph.GetVelocity().Length());
		}
		if (time - s_fOffT < 5)
			return;
		Print(string.Format("MUD mud sheet PERF OFF: fps %1, worst frame %2 ms, top speed %3 m/s",
			s_iOffN / (time - s_fOffT), s_fOffDtMax * 1000, s_fOffSpeed), LogLevel.NORMAL);
		s_fOffT = time;
		s_iOffN = 0;
		s_fOffDtMax = 0;
		s_fOffSpeed = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Synced sheet Stage 3 line (TEST_KIT, every LOG10_S s). PERF numbers: judge them only on a DS / uploaded mod
	//! client, never in Workbench (project testing rule 2026-10-16).
	protected void Log10(float time)
	{
		if (m_fSt10T < 0 || time < m_fSt10T)
			m_fSt10T = time;
		if (time - m_fSt10T < LOG10_S)
			return;
		m_fSt10T = time;
		int meshes = 0;
		int kept = 0;
		foreach (int key : m_aTrailOrder)
		{
			MUD_MudSheetStore st = m_mTrail.Get(key);
			if (!st)
				continue;
			if (st.m_Ent)
				meshes++;
			else
				kept++;
		}
		int live = 0;
		if (m_bPlaced)
		{
			for (int t = 0; t < TILES * TILES; t++)
			{
				if (m_aSlotZone[t] != 0)
					live++;
			}
		}
		float mean = m_iSt10BuildMs;
		if (m_iSt10Builds > 0)
			mean /= m_iSt10Builds;
		int touched = MUD_MudSync.TileCount();
		int syncStamps = MUD_MudSync.TakeStamps10();
		// separate statements, at most 9 values each (string.Format)
		string p1 = string.Format("MUD sheet: tiles touched %1, meshes %2, builds %3, mean %4 ms, worst %5 ms, sync stamps %6, replay stamps %7",
			touched, meshes + live, m_iSt10Builds, mean, m_iSt10BuildMax, syncStamps, m_iSt10Replay);
		string p2 = string.Format(" | (%1 stored + %2 live meshes, %3 kept data-only, %4 dropped, %5 to build, radius %6 m)",
			meshes, live, kept, m_iSt10Drops, m_aBuildTx.Count(), RADIUS)
			+ string.Format(" | build radius %1 m at %2 m/s, last queue %3, sparse %4", m_fRadBuild, m_vRadVel.Length(), m_iRadQueued, PatchLiftOn());
		string p3 = string.Format(" | stored builds held (not rendered) %1 (tile-frames, A5 gate %2)", m_iSt10StoredHeld, STORED_RENDERED_GATE);
		Print(p1 + p2 + p3, LogLevel.NORMAL);
		m_iSt10StoredHeld = 0;
		Print(string.Format("MUD sheet gaps: live builds %1, with untraced quads %2 (quads %3, max %4 in one build) | border gap max %5 m, builds over 2 cm %6",
			m_iGapLiveBuilds, m_iGapBuildsUnt, m_iGapUntraced, m_iGapUntracedMax, m_fGapEdgeMax, m_iGapEdgeOver)
			+ string.Format(" | re-ground exact: %1 cells in %2 ops | look ruts: %3 cells in %4 calls", m_iReCells10, m_iReOps10, m_iLookCells10, m_iLookCuts10) + " | " + MUD_MudZoneEntity.OffroadStats(), LogLevel.NORMAL);
		m_iLookCells10 = 0;
		m_iLookCuts10 = 0;
		m_iReCells10 = 0;
		m_iReOps10 = 0;
		m_iGapLiveBuilds = 0;
		m_iGapBuildsUnt = 0;
		m_iGapUntraced = 0;
		m_iGapUntracedMax = 0;
		m_fGapEdgeMax = 0;
		m_iGapEdgeOver = 0;
		if (m_mGapEdge.Count() > 2000)
			m_mGapEdge.Clear();
		if (m_bLocal)
			LocalLog();
		DlLog(time);
		m_iSt10Builds = 0;
		m_iSt10BuildMs = 0;
		m_iSt10BuildMax = 0;
		m_iSt10Replay = 0;
		m_iSt10Drops = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP delay meter line (Log10), then reset. Tiles stamped but not drawn for 5 s (left the window) are dropped.
	protected void DlLog(float time)
	{
		if (m_iDlN > 0 || m_iDlHolds > 0 || m_iDlDue > 0)
		{
			float avg = 1000 * m_fDlSum / Math.Max(1, m_iDlN);
			float dist = m_fDlDist / Math.Max(1, m_iDlN);
			float holdAvg = m_iDlHoldMs / Math.Max(1, m_iDlHolds);
			Print(string.Format("MUD delay sheet 10 s (TEMP): own wheel stamp -> drawn tile avg %1 max %2 ms, ~%3 m behind the wheel (%4 tiles) | upload held by replay: %5 holds, avg %6 max %7 ms, %8 stuck",
				Math.Round(avg), Math.Round(1000 * m_fDlMax), dist.ToString(-1, 2), m_iDlN, m_iDlHolds, Math.Round(holdAvg), m_iDlHoldMax, m_iDlStuck)
				+ string.Format(", %1 frames an upload was due but held | UPLOAD_EVERY %2 s | wheel tiles picked first %3 (WHEEL_SLOTS_MAX %4) | D1 slot passes held %5 (SLOT_MIN_S %6)", m_iDlDue, UPLOAD_EVERY, m_iDlWheelPicks, WHEEL_SLOTS_MAX, m_iUpThrottled, SLOT_MIN_S), LogLevel.NORMAL);
		}
		array<int> old = {};
		foreach (int k, float st : m_mDlStampT)
		{
			if (time - st > 5 || time < st)
				old.Insert(k);
		}
		foreach (int ok : old)
		{
			m_mDlStampT.Remove(ok);
			m_mDlSpeed.Remove(ok);
		}
		m_fDlSum = 0;
		m_fDlMax = 0;
		m_fDlDist = 0;
		m_iDlN = 0;
		m_iDlDue = 0;
		m_iDlHolds = 0;
		m_iDlHoldMs = 0;
		m_iDlHoldMax = 0;
		m_iDlStuck = 0;
		m_iDlWheelPicks = 0;
		if (LOD_ON)
		{
			int lodNow = 0;
			foreach (int lk, MUD_MudSheetStore lst : m_mTrail)
			{
				if (lst && lst.m_D.m_iLow != 0)
					lodNow++;
			}
			m_iLowNow = lodNow;
			Print(string.Format("MUD lod 10 s (PAPER TEST 2): low builds %1, queued back to full %2, stored tiles low now %3 of %4 (LOD_M %5 m) | LIFT BY DRAWN %6: patch tells %7", m_iLodLow, m_iLodUp, lodNow, m_mTrail.Count(), LOD_M, LIFT_BY_DRAWN, m_iLiftTold), LogLevel.NORMAL);
			m_iLiftTold = 0;
		}
		m_iLodLow = 0;
		m_iLodUp = 0;
		m_iUpThrottled = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Called every frame from MUD_WorldGround (time = its m_fTime, back to 0 at each play)
	static void Tick(float time)
	{
		if (!ENABLED)
			return;
		if (RplSession.Mode() == RplMode.Dedicated)
		{
			if (SYNC)
				MUD_MudSync.DsTick();      // synced sheet Stage 5: the server keeps the layer for late joiners (no sheet drawn)
			return;
		}
		if (OFF_TEST)
		{
			OffLog(time);
			return;
		}
		if (!s_Inst || time < s_Inst.m_fLast)
		{
			s_Inst = new MUD_MudSheet();    // first frame, or a new play (the old entity went with the old world)
			s_iLazyPhase = -1;
			s_Inst.m_bLocal = !SYNC;        // MUD LAB sheet local: client-only, deposit band, no sync layer
		}
		LazyAbTick(time);
		int tP = System.GetTickCount();
		if (SYNC)
			MUD_MudSync.Drain();            // queued rut ops, a few ms a frame (they restore + replay this sheet)
		MUD_Prof.Add(MUD_Prof.SHEET_SYNC, System.GetTickCount(tP));
		tP = System.GetTickCount();
		s_Inst.Frame(time);
		MUD_Prof.Add(MUD_Prof.SHEET, System.GetTickCount(tP));
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID: builds the stored tiles trench / fill ops changed (live ones draw on their own upload)
	protected void FlushOneBuild()
	{
		int t0 = System.GetTickCount();
		int built = 0;
		m_iTaperMs = 0;
		vector gcam[4];
		if (STORED_RENDERED_GATE)
			GetGame().GetWorld().GetCurrentCamera(gcam);
		while (!m_aOneBuild.IsEmpty())
		{
			if (STORED_MS > 0 && built > 0 && System.GetTickCount(t0) >= STORED_MS)
				break;      // perf fix 2: the rest next frame (Frame calls this while m_aOneBuild has keys)
			int key = m_aOneBuild[0];
			m_aOneBuild.RemoveOrdered(0);
			int qWhy;
			if (m_mQWhy.Find(key, qWhy))
				m_mQWhy.Remove(key);
			else
				qWhy = BW_RESTORE;
			MUD_MudSheetStore st = m_mTrail.Get(key);
			if (!st || !st.m_Ent)
				continue;
			int tx = Math.Round(key / 20000.0);
			int tz = key - tx * 20000;
			if (IsLive(tx, tz))
				continue;
			if (STORED_RENDERED_GATE && StoredWait(st, tx, tz, gcam[3]))
			{
				m_aHeldBuild.Insert(key);
				continue;
			}
			if (BAND_STORED_HOLD && qWhy == BW_BAND && st.m_Res && m_mStoredAt.Contains(key) && m_fLast < m_mStoredAt.Get(key) + SLOT_MIN_S)
			{
				m_aHeldBuild.Insert(key);     // stage 2 C5: band only, built < SLOT_MIN_S ago - end of the queue
				if (!m_mQWhy.Contains(key))
					m_mQWhy.Set(key, BW_BAND);     // still band only when it comes up again
				m_iBandHeld++;
				continue;
			}
			m_iBuildWhy = qWhy;
			Resource res = BuildTile(st.m_Ent, st.m_D, tx, tz, false);
			if (res)
				st.m_Res = res;
			built++;
		}
		// A5: held tiles wait at the end of the queue (checked again next frame)
		foreach (int hk : m_aHeldBuild)
			m_aOneBuild.Insert(hk);
		m_aHeldBuild.Clear();
		MUD_Prof.Add(MUD_Prof.SH_STORED, System.GetTickCount(t0));
	}

	//------------------------------------------------------------------------------------------------
	//! A5 STORED_RENDERED_GATE: true = stored tile st's rebuild waits (it has a mesh, is shown, was not drawn last
	//! frame, and its middle is over STORED_GATE_NEAR m from the camera cp)
	protected bool StoredWait(MUD_MudSheetStore st, int tx, int tz, vector cp)
	{
		if (!st.m_Res || (st.m_Ent.GetFlags() & EntityFlags.VISIBLE) == 0)
			return false;
		if (st.m_Ent.IsRendered())
			return false;
		float ux = (tx + 0.5) * TILE_M - cp[0];
		float uz = (tz + 0.5) * TILE_M - cp[2];
		if (ux * ux + uz * uz < STORED_GATE_NEAR * STORED_GATE_NEAR)
			return false;
		m_iSt10StoredHeld++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Frame(float time)
	{
		if (MUD_TestKit.TEST_KIT)
			MUD_CraterTrace.Tick();     // stage 2 B1 crater log: prints finished craters
		int tPre = System.GetTickCount();
		int bPre = s_iBandMsAll;     // stage 2 A1: BuildNowSlots' BandTile
		if (m_bNowDue)
			StepNow();
		else if (!m_aPend.IsEmpty() && System.GetTickCount(m_iPendT) > PEND_MAX_MS)
		{
			Print(string.Format("MUD one grid: %1 held ops, no voxel go-live in %2 ms -> sheet now", m_aPend.Count(), PEND_MAX_MS), LogLevel.WARNING);
			ApplyPending();
		}
		MUD_Prof.Add(MUD_Prof.SH_PRE, System.GetTickCount(tPre) - (s_iBandMsAll - bPre));
		if (!m_mEase.IsEmpty() && (!LEV_L6_EASE || time >= m_fNextEase))
		{
			if (LEV_L6_EASE)
				m_fNextEase = time + UPLOAD_EVERY;     // L6: at upload rate
			EaseTick(time);
		}
		if (!m_aOneBuild.IsEmpty())
			FlushOneBuild();
		bool frozen = false;
		if (FREEZE_TEST)
		{
			int cyc = Math.Floor(time / FREEZE_CYCLE);
			frozen = cyc % 2 == 1;
		}
		if (m_fStatPrevT >= 0)
		{
			float fdt = time - m_fStatPrevT;
			m_fStatDtMax = Math.Max(m_fStatDtMax, fdt);
			if (frozen)
			{
				m_fFrzT += fdt;
				m_iFrzN++;
			}
			else
			{
				m_fLiveT += fdt;
				m_iLiveN++;
			}
		}
		m_fStatPrevT = time;
		m_fLast = time;
		BenchTick(time);
		int t0 = System.GetTickCount();
		for (int dI = m_aDying.Count() - 1; dI >= 0; dI--)
		{
			if (time < m_aDying[dI].m_fDieAt)
				continue;
			if (m_aDying[dI].m_Ent)
			{
				DropColl(m_aDying[dI].m_Ent);
				SCR_EntityHelper.DeleteEntityAndChildren(m_aDying[dI].m_Ent);
			}
			m_aDying.Remove(dI);
		}
		int tZ = System.GetTickCount();
		int bZ = s_iBandMsAll;     // stage 2 A1: BandTile ms counts as 'band' only, not in the part that called it
		BuildZones();
		MUD_Prof.Add(MUD_Prof.SH_ZONES, System.GetTickCount(tZ) - (s_iBandMsAll - bZ));
		tZ = System.GetTickCount();
		GroundJobs();
		MUD_Prof.Add(MUD_Prof.SH_JOBS, System.GetTickCount(tZ));
		if (COLLIDE && time >= m_fCollCheck)
		{
			m_fCollCheck = time + COLL_S * 0.5;
			CollPending();
		}
		Vehicle veh = NearVehicle();
		if (veh)
		{
			m_fVehY = veh.GetOrigin()[1];
			Physics vph = veh.GetPhysics();
			if (vph)
				m_fStatSpeed = Math.Max(m_fStatSpeed, vph.GetVelocity().Length());
			if (!m_bPlaced)
			{
				m_fEstY = veh.GetOrigin()[1] - 0.5;     // until a wheel contact gives the ground
				m_fDebugY = veh.GetOrigin()[1] + 3;
			}
			// user 2026-10-14 23:20: the window made its own tiles under the car while the zone was still building
			// (mud grew outward from the car) - it waits until every zone tile is built, then takes those over.
			// CAMERA RADIUS: no such wait - a slot with no built tile takes the sync layer's mud (FillSlot)
			if (FOLLOW && !m_bTeleported && (RadiusOn() || m_aBuildTx.IsEmpty()))
			{
				int tF = System.GetTickCount();
				int bF = s_iBandMsAll;     // A1
				Follow(veh.GetOrigin());
				m_aStatSec[0] = m_aStatSec[0] + System.GetTickCount(tF);
				MUD_Prof.Add(MUD_Prof.SH_FOLLOW, System.GetTickCount(tF) - (s_iBandMsAll - bF));
			}
		}
		if (m_bPlaced)
		{
			int tS = System.GetTickCount();
			if (LEV_L8_VEH)
			{
				WheelsNear(time);
			}
			else
			{
				foreach (Vehicle v : MUD_Wheels.GetAll())
				{
					if (v)
						Wheels(v, time);
				}
			}
			m_aStatSec[1] = m_aStatSec[1] + System.GetTickCount(tS);
			MUD_Prof.Add(MUD_Prof.SH_WHEELS, System.GetTickCount(tS));
			if (SYNC && time >= m_fNextExpire)
			{
				int tE = System.GetTickCount();
				ExpirePreds(time);
				MUD_Prof.Add(MUD_Prof.SH_EXPIRE, System.GetTickCount(tE));
			}
			// SYNC: no local slump - the sync layer slumps per op (the same on every machine)
			if (!SYNC && m_bAnyZone && time >= m_fNextSlump)
			{
				// 10 Hz, with the upload (COST 23:01: every frame was ~3 ms of the ~5)
				m_fNextSlump = time + UPLOAD_EVERY;
				tS = System.GetTickCount();
				m_iMarkWhy = WHY_SLUMP;
				Slump();
				m_iMarkWhy = WHY_OTHER;
				m_aStatSec[2] = m_aStatSec[2] + System.GetTickCount(tS);
				s_iSlumpMsAll += System.GetTickCount(tS);
			}
			tS = System.GetTickCount();
			if (m_bAnyZone)
				Grounds();
			m_aStatSec[3] = m_aStatSec[3] + System.GetTickCount(tS);
			MUD_Prof.Add(MUD_Prof.SH_GROUNDS, System.GetTickCount(tS));
			// SYNC op split: hold the upload while an op's own-stamp replay is unfinished (a stuck one lets go)
			if (m_bReplayHold && System.GetTickCount(m_iReplayHoldAt) > REPLAY_HOLD_MS)
			{
				DlHoldEnd(true);     // TEMP delay meter
				m_bReplayHold = false;
				m_aReplay.Clear();
			}
			if (m_bDirty && time >= m_fNextUp && !frozen && m_bReplayHold)
				m_iDlDue++;     // TEMP delay meter
			if (m_bDirty && time >= m_fNextUp && !frozen && !m_bReplayHold)
			{
				m_fNextUp = time + UPLOAD_EVERY;
				tS = System.GetTickCount();
				int bU = s_iBandMsAll;     // A1
				Upload();
				m_aStatSec[4] = m_aStatSec[4] + System.GetTickCount(tS);
				MUD_Prof.Add(MUD_Prof.SH_UPLOAD, System.GetTickCount(tS) - (s_iBandMsAll - bU));
			}
		}
		int ms = System.GetTickCount(t0);
		m_iStatFrames++;
		m_iStatMs += ms;
		if (ms > m_iStatMax)
			m_iStatMax = ms;
		if (MUD_TestKit.TEST_KIT)
			Log10(time);
		if (time - m_fStatT >= 5)
		{
			if (m_bPlaced && m_iStatFrames > 0)
			{
				float dmax = 0;
				for (int v2 = 0; v2 < NN; v2++)
					dmax = Math.Max(dmax, m_F.m_aD[v2]);
				float avg = m_iStatMs;
				avg /= m_iStatFrames;
				// string.Format takes at most 9 values
				Print(string.Format("MUD mud sheet: %1 frames, %2 ms/frame avg, max %3 ms | traces %4 | uploads %5 | drawn quads %6 | highest dirt %7 m (layer THICK), last ridge H %8 m",
					m_iStatFrames, avg, m_iStatMax, m_iStatTraces, m_iStatUploads, m_iStatQuads, dmax, m_fStatH)
					+ string.Format(" | box %1..%2 x %3..%4", m_iBoxI0, m_iBoxI1, m_iBoxK0, m_iBoxK1)
					+ string.Format(" | side ground - wheel y: avg %1 min %2 max %3 (%4 stamps) | entity %5 at %6", m_fStatSink / Math.Max(1, m_iStatSinkN), m_fStatSinkMin, m_fStatSinkMax, m_iStatSinkN, m_Ent != null, m_iX0 * CELL)
					+ string.Format(" | DRAG max %1 m/s2, mud share max %2, gearbox x%3 min | trail %4 tiles", m_fStatDragA, m_fStatMud, m_fStatEff, m_aTrailOrder.Count()) + string.Format(" (zone tiles %1, to build %2)", m_iZoneTiles, m_aBuildTx.Count())
					+ string.Format(" | ms in 5 s: follow %1 wheels %2 slump %3 grounds %4 upload %5 (slot builds put off %7) | slump box %6 cells",
						m_aStatSec[0], m_aStatSec[1], m_aStatSec[2], m_aStatSec[3], m_aStatSec[4], Math.Max(0, m_iBoxI1 - m_iBoxI0 + 1) * Math.Max(0, m_iBoxK1 - m_iBoxK0 + 1), m_iUpSkipped)
					+ string.Format(" | colliders: %1 built in %2 ms, %3 held back, %4 alive", m_iCollBuilt, m_iCollMs, m_iCollHeld, m_aCollEnt.Count()), LogLevel.NORMAL);
				Print(string.Format("MUD mud sheet PERF: fps %1, worst frame %2 ms, COLLIDE %3, top speed %4 m/s, our ms/frame %5, drawn quads %6",
					m_iStatFrames / (time - m_fStatT), m_fStatDtMax * 1000, COLLIDE, m_fStatSpeed, avg, m_iStatQuads)
					+ string.Format(" | FREEZE %1: live fps %2 (%3 s), frozen fps %4 (%5 s)", FREEZE_TEST,
						m_iLiveN / Math.Max(0.001, m_fLiveT), m_fLiveT, m_iFrzN / Math.Max(0.001, m_fFrzT), m_fFrzT), LogLevel.NORMAL);
				if (SYNC)
				{
					// separate statements: Format + a call that Formats in one expression printed only the call's text,
					// twice (WB log 2026-10-18 01:35)
					string syncStat = MUD_MudSync.StatLine();
					string sheetStat = string.Format("MUD mud sheet sync: own stamps %1 held, %2 taken by ops, %3 expired, %4 replayed | %5 restores in %6 ms, %7 stored tiles rebuilt | ",
						m_aPred.Count(), m_iStatCovered, m_iStatExpired, m_iStatReplayed, m_iStatResync, m_iStatResyncMs, m_iStatStoredRe);
					Print(sheetStat + syncStat, LogLevel.NORMAL);
				}
			}
			m_iStatCovered = 0;
			m_iUpSkipped = 0;
			m_iStatExpired = 0;
			m_iStatReplayed = 0;
			m_iStatResync = 0;
			m_iStatResyncMs = 0;
			m_iStatStoredRe = 0;
			m_fStatT = time;
			m_iStatFrames = 0;
			m_iStatMs = 0;
			m_iStatMax = 0;
			m_iStatTraces = 0;
			m_iStatUploads = 0;
			m_iCollBuilt = 0;
			m_fStatDtMax = 0;
			m_fStatSpeed = 0;
			m_fFrzT = 0;
			m_iFrzN = 0;
			m_fLiveT = 0;
			m_iLiveN = 0;
			m_iCollMs = 0;
			m_iCollHeld = 0;
			m_fStatSink = 0;
			m_iStatSinkN = 0;
			m_fStatSinkMin = 1000;
			m_fStatSinkMax = -1000;
			m_fStatDragA = 0;
			m_fStatMud = 0;
			for (int sI = 0; sI < 5; sI++)
				m_aStatSec[sI] = 0;
			m_fStatEff = 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle closest to the local player (within 8 m: the one they sit in or stand by), else null
	protected Vehicle NearVehicle()
	{
		IEntity pl = SCR_PlayerController.GetLocalControlledEntity();
		if (!pl)
			return null;
		vector pp = pl.GetOrigin();
		Vehicle best;
		float bestD = 64;
		foreach (Vehicle v : MUD_Wheels.GetAll())
		{
			if (!v)
				continue;
			float d = vector.DistanceSq(v.GetOrigin(), pp);
			if (d < bestD)
			{
				bestD = d;
				best = v;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Places the sheet round pos the first time; later shifts it by whole tiles when pos is RECENTER off the middle.
	//! Leaving tiles a wheel touched become trail tiles; entering tiles come back from the trail if they are in it.
	protected void Follow(vector pos)
	{
		int half = (N - 1) / 2;
		// corner on the tile grid (world x, z >= 0 on Arland, so int division by TQ is exact)
		int wantX = (Math.Round(pos[0] / TILE_M) - TILES / 2) * TQ;
		int wantZ = (Math.Round(pos[2] / TILE_M) - TILES / 2) * TQ;
		if (!m_bPlaced)
		{
			m_iX0 = wantX;
			m_iZ0 = wantZ;
			m_bPlaced = true;
			MakeMesh();
			LiftDirtyWindow(m_iX0 / TQ, m_iZ0 / TQ, -100000, -100000);
			return;
		}
		float cx = (m_iX0 + half) * CELL;
		float cz = (m_iZ0 + half) * CELL;
		if (Math.AbsFloat(pos[0] - cx) < RECENTER && Math.AbsFloat(pos[2] - cz) < RECENTER)
			return;
		if (wantX == m_iX0 && wantZ == m_iZ0)
			return;
		int sti = (wantX - m_iX0) / TQ;
		int stk = (wantZ - m_iZ0) / TQ;
		int oldTx = m_iX0 / TQ;
		int oldTz = m_iZ0 / TQ;
		if (m_bLocal && BAND_INCR)
			BandIncr();     // stage 2 C1: pending touches into the distances before slots freeze / move
		// 1. tiles that stay move to their new slot; leaving ones become trail (touched) or spare entities
		array<IEntity> spare = {};
		array<IEntity> newEnt = {};
		array<ref Resource> newRes = {};
		int newBuilt[16];       // D1: a kept tile's last build time moves with it (new slots 0 = build at once)
		int newUnlift[16];      // LIFT BY DRAWN: the same for the on-screen flag
		int newBand[16];        // SHEET LOCAL: the same for the band-due flag
		int newDirty[16];       // L1: the same for the dirty flag and its reasons (entering slots: dirty)
		int newWhy[16];
		for (int t0 = 0; t0 < TILES * TILES; t0++)
		{
			newEnt.Insert(null);
			newRes.Insert(null);
			newBuilt[t0] = 0;
			newUnlift[t0] = 0;
			newBand[t0] = 0;
			newDirty[t0] = 1;
			newWhy[t0] = WHY_OTHER;
		}
		for (int tk = 0; tk < TILES; tk++)
		{
			for (int ti = 0; ti < TILES; ti++)
			{
				int t = tk * TILES + ti;
				IEntity te = m_aTileEnt[t];
				int nti = ti - sti;
				int ntk = tk - stk;
				if (nti >= 0 && nti < TILES && ntk >= 0 && ntk < TILES)
				{
					newEnt[ntk * TILES + nti] = te;
					newRes[ntk * TILES + nti] = m_aTileRes[t];
					newBuilt[ntk * TILES + nti] = m_aSlotBuiltMs[t];
					newUnlift[ntk * TILES + nti] = m_aSlotUnlift[t];
					newBand[ntk * TILES + nti] = m_aSlotBand[t];
					newDirty[ntk * TILES + nti] = m_aSlotDirty[t];
					newWhy[ntk * TILES + nti] = m_aSlotWhy[t];
					continue;
				}
				if (!te)
					continue;
				if (TileInZone(oldTx + ti, oldTz + tk))
				{
					Freeze(oldTx + ti, oldTz + tk, ti, tk, te, m_aTileRes[t]);
				}
				else
				{
					te.ClearFlags(EntityFlags.VISIBLE, false);
					DropColl(te);
					spare.Insert(te);
					if (LIFT_BY_DRAWN && m_aSlotUnlift[t] != 0)
						MUD_WorldGroundEntity.SheetLiftDirty((oldTx + ti) * TILE_M, (oldTz + tk) * TILE_M);   // zone edge: lift back
				}
			}
		}
		for (int tb = 0; tb < TILES * TILES; tb++)
		{
			m_aSlotBuiltMs[tb] = newBuilt[tb];
			m_aSlotUnlift[tb] = newUnlift[tb];
			m_aSlotBand[tb] = newBand[tb];
			if (LEV_L1_CARRY)
			{
				m_aSlotDirty[tb] = newDirty[tb];
				m_aSlotWhy[tb] = newWhy[tb];
			}
		}
		LiftDirtyWindow(wantX / TQ, wantZ / TQ, oldTx, oldTz);
		// 2. field data moves by whole tiles
		int si = sti * TQ;
		int sk = stk * TQ;
		for (int k = 0; k < N; k++)
		{
			for (int i = 0; i < N; i++)
			{
				int oi = i + si;
				int ok = k + sk;
				int v = k * N + i;
				if (oi >= 0 && oi < N && ok >= 0 && ok < N)
				{
					int o = ok * N + oi;
					m_T.m_aD[v] = m_F.m_aD[o];
					m_T.m_aG[v] = m_F.m_aG[o];
					m_T.m_aGs[v] = m_F.m_aGs[o];
					m_T.m_aOu[v] = m_F.m_aOu[o];
					m_T.m_aOv[v] = m_F.m_aOv[o];
					m_T.m_aBare[v] = m_F.m_aBare[o];
					m_T.m_aTouch[v] = m_F.m_aTouch[o];
					m_T.m_aBand[v] = m_F.m_aBand[o];
					m_T.m_aLastY[v] = m_F.m_aLastY[o];
				}
				else
				{
					m_T.m_aD[v] = THICK;
					m_T.m_aG[v] = 0;
					m_T.m_aGs[v] = 0;
					m_T.m_aOu[v] = 0;
					m_T.m_aOv[v] = 0;
					m_T.m_aBare[v] = 0;
					m_T.m_aTouch[v] = 0;
					m_T.m_aBand[v] = 0;
					m_T.m_aLastY[v] = 0;
				}
			}
		}
		// swap the two holders instead of copying back
		MUD_MudSheetField tmp = m_F;
		m_F = m_T;
		m_T = tmp;
		m_iX0 = wantX;
		m_iZ0 = wantZ;
		m_iBoxI0 -= si;
		m_iBoxI1 -= si;
		m_iBoxK0 -= sk;
		m_iBoxK1 -= sk;
		m_Drag.Shift(si, sk);     // L2 / L3 cell sets move with the field (empty while those levers are off)
		m_Chg.Shift(si, sk);
		for (int cn = 0; cn < TILES * TILES; cn++)
			m_aCoNext[cn] = 0;     // stage 2 A3: slot numbers moved (the shift rebuilds its slots anyway)
		if (m_bLocal && BAND_INCR)
			DistShift(sti, stk);     // stage 2 C1
		for (int sgr = 0; sgr < TILES * TILES; sgr++)
		{
			m_aSlotSig1[sgr] = 0;     // stage 2 C3: slot numbers moved - unknown until the next build
			m_aSlotSig2[sgr] = 0;
		}
		// 3. empty slots: the trail tile there comes back to life, else a spare or new entity
		int nTx = m_iX0 / TQ;
		int nTz = m_iZ0 / TQ;
		for (int tk2 = 0; tk2 < TILES; tk2++)
		{
			for (int ti2 = 0; ti2 < TILES; ti2++)
			{
				int t2 = tk2 * TILES + ti2;
				if (newEnt[t2])
					continue;
				m_aSlotBand[t2] = 1;     // SHEET LOCAL: an entering slot's band is computed again before its build
				int tx = nTx + ti2;
				int tz = nTz + tk2;
				int key = TrailKey(tx, tz);
				MUD_MudSheetStore st = m_mTrail.Get(key);
				if (st && st.m_Ent)
				{
					if (LEV_L1_CARRY && StoreCurrent(st, key))
					{
						m_aSlotDirty[t2] = 0;     // L1: comes back with a current full mesh - clean
						m_aSlotWhy[t2] = 0;
						m_aSlotBand[t2] = 0;
					}
					Unpack(st.m_D, ti2, tk2);
					if (m_bLocal && BAND_INCR)
						DistUnpack(st.m_D, key, t2, ti2, tk2);     // stage 2 C1: its kept distances, if current
					m_aSlotSig1[t2] = st.m_D.m_iSig1;     // stage 2 C3: the signature of the mesh it comes back with
					m_aSlotSig2[t2] = st.m_D.m_iSig2;
					m_aSlotUnlift[t2] = st.m_D.m_iUnlift;
					newEnt[t2] = st.m_Ent;
					newRes[t2] = st.m_Res;
					m_mTrail.Remove(key);
					RemoveTrailKey(key);
				}
				else
				{
					FillSlot(ti2, tk2, tx, tz);
				}
				if (newEnt[t2])
					continue;
				if (!spare.IsEmpty())
				{
					IEntity se = spare[spare.Count() - 1];
					spare.Remove(spare.Count() - 1);
					se.SetOrigin(Vector(tx * TQ * CELL, m_fEstY, tz * TQ * CELL));
					se.Update();
					newEnt[t2] = se;
				}
				else
				{
					newEnt[t2] = SpawnTile(tx, tz, m_fEstY);
				}
			}
		}
		foreach (IEntity left : spare)
			SCR_EntityHelper.DeleteEntityAndChildren(left);     // never had a mesh on screen (hidden above)
		m_aTileEnt = newEnt;
		m_aTileRes = newRes;
		m_Ent = m_aTileEnt[0];
		m_bSlotsKeep = LEV_L1_CARRY;
		UpdateSlots();
		m_bSlotsKeep = false;
		m_iUntraced = 0;
		for (int u = 0; u < NN; u++)
		{
			if (m_F.m_aGs[u] == 0)
				m_iUntraced++;
		}
		m_bIdxDirty = true;
		m_bDirty = true;
		if (LEV_L1_CARRY && !AnySlotDirty())
			return;         // L1: no slot really changed - no forced upload
		m_fNextUp = 0;      // upload at once, else the old mesh shows one shift off
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB sheet local switch (MUD_LabSheetLocal): preset 0 = off (sheet parked as before, SYNC back to true),
	//! 1..4 = BAND_IN 0.4 / 0.4 / 0.8 / 0.8 x BAND_FADE 0.8 / 1.2 / 0.8 / 1.2. Every switch starts fresh: the old
	//! instance's tiles are deleted and its data freed (the next Tick makes a new instance).
	static void LabSet(int preset, string why)
	{
		if (s_Inst)
			s_Inst.DropAllLab(why);
		s_Inst = null;
		s_iLabPreset = preset;
		ENABLED = preset > 0;
		SYNC = preset <= 0;
		BAND_IN = 0.4;
		BAND_FADE = 0.8;
		if (preset == 3 || preset == 4)
			BAND_IN = 0.8;
		if (preset == 2 || preset == 4)
			BAND_FADE = 1.2;
		s_iCoverGen++;
		s_iDistGen++;
		BandLutBuild();     // C1 / C2: band by distance for this preset
		if (preset <= 0 && s_iLeverSet != 0)
			LeverApply(0, why);
		if (preset <= 0 && s_iFixSet != 0)
			FixApply(0, why);
		Print(string.Format("MUD sheet local: preset %1 (enabled %2, sync %3, band in %4 m, fade %5 m) | %6", preset, ENABLED, SYNC, BAND_IN, BAND_FADE, why), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! LabSet: every tile entity of this instance deleted now (live slots, stored meshes, dying ones), data freed
	protected void DropAllLab(string why)
	{
		int live = 0;
		foreach (IEntity te : m_aTileEnt)
		{
			if (!te)
				continue;
			DropColl(te);
			SCR_EntityHelper.DeleteEntityAndChildren(te);
			live++;
		}
		int stored = 0;
		int dataOnly = 0;
		foreach (int key, MUD_MudSheetStore st : m_mTrail)
		{
			if (!st)
				continue;
			if (!st.m_Ent)
			{
				dataOnly++;
				continue;
			}
			DropColl(st.m_Ent);
			SCR_EntityHelper.DeleteEntityAndChildren(st.m_Ent);
			stored++;
		}
		int dying = 0;
		foreach (MUD_MudSheetStore dy : m_aDying)
		{
			if (!dy || !dy.m_Ent)
				continue;
			DropColl(dy.m_Ent);
			SCR_EntityHelper.DeleteEntityAndChildren(dy.m_Ent);
			dying++;
		}
		Print(string.Format("MUD sheet local: exit (%1) - deleted %2 live, %3 stored, %4 dying tile entities; freed %5 data-only, %8 compact tiles; %6 born tiles, %7 touched cells",
			why, live, stored, dying, dataOnly, m_mBorn.Count(), m_iTouchCells, m_mFar.Count()), LogLevel.NORMAL);
		m_mFar.Clear();
		m_aFarOrder.Clear();
		m_mOwn.Clear();
		m_mEdgeRec.Clear();
		m_aTileEnt.Clear();
		m_aTileRes.Clear();
		m_mTrail.Clear();
		m_aTrailOrder.Clear();
		m_aDying.Clear();
		m_aOneBuild.Clear();
		m_mQWhy.Clear();
		m_aBandQ.Clear();
		m_mBorn.Clear();
		m_aBuildTx.Clear();
		m_aBuildTz.Clear();
		m_mEase.Clear();
		m_TchSt = null;
		m_TchFar = null;
		m_bPlaced = false;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB 10 s line: 'tiles <n> built <n> kept <n> | born <n> touched cells <n> | live <n> stored <n> data-only <n>'
	static string LabCounts()
	{
		if (!s_Inst)
			return "tiles 0 built 0 kept 0 | born 0 touched cells 0 | live 0 stored 0 data-only 0";
		return s_Inst.LabCountsI();
	}

	protected string LabCountsI()
	{
		int live = 0;
		int liveBuilt = 0;
		if (m_bPlaced)
		{
			for (int t = 0; t < TILES * TILES; t++)
			{
				if (m_aSlotZone[t] == 0)
					continue;
				live++;
				// stage 2 D: a null build ('nothing to draw') hides the entity but leaves the old m_aTileRes - count only
				// slots whose last build gave a mesh (BuildTileMesh sets VISIBLE then)
				if (t < m_aTileRes.Count() && m_aTileRes[t] && m_aTileEnt[t] && (m_aTileEnt[t].GetFlags() & EntityFlags.VISIBLE) != 0)
					liveBuilt++;
			}
		}
		int stored = 0;
		int dataOnly = 0;
		foreach (int key, MUD_MudSheetStore st : m_mTrail)
		{
			if (!st)
				continue;
			if (st.m_Ent)
				stored++;
			else
				dataOnly++;
		}
		return string.Format("tiles %1 built %2 kept %3 | born %4 touched cells %5 | live %6 stored %7 data-only %8",
			live + stored + dataOnly, liveBuilt + stored, dataOnly, m_mBorn.Count(), m_iTouchCells, live, stored, dataOnly)
			+ string.Format(" compact %1", m_mFar.Count());     // MUD LAB part D
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB: running ms totals (never reset)
	static int SlumpMsAll()
	{
		return s_iSlumpMsAll;
	}

	static int BandMsAll()
	{
		return s_iBandMsAll;
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL: band reach in cells across a tile side (at most (BAND_W - TV) / 2)
	protected int BandMargin()
	{
		int mg = Math.Ceil((BAND_IN + BAND_FADE) / CELL);
		int cap = (BAND_W - TV) / 2;
		if (mg > cap)
			mg = cap;
		return mg;
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL: live slot t (world tile key) is born - a zone slot from now on, traced, band computed, built
	protected void Birth(int t, int key)
	{
		m_mBorn.Set(key, 1);
		m_aSlotZone[t] = 1;
		m_aSlotDirty[t] = 1;
		m_aSlotBand[t] = 1;
		m_aSlotWhy[t] = m_aSlotWhy[t] | WHY_BAND;
		m_bAnyZone = true;
		m_iScan = 0;
		m_iUntraced = NN;     // Grounds makes one pass and traces the new slot
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL: a stamp touched new cells in window cells i0..i1 x k0..k1. Every tile within the band margin is
	//! born (live: now; outside the window: CAMERA RADIUS builds it) and its band is due.
	protected void BandMark(int i0, int i1, int k0, int k1)
	{
		if (!m_bPlaced)
			return;
		BandMarkWorld(m_iX0 + i0, m_iX0 + i1, m_iZ0 + k0, m_iZ0 + k1);
	}

	//------------------------------------------------------------------------------------------------
	//! BandMark on world cells wi0..wi1 x wk0..wk1 (MUD LAB part E: op stamps outside the window too)
	protected void BandMarkWorld(int wi0, int wi1, int wk0, int wk1)
	{
		int mg = BandMargin();
		int gi0 = wi0 - mg;
		int gi1 = wi1 + mg;
		int gk0 = wk0 - mg;
		int gk1 = wk1 + mg;
		int tx0 = (gi0 - 1) / TQ;     // tile tx holds cells tx * TQ .. tx * TQ + TQ (sides shared)
		int tx1 = gi1 / TQ;
		int tz0 = (gk0 - 1) / TQ;
		int tz1 = gk1 / TQ;
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		for (int tz = tz0; tz <= tz1; tz++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				int key = TrailKey(tx, tz);
				bool born = m_mBorn.Contains(key);
				if (IsLive(tx, tz))
				{
					int t = (tz - z0) * TILES + tx - x0;
					if (!born)
						Birth(t, key);
					if (m_bIncrMark)
						continue;
					if (BAND_INCR)
						m_aSlotDist[t] = 0;     // stage 2 C1: a touch outside the window (op) - BandTile again
					m_aSlotBand[t] = 1;
					m_aSlotDirty[t] = 1;
					m_aSlotWhy[t] = m_aSlotWhy[t] | WHY_BAND;
					m_bDirty = true;
					continue;
				}
				if (BAND_BORN_FRONT)
					BornFront(tx, tz, key);     // stage 2 A5: no mesh yet -> built before the radius list
				if (!born)
				{
					m_mBorn.Set(key, 1);     // outside the window: QueueNear builds it, its band in BuildZoneTile
					continue;
				}
				if (TrailGet(key) && !m_aBandQ.Contains(key))     // MUD LAB part D: a compact one comes back first
					m_aBandQ.Insert(key);
				m_bDirty = true;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 A5 BAND_BORN_FRONT: tile tx, tz outside the window (born or re-banded now) without a mesh goes to the front
	//! of the radius builds (Radius takes m_aFrontTx first; QueueNear's re-sort does not touch it). Only within MESH_R
	//! of the last radius camera and where the sheet is wanted (as QueueNear).
	protected void BornFront(int tx, int tz, int key)
	{
		MUD_MudSheetStore st = m_mTrail.Get(key);
		if (st && st.m_Ent)
			return;     // has its mesh: a re-band rebuilds it (BandStored)
		if (!m_bLodCam || !SheetWanted(tx, tz))
			return;
		float mx = (tx + 0.5) * TILE_M - m_vLodCam[0];
		float mz = (tz + 0.5) * TILE_M - m_vLodCam[2];
		if (mx * mx + mz * mz > MESH_R * MESH_R)
			return;
		for (int f = 0; f < m_aFrontTx.Count(); f++)
		{
			if (m_aFrontTx[f] == tx && m_aFrontTz[f] == tz)
				return;
		}
		m_aFrontTx.Insert(tx);
		m_aFrontTz.Insert(tz);
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL: stored tile tx, tz (BandTile's one-store cache)
	protected MUD_MudSheetStore TouchStore(int tx, int tz)
	{
		int key = TrailKey(tx, tz);
		if (key != m_iTchKey)
		{
			m_iTchKey = key;
			m_TchSt = m_mTrail.Get(key);
			m_TchFar = null;
			if (!m_TchSt && !m_mFar.IsEmpty())
				m_TchFar = m_mFar.Get(key);     // MUD LAB part D: or its compact far data
		}
		return m_TchSt;
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL: was world cell gi, gk touched by a stamp? The window field, else the stored tile that holds it
	protected int TouchAt(int gi, int gk)
	{
		if (m_bPlaced)
		{
			int i = gi - m_iX0;
			int k = gk - m_iZ0;
			if (i >= 0 && i < N && k >= 0 && k < N)
				return m_F.m_aTouch[k * N + i];
		}
		if (gi < 0 || gk < 0)
			return 0;
		int tx = gi / TQ;
		int tz = gk / TQ;
		int li = gi - tx * TQ;
		int lk = gk - tz * TQ;
		MUD_MudSheetStore st = TouchStore(tx, tz);
		if (!st && !m_TchFar && li == 0 && tx > 0)
		{
			tx = tx - 1;     // a shared side cell: the tile on its other side may hold it
			li = TQ;
			st = TouchStore(tx, tz);
		}
		if (!st && !m_TchFar && lk == 0 && tz > 0)
		{
			tz = tz - 1;
			lk = TQ;
			st = TouchStore(tx, tz);
		}
		if (st)
			return st.m_D.m_aTouch[lk * TV + li];
		if (m_TchFar)
			return (m_TchFar.m_aC[lk * TV + li] >> 25) & 1;     // MUD LAB part D: class bit 1
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL deposit band of world tile tx, tz: 2-pass chamfer distance to the nearest touched cell over the tile
	//! + BandMargin cells, then band 1 within BAND_IN, a sine fall to 0 at BAND_IN + BAND_FADE. Written to d (a store)
	//! or, d null, to live slot ti, tk of the field.
	protected void BandTile(int tx, int tz, MUD_MudSheetTileData d, int ti, int tk)
	{
		if (BAND_MICRO)
		{
			BandTileMicro(tx, tz, d, ti, tk);     // stage 2 C2
			return;
		}
		int tB = System.GetTickCount();
		m_iTchKey = -1;
		m_TchSt = null;
		m_TchFar = null;
		int mg = BandMargin();
		int w = TV + 2 * mg;
		int gi0 = tx * TQ - mg;
		int gk0 = tz * TQ - mg;
		float dist[2401];      // BAND_W x BAND_W
		bool any = false;
		for (int k = 0; k < w; k++)
		{
			for (int i = 0; i < w; i++)
			{
				int li = i - mg;
				int lk = k - mg;
				int tc;
				if (d && li >= 0 && li < TV && lk >= 0 && lk < TV)
					tc = d.m_aTouch[lk * TV + li];
				else
					tc = TouchAt(gi0 + i, gk0 + k);
				if (tc != 0)
				{
					dist[k * w + i] = 0;
					any = true;
				}
				else
				{
					dist[k * w + i] = 1000000;
				}
			}
		}
		if (any)
		{
			for (int k2 = 0; k2 < w; k2++)
			{
				for (int i2 = 0; i2 < w; i2++)
				{
					int a = k2 * w + i2;
					float best = dist[a];
					if (best == 0)
						continue;
					if (i2 > 0)
						best = Math.Min(best, dist[a - 1] + 1);
					if (k2 > 0)
					{
						best = Math.Min(best, dist[a - w] + 1);
						if (i2 > 0)
							best = Math.Min(best, dist[a - w - 1] + 1.4142);
						if (i2 < w - 1)
							best = Math.Min(best, dist[a - w + 1] + 1.4142);
					}
					dist[a] = best;
				}
			}
			for (int k3 = w - 1; k3 >= 0; k3--)
			{
				for (int i3 = w - 1; i3 >= 0; i3--)
				{
					int b = k3 * w + i3;
					float best2 = dist[b];
					if (best2 == 0)
						continue;
					if (i3 < w - 1)
						best2 = Math.Min(best2, dist[b + 1] + 1);
					if (k3 < w - 1)
					{
						best2 = Math.Min(best2, dist[b + w] + 1);
						if (i3 < w - 1)
							best2 = Math.Min(best2, dist[b + w + 1] + 1.4142);
						if (i3 > 0)
							best2 = Math.Min(best2, dist[b + w - 1] + 1.4142);
					}
					dist[b] = best2;
				}
			}
		}
		float cap = mg + 1;
		if (BAND_INCR)
			DistPrep(d);     // stage 2 C1: the distances are kept too
		for (int lk2 = 0; lk2 < TV; lk2++)
		{
			for (int li2 = 0; li2 < TV; li2++)
			{
				float dm = dist[(lk2 + mg) * w + li2 + mg] * CELL;
				float band = 0;
				if (dm <= BAND_IN)
				{
					band = 1;
				}
				else if (dm < BAND_IN + BAND_FADE)
				{
					float f = (dm - BAND_IN) / BAND_FADE;
					band = 0.5 + 0.5 * Math.Cos(f * Math.PI);
				}
				if (d)
					d.m_aBand[lk2 * TV + li2] = band;
				else
					m_F.m_aBand[(tk * TQ + lk2) * N + ti * TQ + li2] = band;
				if (BAND_INCR)
				{
					float dv = dist[(lk2 + mg) * w + li2 + mg];
					if (dv > cap)
						dv = cap;
					if (d)
						d.m_Dist.m_aDist[lk2 * TV + li2] = dv;
					else
						m_DistF.m_aDist[(tk * TQ + lk2) * N + ti * TQ + li2] = dv;
				}
			}
		}
		if (BAND_INCR)
			DistDone(d, ti, tk);
		m_TchSt = null;
		m_TchFar = null;
		s_iBandMsAll += System.GetTickCount(tB);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1 / C2: band by distance (cells) - the same curve as BandTile (1 within BAND_IN, a cosine fall to 0
	//! at BAND_IN + BAND_FADE), 32 steps a cell, read with a straight line between steps (LabSet; first use)
	static void BandLutBuild()
	{
		for (int j = 0; j < LUT_N; j++)
		{
			float dm = j / 32.0 * CELL;
			float band = 0;
			if (dm <= BAND_IN)
			{
				band = 1;
			}
			else if (dm < BAND_IN + BAND_FADE)
			{
				float f = (dm - BAND_IN) / BAND_FADE;
				band = 0.5 + 0.5 * Math.Cos(f * Math.PI);
			}
			s_aBandLut[j] = band;
		}
		s_bLutOk = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1 / C2: band at distance dv (cells) from the table
	protected float BandOf(float dv)
	{
		float fj = dv * 32;
		int j = fj;
		if (j >= LUT_N - 1)
			return 0;
		float b0 = s_aBandLut[j];
		return b0 + (s_aBandLut[j + 1] - b0) * (fj - j);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1: kept distances of an older switch / preset are dropped (every live slot due again); holders made
	protected void DistGenCheck()
	{
		if (!m_DistF)
		{
			m_DistF = new MUD_MudSheetDist();
			m_DistT = new MUD_MudSheetDist();
		}
		if (m_iDistGen == s_iDistGen)
			return;
		m_iDistGen = s_iDistGen;
		for (int t = 0; t < TILES * TILES; t++)
			m_aSlotDist[t] = 0;
		m_aNewGi.Clear();
		m_aNewGk.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1, BandTile before writing distances: d's holder (a store) or the window's
	protected void DistPrep(MUD_MudSheetTileData d)
	{
		DistGenCheck();
		if (d && !d.m_Dist)
			d.m_Dist = new MUD_MudSheetTileDist();
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1, BandTile done: d's distances (or live slot ti, tk's) are current
	protected void DistDone(MUD_MudSheetTileData d, int ti, int tk)
	{
		if (d)
			d.m_Dist.m_iGen = s_iDistGen;
		else
			m_aSlotDist[tk * TILES + ti] = 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C2 BAND_MICRO: BandTile with direct m_F.m_aTouch reads when the tile + margin lies in the window, if
	//! compares instead of Math.Min and the band table instead of Math.Cos (same distances, band within the table's
	//! step error)
	protected void BandTileMicro(int tx, int tz, MUD_MudSheetTileData d, int ti, int tk)
	{
		int tB = System.GetTickCount();
		if (!s_bLutOk)
			BandLutBuild();
		m_iTchKey = -1;
		m_TchSt = null;
		m_TchFar = null;
		int mg = BandMargin();
		int w = TV + 2 * mg;
		int gi0 = tx * TQ - mg;
		int gk0 = tz * TQ - mg;
		int wi0 = gi0 - m_iX0;
		int wk0 = gk0 - m_iZ0;
		bool inWin = m_bPlaced && wi0 >= 0 && wk0 >= 0 && wi0 + w <= N && wk0 + w <= N;
		float dist[2401];      // BAND_W x BAND_W
		bool any = false;
		for (int k = 0; k < w; k++)
		{
			for (int i = 0; i < w; i++)
			{
				int li = i - mg;
				int lk = k - mg;
				int tc;
				if (d && li >= 0 && li < TV && lk >= 0 && lk < TV)
					tc = d.m_aTouch[lk * TV + li];
				else if (inWin)
					tc = m_F.m_aTouch[(wk0 + k) * N + wi0 + i];
				else
					tc = TouchAt(gi0 + i, gk0 + k);
				if (tc != 0)
				{
					dist[k * w + i] = 0;
					any = true;
				}
				else
				{
					dist[k * w + i] = 1000000;
				}
			}
		}
		if (any)
		{
			for (int k2 = 0; k2 < w; k2++)
			{
				for (int i2 = 0; i2 < w; i2++)
				{
					int a = k2 * w + i2;
					float best = dist[a];
					if (best == 0)
						continue;
					float c;
					if (i2 > 0)
					{
						c = dist[a - 1] + 1;
						if (c < best)
							best = c;
					}
					if (k2 > 0)
					{
						c = dist[a - w] + 1;
						if (c < best)
							best = c;
						if (i2 > 0)
						{
							c = dist[a - w - 1] + 1.4142;
							if (c < best)
								best = c;
						}
						if (i2 < w - 1)
						{
							c = dist[a - w + 1] + 1.4142;
							if (c < best)
								best = c;
						}
					}
					dist[a] = best;
				}
			}
			for (int k3 = w - 1; k3 >= 0; k3--)
			{
				for (int i3 = w - 1; i3 >= 0; i3--)
				{
					int b = k3 * w + i3;
					float best2 = dist[b];
					if (best2 == 0)
						continue;
					float c2;
					if (i3 < w - 1)
					{
						c2 = dist[b + 1] + 1;
						if (c2 < best2)
							best2 = c2;
					}
					if (k3 < w - 1)
					{
						c2 = dist[b + w] + 1;
						if (c2 < best2)
							best2 = c2;
						if (i3 < w - 1)
						{
							c2 = dist[b + w + 1] + 1.4142;
							if (c2 < best2)
								best2 = c2;
						}
						if (i3 > 0)
						{
							c2 = dist[b + w - 1] + 1.4142;
							if (c2 < best2)
								best2 = c2;
						}
					}
					dist[b] = best2;
				}
			}
		}
		float cap = mg + 1;
		if (BAND_INCR)
			DistPrep(d);
		for (int lk2 = 0; lk2 < TV; lk2++)
		{
			for (int li2 = 0; li2 < TV; li2++)
			{
				float dv = dist[(lk2 + mg) * w + li2 + mg];
				if (dv > cap)
					dv = cap;
				float band = BandOf(dv);
				if (d)
				{
					d.m_aBand[lk2 * TV + li2] = band;
					if (BAND_INCR)
						d.m_Dist.m_aDist[lk2 * TV + li2] = dv;
				}
				else
				{
					int wv = (tk * TQ + lk2) * N + ti * TQ + li2;
					m_F.m_aBand[wv] = band;
					if (BAND_INCR)
						m_DistF.m_aDist[wv] = dv;
				}
			}
		}
		if (BAND_INCR)
			DistDone(d, ti, tk);
		m_TchSt = null;
		m_TchFar = null;
		s_iBandMsAll += System.GetTickCount(tB);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1: live slot t's kept window distances are current (its band needs no BandTile)
	protected bool DistOk(int t)
	{
		return m_aSlotDist[t] != 0 && m_iDistGen == s_iDistGen;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1 BAND_INCR, once per Upload and before a Follow shift: the cells touched since the last call lower the
	//! kept distances round them. Box = the new cells + BandMargin, seeded from the kept distances (new cells 0, no
	//! TouchAt), two chamfer passes inside the box only; cells whose distance fell get their band from the table, and
	//! only slots where a band value changed are dirtied. Exact: touches only go 0 -> 1, and a chamfer path between two
	//! cells stays inside their box. Slots in the box without current distances get a BandTile first.
	protected void BandIncr()
	{
		if (m_aNewGi.IsEmpty() || !m_bPlaced)
		{
			m_aNewGi.Clear();
			m_aNewGk.Clear();
			return;
		}
		int tB = System.GetTickCount();
		int bandAt = s_iBandMsAll;
		DistGenCheck();
		if (!s_bLutOk)
			BandLutBuild();
		int mg = BandMargin();
		int i0 = N;
		int i1 = -1;
		int k0 = N;
		int k1 = -1;
		m_aNewV.Clear();
		for (int n = 0; n < m_aNewGi.Count(); n++)
		{
			int ni = m_aNewGi[n] - m_iX0;
			int nk = m_aNewGk[n] - m_iZ0;
			if (ni < 0 || ni >= N || nk < 0 || nk >= N)
				continue;
			m_aNewV.Insert(nk * N + ni);
			if (ni < i0)
				i0 = ni;
			if (ni > i1)
				i1 = ni;
			if (nk < k0)
				k0 = nk;
			if (nk > k1)
				k1 = nk;
		}
		m_aNewGi.Clear();
		m_aNewGk.Clear();
		if (i1 < 0)
			return;
		int bi0 = i0 - mg;
		if (bi0 < 0)
			bi0 = 0;
		int bi1 = i1 + mg;
		if (bi1 > N - 1)
			bi1 = N - 1;
		int bk0 = k0 - mg;
		if (bk0 < 0)
			bk0 = 0;
		int bk1 = k1 + mg;
		if (bk1 > N - 1)
			bk1 = N - 1;
		// slots in the box need current distances first (BandTile: exact, the new touches included)
		int ta = 0;
		if (bi0 > 0)
			ta = (bi0 - 1) / TQ;
		int tb = bi1 / TQ;
		if (tb > TILES - 1)
			tb = TILES - 1;
		int ka = 0;
		if (bk0 > 0)
			ka = (bk0 - 1) / TQ;
		int kb = bk1 / TQ;
		if (kb > TILES - 1)
			kb = TILES - 1;
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		for (int sk = ka; sk <= kb; sk++)
		{
			for (int si = ta; si <= tb; si++)
			{
				int st = sk * TILES + si;
				if (m_aSlotDist[st] != 0)
					continue;
				BandTile(x0 + si, z0 + sk, null, si, sk);
				m_aSlotBand[st] = 0;
				m_aSlotDirty[st] = 1;
				m_aSlotWhy[st] = m_aSlotWhy[st] | WHY_BAND;
				m_bDirty = true;
				m_iIncrFull++;
			}
		}
		MUD_MudSheetDist df = m_DistF;
		foreach (int nv : m_aNewV)
		{
			if (df.m_aDist[nv] != 0)
			{
				df.m_aDist[nv] = 0;
				df.m_aChg[nv] = 1;
			}
		}
		for (int k = bk0; k <= bk1; k++)
		{
			for (int i = bi0; i <= bi1; i++)
			{
				int a = k * N + i;
				float best = df.m_aDist[a];
				if (best == 0)
					continue;
				float c;
				if (i > bi0)
				{
					c = df.m_aDist[a - 1] + 1;
					if (c < best)
						best = c;
				}
				if (k > bk0)
				{
					c = df.m_aDist[a - N] + 1;
					if (c < best)
						best = c;
					if (i > bi0)
					{
						c = df.m_aDist[a - N - 1] + 1.4142;
						if (c < best)
							best = c;
					}
					if (i < bi1)
					{
						c = df.m_aDist[a - N + 1] + 1.4142;
						if (c < best)
							best = c;
					}
				}
				if (best < df.m_aDist[a])
				{
					df.m_aDist[a] = best;
					df.m_aChg[a] = 1;
				}
			}
		}
		for (int k2 = bk1; k2 >= bk0; k2--)
		{
			for (int i2 = bi1; i2 >= bi0; i2--)
			{
				int b = k2 * N + i2;
				float best2 = df.m_aDist[b];
				if (best2 == 0)
					continue;
				float c2;
				if (i2 < bi1)
				{
					c2 = df.m_aDist[b + 1] + 1;
					if (c2 < best2)
						best2 = c2;
				}
				if (k2 < bk1)
				{
					c2 = df.m_aDist[b + N] + 1;
					if (c2 < best2)
						best2 = c2;
					if (i2 < bi1)
					{
						c2 = df.m_aDist[b + N + 1] + 1.4142;
						if (c2 < best2)
							best2 = c2;
					}
					if (i2 > bi0)
					{
						c2 = df.m_aDist[b + N - 1] + 1.4142;
						if (c2 < best2)
							best2 = c2;
					}
				}
				if (best2 < df.m_aDist[b])
				{
					df.m_aDist[b] = best2;
					df.m_aChg[b] = 1;
				}
			}
		}
		// bands of the lowered cells; slots (a side cell: both) where a value changed are dirtied
		int mask = 0;
		for (int k3 = bk0; k3 <= bk1; k3++)
		{
			for (int i3 = bi0; i3 <= bi1; i3++)
			{
				int e = k3 * N + i3;
				if (df.m_aChg[e] == 0)
					continue;
				df.m_aChg[e] = 0;
				float band = BandOf(df.m_aDist[e]);
				if (band == m_F.m_aBand[e])
					continue;
				m_F.m_aBand[e] = band;
				int ri = i3 % TQ;
				int rk = k3 % TQ;
				int ma = i3 / TQ;
				int mb = ma;
				if (ri == 0 && ma > 0)
					ma = ma - 1;
				if (mb > TILES - 1)
					mb = TILES - 1;
				int mc = k3 / TQ;
				int md = mc;
				if (rk == 0 && mc > 0)
					mc = mc - 1;
				if (md > TILES - 1)
					md = TILES - 1;
				for (int mk = mc; mk <= md; mk++)
				{
					for (int mi = ma; mi <= mb; mi++)
						mask = mask | (1 << (mk * TILES + mi));
				}
			}
		}
		for (int t = 0; t < TILES * TILES; t++)
		{
			if ((mask & (1 << t)) == 0)
				continue;
			m_aSlotDirty[t] = 1;
			m_aSlotWhy[t] = m_aSlotWhy[t] | WHY_BAND;
			m_bDirty = true;
			m_iIncrDirty++;
		}
		m_iIncrRuns++;
		m_iIncrCells += (bi1 - bi0 + 1) * (bk1 - bk0 + 1);
		s_iBandMsAll += System.GetTickCount(tB) - (s_iBandMsAll - bandAt);     // its BandTiles counted once
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1, Follow after the field moved by sti, stk tiles: the kept distances and slot flags move with it
	//! (entering slots: not current until DistUnpack / BandTile)
	protected void DistShift(int sti, int stk)
	{
		DistGenCheck();
		int si = sti * TQ;
		int sk = stk * TQ;
		float cap = BandMargin() + 1;
		MUD_MudSheetDist f = m_DistF;
		MUD_MudSheetDist nt = m_DistT;
		for (int k = 0; k < N; k++)
		{
			for (int i = 0; i < N; i++)
			{
				int oi = i + si;
				int ok = k + sk;
				int v = k * N + i;
				nt.m_aChg[v] = 0;
				if (oi >= 0 && oi < N && ok >= 0 && ok < N)
					nt.m_aDist[v] = f.m_aDist[ok * N + oi];
				else
					nt.m_aDist[v] = cap;
			}
		}
		m_DistF = nt;
		m_DistT = f;
		int nd[16];
		for (int t = 0; t < TILES * TILES; t++)
			nd[t] = 0;
		for (int tk = 0; tk < TILES; tk++)
		{
			for (int ti = 0; ti < TILES; ti++)
			{
				int nti = ti - sti;
				int ntk = tk - stk;
				if (nti >= 0 && nti < TILES && ntk >= 0 && ntk < TILES)
					nd[ntk * TILES + nti] = m_aSlotDist[tk * TILES + ti];
			}
		}
		for (int t2 = 0; t2 < TILES * TILES; t2++)
			m_aSlotDist[t2] = nd[t2];
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1, Freeze after Pack: live slot ti, tk's distances go with its store (none if not current)
	protected void DistPack(int ti, int tk, MUD_MudSheetTileData d)
	{
		if (!DistOk(tk * TILES + ti))
		{
			d.m_Dist = null;
			return;
		}
		if (!d.m_Dist)
			d.m_Dist = new MUD_MudSheetTileDist();
		for (int lk = 0; lk < TV; lk++)
		{
			for (int li = 0; li < TV; li++)
				d.m_Dist.m_aDist[lk * TV + li] = m_DistF.m_aDist[(tk * TQ + lk) * N + ti * TQ + li];
		}
		d.m_Dist.m_iGen = s_iDistGen;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1, Follow after Unpack: a store's current distances come back into slot t (ti, tk); none, stale or its
	//! band still queued (m_aBandQ) -> the slot is not current (BandTile before its build, as before)
	protected void DistUnpack(MUD_MudSheetTileData d, int key, int t, int ti, int tk)
	{
		MUD_MudSheetTileDist sd = d.m_Dist;
		if (!sd || sd.m_iGen != s_iDistGen || m_aBandQ.Contains(key))
			return;
		for (int lk = 0; lk < TV; lk++)
		{
			for (int li = 0; li < TV; li++)
				m_DistF.m_aDist[(tk * TQ + lk) * N + ti * TQ + li] = sd.m_aDist[lk * TV + li];
		}
		m_aSlotDist[t] = 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 C1 TEST_KIT bench (once, first Upload with BAND_INCR): 1000 random stamps (discs r 0..2 cells) through
	//! BandIncr every 10 stamps, then every live slot's band again from scratch (BandTile into a scratch store) - max
	//! |incremental - full| of band and distance. The window is put back as it was (touches, bands, distances, slots).
	protected void BandBench()
	{
		m_bBenchDone = true;
		if (!m_bPlaced)
			return;
		int bandAll = s_iBandMsAll;
		bool wasDirty = m_bDirty;
		DistGenCheck();
		MUD_MudSheetBench sv = new MUD_MudSheetBench();
		for (int v = 0; v < NN; v++)
		{
			sv.m_aTouch[v] = m_F.m_aTouch[v];
			sv.m_aBand[v] = m_F.m_aBand[v];
			sv.m_aDist[v] = m_DistF.m_aDist[v];
		}
		for (int t = 0; t < TILES * TILES; t++)
		{
			sv.m_aSlot[t] = m_aSlotDist[t];
			sv.m_aSlot[16 + t] = m_aSlotBand[t];
			sv.m_aSlot[32 + t] = m_aSlotDirty[t];
			sv.m_aSlot[48 + t] = m_aSlotWhy[t];
		}
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		for (int t1 = 0; t1 < TILES * TILES; t1++)
		{
			int ti1 = t1 % TILES;
			int tk1 = t1 / TILES;
			if (m_aSlotDist[t1] == 0)
				BandTile(x0 + ti1, z0 + tk1, null, ti1, tk1);
		}
		int tI = System.GetTickCount();
		int news = 0;
		for (int s = 0; s < 1000; s++)
		{
			int ci = Math.RandomInt(0, N);
			int ck = Math.RandomInt(0, N);
			int r = Math.RandomInt(0, 3);
			for (int dk = -r; dk <= r; dk++)
			{
				for (int di = -r; di <= r; di++)
				{
					int i = ci + di;
					int k = ck + dk;
					if (i < 0 || i >= N || k < 0 || k >= N || di * di + dk * dk > r * r)
						continue;
					if (m_F.m_aTouch[k * N + i] != 0)
						continue;
					m_F.m_aTouch[k * N + i] = 1;
					m_aNewGi.Insert(m_iX0 + i);
					m_aNewGk.Insert(m_iZ0 + k);
					news++;
				}
			}
			int sm = s % 10;
			if (sm == 9)
				BandIncr();
		}
		BandIncr();
		int incrMs = System.GetTickCount(tI);
		MUD_MudSheetTileData sc = new MUD_MudSheetTileData();
		float maxBand = 0;
		float maxDist = 0;
		int at = -1;
		int tF = System.GetTickCount();
		for (int t2 = 0; t2 < TILES * TILES; t2++)
		{
			int ti = t2 % TILES;
			int tk = t2 / TILES;
			for (int lk = 0; lk < TV; lk++)
			{
				for (int li = 0; li < TV; li++)
					sc.m_aTouch[lk * TV + li] = m_F.m_aTouch[(tk * TQ + lk) * N + ti * TQ + li];
			}
			BandTile(x0 + ti, z0 + tk, sc, ti, tk);
			for (int lk2 = 0; lk2 < TV; lk2++)
			{
				for (int li2 = 0; li2 < TV; li2++)
				{
					int wv = (tk * TQ + lk2) * N + ti * TQ + li2;
					float db = Math.AbsFloat(sc.m_aBand[lk2 * TV + li2] - m_F.m_aBand[wv]);
					if (db > maxBand)
					{
						maxBand = db;
						at = wv;
					}
					float dd = Math.AbsFloat(sc.m_Dist.m_aDist[lk2 * TV + li2] - m_DistF.m_aDist[wv]);
					if (dd > maxDist)
						maxDist = dd;
				}
			}
		}
		int fullMs = System.GetTickCount(tF);
		Print(string.Format("MUD band incr bench: 1000 stamps (%1 new cells), incremental %2 ms in 101 passes, full BandTile x 16 %3 ms | max |incr - full| band %4 (window cell %5), distance %6 cells | band micro %7, within 1e-4: %8",
			news, incrMs, fullMs, maxBand, at, maxDist, BAND_MICRO, maxBand <= 0.0001), LogLevel.NORMAL);
		for (int v2 = 0; v2 < NN; v2++)
		{
			m_F.m_aTouch[v2] = sv.m_aTouch[v2];
			m_F.m_aBand[v2] = sv.m_aBand[v2];
			m_DistF.m_aDist[v2] = sv.m_aDist[v2];
			m_DistF.m_aChg[v2] = 0;
		}
		for (int t3 = 0; t3 < TILES * TILES; t3++)
		{
			m_aSlotDist[t3] = sv.m_aSlot[t3];
			m_aSlotBand[t3] = sv.m_aSlot[16 + t3];
			m_aSlotDirty[t3] = sv.m_aSlot[32 + t3];
			m_aSlotWhy[t3] = sv.m_aSlot[48 + t3];
		}
		m_aNewGi.Clear();
		m_aNewGk.Clear();
		m_bDirty = wasDirty;
		s_iBandMsAll = bandAll;
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL, after each Upload: stored tiles a band change reached - band again; a shown one is rebuilt
	protected void BandStored()
	{
		foreach (int key : m_aBandQ)
		{
			MUD_MudSheetStore st = TrailGet(key);     // MUD LAB part D: compacted since it was queued: back first
			if (!st)
				continue;
			BandTile(st.m_iTx, st.m_iTz, st.m_D, 0, 0);
			if (!st.m_Ent)
				continue;     // data only: its mesh comes on approach with this band
			if (!m_mQWhy.Contains(key))
				m_mQWhy.Set(key, BW_BAND);
			QueueBuild(st.m_iTx, st.m_iTz);
		}
		m_aBandQ.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Builds-by-reason: what live slot t's Upload build is for (mesh-less first, then slump-only / band-only dirt,
	//! then the reason UploadPick picked it)
	protected int UploadWhy(int t)
	{
		int pw = m_aPickWhy[t];
		if (pw == BW_MESHLESS || pw == BW_COBUILD)
			return pw;
		int w = m_aSlotWhy[t];
		if (w == WHY_SLUMP)
			return BW_SLUMP;
		if (w == WHY_BAND)
			return BW_BAND;
		if (pw >= 0)
			return pw;
		return BW_NEAREST;
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL: the heights live slot ti, tk was just drawn with (m_TD) back to the field ('would skip' meter)
	protected void CopyLastY(int ti, int tk)
	{
		for (int lk = 0; lk < TV; lk++)
		{
			for (int li = 0; li < TV; li++)
				m_F.m_aLastY[(tk * TQ + lk) * N + ti * TQ + li] = m_TD.m_aLastY[lk * TV + li];
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL Freeze: the slot's ground eases end at their final ground; its band is brought up to date
	protected void FreezeLocal(int tx, int tz, int ti, int tk)
	{
		if (!m_mEase.IsEmpty())
		{
			int i0 = ti * TQ;
			int k0 = tk * TQ;
			array<int> done = {};
			foreach (int key, MUD_MudSheetEase e : m_mEase)
			{
				int i = e.m_iGi - m_iX0;
				int k = e.m_iGk - m_iZ0;
				if (i < i0 || i > i0 + TQ || k < k0 || k > k0 + TQ)
					continue;
				int v = k * N + i;
				if (m_F.m_aGs[v] != 0)
				{
					m_F.m_aG[v] = e.m_fTo;
					FillAround(i, k);
				}
				done.Insert(key);
			}
			foreach (int dk : done)
				m_mEase.Remove(dk);
		}
		int t = tk * TILES + ti;
		if (m_aSlotBand[t] != 0)
		{
			m_aSlotBand[t] = 0;
			if (!BAND_INCR || !DistOk(t))
				BandTile(tx, tz, null, ti, tk);     // stage 2 C1: not when the kept distances are current
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Grounds: window cell i, k lies on a zone slot. SHEET LOCAL: any slot that holds it (side and corner cells are
	//! shared, and a born slot next to an unborn one needs its side traced too)
	protected bool ZoneAtCellAny(int i, int k)
	{
		if (!m_bLocal)
			return ZoneAtCell(i, k);
		if (i < 0 || k < 0 || i >= N || k >= N)
			return false;
		int ri = i % TQ;
		int rk = k % TQ;
		int ta = i / TQ;
		int tb = ta;
		if (ri == 0 && ta > 0)
			ta = ta - 1;
		int ka = k / TQ;
		int kb = ka;
		if (rk == 0 && ka > 0)
			ka = ka - 1;
		if (ta >= TILES)
			ta = TILES - 1;
		if (tb >= TILES)
			tb = TILES - 1;
		if (ka >= TILES)
			ka = TILES - 1;
		if (kb >= TILES)
			kb = TILES - 1;
		for (int tk = ka; tk <= kb; tk++)
		{
			for (int ti = ta; ti <= tb; ti++)
			{
				if (m_aSlotZone[tk * TILES + ti] != 0)
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL holes (DoDigNow): live zone slots a crater / dig marked are built in the blast frame (not through
	//! UploadPick / UPLOAD_SLOTS / SLOT_MIN_S). A slot not traced yet stays dirty for Upload.
	protected int BuildNowSlots()
	{
		if (!m_bPlaced)
			return 0;
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		int built = 0;
		for (int t = 0; t < TILES * TILES; t++)
		{
			if (m_aSlotNow[t] == 0)
				continue;
			m_aSlotNow[t] = 0;
			int ti = t % TILES;
			int tk = t / TILES;
			bool traceNow = false;
			if (CRATER_TRACE_NOW && m_aSlotZone[t] != 0 && t < m_aTileEnt.Count() && m_aTileEnt[t] && !SlotTraced(t))
			{
				TraceSlotNow(ti, tk);     // stage 2 B2 (O1a): its missing cells now, built below
				traceNow = true;
			}
			if (m_aSlotZone[t] == 0 || t >= m_aTileEnt.Count() || !m_aTileEnt[t] || !SlotTraced(t))
			{
				if (MUD_TestKit.TEST_KIT)
					CraterSkip(t, x0 + ti, z0 + tk);     // stage 2 B1 crater log
				continue;
			}
			if (MUD_TestKit.TEST_KIT && traceNow)
				MUD_CraterTrace.Tile(MUD_CraterTrace.C_TRACED_NOW, x0 + ti, z0 + tk, true);
			else if (MUD_TestKit.TEST_KIT)
				MUD_CraterTrace.Tile(MUD_CraterTrace.C_LIVE_BUILT, x0 + ti, z0 + tk, true);
			if (m_aSlotBand[t] != 0)
			{
				m_aSlotBand[t] = 0;
				if (!BAND_INCR || !DistOk(t))
					BandTile(x0 + ti, z0 + tk, null, ti, tk);     // stage 2 C1: not when the kept distances are current
			}
			Pack(ti, tk, m_TD);
			m_iBuildWhy = BW_HOLE;
			Resource r = BuildTile(m_aTileEnt[t], m_TD, x0 + ti, z0 + tk, true);
			if (r)
				m_aTileRes[t] = r;
			CopyLastY(ti, tk);
			m_aSlotDirty[t] = 0;
			m_aSlotWhy[t] = 0;
			m_aSlotWait[t] = 0;
			m_aSlotBuiltMs[t] = Math.Round(m_fLast * 1000);
			built++;
		}
		return built;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 B2 CRATER_TRACE_NOW (O1a): live slot ti, tk - every untraced even cell traced now and the cells between
	//! filled, as Grounds would (Trace reads HoleGround over our holes), so BuildNowSlots builds it in the go-live frame
	protected void TraceSlotNow(int ti, int tk)
	{
		int i0 = ti * TQ;
		int k0 = tk * TQ;
		for (int k = k0; k <= k0 + TQ; k += 2)
		{
			for (int i = i0; i <= i0 + TQ; i += 2)
			{
				if (m_F.m_aGs[k * N + i] != 0)
					continue;
				Trace(i, k);
				m_iUntraced--;
				m_bIdxDirty = true;
			}
		}
		for (int fk = k0; fk <= k0 + TQ; fk += 2)
		{
			for (int fi = i0; fi <= i0 + TQ; fi += 2)
				FillAround(fi, fk);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 B1 crater log (TEST_KIT): why BuildNowSlots skipped now-slot t (world tile tx, tz)
	protected void CraterSkip(int t, int tx, int tz)
	{
		if (m_aSlotZone[t] == 0)
			MUD_CraterTrace.Tile(MUD_CraterTrace.C_SKIP_UNBORN, tx, tz, false);
		else if (t >= m_aTileEnt.Count() || !m_aTileEnt[t])
			MUD_CraterTrace.Tile(MUD_CraterTrace.C_SKIP_NOENT, tx, tz, true);
		else
			MUD_CraterTrace.Tile(MUD_CraterTrace.C_SKIP_UNTRACED, tx, tz, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 B3 CRATER_TAPER_NOW (O1b), right after BuildNowSlots: the BW_HOLE entries at the front of m_aOneBuild
	//! (DigNow's taper neighbours) are built in the go-live frame too, up to TAPER_NOW_MAX tiles / TAPER_NOW_MS ms;
	//! the rest stay in front for FlushOneBuild (logged). Same build as FlushOneBuild (no A5 rendered gate).
	protected int TaperNow()
	{
		int t0 = System.GetTickCount();
		int built = 0;
		while (!m_aOneBuild.IsEmpty())
		{
			int key = m_aOneBuild[0];
			int qWhy;
			if (!m_mQWhy.Find(key, qWhy) || qWhy != BW_HOLE)
				break;
			if (built >= TAPER_NOW_MAX || System.GetTickCount(t0) >= TAPER_NOW_MS)
			{
				int left = 0;
				for (int q = 0; q < m_aOneBuild.Count(); q++)
				{
					int lw;
					if (!m_mQWhy.Find(m_aOneBuild[q], lw) || lw != BW_HOLE)
						break;
					left++;
				}
				Print(string.Format("MUD sheet taper now: cap hit after %1 tiles / %2 ms, %3 hole tiles left for the next frame", built, System.GetTickCount(t0), left), LogLevel.NORMAL);
				break;
			}
			m_aOneBuild.RemoveOrdered(0);
			m_mQWhy.Remove(key);
			MUD_MudSheetStore st = m_mTrail.Get(key);
			if (!st || !st.m_Ent)
				continue;
			int tx = Math.Round(key / 20000.0);
			int tz = key - tx * 20000;
			if (IsLive(tx, tz))
				continue;
			if (MUD_TestKit.TEST_KIT)
				MUD_CraterTrace.Tile(MUD_CraterTrace.C_TAPER_NOW, tx, tz, false);
			m_iBuildWhy = BW_HOLE;
			Resource res = BuildTile(st.m_Ent, st.m_D, tx, tz, false);
			if (res)
				st.m_Res = res;
			built++;
		}
		return built;
	}

	//------------------------------------------------------------------------------------------------
	//! EDGE AUDIT: tile vertex l of border vert j on side s (0 south k 0, 1 north k TQ, 2 west i 0, 3 east i TQ)
	protected int EdgeL(int s, int j)
	{
		if (s == 0)
			return j;
		if (s == 1)
			return TQ * TV + j;
		if (s == 2)
			return j * TV;
		return j * TV + TQ;
	}

	//------------------------------------------------------------------------------------------------
	//! EDGE AUDIT: tile vertex a (lower corner) of quad q along side s, on the row / column next to the side
	protected int EdgeQuad(int s, int q)
	{
		if (s == 0)
			return q;
		if (s == 1)
			return (TQ - 1) * TV + q;
		if (s == 2)
			return q * TV;
		return q * TV + TQ - 1;
	}

	//------------------------------------------------------------------------------------------------
	//! EDGE AUDIT, after each build of tile tx, tz: per border vert (S N W E x TV) the drawn world y (-10000 not drawn:
	//! untraced quad, -20000 band 0 quads, -30000 no mesh), its height over the ground and its band. Drawn = a quad of
	//! this tile beside it passed BuildTileMesh's traced + band skips (bare / hole skips not counted)
	protected void EdgeRecord(MUD_MudSheetTileData d, int tx, int tz, bool drawn)
	{
		array<float> ea = {};
		ea.Resize(12 * TV);
		for (int s = 0; s < 4; s++)
		{
			for (int j = 0; j < TV; j++)
			{
				int l = EdgeL(s, j);
				int e = s * TV + j;
				ea[4 * TV + e] = d.m_aLastY[l] - d.m_aG[l];
				ea[8 * TV + e] = d.m_aBand[l];
				float y = -20000;
				for (int q = j - 1; q <= j; q++)
				{
					if (q < 0 || q >= TQ)
						continue;
					int la = EdgeQuad(s, q);
					if (d.m_aGs[la] == 0 || d.m_aGs[la + 1] == 0 || d.m_aGs[la + TV] == 0 || d.m_aGs[la + TV + 1] == 0)
					{
						if (y < -9000)
							y = -10000;
						continue;
					}
					if (d.m_aBand[la] > 0 || d.m_aBand[la + 1] > 0 || d.m_aBand[la + TV] > 0 || d.m_aBand[la + TV + 1] > 0)
						y = d.m_aLastY[l];
				}
				if (!drawn)
					y = -30000;
				ea[e] = y;
			}
		}
		m_mEdgeRec.Set(TrailKey(tx, tz), ea);
	}

	//------------------------------------------------------------------------------------------------
	//! EDGE AUDIT: is world tile key's mesh on screen now (live slot or stored tile, entity visible)?
	protected bool EdgeDrawn(int key)
	{
		int tx = key / 20000;
		int tz = key - tx * 20000;
		IEntity e;
		if (m_bPlaced && IsLive(tx, tz))
		{
			int t = (tz - m_iZ0 / TQ) * TILES + tx - m_iX0 / TQ;
			if (t < 0 || t >= m_aTileEnt.Count() || t >= m_aTileRes.Count() || m_aSlotZone[t] == 0 || !m_aTileRes[t])
				return false;
			e = m_aTileEnt[t];
		}
		else
		{
			MUD_MudSheetStore st = m_mTrail.Get(key);
			if (!st || !st.m_Res)
				return false;
			e = st.m_Ent;
		}
		return e && (e.GetFlags() & EntityFlags.VISIBLE) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! EDGE AUDIT 10 s part: every border vert (corners left out) of a drawn tile drawn EDGE_RAISED over the ground,
	//! against the facing vert of the tile beside it: ok, a step (beside drawn lower), or open (beside not drawn: tile
	//! not born, no mesh, its vert untraced, its band 0 there). Records of tiles no longer drawn go.
	protected string EdgeAudit()
	{
		if (!EDGE_AUDIT)
			return "";
		int raised = 0;
		int ok = 0;
		int step = 0;
		float stepMax = 0;
		int stepBand = 0;
		int open = 0;
		float openMax = 0;
		int nbUnborn = 0;
		int nbNoMesh = 0;
		int nbUnt = 0;
		int nbBand0 = 0;
		array<int> gone = {};
		foreach (int key, array<float> ea : m_mEdgeRec)
		{
			if (!ea || !EdgeDrawn(key))
			{
				gone.Insert(key);
				continue;
			}
			int tx = key / 20000;
			int tz = key - tx * 20000;
			for (int s = 0; s < 4; s++)
			{
				int nk = TrailKey(tx + 1, tz);
				int os = 2;
				if (s == 0)
				{
					nk = TrailKey(tx, tz - 1);
					os = 1;
				}
				else if (s == 1)
				{
					nk = TrailKey(tx, tz + 1);
					os = 0;
				}
				else if (s == 2)
				{
					nk = TrailKey(tx - 1, tz);
					os = 3;
				}
				array<float> nb = null;
				if (EdgeDrawn(nk))
					nb = m_mEdgeRec.Get(nk);
				for (int j = 1; j < TQ; j++)
				{
					int e = s * TV + j;
					float rz = ea[4 * TV + e];
					if (ea[e] < -9000 || rz < EDGE_RAISED)
						continue;
					raised++;
					float ny = -30000;
					if (nb)
						ny = nb[os * TV + j];
					if (ny < -9000)
					{
						open++;
						openMax = Math.Max(openMax, rz);
						if (!m_mBorn.Contains(nk))
							nbUnborn++;
						else if (ny > -15000)
							nbUnt++;
						else if (ny > -25000)
							nbBand0++;
						else
							nbNoMesh++;
						continue;
					}
					float dy = ea[e] - ny;
					if (dy > EDGE_STEP)
					{
						step++;
						stepMax = Math.Max(stepMax, dy);
						if (nb[8 * TV + os * TV + j] < ea[8 * TV + e] - 0.05)
							stepBand++;
						continue;
					}
					ok++;
				}
			}
		}
		foreach (int g : gone)
			m_mEdgeRec.Remove(g);
		int stepCm = Math.Round(stepMax * 100);
		int openCm = Math.Round(openMax * 100);
		string a = string.Format(" | edge audit: raised border verts %1: ok %2, step > 3 cm %3 (max %4 cm, beside band lower %5)",
			raised, ok, step, stepCm, stepBand);
		return a + string.Format(", open %1 (max %2 cm: beside not born %3, no mesh %4, vert untraced %5, band 0 %6)",
			open, openCm, nbUnborn, nbNoMesh, nbUnt, nbBand0);
	}

	//------------------------------------------------------------------------------------------------
	//! A3 with EDGE_AUDIT off: the edge records of tiles no longer drawn go (EdgeAudit does it while it runs)
	protected void EdgePrune()
	{
		array<int> gone = {};
		foreach (int key, array<float> ea : m_mEdgeRec)
		{
			if (!ea || !EdgeDrawn(key))
				gone.Insert(key);
		}
		foreach (int g : gone)
			m_mEdgeRec.Remove(g);
	}

	//------------------------------------------------------------------------------------------------
	//! A2: m_aWheelSlot[t] = 1 where a wheel contact of a vehicle lies in live slot t (as PickWheelSlots finds them)
	protected void WheelSlotMask()
	{
		for (int c = 0; c < TILES * TILES; c++)
			m_aWheelSlot[c] = 0;
		array<Vehicle> vs = MUD_Wheels.GetAll();
		if (LEV_L8_VEH)
			vs = m_aNearVeh;
		foreach (int vi, Vehicle v : vs)
		{
			if (!v)
				continue;
			VehicleWheeledSimulation sim;
			if (LEV_L8_VEH)
				sim = m_aNearSim[vi];
			else
				sim = VehicleWheeledSimulation.Cast(v.FindComponent(VehicleWheeledSimulation));
			if (!sim)
				continue;
			int count = sim.WheelCount();
			for (int w = 0; w < count; w++)
			{
				if (!sim.WheelHasContact(w))
					continue;
				vector cp = sim.WheelGetContactPosition(w);
				int i = Math.Round(cp[0] / CELL) - m_iX0;
				int k = Math.Round(cp[2] / CELL) - m_iZ0;
				if (i < 0 || i >= N || k < 0 || k >= N)
					continue;
				int ti = Math.Min(i / TQ, TILES - 1);     // ints first: Math.Min returns float
				int tk = Math.Min(k / TQ, TILES - 1);
				m_aWheelSlot[tk * TILES + ti] = 1;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 part A, after UploadPick. A2 (EDGE_COBUILD_PICK): a dirty zone slot dirtied by the band or a stamp, beside
	//! a picked wheel slot, is picked too (no SLOT_MIN_S hold, outside UPLOAD_SLOTS; Upload still waits for its traces).
	//! A3 (EDGE_COBUILD): slots a co-build asked for at the last upload (m_aCoNext) are picked as co-builds.
	protected void CoPick()
	{
		if (EDGE_COBUILD)
		{
			for (int n = 0; n < TILES * TILES; n++)
			{
				if (m_aCoNext[n] == 0)
					continue;
				m_aCoNext[n] = 0;
				if (m_aSlotZone[n] == 0)
					continue;
				m_aSlotDirty[n] = 1;
				m_aPick[n] = 1;
				m_aPickWhy[n] = BW_COBUILD;
				m_aCoLive[n] = 1;
			}
		}
		if (!EDGE_COBUILD_PICK)
			return;
		WheelSlotMask();
		for (int t = 0; t < TILES * TILES; t++)
		{
			if (m_aWheelSlot[t] == 0 || m_aSlotDirty[t] == 0 || m_aPick[t] != 1)
				continue;
			int ti = t % TILES;
			int tk = t / TILES;
			for (int s = 0; s < 4; s++)
			{
				int ni = ti;
				int nk = tk;
				if (s == 0)
					nk--;
				else if (s == 1)
					nk++;
				else if (s == 2)
					ni--;
				else
					ni++;
				if (ni < 0 || ni >= TILES || nk < 0 || nk >= TILES)
					continue;
				int u = nk * TILES + ni;
				if (m_aSlotDirty[u] == 0 || m_aSlotZone[u] == 0 || (m_aSlotWhy[u] & (WHY_BAND | WHY_STAMP)) == 0)
					continue;
				if (m_aPick[u] != 0 && m_aPick[u] != 2)
					continue;
				m_aPick[u] = 1;
				m_aPickWhy[u] = BW_COBUILD;
				m_iCoPick++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A3 EDGE_COBUILD, after a build of tile tx, tz (its edge record is new): each side whose border verts differ from
	//! the drawn neighbour's by more than EDGE_CO_STEP, or where one side is drawn raised and the other is not drawn,
	//! gets the neighbour built too. Live neighbour: in this upload (we are in Upload's slot loop) or the next one.
	//! Stored neighbour with an entity: now, at most CO_MAX per upload (the rest queued). Unborn neighbours: nothing to
	//! build. A co-build triggers no co-builds (m_bCoBuilding).
	protected void CoCheck(int tx, int tz)
	{
		array<float> ea = m_mEdgeRec.Get(TrailKey(tx, tz));
		if (!ea)
			return;
		for (int s = 0; s < 4; s++)
		{
			int ntx = tx + 1;
			int ntz = tz;
			int os = 2;
			if (s == 0)
			{
				ntx = tx;
				ntz = tz - 1;
				os = 1;
			}
			else if (s == 1)
			{
				ntx = tx;
				ntz = tz + 1;
				os = 0;
			}
			else if (s == 2)
			{
				ntx = tx - 1;
				os = 3;
			}
			int nk = TrailKey(ntx, ntz);
			if (!m_mBorn.Contains(nk))
				continue;
			array<float> nb = null;
			if (EdgeDrawn(nk))
				nb = m_mEdgeRec.Get(nk);
			bool need = false;
			for (int j = 1; j < TQ && !need; j++)
			{
				int e = s * TV + j;
				float y = ea[e];
				float ny = -30000;
				float nr = 0;
				if (nb)
				{
					ny = nb[os * TV + j];
					nr = nb[4 * TV + os * TV + j];
				}
				if (y < -9000 && ny < -9000)
					continue;
				if (y < -9000)
					need = nr >= EDGE_RAISED;     // the neighbour shows raised here, we draw nothing
				else if (ny < -9000)
					need = ea[4 * TV + e] >= EDGE_RAISED;
				else
					need = Math.AbsFloat(y - ny) > EDGE_CO_STEP;
			}
			if (need)
				CoBuild(ntx, ntz);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A3: build neighbour tile tx, tz for a co-build (see CoCheck)
	protected void CoBuild(int tx, int tz)
	{
		if (m_bPlaced && IsLive(tx, tz))
		{
			int t = (tz - m_iZ0 / TQ) * TILES + tx - m_iX0 / TQ;
			if (t < 0 || t >= TILES * TILES || m_aSlotZone[t] == 0)
				return;
			if (m_bInUpload)
			{
				if (m_aCoLive[t] != 0)
					return;
				m_aCoLive[t] = 1;
				m_aSlotDirty[t] = 1;
				m_aPick[t] = 1;
				m_aPickWhy[t] = BW_COBUILD;
				m_bCoAny = true;
				return;
			}
			m_aCoNext[t] = 1;
			m_aSlotDirty[t] = 1;
			m_bDirty = true;
			return;
		}
		int key = TrailKey(tx, tz);
		MUD_MudSheetStore st = m_mTrail.Get(key);
		if (!st || !st.m_Ent)
			return;     // data only or none: its mesh comes with the radius builds
		if (m_iCoStoredUp >= CO_MAX)
		{
			m_iCoCapped++;
			if (!m_mQWhy.Contains(key))
				m_mQWhy.Set(key, BW_COBUILD);
			QueueBuild(tx, tz);
			return;
		}
		m_iCoStoredUp++;
		m_iCoStored++;
		int bq = m_aBandQ.Find(key);
		if (bq >= 0)
		{
			m_aBandQ.Remove(bq);
			BandTile(tx, tz, st.m_D, 0, 0);     // its band is due: bring it up to date first
		}
		int keepWhy = m_iBuildWhy;
		m_bCoBuilding = true;
		m_iBuildWhy = BW_COBUILD;
		Resource res = BuildTile(st.m_Ent, st.m_D, tx, tz, false);
		if (res)
			st.m_Res = res;
		m_bCoBuilding = false;
		m_iBuildWhy = keepWhy;
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET LOCAL 10 s line (Log10): builds by reason, then reset
	protected void LocalLog()
	{
		string p1 = string.Format("MUD sheet builds: wheel %1, oldest %2, nearest %3, mesh-less %4, freeze %5, restore %6, slump %7, band %8",
			m_aBuildsBy[BW_WHEEL], m_aBuildsBy[BW_OLDEST], m_aBuildsBy[BW_NEAREST], m_aBuildsBy[BW_MESHLESS], m_aBuildsBy[BW_FREEZE], m_aBuildsBy[BW_RESTORE], m_aBuildsBy[BW_SLUMP], m_aBuildsBy[BW_BAND]);
		string p2 = string.Format(" | would skip (max change < 3 mm) %1 | fit %2, hole %3, op %5 | born slots waited for traces %4 uploads",
			m_iWouldSkip, m_aBuildsBy[BW_FIT], m_aBuildsBy[BW_HOLE], m_iBornWait, m_aBuildsBy[BW_OP]);
		// stage 2 A6: co-builds (the cobuild builds by reason = pick + live + stored + queued ones that ran)
		string p3 = string.Format(" | cobuild pick %1, cobuild live %2, cobuild stored %3 (capped %4), cobuild builds %5, born front %6",
			m_iCoPick, m_iCoLive, m_iCoStored, m_iCoCapped, m_aBuildsBy[BW_COBUILD], m_iBornFront);
		if (BAND_INCR)
			p3 += string.Format(" | band incr: passes %1, box cells %2, slots dirtied %3, slot BandTiles %4", m_iIncrRuns, m_iIncrCells, m_iIncrDirty, m_iIncrFull);
		if (EDGE_DRAPE)
			p3 += string.Format(" | drape cut at cap %1", m_iDrapeCut);
		if (EDGE_OPEN_FADE)
			p3 += string.Format(" | sides faded (beside not drawn) %1", m_iOpenFade);
		if (BUILD_SKIP_SAME)
			p3 += string.Format(" | skip same: kept old mesh %1", m_iSkipSame);
		if (BAND_STORED_HOLD)
			p3 += string.Format(" | stored band builds held %1", m_iBandHeld);
		if (!EDGE_AUDIT && EDGE_COBUILD)
			EdgePrune();
		Print(p1 + p2 + p3 + FarLog() + EdgeAudit(), LogLevel.NORMAL);
		for (int b = 0; b < BW_N; b++)
			m_aBuildsBy[b] = 0;
		m_iWouldSkip = 0;
		m_iBornWait = 0;
		m_iCoPick = 0;
		m_iCoLive = 0;
		m_iCoStored = 0;
		m_iCoCapped = 0;
		m_iBornFront = 0;
		m_iIncrRuns = 0;
		m_iIncrCells = 0;
		m_iIncrDirty = 0;
		m_iIncrFull = 0;
		m_iSkipSame = 0;
		m_iDrapeCut = 0;
		m_iOpenFade = 0;
		m_iBandHeld = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part C: lever set s (0 none, 1 all, 2 L1, 3 L2, 4 L3, 5 L6, 6 L8)
	static void LeverApply(int s, string why)
	{
		s_iLeverSet = s;
		LEV_L1_CARRY = s == 1 || s == 2;
		LEV_L2_DRAG = s == 1 || s == 3;
		LEV_L3_SLUMP = s == 1 || s == 4;
		LEV_L6_EASE = s == 1 || s == 5;
		LEV_L8_VEH = s == 1 || s == 6;
		Print(string.Format("MUD sheet levers: %1 | %2", LeverNames(), why), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB stage 2: fix set s. 0 none, 1 A2, 2 A2 A3, 3 A2 A3 A4 A5 (all A), 4 C1 C2 (band), 5 all C, 6 all A + all C,
	//! 7 all B (craters), 8 all A + B + C
	static void FixApply(int s, string why)
	{
		s_iFixSet = s;
		bool a = s == 3 || s == 6 || s == 8;
		bool c = s == 5 || s == 6 || s == 8;
		bool b = s == 7 || s == 8;
		EDGE_COBUILD_PICK = a || s == 1 || s == 2 || s >= 9;
		EDGE_COBUILD = a || s == 2 || s >= 9;     // A8 needs A3 to lift the fade again
		SKIRT_APRON = a;
		EDGE_DRAPE = s == 9 || s == 10;     // A7: set 9 = cobuild pick + cobuild + drape + born front (set 3 with the drape)
		EDGE_OPEN_FADE = s == 10;          // A8: set 10 = set 9 + open fade
		BAND_BORN_FRONT = a || s >= 9;
		BAND_INCR = c || s == 4;
		BAND_MICRO = c || s == 4;
		BUILD_SKIP_SAME = c;
		UV64_FAST = c;
		BAND_STORED_HOLD = c;
		CRATER_TRACE_NOW = b;
		CRATER_TAPER_NOW = b;
		CRATER_EASE_DROP = b;
		s_iDistGen++;     // C1: kept distances were not updated while BAND_INCR was off
		Print(string.Format("MUD sheet fixes: set %1: %2 | %3", s, FixNames(), why), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! TEST_KIT numpad * while sheet local runs: the next fix set (FixApply)
	static int FixSet()
	{
		return s_iFixSet;
	}

	//------------------------------------------------------------------------------------------------
	static void LabFixStep(string why)
	{
		int s = s_iFixSet + 1;
		if (s > FIX_SETS)
			s = 0;
		FixApply(s, why);
	}

	//------------------------------------------------------------------------------------------------
	static string FixNames()
	{
		string s = "";
		if (EDGE_COBUILD_PICK)
			s += " cobuild_pick";
		if (EDGE_COBUILD)
			s += " cobuild";
		if (SKIRT_APRON)
			s += " apron";
		if (EDGE_DRAPE)
			s += " drape";
		if (EDGE_OPEN_FADE)
			s += " open_fade";
		if (BAND_BORN_FRONT)
			s += " born_front";
		if (BAND_INCR)
			s += " band_incr";
		if (BAND_MICRO)
			s += " band_micro";
		if (BUILD_SKIP_SAME)
			s += " skip_same";
		if (UV64_FAST)
			s += " uv64_fast";
		if (BAND_STORED_HOLD)
			s += " stored_hold";
		if (CRATER_TRACE_NOW)
			s += " crater_trace";
		if (CRATER_TAPER_NOW)
			s += " crater_taper";
		if (CRATER_EASE_DROP)
			s += " crater_ease";
		if (s == "")
			return "off";
		return s.Trim();
	}

	//------------------------------------------------------------------------------------------------
	//! TEST_KIT numpad 0 while sheet local runs: the next lever set (none -> all -> L1 -> L2 -> L3 -> L6 -> L8 -> none)
	static void LabLeverStep(string why)
	{
		int s = s_iLeverSet + 1;
		if (s > LEVER_SETS)
			s = 0;
		LeverApply(s, why);
	}

	//------------------------------------------------------------------------------------------------
	static string LeverNames()
	{
		string s = "";
		if (LEV_L1_CARRY)
			s += " L1";
		if (LEV_L2_DRAG)
			s += " L2";
		if (LEV_L3_SLUMP)
			s += " L3";
		if (LEV_L6_EASE)
			s += " L6";
		if (LEV_L8_VEH)
			s += " L8";
		if (s == "")
			return "off";
		return s.Trim();
	}

	//------------------------------------------------------------------------------------------------
	//! L1: does any live slot need a build?
	protected bool AnySlotDirty()
	{
		for (int t = 0; t < TILES * TILES; t++)
		{
			if (m_aSlotDirty[t] != 0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! L1: stored tile key (store st) shows a current full mesh: built, no rebuild queued or held, no band due, not low
	protected bool StoreCurrent(MUD_MudSheetStore st, int key)
	{
		if (!st || !st.m_Ent || !st.m_Res || st.m_D.m_iLow != 0)
			return false;
		return !m_aOneBuild.Contains(key) && !m_aHeldBuild.Contains(key) && !m_aBandQ.Contains(key);
	}

	//------------------------------------------------------------------------------------------------
	//! L2: LimitDrag once per upload over the cells stamps wrote since the last one + 1 ring (all inside 1..N-2), two
	//! sweeps in opposite raster order as LimitDrag, clamps inline
	protected void LimitDragCells()
	{
		array<int> list = m_Drag.m_aList;
		int n0 = list.Count();
		for (int a = 0; a < n0; a++)
		{
			int va = list[a];
			int ia = va % N;
			int ka = va / N;
			for (int dk = -1; dk <= 1; dk++)
			{
				for (int di = -1; di <= 1; di++)
				{
					int ci = ia + di;
					int ck = ka + dk;
					if (ci >= 1 && ci <= N - 2 && ck >= 1 && ck <= N - 2)
						m_Drag.Add(ck * N + ci);
				}
			}
		}
		list.Sort();
		int cnt = list.Count();
		for (int sweep = 0; sweep < 2; sweep++)
		{
			for (int b = 0; b < cnt; b++)
			{
				int v = list[b];
				if (sweep == 1)
					v = list[cnt - 1 - b];
				int i = v % N;
				int k = v / N;
				if (i < 1 || i > N - 2 || k < 1 || k > N - 2)
					continue;
				float ou = m_F.m_aOu[v];
				float ov = m_F.m_aOv[v];
				for (int q = 0; q < 4; q++)
				{
					int nb = v - 1;
					if (q == 1)
						nb = v + 1;
					else if (q == 2)
						nb = v - N;
					else if (q == 3)
						nb = v + N;
					float lo = m_F.m_aOu[nb] - DRAG_LIM;
					if (ou < lo)
						ou = lo;
					else if (ou > lo + 2 * DRAG_LIM)
						ou = lo + 2 * DRAG_LIM;
					float lo2 = m_F.m_aOv[nb] - DRAG_LIM;
					if (ov < lo2)
						ov = lo2;
					else if (ov > lo2 + 2 * DRAG_LIM)
						ov = lo2 + 2 * DRAG_LIM;
				}
				m_F.m_aOu[v] = ou;
				m_F.m_aOv[v] = ov;
			}
		}
		m_Drag.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! L3 Slump: only the cells that changed since the last slump (Grow marks them) + 1 ring, one pass at L3_FACTOR
	//! (offline: equal to 3 passes at 0.25, results/2026-10-19-sheet-sync-maths-offline.md), mud flows only inside
	//! that set; the changed rectangle is marked once
	protected void SlumpCells()
	{
		if (m_Chg.m_aList.IsEmpty())
			return;
		m_iSlumpGen++;
		array<int> work = {};
		foreach (int vc : m_Chg.m_aList)
		{
			int ic = vc % N;
			int kc = vc / N;
			for (int dk = -1; dk <= 1; dk++)
			{
				for (int di = -1; di <= 1; di++)
				{
					int wi = ic + di;
					int wk = kc + dk;
					if (wi < 0 || wi >= N || wk < 0 || wk >= N)
						continue;
					int wv = wk * N + wi;
					if (m_Chg.m_aGen[wv] == m_iSlumpGen)
						continue;
					m_Chg.m_aGen[wv] = m_iSlumpGen;
					work.Insert(wv);
					m_T.m_aD[wv] = 0;
				}
			}
		}
		m_Chg.Clear();
		float step = REPOSE * CELL;
		bool moved = false;
		foreach (int v : work)
		{
			float da = m_F.m_aD[v];
			if (da <= 0)
				continue;
			int i = v % N;
			int k = v / N;
			float sa = GroundAt(v) + da;
			float give = 0;
			for (int n = 0; n < 4; n++)
			{
				int ni = i;
				int nk = k;
				if (n == 0)
					ni++;
				else if (n == 1)
					ni--;
				else if (n == 2)
					nk++;
				else
					nk--;
				if (ni < 0 || ni >= N || nk < 0 || nk >= N)
					continue;
				int nv = nk * N + ni;
				if (m_Chg.m_aGen[nv] != m_iSlumpGen)
					continue;     // only the set is applied (as the box in Slump)
				float drop = sa - GroundAt(nv) - m_F.m_aD[nv] - step;
				if (drop <= 0)
					continue;
				float f = Math.Min(drop * L3_FACTOR * 0.5, da * 0.25);
				if (f < 0.0002)
					continue;
				m_T.m_aD[nv] = m_T.m_aD[nv] + f;
				give += f;
				moved = true;
			}
			m_T.m_aD[v] = m_T.m_aD[v] - give;
		}
		if (!moved)
			return;
		int ri0 = N;
		int ri1 = -1;
		int rk0 = N;
		int rk1 = -1;
		foreach (int v3 : work)
		{
			float dd = m_T.m_aD[v3];
			if (dd == 0)
				continue;
			m_F.m_aD[v3] = m_F.m_aD[v3] + dd;
			int i3 = v3 % N;
			int k3 = v3 / N;
			Grow(i3, k3);     // marks it changed for the next slump (L3), keeps the Slump box when L3 goes off
			if (Math.AbsFloat(dd) <= SLUMP_SHOW)
				continue;
			if (i3 < ri0)
				ri0 = i3;
			if (i3 > ri1)
				ri1 = i3;
			if (k3 < rk0)
				rk0 = k3;
			if (k3 > rk1)
				rk1 = k3;
		}
		if (ri1 < 0)
			return;
		MarkRect(ri0, ri1, rk0, rk1);
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! L8 Frame: the vehicles near the window (origin within VEH_NEAR_M of it; their sims cached), then Wheels for each
	protected void WheelsNear(float time)
	{
		m_aNearVeh.Clear();
		m_aNearSim.Clear();
		float wx0 = m_iX0 * CELL - VEH_NEAR_M;
		float wz0 = m_iZ0 * CELL - VEH_NEAR_M;
		float wx1 = (m_iX0 + N - 1) * CELL + VEH_NEAR_M;
		float wz1 = (m_iZ0 + N - 1) * CELL + VEH_NEAR_M;
		foreach (Vehicle v : MUD_Wheels.GetAll())
		{
			if (!v)
				continue;
			vector o = v.GetOrigin();
			if (o[0] < wx0 || o[0] > wx1 || o[2] < wz0 || o[2] > wz1)
				continue;
			MUD_MudSheetVeh mv = m_mVeh.Get(v);
			if (!mv)
			{
				mv = new MUD_MudSheetVeh();
				mv.m_Veh = v;
				mv.m_Sim = VehicleWheeledSimulation.Cast(v.FindComponent(VehicleWheeledSimulation));
				m_iNextVehId++;
				mv.m_iId = m_iNextVehId;
				m_mVeh.Set(v, mv);
				m_aVeh.Insert(mv);
			}
			if (!mv.m_Sim)
				continue;
			m_aNearVeh.Insert(v);
			m_aNearSim.Insert(mv.m_Sim);
			WheelsSim(v, mv.m_Sim, mv, time);
		}
		if (time < m_fVehPrune)
			return;
		m_fVehPrune = time + VEH_PRUNE_S;
		// dead or far entries go (a far one comes back as a new entry: its last wheel points are stale anyway)
		for (int n = m_aVeh.Count() - 1; n >= 0; n--)
		{
			MUD_MudSheetVeh pv = m_aVeh[n];
			bool keep = pv && pv.m_Veh;
			if (keep)
			{
				vector po = pv.m_Veh.GetOrigin();
				keep = po[0] >= wx0 - VEH_PRUNE_M && po[0] <= wx1 + VEH_PRUNE_M && po[2] >= wz0 - VEH_PRUNE_M && po[2] <= wz1 + VEH_PRUNE_M;
			}
			if (keep)
				continue;
			if (pv && pv.m_Veh)
				m_mVeh.Remove(pv.m_Veh);
			m_aVeh.Remove(n);
		}
		// keys of deleted vehicles (null) cannot be looked up: rebuild the map when it holds more than the list
		if (m_mVeh.Count() > m_aVeh.Count())
		{
			m_mVeh.Clear();
			foreach (MUD_MudSheetVeh kv : m_aVeh)
			{
				if (kv && kv.m_Veh)
					m_mVeh.Set(kv.m_Veh, kv);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part D: store of key; SHEET LOCAL: a compact far tile comes back as a full data-only store first
	protected MUD_MudSheetStore TrailGet(int key)
	{
		MUD_MudSheetStore st = m_mTrail.Get(key);
		if (st || m_mFar.IsEmpty())
			return st;
		return FarTake(key);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part D, SHEET LOCAL: stored tile key (no mesh) leaves m_mTrail as compact far data - one int a cell:
	//! byte 0 height 0..FAR_H, byte 1 / 2 drag u / v (-DRAG_MAX..DRAG_MAX), byte 3 class (bit 0 bare, bit 1 touched,
	//! bits 2-6 deposit band 0..31). Ground, traces and the last-build heights are not kept (traced again on approach).
	//! Past TILE_DATA_CAP data tiles the oldest compact ones go.
	protected void FarPut(int key, MUD_MudSheetStore st)
	{
		ref MUD_MudSheetFar f = new MUD_MudSheetFar();
		f.m_iTx = st.m_iTx;
		f.m_iTz = st.m_iTz;
		MUD_MudSheetTileData d = st.m_D;
		for (int l = 0; l < TV * TV; l++)
		{
			int h = Math.Round(Math.Clamp(d.m_aD[l], 0, FAR_H) * (255 / FAR_H));
			int u = Math.Round(Math.Clamp(d.m_aOu[l], -DRAG_MAX, DRAG_MAX) * (127 / DRAG_MAX));
			int w = Math.Round(Math.Clamp(d.m_aOv[l], -DRAG_MAX, DRAG_MAX) * (127 / DRAG_MAX));
			int bq = Math.Round(Math.Clamp(d.m_aBand[l], 0, 1) * 31);
			int c = bq * 4;
			if (d.m_aBare[l] != 0)
				c += 1;
			if (d.m_aTouch[l] != 0)
				c += 2;
			f.m_aC[l] = h | ((u + 128) << 8) | ((w + 128) << 16) | (c << 24);
		}
		if (d.m_iLow != 0)
			m_iLowNow--;
		m_iTchKey = -1;     // BandTile's store cache
		m_TchSt = null;
		m_TchFar = null;
		m_mTrail.Remove(key);     // st is gone from here on
		RemoveTrailKey(key);
		m_mFar.Set(key, f);
		int at = m_aFarOrder.Find(key);
		if (at >= 0)
			m_aFarOrder.RemoveOrdered(at);
		m_aFarOrder.Insert(key);
		m_iFarIn++;
		int over = m_mTrail.Count() + m_mFar.Count() - TILE_DATA_CAP;
		while (over > 0 && !m_aFarOrder.IsEmpty())
		{
			int ok = m_aFarOrder[0];
			m_aFarOrder.RemoveOrdered(0);
			if (!m_mFar.Contains(ok))
				continue;
			m_mFar.Remove(ok);
			m_iFarCap++;
			over--;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part D: compact far tile key back as a full data-only store in m_mTrail (null: none)
	protected MUD_MudSheetStore FarTake(int key)
	{
		MUD_MudSheetFar f = m_mFar.Get(key);
		if (!f)
			return null;
		ref MUD_MudSheetStore st = new MUD_MudSheetStore();
		st.m_iTx = f.m_iTx;
		st.m_iTz = f.m_iTz;
		MUD_MudSheetTileData d = st.m_D;
		for (int l = 0; l < TV * TV; l++)
		{
			int c = f.m_aC[l];
			int h = c & 255;
			int u = (c >> 8) & 255;
			int w = (c >> 16) & 255;
			int cls = (c >> 24) & 255;
			int bq = (cls >> 2) & 31;
			d.m_aD[l] = h * (FAR_H / 255);
			d.m_aOu[l] = (u - 128) * (DRAG_MAX / 127);
			d.m_aOv[l] = (w - 128) * (DRAG_MAX / 127);
			d.m_aBare[l] = cls & 1;
			d.m_aTouch[l] = (cls >> 1) & 1;
			d.m_aBand[l] = bq / 31.0;
		}
		m_iTchKey = -1;
		m_TchSt = null;
		m_TchFar = null;
		m_mFar.Remove(key);     // f is gone from here on
		int at = m_aFarOrder.Find(key);
		if (at >= 0)
			m_aFarOrder.RemoveOrdered(at);
		m_mTrail.Set(key, st);
		m_aTrailOrder.Insert(key);
		m_iFarOut++;
		return st;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part D: MudAt of cell l of compact far tile key (mud x band; 0 when there is none)
	protected float FarMud(int key, int l)
	{
		if (m_mFar.IsEmpty())
			return 0;
		MUD_MudSheetFar f = m_mFar.Get(key);
		if (!f)
			return 0;
		int c = f.m_aC[l];
		int h = c & 255;
		int cls = (c >> 24) & 255;
		int bq = (cls >> 2) & 31;
		return h * (FAR_H / 255) * (bq / 31.0);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part D, 10 s line part: 'far data: compacted <n> expanded <n> cap drops <n> | compact <n> (<n> KB),
	//! full stores <n> | far-ring 11 x 11 builds <n>'
	protected string FarLog()
	{
		int kb = m_mFar.Count() * TV * TV * 4 / 1024;
		string s = string.Format(" | far data: compacted %1 expanded %2 cap drops %3 | compact %4 (%5 KB), full stores %6 | far-ring 11 x 11 builds %7",
			m_iFarIn, m_iFarOut, m_iFarCap, m_mFar.Count(), kb, m_mTrail.Count(), m_iFarLow);
		m_iFarIn = 0;
		m_iFarOut = 0;
		m_iFarCap = 0;
		m_iFarLow = 0;
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E (sync-lite), MUD_MudSync.RutOp in sheet local: every machine's rut op (kind 4), in seq order. Its
	//! segments are stamped into the tile data with the Stamp maths. The op has no vehicle or speed (the op tag stays
	//! off): OP_SPEED, TyreHalf of OP_RADIUS. Segments within COVER_R of this client's own recent wheel stamps are
	//! skipped (Wheels stamped them already).
	static void OpStamps(MUD_VoxelOp op)
	{
		if (!ENABLED || SYNC || !s_Inst || !s_Inst.m_bLocal || !op || op.m_iKind != 4)
			return;
		s_Inst.OpStampsI(op);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E, MUD_MudSync.ClearOp in sheet local: craters / digs reach the sheet at their go-live already
	//! (MUD_VoxelProbe.GoLive -> GroundChanged, every op on this machine) - only counted here, or they would clear twice
	static void OpClear(MUD_VoxelOp op)
	{
		if (!ENABLED || SYNC || !s_Inst || !op || op.m_iKind < 0 || op.m_iKind > 3)
			return;
		s_Inst.m_iOpClears++;
	}

	//------------------------------------------------------------------------------------------------
	protected void OpStampsI(MUD_VoxelOp op)
	{
		int tO = System.GetTickCount();
		float half = TyreHalf(OP_RADIUS);
		m_iOpKey = -1;
		m_OpD = null;
		m_aOpShown.Clear();
		int segs = Math.Min(op.m_aSegA.Count(), op.m_aSegB.Count());
		for (int s = 0; s < segs; s++)
		{
			vector a = op.m_aSegA[s];
			vector b = op.m_aSegB[s];
			float len = Math.Sqrt((b[0] - a[0]) * (b[0] - a[0]) + (b[2] - a[2]) * (b[2] - a[2]));
			if (len < 0.01 || len >= 3)
				continue;     // as MUD_MudSync.StepStamp: never across a jump
			int ztx = Math.Floor(b[0] / TILE_M);
			int ztz = Math.Floor(b[2] / TILE_M);
			if (!MUD_MudZoneEntity.TileCovered(ztx, ztz))
				continue;     // as Wheels (ZoneAtWorld in sheet local): only where the wheel stands on holed offroad ground
			if (OwnNear(b))
			{
				m_iOpSkipOwn++;
				continue;
			}
			m_iOpStamps++;
			if (OpInWindow(a, b))
				Stamp(a, b, OP_SPEED, m_fLast, OP_VEH, half);
			else
				OpStampStore(a, b, OP_SPEED, m_fLast, OP_VEH, half);
		}
		// shown stored tiles the stamps changed: rebuilt by FlushOneBuild
		foreach (int key : m_aOpShown)
		{
			int tx = key / 20000;     // TrailKey back to the tile (tx, tz >= 0 on Arland)
			int tz = key - tx * 20000;
			if (!m_mQWhy.Contains(key))
				m_mQWhy.Set(key, BW_OP);
			QueueBuild(tx, tz);
		}
		m_aOpShown.Clear();
		m_iOpKey = -1;
		m_OpD = null;
		if (!m_bPlaced && !m_aBandQ.IsEmpty())
			BandStored();     // no window: no Upload to band them
		m_iOpMs += System.GetTickCount(tO);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: does op segment a -> b stamp only inside the window (OP_EDGE_M from its sides)? Then Stamp does it.
	protected bool OpInWindow(vector a, vector b)
	{
		if (!m_bPlaced)
			return false;
		float x0 = m_iX0 * CELL + OP_EDGE_M;
		float x1 = (m_iX0 + N - 1) * CELL - OP_EDGE_M;
		float z0 = m_iZ0 * CELL + OP_EDGE_M;
		float z1 = (m_iZ0 + N - 1) * CELL - OP_EDGE_M;
		if (a[0] < x0 || a[0] > x1 || a[2] < z0 || a[2] > z1)
			return false;
		return b[0] >= x0 && b[0] <= x1 && b[2] >= z0 && b[2] <= z1;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: an own wheel stamp at p (WheelsSim) covers the cells within COVER_R for OWN_S seconds
	protected void OwnMark(vector p, float time)
	{
		int ci = Math.Round(p[0] / CELL);
		int ck = Math.Round(p[2] / CELL);
		int rc = Math.Ceil(COVER_R / CELL);
		float r2 = (COVER_R * COVER_R) / (CELL * CELL);
		for (int dk = -rc; dk <= rc; dk++)
		{
			for (int di = -rc; di <= rc; di++)
			{
				if (di * di + dk * dk > r2)
					continue;
				m_mOwn.Set(OwnKey(ci + di, ck + dk), time);
			}
		}
		if (time < m_fOwnPrune)
			return;
		m_fOwnPrune = time + OWN_PRUNE_S;
		array<int> old = {};
		foreach (int key, float t : m_mOwn)
		{
			if (time - t > OWN_S)
				old.Insert(key);
		}
		foreach (int ok : old)
			m_mOwn.Remove(ok);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: did this client stamp within COVER_R of p in the last OWN_S seconds?
	protected bool OwnNear(vector p)
	{
		if (m_mOwn.IsEmpty())
			return false;
		float t;
		int gi = Math.Round(p[0] / CELL);
		int gk = Math.Round(p[2] / CELL);
		if (!m_mOwn.Find(OwnKey(gi, gk), t))
			return false;
		return m_fLast - t <= OWN_S;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: map key of world cell gi, gk (wraps every 32768 cells = 4.9 km; a wrap only skips a segment)
	protected int OwnKey(int gi, int gk)
	{
		return (gi & 32767) | ((gk & 32767) << 15);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: one op segment a -> b on stored tiles (Stamp's BOX + SOFT + GRADUAL maths on world cells, as
	//! MUD_MudSync.Stamp; cells of live tiles are left to the window). Sink: the data ground beside the tyre.
	protected void OpStampStore(vector a, vector b, float speed, float time, int veh, float half)
	{
		if (SOFT && GRADUAL)
		{
			// WIDEN: how pressed the mud at the contact was when this pass began
			int wgi = Math.Round(b[0] / CELL);
			int wgk = Math.Round(b[2] / CELL);
			float wd;
			float wou;
			float wov;
			OpGet(wgi, wgk, wd, wou, wov);
			int wpk = PassKey(wgi, wgk);
			if (m_mPassT.Contains(wpk) && m_mPassV.Get(wpk) == veh && time - m_mPassT.Get(wpk) <= PASS_GAP)
				wd = m_mPassD.Get(wpk);
			float pressed = Math.Clamp((THICK - wd) / THICK, 0, 1);
			half = half * (W_FIRST + (1 - W_FIRST) * pressed);
		}
		float band = BAND * half / HALF;
		vector dir = Vector(b[0] - a[0], 0, b[2] - a[2]);
		float len = dir.Length();
		dir = dir * (1 / len);
		vector side = Vector(dir[2], 0, -dir[0]);
		float reach = half + band;
		float nose = len + NOSE;
		float far = nose + FRONT_LEN;
		float ex = a[0] + dir[0] * far;
		float ez = a[2] + dir[2] * far;
		int i0 = Math.Floor((Math.Min(a[0], ex) - reach) / CELL);
		int i1 = Math.Ceil((Math.Max(a[0], ex) + reach) / CELL);
		int k0 = Math.Floor((Math.Min(a[2], ez) - reach) / CELL);
		int k1 = Math.Ceil((Math.Max(a[2], ez) + reach) / CELL);
		float sideSum = 0;
		int sideN = 0;
		for (int sd = -1; sd <= 1; sd += 2)
		{
			vector q = b + side * (sd * (reach + CELL));
			float gq;
			if (!OpDataGround(q[0], q[2], gq))
				continue;
			sideSum += gq;
			sideN++;
		}
		float sink = 0;
		if (sideN > 0)
			sink = Math.Max(0, sideSum / sideN - b[1]);
		float h = Math.Min(HMAX, H0 + H_SPEED * speed + H_SINK * sink);
		float drag = DRAG * len;
		bool any = false;
		for (int k = k0; k <= k1; k++)
		{
			for (int i = i0; i <= i1; i++)
			{
				float px = i * CELL - a[0];
				float pz = k * CELL - a[2];
				float t = px * dir[0] + pz * dir[2];
				if (t < 0 || t > far)
					continue;
				float latS = px * side[0] + pz * side[2];
				float lat = Math.AbsFloat(latS);
				if (lat >= reach || (t > nose && lat >= half))
					continue;
				if (OpCell(i, k, t, lat, latS, nose, half, band, h, drag, dir, side, time, veh))
					any = true;
			}
		}
		if (!any)
			return;
		if (DRAG_LIM > 0)
			OpLimitDrag(i0, i1, k0, k1);
		BandMarkWorld(i0, i1, k0, k1);     // tiles born, bands due
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: Stamp's per-cell PRESS / PUSH (BOX path) on world cell i, k of the tile data. False = no data
	//! (a live tile: the window's).
	protected bool OpCell(int i, int k, float t, float lat, float latS, float nose, float half, float band, float h, float drag, vector dir, vector side, float time, int veh)
	{
		if (i < 0 || k < 0)
			return false;     // off the map side (int division would put it in tile 0 at a negative index)
		int tx = i / TQ;
		int tz = k / TQ;
		MUD_MudSheetTileData d = OpTile(tx, tz, true);
		if (!d)
			return false;
		int l = (k - tz * TQ) * TV + (i - tx * TQ);
		float cur = d.m_aD[l];
		float d0 = cur;     // mud depth when this pass over the cell began
		if (GRADUAL)
		{
			int pk = PassKey(i, k);
			if (!m_mPassT.Contains(pk) || m_mPassV.Get(pk) != veh || time - m_mPassT.Get(pk) > PASS_GAP)
				m_mPassD.Set(pk, d0);     // a new pass starts here
			else
				d0 = m_mPassD.Get(pk);
			m_mPassT.Set(pk, time);
			m_mPassV.Set(pk, veh);
		}
		float target = 0;
		float du = 0;
		float dv = 0;
		if (t <= nose && lat < half)
		{
			// PRESS (SOFT_WALL: full in the middle, none at the side), texture dragged along
			float pf = 1;
			if (SOFT)
			{
				float inW = half * SOFT_WALL;
				float fromSide = half - lat;
				if (fromSide < inW)
					pf = 0.5 * (1 - Math.Cos(Math.PI * fromSide / inW));
			}
			if (GRADUAL)
				cur = Math.Min(cur, Math.Max(0, Math.Min(d0, THICK) - PRESS_STEP * pf));
			else
				cur = Math.Min(cur, THICK * (1 - pf));
			du = dir[0] * drag;
			dv = dir[2] * drag;
		}
		else
		{
			// PUSH: push ahead (tyre wide) or side ridge (round shoulder, flat top, falls off)
			float w = 1;
			float hh = h;
			vector pushDir = dir;
			if (t > nose)
			{
				hh = h * ARCH_H;
				w = Math.Cos((t - nose) / FRONT_LEN * Math.PI * 0.5);
			}
			else
			{
				float x = (lat - half) / band;
				if (x > RIDGE_TOP)
					w = 0.5 * (1 + Math.Cos(Math.PI * (x - RIDGE_TOP) / (1 - RIDGE_TOP)));
				else if (SOFT)
					w = Math.Sin(0.5 * Math.PI * x / RIDGE_TOP);
				pushDir = side;
				if (latS < 0)
					pushDir = -side;
			}
			target = THICK + hh * w;
			if (GRADUAL)
				target = Math.Min(target, Math.Max(d0, THICK) + hh * w * PRESS_STEP / THICK);
			du = pushDir[0] * drag * 0.5 * w;
			dv = pushDir[2] * drag * 0.5 * w;
		}
		if (target > cur)
			cur = target;
		float ou = d.m_aOu[l] + du;
		float ov = d.m_aOv[l] + dv;
		float ol = Math.Sqrt(ou * ou + ov * ov);
		if (ol > DRAG_MAX)
		{
			ou *= DRAG_MAX / ol;
			ov *= DRAG_MAX / ol;
		}
		OpSet(i, k, cur, ou, ov, true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: tile tx, tz's data for op stamps (one-tile cache). make: a missing store is made (THICK, data
	//! only) and the tile born. Null for a live tile (the window holds it) or nothing there (make false).
	protected MUD_MudSheetTileData OpTile(int tx, int tz, bool make)
	{
		int key = TrailKey(tx, tz);
		if (key == m_iOpKey && (m_bOpMake || !make))
			return m_OpD;
		m_iOpKey = key;
		m_bOpMake = make;
		m_OpD = null;
		if (tx < 0 || tz < 0 || IsLive(tx, tz))
			return null;
		ref MUD_MudSheetStore st = TrailGet(key);
		if (!st)
		{
			if (!make)
				return null;
			st = new MUD_MudSheetStore();
			st.m_iTx = tx;
			st.m_iTz = tz;
			for (int l = 0; l < TV * TV; l++)
				st.m_D.m_aD[l] = THICK;     // untouched mud (no sync layer to ask)
			m_mTrail.Set(key, st);
			m_aTrailOrder.Insert(key);
		}
		if (make)
		{
			if (!m_mBorn.Contains(key))
				m_mBorn.Set(key, 1);
			if (st.m_Ent && !m_aOpShown.Contains(key))
				m_aOpShown.Insert(key);
		}
		m_OpD = st.m_D;
		return m_OpD;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: world cell gi, gk's mud and drag into every stored tile holding it (sides are shared);
	//! stamp: a deposit band source too
	protected void OpSet(int gi, int gk, float dd, float ou, float ov, bool stamp)
	{
		if (gi < 0 || gk < 0)
			return;
		int tx1 = gi / TQ;
		int tz1 = gk / TQ;
		int ri = gi - tx1 * TQ;
		int rk = gk - tz1 * TQ;
		for (int ax = 0; ax < 2; ax++)
		{
			int tx = tx1 - ax;
			if (ax == 1 && (ri != 0 || tx < 0))
				continue;
			for (int az = 0; az < 2; az++)
			{
				int tz = tz1 - az;
				if (az == 1 && (rk != 0 || tz < 0))
					continue;
				MUD_MudSheetTileData d = OpTile(tx, tz, stamp);
				if (!d)
					continue;
				int l = (gk - tz * TQ) * TV + gi - tx * TQ;
				if (stamp)
				{
					d.m_aD[l] = dd;
					if (d.m_aTouch[l] == 0)
					{
						d.m_aTouch[l] = 1;
						m_iTouchCells++;
					}
				}
				d.m_aOu[l] = ou;
				d.m_aOv[l] = ov;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: world cell gi, gk's mud and drag - the window, else the stored tile (THICK / 0 where none)
	protected bool OpGet(int gi, int gk, out float dd, out float ou, out float ov)
	{
		dd = THICK;
		ou = 0;
		ov = 0;
		if (gi < 0 || gk < 0)
			return false;
		int tx = gi / TQ;
		int tz = gk / TQ;
		if (IsLive(tx, tz))
		{
			int wi = gi - m_iX0;
			int wk = gk - m_iZ0;
			if (wi < 0 || wi >= N || wk < 0 || wk >= N)
				return false;
			int v = wk * N + wi;
			dd = m_F.m_aD[v];
			ou = m_F.m_aOu[v];
			ov = m_F.m_aOv[v];
			return true;
		}
		MUD_MudSheetTileData d = OpTile(tx, tz, false);
		if (!d)
			return false;
		int l = (gk - tz * TQ) * TV + gi - tx * TQ;
		dd = d.m_aD[l];
		ou = d.m_aOu[l];
		ov = d.m_aOv[l];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: LimitDrag on world cells i0..i1 x k0..k1 of the tile data (two sweeps, opposite directions;
	//! a neighbour with no data reads 0 drag, as MUD_MudSync.LimitDragCell)
	protected void OpLimitDrag(int i0, int i1, int k0, int k1)
	{
		for (int sweep = 0; sweep < 2; sweep++)
		{
			for (int kk = k0; kk <= k1; kk++)
			{
				int k = kk;
				if (sweep == 1)
					k = k0 + k1 - kk;
				for (int ii = i0; ii <= i1; ii++)
				{
					int i = ii;
					if (sweep == 1)
						i = i0 + i1 - ii;
					if (i < 0 || k < 0 || IsLive(i / TQ, k / TQ))
						continue;
					float dd;
					float ou;
					float ov;
					if (!OpGet(i, k, dd, ou, ov))
						continue;
					for (int q = 0; q < 4; q++)
					{
						int ni = i;
						int nk = k;
						if (q == 0)
							ni = i - 1;
						else if (q == 1)
							ni = i + 1;
						else if (q == 2)
							nk = k - 1;
						else
							nk = k + 1;
						float nd;
						float nou;
						float nov;
						OpGet(ni, nk, nd, nou, nov);
						ou = Math.Clamp(ou, nou - DRAG_LIM, nou + DRAG_LIM);
						ov = Math.Clamp(ov, nov - DRAG_LIM, nov + DRAG_LIM);
					}
					OpSet(i, k, dd, ou, ov, false);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB part E: the data ground at x, z (as MUD_MudSync.GroundY: voxel top, else the heightfield + rut floor)
	protected bool OpDataGround(float x, float z, out float y)
	{
		if (MUD_VoxelProbe.ColumnAt(x, z) && MUD_VoxelProbe.TopAt(x, z, y))
			return true;
		float dug;
		float undug;
		if (MUD_WorldGroundEntity.DugUndug(x, z, dug, undug))
		{
			y = dug;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD LAB 10 s line part E: 'op stamps <n> skipped own <n>' (+ ms, clear ops seen), then reset
	static string OpLog()
	{
		if (!s_Inst)
			return " | op stamps 0 skipped own 0";
		string s = string.Format(" | op stamps %1 skipped own %2 (%3 ms, own cells %4) | clear ops %5 (taken at go-live)",
			s_Inst.m_iOpStamps, s_Inst.m_iOpSkipOwn, s_Inst.m_iOpMs, s_Inst.m_mOwn.Count(), s_Inst.m_iOpClears);
		s_Inst.m_iOpStamps = 0;
		s_Inst.m_iOpSkipOwn = 0;
		s_Inst.m_iOpMs = 0;
		s_Inst.m_iOpClears = 0;
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Map key of world tile tx, tz (Arland is < 4300 tiles a side, so tz < 20000 never overlaps)
	static int TrailKey(int tx, int tz)
	{
		return tx * 20000 + tz;
	}

	//------------------------------------------------------------------------------------------------
	//! Did a wheel change live tile ti, tk (mud not the plain layer, or texture dragged)?
	protected bool Touched(int ti, int tk)
	{
		for (int lk = 0; lk < TV; lk++)
		{
			for (int li = 0; li < TV; li++)
			{
				int v = (tk * TQ + lk) * N + ti * TQ + li;
				if (Math.AbsFloat(m_F.m_aD[v] - THICK) > EPS || m_F.m_aOu[v] != 0 || m_F.m_aOv[v] != 0)
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Live tile ti, tk (world tile tx, tz) becomes a trail tile: data kept, entity and mesh handed over
	protected void Freeze(int tx, int tz, int ti, int tk, IEntity te, Resource res)
	{
		int key = TrailKey(tx, tz);
		MUD_MudSheetStore st = TrailGet(key);     // MUD LAB part D: no stale compact copy left behind
		if (st)
		{
			// cannot happen (a trail tile is unpacked when the sheet covers it); keep the newer one
			st.m_fDieAt = m_fLast + 1;
			m_aDying.Insert(st);
			RemoveTrailKey(key);
		}
		st = new MUD_MudSheetStore();
		st.m_iTx = tx;
		st.m_iTz = tz;
		st.m_Ent = te;
		st.m_Res = res;
		if (m_bLocal)
			FreezeLocal(tx, tz, ti, tk);     // SHEET LOCAL: eases end, band up to date
		Pack(ti, tk, st.m_D);
		if (m_bLocal && BAND_INCR)
			DistPack(ti, tk, st.m_D);     // stage 2 C1
		st.m_D.m_iSig1 = m_aSlotSig1[tk * TILES + ti];     // stage 2 C3: its mesh goes with it
		st.m_D.m_iSig2 = m_aSlotSig2[tk * TILES + ti];
		st.m_D.m_iDrawn = m_aSlotReady[tk * TILES + ti];     // LAZY RUT MESH: the slot's mesh had no hole
		st.m_D.m_iUnlift = m_aSlotUnlift[tk * TILES + ti];   // LIFT BY DRAWN: the mesh stays on screen
		if (m_bLocal && te && !SlotTraced(tk * TILES + ti))
		{
			// SHEET LOCAL: never built before every cell is traced - data only; CAMERA RADIUS traces + builds it
			te.ClearFlags(EntityFlags.VISIBLE, false);
			MUD_MudSheetStore fdy = new MUD_MudSheetStore();
			fdy.m_Ent = te;
			fdy.m_Res = res;
			fdy.m_fDieAt = m_fLast + 1;
			m_aDying.Insert(fdy);
			st.m_Ent = null;
			st.m_Res = null;
		}
		// S1 (UPLOAD_SLOTS): a slot Upload put off leaves with a stale mesh - build it from the stored data now
		else if (UPLOAD_SLOTS > 0 && te && m_aSlotDirty[tk * TILES + ti] != 0 && m_aSlotZone[tk * TILES + ti] != 0)
		{
			m_iBuildWhy = BW_FREEZE;
			Resource r = BuildTile(te, st.m_D, tx, tz, false);
			if (r)
				st.m_Res = r;
		}
		m_mTrail.Set(key, st);
		m_aTrailOrder.Insert(key);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveTrailKey(int key)
	{
		int at = m_aTrailOrder.Find(key);
		if (at >= 0)
			m_aTrailOrder.RemoveOrdered(at);
	}

	//------------------------------------------------------------------------------------------------
	//! Copy live tile ti, tk (TV x TV cells, edges shared with the neighbours) out of the field / back into it
	protected void Pack(int ti, int tk, MUD_MudSheetTileData d)
	{
		for (int lk = 0; lk < TV; lk++)
		{
			for (int li = 0; li < TV; li++)
			{
				int v = (tk * TQ + lk) * N + ti * TQ + li;
				int l = lk * TV + li;
				d.m_aD[l] = m_F.m_aD[v];
				d.m_aG[l] = m_F.m_aG[v];
				d.m_aGs[l] = m_F.m_aGs[v];
				d.m_aOu[l] = m_F.m_aOu[v];
				d.m_aOv[l] = m_F.m_aOv[v];
				d.m_aBare[l] = m_F.m_aBare[v];
				d.m_aTouch[l] = m_F.m_aTouch[v];
				d.m_aBand[l] = m_F.m_aBand[v];
				d.m_aLastY[l] = m_F.m_aLastY[v];
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Unpack(MUD_MudSheetTileData d, int ti, int tk)
	{
		for (int lk = 0; lk < TV; lk++)
		{
			for (int li = 0; li < TV; li++)
			{
				int v = (tk * TQ + lk) * N + ti * TQ + li;
				int l = lk * TV + li;
				m_F.m_aD[v] = d.m_aD[l];
				m_F.m_aG[v] = d.m_aG[l];
				m_F.m_aGs[v] = d.m_aGs[l];
				m_F.m_aOu[v] = d.m_aOu[l];
				m_F.m_aOv[v] = d.m_aOv[l];
				m_F.m_aBare[v] = d.m_aBare[l];
				m_F.m_aTouch[v] = d.m_aTouch[l];
				m_F.m_aBand[v] = d.m_aBand[l];
				m_F.m_aLastY[v] = d.m_aLastY[l];
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Is world tile tx, tz a mud tile (its middle inside a zone)? Fades go where the next tile is not one.
	protected bool HasTile(int tx, int tz)
	{
		if (m_bLocal && EDGE_OPEN_FADE)
			return OpenDrawn(tx, tz);     // stage 2 A8
		if (m_bLocal)
			return true;     // SHEET LOCAL: no EDGE fade - the deposit band takes the mud into the ground
		return TileInZone(tx, tz);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 A8 EDGE_OPEN_FADE: world tile tx, tz counts as a drawn neighbour (no fade toward it) when its mesh is on
	//! screen or it is a live slot built in this upload. Otherwise a raised border beside it would show its skirt / drape
	//! (the striped wedges, user screenshots 2026-10-20). When it gets drawn, A3 (CoCheck: raised beside not drawn, or
	//! step) builds this tile again without the fade.
	protected bool OpenDrawn(int tx, int tz)
	{
		if (m_bPlaced && IsLive(tx, tz))
		{
			int t = (tz - m_iZ0 / TQ) * TILES + tx - m_iX0 / TQ;
			if (t >= 0 && t < TILES * TILES && m_aPick[t] == 1)
				return true;
		}
		bool drawn = EdgeDrawn(TrailKey(tx, tz));
		if (!drawn)
			m_iOpenFade++;
		return drawn;
	}

	//------------------------------------------------------------------------------------------------
	//! SPARSE SHEET A2: is world tile tx, tz in the live window (no patch lift under it: own fresh ruts are drawn there
	//! before their op's sync tile comes)?
	static bool LiveTile(int tx, int tz)
	{
		return s_Inst && s_Inst.IsLive(tx, tz);
	}

	//------------------------------------------------------------------------------------------------
	//! LIFT BY DRAWN: is the sheet mesh of world tile tx, tz on screen (patches under it are drawn unlifted)?
	static bool Unlifted(int tx, int tz)
	{
		if (!s_Inst)
			return false;
		if (s_Inst.IsLive(tx, tz))
		{
			int ux = tx - s_Inst.m_iX0 / TQ;
			int uz = tz - s_Inst.m_iZ0 / TQ;
			return s_Inst.m_aSlotUnlift[uz * TILES + ux] != 0;
		}
		MUD_MudSheetStore ust = s_Inst.m_mTrail.Get(TrailKey(tx, tz));
		return ust && ust.m_Ent && ust.m_D.m_iUnlift != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! SPARSE SHEET A2: the window moved from tile corner oldTx, oldTz to newTx, newTz - patches under the tiles that left
	//! or came rebuild (their lift changes). A far old corner (first placement) = every new tile.
	protected void LiftDirtyWindow(int newTx, int newTz, int oldTx, int oldTz)
	{
		if (!PatchLiftOn() || LIFT_BY_DRAWN)
			return;     // LIFT BY DRAWN: the flags move with the tiles; builds and drops tell the patches
		for (int lt = 0; lt < TILES * TILES; lt++)
		{
			int lox = oldTx + lt % TILES;
			int loz = oldTz + lt / TILES;
			if (oldTx > -100000 && (lox < newTx || lox >= newTx + TILES || loz < newTz || loz >= newTz + TILES))
				MUD_WorldGroundEntity.SheetLiftDirty(lox * TILE_M, loz * TILE_M);
			int lnx = newTx + lt % TILES;
			int lnz = newTz + lt / TILES;
			if (lnx < oldTx || lnx >= oldTx + TILES || lnz < oldTz || lnz >= oldTz + TILES)
				MUD_WorldGroundEntity.SheetLiftDirty(lnx * TILE_M, lnz * TILE_M);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SPARSE SHEET: is the A2 patch lift on (MUD_WorldGroundEntity mesh builds ask this)?
	static bool PatchLiftOn()
	{
		return ENABLED && SPARSE && SYNC && MUD_MudZoneEntity.ALL_OFFROAD;
	}

	//------------------------------------------------------------------------------------------------
	//! SPARSE SHEET A2: m a ground patch mesh is drawn higher on untouched covered ground (times its edge lock)
	static float PatchLift()
	{
		if (LIFT_BY_DRAWN)
			return THICK + LIFT - LIFT_UNDER;
		return THICK - PATCH_GAP;
	}

	//------------------------------------------------------------------------------------------------
	//! CAMERA RADIUS: build a sheet tile at tx, tz? Covered ground, and with SPARSE only where there is mud data
	//! (tiles tx..tx+1 x tz..tz+1: this tile's cells reach into the next one)
	protected bool SheetWanted(int tx, int tz)
	{
		if (!TileInZone(tx, tz))
			return false;
		if (!PatchLiftOn())
			return true;
		return MUD_MudSync.AnyTileNear(tx, tz);
	}

	//------------------------------------------------------------------------------------------------
	protected bool TileInZone(int tx, int tz)
	{
		if (m_bLocal)
			return m_mBorn.Contains(TrailKey(tx, tz));     // SHEET LOCAL: only tiles a stamp's band reached
		return MUD_MudZoneEntity.TileCovered(tx, tz);     // placed zones, or H3 ALL_OFFROAD holed ground
	}

	//------------------------------------------------------------------------------------------------
	//! Is world tile tx, tz in the live window?
	protected bool IsLive(int tx, int tz)
	{
		if (!m_bPlaced)
			return false;
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		return tx >= x0 && tx < x0 + TILES && tz >= z0 && tz < z0 + TILES;
	}

	//------------------------------------------------------------------------------------------------
	//! Window slots: which are zone tiles; all need a new mesh
	protected void UpdateSlots()
	{
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		for (int t = 0; t < TILES * TILES; t++)
		{
			bool inZ = TileInZone(x0 + t % TILES, z0 + t / TILES);
			m_aSlotZone[t] = 0;
			if (inZ)
				m_aSlotZone[t] = 1;
			if (m_bSlotsKeep)
				continue;     // L1: Follow carried each slot's dirty flag
			m_aSlotDirty[t] = 1;
			m_aSlotWhy[t] = m_aSlotWhy[t] | WHY_OTHER;
		}
		m_bAnyZone = false;
		for (int tz2 = 0; tz2 < TILES * TILES; tz2++)
		{
			if (m_aSlotZone[tz2] != 0)
				m_bAnyZone = true;
			// LAZY RUT MESH: the slot's entity moved with its tile (Follow) - ready while its old mesh is up and traced
			int was = m_aSlotReady[tz2];
			m_aSlotReady[tz2] = 0;
			if (m_aSlotZone[tz2] != 0 && tz2 < m_aTileRes.Count() && m_aTileRes[tz2] && SlotTraced(tz2))
				m_aSlotReady[tz2] = 1;
			if (was != 0 && m_aSlotReady[tz2] == 0)
				s_iCoverGen++;
		}
		m_iScan = 0;
		m_iUntraced = NN;       // Grounds makes one pass and sets it back to 0
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! LAZY RUT MESH: every cell of live slot t has its ground (untraced cells draw no quad)
	protected bool SlotTraced(int t)
	{
		int ti = t % TILES;
		int tk = t / TILES;
		for (int lk = 0; lk < TV; lk++)
		{
			for (int li = 0; li < TV; li++)
			{
				if (m_F.m_aGs[(tk * TQ + lk) * N + ti * TQ + li] == 0)
					return false;
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! LAZY RUT MESH (MUD_WorldGroundEntity): changes whenever the sheet's cover may have shrunk (a mesh dropped, a
	//! live slot not ready, new bare cells) - held patches are checked again at once
	static int CoverGen()
	{
		return s_iCoverGen;
	}

	//------------------------------------------------------------------------------------------------
	//! LAZY RUT MESH (MUD_WorldGroundEntity): does the sheet draw over all of world box x0..x1, z0..z1 (x z)? Every
	//! tile within EDGE of it is a zone tile (no fade into the ground), and every tile it touches has its sheet mesh
	//! up (stored with a mesh, or a ready live slot) and no bare cell in reach (no holes, no taper). False when off.
	static bool LazyOn()
	{
		return LAZY_RUT_MESH && s_bLazyOn && SYNC;     // MUD LAB sheet local (SYNC false): patch meshes never hidden
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP LAZY A/B: flips s_bLazyOn every LAZY_AB_S s of play (rendering machines)
	protected static void LazyAbTick(float time)
	{
		if (LAZY_AB_S <= 0 || !LAZY_RUT_MESH)
			return;
		int phase = Math.Floor(time / LAZY_AB_S);
		if (phase == s_iLazyPhase)
			return;
		s_iLazyPhase = phase;
		s_bLazyOn = phase % 2 == 0;
		s_iCoverGen++;      // cached cover answers go: hidden patches show again when off
		string txt = "LAZY MESH OFF (patch meshes drawn under the mud)";
		if (s_bLazyOn)
			txt = "LAZY MESH ON (sheet only)";
		Print(string.Format("MUD lazy A/B: %1 at %2 s", txt, time), LogLevel.NORMAL);
		SCR_HintManagerComponent.ShowCustomHint(txt + ". Next switch in " + LAZY_AB_S.ToString() + " s", "Mud A/B", 6, true);
	}

	//------------------------------------------------------------------------------------------------
	static bool Covers(float x0, float z0, float x1, float z1)
	{
		if (!ENABLED || !LazyOn() || !s_Inst)
			return false;
		int why = s_Inst.CoversBox(x0, z0, x1, z1);
		s_aCoverNo.Set(why, s_aCoverNo[why] + 1);
		return why == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! R5 perf 2026-10-18 (research/2026-10-18-mud-zone-rut-perf.md): Covers answers since the last call, for the
	//! ground's 10 s line; resets them
	static string CoverWhy()
	{
		string s = string.Format("asked: yes %1, too big %2, zone edge %3, bare near %4, live slot not ready %5, stored tile no mesh %6",
			s_aCoverNo[0], s_aCoverNo[1], s_aCoverNo[2], s_aCoverNo[3], s_aCoverNo[4], s_aCoverNo[5]);
		for (int i = 0; i < 6; i++)
			s_aCoverNo.Set(i, 0);
		return s;
	}

	//! 0 = covered, else why not (s_aCoverNo)
	protected int CoversBox(float x0, float z0, float x1, float z1)
	{
		int ea = Math.Floor((x0 - EDGE) / TILE_M);
		int eb = Math.Floor((x1 + EDGE) / TILE_M);
		int ec = Math.Floor((z0 - EDGE) / TILE_M);
		int ed = Math.Floor((z1 + EDGE) / TILE_M);
		if (eb - ea > 8 || ed - ec > 8)
			return 1;     // a ground patch is 8 m: anything bigger is not asked for
		for (int ez = ec; ez <= ed; ez++)
		{
			for (int ex = ea; ex <= eb; ex++)
			{
				if (!TileInZone(ex, ez))
					return 2;
			}
		}
		int x0t = m_iX0 / TQ;
		int z0t = m_iZ0 / TQ;
		for (int tz = Math.Floor(z0 / TILE_M); tz <= Math.Floor(z1 / TILE_M); tz++)
		{
			for (int tx = Math.Floor(x0 / TILE_M); tx <= Math.Floor(x1 / TILE_M); tx++)
			{
				if (NearBareTile(tx, tz))
					return 3;
				if (IsLive(tx, tz))
				{
					int t = (tz - z0t) * TILES + tx - x0t;
					if (m_aSlotReady[t] == 0)
						return 4;
					continue;
				}
				MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
				if (!st || !st.m_Ent || !st.m_Res || st.m_D.m_iDrawn == 0)
					return 5;
			}
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Window cells i0..i1 x k0..k1 changed: their tiles (a tile owns cells ti * TQ .. ti * TQ + TQ) need a new mesh
	protected void MarkRect(int i0, int i1, int k0, int k1)
	{
		int ta = Math.Max(0, (i0 - 1) / TQ);
		int tb = Math.Min(TILES - 1, i1 / TQ);
		int ka = Math.Max(0, (k0 - 1) / TQ);
		int kb = Math.Min(TILES - 1, k1 / TQ);
		for (int tk = ka; tk <= kb; tk++)
		{
			for (int ti = ta; ti <= tb; ti++)
			{
				int t = tk * TILES + ti;
				m_aSlotDirty[t] = 1;
				m_aSlotWhy[t] = m_aSlotWhy[t] | m_iMarkWhy;     // builds by reason (sheet local log)
				if (m_bMarkNow)
					m_aSlotNow[t] = 1;     // SHEET LOCAL: a hole - built in the blast frame (DoDigNow)
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Map load: find the zones once. CAMERA RADIUS (Stage 3): then Radius() builds / drops tiles round the camera.
	//! Else (ONE_GRID / RADIUS_ON false): queue every zone tile once, build BUILD_MS per frame (traced, THICK, stored).
	protected void BuildZones()
	{
		if (!m_bZonesQueued)
		{
			m_bZonesQueued = true;
			MUD_MudZoneEntity.GetInWorld(m_aZones);
			m_iZoneTiles = 0;
			foreach (MUD_MudZoneEntity z : m_aZones)
			{
				float x0, z0, x1, z1;
				z.GetRect(x0, z0, x1, z1);
				int tx0 = Math.Floor(x0 / TILE_M);
				int tx1 = Math.Floor(x1 / TILE_M);
				int tz0 = Math.Floor(z0 / TILE_M);
				int tz1 = Math.Floor(z1 / TILE_M);
				for (int tz = tz0; tz <= tz1; tz++)
				{
					for (int tx = tx0; tx <= tx1; tx++)
					{
						if (!z.Contains((tx * TQ + TQ / 2) * CELL, (tz * TQ + TQ / 2) * CELL))
							continue;
						m_iZoneTiles++;
						if (RadiusOn())
							continue;
						m_aBuildTx.Insert(tx);
						m_aBuildTz.Insert(tz);
					}
				}
			}
			if (RadiusOn())
				Print(string.Format("MUD mud zones: %1 zones, %2 tiles, built within %3 m of the camera (no pre-build)", m_aZones.Count(), m_iZoneTiles, RADIUS), LogLevel.NORMAL);
			else
				Print(string.Format("MUD mud zones: %1 zones, %2 tiles to build (BUILD_MS per frame)", m_aZones.Count(), m_iZoneTiles), LogLevel.NORMAL);
			if (m_bPlaced)
				UpdateSlots();
		}
		if (RadiusOn())
		{
			Radius();
			RenderAB();
			return;
		}
		if (m_aBuildTx.IsEmpty())
			return;
		int tB = System.GetTickCount();
		while (!m_aBuildTx.IsEmpty() && System.GetTickCount(tB) < BUILD_MS)
		{
			int n = m_aBuildTx.Count();
			int bx = m_aBuildTx[n - 1];
			int bz = m_aBuildTz[n - 1];
			m_aBuildTx.Remove(n - 1);
			m_aBuildTz.Remove(n - 1);
			BuildZoneTile(bx, bz);
		}
		if (m_aBuildTx.IsEmpty())
		{
			Print(string.Format("MUD mud zones: all %1 tiles built, %2 stored, %3 ground reads on our ground (hole cells)", m_iZoneTiles, m_aTrailOrder.Count(), m_iStatHoles), LogLevel.NORMAL);
			m_iStatHoles = 0;
			if (ONE_GRID)
				HideZoneGround();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CAMERA RADIUS on (ONE_GRID needs every zone tile built, so it keeps the pre-build)
	protected bool RadiusOn()
	{
		return RADIUS_ON && !ONE_GRID;
	}

	//------------------------------------------------------------------------------------------------
	//! CAMERA RADIUS: every RADIUS_EVERY s the far meshes go and the near zone tiles without one are queued (nearest
	//! at the end); then this frame builds from the end for RAD_BUILD_MS (at least one tile)
	protected void Radius()
	{
		if (m_fLast >= m_fNextRad)
		{
			m_fNextRad = m_fLast + RADIUS_EVERY;
			vector cam[4];
			GetGame().GetWorld().GetCurrentCamera(cam);
			vector cp = cam[3];
			RadSpeed(cp);
			DropFar(cp);
			QueueNear(cp);
			m_vLodCam = cp;
			m_bLodCam = true;
			LodUpgrade(cp);
		}
		if (m_aBuildTx.IsEmpty() && m_aFrontTx.IsEmpty())
			return;
		int tB = System.GetTickCount();
		int built = 0;
		int buildMs = RAD_BUILD_MS;
		if (m_bLocal)
			buildMs = MESH_MS;     // MUD LAB part D
		// stage 2 A5: born / re-banded tiles first (same ms budget)
		while (!m_aFrontTx.IsEmpty() && (built == 0 || System.GetTickCount(tB) < buildMs))
		{
			int fx = m_aFrontTx[0];
			int fz = m_aFrontTz[0];
			m_aFrontTx.RemoveOrdered(0);
			m_aFrontTz.RemoveOrdered(0);
			BuildZoneTile(fx, fz);
			m_iBornFront++;
			built++;
		}
		while (!m_aBuildTx.IsEmpty() && (built == 0 || System.GetTickCount(tB) < buildMs))
		{
			int n = m_aBuildTx.Count();
			int bx = m_aBuildTx[n - 1];
			int bz = m_aBuildTz[n - 1];
			m_aBuildTx.Remove(n - 1);
			m_aBuildTz.Remove(n - 1);
			BuildZoneTile(bx, bz);
			built++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! TEST KIT render A/B (2026-10-18, research/2026-10-18-sheet-alternatives-and-perf.md P3; CLIENT file setting
	//! sheet_hide_ab = s, 0 = off): every s seconds the stored sheet meshes are hidden, then shown again (live slots
	//! stay). Not dropped - no rebuilds - so the 'MUD prof 5 s' render ms of shown vs hidden = the cost of drawing them.
	protected void RenderAB()
	{
		float per = MUD_TestKit.SheetHideAB();
		if (per <= 0)
			return;
		int cyc = Math.Floor(m_fLast / per);
		int phase = cyc % 2;
		if (phase == m_iAbPhase && m_fLast < m_fAbNext)
			return;
		m_fAbNext = m_fLast + 1;
		bool changed = phase != m_iAbPhase;
		m_iAbPhase = phase;
		if (phase == 0)
		{
			if (!changed)
				return;
			// shown again: only what RenderAB hid (tiles hidden for their own reasons stay hidden)
			int shown = 0;
			foreach (IEntity he : m_aAbHidden)
			{
				if (!he)
					continue;
				he.SetFlags(EntityFlags.VISIBLE, false);
				shown++;
			}
			m_aAbHidden.Clear();
			Print(string.Format("MUD render A/B: phase 0 - %1 stored sheet meshes %2 for %3 s", shown, MUD_Prof.AbWord(0), per), LogLevel.NORMAL);
			return;
		}
		// hidden: every visible stored mesh (again each second - a rebuilt tile shows itself)
		int hid = 0;
		foreach (int key, MUD_MudSheetStore st : m_mTrail)
		{
			if (!st || !st.m_Ent)
				continue;
			if ((st.m_Ent.GetFlags() & EntityFlags.VISIBLE) == 0)
				continue;
			st.m_Ent.ClearFlags(EntityFlags.VISIBLE, false);
			m_aAbHidden.Insert(st.m_Ent);
			hid++;
		}
		if (changed)
			Print(string.Format("MUD render A/B: phase 1 - %1 stored sheet meshes %2 for %3 s", hid, MUD_Prof.AbWord(1), per), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! CAMERA RADIUS: is world x, z within m of any zone's rectangle?
	protected bool ZoneNear(float x, float z, float m)
	{
		if (MUD_MudZoneEntity.AnyBox())
			return true;     // H3 ALL_OFFROAD: QueueNear's per-tile check decides
		foreach (MUD_MudZoneEntity zn : m_aZones)
		{
			if (!zn)
				continue;
			float x0, z0, x1, z1;
			zn.GetRect(x0, z0, x1, z1);
			if (x >= x0 - m && x <= x1 + m && z >= z0 - m && z <= z1 + m)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! CAMERA RADIUS: the build list anew - zone tiles whose middle is within RADIUS of cp (x z), not live, no mesh.
	//! Square rings from the outside in, so the nearest tiles sit at the end (built first).
	protected void QueueNear(vector cp)
	{
		m_aBuildTx.Clear();
		m_aBuildTz.Clear();
		m_iRadQueued = 0;
		float rad = m_fRadBuild;
		if (rad <= 0)
			rad = RADIUS;
		if (!ZoneNear(cp[0], cp[2], rad + TILE_M))
			return;
		// BUILD AHEAD: build order = distance to a point AHEAD_S s ahead of the camera (at most AHEAD_MAX m), so the tiles
		// you drive into come first; which tiles = still every tile within rad of the camera
		vector ap = cp + m_vRadVel * AHEAD_S;
		float ad = vector.DistanceXZ(ap, cp);
		if (ad > AHEAD_MAX)
			ap = cp + (ap - cp) * (AHEAD_MAX / ad);
		array<int> order = {};
		int slots = 1024;
		int dcmMax = 1000000;
		if (m_bLocal)
		{
			slots = 4096;     // MUD LAB part D: ~1710 tiles in the MESH_R 70 circle
			dcmMax = 500000;  // 500000 x 4096 + 4095 < 2^31
		}
		int ctx = Math.Floor(cp[0] / TILE_M);
		int ctz = Math.Floor(cp[2] / TILE_M);
		int rt = Math.Ceil(rad / TILE_M) + 1;
		float r2 = rad * rad;
		for (int ring = rt; ring >= 0; ring--)
		{
			for (int dz = -ring; dz <= ring; dz++)
			{
				// the ring's top and bottom rows whole, the rows between only its two side tiles
				int stepX = 1;
				if (ring > 0 && dz != -ring && dz != ring)
					stepX = 2 * ring;
				for (int dx = -ring; dx <= ring; dx += stepX)
				{
					int tx = ctx + dx;
					int tz = ctz + dz;
					float mx = (tx * TQ + TQ / 2) * CELL - cp[0];
					float mz = (tz * TQ + TQ / 2) * CELL - cp[2];
					if (mx * mx + mz * mz > r2)
						continue;
					if (IsLive(tx, tz))
						continue;
					MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
					if (st && st.m_Ent)
						continue;
					if (!SheetWanted(tx, tz))
						continue;
					// sort code: distance to the ahead point in cm x 1024 + list index (ints sort exactly)
					float ox = (tx * TQ + TQ / 2) * CELL - ap[0];
					float oz = (tz * TQ + TQ / 2) * CELL - ap[2];
					int dcm = Math.Min(Math.Round(Math.Sqrt(ox * ox + oz * oz) * 100), dcmMax);
					order.Insert(dcm * slots + m_aBuildTx.Count());
					m_aBuildTx.Insert(tx);
					m_aBuildTz.Insert(tz);
				}
			}
		}
		m_iRadQueued = m_aBuildTx.Count();
		if (m_iRadQueued < 2 || m_iRadQueued >= slots)
			return;     // (>= 1024 cannot happen at RADIUS 40: ~560 tiles in the circle; then the ring order stays)
		// Radius() builds from the END: farthest from the ahead point first in the list, the nearest last
		order.Sort();
		array<int> sx = {};
		array<int> sz = {};
		for (int o = order.Count() - 1; o >= 0; o--)
		{
			int idx = order[o] % slots;
			sx.Insert(m_aBuildTx[idx]);
			sz.Insert(m_aBuildTz[idx]);
		}
		m_aBuildTx.Copy(sx);
		m_aBuildTz.Copy(sz);
	}

	//------------------------------------------------------------------------------------------------
	//! SPEED RADIUS + BUILD AHEAD: camera velocity (x z, smoothed) from its move since the last radius check; the build
	//! radius shrinks at speed so the tiles a second we must build stay near what one build a frame gives
	protected void RadSpeed(vector cp)
	{
		float dt = m_fLast - m_fRadPrevT;
		if (m_fRadPrevT > 0 && dt > 0.01 && dt < 2)
		{
			vector v = (cp - m_vRadPrev) * (1 / dt);
			v[1] = 0;
			if (v.Length() > 60)
				v = vector.Zero;     // teleport / respawn / camera switch: no speed
			m_vRadVel = m_vRadVel * 0.5 + v * 0.5;
		}
		m_vRadPrev = cp;
		m_fRadPrevT = m_fLast;
		float speed = m_vRadVel.Length();
		m_fRadBuild = RADIUS;
		if (m_bLocal)
			m_fRadBuild = MESH_R;     // MUD LAB part D: fixed mesh radius
		if (SPEED_RADIUS && !m_bLocal && speed > 1)
			m_fRadBuild = Math.Clamp(SPEED_RADIUS_K / speed, RADIUS_MIN, RADIUS);
	}

	//------------------------------------------------------------------------------------------------
	//! CAMERA RADIUS: stored tiles whose middle is more than RADIUS + RADIUS_HYST from cp (x z) lose their mesh (hidden
	//! now, deleted 1 s later as m_aDying) and their store (Stage 4: bare cells come back from the sync layer).
	protected void DropFar(vector cp)
	{
		float far = RADIUS + RADIUS_HYST;
		if (m_bLocal)
			far = MESH_R + MESH_DROP;     // MUD LAB part D
		float far2 = far * far;
		// MESH KEEP: past-radius meshes (key + distance); with MESH_CAP only the farthest over the cap go
		int meshes = 0;
		array<int> farKey = {};
		array<float> farD2 = {};
		for (int n = m_aTrailOrder.Count() - 1; n >= 0; n--)
		{
			int key = m_aTrailOrder[n];
			MUD_MudSheetStore st = m_mTrail.Get(key);
			if (!st)
			{
				m_aTrailOrder.RemoveOrdered(n);
				continue;
			}
			float mx = (st.m_iTx * TQ + TQ / 2) * CELL - cp[0];
			float mz = (st.m_iTz * TQ + TQ / 2) * CELL - cp[2];
			float d2 = mx * mx + mz * mz;
			if (!st.m_Ent)
			{
				if (m_bLocal && d2 > far2)
					FarPut(key, st);     // MUD LAB part D: a far data-only store goes compact
				continue;
			}
			meshes++;
			if (d2 <= far2)
				continue;
			farKey.Insert(key);
			farD2.Insert(d2);
		}
		int drop = farKey.Count();
		bool cap = MESH_CAP > 0 && !m_bLocal;     // MUD LAB part D: SHEET LOCAL drops every mesh past MESH_R + MESH_DROP
		if (cap)
			drop = Math.Min(drop, meshes - MESH_CAP);
		for (int c = 0; c < drop; c++)
		{
			int pick = 0;
			if (cap)
			{
				for (int f = 1; f < farD2.Count(); f++)
				{
					if (farD2[f] > farD2[pick])
						pick = f;
				}
			}
			DropMesh(farKey[pick]);
			farKey.Remove(pick);
			farD2.Remove(pick);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CAMERA RADIUS: stored tile key loses its mesh (hidden now, deleted 1 s later as m_aDying) and its store (Stage 4:
	//! bare cells come back from the sync layer)
	protected void DropMesh(int key)
	{
		MUD_MudSheetStore st = m_mTrail.Get(key);
		if (!st || !st.m_Ent)
			return;
		if (m_bLocal)
		{
			// MUD LAB part D: deleted at once (no patch re-lift to wait for in this mode), the data goes compact
			DropColl(st.m_Ent);
			SCR_EntityHelper.DeleteEntityAndChildren(st.m_Ent);
			st.m_Ent = null;
			st.m_Res = null;
			st.m_D.m_iUnlift = 0;
			m_iSt10Drops++;
			s_iCoverGen++;
			FarPut(key, st);
			return;
		}
		MUD_MudSheetStore dying = new MUD_MudSheetStore();
		dying.m_Ent = st.m_Ent;
		dying.m_Res = st.m_Res;
		dying.m_fDieAt = m_fLast + 1;
		if (LIFT_BY_DRAWN && st.m_D.m_iUnlift != 0)
			MUD_WorldGroundEntity.SheetLiftDirty(st.m_iTx * TILE_M, st.m_iTz * TILE_M);     // patches lift back under it first
		else
			st.m_Ent.ClearFlags(EntityFlags.VISIBLE, false);
		st.m_D.m_iUnlift = 0;
		m_aDying.Insert(dying);
		st.m_Ent = null;
		st.m_Res = null;
		m_iSt10Drops++;
		s_iCoverGen++;     // LAZY RUT MESH: patches under it show again
		if (m_bLocal || (HasBare(st.m_D) && !SYNC))
			return;        // no sync layer to hold the bare cells (SHEET LOCAL: nor the mud): kept, data only
		m_mTrail.Remove(key);
		int at = m_aTrailOrder.Find(key);
		if (at >= 0)
			m_aTrailOrder.RemoveOrdered(at);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasBare(MUD_MudSheetTileData d)
	{
		for (int l = 0; l < TV * TV; l++)
		{
			if (d.m_aBare[l] != 0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! CAMERA RADIUS: live slot ti, tk (world tile tx, tz) with no built tile - its cells the window has no data for
	//! (m_aGs 0: new, or a shared edge not traced yet) take the sync layer's mud; a kept store (data only, bare cells)
	//! gives its bare cells and goes. The ground is traced again by Grounds (a kept store's ground is old).
	protected void FillSlot(int ti, int tk, int tx, int tz)
	{
		int key = TrailKey(tx, tz);
		MUD_MudSheetStore st = TrailGet(key);     // MUD LAB part D: a compact one comes back first
		float sd;
		float sou;
		float sov;
		int sb;
		for (int lk = 0; lk < TV; lk++)
		{
			for (int li = 0; li < TV; li++)
			{
				int v = (tk * TQ + lk) * N + ti * TQ + li;
				if (m_F.m_aGs[v] != 0)
					continue;
				int l = lk * TV + li;
				float d = THICK;
				float ou = 0;
				float ov = 0;
				int bare = 0;
				if (st)
				{
					d = st.m_D.m_aD[l];
					ou = st.m_D.m_aOu[l];
					ov = st.m_D.m_aOv[l];
					bare = st.m_D.m_aBare[l];
				}
				if (SYNC && bare == 0 && MUD_MudSync.Get(tx * TQ + li, tz * TQ + lk, sd, sou, sov, sb))
				{
					d = sd;
					ou = sou;
					ov = sov;
					bare = sb;     // Stage 4: cleared by a crater / dig op
				}
				m_F.m_aD[v] = d;
				m_F.m_aOu[v] = ou;
				m_F.m_aOv[v] = ov;
				m_F.m_aBare[v] = bare;
				m_F.m_aTouch[v] = 0;
				m_F.m_aBand[v] = 0;
				m_F.m_aLastY[v] = 0;
				if (st)
				{
					m_F.m_aTouch[v] = st.m_D.m_aTouch[l];     // SHEET LOCAL: a kept store's deposit band source
					m_F.m_aBand[v] = st.m_D.m_aBand[l];
				}
			}
		}
		if (!st)
			return;
		m_mTrail.Remove(key);
		RemoveTrailKey(key);
	}

	//------------------------------------------------------------------------------------------------
	//! One zone tile: traces on the even cells (11 x 11), the cells between filled, the THICK layer (SYNC: the sync
	//! layer's mud), a mesh. CAMERA RADIUS: a kept store (data only) is built again - fresh ground, the sync mud,
	//! its bare cells kept.
	protected void BuildZoneTile(int tx, int tz)
	{
		int key = TrailKey(tx, tz);
		if (IsLive(tx, tz))
			return;     // the window has it already
		MUD_MudSheetStore st = TrailGet(key);     // MUD LAB part D: a compact one comes back first
		if (st && st.m_Ent)
			return;     // duplicate (overlapping zones) or built already
		bool kept = st != null;
		if (!kept)
		{
			st = new MUD_MudSheetStore();
			st.m_iTx = tx;
			st.m_iTz = tz;
		}
		MUD_MudSheetTileData d = st.m_D;
		int tZ = System.GetTickCount();     // MUD_Prof z parts (2026-10-18 all-offroad deep dive: ~2.9 ms a tile)
		for (int lk = 0; lk < TV; lk += 2)
		{
			for (int li = 0; li < TV; li += 2)
				d.m_aG[lk * TV + li] = GroundDown((tx * TQ + li) * CELL, (tz * TQ + lk) * CELL);
		}
		MUD_Prof.Add(MUD_Prof.Z_GROUND, System.GetTickCount(tZ));
		tZ = System.GetTickCount();
		// TILE CHEAP (2026-10-18): no sync tile here = plain THICK mud everywhere, no 441 Get calls
		bool syncAny = SYNC && MUD_MudSync.AnyTileNear(tx, tz);
		for (int k = 0; k < TV; k++)
		{
			for (int i = 0; i < TV; i++)
			{
				int l = k * TV + i;
				d.m_aGs[l] = 1;
				if (kept && (d.m_aBare[l] != 0 || m_bLocal))
					continue;   // blown / dug mud stays as it was (a kept store: SYNC off only since Stage 4; SHEET LOCAL: all its data)
				d.m_aD[l] = THICK;
				d.m_aOu[l] = 0;
				d.m_aOv[l] = 0;
				d.m_aBare[l] = 0;
				// SYNC: rut ops that came before this tile was built (a no-op where the sync layer has no tile); Stage 4:
				// craters / digs there cleared it (bare)
				if (syncAny)
				{
					float sd;
					float sou;
					float sov;
					int sb;
					if (MUD_MudSync.Get(tx * TQ + i, tz * TQ + k, sd, sou, sov, sb))
					{
						d.m_aD[l] = sd;
						d.m_aOu[l] = sou;
						d.m_aOv[l] = sov;
						d.m_aBare[l] = sb;
					}
				}
			}
		}
		FillTile(d);
		int bD = s_iBandMsAll;     // stage 2 A1
		if (m_bLocal)
			BandTile(tx, tz, d, 0, 0);     // SHEET LOCAL: band from the touched cells around it
		MUD_Prof.Add(MUD_Prof.Z_DATA, System.GetTickCount(tZ) - (s_iBandMsAll - bD));
		tZ = System.GetTickCount();
		st.m_Ent = SpawnTile(tx, tz, d.m_aG[0]);
		MUD_Prof.Add(MUD_Prof.Z_SPAWN, System.GetTickCount(tZ));
		tZ = System.GetTickCount();
		st.m_Res = BuildTile(st.m_Ent, d, tx, tz, false);
		MUD_Prof.Add(MUD_Prof.Z_MESH, System.GetTickCount(tZ));
		if (kept)
			return;
		m_mTrail.Set(key, st);
		m_aTrailOrder.Insert(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Tile data: every cell with an odd i or k gets the mean of its traced (even) neighbours
	protected void FillTile(MUD_MudSheetTileData d)
	{
		for (int k = 0; k < TV; k++)
		{
			for (int i = 0; i < TV; i++)
			{
				if (i % 2 == 0 && k % 2 == 0)
					continue;
				int i0 = i - i % 2;
				int i1 = i0 + (i % 2) * 2;
				int k0 = k - k % 2;
				int k1 = k0 + (k % 2) * 2;
				d.m_aG[k * TV + i] = (d.m_aG[k0 * TV + i0] + d.m_aG[k0 * TV + i1] + d.m_aG[k1 * TV + i0] + d.m_aG[k1 * TV + i1]) * 0.25;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.GoLive: the voxel ground in the x, z box lo..hi just changed on screen. blastR > 0: a crater at wp
	//! (blastR = its crest radius) blew the mud off (BlastKeep). The mud tiles there are redone over the
	//! next frames (BUILD_MS each), starting JOB_DELAY later so the new brick bodies are in for the traces.
	static void GroundChanged(vector lo, vector hi, vector wp, float blastR)
	{
		if (!ENABLED || !s_Inst)
			return;
		int tCt = System.GetTickCount();
		if (MUD_TestKit.TEST_KIT && blastR > 0)
			MUD_CraterTrace.SheetHit(wp);     // stage 2 B1 crater log
		s_Inst.m_bMarkNow = s_Inst.m_bLocal;     // SHEET LOCAL: slots a hole marks build in the blast frame (DoDigNow)
		s_Inst.QueueChange(lo, hi, wp, blastR);
		s_Inst.m_bMarkNow = false;
		if (MUD_TestKit.TEST_KIT)
			MUD_CraterTrace.SheetMs(System.GetTickCount(tCt));
	}

	//------------------------------------------------------------------------------------------------
	//! Synced sheet Stage 1 (2026-10-18): MUD_VoxelProbe.Submit - a rut op changed the rut-field floor / ridges in the
	//! x, z box lo..hi. The mud's ground there is read again now (HoleGround: the data, not the patch collider); the mud
	//! depth stays (user look choice: drawn = felt ground + mud, the rut deepens under the mud). Live tiles rebuild with
	//! the next upload, stored ones in FlushOneBuild (next Frame).
	//! RE-GROUND EXACT (2026-10-18): xs / zs = the floor samples the op changed (MUD_VoxelProbe); only the traced sheet
	//! cells within one floor sample of one are read again. Empty or capped list -> the whole box as before.
	static void FloorChanged(vector lo, vector hi, array<float> xs, array<float> zs)
	{
		if (!ENABLED || !s_Inst)
			return;
		if (REGROUND_EXACT && xs && !xs.IsEmpty() && xs.Count() < MUD_VoxelProbe.REGROUND_EXACT_MAX)
			s_Inst.ReGroundAt(xs, zs, true);
		else
			s_Inst.ReGround(lo, hi);
	}

	//------------------------------------------------------------------------------------------------
	//! H2: MUD_LookRuts wrote (or dropped) look floor samples under the sheet at xs / zs: those cells re-read their
	//! ground at once (no ease, no drop log: the guess IS the instant rut; the log measures the real cut landing)
	static void LookChanged(array<float> xs, array<float> zs)
	{
		if (!ENABLED || !s_Inst || !xs || xs.IsEmpty())
			return;
		s_Inst.m_iLookCuts10++;
		s_Inst.ReGroundAt(xs, zs, false);
	}

	//------------------------------------------------------------------------------------------------
	//! RE-GROUND EXACT: each changed floor sample re-reads the even (traced) sheet cells within REGROUND_R of it, once
	//! each; a cell on a tile border goes to both tiles (as the box path: border verts live in both)
	protected void ReGroundAt(array<float> xs, array<float> zs, bool fromOp)
	{
		map<int, bool> seen = new map<int, bool>();
		map<int, ref MUD_MudSheetTileData> hitTiles = new map<int, ref MUD_MudSheetTileData>();
		map<int, int> hitTx = new map<int, int>();
		map<int, int> hitTz = new map<int, int>();
		float dropMax = 0;
		float dropX = 0;
		float dropZ = 0;
		int cells = 0;
		for (int n = 0; n < xs.Count(); n++)
		{
			int gi0 = Math.Ceil((xs[n] - REGROUND_R) / CELL);
			int gi1 = Math.Floor((xs[n] + REGROUND_R) / CELL);
			int gk0 = Math.Ceil((zs[n] - REGROUND_R) / CELL);
			int gk1 = Math.Floor((zs[n] + REGROUND_R) / CELL);
			if (gi0 % 2 != 0)
				gi0++;
			if (gk0 % 2 != 0)
				gk0++;
			for (int gk = gk0; gk <= gk1; gk += 2)
			{
				for (int gi = gi0; gi <= gi1; gi += 2)
				{
					int key = PassKey(gi, gk);
					if (seen.Contains(key))
						continue;
					seen.Set(key, true);
					cells++;
					float wx = gi * CELL;
					float wz = gk * CELL;
					// the 1, 2 or 4 tiles holding this vertex (a border vertex is in both tiles' data)
					int txA = gi / TQ;     // world cells are > 0 on Arland
					int tzA = gk / TQ;
					int txB = txA;
					int tzB = tzA;
					if (gi - txA * TQ == 0)
						txB = txA - 1;
					if (gk - tzA * TQ == 0)
						tzB = tzA - 1;
					bool liveDone = false;
					for (int tz = tzB; tz <= tzA; tz++)
					{
						for (int tx = txB; tx <= txA; tx++)
						{
							if (IsLive(tx, tz))
							{
								if (liveDone)
									continue;
								liveDone = true;
								int i = gi - m_iX0;
								int k = gk - m_iZ0;
								if (i < 0 || i >= N || k < 0 || k >= N || m_F.m_aGs[k * N + i] == 0)
									continue;   // not traced yet: Grounds reads it
								float gOld = m_F.m_aG[k * N + i];
								Trace(i, k);
								float gNew = m_F.m_aG[k * N + i];
								if (gOld - gNew > dropMax)
								{
									dropMax = gOld - gNew;
									dropX = wx;
									dropZ = wz;
								}
								if (fromOp && EASE_S > 0 && gOld - gNew > EASE_MIN)
								{
									m_F.m_aG[k * N + i] = gOld;     // EASE DROP: EaseTick slides it down
									EaseAdd(gi, gk, gOld, gNew);
								}
								else if (!fromOp)
								{
									EaseDrop(gi, gk);     // H2: a guess beats a running ease
								}
								FillAround(i, k);
								continue;
							}
							int tkey = TrailKey(tx, tz);
							MUD_MudSheetStore st = m_mTrail.Get(tkey);
							if (!st || !st.m_Ent)
								continue;
							MUD_MudSheetTileData d = st.m_D;
							int l = (gk - tz * TQ) * TV + gi - tx * TQ;
							float ng = GroundDown(wx, wz);
							if (Math.AbsFloat(ng - d.m_aG[l]) <= FIELD_EPS)
								continue;
							if (d.m_aG[l] - ng > dropMax)
							{
								dropMax = d.m_aG[l] - ng;
								dropX = wx;
								dropZ = wz;
							}
							d.m_aG[l] = ng;
							hitTiles.Set(tkey, d);
							hitTx.Set(tkey, tx);
							hitTz.Set(tkey, tz);
						}
					}
				}
			}
		}
		foreach (int hk, MUD_MudSheetTileData hd : hitTiles)
		{
			FillTile(hd);
			QueueBuild(hitTx.Get(hk), hitTz.Get(hk));
		}
		if (!fromOp)
		{
			m_iLookCells10 += cells;
			return;
		}
		m_iReCells10 += cells;
		m_iReOps10++;
		if (dropMax > 0.005)
			DropLog(dropMax, dropX, dropZ);
	}

	protected void ReGround(vector lo, vector hi)
	{
		float rx0 = lo[0] - 2 * CELL;
		float rx1 = hi[0] + 2 * CELL;
		float rz0 = lo[2] - 2 * CELL;
		float rz1 = hi[2] + 2 * CELL;
		// a rut op box is a few m (one vehicle, 0.25 s of segments); the box comes off the wire, so a garbage one must
		// not loop over the whole map's tiles
		if (rx1 - rx0 > REGROUND_MAX || rz1 - rz0 > REGROUND_MAX)
		{
			Print(string.Format("MUD mud sheet: rut floor box %1 x %2 m too big, mud ground not re-read", rx1 - rx0, rz1 - rz0), LogLevel.WARNING);
			return;
		}
		int tx0 = Math.Floor(rx0 / TILE_M);
		int tx1 = Math.Floor(rx1 / TILE_M);
		int tz0 = Math.Floor(rz0 / TILE_M);
		int tz1 = Math.Floor(rz1 / TILE_M);
		// TEMP H1 drop log (plans/2026-10-18-hybrid-sheet-feel-plan.md): biggest drop of the drawn mud surface this op
		// (mud depth stays, so drawn drop = ground drop) and where
		float dropMax = 0;
		float dropX = 0;
		float dropZ = 0;
		for (int tz = tz0; tz <= tz1; tz++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				if (IsLive(tx, tz))
				{
					int i0 = (tx - m_iX0 / TQ) * TQ;
					int k0 = (tz - m_iZ0 / TQ) * TQ;
					// window parity = world parity (m_iX0 / m_iZ0 are whole tiles): even cells are the traced ones
					for (int k = k0; k <= k0 + TQ; k += 2)
					{
						float wz = (m_iZ0 + k) * CELL;
						if (wz < rz0 || wz > rz1)
							continue;
						for (int i = i0; i <= i0 + TQ; i += 2)
						{
							float wx = (m_iX0 + i) * CELL;
							if (wx < rx0 || wx > rx1 || m_F.m_aGs[k * N + i] == 0)
								continue;   // not traced yet: Grounds reads it
							float gOld = m_F.m_aG[k * N + i];
							Trace(i, k);
							float gNew = m_F.m_aG[k * N + i];
							if (gOld - gNew > dropMax)
							{
								dropMax = gOld - gNew;
								dropX = wx;
								dropZ = wz;
							}
							if (EASE_S > 0 && gOld - gNew > EASE_MIN)
							{
								m_F.m_aG[k * N + i] = gOld;     // EASE DROP: EaseTick slides it down
								EaseAdd(m_iX0 + i, m_iZ0 + k, gOld, gNew);
							}
							FillAround(i, k);
						}
					}
					continue;
				}
				MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
				if (!st || !st.m_Ent)
					continue;
				MUD_MudSheetTileData d = st.m_D;
				bool hit = false;
				for (int lk = 0; lk < TV; lk += 2)
				{
					float sz = (tz * TQ + lk) * CELL;
					if (sz < rz0 || sz > rz1)
						continue;
					for (int li = 0; li < TV; li += 2)
					{
						float sx = (tx * TQ + li) * CELL;
						if (sx < rx0 || sx > rx1)
							continue;
						int l = lk * TV + li;
						float ng = GroundDown(sx, sz);
						if (Math.AbsFloat(ng - d.m_aG[l]) <= FIELD_EPS)
							continue;
						if (d.m_aG[l] - ng > dropMax)
						{
							dropMax = d.m_aG[l] - ng;
							dropX = sx;
							dropZ = sz;
						}
						d.m_aG[l] = ng;
						hit = true;
					}
				}
				if (!hit)
					continue;
				FillTile(d);
				QueueBuild(tx, tz);
			}
		}
		if (dropMax > 0.005)
			DropLog(dropMax, dropX, dropZ);
	}

	//------------------------------------------------------------------------------------------------
	//! EASE DROP: world cell gi, gk slides from g0 down to g1 over EASE_S from now (a newer cut on it restarts from g0)
	protected void EaseAdd(int gi, int gk, float g0, float g1)
	{
		MUD_MudSheetEase e = new MUD_MudSheetEase();
		e.m_iGi = gi;
		e.m_iGk = gk;
		e.m_fFrom = g0;
		e.m_fTo = g1;
		e.m_fT0 = m_fLast;
		m_mEase.Set(PassKey(gi, gk), e);
	}

	//------------------------------------------------------------------------------------------------
	//! H2: cell gi, gk was just traced to its final ground - a running ease must not pull it back
	protected void EaseDrop(int gi, int gk)
	{
		if (!m_mEase.IsEmpty())
			m_mEase.Remove(PassKey(gi, gk));
	}

	//------------------------------------------------------------------------------------------------
	//! EASE DROP, each frame: every sliding cell one step on (smoothstep); done or out of the window -> its final ground
	protected void EaseTick(float time)
	{
		array<int> done = {};
		foreach (int key, MUD_MudSheetEase e : m_mEase)
		{
			int i = e.m_iGi - m_iX0;
			int k = e.m_iGk - m_iZ0;
			if (i < 0 || i >= N || k < 0 || k >= N || m_F.m_aGs[k * N + i] == 0)
			{
				done.Insert(key);     // left the window: a later trace reads the real ground
				continue;
			}
			float f = (time - e.m_fT0) / EASE_S;
			float g = e.m_fTo;
			if (f < 1)
			{
				f = Math.Clamp(f, 0, 1);
				g = e.m_fFrom + (e.m_fTo - e.m_fFrom) * f * f * (3 - 2 * f);
			}
			else
			{
				done.Insert(key);
			}
			m_F.m_aG[k * N + i] = g;
			FillAround(i, k);
			MarkRect(i, i, k, k);
			m_bDirty = true;
		}
		foreach (int dk : done)
			m_mEase.Remove(dk);
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP H1 drop log: the floor cut dropped the drawn mud by drop m at x, z; how far is the nearest own-tracked
	//! wheel contact (any vehicle in m_aVeh) - ~1.5 m = the cut lands behind the truck as planned
	protected void DropLog(float drop, float x, float z)
	{
		float best = 1000000;
		foreach (MUD_MudSheetVeh mv : m_aVeh)
		{
			if (!mv || !mv.m_Veh)
				continue;
			for (int w = 0; w < mv.m_aLast.Count(); w++)
			{
				if (!mv.m_aHas[w])
					continue;
				vector p = mv.m_aLast[w];
				float dd = (p[0] - x) * (p[0] - x) + (p[2] - z) * (p[2] - z);
				if (dd < best)
					best = dd;
			}
		}
		float dist = -1;
		if (best < 1000000)
			dist = Math.Sqrt(best);
		Print(string.Format("MUD H1 drop: %1 m at %2 %3, nearest wheel %4 m", drop, x, z, dist), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueChange(vector lo, vector hi, vector wp, float blastR)
	{
		int c = m_aChgLo.Count();
		m_aChgLo.Insert(lo);
		m_aChgHi.Insert(hi);
		m_aChgWp.Insert(wp);
		MarkBareTiles(lo, hi, wp, blastR);
		int tBlast = System.GetTickCount();
		int blastTiles = 0;
		if (DIG_NOW)
		{
			m_aNowLo.Insert(lo);
			m_aNowHi.Insert(hi);
		}
		if (BLAST_NOW && blastR > 0)
		{
			blastTiles = BlastNow(lo, hi, wp, blastR);
			m_aChgR.Insert(-blastR);
		}
		else
			m_aChgR.Insert(blastR);
		tBlast = System.GetTickCount(tBlast);
		int tx0 = Math.Floor(lo[0] / TILE_M);
		int tx1 = Math.Floor(hi[0] / TILE_M);
		int tz0 = Math.Floor(lo[2] / TILE_M);
		int tz1 = Math.Floor(hi[2] / TILE_M);
		int jobs = 0;
		for (int tz = tz0; tz <= tz1; tz++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				if (DIG_NOW && !TRACE_REDO)
					continue;   // DigNow already did it in the go-live frame
				if (!m_mTrail.Contains(TrailKey(tx, tz)) && !IsLive(tx, tz))
					continue;   // no mud tile there
				m_aJobTx.Insert(tx);
				m_aJobTz.Insert(tz);
				m_aJobC.Insert(c);
				jobs++;
			}
		}
		if (m_aJobTx.IsEmpty())
		{
			// no mud there and nothing in flight: forget the change
			m_aChgLo.Clear();
			m_aChgHi.Clear();
			m_aChgWp.Clear();
			m_aChgR.Clear();
		}
		else
		{
			m_fJobsAt = m_fLast + JOB_DELAY;
			if (m_iJobT0 == 0)
				m_iJobT0 = System.GetTickCount();
		}
		Print(string.Format("MUD mud sheet: voxel ground changed x %1..%2 z %3..%4, blast %5 m at %6 -> %7 mud tiles to redo",
			lo[0], hi[0], lo[2], hi[2], blastR, wp, jobs) + string.Format(" | blast now: %1 tiles in %2 ms", blastTiles, tBlast), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! TAPER trim (Stage 3): the tiles change lo..hi (and its blast reach) touches may get bare cells - every way of
	//! baring mud (BlastNow, DigNow, RedoTile) comes from QueueChange. A box bigger than REGROUND_MAX marks all.
	protected void MarkBareTiles(vector lo, vector hi, vector wp, float blastR)
	{
		float x0 = lo[0];
		float x1 = hi[0];
		float z0 = lo[2];
		float z1 = hi[2];
		if (blastR > 0)
		{
			float reach = BlastReach(blastR);
			x0 = Math.Min(x0, wp[0] - reach);
			x1 = Math.Max(x1, wp[0] + reach);
			z0 = Math.Min(z0, wp[2] - reach);
			z1 = Math.Max(z1, wp[2] + reach);
		}
		x0 -= CELL;
		x1 += CELL;
		z0 -= CELL;
		z1 += CELL;
		if (x1 - x0 > REGROUND_MAX || z1 - z0 > REGROUND_MAX)
		{
			if (!m_bBareAll)
				Print(string.Format("MUD mud sheet: voxel change box %1 x %2 m too big to mark, every tile tapers from now", x1 - x0, z1 - z0), LogLevel.WARNING);
			m_bBareAll = true;
			s_iCoverGen++;
			return;
		}
		for (int tz = Math.Floor(z0 / TILE_M); tz <= Math.Floor(z1 / TILE_M); tz++)
		{
			for (int tx = Math.Floor(x0 / TILE_M); tx <= Math.Floor(x1 / TILE_M); tx++)
				m_mBareTiles.Set(TrailKey(tx, tz), 1);
		}
		s_iCoverGen++;      // LAZY RUT MESH: patches near new bare cells show again
	}

	//------------------------------------------------------------------------------------------------
	//! TAPER trim: may tile tx, tz or one of its 8 neighbours hold a bare cell? (false = TaperField has nothing to do)
	protected bool NearBareTile(int tx, int tz)
	{
		if (m_bBareAll || ONE_GRID)
			return true;
		if (m_mBareTiles.IsEmpty())
			return false;
		for (int dz = -1; dz <= 1; dz++)
		{
			for (int dx = -1; dx <= 1; dx++)
			{
				if (m_mBareTiles.Contains(TrailKey(tx + dx, tz + dz)))
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Redo queued mud tiles (BUILD_MS per frame); logs once the queue is empty
	protected void GroundJobs()
	{
		if (m_aJobTx.IsEmpty() || m_fLast < m_fJobsAt)
			return;
		int t0 = System.GetTickCount();
		while (!m_aJobTx.IsEmpty() && System.GetTickCount(t0) < BUILD_MS)
		{
			int n = m_aJobTx.Count() - 1;
			int tx = m_aJobTx[n];
			int tz = m_aJobTz[n];
			int c = m_aJobC[n];
			m_aJobTx.Remove(n);
			m_aJobTz.Remove(n);
			m_aJobC.Remove(n);
			RedoTile(tx, tz, c);
		}
		if (!m_aJobTx.IsEmpty())
			return;
		Print(string.Format("MUD mud sheet: redid %1 mud tiles (%2 live, %3 stored) for %4 voxel changes in %5 ms, %6 ground traces, %7 cells blown bare, %8 reads on our ground",
			m_iJobLive + m_iJobStored, m_iJobLive, m_iJobStored, m_aChgLo.Count(), System.GetTickCount(m_iJobT0), m_iJobTraces, m_iJobBare, m_iStatHoles), LogLevel.NORMAL);
		m_iStatHoles = 0;
		m_aChgLo.Clear();
		m_aChgHi.Clear();
		m_aChgWp.Clear();
		m_aChgR.Clear();
		m_iJobT0 = 0;
		m_iJobLive = 0;
		m_iJobStored = 0;
		m_iJobTraces = 0;
		m_iJobBare = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! DIG NOW: mud vertices in the box lo..hi (+1 cell) whose voxel ground moved more than MOVED lose their mud and
	//! take the new height, in this frame. Returns tiles changed.
	protected ref array<vector> m_aNowLo = {};
	protected ref array<vector> m_aNowHi = {};
	protected ref array<int> m_aNowTiles = {};   // stored tiles BlastNow changed, built in DigNow
	protected bool m_bOneHidden;                  // ONE_GRID: the zone skin cells are VOX, the sheet is the ground
	// ONE_GRID: per tile (TrailKey) the ground height of each vertex before any op changed it (-100000 = never
	// changed, the ground now is the undug one). Replaces a heightmap trace per changed vertex (rut op 11 ms avg).
	protected ref map<int, ref array<float>> m_mUndug = new map<int, ref array<float>>();
	// DIAG (2026-10-15 flat plane / squares): per DigNow, sheet verts over voxels, holes opened, voxel - sheet range
	protected int m_iDgN;
	protected int m_iDgHole;
	protected int m_iDgUp;
	protected float m_fDgMin;
	protected float m_fDgMax;
	protected int m_iOneBuildMs;                  // ONE_GRID: ms of the last op spent in BuildTile (log)
	protected ref array<float> m_aTp = {};       // TaperField: cells to the nearest bare vertex, by tile vertex l
	protected int m_iTaperMs;                   // DIAG: TaperField time since the last stored-tile flush
	protected ref array<ref MUD_VoxelOp> m_aPend = {};   // SYNC: sheet-and-voxel ops waiting for the go-live
	protected int m_iPendT;
	protected bool m_bNowDue;                   // FlushNow came: DigNow once the held ops are applied (StepNow)
	protected ref array<int> m_aOneBuild = {};    // ONE_GRID: stored tiles trench / fill ops changed, built once next Frame

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID: every skin hole cell fully covered by zone tiles goes VOX (our patch mesh + collider out there)
	protected void HideZoneGround()
	{
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!skin)
		{
			Print("MUD one grid: no coarse skin, ground not hidden", LogLevel.WARNING);
			return;
		}
		float sx0, sz0;
		int ncx, ncz;
		skin.GetGrid(sx0, sz0, ncx, ncz);
		float sc = skin.GetCell();
		array<int> cxs = {};
		array<int> czs = {};
		array<bool> want = {};
		int cx0 = 1000000;
		int cx1 = -1000000;
		int cz0 = 1000000;
		int cz1 = -1000000;
		int notHole = 0;
		foreach (MUD_MudZoneEntity z : m_aZones)
		{
			if (!z)
				continue;
			float x0, zz0, x1, zz1;
			z.GetRect(x0, zz0, x1, zz1);
			int ca = Math.Floor((x0 - sx0) / sc);
			int cb = Math.Floor((x1 - sx0) / sc);
			int ka = Math.Floor((zz0 - sz0) / sc);
			int kb = Math.Floor((zz1 - sz0) / sc);
			for (int cz = ka; cz <= kb; cz++)
			{
				for (int cx = ca; cx <= cb; cx++)
				{
					if (cxs.Count() > 0 && HasCell(cxs, czs, cx, cz))
						continue;   // overlapping zones
					// fully covered: every sheet tile the cell touches is a zone tile
					float wx0 = sx0 + cx * sc;
					float wz0 = sz0 + cz * sc;
					int ta = Math.Floor(wx0 / TILE_M);
					int tb = Math.Floor((wx0 + sc - 0.01) / TILE_M);
					int tc = Math.Floor(wz0 / TILE_M);
					int td = Math.Floor((wz0 + sc - 0.01) / TILE_M);
					bool all = true;
					for (int tz = tc; tz <= td && all; tz++)
					{
						for (int tx = ta; tx <= tb; tx++)
						{
							if (!TileInZone(tx, tz))
							{
								all = false;
								break;
							}
						}
					}
					if (!all)
						continue;
					if (!skin.IsHoleCell(cx, cz))
					{
						notHole++;
						continue;
					}
					cxs.Insert(cx);
					czs.Insert(cz);
					want.Insert(true);
					cx0 = Math.Min(cx0, cx);
					cx1 = Math.Max(cx1, cx);
					cz0 = Math.Min(cz0, cz);
					cz1 = Math.Max(cz1, cz);
				}
			}
		}
		int t0 = System.GetTickCount();
		int changed = skin.SetVoxCells(cxs, czs, want);
		int patches = 0;
		if (changed > 0)
			patches = MUD_WorldGroundEntity.VoxCellsChanged(cx0, cx1, cz0, cz1);
		m_bOneHidden = true;
		Print(string.Format("MUD one grid: %1 skin cells hidden under the zones (%2 changed, %3 patches, %4 cells not ours), %5 ms",
			cxs.Count(), changed, patches, notHole, System.GetTickCount(t0)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasCell(array<int> cxs, array<int> czs, int cx, int cz)
	{
		for (int n = 0; n < cxs.Count(); n++)
		{
			if (cxs[n] == cx && czs[n] == cz)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Stored tile tx, tz changed: build it in FlushOneBuild (once)
	protected void QueueBuild(int tx, int tz)
	{
		int key = TrailKey(tx, tz);
		if (m_bQueueFront)
		{
			// SHEET LOCAL hole: first in line (next FlushOneBuild)
			int at = m_aOneBuild.Find(key);
			if (at >= 0)
				m_aOneBuild.RemoveOrdered(at);
			m_aOneBuild.InsertAt(key, 0);
			m_mQWhy.Set(key, BW_HOLE);
			if (MUD_TestKit.TEST_KIT)
				MUD_CraterTrace.Tile(MUD_CraterTrace.C_TAPER, tx, tz, true);     // stage 2 B1 crater log
			return;
		}
		if (!m_aOneBuild.Contains(key))
			m_aOneBuild.Insert(key);
	}

	//------------------------------------------------------------------------------------------------
	//! TAPER across tiles: tile tx, tz changed vertices li0..li1 / lk0..lk1 (local, -1 = none); the neighbour tiles
	//! within TAPER_PAD cells of them read those vertices in their own taper, so they are rebuilt too
	protected void QueueTaperNeighbours(int tx, int tz, int li0, int li1, int lk0, int lk1)
	{
		if (li1 < 0 || TAPER <= 0)
			return;
		for (int dz = -1; dz <= 1; dz++)
		{
			if ((dz < 0 && lk0 > TAPER_PAD) || (dz > 0 && lk1 < TQ - TAPER_PAD))
				continue;
			for (int dx = -1; dx <= 1; dx++)
			{
				if (dx == 0 && dz == 0)
					continue;
				if ((dx < 0 && li0 > TAPER_PAD) || (dx > 0 && li1 < TQ - TAPER_PAD))
					continue;
				int ntx = tx + dx;
				int ntz = tz + dz;
				if (!TileInZone(ntx, ntz))
					continue;
				if (IsLive(ntx, ntz))
				{
					int ni0 = (ntx - m_iX0 / TQ) * TQ;
					int nk0 = (ntz - m_iZ0 / TQ) * TQ;
					MarkRect(ni0 + 1, ni0 + TQ, nk0 + 1, nk0 + TQ);
					m_bDirty = true;
					continue;
				}
				MUD_MudSheetStore st = m_mTrail.Get(TrailKey(ntx, ntz));
				if (st && st.m_Ent)
					QueueBuild(ntx, ntz);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID zone edge: does the box lo..hi (x, z) reach a tile outside every mud zone?
	static bool BoxLeavesZone(vector lo, vector hi)
	{
		if (!ONE_GRID || !ENABLED || !s_Inst || !s_Inst.m_bOneHidden)
			return false;
		for (int tz = Math.Floor(lo[2] / TILE_M); tz <= Math.Floor(hi[2] / TILE_M); tz++)
		{
			for (int tx = Math.Floor(lo[0] / TILE_M); tx <= Math.Floor(hi[0] / TILE_M); tx++)
			{
				if (!s_Inst.TileInZone(tx, tz))
					return true;
			}
		}
		return false;
	}

	//! Does the box lo..hi (x, z) reach a mud zone tile?
	protected bool BoxTouchesZone(vector lo, vector hi)
	{
		for (int tz = Math.Floor(lo[2] / TILE_M); tz <= Math.Floor(hi[2] / TILE_M); tz++)
		{
			for (int tx = Math.Floor(lo[0] / TILE_M); tx <= Math.Floor(hi[0] / TILE_M); tx++)
			{
				if (TileInZone(tx, tz))
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.MarkCarved: is the skin cell x0, z0 (size m) sheet ground (every tile it touches is a zone
	//! tile, as the hiding)? Then a crater / trench there does not make the voxels draw: the sheet shows it.
	static bool CoversCell(float x0, float z0, float size)
	{
		if (!ONE_GRID || !ENABLED || !s_Inst || !s_Inst.m_bOneHidden)
			return false;
		int ta = Math.Floor(x0 / TILE_M);
		int tb = Math.Floor((x0 + size - 0.01) / TILE_M);
		int tc = Math.Floor(z0 / TILE_M);
		int td = Math.Floor((z0 + size - 0.01) / TILE_M);
		for (int tz = tc; tz <= td; tz++)
		{
			for (int tx = ta; tx <= tb; tx++)
			{
				if (!s_Inst.TileInZone(tx, tz))
					return false;
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC: the held sheet-and-voxel ops change the sheet's ground now (tiles queued for FlushOneBuild)
	protected void ApplyPending(int budgetMs = 100000)
	{
		int t0 = System.GetTickCount();
		int tiles = 0;
		int n = 0;
		while (!m_aPend.IsEmpty())
		{
			if (n > 0 && System.GetTickCount(t0) >= budgetMs)
				break;
			MUD_VoxelOp op = m_aPend[0];
			m_aPend.RemoveOrdered(0);
			tiles += ApplyGroundOp(op);
			n++;
		}
		Print(string.Format("MUD one grid: %1 held ops -> sheet %2 tiles in %3 ms, %4 left", n, tiles, System.GetTickCount(t0), m_aPend.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! After a go-live (FlushNow): held ops PEND_STEP_MS per frame, then DigNow once all are in
	protected void StepNow()
	{
		if (!m_aPend.IsEmpty())
			ApplyPending(PEND_STEP_MS);
		if (!m_aPend.IsEmpty())
			return;
		m_bNowDue = false;
		DoDigNow();
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.Submit (after Prepare): a crater (0), trench / shovel dig (2), fill (3) or rut (4) op whose
	//! point lies on a zone tile changes the sheet's ground instead of voxels. True = taken (the probe drops it).
	//! Trench / fill point = middle of the op box; zone tiles only (a trench leaving the zone stops at its edge).
	//! both: the voxels take it too (tunnel / zone edge) - held until their go-live (SYNC), true returned.
	static bool TakeOp(MUD_VoxelOp op, bool both)
	{
		if (!ONE_GRID || !ENABLED || !s_Inst || !s_Inst.m_bOneHidden || !op)
			return false;
		vector p;
		if (op.m_iKind == 0)
			p = op.m_vWp;
		else if (op.m_iKind == 4)
		{
			if (op.m_aSegA.IsEmpty())
				return false;
			p = op.m_aSegA[0];
		}
		else if (op.m_iKind == 2 || op.m_iKind == 3)
		{
			if (!op.m_Lay)
				return false;
			p = (op.m_vLo + op.m_vHi) * 0.5;
		}
		else
			return false;
		int ptx = Math.Floor(p[0] / TILE_M);
		int ptz = Math.Floor(p[2] / TILE_M);
		if (!s_Inst.TileInZone(ptx, ptz))
		{
			// zone edge (user 2026-10-15: seam + see-through after a shell next to the zone): a crater outside that
			// reaches zone tiles digs the sheet there too; false = the voxels still take it (outside part)
			if (op.m_iKind == 0 && s_Inst.BoxTouchesZone(op.m_vLo, op.m_vHi))
			{
				s_Inst.Hold(op);
				Print(string.Format("MUD one grid: crater at %1 outside the zone reaches it -> sheet (at go-live) and voxels", p), LogLevel.NORMAL);
			}
			return false;
		}
		if (op.m_iKind == 0 && s_Inst.InVoxels(p))
		{
			// a shell inside a tunnel: the voxels blast it (3D); DigNow opens the sheet where they moved
			Print(string.Format("MUD one grid: crater at %1 is inside voxel ground (tunnel) -> voxels", p), LogLevel.NORMAL);
			return false;
		}
		if (both)
		{
			s_Inst.Hold(op);
			return true;
		}
		int t0 = System.GetTickCount();
		int tiles = s_Inst.ApplyGroundOp(op);
		// crater / rut: shown this frame (a trench comes as many ops: built once, next Frame)
		if (op.m_iKind == 0 || op.m_iKind == 4)
			s_Inst.FlushOneBuild();
		Print(string.Format("MUD one grid: op kind %1 at %2 -> %3 tiles in %4 ms", op.m_iKind, p, tiles, System.GetTickCount(t0)), LogLevel.NORMAL);
		return true;
	}

	//! SYNC: is op held for the next go-live?
	static bool Holds(MUD_VoxelOp op)
	{
		return s_Inst && op && s_Inst.m_aPend.Find(op) >= 0;
	}

	protected void Hold(MUD_VoxelOp op)
	{
		if (m_aPend.IsEmpty())
			m_iPendT = System.GetTickCount();
		m_aPend.Insert(op);
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID: new ground height at world x, z for a crater op (g = ground now, undug = before any op): as
	//! MUD_VoxelProbe.Carve (rim bump, half-ellipsoid bowl, funnel above the hit point), never deeper than ONE_FLOOR
	//! under the undug ground. Returns g when nothing changes.
	protected float CraterGround(MUD_VoxelOp op, float x, float z, float g, float undug)
	{
		vector wp = op.m_vWp;
		float depth = op.m_fDepth;
		float crest = Math.Max(0.75, depth * 2.2);
		float rimH = depth * 0.2;
		float rimW = Math.Max(0.5, depth);
		float inner = crest * 0.8;
		float outer = crest + rimW;
		float dx = x - wp[0];
		float dz = z - wp[2];
		float rr = Math.Sqrt(dx * dx + dz * dz);
		if (rr >= outer && rr - crest >= (g - wp[1]) * ONE_FUNNEL)
			return g;
		float bump = 0;
		if (rr > inner && rr < outer)
			bump = rimH * Math.Sin(Math.PI * (rr - inner) / (outer - inner));
		float air;
		if (rr < crest)
			air = wp[1] - depth * Math.Sqrt(1 - (rr / crest) * (rr / crest));
		else
			air = wp[1] + (rr - crest) / ONE_FUNNEL;
		float ng = Math.Min(g + bump, air);
		if (ng < g)
			ng = Math.Max(ng, undug - ONE_FLOOR);
		return ng;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID rut op: per world cell of the op box (gi0, gk0, w x h) the strongest CarveRut profile over the
	//! segments (best, -1 = none) and the wheel height there (atY). One pass per segment over its own box.
	protected void RutProfile(MUD_VoxelOp op, int gi0, int gk0, int w, int h, array<float> best, array<float> atY)
	{
		best.Resize(w * h);
		atY.Resize(w * h);
		for (int c = 0; c < w * h; c++)
			best[c] = 1000;     // SOFT: nearest segment distance (m), 1000 = none
		float r = ONE_RUT_R;
		for (int s = 0; s < op.m_aSegA.Count(); s++)
		{
			vector a = op.m_aSegA[s];
			vector b = op.m_aSegB[s];
			float abx = b[0] - a[0];
			float abz = b[2] - a[2];
			float len2 = abx * abx + abz * abz;
			if (len2 < 0.000001)
				continue;
			int i0 = Math.Max(0, Math.Floor((Math.Min(a[0], b[0]) - r) / CELL) - gi0);
			int i1 = Math.Min(w - 1, Math.Ceil((Math.Max(a[0], b[0]) + r) / CELL) - gi0);
			int k0 = Math.Max(0, Math.Floor((Math.Min(a[2], b[2]) - r) / CELL) - gk0);
			int k1 = Math.Min(h - 1, Math.Ceil((Math.Max(a[2], b[2]) + r) / CELL) - gk0);
			for (int k = k0; k <= k1; k++)
			{
				for (int i = i0; i <= i1; i++)
				{
					float px = (gi0 + i) * CELL;
					float pz = (gk0 + k) * CELL;
					float u = ((px - a[0]) * abx + (pz - a[2]) * abz) / len2;
					if (u < 0 || u >= 1)
						continue;
					float qx = a[0] + abx * u - px;
					float qz = a[2] + abz * u - pz;
					float dist = Math.Sqrt(qx * qx + qz * qz);
					if (dist >= r)
						continue;
					int c2 = k * w + i;
					if (dist >= best[c2])
						continue;
					best[c2] = dist;
					atY[c2] = a[1] + (b[1] - a[1]) * u;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID: new ground at world cell gi, gk (g now, undug before any op). Crater: CraterGround; rut: the
	//! profile cell (skipped where the wheel was > ONE_RUT_ON off the ground), step m_fDepth, cap m_fB under undug.
	protected float OpGround(MUD_VoxelOp op, int gi, int gk, float g, float undug, int gi0, int gk0, int w, int h, array<float> best, array<float> atY)
	{
		if (op.m_iKind == 0)
			return CraterGround(op, gi * CELL, gk * CELL, g, undug);
		if (op.m_iKind == 2 || op.m_iKind == 3)
			return TrenchGround(op, gi * CELL, gk * CELL, g, undug);
		int ci = gi - gi0;
		int ck = gk - gk0;
		if (ci < 0 || ck < 0 || ci >= w || ck >= h)
			return g;
		int c = ck * w + ci;
		float dist = best[c];
		if (dist >= ONE_RUT_R || Math.AbsFloat(g - atY[c]) > ONE_RUT_ON)
			return g;
		// SOFT: narrow on undug ground, full radius once the wheel runs m_fB deep; cosine = no step at the edge
		float dig = 1;
		if (op.m_fB > 0.001)
			dig = Math.Clamp((undug - atY[c]) / op.m_fB, 0, 1);
		float rEff = ONE_RUT_R * (ONE_W0 + (1 - ONE_W0) * dig);
		if (dist >= rEff)
			return g;
		float f = 0.5 * (1 + Math.Cos(Math.PI * dist / rEff));
		float floorY = g - op.m_fDepth * f;
		floorY = Math.Max(floorY, undug - op.m_fB * f);
		floorY = Math.Max(floorY, undug - ONE_FLOOR);
		if (floorY >= g)
			return g;
		return floorY;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID trench / shovel dig (2) and fill (3) at world x, z: as MUD_VoxelProbe.CutTrench / FillTrench, with
	//! the vertex's own undug height and depth (undug - g). Dig only lowers (cap ONE_FLOOR), fill only raises.
	protected float TrenchGround(MUD_VoxelOp op, float x, float z, float g, float undug)
	{
		MUD_TrenchLayout lay = op.m_Lay;
		float slope = MUD_TrenchPlan.SLOPE_RUN;
		if (op.m_iKind == 2)
		{
			float depth = op.m_fDepth * MUD_TrenchPlan.LAYER;
			float run = depth * slope;
			int owner = lay.PieceAt(x, z);
			float want = -1;
			for (int p = 0; p < lay.Count(); p++)
			{
				if (op.m_iPiece >= 0 && p != op.m_iPiece)
					continue;
				float w = depth;
				if (owner != p)
				{
					if (owner >= 0)
						continue;   // another piece's vertex: its own swings
					float od = lay.OutsideDist(p, x, z);
					if (od >= run)
						continue;
					w = depth - od / slope;
				}
				if (w > want)
					want = w;
			}
			if (want < 0)
				return g;
			float bottom = Math.Max(undug - want, undug - ONE_FLOOR);
			return Math.Min(g, bottom);
		}
		float holeDepth = op.m_fDepth;
		float newDepth = op.m_fB;
		float fillTo = newDepth;
		if (lay.PieceAt(x, z) < 0)
		{
			float margin = MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER * slope;
			float od2 = margin;
			for (int p2 = 0; p2 < lay.Count(); p2++)
				od2 = Math.Min(od2, lay.OutsideDist(p2, x, z));
			if (od2 >= margin)
				return g;
			float wall = Math.Max(0, holeDepth - od2 / slope);
			if (undug - g > wall + 0.01)
				fillTo = newDepth + od2 / slope;
			else
				fillTo = Math.Max(0, newDepth - od2 / slope);
		}
		return Math.Max(g, undug - fillTo);
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID (MUD_VoxelProbe.TrenchCols on the server / host): depth under the undug ground of the sheet vertex
	//! nearest x, z. False = not on a sheet zone tile (the caller reads voxels / heightfield).
	static bool SheetDepth(float x, float z, out float dep)
	{
		dep = 0;
		if (!ONE_GRID || !ENABLED || !s_Inst || !s_Inst.m_bOneHidden)
			return false;
		int tx = Math.Floor(x / TILE_M);
		int tz = Math.Floor(z / TILE_M);
		if (!s_Inst.TileInZone(tx, tz))
			return false;
		int gi = Math.Round(x / CELL);
		int gk = Math.Round(z / CELL);
		int li = Math.Clamp(gi - tx * TQ, 0, TQ);
		int lk = Math.Clamp(gk - tz * TQ, 0, TQ);
		int l = lk * TV + li;
		float g;
		if (s_Inst.IsLive(tx, tz))
		{
			int v = (tz * TQ + lk - s_Inst.m_iZ0) * N + tx * TQ + li - s_Inst.m_iX0;
			if (s_Inst.m_F.m_aGs[v] == 0)
				return false;
			g = s_Inst.m_F.m_aG[v];
		}
		else
		{
			MUD_MudSheetStore st = s_Inst.m_mTrail.Get(TrailKey(tx, tz));
			if (!st || !st.m_D || st.m_D.m_aGs[l] == 0)
				return false;
			g = st.m_D.m_aG[l];
		}
		array<float> u = s_Inst.m_mUndug.Get(TrailKey(tx, tz));
		if (u && u[l] > -50000)
			dep = u[l] - g;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID: hit point p is in voxel ground: the sheet vertex nearest it is a voxel hole (BARE_VOX), or p lies
	//! more than IN_VOX under the sheet's ground there (a tunnel under an intact sheet roof)
	protected bool InVoxels(vector p)
	{
		int tx = Math.Floor(p[0] / TILE_M);
		int tz = Math.Floor(p[2] / TILE_M);
		int gi = Math.Round(p[0] / CELL);
		int gk = Math.Round(p[2] / CELL);
		int li = Math.Clamp(gi - tx * TQ, 0, TQ);
		int lk = Math.Clamp(gk - tz * TQ, 0, TQ);
		int l = lk * TV + li;
		float g;
		int bare;
		if (IsLive(tx, tz))
		{
			int v = (tz * TQ + lk - m_iZ0) * N + tx * TQ + li - m_iX0;
			if (m_F.m_aGs[v] == 0)
				return false;
			g = m_F.m_aG[v];
			bare = m_F.m_aBare[v];
		}
		else
		{
			MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
			if (!st || !st.m_D || st.m_D.m_aGs[l] == 0)
				return false;
			g = st.m_D.m_aG[l];
			bare = st.m_D.m_aBare[l];
		}
		return bare == BARE_VOX || p[1] < g - IN_VOX;
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID: undug height of vertex l of tile tx, tz; the first call records g (never changed = undug)
	protected float UndugAt(int tx, int tz, int l, float g)
	{
		int key = TrailKey(tx, tz);
		array<float> u = m_mUndug.Get(key);
		if (!u)
		{
			u = new array<float>();
			u.Resize(TV * TV);
			for (int n = 0; n < TV * TV; n++)
				u[n] = -100000;
			m_mUndug.Set(key, u);
		}
		if (u[l] < -50000)
			u[l] = g;
		return u[l];
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID: op into every live / stored zone tile it reaches. Craters take the mud off (BlastKeep) and bare
	//! what moved; ruts keep the mud on top. Stored tiles are built now, the live window on its next upload.
	protected int ApplyGroundOp(MUD_VoxelOp op)
	{
		float blastR = 0;
		vector wp = op.m_vWp;
		vector lo = op.m_vLo;
		vector hi = op.m_vHi;
		if (op.m_iKind == 0)
		{
			blastR = Math.Max(0.75, op.m_fDepth * 2.2);
			// the funnel can reach past Prepare's box up a slope: at least the mud reach
			float reach = BlastReach(blastR) + CELL;
			lo = Vector(Math.Min(lo[0], wp[0] - reach), 0, Math.Min(lo[2], wp[2] - reach));
			hi = Vector(Math.Max(hi[0], wp[0] + reach), 0, Math.Max(hi[2], wp[2] + reach));
		}
		int gi0 = Math.Floor(lo[0] / CELL);
		int gk0 = Math.Floor(lo[2] / CELL);
		int gw = Math.Ceil(hi[0] / CELL) - gi0 + 1;
		int gh = Math.Ceil(hi[2] / CELL) - gk0 + 1;
		array<float> best = {};
		array<float> atY = {};
		if (op.m_iKind == 4)
			RutProfile(op, gi0, gk0, gw, gh, best, atY);
		bool trench = op.m_iKind == 2 || op.m_iKind == 3;
		int tx0 = Math.Floor(lo[0] / TILE_M);
		int tx1 = Math.Floor(hi[0] / TILE_M);
		int tz0 = Math.Floor(lo[2] / TILE_M);
		int tz1 = Math.Floor(hi[2] / TILE_M);
		int done = 0;
		for (int tz = tz0; tz <= tz1; tz++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				if (!TileInZone(tx, tz))
					continue;
				if (IsLive(tx, tz))
				{
					int i0 = (tx - m_iX0 / TQ) * TQ;
					int k0 = (tz - m_iZ0 / TQ) * TQ;
					bool liveHit = false;
					for (int k = k0; k <= k0 + TQ; k++)
					{
						for (int i = i0; i <= i0 + TQ; i++)
						{
							int v = k * N + i;
							if (m_F.m_aGs[v] == 0)
								continue;
							int wi = m_iX0 + i;
							int wk = m_iZ0 + k;
							if (wi < gi0 || wk < gk0 || wi >= gi0 + gw || wk >= gk0 + gh)
								continue;
							float g = m_F.m_aG[v];
							float und = UndugAt(tx, tz, (k - k0) * TV + i - i0, g);
							float ng = OpGround(op, wi, wk, g, und, gi0, gk0, gw, gh, best, atY);
							float keep = BlastKeep(wi * CELL, wk * CELL, wp, blastR);
							if (ng == g && keep >= 1)
								continue;
							m_F.m_aG[v] = ng;
							if (keep < 1)
								m_F.m_aD[v] = m_F.m_aD[v] * keep;
							bool dug = trench && Math.AbsFloat(ng - g) > MOVED;
							if (dug)
								m_F.m_aD[v] = 0;
							if ((keep < 1 || dug || (blastR > 0 && g - ng > MOVED)) && m_F.m_aBare[v] != BARE_VOX)
								m_F.m_aBare[v] = 1;
							liveHit = true;
						}
					}
					if (liveHit)
					{
						MarkRect(i0 - TAPER_PAD, i0 + TQ + TAPER_PAD, k0 - TAPER_PAD, k0 + TQ + TAPER_PAD);
						m_bDirty = true;
						done++;
					}
					continue;
				}
				MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
				if (!st || !st.m_Ent)
					continue;
				MUD_MudSheetTileData d = st.m_D;
				bool hit = false;
				int hi0 = TV;
				int hi1 = -1;
				int hk0 = TV;
				int hk1 = -1;
				for (int lk = 0; lk < TV; lk++)
				{
					for (int li = 0; li < TV; li++)
					{
						int l = lk * TV + li;
						if (d.m_aGs[l] == 0)
							continue;
						int si = tx * TQ + li;
						int sk = tz * TQ + lk;
						if (si < gi0 || sk < gk0 || si >= gi0 + gw || sk >= gk0 + gh)
							continue;
						float g2 = d.m_aG[l];
						float und2 = UndugAt(tx, tz, l, g2);
						float ng2 = OpGround(op, si, sk, g2, und2, gi0, gk0, gw, gh, best, atY);
						float keep2 = BlastKeep(si * CELL, sk * CELL, wp, blastR);
						if (ng2 == g2 && keep2 >= 1)
							continue;
						d.m_aG[l] = ng2;
						if (keep2 < 1)
							d.m_aD[l] = d.m_aD[l] * keep2;
						bool dug2 = trench && Math.AbsFloat(ng2 - g2) > MOVED;
						if (dug2)
							d.m_aD[l] = 0;
						if ((keep2 < 1 || dug2 || (blastR > 0 && g2 - ng2 > MOVED)) && d.m_aBare[l] != BARE_VOX)
							d.m_aBare[l] = 1;
						hit = true;
						hi0 = Math.Min(hi0, li);
						hi1 = Math.Max(hi1, li);
						hk0 = Math.Min(hk0, lk);
						hk1 = Math.Max(hk1, lk);
					}
				}
				if (!hit)
					continue;
				// built in FlushOneBuild (once per tile), with the taper neighbours
				QueueBuild(tx, tz);
				QueueTaperNeighbours(tx, tz, hi0, hi1, hk0, hk1);
				done++;
			}
		}
		return done;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_VoxelProbe.GoLive, after all GroundChanged calls of that go-live
	static void FlushNow()
	{
		if (!ENABLED || !s_Inst)
			return;
		int tCt = System.GetTickCount();
		s_Inst.m_bNowDue = true;
		s_Inst.StepNow();
		if (MUD_TestKit.TEST_KIT)
			MUD_CraterTrace.SheetMs(System.GetTickCount(tCt));     // stage 2 B1: the sheet part of the go-live frame
	}

	protected void DoDigNow()
	{
		// the tiles are built next Frame (FlushOneBuild): the go-live frame was ~100 ms for a mortar on a tunnel
		// (voxels ~50 + sheet tiles ~50, user 'a little stutter' 16:55) - one frame apart is not visible
		if (m_aNowLo.IsEmpty())
			return;
		int t0 = System.GetTickCount();
		int boxes = m_aNowLo.Count();
		m_iDgN = 0;
		m_iDgHole = 0;
		m_iDgUp = 0;
		m_fDgMin = 1000;
		m_fDgMax = -1000;
		m_bMarkNow = m_bLocal;
		int tiles = DigNow();
		m_bMarkNow = false;
		if (m_bLocal)
			tiles += BuildNowSlots();     // SHEET LOCAL: holed live slots in the blast frame
		if (m_bLocal && CRATER_TAPER_NOW)
			tiles += TaperNow();     // stage 2 B3: the hole's taper tiles too
		m_aNowLo.Clear();
		m_aNowHi.Clear();
		Print(string.Format("MUD mud sheet: dig now: %1 voxel changes, %2 tiles in %3 ms", boxes, tiles, System.GetTickCount(t0)), LogLevel.NORMAL);
		if (ONE_GRID)
			Print(string.Format("MUD mud sheet: DIAG dig now: %1 sheet verts over voxels, %2 opened, %3 with voxel top > sheet ground + 0.1, voxel - sheet %4 .. %5 m", m_iDgN, m_iDgHole, m_iDgUp, m_fDgMin, m_fDgMax), LogLevel.NORMAL);
	}

	//! DIAG: one sheet vertex DigNow looked at (diff = voxel top - sheet ground)
	protected void DiagVert(float diff)
	{
		m_iDgN++;
		if (diff > 0.1)
			m_iDgUp++;
		m_fDgMin = Math.Min(m_fDgMin, diff);
		m_fDgMax = Math.Max(m_fDgMax, diff);
	}

	//! DIG NOW: x, z inside any collected change box (+1 cell)
	protected bool InNowBox(float x, float z)
	{
		for (int b = 0; b < m_aNowLo.Count(); b++)
		{
			vector bl = m_aNowLo[b];
			vector bh = m_aNowHi[b];
			if (x >= bl[0] - CELL && x <= bh[0] + CELL && z >= bl[2] - CELL && z <= bh[2] + CELL)
				return true;
		}
		return false;
	}

	protected int DigNow()
	{
		float x0 = 1000000;
		float x1 = -1000000;
		float z0 = 1000000;
		float z1 = -1000000;
		for (int nb = 0; nb < m_aNowLo.Count(); nb++)
		{
			vector nlo = m_aNowLo[nb];
			vector nhi = m_aNowHi[nb];
			x0 = Math.Min(x0, nlo[0] - CELL);
			x1 = Math.Max(x1, nhi[0] + CELL);
			z0 = Math.Min(z0, nlo[2] - CELL);
			z1 = Math.Max(z1, nhi[2] + CELL);
		}
		int tx0 = Math.Floor(x0 / TILE_M);
		int tx1 = Math.Floor(x1 / TILE_M);
		int tz0 = Math.Floor(z0 / TILE_M);
		int tz1 = Math.Floor(z1 / TILE_M);
		int done = 0;
		for (int tz = tz0; tz <= tz1; tz++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				bool hit = false;
				if (IsLive(tx, tz))
				{
					int i0 = (tx - m_iX0 / TQ) * TQ;
					int k0 = (tz - m_iZ0 / TQ) * TQ;
					for (int k = k0; k <= k0 + TQ; k++)
					{
						float wz = (m_iZ0 + k) * CELL;
						if (wz < z0 || wz > z1)
							continue;
						for (int i = i0; i <= i0 + TQ; i++)
						{
							float wx = (m_iX0 + i) * CELL;
							if (wx < x0 || wx > x1 || !InNowBox(wx, wz))
								continue;
							int v = k * N + i;
							float ny;
							if (m_F.m_aGs[v] == 0 || !MUD_VoxelProbe.SurfaceAt(wx, wz, ny))
								continue;
							// ONE_GRID: the voxels draw only in carved cells (zone craters do not carve): elsewhere the
							// sheet keeps its own ground (an opened hole there would show nothing)
							if (ONE_GRID && m_bOneHidden && !MUD_VoxelProbe.DrawsAt(wx, wz))
								continue;
							float dy = Math.AbsFloat(ny - m_F.m_aG[v]);
							DiagVert(ny - m_F.m_aG[v]);
							if (dy <= FIELD_EPS)
								continue;
							if (ONE_GRID && m_bOneHidden && dy > MOVED)
							{
								m_F.m_aD[v] = 0;
								m_F.m_aBare[v] = BARE_VOX;
								m_iDgHole++;
								hit = true;
								continue;
							}
							if (MUD_TestKit.TEST_KIT && m_mEase.Contains(PassKey(m_iX0 + i, m_iZ0 + k)))
								MUD_CraterTrace.EaseHit();     // stage 2 B1 crater log
							if (CRATER_EASE_DROP)
								EaseDrop(m_iX0 + i, m_iZ0 + k);     // stage 2 B4 (O1c): EaseTick must not write the old floor back
							m_F.m_aG[v] = ny;
							if (dy > MOVED)
							{
								m_F.m_aD[v] = 0;
								m_F.m_aBare[v] = 1;
							}
							hit = true;
						}
					}
					if (hit)
					{
						MarkRect(i0 - TAPER_PAD, i0 + TQ + TAPER_PAD, k0 - TAPER_PAD, k0 + TQ + TAPER_PAD);
						m_bDirty = true;
						done++;
					}
					continue;
				}
				MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
				if (!st || (!st.m_Ent && !m_bLocal))
					continue;     // SHEET LOCAL: a data-only store takes the new ground too
				MUD_MudSheetTileData d = st.m_D;
				int hi0 = TV;
				int hi1 = -1;
				int hk0 = TV;
				int hk1 = -1;
				for (int lk = 0; lk < TV; lk++)
				{
					float sz = (tz * TQ + lk) * CELL;
					if (sz < z0 || sz > z1)
						continue;
					for (int li = 0; li < TV; li++)
					{
						float sx = (tx * TQ + li) * CELL;
						if (sx < x0 || sx > x1 || !InNowBox(sx, sz))
							continue;
						int l = lk * TV + li;
						float ny2;
						if (d.m_aGs[l] == 0 || !MUD_VoxelProbe.SurfaceAt(sx, sz, ny2))
							continue;
						if (ONE_GRID && m_bOneHidden && !MUD_VoxelProbe.DrawsAt(sx, sz))
							continue;
						float dy2 = Math.AbsFloat(ny2 - d.m_aG[l]);
						DiagVert(ny2 - d.m_aG[l]);
						if (dy2 <= FIELD_EPS)
							continue;
						hi0 = Math.Min(hi0, li);
						hi1 = Math.Max(hi1, li);
						hk0 = Math.Min(hk0, lk);
						hk1 = Math.Max(hk1, lk);
						if (ONE_GRID && m_bOneHidden && dy2 > MOVED)
						{
							d.m_aD[l] = 0;
							d.m_aBare[l] = BARE_VOX;
							m_iDgHole++;
							hit = true;
							continue;
						}
						if (CRATER_EASE_DROP)
							EaseDrop(tx * TQ + li, tz * TQ + lk);     // stage 2 B4: no ease may carry the old floor into it
						d.m_aG[l] = ny2;
						if (dy2 > MOVED)
						{
							d.m_aD[l] = 0;
							d.m_aBare[l] = 1;
						}
						hit = true;
					}
				}
				int nk = m_aNowTiles.Find(TrailKey(tx, tz));
				if (nk >= 0)
				{
					m_aNowTiles.Remove(nk);
					hit = true;     // BlastNow changed it: build once, here
				}
				if (!hit)
					continue;
				// ONE_GRID: built in FlushOneBuild (FlushNow, same frame) with its taper neighbours
				if (ONE_GRID && m_bOneHidden)
				{
					QueueBuild(tx, tz);
					QueueTaperNeighbours(tx, tz, hi0, hi1, hk0, hk1);
					done++;
					continue;
				}
				if (!st.m_Ent)
					continue;     // SHEET LOCAL: data only - its mesh comes on approach (CAMERA RADIUS)
				m_iBuildWhy = BW_HOLE;
				Resource res = BuildTile(st.m_Ent, d, tx, tz, false);
				if (res)
					st.m_Res = res;
				if (MUD_TestKit.TEST_KIT && d.m_iLow != 0)
					MUD_CraterTrace.Tile(MUD_CraterTrace.C_LOW, tx, tz, false);     // stage 2 B1 crater log
				done++;
				if (m_bLocal)
				{
					// SHEET LOCAL: its taper neighbours at the FRONT of the stored build queue (live ones build now)
					m_bQueueFront = true;
					QueueTaperNeighbours(tx, tz, hi0, hi1, hk0, hk1);
					m_bQueueFront = false;
				}
			}
		}
		// BlastNow tiles outside the dig boxes (should not happen): build them too
		foreach (int lk2 : m_aNowTiles)
		{
			MUD_MudSheetStore st2 = m_mTrail.Get(lk2);
			if (!st2 || !st2.m_Ent)
				continue;
			int ltx = lk2 / 20000;     // TrailKey back to the tile (tx, tz >= 0 on Arland)
			int ltz = lk2 - ltx * 20000;
			m_iBuildWhy = BW_HOLE;
			Resource res2 = BuildTile(st2.m_Ent, st2.m_D, ltx, ltz, false);
			if (res2)
				st2.m_Res = res2;
			done++;
		}
		m_aNowTiles.Clear();
		return done;
	}

	//------------------------------------------------------------------------------------------------
	//! Outer reach of a crater's mud effect (BlastKeep's outer edge)
	static float BlastReach(float r)
	{
		return r + Math.Max(0.5, r / 2.2);
	}

	//------------------------------------------------------------------------------------------------
	//! BLAST NOW: takes the mud off every live / stored tile the crater reaches, in this frame. Returns tiles done.
	protected int BlastNow(vector lo, vector hi, vector wp, float r)
	{
		float reach = BlastReach(r);
		int tx0 = Math.Floor((wp[0] - reach) / TILE_M);
		int tx1 = Math.Floor((wp[0] + reach) / TILE_M);
		int tz0 = Math.Floor((wp[2] - reach) / TILE_M);
		int tz1 = Math.Floor((wp[2] + reach) / TILE_M);
		int done = 0;
		for (int tz = tz0; tz <= tz1; tz++)
		{
			for (int tx = tx0; tx <= tx1; tx++)
			{
				if (IsLive(tx, tz))
				{
					int i0 = (tx - m_iX0 / TQ) * TQ;
					int k0 = (tz - m_iZ0 / TQ) * TQ;
					for (int k = k0; k <= k0 + TQ; k++)
					{
						for (int i = i0; i <= i0 + TQ; i++)
						{
							float keep = BlastKeep((m_iX0 + i) * CELL, (m_iZ0 + k) * CELL, wp, r);
							if (keep >= 1)
								continue;
							int v = k * N + i;
							m_F.m_aD[v] = m_F.m_aD[v] * keep;
							m_F.m_aBare[v] = 1;
						}
					}
					MarkRect(i0, i0 + TQ, k0, k0 + TQ);
					m_bDirty = true;
					done++;
					continue;
				}
				if (MUD_TestKit.TEST_KIT && !m_mTrail.Contains(TrailKey(tx, tz)) && m_mFar.Contains(TrailKey(tx, tz)))
					MUD_CraterTrace.Tile(MUD_CraterTrace.C_FAR, tx, tz, false);     // stage 2 B1 crater log
				MUD_MudSheetStore st = TrailGet(TrailKey(tx, tz));     // MUD LAB part D: a compact one comes back first
				if (MUD_TestKit.TEST_KIT && st && st.m_Ent)
					MUD_CraterTrace.Tile(MUD_CraterTrace.C_STORED, tx, tz, true);
				if (!st || !st.m_Ent)
				{
					// CAMERA RADIUS: no mesh here - SYNC: the sync layer cleared it (Stage 4, MUD_MudSync.ClearOp), the
					// tile is built bare on approach; without SYNC the blown cells go into a kept store
					if (!SYNC && RadiusOn() && TileInZone(tx, tz))
						BlastKept(st, tx, tz, wp, r);
					if (MUD_TestKit.TEST_KIT && (st || TileInZone(tx, tz)))
						MUD_CraterTrace.Tile(MUD_CraterTrace.C_DATA, tx, tz, false);     // stage 2 B1 crater log
					continue;
				}
				MUD_MudSheetTileData d = st.m_D;
				bool hit = false;
				for (int lk = 0; lk < TV; lk++)
				{
					for (int li = 0; li < TV; li++)
					{
						float keep2 = BlastKeep((tx * TQ + li) * CELL, (tz * TQ + lk) * CELL, wp, r);
						if (keep2 >= 1)
							continue;
						int l = lk * TV + li;
						d.m_aD[l] = d.m_aD[l] * keep2;
						d.m_aBare[l] = 1;
						hit = true;
					}
				}
				if (!hit)
					continue;
				done++;
				if (DIG_NOW)
				{
					if (!m_aNowTiles.Contains(TrailKey(tx, tz)))
						m_aNowTiles.Insert(TrailKey(tx, tz));   // FlushNow builds it once
					continue;
				}
				Resource res = BuildTile(st.m_Ent, d, tx, tz, false);
				if (res)
					st.m_Res = res;
			}
		}
		return done;
	}

	//------------------------------------------------------------------------------------------------
	//! CAMERA RADIUS, SYNC off only (Stage 4: the sync layer clears craters and digs from the ops): a crater on zone
	//! tile tx, tz with no mesh (st = its kept store, or null): the blown cells go into a kept store (data only).
	protected void BlastKept(MUD_MudSheetStore kept, int tx, int tz, vector wp, float r)
	{
		ref MUD_MudSheetStore st = kept;     // a strong local: a new store made here must live until it is in m_mTrail
		bool fresh = st == null;
		if (fresh)
		{
			st = new MUD_MudSheetStore();
			st.m_iTx = tx;
			st.m_iTz = tz;
		}
		MUD_MudSheetTileData d = st.m_D;
		bool hit = false;
		float sd;
		float sou;
		float sov;
		if (fresh && m_bLocal)
		{
			for (int fl = 0; fl < TV * TV; fl++)
				d.m_aD[fl] = THICK;     // SHEET LOCAL: untouched mud (no sync layer to ask)
		}
		for (int lk = 0; lk < TV; lk++)
		{
			for (int li = 0; li < TV; li++)
			{
				float keep = BlastKeep((tx * TQ + li) * CELL, (tz * TQ + lk) * CELL, wp, r);
				if (keep >= 1)
					continue;
				int l = lk * TV + li;
				float was = d.m_aD[l];
				if (d.m_aBare[l] == 0 && !m_bLocal)
				{
					int sb;
					if (!MUD_MudSync.Get(tx * TQ + li, tz * TQ + lk, sd, sou, sov, sb))
					{
						sd = THICK;     // no sync tile there: untouched mud
						sou = 0;
						sov = 0;
					}
					was = sd;
					d.m_aOu[l] = sou;
					d.m_aOv[l] = sov;
				}
				d.m_aD[l] = was * keep;
				d.m_aBare[l] = 1;
				hit = true;
			}
		}
		if (!hit || !fresh)
			return;
		int key = TrailKey(tx, tz);
		m_mTrail.Set(key, st);
		m_aTrailOrder.Insert(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Share of the mud a crater with crest radius r at wp leaves at x, z (1 = untouched): none inside BLAST_CLEAR x r
	//! (hole + walls), then growing back over the rim to full at its outer edge (r + Carve's rimW = max(0.5, depth),
	//! depth = r / 2.2). User 2026-10-15: the old 0.8 r .. r fade left mud over much of the hole and the rims.
	static float BlastKeep(float x, float z, vector wp, float r)
	{
		if (r <= 0)
			return 1;
		float dx = x - wp[0];
		float dz = z - wp[2];
		float dist = Math.Sqrt(dx * dx + dz * dz);
		float clear = r * BLAST_CLEAR;
		float outer = BlastReach(r);
		if (RIM_BARE)
			clear = outer;
		if (dist >= outer)
			return 1;
		if (dist <= clear)
			return 0;
		float t = (dist - clear) / (outer - clear);
		return t * t;      // thin over the rim crest, most of it back only at the rim's outer foot
	}

	//------------------------------------------------------------------------------------------------
	//! One mud tile after voxel change c: the traced cells inside the change box re-read the ground, the cells
	//! between are refilled, the blast takes the mud off, the mesh is rebuilt. Live window tile or stored tile.
	protected void RedoTile(int tx, int tz, int c)
	{
		vector lo = m_aChgLo[c];
		vector hi = m_aChgHi[c];
		vector wp = m_aChgWp[c];
		float r = m_aChgR[c];
		bool blownNow = r < 0;     // BLAST_NOW already took the mud off: only re-read the ground
		r = Math.AbsFloat(r);
		float reach2 = 0;
		if (r > 0)
			reach2 = (BlastReach(r) + 2 * CELL) * (BlastReach(r) + 2 * CELL);
		if (IsLive(tx, tz))
		{
			m_iJobLive++;
			// m_iX0 / m_iZ0 are whole tiles (multiples of TQ = 20), so window parity = world parity
			int i0 = (tx - m_iX0 / TQ) * TQ;
			int k0 = (tz - m_iZ0 / TQ) * TQ;
			float oldG[441];
			int oldGs[441];
			for (int ok = 0; ok < TV; ok++)
			{
				for (int oi = 0; oi < TV; oi++)
				{
					oldG[ok * TV + oi] = m_F.m_aG[(k0 + ok) * N + i0 + oi];
					oldGs[ok * TV + oi] = m_F.m_aGs[(k0 + ok) * N + i0 + oi];
				}
			}
			for (int k = k0; k <= k0 + TQ; k += 2)
			{
				for (int i = i0; i <= i0 + TQ; i += 2)
				{
					float x = (m_iX0 + i) * CELL;
					float z = (m_iZ0 + k) * CELL;
					if (x < lo[0] || x > hi[0] || z < lo[2] || z > hi[2])
						continue;
					Trace(i, k);
					FillAround(i, k);
					m_iJobTraces++;
				}
			}
			// round the crater every cell gets its own trace (seam: the filled-in cells cut the rim's curve)
			if (reach2 > 0)
			{
				for (int fk = k0; fk <= k0 + TQ; fk++)
				{
					for (int fi = i0; fi <= i0 + TQ; fi++)
					{
						if (fi % 2 == 0 && fk % 2 == 0)
							continue;
						float fdx = (m_iX0 + fi) * CELL - wp[0];
						float fdz = (m_iZ0 + fk) * CELL - wp[2];
						if (fdx * fdx + fdz * fdz > reach2)
							continue;
						Trace(fi, fk);
						m_iJobTraces++;
					}
				}
			}
			// TRENCH FIX (user 2026-10-15: trenches were under the mud): where the ground moved, the mud goes
			for (int mk = 0; mk < TV; mk++)
			{
				for (int mi = 0; mi < TV; mi++)
				{
					int mv = (k0 + mk) * N + i0 + mi;
					if (oldGs[mk * TV + mi] == 0 || m_F.m_aGs[mv] == 0)
						continue;
					if (Math.AbsFloat(m_F.m_aG[mv] - oldG[mk * TV + mi]) <= MOVED)
						continue;
					m_F.m_aD[mv] = 0;
					m_F.m_aBare[mv] = 1;
					m_iJobBare++;
				}
			}
			if (r > 0 && !blownNow)
			{
				for (int k2 = k0; k2 <= k0 + TQ; k2++)
				{
					for (int i2 = i0; i2 <= i0 + TQ; i2++)
					{
						float keep = BlastKeep((m_iX0 + i2) * CELL, (m_iZ0 + k2) * CELL, wp, r);
						if (keep >= 1)
							continue;
						int v = k2 * N + i2;
						m_F.m_aD[v] = m_F.m_aD[v] * keep;
						m_F.m_aBare[v] = 1;
						m_iJobBare++;
					}
				}
			}
			MarkRect(i0, i0 + TQ, k0, k0 + TQ);
			m_bDirty = true;
			return;
		}
		MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
		if (!st || !st.m_Ent)
			return;
		m_iJobStored++;
		MUD_MudSheetTileData d = st.m_D;
		float oldS[441];
		for (int sl = 0; sl < TV * TV; sl++)
			oldS[sl] = d.m_aG[sl];
		for (int lk = 0; lk < TV; lk += 2)
		{
			for (int li = 0; li < TV; li += 2)
			{
				float sx = (tx * TQ + li) * CELL;
				float sz = (tz * TQ + lk) * CELL;
				if (sx < lo[0] || sx > hi[0] || sz < lo[2] || sz > hi[2])
					continue;
				d.m_aG[lk * TV + li] = GroundDown(sx, sz);
				m_iJobTraces++;
			}
		}
		FillTile(d);
		if (reach2 > 0)
		{
			for (int fk2 = 0; fk2 < TV; fk2++)
			{
				for (int fi2 = 0; fi2 < TV; fi2++)
				{
					if (fi2 % 2 == 0 && fk2 % 2 == 0)
						continue;
					float gx = (tx * TQ + fi2) * CELL;
					float gz = (tz * TQ + fk2) * CELL;
					if ((gx - wp[0]) * (gx - wp[0]) + (gz - wp[2]) * (gz - wp[2]) > reach2)
						continue;
					d.m_aG[fk2 * TV + fi2] = GroundDown(gx, gz);
					m_iJobTraces++;
				}
			}
		}
		for (int ml = 0; ml < TV * TV; ml++)
		{
			if (Math.AbsFloat(d.m_aG[ml] - oldS[ml]) <= MOVED)
				continue;
			d.m_aD[ml] = 0;
			d.m_aBare[ml] = 1;
			m_iJobBare++;
		}
		if (r > 0 && !blownNow)
		{
			for (int lk2 = 0; lk2 < TV; lk2++)
			{
				for (int li2 = 0; li2 < TV; li2++)
				{
					float keep2 = BlastKeep((tx * TQ + li2) * CELL, (tz * TQ + lk2) * CELL, wp, r);
					if (keep2 >= 1)
						continue;
					int l = lk2 * TV + li2;
					d.m_aD[l] = d.m_aD[l] * keep2;
					d.m_aBare[l] = 1;
					m_iJobBare++;
				}
			}
		}
		Resource res = BuildTile(st.m_Ent, d, tx, tz, false);
		if (res)
			st.m_Res = res;
	}

	//------------------------------------------------------------------------------------------------
	//! Our ground top on a hole cell: ENTS-only trace, our ground entities only (skin, fine patch, voxel brick; not
	//! the mud tiles). MUD_Surface.Y alone fails over dug voxels: its WORLD trace stops on the holed heightmap above
	//! the dug floor (DIAG 2026-10-15 01:00: heightmap 40.09, voxel floor 38.62 -> dig read as 0.02 m move)
	protected float OurY(float x, float z)
	{
		BaseWorld world = GetGame().GetWorld();
		float top = MUD_Surface.Get().Y(world, x, z) + 10;
		if (!m_OurTrace)
			m_OurTrace = new TraceParam();
		m_OurTrace.Start = Vector(x, top, z);
		m_OurTrace.End = Vector(x, top - 60, z);
		m_OurTrace.Flags = TraceFlags.ENTS;
		m_OurTrace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_OurTrace.TraceEnt = null;
		float f = world.TraceMove(m_OurTrace, OnlyOurs);
		if (f < 1)
			return top - 60 * f;
		return top - 10;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnlyOurs(notnull IEntity e)
	{
		return MUD_Surface.IsOurs(e);
	}

	//------------------------------------------------------------------------------------------------
	//! Synced sheet Stage 1 (2026-10-18): ground under the mud on a hole cell. No voxel column there = our heightfield
	//! + rut floor + ridges from the data (MUD_WorldGroundEntity.DugUndug, what the patches draw): right at once after a
	//! rut op (the patch collider may still wait for FLOOR_SETTLE) and no trace. Voxel ground (crater, tunnel): OurY.
	protected float HoleGround(float x, float z)
	{
		float hgDug;
		float hgUndug;
		if (!MUD_VoxelProbe.ColumnAt(x, z) && MUD_WorldGroundEntity.DugUndug(x, z, hgDug, hgUndug))
		{
			// H2: this machine's look rut guess (MUD_LookRuts, same cut rule as the server's op) - drawn at once,
			// the server's cut lands at the same depth later
			float look = MUD_LookRuts.SheetFloorAt(x, z, hgDug);
			if (look < hgDug)
				return look;
			return hgDug;
		}
		return OurY(x, z);
	}

	//------------------------------------------------------------------------------------------------
	//! For others (trench ghosts): our ground top on a hole cell (ENTS-only, as OurY: the holed heightmap above a
	//! dug voxel floor is skipped); false on kept terrain
	static bool OurGroundY(float x, float z, out float y)
	{
		if (!s_Inst || !MUD_Surface.IsHole(GetGame().GetWorld(), x, z))
			return false;
		y = s_Inst.OurY(x, z);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_GroundEntity.HideFromTraces: the native get in / out check sweeps a box at the feet against every
	//! traceable entity (mud-gotchas 2026-10-08 13:2x); the mud collider is hidden while someone gets in / out
	static void SetTraceHidden(bool hide)
	{
		s_bTraceHidden = hide;
		if (!s_Inst)
			return;
		if (ONE_GRID)
			return;     // the sheet collider is the ground: wheels and feet need it
		foreach (IEntity e : s_Inst.m_aCollEnt)
		{
			if (!e)
				continue;
			if (hide)
				e.ClearFlags(EntityFlags.TRACEABLE, false);
			else
				e.SetFlags(EntityFlags.TRACEABLE, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! For others (trench ghosts drawn over the mud): mud thickness over the ground at x, z (0: no mud drawn there)
	static float MudOver(float x, float z)
	{
		if (!s_Inst)
			return 0;
		return s_Inst.MudAt(x, z);
	}

	//------------------------------------------------------------------------------------------------
	protected float MudAt(float x, float z)
	{
		int i = Math.Round(x / CELL);
		int k = Math.Round(z / CELL);
		int tx = Math.Floor(i / TQ);
		int tz = Math.Floor(k / TQ);
		if (m_bLocal)
		{
			// SHEET LOCAL: mud only on born tiles, times the deposit band (unborn ground: none, never THICK)
			if (!TileInZone(tx, tz))
				return 0;
			if (IsLive(tx, tz))
			{
				int lv = (k - m_iZ0) * N + (i - m_iX0);
				return m_F.m_aD[lv] * m_F.m_aBand[lv];
			}
			MUD_MudSheetStore lst = m_mTrail.Get(TrailKey(tx, tz));
			int ll = (k - tz * TQ) * TV + (i - tx * TQ);
			if (!lst)
				return FarMud(TrailKey(tx, tz), ll);     // MUD LAB part D: compact far data (read, not taken back)
			return lst.m_D.m_aD[ll] * lst.m_D.m_aBand[ll];
		}
		if (IsLive(tx, tz))
		{
			if (!TileInZone(tx, tz))
				return 0;
			int v = (k - m_iZ0) * N + (i - m_iX0);
			return m_F.m_aD[v];
		}
		MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
		int l = (k - tz * TQ) * TV + (i - tx * TQ);
		if (st && st.m_Ent)
			return st.m_D.m_aD[l];
		// CAMERA RADIUS: a zone tile with no mesh (yet) still has its mud - the sync layer's (THICK where untouched,
		// cleared by craters / digs), a kept store's bare cells (SYNC off)
		if (!TileInZone(tx, tz))
			return 0;
		if (st && st.m_D.m_aBare[l] != 0)
			return st.m_D.m_aD[l];
		float sd;
		float sou;
		float sov;
		int sb;
		MUD_MudSync.Get(i, k, sd, sou, sov, sb);
		return sd;
	}

	//------------------------------------------------------------------------------------------------
	//! Ground height at x, z from a trace down (vehicles and characters skipped); terrain height if nothing is hit
	protected float GroundDown(float x, float z)
	{
		float sy = GetGame().GetWorld().GetSurfaceY(x, z);
		if (sy < -255)
		{
			// hole cell = our ground: GetSurfaceY is -256 there (mud was drawn 256 m down, only roads / runway showed
			// it - user 2026-10-15) and a WORLD trace still hits the holed heightmap (mud-gotchas 19:58)
			m_iStatHoles++;
			m_iStatTraces++;
			return HoleGround(x, z);
		}
		float top = sy + 8;
		TraceParam p = new TraceParam();
		p.Start = Vector(x, top, z);
		p.End = Vector(x, top - 30, z);
		p.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		p.LayerMask = EPhysicsLayerPresets.Projectile;
		float f = GetGame().GetWorld().TraceMove(p, NotMoving);
		m_iStatTraces++;
		if (f < 1)
			return top - 30 * f;
		return sy;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnTile(int tx, int tz, float y)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = Vector(tx * TQ * CELL, y, tz * TQ * CELL);
		IEntity te = GetGame().SpawnEntity(MUD_MudTileEntity, GetGame().GetWorld(), params);
		if (STORED_RENDERED_GATE && te)
			te.SetFlags(EntityFlags.SET_RENDERED, false);     // A5: IsRendered works
		return te;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP 2026-10-14 control: a 3 x 3 m double-sided quad 2.5 m over the player, built the G1-dome way (mesh first,
	//! then spawn + SetObject once), terrain material (the sheet is dirt). Seen + sheet not seen = the sheet mesh itself.
	protected void DebugQuad()
	{
		IEntity pl = SCR_PlayerController.GetLocalControlledEntity();
		if (!pl)
			return;
		vector qv[4];
		float quv[8];
		int qi[12];
		qv[0] = Vector(-1.5, 0, -1.5);
		qv[1] = Vector(1.5, 0, -1.5);
		qv[2] = Vector(-1.5, 0, 1.5);
		qv[3] = Vector(1.5, 0, 1.5);
		quv[0] = 0;
		quv[1] = 0;
		quv[2] = 1;
		quv[3] = 0;
		quv[4] = 0;
		quv[5] = 1;
		quv[6] = 1;
		quv[7] = 1;
		// a c d / a d b (up) and the same reversed (down)
		qi[0] = 0;
		qi[1] = 2;
		qi[2] = 3;
		qi[3] = 0;
		qi[4] = 3;
		qi[5] = 1;
		qi[6] = 0;
		qi[7] = 3;
		qi[8] = 2;
		qi[9] = 0;
		qi[10] = 1;
		qi[11] = 3;
		int nvs[1];
		int nis[1];
		string mats[1];
		nvs[0] = 4;
		nis[0] = 12;
		mats[0] = MUD_VoxelProbe.MAT_TOP;      // terrain look (grass), so it differs from the sheet's dirt
		m_QuadRes = MeshObject.Create(1, nvs, nis, mats, 0);
		if (!m_QuadRes || !m_QuadRes.GetResource())
		{
			Print("MUD mud sheet: debug quad Create failed", LogLevel.ERROR);
			return;
		}
		MeshObject mesh = m_QuadRes.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, qi);
		mesh.UpdateVerts(0, qv, quv);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = pl.GetOrigin() + "0 2.5 0";
		m_QuadEnt = GetGame().SpawnEntity(MUD_VoxelSubEntity, GetGame().GetWorld(), params);
		if (!m_QuadEnt)
			return;
		m_QuadEnt.SetObject(mesh, "");
		m_QuadEnt.Update();
		Print(string.Format("MUD mud sheet: debug quad at %1", m_QuadEnt.GetOrigin()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void MakeMesh()
	{
		if (DEBUG_SHOW)
			DebugQuad();
		for (int v = 0; v < NN; v++)
			m_F.m_aD[v] = THICK;
		m_iUntraced = NN;
		m_bIdxDirty = true;
		m_fOrgY = m_fEstY;
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		for (int t = 0; t < TILES * TILES; t++)
		{
			int tx = x0 + t % TILES;
			int tz = z0 + t / TILES;
			int key = TrailKey(tx, tz);
			MUD_MudSheetStore st = m_mTrail.Get(key);
			if (st && st.m_Ent)
			{
				// built at load: its data and entity go into the window
				Unpack(st.m_D, t % TILES, t / TILES);
				m_aSlotUnlift[t] = st.m_D.m_iUnlift;
				m_aTileEnt.Insert(st.m_Ent);
				m_aTileRes.Insert(st.m_Res);
				m_mTrail.Remove(key);
				RemoveTrailKey(key);
				continue;
			}
			FillSlot(t % TILES, t / TILES, tx, tz);
			m_aTileEnt.Insert(SpawnTile(tx, tz, m_fEstY));
			m_aTileRes.Insert(null);
		}
		m_iUntraced = 0;
		for (int u = 0; u < NN; u++)
		{
			if (m_F.m_aGs[u] == 0)
				m_iUntraced++;
		}
		m_Ent = m_aTileEnt[0];
		UpdateSlots();
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Wheels(Vehicle veh, float time)
	{
		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
		if (!sim)
			return;
		MUD_MudSheetVeh mv;
		foreach (MUD_MudSheetVeh m : m_aVeh)
		{
			if (m.m_Veh == veh)
				mv = m;
		}
		if (!mv)
		{
			mv = new MUD_MudSheetVeh();
			mv.m_Veh = veh;
			m_iNextVehId++;
			mv.m_iId = m_iNextVehId;
			m_aVeh.Insert(mv);
		}
		WheelsSim(veh, sim, mv, time);
	}

	//------------------------------------------------------------------------------------------------
	//! Wheels of veh (its sim and sheet entry found): stamps from each wheel's last contact to its new one
	protected void WheelsSim(Vehicle veh, VehicleWheeledSimulation sim, MUD_MudSheetVeh mv, float time)
	{
		int count = sim.WheelCount();
		while (mv.m_aLast.Count() < count)
		{
			mv.m_aLast.Insert(vector.Zero);
			mv.m_aTime.Insert(0);
			mv.m_aHas.Insert(false);
		}
		for (int w = 0; w < count; w++)
		{
			if (!sim.WheelHasContact(w))
			{
				mv.m_aHas[w] = false;
				continue;
			}
			vector cp = sim.WheelGetContactPosition(w);
			m_fEstY = cp[1];
			if (mv.m_aHas[w])
			{
				vector lp = mv.m_aLast[w];
				float len = Math.Sqrt((cp[0] - lp[0]) * (cp[0] - lp[0]) + (cp[2] - lp[2]) * (cp[2] - lp[2]));
				if (len < CELL)
					continue;   // keep the old point until the wheel has moved a whole cell
				float dt = time - mv.m_aTime[w];
				if (len < 3 && dt > 0 && ZoneAtWorld(cp[0], cp[2]))
				{
					float half = TyreHalf(sim.WheelGetRadius(w));
					Stamp(lp, cp, len / dt, time, mv.m_iId, half);
					if (m_bLocal)
						OwnMark(cp, time);     // MUD LAB part E: rut ops near it are this stamp again
					// TEMP delay meter: first undrawn own stamp of the wheel's tile (cells >= 0 on Arland: int division ok)
					int dlI = Math.Round(cp[0] / CELL);
					int dlK = Math.Round(cp[2] / CELL);
					int dlTx = dlI / TQ;
					int dlTz = dlK / TQ;
					int dlKey = dlTx * 20000 + dlTz;
					if (!m_mDlStampT.Contains(dlKey))
					{
						m_mDlStampT.Set(dlKey, time);
						m_mDlSpeed.Set(dlKey, len / dt);
					}
					if (SYNC)
						AddPred(lp, cp, len / dt, time, mv.m_iId, half, veh);
				}
			}
			mv.m_aLast[w] = cp;
			mv.m_aTime[w] = time;
			mv.m_aHas[w] = true;
		}
		if (DRAG_TEST)
			Drag(veh, sim, mv, time);
	}

	//------------------------------------------------------------------------------------------------
	//! Mud over the hard ground at x,z, measured from y (the wheel bottom); -1 off the sheet or not traced yet
	protected float DepthAt(float x, float z, float y)
	{
		int i = Math.Round(x / CELL) - m_iX0;
		int k = Math.Round(z / CELL) - m_iZ0;
		if (i < 0 || i >= N || k < 0 || k >= N)
			return -1;
		// ints first: Math.Min returns float, and a float index is read as its raw bits (crash 2026-10-14)
		int sti = Math.Min(i / TQ, TILES - 1);
		int stk = Math.Min(k / TQ, TILES - 1);
		if (m_aSlotZone[stk * TILES + sti] == 0)
			return -1;      // not a mud tile
		int v = k * N + i;
		if (m_F.m_aGs[v] == 0)
			return -1;
		return Math.Max(0, m_F.m_aG[v] + LIFT + m_F.m_aD[v] - y);
	}

	//------------------------------------------------------------------------------------------------
	//! MUD DRAG TEST: per wheel in mud, an impulse against its motion (MudRunner's drag shape) at the hub, and the
	//! gearbox efficiency lowered by the share of wheels in mud. SP only: impulses / gearbox are not replicated.
	protected void Drag(Vehicle veh, VehicleWheeledSimulation sim, MUD_MudSheetVeh mv, float time)
	{
		if (RplSession.Mode() != RplMode.None)
			return;
		Physics ph = veh.GetPhysics();
		if (!ph || !ph.IsDynamic())
			return;
		float dt = time - mv.m_fDragT;
		mv.m_fDragT = time;
		if (dt <= 0 || dt > 0.1)
			return;     // first frame, or a hitch: skip rather than kick
		if (mv.m_fEff0 < 0)
			mv.m_fEff0 = sim.GearboxGetEfficiency();
		vector m[4];
		veh.GetWorldTransform(m);
		vector fwd = Vector(m[2][0], 0, m[2][2]).Normalized();
		vector side = Vector(m[0][0], 0, m[0][2]).Normalized();
		int count = sim.WheelCount();
		if (count <= 0)
			return;
		float refArea = 2 * HALF * THICK;
		float mass = ph.GetMass();
		float mudSum = 0;
		vector total = vector.Zero;
		array<vector> imp = {};
		array<vector> at = {};
		for (int w = 0; w < count; w++)
		{
			if (!sim.WheelHasContact(w))
				continue;
			vector cp = sim.WheelGetContactPosition(w);
			vector vel = ph.GetVelocityAt(cp);
			float vf = vector.Dot(vel, fwd);
			float vs = vector.Dot(vel, side);
			float sp = Math.Sqrt(vf * vf + vs * vs);
			float depth = DepthAt(cp[0], cp[2], cp[1]);
			if (sp > 0.3)
			{
				vector ahead = cp + Vector(vel[0], 0, vel[2]).Normalized() * DRAG_LOOK;
				depth = Math.Max(depth, DepthAt(ahead[0], ahead[2], cp[1]));
			}
			if (depth < DRAG_MIN_DEPTH)
				continue;
			mudSum += Math.Min(depth / THICK, 1);
			if (sp < 0.01)
				continue;
			if (sp > 1)
			{
				float k = Math.Pow(sp, 0.75) / sp;
				vf *= k;
				vs *= k;
			}
			float r = sim.WheelGetRadius(w);
			float dEff = Math.Min(2 * r, depth);
			float aF = vf * 2 * HALF * dEff / refArea;
			float aS = vs * Math.PI * r * dEff / refArea;
			vector a = (fwd * aF + side * aS) * (-SLOW / count);
			total = total + a;
			imp.Insert(a);
			at.Insert(cp + Vector(0, r, 0));
		}
		// cap the whole vehicle's slowing, scale every wheel alike
		float tl = total.Length();
		float scale = mass * dt;
		if (tl > SLOW_MAX)
			scale *= SLOW_MAX / tl;
		// never take more than half the speed in one frame (deep mud at a crawl would push it backwards)
		vector bv = ph.GetVelocity();
		float hs = Math.Sqrt(bv[0] * bv[0] + bv[2] * bv[2]);
		float dv = tl * scale / Math.Max(mass, 1);
		if (dv > 0.5 * hs && dv > 0)
			scale *= 0.5 * hs / dv;
		for (int j = 0; j < imp.Count(); j++)
			ph.ApplyImpulseAt(at[j], imp[j] * scale);
		m_fStatDragA = Math.Max(m_fStatDragA, Math.Min(tl, SLOW_MAX));
		float share = mudSum / count;
		m_fStatMud = Math.Max(m_fStatMud, share);
		float eff = 1 - EFF_LOSS * share;
		if (Math.AbsFloat(eff - mv.m_fEffSet) > 0.02 || (eff == 1 && mv.m_fEffSet != 1))
		{
			sim.GearboxSetEfficiencyState(mv.m_fEff0 * eff);
			mv.m_fEffSet = eff;
		}
		m_fStatEff = Math.Min(m_fStatEff, mv.m_fEffSet);
	}

	//------------------------------------------------------------------------------------------------
	//! GRADUAL: world cell -> map key (12 bits each way: cells 614 m apart share a key, harmless for a few seconds)
	protected int PassKey(int gi, int gk)
	{
		return (gi & 4095) | ((gk & 4095) << 12);
	}

	//------------------------------------------------------------------------------------------------
	//! GRADUAL: forget cells no vehicle touched for longer than PASS_GAP
	protected void PrunePasses(float time)
	{
		m_fPassPruneT = time;
		array<int> old = {};
		foreach (int pk, float pt : m_mPassT)
		{
			if (time - pt > PASS_GAP)
				old.Insert(pk);
		}
		foreach (int ok : old)
		{
			m_mPassT.Remove(ok);
			m_mPassV.Remove(ok);
			m_mPassD.Remove(ok);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! BOX: tyre half width from the wheel radius (no width in the script API)
	static float TyreHalf(float radius)
	{
		return Math.Clamp(radius * HALF_PER_R, HALF_MIN, HALF_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC: one own stamp (Wheels) kept until the op holding it comes (it is stamped again after each restore)
	protected void AddPred(vector a, vector b, float speed, float time, int vehId, float half, Vehicle veh)
	{
		if (m_aPred.Count() >= PRED_CAP)
		{
			// the oldest tenth goes in one copy (not one RemoveOrdered per stamp); its mud stays drawn until the
			// next restore there
			int drop = PRED_CAP / 10;
			array<ref MUD_MudSheetPred> kept = {};
			for (int c = drop; c < m_aPred.Count(); c++)
				kept.Insert(m_aPred[c]);
			m_aPred = kept;
			m_iStatExpired += drop;
		}
		MUD_MudSheetPred pr = new MUD_MudSheetPred();
		pr.m_vA = a;
		pr.m_vB = b;
		pr.m_fSpeed = speed;
		pr.m_fT = time;
		pr.m_iVeh = vehId;
		pr.m_fHalf = half;
		pr.m_Veh = veh;
		m_aPred.Insert(pr);
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC Stage 5 (MUD_MudSync.LoadSnap, a late joiner): the sync layer was replaced in world cells gi0..gi1 x
	//! gk0..gk1 - live window cells and built stored tiles take it again (as an op's restore; bare cells keep theirs).
	//! Tiles not built yet read the layer when they build.
	static void SyncReload(int gi0, int gi1, int gk0, int gk1)
	{
		if (!ENABLED || !SYNC || !s_Inst)
			return;
		s_Inst.RestoreBox(gi0, gi1, gk0, gk1);
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC (MUD_MudSync JOB_SHEET, after each rut op that reached a zone): the sync layer changed world cells
	//! gi0..gi1 x gk0..gk1 for the op segments segA - segB. Own stamps those segments hold go, then the box is
	//! restored from the sync layer. True = own stamps are left to stamp again inside it: SyncReplay does that a
	//! REPLAY_CHUNK at a time (op split 2026-10-18), and the upload waits until it is done.
	static bool SyncBegin(int gi0, int gi1, int gk0, int gk1, array<vector> segA, array<vector> segB)
	{
		if (!ENABLED || !SYNC || !s_Inst)
			return false;     // no sheet yet: BuildZoneTile reads the sync layer
		s_Inst.TakeCovered(gi0, gi1, gk0, gk1, segA, segB);
		bool inWin = s_Inst.RestoreBox(gi0, gi1, gk0, gk1);
		s_Inst.m_aReplay.Clear();
		s_Inst.m_iReplayAt = 0;
		if (inWin)
			s_Inst.PickReplay(gi0, gi1, gk0, gk1, s_Inst.m_aReplay);
		if (s_Inst.m_aReplay.IsEmpty())
			return false;
		s_Inst.m_iRpGi0 = gi0;
		s_Inst.m_iRpGi1 = gi1;
		s_Inst.m_iRpGk0 = gk0;
		s_Inst.m_iRpGk1 = gk1;
		s_Inst.m_bReplayHold = true;
		s_Inst.m_iReplayHoldAt = System.GetTickCount();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC (MUD_MudSync JOB_REPLAY): the next REPLAY_CHUNK own stamps of SyncBegin's box. True = more are left.
	static bool SyncReplay()
	{
		if (!s_Inst || s_Inst.m_aReplay.IsEmpty())
			return false;
		int count = s_Inst.m_aReplay.Count();
		int rFrom = s_Inst.m_iReplayAt;
		int rTo = Math.Min(rFrom + REPLAY_CHUNK, count);
		s_Inst.ReplayList(s_Inst.m_aReplay, rFrom, rTo, s_Inst.m_iRpGi0, s_Inst.m_iRpGi1, s_Inst.m_iRpGk0, s_Inst.m_iRpGk1);
		s_Inst.m_iReplayAt = rTo;
		if (rTo < count)
			return true;
		s_Inst.m_aReplay.Clear();
		if (s_Inst.m_bReplayHold)
			s_Inst.DlHoldEnd(false);     // TEMP delay meter
		s_Inst.m_bReplayHold = false;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP delay meter: a replay hold ended (stuck = let go by REPLAY_HOLD_MS)
	protected void DlHoldEnd(bool stuck)
	{
		int ms = System.GetTickCount(m_iReplayHoldAt);
		m_iDlHolds++;
		m_iDlHoldMs += ms;
		if (ms > m_iDlHoldMax)
			m_iDlHoldMax = ms;
		if (stuck)
			m_iDlStuck++;
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC: own stamps whose middle lies within COVER_R (x z) of one of the op segments go (geometric: the op's
	//! vehicle number is the server's, not ours). Stamps outside the op's cell box are not tested (cost: up to
	//! PRED_CAP stamps x the op's segments, several ops a second).
	protected void TakeCovered(int gi0, int gi1, int gk0, int gk1, array<vector> segA, array<vector> segB)
	{
		if (m_aPred.IsEmpty())
			return;
		float r2 = COVER_R * COVER_R;
		float x0 = gi0 * CELL;
		float x1 = gi1 * CELL;
		float z0 = gk0 * CELL;
		float z1 = gk1 * CELL;
		array<ref MUD_MudSheetPred> kept = {};
		foreach (MUD_MudSheetPred pr : m_aPred)
		{
			vector mid = (pr.m_vA + pr.m_vB) * 0.5;
			bool covered = false;
			if (mid[0] >= x0 && mid[0] <= x1 && mid[2] >= z0 && mid[2] <= z1)
			{
				for (int s = 0; s < segA.Count(); s++)
				{
					if (SegDistSq(mid, segA[s], segB[s]) < r2)
					{
						covered = true;
						break;
					}
				}
			}
			if (covered)
				m_iStatCovered++;
			else
				kept.Insert(pr);
		}
		m_aPred = kept;
	}

	//------------------------------------------------------------------------------------------------
	//! Squared x z distance from p to segment a - b
	protected static float SegDistSq(vector p, vector a, vector b)
	{
		float ax = b[0] - a[0];
		float az = b[2] - a[2];
		float l2 = ax * ax + az * az;
		float t = 0;
		if (l2 > 0.000001)
			t = Math.Clamp(((p[0] - a[0]) * ax + (p[2] - a[2]) * az) / l2, 0, 1);
		float dx = a[0] + ax * t - p[0];
		float dz = a[2] + az * t - p[2];
		return dx * dx + dz * dz;
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC, every 0.5 s: own stamps no op took go after PRED_GONE_S once their vehicle is gone or PRED_FAR m away,
	//! and after PRED_MAX_S in any case; their cells are restored (boxes of at most EXPIRE_BOX cells a side)
	protected void ExpirePreds(float time)
	{
		m_fNextExpire = time + 0.5;
		if (m_aPred.IsEmpty())
			return;
		float far2 = PRED_FAR * PRED_FAR;
		array<ref MUD_MudSheetPred> gone = {};
		array<ref MUD_MudSheetPred> kept = {};
		foreach (MUD_MudSheetPred pr : m_aPred)
		{
			float age = time - pr.m_fT;
			bool drop = false;
			if (age > PRED_GONE_S)
			{
				drop = age > PRED_MAX_S || !pr.m_Veh;
				if (!drop)
				{
					vector vo = pr.m_Veh.GetOrigin();
					float dx = vo[0] - pr.m_vB[0];
					float dz = vo[2] - pr.m_vB[2];
					drop = dx * dx + dz * dz > far2;
				}
			}
			if (drop)
			{
				pr.m_bGone = true;
				gone.Insert(pr);
			}
			else
			{
				kept.Insert(pr);
			}
		}
		if (gone.IsEmpty())
			return;
		m_aPred = kept;
		m_iStatExpired += gone.Count();
		// restore after all are out, so a restore does not stamp a going one again
		int bi0 = 0;
		int bi1 = -1;
		int bk0 = 0;
		int bk1 = -1;
		foreach (MUD_MudSheetPred g : gone)
		{
			int i0 = Math.Floor((Math.Min(g.m_vA[0], g.m_vB[0]) - PRED_PAD) / CELL);
			int i1 = Math.Ceil((Math.Max(g.m_vA[0], g.m_vB[0]) + PRED_PAD) / CELL);
			int k0 = Math.Floor((Math.Min(g.m_vA[2], g.m_vB[2]) - PRED_PAD) / CELL);
			int k1 = Math.Ceil((Math.Max(g.m_vA[2], g.m_vB[2]) + PRED_PAD) / CELL);
			if (bi1 >= bi0)
			{
				int ni0 = Math.Min(bi0, i0);
				int ni1 = Math.Max(bi1, i1);
				int nk0 = Math.Min(bk0, k0);
				int nk1 = Math.Max(bk1, k1);
				if (ni1 - ni0 < EXPIRE_BOX && nk1 - nk0 < EXPIRE_BOX)
				{
					bi0 = ni0;
					bi1 = ni1;
					bk0 = nk0;
					bk1 = nk1;
					continue;
				}
				Resync(bi0, bi1, bk0, bk1);
			}
			bi0 = i0;
			bi1 = i1;
			bk0 = k0;
			bk1 = k1;
		}
		if (bi1 >= bi0)
			Resync(bi0, bi1, bk0, bk1);
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC: world cells gi0..gi1 x gk0..gk1 take the sync layer's mud again - live window cells and stored tiles;
	//! bare cells (craters) keep theirs until Stage 4 clears them in op order - then the own stamps still held are
	//! stamped again inside the window part of the box only (all at once: ExpirePreds)
	protected void Resync(int gi0, int gi1, int gk0, int gk1)
	{
		if (!RestoreBox(gi0, gi1, gk0, gk1))
			return;
		array<ref MUD_MudSheetPred> list = {};
		PickReplay(gi0, gi1, gk0, gk1, list);
		ReplayList(list, 0, list.Count(), gi0, gi1, gk0, gk1);
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC: world cells gi0..gi1 x gk0..gk1 take the sync layer's mud again (live window cells, stored tiles; bare
	//! cells keep theirs; Stage 4: a cell the sync layer has bare goes bare). True = the box meets the placed window
	//! (own stamps may need a replay there).
	protected bool RestoreBox(int gi0, int gi1, int gk0, int gk1)
	{
		int t0 = System.GetTickCount();
		m_iStatResync++;
		float d;
		float ou;
		float ov;
		int sb;
		int i0 = gi0 - m_iX0;
		int i1 = gi1 - m_iX0;
		int k0 = gk0 - m_iZ0;
		int k1 = gk1 - m_iZ0;
		if (i0 < 0)
			i0 = 0;
		if (k0 < 0)
			k0 = 0;
		if (i1 > N - 1)
			i1 = N - 1;
		if (k1 > N - 1)
			k1 = N - 1;
		bool inWin = m_bPlaced && i0 <= i1 && k0 <= k1;
		if (inWin)
		{
			for (int k = k0; k <= k1; k++)
			{
				for (int i = i0; i <= i1; i++)
				{
					int v = k * N + i;
					if (m_F.m_aBare[v] != 0)
						continue;
					MUD_MudSync.Get(m_iX0 + i, m_iZ0 + k, d, ou, ov, sb);
					m_F.m_aD[v] = d;
					m_F.m_aOu[v] = ou;
					m_F.m_aOv[v] = ov;
					m_F.m_aBare[v] = sb;
				}
			}
			MarkRect(i0, i1, k0, k1);
			m_bDirty = true;
		}
		// stored tiles (TV x TV verts, edges shared: tile tx holds world cells tx * TQ .. tx * TQ + TQ)
		int txa = (gi0 - 1) / TQ;
		int txb = gi1 / TQ;
		int tza = (gk0 - 1) / TQ;
		int tzb = gk1 / TQ;
		for (int tz = tza; tz <= tzb; tz++)
		{
			for (int tx = txa; tx <= txb; tx++)
			{
				if (IsLive(tx, tz))
					continue;
				MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
				if (!st || !st.m_Ent)
					continue;
				MUD_MudSheetTileData td = st.m_D;
				bool hit = false;
				for (int lk = 0; lk < TV; lk++)
				{
					int sgk = tz * TQ + lk;
					if (sgk < gk0 || sgk > gk1)
						continue;
					for (int li = 0; li < TV; li++)
					{
						int sgi = tx * TQ + li;
						if (sgi < gi0 || sgi > gi1)
							continue;
						int l = lk * TV + li;
						if (td.m_aBare[l] != 0)
							continue;
						MUD_MudSync.Get(sgi, sgk, d, ou, ov, sb);
						if (d == td.m_aD[l] && ou == td.m_aOu[l] && ov == td.m_aOv[l] && sb == 0)
							continue;
						td.m_aD[l] = d;
						td.m_aOu[l] = ou;
						td.m_aOv[l] = ov;
						td.m_aBare[l] = sb;
						hit = true;
					}
				}
				if (!hit)
					continue;
				QueueBuild(tx, tz);
				m_iStatStoredRe++;
			}
		}
		m_iStatResyncMs += System.GetTickCount(t0);
		return inWin;
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC: the own stamps (oldest first) that reach world cells gi0..gi1 x gk0..gk1, into 'into'
	protected void PickReplay(int gi0, int gi1, int gk0, int gk1, array<ref MUD_MudSheetPred> into)
	{
		float x0 = gi0 * CELL - PRED_PAD;
		float x1 = gi1 * CELL + PRED_PAD;
		float z0 = gk0 * CELL - PRED_PAD;
		float z1 = gk1 * CELL + PRED_PAD;
		foreach (MUD_MudSheetPred pr : m_aPred)
		{
			if (Math.Max(pr.m_vA[0], pr.m_vB[0]) < x0 || Math.Min(pr.m_vA[0], pr.m_vB[0]) > x1)
				continue;
			if (Math.Max(pr.m_vA[2], pr.m_vB[2]) < z0 || Math.Min(pr.m_vA[2], pr.m_vB[2]) > z1)
				continue;
			into.Insert(pr);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! SYNC: own stamps list[rFrom..rTo) again, clipped to the window cells of world box gi0..gi1 x gk0..gk1 (a stamp
	//! only ever wrote window cells; the clip is taken from the window as it is now - it may have moved since the
	//! restore). Expired ones are skipped.
	protected void ReplayList(array<ref MUD_MudSheetPred> list, int rFrom, int rTo, int gi0, int gi1, int gk0, int gk1)
	{
		if (!m_bPlaced || rFrom >= rTo)
			return;
		int i0 = Math.Max(gi0 - m_iX0, 0);
		int i1 = Math.Min(gi1 - m_iX0, N - 1);
		int k0 = Math.Max(gk0 - m_iZ0, 0);
		int k1 = Math.Min(gk1 - m_iZ0, N - 1);
		if (i0 > i1 || k0 > k1)
			return;
		int t0 = System.GetTickCount();
		m_bClip = true;
		m_iClipI0 = i0;
		m_iClipI1 = i1;
		m_iClipK0 = k0;
		m_iClipK1 = k1;
		for (int r = rFrom; r < rTo; r++)
		{
			MUD_MudSheetPred pr = list[r];
			if (!pr || pr.m_bGone)
				continue;
			Stamp(pr.m_vA, pr.m_vB, pr.m_fSpeed, pr.m_fT, pr.m_iVeh, pr.m_fHalf);
			m_iStatReplayed++;
			m_iSt10Replay++;
		}
		m_bClip = false;
		m_iStatResyncMs += System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! One tyre step a -> b at speed (m/s): PRESS under the tyre, PUSH a ridge each side and a bow wave ahead
	protected void Stamp(vector a, vector b, float speed, float time, int vehId, float half)
	{
		if (!BOX)
			half = HALF;
		if (GRADUAL && time - m_fPassPruneT > 5)
			PrunePasses(time);
		if (SOFT && GRADUAL)
		{
			// WIDEN: how pressed the mud at the contact was when this pass began
			int wi = Math.Round(b[0] / CELL) - m_iX0;
			int wk = Math.Round(b[2] / CELL) - m_iZ0;
			if (wi >= 0 && wi < N && wk >= 0 && wk < N)
			{
				float wd = m_F.m_aD[wk * N + wi];
				int wpk = PassKey(m_iX0 + wi, m_iZ0 + wk);
				if (m_mPassT.Contains(wpk) && m_mPassV.Get(wpk) == vehId && time - m_mPassT.Get(wpk) <= PASS_GAP)
					wd = m_mPassD.Get(wpk);
				float pressed = Math.Clamp((THICK - wd) / THICK, 0, 1);
				half = half * (W_FIRST + (1 - W_FIRST) * pressed);
			}
		}
		float band = BAND * half / HALF;   // ridge width grows with the tyre
		vector dir = Vector(b[0] - a[0], 0, b[2] - a[2]);
		float len = dir.Length();
		dir = dir * (1 / len);
		vector side = Vector(dir[2], 0, -dir[0]);
		float reach = half + band;
		float nose = len + NOSE;            // footprint runs a -> a + dir * nose
		float far = nose + reach;           // the arch round the front ends here
		if (BOX)
			far = nose + FRONT_LEN;          // BOX: the push ahead ends here
		float ex = a[0] + dir[0] * far;
		float ez = a[2] + dir[2] * far;
		int i0 = Math.Floor((Math.Min(a[0], ex) - reach) / CELL) - m_iX0;
		int i1 = Math.Ceil((Math.Max(a[0], ex) + reach) / CELL) - m_iX0;
		int k0 = Math.Floor((Math.Min(a[2], ez) - reach) / CELL) - m_iZ0;
		int k1 = Math.Ceil((Math.Max(a[2], ez) + reach) / CELL) - m_iZ0;
		if (i0 < 1)
			i0 = 1;
		if (k0 < 1)
			k0 = 1;
		if (i1 > N - 2)
			i1 = N - 2;
		if (k1 > N - 2)
			k1 = N - 2;
		if (m_bClip)
		{
			// SYNC replay (Resync): only inside the box just restored from the sync layer
			if (i0 < m_iClipI0)
				i0 = m_iClipI0;
			if (i1 > m_iClipI1)
				i1 = m_iClipI1;
			if (k0 < m_iClipK0)
				k0 = m_iClipK0;
			if (k1 > m_iClipK1)
				k1 = m_iClipK1;
		}
		if (i1 < i0 || k1 < k0)
			return;
		m_iMarkWhy = WHY_STAMP;
		MarkRect(i0, i1, k0, k1);
		m_iMarkWhy = WHY_OTHER;
		// sink: the known ground just outside the ridge band against the tyre's contact height
		float sideSum = 0;
		int sideN = 0;
		for (int s = -1; s <= 1; s += 2)
		{
			vector q = b + side * (s * (reach + CELL));
			int qi = Math.Round(q[0] / CELL) - m_iX0;
			int qk = Math.Round(q[2] / CELL) - m_iZ0;
			if (qi < 0 || qi >= N || qk < 0 || qk >= N || m_F.m_aGs[qk * N + qi] == 0)
				continue;
			sideSum += m_F.m_aG[qk * N + qi];
			sideN++;
		}
		float sink = 0;
		if (sideN > 0)
		{
			float raw = sideSum / sideN - b[1];
			sink = Math.Max(0, raw);
			m_fStatSink += raw;
			m_iStatSinkN++;
			m_fStatSinkMin = Math.Min(m_fStatSinkMin, raw);
			m_fStatSinkMax = Math.Max(m_fStatSinkMax, raw);
		}
		float h = Math.Min(HMAX, H0 + H_SPEED * speed + H_SINK * sink);
		m_fStatH = h;
		float drag = DRAG * len;
		for (int k = k0; k <= k1; k++)
		{
			for (int i = i0; i <= i1; i++)
			{
				vector p = Vector((m_iX0 + i) * CELL - a[0], 0, (m_iZ0 + k) * CELL - a[2]);
				float t = vector.Dot(p, dir);
				if (t < 0 || t > far)
					continue;
				// ARCH (user 2026-10-15 sketch): the footprint is a capsule a -> nose with radius HALF; the ridge is the
				// band BAND wide round it, so beside the tyre it runs straight and round the front it bends in an arch
				float tc = Math.Clamp(t, 0, nose);
				vector off = p - dir * tc;
				float dist = off.Length();
				float lat = Math.AbsFloat(vector.Dot(p, side));     // BOX: distance across the track
				if (BOX)
				{
					if (lat >= reach || (t > nose && lat >= half))
						continue;
				}
				else if (dist >= reach)
					continue;
				int v = k * N + i;
				if (m_bLocal && m_F.m_aTouch[v] == 0)
				{
					m_F.m_aTouch[v] = 1;     // SHEET LOCAL: deposit band source
					m_bTouchNew = true;
					m_iTouchCells++;
					if (BAND_INCR)
					{
						m_aNewGi.Insert(m_iX0 + i);     // stage 2 C1: BandIncr at the next upload
						m_aNewGk.Insert(m_iZ0 + k);
					}
				}
				if (LEV_L2_DRAG)
					m_Drag.Add(v);     // L2: LimitDragCells at the next upload
				float d0 = m_F.m_aD[v];     // mud depth when this vehicle's pass over the cell began
				if (GRADUAL)
				{
					int pk = PassKey(m_iX0 + i, m_iZ0 + k);
					if (!m_mPassT.Contains(pk) || m_mPassV.Get(pk) != vehId || time - m_mPassT.Get(pk) > PASS_GAP)
						m_mPassD.Set(pk, d0);     // a new pass starts here
					else
						d0 = m_mPassD.Get(pk);
					m_mPassT.Set(pk, time);
					m_mPassV.Set(pk, vehId);
				}
				float target = 0;
				float du = 0;
				float dv = 0;
				bool under = dist < half;
				if (BOX)
					under = t <= nose && lat < half;
				if (under)
				{
					// PRESS: no mud under the tyre (GRADUAL: PRESS_STEP thinner than at the pass start), the texture is dragged along
					float pf = 1;
					if (SOFT && BOX)
					{
						// SOFT_WALL: full press in the middle, none at the side
						float inW = half * SOFT_WALL;
						float fromSide = half - lat;
						if (fromSide < inW)
							pf = 0.5 * (1 - Math.Cos(Math.PI * fromSide / inW));
					}
					if (GRADUAL)
						m_F.m_aD[v] = Math.Min(m_F.m_aD[v], Math.Max(0, Math.Min(d0, THICK) - PRESS_STEP * pf));
					else
						m_F.m_aD[v] = Math.Min(m_F.m_aD[v], THICK * (1 - pf));
					du = dir[0] * drag;
					dv = dir[2] * drag;
				}
				else
				{
					// PUSH: ridge round the tyre (sides full height, the front arch ARCH_H), texture pushed outward
					float w = Math.Sin(Math.PI * (dist - half) / band);
					float hh = h;
					if (t > nose)
						hh = h * ARCH_H;
					if (BOX)
					{
						if (t > nose)
						{
							// push ahead: tyre wide, highest right at the tyre front, gone at FRONT_LEN
							float tf = (t - nose) / FRONT_LEN;
							w = Math.Cos(tf * Math.PI * 0.5);
						}
						else
						{
							// side ridge: steep at the wall, flat top, then falls off to the band's outer edge
							float x = (lat - half) / band;
							w = 1;
							if (x > RIDGE_TOP)
								w = 0.5 * (1 + Math.Cos(Math.PI * (x - RIDGE_TOP) / (1 - RIDGE_TOP)));
							else if (SOFT)
								w = Math.Sin(0.5 * Math.PI * x / RIDGE_TOP);    // round shoulder up from the wall
						}
					}
					target = THICK + hh * w;
					if (GRADUAL)
						target = Math.Min(target, Math.Max(d0, THICK) + hh * w * PRESS_STEP / THICK);
					if (BOX)
					{
						vector pushDir = dir;
						if (t <= nose)
						{
							pushDir = side;
							if (vector.Dot(p, side) < 0)
								pushDir = -side;
						}
						du = pushDir[0] * drag * 0.5 * w;
						dv = pushDir[2] * drag * 0.5 * w;
					}
					else
					{
						du = off[0] / dist * drag * 0.5 * w;
						dv = off[2] / dist * drag * 0.5 * w;
					}
				}
				if (target > m_F.m_aD[v])
					m_F.m_aD[v] = target;
				float ou = m_F.m_aOu[v] + du;
				float ov = m_F.m_aOv[v] + dv;
				float ol = Math.Sqrt(ou * ou + ov * ov);
				if (ol > DRAG_MAX)
				{
					ou *= DRAG_MAX / ol;
					ov *= DRAG_MAX / ol;
				}
				m_F.m_aOu[v] = ou;
				m_F.m_aOv[v] = ov;
				if (m_F.m_aD[v] > EPS)
					Grow(i, k);
			}
		}
		if (DRAG_LIM > 0 && !LEV_L2_DRAG)
			LimitDrag(i0, i1, k0, k1);
		m_bDirty = true;
		if (m_bTouchNew)
		{
			m_bTouchNew = false;
			m_bIncrMark = BAND_INCR;     // stage 2 C1: live slots only get born (BandIncr dirties by band change)
			BandMark(i0, i1, k0, k1);     // SHEET LOCAL: tiles born, bands due
			m_bIncrMark = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! UV STRETCH: pulls each vertex's drag in window cells i0..i1 x k0..k1 (all inside 1..N-2) back to within DRAG_LIM
	//! of its 4 neighbours (only ever toward them, so drag never spreads). Two sweeps, opposite directions.
	protected void LimitDrag(int i0, int i1, int k0, int k1)
	{
		for (int sweep = 0; sweep < 2; sweep++)
		{
			for (int kk = k0; kk <= k1; kk++)
			{
				int k = kk;
				if (sweep == 1)
					k = k0 + k1 - kk;
				for (int ii = i0; ii <= i1; ii++)
				{
					int i = ii;
					if (sweep == 1)
						i = i0 + i1 - ii;
					int v = k * N + i;
					float ou = m_F.m_aOu[v];
					float ov = m_F.m_aOv[v];
					for (int q = 0; q < 4; q++)
					{
						int nb = v - 1;
						if (q == 1)
							nb = v + 1;
						else if (q == 2)
							nb = v - N;
						else if (q == 3)
							nb = v + N;
						ou = Math.Clamp(ou, m_F.m_aOu[nb] - DRAG_LIM, m_F.m_aOu[nb] + DRAG_LIM);
						ov = Math.Clamp(ov, m_F.m_aOv[nb] - DRAG_LIM, m_F.m_aOv[nb] + DRAG_LIM);
					}
					m_F.m_aOu[v] = ou;
					m_F.m_aOv[v] = ov;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Window cell i, k lies on a zone tile (mud drawn there)
	protected bool ZoneAtCell(int i, int k)
	{
		if (i < 0 || k < 0 || i >= N || k >= N)
			return false;
		int ti = i / TQ;
		int tk = k / TQ;
		if (ti >= TILES)
			ti = TILES - 1;
		if (tk >= TILES)
			tk = TILES - 1;
		return m_aSlotZone[tk * TILES + ti] != 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ZoneAtWorld(float x, float z)
	{
		if (m_bLocal)
		{
			int ltx = Math.Floor(x / TILE_M);
			int ltz = Math.Floor(z / TILE_M);
			return MUD_MudZoneEntity.TileCovered(ltx, ltz);     // SHEET LOCAL: holed offroad ground (the stamp bears the tile)
		}
		int i = Math.Round(x / CELL);
		int k = Math.Round(z / CELL);
		return ZoneAtCell(i - m_iX0, k - m_iZ0);
	}

	//------------------------------------------------------------------------------------------------
	//! Mud box grows to hold cell i, k (+1)
	protected void Grow(int i, int k)
	{
		if (m_iBoxI1 < m_iBoxI0)
		{
			m_iBoxI0 = i - 1;
			m_iBoxI1 = i + 1;
			m_iBoxK0 = k - 1;
			m_iBoxK1 = k + 1;
		}
		m_iBoxI0 = Math.Max(0, Math.Min(m_iBoxI0, i - 1));
		m_iBoxI1 = Math.Min(N - 1, Math.Max(m_iBoxI1, i + 1));
		m_iBoxK0 = Math.Max(0, Math.Min(m_iBoxK0, k - 1));
		m_iBoxK1 = Math.Min(N - 1, Math.Max(m_iBoxK1, k + 1));
		if (LEV_L3_SLUMP && i >= 0 && i < N && k >= 0 && k < N)
			m_Chg.Add(k * N + i);     // L3: slump looks here next time
	}

	//------------------------------------------------------------------------------------------------
	protected float GroundAt(int v)
	{
		if (m_F.m_aGs[v] != 0)
			return m_F.m_aG[v];
		return m_fEstY;
	}

	//------------------------------------------------------------------------------------------------
	//! Mud flows to each lower 4-neighbour where its surface (ground + mud) is steeper than REPOSE; volume kept
	protected void Slump()
	{
		if (LEV_L3_SLUMP)
		{
			SlumpCells();
			return;
		}
		if (m_iBoxI1 < m_iBoxI0)
			return;
		m_iBoxI0 = Math.Max(0, m_iBoxI0);
		m_iBoxK0 = Math.Max(0, m_iBoxK0);
		m_iBoxI1 = Math.Min(N - 1, m_iBoxI1);
		m_iBoxK1 = Math.Min(N - 1, m_iBoxK1);
		if (m_iBoxI1 < m_iBoxI0 || m_iBoxK1 < m_iBoxK0)
		{
			m_iBoxI1 = -1;
			m_iBoxI0 = 0;
			return;
		}
		float step = REPOSE * CELL;
		for (int k = m_iBoxK0; k <= m_iBoxK1; k++)
		{
			for (int i = m_iBoxI0; i <= m_iBoxI1; i++)
				m_T.m_aD[k * N + i] = 0;
		}
		bool moved = false;
		for (int k2 = m_iBoxK0; k2 <= m_iBoxK1; k2++)
		{
			for (int i2 = m_iBoxI0; i2 <= m_iBoxI1; i2++)
			{
				int v = k2 * N + i2;
				float da = m_F.m_aD[v];
				if (da <= 0)
					continue;
				float sa = GroundAt(v) + da;
				float give = 0;
				// 4 neighbours; at most a quarter of the mud leaves per direction, so it never goes negative
				for (int n = 0; n < 4; n++)
				{
					int ni = i2;
					int nk = k2;
					if (n == 0)
						ni++;
					else if (n == 1)
						ni--;
					else if (n == 2)
						nk++;
					else
						nk--;
					if (ni < m_iBoxI0 || ni > m_iBoxI1 || nk < m_iBoxK0 || nk > m_iBoxK1)
						continue;   // the box is 1 cell wider than the mud, and only the box is applied
					int nv = nk * N + ni;
					float drop = sa - GroundAt(nv) - m_F.m_aD[nv] - step;
					if (drop <= 0)
						continue;
					float f = Math.Min(drop * SLUMP * 0.5, da * 0.25);
					if (f < 0.0002)
						continue;
					m_T.m_aD[nv] = m_T.m_aD[nv] + f;
					give += f;
					moved = true;
				}
				m_T.m_aD[v] = m_T.m_aD[v] - give;
			}
		}
		if (!moved)
		{
			// settled: nothing here moves again until a stamp changes it, and Stamp grows a new box round the wheel
			// (COST 2026-10-14: the box only ever grew, to ~the whole sheet with the THICK layer everywhere)
			m_iBoxI1 = -1;
			m_iBoxI0 = 0;
			m_iBoxK1 = -1;
			m_iBoxK0 = 0;
			return;
		}
		int bi0 = m_iBoxI0;
		int bi1 = m_iBoxI1;
		int bk0 = m_iBoxK0;
		int bk1 = m_iBoxK1;
		// next box: only the cells that moved now (+1 via Grow); a cell that did not move and has no moving
		// neighbour stays put next pass too (PERF 2026-10-15: the box only shrank when ALL was settled)
		m_iBoxI1 = -1;
		m_iBoxI0 = 0;
		m_iBoxK1 = -1;
		m_iBoxK0 = 0;
		int shown = 0;
		for (int k3 = bk0; k3 <= bk1; k3++)
		{
			for (int i3 = bi0; i3 <= bi1; i3++)
			{
				int v3 = k3 * N + i3;
				float dd = m_T.m_aD[v3];
				if (dd == 0)
					continue;
				m_F.m_aD[v3] = m_F.m_aD[v3] + dd;
				Grow(i3, k3);
				if (Math.AbsFloat(dd) > SLUMP_SHOW)
				{
					MarkRect(i3, i3, k3, k3);
					shown++;
				}
			}
		}
		if (shown > 0)
			m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ground heights over the whole sheet: only cells with even i and k are traced (NEW_TRACES for cells never traced,
	//! then OLD_TRACES re-traces round robin); each trace refills the in-between cells next to it (FillBetween)
	protected void Grounds()
	{
		if (m_iUntraced > 0)
		{
			int newLeft = NEW_TRACES;
			while (m_iScan < NN && newLeft > 0)
			{
				int v = m_iScan;
				m_iScan++;
				if (m_F.m_aGs[v] != 0)
					continue;
				int i = v % N;
				int k = v / N;
				if (i % 2 != 0 || k % 2 != 0)
					continue;   // filled when its traced neighbours are there
				if (!ZoneAtCellAny(i, k))
					continue;   // no mud drawn there (PERF 2026-10-15)
				Trace(i, k);
				newLeft--;
				m_iUntraced--;
				m_bIdxDirty = true;
				FillAround(i, k);
			}
			if (m_iScan >= NN)
				m_iUntraced = 0;
		}
		if (ONE_GRID && m_bOneHidden)
			return;
		for (int r = 0; r < OLD_TRACES; r++)
		{
			// step over the traced cells only: 2 cells each way
			m_iRobin = (m_iRobin + 2) % NN;
			int ri = m_iRobin % N;
			int rk = m_iRobin / N;
			if (ri % 2 != 0)
				ri--;
			if (rk % 2 != 0)
				rk--;
			int rv = rk * N + ri;
			if (m_F.m_aGs[rv] == 0 || !ZoneAtCellAny(ri, rk))
				continue;
			Trace(ri, rk);
			FillAround(ri, rk);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The 8 cells round traced cell i, k that are not traced themselves: refill each one whose traced corners are there
	protected void FillAround(int i, int k)
	{
		for (int dk = -1; dk <= 1; dk++)
		{
			for (int di = -1; di <= 1; di++)
			{
				if (di == 0 && dk == 0)
					continue;
				int ci = i + di;
				int ck = k + dk;
				if (ci < 0 || ci >= N || ck < 0 || ck >= N)
					continue;
				FillBetween(ci, ck);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Cell i, k (odd i or k) = mean of its traced neighbours (2 along a line, 4 in a square), once all are traced
	protected void FillBetween(int i, int k)
	{
		int i0 = i - i % 2;
		int i1 = i0;
		if (i % 2 != 0)
			i1 = i0 + 2;
		int k0 = k - k % 2;
		int k1 = k0;
		if (k % 2 != 0)
			k1 = k0 + 2;
		if (i1 >= N || k1 >= N)
			return;     // N is odd, so this never happens (last index is even)
		int a = k0 * N + i0;
		int b = k0 * N + i1;
		int c = k1 * N + i0;
		int d = k1 * N + i1;
		if (m_F.m_aGs[a] == 0 || m_F.m_aGs[b] == 0 || m_F.m_aGs[c] == 0 || m_F.m_aGs[d] == 0)
			return;
		float g = (m_F.m_aG[a] + m_F.m_aG[b] + m_F.m_aG[c] + m_F.m_aG[d]) * 0.25;
		int v = k * N + i;
		if (m_F.m_aGs[v] == 0)
		{
			m_iUntraced--;
			m_bIdxDirty = true;
			m_bDirty = true;
			MarkRect(i, i, k, k);
		}
		else if (Math.AbsFloat(g - m_F.m_aG[v]) > 0.002)
		{
			m_bDirty = true;
			MarkRect(i, i, k, k);
		}
		m_F.m_aGs[v] = 1;
		m_F.m_aG[v] = g;
	}

	//------------------------------------------------------------------------------------------------
	protected void Trace(int i, int k)
	{
		int v = k * N + i;
		float x = (m_iX0 + i) * CELL;
		float z = (m_iZ0 + k) * CELL;
		float g = m_fEstY;
		m_iStatTraces++;
		if (MUD_Surface.IsHole(GetGame().GetWorld(), x, z))
		{
			// our ground: the plain trace below would stop on the holed heightmap above a crater floor (GroundDown)
			m_iStatHoles++;
			g = HoleGround(x, z);
		}
		else
		{
			float top = m_fEstY + 4;
			if (m_F.m_aGs[v] != 0)
				top = m_F.m_aG[v] + 2;
			TraceParam p = new TraceParam();
			p.Start = Vector(x, top, z);
			p.End = Vector(x, top - 10, z);
			p.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			p.LayerMask = EPhysicsLayerPresets.Projectile;
			float f = GetGame().GetWorld().TraceMove(p, NotMoving);
			if (f < 1)
				g = top - 10 * f;
		}
		if (m_F.m_aGs[v] == 0 || Math.AbsFloat(g - m_F.m_aG[v]) > 0.002)
		{
			m_bDirty = true;
			MarkRect(i, i, k, k);
		}
		m_F.m_aGs[v] = 1;
		m_F.m_aG[v] = g;
	}

	//------------------------------------------------------------------------------------------------
	protected bool NotMoving(notnull IEntity e)
	{
		// own mud tiles skipped: their collider (COLLIDE) is hit by these Projectile traces (log 2026-10-15 00:53)
		return !Vehicle.Cast(e) && !ChimeraCharacter.Cast(e) && !MUD_MudTileEntity.Cast(e);
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuild every live tile's mesh from the field
	protected void Upload()
	{
		if (m_bLocal && BAND_INCR)
		{
			BandIncr();     // stage 2 C1: the band round the touches since the last upload
			if (MUD_TestKit.TEST_KIT && !m_bBenchDone)
				BandBench();
		}
		m_bDirty = false;
		if (DRAG_LIM > 0 && !m_Drag.m_aList.IsEmpty())
			LimitDragCells();     // L2 (cells stay listed only while it is on, or one last time after)
		if (!m_Ent)
			return;
		m_iStatUploads++;
		m_bIdxDirty = false;
		m_iBuildQuads = 0;
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		UploadPick(x0, z0);
		// stage 2 part A: co-build picks; pass 1 builds the live neighbours an earlier slot's build asked for (A3)
		m_iCoStoredUp = 0;
		m_bCoAny = false;
		for (int c = 0; c < TILES * TILES; c++)
			m_aCoLive[c] = 0;
		if (m_bLocal && (EDGE_COBUILD_PICK || EDGE_COBUILD))
			CoPick();
		m_bInUpload = true;
		for (int pass = 0; pass < 2; pass++)
		{
			if (pass == 1 && !m_bCoAny)
				break;
			for (int tk = 0; tk < TILES; tk++)
			{
				for (int ti = 0; ti < TILES; ti++)
				{
					int t = tk * TILES + ti;
					if (pass == 1 && m_aCoLive[t] == 0)
						continue;
					UploadSlot(t, ti, tk, x0, z0);
				}
			}
		}
		m_bInUpload = false;
		m_iStatQuads = m_iBuildQuads;
		if (m_bLocal && !m_aBandQ.IsEmpty())
			BandStored();
	}

	//------------------------------------------------------------------------------------------------
	//! Upload: live slot t (ti, tk) - built if dirty and picked
	protected void UploadSlot(int t, int ti, int tk, int x0, int z0)
	{
		if (m_aSlotDirty[t] == 0)
			return;
		if (m_aPick[t] == 2)
		{
			m_iUpThrottled++;
			m_bDirty = true;      // D1: built less than SLOT_MIN_S ago - next upload
			return;
		}
		if (m_aPick[t] == 0)
		{
			m_aSlotWait[t] = m_aSlotWait[t] + 1;
			m_iUpSkipped++;
			m_bDirty = true;      // S1: next upload
			return;
		}
		if (m_bLocal && m_aSlotZone[t] != 0 && !SlotTraced(t))
		{
			m_iBornWait++;
			m_bDirty = true;      // SHEET LOCAL: never built before every cell has its ground
			return;
		}
		m_aSlotDirty[t] = 0;
		m_aSlotWait[t] = 0;
		if (m_aSlotZone[t] == 0)
		{
			if (m_aTileEnt[t])
			{
				m_aTileEnt[t].ClearFlags(EntityFlags.VISIBLE, false);    // no mud outside the zones
				DropColl(m_aTileEnt[t]);
			}
			if (m_aSlotReady[t] != 0)
				s_iCoverGen++;
			m_aSlotReady[t] = 0;
			m_aCoLive[t] = 0;
			return;
		}
		if (m_bLocal && m_aSlotBand[t] != 0)
		{
			m_aSlotBand[t] = 0;
			if (!BAND_INCR || !DistOk(t))
				BandTile(x0 + ti, z0 + tk, null, ti, tk);     // stage 2 C1: not when the kept distances are current
		}
		Pack(ti, tk, m_TD);
		m_iBuildWhy = UploadWhy(t);
		m_bCoBuilding = m_aCoLive[t] != 0;     // A3: a co-build asks for no co-builds itself
		if (m_bCoBuilding)
			m_iCoLive++;
		m_aCoLive[t] = 0;
		Resource r = BuildTile(m_aTileEnt[t], m_TD, x0 + ti, z0 + tk, true);
		m_bCoBuilding = false;
		m_aSlotWhy[t] = 0;
		if (m_bLocal)
			CopyLastY(ti, tk);
		m_aSlotBuiltMs[t] = Math.Round(m_fLast * 1000);
		if (r)
			m_aTileRes[t] = r;    // keep the new mesh alive, drop the old one only now
		int dlKey = (x0 + ti) * 20000 + z0 + tk;     // TEMP delay meter
		if (r && m_mDlStampT.Contains(dlKey))
		{
			float dl = m_fLast - m_mDlStampT.Get(dlKey);
			m_fDlSum += dl;
			m_iDlN++;
			if (dl > m_fDlMax)
				m_fDlMax = dl;
			m_fDlDist += dl * m_mDlSpeed.Get(dlKey);
			m_mDlStampT.Remove(dlKey);
			m_mDlSpeed.Remove(dlKey);
		}
		// LAZY RUT MESH: ready = a mesh drawn with every cell's ground
		int ready = 0;
		if (r)
		{
			ready = 1;
			for (int gl = 0; gl < TV * TV; gl++)
			{
				if (m_TD.m_aGs[gl] == 0)
				{
					ready = 0;
					break;
				}
			}
		}
		if (m_aSlotReady[t] != 0 && ready == 0)
			s_iCoverGen++;
		m_aSlotReady[t] = ready;
	}

	//------------------------------------------------------------------------------------------------
	//! S1: which dirty slots Upload builds now (m_aPick[t] 1). Always: non-zone slots (only hidden, cheap) and zone slots
	//! without a mesh. Then up to UPLOAD_SLOTS zone slots: the longest-waiting one (no slot starves while the near
	//! ones stay dirty), then the nearest to a vehicle (slot centre to vehicle origin, x z).
	protected void UploadPick(int x0, int z0)
	{
		int nDirty = 0;
		for (int t = 0; t < TILES * TILES; t++)
		{
			m_aPick[t] = 0;
			m_aPickWhy[t] = -1;
			if (m_aSlotDirty[t] == 0)
				continue;
			if (UPLOAD_SLOTS <= 0 || m_aSlotZone[t] == 0 || t >= m_aTileRes.Count() || !m_aTileRes[t])
			{
				m_aPick[t] = 1;
				m_aPickWhy[t] = BW_MESHLESS;
			}
			else if (SLOT_MIN_S > 0 && m_fLast * 1000 - m_aSlotBuiltMs[t] < SLOT_MIN_S * 1000 && m_fLast * 1000 >= m_aSlotBuiltMs[t])
				m_aPick[t] = 2;     // D1: has a mesh, built too recently (a time jump back never holds)
			else
				nDirty++;
		}
		if (SLOT_MIN_S > 0 && SLOT_MIN_WHEEL_FREE)
			PickWheelSlots(true);     // D1 fix: held wheel slots build now (they use up UPLOAD_SLOTS below)
		for (int wh = 0; wh < TILES * TILES; wh++)
		{
			if (m_aPick[wh] == 3)
				m_aPickWhy[wh] = BW_WHEEL;
		}
		if (nDirty <= UPLOAD_SLOTS)
		{
			for (int a = 0; a < TILES * TILES; a++)
			{
				if (m_aSlotDirty[a] != 0 && m_aPick[a] == 0)
				{
					m_aPick[a] = 1;
					m_aPickWhy[a] = BW_FIT;
				}
			}
			return;
		}
		// WHEEL TILES FIRST: dirty zone slots under a wheel contact (they also use up UPLOAD_SLOTS)
		int wheelPicked = PickWheelSlots(false);
		for (int wp = 0; wp < TILES * TILES; wp++)
		{
			if (m_aPick[wp] == 3)
			{
				m_aPick[wp] = 1;     // D1 fix: released held wheel slot
				wheelPicked++;
			}
		}
		for (int wq = 0; wq < TILES * TILES; wq++)
		{
			if (m_aPick[wq] == 1 && m_aPickWhy[wq] < 0)
				m_aPickWhy[wq] = BW_WHEEL;
		}
		// the longest-waiting dirty zone slot
		int old = -1;
		for (int o = 0; o < TILES * TILES; o++)
		{
			if (m_aSlotDirty[o] == 0 || m_aPick[o] != 0)
				continue;
			if (m_aSlotWait[o] > 0 && (old < 0 || m_aSlotWait[o] > m_aSlotWait[old]))
				old = o;
		}
		int left = UPLOAD_SLOTS - wheelPicked;
		if (old >= 0)
		{
			m_aPick[old] = 1;
			m_aPickWhy[old] = BW_OLDEST;
			left--;
		}
		// nearest to a vehicle: smallest distance first, left times
		float dist[16];
		for (int d = 0; d < TILES * TILES; d++)
		{
			dist[d] = 1000000000.0;
			if (m_aSlotDirty[d] == 0 || m_aPick[d] != 0)
				continue;
			int dI = d % TILES;     // own int line: '%' inside a float expression does not compile (mud-gotchas)
			int dK = d / TILES;
			float cx = ((x0 + dI) * TQ + TQ / 2) * CELL;
			float cz = ((z0 + dK) * TQ + TQ / 2) * CELL;
			float best = 100000000.0;
			array<Vehicle> pvs = MUD_Wheels.GetAll();
			if (LEV_L8_VEH)
				pvs = m_aNearVeh;     // L8: only the vehicles near the window
			foreach (Vehicle v : pvs)
			{
				if (!v)
					continue;
				vector vo = v.GetOrigin();
				float dx = vo[0] - cx;
				float dz = vo[2] - cz;
				best = Math.Min(best, dx * dx + dz * dz);
			}
			dist[d] = best;
		}
		for (int n = 0; n < left; n++)     // left <= 0: none
		{
			int at = -1;
			for (int s = 0; s < TILES * TILES; s++)
			{
				if (dist[s] < 1000000000.0 && m_aPick[s] == 0 && (at < 0 || dist[s] < dist[at]))
					at = s;
			}
			if (at < 0)
				break;
			m_aPick[at] = 1;
			m_aPickWhy[at] = BW_NEAREST;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! WHEEL TILES FIRST (UploadPick): m_aPick 1 on every dirty zone slot (not yet picked) holding a wheel contact of a
	//! vehicle, at most WHEEL_SLOTS_MAX. Returns how many it picked.
	//! heldOnly (D1 fix): only slots held by SLOT_MIN_S (m_aPick 2) -> 3 (UploadPick turns them into 1)
	protected int PickWheelSlots(bool heldOnly)
	{
		int picked = 0;
		if (WHEEL_SLOTS_MAX <= 0)
			return 0;
		array<Vehicle> vs = MUD_Wheels.GetAll();
		if (LEV_L8_VEH)
			vs = m_aNearVeh;     // L8: near vehicles, their sims cached
		foreach (int vi, Vehicle v : vs)
		{
			if (!v)
				continue;
			VehicleWheeledSimulation sim;
			if (LEV_L8_VEH)
				sim = m_aNearSim[vi];
			else
				sim = VehicleWheeledSimulation.Cast(v.FindComponent(VehicleWheeledSimulation));
			if (!sim)
				continue;
			int count = sim.WheelCount();
			for (int w = 0; w < count; w++)
			{
				if (!sim.WheelHasContact(w))
					continue;
				vector cp = sim.WheelGetContactPosition(w);
				int i = Math.Round(cp[0] / CELL) - m_iX0;
				int k = Math.Round(cp[2] / CELL) - m_iZ0;
				if (i < 0 || i >= N || k < 0 || k >= N)
					continue;
				// ints first: Math.Min returns float (see DepthAt)
				int ti = Math.Min(i / TQ, TILES - 1);
				int tk = Math.Min(k / TQ, TILES - 1);
				int t = tk * TILES + ti;
				if (m_aSlotDirty[t] == 0)
					continue;
				if (heldOnly)
				{
					if (m_aPick[t] == 2)
					{
						m_aPick[t] = 3;
						picked++;
					}
					if (picked >= WHEEL_SLOTS_MAX)
						return picked;
					continue;
				}
				if (m_aPick[t] != 0)
					continue;
				m_aPick[t] = 1;
				picked++;
				m_iDlWheelPicks++;     // TEMP delay meter
				if (picked >= WHEEL_SLOTS_MAX)
					return picked;
			}
		}
		return picked;
	}

	//------------------------------------------------------------------------------------------------
	//! Cell whose mud something changed (thickness off THICK, texture dragged, or blasted): its quads draw as dirt
	protected bool Changed(MUD_MudSheetTileData d, int l)
	{
		return Math.AbsFloat(d.m_aD[l] - THICK) > TOUCH || d.m_aOu[l] != 0 || d.m_aOv[l] != 0 || d.m_aBare[l] != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! SOFT: quad la (world quad gi, gk) draws dirt when a corner is bare, or its mud changed more than TOUCH + a
	//! fixed random share of DIRT_NOISE, or its texture was dragged more than DRAG_TOUCH (ragged dirt edge)
	protected bool DirtQuad(MUD_MudSheetTileData d, int la, int gi, int gk)
	{
		// plain arithmetic hash (the compiler refused '&' here, 2026-10-15)
		int hsh = gi * 73856093 + gk * 19349663;
		hsh = hsh + hsh / 8192;
		int hm = hsh % 1024;
		if (hm < 0)
			hm += 1024;
		float nz = hm / 1023.0;
		float lim = TOUCH + DIRT_NOISE * nz;
		float dl2 = (DRAG_TOUCH + DRAG_TOUCH * nz) * (DRAG_TOUCH + DRAG_TOUCH * nz);
		for (int c = 0; c < 4; c++)
		{
			int l = la + (c % 2) + (c / 2) * TV;
			if (d.m_aBare[l] != 0)
				return true;
			if (Math.AbsFloat(d.m_aD[l] - THICK) > lim)
				return true;
			if (d.m_aOu[l] * d.m_aOu[l] + d.m_aOv[l] * d.m_aOv[l] > dl2)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Blown-off cell (crater) with less than EPS mud: not drawn
	protected bool Bare(MUD_MudSheetTileData d, int l)
	{
		return d.m_aBare[l] != 0 && d.m_aD[l] < EPS;
	}

	//------------------------------------------------------------------------------------------------
	//! TAPER across tiles: m_aTp[l] (tile vertex l) = cells (chamfer 1 / 1.4) to the nearest bare vertex of this tile
	//! (d) or a zone neighbour (live: m_F, else stored), over a TAPER_PAD padded field. False = no bare vertex in reach.
	protected bool TaperField(MUD_MudSheetTileData d, int tx, int tz)
	{
		// Stage 3 trim: rut stamps make no bare cells, so most builds have none in reach - skip the scan (~1/3 of a
		// stored build before)
		if (!NearBareTile(tx, tz))
			return false;
		int tT = System.GetTickCount();
		int w = TAPER_W;
		float tp[1089];         // TAPER_W x TAPER_W, local (faster than the member array)
		for (int p = 0; p < w * w; p++)
			tp[p] = 1000;
		bool any = false;
		for (int l = 0; l < TV * TV; l++)
		{
			if (!Bare(d, l))
				continue;
			int oli = l % TV;
			int olk = l / TV;
			tp[(olk + TAPER_PAD) * w + oli + TAPER_PAD] = 0;
			any = true;
		}
		for (int dz = -1; dz <= 1; dz++)
		{
			for (int dx = -1; dx <= 1; dx++)
			{
				if (dx == 0 && dz == 0)
					continue;
				int ntx = tx + dx;
				int ntz = tz + dz;
				if (!TileInZone(ntx, ntz))
					continue;
				bool live = IsLive(ntx, ntz);
				MUD_MudSheetTileData nd = null;
				if (!live)
				{
					MUD_MudSheetStore st = m_mTrail.Get(TrailKey(ntx, ntz));
					if (!st || !st.m_D)
						continue;
					nd = st.m_D;
				}
				// only the rows / columns that land in the padded field (TAPER_PAD + 1 deep for a side tile)
				int lk0 = Math.Max(0, -dz * TQ - TAPER_PAD);
				int lk1 = Math.Min(TQ, w - 1 - dz * TQ - TAPER_PAD);
				int li0 = Math.Max(0, -dx * TQ - TAPER_PAD);
				int li1 = Math.Min(TQ, w - 1 - dx * TQ - TAPER_PAD);
				for (int lk = lk0; lk <= lk1; lk++)
				{
					int pk = dz * TQ + lk + TAPER_PAD;
					for (int li = li0; li <= li1; li++)
					{
						int pi = dx * TQ + li + TAPER_PAD;
						bool bare;
						if (live)
						{
							int v = (ntz * TQ + lk - m_iZ0) * N + ntx * TQ + li - m_iX0;
							bare = m_F.m_aBare[v] != 0 && m_F.m_aD[v] < EPS;
						}
						else
							bare = Bare(nd, lk * TV + li);
						if (!bare)
							continue;
						tp[pk * w + pi] = 0;
						any = true;
					}
				}
			}
		}
		if (!any)
		{
			m_iTaperMs += System.GetTickCount(tT);
			return false;
		}
		for (int k = 0; k < w; k++)
		{
			for (int i = 0; i < w; i++)
			{
				int c = k * w + i;
				if (i > 0)
					tp[c] = Math.Min(tp[c], tp[c - 1] + 1);
				if (k > 0)
				{
					tp[c] = Math.Min(tp[c], tp[c - w] + 1);
					if (i > 0)
						tp[c] = Math.Min(tp[c], tp[c - w - 1] + 1.4);
					if (i < w - 1)
						tp[c] = Math.Min(tp[c], tp[c - w + 1] + 1.4);
				}
			}
		}
		for (int k2 = w - 1; k2 >= 0; k2--)
		{
			for (int i2 = w - 1; i2 >= 0; i2--)
			{
				int c2 = k2 * w + i2;
				if (i2 < w - 1)
					tp[c2] = Math.Min(tp[c2], tp[c2 + 1] + 1);
				if (k2 < w - 1)
				{
					tp[c2] = Math.Min(tp[c2], tp[c2 + w] + 1);
					if (i2 < w - 1)
						tp[c2] = Math.Min(tp[c2], tp[c2 + w + 1] + 1.4);
					if (i2 > 0)
						tp[c2] = Math.Min(tp[c2], tp[c2 + w - 1] + 1.4);
				}
			}
		}
		m_aTp.Resize(TV * TV);
		for (int l2 = 0; l2 < TV * TV; l2++)
		{
			int li2 = l2 % TV;
			int lk2 = l2 / TV;
			m_aTp[l2] = tp[(lk2 + TAPER_PAD) * w + li2 + TAPER_PAD];
		}
		m_iTaperMs += System.GetTickCount(tT);
		return true;
	}

	//! TAPER: mud share (0..1) at this many cells from bare ground; TAPER_CURVE = rounded shoulder (sine)
	protected float TaperFade(float cells)
	{
		float t = Math.Clamp(cells * CELL / TAPER, 0, 1);
		if (TAPER_CURVE)
			return Math.Sin(t * Math.PI * 0.5);
		return t;
	}

	//------------------------------------------------------------------------------------------------
	//! PAPER TEST 1 bench (TEST KIT): every BENCH_EVERY_S, BENCH_N old-path + BENCH_N fast-path builds of one touched live
	//! tile with a mesh, same data; 'same quads' checks that both paths draw the same quad count
	protected void BenchTick(float time)
	{
		if (!MUD_TestKit.TEST_KIT || BENCH_EVERY_S <= 0 || !m_bPlaced || !m_Ent)
			return;
		if (m_fBenchAt < 0)
		{
			m_fBenchAt = time + BENCH_EVERY_S;
			return;
		}
		if (time < m_fBenchAt)
			return;
		m_fBenchAt = time + BENCH_EVERY_S;
		int pick = -1;
		for (int t = 0; t < TILES * TILES; t++)
		{
			if (m_aSlotZone[t] == 0 || t >= m_aTileRes.Count() || !m_aTileRes[t] || !m_aTileEnt[t])
				continue;
			if (Touched(t % TILES, t / TILES))
			{
				pick = t;
				break;
			}
		}
		if (pick < 0)
			return;
		int ti = pick % TILES;
		int tk = pick / TILES;
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		Pack(ti, tk, m_TD);
		bool was = s_bFastBuild;
		Resource r;
		s_bFastBuild = false;
		int q0 = m_iBuildQuads;
		int tA = System.GetTickCount();
		for (int b = 0; b < BENCH_N; b++)
			r = BuildTileMesh(m_aTileEnt[pick], m_TD, x0 + ti, z0 + tk, true, false);
		int msOld = System.GetTickCount(tA);
		int qOld = m_iBuildQuads - q0;
		s_bFastBuild = true;
		q0 = m_iBuildQuads;
		tA = System.GetTickCount();
		for (int b2 = 0; b2 < BENCH_N; b2++)
			r = BuildTileMesh(m_aTileEnt[pick], m_TD, x0 + ti, z0 + tk, true, false);
		int msFast = System.GetTickCount(tA);
		int qFast = m_iBuildQuads - q0;
		s_bFastBuild = was;
		if (r)
			m_aTileRes[pick] = r;
		m_iBenchOldMs += msOld;
		m_iBenchFastMs += msFast;
		m_iBenchRuns++;
		float avgOld = m_iBenchOldMs / (BENCH_N * m_iBenchRuns * 1.0);
		float avgFast = m_iBenchFastMs / (BENCH_N * m_iBenchRuns * 1.0);
		int qPer = qOld / BENCH_N;
		Print(string.Format("MUD bench (PAPER TEST 1): %1 builds of tile %2 - old %3 ms, fast %4 ms, same quads %5 (%6) | all %7 runs: old %8 / fast %9 ms per build",
			BENCH_N, TrailKey(x0 + ti, z0 + tk), msOld, msFast, qOld == qFast, qPer, m_iBenchRuns, avgOld.ToString(-1, 3), avgFast.ToString(-1, 3)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! PAPER TEST 2: low-detail stored tiles the camera came near -> full rebuild (FlushOneBuild, budgeted)
	protected void LodUpgrade(vector cp)
	{
		if (!LOD_ON || m_iLowNow <= 0 || m_bLocal)
			return;     // MUD LAB part D: SHEET LOCAL far-ring tiles are never upgraded on screen
		float ur = LOD_M - LOD_BACK;
		foreach (int key : m_aTrailOrder)
		{
			MUD_MudSheetStore st = m_mTrail.Get(key);
			if (!st || !st.m_Ent || st.m_D.m_iLow == 0)
				continue;
			float ux = (st.m_iTx + 0.5) * TILE_M - cp[0];
			float uz = (st.m_iTz + 0.5) * TILE_M - cp[2];
			if (ux * ux + uz * uz > ur * ur)
				continue;
			st.m_D.m_iLow = 0;      // queued once; BuildTile sets it again if still far
			m_iLowNow--;
			QueueBuild(st.m_iTx, st.m_iTz);
			m_iLodUp++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One tile's mesh (world tile tx, tz; entity origin at its corner): every quad whose 4 corners have a ground
	//! height, vertex = ground + LIFT + d, faded under the ground near sides with no tile beyond; UV = world position
	//! (repeats every UV_TILES tiles) - drag. Returns the new mesh, or null (nothing drawn: entity hidden).
	//! live: a window tile (collider throttled to COLL_S); stored tiles always rebuild theirs
	protected Resource BuildTile(IEntity te, MUD_MudSheetTileData d, int tx, int tz, bool live)
	{
		if (!te)
		{
			m_iBuildWhy = BW_RESTORE;
			return null;
		}
		// Stage 3 log: builds and their ms (every tile mesh, live or stored, goes through here)
		int tB = System.GetTickCount();
		// PAPER TEST 2: stored tile far from the camera -> low-detail mesh
		bool low = false;
		if (LOD_ON && !m_bLocal && !live && m_bLodCam)
		{
			float lcx = (tx + 0.5) * TILE_M - m_vLodCam[0];
			float lcz = (tz + 0.5) * TILE_M - m_vLodCam[2];
			low = lcx * lcx + lcz * lcz > LOD_M * LOD_M;
		}
		if (m_bLocal && !live && m_bLodCam)
		{
			// MUD LAB part D: a stored tile past FAR_R from the camera is built at 11 x 11 (LodUpgrade never upgrades it)
			float fcx = (tx + 0.5) * TILE_M - m_vLodCam[0];
			float fcz = (tz + 0.5) * TILE_M - m_vLodCam[2];
			low = fcx * fcx + fcz * fcz > FAR_R * FAR_R;
		}
		m_iLowWas = d.m_iLow;     // stage 2 C3
		if (d.m_iLow != 0)
			m_iLowNow--;
		d.m_iLow = 0;
		if (low)
		{
			d.m_iLow = 1;
			m_iLodLow++;
			m_iLowNow++;
			if (m_bLocal)
				m_iFarLow++;
		}
		Resource res = BuildTileMesh(te, d, tx, tz, live, low);
		if (m_bSkipSame)
		{
			res = m_SkipRes;     // stage 2 C3: nothing moved 3 mm, same classes - the old mesh stays
			m_bSkipSame = false;
			m_SkipRes = null;
			m_iSkipSame++;
		}
		if (BAND_STORED_HOLD && !live)
			m_mStoredAt.Set(TrailKey(tx, tz), m_fLast);     // stage 2 C5
		if (m_bLocal && (EDGE_AUDIT || EDGE_COBUILD))
			EdgeRecord(d, tx, tz, res != null);
		if (m_iBuildWhy >= 0 && m_iBuildWhy < BW_N)
			m_aBuildsBy[m_iBuildWhy] = m_aBuildsBy[m_iBuildWhy] + 1;
		if (MUD_TestKit.TEST_KIT)
			MUD_CraterTrace.Built(tx, tz, m_iBuildWhy, res != null);     // stage 2 B1 crater log
		m_iBuildWhy = BW_RESTORE;
		int bms = System.GetTickCount(tB);
		m_iSt10Builds++;
		m_iSt10BuildMs += bms;
		if (bms > m_iSt10BuildMax)
			m_iSt10BuildMax = bms;
		// LAZY RUT MESH: untraced cells draw no quad - such a mesh does not cover the ground patches under it
		d.m_iDrawn = 0;
		if (res)
			d.m_iDrawn = 1;
		for (int l = 0; l < TV * TV && d.m_iDrawn != 0; l++)
		{
			if (d.m_aGs[l] == 0)
				d.m_iDrawn = 0;
		}
		if (LIFT_BY_DRAWN && PatchLiftOn())
		{
			int unNow = 0;
			if (res)
				unNow = 1;
			int unOld = d.m_iUnlift;
			int unSlot = -1;
			if (live && IsLive(tx, tz))
			{
				unSlot = (tz - m_iZ0 / TQ) * TILES + tx - m_iX0 / TQ;
				unOld = m_aSlotUnlift[unSlot];
				m_aSlotUnlift[unSlot] = unNow;
			}
			else
				d.m_iUnlift = unNow;
			if (unNow != unOld)
			{
				m_iLiftTold++;
				MUD_WorldGroundEntity.SheetLiftDirty(tx * TILE_M, tz * TILE_M);
			}
		}
		if (m_bLocal && EDGE_COBUILD && !m_bCoBuilding)
			CoCheck(tx, tz);     // stage 2 A3 (last: its builds are timed on their own)
		return res;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP GAP PROBE: keep this tile's border heights (world y, from vy) and compare each border with the facing one of
	//! the neighbour's last build (both traced) -> m_fGapEdgeMax / m_iGapEdgeOver
	protected void GapEdges(array<float> mine, int tx, int tz)
	{
		m_mGapEdge.Set(TrailKey(tx, tz), mine);
		float worst = 0;
		worst = Math.Max(worst, GapCmp(mine, 0, TrailKey(tx, tz - 1), TV));          // my south vs its north
		worst = Math.Max(worst, GapCmp(mine, TV, TrailKey(tx, tz + 1), 0));          // my north vs its south
		worst = Math.Max(worst, GapCmp(mine, 2 * TV, TrailKey(tx - 1, tz), 3 * TV)); // my west vs its east
		worst = Math.Max(worst, GapCmp(mine, 3 * TV, TrailKey(tx + 1, tz), 2 * TV)); // my east vs its west
		if (worst > m_fGapEdgeMax)
			m_fGapEdgeMax = worst;
		if (worst > 0.02)
			m_iGapEdgeOver++;
	}

	protected float GapCmp(array<float> mine, int mo, int nkey, int no)
	{
		array<float> other = m_mGapEdge.Get(nkey);
		if (!other)
			return 0;
		float w = 0;
		for (int j = 0; j < TV; j++)
		{
			if (mine[mo + j] < -9000 || other[no + j] < -9000)
				continue;
			w = Math.Max(w, Math.AbsFloat(mine[mo + j] - other[no + j]));
		}
		return w;
	}

	//------------------------------------------------------------------------------------------------
	//! low (PAPER TEST 2): every 2nd vertex, plain quads, own skirt (far stored tiles)
	//! Stage 2 C3 BUILD_SKIP_SAME (from BuildTileMesh): signature of the per-vertex classes (bare / hole, traced,
	//! band > 0, touched, untouched top), stored for the slot (live) or in d (stored). True = 'still' (no vert moved
	//! WOULD_SKIP_M) and the last mesh had the same signature and LOD and is still on te: m_SkipRes = that mesh.
	//! UV drag (m_aOu / m_aOv) is not compared except 'untouched top' (dirt UVs can lag up to one rebuild).
	protected bool SkipSame(IEntity te, MUD_MudSheetTileData d, int tx, int tz, bool live, bool low, bool still)
	{
		int sg1 = 1;
		int sg2 = 0;
		for (int l = 0; l < TV * TV; l++)
		{
			int c = 2 * d.m_aBare[l] + 8 * d.m_aGs[l] + 32 * d.m_aTouch[l];
			if (Bare(d, l))
				c += 1;
			if (d.m_aBand[l] > 0)
				c += 16;
			if (d.m_aD[l] == THICK && d.m_aOu[l] == 0 && d.m_aOv[l] == 0)
				c += 64;
			sg1 = sg1 * 31 + c;
			sg2 = sg2 + c * (l + 7);
		}
		sg1 = sg1 | 1;     // never 0 (0 = unknown)
		Resource old;
		int was1 = d.m_iSig1;
		int was2 = d.m_iSig2;
		d.m_iSig1 = sg1;
		d.m_iSig2 = sg2;
		if (live && IsLive(tx, tz))
		{
			int slot = (tz - m_iZ0 / TQ) * TILES + tx - m_iX0 / TQ;
			was1 = m_aSlotSig1[slot];
			was2 = m_aSlotSig2[slot];
			m_aSlotSig1[slot] = sg1;
			m_aSlotSig2[slot] = sg2;
			if (slot < m_aTileRes.Count() && m_aTileEnt[slot] == te)
				old = m_aTileRes[slot];
		}
		else if (!live)
		{
			MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
			if (st && st.m_Ent == te)
				old = st.m_Res;
		}
		int lowNow = 0;
		if (low)
			lowNow = 1;
		if (!still || !old || was1 == 0 || was1 != sg1 || was2 != sg2 || lowNow != m_iLowWas)
			return false;
		m_bSkipSame = true;
		m_SkipRes = old;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected Resource BuildTileMesh(IEntity te, MUD_MudSheetTileData d, int tx, int tz, bool live, bool low)
	{
		bool fast = s_bFastBuild;
		bool fW = !HasTile(tx - 1, tz);
		bool fE = !HasTile(tx + 1, tz);
		bool fS = !HasTile(tx, tz - 1);
		bool fN = !HasTile(tx, tz + 1);
		float orgY = te.GetOrigin()[1];
		int tPart = System.GetTickCount();
		int um = ((tx % UV_TILES) + UV_TILES) % UV_TILES;
		int vm = ((tz % UV_TILES) + UV_TILES) % UV_TILES;
		// one fresh mesh per tile every time: a flags-0 MeshObject never redraws later UpdateVerts (KB
		// runtime-procedural-mesh.md section 2); MeshObject.Create costs ~0 (mud-gotchas 2026-10-09)
		// local static arrays (the member holder MUD_MudSheetTile crashed in UpdateVerts twice)
		// locals of one method must fit the 80 KB VM stack (compile error 2026-10-15 at 1280 / 4080 / 3600 sizes)
		vector tv[1024];        // EDGE_VCAP: 441 grid + SMOOTH_EDGE cuts + 320 ONE_SKIRT (80 border quads x 4 own verts)
		vector cvv[441];        // collider, by tile vertex l (ONE_GRID: ground, no mud; else the drawn height)
		float tuv[2048];
		int tix[3360];          // dirt (MAT) tris: EDGE_ICAP + 80 skirt quads
		int tixTop[2880];       // untouched tris (MAT_TOP, TOP_LOOK): EDGE_ICAP
		int cix[2400];          // collider: the plain quads (a c d / a d b), not cut, no skirt
		int nc = 0;
		// SMOOTH_EDGE: cut vertex on each tile edge (index = lower end vertex): horizontal, vertical (the diagonal
		// a-d is inside one quad: eDq). The dirt score per vertex goes into bd (TAPER is done with it by then).
		int eH[441];
		int eV[441];
		for (int em = 0; em < TV * TV; em++)
		{
			eH[em] = -1;
			eV[em] = -1;
		}
		int tmap[441];
		for (int m = 0; m < TV * TV; m++)
		{
			tmap[m] = -1;
			// int parts first: '(int % int) * float' inside a float expression is refused ("Unknown operator '%'")
			if (!COLLIDE)
				continue;       // D2: no collider, no collider verts
			int cmi = m % TV;
			int cmk = m / TV;
			cvv[m] = Vector(cmi * CELL, d.m_aG[m] - orgY, cmk * CELL);   // unused ones still sane (body bounds)
		}
		int nv = 0;
		int ngrid = 0;          // grid verts added so far (the cut cap keeps room for the rest + the skirt)
		int ni = 0;
		int niTop = 0;
		// TAPER: distance (cells) from each vertex to the nearest Bare vertex, across tile borders (TaperField)
		float bd[441];
		bool anyBare = TAPER > 0 && TaperField(d, tx, tz);
		for (int bl = 0; bl < TV * TV; bl++)
		{
			bd[bl] = 1000;
			if (anyBare)
				bd[bl] = m_aTp[bl];
		}
		// vertex heights (world y): mud on the ground, faded at open tile sides (EDGE) and toward bare cells (TAPER)
		float vy[441];
		int bareV[441];         // PAPER TEST 1: Bare() per vertex, read once
		for (int yk = 0; yk < TV; yk++)
		{
			for (int yi = 0; yi < TV; yi++)
			{
				int yl = yk * TV + yi;
				bool yBare;
				if (fast)
					yBare = d.m_aBare[yl] != 0 && d.m_aD[yl] < EPS;     // Bare() inline
				else
					yBare = Bare(d, yl);
				bareV[yl] = 0;
				if (yBare)
					bareV[yl] = 1;
				float fade = 1;         // no open side: e stays 1000 -> fade 1
				if (!fast || fW || fE || fS || fN)
				{
					int e = 1000;
					if (fW)
						e = Math.Min(e, yi);
					if (fE)
						e = Math.Min(e, TQ - yi);
					if (fS)
						e = Math.Min(e, yk);
					if (fN)
						e = Math.Min(e, TQ - yk);
					fade = Math.Clamp(e * CELL / EDGE, 0, 1);
				}
				if (ONE_GRID)
				{
					// the sheet is the ground: bare = ground height; only the side toward ground not ours dips
					float fadeB = fade;
					if (anyBare && TAPER > 0)
						fadeB = Math.Min(fade, TaperFade(bd[yl]));
					vy[yl] = d.m_aG[yl] - ONE_DIP * (1 - fade) + (LIFT + d.m_aD[yl]) * fadeB;
					if (yBare)
						vy[yl] = d.m_aG[yl] - ONE_DIP * (1 - fade);
					continue;
				}
				if (anyBare && TAPER > 0)
					fade = Math.Min(fade, TaperFade(bd[yl]));
				if (m_bLocal)
				{
					// SHEET LOCAL deposit band: h = G + band * (m + LIFT) - (1 - band) * BAND_UNDER (no EDGE fade)
					vy[yl] = d.m_aG[yl] - BAND_UNDER + (BAND_UNDER + LIFT + d.m_aD[yl]) * fade * d.m_aBand[yl];
					if (yBare)
						vy[yl] = d.m_aG[yl] - SINK;
					continue;
				}
				vy[yl] = d.m_aG[yl] - SINK + (SINK + LIFT + d.m_aD[yl]) * fade;
				if (yBare)
					vy[yl] = d.m_aG[yl] - SINK;     // the mesh dips under the ground toward a blown-off patch
			}
		}
		// SHEET LOCAL 'would skip' meter: the biggest drawn height change since this tile's last build (own block:
		// BuildTileMesh is near the 64-locals limit)
		if (m_bLocal)
		{
			float lyMax = 0;
			for (int lyl = 0; lyl < TV * TV; lyl++)
			{
				float lyd = Math.AbsFloat(vy[lyl] - d.m_aLastY[lyl]);
				if (lyd > lyMax)
					lyMax = lyd;
				if (!BUILD_SKIP_SAME)
					d.m_aLastY[lyl] = vy[lyl];
			}
			if (lyMax < WOULD_SKIP_M)
				m_iWouldSkip++;
			// stage 2 C3: same classes and no vert moved 3 mm -> the old mesh stays (BuildTile takes m_SkipRes)
			if (BUILD_SKIP_SAME && SkipSame(te, d, tx, tz, live, low, lyMax < WOULD_SKIP_M))
				return null;
			if (BUILD_SKIP_SAME)
			{
				for (int lyw = 0; lyw < TV * TV; lyw++)
					d.m_aLastY[lyw] = vy[lyw];     // drawn heights of the mesh built now
			}
		}
		// TEMP GAP PROBE: border heights S N W E (world y; -10000 = untraced). Own block: BuildTileMesh is at the
		// 64-locals limit (mud-gotchas 2026-10-18)
		if (GAP_PROBE)
		{
			array<float> gapB = {};
			gapB.Resize(4 * TV);
			for (int gj = 0; gj < TV; gj++)
			{
				int gl0 = gj;
				int gl1 = TQ * TV + gj;
				int gl2 = gj * TV;
				int gl3 = gj * TV + TQ;
				gapB[gj] = -10000;
				gapB[TV + gj] = -10000;
				gapB[2 * TV + gj] = -10000;
				gapB[3 * TV + gj] = -10000;
				if (d.m_aGs[gl0] != 0)
					gapB[gj] = vy[gl0];
				if (d.m_aGs[gl1] != 0)
					gapB[TV + gj] = vy[gl1];
				if (d.m_aGs[gl2] != 0)
					gapB[2 * TV + gj] = vy[gl2];
				if (d.m_aGs[gl3] != 0)
					gapB[3 * TV + gj] = vy[gl3];
			}
			GapEdges(gapB, tx, tz);
		}
		m_iGapUntNow = 0;
		bool smooth = TOP_LOOK && SOFT && SMOOTH_EDGE;
		if (smooth)
		{
			for (int ql = 0; ql < TV * TV; ql++)
			{
				if (fast && d.m_aD[ql] == THICK && d.m_aOu[ql] == 0 && d.m_aOv[ql] == 0 && d.m_aBare[ql] == 0)
				{
					bd[ql] = 0;     // PAPER TEST 1: untouched cell - the maths below gives 0 too
					continue;
				}
				int qhs = (tx * TQ + ql % TV) * 73856093 + (tz * TQ + ql / TV) * 19349663;
				qhs = qhs + qhs / 8192;
				int qhm = qhs % 1024;
				if (qhm < 0)
					qhm += 1024;
				float qnz = qhm / 1023.0;
				float qs = Math.AbsFloat(d.m_aD[ql] - THICK) / (TOUCH + DIRT_NOISE * qnz);
				float qdr = Math.Sqrt(d.m_aOu[ql] * d.m_aOu[ql] + d.m_aOv[ql] * d.m_aOv[ql]) / (DRAG_TOUCH * (1 + qnz));
				if (qdr > qs)
					qs = qdr;
				if (d.m_aBare[ql] != 0 && qs < 3)
					qs = 3;
				bd[ql] = qs;
			}
		}
		MUD_Prof.Add(MUD_Prof.T_SETUP, System.GetTickCount(tPart));
		tPart = System.GetTickCount();
		if (low)
		{
			// PAPER TEST 2: 0.3 m quads over every 2nd vertex; dirt when any of the 3 x 3 vertices under it scores dirt
			for (int lwk = 0; lwk < TQ; lwk += 2)
			{
				for (int lwi = 0; lwi < TQ; lwi += 2)
				{
					int lwa0 = lwk * TV + lwi;
					int lwb0 = lwa0 + 2;
					int lwc0 = lwa0 + 2 * TV;
					int lwd0 = lwc0 + 2;
					if (d.m_aGs[lwa0] == 0 || d.m_aGs[lwb0] == 0 || d.m_aGs[lwc0] == 0 || d.m_aGs[lwd0] == 0)
						continue;
					if (m_bLocal && d.m_aBand[lwa0] <= 0 && d.m_aBand[lwb0] <= 0 && d.m_aBand[lwc0] <= 0 && d.m_aBand[lwd0] <= 0)
						continue;   // SHEET LOCAL: outside the deposit band (under the ground)
					if (ONE_GRID && (d.m_aBare[lwa0] == BARE_VOX || d.m_aBare[lwb0] == BARE_VOX || d.m_aBare[lwc0] == BARE_VOX || d.m_aBare[lwd0] == BARE_VOX))
						continue;
					if (!ONE_GRID && bareV[lwa0] != 0 && bareV[lwb0] != 0 && bareV[lwc0] != 0 && bareV[lwd0] != 0)
						continue;
					if (!ONE_GRID && HOLE_SKIP > 0 && (bareV[lwa0] != 0 || bareV[lwb0] != 0 || bareV[lwc0] != 0 || bareV[lwd0] != 0))
					{
						float lwlo = Math.Min(Math.Min(vy[lwa0], vy[lwb0]), Math.Min(vy[lwc0], vy[lwd0]));
						float lwhi = Math.Max(Math.Max(vy[lwa0], vy[lwb0]), Math.Max(vy[lwc0], vy[lwd0]));
						if (lwhi - lwlo > HOLE_SKIP)
							continue;
					}
					for (int lwn = 0; lwn < 4; lwn++)
					{
						int lwv = lwa0;
						if (lwn == 1)
							lwv = lwb0;
						else if (lwn == 2)
							lwv = lwc0;
						else if (lwn == 3)
							lwv = lwd0;
						if (tmap[lwv] >= 0)
							continue;
						int lwci = lwv % TV;    // own int lines: '%' inside a float expression does not compile
						int lwck = lwv / TV;
						tmap[lwv] = nv;
						tv[nv] = Vector(lwci * CELL, vy[lwv] - orgY, lwck * CELL);
						tuv[nv * 2] = Math.Clamp(((um * TQ + lwci) * CELL - d.m_aOu[lwv]) / SIZE, 0, 1);
						tuv[nv * 2 + 1] = 1 - Math.Clamp(((vm * TQ + lwck) * CELL - d.m_aOv[lwv]) / SIZE, 0, 1);
						nv++;
					}
					bool lwDirt = false;
					if (smooth)
					{
						for (int lwz = 0; lwz <= 2 && !lwDirt; lwz++)
						{
							for (int lwx = 0; lwx <= 2; lwx++)
							{
								if (bd[lwa0 + lwz * TV + lwx] > 1)
								{
									lwDirt = true;
									break;
								}
							}
						}
					}
					else
						lwDirt = DirtQuad(d, lwa0, tx * TQ + lwi, tz * TQ + lwk);
					int lwa = tmap[lwa0];
					int lwb = tmap[lwb0];
					int lwc = tmap[lwc0];
					int lwd = tmap[lwd0];
					if (TOP_LOOK && !lwDirt)
					{
						tixTop[niTop] = lwa;
						tixTop[niTop + 1] = lwc;
						tixTop[niTop + 2] = lwd;
						tixTop[niTop + 3] = lwa;
						tixTop[niTop + 4] = lwd;
						tixTop[niTop + 5] = lwb;
						niTop += 6;
					}
					else
					{
						tix[ni] = lwa;
						tix[ni + 1] = lwc;
						tix[ni + 2] = lwd;
						tix[ni + 3] = lwa;
						tix[ni + 4] = lwd;
						tix[ni + 5] = lwb;
						ni += 6;
					}
				}
			}
			// own skirt over the 2-vertex border steps (same faces as the full one below)
			m_iSkirtFrom = nv;     // stage 2 A4
			m_iSkirtTix = ni;      // stage 2 A7
			if ((ONE_GRID || SKIRT_ALL) && ONE_SKIRT > 0 && ni + niTop > 0)
			{
				for (int lsd = 0; lsd < 4; lsd++)
				{
					for (int lsq = 0; lsq < TQ; lsq += 2)
					{
						int ls0;
						int ls1;
						if (lsd == 0)
						{
							ls0 = lsq;
							ls1 = lsq + 2;
						}
						else if (lsd == 1)
						{
							ls0 = TQ * TV + lsq + 2;
							ls1 = TQ * TV + lsq;
						}
						else if (lsd == 2)
						{
							ls0 = (lsq + 2) * TV;
							ls1 = lsq * TV;
						}
						else
						{
							ls0 = lsq * TV + TQ;
							ls1 = (lsq + 2) * TV + TQ;
						}
						int lm0 = tmap[ls0];
						int lm1 = tmap[ls1];
						if (lm0 < 0 || lm1 < 0)
							continue;
						if (d.m_aBare[ls0] == BARE_VOX || d.m_aBare[ls1] == BARE_VOX)
							continue;
						if (nv + 4 > EDGE_VCAP || ni + 6 > 3360)
							continue;
						tv[nv] = tv[lm0];
						tv[nv + 1] = tv[lm1];
						tv[nv + 2] = tv[lm1] - Vector(0, ONE_SKIRT, 0);
						tv[nv + 3] = tv[lm0] - Vector(0, ONE_SKIRT, 0);
						for (int lsu = 0; lsu < 2; lsu++)
						{
							int lsm = lm0;
							if (lsu == 1)
								lsm = lm1;
							tuv[(nv + lsu) * 2] = tuv[lsm * 2];
							tuv[(nv + lsu) * 2 + 1] = tuv[lsm * 2 + 1];
							tuv[(nv + 3 - lsu) * 2] = tuv[lsm * 2];
							tuv[(nv + 3 - lsu) * 2 + 1] = tuv[lsm * 2 + 1];
						}
						tix[ni] = nv;
						tix[ni + 1] = nv + 1;
						tix[ni + 2] = nv + 2;
						tix[ni + 3] = nv;
						tix[ni + 4] = nv + 2;
						tix[ni + 5] = nv + 3;
						ni += 6;
						nv += 4;
					}
				}
			}
		}
		for (int qk = 0; qk < TQ && !low; qk++)
		{
			for (int qi = 0; qi < TQ; qi++)
			{
				int la = qk * TV + qi;
				if (d.m_aGs[la] == 0 || d.m_aGs[la + 1] == 0 || d.m_aGs[la + TV] == 0 || d.m_aGs[la + TV + 1] == 0)
				{
					m_iGapUntNow++;     // TEMP GAP PROBE
					continue;
				}
				if (m_bLocal && d.m_aBand[la] <= 0 && d.m_aBand[la + 1] <= 0 && d.m_aBand[la + TV] <= 0 && d.m_aBand[la + TV + 1] <= 0)
					continue;   // SHEET LOCAL: outside the deposit band (under the ground)
				if (ONE_GRID && (d.m_aBare[la] == BARE_VOX || d.m_aBare[la + 1] == BARE_VOX || d.m_aBare[la + TV] == BARE_VOX || d.m_aBare[la + TV + 1] == BARE_VOX))
					continue;   // VOXEL HOLES: the voxel ground draws and collides here
				if (!ONE_GRID && !fast && Bare(d, la) && Bare(d, la + 1) && Bare(d, la + TV) && Bare(d, la + TV + 1))
					continue;   // blown off: the voxel ground shows
				if (!ONE_GRID && fast && bareV[la] != 0 && bareV[la + 1] != 0 && bareV[la + TV] != 0 && bareV[la + TV + 1] != 0)
					continue;   // same, PAPER TEST 1 flags
				bool qAnyBare;
				if (fast)
					qAnyBare = bareV[la] != 0 || bareV[la + 1] != 0 || bareV[la + TV] != 0 || bareV[la + TV + 1] != 0;
				else
					qAnyBare = Bare(d, la) || Bare(d, la + 1) || Bare(d, la + TV) || Bare(d, la + TV + 1);
				if (!ONE_GRID && HOLE_SKIP > 0 && qAnyBare)
				{
					float qlo = Math.Min(Math.Min(vy[la], vy[la + 1]), Math.Min(vy[la + TV], vy[la + TV + 1]));
					float qhi = Math.Max(Math.Max(vy[la], vy[la + 1]), Math.Max(vy[la + TV], vy[la + TV + 1]));
					if (qhi - qlo > HOLE_SKIP)
						continue;   // HOLE_SKIP: would stretch down into a dug hole
				}
				// corners 0 a (i, k), 1 b (i+1, k), 2 c (i, k+1), 3 d (i+1, k+1); only used vertices go in (tiles
				// with unused ones crashed in UpdateVerts 3x - inference)
				for (int cn = 0; cn < 4; cn++)
				{
					int ci = qi + cn % 2;
					int ck = qk + cn / 2;
					int lv = ck * TV + ci;
					if (tmap[lv] >= 0)
						continue;
					float y = vy[lv];
					tmap[lv] = nv;
					tv[nv] = Vector(ci * CELL, y - orgY, ck * CELL);
					if (COLLIDE && ONE_GRID)
						cvv[lv] = Vector(ci * CELL, d.m_aG[lv] - orgY, ck * CELL);
					else if (COLLIDE)
						cvv[lv] = Vector(ci * CELL, y - orgY, ck * CELL);
					// texture drag: the vertex shows the texture from where its mud came from (UVs clamp to 0..1)
					tuv[nv * 2] = Math.Clamp(((um * TQ + ci) * CELL - d.m_aOu[lv]) / SIZE, 0, 1);
					tuv[nv * 2 + 1] = 1 - Math.Clamp(((vm * TQ + ck) * CELL - d.m_aOv[lv]) / SIZE, 0, 1);   // 1 - v as the voxel UVs (MERGE A)
					nv++;
					ngrid++;
				}
				int a = tmap[qk * TV + qi];
				int b = tmap[qk * TV + qi + 1];
				int c = tmap[(qk + 1) * TV + qi];
				int d2 = tmap[(qk + 1) * TV + qi + 1];
				// a c d / a d b: upward (as MUD_VoxelProbe.Quad + Tri)
				if (COLLIDE)
				{
					cix[nc] = la;           // by tile vertex (cvv), not by mesh vertex
					cix[nc + 1] = la + TV;
					cix[nc + 2] = la + TV + 1;
					cix[nc + 3] = la;
					cix[nc + 4] = la + TV + 1;
					cix[nc + 5] = la + 1;
					nc += 6;
				}
				// D2: smooth sets clean from bd below - the per-quad Changed / DirtQuad calls were thrown away then
				bool clean = false;
				if (!smooth && SOFT)
					clean = !DirtQuad(d, la, tx * TQ + qi, tz * TQ + qk);
				else if (!smooth)
					clean = !Changed(d, la) && !Changed(d, la + 1) && !Changed(d, la + TV) && !Changed(d, la + TV + 1);
				if (smooth)
				{
					int dc = 0;
					if (bd[la] > 1)
						dc++;
					if (bd[la + 1] > 1)
						dc++;
					if (bd[la + TV] > 1)
						dc++;
					if (bd[la + TV + 1] > 1)
						dc++;
					clean = dc == 0;
					// mixed quad, room left (cuts + the grid verts still to come + the skirt's 320 verts): cut it along
					// q = 1 (without the grid reserve a mud-zone drive overflowed tv: 'Index out of range 1024', 17:0x)
					if (dc > 0 && dc < 4 && nv + 6 + (TV * TV - ngrid) + 320 <= EDGE_VCAP && ni + 12 <= EDGE_ICAP && niTop + 12 <= EDGE_ICAP)
					{
						int eDq = -1;
						for (int tr = 0; tr < 2; tr++)
						{
							int tl[3];
							tl[0] = la;
							tl[1] = la + TV;
							tl[2] = la + TV + 1;
							if (tr == 1)
							{
								tl[1] = la + TV + 1;
								tl[2] = la + 1;
							}
							int pd[4];
							int pt[4];
							int npd = 0;
							int npt = 0;
							for (int ej = 0; ej < 3; ej++)
							{
								int el0 = tl[ej];
								int el1 = tl[(ej + 1) % 3];
								bool ein0 = bd[el0] > 1;
								bool ein1 = bd[el1] > 1;
								if (ein0)
								{
									pd[npd] = tmap[el0];
									npd++;
								}
								else
								{
									pt[npt] = tmap[el0];
									npt++;
								}
								if (ein0 == ein1)
									continue;
								int elo = el0;
								int ehi = el1;
								if (el1 < el0)
								{
									elo = el1;
									ehi = el0;
								}
								int espan = ehi - elo;
								int xv;
								if (espan == 1)
									xv = eH[elo];
								else if (espan == TV)
									xv = eV[elo];
								else
									xv = eDq;
								if (xv < 0)
								{
									float eft = (1 - bd[elo]) / (bd[ehi] - bd[elo]);
									int mlo = tmap[elo];
									int mhi = tmap[ehi];
									tv[nv] = tv[mlo] + (tv[mhi] - tv[mlo]) * eft;
									tuv[nv * 2] = tuv[mlo * 2] + (tuv[mhi * 2] - tuv[mlo * 2]) * eft;
									tuv[nv * 2 + 1] = tuv[mlo * 2 + 1] + (tuv[mhi * 2 + 1] - tuv[mlo * 2 + 1]) * eft;
									xv = nv;
									nv++;
									if (espan == 1)
										eH[elo] = xv;
									else if (espan == TV)
										eV[elo] = xv;
									else
										eDq = xv;
								}
								pd[npd] = xv;
								npd++;
								pt[npt] = xv;
								npt++;
							}
							// fans keep the triangle's turn (walk order)
							for (int fd = 1; fd + 1 < npd; fd++)
							{
								tix[ni] = pd[0];
								tix[ni + 1] = pd[fd];
								tix[ni + 2] = pd[fd + 1];
								ni += 3;
							}
							for (int ftp = 1; ftp + 1 < npt; ftp++)
							{
								tixTop[niTop] = pt[0];
								tixTop[niTop + 1] = pt[ftp];
								tixTop[niTop + 2] = pt[ftp + 1];
								niTop += 3;
							}
						}
						continue;
					}
				}
				if (TOP_LOOK && clean)
				{
					tixTop[niTop] = a;
					tixTop[niTop + 1] = c;
					tixTop[niTop + 2] = d2;
					tixTop[niTop + 3] = a;
					tixTop[niTop + 4] = d2;
					tixTop[niTop + 5] = b;
					niTop += 6;
					continue;
				}
				tix[ni] = a;
				tix[ni + 1] = c;
				tix[ni + 2] = d2;
				tix[ni + 3] = a;
				tix[ni + 4] = d2;
				tix[ni + 5] = b;
				ni += 6;
			}
		}
		if (live)
		{
			m_iGapLiveBuilds++;
			if (m_iGapUntNow > 0)
				m_iGapBuildsUnt++;
			m_iGapUntraced += m_iGapUntNow;
			if (m_iGapUntNow > m_iGapUntracedMax)
				m_iGapUntracedMax = m_iGapUntNow;
		}
		MUD_Prof.Add(MUD_Prof.T_QUADS, System.GetTickCount(tPart));
		tPart = System.GetTickCount();
		int nvColl = nv;        // collider: tile verts / indices only, not the skirt
		int niColl = ni;
		if (!low)
		{
			m_iSkirtFrom = nv;     // stage 2 A4
			m_iSkirtTix = ni;      // stage 2 A7
		}
		if (!low && (ONE_GRID || SKIRT_ALL) && ONE_SKIRT > 0 && ni + niTop > 0)
		{
			// side 0 south (k 0), 1 north (k TQ), 2 west (i 0), 3 east (i TQ); T0 -> T1 runs so that the quad faces
			// out ((T1 - T0) x down = outward, as the up quads' a c d order)
			for (int sd = 0; sd < 4; sd++)
			{
				for (int sq = 0; sq < TQ; sq++)
				{
					int s0;
					int s1;
					if (sd == 0)
					{
						s0 = sq;
						s1 = sq + 1;
					}
					else if (sd == 1)
					{
						s0 = TQ * TV + sq + 1;
						s1 = TQ * TV + sq;
					}
					else if (sd == 2)
					{
						s0 = (sq + 1) * TV;
						s1 = sq * TV;
					}
					else
					{
						s0 = sq * TV + TQ;
						s1 = (sq + 1) * TV + TQ;
					}
					int m0 = tmap[s0];
					int m1 = tmap[s1];
					if (m0 < 0 || m1 < 0)
						continue;
					// VOXEL HOLES: no curtain into a tunnel mouth (its quad is not drawn, the voxels are)
					if (d.m_aBare[s0] == BARE_VOX || d.m_aBare[s1] == BARE_VOX)
						continue;
					if (nv + 4 > EDGE_VCAP || ni + 6 > 3360)
						continue;       // safety: never past tv / tix
					// own top copies: the border verts keep their up normals
					tv[nv] = tv[m0];
					tv[nv + 1] = tv[m1];
					tv[nv + 2] = tv[m1] - Vector(0, ONE_SKIRT, 0);
					tv[nv + 3] = tv[m0] - Vector(0, ONE_SKIRT, 0);
					for (int su = 0; su < 2; su++)
					{
						int sm = m0;
						if (su == 1)
							sm = m1;
						tuv[(nv + su) * 2] = tuv[sm * 2];
						tuv[(nv + su) * 2 + 1] = tuv[sm * 2 + 1];
						tuv[(nv + 3 - su) * 2] = tuv[sm * 2];
						tuv[(nv + 3 - su) * 2 + 1] = tuv[sm * 2 + 1];
					}
					tix[ni] = nv;
					tix[ni + 1] = nv + 1;
					tix[ni + 2] = nv + 2;
					tix[ni + 3] = nv;
					tix[ni + 4] = nv + 2;
					tix[ni + 5] = nv + 3;
					ni += 6;
					nv += 4;
				}
			}
		}
		// stage 2 A4 SKIRT_APRON (sheet local): each skirt quad (top0, top1, bottom1, bottom0 from m_iSkirtFrom) gets its
		// bottom verts moved out from the tile side by the drop (~45 deg) and down to G - BAND_UNDER - 0.02, so the
		// curtain lies on the ground instead of standing as a wall. Their dirt UVs come from their own position (the top
		// vert's UV + the offset / SIZE, same frame, its drag kept); TOP_UV64 recomputes all from xz anyway. Own block:
		// BuildTileMesh is at the 64-locals limit.
		if (m_bLocal && SKIRT_APRON && !EDGE_DRAPE)
		{
			for (int ab = m_iSkirtFrom; ab + 3 < nv; ab += 4)
			{
				// outward = (T1 - T0) turned to the right: south -z, north +z, west -x, east +x (see the skirt side order)
				vector aOut = Vector(tv[ab + 1][2] - tv[ab][2], 0, tv[ab][0] - tv[ab + 1][0]);
				aOut = aOut * (1 / aOut.Length());
				for (int au = 0; au < 2; au++)
				{
					vector aTop = tv[ab + au];
					int al = Math.Round(aTop[2] / CELL) * TV + Math.Round(aTop[0] / CELL);
					float aBy = d.m_aG[al] - BAND_UNDER - 0.02 - orgY;
					float aDrop = Math.Max(0, aTop[1] - aBy);
					tv[ab + 3 - au] = Vector(aTop[0] + aOut[0] * aDrop, aBy, aTop[2] + aOut[2] * aDrop);
					tuv[(ab + 3 - au) * 2] = Math.Clamp(tuv[(ab + au) * 2] + aOut[0] * aDrop / SIZE, 0, 1);
					tuv[(ab + 3 - au) * 2 + 1] = Math.Clamp(tuv[(ab + au) * 2 + 1] - aOut[2] * aDrop / SIZE, 0, 1);
				}
			}
		}
		// stage 2 A7 EDGE_DRAPE (sheet local, user idea 2026-10-20): each skirt quad (top0, top1, bottom1, bottom0) becomes
		// a quarter round of radius R = top height over G - BAND_UNDER - 0.02: it leaves the top flat, a middle ring at
		// 45 deg (R * 0.707 out, R * 0.293 down; 2 new verts) and ends straight down R out. Two quads instead of one;
		// stops at the vert / index caps (m_iDrapeCut). Own block: BuildTileMesh is at the 64-locals limit.
		if (m_bLocal && EDGE_DRAPE)
		{
			int dn = (nv - m_iSkirtFrom) / 4;
			for (int dq = 0; dq < dn; dq++)
			{
				if (nv + 2 > EDGE_VCAP || ni + 6 > 3360)
				{
					m_iDrapeCut++;
					break;
				}
				int db = m_iSkirtFrom + dq * 4;
				// outward = (T1 - T0) turned to the right (as A4)
				vector dOut = Vector(tv[db + 1][2] - tv[db][2], 0, tv[db][0] - tv[db + 1][0]);
				dOut = dOut * (1 / dOut.Length());
				for (int du = 0; du < 2; du++)
				{
					vector dTop = tv[db + du];
					int dl = Math.Round(dTop[2] / CELL) * TV + Math.Round(dTop[0] / CELL);
					float dR = Math.Max(0, dTop[1] - (d.m_aG[dl] - BAND_UNDER - 0.02 - orgY));
					tv[nv + du] = Vector(dTop[0] + dOut[0] * dR * 0.7071, dTop[1] - dR * 0.2929, dTop[2] + dOut[2] * dR * 0.7071);
					tv[db + 3 - du] = Vector(dTop[0] + dOut[0] * dR, dTop[1] - dR, dTop[2] + dOut[2] * dR);
					tuv[(nv + du) * 2] = Math.Clamp(tuv[(db + du) * 2] + dOut[0] * dR * 0.7071 / SIZE, 0, 1);
					tuv[(nv + du) * 2 + 1] = Math.Clamp(tuv[(db + du) * 2 + 1] - dOut[2] * dR * 0.7071 / SIZE, 0, 1);
					tuv[(db + 3 - du) * 2] = Math.Clamp(tuv[(db + du) * 2] + dOut[0] * dR / SIZE, 0, 1);
					tuv[(db + 3 - du) * 2 + 1] = Math.Clamp(tuv[(db + du) * 2 + 1] - dOut[2] * dR / SIZE, 0, 1);
				}
				// upper quad top0 top1 mid1 mid0 over the old skirt tris, lower quad mid0 mid1 bottom1 bottom0 appended
				tix[m_iSkirtTix + dq * 6 + 2] = nv + 1;
				tix[m_iSkirtTix + dq * 6 + 4] = nv + 1;
				tix[m_iSkirtTix + dq * 6 + 5] = nv;
				tix[ni] = nv;
				tix[ni + 1] = nv + 1;
				tix[ni + 2] = db + 2;
				tix[ni + 3] = nv;
				tix[ni + 4] = db + 2;
				tix[ni + 5] = db + 3;
				ni += 6;
				nv += 2;
			}
		}
		MUD_Prof.Add(MUD_Prof.T_SKIRT, System.GetTickCount(tPart));
		tPart = System.GetTickCount();
		if (ni + niTop == 0)
		{
			// nothing to draw: hide (no SetObject(null) - the 21:42 crash came right after 16 of those; inference)
			te.ClearFlags(EntityFlags.VISIBLE, false);
			DropColl(te);
			return null;
		}
		m_iBuildQuads += (ni + niTop) / 6;
		te.SetFlags(EntityFlags.VISIBLE, false);
		// counts built at the call, as MUD_VoxelProbe (bricks draw). IDA 2026-10-14: the old 'int nvs[1]' filled
		// earlier reached Create as 6 vertices while the indices went to 440 -> UpdateVerts' normal pass smashed its
		// stack buffer (mud-gotchas)
		// two sections share one vertex list, as MUD_VoxelProbe.MakeRes (bricks: MAT_TOP + MAT_DIRT, proven)
		Resource res;
		if (ni > 0 && niTop > 0)
		{
			int nvs2[] = {nv, nv};
			int nis2[] = {niTop, ni};
			string mats2[] = {MAT_TOP, MAT};
			res = MeshObject.Create(2, nvs2, nis2, mats2, 0);
		}
		else
		{
			int nvs[] = {nv};
			int nis[] = {ni + niTop};
			string mats[] = {MAT};
			if (niTop > 0)
				mats[0] = MAT_TOP;
			res = MeshObject.Create(1, nvs, nis, mats, 0);
		}
		if (!res || !res.GetResource())
		{
			Print("MUD mud sheet: MeshObject.Create failed", LogLevel.ERROR);
			return null;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		// SHEET LOCAL TOP_UV64: the MAT_TOP section takes the 64 m skin chunk UV frame (as voxel tops,
		// MUD_VoxelProbe.KindAB kind 0), chunk picked by the tile middle; the dirt UVs wait in cix (unused: COLLIDE off)
		bool uv64 = m_bLocal && TOP_UV64 && !COLLIDE && niTop > 0;
		if (uv64 && !UV64_FAST)
		{
			for (int us = 0; us < nv * 2; us++)
				cix[us] = Math.Round(tuv[us] * 1000000);
			float ux0;
			float uz0;
			MUD_VoxelProbe.LookOrigin(ux0, uz0);
			float ucx = ux0 + Math.Floor(((tx + 0.5) * TILE_M - ux0) / TOP_UV_M) * TOP_UV_M;
			float ucz = uz0 + Math.Floor(((tz + 0.5) * TILE_M - uz0) / TOP_UV_M) * TOP_UV_M;
			for (int un = 0; un < nv; un++)
			{
				vector upv = tv[un];
				tuv[un * 2] = Math.Clamp((tx * TILE_M + upv[0] - ucx) / TOP_UV_M, 0, 1);
				tuv[un * 2 + 1] = 1 - Math.Clamp((tz * TILE_M + upv[2] - ucz) / TOP_UV_M, 0, 1);
			}
		}
		if (uv64 && UV64_FAST)
		{
			// stage 2 C4 UV64_FAST: dirt section first with its own (dragged) UVs, then the top UVs over tuv in place
			if (ni > 0)
			{
				mesh.UpdateIndices(1, tix);
				mesh.UpdateVerts(1, tv, tuv);     // its own (dragged) UVs
			}
			float fx0;
			float fz0;
			MUD_VoxelProbe.LookOrigin(fx0, fz0);
			float fox = tx * TILE_M - fx0 - Math.Floor(((tx + 0.5) * TILE_M - fx0) / TOP_UV_M) * TOP_UV_M;
			float foz = tz * TILE_M - fz0 - Math.Floor(((tz + 0.5) * TILE_M - fz0) / TOP_UV_M) * TOP_UV_M;
			for (int fn = 0; fn < nv; fn++)
			{
				vector fpv = tv[fn];
				tuv[fn * 2] = Math.Clamp((fox + fpv[0]) / TOP_UV_M, 0, 1);
				tuv[fn * 2 + 1] = 1 - Math.Clamp((foz + fpv[2]) / TOP_UV_M, 0, 1);
			}
			mesh.UpdateIndices(0, tixTop);
			mesh.UpdateVerts(0, tv, tuv);
		}
		// indices first: UpdateVerts rebuilds normals from the triangle list
		else if (ni > 0 && niTop > 0)
		{
			mesh.UpdateIndices(0, tixTop);
			mesh.UpdateVerts(0, tv, tuv);
			if (uv64)
			{
				for (int ur = 0; ur < nv * 2; ur++)
					tuv[ur] = cix[ur] / 1000000.0;     // the dirt section keeps its own (dragged) UVs
			}
			mesh.UpdateIndices(1, tix);
			mesh.UpdateVerts(1, tv, tuv);
		}
		else if (niTop > 0)
		{
			mesh.UpdateIndices(0, tixTop);
			mesh.UpdateVerts(0, tv, tuv);
		}
		else
		{
			mesh.UpdateIndices(0, tix);
			mesh.UpdateVerts(0, tv, tuv);
		}
		te.SetObject(mesh, "");
		te.Update();
		MUD_Prof.Add(MUD_Prof.T_ENGINE, System.GetTickCount(tPart));
		if (!COLLIDE)
			return res;
		// soldier-only static body from the same verts (both sections in one trimesh); inline, not a helper:
		// the local static arrays stay here (fixed-size array parameters untested in Enforce)
		int at = m_aCollEnt.Find(te);
		if (live && at >= 0 && m_fLast < m_aCollAt[at] + COLL_S)
		{
			m_aCollPend[at] = 1;
			m_iCollHeld++;
			return res;
		}
		int tC = System.GetTickCount();
		DropColl(te);
		// plain quads only (SMOOTH_EDGE cuts and the skirt are look only); cut verts sit on these quads' planes
		int na = nc;
		PhysicsGeom g;
		int layer = COLL_LAYER;
		if (ONE_GRID)
		{
			g = PhysicsGeom.CreateTriMesh(cvv, cix, TV * TV, na);
			layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;   // as our patches (wheels, feet, bullets)
		}
		else
			g = PhysicsGeom.CreateTriMesh(cvv, cix, TV * TV, na);
		if (!g)
		{
			Print("MUD mud sheet: collider trimesh failed", LogLevel.ERROR);
			return res;
		}
		autoptr PhysicsGeomDef defs[] = {PhysicsGeomDef("", g, COLL_MAT, layer)};
		Physics ph = Physics.CreateStaticEx(te, defs);
		if (!ph)
		{
			g.Destroy();
			Print("MUD mud sheet: collider body failed", LogLevel.ERROR);
			return res;
		}
		if (s_bTraceHidden && !ONE_GRID)
			te.ClearFlags(EntityFlags.TRACEABLE, false);
		m_aCollEnt.Insert(te);
		m_aCollGeom.Insert(g);
		m_aCollAt.Insert(m_fLast);
		m_aCollPend.Insert(0);
		m_iCollBuilt++;
		m_iCollMs += System.GetTickCount(tC);
		return res;
	}

	//------------------------------------------------------------------------------------------------
	//! Body and geom of te gone (hidden, moved or deleted tile)
	protected void DropColl(IEntity te)
	{
		if (!te)
			return;
		int at = m_aCollEnt.Find(te);
		if (at < 0)
			return;
		Physics ph = te.GetPhysics();
		if (ph)
			ph.Destroy();
		PhysicsGeom g = m_aCollGeom[at];
		if (g)
			g.Destroy();
		m_aCollEnt.Remove(at);
		m_aCollGeom.Remove(at);
		m_aCollAt.Remove(at);
		m_aCollPend.Remove(at);
	}

	//------------------------------------------------------------------------------------------------
	//! Window tiles whose collider was held back: mesh rebuilt again (with collider) once COLL_S is up
	protected void CollPending()
	{
		for (int t = 0; t < m_aTileEnt.Count() && t < TILES * TILES; t++)
		{
			IEntity te = m_aTileEnt[t];
			if (!te)
				continue;
			int at = m_aCollEnt.Find(te);
			if (at < 0 || m_aCollPend[at] == 0 || m_fLast < m_aCollAt[at] + COLL_S)
				continue;
			m_aSlotDirty[t] = 1;
			m_bDirty = true;
		}
	}
}

//! Entity of one mud tile: own class, NOT MUD_VoxelSubEntity - MUD_Surface.IsOurs took the mud collider for our
//! ground (2026-10-15 00:53: crater redo 4676 cells bare, then a 60-op dig 0 bare - every read was the mud top)
class MUD_MudTileEntityClass : GenericEntityClass
{
}

class MUD_MudTileEntity : GenericEntity
{
}

//! EASE DROP: one sliding sheet ground cell (world cell index, heights, start time)
class MUD_MudSheetEase
{
	int m_iGi;
	int m_iGk;
	float m_fFrom;
	float m_fTo;
	float m_fT0;
}

//! MUD LAB part C (L2 / L3): a set of window cells (index k * N + i), listed once each; m_aGen = scratch marks
class MUD_MudSheetCells
{
	int m_aIn[6561];
	int m_aGen[6561];
	ref array<int> m_aList = {};

	void Add(int v)
	{
		if (m_aIn[v] != 0)
			return;
		m_aIn[v] = 1;
		m_aList.Insert(v);
	}

	void Clear()
	{
		foreach (int v : m_aList)
			m_aIn[v] = 0;
		m_aList.Clear();
	}

	//! Follow moved the field by si, sk cells: cells that stay move with it, the rest go (scratch marks reset)
	void Shift(int si, int sk)
	{
		if (m_aList.IsEmpty())
			return;
		int n = MUD_MudSheet.N;
		array<int> keep = {};
		foreach (int v : m_aList)
		{
			m_aIn[v] = 0;
			int i = v % n;
			int k = v / n;
			int ni = i - si;
			int nk = k - sk;
			if (ni >= 0 && ni < n && nk >= 0 && nk < n)
				keep.Insert(nk * n + ni);
		}
		m_aList.Clear();
		foreach (int kv : keep)
			Add(kv);
		for (int g = 0; g < n * n; g++)
			m_aGen[g] = 0;
	}
}

class MUD_MudSheetVeh
{
	Vehicle m_Veh;
	ref array<vector> m_aLast = {};
	ref array<float> m_aTime = {};
	ref array<bool> m_aHas = {};
	float m_fDragT;                 // last Drag time
	float m_fEff0 = -1;             // gearbox efficiency before the mud (-1: not read yet)
	float m_fEffSet = 1;            // factor last set on it
	int m_iId;                      // GRADUAL: which vehicle made a pass
	VehicleWheeledSimulation m_Sim; // L8: cached
}

//! SYNC: one own stamp (prediction) waiting for the rut op that holds it
class MUD_MudSheetPred
{
	vector m_vA;
	vector m_vB;
	float m_fSpeed;
	float m_fT;                     // sheet time of the stamp
	int m_iVeh;                     // MUD_MudSheetVeh.m_iId (GRADUAL passes)
	float m_fHalf;                  // TyreHalf before WIDEN
	Vehicle m_Veh;                  // weak: gone = expire sooner
	bool m_bGone;                   // expired: a replay still holding it skips it
}

//! Static arrays (MeshObject takes them); a class holds at most 256 KB (mud-gotchas)
class MUD_MudSheetBuf
{
	vector m_aV[6561];
	float m_aUV[13122];
}

//! One tile's field data (TV x TV cells, edges shared with the neighbours): 5 x 441 x 4 bytes
class MUD_MudSheetTileData
{
	float m_aD[441];
	float m_aG[441];
	int m_aGs[441];
	float m_aOu[441];
	float m_aOv[441];
	int m_aBare[441];
	int m_aTouch[441];      // SHEET LOCAL (MUD LAB): 1 = a stamp wrote this cell (deposit band source)
	float m_aBand[441];     // SHEET LOCAL: deposit band 0..1
	float m_aLastY[441];    // SHEET LOCAL: drawn height at the last build ('would skip' meter)
	int m_iUnlift;          // LIFT BY DRAWN: 1 = patches were told this (stored) tile's mesh is on screen
	int m_iDrawn;           // LAZY RUT MESH: 1 = every cell was traced at its last mesh build (that mesh has no hole)
	int m_iLow;             // PAPER TEST 2: 1 = its last mesh was the low-detail one (far from the camera)
	ref MUD_MudSheetTileDist m_Dist;     // stage 2 C1 BAND_INCR: its kept distances (null: none)
	int m_iSig1;            // stage 2 C3 BUILD_SKIP_SAME: class signature of its last mesh (0 = unknown)
	int m_iSig2;
}

//! Stage 2 C1 BAND_INCR: a stored tile's distances (cells) to the nearest touched cell, as BandTile last made them
class MUD_MudSheetTileDist
{
	float m_aDist[441];
	int m_iGen = -1;        // MUD_MudSheet.s_iDistGen when made (another = stale)
}

//! Stage 2 C1 BAND_INCR: the window's distances - own class, MUD_MudSheetField is near the 256 KB limit
class MUD_MudSheetDist
{
	float m_aDist[6561];    // chamfer distance (cells) to the nearest touched cell, capped at BandMargin + 1
	int m_aChg[6561];       // 1 = lowered in the running BandIncr
}

//! Stage 2 C1 TEST_KIT bench: the window state it puts back
class MUD_MudSheetBench
{
	int m_aTouch[6561];
	float m_aBand[6561];
	float m_aDist[6561];
	int m_aSlot[64];
}

//! SHEET LOCAL far data (MUD LAB part D): one int a cell = 1 byte each for height, drag u, drag v, class (bit 0 bare,
//! bit 1 touched, bits 2-6 deposit band): 441 x 4 bytes
class MUD_MudSheetFar
{
	int m_iTx;
	int m_iTz;
	int m_aC[441];
}

//! A frozen trail tile: its data, entity (origin at the tile corner) and the mesh it draws
class MUD_MudSheetStore
{
	int m_iTx;
	int m_iTz;
	ref MUD_MudSheetTileData m_D = new MUD_MudSheetTileData();
	IEntity m_Ent;
	ref Resource m_Res;
	float m_fDieAt;         // dropped: delete the entity at this time
}

//! One tile's buffers (TV x TV verts, TQ x TQ quads)
class MUD_MudSheetTile
{
	vector m_aV[441];
	float m_aUV[882];
	int m_aI[2400];
}

//! 9 x 6561 x 4 bytes = 236 KB (a class holds at most 256 KB)
class MUD_MudSheetField
{
	float m_aD[6561];       // mud height over the ground (m)
	float m_aG[6561];       // ground height (world y)
	int m_aGs[6561];        // 1 = m_aG traced
	float m_aOu[6561];      // texture drag x / z (m)
	float m_aOv[6561];
	int m_aBare[6561];      // 1 = mud blown off here (crater): thin mud is not drawn, the ground shows
	int m_aTouch[6561];     // SHEET LOCAL (MUD LAB): 1 = a stamp wrote this cell
	float m_aBand[6561];    // SHEET LOCAL: deposit band 0..1 (distance to the nearest touched cell)
	float m_aLastY[6561];   // SHEET LOCAL: drawn height at the last build ('would skip' meter)
}
