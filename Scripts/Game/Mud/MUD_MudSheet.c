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
	static const bool ENABLED = false;
	static const bool FOLLOW = true;            // live window under the vehicle (wheel marks + drag); false = only the zone tiles built at load show. Off 23:35 -> back on 23:42 (user: mud not pressed)
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
	static const float MOVED = 0.08;            // ground under a mud cell moved more than this (m, trench / fill / crater): mud gone
	// COLLIDE (user 2026-10-15 "we just walk inside of it"): each mud tile gets a static trimesh on CharCollide
	// (0x80000) = solid for Character + CharacterAI only (gproj matrix, mud-gotchas 2026-10-13). NOT FireGeo: every
	// ground read of ours is a Projectile trace (GroundDown, Trace, MUD_Surface.Y ...) and would read the mud top as
	// ground. The wheel raycast (VehicleCast) does not hit CharCollide; the Wheel preset geoms MAY touch it (unproven).
	static const bool COLLIDE = true;            // PERF A/B 2026-10-15: off gave the same fps (38-48 driving)
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
	static const bool ONE_GRID = true;
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
	static const int BUILD_MS = 8;              // zone tiles built per frame at load: until this many ms are used (~0.8 ms a tile)
	static const float UPLOAD_EVERY = 0.1;      // s between mesh rebuilds (10 Hz, as the KB mud tile)
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
	static const ResourceName MAT_TOP = "{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat";   // = MUD_VoxelProbe.MAT_TOP
	static const float TOUCH = 0.01;            // m off THICK (or any texture drag, or blasted) = a dirt quad
	static const float THICK = 0.15;            // soft dirt layer on the hard ground (m)
	// MUD DRAG TEST (2026-10-14, SP only; shape from MudRunner sub_140930110, research/2026-10-14-mudrunner-ida.md):
	// the wheels stand on the hard ground, so they already look sunk THICK; the mud only adds drag by submerged area
	// (front: tyre width x depth, side: pi x radius x depth), speed above 1 m/s counted as v^0.75, and less engine.
	static const bool DRAG_TEST = true;          // PERF test 2026-10-15: off gave the same fps in mud (39-49)
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
	protected bool m_bZonesQueued;
	protected ref array<int> m_aBuildTx = {};   // zone tiles still to build at load
	protected ref array<int> m_aBuildTz = {};
	protected int m_iZoneTiles;
	protected ref array<MUD_MudZoneEntity> m_aZones = {};
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
	//! Called every frame from MUD_WorldGround (time = its m_fTime, back to 0 at each play)
	static void Tick(float time)
	{
		if (!ENABLED || RplSession.Mode() == RplMode.Dedicated)
			return;
		if (OFF_TEST)
		{
			OffLog(time);
			return;
		}
		if (!s_Inst || time < s_Inst.m_fLast)
			s_Inst = new MUD_MudSheet();    // first frame, or a new play (the old entity went with the old world)
		s_Inst.Frame(time);
	}

	//------------------------------------------------------------------------------------------------
	//! ONE_GRID: builds the stored tiles trench / fill ops changed (live ones draw on their own upload)
	protected void FlushOneBuild()
	{
		int t0 = System.GetTickCount();
		int built = 0;
		m_iTaperMs = 0;
		while (!m_aOneBuild.IsEmpty())
		{
			int key = m_aOneBuild[0];
			m_aOneBuild.RemoveOrdered(0);
			MUD_MudSheetStore st = m_mTrail.Get(key);
			if (!st || !st.m_Ent)
				continue;
			int tx = Math.Round(key / 20000.0);
			int tz = key - tx * 20000;
			if (IsLive(tx, tz))
				continue;
			Resource res = BuildTile(st.m_Ent, st.m_D, tx, tz, false);
			if (res)
				st.m_Res = res;
			built++;
		}
		if (built > 0)
			Print(string.Format("MUD one grid: stored tiles built %1 in %2 ms (taper %3 ms)", built, System.GetTickCount(t0), m_iTaperMs), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void Frame(float time)
	{
		if (m_bNowDue)
			StepNow();
		else if (!m_aPend.IsEmpty() && System.GetTickCount(m_iPendT) > PEND_MAX_MS)
		{
			Print(string.Format("MUD one grid: %1 held ops, no voxel go-live in %2 ms -> sheet now", m_aPend.Count(), PEND_MAX_MS), LogLevel.WARNING);
			ApplyPending();
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
		BuildZones();
		GroundJobs();
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
			// (mud grew outward from the car) - it waits until every zone tile is built, then takes those over
			if (FOLLOW && !m_bTeleported && m_aBuildTx.IsEmpty())
			{
				int tF = System.GetTickCount();
				Follow(veh.GetOrigin());
				m_aStatSec[0] = m_aStatSec[0] + System.GetTickCount(tF);
			}
		}
		if (m_bPlaced)
		{
			int tS = System.GetTickCount();
			foreach (Vehicle v : MUD_Wheels.GetAll())
			{
				if (v)
					Wheels(v, time);
			}
			m_aStatSec[1] = m_aStatSec[1] + System.GetTickCount(tS);
			if (m_bAnyZone && time >= m_fNextSlump)
			{
				// 10 Hz, with the upload (COST 23:01: every frame was ~3 ms of the ~5)
				m_fNextSlump = time + UPLOAD_EVERY;
				tS = System.GetTickCount();
				Slump();
				m_aStatSec[2] = m_aStatSec[2] + System.GetTickCount(tS);
			}
			tS = System.GetTickCount();
			if (m_bAnyZone)
				Grounds();
			m_aStatSec[3] = m_aStatSec[3] + System.GetTickCount(tS);
			if (m_bDirty && time >= m_fNextUp && !frozen)
			{
				m_fNextUp = time + UPLOAD_EVERY;
				tS = System.GetTickCount();
				Upload();
				m_aStatSec[4] = m_aStatSec[4] + System.GetTickCount(tS);
			}
		}
		int ms = System.GetTickCount(t0);
		m_iStatFrames++;
		m_iStatMs += ms;
		if (ms > m_iStatMax)
			m_iStatMax = ms;
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
					+ string.Format(" | ms in 5 s: follow %1 wheels %2 slump %3 grounds %4 upload %5 | slump box %6 cells",
						m_aStatSec[0], m_aStatSec[1], m_aStatSec[2], m_aStatSec[3], m_aStatSec[4], Math.Max(0, m_iBoxI1 - m_iBoxI0 + 1) * Math.Max(0, m_iBoxK1 - m_iBoxK0 + 1))
					+ string.Format(" | colliders: %1 built in %2 ms, %3 held back, %4 alive", m_iCollBuilt, m_iCollMs, m_iCollHeld, m_aCollEnt.Count()), LogLevel.NORMAL);
				Print(string.Format("MUD mud sheet PERF: fps %1, worst frame %2 ms, COLLIDE %3, top speed %4 m/s, our ms/frame %5, drawn quads %6",
					m_iStatFrames / (time - m_fStatT), m_fStatDtMax * 1000, COLLIDE, m_fStatSpeed, avg, m_iStatQuads)
					+ string.Format(" | FREEZE %1: live fps %2 (%3 s), frozen fps %4 (%5 s)", FREEZE_TEST,
						m_iLiveN / Math.Max(0.001, m_fLiveT), m_fLiveT, m_iFrzN / Math.Max(0.001, m_fFrzT), m_fFrzT), LogLevel.NORMAL);
			}
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
		// 1. tiles that stay move to their new slot; leaving ones become trail (touched) or spare entities
		array<IEntity> spare = {};
		array<IEntity> newEnt = {};
		array<ref Resource> newRes = {};
		for (int t0 = 0; t0 < TILES * TILES; t0++)
		{
			newEnt.Insert(null);
			newRes.Insert(null);
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
				}
			}
		}
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
				}
				else
				{
					m_T.m_aD[v] = THICK;
					m_T.m_aG[v] = 0;
					m_T.m_aGs[v] = 0;
					m_T.m_aOu[v] = 0;
					m_T.m_aOv[v] = 0;
					m_T.m_aBare[v] = 0;
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
				int tx = nTx + ti2;
				int tz = nTz + tk2;
				int key = TrailKey(tx, tz);
				MUD_MudSheetStore st = m_mTrail.Get(key);
				if (st && st.m_Ent)
				{
					Unpack(st.m_D, ti2, tk2);
					newEnt[t2] = st.m_Ent;
					newRes[t2] = st.m_Res;
					m_mTrail.Remove(key);
					RemoveTrailKey(key);
				}
				else if (!spare.IsEmpty())
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
		UpdateSlots();
		m_iUntraced = 0;
		for (int u = 0; u < NN; u++)
		{
			if (m_F.m_aGs[u] == 0)
				m_iUntraced++;
		}
		m_bIdxDirty = true;
		m_bDirty = true;
		m_fNextUp = 0;      // upload at once, else the old mesh shows one shift off
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
		MUD_MudSheetStore st = m_mTrail.Get(key);
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
		Pack(ti, tk, st.m_D);
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
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Is world tile tx, tz a mud tile (its middle inside a zone)? Fades go where the next tile is not one.
	protected bool HasTile(int tx, int tz)
	{
		return TileInZone(tx, tz);
	}

	//------------------------------------------------------------------------------------------------
	protected bool TileInZone(int tx, int tz)
	{
		float cx = (tx * TQ + TQ / 2) * CELL;
		float cz = (tz * TQ + TQ / 2) * CELL;
		foreach (MUD_MudZoneEntity z : m_aZones)
		{
			if (z && z.Contains(cx, cz))
				return true;
		}
		return false;
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
			m_aSlotDirty[t] = 1;
		}
		m_bAnyZone = false;
		for (int tz2 = 0; tz2 < TILES * TILES; tz2++)
		{
			if (m_aSlotZone[tz2] != 0)
				m_bAnyZone = true;
		}
		m_iScan = 0;
		m_iUntraced = NN;       // Grounds makes one pass and sets it back to 0
		m_bDirty = true;
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
				m_aSlotDirty[tk * TILES + ti] = 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Map load: queue every zone tile once, then build one per frame (traced, THICK layer, meshed, stored)
	protected void BuildZones()
	{
		if (!m_bZonesQueued)
		{
			m_bZonesQueued = true;
			MUD_MudZoneEntity.GetInWorld(m_aZones);
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
						m_aBuildTx.Insert(tx);
						m_aBuildTz.Insert(tz);
					}
				}
			}
			m_iZoneTiles = m_aBuildTx.Count();
			Print(string.Format("MUD mud zones: %1 zones, %2 tiles to build (BUILD_MS per frame)", m_aZones.Count(), m_iZoneTiles), LogLevel.NORMAL);
			if (m_bPlaced)
				UpdateSlots();
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
	//! One zone tile at load: traces on the even cells (11 x 11), the cells between filled, the THICK layer, a mesh
	protected void BuildZoneTile(int tx, int tz)
	{
		int key = TrailKey(tx, tz);
		if (m_mTrail.Contains(key) || IsLive(tx, tz))
			return;     // duplicate (overlapping zones) or the window has it already
		MUD_MudSheetStore st = new MUD_MudSheetStore();
		st.m_iTx = tx;
		st.m_iTz = tz;
		MUD_MudSheetTileData d = st.m_D;
		for (int lk = 0; lk < TV; lk += 2)
		{
			for (int li = 0; li < TV; li += 2)
				d.m_aG[lk * TV + li] = GroundDown((tx * TQ + li) * CELL, (tz * TQ + lk) * CELL);
		}
		for (int k = 0; k < TV; k++)
		{
			for (int i = 0; i < TV; i++)
			{
				int l = k * TV + i;
				d.m_aD[l] = THICK;
				d.m_aOu[l] = 0;
				d.m_aOv[l] = 0;
				d.m_aGs[l] = 1;
				d.m_aBare[l] = 0;
			}
		}
		FillTile(d);
		st.m_Ent = SpawnTile(tx, tz, d.m_aG[0]);
		st.m_Res = BuildTile(st.m_Ent, d, tx, tz, false);
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
		s_Inst.QueueChange(lo, hi, wp, blastR);
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueChange(vector lo, vector hi, vector wp, float blastR)
	{
		int c = m_aChgLo.Count();
		m_aChgLo.Insert(lo);
		m_aChgHi.Insert(hi);
		m_aChgWp.Insert(wp);
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
		s_Inst.m_bNowDue = true;
		s_Inst.StepNow();
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
		int tiles = DigNow();
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
				if (!st || !st.m_Ent)
					continue;
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
				Resource res = BuildTile(st.m_Ent, d, tx, tz, false);
				if (res)
					st.m_Res = res;
				done++;
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
				MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
				if (!st || !st.m_Ent)
					continue;
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
		if (IsLive(tx, tz))
		{
			if (!TileInZone(tx, tz))
				return 0;
			int v = (k - m_iZ0) * N + (i - m_iX0);
			return m_F.m_aD[v];
		}
		MUD_MudSheetStore st = m_mTrail.Get(TrailKey(tx, tz));
		if (!st)
			return 0;
		int l = (k - tz * TQ) * TV + (i - tx * TQ);
		return st.m_D.m_aD[l];
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
			return OurY(x, z);
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
		return GetGame().SpawnEntity(MUD_MudTileEntity, GetGame().GetWorld(), params);
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
				m_aTileEnt.Insert(st.m_Ent);
				m_aTileRes.Insert(st.m_Res);
				m_mTrail.Remove(key);
				RemoveTrailKey(key);
				continue;
			}
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
					Stamp(lp, cp, len / dt, time, mv.m_iId, TyreHalf(sim.WheelGetRadius(w)));
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
		if (i1 < i0 || k1 < k0)
			return;
		MarkRect(i0, i1, k0, k1);
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
		if (DRAG_LIM > 0)
			LimitDrag(i0, i1, k0, k1);
		m_bDirty = true;
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
				if (!ZoneAtCell(i, k))
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
			if (m_F.m_aGs[rv] == 0 || !ZoneAtCell(ri, rk))
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
			g = OurY(x, z);
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
		m_bDirty = false;
		if (!m_Ent)
			return;
		m_iStatUploads++;
		m_bIdxDirty = false;
		m_iBuildQuads = 0;
		int x0 = m_iX0 / TQ;
		int z0 = m_iZ0 / TQ;
		for (int tk = 0; tk < TILES; tk++)
		{
			for (int ti = 0; ti < TILES; ti++)
			{
				int t = tk * TILES + ti;
				if (m_aSlotDirty[t] == 0)
					continue;
				m_aSlotDirty[t] = 0;
				if (m_aSlotZone[t] == 0)
				{
					if (m_aTileEnt[t])
					{
						m_aTileEnt[t].ClearFlags(EntityFlags.VISIBLE, false);    // no mud outside the zones
						DropColl(m_aTileEnt[t]);
					}
					continue;
				}
				Pack(ti, tk, m_TD);
				Resource r = BuildTile(m_aTileEnt[t], m_TD, x0 + ti, z0 + tk, true);
				if (r)
					m_aTileRes[t] = r;    // keep the new mesh alive, drop the old one only now
			}
		}
		m_iStatQuads = m_iBuildQuads;
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
	//! One tile's mesh (world tile tx, tz; entity origin at its corner): every quad whose 4 corners have a ground
	//! height, vertex = ground + LIFT + d, faded under the ground near sides with no tile beyond; UV = world position
	//! (repeats every UV_TILES tiles) - drag. Returns the new mesh, or null (nothing drawn: entity hidden).
	//! live: a window tile (collider throttled to COLL_S); stored tiles always rebuild theirs
	protected Resource BuildTile(IEntity te, MUD_MudSheetTileData d, int tx, int tz, bool live)
	{
		if (!te)
			return null;
		bool fW = !HasTile(tx - 1, tz);
		bool fE = !HasTile(tx + 1, tz);
		bool fS = !HasTile(tx, tz - 1);
		bool fN = !HasTile(tx, tz + 1);
		float edgeCells = EDGE / CELL;
		float orgY = te.GetOrigin()[1];
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
		for (int yk = 0; yk < TV; yk++)
		{
			for (int yi = 0; yi < TV; yi++)
			{
				int yl = yk * TV + yi;
				int e = 1000;
				if (fW)
					e = Math.Min(e, yi);
				if (fE)
					e = Math.Min(e, TQ - yi);
				if (fS)
					e = Math.Min(e, yk);
				if (fN)
					e = Math.Min(e, TQ - yk);
				float fade = Math.Clamp(e / edgeCells, 0, 1);
				if (ONE_GRID)
				{
					// the sheet is the ground: bare = ground height; only the side toward ground not ours dips
					float fadeB = fade;
					if (anyBare && TAPER > 0)
						fadeB = Math.Min(fade, TaperFade(bd[yl]));
					vy[yl] = d.m_aG[yl] - ONE_DIP * (1 - fade) + (LIFT + d.m_aD[yl]) * fadeB;
					if (Bare(d, yl))
						vy[yl] = d.m_aG[yl] - ONE_DIP * (1 - fade);
					continue;
				}
				if (anyBare && TAPER > 0)
					fade = Math.Min(fade, TaperFade(bd[yl]));
				vy[yl] = d.m_aG[yl] - SINK + (SINK + LIFT + d.m_aD[yl]) * fade;
				if (Bare(d, yl))
					vy[yl] = d.m_aG[yl] - SINK;     // the mesh dips under the ground toward a blown-off patch
			}
		}
		bool smooth = TOP_LOOK && SOFT && SMOOTH_EDGE;
		if (smooth)
		{
			for (int ql = 0; ql < TV * TV; ql++)
			{
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
		for (int qk = 0; qk < TQ; qk++)
		{
			for (int qi = 0; qi < TQ; qi++)
			{
				int la = qk * TV + qi;
				if (d.m_aGs[la] == 0 || d.m_aGs[la + 1] == 0 || d.m_aGs[la + TV] == 0 || d.m_aGs[la + TV + 1] == 0)
					continue;
				if (ONE_GRID && (d.m_aBare[la] == BARE_VOX || d.m_aBare[la + 1] == BARE_VOX || d.m_aBare[la + TV] == BARE_VOX || d.m_aBare[la + TV + 1] == BARE_VOX))
					continue;   // VOXEL HOLES: the voxel ground draws and collides here
				if (!ONE_GRID && Bare(d, la) && Bare(d, la + 1) && Bare(d, la + TV) && Bare(d, la + TV + 1))
					continue;   // blown off: the voxel ground shows
				if (!ONE_GRID && HOLE_SKIP > 0 && (Bare(d, la) || Bare(d, la + 1) || Bare(d, la + TV) || Bare(d, la + TV + 1)))
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
					if (ONE_GRID)
						cvv[lv] = Vector(ci * CELL, d.m_aG[lv] - orgY, ck * CELL);
					else
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
				cix[nc] = la;           // by tile vertex (cvv), not by mesh vertex
				cix[nc + 1] = la + TV;
				cix[nc + 2] = la + TV + 1;
				cix[nc + 3] = la;
				cix[nc + 4] = la + TV + 1;
				cix[nc + 5] = la + 1;
				nc += 6;
				bool clean = !Changed(d, la) && !Changed(d, la + 1) && !Changed(d, la + TV) && !Changed(d, la + TV + 1);
				if (SOFT)
					clean = !DirtQuad(d, la, tx * TQ + qi, tz * TQ + qk);
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
		int nvColl = nv;        // collider: tile verts / indices only, not the skirt
		int niColl = ni;
		if (ONE_GRID && ONE_SKIRT > 0 && ni + niTop > 0)
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
		// indices first: UpdateVerts rebuilds normals from the triangle list
		if (ni > 0 && niTop > 0)
		{
			mesh.UpdateIndices(0, tixTop);
			mesh.UpdateVerts(0, tv, tuv);
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

//! 6 x 6561 x 4 bytes = 157 KB
class MUD_MudSheetField
{
	float m_aD[6561];       // mud height over the ground (m)
	float m_aG[6561];       // ground height (world y)
	int m_aGs[6561];        // 1 = m_aG traced
	float m_aOu[6561];      // texture drag x / z (m)
	float m_aOv[6561];
	int m_aBare[6561];      // 1 = mud blown off here (crater): thin mud is not drawn, the ground shows
}
