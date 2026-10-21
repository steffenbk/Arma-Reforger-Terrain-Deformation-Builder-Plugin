//! TEMP probe 2026-10-13: VOXEL BRICKS (spec docs/superpowers/specs/2026-10-13-voxel-bricks-design.md). Runs 1-10
//! (2026-10-12, mud-gotchas) proved voxels in the ground: surface nets (one vertex per cell, one quad per sign-changing
//! sample edge), 8^3-cell blocks, collider + walkable cave, DirtCover_01 at UVs 0..1 (script-mesh UVs are CLAMPED to 0..1).
//! Before this the voxels lived in 64 m tiles: a blast on fresh ground took 1.1-1.3 s to show, ruts vanished under a
//! new tile (seeded from undug ground), the first tunnel dig waited (user test 2026-10-13 19:40, mud-gotchas).
//! Now: sparse 4 m bricks (8 x 8 x 8 cells of 0.5 m) on the skin grid (one brick column = 2 x 2 skin cells).
//!  - Every op (0 crater, 1 tunnel capsule, 2 trench cut, 3 fill one layer) converts the brick COLUMNS its box touches
//!    plus one ring (it never carves that ring, so voxels meet the heightfield only on untouched ground).
//!  - A new column is seeded from the server's ground heights: the op text carries (dug - undug) in cm per sample column
//!    of every column it converts (MakeSeed), every machine adds that to its own undug height (the same terrain data).
//!    So ruts / berms / heightfield craters carry into the voxels. Columns converted earlier ignore later seeds.
//!  - Brick rows are made where the seeded surface is (EnsureRows) and where an op changes or exposes something
//!    (Put, QueueBox); a missing row of a converted column reads the seeded field (Seeded), so nothing is lost.
//!  - Voxels are drawn (mesh + collider) only in CARVED skin cells: cells where an op changed samples (MarkCarved).
//!    Every other cell keeps the heightfield with its ruts (user test 2026-10-13 20:30: converting whole columns
//!    hid the ruts over ~20 m round a crater; 0.5 m samples cannot show a rut anyway).
//!  - Every changed brick is built hidden; once nothing is queued, all of it goes in at one frame (GoLive): new
//!    meshes shown, rebuilt live meshes swapped, and the newly carved hole cells go out on the skin + fine patches
//!    (CELL_VOX / MUD_WorldGroundEntity.VOX, patch meshes rebuilt that frame). A quad over a cell line is drawn when
//!    any corner is in a carved cell (LIFT above the heightfield there), so the line has no crack.
//!  - An op is all voxel or all heightfield: the server logs and sends it only if every column it needs is converted or
//!    can be (MAX_COLUMNS, SEED_CHARS, ground heights known); else the caller digs the heightfield (Trench returns
//!    REFUSED). Clients apply what the server sent, in seq order (MUD_WorldGroundEntity.RpcDo_VoxelOp, JIP batches
//!    via MUD_VoxelNet.c), a few ms of ops per frame (DRAIN_MS, JOIN_MS while joining).
//!  - Hard columns (on / next to kept terrain) are never dug; kept columns sit KEPT_SINK under the terrain.
//! Tunnel test: on foot, hold T (MUD_VoxelTunnelContext in Configs/System/chimeraInputCommon.conf) digs an upright
//! capsule (r 0.8, 2.5 m tall) 1.1 m ahead, now and every 300 ms; look down/up to ramp down/up.
//! Nothing digs deeper than FLOOR (3 m) under the undug ground; tunnels and the C4 sphere TUNNEL_FLOOR (10 m, U1),
//! never under the sea guard. Clamps add no NEW air below the floor; older air stays (U1 2026-10-17).
//! Remove with the hook lines in MUD_WorldGround.c (Clear, Tick, Crater, HasHole, trench hooks, RpcDo_VoxelOp,
//! VoxCellsChanged, UndugY, DugUndug, VOX, AtGround), CELL_VOX in MUD_CoarseSkin.c, MUD_VoxelNet.c, m_sPlan in
//! MUD_TrenchLayout.c, MUD_VoxelSubEntity in MUD_Surface.c / MUD_BuildingDestructionFix.c / GroundCovers.
//! Enforce limits (compile errors 2026-10-14): a static array holds at most 65536 entries, a class at most 256 KB
//! (fine: 10000 verts = 120 KB + UVs 80 KB)
class MUD_VoxelVerts
{
	static const int SIZE_V = 8000;        // SIZE 8000 | 10000 (= MUD_VoxelProbe.MAX_V, Init checks)
	static const int SIZE_UV = SIZE_V * 2;
	vector m_aV[SIZE_V];
	float m_aUV[SIZE_UV];
}

class MUD_VoxelIdx
{
	static const int SIZE_I = 24000;        // SIZE 24000 | 30000 (= MUD_VoxelProbe.MAX_I, Init checks)
	int m_aI[SIZE_I];
}

//! Cell cache of one MUD_VoxelHouse build (9^3 cells, local index lk * 81 + lj * 9 + li)
class MUD_VoxelCells
{
	int m_aCv[11664];       // 729 cells * 4 UV kinds * 4 tile parities: stamp * 8192 + vertex
	vector m_aCp[729];      // cell vertex position (local)
	int m_aCs[729];         // stamp of m_aCp
}

//! Cell cache of one brick build: cells -1..SUB-1 around the brick (CN^3), local index lk * CN2 + lj * CN + li.
//! Two vertex slots per cell and UV kind (fine: 4913 * 16 passed the 65536 array limit; fine = 236 KB)
class MUD_VoxelSlots
{
	static const int SIZE_C = 729;          // SIZE 729 | 4913 (= MUD_VoxelProbe.CN3, Init checks)
	static const int SIZE_CV = SIZE_C * 8;
	int m_aCv[SIZE_CV];     // (lc * 4 + kind) * 2 + slot: (stamp * 4 + tile parity) * MUD_VoxelProbe.VKEY + vertex
	vector m_aCp[SIZE_C];   // cell vertex position (brick local)
	int m_aCs[SIZE_C];      // stamp of m_aCp
}

//! The samples round one brick for its build (-1..8 on each axis) and each sample column's seeded top
class MUD_VoxelGather
{
	static const int SIZE_G = 1000;         // SIZE 1000 | 5832 (= MUD_VoxelProbe.GN3, Init checks)
	static const int SIZE_T = 100;          // SIZE 100 | 324 (= GN2)
	static const int SIZE_C = 729;          // SIZE 729 | 4913 (= CN3)
	float m_aG[SIZE_G];     // (lk + 1) * GN2 + (lj + 1) * GN + li + 1; MUD_VoxelProbe.MISS = column not converted
	float m_aTop[SIZE_T];   // seeded top (world y), (lk + 1) * GN + li + 1
	int m_aOk[SIZE_C];      // per cell (as MUD_VoxelSlots)
	int m_aLa[SIZE_T];      // per column (as m_aTop): lowest gathered row lj that is air (GN: none; BAND_NONE: MISS)
	int m_aHs[SIZE_T];      // highest gathered row lj that is solid (-1: none; -BAND_NONE: MISS): stamp * 2 + 1 = all 8 corners have a value, stamp * 2 = not
}

//! Entity of one voxel brick: own class so MUD_Surface.IsOurs, GroundCovers and MUD_BuildingDestructionFix know it
class MUD_VoxelSubEntityClass : GenericEntityClass
{
}

class MUD_VoxelSubEntity : GenericEntity
{
}

//! One 4 m brick: owns the samples 8 bi .. 8 bi + 7 (and the same in j, k), each sample is stored once
class MUD_VoxelBrick
{
	int m_iBi;
	int m_iBj;
	int m_iBk;
	static const int SIZE_D = 512;          // SIZE 512 | 4096 (= MUD_VoxelProbe.SUB3, Init checks)
	float m_aD[SIZE_D];     // density, > 0 solid; (lk * SUB + lj) * SUB + li - the FEEL field (bodies, every op)
	float m_aV[SIZE_D];     // RUT_LOOK: look cuts (op 6) not yet in m_aD, V_NONE = none; the mesh draws min(m_aD, m_aV)
	bool m_bHasV;           // m_aV is set up
	IEntity m_Ent;
	ref Resource m_Res;     // keeps the mesh alive (as MUD_FinePatch.m_MeshRes)
	Physics m_Phys;
	ref array<PhysicsGeom> m_aGeoms = {};
	bool m_bQueued;
	bool m_bShown;          // false: built hidden, shown at the next GoLive
	// shown brick rebuilt: built hidden here, swapped in with every other pending one in one frame (SwapPending)
	bool m_bNew;            // a pending build waits (m_NewEnt null = the new mesh is empty)
	bool m_bMeshOnly;       // RUT_LOOK: the pending build is a new mesh (m_NewRes) for m_Ent only - the body stays
	bool m_bFeelQ;          // queued for a change the body can feel (anything but a look op); false = look only
	IEntity m_NewEnt;
	ref Resource m_NewRes;
	Physics m_NewPhys;
	ref array<PhysicsGeom> m_aNewGeoms = {};
}

//! A brick entity that was swapped out or dropped: hidden, collision off (layer 0), deleted GRAVE_MS later. Deleting
//! it at once crashed a client (2026-10-13 23:17, Exp 192624, access violation in VehicleDustSystem: a wheel's
//! contact still pointed at a brick deleted that frame - IDA, sub_140B1ED70 reads contact + 48 -> + 0xD8 = 0).
class MUD_VoxelGrave
{
	IEntity m_Ent;
	ref Resource m_Res;     // its mesh stays alive until the entity goes
	ref array<PhysicsGeom> m_aGeoms = {};
	int m_iAt;
}

//! One converted 4 x 4 m brick column: its seed and its brick rows
class MUD_VoxelColumn
{
	int m_iBi;
	int m_iBk;
	static const int SIZE_N = 64;           // SIZE 64 | 256 (= MUD_VoxelProbe.SUB2, Init checks)
	float m_aTop[SIZE_N];   // seeded surface (world y) per owned sample column lk * SUB + li: dug + LIFT, kept: undug - KEPT_SINK
	float m_aUndug[SIZE_N]; // undug ground (FLOOR, trench depths)
	int m_aHard[SIZE_N];    // 1 = never dug (int: a fresh bool array reads true, MUD_FinePatch.m_aCellHard)
	int m_aKept[SIZE_N];
	int m_aCarved[4];       // its 2 x 2 skin cells (lz * 2 + lx): 1 = an op changed samples there, the voxels show
	int m_aLook[4];         // RUT_LOOK, rendering machines: 1 = a look rut moved the drawn surface there - the voxel MESH
	                        // shows (patch mesh out, LOOK bit), the bodies wait for the feel cut to carve it
	ref map<int, ref MUD_VoxelBrick> m_mRows = new map<int, ref MUD_VoxelBrick>();
}

//! RUT FIELD Stage 1 (plan docs/superpowers/plans/2026-10-15-rut-field-plan.md): the rut floor of one 4 x 4 m tile
//! (same key as its voxel column, ColKey), on its own - a tile can exist without a column. Made at its first floor
//! write (MUD_VoxelProbe.RutTileMake fills it with FLOOR_NONE: a fresh float array reads 0 = a floor at y 0).
class MUD_RutTile
{
	static const int SIZE_F = 256;  // = MUD_VoxelProbe.FSUB^2 (SizesOk checks)
	float m_aFloor[SIZE_F];         // rut floor (world y) per floor sample fk_local * FSUB + fi_local, FLOOR_NONE = none
	float m_aRidge[SIZE_F];         // Build 3 Stage 3: bulldozed ridge top (world y), same index, FLOOR_NONE = none. Plain RUT
	                                // FIELD ground only (MUD_VoxelProbe.GrowRidges); a sample with a floor never has one
	float m_aWear[SIZE_F];          // WEAR COUNT (2026-10-19): passes counted on a sample with no floor yet (whole 0.01,
	                                // RUT_RAMP weights add), 0 = never driven (a fresh array reads 0). MUD_VoxelProbe.WearPass
}

class MUD_VoxelOp
{
	int m_iSeq;                         // server order, 1 = first (MP: clients apply in this order)
	int m_iKind;                        // 0 crater, 1 tunnel dig (capsule), 2 trench cut, 3 fill one layer, 4 ruts,
	                                    // 5 convert ahead (no sample changes: the cells go VOX before the wheels get there)
	vector m_vWp;                       // crater: hit point; tunnel: capsule middle; 2 / 3: unused
	float m_fDepth;                     // crater: depth (clamped); tunnel: radius; 2: layers; 3: hole depth before
	float m_fB;                         // 3: depth after the fill
	int m_iPiece;                       // 2: trench piece, -1 = every piece (free dig square)
	string m_sPlan;                     // 2 / 3: MUD_TrenchPlan text
	string m_sSeed;                     // seed of the columns it converts (MakeSeed), empty = all undug
	string m_sText;                     // the op as sent (JIP)
	string m_sRows;                     // 4: brick rows this rut op made, "colkey,bj;..." (J2 snapshot: a joiner skips the
	                                    // op but makes these rows at its seq - a row changes later Puts, KEEP_ROW)
	ref MUD_TrenchLayout m_Lay;         // 2 / 3: built from m_sPlan
	vector m_vLo;                       // x, z box of everything it changes (Prepare)
	vector m_vHi;
	// 4 / 5 / 6: segments (Prepare, from m_sPlan "x,y,z,x,y,z;..." in cm). 4 (feel) and 6 (look): m_fDepth = cut per
	// pass, m_fB = deepest;
	// 5: m_fDepth = half width of the strip
	ref array<vector> m_aSegA = {};
	ref array<vector> m_aSegB = {};
}

//! J2 (Build 2 Stage 3, 2026-10-17), server: one joiner's snapshot send. Ops <= m_iS go as voxel ops with the rut ops
//! (kind 4) as placeholders "a|R|b" (rows: "a|R|a|rows"), then the rut tiles as they were at m_iS (built at the ask:
//! a tile written after S would take a later rut op twice), then the live ops > m_iS as before.
class MUD_JipSnap
{
	int m_iS;                           // newest seq at the ask
	ref array<string> m_aTiles = {};    // tile parts "T|S|i|key:data/key:data..."
	int m_iTile;                        // next part to send
	bool m_bHead;                       // head "S|S|parts" sent
	bool m_bDone;                       // all sent (tile texts freed); kept so a J1 ask from <= S gets placeholders again:
	                                    // real rut ops there would make other rows on a joiner without the earlier ruts
}

//! All-voxel test (2026-10-13): one vehicle's wheel points since its last rut op (server)
class MUD_VoxelRutTrack
{
	Vehicle m_Veh;                      // weak: a deleted vehicle drops its track
	ref array<vector> m_aLast = {};     // per wheel: last point a segment ended at
	ref array<int> m_aHas = {};         // per wheel: 1 = m_aLast is set
	ref array<vector> m_aSegA = {};
	ref array<vector> m_aSegB = {};
	ref array<vector> m_aAheadA = {};   // strips ahead of the leading wheels (RutAhead), newest only
	ref array<vector> m_aAheadB = {};
	ref array<vector> m_aPendA = {};    // RUT_BEHIND: segments waiting for the whole vehicle to pass
	ref array<vector> m_aPendB = {};
	ref array<int> m_aPendAt = {};      // tick each was made
	vector m_vTail;                     // rearmost wheel contact along the travel direction (RutTail)
	vector m_vTailDir;
	bool m_bTail;
	int m_iFlushAt;
	float m_fStep;
	float m_fCap;
	ref array<vector> m_aHeldA = {};    // RUT_LOOK, per wheel: its newest segment, held back one step (m_aHeldHas)
	ref array<vector> m_aHeldB = {};
	ref array<int> m_aHeldHas = {};
	ref array<vector> m_aLookA = {};    // RUT_LOOK: segments to show now (op 6)
	ref array<vector> m_aLookB = {};
	int m_iNear;                        // RUT_BEHIND log: segments cut at the voxel gap / the fresh-grass gap /
	int m_iFar;                         // forced (timeout or RUT_PEND_MAX) since the last op
	int m_iForced;
	int m_iSheetId;                     // synced mud sheet: this vehicle's number in its rut ops' tag (MUD_MudSync.MakeTag)
	int m_iRutAt;                       // R1: under the sheet, no feel rut op before this tick (RUT_SHEET_MS)
	int m_iWaits;                       // R1 log: flushes that held the rut op under the sheet / segments RutWheel had to drop
	int m_iDropped;
}

class MUD_VoxelProbe
{
	static const bool ENABLED = true;       // back on 2026-10-13 after the band mesher test (user: digging back); was false for the band mesher test. Tunnels (T) need it.
	// false (user test 4, 2026-10-13 20:42): craters, trenches and shovel digs on ground no tunnel has converted stay on
	// the heightfield (ruts keep working round and in them); only tunnels convert. true = every op makes voxels.
	static const bool SURFACE_OPS = false;  // user 2026-10-21: craters / trenches / shovel back on the heightfield (v010 feel), voxels only for tunnels
	                                        // (and ops inside columns a tunnel converted). git 3ab5bb8 = before, v273 = after (true since the all-voxel test 2026-10-13 22:30)
	// All-voxel test (user 2026-10-13 22:30, checkpoint v031 = before): wheels cut ruts into the voxels (op 4), the
	// heightfield takes no rut. 0.5 m samples: a rut shows as a soft dip ~1 m wide, no berm (user picked 0.5 m).
	static const bool RUTS = true;      // back on 2026-10-19 after the mesh-rut feel test (results/2026-10-19-mesh-vs-voxel-ruts-ds.md). user 2026-10-15: back to the v035 voxel ruts (ground heightfield ruts cost ~13 fps, docs/superpowers/results/2026-10-15-freeze-test.md); was false for the MUD DRAG TEST
	static const float RUT_STEP = 0.5;          // a track point every this many m a wheel travels
	static const float RUT_JUMP = 2;            // farther than this since the last point (air, teleport): track restarts
	static const int RUT_EVERY_MS = 250;        // one rut op per vehicle at most this often
	// CRUDE UNDER SHEET (2026-10-18, user 'the hidden rut physics can be cruder for perf'; results/2026-10-18-h1-snap-probe.md:
	// CarveFloor = the op hitch): a rut op whose segments are all under the mud sheet skips RelaxFloor + GrowRidges (the
	// sheet draws its own ridges). Same zones + same seq on every machine = same floor everywhere. (SHEET-ONLY TEST and
	// the CRUDE_AB_OPS A/B: archived 2026-10-21, Github/Terrain deformation/Archived code/2026-10-21-cleanup.)
	static const bool RUT_SHEET_CRUDE = true;
	protected bool m_bOpCrude;                  // this op (log)
	protected int m_iCSeg;                      // this op's CarveFloor parts (ms, 'MUD op time')
	protected int m_iCWrite;
	protected int m_iCRelax;
	protected int m_iCRidge;
	protected int m_iCLog;                      // the 'MUD rut floor' print at its end (server carve had ~2 ms outside the parts)
	static const int RUT_SHEET_MS = 750;        // R1 perf 2026-10-18 (research/2026-10-18-mud-zone-rut-perf.md): under the mud sheet
	                                            // a vehicle's feel rut op goes at most this often (the sheet shows the track), or
	                                            // sooner once RUT_MAX_SEGS segments are ready. Server only (op content). <= RUT_EVERY_MS = off
	static const float RUT_R = 0.3;             // rut radius. 0.5 until 2026-10-17 (user: too wide for the tyres; offline
	                                            // heavy 6 passes with relax 40: 0.78 m wide, max wall 50 deg - build-3 plan Stage 2)
	static const float RUT_SHARE = 0.4;         // Build 3 Stage 1 (R4): a pass cuts at least this share of what is left to
	                                            // the cap (heavy: cap after 4 passes, was 6; light unchanged).
	                                            // results/2026-10-17-scm-lite-proto.md. Build const = same on every machine
	// RUT RAMP (user 2026-10-18 'when we make the ruts it shouldnt bump so much', v221 = before). With the cut at the
	// wheel (RUT_BEHIND false) every flush cut full depth right up to the tyre and stopped dead: a step under the tyre
	// every RUT_EVERY_MS (inference: the pumping). Now each op fades its cut out over the last RUT_RAMP_LEN m of each
	// wheel's chain and fades it in over RUT_RAMP_LEN m behind the chain's start (= the previous op's fade-out). The
	// two weights add to 1, so the finished rut is as deep as before (approx: RUT_SHARE and the whole-cm round are not
	// exactly additive). Op segments only = same on every machine (J2). false = full depth to the tyre.
	static const bool RUT_RAMP = true;
	// RUT PASS MAX (user 2026-10-18 DS: 'have the first rut be smooth', v223 = before): one pass cuts at most this deep
	// (before the RUT_RAMP weight), so a fresh rut starts shallow and deepens with traffic up to the cap. Default first
	// pass was max(Rut Step 0.08, RUT_SHARE x cap) = 0.11 m for an 8 t truck. A fixed cap keeps the ramp weights
	// adding up. 0 = off.
	static const float RUT_PASS_MAX = 0.03;     // was 0.06 (v223-v225); now the cap for every pass AFTER the first
	// FIRST PASS TO DIRT (user 2026-10-19 DS: 'the first rut should go down to the dirt and then slowly after each pass
	// we go deeper', v225 = before): where the rut is still shallower than RUT_FIRST_D x profile (the uncut ground), one
	// pass cuts straight to that depth (centre 0.10 m: the groove reveal is full dirt from ~0.09 m with the ragged edge;
	// 0.06 left grass patches); deeper than that, each pass cuts at most RUT_PASS_MAX. Depends on the depth so far, not
	// a pass count, so the RUT_RAMP weights still add up (two half passes = one first pass). RUT_FIRST_TOL: the whole-cm
	// round can leave a first-pass floor up to 0.005 short - that must not count as uncut. 0 = off.
	static const float RUT_FIRST_D = 0;     // WEAR (user 2026-10-19 evening, v230 = before): off - grass first, dirt after repeated passes; was 0.15 (0.10 -> 0.12 -> 0.15)
	static const float RUT_FIRST_TOL = 0.01;
	// WEAR COUNT (user 2026-10-19 'if i drive over grass the first time i dont break the ground, but repeated use will';
	// perf plan 2026-10-19-rut-wear-perf-plan.md idea 1, v233 = before): a floor sample with no rut floor yet only counts
	// passes (m_aWear += the RUT_RAMP weight, so two half passes = one) until WEAR_PASSES; the pass after that cuts. A
	// wear-only pass writes no floor = no FloorDirty, no QueueBox, no patch rebuild. Wear is J2 state: snapshot '~' list,
	// self-test compare, disp hash. The trail rule rides a worn track; look ruts skip worn-only samples. 0 = off.
	static const float WEAR_PASSES = 1;
	static const float WEAR_TOL = 0.05;     // 0.95 counts as a pass (ramp weights + the 0.01 round)
	static const int WEAR_SALT = 70001;     // disp hash: a wear value is not a floor value
	protected static int s_iWearAdd;        // 10 s line: samples a pass only wore / samples cut
	protected static int s_iWearCut;
	protected static int s_iWearLogAt;
	// LEAD AXLE RUTS (user decision 2026-10-18, v223 = before): only the leading axle cuts - the front wheels driving
	// forward, the rear wheels backing up; the trailing wheels ride the leading rut. Server feel ruts and client look
	// ruts use the same rule (LeadAxle). Fewer rut ops and patch rebuilds. false = every wheel.
	static const bool RUT_LEAD_AXLE = true;
	static const float RUT_AXLE_TOL = 0.5;     // m of vehicle-local z: contacts this close to the leading one = its axle
	protected static ref array<float> s_aLeadZ = {};
	// TRAIL CUT (user 2026-10-19 screenshot: in a turn the rear wheels run inside the front rut, on grass; v226 = before):
	// a trailing wheel cuts too when the ground RUT_TRAIL_AHEAD m ahead of it (along the travel) is not already a rut at
	// least RUT_TRAIL_IN x RUT_FIRST_D deep (0.105 m = within ~0.15 m of a rut centre). Ahead, so its own fresh cut never
	// counts. Straight driving: rear wheels ride the front rut (no cut, the RUT_LEAD_AXLE saving stays); turning: they cut
	// their own line. Server reads the rut floor, look ruts the drawn floor (MeshFloor). Input side only (who cuts), ops
	// stay floor-blind (J2). false = leading axle only.
	static const bool RUT_TRAIL_CUT = true;
	static const float RUT_TRAIL_AHEAD = 0.5;
	static const float RUT_TRAIL_IN = 0.7;
	static const bool TRAIL_PROBE = true;          // TEMP 2026-10-19: TrailProbe lines (test kit only)
	static const int TRAIL_PROBE_MS = 500;
	protected static ref array<int> s_aTrailPrintAt = {-100000, -100000};
	static const float RUT_RAMP_LEN = 1.0;
	static const float RUT_REPOSE_TAN = 0.8390996;  // Build 3 Stage 2: tan 40 deg (was 30 with RUT_R 0.5; user 2026-10-17
	                                            // narrower ruts: 30 widened RUT_R 0.3 back to 0.94 m) - after the relax no rut depth step
	                                            // to a 4-neighbour floor sample is over this x FCELL
	static const int RUT_REPOSE_ITERS = 4;      // relax passes per rut op (0 = off). Proto: 30 deg x4, 4 neighbours, heavy
	                                            // max wall 49.9 -> 40.3 deg, rut 0.98 -> 1.19 m (results/2026-10-17-scm-lite-proto.md)
	// Build 3 Stage 3 (user OK 2026-10-17; MEASURED offline, results/2026-10-17-scm-lite-proto.md "Re-run 2026-10-17"):
	// bulldozed ridges. Each rut op raises a band RUT_R + GAP .. + W beside its segments by B x (deepest cut this op) x
	// pi / 3 x RUT_R / W x sin(pi (d - RUT_R - GAP) / W) - it GROWS each pass, capped RUT_RIDGE_MAX over the undug
	// ground. Plain RUT FIELD ground only (no voxel column round the sample): no bricks, no Displace, no rows (J2
	// placeholders untouched). false = v143 exactly
	static const bool RUT_RIDGES = true;
	static const float RUT_RIDGE_B = 0.4;       // share of the cut volume pushed up beside the rut
	static const float RUT_RIDGE_GAP = 0.2;     // flat strip between the rut edge (RUT_R) and the ridge
	static const float RUT_RIDGE_W = 0.5;       // ridge band width
	static const float RUT_RIDGE_MAX = 0.08;    // highest ridge over the undug ground
	static const float RUT_RIDGE_FLAT = 0.02;   // patches draw a ridge only where the heightfield is this close to undug
	static const int RUT_RIDGE_SALT = 50021;    // disp hash: a ridge value is not a floor value
	static const int RUT_RIDGE_GONE = 7927;     // disp hash: a ridge back to none (7919 = a floor back to none)
	static const int RUT_MAX_SEGS = 40;         // segments per op (the rest of a long frame gap is dropped)
	static const float RUT_ON = 0.6;            // CarveRut: wheel this far off a column's surface = not on it (skip)
	// convert ahead (user 2026-10-13 23:00, v032 = before): the 2 leading wheels' strip ahead goes VOX before they
	// get there (first pass over fresh grass felt rough: heightfield -> voxel swap under the tyre, 2 cm LIFT step)
	// 2026-10-14: off again - RUT_LOOK look cells show the look rut on fresh grass without converting the physics
	static const bool RUT_AHEAD = false;
	// cut behind (user 2026-10-13 23:05, v033 = before): a rut segment is cut only once the rearmost wheel is
	// RUT_BEHIND_GAP past it, so the ground under the tyres never changes while driving (no sinking into your own rut)
	// REAL CUT AT THE WHEEL (user 2026-10-18, v211 = before): false = every flush (RUT_EVERY_MS) cuts the segments the
	// wheels just drove - the tyres sink into their own rut. Old stutter / snaps were seen on DS + own PC; user tests it
	// on a live server now. true = wait for the rearmost wheel RUT_BEHIND_GAP past.
	static const bool RUT_BEHIND = false;
	// 0.5 m was still at the rear wheel (user 2026-10-13): a cut reaches RUT_R + 1 sample past the segment and flips
	// whole 2 m skin cells to voxels (MarkCarved), so the rear tyre's cell could switch under it. 3 m clears that.
	// 2026-10-14 (user felt lag with the look test, "delay more"): 4 m. RUT_LOOK draws the rut at once, so the feel
	// cut can wait longer
	static const float RUT_BEHIND_GAP = 1.5;  // user 2026-10-17: ruts showed too late (3 = v035, 4 was for RUT_LOOK). RUT_FIELD
	                                          // never flips skin cells (no MarkCarved); a cut reaches RUT_R + 2 relax samples = 0.8 m
	// smart gap (user 2026-10-13 23:40: 3 m behind looked too far): a segment whose cut reaches only skin cells that
	// are voxel already (SegOnVox) flips no cell, so it needs only RUT_R + 1 sample clear of the rear wheel
	static const float RUT_BEHIND_GAP_VOX = 1.5;  // = RUT_BEHIND_GAP: smart gap off (v035 had none; 2026-10-14 4, was 1)
	static const int RUT_BEHIND_MAX_MS = 4000;  // waiting longer than this (parked, reversing back): cut anyway ...
	static const float RUT_BEHIND_CLEAR = 6;    // ... but only once both ends are this far (flat) from the vehicle
	                                            // origin: slow driving took > 4 s to pass a front-wheel segment
	static const int RUT_PEND_MAX = 200;        // waiting segments per vehicle; past this the oldest are cut
	// look now, feel later (user 2026-10-13 23:45, v036 = before): each wheel's segment one step behind it is drawn at
	// once (op 6: mesh only, from a look field over the feel one); the cut the wheels feel stays op 4 under the
	// RUT_BEHIND rule, the same moment on every machine. Draws only in cells already voxel (convert ahead).
	static const bool RUT_LOOK = false;     // v035 had no look rut (user 2026-10-15: ruts as v035)
	static const float V_NONE = 1000000;        // brick m_aV: no look cut at this sample
	static const float RUT_AHEAD_W = 0.75;      // strip half width
	static const float RUT_LEAD_MIN = 2;        // strip length = this + RUT_LEAD_PER_S x speed, capped (as the ahead ruts)
	static const float RUT_LEAD_PER_S = 0.5;
	static const float RUT_LEAD_MAX = 8;
	static const int RUT_KEY_W = 65536;         // CarveRut column keys, op local (m_iRutI0 / K0): global keys passed 2^31
	static const int RUT_KEY_OFF = 1024;        // at 0.25 m cells past ~7.9 km
	static const bool GROUND = false;   // the old ground test (fine patches off) is gone; MUD_WorldGroundEntity.HasHole reads it
	// ---- SIZE SWITCH: Tools/voxel_size.py coarse|fine flips every '// SIZE coarse | fine' line (here and in the data
	// classes above); Init checks they match and turns the probe off if not. Fine = 0.25 m cell probe (2026-10-14).
	static const int SUB = 8;           // SIZE 8 | 16 - cells per brick side
	static const float CELL = 0.5;      // SIZE 0.5 | 0.25 - sample spacing (0.5: sample columns sit on fine vertices)
	static const int MAX_V = 8000;     // SIZE 8000 | 10000 - verts per brick build (MUD_VoxelVerts: class <= 256 KB)
	static const int MAX_I = 24000;     // SIZE 24000 | 30000 (static arrays <= 65536)
	static const int SLOW_MS = 4;       // SIZE 4 | 16 - 'slow brick' log from this many ms (fine bricks are ~4x the work)
	// ---- end SIZE SWITCH
	static const float BRICK = 4;       // SUB * CELL: 2 skin cells (Init checks)
	static const int SUB2 = SUB * SUB;
	static const int SUB3 = SUB2 * SUB;
	static const int SPS = SUB / 2;     // samples per skin cell side
	static const int GN = SUB + 2;      // gather grid side: the brick's samples + one ring
	static const int GN2 = GN * GN;
	static const int GN3 = GN2 * GN;
	static const int CN = SUB + 1;      // cell cache side: the brick's cells + one ring below
	static const int CN2 = CN * CN;
	static const int CN3 = CN2 * CN;
	// DISPLACED RUTS (plan docs/superpowers/plans/2026-10-15-displaced-ruts-plan.md, checkpoint v086 = before): op 4
	// writes a 2D rut floor (FCELL grid, world y, CarveFloor) instead of the 3D samples; a brick the floor reaches is
	// split 2 x 2 and its vertices pulled down to it (Displace). false = the v035 voxel ruts (CarveRut), exactly.
	static const bool DISPLACE_RUTS = true;     // Stage C / plan Step 4 (user 2026-10-15); false = v035 ruts exactly (checkpoint v089)
	static const float FCELL = 0.25;            // floor sample spacing = CELL / 2 (SizesOk checks)
	static const int FSUB = SUB * 2;            // floor samples per column side
	static const float FLOOR_NONE = 1000000;    // no rut floor at this sample
	// RUT FIELD (plan docs/superpowers/plans/2026-10-15-rut-field-plan.md, checkpoint v098 = before): ruts stop
	// converting voxel columns, the floor lives in its own tile map (m_mRutTiles) and the heightfield patches draw it.
	// Needs DISPLACE_RUTS. false = v097 (Stage 1: the tile map is filled exactly where v097 filled the columns).
	static const bool RUT_FIELD = true;    // Stage 2 on (user 2026-10-15); false = v097 (checkpoint v098)
	// Review F5 (2026-10-16): the per-op log lines ('MUD voxel probe: ...', 'MUD rut field: N samples', 'MUD rut floor')
	// come ~4 per second per driving vehicle = DS log spam. true in dev; FLIP TO false BEFORE RELEASE (master list Lane 2).
	// OP_PRINTS (perf quick win 2026-10-18, research/2026-10-18-perf-assessment.md item 1: a Print ~2 ms on a DS): false =
	// no per-rut-op lines - test-kit 'MUD op time' per rut op, GoLive 'MUD voxel probe: live' on batches that made no
	// bricks, 'MUD rutmat', JoltProbe 'MUD corr visual only'. Slow ops (OP_SLOW_MS) still print. true = lines back.
	static const bool OP_PRINTS = false;
	static const bool OP_LOG = false;    // false 2026-10-18 (quick win 1: the 'MUD rut floor' Print = ~2.2 ms per rut op on the DS, results/2026-10-18-h1-snap-probe.md run 5); true = per-op lines back
	static const float DISP_GUARD = 1.0;        // pull only vertices less than this above the floor (tunnel roofs stay)
	static const float DISP_EPS = 0.001;
	static const float DISP_SOIL_CUT = 0.05; // a top tri this far (m) under the seeded ground is groove: soil (DispEmit)
	static const int DISP_PAD = 3;              // Displace floor grid: the brick's floor samples + this many each side
	static const int DISP_N = FSUB + DISP_PAD * 2;
	static const int DISP_N2 = DISP_N * DISP_N;
	// DISPLACED RUTS debug (Stage B): diag menu GameCode > Mud > Displaced ruts (diag builds only: Workbench, Diag exe),
	// every switch off by default. A dedicated server has no menu: the *_ALWAYS consts turn its log / hash on.
	static const bool DISP_LOG_ALWAYS = false;
	static const bool DISP_HASH_ALWAYS = false;
	// HASH LOG (perf plan A1 2026-10-18, plans/2026-10-18-perf-plan.md): the test kit no longer turns on the per-op
	// 'MUD disp hash' / 'MUD field hash' lines by itself (a Print ~2 ms on a DS, one per rut op). true = lines back -
	// set it for J2 / compare_op_hashes runs only.
	static const bool HASH_LOG = false;
	static const int DSW_LOG = 0;               // DispOn switch numbers (= menu id - SCR_DebugMenuID.MUD_DISP_LOG)
	static const int DSW_FLOOR = 1;
	static const int DSW_BRICKS = 2;
	static const int DSW_HOLE = 3;
	static const int DSW_HASH = 4;
	static const int DISP_LOG_EVERY_S = 10;     // 'MUD disp:' summary line period (only when something happened)
	static const float DISP_FLOOR_DRAW_R = 12;  // draw floor: samples this far round the controlled entity
	static const int DISP_FLOOR_MAX = 4000;     // ... at most this many
	static const float DISP_BRICK_DRAW_R = 30;  // draw bricks: overflow / fold bricks this far round it
	static const int DISP_MARK_MS = 5000;       // brick / hole marks stay this long
	static const int DISP_MARK_MAX = 300;
	static const int DISP_HOLE_EVERY_MS = 500;  // hole check (and draw floor refresh) period
	static const float DISP_HOLE_TOL = 0.25;    // hit this far off the expected top = hole (v035 ruts baseline 2026-10-15: 0.10 .. 0.16, docs/superpowers/results/2026-10-15-disp-hole-baseline.md)
	static const int DISP_HOLE_MAX_LOG = 5;     // HOLE lines per check
	static const int DISP_HASH_M = 1000003;
	static const float LIFT = 0.02;     // seeded surface above our ground
	static const int GRAVE_MS = 1000;   // a dropped brick entity is deleted this long after (MUD_VoxelGrave)
	static const float FLOOR = 3;       // nothing digs deeper than this under the undug ground
	// U1 deep tunnels (user 2026-10-17: 10 m; was 5 m). Build consts = the same on every machine (rule 3).
	// results/2026-10-17-u1-deep-tunnels.md (10 m: Carve gj1 with TUNNEL_FLOOR still opens a crater at a 10 m shaft bottom)
	static const float TUNNEL_FLOOR = 10;       // tunnels (CarveTunnel) and the C4 sphere dig this deep under the undug ground
	static const float SEA_GAP = 1;             // sea guard (P1): no tunnel / sphere air lower than this above the ocean base height
	static const float MAX_DEPTH = 3;   // crater depth clamp
	static const float FUNNEL = 0.5;    // crater radius growth per metre above the hit point (no undercut) - old Carve; MUD_MudSheet copies it
	// Build 1 crater (2026-10-17): the heightfield's cosine bowl (C1), = MUD_WorldGround attribute defaults (Crater Max
	// Slope, Rim Share, Rim Width Share). Consts, not attributes: every machine carves the same (rule 3).
	static const float CRATER_SLOPE = 0.9;      // steepest bowl wall, rise per m (42 deg; offline <= 44 deg drawn)
	static const float CRATER_RIM_SHARE = 0.17; // rim height / depth
	static const float CRATER_RIM_W_SHARE = 0.4; // rim width outside the crest / depth
	static const float CRATER_FADE = 1.0;       // C3: a crater fades to nothing within this of a hard (road / kept) column
	// PIT shape back (user 2026-10-18, photo of real shell craters: steep lip, deep flat-ish floor; the Build 1 cosine
	// bowl looked like a soft saucer). true = the stable backup's (2026-10-13) half ellipsoid + funnel + sine rim
	// (crest 2.2 x depth, rim 0.2 x depth high, depth wide); outline stretch, C3 fade, C4 roof, U1 guards stay.
	// false = Build 1 cosine bowl (v215 = before).
	static const bool CRATER_PIT = true;
	static const float PIT_CREST = 1.6;         // crest radius / depth (backup 2.2; user 2026-10-18: too wide, v216 = before)
	static const float PIT_RIM_SHARE = 0.2;     // rim height / depth
	static const float PIT_RIM_W = 0.5;         // rim width / depth (backup 1.0; user 2026-10-18: mounds too wide)
	// Deep holes (user 2026-10-18: a mortar in a 2.5 m trench dug nothing - the stack cap ~2.4 m + FLOOR 3 stopped it).
	// A hit on ground already dug to the stack cap or deeper (trench, shovel pit, crater pile) digs its full depth below
	// the hit; no crater air deeper than CRATER_FLOOR under the undug ground (sea guard kept).
	static const float CRATER_FLOOR = 5;
	static const float PIT_INNER = 0.8;         // the rim bump starts at this share of the crest radius
	// Bumpy rim + uneven floor (user 2026-10-18, v219 = before): smooth sum-of-sines noise (~1-4 m bumps) with phases
	// from the op's outline roll (server-rolled, in the op) = the same on every machine, no random on clients.
	static const float PIT_RIM_NOISE = 0.5;     // rim height x 1 +- this
	static const float PIT_FLOOR_NOISE = 0.12;  // floor up / down by this x depth in the middle, 0 at the crest
	static const float ROOF_MIN = 0.75;         // C4: a hit this far under the undug ground ...
	static const float ROOF_GAP = 0.5;          // ... with solid ground from this ...
	static const float ROOF_REACH = 3;          // ... up to this far above it is under a roof: a sphere, not a crater
	static const float CARVE_TOL = 0.05;  // a sample next to the surface moving more than this marks its cells carved
	static const float KEPT_SINK = 0.3;// kept terrain columns: seeded this far under the terrain, which carries them
	// tunnel test (2026-10-13): hold T on foot = dig a vertical capsule in front of you every TUNNEL_EVERY_MS.
	// Look up / down to dig a ramp. T in vanilla is only SkipScene (cutscenes), read from the pak 2026-10-13.
	static const string TUNNEL_CONTEXT = "MUD_VoxelTunnelContext";
	static const string TUNNEL_ACTION = "MUD_VoxelTunnel";
	static const float TUNNEL_R = 0.8;          // capsule radius
	static const float TUNNEL_HALF = 0.45;      // capsule half length (vertical): 2.5 m tall, floor at the feet at pitch 0 (TUNNEL_MID)
	static const float TUNNEL_AHEAD = 1.1;      // capsule middle this far ahead of the feet (flat look direction)
	static const float TUNNEL_MID = 1.25;       // capsule middle above the feet (U2 2026-10-17: was 0.95 = floor 0.3 m under the feet, a level look sank ~18 deg; 1.25 = level, Tools/u2_level_check.py)
	static const float TUNNEL_SLOPE = 1;        // m up / down per click at the steepest look (pitch clamped to the slope choice TunnelDrop / TUNNEL_SLOPE)
	static const float TUNNEL_DOWN_MAX = 1.0;   // server: the steepest slope choice (TunnelDrop); the client's choice is not sent
	static const int TUNNEL_STEEP_COUNT = 5;    // slope choices, X in shovel TUNNEL mode (user 2026-10-17: "let the player choose")
	static const int TUNNEL_STEEP_START = 1;    // 0.5 m per click (~27 deg)
	static const string TUNNEL_STEEP_CONTEXT = "MUD_TunnelShovelContext";
	static const string TUNNEL_STEEP_ACTION = "MUD_TunnelSteep";
	static const float TUNNEL_LAG = 0.5;        // server down check: feet may lag the client's by this much
	static const bool TUNNEL_DEBUG_T = true;    // debug key, user 2026-10-16: OFF before release (false = T does nothing, the server refuses it)
	static const int TUNNEL_EVERY_MS = 300;
	// Tunnel darkness (user 2026-10-17, probe): client / host spawns one MudTunnelProbe.et (vanilla ProbeEnvCave_base values
	// + a 5 m ProbeVolume box; script cannot size a ProbeVolume, no methods) per TUNNEL_DARK_STEP grid cell a tunnel dig
	// under a roof (RoofOver) reaches. Unproven: a runtime-spawned probe darkens anything.
	static const bool TUNNEL_DARK = false;    // PARKED 2026-10-17 (user): no tunnel darkening - docs/features/trenches-tunnels/README.md
	static const ResourceName TUNNEL_PROBE = "{5AFD9F654A6D42F7}Prefabs/Mud/MudTunnelProbe.et";
	// darker the deeper (user 2026-10-17: 'pitch black at 10 m'): feet depth under the undug ground picks a child prefab
	// (_1.._4 lower LVDiffuse / LVReflection / TopColorEV; tune the EVs in the prefabs). Depth from TUNNEL_DARK_D1 up.
	static const ResourceName TUNNEL_PROBE_1 = "{1FBBFDEBCD8F6776}Prefabs/Mud/MudTunnelProbe_1.et";
	static const ResourceName TUNNEL_PROBE_2 = "{153D30A1A6B1E88F}Prefabs/Mud/MudTunnelProbe_2.et";
	static const ResourceName TUNNEL_PROBE_3 = "{6C3251C97CD9B1FC}Prefabs/Mud/MudTunnelProbe_3.et";
	static const ResourceName TUNNEL_PROBE_4 = "{7976E85D844A3442}Prefabs/Mud/MudTunnelProbe_4.et";
	static const float TUNNEL_DARK_D1 = 1;     // m feet depth: level 1 from here, then one level darker every TUNNEL_DARK_BAND (was 3)
	static const float TUNNEL_DARK_BAND = 1;   // levels at 1 / 2 / 3 / 4 m: level 4 = darkest from 4 m (user 2026-10-17; was 2 = 3 / 5 / 7 / 9 m)
	// the probe box (MudTunnelProbe.et Size 5 3 5) must sit wholly under ground (user shot 2026-10-17: a tunnel dug from a
	// trench blacked the trench wall): ground top over 9 points (middle, edges, corners) at least ROOF above the middle
	static const float TUNNEL_DARK_HALF = 2.5;  // m, half the box side (keep = prefab Size x / 2)
	static const float TUNNEL_DARK_ROOF = 1.5;  // m, half the box height (keep = prefab Size y / 2; 1.0 tried 2026-10-17: no visible gain)
	// sun-shadow LID PROBE (user 2026-10-17): our heightfield (MatTerrainObject) casts no sun shadow (PROVEN WB), so sun
	// shines through the roof. Does a script MeshObject with MAT_DIRT cast? One up-facing square per probe, TUNNEL_LID_SINK
	// under the lowest of BoxRoofed's 9 ground tops (hidden under the surface; seen from a tunnel below = back face, culled).
	static const bool TUNNEL_LID = false;   // off 2026-10-17: lids did not stop the spots, and blasts expose them as flat squares
	static const float TUNNEL_LID_HALF = 2.5;  // m, half the square side (= TUNNEL_DARK_HALF: only that ground was checked)
	// VC flags readback probe (IDA 2026-10-17: SetVComponentFlags ORs into the mesh instance flags and returns them,
	// so (0) only reads; the shadow caster gather skips an instance with bit 8 = NOSHADOW). Log only.
	protected static int s_iVcLog;
	static const float TUNNEL_LID_SINK = 0.15; // m under the lowest ground top (BoxRoofed: >= ROOF above mid, so over the capsule top)
	static const float TUNNEL_DARK_STEP = 4;     // m, probe grid across (box 5 m; was 2 = up to ~18 boxes over one point: lit spot inside cover, gone at 4 / 2.5 - WB OK 2026-10-17)
	static const float TUNNEL_DARK_STEP_Y = 2.5; // m, probe grid up (box 3 m; WB OK 2026-10-17)
	static const int MAX_COLUMNS = 6000;        // converted columns per match (~250 craters); past this the heightfield
	static const int MAX_OP_COLUMNS = 400;      // one op converting more (a box past ~80 m): the heightfield
	static const int SEED_CHARS = 4000;         // seed text longer than this: the heightfield (measure, then tune)
	static const int REFUSED = -9;              // Trench / FreeDig / FreeFill: not voxels, the heightfield digs it
	static const int URGENT_MS = 10;            // brick builds per frame until this is spent (at least one)
	static const int JOIN_MS = 25;              // build / drain budget per frame while a client joins
	static const int DRAIN_MS = 4;              // MP client: received ops applied per frame until this is spent
	static const int JIP_CHARS = 1500;          // MP: ops per JIP message, joined with '#' (one longer op goes alone)
	// Build 3 Stage 6 (T2, user 2026-10-17: both sides, none for the shovel square): spoil parapet beside a planned
	// trench. Starts PARA_BERM past the wall top of the plan's full depth; rises over PARA_RISE, flat PARA_TOP, falls
	// over PARA_RISE; height = this swing's depth x PARA_K, at most PARA_MAX. false = no parapet (v150 = before).
	static const bool PARAPET = true;
	static const float PARA_BERM = 0;       // was 0.25: left a grass strip between the wall top and the mound (user 2026-10-17)
	static const float PARA_RISE = 0.5;
	static const float PARA_TOP = 1.0;
	static const float PARA_K = 0.25;
	static const float PARA_MAX = 0.4;
	static const float TRENCH_PAD = 0.25;      // T1 2026-10-17: trench cut / fill take columns within this of a piece as inside (drawn floor = plan width within +0.10 m on average, +-0.15 m by grid offset; results/2026-10-16-fix-checks.md)
	static const int JIP_MSGS_PER_FRAME = 4;    // MP server: JIP messages per frame per asking player (J1 2026-10-17; was the whole log in one frame)
	static const bool JIP_SNAPSHOT = true;      // J2 2026-10-17: a first ask (seq 1) gets voxel ops + rut tiles, not every rut op
	static const int SNAP_CHARS = 1500;         // J2: rut tiles per snapshot message, joined with '/' (= JIP_CHARS, a size already sent)
	// 2026-10-18 (WB 11:00: this self-test = the 430-460 ms 'vox' frame every 30 s while driving in the zone; v189 = before,
	// was 30000): 120 s. Test kit SP / WB only - a real join builds its snapshot once, in ServerJipAsk.
	static const int SNAP_TEST_MS = 120000;     // J2 test kit: server snapshot self-test at most this often (when the seq moved)
	static const int HELD_CAP = 20000;          // MP client: ops held out of order; past it they are dropped and asked again (J1)
	static const int RESEND_WAIT_MS = 15000;    // MP client: a seq gap this old asks the server again from that seq (J1)
	static const float MISS = 100000;           // gathered sample of a column that is not converted
	static const float ROW_PAD = 0.5;           // rows reach this far past the seeded tops of a column and its neighbours
	static const float KEEP_ROW = 1.0;          // Put: a value this far from 0 on both sides of a change makes no row
	static const string ALPHA = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	static const int ALPHA_MID = 30;            // ALPHA index of seed delta 0 (deltas -30 .. 31 cm are one char)
	static const int KEY_W = 8192;              // column / cell keys: (z + KEY_OFF) * KEY_W + x + KEY_OFF
	static const int KEY_OFF = 64;
	static const int CELL_KEY_W = 16384;
	// Step C (2026-10-14): after an op writes its samples, blur the changed box + 1 (feel field only, so every machine
	// and JIP get the same ground): new = (1 - SMOOTH_W) v + SMOOTH_W x mean of the 6 neighbours, only samples near the
	// surface (|v| < CELL x 1.2), never in hard columns or next to a column that is not converted.
	static const bool SMOOTH_OPS = false;
	static const int SMOOTH_KINDS = 19;         // bit per op kind: 1 crater, 2 tunnel, 4 trench, 8 fill, 16 rut (feel);
	                                            // trench / fill off: one op can span ~80 m (cost untested)
	static const int SMOOTH_PASSES = 1;
	static const float SMOOTH_W = 0.5;
	static const int BAND_NONE = 100000;        // MUD_VoxelGather m_aLa / m_aHs of a column that is not converted
	                                            // (timed apart: same-brick A/B), compares (0 = off); a mismatch prints
	                                            // 'MUD scan check: MISMATCH'. The band result is kept.
	static const int VKEY = 16384;              // cell vertex cache: (stamp * 4 + parity) * VKEY + vertex (VKEY > MAX_V)
	static const int STAMP_MAX = 30000;         // (m_iStamp * 4 + 3) * VKEY stays under 2^31 (caches reset past this)
	// crater decal PROBE (user 2026-10-17): vanilla's mortar decal lands on the surface the crater then hides; at go-live
	// each client lays one on the brick entities under the crater (World.CreateDecal, projected down). 'MUD decal:' lines.
	// run 2 (user 2026-10-17 ~22:40 'just have a decal in our craters'): no trace - the decal goes straight onto every
	// shown brick entity of the columns under the crater (run 1: the trace hit the hidden terrain, CreateDecal null)
	static const bool CRATER_DECAL = true;
	static const ResourceName DECAL_MAT = "{039FA43299C7980D}Assets/Decals/Miscellaneous/Mortar/Decal_Impact_Mortar_01.emat";   // Warhead_Shell_HE.et SpawnDecalEffect (Exp 1.9 pak)
	static const float DECAL_SIZE = 1.0;     // decal size per m of crater crest radius (vanilla M821: Scale 2.75 - tune in WB)
	static const float DECAL_LIFE = 60;      // s, as Warhead_Shell_HE.et
	static const float GRASS_TOL = 0.05;    // Stage 6 (user 2026-10-17): a flat face draws grass only when all 4 corners are within this of the seeded ground (OnGround); dug or raised more (parapet, crater rim, the strip between) draws dirt (was -0.15 .. any, then RAISED_DIRT 0.1 left a grass strip)
	static const float TOP_TILE = 64;   // grass top UVs 0..1 per skin chunk, as our fine patches
	static const float SOIL_TILE = 12;  // soil UVs 0..1 per 12 m = MUD_MudSheet.SIZE, so the dirt grain lines up with the mud sheet (8 until 2026-10-15, user 'layered like a cake')
	static const ResourceName MAT_TOP = "{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat";
	static const ResourceName MAT_DIRT = "{6EBF7ADA9FD582F7}Assets/Structures/Military/Fortifications/DirtMounds/Data/DirtCover_01.emat";   // as our groove layer (user pick 2026-10-12; Soil_01 at tiling 8 also worked, v013)
	// Tunnel walls (DirtMat): ambient light cut by material (DiffuseIBL / SpecularIBL) - kills the lit spots probes could not
	// (all-black IBL 0 fixed them, user 2026-10-17, but looked bad). Roof over the brick's highest vertex picks the level.
	static const ResourceName MAT_DARK_1 = "{4D55445441524B32}Assets/Mud/Data/MUD_TunnelDark1.emat";   // IBL 0.5
	static const ResourceName MAT_DARK_2 = "{4D55445441524B33}Assets/Mud/Data/MUD_TunnelDark2.emat";   // IBL 0.25
	static const ResourceName MAT_DARK_3 = "{4D55445441524B31}Assets/Mud/Data/MUD_TunnelDark.emat";    // IBL 0.1
	static const bool DARK_MAT = false;     // PARKED 2026-10-17 (user) with TUNNEL_DARK: false = MAT_DIRT everywhere
	static const float DARK_ROOF_1 = 1;     // m of ground over the brick's top vertex: level 1 from here
	static const float DARK_ROOF_2 = 2.5;
	static const float DARK_ROOF_3 = 4;   // as our groove layer (user pick 2026-10-12; Soil_01 at tiling 8 also worked, v013)
	static const ResourceName GAMEMAT = "{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat";

	protected static ref MUD_VoxelProbe s_Inst;
	protected static bool s_bTraceHidden;  // MUD_GroundEntity.HideFromTraces: bricks off traces (get in / out)
	protected static bool s_bDigHeld;
	protected static int s_iTunnelSteep = 1;   // = TUNNEL_STEEP_START; local player's slope choice (T and the shovel)
	protected static int s_iDigAt;
	// MP client: ops by seq until they can go in order (held before s_Inst exists too)
	protected static ref map<int, string> s_mPending = new map<int, string>();
	protected static int s_iNextSeq = 1;
	protected static bool s_bAskedJip;
	protected static int s_iGapAt;      // tick a seq gap was first seen (0 = none)
	protected static bool s_bToldGap;
	protected static bool s_bLost;     // J1: an op was dropped at the held cap - asked again from s_iNextSeq (Drain)
	protected static int s_iAskAt;     // tick of the last op log ask (0 = none)
	protected static int s_iAsks;      // op log asks this match (log)
	protected static ref map<int, int> s_mJipFrom = new map<int, int>();   // server: player id -> next seq to send (J1)
	protected static ref map<int, ref MUD_JipSnap> s_mSnap = new map<int, ref MUD_JipSnap>();   // server: player id -> snapshot send (J2)
	protected static int s_iSnapS;      // J2 joiner: seq of the snapshot coming (0 = none): its tiles load once ops <= it are in
	protected static ref array<string> s_aSnapParts = {};   // ... its tile parts by index ("" = not here yet)
	protected static int s_iSnapGot;    // ... parts here
	protected static int s_iSnapWaitAt; // ... tick the ops reached it with parts missing (0 = not waiting)
	protected static bool s_bToldSnap;
	protected static int s_iSnapRows;   // ... rows made from rut placeholders (log)
	protected static int s_iSnapSkipped;   // ... rut ops covered by placeholders (log)
	protected static ref array<ref MUD_VoxelRutTrack> s_aTracks = {};
	protected static int s_iSheetVeh;         // server: last MUD_VoxelRutTrack.m_iSheetId given out (synced mud sheet)
	protected static float s_fFakeAt = -1;     // test kit FakeRutTick: time the fake rut ops start (-1 = not yet known)
	protected static int s_iFakeN;             // ... issued so far
	protected static int s_iFakeSeq0;          // ... m_iSeq before the first
	protected static vector s_vFakeO;          // ... grid corner

	protected ref map<int, ref MUD_VoxelColumn> m_mCols = new map<int, ref MUD_VoxelColumn>();
	// RUT FIELD: rut floor tiles (ColKey), only where a floor sample was ever written
	protected ref map<int, ref MUD_RutTile> m_mRutTiles = new map<int, ref MUD_RutTile>();
	// DIG NOW (2026-10-15): surface height per sample column, read from the field (no traces); cleared every go-live
	protected ref map<int, float> m_mSurf = new map<int, float>();
	protected ref map<string, int> m_mDark = new map<string, int>();   // tunnel darkness: probe grid cells done
	protected ref array<ref Resource> m_aLidRes = {};                  // lid probe meshes (the mesh goes without a ref)
	protected ref array<ref MUD_VoxelOp> m_aOps = {};    // every op, in seq order (JIP)
	protected bool m_bRowLog;           // J2: Submit of a rut op - Row() notes the rows it makes (m_sRowLog -> op.m_sRows)
	protected string m_sRowLog;
	protected int m_iSnapTestSeq;       // J2 test kit: seq of the last self-test
	protected int m_iSnapTestAt;        // ... its tick (0 = none yet)
	protected ref array<MUD_VoxelBrick> m_aQueue = {};  // bricks to build, in order (weak: columns own them)
	protected int m_iQHead;
	protected ref array<MUD_VoxelBrick> m_aShow = {};   // made since the last GoLive: shown at the next
	// MUD_MudSheet hook (2026-10-14): ops since the last GoLive that change the surface (kinds 0-3), told to the mud
	// layer at go-live: x, z box, blast middle, blast radius (craters only, else 0)
	protected ref array<vector> m_aMudLo = {};
	protected ref array<vector> m_aMudHi = {};
	protected ref array<vector> m_aMudWp = {};
	protected ref array<float> m_aMudR = {};
	protected ref array<int> m_aMudKind = {};      // Build 2 Stage 5: op kind, and a fill's rise (m), for MUD_GroundFollow
	protected ref array<float> m_aMudLift = {};
	protected ref array<MUD_VoxelBrick> m_aSwap = {};   // shown bricks with a pending build
	protected ref array<ref MUD_VoxelGrave> m_aGrave = {}; // old brick entities waiting GRAVE_MS to be deleted (Bury)
	protected ref array<int> m_aNewCarved = {};         // skin cells (CellKey) carved since the last GoLive: they go VOX
	protected static ref array<MUD_RutTile> s_aFPTiles = {};   // perf plan B2: FieldPatchBegin's tiles (row-major, may hold null)
	protected static int s_iFPBi0;                     // perf plan B2: first tile column / row, tiles per row
	protected static int s_iFPBk0;
	protected static int s_iFPNx;
	protected static bool s_bFPOn;                     // perf plan B2: false = the field is off (FieldFloor gives none)
	protected ref array<float> m_aRampT0 = {};          // RUT_RAMP: per op segment, chain length before its start (m)
	protected ref array<float> m_aRampL = {};           // RUT_RAMP: per op segment, its wheel chain's length (m)
	protected ref array<int> m_aRampHead = {};          // RUT_RAMP: 1 = first segment of a chain (fade-in behind it)
	protected ref map<int, float> m_mRampW = new map<int, float>();   // RUT_RAMP: floor sample key -> ramp weight
	protected ref array<int> m_aOpCarved = {};          // cells the op being applied carved first (their bricks rebuild)
	protected ref array<int> m_aNewLook = {};           // RUT_LOOK: cells look-carved since the last GoLive (patch LOOK bit)
	protected ref array<int> m_aOpLook = {};            // RUT_LOOK: cells the op being applied look-carved first
	protected int m_aEmitFeel[16];                      // BuildSub: m_aEmit for the body (carved cells)
	protected int m_aEmitLook[16];                      // BuildSub: m_aEmit for the mesh (carved + look cells)
	protected int m_iEmitFeelAny;
	protected int m_iMarkGi;                            // Put: last sample column whose cells were marked
	protected bool m_bSheetOp;                          // ApplyOp: crater / trench / fill (MarkCarved skips sheet cells)
	protected bool m_bOpBoth;                           // Submit: this op went to the sheet too (held, SYNC)
	protected int m_iMarkGk;
	protected int m_aEmit[16];                          // the brick being built: skin cells -1..2 round it, 1 = draw there
	protected int m_iBricks;
	protected ref MUD_VoxelVerts m_Vb = new MUD_VoxelVerts();
	protected ref MUD_VoxelIdx m_IbTop = new MUD_VoxelIdx();
	protected ref MUD_VoxelIdx m_IbDirt = new MUD_VoxelIdx();
	protected ref MUD_VoxelSlots m_Cb = new MUD_VoxelSlots();
	protected ref MUD_VoxelGather m_Gb = new MUD_VoxelGather();
	protected int m_aOff[8];            // cube corner q (bit 1 +x, 2 +y, 4 +z) -> gather index offset
	protected vector m_aOffV[8];        // the same corner as a local position offset
	protected int m_aEa[12];            // the 12 cube edges as corner pairs
	protected int m_aEb[12];
	protected float m_fX0;              // skin grid corner = brick grid corner (top UVs too)
	// synced mud sheet Stage 4 (2026-10-18): column tops before the current dig op changed them (KeepTop, TopsMoved)
	protected bool m_bTopTrack;
	protected ref map<int, float> m_mTopWas = new map<int, float>();
	static const int TOP_KEY = 32768;
	static const float TOP_NONE = -99999;
	protected float m_fZ0;
	protected float m_fCell;            // skin cell side
	protected float m_fSeaY = -1000000;  // U1 sea guard: tunnel / sphere floors never under this (ocean base + SEA_GAP; no ocean: off)
	protected float m_fLastG;           // ground y for a sample column our ground cannot give
	protected bool m_bHeadless;         // dedicated server: bounds meshes, no rendering
	protected int m_iSeq;               // server: ops made so far
	protected ref map<string, ref MUD_TrenchLayout> m_mLays = new map<string, ref MUD_TrenchLayout>();
	protected int m_iRefused;

	// the brick being built
	protected MUD_VoxelBrick m_B;
	protected int m_iSmoothN;           // SmoothBox of the last op: samples changed, ms
	protected int m_iSmoothMs;
	protected int m_iRutI0;             // CarveRut: key origin (sample column of the op's first segment)
	protected int m_iRutK0;
	protected int m_iFloorI0;           // CarveFloor: key origin (floor sample of the op's first segment)
	protected int m_iFloorK0;
	protected int m_iOpKind = -1;       // Submit: kind of the op being applied (Put reads it)
	protected ref array<int> m_aClrI = {};  // DISPLACE_RUTS: sample columns this op's Put moved (ClearFloor)
	protected ref array<int> m_aClrK = {};
	protected float m_aDispF[DISP_N2];  // Displace: floor samples round the brick being built
	protected float m_aDispY[MAX_V];    // Displace: each vertex's y before the pull (fold check)
	protected ref MUD_VoxelIdx m_IbDispTop;     // Displace: the coarse index lists while the split ones are written
	protected ref MUD_VoxelIdx m_IbDispDirt;    // (made on first use)
	protected ref map<int, int> m_mDispSoil = new map<int, int>();  // Displace: soil-UV copy of a vertex, key v * 4 + kind - 1
	protected vector m_vBOrg;           // its world corner
	protected int m_iStamp;
	protected int m_iV;
	protected int m_iTop;
	protected int m_iDirt;
	protected int m_iDegen;
	protected bool m_bOverflow;
	protected bool m_bAnyMiss;          // some gathered sample is MISS: quads check their cells
	protected bool m_bLookDiff;         // Gather (feel): a look cut (RUT_LOOK) lies under a gathered sample

	// the op being applied: box of the samples it changed (global indices)
	protected bool m_bChanged;
	protected int m_iCi0;
	protected int m_iCi1;
	protected int m_iCj0;
	protected int m_iCj1;
	protected int m_iCk0;
	protected int m_iCk1;

	// batch: ops since the last GoLive
	protected bool m_bOpen;
	protected int m_iBatchT0;
	protected int m_iBatchOps;
	protected int m_iBatchMade;
	protected int m_iBatchBuilt;
	protected int m_iBatchLook;         // RUT_LOOK: builds whose mesh came from the look field
	protected int m_iBatchMeshOnly;     // RUT_LOOK: look-only builds that kept their body (new mesh only)
	protected int m_iBatchSkipped;      // RUT_LOOK: look-only builds with nothing to change
	protected bool m_bQueueLook;        // Queue: the bricks queued now change only their look (Submit, op 6)
	protected int m_iBatchMs;
	protected int m_iBatchFrames;
	protected int m_iBatchWorst;
	protected int m_iBatchWorstSub;
	protected int m_iBatchVerts;
	protected int m_iBatchFirstSeq;
	protected int m_iBatchGatherMs;     // TEMP 2026-10-14 band scan probe: Gather / Polygonize ms, samples scanned
	protected int m_iBatchPolyMs;
	protected int m_iBatchScanned;
	protected int m_iBatchSplit;        // DISPLACE_RUTS: bricks split, verts they added, wall tris dropped as folded,
	protected int m_iBatchSplitV;       // bricks too big to split (coarse pull only)
	protected int m_iBatchFold;
	protected int m_iBatchDispOvf;
	protected int m_iBatchSoil;        // grass tris the pull made steep: drawn as soil (user 2026-10-15: soil rut walls)
	// DISPLACED RUTS debug (Stage B): counts since the last 'MUD disp:' line (DispTick), this op's hash (DispOpDone)
	protected int m_iDispT0;
	protected int m_iDispOps;
	protected int m_iDispWritten;
	protected int m_iDispCleared;
	protected int m_iDispCarved;
	protected int m_iDispSplit;
	protected int m_iDispSplitV;
	protected int m_iDispFold;
	protected int m_iDispOvf;
	protected int m_iDispBorder;        // pulled vertices within 0.5 m of a brick side
	protected int m_iDispMs;
	protected int m_iDispWorst;
	protected int m_iDispHoles;
	protected int m_iDispChecks;
	protected int m_iDispPlainChecks;   // RUT_FIELD hole check (review F3): not-carved points on patches
	protected int m_iDispPlainHoles;
	protected int m_iDispSeamChecks;    // ... and either side of a carved / not carved cell line
	protected int m_iDispSeamHoles;
	protected int m_iDispLogged;        // HOLE lines this check (DISP_HOLE_MAX_LOG)
	protected int m_iDispHash;
	protected int m_iFieldHash;     // test kit (2026-10-17): this op's changed FEEL samples, mod DISP_HASH_M (FieldHashAdd)
	protected int m_iFieldHashN;    // ... writes hashed
	protected int m_iDispOpW;           // this op: floor samples written, cleared
	protected int m_iFieldDirty;        // RUT FIELD: this op's heightfield patches marked dirty (Stage 3)
	protected bool m_bFChg;             // RUT FIELD: this op changed a floor sample; box m_iFi0..1 x m_iFk0..1
	// RE-GROUND EXACT (2026-10-18, results/2026-10-18-h1-snap-probe.md: sheet re-ground 5.3 ms of a 9.8 ms client op): this
	// op's changed floor samples (world x, z; dupes possible) - the mud sheet re-reads its ground only round them
	static const int REGROUND_EXACT_MAX = 4000;     // more samples than this: the sheet re-reads the box as before
	protected ref array<float> m_aFChgX = {};
	protected ref array<float> m_aFChgZ = {};
	protected int m_iFi0;
	protected int m_iFi1;
	protected int m_iFk0;
	protected int m_iFk1;
	protected int m_iDispOpC;
	protected int m_iDispHoleT;
	protected int m_iDispFloorT;
	protected ref array<vector> m_aDispFloorP = {};     // draw floor: sample points and colours (refreshed every 0.5 s)
	protected ref array<int> m_aDispFloorC = {};
	protected ref array<vector> m_aDispMarkLo = {};     // draw bricks: brick corner, colour, until (ms)
	protected ref array<int> m_aDispMarkC = {};
	protected ref array<int> m_aDispMarkUntil = {};
	protected ref array<vector> m_aDispHolePos = {};    // hole check: marks, until (ms)
	protected ref array<int> m_aDispHoleUntil = {};
	protected ref TraceParam m_DispTrace;
	protected ref TraceParam m_DecalTrace;    // crater decal probe
#ifdef ENABLE_DIAG
	protected static bool s_bDispMenu;
#endif

	// budgets of this frame (JOIN_MS while a client joins)
	protected int m_iUrgent = URGENT_MS;
	protected bool m_bJoinBoost;
	protected int m_iJoinT0;

	//------------------------------------------------------------------------------------------------
	//! MUD_WorldGroundEntity.VoxelCrater: dig this crater into voxels. depth = this charge's (jittered) depth on flat
	//! ground; cap / gain = the stack cap (C2); stretch, lobeW, turn2, turn3 = the outline (C6), all rolled by the server.
	//! False = not taken (client, refused), the caller digs the heightfield crater.
	static bool Blast(vector wp, float depth, float cap, float gain, float stretch, float lobeW, float turn2, float turn3)
	{
		if (!ENABLED || !Replication.IsServer())
			return false;   // clients get the server's ops
		if (!Ensure())
			return false;
		if (!SURFACE_OPS && !InArea(wp))
			return false;   // heightfield crater
		return s_Inst.DoBlast(wp, depth, cap, gain, stretch, lobeW, turn2, turn3);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Ensure()
	{
		if (s_Inst)
			return true;
		s_Inst = new MUD_VoxelProbe();
		if (!s_Inst.Init())
		{
			s_Inst = null;
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The probe is up on this machine (MUD_CoarseSkin.BuildChunk warns: a rebuilt chunk loses its voxel cells)
	static bool Started()
	{
		return s_Inst != null;
	}

	//------------------------------------------------------------------------------------------------
	//! wp (x, z) is in a converted column: its crater goes to Blast BEFORE Crater's height checks (our heightfield
	//! there is hidden, the voxel ground may be dug far under it)
	static bool InArea(vector wp)
	{
		if (!s_Inst)
			return false;
		return s_Inst.ColAt(s_Inst.SampleI(wp[0]), s_Inst.SampleK(wp[2])) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Carve reach of a crater of this (clamped) depth and outline stretch: rim outer edge + one cell
	protected static float Reach(float depth, float stretch)
	{
		return (CraterCrest(depth) + CraterRimW(depth)) * (1 + stretch) + CELL;
	}

	//------------------------------------------------------------------------------------------------
	//! C1: crest radius of the cosine bowl (rim crest), as MUD_WorldGroundEntity.Crater: walls no steeper than CRATER_SLOPE
	static float CraterCrest(float depth)
	{
		if (CRATER_PIT)
			return Math.Max(0.75, depth * PIT_CREST);
		return Math.Max(0.75, depth * (1 + CRATER_RIM_SHARE) * Math.PI / (2 * CRATER_SLOPE));
	}

	//------------------------------------------------------------------------------------------------
	//! C1: rim width outside the crest (cosine fall to the undug ground)
	static float CraterRimW(float depth)
	{
		if (CRATER_PIT)
			return Math.Max(0.5, depth * PIT_RIM_W);
		float w = Math.Max(0.5, depth * CRATER_RIM_W_SHARE);
		return Math.Max(w, depth * CRATER_RIM_SHARE * Math.PI / (2 * CRATER_SLOPE));
	}

	//------------------------------------------------------------------------------------------------
	static void Tick(float time)
	{
		if (ENABLED)
			TunnelInput();
		if (!s_Inst && ENABLED)
		{
			// start once our ground and the skin are up (a client applies ops only from then on)
			MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
			float gx;
			float gz;
			if (skin && skin.IsReady() && MUD_WorldGroundEntity.ProbePatchGrid(gx, gz) > 0 && Ensure())
			{
				Print(string.Format("MUD voxel probe: BRICKS on - 4 m bricks where ops are, kept for the match, max %1 columns | rpl mode %2, server %3, headless %4",
					MAX_COLUMNS, RplSession.Mode(), Replication.IsServer(), s_Inst.m_bHeadless), LogLevel.NORMAL);
			}
		}
		if (!s_Inst)
			return;
		if (!Replication.IsServer())
		{
			AskJip();
			Drain();
		}
		else
		{
			s_Inst.JipSendTick();
			if (MUD_TestKit.TEST_KIT && RplSession.Mode() == RplMode.None)
				s_Inst.SnapSelfTestTick();     // SP / WB only: on a live server it would spoil the PERF frames
		}
		s_Inst.Graves();
		s_Inst.Step();
		s_Inst.DispTick();
		if (MUD_TestKit.FakeRutOps() > 0 && Replication.IsServer())
			s_Inst.FakeRutTick(time);
	}

	//------------------------------------------------------------------------------------------------
	//! The old ground test (fine patches off): gone, always false (MUD_WorldGroundEntity.HasHole reads it)
	static bool GroundMode()
	{
		return ENABLED && GROUND;
	}

	//------------------------------------------------------------------------------------------------
	//! Voxel overlay on (bricks): skin + fine patch cells under converted columns go out
	static bool OverlayMode()
	{
		return ENABLED;
	}

	//------------------------------------------------------------------------------------------------
	//! Ops made by the server and sent to every client
	static bool Networked()
	{
		return ENABLED;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_WorldGroundEntity CarveTrench / FreeDig / FreeFill: this dig goes to voxel ops when the columns its box
	//! needs fit under MAX_COLUMNS. Trench / FreeDig / FreeFill can still return REFUSED (seed too long, no ground).
	static bool TakesDig(MUD_TrenchLayout lay)
	{
		if (!ENABLED || !lay || lay.Count() < 1 || !Ensure())
			return false;
		float margin = MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER * MUD_TrenchPlan.SLOPE_RUN + TRENCH_PAD + CELL;
		if (PARAPET)
			margin = Math.Max(margin, ParapetStart(lay) + PARA_RISE * 2 + PARA_TOP + CELL);     // Stage 6: as Prepare
		float x0 = 100000000;
		float z0 = 100000000;
		float x1 = -100000000;
		float z1 = -100000000;
		for (int p = 0; p < lay.Count(); p++)
		{
			vector lo;
			vector hi;
			lay.PieceBox(p, lo, hi);
			x0 = Math.Min(x0, lo[0] - margin);
			z0 = Math.Min(z0, lo[2] - margin);
			x1 = Math.Max(x1, hi[0] + margin);
			z1 = Math.Max(z1, hi[2] + margin);
		}
		array<int> keys = {};
		s_Inst.ConvCols(Vector(x0, 0, z0), Vector(x1, 0, z1), keys);
		if (keys.Count() > MAX_OP_COLUMNS)
			return false;
		int fresh;
		foreach (int key : keys)
		{
			if (!s_Inst.m_mCols.Contains(key))
				fresh++;
		}
		if (!SURFACE_OPS && fresh == keys.Count())
			return false;   // nothing converted there yet: the heightfield digs it
		return s_Inst.m_mCols.Count() + fresh <= MAX_COLUMNS;
	}

	//------------------------------------------------------------------------------------------------
	//! MP client that controls no entity yet (joining, deploy menu): build with JOIN_MS budgets
	protected static bool Joining()
	{
		if (!ENABLED || RplSession.Mode() == RplMode.None || Replication.IsServer())
			return false;
		return SCR_PlayerController.GetLocalControlledEntity() == null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tunnel test: T held on foot digs a capsule ahead of the player, at once then every TUNNEL_EVERY_MS.
	//! Debug key (TUNNEL_DEBUG_T): no tool; the shovel TUNNEL mode is the real way.
	protected static void TunnelInput()
	{
		if (!TUNNEL_DEBUG_T || System.IsConsoleApp())
			return;
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player || player.IsInVehicle())
		{
			s_bDigHeld = false;
			return;
		}
		InputManager input = GetGame().GetInputManager();
		CameraManager cameras = GetGame().GetCameraManager();
		if (!input || !cameras || !cameras.CurrentCamera())
			return;
		input.ActivateContext(TUNNEL_CONTEXT);
		if (input.GetActionValue(TUNNEL_ACTION) < 0.5)
		{
			s_bDigHeld = false;
			return;
		}
		int now = System.GetTickCount();
		if (s_bDigHeld && now - s_iDigAt < TUNNEL_EVERY_MS)
			return;
		s_bDigHeld = true;
		s_iDigAt = now;

		vector mid;
		if (!TunnelAimLocal(player, mid))
			return;
		// MP client: the server checks and digs it (and sends it to everyone); host / SP: the same checks, run here
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.MUD_RequestVoxelTunnel(mid);
	}

	//------------------------------------------------------------------------------------------------
	//! U1 stage 2: where a tunnel click (T or a shovel TUNNEL swing) digs for the local player, from the camera look.
	//! False with no camera.
	static bool TunnelAimLocal(notnull IEntity player, out vector mid)
	{
		CameraManager cameras = GetGame().GetCameraManager();
		if (!cameras || !cameras.CurrentCamera())
			return false;
		vector cam[4];
		cameras.CurrentCamera().GetTransform(cam);
		mid = TunnelAim(player, cam[2], TunnelDrop(s_iTunnelSteep));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Most a click goes up / down (m) for slope choice i. Ramp angles MEASURED offline (Tools/u1_deep_tunnel_checks.py
	//! deep10, mean deg): 0.35 = 19, 0.5 = 27, 0.7 = 39, 0.85 = 47, 1.0 = 54. The character climbs ~50 deg (IDA
	//! 2026-10-15), so the two steepest may not walk back out.
	static float TunnelDrop(int i)
	{
		if (i <= 0)
			return 0.35;
		if (i == 1)
			return 0.5;
		if (i == 2)
			return 0.7;
		if (i == 3)
			return 0.85;
		return TUNNEL_DOWN_MAX;
	}

	//------------------------------------------------------------------------------------------------
	//! Local: the next slope choice (X in shovel TUNNEL mode), with a hint
	static void NextTunnelSteep()
	{
		s_iTunnelSteep = (s_iTunnelSteep + 1) % TUNNEL_STEEP_COUNT;
		SCR_HintManagerComponent.ShowCustomHint(TunnelSteepText() + "\nX: next slope", "Tunnel slope", 3, true);
	}

	//------------------------------------------------------------------------------------------------
	static string TunnelSteepText()
	{
		int i = s_iTunnelSteep;
		if (i <= 0)
			return "Gentle: ~19 deg";
		if (i == 1)
			return "Normal: ~27 deg";
		if (i == 2)
			return "Steep: ~39 deg";
		if (i == 3)
			return "Very steep: ~47 deg (may not walk back out)";
		return "Steepest: ~54 deg (too steep to walk out)";
	}

	//------------------------------------------------------------------------------------------------
	//! Local, every frame while the shovel is in TUNNEL mode (MUD_Dig): X cycles the slope. Own context, only on then,
	//! so X is not taken from vanilla at other times.
	static void TunnelSteepInput()
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;
		input.ActivateContext(TUNNEL_STEEP_CONTEXT);
		if (input.GetActionTriggered(TUNNEL_STEEP_ACTION))
			NextTunnelSteep();
	}

	//------------------------------------------------------------------------------------------------
	//! U1 stage 2 (user 2026-10-17): capsule middle for a look direction. Up / down per click capped at drop
	//! (TunnelDrop); a look (near) straight down / up digs ahead along the body facing, never a shaft in place.
	static vector TunnelAim(notnull IEntity player, vector look, float drop)
	{
		vector flat = Vector(look[0], 0, look[2]);
		if (flat.Length() > 0.1)
			flat = flat.Normalized();
		else
			flat = MUD_Dig.Facing(player);
		float lim = Math.Min(drop, TUNNEL_DOWN_MAX) / TUNNEL_SLOPE;
		float pitch = Math.Clamp(look[1], -lim, lim);
		return player.GetOrigin() + flat * TUNNEL_AHEAD + Vector(0, TUNNEL_MID + pitch * TUNNEL_SLOPE, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, both tunnel RPCs: the capsule middle is not lower than the steepest allowed click from these feet
	//! (TUNNEL_LAG for the server's feet lagging the client's)
	static bool TunnelDownOk(vector mid, vector feet)
	{
		return mid[1] >= feet[1] + TUNNEL_MID - TUNNEL_DOWN_MAX - TUNNEL_LAG;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a client's tunnel dig, checked by MUD_VoxelNet.c (on foot, near, rate)
	static void ServerTunnel(vector mid)
	{
		if (ENABLED && Replication.IsServer() && Ensure())
			s_Inst.DoTunnel(mid);
	}

	//------------------------------------------------------------------------------------------------
	//! All-voxel test: voxel ruts are on here (server with the probe up). MUD_WorldGroundEntity.Wheels then skips
	//! the heightfield press, ahead ruts and rut fill.
	static bool RutsOn()
	{
		return ENABLED && RUTS && s_Inst != null && Replication.IsServer();
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_LEAD_AXLE: lead[w] = 1 for a grounded wheel on the leading axle - vehicle-local z within RUT_AXLE_TOL of the
	//! frontmost contact moving forward (vel along local +z), of the rearmost backing up - else 0. Every grounded wheel
	//! 1 with RUT_LEAD_AXLE false. Server (feel ruts) and client (look ruts) both call it.
	static void LeadAxle(Vehicle veh, VehicleWheeledSimulation sim, vector vel, array<int> lead, bool look = false)
	{
		lead.Clear();
		s_aLeadZ.Clear();
		int count = sim.WheelCount();
		vector lv = veh.VectorToLocal(vel);
		float sgn = 1;
		if (lv[2] < 0)
			sgn = -1;
		float best = -1000000;
		for (int w = 0; w < count; w++)
		{
			float z = -1000000;
			if (sim.WheelHasContact(w))
			{
				vector lc = veh.CoordToLocal(RutPoint(veh, sim, w));
				z = lc[2] * sgn;
				if (z > best)
					best = z;
			}
			s_aLeadZ.Insert(z);
		}
		for (int i = 0; i < count; i++)
		{
			if (s_aLeadZ[i] < -999999)
				lead.Insert(0);
			else if (!RUT_LEAD_AXLE || s_aLeadZ[i] >= best - RUT_AXLE_TOL)
				lead.Insert(1);
			else if (RUT_TRAIL_CUT && !InLeadRut(RutPoint(veh, sim, i), vel, look))
				lead.Insert(2);     // TRAIL CUT: off the leading rut (turning); 2 = MUD_RutDebug colour, callers test != 0
			else
				lead.Insert(0);
		}
		MUD_RutDebug.OnAxle(veh, sim, vel, lead, look);     // diag menu Mud > Ruts and wheels
	}

	//------------------------------------------------------------------------------------------------
	//! RUT AT THE HUB (user 2026-10-19 with the rut debugger: the contact ball stays put while a steered front tyre swings
	//! out; v227 = before). WheelGetPosition is VEHICLE-LOCAL (WB 00:38 log: the same x 1.02 z 2.65 at every speed - the
	//! first build read it as world and cut near the map corner = no ruts). Where a wheel cuts its rut -
	//! the hub's x / z on the contact's height. Ruts only (server RutWheel, look ruts, LeadAxle, TRAIL CUT); physics code
	//! keeps the contact point. false = the contact point as before.
	static const bool RUT_AT_HUB = true;
	static vector RutPoint(IEntity veh, VehicleWheeledSimulation sim, int w)
	{
		vector c = sim.WheelGetContactPosition(w);
		if (!RUT_AT_HUB)
			return c;
		if (!veh)
			return c;
		vector h = veh.CoordToParent(sim.WheelGetPosition(w));
		return Vector(h[0], c[1], h[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! TRAIL CUT: is the ground RUT_TRAIL_AHEAD m ahead of contact cp (along vel) already a rut deep enough to ride in?
	//! look = the drawn floor (client look ruts), else the real rut floor. True when unsure (no speed / no ground).
	static bool InLeadRut(vector cp, vector vel, bool look)
	{
		float hs = Math.Sqrt(vel[0] * vel[0] + vel[2] * vel[2]);
		if (hs < 0.2 || !s_Inst)
			return true;
		float dx = vel[0] / hs;
		float dz = vel[2] / hs;
		// the REAL rut floor on both sides (WB 00:30 log): the look cut is laid up to LEAD_MAX ahead, so a rear wheel that
		// started a look cut found its own fresh rut 0.5 m ahead next frame and stopped (on / off). Same rule as the server.
		float dep = RutDepthAt(cp[0] + dx * RUT_TRAIL_AHEAD, cp[2] + dz * RUT_TRAIL_AHEAD, false);
		if (dep < -1)
			return true;     // no ground there
		float thr = RUT_TRAIL_IN * RUT_PASS_MAX;     // WEAR: a one-pass rut (RUT_PASS_MAX) counts as a rut to ride in
		if (RUT_FIRST_D > 0)
			thr = RUT_TRAIL_IN * RUT_FIRST_D;
		if (TRAIL_PROBE && MUD_TestKit.TEST_KIT)
			TrailProbe(cp, dx, dz, look, dep, hs);
		if (dep >= thr)
			return true;
		// WEAR COUNT: any worn track (no floor yet) is a rut to ride in too, else every axle counts a pass. Any wear, not
		// a full pass: in the front wheel's RUT_RAMP fade only one op's weight is in yet (review 2026-10-19)
		return WEAR_PASSES > 0 && WearAt(cp[0] + dx * RUT_TRAIL_AHEAD, cp[2] + dz * RUT_TRAIL_AHEAD) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! TRAIL CUT: rut depth (m, undug minus floor; 0 = no rut) at world x, z on the nearest floor sample. look = the drawn
	//! floor (MeshFloor). -2 = no ground.
	static float RutDepthAt(float x, float z, bool look)
	{
		float undug;
		if (!s_Inst || !MUD_WorldGroundEntity.UndugY(x, z, undug))
			return -2;
		int gx = Math.Round((x - s_Inst.m_fX0) / FCELL);
		int gz = Math.Round((z - s_Inst.m_fZ0) / FCELL);
		float fl;
		if (look)
			fl = MUD_LookRuts.MeshFloor(gx, gz);
		else
			fl = FieldFloor(gx, gz);
		if (fl > FLOOR_NONE * 0.5)
			return 0;
		return undug - fl;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP trail probe 2026-10-19 (user: rear wheels cut driving straight, turn ruts off the wheels): at most one line
	//! per TRAIL_PROBE_MS per side (server / look) - the depth RUT_TRAIL_AHEAD m ahead of a trailing wheel, and the deepest
	//! rut across -0.8 .. +0.8 m sideways there (side + = right of the travel). Says how far the rut lies off the wheel.
	static void TrailProbe(vector cp, float dx, float dz, bool look, float dep, float hs)
	{
		int now = System.GetTickCount();
		int li = 0;
		if (look)
			li = 1;
		if (now - s_aTrailPrintAt[li] < TRAIL_PROBE_MS)
			return;
		s_aTrailPrintAt[li] = now;
		float ax = cp[0] + dx * RUT_TRAIL_AHEAD;
		float az = cp[2] + dz * RUT_TRAIL_AHEAD;
		float best = -1;
		float bestS = 0;
		for (int s = -8; s <= 8; s++)
		{
			float o = s * 0.1;
			float d = RutDepthAt(ax + dz * o, az - dx * o, look);     // right of the travel = (dz, -dx) (x right, z forward)
			if (d > best)
			{
				best = d;
				bestS = o;
			}
		}
		Print(string.Format("MUD trail probe: look %1 | speed %2 m/s | depth ahead %3 m (in rut %4) | deepest %5 m at side %6 m | wheel %7", look, hs.ToString(-1, 1), dep.ToString(-1, 3), dep >= RUT_TRAIL_IN * RUT_FIRST_D, best.ToString(-1, 3), bestS.ToString(-1, 1), cp.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: wheel w of veh does not cut this frame (RUT_LEAD_AXLE trailing wheel) - its track restarts where it is
	//! when it leads again (no long segment from a stale point)
	static void RutWheelOff(Vehicle veh, int w)
	{
		if (!RutsOn() || !veh || w < 0)
			return;
		MUD_VoxelRutTrack t = Track(veh);
		if (w < t.m_aHas.Count())
			t.m_aHas.Set(w, 0);
		if (w < t.m_aHeldHas.Count())
			t.m_aHeldHas.Set(w, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, all-voxel test: wheel w of veh touches at cp. Adds a segment every RUT_STEP m of travel; RutFlush
	//! sends them. step = cut per pass, cap = deepest rut under the undug ground (both m).
	static void RutWheel(Vehicle veh, int w, vector cp, float step, float cap)
	{
		if (!RutsOn() || !veh || w < 0)
			return;
		MUD_VoxelRutTrack t = Track(veh);
		while (t.m_aLast.Count() <= w)
		{
			t.m_aLast.Insert(cp);
			t.m_aHas.Insert(0);
		}
		t.m_fStep = step;
		t.m_fCap = cap;
		if (t.m_aHas[w] == 0)
		{
			t.m_aLast.Set(w, cp);
			t.m_aHas.Set(w, 1);
			return;
		}
		vector last = t.m_aLast[w];
		float dx = cp[0] - last[0];
		float dz = cp[2] - last[2];
		float dd = dx * dx + dz * dz;
		while (t.m_aHeldHas.Count() <= w)
		{
			t.m_aHeldA.Insert(cp);
			t.m_aHeldB.Insert(cp);
			t.m_aHeldHas.Insert(0);
		}
		if (dd > RUT_JUMP * RUT_JUMP)
		{
			t.m_aLast.Set(w, cp);
			t.m_aHeldHas.Set(w, 0);
			return;
		}
		if (dd < RUT_STEP * RUT_STEP)
			return;
		if (t.m_aSegA.Count() < RUT_MAX_SEGS)
		{
			t.m_aSegA.Insert(last);
			t.m_aSegB.Insert(cp);
		}
		else if (RUT_BEHIND)
		{
			// R1 2026-10-18: full this flush interval - straight to the wait list (it was dropped)
			t.m_aPendA.Insert(last);
			t.m_aPendB.Insert(cp);
			t.m_aPendAt.Insert(System.GetTickCount());
		}
		else
			t.m_iDropped++;
		if (LookRuts())
		{
			// the segment before this one shows now: it ends >= RUT_STEP behind the tyre, so the tyre hides where the
			// look rut starts (the newest one would show under it while the wheel still rides the feel ground)
			if (t.m_aHeldHas[w] != 0 && t.m_aLookA.Count() < RUT_MAX_SEGS)
			{
				t.m_aLookA.Insert(t.m_aHeldA[w]);
				t.m_aLookB.Insert(t.m_aHeldB[w]);
			}
			t.m_aHeldA.Set(w, last);
			t.m_aHeldB.Set(w, cp);
			t.m_aHeldHas.Set(w, 1);
		}
		t.m_aLast.Set(w, cp);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, all-voxel test: after veh's wheels - its segments go out as one op, at most every RUT_EVERY_MS
	static void RutFlush(Vehicle veh)
	{
		if (!RutsOn() || !veh)
			return;
		MUD_VoxelRutTrack t = Track(veh);
		int now = System.GetTickCount();
		if (now < t.m_iFlushAt)
			return;
		bool sent = false;
		// convert ahead first (lower seq): the rut cut then lands on cells that are already voxels
		if (!t.m_aAheadA.IsEmpty() && s_Inst.AheadOpen(t))
		{
			s_Inst.DoAhead(t);
			sent = true;
		}
		// RUT_LOOK: the look op before the feel one (a feel cut then never lands before its look)
		if (LookRuts() && !t.m_aLookA.IsEmpty())
		{
			s_Inst.DoRutOp(t, t.m_aLookA, t.m_aLookB, 6);
			sent = true;
		}
		t.m_aLookA.Clear();
		t.m_aLookB.Clear();
		// R1: under the mud sheet the feel rut op waits up to RUT_SHEET_MS (fewer, bigger ops), unless a full op is ready.
		// Convert ahead and look ops above keep the RUT_EVERY_MS rate.
		bool rutNow = true;
		if (RUT_SHEET_MS > RUT_EVERY_MS && now < t.m_iRutAt && RutUnderSheet(t))
		{
			rutNow = ReadyCount(t, now) >= RUT_MAX_SEGS;
			if (!rutNow)
				t.m_iWaits++;
		}
		if (rutNow)
		{
			if (RUT_BEHIND)
				Behind(t, now);
			if (!t.m_aSegA.IsEmpty())
			{
				s_Inst.DoRut(t);
				sent = true;
				t.m_iRutAt = now + RUT_SHEET_MS;
				if (RUT_BEHIND)
				{
					Print(string.Format("MUD voxel probe: rut behind - %1 cut at the voxel gap %2 m, %3 at the grass gap %4 m, %5 forced, %6 waiting, %7 sheet waits, %8 dropped",
						t.m_iNear, RUT_BEHIND_GAP_VOX, t.m_iFar, RUT_BEHIND_GAP, t.m_iForced, t.m_aPendA.Count(), t.m_iWaits, t.m_iDropped), LogLevel.NORMAL);
					t.m_iNear = 0;
					t.m_iFar = 0;
					t.m_iForced = 0;
					t.m_iWaits = 0;
					t.m_iDropped = 0;
				}
			}
			t.m_aSegA.Clear();
			t.m_aSegB.Clear();
		}
		else if (RUT_BEHIND)
			ToPend(t, now);     // R1: this interval's segments wait (RutWheel's per-interval cap never drops them)
		// (else: m_aSegA stays for the next flush - ReadyCount is its count)
		t.m_aAheadA.Clear();
		t.m_aAheadB.Clear();
		if (sent || !rutNow)
			t.m_iFlushAt = now + RUT_EVERY_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! R1: does veh drive under the mud sheet? ONE place for "under the sheet" (mud zones today, offroad later -
	//! goal in research/2026-10-18-mud-zone-rut-perf.md)
	protected static bool RutUnderSheet(MUD_VoxelRutTrack t)
	{
		if (!MUD_MudSheet.ENABLED || !MUD_MudSheet.SYNC || !t.m_Veh)
			return false;
		vector o = t.m_Veh.GetOrigin();
		return MUD_MudZoneEntity.SheetCovers(o[0], o[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! R1: segments Behind would let out now (RUT_BEHIND), or the collected ones (no RUT_BEHIND). Changes nothing.
	protected static int ReadyCount(MUD_VoxelRutTrack t, int now)
	{
		if (!RUT_BEHIND)
			return t.m_aSegA.Count();
		vector vehAt = vector.Zero;
		bool hasVeh = false;
		if (t.m_Veh)
		{
			vehAt = t.m_Veh.GetOrigin();
			hasVeh = true;
		}
		int nPend = t.m_aPendA.Count();
		int count = nPend + t.m_aSegA.Count();
		int ready = 0;
		for (int i = 0; i < count; i++)
		{
			vector pa;
			vector pb;
			int at = now;
			if (i < nPend)
			{
				pa = t.m_aPendA[i];
				pb = t.m_aPendB[i];
				at = t.m_aPendAt[i];
			}
			else
			{
				pa = t.m_aSegA[i - nPend];
				pb = t.m_aSegB[i - nPend];
			}
			if (ReadyWhy(t, pa, pb, at, count - i > RUT_PEND_MAX, now, vehAt, hasVeh) > 0)
			{
				ready++;
				if (ready >= RUT_MAX_SEGS)
					return ready;
			}
		}
		return ready;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_BEHIND: this flush interval's segments (m_aSegA / B) join the wait list
	protected static void ToPend(MUD_VoxelRutTrack t, int now)
	{
		for (int n = 0; n < t.m_aSegA.Count(); n++)
		{
			t.m_aPendA.Insert(t.m_aSegA[n]);
			t.m_aPendB.Insert(t.m_aSegB[n]);
			t.m_aPendAt.Insert(now);
		}
		t.m_aSegA.Clear();
		t.m_aSegB.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_BEHIND: why a waiting segment goes now - 0 = waits, 1 = past the voxel gap, 2 = past the grass gap,
	//! 3 = forced (over RUT_PEND_MAX, or waiting too long and away from the vehicle)
	protected static int ReadyWhy(MUD_VoxelRutTrack t, vector pa, vector pb, int at, bool overMax, int now, vector vehAt, bool hasVeh)
	{
		float clear2 = RUT_BEHIND_CLEAR * RUT_BEHIND_CLEAR;
		bool away = !hasVeh || (vector.DistanceSqXZ(pa, vehAt) > clear2 && vector.DistanceSqXZ(pb, vehAt) > clear2);
		if (overMax || (now - at > RUT_BEHIND_MAX_MS && away))
			return 3;
		if (!t.m_bTail)
			return 0;
		float back = -Math.Max(vector.Dot(pa - t.m_vTail, t.m_vTailDir), vector.Dot(pb - t.m_vTail, t.m_vTailDir));
		if (back > RUT_BEHIND_GAP)
			return 2;
		if (back > RUT_BEHIND_GAP_VOX && s_Inst.SegOnVox(pa, pb))
			return 1;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_BEHIND: new segments wait in m_aPend; those the rearmost wheel is RUT_BEHIND_GAP past (or waiting too long,
	//! or past RUT_PEND_MAX) go back into m_aSegA / B for this flush's op (at most RUT_MAX_SEGS)
	protected static void Behind(MUD_VoxelRutTrack t, int now)
	{
		ToPend(t, now);
		array<vector> keepA = {};
		array<vector> keepB = {};
		array<int> keepAt = {};
		vector vehAt = vector.Zero;
		bool hasVeh = false;
		if (t.m_Veh)
		{
			vehAt = t.m_Veh.GetOrigin();
			hasVeh = true;
		}
		int count = t.m_aPendA.Count();
		for (int i = 0; i < count; i++)
		{
			vector pa = t.m_aPendA[i];
			vector pb = t.m_aPendB[i];
			int at = t.m_aPendAt[i];
			int why = ReadyWhy(t, pa, pb, at, count - i > RUT_PEND_MAX, now, vehAt, hasVeh);
			if (why > 0 && t.m_aSegA.Count() < RUT_MAX_SEGS)
			{
				t.m_aSegA.Insert(pa);
				t.m_aSegB.Insert(pb);
				if (why == 1)
					t.m_iNear++;
				else if (why == 2)
					t.m_iFar++;
				else
					t.m_iForced++;
				continue;
			}
			keepA.Insert(pa);
			keepB.Insert(pb);
			keepAt.Insert(at);
		}
		t.m_aPendA = keepA;
		t.m_aPendB = keepB;
		t.m_aPendAt = keepAt;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, RUT_BEHIND: the rearmost wheel contact of veh along its travel direction dir (flat)
	static void RutTail(Vehicle veh, vector tail, vector dir)
	{
		if (!RUT_BEHIND || !RutsOn() || !veh)
			return;
		MUD_VoxelRutTrack t = Track(veh);
		t.m_vTail = tail;
		t.m_vTailDir = dir;
		t.m_bTail = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, all-voxel test: w0 / w1 = contacts of the 2 leading wheels (along the travel direction dir), speed m/s.
	//! Their strips ahead go VOX at the next RutFlush (if any cell there is not yet).
	static void RutAhead(Vehicle veh, vector w0, vector w1, vector dir, float speed)
	{
		if (!RUT_AHEAD || !RutsOn() || !veh || speed < 0.2)
			return;
		MUD_VoxelRutTrack t = Track(veh);
		float lead = Math.Min(RUT_LEAD_MIN + RUT_LEAD_PER_S * speed, RUT_LEAD_MAX);
		t.m_aAheadA.Clear();
		t.m_aAheadB.Clear();
		t.m_aAheadA.Insert(w0);
		t.m_aAheadB.Insert(w0 + dir * lead);
		t.m_aAheadA.Insert(w1);
		t.m_aAheadB.Insert(w1 + dir * lead);
	}

	//------------------------------------------------------------------------------------------------
	//! Track of veh (made on first use); tracks of deleted vehicles are dropped
	protected static MUD_VoxelRutTrack Track(Vehicle veh)
	{
		for (int i = s_aTracks.Count() - 1; i >= 0; i--)
		{
			MUD_VoxelRutTrack t = s_aTracks[i];
			if (!t.m_Veh)
			{
				s_aTracks.Remove(i);
				continue;
			}
			if (t.m_Veh == veh)
				return t;
		}
		MUD_VoxelRutTrack fresh = new MUD_VoxelRutTrack();
		fresh.m_Veh = veh;
		s_iSheetVeh++;
		fresh.m_iSheetId = s_iSheetVeh;
		s_aTracks.Insert(fresh);
		return fresh;
	}

	//------------------------------------------------------------------------------------------------
	//! Test kit 10 s line (2026-10-17): ops in m_aOps - server: every op; client: the ops applied. -1 = no probe yet
	static int OpCount()
	{
		if (!s_Inst)
			return -1;
		return s_Inst.m_aOps.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! MP client (MUD_WorldGroundEntity.RpcDo_VoxelOp, JIP batches): hold the op until every older one is in
	static void Receive(string text)
	{
		if (Replication.IsServer())
			return;
		int bar = text.IndexOf("|");
		if (bar < 1)
			return;
		string head = text.Substring(0, bar);
		if (!MUD_TrenchPlan.IsInt(head))
			return;
		int seq = head.ToInt();
		int last;
		string rows;
		if (RangeOf(text, last, rows))
		{
			// J2: placeholder for rut ops seq..last (the snapshot tiles hold them). It REPLACES a real rut op held in
			// that range (a broadcast between connect and the ask): run here without the earlier ruts it would make
			// other brick rows than the server logged. Ops already applied are skipped.
			int start = seq;
			if (start < s_iNextSeq)
				start = s_iNextSeq;
			if (start > last)
				return;
			DropHeld(start, last);
			if (s_mPending.Count() >= HeldCap())
			{
				s_bLost = true;
				return;
			}
			s_mPending.Insert(start, string.Format("%1|R|%2|", start, last) + rows);
			return;
		}
		if (seq < s_iNextSeq || s_mPending.Contains(seq))
			return;     // already applied / held (a JIP batch and a broadcast can both carry it)
		if (s_mPending.Count() >= HeldCap())
		{
			s_bLost = true;     // J1: dropped, so asked again from s_iNextSeq once there is room (Drain)
			return;
		}
		s_mPending.Insert(seq, text);
		if (MUD_TestKit.TEST_KIT) MUD_CraterTrace.Recv(text, s_mPending.Count());     // stage 2 B1 crater log
	}

	//------------------------------------------------------------------------------------------------
	//! MP client: ops held out of order at most (past it Receive drops and Drain asks again)
	protected static int HeldCap()
	{
		if (MUD_TestKit.JipTestCap() > 0)
			return MUD_TestKit.JipTestCap();     // test kit: J1 shows with ~500 ops
		return HELD_CAP;
	}

	//------------------------------------------------------------------------------------------------
	//! MP client: one JIP message, ops joined with '#'
	static void ReceiveBatch(string batch)
	{
		array<string> texts = {};
		batch.Split("#", texts, true);
		foreach (string t : texts)
		{
			string tag;
			if (t.Length() >= 2)
				tag = t.Substring(0, 2);
			if (tag == "S|")
				ReceiveSnapHead(t);
			else if (tag == "T|" || tag == "M|")
				ReceiveSnapTile(t);     // M| = synced mud sheet layer part (Stage 5), numbered with the tile parts
			else
				Receive(t);
		}
		Print(string.Format("MUD voxel probe: JIP batch in, %1 ops, next seq %2, %3 held", texts.Count(), s_iNextSeq, s_mPending.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! MP client: apply the held ops that are next in seq order, DRAIN_MS per frame (JOIN_MS while joining: a joiner
	//! gets the whole log at once). A gap that stays 10 s is logged once.
	protected static void Drain()
	{
		int t0 = System.GetTickCount();
		int budget = DRAIN_MS;
		if (Joining())
			budget = JOIN_MS;
		if (!SnapGate())
			return;
		while (s_mPending.Contains(s_iNextSeq))
		{
			if (System.GetTickCount(t0) >= budget)
				return;     // the rest next frame
			string text = s_mPending.Get(s_iNextSeq);
			// J2: a real rut op inside a coming snapshot waits for its placeholder (Receive swaps it in) - treated as a
			// gap below, so J1 asks again if it never comes
			if (s_iSnapS > 0 && s_iNextSeq <= s_iSnapS && IsRutText(text))
				break;
			s_mPending.Remove(s_iNextSeq);
			s_iGapAt = 0;     // J1: a gap that filled is not 'old' when the next one shows
			int last;
			string rows;
			if (RangeOf(text, last, rows))
			{
				// J2: rut ops the snapshot tiles hold - only the brick rows they made, at their seq
				MUD_MudSync.Flush();     // synced mud sheet: open op work ends before rows change the ground
				s_Inst.RangeRows(rows);
				s_iSnapSkipped += last - s_iNextSeq + 1;
				DropHeld(s_iNextSeq + 1, last);
				s_iNextSeq = last + 1;
			}
			else
			{
				s_iNextSeq++;
				MUD_VoxelOp op = Decode(text);
				if (op)
					s_Inst.Submit(op);
				else
					Print("MUD voxel probe: bad op text from the server: " + text, LogLevel.ERROR);
			}
			if (!SnapGate())
				return;
		}
		if (s_mPending.IsEmpty())
		{
			s_iGapAt = 0;
			if (s_bLost)
				AskFrom("ops were dropped at the held cap");
			return;
		}
		if (s_bLost && s_mPending.Count() >= HeldCap())
		{
			// J1: full of newer ops while the next one was dropped - nothing can drain. Start again from s_iNextSeq.
			int cleared = s_mPending.Count();
			s_mPending.Clear();
			s_iGapAt = 0;
			AskFrom(string.Format("held cap full without seq %1, %2 newer ones cleared", s_iNextSeq, cleared));
			return;
		}
		if (s_iGapAt == 0)
			s_iGapAt = System.GetTickCount();
		else if (!s_bToldGap && System.GetTickCount(s_iGapAt) > 10000)
		{
			s_bToldGap = true;
			Print(string.Format("MUD voxel probe: waiting 10 s for op seq %1 (%2 newer ones held) - JIP asked %3", s_iNextSeq, s_mPending.Count(), s_bAskedJip), LogLevel.WARNING);
		}
		// J1 safety net: a gap that stays (an op lost any other way) asks again, at most every RESEND_WAIT_MS
		if (s_bAskedJip && System.GetTickCount(s_iGapAt) > RESEND_WAIT_MS && System.GetTickCount(s_iAskAt) > RESEND_WAIT_MS)
			AskFrom(string.Format("seq %1 missing for %2 s", s_iNextSeq, RESEND_WAIT_MS / 1000));
	}

	//------------------------------------------------------------------------------------------------
	//! MP client: ask the server once for every op made so far (late join, or ops made before this entity was up)
	protected static void AskJip()
	{
		if (s_bAskedJip || RplSession.Mode() == RplMode.None)
			return;
		if (AskFrom("first ask (late join / ops made before the ground was up)"))
			s_bAskedJip = true;
	}

	//------------------------------------------------------------------------------------------------
	//! MP client (J1): ask the server for every op from s_iNextSeq on. False = not now (no controller yet, or asked
	//! < 1 s ago - the server takes one ask per second); the caller tries again next frame.
	protected static bool AskFrom(string why)
	{
		if (RplSession.Mode() == RplMode.None)
			return false;
		if (s_iAskAt != 0 && System.GetTickCount(s_iAskAt) < 1000)
			return false;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return false;
		s_bLost = false;
		s_iAskAt = System.GetTickCount();
		s_iAsks++;
		pc.MUD_RequestVoxelJip(s_iNextSeq);
		Print(string.Format("MUD voxel probe: asked the server for the op log from seq %1 (%2 held, ask %3) - %4", s_iNextSeq, s_mPending.Count(), s_iAsks, why), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server (J1, MUD_VoxelNet.c): send player pid every op from seq 'from' on, JIP_MSGS_PER_FRAME messages per
	//! frame (JipSendTick). A new ask replaces that player's running send.
	static void ServerJipAsk(int pid, int from)
	{
		if (from < 1)
			from = 1;
		s_mJipFrom.Set(pid, from);
		int newest = -1;
		if (s_Inst)
			newest = s_Inst.m_iSeq;
		Print(string.Format("MUD voxel probe: player %1 asked for the op log from seq %2 - server at seq %3, %4 messages per frame", pid, from, newest, JIP_MSGS_PER_FRAME), LogLevel.NORMAL);
		// J2: a first ask gets a new snapshot; a later ask (J1, a lost op) from <= its S resumes it (placeholders
		// again; tiles not sent twice - reliable, in order), past S gets real ops as before
		MUD_JipSnap had = s_mSnap.Get(pid);
		if (from > 1 && had && from <= had.m_iS)
			had.m_bDone = false;
		if (from == 1 && JIP_SNAPSHOT && s_Inst && s_Inst.m_iSeq > 0)
		{
			int t0 = System.GetTickCount();
			MUD_JipSnap snap = s_Inst.MakeSnap();
			s_mSnap.Set(pid, snap);
			int bytes;
			foreach (string part : snap.m_aTiles)
				bytes += part.Length();
			Print(string.Format("MUD voxel probe: player %1 snapshot at seq %2: %3 rut tiles in %4 parts, %5 bytes, built in %6 ms",
				pid, snap.m_iS, s_Inst.m_mRutTiles.Count(), snap.m_aTiles.Count(), bytes, System.GetTickCount(t0)), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server (J1): the next JIP_MSGS_PER_FRAME messages to each asking player, oldest ops first
	protected void JipSendTick()
	{
		if (s_mJipFrom.IsEmpty())
			return;
		int newest = 0;
		if (!m_aOps.IsEmpty())
			newest = m_aOps[m_aOps.Count() - 1].m_iSeq;
		array<int> pids = {};
		array<int> nexts = {};
		array<int> gone = {};
		for (int j = 0; j < s_mJipFrom.Count(); j++)
		{
			int pid = s_mJipFrom.GetKey(j);
			int from = s_mJipFrom.GetElement(j);
			int next = -1;     // -1 = done (player left or everything sent)
			SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(pid));
			if (pc)
			{
				array<string> batches = {};
				MUD_JipSnap snap = s_mSnap.Get(pid);
				if (snap && snap.m_bDone)
					snap = null;
				if (snap)
				{
					next = SnapBatches(snap, from, JIP_MSGS_PER_FRAME, batches);
					if (next > snap.m_iS && snap.m_iTile >= snap.m_aTiles.Count())
					{
						Print(string.Format("MUD voxel probe: player %1 snapshot sent (seq %2, %3 tile parts), live ops next", pid, snap.m_iS, snap.m_aTiles.Count()), LogLevel.NORMAL);
						snap.m_bDone = true;
						snap.m_aTiles.Clear();
						snap.m_iTile = 0;
						snap = null;
					}
				}
				else
					next = JipBatches(from, JIP_MSGS_PER_FRAME, batches);
				foreach (string b : batches)
					pc.MUD_SendVoxelJip(b);
				if (!snap && next > newest)
				{
					Print(string.Format("MUD voxel probe: player %1 op log sent up to seq %2", pid, newest), LogLevel.NORMAL);
					next = -1;
				}
			}
			else
				gone.Insert(pid);
			pids.Insert(pid);
			nexts.Insert(next);
		}
		for (int k = 0; k < pids.Count(); k++)
		{
			if (nexts[k] < 0)
				s_mJipFrom.Remove(pids[k]);
			else
				s_mJipFrom.Set(pids[k], nexts[k]);
		}
		foreach (int g : gone)
			s_mSnap.Remove(g);     // player left: its snapshot goes (a rejoin asks from seq 1 = a new one)
	}

	//------------------------------------------------------------------------------------------------
	//! Server: ops from seq 'from' on as JIP messages of at most JIP_CHARS (one longer op goes alone), at most
	//! maxMsgs messages. Returns the seq after the last op put in (> the newest seq = all sent).
	protected int JipBatches(int from, int maxMsgs, array<string> batches)
	{
		int n = m_aOps.Count();
		int i = from - 1;     // m_aOps is in seq order, seq 1 first: start near, then step to the first seq >= from
		if (i < 0)
			i = 0;
		if (i > n)
			i = n;
		while (i > 0 && m_aOps[i - 1].m_iSeq >= from)
			i--;
		while (i < n && m_aOps[i].m_iSeq < from)
			i++;
		string cur;
		int next = from;
		while (i < n)
		{
			MUD_VoxelOp op = m_aOps[i];
			if (!cur.IsEmpty() && cur.Length() + op.m_sText.Length() + 1 > JIP_CHARS)
			{
				batches.Insert(cur);
				cur = "";
				if (batches.Count() >= maxMsgs)
					return next;     // this op goes first next frame
			}
			next = op.m_iSeq + 1;
			i++;
			if (op.m_sText.IsEmpty())
				continue;
			if (cur.IsEmpty())
				cur = op.m_sText;
			else
				cur = cur + "#" + op.m_sText;
		}
		if (!cur.IsEmpty())
			batches.Insert(cur);
		return next;
	}

	//------------------------------------------------------------------------------------------------
	//! J2 server: op entry at m_aOps[i] for a snapshot up to seq s: a voxel op as sent; a rut op as a placeholder - a
	//! run of rut ops that made no rows as one "a|R|b", a rut op that made rows alone as "a|R|a|rows". iNext = the
	//! index after it.
	protected string SnapEntry(int i, int s, out int iNext)
	{
		MUD_VoxelOp op = m_aOps[i];
		iNext = i + 1;
		if (op.m_iKind != 4)
			return op.m_sText;
		if (!op.m_sRows.IsEmpty())
			return string.Format("%1|R|%1|", op.m_iSeq) + op.m_sRows;
		int last = op.m_iSeq;
		while (iNext < m_aOps.Count())
		{
			MUD_VoxelOp o = m_aOps[iNext];
			if (o.m_iSeq > s || o.m_iSeq != last + 1 || o.m_iKind != 4 || !o.m_sRows.IsEmpty())
				break;
			last = o.m_iSeq;
			iNext++;
		}
		return string.Format("%1|R|%2", op.m_iSeq, last);
	}

	//------------------------------------------------------------------------------------------------
	//! J2 server: the next messages of a snapshot send from seq 'from': head first, then the ops <= S (SnapEntry,
	//! JIP_CHARS per message), then the tile parts, at most maxMsgs messages. Returns the seq after the last op put in
	//! (> S = every op <= S sent; the caller checks the tiles).
	protected int SnapBatches(MUD_JipSnap snap, int from, int maxMsgs, array<string> batches)
	{
		string cur;
		if (!snap.m_bHead)
		{
			cur = string.Format("S|%1|%2", snap.m_iS, snap.m_aTiles.Count());
			snap.m_bHead = true;
		}
		int next = from;
		if (from <= snap.m_iS)
		{
			int n = m_aOps.Count();
			int i = from - 1;
			if (i < 0)
				i = 0;
			if (i > n)
				i = n;
			while (i > 0 && m_aOps[i - 1].m_iSeq >= from)
				i--;
			while (i < n && m_aOps[i].m_iSeq < from)
				i++;
			while (i < n && m_aOps[i].m_iSeq <= snap.m_iS)
			{
				int iNext;
				string e = SnapEntry(i, snap.m_iS, iNext);
				if (!cur.IsEmpty() && cur.Length() + e.Length() + 1 > JIP_CHARS)
				{
					batches.Insert(cur);
					cur = "";
					if (batches.Count() >= maxMsgs)
						return next;     // this entry goes first next frame
				}
				next = m_aOps[iNext - 1].m_iSeq + 1;
				i = iNext;
				if (e.IsEmpty())
					continue;
				if (cur.IsEmpty())
					cur = e;
				else
					cur = cur + "#" + e;
			}
			next = snap.m_iS + 1;
		}
		if (!cur.IsEmpty())
			batches.Insert(cur);
		while (snap.m_iTile < snap.m_aTiles.Count() && batches.Count() < maxMsgs)
		{
			batches.Insert(snap.m_aTiles[snap.m_iTile]);
			snap.m_iTile++;
		}
		return next;
	}

	//------------------------------------------------------------------------------------------------
	//! J2 server: a snapshot of now (seq m_iSeq) - the rut tiles as text parts
	protected MUD_JipSnap MakeSnap()
	{
		MUD_JipSnap snap = new MUD_JipSnap();
		snap.m_iS = m_iSeq;
		SnapTiles(snap.m_iS, snap.m_aTiles);
		// synced mud sheet Stage 5: the sheet layer at S too ("M|" parts after the rut tiles; open op work ends first)
		MUD_MudSync.Flush();
		MUD_MudSync.SnapParts(snap.m_iS, snap.m_aTiles);
		return snap;
	}

	//------------------------------------------------------------------------------------------------
	//! J2: every rut tile (all-none ones too: a tile's existence is part of the state) as parts "T|s|i|tile/tile..."
	//! of at most SNAP_CHARS (one longer tile goes alone)
	protected void SnapTiles(int s, array<string> parts)
	{
		string cur;
		foreach (int key, MUD_RutTile tile : m_mRutTiles)
		{
			string tt = TileText(key, tile);
			if (!cur.IsEmpty() && cur.Length() + tt.Length() + 1 > SNAP_CHARS)
			{
				parts.Insert(string.Format("T|%1|%2|", s, parts.Count()) + cur);
				cur = "";
			}
			if (cur.IsEmpty())
				cur = tt;
			else
				cur = cur + "/" + tt;
		}
		if (!cur.IsEmpty())
			parts.Insert(string.Format("T|%1|%2|", s, parts.Count()) + cur);
	}

	//------------------------------------------------------------------------------------------------
	//! J2: "key:" + the 256 samples in order, comma separated: "n<count>" = a run of FLOOR_NONE, else the floor in
	//! whole cm (absolute y, Stage 1 rounding) as the change from the floor before it in the tile (the first from 0).
	//! Trailing none samples are left out. Build 3 Stage 3: then ";" + the ridge samples the same way (left out when
	//! the tile has no ridge: "key:<floor>" as before).
	protected static string TileText(int key, MUD_RutTile tile)
	{
		string s = key.ToString() + ":" + RunText(tile, 0);
		string rg = RunText(tile, 1);
		if (!rg.IsEmpty())
			s = s + ";" + rg;
		string wr = RunText(tile, 2);     // WEAR COUNT: "~" + the wear list in whole 0.01 passes (left out when none)
		if (!wr.IsEmpty())
			s = s + "~" + wr;
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! J2: one sample list of a tile as TileText's tokens (which 0 = m_aFloor, 1 = m_aRidge, 2 = m_aWear in whole 0.01
	//! passes, none = 0); "" = all none
	protected static string RunText(MUD_RutTile tile, int which)
	{
		string s = "";
		bool first = true;
		int prev = 0;
		int none = 0;
		for (int f = 0; f < MUD_RutTile.SIZE_F; f++)
		{
			float v = tile.m_aFloor[f];
			if (which == 1)
				v = tile.m_aRidge[f];
			bool isNone = v >= FLOOR_NONE * 0.5;
			if (which == 2)
			{
				v = tile.m_aWear[f];
				isNone = v <= 0;
			}
			if (isNone)
			{
				none++;
				continue;
			}
			string tok;
			if (none > 0)
			{
				tok = "n" + none.ToString() + ",";
				none = 0;
			}
			int cm = Math.Round(v * 100);
			int d = cm - prev;
			prev = cm;
			tok = tok + d.ToString();
			if (first)
				s = s + tok;
			else
				s = s + "," + tok;
			first = false;
		}
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! J2: tile part "T|s|i|tile/tile..." into 'into' (a tile already there is replaced). False = malformed or not
	//! from snapshot s (what parsed before the fault stays).
	protected static bool ParseTilePart(string part, int s, map<int, ref MUD_RutTile> into)
	{
		array<string> f = {};
		part.Split("|", f, false);
		if (f.Count() != 4 || f[0] != "T" || !MUD_TrenchPlan.IsInt(f[1]) || f[1].ToInt() != s)
			return false;
		array<string> tiles = {};
		f[3].Split("/", tiles, true);
		array<string> kd = {};
		array<string> toks = {};
		foreach (string t : tiles)
		{
			kd.Clear();
			t.Split(":", kd, false);
			if (kd.Count() != 2 || !MUD_TrenchPlan.IsInt(kd[0]))
				return false;
			MUD_RutTile tile = new MUD_RutTile();
			for (int z = 0; z < MUD_RutTile.SIZE_F; z++)
			{
				tile.m_aFloor[z] = FLOOR_NONE;
				tile.m_aRidge[z] = FLOOR_NONE;
			}
			// Build 3 Stage 3: "<floor>;<ridge>" (no ';' = no ridge); WEAR COUNT: then "~<wear>" (no '~' = no wear)
			string fl = kd[1];
			string wl = "";
			int tilde = fl.IndexOf("~");
			if (tilde >= 0)
			{
				wl = fl.Substring(tilde + 1, fl.Length() - tilde - 1);
				fl = fl.Substring(0, tilde);
				if (wl.IsEmpty() || wl.IndexOf("~") >= 0)
					return false;
			}
			string rl = "";
			int semi = fl.IndexOf(";");
			if (semi >= 0)
			{
				rl = fl.Substring(semi + 1, fl.Length() - semi - 1);
				fl = fl.Substring(0, semi);
				if (rl.IsEmpty() || rl.IndexOf(";") >= 0)
					return false;
			}
			if (!ParseRuns(fl, tile, 0, toks))
				return false;
			if (!rl.IsEmpty() && !ParseRuns(rl, tile, 1, toks))
				return false;
			if (!wl.IsEmpty() && !ParseRuns(wl, tile, 2, toks))
				return false;
			into.Set(kd[0].ToInt(), tile);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! J2: RunText's tokens into tile (which 0 = m_aFloor, 1 = m_aRidge, 2 = m_aWear). toks = scratch. False = malformed.
	protected static bool ParseRuns(string text, MUD_RutTile tile, int which, array<string> toks)
	{
		toks.Clear();
		text.Split(",", toks, true);
		int at = 0;
		int prev = 0;
		foreach (string tok : toks)
		{
			if (tok.Substring(0, 1) == "n")
			{
				string cnt = tok.Substring(1, tok.Length() - 1);
				if (!MUD_TrenchPlan.IsInt(cnt) || cnt.ToInt() < 1)
					return false;
				at += cnt.ToInt();
				continue;
			}
			if (!MUD_TrenchPlan.IsInt(tok) || at >= MUD_RutTile.SIZE_F)
				return false;
			int cm = prev + tok.ToInt();
			prev = cm;
			float v = cm;
			if (which == 2)
				tile.m_aWear[at] = v / 100.0;     // = WearPass' Math.Round(w * 100) / 100.0 (same float)
			else if (which == 1)
				tile.m_aRidge[at] = v / 100.0;     // = GrowRidges' Math.Round(y * 100) / 100.0 (same float)
			else
				tile.m_aFloor[at] = v / 100.0;     // = CarveFloor's Math.Round(y * 100) / 100.0 (same float)
			at++;
		}
		return at <= MUD_RutTile.SIZE_F;
	}

	//------------------------------------------------------------------------------------------------
	//! J2: text is a rut placeholder "a|R|b" / "a|R|b|rows" -> true, last = b, rows (may be empty)
	protected static bool RangeOf(string text, out int last, out string rows)
	{
		last = 0;
		rows = "";
		int bar = text.IndexOf("|");
		if (bar < 1 || text.Length() < bar + 3 || text.Substring(bar + 1, 2) != "R|")
			return false;
		array<string> f = {};
		text.Split("|", f, false);
		if (f.Count() < 3 || f.Count() > 4 || !MUD_TrenchPlan.IsInt(f[0]) || !MUD_TrenchPlan.IsInt(f[2]))
			return false;
		last = f[2].ToInt();
		if (last < f[0].ToInt())
			return false;
		if (f.Count() == 4)
			rows = f[3];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! J2: op text "seq|4|..." (a real rut op)
	protected static bool IsRutText(string text)
	{
		int bar = text.IndexOf("|");
		return bar > 0 && text.Length() >= bar + 3 && text.Substring(bar + 1, 2) == "4|";
	}

	//------------------------------------------------------------------------------------------------
	//! J2 joiner: held ops a..b go (a placeholder covered them)
	protected static void DropHeld(int a, int b)
	{
		if (b < a || s_mPending.IsEmpty())
			return;
		if (b - a + 1 <= s_mPending.Count())
		{
			for (int q = a; q <= b; q++)
				s_mPending.Remove(q);
			return;
		}
		array<int> drop = {};
		foreach (int k, string t : s_mPending)
		{
			if (k >= a && k <= b)
				drop.Insert(k);
		}
		foreach (int dk : drop)
			s_mPending.Remove(dk);
	}

	//------------------------------------------------------------------------------------------------
	//! J2 joiner: snapshot head "S|s|parts"
	protected static void ReceiveSnapHead(string t)
	{
		array<string> f = {};
		t.Split("|", f, false);
		if (f.Count() != 3 || !MUD_TrenchPlan.IsInt(f[1]) || !MUD_TrenchPlan.IsInt(f[2]))
		{
			Print("MUD voxel probe: bad snapshot head from the server: " + t, LogLevel.ERROR);
			return;
		}
		int s = f[1].ToInt();
		int n = f[2].ToInt();
		if (s < 1 || n < 0 || n > 200000)
			return;
		if (s == s_iSnapS)
			return;     // the same snapshot again
		if (s_iNextSeq > s + 1)
		{
			Print(string.Format("MUD voxel probe: snapshot at seq %1 ignored - ops up to seq %2 already applied", s, s_iNextSeq - 1), LogLevel.WARNING);
			return;
		}
		s_iSnapS = s;
		s_aSnapParts.Clear();
		for (int i = 0; i < n; i++)
			s_aSnapParts.Insert("");
		s_iSnapGot = 0;
		s_iSnapWaitAt = 0;
		s_bToldSnap = false;
		s_iSnapRows = 0;
		s_iSnapSkipped = 0;
		MUD_MudSync.SnapComing(s);     // synced mud sheet Stage 5: ops <= s clear nothing here (the layer comes at s)
		Print(string.Format("MUD voxel probe: snapshot coming - ops up to seq %1 (rut ops as placeholders), then %2 rut tile parts", s, n), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! J2 joiner: one tile part "T|s|i|..." (kept as text until SnapGate loads them all)
	protected static void ReceiveSnapTile(string t)
	{
		array<string> f = {};
		t.Split("|", f, false);
		if (f.Count() != 4 || !MUD_TrenchPlan.IsInt(f[1]) || !MUD_TrenchPlan.IsInt(f[2]))
		{
			Print("MUD voxel probe: bad snapshot tile part from the server", LogLevel.ERROR);
			return;
		}
		int s = f[1].ToInt();
		int i = f[2].ToInt();
		if (s != s_iSnapS || i < 0 || i >= s_aSnapParts.Count())
		{
			Print(string.Format("MUD voxel probe: snapshot tile part %1 of seq %2 dropped (expecting seq %3, %4 parts)", i, s, s_iSnapS, s_aSnapParts.Count()), LogLevel.WARNING);
			return;
		}
		if (!s_aSnapParts[i].IsEmpty())
			return;
		s_aSnapParts.Set(i, t);
		s_iSnapGot++;
	}

	//------------------------------------------------------------------------------------------------
	//! J2 joiner: true = ops may go on. Once every op <= the snapshot seq is in, its rut tiles load first (every part
	//! must be here; reliable messages in order, so a wait is logged once after 10 s, not asked again).
	protected static bool SnapGate()
	{
		if (s_iSnapS <= 0 || s_iNextSeq <= s_iSnapS)
			return true;
		if (s_iSnapGot < s_aSnapParts.Count())
		{
			if (s_iSnapWaitAt == 0)
				s_iSnapWaitAt = System.GetTickCount();
			else if (!s_bToldSnap && System.GetTickCount(s_iSnapWaitAt) > 10000)
			{
				s_bToldSnap = true;
				Print(string.Format("MUD voxel probe: waiting 10 s for snapshot tiles of seq %1 (%2 of %3 parts here)", s_iSnapS, s_iSnapGot, s_aSnapParts.Count()), LogLevel.WARNING);
			}
			return false;
		}
		s_Inst.LoadSnap();
		s_iSnapS = 0;
		s_aSnapParts.Clear();
		s_iSnapGot = 0;
		s_iSnapWaitAt = 0;
		s_bToldSnap = false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! J2 joiner: the snapshot's rut tiles replace ours (every op <= S is in: craters / trenches / fills cleared their
	//! floor on the server before S, so the tiles come after them), then the patches and bricks over them redraw
	protected void LoadSnap()
	{
		int t0 = System.GetTickCount();
		// every tile key before or after: its patches redraw
		set<int> keys = new set<int>();
		foreach (int ok, MUD_RutTile ot : m_mRutTiles)
			keys.Insert(ok);
		m_mRutTiles.Clear();
		int bad;
		array<string> sheetParts = {};
		foreach (string part : s_aSnapParts)
		{
			if (part.Length() >= 2 && part.Substring(0, 2) == "M|")
			{
				sheetParts.Insert(part);     // synced mud sheet Stage 5 (MUD_MudSync.LoadSnap below)
				continue;
			}
			if (!ParseTilePart(part, s_iSnapS, m_mRutTiles))
				bad++;
		}
		int floorN;
		int ridgeN;
		foreach (int key, MUD_RutTile tile : m_mRutTiles)
		{
			keys.Insert(key);
			for (int f = 0; f < MUD_RutTile.SIZE_F; f++)
			{
				if (tile.m_aFloor[f] < FLOOR_NONE * 0.5)
					floorN++;
				if (tile.m_aRidge[f] < FLOOR_NONE * 0.5)
					ridgeN++;
			}
		}
		OpenBatch(t0, s_iSnapS);
		int queued;
		foreach (int dk : keys)
		{
			int bi = KeyBi(dk);
			int bk = KeyBk(dk);
			MUD_WorldGroundEntity.FloorDirty(bi * FSUB - 1, bi * FSUB + FSUB, bk * FSUB - 1, bk * FSUB + FSUB);
			MUD_VoxelColumn col = m_mCols.Get(dk);
			if (col)
				queued += QueueCol(col);
		}
		Print(string.Format("MUD voxel probe: snapshot loaded at seq %1: %2 rut tiles (%3 floor, %4 ridge samples), %5 bad parts, %6 rut ops skipped, %7 rows from them, %8 bricks queued, %9 ms",
			s_iSnapS, m_mRutTiles.Count(), floorN, ridgeN, bad, s_iSnapSkipped, s_iSnapRows, queued, System.GetTickCount(t0)), LogLevel.NORMAL);
		if (bad > 0)
			Print("MUD voxel probe: snapshot tile parts did not parse - ruts on this machine differ from the server's", LogLevel.ERROR);
		// synced mud sheet Stage 5: the skipped rut ops' mud comes as the server's layer at S (no M parts = none sent)
		MUD_MudSync.LoadSnap(s_iSnapS, sheetParts);
	}

	//------------------------------------------------------------------------------------------------
	//! J2 joiner: the brick rows a skipped rut op made on the server ("colkey,bj;..."), made here at its seq
	protected void RangeRows(string rows)
	{
		if (rows.IsEmpty())
			return;
		array<string> rs = {};
		rows.Split(";", rs, true);
		array<string> kv = {};
		foreach (string r : rs)
		{
			kv.Clear();
			r.Split(",", kv, false);
			if (kv.Count() != 2 || !MUD_TrenchPlan.IsInt(kv[0]) || !MUD_TrenchPlan.IsInt(kv[1]))
			{
				Print("MUD voxel probe: bad rut placeholder rows from the server: " + rows, LogLevel.ERROR);
				return;
			}
			MUD_VoxelColumn col = m_mCols.Get(kv[0].ToInt());
			if (!col)
			{
				Print(string.Format("MUD voxel probe: rut placeholder row %1 for a column this machine does not have (op log differs?)", r), LogLevel.ERROR);
				continue;
			}
			if (!m_bOpen)
				OpenBatch(System.GetTickCount(), s_iNextSeq);
			Row(col, kv[1].ToInt(), true);
			s_iSnapRows++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! J2 test kit, server: SnapSelfTest every SNAP_TEST_MS while the seq moves and rut tiles exist
	protected void SnapSelfTestTick()
	{
		if (m_iSeq == m_iSnapTestSeq || m_mRutTiles.IsEmpty())
			return;
		if (m_iSnapTestAt != 0 && System.GetTickCount(m_iSnapTestAt) < SNAP_TEST_MS)
			return;
		m_iSnapTestAt = System.GetTickCount();
		m_iSnapTestSeq = m_iSeq;
		SnapSelfTest();
	}

	//------------------------------------------------------------------------------------------------
	//! J2 test kit, server: builds the snapshot of now, parses it into a fresh tile map and compares every sample with
	//! the live tiles; sizes vs the rut op log; checks the op stream (voxel ops + placeholders) covers seq 1..S once.
	protected void SnapSelfTest()
	{
		int s = m_iSeq;
		MUD_MudSync.SnapSelfTest(s);     // synced mud sheet Stage 5: its layer through the snapshot text and back
		int t0 = System.GetTickCount();
		array<string> parts = {};
		SnapTiles(s, parts);
		int buildMs = System.GetTickCount(t0);
		int t1 = System.GetTickCount();
		map<int, ref MUD_RutTile> back = new map<int, ref MUD_RutTile>();
		int badParts;
		int tileBytes;
		foreach (string part : parts)
		{
			tileBytes += part.Length();
			if (!ParseTilePart(part, s, back))
				badParts++;
		}
		int parseMs = System.GetTickCount(t1);
		int compared;
		int floorN;
		int ridgeN;
		int bad;
		int missing;
		foreach (int key, MUD_RutTile tile : m_mRutTiles)
		{
			MUD_RutTile bt = back.Get(key);
			if (!bt)
			{
				missing++;
				continue;
			}
			for (int f = 0; f < MUD_RutTile.SIZE_F; f++)
			{
				compared++;
				if (tile.m_aFloor[f] < FLOOR_NONE * 0.5)
					floorN++;
				if (tile.m_aRidge[f] < FLOOR_NONE * 0.5)
					ridgeN++;
				// a sample counts once when its floor, its ridge (Build 3 Stage 3) or its wear (WEAR COUNT) differs
				if (tile.m_aFloor[f] != bt.m_aFloor[f] || tile.m_aRidge[f] != bt.m_aRidge[f] || tile.m_aWear[f] != bt.m_aWear[f])
					bad++;
			}
		}
		int extra = back.Count() - (m_mRutTiles.Count() - missing);
		Print(string.Format("MUD snapshot self-test: seq %1, %2 tiles, %3 samples (%4 floor, %5 ridge), %6 mismatched, %7 missing, %8 extra, %9 bad parts",
			s, m_mRutTiles.Count(), compared, floorN, ridgeN, bad, missing, extra, badParts), LogLevel.NORMAL);
		// sizes: the rut op log vs the tiles; the op stream a joiner gets up to s, and its seq cover
		int rutOps;
		int rutBytes;
		int rowOps;
		int rowsN;
		int logBytes;
		foreach (MUD_VoxelOp op : m_aOps)
		{
			if (op.m_iSeq > s)
				break;
			logBytes += op.m_sText.Length() + 1;
			if (op.m_iKind != 4)
				continue;
			rutOps++;
			rutBytes += op.m_sText.Length() + 1;
			if (!op.m_sRows.IsEmpty())
			{
				rowOps++;
				array<string> rr = {};
				op.m_sRows.Split(";", rr, true);
				rowsN += rr.Count();
			}
		}
		int streamBytes;
		int entries;
		int gaps;
		int want = 1;
		int i;
		while (i < m_aOps.Count() && m_aOps[i].m_iSeq <= s)
		{
			int iNext;
			string e = SnapEntry(i, s, iNext);
			i = iNext;
			entries++;
			streamBytes += e.Length() + 1;
			int bar = e.IndexOf("|");
			int a = e.Substring(0, bar).ToInt();
			int last;
			string rows;
			if (!RangeOf(e, last, rows))
				last = a;
			if (a != want)
				gaps++;
			want = last + 1;
		}
		if (want != s + 1)
			gaps++;
		Print(string.Format("MUD snapshot self-test: tiles %1 bytes in %2 parts (build %3 ms, parse %4 ms) vs rut op log %5 bytes (%6 rut ops, %7 made rows, %8 rows)",
			tileBytes, parts.Count(), buildMs, parseMs, rutBytes, rutOps, rowOps, rowsN), LogLevel.NORMAL);
		Print(string.Format("MUD snapshot self-test: joiner op stream to seq %1 = %2 bytes in %3 entries (full op log %4 bytes), %5 seq gaps",
			s, streamBytes, entries, logBytes, gaps), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static void Clear()
	{
		if (s_Inst)
			s_Inst.Free();
		s_Inst = null;
		s_bDigHeld = false;
		s_mPending.Clear();
		s_iNextSeq = 1;
		s_bAskedJip = false;
		s_iGapAt = 0;
		s_bToldGap = false;
		s_bLost = false;
		s_iAskAt = 0;
		s_iAsks = 0;
		s_mJipFrom.Clear();
		s_mSnap.Clear();
		s_iSnapS = 0;
		s_aSnapParts.Clear();
		s_iSnapGot = 0;
		s_iSnapWaitAt = 0;
		s_bToldSnap = false;
		s_iSnapRows = 0;
		s_iSnapSkipped = 0;
		s_aTracks.Clear();
		s_iSheetVeh = 0;
		MUD_MudSync.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected bool Init()
	{
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!skin)
		{
			Print("MUD voxel probe: no coarse skin - off", LogLevel.ERROR);
			return false;
		}
		int ncx;
		int ncz;
		skin.GetGrid(m_fX0, m_fZ0, ncx, ncz);
		m_fCell = skin.GetCell();
		// U1 sea guard: world data, so the same on every machine (logged here: 1-MP compares server and client lines)
		BaseWorld seaWorld = GetGame().GetWorld();
		if (seaWorld && seaWorld.IsOcean())
			m_fSeaY = seaWorld.GetOceanBaseHeight() + SEA_GAP;
		Print(string.Format("MUD voxel probe: tunnel floor %1 m, sea guard y %2 (ocean %3)", TUNNEL_FLOOR, m_fSeaY, seaWorld && seaWorld.IsOcean()), LogLevel.NORMAL);
		if (Math.AbsFloat(m_fCell * 2 - BRICK) > 0.001)
		{
			Print(string.Format("MUD voxel probe: skin cell %1 m - bricks need 2 m cells, off", m_fCell), LogLevel.ERROR);
			return false;
		}
		// RUT FIELD (plan 'facts'): floor sample fi, fk must be fine patch vertex gx, gz (same origin, same spacing)
		if (RUT_FIELD)
		{
			float pgx0;
			float pgz0;
			float psize = MUD_WorldGroundEntity.ProbePatchGrid(pgx0, pgz0);
			float pfine = psize / MUD_WorldGroundEntity.PCELLS;
			bool same = Math.AbsFloat(pfine - FCELL) < 0.0001 && Math.AbsFloat(pgx0 - m_fX0) < 0.001 && Math.AbsFloat(pgz0 - m_fZ0) < 0.001;
			LogLevel sameLvl = LogLevel.NORMAL;
			if (!same)
				sameLvl = LogLevel.ERROR;
			Print(string.Format("MUD rut field: floor grid on the patch vertices %1 (patch fine %2 m, floor %3 m, origin %4 %5 / %6 %7)",
				same, pfine, FCELL, pgx0, pgz0, m_fX0, m_fZ0), sameLvl);
		}
		if (!SizesOk())
			return false;
		if (DISPLACE_RUTS && RUT_LOOK)
			Print("MUD voxel probe: DISPLACE_RUTS and RUT_LOOK both on - LookRuts() keeps the look rut off; turn RUT_LOOK off", LogLevel.ERROR);
		m_bHeadless = RplSession.Mode() == RplMode.Dedicated;    // as MUD_CoarseSkin m_bRender
		m_iDispT0 = System.GetTickCount();
#ifdef ENABLE_DIAG
		DispMenu();
#endif
		// x edges, y edges, z edges of a cube (corner bit 1 = +x, 2 = +y, 4 = +z)
		int ea[] = {0, 2, 4, 6, 0, 1, 4, 5, 0, 1, 2, 3};
		int eb[] = {1, 3, 5, 7, 2, 3, 6, 7, 4, 5, 6, 7};
		for (int e = 0; e < 12; e++)
		{
			m_aEa[e] = ea[e];
			m_aEb[e] = eb[e];
		}
		for (int q = 0; q < 8; q++)
		{
			// bit ops inside Vector(...) args: ints first (as the '%' compile error 2026-10-12)
			int bx = q & 1;
			int by = (q >> 1) & 1;
			int bz = (q >> 2) & 1;
			m_aOffV[q] = Vector(bx * CELL, by * CELL, bz * CELL);
			m_aOff[q] = bx + by * GN + bz * GN2;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The SIZE SWITCH lines all on one setting (Tools/voxel_size.py): the data-class array sizes match the probe's
	protected bool SizesOk()
	{
		bool ok = Math.AbsFloat(SUB * CELL - BRICK) < 0.001 && MAX_V < VKEY;
		ok = ok && MUD_VoxelVerts.SIZE_V == MAX_V && MUD_VoxelIdx.SIZE_I == MAX_I && MUD_VoxelSlots.SIZE_C == CN3;
		ok = ok && MUD_VoxelGather.SIZE_G == GN3 && MUD_VoxelGather.SIZE_T == GN2 && MUD_VoxelGather.SIZE_C == CN3;
		ok = ok && MUD_VoxelBrick.SIZE_D == SUB3 && MUD_VoxelColumn.SIZE_N == SUB2;
		ok = ok && MUD_RutTile.SIZE_F == FSUB * FSUB;
		if (DISPLACE_RUTS)
			ok = ok && Math.AbsFloat(FCELL * 2 - CELL) < 0.001;
		if (!ok)
		{
			Print(string.Format("MUD voxel probe: SIZE SWITCH mixed (SUB %1 CELL %2, brick %3 column %4 verts %5) - run Tools/voxel_size.py, off",
				SUB, CELL, MUD_VoxelBrick.SIZE_D, MUD_VoxelColumn.SIZE_N, MUD_VoxelVerts.SIZE_V), LogLevel.ERROR);
			return false;
		}
		Print(string.Format("MUD voxel probe: cell %1 m, %2 per brick side", CELL, SUB), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! a / b rounded down (b > 0), exact for negative a
	protected static int FloorDiv(int a, int b)
	{
		if (a >= 0)
			return a / b;
		return -((-a + b - 1) / b);
	}

	//------------------------------------------------------------------------------------------------
	//! Play stop, as ~MUD_WorldGroundEntity: body (via the live entity), geoms, entity. An entity already gone:
	//! leave its body and geoms alone (destroying them then risks the 12:56 heap corruption).
	protected void Free()
	{
		foreach (MUD_VoxelGrave gr : m_aGrave)
			FreeGrave(gr);
		m_aGrave.Clear();
		foreach (int key, MUD_VoxelColumn col : m_mCols)
		{
			foreach (int bj, MUD_VoxelBrick b : col.m_mRows)
				FreeBrick(b);
			col.m_mRows.Clear();
		}
		m_mCols.Clear();
		m_mRutTiles.Clear();
		m_aOps.Clear();
		m_aQueue.Clear();
		m_aShow.Clear();
		m_aSwap.Clear();
		m_aNewCarved.Clear();
		m_aOpCarved.Clear();
		m_aNewLook.Clear();
		m_aOpLook.Clear();
		m_mLays.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void FreeBrick(MUD_VoxelBrick b)
	{
		b.m_bNew = false;
		b.m_NewPhys = null;
		if (b.m_NewEnt)
		{
			Physics nph = b.m_NewEnt.GetPhysics();
			if (nph)
				nph.Destroy();
			foreach (PhysicsGeom ng : b.m_aNewGeoms)
			{
				if (ng)
					ng.Destroy();
			}
			delete b.m_NewEnt;
		}
		b.m_aNewGeoms.Clear();
		b.m_NewRes = null;
		b.m_Phys = null;
		if (b.m_Ent)
		{
			Physics ph = b.m_Ent.GetPhysics();
			if (ph)
				ph.Destroy();
			foreach (PhysicsGeom g : b.m_aGeoms)
			{
				if (g)
					g.Destroy();
			}
			delete b.m_Ent;
		}
		b.m_aGeoms.Clear();
		b.m_Res = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void DropBody(MUD_VoxelBrick b)
	{
		if (b.m_Phys)
		{
			b.m_Phys.Destroy();
			b.m_Phys = null;
		}
		foreach (PhysicsGeom g : b.m_aGeoms)
		{
			if (g)
				g.Destroy();
		}
		b.m_aGeoms.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! A shown brick's pending build out (rebuilt again, or play stop)
	protected void DropPending(MUD_VoxelBrick b)
	{
		b.m_bNew = false;
		b.m_bMeshOnly = false;
		if (b.m_NewEnt)
		{
			Bury(b.m_NewEnt, b.m_NewRes, b.m_aNewGeoms);
		}
		else
		{
			if (b.m_NewPhys)
				b.m_NewPhys.Destroy();
			foreach (PhysicsGeom g : b.m_aNewGeoms)
			{
				if (g)
					g.Destroy();
			}
		}
		b.m_NewPhys = null;
		b.m_aNewGeoms.Clear();
		b.m_NewEnt = null;
		b.m_NewRes = null;
	}

	//------------------------------------------------------------------------------------------------
	//! ent (a brick entity, res its mesh, geoms its collider's) out now - hidden, collision off - and deleted
	//! GRAVE_MS later by Graves. Takes the geoms (geoms is cleared).
	protected void Bury(IEntity ent, Resource res, array<PhysicsGeom> geoms)
	{
		MUD_VoxelGrave g = new MUD_VoxelGrave();
		g.m_Ent = ent;
		g.m_Res = res;
		g.m_iAt = System.GetTickCount();
		foreach (PhysicsGeom pg : geoms)
			g.m_aGeoms.Insert(pg);
		geoms.Clear();
		ent.ClearFlags(EntityFlags.VISIBLE, false);
		Physics ph = ent.GetPhysics();
		if (ph)
		{
			ph.SetInteractionLayer(0);
			int n = ph.GetNumGeoms();
			for (int i = 0; i < n; i++)
				ph.SetGeomInteractionLayer(i, 0);
		}
		m_aGrave.Insert(g);
	}

	//------------------------------------------------------------------------------------------------
	//! Buried brick entities older than GRAVE_MS go: body (via the live entity), geoms, entity (Free's order)
	protected void Graves()
	{
		if (m_aGrave.IsEmpty())
			return;
		int now = System.GetTickCount();
		for (int i = m_aGrave.Count() - 1; i >= 0; i--)
		{
			if (now - m_aGrave[i].m_iAt < GRAVE_MS)
				continue;
			FreeGrave(m_aGrave[i]);
			m_aGrave.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FreeGrave(MUD_VoxelGrave g)
	{
		if (g.m_Ent)
		{
			Physics ph = g.m_Ent.GetPhysics();
			if (ph)
				ph.Destroy();
			foreach (PhysicsGeom pg : g.m_aGeoms)
			{
				if (pg)
					pg.Destroy();
			}
			delete g.m_Ent;
		}
		g.m_aGeoms.Clear();
		g.m_Res = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Keys and grid
	protected static int ColKey(int bi, int bk)
	{
		return (bk + KEY_OFF) * KEY_W + bi + KEY_OFF;
	}

	//------------------------------------------------------------------------------------------------
	protected static int KeyBi(int key)
	{
		return key % KEY_W - KEY_OFF;
	}

	//------------------------------------------------------------------------------------------------
	protected static int KeyBk(int key)
	{
		return key / KEY_W - KEY_OFF;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CellKey(int cx, int cz)
	{
		return (cz + KEY_OFF) * CELL_KEY_W + cx + KEY_OFF;
	}

	//------------------------------------------------------------------------------------------------
	//! Global sample index nearest world x / z
	protected int SampleI(float x)
	{
		return Math.Round((x - m_fX0) / CELL);
	}

	//------------------------------------------------------------------------------------------------
	protected int SampleK(float z)
	{
		return Math.Round((z - m_fZ0) / CELL);
	}

	//------------------------------------------------------------------------------------------------
	//! Converted column holding global sample column gi, gk, or null
	protected MUD_VoxelColumn ColAt(int gi, int gk)
	{
		int bi = FloorDiv(gi, SUB);
		int bk = FloorDiv(gk, SUB);
		return m_mCols.Get(ColKey(bi, bk));
	}

	//------------------------------------------------------------------------------------------------
	//! Any converted column (4 m brick column) meets the x, z box lo..hi
	protected bool HasCols(vector lo, vector hi)
	{
		int bi0 = Math.Floor((lo[0] - m_fX0) / BRICK);
		int bi1 = Math.Floor((hi[0] - m_fX0) / BRICK);
		int bk0 = Math.Floor((lo[2] - m_fZ0) / BRICK);
		int bk1 = Math.Floor((hi[2] - m_fZ0) / BRICK);
		for (int bk = bk0; bk <= bk1; bk++)
		{
			for (int bi = bi0; bi <= bi1; bi++)
			{
				if (m_mCols.Get(ColKey(bi, bk)))
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Columns an op with this x, z box converts: those it touches + one ring, bk then bi ascending (the same order on
	//! every machine: the seed follows it)
	protected void ConvCols(vector lo, vector hi, array<int> keys)
	{
		int bi0 = Math.Floor((lo[0] - m_fX0) / BRICK) - 1;
		int bi1 = Math.Floor((hi[0] - m_fX0) / BRICK) + 1;
		int bk0 = Math.Floor((lo[2] - m_fZ0) / BRICK) - 1;
		int bk1 = Math.Floor((hi[2] - m_fZ0) / BRICK) + 1;
		for (int bk = bk0; bk <= bk1; bk++)
		{
			for (int bi = bi0; bi <= bi1; bi++)
				keys.Insert(ColKey(bi, bk));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Values within 5 mm of 0 move off it, so no crossing sits exactly on a sample
	protected static float Nudge(float d)
	{
		if (d > -0.005 && d < 0.005)
		{
			if (d >= 0)
				return 0.005;
			return -0.005;
		}
		return d;
	}

	//------------------------------------------------------------------------------------------------
	//! The seeded field of col at owned sample column n, global row gj (what a missing brick row holds)
	protected static float Seeded(MUD_VoxelColumn col, int n, int gj)
	{
		return Nudge(col.m_aTop[n] - gj * CELL);
	}

	//------------------------------------------------------------------------------------------------
	//! Sample li, lk (column local), global row gj of col: its brick, else the seeded field
	protected float Value(MUD_VoxelColumn col, int li, int lk, int gj)
	{
		int bj = FloorDiv(gj, SUB);
		MUD_VoxelBrick b = col.m_mRows.Get(bj);
		if (!b)
			return Seeded(col, lk * SUB + li, gj);
		return b.m_aD[(lk * SUB + gj - bj * SUB) * SUB + li];
	}

	//------------------------------------------------------------------------------------------------
	//! Test kit (2026-10-17): sample li, lk, gj of col now d -> into this op's field hash. Column key, sample index and
	//! the value in cm, mixed (one non-linear step) and summed mod DISP_HASH_M; every product stays under 2^31.
	protected void FieldHashAdd(MUD_VoxelColumn col, int li, int lk, int gj, float d)
	{
		int ck = ColKey(col.m_iBi, col.m_iBk) % DISP_HASH_M;
		int si = ((gj * SUB + lk) * SUB + li) % DISP_HASH_M;
		if (si < 0)
			si += DISP_HASH_M;
		int cmRaw = Math.Round(d * 100);
		int cm = cmRaw % DISP_HASH_M;
		if (cm < 0)
			cm += DISP_HASH_M;
		int hv = (ck * 31 + si) % DISP_HASH_M;
		hv = (hv * 1009 + cm) % DISP_HASH_M;
		hv = (hv * (hv % 2003 + 1)) % DISP_HASH_M;
		m_iFieldHash = (m_iFieldHash + hv) % DISP_HASH_M;
		m_iFieldHashN++;
	}

	//------------------------------------------------------------------------------------------------
	//! Sample li, lk, gj of col (global gi, gk) from old to d. A missing row is made only when the change matters for
	//! a mesh (a sign change, or a value near 0); else the seeded field stays. Marks the changed box.
	protected void Put(MUD_VoxelColumn col, int li, int lk, int gi, int gj, int gk, float old, float d)
	{
		if (Math.AbsFloat(d - old) < 0.00001)
			return;
		if (m_bTopTrack)
			KeepTop(gi, gk);
		int bj = FloorDiv(gj, SUB);
		MUD_VoxelBrick b = col.m_mRows.Get(bj);
		if (!b)
		{
			if ((d > 0) == (old > 0) && Math.AbsFloat(d) >= KEEP_ROW && Math.AbsFloat(old) >= KEEP_ROW)
				return;
			b = Row(col, bj, true);
		}
		int pi = (lk * SUB + gj - bj * SUB) * SUB + li;
		b.m_aD[pi] = d;
		if (MUD_TestKit.TEST_KIT)
			FieldHashAdd(col, li, lk, gj, d);
		// a raise (fill) drops a look cut still waiting there (RUT_LOOK): it would draw under the new top
		if (b.m_bHasV && d > old)
			b.m_aV[pi] = V_NONE;
		// carved only where the drawn surface moves: a sign change, or a sample next to the surface moving more than
		// CARVE_TOL. The crater funnel also lowers air samples out to ~6 m (no visible change); counting those made
		// 40 cells VOX per mortar and hid the ruts round it (user test 3, 2026-10-13 20:36).
		bool moved = (d > 0) != (old > 0);
		if (!moved && Math.AbsFloat(d - old) > CARVE_TOL && Math.Min(Math.AbsFloat(d), Math.AbsFloat(old)) < CELL * 1.2)
			moved = true;
		if (moved && (gi != m_iMarkGi || gk != m_iMarkGk))
		{
			m_iMarkGi = gi;
			m_iMarkGk = gk;
			MarkCarved(gi, gk);
			// DISPLACED RUTS: crater / trench / fill moved the ground here - its rut floor goes (ClearFloor)
			if (DISPLACE_RUTS && (m_iOpKind == 0 || m_iOpKind == 2 || m_iOpKind == 3))
			{
				m_aClrI.Insert(gi);
				m_aClrK.Insert(gk);
			}
		}
		MarkBox(gi, gj, gk);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_LOOK (op 6): sample li, lk, gj of col (global gi, gk) drawn at d (under the feel value old) until a feel cut
	//! catches up - only the mesh reads it (Gather look); bodies and every later op read the feel field. Makes the row
	//! like Put. Never marks a cell carved: a cell not voxel yet keeps the heightfield (the look shows there once a
	//! feel cut converts it).
	protected void PutLook(MUD_VoxelColumn col, int li, int lk, int gi, int gj, int gk, float old, float d)
	{
		int bj = FloorDiv(gj, SUB);
		MUD_VoxelBrick b = col.m_mRows.Get(bj);
		if (!b)
		{
			if ((d > 0) == (old > 0) && Math.AbsFloat(d) >= KEEP_ROW && Math.AbsFloat(old) >= KEEP_ROW)
				return;
			b = Row(col, bj, true);
		}
		if (!b.m_bHasV)
		{
			for (int n = 0; n < SUB3; n++)
				b.m_aV[n] = V_NONE;
			b.m_bHasV = true;
		}
		int pi = (lk * SUB + gj - bj * SUB) * SUB + li;
		if (d >= b.m_aV[pi] - 0.00001)
			return;
		b.m_aV[pi] = d;
		// the drawn surface moved (as Put): its cells show the voxel mesh now (look cells), the bodies wait
		bool moved = (d > 0) != (old > 0);
		if (!moved && Math.AbsFloat(d - old) > CARVE_TOL && Math.Min(Math.AbsFloat(d), Math.AbsFloat(old)) < CELL * 1.2)
			moved = true;
		if (moved && !m_bHeadless && (gi != m_iMarkGi || gk != m_iMarkGk))
		{
			m_iMarkGi = gi;
			m_iMarkGk = gk;
			MarkLook(gi, gk);
		}
		MarkBox(gi, gj, gk);
	}

	//------------------------------------------------------------------------------------------------
	//! Grows the changed box (Submit queues its bricks) by sample gi, gj, gk
	protected void MarkBox(int gi, int gj, int gk)
	{
		if (!m_bChanged)
		{
			m_bChanged = true;
			m_iCi0 = gi;
			m_iCi1 = gi;
			m_iCj0 = gj;
			m_iCj1 = gj;
			m_iCk0 = gk;
			m_iCk1 = gk;
			return;
		}
		if (gi < m_iCi0)
			m_iCi0 = gi;
		if (gi > m_iCi1)
			m_iCi1 = gi;
		if (gj < m_iCj0)
			m_iCj0 = gj;
		if (gj > m_iCj1)
			m_iCj1 = gj;
		if (gk < m_iCk0)
			m_iCk0 = gk;
		if (gk > m_iCk1)
			m_iCk1 = gk;
	}

	//------------------------------------------------------------------------------------------------
	//! Sample column gi, gk changed: the skin cells its surface-net cells reach (+-1 sample) are carved. A cell
	//! carved for the first time goes VOX at the next GoLive; only carved cells draw voxels (so ruts and the rest of
	//! the heightfield stay shown round a crater, user test 2026-10-13 20:30).
	protected void MarkCarved(int gi, int gk)
	{
		int cx0 = FloorDiv(gi - 1, SPS);
		int cx1 = FloorDiv(gi + 1, SPS);
		int cz0 = FloorDiv(gk - 1, SPS);
		int cz1 = FloorDiv(gk + 1, SPS);
		for (int cz = cz0; cz <= cz1; cz++)
		{
			for (int cx = cx0; cx <= cx1; cx++)
			{
				int bi = Math.Floor(cx / 2.0);
				int bk = Math.Floor(cz / 2.0);
				MUD_VoxelColumn col = m_mCols.Get(ColKey(bi, bk));
				if (!col)
					continue;
				int c = (cz - bk * 2) * 2 + cx - bi * 2;
				if (col.m_aCarved[c] != 0)
					continue;
				// ONE_GRID: a crater / trench in a mud zone cell is the sheet's to show (a tunnel's cells stay carved)
				if (m_bSheetOp && MUD_MudSheet.CoversCell(m_fX0 + cx * SPS * CELL, m_fZ0 + cz * SPS * CELL, SPS * CELL))
					continue;
				col.m_aCarved[c] = 1;
				int ck = CellKey(cx, cz);
				m_aNewCarved.Insert(ck);
				m_aOpCarved.Insert(ck);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_LOOK (as MarkCarved): the skin cells round sample column gi, gk are look cells - not carved yet, the patch
	//! mesh goes out there at the next GoLive (collider kept) and the bricks draw their look mesh there
	protected void MarkLook(int gi, int gk)
	{
		int cx0 = FloorDiv(gi - 1, SPS);
		int cx1 = FloorDiv(gi + 1, SPS);
		int cz0 = FloorDiv(gk - 1, SPS);
		int cz1 = FloorDiv(gk + 1, SPS);
		for (int cz = cz0; cz <= cz1; cz++)
		{
			for (int cx = cx0; cx <= cx1; cx++)
			{
				int bi = Math.Floor(cx / 2.0);
				int bk = Math.Floor(cz / 2.0);
				MUD_VoxelColumn col = m_mCols.Get(ColKey(bi, bk));
				if (!col)
					continue;
				int c = (cz - bk * 2) * 2 + cx - bi * 2;
				if (col.m_aCarved[c] != 0 || col.m_aLook[c] != 0)
					continue;
				col.m_aLook[c] = 1;
				int ck = CellKey(cx, cz);
				m_aNewLook.Insert(ck);
				m_aOpLook.Insert(ck);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_LOOK: skin cell cx, cz is a look cell (in a converted column)
	protected bool LookCarved(int cx, int cz)
	{
		int bi = Math.Floor(cx / 2.0);
		int bk = Math.Floor(cz / 2.0);
		MUD_VoxelColumn col = m_mCols.Get(ColKey(bi, bk));
		if (!col)
			return false;
		return col.m_aLook[(cz - bk * 2) * 2 + cx - bi * 2] != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_LOOK, for MUD_WorldGround patches: a look rut's voxel mesh draws in skin cell cx, cz (patch mesh out there,
	//! patch collider kept)
	static bool IsLookCell(int cx, int cz)
	{
		if (!s_Inst || !LookRuts())
			return false;
		return s_Inst.LookCarved(cx, cz);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_LOOK code paths run: never with DISPLACE_RUTS (op 6 would draw the soft voxel rut over the floor)
	static bool LookRuts()
	{
		return RUT_LOOK && !DISPLACE_RUTS;
	}

	//------------------------------------------------------------------------------------------------
	//! Skin cell cx, cz is carved (in a converted column)
	protected bool Carved(int cx, int cz)
	{
		int bi = Math.Floor(cx / 2.0);
		int bk = Math.Floor(cz / 2.0);
		MUD_VoxelColumn col = m_mCols.Get(ColKey(bi, bk));
		if (!col)
			return false;
		return col.m_aCarved[(cz - bk * 2) * 2 + cx - bi * 2] != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Brick row bj of col; make = made from the seeded field when missing (queued, shown at the next GoLive)
	protected MUD_VoxelBrick Row(MUD_VoxelColumn col, int bj, bool make)
	{
		MUD_VoxelBrick b = col.m_mRows.Get(bj);
		if (b || !make)
			return b;
		b = new MUD_VoxelBrick();
		b.m_iBi = col.m_iBi;
		b.m_iBj = bj;
		b.m_iBk = col.m_iBk;
		for (int lk = 0; lk < SUB; lk++)
		{
			for (int lj = 0; lj < SUB; lj++)
			{
				float y = (bj * SUB + lj) * CELL;
				int id = (lk * SUB + lj) * SUB;
				for (int li = 0; li < SUB; li++)
					b.m_aD[id + li] = Nudge(col.m_aTop[lk * SUB + li] - y);
			}
		}
		col.m_mRows.Insert(bj, b);
		m_iBricks++;
		m_iBatchMade++;
		if (m_bRowLog)
		{
			// J2: a rut op made this row - a joiner that skips the op makes it at the same seq (MUD_VoxelOp.m_sRows)
			string rl = string.Format("%1,%2", ColKey(col.m_iBi, col.m_iBk), bj);
			if (m_sRowLog.IsEmpty())
				m_sRowLog = rl;
			else
				m_sRowLog = m_sRowLog + ";" + rl;
		}
		m_aShow.Insert(b);
		Queue(b);
		return b;
	}

	//------------------------------------------------------------------------------------------------
	protected void Queue(MUD_VoxelBrick b)
	{
		if (!m_bQueueLook)
			b.m_bFeelQ = true;
		if (b.m_bQueued)
			return;
		b.m_bQueued = true;
		m_aQueue.Insert(b);
	}

	//------------------------------------------------------------------------------------------------
	//! Queues the bricks whose mesh uses samples gi0..gi1, gj0..gj1, gk0..gk1 (a brick's cells reach one sample past
	//! it on each side, a changed sample moves the cells round it: grow by 2). A missing row of a converted column in
	//! that range is made: it can hold a surface the change exposed. Returns the newly queued.
	protected int QueueBox(int gi0, int gi1, int gj0, int gj1, int gk0, int gk1)
	{
		int bi0 = FloorDiv(gi0 - 2, SUB);
		int bi1 = FloorDiv(gi1 + 2, SUB);
		int bj0 = FloorDiv(gj0 - 2, SUB);
		int bj1 = FloorDiv(gj1 + 2, SUB);
		int bk0 = FloorDiv(gk0 - 2, SUB);
		int bk1 = FloorDiv(gk1 + 2, SUB);
		int queued;
		for (int bk = bk0; bk <= bk1; bk++)
		{
			for (int bi = bi0; bi <= bi1; bi++)
			{
				MUD_VoxelColumn col = m_mCols.Get(ColKey(bi, bk));
				if (!col)
					continue;
				for (int bj = bj0; bj <= bj1; bj++)
				{
					MUD_VoxelBrick b = col.m_mRows.Get(bj);
					if (!b)
					{
						Row(col, bj, true);     // queues it
						queued++;
						continue;
					}
					if (!b.m_bQueued)
						queued++;
					Queue(b);
				}
			}
		}
		return queued;
	}

	//------------------------------------------------------------------------------------------------
	//! Every row of col queued (its edge cells changed: a neighbour column came)
	protected int QueueCol(MUD_VoxelColumn col)
	{
		int queued;
		foreach (int bj, MUD_VoxelBrick b : col.m_mRows)
		{
			if (!b.m_bQueued)
				queued++;
			Queue(b);
		}
		return queued;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: logs the blast as an op. True = taken as voxels; false = the caller digs the heightfield crater.
	//! C2: a hit on dug ground (not under a roof) digs from what is there down to max(depth, min(cap, now + gain x
	//! depth)), as MUD_WorldGroundEntity.Crater - so repeated hits settle at the cap. C6: the outline goes in the op
	//! (m_fB = stretch, m_iPiece = lobe share x 1000000 + turn2 deg x 1000 + turn3 deg, CraterShape).
	protected bool DoBlast(vector wp, float depth, float cap, float gain, float stretch, float lobeW, float turn2, float turn3)
	{
		float dig = depth;
		float now = 0;
		if (!RoofOver(wp))
		{
			float undug;
			if (MUD_WorldGroundEntity.UndugY(wp[0], wp[2], undug))
				now = Math.Max(0, undug - wp[1]);
			if (now > 0.05)
			{
				float target = Math.Max(depth, Math.Min(cap, now + gain * depth));
				dig = target - now;
				if (now >= cap - 0.05)
					dig = depth;    // deep hole (trench, pit): its own crater below the hit, down to CRATER_FLOOR
				if (now >= CRATER_FLOOR - 0.05)
					dig = 0;
				// at the cap: no op (the 0.15 m minimum below made every shell dig on, WB 2026-10-17: 1.99 -> 2.48 m past cap 2.38)
				if (dig < 0.05)
				{
					Print(string.Format("MUD voxel probe: crater on dug ground %1 m deep: at the cap %2, no dig", now, cap), LogLevel.NORMAL);
					return true;
				}
			}
		}
		MUD_VoxelOp op = new MUD_VoxelOp();
		op.m_iKind = 0;
		op.m_vWp = wp;
		op.m_fDepth = Math.Clamp(dig, 0.15, MAX_DEPTH);
		op.m_fB = Math.Clamp(stretch, 0, 0.5);
		int lobe = Math.Round(lobeW * 99);
		lobe = Math.ClampInt(lobe, 0, 99);
		int t2 = Math.Round(turn2 * Math.RAD2DEG);
		int t3 = Math.Round(turn3 * Math.RAD2DEG);
		t2 = Math.ClampInt(t2, 0, 359);
		t3 = Math.ClampInt(t3, 0, 359);
		op.m_iPiece = lobe * 1000000 + t2 * 1000 + t3;
		if (now > 0.05)
			Print(string.Format("MUD voxel probe: crater on dug ground %1 m deep: digs %2 m (charge %3, cap %4)", now, op.m_fDepth, depth, cap), LogLevel.NORMAL);
		return Issue(op);
	}

	//------------------------------------------------------------------------------------------------
	//! C6: op.m_iPiece of a crater back into the outline's lobe share (0..1) and turns (radians). 0 / negative = round.
	protected static void CraterShape(int piece, out float lobeW, out float turn2, out float turn3)
	{
		if (piece < 0)
			piece = 0;
		int lobe = piece / 1000000;
		int rest = piece - lobe * 1000000;
		int t2 = rest / 1000;
		int t3 = rest - t2 * 1000;
		lobeW = lobe / 99.0;
		turn2 = t2 * Math.DEG2RAD;
		turn3 = t3 * Math.DEG2RAD;
	}

	//------------------------------------------------------------------------------------------------
	//! C4: the hit is under a roof (a tunnel): ROOF_MIN or more under the undug ground and solid field from ROOF_GAP up to
	//! ROOF_REACH above it, in the nearest sample column. Reads the field, so the same on every machine. A wall hit whose
	//! nearest column is inside the wall counts as a roof too (a chamber, not a hole to the sky).
	protected bool RoofOver(vector wp)
	{
		int gi = Math.Round((wp[0] - m_fX0) / CELL);
		int gk = Math.Round((wp[2] - m_fZ0) / CELL);
		MUD_VoxelColumn col = ColAt(gi, gk);
		if (!col)
			return false;     // fresh ground: nothing dug, no tunnel
		int li = gi - col.m_iBi * SUB;
		int lk = gk - col.m_iBk * SUB;
		int n = lk * SUB + li;
		if (col.m_aUndug[n] - wp[1] < ROOF_MIN)
			return false;
		int gj0 = Math.Ceil((wp[1] + ROOF_GAP) / CELL);
		int gj1 = Math.Floor((wp[1] + ROOF_REACH) / CELL);
		for (int gj = gj0; gj <= gj1; gj++)
		{
			if (Value(col, li, lk, gj) > 0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server
	protected void DoTunnel(vector mid)
	{
		MUD_VoxelOp op = new MUD_VoxelOp();
		op.m_iKind = 1;
		op.m_vWp = mid;
		op.m_fDepth = TUNNEL_R;
		Issue(op);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, all-voxel test: t's segments as one rut op (refused: dropped, the heightfield takes no rut either)
	protected void DoRut(MUD_VoxelRutTrack t)
	{
		DoRutOp(t, t.m_aSegA, t.m_aSegB, 4);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: segments sa[n] - sb[n] as one rut op of kind (4 = feel cut, 6 = look, RUT_LOOK)
	protected void DoRutOp(MUD_VoxelRutTrack t, array<vector> sa, array<vector> sb, int kind)
	{
		MUD_VoxelOp op = new MUD_VoxelOp();
		op.m_iKind = kind;
		op.m_vWp = sa[0];
		op.m_fDepth = t.m_fStep;
		op.m_fB = t.m_fCap;
		string plan;
		for (int s = 0; s < sa.Count(); s++)
		{
			if (s > 0)
				plan += ";";
			plan += Cm(sa[s]) + "," + Cm(sb[s]);
		}
		op.m_sPlan = plan;
		// synced mud sheet Stage 2 (2026-10-18): a RUT FIELD rut op converts no column, so its seed field carries the
		// sheet tag (Issue keeps it; MUD_MudSync reads it on every machine)
		if (kind == 4 && RUT_FIELD && MUD_MudSheet.ENABLED && MUD_MudSheet.SYNC)
			op.m_sSeed = SheetTag(t);
		Issue(op);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, synced mud sheet Stage 2 (2026-10-18): the sheet tag of t's rut op (MUD_MudSync.MakeTag) - t's vehicle
	//! number, its ground speed and largest wheel radius at the flush, server time. The speed is the flush's, not each
	//! segment's (RUT_BEHIND holds segments up to RUT_BEHIND_MAX_MS). No vehicle (test kit fake ruts): 0 m/s, 0.5 m.
	protected static string SheetTag(MUD_VoxelRutTrack t)
	{
		float speed = 0;
		float radius = 0.5;
		if (t.m_Veh)
		{
			Physics ph = t.m_Veh.GetPhysics();
			if (ph)
			{
				vector vel = ph.GetVelocity();
				speed = Math.Sqrt(vel[0] * vel[0] + vel[2] * vel[2]);
			}
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(t.m_Veh.FindComponent(VehicleWheeledSimulation));
			if (sim && sim.WheelCount() > 0)
			{
				radius = 0;
				for (int w = 0; w < sim.WheelCount(); w++)
					radius = Math.Max(radius, sim.WheelGetRadius(w));
			}
		}
		return MUD_MudSync.MakeTag(t.m_iSheetId, speed, radius, System.GetTickCount() / 100);
	}

	//------------------------------------------------------------------------------------------------
	//! Test kit Stage 3 (2026-10-17, J1 test): server, MUD_TestKit.FakeRutOps() kind-4 rut ops, once, FAKE_RUT_WAIT s
	//! after the probe is up, at most FAKE_RUT_PER_FRAME a frame, through DoRutOp / Issue (seq, broadcast, op log).
	//! One op = a 2 m straight track (4 RUT_STEP segments, y = ground) in a 20 x 20 grid 3 m apart, 10 m off the first
	//! player (none: settings fake_rut_x / z). Ops past 400 go over the same cells again (deeper to the cap, then no change).
	protected void FakeRutTick(float time)
	{
		if (s_iFakeN >= MUD_TestKit.FakeRutOps())
			return;
		if (s_fFakeAt < 0)
		{
			s_fFakeAt = time + MUD_TestKit.FAKE_RUT_WAIT;
			Print(string.Format("MUD fake ruts: %1 ops in %2 s", MUD_TestKit.FakeRutOps(), MUD_TestKit.FAKE_RUT_WAIT), LogLevel.NORMAL);
			return;
		}
		if (time < s_fFakeAt)
			return;
		if (s_iFakeN == 0)
		{
			vector o = Vector(MUD_TestKit.FakeRutX(), 0, MUD_TestKit.FakeRutZ());
			string spot = "fixed spot fake_rut_x / z";
			PlayerManager pm = GetGame().GetPlayerManager();
			if (pm)
			{
				array<int> ids = {};
				pm.GetPlayers(ids);
				foreach (int id : ids)
				{
					IEntity ent = pm.GetPlayerControlledEntity(id);
					if (!ent)
						continue;
					o = ent.GetOrigin() + Vector(10, 0, 10);
					spot = string.Format("10 m off player %1", id);
					break;
				}
			}
			s_vFakeO = o;
			s_iFakeSeq0 = m_iSeq;
			Print(string.Format("MUD fake ruts: start - %1 ops at %2 (%3), next seq %4, %5 per frame",
				MUD_TestKit.FakeRutOps(), o, spot, m_iSeq + 1, MUD_TestKit.FAKE_RUT_PER_FRAME), LogLevel.NORMAL);
		}
		MUD_VoxelRutTrack t = new MUD_VoxelRutTrack();
		t.m_fStep = MUD_TestKit.FAKE_RUT_STEP;
		t.m_fCap = MUD_TestKit.FAKE_RUT_CAP;
		BaseWorld world = GetGame().GetWorld();
		int made;
		while (made < MUD_TestKit.FAKE_RUT_PER_FRAME && s_iFakeN < MUD_TestKit.FakeRutOps())
		{
			int cell = s_iFakeN % 400;
			int col = cell % 20;     // int maths on its own line: a float on the left makes Enforce read '%' as float
			int row = cell / 20;
			float x0 = s_vFakeO[0] + col * 3;
			float z = s_vFakeO[2] + row * 3;
			array<vector> sa = {};
			array<vector> sb = {};
			for (int s = 0; s < 4; s++)
			{
				float xa = x0 + s * RUT_STEP;
				float xb = xa + RUT_STEP;
				sa.Insert(Vector(xa, world.GetSurfaceY(xa, z), z));
				sb.Insert(Vector(xb, world.GetSurfaceY(xb, z), z));
			}
			DoRutOp(t, sa, sb, 4);
			s_iFakeN++;
			made++;
		}
		if (s_iFakeN >= MUD_TestKit.FakeRutOps())
			Print(string.Format("MUD fake ruts: end - %1 tried, %2 issued (seq %3 .. %4)",
				s_iFakeN, m_iSeq - s_iFakeSeq0, s_iFakeSeq0 + 1, m_iSeq), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, all-voxel test: t's ahead strips as one convert op (5)
	protected void DoAhead(MUD_VoxelRutTrack t)
	{
		MUD_VoxelOp op = new MUD_VoxelOp();
		op.m_iKind = 5;
		op.m_vWp = t.m_aAheadA[0];
		op.m_fDepth = RUT_AHEAD_W;
		string plan;
		for (int s = 0; s < t.m_aAheadA.Count(); s++)
		{
			if (s > 0)
				plan += ";";
			plan += Cm(t.m_aAheadA[s]) + "," + Cm(t.m_aAheadB[s]);
		}
		op.m_sPlan = plan;
		Issue(op);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: some skin cell along t's ahead strips (centre line and both edges, every CELL) is not VOX yet
	protected bool AheadOpen(MUD_VoxelRutTrack t)
	{
		for (int s = 0; s < t.m_aAheadA.Count(); s++)
		{
			vector a = t.m_aAheadA[s];
			vector b = t.m_aAheadB[s];
			vector ab = Vector(b[0] - a[0], 0, b[2] - a[2]);
			float len = ab.Length();
			if (len < 0.01)
				continue;
			vector side = Vector(-ab[2] / len, 0, ab[0] / len) * RUT_AHEAD_W;
			int steps = Math.Ceil(len / CELL);
			float fsteps = steps;
			for (int i = 0; i <= steps; i++)
			{
				vector c = a + ab * (i / fsteps);
				for (int e = -1; e <= 1; e++)
				{
					float fe = e;
					vector q = c + side * fe;
					// hard ground (road edge) never goes VOX: it would ask for an op every flush
					int gi = SampleI(q[0]);
					int gk = SampleK(q[2]);
					MUD_VoxelColumn col = ColAt(gi, gk);
					if (col && col.m_aHard[(gk - col.m_iBk * SUB) * SUB + gi - col.m_iBi * SUB] != 0)
						continue;
					int cx = Math.Floor((q[0] - m_fX0) / m_fCell);
					int cz = Math.Floor((q[2] - m_fZ0) / m_fCell);
					if (!Carved(cx, cz))
						return true;
				}
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_BEHIND smart gap: true when every skin cell a cut along a-b can reach (RUT_R + 1 sample round it, as
	//! CarveRut + MarkCarved) is voxel already, so the cut flips no cell from heightfield to voxel
	protected bool SegOnVox(vector a, vector b)
	{
		float r = RUT_R + CELL;
		int cx0 = Math.Floor((Math.Min(a[0], b[0]) - r - m_fX0) / m_fCell);
		int cx1 = Math.Floor((Math.Max(a[0], b[0]) + r - m_fX0) / m_fCell);
		int cz0 = Math.Floor((Math.Min(a[2], b[2]) - r - m_fZ0) / m_fCell);
		int cz1 = Math.Floor((Math.Max(a[2], b[2]) + r - m_fZ0) / m_fCell);
		for (int cz = cz0; cz <= cz1; cz++)
		{
			for (int cx = cx0; cx <= cx1; cx++)
			{
				if (!Carved(cx, cz))
					return false;
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! "x,y,z" in whole cm
	protected static string Cm(vector v)
	{
		int x = Math.Round(v[0] * 100);
		int y = Math.Round(v[1] * 100);
		int z = Math.Round(v[2] * 100);
		return string.Format("%1,%2,%3", x, y, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the op is voxels only if every column it needs is converted or can be (cap, seed size, ground known).
	//! Then: numbered, seeded, turned into text, taken back from the text (so the server rounds as the clients do),
	//! applied and sent to every client. False = refused, nothing logged or sent (the caller digs the heightfield).
	protected bool Issue(MUD_VoxelOp src)
	{
		// round first: the columns (and so the seed) come from the op exactly as every client will apply it (the
		// cm-rounded point can move the box over a brick line)
		src.m_iSeq = m_iSeq + 1;
		MUD_VoxelOp op = Decode(Encode(src));
		if (!op || !Prepare(op))
		{
			Print(string.Format("MUD voxel probe: op kind %1 did not round-trip or its plan does not build - dropped", src.m_iKind), LogLevel.ERROR);
			return false;
		}
		// RUT FIELD: a rut op converts no column (the floor field takes it): no box / cap / seed check, empty seed.
		// Still numbered, sent and logged (JIP replays it in order).
		string seed;
		if (!RUT_FIELD || op.m_iKind != 4)
		{
			array<int> keys = {};
			ConvCols(op.m_vLo, op.m_vHi, keys);
			if (keys.Count() > MAX_OP_COLUMNS)
			{
				Refused(op, string.Format("box needs %1 columns > %2", keys.Count(), MAX_OP_COLUMNS));
				return false;
			}
			array<int> fresh = {};
			foreach (int key : keys)
			{
				if (!m_mCols.Contains(key))
					fresh.Insert(key);
			}
			if (m_mCols.Count() + fresh.Count() > MAX_COLUMNS)
			{
				Refused(op, string.Format("column cap %1 reached (%2 + %3 new)", MAX_COLUMNS, m_mCols.Count(), fresh.Count()));
				return false;
			}
			if (!MakeSeed(fresh, seed))
			{
				Refused(op, "no ground height under a new column");
				return false;
			}
			if (seed.Length() > SEED_CHARS)
			{
				Refused(op, string.Format("seed %1 chars > %2 for %3 new columns", seed.Length(), SEED_CHARS, fresh.Count()));
				return false;
			}
		}
		// synced mud sheet Stage 2: a RUT FIELD rut op keeps its sheet tag (DoRutOp) in the seed field
		if (!RUT_FIELD || op.m_iKind != 4)
			op.m_sSeed = seed;
		string text = Encode(op);
		MUD_VoxelOp back = Decode(text);
		if (!back || !Prepare(back) || back.m_sSeed != op.m_sSeed)
		{
			Print("MUD voxel probe: op text did not decode, dropped: " + text, LogLevel.ERROR);
			return false;
		}
		m_iSeq++;
		if (MUD_TestKit.TEST_KIT) MUD_CraterTrace.Issued(back.m_iSeq, back.m_iKind, back.m_vWp);     // stage 2 B1 crater log
		Submit(back);
		if (RplSession.Mode() != RplMode.None)
			MUD_WorldGroundEntity.VoxelBroadcast(text);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Refused(MUD_VoxelOp op, string why)
	{
		m_iRefused++;
		Print(string.Format("MUD voxel probe: op kind %1 at %2 not taken as voxels (%3) - the heightfield digs it | refused so far %4",
			op.m_iKind, (op.m_vLo + op.m_vHi) * 0.5, why, m_iRefused), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the seed of the fresh columns (in order, 8 x 8 sample columns each, lk then li): (dug - undug) in cm,
	//! each as the change from the one before - one ALPHA char for -30..31, '!' n '.' past that, '~' n '.' for a run
	//! of n unchanged ones. Trailing unchanged ones are left out (fresh ground: an empty seed). False = a ground
	//! height is unknown.
	protected bool MakeSeed(array<int> fresh, out string seed)
	{
		seed = "";
		int prev = 0;
		int same = 0;
		foreach (int key : fresh)
		{
			int bi = KeyBi(key);
			int bk = KeyBk(key);
			for (int lk = 0; lk < SUB; lk++)
			{
				for (int li = 0; li < SUB; li++)
				{
					float x = m_fX0 + (bi * SUB + li) * CELL;
					float z = m_fZ0 + (bk * SUB + lk) * CELL;
					float dug;
					float undug;
					if (!MUD_WorldGroundEntity.DugUndug(x, z, dug, undug))
						return false;
					int v = Math.Round((dug - undug) * 100);
					int d = v - prev;
					prev = v;
					if (d == 0)
					{
						same++;
						continue;
					}
					seed += SeedRun(same);
					same = 0;
					if (d >= -ALPHA_MID && d < ALPHA.Length() - ALPHA_MID)
						seed += ALPHA.Substring(d + ALPHA_MID, 1);
					else
						seed += "!" + d.ToString() + ".";
				}
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static string SeedRun(int n)
	{
		if (n <= 0)
			return "";
		if (n >= 4)
			return "~" + n.ToString() + ".";
		string zero = ALPHA.Substring(ALPHA_MID, 1);
		string s = "";
		for (int i = 0; i < n; i++)
			s += zero;
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! MakeSeed back into count values (cm). False = malformed or too long.
	protected static bool DecodeSeed(string s, int count, array<int> vals)
	{
		vals.Clear();
		int v = 0;
		int len = s.Length();
		int i = 0;
		while (i < len)
		{
			string ch = s.Substring(i, 1);
			if (ch == "~" || ch == "!")
			{
				int dot = -1;
				for (int j = i + 1; j < len; j++)
				{
					if (s.Substring(j, 1) == ".")
					{
						dot = j;
						break;
					}
				}
				if (dot < 0)
					return false;
				string num = s.Substring(i + 1, dot - i - 1);
				if (!MUD_TrenchPlan.IsInt(num))
					return false;
				int k = num.ToInt();
				if (ch == "~")
				{
					if (k < 0 || vals.Count() + k > count)
						return false;
					for (int r = 0; r < k; r++)
						vals.Insert(v);
				}
				else
				{
					v += k;
					vals.Insert(v);
				}
				i = dot + 1;
				continue;
			}
			int a = ALPHA.IndexOf(ch);
			if (a < 0)
				return false;
			v += a - ALPHA_MID;
			vals.Insert(v);
			i++;
		}
		if (vals.Count() > count)
			return false;
		while (vals.Count() < count)
			vals.Insert(v);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! "seq|kind|x|y|z|a|b|piece|plan|seed": x, y, z in cm, a, b in mm, plan the MUD_TrenchPlan text or "-",
	//! seed MakeSeed's or "-"
	protected static string Encode(MUD_VoxelOp op)
	{
		int x = Math.Round(op.m_vWp[0] * 100);
		int y = Math.Round(op.m_vWp[1] * 100);
		int z = Math.Round(op.m_vWp[2] * 100);
		int a = Math.Round(op.m_fDepth * 1000);
		int b = Math.Round(op.m_fB * 1000);
		string plan = op.m_sPlan;
		if (plan.IsEmpty())
			plan = "-";
		string seed = op.m_sSeed;
		if (seed.IsEmpty())
			seed = "-";
		return string.Format("%1|%2|%3|%4|%5|%6|%7|%8|", op.m_iSeq, op.m_iKind, x, y, z, a, b, op.m_iPiece) + plan + "|" + seed;
	}

	//------------------------------------------------------------------------------------------------
	//! null on anything malformed. The layout (2 / 3) and the box come later (Prepare). 9 fields = no seed.
	protected static MUD_VoxelOp Decode(string text)
	{
		array<string> f = {};
		text.Split("|", f, false);
		if (f.Count() != 9 && f.Count() != 10)
			return null;
		for (int n = 0; n < 8; n++)
		{
			if (!MUD_TrenchPlan.IsInt(f[n]))
				return null;
		}
		MUD_VoxelOp op = new MUD_VoxelOp();
		op.m_iSeq = f[0].ToInt();
		op.m_iKind = f[1].ToInt();
		op.m_vWp = Vector(f[2].ToInt() * 0.01, f[3].ToInt() * 0.01, f[4].ToInt() * 0.01);
		op.m_fDepth = f[5].ToInt() * 0.001;
		op.m_fB = f[6].ToInt() * 0.001;
		op.m_iPiece = f[7].ToInt();
		if (f[8] != "-")
			op.m_sPlan = f[8];
		if (f.Count() == 10 && f[9] != "-")
			op.m_sSeed = f[9];
		op.m_sText = text;
		if (op.m_iSeq < 1 || op.m_iKind < 0 || op.m_iKind > 6)
			return null;
		if (op.m_iKind >= 2 && op.m_sPlan.IsEmpty())
			return null;
		return op;
	}

	//------------------------------------------------------------------------------------------------
	//! Op layout (2 / 3, cached per plan text) and x, z box. False = the plan does not build (op dropped).
	protected bool Prepare(MUD_VoxelOp op)
	{
		if (op.m_iKind >= 4)
			return PrepareRut(op);
		if (op.m_iKind <= 1)
		{
			float reach = Reach(op.m_fDepth, op.m_fB);
			if (op.m_iKind == 1)
				reach = op.m_fDepth + CELL;
			op.m_vLo = op.m_vWp - Vector(reach, 0, reach);
			op.m_vHi = op.m_vWp + Vector(reach, 0, reach);
			return true;
		}
		MUD_TrenchLayout lay = m_mLays.Get(op.m_sPlan);
		if (!lay)
		{
			MUD_TrenchPlan plan = MUD_TrenchPlan.Unpack(op.m_sPlan);
			lay = new MUD_TrenchLayout();
			if (!plan || !lay.Build(plan))
				return false;
			m_mLays.Insert(op.m_sPlan, lay);
		}
		if (op.m_iPiece >= lay.Count())
			return false;
		op.m_Lay = lay;
		float margin = op.m_fDepth * MUD_TrenchPlan.LAYER * MUD_TrenchPlan.SLOPE_RUN + TRENCH_PAD + CELL;
		if (op.m_iKind == 3)
			margin = MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER * MUD_TrenchPlan.SLOPE_RUN + TRENCH_PAD + CELL;
		else if (PARAPET && op.m_iPiece >= 0)
			margin = Math.Max(margin, ParapetStart(lay) + PARA_RISE * 2 + PARA_TOP + CELL);
		float x0 = 100000000;
		float z0 = 100000000;
		float x1 = -100000000;
		float z1 = -100000000;
		for (int p = 0; p < lay.Count(); p++)
		{
			if (op.m_iPiece >= 0 && p != op.m_iPiece)
				continue;
			vector lo;
			vector hi;
			lay.PieceBox(p, lo, hi);
			x0 = Math.Min(x0, lo[0] - margin);
			z0 = Math.Min(z0, lo[2] - margin);
			x1 = Math.Max(x1, hi[0] + margin);
			z1 = Math.Max(z1, hi[2] + margin);
		}
		op.m_vLo = Vector(x0, 0, z0);
		op.m_vHi = Vector(x1, 0, z1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Op 4: segments from m_sPlan (6 cm ints each) and their box (+ RUT_R + CELL). False = malformed.
	protected bool PrepareRut(MUD_VoxelOp op)
	{
		op.m_aSegA.Clear();
		op.m_aSegB.Clear();
		array<string> segs = {};
		op.m_sPlan.Split(";", segs, true);
		if (segs.IsEmpty() || segs.Count() > RUT_MAX_SEGS)
			return false;
		float x0 = 100000000;
		float z0 = 100000000;
		float x1 = -100000000;
		float z1 = -100000000;
		array<string> f = {};
		foreach (string seg : segs)
		{
			f.Clear();
			seg.Split(",", f, false);
			if (f.Count() != 6)
				return false;
			for (int n = 0; n < 6; n++)
			{
				if (!MUD_TrenchPlan.IsInt(f[n]))
					return false;
			}
			vector a = Vector(f[0].ToInt() * 0.01, f[1].ToInt() * 0.01, f[2].ToInt() * 0.01);
			vector b = Vector(f[3].ToInt() * 0.01, f[4].ToInt() * 0.01, f[5].ToInt() * 0.01);
			op.m_aSegA.Insert(a);
			op.m_aSegB.Insert(b);
			x0 = Math.Min(x0, Math.Min(a[0], b[0]));
			z0 = Math.Min(z0, Math.Min(a[2], b[2]));
			x1 = Math.Max(x1, Math.Max(a[0], b[0]));
			z1 = Math.Max(z1, Math.Max(a[2], b[2]));
		}
		float m = RUT_R + CELL;
		if (op.m_iKind == 4 && DISPLACE_RUTS && RUT_REPOSE_ITERS > 0)
			m += 2 * FCELL;     // Build 3 Stage 2: the repose relax writes up to 2 floor samples past the cut
		// Build 3 Stage 3: ridges reach RUT_R + GAP + W sideways (u < 1 keeps them off the track past a segment end)
		if (op.m_iKind == 4 && DISPLACE_RUTS && RUT_FIELD && RUT_RIDGES)
			m = Math.Max(m, RUT_R + RUT_RIDGE_GAP + RUT_RIDGE_W + CELL);
		if (op.m_iKind == 5)
			m = op.m_fDepth + CELL;
		op.m_vLo = Vector(x0 - m, 0, z0 - m);
		op.m_vHi = Vector(x1 + m, 0, z1 + m);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyOp(MUD_VoxelOp op)
	{
		// only an op the sheet ALSO took (MudSheet SYNC): a shell inside a tunnel goes to the voxels alone and must
		// carve (draw) there - skipping it left sheet holes with nothing under them (user shot 16:52, sky through)
		m_bSheetOp = m_bOpBoth && (op.m_iKind == 0 || op.m_iKind == 2 || op.m_iKind == 3);
		ApplyOpKind(op);
		m_bSheetOp = false;
	}

	protected void ApplyOpKind(MUD_VoxelOp op)
	{
		if (op.m_iKind == 4)
		{
			if (DISPLACE_RUTS)
				CarveFloor(op);
			else
				CarveRut(op, false);
		}
		else if (op.m_iKind == 6)
			CarveRut(op, true);
		else if (op.m_iKind == 5)
			AheadCells(op);
		else if (op.m_iKind == 1)
		{
			CarveTunnel(op);
			if (TUNNEL_DARK && !m_bHeadless)
				TunnelDark(op.m_vWp);
		}
		else if (op.m_iKind == 2)
			CutTrench(op);
		else if (op.m_iKind == 3)
			FillTrench(op);
		else
			Carve(op);
	}

	//------------------------------------------------------------------------------------------------
	//! SHEET-ONLY TEST: every segment end of op under the mud sheet (MUD_MudZoneEntity.SheetCovers)
	protected static bool OpUnderSheet(MUD_VoxelOp op)
	{
		if (op.m_aSegA.IsEmpty())
			return false;
		for (int i = 0; i < op.m_aSegA.Count(); i++)
		{
			vector a = op.m_aSegA[i];
			vector c = op.m_aSegB[i];
			if (!MUD_MudZoneEntity.SheetCovers(a[0], a[2]) || !MUD_MudZoneEntity.SheetCovers(c[0], c[2]))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A new batch (bricks queued from now go live together at the next GoLive), first op seq firstSeq
	protected void OpenBatch(int t0, int firstSeq)
	{
		m_bOpen = true;
		m_iBatchT0 = t0;
		m_iBatchOps = 0;
		m_iBatchMade = 0;
		m_iBatchBuilt = 0;
		m_iBatchLook = 0;
		m_iBatchMeshOnly = 0;
		m_iBatchSkipped = 0;
		m_iBatchMs = 0;
		m_iBatchFrames = 0;
		m_iBatchWorst = 0;
		m_iBatchWorstSub = 0;
		m_iBatchVerts = 0;
		m_iBatchGatherMs = 0;
		m_iBatchPolyMs = 0;
		m_iBatchScanned = 0;
		m_iBatchFirstSeq = firstSeq;
		m_iBatchSplit = 0;
		m_iBatchSplitV = 0;
		m_iBatchFold = 0;
		m_iBatchDispOvf = 0;
		m_iBatchSoil = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine, ops in seq order: converts the op's fresh columns from its seed, makes the brick rows the new
	//! surface needs (and its old neighbours' rows that now meet it), applies the op, queues the changed bricks and
	//! logs the op. Always true (the server decided in Issue).
	//! MUD_Prof (TEMP 2026-10-18, H1 hitch hunt; results/2026-10-18-h1-snap-probe.md): Submit timed in parts; an op
	//! over OP_SLOW_MS (test kit: every rut op) prints its split ('MUD op time')
	static const int OP_SLOW_MS = 25;     // perf plan A1 2026-10-18: was 8 = below the mean rut op (12.8 ms DS 22:31) = a line per op
	protected ref array<int> m_aOpMs = {0, 0, 0, 0, 0, 0, 0, 0};     // this op: flush, pre, carve, smooth, dirty, reground, sync (index = part - OP_FLUSH)
	protected int m_iOpT;
	protected void OpPart(int part)
	{
		int ms = System.GetTickCount(m_iOpT);
		m_aOpMs.Set(part - MUD_Prof.OP_FLUSH, m_aOpMs[part - MUD_Prof.OP_FLUSH] + ms);
		MUD_Prof.Add(part, ms);
		m_iOpT = System.GetTickCount();
	}

	protected bool Submit(MUD_VoxelOp op)
	{
		for (int pi = 0; pi < m_aOpMs.Count(); pi++)
			m_aOpMs.Set(pi, 0);
		m_bOpCrude = false;
		m_iCSeg = 0;
		m_iCWrite = 0;
		m_iCRelax = 0;
		m_iCRidge = 0;
		m_iCLog = 0;
		int tAll = System.GetTickCount();
		m_iOpT = tAll;
		bool ok = SubmitBody(op);
		int all = System.GetTickCount(tAll);
		int sub = 0;
		foreach (int pm : m_aOpMs)
			sub += pm;
		int post = Math.Max(0, all - sub);
		MUD_Prof.Add(MUD_Prof.OP_ALL, all);
		MUD_Prof.Add(MUD_Prof.OP_POST, post);
		if (all >= OP_SLOW_MS || (op.m_iKind == 4 && MUD_TestKit.TEST_KIT && OP_PRINTS))     // test kit: every rut op (crude A/B)
		{
			Print(string.Format("MUD op time: seq %1 kind %2, %3 ms | flush %4 pre %5 carve %6 smooth %7 dirty %8 reground %9",
				op.m_iSeq, op.m_iKind, all, m_aOpMs[0], m_aOpMs[1], m_aOpMs[2], m_aOpMs[3], m_aOpMs[4], m_aOpMs[5])
				+ string.Format(" sync %1 post %2 | patches dirty %3, floor box %4 x %5 samples", m_aOpMs[6], post, m_iFieldDirty,
				m_iFi1 - m_iFi0 + 1, m_iFk1 - m_iFk0 + 1)
				+ string.Format(" | carve seg %1 write %2 relax %3 ridge %4 log %6, crude %5", m_iCSeg, m_iCWrite, m_iCRelax, m_iCRidge, m_bOpCrude, m_iCLog), LogLevel.NORMAL);
		}
		return ok;
	}

	protected bool SubmitBody(MUD_VoxelOp op)
	{
		if (MUD_TestKit.TEST_KIT) MUD_CraterTrace.Submitted(op.m_iSeq, op.m_iKind);     // stage 2 B1 crater log
		// synced mud sheet op split (2026-10-18): the sync layer's open op work ends before this op changes the
		// ground (its ground reads must not see a later op; no-op on a DS / with nothing open)
		MUD_MudSync.Flush();
		OpPart(MUD_Prof.OP_FLUSH);
		int t0 = System.GetTickCount();
		if (!Prepare(op))
		{
			Print("MUD voxel probe: op plan does not build, dropped: " + op.m_sText, LogLevel.ERROR);
			return false;
		}
		// ONE_GRID: a crater / trench / fill / rut in a mud zone changes the sheet's ground, no voxels - except a crater /
		// trench / fill that meets voxel ground (a tunnel): the voxels take it too, so their ground (drawn in whole
		// carved 2 m cells) sinks with the sheet's instead of standing as a flat plane over it (user 2026-10-15)
		// ... or a crater reaching past the zone edge (its outside part is voxel / heightfield ground). Such ops reach
		// the sheet at their go-live (MudSheet SYNC); in zone cells the voxels do not draw them (m_bSheetOp)
		bool edge = op.m_iKind == 0 && MUD_MudSheet.BoxLeavesZone(op.m_vLo, op.m_vHi);
		bool both = op.m_iKind != 4 && (edge || HasCols(op.m_vLo, op.m_vHi));
		bool taken = MUD_MudSheet.TakeOp(op, both);
		// the sheet holds it (tunnel / zone edge, also a crater outside the zone reaching it: TakeOp false) -> the
		// voxels skip the sheet's cells; a shell inside a tunnel (not held) carves them as usual
		m_bOpBoth = MUD_MudSheet.Holds(op);
		if (taken)
		{
			if (!both)
				return true;
			Print(string.Format("MUD voxel probe: op kind %1 at %2 meets voxel ground -> sheet and voxels", op.m_iKind, op.m_vWp), LogLevel.NORMAL);
		}
		if (!m_bOpen)
			OpenBatch(t0, op.m_iSeq);
		m_iBatchOps++;
		// J2: the rows a rut op makes go with it (snapshot placeholders); m_sRowLog is read back before m_aOps.Insert
		m_bRowLog = op.m_iKind == 4;
		m_sRowLog = "";
		if (op.m_iKind <= 3)
		{
			float mudR = 0;
			if (op.m_iKind == 0)
				mudR = CraterCrest(op.m_fDepth);     // = Carve's crest
			m_aMudLo.Insert(op.m_vLo);
			m_aMudHi.Insert(op.m_vHi);
			m_aMudWp.Insert(op.m_vWp);
			m_aMudR.Insert(mudR);
			m_aMudKind.Insert(op.m_iKind);
			float lift = 0;
			if (op.m_iKind == 3)
				lift = Math.Max(0, op.m_fDepth - op.m_fB);
			m_aMudLift.Insert(lift);
		}
		int made0 = m_iBatchMade;
		int fieldCols0 = m_mCols.Count();
		array<int> keys = {};
		if (!RUT_FIELD || op.m_iKind != 4)
			ConvCols(op.m_vLo, op.m_vHi, keys);     // RUT FIELD: a rut op converts nothing (Issue sent no seed)
		array<int> fresh = {};
		foreach (int key : keys)
		{
			if (!m_mCols.Contains(key))
				fresh.Insert(key);
		}
		int queued;
		if (!fresh.IsEmpty())
		{
			array<int> vals = {};
			if (!DecodeSeed(op.m_sSeed, fresh.Count() * SUB * SUB, vals))
			{
				Print(string.Format("MUD voxel probe: seq %1 - seed does not fit its %2 new columns (op log differs from the server's?) - seeded as undug: %3",
					op.m_iSeq, fresh.Count(), op.m_sSeed), LogLevel.ERROR);
				vals.Clear();
				for (int z = 0; z < fresh.Count() * SUB * SUB; z++)
					vals.Insert(0);
			}
			for (int n = 0; n < fresh.Count(); n++)
				Convert(fresh[n], vals, n * SUB * SUB);
			// rows: the new columns and their old neighbours (a step between them needs both sides' rows), then every
			// row of an old neighbour is rebuilt (its edge cells reach into the new column)
			array<int> near = {};
			set<int> nearSeen = new set<int>();
			foreach (int fk : fresh)
			{
				int fbi = KeyBi(fk);
				int fbk = KeyBk(fk);
				for (int dk = -1; dk <= 1; dk++)
				{
					for (int di = -1; di <= 1; di++)
					{
						int nk = ColKey(fbi + di, fbk + dk);
						if (m_mCols.Contains(nk) && !nearSeen.Contains(nk))
						{
							nearSeen.Insert(nk);
							near.Insert(nk);
						}
					}
				}
			}
			foreach (int ek : near)
			{
				MUD_VoxelColumn ec = m_mCols.Get(ek);
				EnsureRows(ec);
				queued += QueueCol(ec);
			}
		}
		// synced mud sheet Stage 4 (2026-10-18): a dig near a mud zone keeps each column top it changes, as it was
		// before the op's first Put there (MUD_MudSync dig clearing; false on a DS)
		m_mTopWas.Clear();
		m_bTopTrack = MUD_MudSync.ClearBefore(op);
		m_bChanged = false;
		m_iMarkGi = -1000000;
		m_iMarkGk = -1000000;
		m_aOpCarved.Clear();
		m_aOpLook.Clear();
		m_iOpKind = op.m_iKind;
		m_aClrI.Clear();
		m_aClrK.Clear();
		m_iDispHash = 0;
		m_iFieldHash = 0;
		m_iFieldHashN = 0;
		m_iDispOpW = 0;
		m_iDispOpC = 0;
		m_iFieldDirty = 0;
		m_bFChg = false;
		m_aFChgX.Clear();
		m_aFChgZ.Clear();
		OpPart(MUD_Prof.OP_PRE);
		ApplyOp(op);
		OpPart(MUD_Prof.OP_CARVE);
		int fieldW = m_iDispOpW;
		m_iSmoothN = 0;
		m_iSmoothMs = 0;
		if (m_bChanged && SMOOTH_OPS && op.m_iKind >= 0 && op.m_iKind <= 4 && (SMOOTH_KINDS & (1 << op.m_iKind)) != 0)
			SmoothBox();
		m_bTopTrack = false;     // smoothing moves tops too: tracked through it
		ClearFloor();
		OpPart(MUD_Prof.OP_SMOOTH);     // SmoothBox + ClearFloor
		// RUT FIELD Stage 3: the heightfield patches holding changed floor samples (+1 vertex) redraw - mesh + collider,
		// never m_aH, never a net send (the op stream carries the floor)
		if (RUT_FIELD && m_bFChg)
		{
			// perf plan A3 (2026-10-18): no +1 margin - PatchLo / PatchHi already give an edge sample to both patches and the
			// band mesher reads only its own patch samples (audit-ground.md item 7). Check: 'MUD world seams', patch borders
			m_iFieldDirty = MUD_WorldGroundEntity.FloorDirty(m_iFi0, m_iFi1, m_iFk0, m_iFk1, op.m_iKind == 4 && LookCoversOp());
			OpPart(MUD_Prof.OP_DIRTY);
			// synced mud sheet Stage 1 (2026-10-18): the mud's ground follows the new floor (look only; no-op on a DS).
			// R2 perf 2026-10-18: the box of the floor samples this op really changed (+1 sample), not the op box (segments
			// + 1.5 m ridge reach) - in WB an in-zone rut op cost 10.3 ms vs 5.7 outside (research/2026-10-18-mud-zone-rut-perf.md)
			vector fLo = Vector(m_fX0 + (m_iFi0 - 1) * FCELL, 0, m_fZ0 + (m_iFk0 - 1) * FCELL);
			vector fHi = Vector(m_fX0 + (m_iFi1 + 1) * FCELL, 0, m_fZ0 + (m_iFk1 + 1) * FCELL);
			MUD_MudSheet.FloorChanged(fLo, fHi, m_aFChgX, m_aFChgZ);
			OpPart(MUD_Prof.OP_REGROUND);
		}
		// synced mud sheet Stage 2 (2026-10-18): every rut op presses the sync mud layer, in seq order (no-op on a DS)
		if (op.m_iKind == 4)
			MUD_MudSync.RutOp(op);
		// Stage 4: a crater / tunnel / trench / fill clears the sync mud, in seq order (no-op on a DS)
		else if (op.m_iKind <= 3)
			MUD_MudSync.ClearOp(op);
		OpPart(MUD_Prof.OP_SYNC);
		m_iOpKind = -1;
		if (DISPLACE_RUTS)
			DispOpDone(op);
		// test kit: every machine logs the same field hash per seq when their FEEL fields agree (Tools/compare_op_hashes.py).
		// RUT_FIELD rut ops change no samples (the disp hash covers them): no line
		if (MUD_TestKit.TEST_KIT && HASH_LOG && m_iFieldHashN > 0)
			Print(string.Format("MUD field hash seq %1 kind %2: %3 (%4 samples)", op.m_iSeq, op.m_iKind, m_iFieldHash, m_iFieldHashN), LogLevel.NORMAL);
		if (m_bChanged)
		{
			m_bQueueLook = op.m_iKind == 6;
			queued += QueueBox(m_iCi0, m_iCi1, m_iCj0, m_iCj1, m_iCk0, m_iCk1);
			m_bQueueLook = false;
		}
		// a newly carved cell draws quads it did not before, in every row and in the bricks next to it (quads reach
		// 0.5 m past a brick)
		int carvedNew = m_aOpCarved.Count();
		if (carvedNew > 0)
		{
			set<int> qcols = new set<int>();
			foreach (int ck : m_aOpCarved)
			{
				int ccx = ck % CELL_KEY_W - KEY_OFF;
				int ccz = ck / CELL_KEY_W - KEY_OFF;
				for (int qz = ccz - 1; qz <= ccz + 1; qz++)
				{
					for (int qx = ccx - 1; qx <= ccx + 1; qx++)
					{
						int qbi = Math.Floor(qx / 2.0);
						int qbk = Math.Floor(qz / 2.0);
						int qk = ColKey(qbi, qbk);
						if (qcols.Contains(qk))
							continue;
						qcols.Insert(qk);
						MUD_VoxelColumn qc = m_mCols.Get(qk);
						if (qc)
							queued += QueueCol(qc);
					}
				}
			}
			m_aOpCarved.Clear();
		}
		// RUT_LOOK: a new look cell draws the look mesh in every row and the bricks next to it - mesh only (look-only
		// queue: a shown brick keeps its body)
		if (!m_aOpLook.IsEmpty())
		{
			set<int> lcols = new set<int>();
			m_bQueueLook = op.m_iKind == 6;
			foreach (int lck : m_aOpLook)
			{
				int lcx = lck % CELL_KEY_W - KEY_OFF;
				int lcz = lck / CELL_KEY_W - KEY_OFF;
				for (int lz = lcz - 1; lz <= lcz + 1; lz++)
				{
					for (int lx = lcx - 1; lx <= lcx + 1; lx++)
					{
						int lbi = Math.Floor(lx / 2.0);
						int lbk = Math.Floor(lz / 2.0);
						int lkey = ColKey(lbi, lbk);
						if (lcols.Contains(lkey))
							continue;
						lcols.Insert(lkey);
						MUD_VoxelColumn lcol = m_mCols.Get(lkey);
						if (lcol)
							queued += QueueCol(lcol);
					}
				}
			}
			m_bQueueLook = false;
			m_aOpLook.Clear();
		}
		if (op.m_iKind == 4)
			op.m_sRows = m_sRowLog;
		m_bRowLog = false;
		m_sRowLog = "";
		m_aOps.Insert(op);

		string label = "blast";
		string what = string.Format("depth %1 crest %2 m, stretch %3", op.m_fDepth, CraterCrest(op.m_fDepth), op.m_fB);
		vector wp = op.m_vWp;
		if (op.m_iKind == 1)
		{
			label = "tunnel dig";
			what = string.Format("capsule r %1 half %2 m", op.m_fDepth, TUNNEL_HALF);
		}
		else if (op.m_iKind == 2)
		{
			label = "trench cut";
			what = string.Format("piece %1 (-1 = free dig square) to %2 layers", op.m_iPiece, op.m_fDepth);
			wp = (op.m_vLo + op.m_vHi) * 0.5;
		}
		else if (op.m_iKind == 5)
		{
			label = "convert ahead";
			what = string.Format("%1 strips, half width %2 m", op.m_aSegA.Count(), op.m_fDepth);
		}
		else if (op.m_iKind == 4)
		{
			label = "rut";
			what = string.Format("%1 segments, cut %2 m, deepest %3 m", op.m_aSegA.Count(), op.m_fDepth, op.m_fB);
		}
		else if (op.m_iKind == 6)
		{
			label = "rut look";
			what = string.Format("%1 segments, cut %2 m, deepest %3 m", op.m_aSegA.Count(), op.m_fDepth, op.m_fB);
		}
		else if (op.m_iKind == 3)
		{
			label = "fill";
			what = string.Format("hole %1 m -> %2 m", op.m_fDepth, op.m_fB);
			wp = (op.m_vLo + op.m_vHi) * 0.5;
		}
		if (!OP_LOG)
			return true;
		Print(string.Format("MUD voxel probe: %1 seq %2 at %3: %4 | %5 columns (%6 new, seed %7 chars), %8 new cells carved, %9 bricks made",
			label, op.m_iSeq, wp, what, keys.Count(), fresh.Count(), op.m_sSeed.Length(), carvedNew, m_iBatchMade - made0)
			+ string.Format(", %1 queued", queued)
			+ string.Format(", %1 ms | %2 columns, %3 bricks in all", System.GetTickCount(t0), m_mCols.Count(), m_iBricks)
			+ string.Format(" | smooth %1 samples %2 ms", m_iSmoothN, m_iSmoothMs), LogLevel.NORMAL);
		if (RUT_FIELD && op.m_iKind == 4)
			Print(string.Format("MUD rut field: %1 samples, %2 patches dirty, %3 new columns | %4 floor tiles, %5 columns in all",
				fieldW, m_iFieldDirty, m_mCols.Count() - fieldCols0, m_mRutTiles.Count(), m_mCols.Count()), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Step C: blurs the samples of the op's changed box + 1 (SMOOTH_PASSES times). All new values are worked out
	//! from the old ones first, then written with Put (which marks cells carved and grows the box for QueueBox).
	protected void SmoothBox()
	{
		int t0 = System.GetTickCount();
		int i0 = m_iCi0 - 1;
		int i1 = m_iCi1 + 1;
		int j0 = m_iCj0 - 1;
		int j1 = m_iCj1 + 1;
		int k0 = m_iCk0 - 1;
		int k1 = m_iCk1 + 1;
		float nearV = CELL * 1.2;
		array<int> ai = {};
		array<int> aj = {};
		array<int> ak = {};
		array<float> aOld = {};
		array<float> aNew = {};
		for (int pass = 0; pass < SMOOTH_PASSES; pass++)
		{
			ai.Clear();
			aj.Clear();
			ak.Clear();
			aOld.Clear();
			aNew.Clear();
			for (int gk = k0; gk <= k1; gk++)
			{
				for (int gi = i0; gi <= i1; gi++)
				{
					MUD_VoxelColumn col = ColAt(gi, gk);
					if (!col)
						continue;
					int li = gi - col.m_iBi * SUB;
					int lk = gk - col.m_iBk * SUB;
					if (col.m_aHard[lk * SUB + li] != 0)
						continue;
					MUD_VoxelColumn xm = ColAt(gi - 1, gk);
					MUD_VoxelColumn xp = ColAt(gi + 1, gk);
					MUD_VoxelColumn zm = ColAt(gi, gk - 1);
					MUD_VoxelColumn zp = ColAt(gi, gk + 1);
					if (!xm || !xp || !zm || !zp)
						continue;
					for (int gj = j0; gj <= j1; gj++)
					{
						float v = Value(col, li, lk, gj);
						if (v >= nearV || v <= -nearV)
							continue;
						float sum = Value(col, li, lk, gj - 1) + Value(col, li, lk, gj + 1);
						sum += Value(xm, gi - 1 - xm.m_iBi * SUB, gk - xm.m_iBk * SUB, gj);
						sum += Value(xp, gi + 1 - xp.m_iBi * SUB, gk - xp.m_iBk * SUB, gj);
						sum += Value(zm, gi - zm.m_iBi * SUB, gk - 1 - zm.m_iBk * SUB, gj);
						sum += Value(zp, gi - zp.m_iBi * SUB, gk + 1 - zp.m_iBk * SUB, gj);
						float nv = (1 - SMOOTH_W) * v + SMOOTH_W * sum / 6;
						if (Math.AbsFloat(nv - v) < 0.001)
							continue;
						ai.Insert(gi);
						aj.Insert(gj);
						ak.Insert(gk);
						aOld.Insert(v);
						aNew.Insert(nv);
					}
				}
			}
			for (int n = 0; n < ai.Count(); n++)
			{
				int wi = ai[n];
				int wk = ak[n];
				MUD_VoxelColumn wc = ColAt(wi, wk);
				Put(wc, wi - wc.m_iBi * SUB, wk - wc.m_iBk * SUB, wi, aj[n], wk, aOld[n], Nudge(aNew[n]));
			}
			m_iSmoothN += ai.Count();
		}
		m_iSmoothMs = System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! Column key converted from seed values vals[base .. base + 63] (cm over the undug ground). No rows yet.
	protected MUD_VoxelColumn Convert(int key, array<int> vals, int base)
	{
		MUD_VoxelColumn col = new MUD_VoxelColumn();
		col.m_iBi = KeyBi(key);
		col.m_iBk = KeyBk(key);
		for (int lk = 0; lk < SUB; lk++)
		{
			for (int li = 0; li < SUB; li++)
			{
				int n = lk * SUB + li;
				float x = m_fX0 + (col.m_iBi * SUB + li) * CELL;
				float z = m_fZ0 + (col.m_iBk * SUB + lk) * CELL;
				float undug;
				if (MUD_WorldGroundEntity.UndugY(x, z, undug))
					m_fLastG = undug;
				else
					undug = m_fLastG;
				int kept = 0;
				int hard = 0;
				if (Kept(x, z))
					kept = 1;
				if (kept == 1 || Hard(x, z))
					hard = 1;
				col.m_aUndug[n] = undug;
				col.m_aKept[n] = kept;
				col.m_aHard[n] = hard;
				if (kept == 1)
					col.m_aTop[n] = undug - KEPT_SINK;
				else
					col.m_aTop[n] = undug + vals[base + n] * 0.01 + LIFT;
			}
		}
		m_mCols.Insert(key, col);
		return col;
	}

	//------------------------------------------------------------------------------------------------
	//! The rows of col covering the seeded tops of col and its converted neighbours (ROW_PAD over and under)
	protected void EnsureRows(MUD_VoxelColumn col)
	{
		float lo = 100000;
		float hi = -100000;
		for (int dk = -1; dk <= 1; dk++)
		{
			for (int di = -1; di <= 1; di++)
			{
				MUD_VoxelColumn c = m_mCols.Get(ColKey(col.m_iBi + di, col.m_iBk + dk));
				if (!c)
					continue;
				for (int n = 0; n < SUB * SUB; n++)
				{
					lo = Math.Min(lo, c.m_aTop[n]);
					hi = Math.Max(hi, c.m_aTop[n]);
				}
			}
		}
		int r0 = Math.Floor((lo - ROW_PAD) / BRICK);
		int r1 = Math.Floor((hi + ROW_PAD) / BRICK);
		for (int bj = r0; bj <= r1; bj++)
			Row(col, bj, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Build 1 crater (2026-10-17). C1: the heightfield's cosine bowl as a vertical-distance field (Tools/
	//! crater_slope_voxel.py CraterCos: every wall <= 44 deg, ~25 % narrower than the old ellipsoid): the rim raises the
	//! ground by H out to the crest, falling over the rim width; inside the crest the bowl cuts to wp - depth at the
	//! middle (cannot undercut: no funnel needed). C6: the outline stretched by angle (op m_fB, CraterShape; only
	//! widens). C3: the change fades to nothing within CRATER_FADE of hard columns (roads, kept ground). C4: under a
	//! roof (RoofOver) a sphere r = depth instead. Never deeper than CRATER_FLOOR under the undug ground, never in hard columns.
	protected void Carve(MUD_VoxelOp op)
	{
		vector wp = op.m_vWp;
		float depth = op.m_fDepth;
		if (RoofOver(wp))
		{
			CarveSphere(op);
			return;
		}
		float rimH = depth * CRATER_RIM_SHARE;
		if (CRATER_PIT)
			rimH = depth * PIT_RIM_SHARE;
		float crest = CraterCrest(depth);
		float rimW = CraterRimW(depth);
		float outer = crest + rimW;
		float stretch = op.m_fB;
		float lobeW;
		float turn2;
		float turn3;
		CraterShape(op.m_iPiece, lobeW, turn2, turn3);
		float reach = outer * (1 + stretch);
		int gi0 = Math.Floor((wp[0] - reach - m_fX0) / CELL);
		int gi1 = Math.Ceil((wp[0] + reach - m_fX0) / CELL);
		int gk0 = Math.Floor((wp[2] - reach - m_fZ0) / CELL);
		int gk1 = Math.Ceil((wp[2] + reach - m_fZ0) / CELL);
		int gj0 = Math.Floor((wp[1] - depth - 1) / CELL);
		// up to everything a hit TUNNEL_FLOOR down (a shaft bottom, U1) can have above it (ground + rims): the bowl cut opens it all
		int gj1 = Math.Ceil((wp[1] + TUNNEL_FLOOR + rimH + 1.5) / CELL);
		int fadeN = Math.Ceil(CRATER_FADE / CELL);
		float inner = crest * PIT_INNER;
		float rad = Math.Min(crest, depth);
		float ph3 = lobeW * Math.PI2;
		for (int gk = gk0; gk <= gk1; gk++)
		{
			for (int gi = gi0; gi <= gi1; gi++)
			{
				MUD_VoxelColumn col = ColAt(gi, gk);
				if (!col)
					continue;
				int li = gi - col.m_iBi * SUB;
				int lk = gk - col.m_iBk * SUB;
				int n = lk * SUB + li;
				if (col.m_aHard[n] != 0)
					continue;   // real terrain over it (round 2: digging here opened a gap under the terrain's cut edge)
				float dx = m_fX0 + gi * CELL - wp[0];
				float dz = m_fZ0 + gk * CELL - wp[2];
				float rr = Math.Sqrt(dx * dx + dz * dz);
				if (rr >= reach)
					continue;
				if (stretch > 0)
				{
					float ang = Math.Atan2(dz, dx);
					rr /= 1 + stretch * (lobeW * (0.5 + 0.5 * Math.Cos(2 * (ang - turn2)))
						+ (1 - lobeW) * (0.5 + 0.5 * Math.Cos(3 * (ang - turn3))));
				}
				if (rr >= outer)
					continue;
				float bump = rimH;
				if (CRATER_PIT)
				{
					bump = 0;
					if (rr > inner)
					{
						bump = rimH * Math.Sin(Math.PI * (rr - inner) / (outer - inner));
						bump *= Math.Max(0, 1 + PIT_RIM_NOISE * CraterNoise(dx, dz, turn2, turn3, ph3));
					}
				}
				else if (rr > crest)
					bump = rimH * (1 + Math.Cos(Math.PI * (rr - crest) / rimW)) * 0.5;
				bool bowl = rr < crest;
				float bowlY = wp[1] + rimH - (depth + rimH) * (1 + Math.Cos(Math.PI * rr / crest)) * 0.5;
				float fade = CraterFade(gi, gk, fadeN);
				if (fade <= 0)
					continue;
				float floorY = Math.Max(col.m_aUndug[n] - CRATER_FLOOR, m_fSeaY);
				// U1: the column's top solid sample in range; air under it is a buried hole (tunnel) the rim must not fill
				// (MEASURED: 81 mm over a 5 m tunnel refilled 44 tunnel samples through the rim, 0 with this)
				int topJ = gj0 - 1;
				for (int tj = gj1; tj >= gj0; tj--)
				{
					if (Value(col, li, lk, tj) > 0)
					{
						topJ = tj;
						break;
					}
				}
				for (int gj = gj0; gj <= gj1; gj++)
				{
					float y = gj * CELL;
					float old = Value(col, li, lk, gj);
					float rimB = bump;
					if (gj < topJ && old < 0)
						rimB = 0;
					float d = old + rimB;
					if (CRATER_PIT)
					{
						// stable backup Carve: half ellipsoid under the hit, a funnel widening FUNNEL per m above it
						float up = y - wp[1];
						float s;
						if (up <= 0)
						{
							float er = rr / crest;
							float ey = up / depth;
							s = (Math.Sqrt(er * er + ey * ey) - 1) * rad;
							float fw = Math.Clamp(1 - er, 0, 1);
							s += PIT_FLOOR_NOISE * depth * fw * CraterNoise(dz, dx, turn3, ph3, turn2);
						}
						else
							s = (rr - crest - FUNNEL * up) * rad / crest;
						if (s < d)
							d = s;
					}
					else if (bowl && bowlY - y < d)
						d = bowlY - y;
					if (fade < 1)
						d = old + (d - old) * fade;
					d = Math.Max(d, Math.Min(old, floorY - y));     // U1: no NEW air below the floor (older air stays)
					Put(col, li, lk, gi, gj, gk, old, Nudge(d));
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Smooth noise about -1 .. 1 at offset dx, dz from the hit (bumps ~1-4 m), phases p1-p3 from the op: every machine
	//! gets the same numbers
	static float CraterNoise(float dx, float dz, float p1, float p2, float p3)
	{
		float a = Math.Sin(1.7 * dx + p1) * Math.Sin(1.9 * dz + p2);
		float b = Math.Sin(3.1 * (dx + dz) + p3);
		float c = Math.Sin(2.6 * (dx - dz) + p1 + p2);
		return (a + 0.5 * b + 0.5 * c) * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	//! C3: 0 on a hard sample, smoothstep up to 1 at CRATER_FADE from the nearest one (searched fadeN samples around).
	//! Columns not converted read Hard / Kept from the skin, as Convert does - the same on every machine.
	protected float CraterFade(int gi, int gk, int fadeN)
	{
		float best = CRATER_FADE;
		for (int k = gk - fadeN; k <= gk + fadeN; k++)
		{
			for (int i = gi - fadeN; i <= gi + fadeN; i++)
			{
				int di = i - gi;
				int dk = k - gk;
				int q = di * di + dk * dk;
				float dd = Math.Sqrt(q) * CELL;
				if (dd >= best)
					continue;
				if (HardSample(i, k))
					best = dd;
			}
		}
		float t = Math.Clamp(best / CRATER_FADE, 0, 1);
		return t * t * (3 - 2 * t);
	}

	//------------------------------------------------------------------------------------------------
	//! Sample gi, gk is hard (Convert's rule): from its column when converted, else from the skin
	protected bool HardSample(int gi, int gk)
	{
		MUD_VoxelColumn col = ColAt(gi, gk);
		if (col)
		{
			int li = gi - col.m_iBi * SUB;
			int lk = gk - col.m_iBk * SUB;
			return col.m_aHard[lk * SUB + li] != 0;
		}
		float x = m_fX0 + gi * CELL;
		float z = m_fZ0 + gk * CELL;
		return Kept(x, z) || Hard(x, z);
	}

	//------------------------------------------------------------------------------------------------
	//! C4: a blast under a roof carves a sphere r = depth around the hit (a chamber; TUNNEL_FLOOR + sea guard kept (U1), no rim, hard columns
	//! skipped). MEASURED offline (Tools/improve_checks.py blast): 0 columns opened to the sky for RPG .. 81 mm sizes.
	protected void CarveSphere(MUD_VoxelOp op)
	{
		vector wp = op.m_vWp;
		float r = op.m_fDepth;
		int gi0 = Math.Floor((wp[0] - r - m_fX0) / CELL);
		int gi1 = Math.Ceil((wp[0] + r - m_fX0) / CELL);
		int gk0 = Math.Floor((wp[2] - r - m_fZ0) / CELL);
		int gk1 = Math.Ceil((wp[2] + r - m_fZ0) / CELL);
		int gj0 = Math.Floor((wp[1] - r) / CELL);
		int gj1 = Math.Ceil((wp[1] + r) / CELL);
		for (int gk = gk0; gk <= gk1; gk++)
		{
			for (int gi = gi0; gi <= gi1; gi++)
			{
				MUD_VoxelColumn col = ColAt(gi, gk);
				if (!col)
					continue;
				int li = gi - col.m_iBi * SUB;
				int lk = gk - col.m_iBk * SUB;
				int n = lk * SUB + li;
				if (col.m_aHard[n] != 0)
					continue;
				float dx = m_fX0 + gi * CELL - wp[0];
				float dz = m_fZ0 + gk * CELL - wp[2];
				float floorY = DeepFloor(col.m_aUndug[n]);     // U1: only under a roof, so the tunnel floor (FLOOR 3 + new clamp = no chamber in a 5 m tunnel)
				for (int gj = gj0; gj <= gj1; gj++)
				{
					float y = gj * CELL;
					float dy = y - wp[1];
					float s = Math.Sqrt(dx * dx + dy * dy + dz * dz) - r;
					float old = Value(col, li, lk, gj);
					float d = old;
					if (s < d)
						d = s;
					d = Math.Max(d, Math.Min(old, floorY - y));     // U1: no NEW air below the floor (older air stays)
					Put(col, li, lk, gi, gj, gk, old, Nudge(d));
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Op 4, all-voxel test: ruts, cut from the GROUND's own height (smoothing fix, user 2026-10-13 22:50: cutting
	//! from the wheel's contact height wrote every bounce into the ground, so ruts got bumpy). Per sample column, once
	//! per op: surface now - m_fDepth x the rut profile (full in the middle, half at RUT_R; the strongest segment
	//! wins), never deeper than m_fB under the undug ground (or FLOOR). A column counts only for the segment it
	//! projects into (0 <= u < 1), so the joints between segments / ops are not cut twice. Skipped where the wheel was
	//! not on that surface (> RUT_ON m off: air, a roof). Only lowers; hard columns as Carve.
	//! look (op 6, RUT_LOOK): the same cut into the look field only (PutLook) - floors still from the feel surface.
	protected void CarveRut(MUD_VoxelOp op, bool look)
	{
		float r = RUT_R;
		map<int, float> best = new map<int, float>();      // column key -> strongest profile (fall)
		map<int, float> atY = new map<int, float>();       // wheel height there
		if (op.m_aSegA.IsEmpty())
			return;
		vector a0 = op.m_aSegA[0];
		m_iRutI0 = Math.Floor((a0[0] - m_fX0) / CELL);
		m_iRutK0 = Math.Floor((a0[2] - m_fZ0) / CELL);
		for (int s = 0; s < op.m_aSegA.Count(); s++)
		{
			vector a = op.m_aSegA[s];
			vector b = op.m_aSegB[s];
			float abx = b[0] - a[0];
			float abz = b[2] - a[2];
			float len2 = abx * abx + abz * abz;
			if (len2 < 0.000001)
				continue;
			int gi0 = Math.Floor((Math.Min(a[0], b[0]) - r - m_fX0) / CELL);
			int gi1 = Math.Ceil((Math.Max(a[0], b[0]) + r - m_fX0) / CELL);
			int gk0 = Math.Floor((Math.Min(a[2], b[2]) - r - m_fZ0) / CELL);
			int gk1 = Math.Ceil((Math.Max(a[2], b[2]) + r - m_fZ0) / CELL);
			for (int gk = gk0; gk <= gk1; gk++)
			{
				for (int gi = gi0; gi <= gi1; gi++)
				{
					float px = m_fX0 + gi * CELL;
					float pz = m_fZ0 + gk * CELL;
					float u = ((px - a[0]) * abx + (pz - a[2]) * abz) / len2;
					if (u < 0 || u >= 1)
						continue;
					float qx = a[0] + abx * u - px;
					float qz = a[2] + abz * u - pz;
					float dist = Math.Sqrt(qx * qx + qz * qz);
					if (dist >= r)
						continue;
					float k = dist / r;
					float fall = 1 - k * k * 0.5;
					int key = RutKey(gi, gk);
					float had = -1;
					if (best.Contains(key))
						had = best.Get(key);
					if (fall <= had)
						continue;
					best.Set(key, fall);
					atY.Set(key, a[1] + (b[1] - a[1]) * u);
				}
			}
		}
		foreach (int ck, float cf : best)
		{
			int cgi = ck % RUT_KEY_W - RUT_KEY_OFF + m_iRutI0;
			int cgk = ck / RUT_KEY_W - RUT_KEY_OFF + m_iRutK0;
			MUD_VoxelColumn col = ColAt(cgi, cgk);
			if (!col)
				continue;
			int li = cgi - col.m_iBi * SUB;
			int lk = cgk - col.m_iBk * SUB;
			int n = lk * SUB + li;
			if (col.m_aHard[n] != 0)
				continue;
			float wheelY = atY.Get(ck);
			float surf = 0;
			if (!SurfaceY(col, li, lk, wheelY, surf))
				continue;
			if (Math.AbsFloat(surf - wheelY) > RUT_ON)
				continue;
			float floorY = surf - op.m_fDepth * cf;
			float capY = col.m_aUndug[n] - op.m_fB * cf;
			if (floorY < capY)
				floorY = capY;
			float deepY = col.m_aUndug[n] - FLOOR;
			if (floorY < deepY)
				floorY = deepY;
			if (floorY >= surf)
				continue;   // already at its deepest
			int gj0 = Math.Floor(floorY / CELL) - 1;
			int gj1 = Math.Ceil(surf / CELL) + 1;
			for (int gj = gj0; gj <= gj1; gj++)
			{
				float y = gj * CELL;
				float cut = floorY - y;
				float old = Value(col, li, lk, gj);
				if (cut >= old)
					continue;
				if (look)
					PutLook(col, li, lk, cgi, gj, cgk, old, Nudge(cut));
				else
					Put(col, li, lk, cgi, gj, cgk, old, Nudge(cut));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Op 5, all-voxel test: every sample column within m_fDepth of a strip marks its cells carved (they go VOX at the
	//! next GoLive and draw the seeded voxels). No sample changes. Hard columns are left alone.
	protected void AheadCells(MUD_VoxelOp op)
	{
		float r = op.m_fDepth;
		for (int s = 0; s < op.m_aSegA.Count(); s++)
		{
			vector a = op.m_aSegA[s];
			vector b = op.m_aSegB[s];
			float abx = b[0] - a[0];
			float abz = b[2] - a[2];
			float len2 = abx * abx + abz * abz;
			int gi0 = Math.Floor((Math.Min(a[0], b[0]) - r - m_fX0) / CELL);
			int gi1 = Math.Ceil((Math.Max(a[0], b[0]) + r - m_fX0) / CELL);
			int gk0 = Math.Floor((Math.Min(a[2], b[2]) - r - m_fZ0) / CELL);
			int gk1 = Math.Ceil((Math.Max(a[2], b[2]) + r - m_fZ0) / CELL);
			for (int gk = gk0; gk <= gk1; gk++)
			{
				for (int gi = gi0; gi <= gi1; gi++)
				{
					MUD_VoxelColumn col = ColAt(gi, gk);
					if (!col)
						continue;
					int n = (gk - col.m_iBk * SUB) * SUB + gi - col.m_iBi * SUB;
					if (col.m_aHard[n] != 0)
						continue;
					float px = m_fX0 + gi * CELL;
					float pz = m_fZ0 + gk * CELL;
					float u = 0;
					if (len2 > 0.000001)
						u = Math.Clamp(((px - a[0]) * abx + (pz - a[2]) * abz) / len2, 0, 1);
					float qx = a[0] + abx * u - px;
					float qz = a[2] + abz * u - pz;
					if (qx * qx + qz * qz > r * r)
						continue;
					MarkCarved(gi, gk);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Global sample column gi, gk as one int (CarveRut's maps), relative to the op's first segment
	protected int RutKey(int gi, int gk)
	{
		return (gk - m_iRutK0 + RUT_KEY_OFF) * RUT_KEY_W + gi - m_iRutI0 + RUT_KEY_OFF;
	}

	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS (op 4 when DISPLACE_RUTS): CarveRut's profile on the FCELL floor grid, written into the
	//! columns' rut floor (m_aFloor = min) - no sample changes, no Put. Marks the cells carved where the floor goes
	//! under the field (a rut on fresh ground must go voxel to draw) and the changed box (Submit's QueueBox rebuilds
	//! the bricks there + 1 m, which covers the brick-line margin and the bricks above / below).
	protected void CarveFloor(MUD_VoxelOp op)
	{
		int t0 = System.GetTickCount();
		int tC = t0;     // MUD_Prof carve parts (TEMP 2026-10-18)
		m_bOpCrude = RUT_SHEET_CRUDE && OpUnderSheet(op);
		float r = RUT_R;
		map<int, float> best = new map<int, float>();      // floor sample key -> strongest profile (fall)
		map<int, float> atY = new map<int, float>();       // wheel height there
		map<int, int> bestSeg = new map<int, int>();       // Build 3 Stage 3: the segment that gave 'best'
		if (op.m_aSegA.IsEmpty())
			return;
		MUD_RutDebug.OpSegs(op.m_aSegA, op.m_aSegB);     // diag menu Mud > Ruts and wheels
		vector a0 = op.m_aSegA[0];
		m_iFloorI0 = Math.Floor((a0[0] - m_fX0) / FCELL);
		m_iFloorK0 = Math.Floor((a0[2] - m_fZ0) / FCELL);
		m_mRampW.Clear();
		if (RUT_RAMP)
			RampChains(op);
		for (int s = 0; s < op.m_aSegA.Count(); s++)
		{
			vector a = op.m_aSegA[s];
			vector b = op.m_aSegB[s];
			float abx = b[0] - a[0];
			float abz = b[2] - a[2];
			float len2 = abx * abx + abz * abz;
			if (len2 < 0.000001)
				continue;
			int fi0 = Math.Floor((Math.Min(a[0], b[0]) - r - m_fX0) / FCELL);
			int fi1 = Math.Ceil((Math.Max(a[0], b[0]) + r - m_fX0) / FCELL);
			int fk0 = Math.Floor((Math.Min(a[2], b[2]) - r - m_fZ0) / FCELL);
			int fk1 = Math.Ceil((Math.Max(a[2], b[2]) + r - m_fZ0) / FCELL);
			for (int fk = fk0; fk <= fk1; fk++)
			{
				for (int fi = fi0; fi <= fi1; fi++)
				{
					float px = m_fX0 + fi * FCELL;
					float pz = m_fZ0 + fk * FCELL;
					// as CarveRut: u < 0 || u >= 1 skipped, never clamped (a sample on a joint is cut once)
					float u = ((px - a[0]) * abx + (pz - a[2]) * abz) / len2;
					if (u < 0 || u >= 1)
						continue;
					float qx = a[0] + abx * u - px;
					float qz = a[2] + abz * u - pz;
					float dist = Math.Sqrt(qx * qx + qz * qz);
					if (dist >= r)
						continue;
					int ri = fi - m_iFloorI0 + RUT_KEY_OFF;
					int rk = fk - m_iFloorK0 + RUT_KEY_OFF;
					if (ri < 0 || ri >= RUT_KEY_OFF * 2 || rk < 0 || rk >= RUT_KEY_OFF * 2)
						continue;   // over 256 m from the first segment: the keys would overlap
					float k = dist / r;
					float w = 1;
					if (RUT_RAMP)
						w = RampW(m_aRampT0[s] + u * Math.Sqrt(len2), m_aRampL[s]);
					if (w <= 0)
						continue;
					// plan Step 4 Feel fix (user 2026-10-15: stuck in the rut): fall 0 at the rim, not 0.5 (CarveRut's
					// half-depth step showed as a curb on the 0.25 m floor). RUT_RAMP: 'best' = fall x weight
					float fall = (1 - k * k) * w;
					int key = rk * RUT_KEY_W + ri;
					if (best.Contains(key) && fall <= best.Get(key))
						continue;
					best.Set(key, fall);
					m_mRampW.Set(key, w);
					atY.Set(key, a[1] + (b[1] - a[1]) * u);
					bestSeg.Set(key, s);
				}
			}
		}
		if (RUT_RAMP)
			RampBehind(op, best, atY, bestSeg);
		m_iCSeg = System.GetTickCount(tC);
		MUD_Prof.Add(MUD_Prof.C_SEG, m_iCSeg);
		tC = System.GetTickCount();
		int written = 0;
		array<int> wrote = {};      // keys the cut wrote (Build 3 Stage 2: the relax works round them)
		// Build 3 Stage 3: deepest cut per segment, before the relax (the proto's maxcut, per segment: a stretch the cut
		// skipped - hard ground, road edge - pushes up no ridge beside it)
		array<float> segCut = {};
		for (int sc = 0; sc < op.m_aSegA.Count(); sc++)
			segCut.Insert(0);
		foreach (int ck, float cf : best)
		{
			int cfi = ck % RUT_KEY_W - RUT_KEY_OFF + m_iFloorI0;
			int cfk = ck / RUT_KEY_W - RUT_KEY_OFF + m_iFloorK0;
			float wheelY = atY.Get(ck);
			float fieldY = 0;
			float undug = 0;
			int cgi = 0;
			int cgk = 0;
			if (!FloorBase(cfi, cfk, wheelY, fieldY, undug, cgi, cgk))
				continue;
			int fbi = FloorDiv(cfi, FSUB);
			int fbk = FloorDiv(cfk, FSUB);
			MUD_RutTile ftile = m_mRutTiles.Get(ColKey(fbi, fbk));
			int fn = (cfk - fbk * FSUB) * FSUB + cfi - fbi * FSUB;
			float surf = fieldY;
			if (ftile && ftile.m_aFloor[fn] < surf)
				surf = ftile.m_aFloor[fn];    // the rut so far: else ruts never deepen
			if (Math.AbsFloat(surf - wheelY) > RUT_ON)
				continue;
			float rw = 1;
			if (RUT_RAMP)
				rw = m_mRampW.Get(ck);    // > 0: set with every 'best'
			if (WEAR_PASSES > 0 && WearPass(ftile, fbi, fbk, fn, rw))
				continue;     // WEAR COUNT: grass not broken yet - this pass only counted
			float capY = undug - op.m_fB * cf / rw;     // RUT_RAMP: the cap keeps the plain profile, only the cut fades
			float cut = op.m_fDepth * cf / rw;
			float left = RUT_SHARE * (surf - capY);     // R4: the first passes cut most, later ones less
			if (left > cut)
				cut = left;
			// FIRST PASS TO DIRT: cf holds the ramp weight (fall x w), so / rw = the plain profile, as capY (2026-10-19:
			// without it the cut got w x w = half depth where two ops fade over each other = grass spots, worst in slow turns)
			if (RUT_FIRST_D > 0 && undug - surf < RUT_FIRST_D * cf / rw - RUT_FIRST_TOL)
				cut = RUT_FIRST_D * cf / rw;     // plain depth, not a top-up (a top-up under RUT_RAMP
				                            // leaves w (1 - w) short where two ops fade over each other = a dip every op)
			else if (RUT_PASS_MAX > 0 && cut > RUT_PASS_MAX)
				cut = RUT_PASS_MAX;     // RUT PASS MAX: later passes deepen slowly
			cut = cut * rw;
			float floorY = surf - cut;
			if (floorY < capY)
				floorY = capY;
			float deepY = undug - FLOOR;
			if (floorY < deepY)
				floorY = deepY;
			// J2 (Build 2 stage 1, 2026-10-17): whole cm, the same float a joiner gets from the cm snapshot
			floorY = Math.Round(floorY * 100) / 100.0;
			if (floorY >= surf)
				continue;   // already at its deepest
			int cs = bestSeg.Get(ck);
			if (surf - floorY > segCut[cs])
				segCut[cs] = surf - floorY;
			if (!ftile)
				ftile = RutTileMake(fbi, fbk);
			ftile.m_aFloor[fn] = floorY;    // under surf, so under the old floor too: the min
			if (ftile.m_aRidge[fn] < FLOOR_NONE * 0.5)
				RidgeGone(ftile, fn, ColKey(fbi, fbk));     // Build 3 Stage 3: the draw max would hide the cut
			FloorChanged(cfi, cfk);
			wrote.Insert(ck);
			written++;
			m_iDispOpW++;
			int rv = Math.Round(floorY * 1000);
			m_iDispHash = (m_iDispHash + (rv * (fn + 1) + ColKey(fbi, fbk)) % DISP_HASH_M) % DISP_HASH_M;
			if (!RUT_FIELD && floorY < fieldY - CARVE_TOL)
				MarkCarved(cgi, cgk);     // RUT FIELD: never - the heightfield draws plain ruts, bricks draw theirs
			int gjLo = Math.Floor(floorY / CELL);
			int gjHi = Math.Ceil(surf / CELL);
			MarkBox(cgi, gjLo, cgk);
			MarkBox(cgi, gjHi, cgk);
		}
		m_iCWrite = System.GetTickCount(tC);
		MUD_Prof.Add(MUD_Prof.C_WRITE, m_iCWrite);
		tC = System.GetTickCount();
		int relaxed = 0;
		if (RUT_REPOSE_ITERS > 0 && !m_bOpCrude)
			relaxed = RelaxFloor(wrote, atY);
		m_iCRelax = System.GetTickCount(tC);
		MUD_Prof.Add(MUD_Prof.C_RELAX, m_iCRelax);
		tC = System.GetTickCount();
		int ridged = 0;
		if (RUT_RIDGES && RUT_FIELD && !m_bOpCrude)
			ridged = GrowRidges(op, segCut);
		m_iCRidge = System.GetTickCount(tC);
		MUD_Prof.Add(MUD_Prof.C_RIDGE, m_iCRidge);
		m_iDispCarved += m_aOpCarved.Count();
		tC = System.GetTickCount();
		if (OP_LOG)
			Print(string.Format("MUD rut floor: %1 samples, %2 relaxed, %3 ridged, %4 carved cells, %5 ms", written, relaxed, ridged, m_aOpCarved.Count(), System.GetTickCount(t0)), LogLevel.NORMAL);
		s_iWearCut += written;
		if (WEAR_PASSES > 0 && t0 - s_iWearLogAt >= 10000)
		{
			if (s_iWearAdd + s_iWearCut > 0)
				Print(string.Format("MUD rut wear 10 s: %1 samples only worn (no cut), %2 samples cut | WEAR_PASSES %3", s_iWearAdd, s_iWearCut, WEAR_PASSES), LogLevel.NORMAL);
			s_iWearAdd = 0;
			s_iWearCut = 0;
			s_iWearLogAt = t0;
		}
		m_iCLog = System.GetTickCount(tC);
	}

	//------------------------------------------------------------------------------------------------
	//! WEAR COUNT: sample fn of tile bi, bk (ftile may be null) has no rut floor and fewer than WEAR_PASSES passes ->
	//! count this pass (weight rw), true = cut nothing there. Op content only (same on every machine, J2).
	protected bool WearPass(MUD_RutTile ftile, int bi, int bk, int fn, float rw)
	{
		float wr = 0;
		if (ftile)
		{
			if (ftile.m_aFloor[fn] < FLOOR_NONE * 0.5)
				return false;     // already a rut: wear is over
			wr = ftile.m_aWear[fn];
		}
		if (wr >= WEAR_PASSES - WEAR_TOL)
			return false;
		MUD_RutTile wt = ftile;
		if (!wt)
			wt = RutTileMake(bi, bk);
		wt.m_aWear[fn] = Math.Round((wr + rw) * 100) / 100.0;     // J2: the same float as the snapshot's whole 0.01
		int wv = Math.Round(wt.m_aWear[fn] * 100);
		m_iDispHash = (m_iDispHash + ((wv + WEAR_SALT) * (fn + 1) + ColKey(bi, bk)) % DISP_HASH_M) % DISP_HASH_M;
		s_iWearAdd++;
		if (MUD_WorldGroundEntity.PRESS_LIFT)
		{
			// pressed lift look test (2026-10-19): the client patch mesh there builds again (look only, no floor read)
			int wfk = fn / FSUB;
			int wfi = fn - wfk * FSUB;
			MUD_WorldGroundEntity.PressWearMark(m_fX0 + (bi * FSUB + wfi) * FCELL, m_fZ0 + (bk * FSUB + wfk) * FCELL);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! WEAR COUNT: passes counted at global floor sample fi, fk (0 = none / no tile)
	protected float WearSample(int fi, int fk)
	{
		int bi = FloorDiv(fi, FSUB);
		int bk = FloorDiv(fk, FSUB);
		MUD_RutTile tile = m_mRutTiles.Get(ColKey(bi, bk));
		if (!tile)
			return 0;
		return tile.m_aWear[(fk - bk * FSUB) * FSUB + fi - bi * FSUB];
	}

	//------------------------------------------------------------------------------------------------
	//! WEAR COUNT (look ruts, trail rule): true = a pass at global fine vertex gx, gz would only count wear, not cut
	//! (no rut floor there and fewer than WEAR_PASSES passes so far). False when wear is off or the field is off.
	static bool WearOnly(int gx, int gz)
	{
		if (WEAR_PASSES <= 0 || !RUT_FIELD || !DISPLACE_RUTS || !s_Inst)
			return false;
		if (s_Inst.FloorSample(gx, gz) < FLOOR_NONE * 0.5)
			return false;
		return s_Inst.WearSample(gx, gz) < WEAR_PASSES - WEAR_TOL;
	}

	//------------------------------------------------------------------------------------------------
	//! WEAR COUNT (trail rule): passes counted at world x, z on the nearest floor sample (0 = none / field off)
	static float WearAt(float x, float z)
	{
		if (WEAR_PASSES <= 0 || !RUT_FIELD || !DISPLACE_RUTS || !s_Inst)
			return 0;
		int gx = Math.Round((x - s_Inst.m_fX0) / FCELL);
		int gz = Math.Round((z - s_Inst.m_fZ0) / FCELL);
		return s_Inst.WearSample(gx, gz);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_RAMP: link op's segments into wheel chains (a segment's start = an earlier segment's end, exact - the
	//! same vector from RutWheel, or the same cm text on a joiner). Fills m_aRampT0 / m_aRampL / m_aRampHead.
	protected void RampChains(MUD_VoxelOp op)
	{
		int n = op.m_aSegA.Count();
		array<int> prv = {};
		array<int> nxt = {};
		m_aRampT0.Clear();
		m_aRampL.Clear();
		m_aRampHead.Clear();
		for (int i = 0; i < n; i++)
		{
			prv.Insert(-1);
			nxt.Insert(-1);
			m_aRampT0.Insert(0);
			m_aRampL.Insert(0);
			m_aRampHead.Insert(0);
		}
		for (int s = 1; s < n; s++)
		{
			vector sa = op.m_aSegA[s];
			for (int j = s - 1; j >= 0; j--)
			{
				vector jb = op.m_aSegB[j];
				if (jb[0] == sa[0] && jb[1] == sa[1] && jb[2] == sa[2] && nxt[j] < 0)
				{
					prv[s] = j;
					nxt[j] = s;
					break;
				}
			}
		}
		for (int h = 0; h < n; h++)
		{
			if (prv[h] >= 0)
				continue;
			m_aRampHead[h] = 1;
			float run = 0;
			int c = h;
			while (c >= 0)
			{
				m_aRampT0[c] = run;
				vector ca = op.m_aSegA[c];
				vector cb = op.m_aSegB[c];
				run += Math.Sqrt((cb[0] - ca[0]) * (cb[0] - ca[0]) + (cb[2] - ca[2]) * (cb[2] - ca[2]));
				c = nxt[c];
			}
			c = h;
			while (c >= 0)
			{
				m_aRampL[c] = run;
				c = nxt[c];
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_RAMP: cut weight at chain distance t (m from the chain start, < 0 = behind it) of a chain L m long:
	//! fade in over [-RUT_RAMP_LEN, 0], fade out over [L - RUT_RAMP_LEN, L]. Ops that follow on add up to 1.
	static float RampW(float t, float L)
	{
		return Math.Clamp((t + RUT_RAMP_LEN) / RUT_RAMP_LEN, 0, 1) - Math.Clamp((t - L + RUT_RAMP_LEN) / RUT_RAMP_LEN, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_RAMP: the fade-in behind each chain's start - its first segment run back RUT_RAMP_LEN m (straight), same
	//! capsule profile and keys as CarveFloor's segment loop
	protected void RampBehind(MUD_VoxelOp op, map<int, float> best, map<int, float> atY, map<int, int> bestSeg)
	{
		float r = RUT_R;
		for (int s = 0; s < op.m_aSegA.Count(); s++)
		{
			if (m_aRampHead[s] == 0)
				continue;
			vector a = op.m_aSegA[s];
			vector b = op.m_aSegB[s];
			float dx = b[0] - a[0];
			float dz = b[2] - a[2];
			float len = Math.Sqrt(dx * dx + dz * dz);
			if (len < 0.001)
				continue;
			dx = dx / len;
			dz = dz / len;
			float ex = a[0] - dx * RUT_RAMP_LEN;
			float ez = a[2] - dz * RUT_RAMP_LEN;
			int fi0 = Math.Floor((Math.Min(a[0], ex) - r - m_fX0) / FCELL);
			int fi1 = Math.Ceil((Math.Max(a[0], ex) + r - m_fX0) / FCELL);
			int fk0 = Math.Floor((Math.Min(a[2], ez) - r - m_fZ0) / FCELL);
			int fk1 = Math.Ceil((Math.Max(a[2], ez) + r - m_fZ0) / FCELL);
			for (int fk = fk0; fk <= fk1; fk++)
			{
				for (int fi = fi0; fi <= fi1; fi++)
				{
					float px = m_fX0 + fi * FCELL - a[0];
					float pz = m_fZ0 + fk * FCELL - a[2];
					float t = px * dx + pz * dz;     // < 0 behind the start
					if (t >= 0 || t < -RUT_RAMP_LEN)
						continue;
					float qx = px - dx * t;
					float qz = pz - dz * t;
					float dist = Math.Sqrt(qx * qx + qz * qz);
					if (dist >= r)
						continue;
					int ri = fi - m_iFloorI0 + RUT_KEY_OFF;
					int rk = fk - m_iFloorK0 + RUT_KEY_OFF;
					if (ri < 0 || ri >= RUT_KEY_OFF * 2 || rk < 0 || rk >= RUT_KEY_OFF * 2)
						continue;
					float w = RampW(t, m_aRampL[s]);
					if (w <= 0)
						continue;
					float k = dist / r;
					float fall = (1 - k * k) * w;
					int key = rk * RUT_KEY_W + ri;
					if (best.Contains(key) && fall <= best.Get(key))
						continue;
					best.Set(key, fall);
					m_mRampW.Set(key, w);
					atY.Set(key, a[1]);
					bestSeg.Set(key, s);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS: may a rut op cut floor sample fi, fk? False = skip it (hard / kept, no column with the field
	//! off, no surface). fieldY = the ground there without the rut floor (voxel top near nearY, or the undug ground on
	//! plain RUT FIELD ground), undug = the undug ground, cgi / cgk = the nearest 0.5 m sample column (hard check,
	//! carved cells, changed box). CarveFloor's cut and its repose relax share this rule.
	protected bool FloorBase(int fi, int fk, float nearY, out float fieldY, out float undug, out int cgi, out int cgk)
	{
		fieldY = 0;
		undug = 0;
		cgi = Math.Round(fi * 0.5);
		cgk = Math.Round(fk * 0.5);
		MUD_VoxelColumn col = ColAt(cgi, cgk);
		if (!col && !RUT_FIELD)
			return false;
		if (col)
		{
			if (col.m_aHard[(cgk - col.m_iBk * SUB) * SUB + cgi - col.m_iBi * SUB] != 0)
				return false;
		}
		else if (Kept(m_fX0 + cgi * CELL, m_fZ0 + cgk * CELL) || Hard(m_fX0 + cgi * CELL, m_fZ0 + cgk * CELL))
			return false;   // RUT FIELD, no column: Convert's hard rule (kept within 0.5 m OR hard), skin hole map = same
			                // on every machine. Hard alone cut ruts up to the asphalt / concrete edge (user 2026-10-15)
		if (!RUT_FIELD && !m_mCols.Contains(ColKey(FloorDiv(fi, FSUB), FloorDiv(fk, FSUB))))
			return false;
		if (col && FieldSurfAt(fi, fk, nearY, fieldY))
		{
			undug = UndugAt(fi, fk);
			return true;
		}
		// RUT FIELD plain ground (no column, or one FieldSurfAt can't read): the undug ground, deterministic on every
		// machine (not m_aH: it comes by net and can lag)
		if (!RUT_FIELD)
			return false;
		if (!MUD_WorldGroundEntity.UndugY(m_fX0 + fi * FCELL, m_fZ0 + fk * FCELL, undug))
			return false;
		fieldY = undug;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 2 (SCM-lite, Tools/scm_lite_proto.py relax): angle-of-repose relax after CarveFloor's cut. Over
	//! the samples the cut wrote + 1 ring, in SORTED key order (the order changes the result: every machine must
	//! match), RUT_REPOSE_ITERS times: where the rut depth drops more than RUT_REPOSE_TAN x FCELL to a 4-neighbour,
	//! half the excess moves from the high sample to the low one (volume kept, except where the cm / FLOOR clamps
	//! cut it). Depth = under the ground without the
	//! rut (FloorBase fieldY), so hillsides and crater walls never move by themselves. The low side RISES (a floor
	//! goes up, or back to none at the ground), the high side may get a new floor (the rut widens, up to 2 samples
	//! past the cut: PrepareRut's box). Hard / kept samples stay out (FloorBase). Then whole cm (J2), never under
	//! undug - FLOOR. Reads / writes the floor inside a kind 4 op only (J2: joiners take the result from the tiles).
	//! Returns the samples it wrote.
	protected int RelaxFloor(array<int> wrote, map<int, float> atY)
	{
		if (wrote.IsEmpty())
			return 0;
		wrote.Sort();
		int keyMax = RUT_KEY_OFF * 2;
		map<int, int> at = new map<int, int>();     // key -> index in the p arrays, -1 = stays out
		array<int> pKey = {};
		array<float> pD = {};       // rut depth: surface - ground without the rut (<= 0)
		array<float> pBase = {};    // ground without the rut (FloorBase fieldY)
		array<float> pDeep = {};    // undug - FLOOR
		array<int> pGi = {};
		array<int> pGk = {};
		array<int> pMoved = {};
		// pass 0: the written samples with their own wheel height (FieldSurfAt picks the surface near it), pass 1: the
		// samples up to 2 round them with the first (sorted) written neighbour's wheel height
		for (int ph = 0; ph < 2; ph++)
		{
			int reach = ph * 2;
			foreach (int wk : wrote)
			{
				float nearY = atY.Get(wk);
				int wri = wk % RUT_KEY_W;
				int wrk = wk / RUT_KEY_W;
				for (int dk = -reach; dk <= reach; dk++)
				{
					for (int di = -reach; di <= reach; di++)
					{
						int ri = wri + di;
						int rk = wrk + dk;
						if (ri < 0 || ri >= keyMax || rk < 0 || rk >= keyMax)
							continue;
						int nk = rk * RUT_KEY_W + ri;
						if (at.Contains(nk))
							continue;
						int fi = ri - RUT_KEY_OFF + m_iFloorI0;
						int fk = rk - RUT_KEY_OFF + m_iFloorK0;
						float fieldY = 0;
						float undug = 0;
						int cgi = 0;
						int cgk = 0;
						if (!FloorBase(fi, fk, nearY, fieldY, undug, cgi, cgk))
						{
							at.Insert(nk, -1);
							continue;
						}
						float h = FloorSample(fi, fk);
						if (h > fieldY)
							h = fieldY;
						at.Insert(nk, pKey.Count());
						pKey.Insert(nk);
						pD.Insert(h - fieldY);
						pBase.Insert(fieldY);
						pDeep.Insert(undug - FLOOR);
						pGi.Insert(cgi);
						pGk.Insert(cgk);
						pMoved.Insert(0);
					}
				}
			}
		}
		// the relax order: written + 1 ring (8 round), sorted
		set<int> ringSeen = new set<int>();
		array<int> ring = {};
		foreach (int wk2 : wrote)
		{
			for (int dk2 = -1; dk2 <= 1; dk2++)
			{
				for (int di2 = -1; di2 <= 1; di2++)
				{
					int rkey = wk2 + dk2 * RUT_KEY_W + di2;
					if (ringSeen.Contains(rkey) || !at.Contains(rkey))
						continue;
					ringSeen.Insert(rkey);
					if (at.Get(rkey) >= 0)
						ring.Insert(rkey);
				}
			}
		}
		ring.Sort();
		// 4 neighbours in the proto's NB4 order (dk, di) = (0, 1), (1, 0), (0, -1), (-1, 0). A key off the key range is
		// never in 'at' (RUT_KEY_W > keyMax, so +-1 never wraps into a real key)
		array<int> nbOff = {};
		nbOff.Insert(1);
		nbOff.Insert(RUT_KEY_W);
		nbOff.Insert(-1);
		nbOff.Insert(-RUT_KEY_W);
		float lim = RUT_REPOSE_TAN * FCELL;
		for (int it = 0; it < RUT_REPOSE_ITERS; it++)
		{
			foreach (int ak : ring)
			{
				int ia = at.Get(ak);
				for (int nb = 0; nb < 4; nb++)
				{
					int bk = ak + nbOff[nb];
					if (!at.Contains(bk))
						continue;
					int ib = at.Get(bk);
					if (ib < 0)
						continue;
					float dh = pD[ia] - pD[ib];
					if (dh <= lim)
						continue;
					float move = (dh - lim) * 0.5;
					pD[ia] = pD[ia] - move;
					pD[ib] = pD[ib] + move;
					pMoved[ia] = 1;
					pMoved[ib] = 1;
				}
			}
		}
		// write back: whole cm, never under undug - FLOOR; back at the ground = no floor
		int wroteN = 0;
		for (int p = 0; p < pKey.Count(); p++)
		{
			if (pMoved[p] == 0)
				continue;
			int pk = pKey[p];
			int pfi = pk % RUT_KEY_W - RUT_KEY_OFF + m_iFloorI0;
			int pfk = pk / RUT_KEY_W - RUT_KEY_OFF + m_iFloorK0;
			float baseY = pBase[p];
			float d = pD[p];
			if (d > 0)
				d = 0;
			float newY = baseY + d;
			if (newY < pDeep[p])
				newY = pDeep[p];
			newY = Math.Round(newY * 100) / 100.0;
			float storeY = newY;
			if (newY >= baseY)
				storeY = FLOOR_NONE;
			float oldY = FloorSample(pfi, pfk);
			if (storeY == oldY)
				continue;
			int pbi = FloorDiv(pfi, FSUB);
			int pbk = FloorDiv(pfk, FSUB);
			MUD_RutTile ptile = m_mRutTiles.Get(ColKey(pbi, pbk));
			if (!ptile)
				ptile = RutTileMake(pbi, pbk);
			int pn = (pfk - pbk * FSUB) * FSUB + pfi - pbi * FSUB;
			ptile.m_aFloor[pn] = storeY;
			if (ptile.m_aRidge[pn] < FLOOR_NONE * 0.5)
				RidgeGone(ptile, pn, ColKey(pbi, pbk));     // Build 3 Stage 3: a widened rut takes the ridge away
			FloorChanged(pfi, pfk);
			wroteN++;
			m_iDispOpW++;
			int prv = 7919;     // back to none: hashed as ClearFloor's
			if (storeY < FLOOR_NONE * 0.5)
				prv = Math.Round(storeY * 1000);
			m_iDispHash = (m_iDispHash + (prv * (pn + 1) + ColKey(pbi, pbk)) % DISP_HASH_M) % DISP_HASH_M;
			if (!RUT_FIELD && newY < baseY - CARVE_TOL)
				MarkCarved(pGi[p], pGk[p]);
			// bricks from the lowest of old / new up to the highest of ground / old (a raised floor redraws too)
			float botY = newY;
			float topY = baseY;
			if (oldY < FLOOR_NONE * 0.5)
			{
				if (oldY < botY)
					botY = oldY;
				if (oldY > topY)
					topY = oldY;
			}
			int pjLo = Math.Floor(botY / CELL);
			int pjHi = Math.Ceil(topY / CELL);
			MarkBox(pGi[p], pjLo, pGk[p]);
			MarkBox(pGi[p], pjHi, pGk[p]);
		}
		return wroteN;
	}

	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS: rut floor (world y) at global floor sample fi, fk; FLOOR_NONE = none (or no tile)
	protected float FloorSample(int fi, int fk)
	{
		int bi = FloorDiv(fi, FSUB);
		int bk = FloorDiv(fk, FSUB);
		MUD_RutTile tile = m_mRutTiles.Get(ColKey(bi, bk));
		if (!tile)
			return FLOOR_NONE;
		return tile.m_aFloor[(fk - bk * FSUB) * FSUB + fi - bi * FSUB];
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 3: ridge top (world y) at global floor sample fi, fk; FLOOR_NONE = none (or no tile)
	protected float RidgeSample(int fi, int fk)
	{
		int bi = FloorDiv(fi, FSUB);
		int bk = FloorDiv(fk, FSUB);
		MUD_RutTile tile = m_mRutTiles.Get(ColKey(bi, bk));
		if (!tile)
			return FLOOR_NONE;
		return tile.m_aRidge[(fk - bk * FSUB) * FSUB + fi - bi * FSUB];
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 3: tile sample n (tile key key) loses its ridge - hashed (RUT_RIDGE_GONE). The caller marks it
	//! changed (FloorChanged).
	protected void RidgeGone(MUD_RutTile tile, int n, int key)
	{
		tile.m_aRidge[n] = FLOOR_NONE;
		m_iDispHash = (m_iDispHash + (RUT_RIDGE_GONE * (n + 1) + key) % DISP_HASH_M) % DISP_HASH_M;
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 3: a voxel column holds one of the 4 sample columns round floor sample fi, fk (FieldSurfAt's
	//! corners) - no ridge there (ridges are plain RUT FIELD ground only: no bricks, no Displace, no rows)
	protected bool ColRound(int fi, int fk)
	{
		int gi = FloorDiv(fi, 2);
		int gk = FloorDiv(fk, 2);
		return ColAt(gi, gk) != null || ColAt(gi + 1, gk) != null || ColAt(gi, gk + 1) != null || ColAt(gi + 1, gk + 1) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 3 (Tools/scm_lite_proto.py ridge(), blend "add", vol_r = RUT_R, rclr): bulldozed ridges after
	//! CarveFloor's cut + relax. Band samples RUT_R + RUT_RIDGE_GAP <= d < that + RUT_RIDGE_W from a segment, 0 <= u < 1
	//! (as the cut), the strongest rise over the segments: h x sin(pi (d - RUT_R - GAP) / W), h = B x segCut[s] x pi / 3
	//! x RUT_R / W (segCut = CarveFloor's deepest cut per segment; the proto has one track = one maxcut).
	//! Only on plain RUT FIELD ground: no voxel column round the sample (ColRound), FloorBase ok (hard /
	//! kept stay out), no floor there after the cut + relax, the wheel within RUT_ON of the ground (as the cut). SORTED
	//! keys. The ridge grows each op: min(undug + RUT_RIDGE_MAX, max(old, undug) + rise), whole cm (J2 snapshot). Reads
	//! / writes the tiles inside a kind 4 op only (J2: joiners take the result from the tiles). No MarkBox / Row (no
	//! bricks there): the heightfield patches draw it (FloorChanged). Returns the samples it wrote.
	protected int GrowRidges(MUD_VoxelOp op, array<float> segCut)
	{
		float rIn = RUT_R + RUT_RIDGE_GAP;
		float rOut = rIn + RUT_RIDGE_W;
		map<int, float> rise = new map<int, float>();      // floor sample key -> strongest rise
		map<int, float> atY = new map<int, float>();       // wheel height there
		for (int s = 0; s < op.m_aSegA.Count(); s++)
		{
			vector a = op.m_aSegA[s];
			vector b = op.m_aSegB[s];
			float abx = b[0] - a[0];
			float abz = b[2] - a[2];
			float len2 = abx * abx + abz * abz;
			if (len2 < 0.000001 || segCut[s] <= 0)
				continue;
			float h = RUT_RIDGE_B * segCut[s] * Math.PI / 3 * RUT_R / RUT_RIDGE_W;
			int fi0 = Math.Floor((Math.Min(a[0], b[0]) - rOut - m_fX0) / FCELL);
			int fi1 = Math.Ceil((Math.Max(a[0], b[0]) + rOut - m_fX0) / FCELL);
			int fk0 = Math.Floor((Math.Min(a[2], b[2]) - rOut - m_fZ0) / FCELL);
			int fk1 = Math.Ceil((Math.Max(a[2], b[2]) + rOut - m_fZ0) / FCELL);
			for (int fk = fk0; fk <= fk1; fk++)
			{
				for (int fi = fi0; fi <= fi1; fi++)
				{
					float px = m_fX0 + fi * FCELL;
					float pz = m_fZ0 + fk * FCELL;
					float u = ((px - a[0]) * abx + (pz - a[2]) * abz) / len2;
					if (u < 0 || u >= 1)
						continue;
					float qx = a[0] + abx * u - px;
					float qz = a[2] + abz * u - pz;
					float dist = Math.Sqrt(qx * qx + qz * qz);
					if (dist < rIn || dist >= rOut)
						continue;
					int ri = fi - m_iFloorI0 + RUT_KEY_OFF;
					int rk = fk - m_iFloorK0 + RUT_KEY_OFF;
					if (ri < 0 || ri >= RUT_KEY_OFF * 2 || rk < 0 || rk >= RUT_KEY_OFF * 2)
						continue;   // over 256 m from the first segment: the keys would overlap
					float up = h * Math.Sin(Math.PI * (dist - rIn) / RUT_RIDGE_W);
					int key = rk * RUT_KEY_W + ri;
					if (rise.Contains(key) && up <= rise.Get(key))
						continue;
					rise.Set(key, up);
					atY.Set(key, a[1] + (b[1] - a[1]) * u);
				}
			}
		}
		array<int> keys = {};
		foreach (int rkey, float rv0 : rise)
			keys.Insert(rkey);
		keys.Sort();
		int wroteN = 0;
		foreach (int ck : keys)
		{
			int cfi = ck % RUT_KEY_W - RUT_KEY_OFF + m_iFloorI0;
			int cfk = ck / RUT_KEY_W - RUT_KEY_OFF + m_iFloorK0;
			if (FloorSample(cfi, cfk) < FLOOR_NONE * 0.5)
				continue;   // in the rut (cut or relaxed): no ridge
			if (ColRound(cfi, cfk))
				continue;
			float wheelY = atY.Get(ck);
			float fieldY = 0;
			float undug = 0;
			int cgi = 0;
			int cgk = 0;
			if (!FloorBase(cfi, cfk, wheelY, fieldY, undug, cgi, cgk))
				continue;
			if (Math.AbsFloat(undug - wheelY) > RUT_ON)
				continue;   // as the cut: a wheel far over this ground (bridge, roof) does not push it up
			int rbi = FloorDiv(cfi, FSUB);
			int rbk = FloorDiv(cfk, FSUB);
			MUD_RutTile rtile = m_mRutTiles.Get(ColKey(rbi, rbk));
			int rn = (cfk - rbk * FSUB) * FSUB + cfi - rbi * FSUB;
			float oldY = FLOOR_NONE;
			if (rtile)
				oldY = rtile.m_aRidge[rn];
			float fromY = undug;
			if (oldY < FLOOR_NONE * 0.5 && oldY > fromY)
				fromY = oldY;
			float newY = fromY + rise.Get(ck);
			if (newY > undug + RUT_RIDGE_MAX)
				newY = undug + RUT_RIDGE_MAX;
			newY = Math.Round(newY * 100) / 100.0;     // J2: whole cm, the same float a joiner gets
			if (newY <= undug || newY == oldY)
				continue;
			if (!rtile)
				rtile = RutTileMake(rbi, rbk);
			rtile.m_aRidge[rn] = newY;
			FloorChanged(cfi, cfk);
			wroteN++;
			m_iDispOpW++;
			int rv = Math.Round(newY * 1000);
			m_iDispHash = (m_iDispHash + ((rv + RUT_RIDGE_SALT) * (rn + 1) + ColKey(rbi, rbk)) % DISP_HASH_M) % DISP_HASH_M;
		}
		return wroteN;
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 3 (WorldGround Heights): the rut field top at world x, z - bilinear over the 4 floor samples round
	//! it, each corner its floor, else its ridge, else ownY; FLOOR_NONE = no corner has a ridge (Heights then keeps the
	//! floor-only FloorAtW result exactly).
	protected float FieldTopW(float x, float z, float ownY)
	{
		float fx = (x - m_fX0) / FCELL;
		float fz = (z - m_fZ0) / FCELL;
		int ix = Math.Floor(fx);
		int iz = Math.Floor(fz);
		float tx = fx - ix;
		float tz = fz - iz;
		float t00 = ownY;
		float t10 = ownY;
		float t01 = ownY;
		float t11 = ownY;
		int ridges = TopSample(ix, iz, t00) + TopSample(ix + 1, iz, t10) + TopSample(ix, iz + 1, t01) + TopSample(ix + 1, iz + 1, t11);
		if (ridges == 0)
			return FLOOR_NONE;
		return (t00 * (1 - tx) + t10 * tx) * (1 - tz) + (t01 * (1 - tx) + t11 * tx) * tz;
	}

	//------------------------------------------------------------------------------------------------
	//! FieldTopW corner: floor sample fi, fk's floor (y = it, returns 0), else its ridge (y = it, returns 1), else y
	//! untouched (returns 0)
	protected int TopSample(int fi, int fk, inout float y)
	{
		int bi = FloorDiv(fi, FSUB);
		int bk = FloorDiv(fk, FSUB);
		MUD_RutTile tile = m_mRutTiles.Get(ColKey(bi, bk));
		if (!tile)
			return 0;
		int n = (fk - bk * FSUB) * FSUB + fi - bi * FSUB;
		if (tile.m_aFloor[n] < FLOOR_NONE * 0.5)
		{
			y = tile.m_aFloor[n];
			return 0;
		}
		if (tile.m_aRidge[n] < FLOOR_NONE * 0.5)
		{
			y = tile.m_aRidge[n];
			return 1;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! LOOK_COVERS (MUD_WorldGroundEntity): every floor sample this op changed is drawn by a client look sample already
	protected bool LookCoversOp()
	{
		if (!MUD_WorldGroundEntity.LOOK_COVERS || m_aFChgX.IsEmpty() || m_aFChgX.Count() >= REGROUND_EXACT_MAX)
			return false;
		for (int i = 0; i < m_aFChgX.Count(); i++)
		{
			int fi = Math.Round((m_aFChgX[i] - m_fX0) / FCELL);
			int fk = Math.Round((m_aFChgZ[i] - m_fZ0) / FCELL);
			if (!MUD_LookRuts.Covers(fi, fk))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: floor sample fi, fk changed in this op (grows the box Submit hands to FloorDirty)
	protected void FloorChanged(int fi, int fk)
	{
		if (MUD_TestKit.TEST_KIT)
			MUD_JoltProbe.NoteFloor(m_fX0 + fi * FCELL, m_fZ0 + fk * FCELL);     // TEMP snap-distance probe 2026-10-18
		if (m_aFChgX.Count() < REGROUND_EXACT_MAX)     // RE-GROUND EXACT (a snapshot restore outside ops stops here)
		{
			m_aFChgX.Insert(m_fX0 + fi * FCELL);
			m_aFChgZ.Insert(m_fZ0 + fk * FCELL);
		}
		if (!m_bFChg)
		{
			m_bFChg = true;
			m_iFi0 = fi;
			m_iFi1 = fi;
			m_iFk0 = fk;
			m_iFk1 = fk;
			return;
		}
		if (fi < m_iFi0)
			m_iFi0 = fi;
		if (fi > m_iFi1)
			m_iFi1 = fi;
		if (fk < m_iFk0)
			m_iFk0 = fk;
		if (fk > m_iFk1)
			m_iFk1 = fk;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD (heightfield side): rut floor (world y) at global fine vertex gx, gz - the floor sample on the same
	//! point (one grid, Ensure checks). FLOOR_NONE = none, or the field is off.
	static float FieldFloor(int gx, int gz)
	{
		if (!RUT_FIELD || !DISPLACE_RUTS || !s_Inst)
			return FLOOR_NONE;
		return s_Inst.FloorSample(gx, gz);
	}

	//------------------------------------------------------------------------------------------------
	//! Client look ruts (MUD_LookRuts): floor sample grid corner (= the fine vertex grid). False = no probe / field off
	static bool LookOrigin(out float x0, out float z0)
	{
		x0 = 0;
		z0 = 0;
		if (!RUT_FIELD || !DISPLACE_RUTS || !s_Inst)
			return false;
		x0 = s_Inst.m_fX0;
		z0 = s_Inst.m_fZ0;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client look ruts: CarveFloor's base rule (FloorBase) on plain RUT FIELD ground only - false where a voxel column
	//! is (bricks draw the floor there; the look floor only reaches patch meshes) or the cut may not go (hard / kept)
	static bool LookBase(int fi, int fk, float nearY, out float fieldY, out float undug)
	{
		fieldY = 0;
		undug = 0;
		if (!RUT_FIELD || !DISPLACE_RUTS || !s_Inst)
			return false;
		int cgi = Math.Round(fi * 0.5);
		int cgk = Math.Round(fk * 0.5);
		if (s_Inst.ColAt(cgi, cgk))
			return false;
		return s_Inst.FloorBase(fi, fk, nearY, fieldY, undug, cgi, cgk);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: rut floor at world x, z, bilinear (a corner without floor counts as ownY); FLOOR_NONE = none
	static float FieldFloorAt(float x, float z, float ownY)
	{
		if (!RUT_FIELD || !DISPLACE_RUTS || !s_Inst)
			return FLOOR_NONE;
		return s_Inst.FloorAtW(x, z, ownY);
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 3 (heightfield side): ridge top (world y) at global fine vertex gx, gz (one grid, as FieldFloor).
	//! FLOOR_NONE = none, or ridges / the field are off.
	static float FieldRidge(int gx, int gz)
	{
		if (!RUT_RIDGES || !RUT_FIELD || !DISPLACE_RUTS || !s_Inst)
			return FLOOR_NONE;
		return s_Inst.RidgeSample(gx, gz);
	}

	//------------------------------------------------------------------------------------------------
	//! perf plan B2 (2026-10-18, audit-ground.md item 2): one patch build's rut tiles, looked up once instead of a map
	//! Get per vertex. FieldPatchBegin caches the tiles reaching global fine vertices [gx0..gx0+span] x [gz0..gz0+span];
	//! FieldFloorP / FieldRidgeP then read the same samples as FieldFloor / FieldRidge (same arrays, same index = the same
	//! floats). Valid only until the next floor change - call Begin right before one build loop, never across frames.
	static void FieldPatchBegin(int gx0, int gz0, int span)
	{
		s_aFPTiles.Clear();
		s_bFPOn = RUT_FIELD && DISPLACE_RUTS && s_Inst != null;
		if (!s_bFPOn)
			return;
		s_iFPBi0 = FloorDiv(gx0, FSUB);
		s_iFPBk0 = FloorDiv(gz0, FSUB);
		s_iFPNx = FloorDiv(gx0 + span, FSUB) - s_iFPBi0 + 1;
		int nz = FloorDiv(gz0 + span, FSUB) - s_iFPBk0 + 1;
		for (int k = 0; k < nz; k++)
		{
			for (int i = 0; i < s_iFPNx; i++)
				s_aFPTiles.Insert(s_Inst.m_mRutTiles.Get(ColKey(s_iFPBi0 + i, s_iFPBk0 + k)));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! PERF IDEA 3 (2026-10-19): FieldFloorP / FieldRidgeP of the n x n vertices from gx0, gz0 (inside the FieldPatchBegin
	//! box) into fl / rg (index vz * n + vx, size >= n * n) in one call - no call per vertex. Same values (same tiles, same
	//! index). FLOOR_NONE where no tile / field off; rg FLOOR_NONE when RUT_RIDGES is off. False = no tile at all.
	static bool FieldPatchFill(int gx0, int gz0, int n, array<float> fl, array<float> rg)
	{
		int nn = n * n;
		for (int c = 0; c < nn; c++)
		{
			fl[c] = FLOOR_NONE;
			rg[c] = FLOOR_NONE;
		}
		if (!s_bFPOn)
			return false;
		bool any = false;
		for (int vz = 0; vz < n; vz++)
		{
			int oz = gz0 + vz - s_iFPBk0 * FSUB;
			int tk = oz / FSUB;
			int row = (oz - tk * FSUB) * FSUB;
			int tRow = tk * s_iFPNx;
			for (int vx = 0; vx < n; vx++)
			{
				int ox = gx0 + vx - s_iFPBi0 * FSUB;
				int ti = ox / FSUB;
				MUD_RutTile tile = s_aFPTiles[tRow + ti];
				if (!tile)
					continue;
				int at = row + ox - ti * FSUB;
				fl[vz * n + vx] = tile.m_aFloor[at];
				if (RUT_RIDGES)
					rg[vz * n + vx] = tile.m_aRidge[at];
				any = true;
			}
		}
		return any;
	}

	//------------------------------------------------------------------------------------------------
	//! perf plan B2: FieldFloor at gx, gz inside the FieldPatchBegin box
	static float FieldFloorP(int gx, int gz)
	{
		if (!s_bFPOn)
			return FLOOR_NONE;
		int ox = gx - s_iFPBi0 * FSUB;
		int oz = gz - s_iFPBk0 * FSUB;
		int ti = ox / FSUB;
		int tk = oz / FSUB;
		MUD_RutTile tile = s_aFPTiles[tk * s_iFPNx + ti];
		if (!tile)
			return FLOOR_NONE;
		return tile.m_aFloor[(oz - tk * FSUB) * FSUB + ox - ti * FSUB];
	}

	//------------------------------------------------------------------------------------------------
	//! perf plan B2: FieldRidge at gx, gz inside the FieldPatchBegin box
	static float FieldRidgeP(int gx, int gz)
	{
		if (!s_bFPOn || !RUT_RIDGES)
			return FLOOR_NONE;
		int ox = gx - s_iFPBi0 * FSUB;
		int oz = gz - s_iFPBk0 * FSUB;
		int ti = ox / FSUB;
		int tk = oz / FSUB;
		MUD_RutTile tile = s_aFPTiles[tk * s_iFPNx + ti];
		if (!tile)
			return FLOOR_NONE;
		return tile.m_aRidge[(oz - tk * FSUB) * FSUB + ox - ti * FSUB];
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 3: rut floor or ridge at world x, z, bilinear (FieldTopW); FLOOR_NONE = none
	static float FieldTopAt(float x, float z, float ownY)
	{
		if (!RUT_FIELD || !DISPLACE_RUTS || !s_Inst)
			return FLOOR_NONE;
		return s_Inst.FieldTopW(x, z, ownY);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: a floor tile reaches global fine vertices [gx0..gx1] x [gz0..gz1] (a patch that must draw it)
	static bool FieldInBox(int gx0, int gx1, int gz0, int gz1)
	{
		if (!RUT_FIELD || !DISPLACE_RUTS || !s_Inst || s_Inst.m_mRutTiles.IsEmpty())
			return false;
		int bi0 = FloorDiv(gx0, FSUB);
		int bi1 = FloorDiv(gx1, FSUB);
		int bk0 = FloorDiv(gz0, FSUB);
		int bk1 = FloorDiv(gz1, FSUB);
		for (int bk = bk0; bk <= bk1; bk++)
		{
			for (int bi = bi0; bi <= bi1; bi++)
			{
				if (s_Inst.m_mRutTiles.Contains(ColKey(bi, bk)))
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: a new floor tile for column bi, bk, every sample FLOOR_NONE
	protected MUD_RutTile RutTileMake(int bi, int bk)
	{
		MUD_RutTile tile = new MUD_RutTile();
		for (int f = 0; f < MUD_RutTile.SIZE_F; f++)
		{
			tile.m_aFloor[f] = FLOOR_NONE;
			tile.m_aRidge[f] = FLOOR_NONE;
		}
		m_mRutTiles.Insert(ColKey(bi, bk), tile);
		return tile;
	}

	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS: y, or the rut floor at sample column li, lk of col (floor sample 2 gi, 2 gk) if lower
	protected float FloorMin(MUD_VoxelColumn col, int li, int lk, float y)
	{
		if (!DISPLACE_RUTS)
			return y;
		float f = FloorSample((col.m_iBi * SUB + li) * 2, (col.m_iBk * SUB + lk) * 2);
		if (f < y)
			return f;
		return y;
	}

	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS: undug ground at floor sample fi, fk - bilinear over the sample columns round it (the
	//! converted ones; integer weights, so a floor sample on a sample column reads that column only)
	protected float UndugAt(int fi, int fk)
	{
		int gi = FloorDiv(fi, 2);
		int gk = FloorDiv(fk, 2);
		float tx = (fi - gi * 2) * 0.5;
		float tz = (fk - gk * 2) * 0.5;
		float sum = 0;
		float wsum = 0;
		for (int q = 0; q < 4; q++)
		{
			int qx = q & 1;
			int qz = q >> 1;
			float w = 1 - tx;
			if (qx == 1)
				w = tx;
			if (qz == 1)
				w = w * tz;
			else
				w = w * (1 - tz);
			if (w <= 0)
				continue;
			MUD_VoxelColumn col = ColAt(gi + qx, gk + qz);
			if (!col)
				continue;
			sum += w * col.m_aUndug[(gk + qz - col.m_iBk * SUB) * SUB + gi + qx - col.m_iBi * SUB];
			wsum += w;
		}
		if (wsum <= 0)
			return m_fLastG;
		return sum / wsum;
	}

	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS: the field's top surface at floor sample fi, fk (SurfaceY near nearY, bilinear over the
	//! sample columns round it). False = a column it needs is missing or has no surface there (skip the sample).
	protected bool FieldSurfAt(int fi, int fk, float nearY, out float y)
	{
		y = 0;
		int gi = FloorDiv(fi, 2);
		int gk = FloorDiv(fk, 2);
		float tx = (fi - gi * 2) * 0.5;
		float tz = (fk - gk * 2) * 0.5;
		for (int q = 0; q < 4; q++)
		{
			int qx = q & 1;
			int qz = q >> 1;
			float w = 1 - tx;
			if (qx == 1)
				w = tx;
			if (qz == 1)
				w = w * tz;
			else
				w = w * (1 - tz);
			if (w <= 0)
				continue;
			MUD_VoxelColumn col = ColAt(gi + qx, gk + qz);
			if (!col)
				return false;
			float h = 0;
			if (!SurfaceY(col, gi + qx - col.m_iBi * SUB, gk + qz - col.m_iBk * SUB, nearY, h))
				return false;
			y += w * h;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS: after the op, the floor samples round each sample column its Put moved go (a rim or a fill
	//! covers the rut; where the op dug deeper the field is under the floor anyway). Put's MarkBox queues them.
	protected void ClearFloor()
	{
		for (int n = 0; n < m_aClrI.Count(); n++)
		{
			int fi0 = m_aClrI[n] * 2;
			int fk0 = m_aClrK[n] * 2;
			for (int fk = fk0 - 1; fk <= fk0 + 1; fk++)
			{
				for (int fi = fi0 - 1; fi <= fi0 + 1; fi++)
				{
					int bi = FloorDiv(fi, FSUB);
					int bk = FloorDiv(fk, FSUB);
					MUD_RutTile tile = m_mRutTiles.Get(ColKey(bi, bk));
					if (tile)
					{
						int cn = (fk - bk * FSUB) * FSUB + fi - bi * FSUB;
						if (tile.m_aFloor[cn] < FLOOR_NONE * 0.5)
						{
							FloorChanged(fi, fk);
							m_iDispOpC++;
							m_iDispHash = (m_iDispHash + (7919 * (cn + 1) + ColKey(bi, bk)) % DISP_HASH_M) % DISP_HASH_M;
						}
						tile.m_aFloor[cn] = FLOOR_NONE;
						if (tile.m_aRidge[cn] < FLOOR_NONE * 0.5)
						{
							FloorChanged(fi, fk);     // Build 3 Stage 3: the ridge goes with the floor
							m_iDispOpC++;
							RidgeGone(tile, cn, ColKey(bi, bk));
						}
					}
				}
			}
		}
		m_aClrI.Clear();
		m_aClrK.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Top surface of sample column li, lk of col within RUT_ON + CELL of nearY: the highest solid -> air crossing.
	//! False = none there.
	protected bool SurfaceY(MUD_VoxelColumn col, int li, int lk, float nearY, out float y)
	{
		y = 0;
		int top = Math.Ceil((nearY + RUT_ON + CELL) / CELL);
		int bot = Math.Floor((nearY - RUT_ON - CELL) / CELL);
		float above = Value(col, li, lk, top);
		for (int gj = top - 1; gj >= bot; gj--)
		{
			float v = Value(col, li, lk, gj);
			if (v > 0 && above <= 0)
			{
				y = gj * CELL + v / (v - above) * CELL;
				return true;
			}
			above = v;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! U1: the deepest a tunnel / C4 sphere may dig in a column with undug ground undug: TUNNEL_FLOOR under it, never
	//! under the sea guard (ground near the sea: shallower, or nothing)
	protected float DeepFloor(float undug)
	{
		return Math.Max(undug - TUNNEL_FLOOR, m_fSeaY);
	}

	//------------------------------------------------------------------------------------------------
	//! Tunnel dig: vertical capsule (segment mid +- TUNNEL_HALF, radius m_fDepth), never under DeepFloor (U1)
	protected void CarveTunnel(MUD_VoxelOp op)
	{
		vector mid = op.m_vWp;
		float r = op.m_fDepth;
		float h = TUNNEL_HALF;
		int gi0 = Math.Floor((mid[0] - r - m_fX0) / CELL);
		int gi1 = Math.Ceil((mid[0] + r - m_fX0) / CELL);
		int gk0 = Math.Floor((mid[2] - r - m_fZ0) / CELL);
		int gk1 = Math.Ceil((mid[2] + r - m_fZ0) / CELL);
		int gj0 = Math.Floor((mid[1] - h - r) / CELL);
		int gj1 = Math.Ceil((mid[1] + h + r) / CELL);
		for (int gk = gk0; gk <= gk1; gk++)
		{
			for (int gi = gi0; gi <= gi1; gi++)
			{
				MUD_VoxelColumn col = ColAt(gi, gk);
				if (!col)
					continue;
				int li = gi - col.m_iBi * SUB;
				int lk = gk - col.m_iBk * SUB;
				int n = lk * SUB + li;
				if (col.m_aHard[n] != 0)
					continue;
				float dx = m_fX0 + gi * CELL - mid[0];
				float dz = m_fZ0 + gk * CELL - mid[2];
				float floorY = DeepFloor(col.m_aUndug[n]);
				for (int gj = gj0; gj <= gj1; gj++)
				{
					float y = gj * CELL;
					float dy = y - mid[1];
					dy = dy - Math.Clamp(dy, -h, h);
					float s = Math.Sqrt(dx * dx + dy * dy + dz * dz) - r;
					float old = Value(col, li, lk, gj);
					float d = old;
					if (s < d)
						d = s;
					d = Math.Max(d, Math.Min(old, floorY - y));     // U1: no NEW air below the floor (older air stays)
					Put(col, li, lk, gi, gj, gk, old, Nudge(d));
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tunnel darkness (client / host, after CarveTunnel): one MudTunnelProbe per TUNNEL_DARK_STEP grid cell the capsule
	//! touches, centred on the cell (user shots 2026-10-17: one probe at the FIRST dig of a cell left later, lower digs'
	//! floors under the 3 m box = outdoor ambient = day-bright pocket; log 19:09 run: 7 of 56 roofed digs). Box 5 x 3 x 5
	//! on a 2 m grid: every capsule point lies in a probed cell. BoxRoofed reads the field, so the open entry gets none.
	protected void TunnelDark(vector mid)
	{
		float reach = TUNNEL_HALF + TUNNEL_R;
		int di0 = Math.Round((mid[0] - TUNNEL_R) / TUNNEL_DARK_STEP);
		int di1 = Math.Round((mid[0] + TUNNEL_R) / TUNNEL_DARK_STEP);
		int dj0 = Math.Round((mid[1] - reach) / TUNNEL_DARK_STEP_Y);
		int dj1 = Math.Round((mid[1] + reach) / TUNNEL_DARK_STEP_Y);
		int dk0 = Math.Round((mid[2] - TUNNEL_R) / TUNNEL_DARK_STEP);
		int dk1 = Math.Round((mid[2] + TUNNEL_R) / TUNNEL_DARK_STEP);
		for (int di = di0; di <= di1; di++)
		{
			for (int dj = dj0; dj <= dj1; dj++)
			{
				for (int dk = dk0; dk <= dk1; dk++)
					TunnelDarkCell(mid, di, dj, dk);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One probe at the centre of grid cell (di, dj, dk), once; a cell not yet roofed is not marked (a later dig retries).
	//! Depth level from the dig's column at the cell's height: one level per metre, TUNNEL_DARK_D1 .. level 4 (darkest)
	//! from 4 m (user 2026-10-17: 'total darkness below 4 m'; path-distance levels v157 dropped).
	protected void TunnelDarkCell(vector mid, int di, int dj, int dk)
	{
		string key = string.Format("%1 %2 %3", di, dj, dk);
		if (m_mDark.Contains(key))
			return;
		vector c = Vector(di * TUNNEL_DARK_STEP, dj * TUNNEL_DARK_STEP_Y, dk * TUNNEL_DARK_STEP);
		float minTop;
		if (!BoxRoofed(c, minTop))
			return;
		m_mDark.Insert(key, 1);
		float depth = TunnelDepth(Vector(mid[0], c[1], mid[2]));
		int level = Math.Floor((depth - TUNNEL_DARK_D1) / TUNNEL_DARK_BAND) + 1;
		level = Math.ClampInt(level, 0, 4);
		ResourceName prefab = TunnelProbeLevel(level);
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("MUD tunnel dark: prefab not loaded " + prefab, LogLevel.WARNING);
			return;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = c;
		IEntity ent = GetGame().SpawnEntityPrefabLocal(res, GetGame().GetWorld(), params);
		Print(string.Format("MUD tunnel dark: probe %1 at %2 depth %3 level %4 (%5 cells)", ent != null, c, depth, level, m_mDark.Count()), LogLevel.NORMAL);
		if (TUNNEL_LID)
			TunnelLid(Vector(c[0], minTop - TUNNEL_LID_SINK, c[2]));
	}

	//------------------------------------------------------------------------------------------------
	//! Sun-shadow lid probe: one up-facing MAT_DIRT square, TUNNEL_LID_HALF each way round org (winding as Tri: front =
	//! Cross(b - a, c - a) = up)
	protected void TunnelLid(vector org)
	{
		int numVerts[] = {4};
		int numIndices[] = {6};
		string materials[] = {MAT_DIRT};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			Print("MUD tunnel lid: MeshObject.Create failed", LogLevel.ERROR);
			return;
		}
		// MeshObject takes static arrays (array<T> = compile error)
		float h = TUNNEL_LID_HALF;
		vector verts[4];
		verts[0] = Vector(-h, 0, -h);
		verts[1] = Vector(h, 0, -h);
		verts[2] = Vector(-h, 0, h);
		verts[3] = Vector(h, 0, h);
		float uvs[] = {0, 0, 1, 0, 0, 1, 1, 1};
		int idx[] = {0, 2, 1, 1, 2, 3};
		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals from the triangle list
		mesh.UpdateIndices(0, idx);
		mesh.UpdateVerts(0, verts, uvs);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = org;
		IEntity ent = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		if (!ent)
			return;
		ent.SetObject(mesh, "");
		ent.Update();       // fresh bounds, else culling keeps the spawn-time ones
		Print(string.Format("MUD vcflags: lid %1 (8 = NOSHADOW)", ent.SetVComponentFlags(0)), LogLevel.NORMAL);
		m_aLidRes.Insert(res);
		Print(string.Format("MUD tunnel lid: at %1 (%2 lids)", org, m_aLidRes.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Tunnel darkness: the probe box round mid is all under ground - the ground top (voxel column top, else our
	//! heightfield, trenches included) at 9 points TUNNEL_DARK_HALF apart is TUNNEL_DARK_ROOF or more above mid;
	//! minTop = the lowest of them
	protected bool BoxRoofed(vector mid, out float minTop)
	{
		minTop = 1000000;
		for (int a = -1; a <= 1; a++)
		{
			for (int c = -1; c <= 1; c++)
			{
				float x = mid[0] + a * TUNNEL_DARK_HALF;
				float z = mid[2] + c * TUNNEL_DARK_HALF;
				int gi = Math.Round((x - m_fX0) / CELL);
				int gk = Math.Round((z - m_fZ0) / CELL);
				float top;
				if (!ColSurf(gi, gk, top) && !MUD_WorldGroundEntity.SurfaceY(x, z, top))
					return false;
				if (top < mid[1] + TUNNEL_DARK_ROOF)
					return false;
				minTop = Math.Min(minTop, top);
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Tunnel darkness: probe prefab for level 0 (shallow) .. 4 (darkest)
	protected static ResourceName TunnelProbeLevel(int level)
	{
		if (level == 1)
			return TUNNEL_PROBE_1;
		if (level == 2)
			return TUNNEL_PROBE_2;
		if (level == 3)
			return TUNNEL_PROBE_3;
		if (level == 4)
			return TUNNEL_PROBE_4;
		return TUNNEL_PROBE;
	}

	//------------------------------------------------------------------------------------------------
	//! Tunnel darkness: feet depth (capsule middle - TUNNEL_MID) under the undug ground of the nearest column (0: none)
	protected float TunnelDepth(vector mid)
	{
		int gi = Math.Round((mid[0] - m_fX0) / CELL);
		int gk = Math.Round((mid[2] - m_fZ0) / CELL);
		MUD_VoxelColumn col = ColAt(gi, gk);
		if (!col)
			return 0;
		int n = (gk - col.m_iBk * SUB) * SUB + gi - col.m_iBi * SUB;
		return col.m_aUndug[n] - (mid[1] - TUNNEL_MID);
	}

	//------------------------------------------------------------------------------------------------
	//! Global sample columns inside the op's box (gi0..gi1, gk0..gk1)
	protected void BoxCols(MUD_VoxelOp op, out int gi0, out int gi1, out int gk0, out int gk1)
	{
		gi0 = Math.Ceil((op.m_vLo[0] - m_fX0) / CELL);
		gi1 = Math.Floor((op.m_vHi[0] - m_fX0) / CELL);
		gk0 = Math.Ceil((op.m_vLo[2] - m_fZ0) / CELL);
		gk1 = Math.Floor((op.m_vHi[2] - m_fZ0) / CELL);
	}

	//------------------------------------------------------------------------------------------------
	//! Op 2: trench cut, as MUD_WorldGroundEntity.DoCarveTrench per piece (piece -1: every piece, the free dig
	//! square): owned columns go to layers x LAYER under the undug ground, columns within the sloped wall's run
	//! outside the piece rise along it, another piece's columns are left for its own swings. Only lowers; hard
	//! columns and FLOOR as Carve.
	protected void CutTrench(MUD_VoxelOp op)
	{
		MUD_TrenchLayout lay = op.m_Lay;
		float depth = op.m_fDepth * MUD_TrenchPlan.LAYER;
		float run = depth * MUD_TrenchPlan.SLOPE_RUN;
		int gi0;
		int gi1;
		int gk0;
		int gk1;
		BoxCols(op, gi0, gi1, gk0, gk1);
		for (int gk = gk0; gk <= gk1; gk++)
		{
			for (int gi = gi0; gi <= gi1; gi++)
			{
				MUD_VoxelColumn col = ColAt(gi, gk);
				if (!col)
					continue;
				int li = gi - col.m_iBi * SUB;
				int lk = gk - col.m_iBk * SUB;
				int n = lk * SUB + li;
				if (col.m_aHard[n] != 0)
					continue;
				float x = m_fX0 + gi * CELL;
				float z = m_fZ0 + gk * CELL;
				int owner = lay.PieceAt(x, z);
				// the deepest want any piece of the op gives this column (one piece unless the free dig square)
				float want = -1;
				for (int p = 0; p < lay.Count(); p++)
				{
					if (op.m_iPiece >= 0 && p != op.m_iPiece)
						continue;
					float w = depth;
					if (owner != p)
					{
						if (owner >= 0)
							continue;   // another piece's column: its own swings
						float od = Math.Max(0, lay.OutsideDist(p, x, z) - TRENCH_PAD);     // T1: within the pad = floor
						if (od >= run)
							continue;
						w = depth - od / MUD_TrenchPlan.SLOPE_RUN;
					}
					if (w > want)
						want = w;
				}
				if (want < 0)
				{
					if (PARAPET && op.m_iPiece >= 0)
						Parapet(op, col, li, lk, gi, gk, x, z, depth);
					continue;
				}
				float g = col.m_aUndug[n];
				float bottom = Math.Max(g - want, g - FLOOR);
				int jb = Math.Floor(bottom / CELL) - 1;
				int jt = Math.Ceil(Math.Max(g + 2, col.m_aTop[n] + 1) / CELL);
				for (int gj = jb; gj <= jt; gj++)
				{
					float old = Value(col, li, lk, gj);
					float s = bottom - gj * CELL;
					if (s < old)
						Put(col, li, lk, gi, gj, gk, old, Nudge(s));
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 6: side distance (MUD_TrenchLayout.SideDist) where the parapet starts: past the pad, the sloped wall of
	//! the plan's full depth and PARA_BERM - later swings of the plan never cut it
	protected static float ParapetStart(MUD_TrenchLayout lay)
	{
		return TRENCH_PAD + lay.Layers() * MUD_TrenchPlan.LAYER * MUD_TrenchPlan.SLOPE_RUN + PARA_BERM;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 6 (op 2, piece >= 0): raises sample column li, lk of col (world x, z) into the spoil parapet beside the
	//! op's piece - both sides, nothing past the chain's ends; the nearest piece builds it (none where another
	//! piece's shape or wall is nearer). Height from depth (this swing, m): grows swing by swing. Only raises, and
	//! only a column no op dug (density only, no rut floor - J2, as FillTrench).
	protected void Parapet(MUD_VoxelOp op, MUD_VoxelColumn col, int li, int lk, int gi, int gk, float x, float z, float depth)
	{
		MUD_TrenchLayout lay = op.m_Lay;
		float sd = lay.SideDist(op.m_iPiece, x, z);
		if (sd < 0)
			return;
		float u = sd - ParapetStart(lay);
		float width = PARA_RISE * 2 + PARA_TOP;
		if (u <= 0 || u >= width)
			return;
		for (int p = 0; p < lay.Count(); p++)
		{
			if (p != op.m_iPiece && lay.OutsideDist(p, x, z) < sd - 0.001)
				return;
		}
		float t = 1;
		if (u < PARA_RISE)
			t = u / PARA_RISE;
		else if (u > PARA_RISE + PARA_TOP)
			t = (width - u) / PARA_RISE;
		float h = Math.Min(PARA_MAX, depth * PARA_K) * t * t * (3 - 2 * t);
		if (h < 0.005)
			return;
		if (ColumnDepth(col, li, lk, false) > 0.01)
			return;     // dug (crater, rut cut, another hole): leave it
		int n = lk * SUB + li;
		float g = col.m_aUndug[n];
		float target = g + h;
		int jb = Math.Floor((g - FLOOR) / CELL) - 1;
		int jt = Math.Ceil(target / CELL) + 1;
		for (int gj = jb; gj <= jt; gj++)
		{
			float old = Value(col, li, lk, gj);
			float s = target - gj * CELL;
			if (s > old + 0.0005)
				Put(col, li, lk, gi, gj, gk, old, Nudge(s));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Op 3: fill one layer, as MUD_WorldGroundEntity.DoFreeFill: columns of the square come up to the new floor
	//! (m_fB under the undug ground; 0 = back to the undug top). Around it, a column no deeper than this hole's own
	//! sloped wall gets the shallower hole's wall; a deeper one (a neighbour hole) a shoulder down from the new floor
	//! edge. Only raises. The hole depth came from the server (m_fDepth), the column depths are each column's own.
	protected void FillTrench(MUD_VoxelOp op)
	{
		MUD_TrenchLayout lay = op.m_Lay;
		float holeDepth = op.m_fDepth;
		float newDepth = op.m_fB;
		float margin = MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER * MUD_TrenchPlan.SLOPE_RUN;
		int gi0;
		int gi1;
		int gk0;
		int gk1;
		BoxCols(op, gi0, gi1, gk0, gk1);
		for (int gk = gk0; gk <= gk1; gk++)
		{
			for (int gi = gi0; gi <= gi1; gi++)
			{
				MUD_VoxelColumn col = ColAt(gi, gk);
				if (!col)
					continue;
				int li = gi - col.m_iBi * SUB;
				int lk = gk - col.m_iBk * SUB;
				int n = lk * SUB + li;
				if (col.m_aHard[n] != 0)
					continue;
				float x = m_fX0 + gi * CELL;
				float z = m_fZ0 + gk * CELL;
				float g = col.m_aUndug[n];
				float fillTo = newDepth;
				if (lay.PieceAt(x, z) < 0)
				{
					float od = margin;
					for (int p = 0; p < lay.Count(); p++)
						od = Math.Min(od, Math.Max(0, lay.OutsideDist(p, x, z) - TRENCH_PAD));     // T1: as CutTrench
					if (od >= margin)
						continue;
					float wall = Math.Max(0, holeDepth - od / MUD_TrenchPlan.SLOPE_RUN);
					if (ColumnDepth(col, li, lk, false) > wall + 0.01)
						fillTo = newDepth + od / MUD_TrenchPlan.SLOPE_RUN;
					else
						fillTo = Math.Max(0, newDepth - od / MUD_TrenchPlan.SLOPE_RUN);
				}
				float target = g - fillTo;
				if (fillTo <= 0.001)
					target = g + LIFT;     // back to the undug top
				int jb = Math.Floor((g - FLOOR) / CELL) - 1;
				int jt = Math.Ceil(target / CELL) + 1;
				for (int gj = jb; gj <= jt; gj++)
				{
					float old = Value(col, li, lk, gj);
					float s = target - gj * CELL;
					if (s > old + 0.0005)
						Put(col, li, lk, gi, gj, gk, old, Nudge(s));
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! How far sample column li, lk of col lies under its undug ground now: the top crossing from solid to air (a cave
	//! under it does not count). Negative = above (LIFT, a rim, a berm). withFloor false = density only, no rut floor
	//! (FillTrench: a joiner replays fills before the rut snapshot exists, so the fill must not read it - J2)
	protected float ColumnDepth(MUD_VoxelColumn col, int li, int lk, bool withFloor)
	{
		int n = lk * SUB + li;
		int rTop = -1000000;
		int rBot = 1000000;
		foreach (int bj, MUD_VoxelBrick b : col.m_mRows)
		{
			if (bj > rTop)
				rTop = bj;
			if (bj < rBot)
				rBot = bj;
		}
		if (rTop < rBot)
		{
			if (!withFloor)
				return col.m_aUndug[n] - col.m_aTop[n];     // no rows: the seed
			return col.m_aUndug[n] - FloorMin(col, li, lk, col.m_aTop[n]);
		}
		float y = rBot * BRICK;
		for (int gj = rTop * SUB + SUB; gj >= rBot * SUB; gj--)
		{
			float d = Value(col, li, lk, gj);
			if (d <= 0)
				continue;
			y = gj * CELL;
			float du = Value(col, li, lk, gj + 1);
			if (du <= 0)
				y += CELL * d / (d - du);
			break;
		}
		if (!withFloor)
			return col.m_aUndug[n] - y;
		return col.m_aUndug[n] - FloorMin(col, li, lk, y);
	}

	//------------------------------------------------------------------------------------------------
	//! Column x, z is on or next to kept terrain (a point 0.01 m off it in a cell that is no hole): never dug
	protected bool Hard(float x, float z)
	{
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!skin)
			return false;
		for (int q = 0; q < 4; q++)
		{
			float ox = -0.01;
			float oz = -0.01;
			if ((q & 1) != 0)
				ox = 0.01;
			if ((q & 2) != 0)
				oz = 0.01;
			int cx = Math.Floor((x + ox - m_fX0) / m_fCell);
			int cz = Math.Floor((z + oz - m_fZ0) / m_fCell);
			if (!skin.IsHoleCell(cx, cz))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Column x, z has no skin hole cell within 0.5 m (x and z): kept terrain carries it. 4 corners of the +-0.49
	//! square: a 2 m cell meeting that square holds one of them.
	protected bool Kept(float x, float z)
	{
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!skin)
			return false;
		for (int q = 0; q < 4; q++)
		{
			float ox = -0.49;
			float oz = -0.49;
			if ((q & 1) != 0)
				ox = 0.49;
			if ((q & 2) != 0)
				oz = 0.49;
			int cx = Math.Floor((x + ox - m_fX0) / m_fCell);
			int cz = Math.Floor((z + oz - m_fZ0) / m_fCell);
			if (skin.IsHoleCell(cx, cz))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the world sample columns the layout's pieces own (piece -1: any piece) that can be dug (not hard),
	//! with the depth each has now (a column no op converted: its heightfield depth). ready: always true.
	protected int TrenchCols(MUD_TrenchLayout lay, int piece, out float fewest, out float deepest, out bool ready)
	{
		fewest = 100;
		deepest = -100;
		ready = true;
		int n;
		for (int p = 0; p < lay.Count(); p++)
		{
			if (piece >= 0 && p != piece)
				continue;
			vector lo;
			vector hi;
			lay.PieceBox(p, lo, hi);
			int x0 = Math.Ceil((lo[0] - m_fX0) / CELL);
			int x1 = Math.Floor((hi[0] - m_fX0) / CELL);
			int z0 = Math.Ceil((lo[2] - m_fZ0) / CELL);
			int z1 = Math.Floor((hi[2] - m_fZ0) / CELL);
			for (int gk = z0; gk <= z1; gk++)
			{
				for (int gi = x0; gi <= x1; gi++)
				{
					float x = m_fX0 + gi * CELL;
					float z = m_fZ0 + gk * CELL;
					if (lay.PieceAt(x, z) != p || Hard(x, z))
						continue;
					n++;
					float dep = 0;
					MUD_VoxelColumn col = ColAt(gi, gk);
					// ONE_GRID: mud zone ground is the sheet's (its trench / shovel ops never reach the voxels)
					bool onSheet = MUD_MudSheet.SheetDepth(x, z, dep);
					if (!onSheet && col)
					{
						dep = ColumnDepth(col, gi - col.m_iBi * SUB, gk - col.m_iBk * SUB, true);
					}
					else if (!onSheet)
					{
						// not converted: the heightfield's depth (the seed carries it into the voxels)
						float hd;
						float hu;
						if (MUD_WorldGroundEntity.DugUndug(x, z, hd, hu))
							dep = hu - hd;
					}
					if (dep < fewest)
						fewest = dep;
					if (dep > deepest)
						deepest = dep;
				}
			}
		}
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! Server (MUD_WorldGroundEntity.CarveTrench): trench site piece swing to layers. -2 = no column of the piece can
	//! be dug (all on kept terrain), REFUSED = the heightfield digs it, else the diggable columns.
	static int Trench(MUD_TrenchLayout lay, int piece, int layers)
	{
		if (!Replication.IsServer() || !lay || lay.m_sPlan.IsEmpty() || !Ensure())
			return REFUSED;
		float fewest;
		float deepest;
		bool ready;
		int n = s_Inst.TrenchCols(lay, piece, fewest, deepest, ready);
		if (n == 0)
			return -2;
		MUD_VoxelOp op = new MUD_VoxelOp();
		op.m_iKind = 2;
		op.m_iPiece = piece;
		op.m_fDepth = layers;
		op.m_sPlan = lay.m_sPlan;
		if (!s_Inst.Issue(op))
			return REFUSED;
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! Server (MUD_WorldGroundEntity.FreeDig): the square one layer under its shallowest column. -2 = nothing
	//! diggable, -3 = already MAX_LAYERS deep, REFUSED = the heightfield digs it, else columns.
	static int FreeDig(MUD_TrenchLayout lay)
	{
		if (!Replication.IsServer() || !lay || lay.m_sPlan.IsEmpty() || !Ensure())
			return REFUSED;
		float fewest;
		float deepest;
		bool ready;
		int n = s_Inst.TrenchCols(lay, -1, fewest, deepest, ready);
		if (n == 0)
			return -2;
		int has = Math.Floor(fewest / MUD_TrenchPlan.LAYER + 0.01);
		if (has >= MUD_TrenchPlan.MAX_LAYERS)
			return -3;
		if (has < 0)
			has = 0;
		MUD_VoxelOp op = new MUD_VoxelOp();
		op.m_iKind = 2;
		op.m_iPiece = -1;
		op.m_fDepth = has + 1;
		op.m_sPlan = lay.m_sPlan;
		if (!s_Inst.Issue(op))
			return REFUSED;
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! Server (MUD_WorldGroundEntity.FreeFill): the square one layer up. -2 = nothing dug there, REFUSED = the
	//! heightfield fills it, else columns.
	static int FreeFill(MUD_TrenchLayout lay)
	{
		if (!Replication.IsServer() || !lay || lay.m_sPlan.IsEmpty() || !Ensure())
			return REFUSED;
		float fewest;
		float deepest;
		bool ready;
		int n = s_Inst.TrenchCols(lay, -1, fewest, deepest, ready);
		if (n == 0)
			return -2;
		if (deepest < 0.002)
			return -2;
		int left = Math.Ceil(deepest / MUD_TrenchPlan.LAYER - 0.01) - 1;
		if (left < 0)
			left = 0;
		MUD_VoxelOp op = new MUD_VoxelOp();
		op.m_iKind = 3;
		op.m_iPiece = -1;
		op.m_fDepth = deepest;
		op.m_fB = left * MUD_TrenchPlan.LAYER;
		op.m_sPlan = lay.m_sPlan;
		if (!s_Inst.Issue(op))
			return REFUSED;
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds queued bricks until the frame has used m_iUrgent (at least one); once none is queued, the batch goes
	//! live in one frame (GoLive)
	protected void Step()
	{
		int t0 = System.GetTickCount();
		JoinBudgets();
		int built;
		while (m_iQHead < m_aQueue.Count())
		{
			MUD_VoxelBrick b = m_aQueue[m_iQHead];
			m_iQHead++;
			if (!b || !b.m_bQueued)
				continue;
			b.m_bQueued = false;
			BuildSub(b);
			built++;
			if (System.GetTickCount(t0) >= m_iUrgent)
				break;
		}
		if (m_iQHead >= m_aQueue.Count())
		{
			m_aQueue.Clear();
			m_iQHead = 0;
		}
		if (built > 0)
		{
			int ms = System.GetTickCount(t0);
			m_iBatchMs += ms;
			m_iBatchFrames++;
			if (ms > m_iBatchWorst)
				m_iBatchWorst = ms;
		}
		if (m_bOpen && m_aQueue.IsEmpty())
			GoLive();
	}

	//------------------------------------------------------------------------------------------------
	//! JOIN_MS budget while this client joins (Joining), the normal one after. Logs both edges.
	protected void JoinBudgets()
	{
		bool join = Joining();
		if (join != m_bJoinBoost)
		{
			m_bJoinBoost = join;
			if (join)
			{
				m_iJoinT0 = System.GetTickCount();
				Print(string.Format("MUD voxel probe: joining - build / drain budgets %1 ms until a controlled entity, %2 columns, %3 bricks, %4 ops in, next seq %5, %6 held",
					JOIN_MS, m_mCols.Count(), m_iBricks, m_aOps.Count(), s_iNextSeq, s_mPending.Count()), LogLevel.NORMAL);
			}
			else
			{
				Print(string.Format("MUD voxel probe: joined after %1 ms - %2 columns, %3 bricks, %4 still queued, %5 ops in, %6 held, mem %7 KB",
					System.GetTickCount(m_iJoinT0), m_mCols.Count(), m_iBricks, m_aQueue.Count() - m_iQHead, m_aOps.Count(), s_mPending.Count(), System.MemoryAllocationKB()), LogLevel.NORMAL);
			}
		}
		if (join)
			m_iUrgent = JOIN_MS;
		else
			m_iUrgent = URGENT_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! Everything since the last go-live is built: in one frame the new bricks show, the rebuilt shown ones swap,
	//! and the hole cells the new columns carry go VOX (skin + fine patches)
	protected void GoLive()
	{
		int t0 = System.GetTickCount();
		int shown;
		foreach (MUD_VoxelBrick b : m_aShow)
		{
			if (!b || b.m_bShown)
				continue;
			b.m_bShown = true;
			if (b.m_Ent)
			{
				b.m_Ent.SetFlags(EntityFlags.VISIBLE, false);
				shown++;
			}
		}
		m_aShow.Clear();
		int swapped = SwapPending();
		int voxMs;
		int cells = UpdateVox(voxMs);
		UpdateLook();
		m_bOpen = false;
		m_mSurf.Clear();
		for (int mI = 0; mI < m_aMudLo.Count(); mI++)
			MUD_MudSheet.GroundChanged(m_aMudLo[mI], m_aMudHi[mI], m_aMudWp[mI], m_aMudR[mI]);
		MUD_MudSheet.FlushNow();     // DIG NOW: all this go-live's digs in one pass
		// Build 2 Stage 5: things on the ground follow (bricks + bodies are live now; m_mSurf cleared above)
		for (int fI = 0; fI < m_aMudLo.Count(); fI++)
			MUD_GroundFollow.Changed(m_aMudLo[fI], m_aMudHi[fI], m_aMudKind[fI], m_aMudLift[fI]);
		if (CRATER_DECAL && !m_bHeadless)
		{
			for (int dI = 0; dI < m_aMudLo.Count(); dI++)
			{
				if (m_aMudKind[dI] == 0)
					CraterDecal(m_aMudWp[dI], m_aMudR[dI]);
			}
		}
		m_aMudLo.Clear();
		m_aMudHi.Clear();
		m_aMudWp.Clear();
		m_aMudR.Clear();
		m_aMudKind.Clear();
		m_aMudLift.Clear();
		m_iDispSplit += m_iBatchSplit;
		m_iDispSplitV += m_iBatchSplitV;
		m_iDispFold += m_iBatchFold;
		m_iDispOvf += m_iBatchDispOvf;
		if (OP_PRINTS || m_iBatchMade > 0)     // rut-only batches: quiet (OP_PRINTS)
		Print(string.Format("MUD voxel probe: live %1 ms after seq %2 - %3 ops, %4 bricks made, %5 new meshes shown, %6 rebuilt swapped, %7 skin cells to voxel (%8 ms)",
			System.GetTickCount(m_iBatchT0), m_iBatchFirstSeq, m_iBatchOps, m_iBatchMade, shown, swapped, cells, voxMs)
			+ string.Format(" | %1 builds, %2 verts, build %3 ms over %4 frames (worst frame %5 ms, worst brick %6 ms), go-live frame %7 ms",
			m_iBatchBuilt, m_iBatchVerts, m_iBatchMs, m_iBatchFrames, m_iBatchWorst, m_iBatchWorstSub, System.GetTickCount(t0))
			+ string.Format(" | %1 columns, %2 bricks, mem %3 KB, %4 look meshes", m_mCols.Count(), m_iBricks, System.MemoryAllocationKB(), m_iBatchLook)
			+ string.Format(" (%1 kept their body, %2 look-only builds skipped)", m_iBatchMeshOnly, m_iBatchSkipped)
			+ string.Format(" | scan: gather %1 ms, poly %2 ms, %3 samples scanned", m_iBatchGatherMs, m_iBatchPolyMs,
			m_iBatchScanned) + DispBatch(), LogLevel.NORMAL);
		if (MUD_TestKit.TEST_KIT) MUD_CraterTrace.GoLive(m_iBatchFrames, m_iBatchWorst, System.GetTickCount(t0), voxMs);     // stage 2 B1 crater log
	}

	//------------------------------------------------------------------------------------------------
	//! Crater decal probe run 2 (GoLive, client / host): the vanilla mortar decal on every shown brick entity of the
	//! columns within crest of the crater middle, bricks from 4 m under the crest to 4 m over it, projected down
	protected void CraterDecal(vector wp, float crest)
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world || crest <= 0)
			return;
		float angle = Math.RandomFloat(0, 360);
		int bi0 = Math.Floor((wp[0] - crest - m_fX0) / BRICK);
		int bi1 = Math.Floor((wp[0] + crest - m_fX0) / BRICK);
		int bk0 = Math.Floor((wp[2] - crest - m_fZ0) / BRICK);
		int bk1 = Math.Floor((wp[2] + crest - m_fZ0) / BRICK);
		int bj0 = Math.Floor((wp[1] - crest - 4) / BRICK);
		int bj1 = Math.Floor((wp[1] + 4) / BRICK);
		int tried;
		int made;
		for (int bk = bk0; bk <= bk1; bk++)
		{
			for (int bi = bi0; bi <= bi1; bi++)
			{
				MUD_VoxelColumn col = m_mCols.Get(ColKey(bi, bk));
				if (!col)
					continue;
				for (int bj = bj0; bj <= bj1; bj++)
				{
					MUD_VoxelBrick b = col.m_mRows.Get(bj);
					if (!b || !b.m_Ent || !b.m_Ent.GetVObject())
						continue;
					tried++;
					Decal d = world.CreateDecal(b.m_Ent, wp + Vector(0, 2, 0), "0 -1 0", 0, crest + 6, angle, crest * DECAL_SIZE, 1, DECAL_MAT, DECAL_LIFE, 0xFFFFFFFF);
					if (d)
						made++;
				}
			}
		}
		Print(string.Format("MUD decal: crater %1 crest %2 - bricks with a mesh %3, decals made %4", wp, crest.ToString(-1, 2), tried, made), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! GoLive: the batch line's displaced-ruts part (empty with DISPLACE_RUTS off: the v035 line)
	protected string DispBatch()
	{
		if (!DISPLACE_RUTS)
			return "";
		return string.Format(" | disp: %1 split bricks, %2 split verts, %3 fold drops, %4 overflows, %5 soil walls", m_iBatchSplit,
			m_iBatchSplitV, m_iBatchFold, m_iBatchDispOvf, m_iBatchSoil);
	}

#ifdef ENABLE_DIAG
	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS debug (Stage B): diag menu GameCode > Mud > Displaced ruts, all off (once per game)
	protected static void DispMenu()
	{
		if (s_bDispMenu)
			return;
		if (SCR_DebugMenuID.MUD_DISP_HASH >= 512)
		{
			Print(string.Format("MUD disp: diag ids reach %1 (max 511) - no Displaced ruts menu", SCR_DebugMenuID.MUD_DISP_HASH), LogLevel.ERROR);
			return;
		}
		s_bDispMenu = true;
		DiagMenu.RegisterMenu(SCR_DebugMenuID.MUD_DISP_MENU_MUD, "Mud", "GameCode");
		DiagMenu.RegisterMenu(SCR_DebugMenuID.MUD_DISP_MENU, "Displaced ruts", "Mud");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_DISP_LOG, "", "Summary log (10 s)", "Displaced ruts");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_DISP_DRAW_FLOOR, "", "Draw floor", "Displaced ruts");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_DISP_DRAW_BRICKS, "", "Draw overflow / fold bricks", "Displaced ruts");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_DISP_HOLE_CHECK, "", "Hole check", "Displaced ruts");
		DiagMenu.RegisterBool(SCR_DebugMenuID.MUD_DISP_HASH, "", "Floor hash log", "Displaced ruts");
		Print(string.Format("MUD disp: diag menu GameCode > Mud > Displaced ruts, ids %1 .. %2", SCR_DebugMenuID.MUD_DISP_MENU_MUD, SCR_DebugMenuID.MUD_DISP_HASH), LogLevel.NORMAL);
		MUD_RutDebug.Menu();     // Mud > Ruts and wheels
	}
#endif

	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS debug switch n (DSW_*): the diag menu, else off (retail / DS; the *_ALWAYS consts)
	protected static bool DispOn(int n)
	{
		if (n == DSW_LOG && DISP_LOG_ALWAYS)
			return true;
		if (n == DSW_HASH && (DISP_HASH_ALWAYS || (MUD_TestKit.TEST_KIT && HASH_LOG)))
			return true;
#ifdef ENABLE_DIAG
		if (!s_bDispMenu)
			return false;
		return DiagMenu.GetBool(SCR_DebugMenuID.MUD_DISP_LOG + n);
#else
		return false;
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Displace: its time into the summary counters
	protected void DispTime(int t0)
	{
		int ms = System.GetTickCount(t0);
		m_iDispMs += ms;
		if (ms > m_iDispWorst)
			m_iDispWorst = ms;
	}

	//------------------------------------------------------------------------------------------------
	//! Submit, DISPLACE_RUTS: this op's floor counts into the summary, its hash line (every machine logs the same
	//! hash per seq when their floors agree)
	protected void DispOpDone(MUD_VoxelOp op)
	{
		if (op.m_iKind == 4)
			m_iDispOps++;
		m_iDispWritten += m_iDispOpW;
		m_iDispCleared += m_iDispOpC;
		if (DispOn(DSW_HASH) && (op.m_iKind == 4 || m_iDispOpC > 0))
			Print(string.Format("MUD disp hash seq %1 kind %2: %3 (%4 written, %5 cleared)", op.m_iSeq, op.m_iKind, m_iDispHash, m_iDispOpW, m_iDispOpC), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame (Tick): summary line, then (rendering machines) hole check, floor + brick + hole draws
	protected void DispTick()
	{
		int now = System.GetTickCount();
		if (now - m_iDispT0 >= DISP_LOG_EVERY_S * 1000)
		{
			if (DispOn(DSW_LOG) && m_iDispOps + m_iDispCleared + m_iDispSplit + m_iDispOvf + m_iDispHoles > 0)
				DispLog(now);
			DispReset(now);
		}
		if (m_bHeadless)
			return;
		IEntity me = SCR_PlayerController.GetLocalControlledEntity();
		if (me && DispOn(DSW_HOLE) && !m_bOpen && now - m_iDispHoleT >= DISP_HOLE_EVERY_MS)
		{
			m_iDispHoleT = now;
			DispHoles(me.GetOrigin(), now);
		}
		if (me && DISPLACE_RUTS && DispOn(DSW_FLOOR))
		{
			if (now - m_iDispFloorT >= DISP_HOLE_EVERY_MS)
			{
				m_iDispFloorT = now;
				DispFloorList(me.GetOrigin());
			}
			for (int n = 0; n < m_aDispFloorP.Count(); n++)
				Shape.CreateSphere(m_aDispFloorC[n], ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, m_aDispFloorP[n], 0.05);
		}
		DispMarks(me, now);
	}

	//------------------------------------------------------------------------------------------------
	protected void DispLog(int now)
	{
		Print(string.Format("MUD disp %1 s: %2 rut ops, %3 floor written, %4 cleared, %5 carved cells | %6 split bricks, %7 split verts, %8 fold drops, %9 overflows",
			(now - m_iDispT0) / 1000, m_iDispOps, m_iDispWritten, m_iDispCleared, m_iDispCarved, m_iDispSplit, m_iDispSplitV, m_iDispFold, m_iDispOvf)
			+ string.Format(" | %1 border pulls, displace %2 ms (worst brick %3 ms) | holes %4 in %5 checks", m_iDispBorder, m_iDispMs, m_iDispWorst,
			m_iDispHoles, m_iDispChecks)
			+ string.Format(" (plain %1 in %2, seam %3 in %4)", m_iDispPlainHoles, m_iDispPlainChecks, m_iDispSeamHoles, m_iDispSeamChecks), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void DispReset(int now)
	{
		m_iDispT0 = now;
		m_iDispOps = 0;
		m_iDispWritten = 0;
		m_iDispCleared = 0;
		m_iDispCarved = 0;
		m_iDispSplit = 0;
		m_iDispSplitV = 0;
		m_iDispFold = 0;
		m_iDispOvf = 0;
		m_iDispBorder = 0;
		m_iDispMs = 0;
		m_iDispWorst = 0;
		m_iDispHoles = 0;
		m_iDispChecks = 0;
		m_iDispPlainChecks = 0;
		m_iDispPlainHoles = 0;
		m_iDispSeamChecks = 0;
		m_iDispSeamHoles = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Draw floor: the floor samples round o, coloured by depth under the field (grey < 5 cm, yellow < 15 cm, red
	//! deeper, white = no surface found). RUT_FIELD: no voxel column there -> depth under the undug ground.
	protected void DispFloorList(vector o)
	{
		m_aDispFloorP.Clear();
		m_aDispFloorC.Clear();
		float rr = DISP_FLOOR_DRAW_R;
		int bi0 = Math.Floor((o[0] - rr - m_fX0) / BRICK);
		int bi1 = Math.Floor((o[0] + rr - m_fX0) / BRICK);
		int bk0 = Math.Floor((o[2] - rr - m_fZ0) / BRICK);
		int bk1 = Math.Floor((o[2] + rr - m_fZ0) / BRICK);
		for (int bk = bk0; bk <= bk1; bk++)
		{
			for (int bi = bi0; bi <= bi1; bi++)
			{
				MUD_RutTile tile = m_mRutTiles.Get(ColKey(bi, bk));
				if (!tile)
					continue;
				for (int n = 0; n < MUD_RutTile.SIZE_F; n++)
				{
					float fy = tile.m_aFloor[n];
					if (fy > FLOOR_NONE * 0.5)
						continue;
					int fi = bi * FSUB + n % FSUB;
					int fk = bk * FSUB + n / FSUB;
					vector p = Vector(m_fX0 + fi * FCELL, fy, m_fZ0 + fk * FCELL);
					if (vector.DistanceSqXZ(p, o) > rr * rr)
						continue;
					int c = ARGB(255, 255, 255, 255);
					float sy = 0;
					// voxel top where a column reads one; RUT_FIELD plain ground: the undug ground (CarveFloor's plain rule)
					bool known = FieldSurfAt(fi, fk, fy, sy);
					if (!known && RUT_FIELD)
						known = MUD_WorldGroundEntity.UndugY(p[0], p[2], sy);
					if (known)
					{
						float d = sy - fy;
						c = ARGB(255, 150, 150, 150);
						if (d >= 0.05)
							c = ARGB(255, 255, 230, 0);
						if (d >= 0.15)
							c = ARGB(255, 255, 30, 30);
					}
					m_aDispFloorP.Insert(p);
					m_aDispFloorC.Insert(c);
					if (m_aDispFloorP.Count() >= DISP_FLOOR_MAX)
						return;
				}
				// Build 3 Stage 3: ridge samples, cyan
				for (int rn = 0; rn < MUD_RutTile.SIZE_F; rn++)
				{
					float ry = tile.m_aRidge[rn];
					if (ry > FLOOR_NONE * 0.5)
						continue;
					int rfi = bi * FSUB + rn % FSUB;
					int rfk = bk * FSUB + rn / FSUB;
					vector rp = Vector(m_fX0 + rfi * FCELL, ry, m_fZ0 + rfk * FCELL);
					if (vector.DistanceSqXZ(rp, o) > rr * rr)
						continue;
					m_aDispFloorP.Insert(rp);
					m_aDispFloorC.Insert(ARGB(255, 0, 220, 255));
					if (m_aDispFloorP.Count() >= DISP_FLOOR_MAX)
						return;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Hole check: a 9 x 9 grid (CELL apart, on the sample columns) round o. Carved cells: expected top =
	//! min(field top, rut floor); an ENTS trace onto our bricks must hit within DISP_HOLE_TOL of it, with an upward
	//! normal. Not while a batch is open (bricks not live yet). A tunnel roof < 2 m over the ground reads as a hole.
	//! RUT_FIELD (review F3 2026-10-16) also: plain points (not carved, skin hole cell, live patch collider):
	//! expected = WorldGround dug (floor min + edge lock in it), trace onto bricks or patches; and seam points: every
	//! carved / not carved neighbour pair gets two traces 0.05 m either side of the cell line, each side with its own
	//! expected top, bricks or patches.
	protected void DispHoles(vector o, int now)
	{
		if (!m_DispTrace)
			m_DispTrace = new TraceParam();
		m_iDispLogged = 0;
		bool field = RUT_FIELD && DISPLACE_RUTS;
		float x0 = Math.Round((o[0] - m_fX0) / CELL) * CELL + m_fX0;
		float z0 = Math.Round((o[2] - m_fZ0) / CELL) * CELL + m_fZ0;
		for (int hz = -4; hz <= 4; hz++)
		{
			for (int hx = -4; hx <= 4; hx++)
			{
				float x = x0 + hx * CELL;
				float z = z0 + hz * CELL;
				bool carved = DrawsAt(x, z);
				float ey = 0;
				if (carved)
				{
					if (DispExpect(x, z, true, ey))
					{
						m_iDispChecks++;
						DispTraceCheck(x, z, ey, false, "", now);
					}
				}
				else if (field && DispExpect(x, z, false, ey))
				{
					m_iDispChecks++;
					m_iDispPlainChecks++;
					if (DispTraceCheck(x, z, ey, true, " (plain)", now))
						m_iDispPlainHoles++;
				}
				if (!field)
					continue;
				// seam: right and up neighbours (each pair once)
				for (int sn = 0; sn < 2; sn++)
				{
					if ((sn == 0 && hx == 4) || (sn == 1 && hz == 4))
						continue;
					float nx = x;
					float nz = z;
					if (sn == 0)
						nx = x + CELL;
					else
						nz = z + CELL;
					bool nCarved = DrawsAt(nx, nz);
					if (nCarved == carved)
						continue;
					// the carved-cell line between them = start of the neighbour's DrawsAt cell (SPS * CELL)
					float cellW = SPS * CELL;
					float lineX = Math.Floor((nx - m_fX0) / cellW) * cellW + m_fX0;
					float lineZ = Math.Floor((nz - m_fZ0) / cellW) * cellW + m_fZ0;
					for (int side = 0; side < 2; side++)
					{
						// 0.05 m from the cell line toward x (side 0) or the neighbour (side 1)
						float sx = x;
						float sz = z;
						float toward = -0.05;
						if (side == 1)
							toward = 0.05;
						if (sn == 0)
							sx = lineX + toward;
						else
							sz = lineZ + toward;
						bool sCarved = DrawsAt(sx, sz);
						float sy = 0;
						if (!DispExpect(sx, sz, sCarved, sy))
							continue;
						m_iDispChecks++;
						m_iDispSeamChecks++;
						if (DispTraceCheck(sx, sz, sy, true, " (seam)", now))
							m_iDispSeamHoles++;
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Hole check: expected top at world x, z. carved: the voxel field top, min the rut floor. Not carved (RUT_FIELD):
	//! only where a patch draws (skin hole cell) with a live, up-to-date collider; WorldGround dug (rut floor min and
	//! edge lock already in it). False = nothing to check there.
	protected bool DispExpect(float x, float z, bool carved, out float ey)
	{
		ey = 0;
		if (carved)
		{
			float sy = 0;
			if (!SurfaceAtI(x, z, sy))
				return false;
			ey = sy;
			if (DISPLACE_RUTS)
			{
				float fy = FloorAtW(x, z, sy);
				if (fy < ey)
					ey = fy;
			}
			return true;
		}
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!skin)
			return false;
		int cx = Math.Floor((x - m_fX0) / m_fCell);
		int cz = Math.Floor((z - m_fZ0) / m_fCell);
		if (!skin.IsHoleCell(cx, cz))
			return false;     // kept terrain draws it
		if (!MUD_WorldGroundEntity.PatchColliderLive(x, z))
			return false;
		float undug = 0;
		return MUD_WorldGroundEntity.DugUndug(x, z, ey, undug);
	}

	//------------------------------------------------------------------------------------------------
	//! Hole check: one ENTS trace down at x, z onto our bricks (patches too when withPatches). True = hole (missed,
	//! more than DISP_HOLE_TOL off ey, or a downward normal): counted, logged (DISP_HOLE_MAX_LOG per check), marked.
	protected bool DispTraceCheck(float x, float z, float ey, bool withPatches, string what, int now)
	{
		m_DispTrace.Start = Vector(x, ey + 2, z);
		m_DispTrace.End = Vector(x, ey - 3, z);
		m_DispTrace.Flags = TraceFlags.ENTS;
		m_DispTrace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_DispTrace.TraceEnt = null;
		float f;
		if (withPatches)
			f = GetGame().GetWorld().TraceMove(m_DispTrace, DispOursAll);
		else
			f = GetGame().GetWorld().TraceMove(m_DispTrace, DispOurs);
		float hy = ey + 2 - 5 * f;
		vector hn = m_DispTrace.TraceNorm;
		bool bad = f >= 1 || Math.AbsFloat(hy - ey) > DISP_HOLE_TOL || hn[1] < 0;
		if (!bad)
			return false;
		m_iDispHoles++;
		if (m_iDispLogged < DISP_HOLE_MAX_LOG)
		{
			m_iDispLogged++;
			string hit = "none";
			if (f < 1)
				hit = string.Format("%1 (normal y %2)", hy, hn[1]);
			Print(string.Format("MUD disp HOLE%1 at %2 %3: expected %4, hit %5", what, x, z, ey, hit), LogLevel.WARNING);
		}
		if (m_aDispHolePos.Count() < DISP_MARK_MAX)
		{
			m_aDispHolePos.Insert(Vector(x, ey, z));
			m_aDispHoleUntil.Insert(now + DISP_MARK_MS);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Hole check trace: our voxel bricks only
	protected bool DispOurs(notnull IEntity e)
	{
		return MUD_VoxelSubEntity.Cast(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Hole check trace (plain / seam points): our voxel bricks or heightfield patches
	protected bool DispOursAll(notnull IEntity e)
	{
		return MUD_VoxelSubEntity.Cast(e) != null || MUD_FinePatchEntity.Cast(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Hole check: rut floor at world x, z (bilinear over the floor samples, a corner with none counts as ownY;
	//! all four none = FLOOR_NONE) - as Displace's DispFloor
	protected float FloorAtW(float x, float z, float ownY)
	{
		float fx = (x - m_fX0) / FCELL;
		float fz = (z - m_fZ0) / FCELL;
		int ix = Math.Floor(fx);
		int iz = Math.Floor(fz);
		float tx = fx - ix;
		float tz = fz - iz;
		float f00 = FloorSample(ix, iz);
		float f10 = FloorSample(ix + 1, iz);
		float f01 = FloorSample(ix, iz + 1);
		float f11 = FloorSample(ix + 1, iz + 1);
		float isNone = FLOOR_NONE * 0.5;
		if (f00 > isNone && f10 > isNone && f01 > isNone && f11 > isNone)
			return FLOOR_NONE;
		if (f00 > isNone)
			f00 = ownY;
		if (f10 > isNone)
			f10 = ownY;
		if (f01 > isNone)
			f01 = ownY;
		if (f11 > isNone)
			f11 = ownY;
		return (f00 * (1 - tx) + f10 * tx) * (1 - tz) + (f01 * (1 - tx) + f11 * tx) * tz;
	}

	//------------------------------------------------------------------------------------------------
	//! Brick and hole marks: drop the old ones, draw the rest (switch on; within DISP_BRICK_DRAW_R of me if any)
	protected void DispMarks(IEntity me, int now)
	{
		bool bricksOn = DispOn(DSW_BRICKS);
		bool holesOn = DispOn(DSW_HOLE);
		bool haveMe = me != null;
		vector o = vector.Zero;
		if (haveMe)
			o = me.GetOrigin();
		float rr = DISP_BRICK_DRAW_R;
		vector size = Vector(BRICK, BRICK, BRICK);
		for (int n = m_aDispMarkLo.Count() - 1; n >= 0; n--)
		{
			if (now > m_aDispMarkUntil[n] || !bricksOn)
			{
				m_aDispMarkLo.RemoveOrdered(n);
				m_aDispMarkC.RemoveOrdered(n);
				m_aDispMarkUntil.RemoveOrdered(n);
				continue;
			}
			vector lo = m_aDispMarkLo[n];
			if (haveMe && vector.DistanceSqXZ(lo + size * 0.5, o) > rr * rr)
				continue;
			Shape.Create(ShapeType.BBOX, m_aDispMarkC[n], ShapeFlags.WIREFRAME | ShapeFlags.ONCE, lo, lo + size);
		}
		for (int h = m_aDispHolePos.Count() - 1; h >= 0; h--)
		{
			if (now > m_aDispHoleUntil[h] || !holesOn)
			{
				m_aDispHolePos.RemoveOrdered(h);
				m_aDispHoleUntil.RemoveOrdered(h);
				continue;
			}
			Shape.CreateSphere(ARGB(255, 255, 0, 0), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, m_aDispHolePos[h], 0.15);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every pending build of a shown brick in: old body + entity out, the new one shown (an empty new mesh just
	//! removes the old). Returns the bricks swapped.
	protected int SwapPending()
	{
		int swapped;
		foreach (MUD_VoxelBrick s : m_aSwap)
		{
			if (!s || !s.m_bNew)
				continue;    // already swapped (queued twice)
			s.m_bNew = false;
			if (s.m_bMeshOnly)
			{
				// RUT_LOOK look-only: the new mesh on the shown entity, its body untouched (no body swap under a wheel)
				s.m_bMeshOnly = false;
				if (s.m_Ent && s.m_NewRes)
				{
					s.m_Ent.SetObject(s.m_NewRes.GetResource().ToMeshObject(), "");
					s.m_Ent.Update();
					s.m_Res = s.m_NewRes;
				}
				s.m_NewRes = null;
				swapped++;
				continue;
			}
			if (s.m_Ent)
			{
				Bury(s.m_Ent, s.m_Res, s.m_aGeoms);
				s.m_Phys = null;
			}
			else
			{
				DropBody(s);
			}
			s.m_Ent = s.m_NewEnt;
			s.m_Res = s.m_NewRes;
			s.m_Phys = s.m_NewPhys;
			foreach (PhysicsGeom g : s.m_aNewGeoms)
				s.m_aGeoms.Insert(g);
			s.m_aNewGeoms.Clear();
			s.m_NewEnt = null;
			s.m_NewRes = null;
			s.m_NewPhys = null;
			if (s.m_Ent)
				s.m_Ent.SetFlags(EntityFlags.VISIBLE, false);
			swapped++;
		}
		m_aSwap.Clear();
		return swapped;
	}

	//------------------------------------------------------------------------------------------------
	//! The hole cells carved since the last go-live go VOX (their heightfield out, the voxels drawn there). Cells no op
	//! changed keep the heightfield and its ruts. Returns cells changed.
	protected int UpdateVox(out int ms)
	{
		int t0 = System.GetTickCount();
		ms = 0;
		if (m_aNewCarved.IsEmpty())
			return 0;
		MUD_CoarseSkinEntity skin = MUD_CoarseSkinEntity.GetInstance();
		if (!skin)
		{
			m_aNewCarved.Clear();
			return 0;
		}
		array<int> cxs = {};
		array<int> czs = {};
		array<bool> want = {};
		int cx0 = 1000000;
		int cx1 = -1000000;
		int cz0 = 1000000;
		int cz1 = -1000000;
		foreach (int ck : m_aNewCarved)
		{
			int cx = ck % CELL_KEY_W - KEY_OFF;
			int cz = ck / CELL_KEY_W - KEY_OFF;
			if (!skin.IsHoleCell(cx, cz))
				continue;
			cxs.Insert(cx);
			czs.Insert(cz);
			want.Insert(true);
			if (cx < cx0)
				cx0 = cx;
			if (cx > cx1)
				cx1 = cx;
			if (cz < cz0)
				cz0 = cz;
			if (cz > cz1)
				cz1 = cz;
		}
		m_aNewCarved.Clear();
		int changed = skin.SetVoxCells(cxs, czs, want);
		if (changed > 0)
			MUD_WorldGroundEntity.VoxCellsChanged(cx0, cx1, cz0, cz1);
		ms = System.GetTickCount(t0);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_LOOK: the look cells marked since the last GoLive hide their patch mesh (MUD_WorldGround LOOK bit) in the
	//! frame the look bricks show
	protected void UpdateLook()
	{
		// one cell at a time (not one box over all: two vehicles far apart would scan every patch between them); a
		// patch takes all its look cells at its first call, the later calls find no change
		foreach (int ck : m_aNewLook)
		{
			int cx = ck % CELL_KEY_W - KEY_OFF;
			int cz = ck / CELL_KEY_W - KEY_OFF;
			MUD_WorldGroundEntity.LookCellsChanged(cx, cx, cz, cz);
		}
		m_aNewLook.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! The samples round brick b (-1..SUB per axis) into m_Gb: its own, its neighbours', the seeded field for a missing
	//! row, MISS for a column not converted (m_bAnyMiss)
	//! look (RUT_LOOK): min(feel, look cut) per sample, for the mesh; feel: sets m_bLookDiff when they differ.
	protected void Gather(MUD_VoxelBrick b, bool look)
	{
		int tg = System.GetTickCount();
		m_bAnyMiss = false;
		if (!look)
			m_bLookDiff = false;
		int gi0 = b.m_iBi * SUB - 1;
		int gj0 = b.m_iBj * SUB - 1;
		int gk0 = b.m_iBk * SUB - 1;
		MUD_VoxelColumn col;
		int cbi = -1000000;
		int cbk = -1000000;
		for (int lk = 0; lk < GN; lk++)
		{
			int gk = gk0 + lk;
			int bk = FloorDiv(gk, SUB);
			for (int li = 0; li < GN; li++)
			{
				int gi = gi0 + li;
				int bi = FloorDiv(gi, SUB);
				if (bi != cbi || bk != cbk)
				{
					col = m_mCols.Get(ColKey(bi, bk));
					cbi = bi;
					cbk = bk;
				}
				if (!col)
				{
					m_Gb.m_aTop[lk * GN + li] = 0;
					m_Gb.m_aLa[lk * GN + li] = BAND_NONE;
					m_Gb.m_aHs[lk * GN + li] = -BAND_NONE;
					for (int mj = 0; mj < GN; mj++)
						m_Gb.m_aG[lk * GN2 + mj * GN + li] = MISS;
					m_bAnyMiss = true;
					continue;
				}
				int ci = gi - bi * SUB;
				int ck = gk - bk * SUB;
				float top = col.m_aTop[ck * SUB + ci];
				m_Gb.m_aTop[lk * GN + li] = top;
				MUD_VoxelBrick rb = null;     // explicit: a loop-body local is not reset each pass
				int rbj = -1000000;
				int la = GN;
				int hs = -1;
				for (int lj = 0; lj < GN; lj++)
				{
					int gj = gj0 + lj;
					int bj = FloorDiv(gj, SUB);
					if (bj != rbj)
					{
						rb = col.m_mRows.Get(bj);
						rbj = bj;
					}
					float v;
					if (rb)
					{
						int vi = (ck * SUB + gj - bj * SUB) * SUB + ci;
						v = rb.m_aD[vi];
						if (rb.m_bHasV && rb.m_aV[vi] < v)
						{
							if (look)
								v = rb.m_aV[vi];
							else
								m_bLookDiff = true;
						}
					}
					else
					{
						v = Nudge(top - gj * CELL);
					}
					m_Gb.m_aG[lk * GN2 + lj * GN + li] = v;
					if (v > 0)
						hs = lj;
					else if (la == GN)
						la = lj;
				}
				m_Gb.m_aLa[lk * GN + li] = la;
				m_Gb.m_aHs[lk * GN + li] = hs;
			}
		}
		m_iBatchGatherMs += System.GetTickCount(tg);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildSub(MUD_VoxelBrick b)
	{
		int t0 = System.GetTickCount();
		bool feelQ = b.m_bFeelQ;
		b.m_bFeelQ = false;
		m_B = b;
		m_vBOrg = Vector(m_fX0 + b.m_iBi * BRICK, b.m_iBj * BRICK, m_fZ0 + b.m_iBk * BRICK);
		// skin cells -1..2 round the brick (its quads reach 0.5 m past it): draw only in carved hole cells
		MUD_CoarseSkinEntity eskin = MUD_CoarseSkinEntity.GetInstance();
		int emitAny = 0;
		int lookAny = 0;
		for (int ez = 0; ez < 4; ez++)
		{
			for (int ex = 0; ex < 4; ex++)
			{
				int ecx = b.m_iBi * 2 + ex - 1;
				int ecz = b.m_iBk * 2 + ez - 1;
				int em = 0;
				int eml = 0;
				if (eskin && eskin.IsHoleCell(ecx, ecz))
				{
					if (Carved(ecx, ecz))
						em = 1;
					if (em == 1 || (!m_bHeadless && LookCarved(ecx, ecz)))
						eml = 1;
				}
				m_aEmitFeel[ez * 4 + ex] = em;
				m_aEmitLook[ez * 4 + ex] = eml;
				emitAny += em;
				lookAny += eml;
			}
		}
		m_iEmitFeelAny = emitAny;
		UseEmit(false);
		Gather(b, false);
		// RUT_LOOK: a look rut (op 6) the feel field has not caught up with, or a look cell, reaches this brick - the
		// mesh comes from the look field over the look cells, the body (FeelPass) from the feel field over the carved
		// cells only, so every machine's wheels touch the same ground (the patch collider still holds the look cells)
		bool split = (m_bLookDiff || lookAny != emitAny) && !m_bHeadless;
		if (split)
		{
			UseEmit(true);
			Gather(b, true);
			Polygonize(lookAny);
		}
		else
		{
			Polygonize(emitAny);
		}
		if (split && !m_bOverflow && m_iTop + m_iDirt == 0)
		{
			split = false;    // the look field draws nothing here: the feel one draws
			UseEmit(false);
			Gather(b, false);
			Polygonize(emitAny);
		}
		int tPoly = System.GetTickCount(t0);
		if (m_bOverflow)
		{
			Print(string.Format("MUD voxel probe: brick %1 %2 %3 overflow (%4 verts) - not built", b.m_iBi, b.m_iBj, b.m_iBk, m_iV), LogLevel.ERROR);
			b.m_bFeelQ = feelQ;    // not built: a later look-only build must not skip the body
			return;
		}
		if (split)
			m_iBatchLook++;

		int t1 = System.GetTickCount();
		int tMesh;
		int t2;
		// look-only = queued by look ops alone, on a shown brick with a body and no build still waiting to swap in
		bool lookOnly = b.m_bShown && !feelQ && b.m_Ent != null && !b.m_bNew;
		if (lookOnly && !split)
		{
			// no look cut reaches it: mesh and body are what they were
			m_iBatchSkipped++;
			return;
		}
		if (lookOnly)
		{
			// RUT_LOOK look-only: a new mesh for the shown entity at the next GoLive; the body stays as it is
			DropPending(b);
			Resource mres = MakeRes();
			if (mres)
			{
				b.m_NewRes = mres;
				b.m_bNew = true;
				b.m_bMeshOnly = true;
				m_aSwap.Insert(b);
				m_iBatchMeshOnly++;
			}
			m_iBatchBuilt++;
			m_iBatchVerts += m_iV;
			return;
		}
		if (b.m_bShown)
		{
			// shown brick: build hidden, swap in with the rest (SwapPending)
			DropPending(b);
			b.m_bNew = true;
			m_aSwap.Insert(b);
			if (m_iTop + m_iDirt > 0)
			{
				Resource pres;
				b.m_NewEnt = MeshEntity(null, true, pres);
				b.m_NewRes = pres;
				tMesh = System.GetTickCount(t1);
				t2 = System.GetTickCount();
				if (b.m_NewEnt && pres && FeelPass(b, split, emitAny))
					b.m_NewPhys = MakeBody(b.m_NewEnt, b.m_aNewGeoms);
				if (m_bOverflow)
				{
					DropPending(b);    // the feel pass overflowed: the old brick stays
					b.m_bFeelQ = feelQ;
				}
			}
			else
			{
				tMesh = System.GetTickCount(t1);
				t2 = System.GetTickCount();
			}
		}
		else
		{
			bool made = MakeMesh(b);
			tMesh = System.GetTickCount(t1);
			t2 = System.GetTickCount();
			DropBody(b);
			if (made && FeelPass(b, split, emitAny))
				b.m_Phys = MakeBody(b.m_Ent, b.m_aGeoms);
		}
		int tCol = System.GetTickCount(t2);

		int total = System.GetTickCount(t0);
		m_iBatchBuilt++;
		m_iBatchVerts += m_iV;
		if (total > m_iBatchWorstSub)
			m_iBatchWorstSub = total;
		if (total >= SLOW_MS)
			Print(string.Format("MUD voxel probe: slow brick %1 %2 %3: %4 verts, %5 top + %6 dirt tris | poly %7 ms mesh %8 ms collider %9 ms",
				b.m_iBi, b.m_iBj, b.m_iBk, m_iV, m_iTop / 3, m_iDirt / 3, tPoly, tMesh, tCol), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT_LOOK: after a split mesh (look field), the feel field's triangles into the buffers for the body. True =
	//! triangles for a body; false = none (or the feel pass overflowed: logged, m_bOverflow set).
	protected bool FeelPass(MUD_VoxelBrick b, bool split, int emitAny)
	{
		if (split)
		{
			UseEmit(false);
			Gather(b, false);
			Polygonize(m_iEmitFeelAny);
			if (m_bOverflow)
			{
				Print(string.Format("MUD voxel probe: brick %1 %2 %3 feel pass overflow (%4 verts) - old body kept", b.m_iBi, b.m_iBj, b.m_iBk, m_iV), LogLevel.ERROR);
				return false;
			}
		}
		return m_iTop + m_iDirt > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Surface nets over the gathered samples (m_Gb) into the buffers (m_Vb, m_IbTop, m_IbDirt). emitAny = 0
	//! (nothing carved round the brick): no quads at all (empty mesh, no body). Scans only the sign band of each
	//! sample column (Gather m_aLa / m_aHs). (The TEMP full-scan self-check, SCAN_CHECK_EVERY: archived 2026-10-21.)
	protected void Polygonize(int emitAny)
	{
		int tp = System.GetTickCount();
		PolyPass(emitAny, false);
		m_iBatchPolyMs += System.GetTickCount(tp);
		Displace();
	}

	//------------------------------------------------------------------------------------------------
	//! DISPLACED RUTS, after Polygonize: a brick the rut floor reaches - any vertex or triangle edge middle would be
	//! pulled (the plan's 6-split-point rule; every brick runs the same test on the same floor, so the lines between
	//! bricks match) - gets every triangle split 2 x 2 (one shared midpoint per edge), every vertex pulled to
	//! min(y, floor) (only if less than DISP_GUARD above it: tunnel roofs stay), then re-emitted through Tri.
	//! A wall triangle the pull turns over is dropped (fold). Too many triangles to split: coarse pull only (logged).
	//! Mesh and body read the same buffers, so the wheels feel what is drawn.
	protected void Displace()
	{
		if (!DISPLACE_RUTS || m_bOverflow || m_iTop + m_iDirt == 0)
			return;
		int bi = m_B.m_iBi;
		int bk = m_B.m_iBk;
		bool any = false;
		for (int dk = -1; dk <= 1; dk++)
		{
			for (int di = -1; di <= 1; di++)
			{
				if (m_mRutTiles.Contains(ColKey(bi + di, bk + dk)))
					any = true;
			}
		}
		if (!any)
			return;
		int dt0 = System.GetTickCount();
		// the floor round the brick: its quads reach local -0.5 .. 4 (cells -1 .. SUB-1), the bilinear one more.
		// Stored brick-local (minus m_vBOrg y) like m_Vb: the floor is world y (user 2026-10-15: ruts not drawn)
		int fi0 = bi * FSUB - DISP_PAD;
		int fk0 = bk * FSUB - DISP_PAD;
		float oy = m_vBOrg[1];
		for (int gz = 0; gz < DISP_N; gz++)
		{
			for (int gx = 0; gx < DISP_N; gx++)
			{
				float fs = FloorSample(fi0 + gx, fk0 + gz);
				if (fs < FLOOR_NONE * 0.5)
					fs = MUD_WorldGroundEntity.FieldEdgeLocked(fi0 + gx, fk0 + gz, fs) - oy;     // edge lock, as the patches (F4)
				m_aDispF[gz * DISP_N + gx] = fs;
			}
		}
		bool rutted = false;
		for (int v = 0; v < m_iV && !rutted; v++)
			rutted = DispPulled(m_Vb.m_aV[v]);
		for (int t = 0; t < m_iTop && !rutted; t += 3)
			rutted = DispEdgesPulled(m_IbTop.m_aI[t], m_IbTop.m_aI[t + 1], m_IbTop.m_aI[t + 2]);
		for (int t2 = 0; t2 < m_iDirt && !rutted; t2 += 3)
			rutted = DispEdgesPulled(m_IbDirt.m_aI[t2], m_IbDirt.m_aI[t2 + 1], m_IbDirt.m_aI[t2 + 2]);
		if (!rutted)
		{
			DispTime(dt0);
			return;
		}
		// budget, COMBINED (Tri caps m_iTop + m_iDirt): 4 x the indices, at most one new vertex per index
		bool split = (m_iTop + m_iDirt) * 4 <= MAX_I && m_iV + m_iTop + m_iDirt <= MAX_V;
		if (!split)
		{
			m_iBatchDispOvf++;
			Print(string.Format("MUD voxel probe: rut split overflow brick %1 %2 %3 (%4 verts, %5 tris) - coarse pull only",
				bi, m_B.m_iBj, bk, m_iV, (m_iTop + m_iDirt) / 3), LogLevel.WARNING);
		}
		if (!m_IbDispTop)
		{
			m_IbDispTop = new MUD_VoxelIdx();
			m_IbDispDirt = new MUD_VoxelIdx();
		}
		int nTop = m_iTop;
		int nDirt = m_iDirt;
		for (int i = 0; i < nTop; i++)
			m_IbDispTop.m_aI[i] = m_IbTop.m_aI[i];
		for (int i2 = 0; i2 < nDirt; i2++)
			m_IbDispDirt.m_aI[i2] = m_IbDirt.m_aI[i2];
		int v0 = m_iV;
		map<int, int> mids = new map<int, int>();
		if (split)
		{
			for (int s = 0; s < nTop; s += 3)
				DispMids(m_IbDispTop.m_aI[s], m_IbDispTop.m_aI[s + 1], m_IbDispTop.m_aI[s + 2], mids);
			for (int s2 = 0; s2 < nDirt; s2 += 3)
				DispMids(m_IbDispDirt.m_aI[s2], m_IbDispDirt.m_aI[s2 + 1], m_IbDispDirt.m_aI[s2 + 2], mids);
		}
		// pull every vertex (the midpoints too), keeping the old y for the fold check
		for (int p = 0; p < m_iV; p++)
		{
			vector pv = m_Vb.m_aV[p];
			m_aDispY[p] = pv[1];
			float f = DispFloor(pv[0], pv[2], pv[1]);
			if (f < pv[1] - DISP_EPS && pv[1] - f < DISP_GUARD)
			{
				pv[1] = f;
				m_Vb.m_aV[p] = pv;
				if (pv[0] < 0.5 || pv[0] > BRICK - 0.5 || pv[2] < 0.5 || pv[2] > BRICK - 0.5)
					m_iDispBorder++;
			}
		}
		// re-emit; a top tri the pull made steep turns soil (DispEmit; user 2026-10-15: soil rut walls)
		m_mDispSoil.Clear();
		m_iTop = 0;
		m_iDirt = 0;
		int fold0 = m_iBatchFold;
		for (int e = 0; e < nTop; e += 3)
			DispTri(m_IbDispTop.m_aI[e], m_IbDispTop.m_aI[e + 1], m_IbDispTop.m_aI[e + 2], true, split, mids);
		for (int e2 = 0; e2 < nDirt; e2 += 3)
			DispTri(m_IbDispDirt.m_aI[e2], m_IbDispDirt.m_aI[e2 + 1], m_IbDispDirt.m_aI[e2 + 2], false, split, mids);
		if (split)
		{
			m_iBatchSplit++;
			m_iBatchSplitV += m_iV - v0;
		}
		DispTime(dt0);
		// draw bricks: overflow (blue) / fold drops (orange), rendering machines
		if (!m_bHeadless && (!split || m_iBatchFold > fold0) && DispOn(DSW_BRICKS) && m_aDispMarkLo.Count() < DISP_MARK_MAX)
		{
			int mc = ARGB(255, 255, 140, 0);
			if (!split)
				mc = ARGB(255, 40, 120, 255);
			m_aDispMarkLo.Insert(m_vBOrg);
			m_aDispMarkC.Insert(mc);
			m_aDispMarkUntil.Insert(System.GetTickCount() + DISP_MARK_MS);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Displace: rut floor at brick-local x, z (bilinear over m_aDispF); a corner with no floor counts as ownY,
	//! all four none (or off the grid) = FLOOR_NONE
	protected float DispFloor(float lx, float lz, float ownY)
	{
		float fx = lx / FCELL + DISP_PAD;
		float fz = lz / FCELL + DISP_PAD;
		int ix = Math.Floor(fx);
		int iz = Math.Floor(fz);
		if (ix < 0 || iz < 0 || ix + 1 >= DISP_N || iz + 1 >= DISP_N)
			return FLOOR_NONE;
		float tx = fx - ix;
		float tz = fz - iz;
		int c = iz * DISP_N + ix;
		float f00 = m_aDispF[c];
		float f10 = m_aDispF[c + 1];
		float f01 = m_aDispF[c + DISP_N];
		float f11 = m_aDispF[c + DISP_N + 1];
		float isNone = FLOOR_NONE * 0.5;
		if (f00 > isNone && f10 > isNone && f01 > isNone && f11 > isNone)
			return FLOOR_NONE;
		if (f00 > isNone)
			f00 = ownY;
		if (f10 > isNone)
			f10 = ownY;
		if (f01 > isNone)
			f01 = ownY;
		if (f11 > isNone)
			f11 = ownY;
		return (f00 * (1 - tx) + f10 * tx) * (1 - tz) + (f01 * (1 - tx) + f11 * tx) * tz;
	}

	//------------------------------------------------------------------------------------------------
	//! Displace: brick-local point p would be pulled (floor under it, less than DISP_GUARD under)
	protected bool DispPulled(vector p)
	{
		float f = DispFloor(p[0], p[2], p[1]);
		return f < p[1] - DISP_EPS && p[1] - f < DISP_GUARD;
	}

	//------------------------------------------------------------------------------------------------
	//! Displace: an edge middle of triangle a, b, c would be pulled
	protected bool DispEdgesPulled(int a, int b, int c)
	{
		vector pa = m_Vb.m_aV[a];
		vector pb = m_Vb.m_aV[b];
		vector pc = m_Vb.m_aV[c];
		return DispPulled((pa + pb) * 0.5) || DispPulled((pb + pc) * 0.5) || DispPulled((pc + pa) * 0.5);
	}

	//------------------------------------------------------------------------------------------------
	//! Displace: the midpoints of triangle a, b, c's edges (made once per edge)
	protected void DispMids(int a, int b, int c, map<int, int> mids)
	{
		DispMid(a, b, mids);
		DispMid(b, c, mids);
		DispMid(c, a, mids);
	}

	//------------------------------------------------------------------------------------------------
	//! Displace: the midpoint vertex of edge a, b (key = the two ids sorted; VKEY > MAX_V). Position and UV = the
	//! mean of the ends (both ends of a quad's edge share its UV tile). CellV's UV-parity copies of one cell vertex
	//! give separate midpoints at the same position: no crack.
	protected int DispMid(int a, int b, map<int, int> mids)
	{
		int lo = a;
		int hi = b;
		if (b < a)
		{
			lo = b;
			hi = a;
		}
		int key = lo * VKEY + hi;
		int m;
		if (mids.Find(key, m))
			return m;
		m = m_iV;
		m_Vb.m_aV[m] = (m_Vb.m_aV[a] + m_Vb.m_aV[b]) * 0.5;
		m_Vb.m_aUV[m * 2] = (m_Vb.m_aUV[a * 2] + m_Vb.m_aUV[b * 2]) * 0.5;
		m_Vb.m_aUV[m * 2 + 1] = (m_Vb.m_aUV[a * 2 + 1] + m_Vb.m_aUV[b * 2 + 1]) * 0.5;
		m_iV++;
		mids.Insert(key, m);
		return m;
	}

	//------------------------------------------------------------------------------------------------
	//! Displace: coarse triangle a, b, c back into the buffers - as it is, or as 4 (same winding)
	protected void DispTri(int a, int b, int c, bool top, bool split, map<int, int> mids)
	{
		if (!split)
		{
			DispEmit(a, b, c, top);
			return;
		}
		int ab = DispMid(a, b, mids);
		int bc = DispMid(b, c, mids);
		int ca = DispMid(c, a, mids);
		DispEmit(a, ab, ca, top);
		DispEmit(ab, b, bc, top);
		DispEmit(ca, bc, c, top);
		DispEmit(ab, bc, ca, top);
	}

	//------------------------------------------------------------------------------------------------
	//! Displace: copy of vertex v with soil UVs (kind 1-3, tile ta / tb as CellV), made once per vertex + kind;
	//! a copy whose tile differs from the cached one is made again (a tile line between two walls)
	protected int DispSoilV(int v, int kind, int ta, int tb)
	{
		int key = v * 4 + kind - 1;
		int got;
		vector p = m_Vb.m_aV[v];
		float a;
		float b;
		float tile;
		KindAB(kind, p, a, b, tile);
		float u = (a - ta * tile) / tile;
		float w = 1 - (b - tb * tile) / tile;
		if (m_mDispSoil.Find(key, got) && Math.AbsFloat(m_Vb.m_aUV[got * 2] - u) < 0.0001 && Math.AbsFloat(m_Vb.m_aUV[got * 2 + 1] - w) < 0.0001)
			return got;
		int nv = m_iV;
		m_Vb.m_aV[nv] = p;
		m_Vb.m_aUV[nv * 2] = u;
		m_Vb.m_aUV[nv * 2 + 1] = w;
		m_iV++;
		m_mDispSoil.Set(key, nv);
		return nv;
	}

	//------------------------------------------------------------------------------------------------
	//! Displace: one triangle through Tri; a wall (non-top) triangle whose normal the pull turned over is dropped
	//! (top ones can't fold: the pull moves y only - MEASURED 0 folds offline). A top one the pull made steeper
	//! than Quad's grass rule (n.y <= 0.7 |n|) or lying in the groove (user 2026-10-15: soil only in a few patches,
	//! should fill the groove) is soil, on soil-UV copies of its corners
	protected void DispEmit(int a, int b, int c, bool top)
	{
		if (top)
		{
			vector ta = m_Vb.m_aV[a];
			vector tn = Cross(m_Vb.m_aV[b] - ta, m_Vb.m_aV[c] - ta);
			vector tmid = (ta + m_Vb.m_aV[b] + m_Vb.m_aV[c]) * 0.3333333;
			// in the groove (more than DISP_SOIL_CUT under the seeded ground, Quad's reference) or steep
			bool groove = GroundLocal(tmid[0], tmid[2]) - tmid[1] > DISP_SOIL_CUT;
			// RUT FIELD: also soil where the floor pulled it more than FIELD_SOIL_D under the voxel surface, as the
			// patches (a rut over a raised crater rim stays above the seeded ground: was grass beside soil patches)
			if (RUT_FIELD && !groove)
			{
				float pulled = (m_aDispY[a] + m_aDispY[b] + m_aDispY[c]) * 0.3333333 - tmid[1];
				groove = pulled > MUD_WorldGroundEntity.FIELD_SOIL_D;
			}
			if ((groove || tn[1] <= 0.7 * tn.Length()) && m_iV + 3 <= MAX_V)
			{
				// UV plane as Quad picks it: top-down soil (3) unless a side faces more (1 along x, 2 along z)
				float ax = Math.AbsFloat(tn[0]);
				float az = Math.AbsFloat(tn[2]);
				int kind = 3;
				if (ax > tn[1] || az > tn[1])
				{
					kind = 2;
					if (ax >= az)
						kind = 1;
				}
				float ma;
				float mb;
				float tile;
				KindAB(kind, tmid, ma, mb, tile);
				int tia = Math.Floor(ma / tile);
				int tib = Math.Floor(mb / tile);
				int sa = DispSoilV(a, kind, tia, tib);
				int sb = DispSoilV(b, kind, tia, tib);
				int sc = DispSoilV(c, kind, tia, tib);
				m_iBatchSoil++;
				Tri(sa, sb, sc, false);
				return;
			}
		}
		else
		{
			vector pa = m_Vb.m_aV[a];
			vector pb = m_Vb.m_aV[b];
			vector pc = m_Vb.m_aV[c];
			vector oa = Vector(pa[0], m_aDispY[a], pa[2]);
			vector ob = Vector(pb[0], m_aDispY[b], pb[2]);
			vector oc = Vector(pc[0], m_aDispY[c], pc[2]);
			vector n = Cross(pb - pa, pc - pa);
			vector n0 = Cross(ob - oa, oc - oa);
			if (vector.Dot(n, n0) < 0)
			{
				m_iBatchFold++;
				return;
			}
		}
		Tri(a, b, c, top);
	}

	//------------------------------------------------------------------------------------------------
	//! One surface-nets pass; full = every sample (the old scan), else each column's sign band only: an edge can
	//! change sign only between the lowest air row and the highest solid row of the columns it joins.
	protected void PolyPass(int emitAny, bool full)
	{
		m_iStamp++;
		if (m_iStamp > STAMP_MAX)
		{
			// stamps * VKEY would pass 2^31: clear the caches once and start over
			for (int c = 0; c < CN3 * 8; c++)
				m_Cb.m_aCv[c] = 0;
			for (int c2 = 0; c2 < CN3; c2++)
			{
				m_Cb.m_aCs[c2] = 0;
				m_Gb.m_aOk[c2] = 0;
			}
			m_iStamp = 1;
		}
		m_iV = 0;
		m_iTop = 0;
		m_iDirt = 0;
		m_iDegen = 0;
		m_bOverflow = false;
		// every sample edge whose base sample is in this brick, +x / +y / +z; a sign change gives one quad between
		// the 4 cells around the edge (cells touching a column that is not converted: none, the edge stays open)
		for (int lk = 0; lk < SUB && emitAny > 0; lk++)
		{
			for (int li = 0; li < SUB; li++)
			{
				int j0 = 0;
				int j1 = SUB - 1;
				if (!full)
				{
					// gathered rows: base sample column c, its +x and +z neighbours; brick row lj = gathered row - 1
					int c = (lk + 1) * GN + li + 1;
					int lo = m_Gb.m_aLa[c] - 1;
					int lx = m_Gb.m_aLa[c + 1];
					int lz = m_Gb.m_aLa[c + GN];
					if (lx < lo)
						lo = lx;
					if (lz < lo)
						lo = lz;
					int hi = m_Gb.m_aHs[c];
					int hx = m_Gb.m_aHs[c + 1];
					int hz = m_Gb.m_aHs[c + GN];
					if (hx > hi)
						hi = hx;
					if (hz > hi)
						hi = hz;
					if (lo - 1 > j0)
						j0 = lo - 1;
					if (hi - 1 < j1)
						j1 = hi - 1;
					if (j1 >= j0)
						m_iBatchScanned += j1 - j0 + 1;
				}
				for (int lj = j0; lj <= j1; lj++)
				{
					int g = (lk + 1) * GN2 + (lj + 1) * GN + li + 1;
					float v = m_Gb.m_aG[g];
					if (v >= MISS)
						continue;
					bool solid = v > 0;
					int lc = (lk + 1) * CN2 + (lj + 1) * CN + li + 1;
					float vx = m_Gb.m_aG[g + 1];
					if (vx < MISS && solid != (vx > 0))
						Quad(lc - CN2 - CN, lc - CN2, lc, lc - CN, Vector(1, 0, 0), solid);
					float vy = m_Gb.m_aG[g + GN];
					if (vy < MISS && solid != (vy > 0))
						Quad(lc - CN2 - 1, lc - CN2, lc, lc - 1, Vector(0, 1, 0), solid);
					float vz = m_Gb.m_aG[g + GN2];
					if (vz < MISS && solid != (vz > 0))
						Quad(lc - CN - 1, lc - CN, lc, lc - 1, Vector(0, 0, 1), solid);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! DIG NOW: world y of the voxel ground's top at world x, z, read straight from the field (bilinear over the 4
	//! sample columns round it, no physics traces, so right in the go-live frame). false: no converted column there
	//! (or real terrain over it) - the caller keeps its own height.
	static bool DrawsAt(float x, float z)
	{
		if (!s_Inst)
			return false;
		int cx = Math.Floor((x - s_Inst.m_fX0) / (SPS * CELL));
		int cz = Math.Floor((z - s_Inst.m_fZ0) / (SPS * CELL));
		return s_Inst.Carved(cx, cz);
	}

	//! Synced mud sheet Stage 4 (2026-10-18): Put, while m_bTopTrack - grid column gi, gk's top (ColTop) before this
	//! op first changes it (TOP_NONE = no top). Key gk * TOP_KEY + gi; columns outside 0..TOP_KEY-1 are not kept.
	protected void KeepTop(int gi, int gk)
	{
		if (gi < 0 || gk < 0 || gi >= TOP_KEY || gk >= TOP_KEY)
			return;
		int key = gk * TOP_KEY + gi;
		if (m_mTopWas.Contains(key))
			return;
		float h;
		if (!ColTop(gi, gk, h))
			h = TOP_NONE;
		m_mTopWas.Set(key, h);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 4 (MUD_MudSync dig clearing, after the op until the next Submit): world x, z box round the grid columns the
	//! last tracked op changed whose top moved (+ one grid cell: TopAt blends 4 columns). False = none moved.
	static bool TopsMoved(out float x0, out float z0, out float x1, out float z1)
	{
		x0 = 0;
		z0 = 0;
		x1 = -1;
		z1 = -1;
		if (!s_Inst || s_Inst.m_mTopWas.IsEmpty())
			return false;
		int i0 = 1000000;
		int i1 = -1000000;
		int k0 = 1000000;
		int k1 = -1000000;
		for (int n = 0; n < s_Inst.m_mTopWas.Count(); n++)
		{
			int key = s_Inst.m_mTopWas.GetKey(n);
			float was = s_Inst.m_mTopWas.GetElement(n);
			int gk = key / TOP_KEY;
			int gi = key - gk * TOP_KEY;
			float now;
			if (!s_Inst.ColTop(gi, gk, now))
				now = TOP_NONE;
			if (Math.AbsFloat(now - was) < 0.001)
				continue;
			i0 = Math.Min(i0, gi);
			i1 = Math.Max(i1, gi);
			k0 = Math.Min(k0, gk);
			k1 = Math.Max(k1, gk);
		}
		if (i1 < i0)
			return false;
		x0 = s_Inst.m_fX0 + (i0 - 1) * CELL;
		x1 = s_Inst.m_fX0 + (i1 + 1) * CELL;
		z0 = s_Inst.m_fZ0 + (k0 - 1) * CELL;
		z1 = s_Inst.m_fZ0 + (k1 + 1) * CELL;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 4: TopAt as it was before the last tracked op (kept tops in place of the changed columns). False = a
	//! corner column has no top then.
	static bool TopWasAt(float x, float z, out float y)
	{
		if (!s_Inst)
			return false;
		float gx = (x - s_Inst.m_fX0) / CELL;
		float gz = (z - s_Inst.m_fZ0) / CELL;
		int gi = Math.Floor(gx);
		int gk = Math.Floor(gz);
		float fx = gx - gi;
		float fz = gz - gk;
		float h00;
		float h10;
		float h01;
		float h11;
		if (!s_Inst.TopWas(gi, gk, h00) || !s_Inst.TopWas(gi + 1, gk, h10) || !s_Inst.TopWas(gi, gk + 1, h01) || !s_Inst.TopWas(gi + 1, gk + 1, h11))
			return false;
		y = (h00 * (1 - fx) + h10 * fx) * (1 - fz) + (h01 * (1 - fx) + h11 * fx) * fz;
		return true;
	}

	protected bool TopWas(int gi, int gk, out float h)
	{
		if (gi >= 0 && gk >= 0 && gi < TOP_KEY && gk < TOP_KEY && m_mTopWas.Find(gk * TOP_KEY + gi, h))
			return h != TOP_NONE;
		return ColTop(gi, gk, h);
	}

	//------------------------------------------------------------------------------------------------
	//! Build 2 Stage 5 (MUD_GroundFollow): voxel ground top at x, z WITHOUT the rut floor (J2: the same on a joiner
	//! that replays ops before the rut snapshot), not cached. False = a corner column is not converted (no op there).
	static bool TopAt(float x, float z, out float y)
	{
		if (!s_Inst)
			return false;
		float gx = (x - s_Inst.m_fX0) / CELL;
		float gz = (z - s_Inst.m_fZ0) / CELL;
		int gi = Math.Floor(gx);
		int gk = Math.Floor(gz);
		float fx = gx - gi;
		float fz = gz - gk;
		float h00;
		float h10;
		float h01;
		float h11;
		if (!s_Inst.ColTop(gi, gk, h00) || !s_Inst.ColTop(gi + 1, gk, h10) || !s_Inst.ColTop(gi, gk + 1, h01) || !s_Inst.ColTop(gi + 1, gk + 1, h11))
			return false;
		y = (h00 * (1 - fx) + h10 * fx) * (1 - fz) + (h01 * (1 - fx) + h11 * fx) * fz;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! TopAt: ColSurf's top of the solid without the rut floor and without the m_mSurf cache
	protected bool ColTop(int gi, int gk, out float h)
	{
		h = -9999;
		MUD_VoxelColumn col = ColAt(gi, gk);
		if (!col)
			return false;
		int li = gi - col.m_iBi * SUB;
		int lk = gk - col.m_iBk * SUB;
		int n = lk * SUB + li;
		float undug = col.m_aUndug[n];
		if (col.m_aHard[n] != 0)
		{
			h = undug;
			return true;
		}
		int j1 = Math.Ceil((undug + 2) / CELL);
		int j0 = Math.Floor((undug - TUNNEL_FLOOR - 1) / CELL);
		float prev = Value(col, li, lk, j1);
		if (prev > 0)
			return false;
		for (int j = j1 - 1; j >= j0; j--)
		{
			float v = Value(col, li, lk, j);
			if (v > 0)
			{
				h = j * CELL + v / (v - prev) * CELL;
				return true;
			}
			prev = v;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Synced mud sheet (2026-10-18): is sample x, z voxel ground (a column), not the heightfield? No cache.
	static bool ColumnAt(float x, float z)
	{
		if (!s_Inst)
			return false;
		int cgi = Math.Floor((x - s_Inst.m_fX0) / CELL);
		int cgk = Math.Floor((z - s_Inst.m_fZ0) / CELL);
		return s_Inst.ColAt(cgi, cgk) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! DIG NOW: voxel ground top at x, z (see SurfaceAtI)
	static bool SurfaceAt(float x, float z, out float y)
	{
		if (!s_Inst)
			return false;
		return s_Inst.SurfaceAtI(x, z, y);
	}

	protected bool SurfaceAtI(float x, float z, out float y)
	{
		float gx = (x - m_fX0) / CELL;
		float gz = (z - m_fZ0) / CELL;
		int gi = Math.Floor(gx);
		int gk = Math.Floor(gz);
		float fx = gx - gi;
		float fz = gz - gk;
		float h00;
		float h10;
		float h01;
		float h11;
		if (!ColSurf(gi, gk, h00) || !ColSurf(gi + 1, gk, h10) || !ColSurf(gi, gk + 1, h01) || !ColSurf(gi + 1, gk + 1, h11))
			return false;
		y = (h00 * (1 - fx) + h10 * fx) * (1 - fz) + (h01 * (1 - fx) + h11 * fx) * fz;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! DIG NOW: top of the solid in sample column gi, gk (first solid sample from above, linear between samples;
	//! value > 0 = solid, as Seeded). Cached in m_mSurf until the next go-live.
	protected bool ColSurf(int gi, int gk, out float h)
	{
		int key = (gi & 65535) | ((gk & 32767) << 16);
		if (m_mSurf.Find(key, h))
			return h > -9000;
		h = -9999;
		MUD_VoxelColumn col = ColAt(gi, gk);
		if (!col)
		{
			m_mSurf.Set(key, h);
			return false;
		}
		int li = gi - col.m_iBi * SUB;
		int lk = gk - col.m_iBk * SUB;
		int n = lk * SUB + li;
		float undug = col.m_aUndug[n];
		if (col.m_aHard[n] != 0)
		{
			h = undug;     // never dug: the real ground
			m_mSurf.Set(key, h);
			return true;
		}
		int j1 = Math.Ceil((undug + 2) / CELL);
		int j0 = Math.Floor((undug - TUNNEL_FLOOR - 1) / CELL);     // U1: an open shaft can reach TUNNEL_FLOOR
		float prev = Value(col, li, lk, j1);
		if (prev <= 0)
		{
			for (int j = j1 - 1; j >= j0; j--)
			{
				float v = Value(col, li, lk, j);
				if (v > 0)
				{
					h = j * CELL + v / (v - prev) * CELL;
					break;
				}
				prev = v;
			}
		}
		if (h > -9000)
			h = FloorMin(col, li, lk, h);     // DISPLACED RUTS: the rut floor under the field's top
		m_mSurf.Set(key, h);
		return h > -9000;
	}

	//------------------------------------------------------------------------------------------------
	//! Brick-local y of the seeded ground at brick-local x, z (nearest gathered sample column)
	protected float GroundLocal(float x, float z)
	{
		int li = Math.ClampInt(Math.Round(x / CELL), -1, SUB);
		int lk = Math.ClampInt(Math.Round(z / CELL), -1, SUB);
		return m_Gb.m_aTop[(lk + 1) * GN + li + 1] - m_vBOrg[1];
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 6 grass test: brick-local point p lies within GRASS_TOL of the seeded ground, bilinear between the gathered
	//! sample columns (a quad corner sits between them; the nearest column is off by the slope on sloped ground)
	protected bool OnGround(vector p)
	{
		float fx = p[0] / CELL;
		float fz = p[2] / CELL;
		int i0 = Math.ClampInt(Math.Floor(fx), -1, SUB - 1);
		int k0 = Math.ClampInt(Math.Floor(fz), -1, SUB - 1);
		float tx = Math.Clamp(fx - i0, 0, 1);
		float tz = Math.Clamp(fz - k0, 0, 1);
		int r0 = (k0 + 1) * GN + i0 + 1;
		int r1 = r0 + GN;
		float a = m_Gb.m_aTop[r0] + (m_Gb.m_aTop[r0 + 1] - m_Gb.m_aTop[r0]) * tx;
		float c = m_Gb.m_aTop[r1] + (m_Gb.m_aTop[r1 + 1] - m_Gb.m_aTop[r1]) * tx;
		float g = a + (c - a) * tz - m_vBOrg[1];
		return Math.AbsFloat(p[1] - g) < GRASS_TOL;
	}

	//------------------------------------------------------------------------------------------------
	//! UV plane of a kind (0 grass top, 1 soil along x, 2 soil along z, 3 soil top-down): the two coordinates and
	//! the tile size. Grass top in metres from the skin grid corner, soil in world metres (lines up across bricks).
	protected void KindAB(int kind, vector p, out float a, out float b, out float tile)
	{
		vector o = m_vBOrg;
		if (kind == 0)
		{
			a = o[0] + p[0] - m_fX0;
			b = o[2] + p[2] - m_fZ0;
			tile = TOP_TILE;
		}
		else if (kind == 1)
		{
			a = o[2] + p[2];
			b = o[1] + p[1];
			tile = SOIL_TILE;
		}
		else if (kind == 2)
		{
			a = o[0] + p[0];
			b = o[1] + p[1];
			tile = SOIL_TILE;
		}
		else
		{
			a = o[0] + p[0];
			b = o[2] + p[2];
			tile = SOIL_TILE;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! All 8 corner samples of local cell lc have a value (no column that is not converted)
	protected bool CellOk(int lc)
	{
		int s = m_Gb.m_aOk[lc];
		if (s / 2 == m_iStamp)
			return (s & 1) != 0;
		int li = lc % CN;
		int lj = (lc / CN) % CN;
		int lk = lc / CN2;
		int base = lk * GN2 + lj * GN + li;
		bool ok = true;
		for (int q = 0; q < 8; q++)
		{
			if (m_Gb.m_aG[base + m_aOff[q]] >= MISS)
			{
				ok = false;
				break;
			}
		}
		int bit = 0;
		if (ok)
			bit = 1;
		m_Gb.m_aOk[lc] = m_iStamp * 2 + bit;
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Cells c0..c3 go round the sample edge along axis; solid = the edge's base sample is solid (air is then
	//! along +axis). Faces the air, picks one UV projection from its normal and ONE tile from its middle (UVs
	//! are clamped to 0..1, so a quad over a tile line must not wrap), splits on the shorter diagonal (or, at a
	//! grass edge, away from the one corner off the ground).
	protected void Quad(int c0, int c1, int c2, int c3, vector axis, bool solid)
	{
		if (m_bOverflow)
			return;
		if (m_bAnyMiss && (!CellOk(c0) || !CellOk(c1) || !CellOk(c2) || !CellOk(c3)))
			return;     // at the edge of the converted area: open there (the heightfield strip shows under it)
		vector p0 = CellPos(c0);
		vector p1 = CellPos(c1);
		vector p2 = CellPos(c2);
		vector p3 = CellPos(c3);
		vector air = axis;
		if (!solid)
			air = axis * -1;
		vector n = Cross(p2 - p0, p3 - p1);
		if (vector.Dot(n, air) < 0)
		{
			// reverse the loop: 0 3 2 1
			int ct = c1;
			c1 = c3;
			c3 = ct;
			vector pt = p1;
			p1 = p3;
			p3 = pt;
			n = n * -1;
		}
		float ax = Math.AbsFloat(n[0]);
		float ay = Math.AbsFloat(n[1]);
		float az = Math.AbsFloat(n[2]);
		int proj = 2;
		if (ay >= ax && ay >= az)
			proj = 0;
		else if (ax >= az)
			proj = 1;
		vector mid = (p0 + p1 + p2 + p3) * 0.25;
		// drawn when any corner is in a carved cell: a quad over a cell line (0.25 m each side) must not leave a
		// sliver of the carved cell open; the part over the heightfield sits LIFT above it
		if (!EmitAt(p0) && !EmitAt(p1) && !EmitAt(p2) && !EmitAt(p3))
			return;     // not carved: the heightfield shows there (with its ruts)
		bool flat = proj == 0 && n[1] > 0.7 * n.Length();
		// no grass under a mud-zone sheet tile: the sheet draws its own top look there (user 2026-10-15)
		if (flat && MUD_MudZoneEntity.SheetCovers(m_vBOrg[0] + mid[0], m_vBOrg[2] + mid[2]))
			flat = false;
		bool g0 = flat && OnGround(p0);
		bool g1 = flat && OnGround(p1);
		bool g2 = flat && OnGround(p2);
		bool g3 = flat && OnGround(p3);
		// grass / dirt per TRIANGLE (user 2026-10-17, softer edge): a quad with one corner off the ground splits on the
		// diagonal away from that corner - the 3 on-ground corners draw grass, the other half dirt, so the 0.5 m
		// staircase edge gets 45 degree cuts. A triangle draws grass only with all 3 corners on the ground (as before).
		bool d02 = vector.DistanceSq(p0, p2) <= vector.DistanceSq(p1, p3);
		int onCnt = 0;
		if (g0)
			onCnt++;
		if (g1)
			onCnt++;
		if (g2)
			onCnt++;
		if (g3)
			onCnt++;
		if (onCnt == 3)
			d02 = g0 && g2;     // the off corner is 1 or 3: split 0-2; it is 0 or 2: split 1-3
		bool topA;
		bool topB;
		if (d02)
		{
			topA = g0 && g1 && g2;
			topB = g0 && g2 && g3;
		}
		else
		{
			topA = g0 && g1 && g3;
			topB = g1 && g2 && g3;
		}
		int kindA = QuadKind(proj, topA);
		int kindB = QuadKind(proj, topB);
		int v0;
		int v1;
		int v2;
		int v3;
		QuadV(kindA, mid, c0, c1, c2, c3, v0, v1, v2, v3);
		int w0 = v0;
		int w1 = v1;
		int w2 = v2;
		int w3 = v3;
		if (kindB != kindA)
			QuadV(kindB, mid, c0, c1, c2, c3, w0, w1, w2, w3);
		if (d02)
		{
			Tri(v0, v1, v2, topA);
			Tri(w0, w2, w3, topB);
		}
		else
		{
			Tri(v0, v1, v3, topA);
			Tri(w1, w2, w3, topB);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! UV kind of a quad face: its projection, a flat face that is not grass = 3 (crater floor / cave roof: soil, top-down)
	protected int QuadKind(int proj, bool top)
	{
		if (proj == 0 && !top)
			return 3;
		return proj;
	}

	//------------------------------------------------------------------------------------------------
	//! The 4 quad vertices for one UV kind, all in the ONE tile under the quad middle (UVs are clamped to 0..1)
	protected void QuadV(int kind, vector mid, int c0, int c1, int c2, int c3, out int v0, out int v1, out int v2, out int v3)
	{
		float ma;
		float mb;
		float tile;
		KindAB(kind, mid, ma, mb, tile);
		int ta = Math.Floor(ma / tile);
		int tb = Math.Floor(mb / tile);
		int par = (ta & 1) + 2 * (tb & 1);
		v0 = CellV(c0, kind, par, ta, tb);
		v1 = CellV(c1, kind, par, ta, tb);
		v2 = CellV(c2, kind, par, ta, tb);
		v3 = CellV(c3, kind, par, ta, tb);
	}

	//------------------------------------------------------------------------------------------------
	//! BuildSub: m_aEmit = the look mask (mesh: carved + look cells) or the feel one (body: carved cells)
	protected void UseEmit(bool look)
	{
		for (int i = 0; i < 16; i++)
		{
			if (look)
				m_aEmit[i] = m_aEmitLook[i];
			else
				m_aEmit[i] = m_aEmitFeel[i];
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Brick-local point p lies in a skin cell the brick draws in (m_aEmit)
	protected bool EmitAt(vector p)
	{
		int erx = Math.Floor((m_vBOrg[0] + p[0] - m_fX0) / m_fCell) - m_B.m_iBi * 2 + 1;
		int erz = Math.Floor((m_vBOrg[2] + p[2] - m_fZ0) / m_fCell) - m_B.m_iBk * 2 + 1;
		if (erx < 0 || erx > 3 || erz < 0 || erz > 3)
			return false;
		return m_aEmit[erz * 4 + erx] != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Surface-net vertex of local cell lc (brick local): mean of the crossings on its 12 edges
	protected vector CellPos(int lc)
	{
		if (m_Cb.m_aCs[lc] == m_iStamp)
			return m_Cb.m_aCp[lc];
		int li = lc % CN;
		int lj = (lc / CN) % CN;
		int lk = lc / CN2;
		int base = lk * GN2 + lj * GN + li;
		vector bp = Vector((li - 1) * CELL, (lj - 1) * CELL, (lk - 1) * CELL);
		vector sum = vector.Zero;
		int cnt = 0;
		for (int e = 0; e < 12; e++)
		{
			int qa = m_aEa[e];
			int qb = m_aEb[e];
			float da = m_Gb.m_aG[base + m_aOff[qa]];
			float db = m_Gb.m_aG[base + m_aOff[qb]];
			if ((da > 0) == (db > 0))
				continue;
			float t = da / (da - db);
			vector pa = bp + m_aOffV[qa];
			sum = sum + pa + (bp + m_aOffV[qb] - pa) * t;
			cnt++;
		}
		vector pos = bp + Vector(CELL * 0.5, CELL * 0.5, CELL * 0.5);
		if (cnt > 0)
			pos = sum * (1.0 / cnt);
		m_Cb.m_aCp[lc] = pos;
		m_Cb.m_aCs[lc] = m_iStamp;
		return pos;
	}

	//------------------------------------------------------------------------------------------------
	//! Vertex of local cell lc for UV kind + tile (par = tile parity, ta / tb = tile index), brick local.
	//! UVs 0..1 inside the tile: script-mesh UVs are packed to int16 and CLAMPED to 0..1 (KB
	//! runtime-procedural-mesh.md; runs 3-8 gave > 1 and every wall sampled one texel). Tiling lives in the emat.
	protected int CellV(int lc, int kind, int par, int ta, int tb)
	{
		// two slots per cell + kind, each with its tile parity; a third parity (a tile corner in the cell): uncached
		int key = (lc * 4 + kind) * 2;
		int tag = m_iStamp * 4 + par;
		int freeSlot = -1;
		for (int sl = 0; sl < 2; sl++)
		{
			int val = m_Cb.m_aCv[key + sl];
			int vt = val / VKEY;
			if (vt == tag)
				return val % VKEY;
			if (freeSlot < 0 && vt / 4 != m_iStamp)
				freeSlot = key + sl;
		}
		if (m_iV >= MAX_V)
		{
			m_bOverflow = true;
			return 0;
		}
		vector p = CellPos(lc);
		int v = m_iV;
		m_Vb.m_aV[v] = p;
		float a;
		float b;
		float tile;
		KindAB(kind, p, a, b, tile);
		m_Vb.m_aUV[v * 2] = (a - ta * tile) / tile;
		m_Vb.m_aUV[v * 2 + 1] = 1 - (b - tb * tile) / tile;
		m_iV++;
		if (freeSlot >= 0)
			m_Cb.m_aCv[freeSlot] = tag * VKEY + v;
		return v;
	}

	//------------------------------------------------------------------------------------------------
	//! Already facing the air (Quad orders it); drops only zero-area ones
	protected void Tri(int a, int b, int c, bool top)
	{
		if (m_bOverflow)
			return;
		if (m_iTop + m_iDirt + 3 > MAX_I)
		{
			m_bOverflow = true;
			return;
		}
		if (a == b || b == c || a == c)
		{
			m_iDegen++;
			return;
		}
		vector pa = m_Vb.m_aV[a];
		vector n = Cross(m_Vb.m_aV[b] - pa, m_Vb.m_aV[c] - pa);
		if (n.LengthSq() < 0.000000000001)
		{
			m_iDegen++;
			return;
		}
		if (top)
		{
			m_IbTop.m_aI[m_iTop] = a;
			m_IbTop.m_aI[m_iTop + 1] = b;
			m_IbTop.m_aI[m_iTop + 2] = c;
			m_iTop += 3;
		}
		else
		{
			m_IbDirt.m_aI[m_iDirt] = a;
			m_IbDirt.m_aI[m_iDirt + 1] = b;
			m_IbDirt.m_aI[m_iDirt + 2] = c;
			m_iDirt += 3;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Brick not shown yet: the built buffers onto b.m_Ent (spawned hidden, GoLive shows it). False = no triangles.
	protected bool MakeMesh(MUD_VoxelBrick b)
	{
		if (m_iTop + m_iDirt == 0)
		{
			if (b.m_Ent)
				b.m_Ent.SetObject(null, "");
			b.m_Res = null;
			return false;
		}
		Resource res;
		IEntity ent = MeshEntity(b.m_Ent, true, res);
		if (!ent)
			return false;
		b.m_Ent = ent;
		b.m_Res = res;
		return res != null;
	}

	//------------------------------------------------------------------------------------------------
	//! TRACEABLE off (hide) or back on for every brick; bricks spawned while hidden follow (MeshEntity).
	//! Called by MUD_GroundEntity.HideFromTraces: the native get in / out check sweeps the feet box against
	//! every script entity (user 2026-10-15: can't get in / out in a rut, the bricks were never hidden)
	static void SetTraceHidden(bool hide)
	{
		s_bTraceHidden = hide;
		if (!s_Inst)
			return;
		foreach (int key, MUD_VoxelColumn col : s_Inst.m_mCols)
		{
			foreach (int row, MUD_VoxelBrick b : col.m_mRows)
			{
				if (b.m_Ent)
				{
					if (hide)
						b.m_Ent.ClearFlags(EntityFlags.TRACEABLE, false);
					else
						b.m_Ent.SetFlags(EntityFlags.TRACEABLE, false);
				}
				if (b.m_NewEnt)
				{
					if (hide)
						b.m_NewEnt.ClearFlags(EntityFlags.TRACEABLE, false);
					else
						b.m_NewEnt.SetFlags(EntityFlags.TRACEABLE, false);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The built buffers (not empty) as a mesh on ent (spawned at the brick when null, hidden when asked). A
	//! dedicated server gets a 2-triangle bounds mesh over the vertices instead (MUD_BoundsMesh.c: no mesh = point
	//! bounds = traces skip it). res = the mesh resource (keep it), null on failure. Returns the entity.
	protected IEntity MeshEntity(IEntity ent, bool hidden, out Resource res)
	{
		res = null;
		if (!ent)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[0] = vector.Right;
			params.Transform[1] = vector.Up;
			params.Transform[2] = vector.Forward;
			params.Transform[3] = m_vBOrg;
			ent = GetGame().SpawnEntity(MUD_VoxelSubEntity, GetGame().GetWorld(), params);
			if (!ent)
				return null;
			if (hidden)
				ent.ClearFlags(EntityFlags.VISIBLE, false);
			if (s_bTraceHidden)
				ent.ClearFlags(EntityFlags.TRACEABLE, false);
		}
		if (m_bHeadless)
		{
			vector lo = m_Vb.m_aV[0];
			vector hi = lo;
			for (int v = 1; v < m_iV; v++)
			{
				vector p = m_Vb.m_aV[v];
				lo = Vector(Math.Min(lo[0], p[0]), Math.Min(lo[1], p[1]), Math.Min(lo[2], p[2]));
				hi = Vector(Math.Max(hi[0], p[0]), Math.Max(hi[1], p[1]), Math.Max(hi[2], p[2]));
			}
			res = MUD_BoundsMesh.Make(ent, lo, hi, MAT_DIRT);
			if (!res)
				Print("MUD voxel probe: bounds mesh failed", LogLevel.ERROR);
			return ent;
		}
		res = MakeRes();
		if (!res)
			return ent;
		ent.SetObject(res.GetResource().ToMeshObject(), "");
		ent.Update();
		if (s_iVcLog < 3)
		{
			s_iVcLog++;
			Print(string.Format("MUD vcflags: brick %1 (8 = NOSHADOW)", ent.SetVComponentFlags(0)), LogLevel.NORMAL);
		}
		return ent;
	}

	//------------------------------------------------------------------------------------------------
	//! Wall material of the built buffers: MAT_DARK_1..3 when the whole brick mesh is under ground (lowest ground top at
	//! 9 points over the brick, ColSurf else our heightfield, minus the highest vertex >= DARK_ROOF_1), else MAT_DIRT
	protected ResourceName DirtMat()
	{
		if (!DARK_MAT || m_iV == 0)
			return MAT_DIRT;
		vector p0 = m_Vb.m_aV[0];
		float hiY = p0[1];
		for (int v = 1; v < m_iV; v++)
		{
			vector p = m_Vb.m_aV[v];
			hiY = Math.Max(hiY, p[1]);
		}
		hiY += m_vBOrg[1];
		float roof = 1000000;
		for (int a = 0; a <= 2; a++)
		{
			for (int c = 0; c <= 2; c++)
			{
				float x = m_vBOrg[0] + a * BRICK * 0.5;
				float z = m_vBOrg[2] + c * BRICK * 0.5;
				int gi = Math.Round((x - m_fX0) / CELL);
				int gk = Math.Round((z - m_fZ0) / CELL);
				float top;
				if (!ColSurf(gi, gk, top) && !MUD_WorldGroundEntity.SurfaceY(x, z, top))
					return MAT_DIRT;
				roof = Math.Min(roof, top - hiY);
			}
		}
		if (roof >= DARK_ROOF_3)
			return MAT_DARK_3;
		if (roof >= DARK_ROOF_2)
			return MAT_DARK_2;
		if (roof >= DARK_ROOF_1)
			return MAT_DARK_1;
		return MAT_DIRT;
	}

	//------------------------------------------------------------------------------------------------
	//! The built buffers (not empty) as a new mesh resource (not set on any entity), null on failure (logged)
	protected Resource MakeRes()
	{
		Resource res;
		ResourceName dirt = DirtMat();
		if (m_iTop > 0 && m_iDirt > 0)
		{
			int nv2[] = {m_iV, m_iV};
			int ni2[] = {m_iTop, m_iDirt};
			string mats2[] = {MAT_TOP, dirt};
			res = MeshObject.Create(2, nv2, ni2, mats2, 0);
		}
		else
		{
			int nv1[] = {m_iV};
			int ni1[] = {m_iTop + m_iDirt};
			string mats1[] = {dirt};
			if (m_iTop > 0)
				mats1[0] = MAT_TOP;
			res = MeshObject.Create(1, nv1, ni1, mats1, 0);
		}
		if (!res || !res.GetResource())
		{
			Print("MUD voxel probe: MeshObject.Create failed", LogLevel.ERROR);
			return null;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals + tangents from the triangle list
		if (m_iTop > 0 && m_iDirt > 0)
		{
			mesh.UpdateIndices(0, m_IbTop.m_aI);
			mesh.UpdateVerts(0, m_Vb.m_aV, m_Vb.m_aUV);
			mesh.UpdateIndices(1, m_IbDirt.m_aI);
			mesh.UpdateVerts(1, m_Vb.m_aV, m_Vb.m_aUV);
		}
		else if (m_iTop > 0)
		{
			mesh.UpdateIndices(0, m_IbTop.m_aI);
			mesh.UpdateVerts(0, m_Vb.m_aV, m_Vb.m_aUV);
		}
		else
		{
			mesh.UpdateIndices(0, m_IbDirt.m_aI);
			mesh.UpdateVerts(0, m_Vb.m_aV, m_Vb.m_aUV);
		}
		return res;
	}

	//------------------------------------------------------------------------------------------------
	//! Static body from the built buffers on ent; its geoms go into geoms (destroyed with the body later)
	protected Physics MakeBody(IEntity ent, array<PhysicsGeom> geoms)
	{
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;
		PhysicsGeom gTop;
		PhysicsGeom gDirt;
		Physics phys;
		if (m_iTop > 0)
			gTop = PhysicsGeom.CreateTriMesh(m_Vb.m_aV, m_IbTop.m_aI, m_iV, m_iTop);
		if (m_iDirt > 0)
			gDirt = PhysicsGeom.CreateTriMesh(m_Vb.m_aV, m_IbDirt.m_aI, m_iV, m_iDirt);
		if (gTop && gDirt)
		{
			autoptr PhysicsGeomDef d2[] = {PhysicsGeomDef("", gTop, GAMEMAT, layer), PhysicsGeomDef("", gDirt, GAMEMAT, layer)};
			phys = Physics.CreateStaticEx(ent, d2);
		}
		else if (gTop)
		{
			autoptr PhysicsGeomDef d1[] = {PhysicsGeomDef("", gTop, GAMEMAT, layer)};
			phys = Physics.CreateStaticEx(ent, d1);
		}
		else if (gDirt)
		{
			autoptr PhysicsGeomDef d3[] = {PhysicsGeomDef("", gDirt, GAMEMAT, layer)};
			phys = Physics.CreateStaticEx(ent, d3);
		}
		if (gTop)
			geoms.Insert(gTop);
		if (gDirt)
			geoms.Insert(gDirt);
		if (!phys)
			Print("MUD voxel probe: collider failed", LogLevel.ERROR);
		return phys;
	}

	//------------------------------------------------------------------------------------------------
	protected static vector Cross(vector a, vector b)
	{
		return Vector(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]);
	}
}

#ifdef ENABLE_DIAG
//! DISPLACED RUTS debug ids (Stage B; vanilla pattern SCR_AutotestDebugMenu.c): after vanilla's, must stay < 512
//! (MUD_VoxelProbe.DispMenu checks). MUD_DISP_LOG .. MUD_DISP_HASH in DSW_* order.
modded enum SCR_DebugMenuID
{
	MUD_DISP_MENU_MUD,
	MUD_DISP_MENU,
	MUD_DISP_LOG,
	MUD_DISP_DRAW_FLOOR,
	MUD_DISP_DRAW_BRICKS,
	MUD_DISP_HOLE_CHECK,
	MUD_DISP_HASH
}
#endif
