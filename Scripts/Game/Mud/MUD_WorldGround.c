[EntityEditorProps(category: "Mud", description: "Mud plan 3: deformable ground on the whole holed map - 8 m fine patches swap in over the coarse skin where vehicles drive and the ground moves. Needs MUD_CoarseSkinEntity in the world. One per world.", color: "120 80 40 255")]
class MUD_WorldGroundEntityClass : GenericEntityClass
{
}

//! Plan 3. The coarse skin (MUD_CoarseSkinEntity) fills every hole cell with the exact 2 m heightmap
//! triangles. This entity swaps 8 m fine patches (MUD_FinePatch, 0.25 m verts) in over 4 x 4 skin
//! cells where a moving wheeled vehicle comes near or a stamp (rut, crater, shovel, server message)
//! lands, and covers those skin cells in the same call. A fresh patch lies exactly on the skin
//! triangles: its fine cells take the diagonal of the skin cell they are in. Undeformed patches with
//! no vehicle near for m_fFreeAfter go again; deformed ones stay for the match.
//! Algorithms copied from MUD_GroundEntity, line refs at each method (that file is archived since 2026-10-22:
//! Archived code/2026-10-22-old-ground-entity).
//! One live height message held on a client (not ready yet, or its patch waits for a JIP full copy)
class MUD_NetMsg
{
	int m_iKey;
	int m_iSeq;
	int m_iRect;
	ref array<int> m_aPacked;
}

//! Server: one full patch copy owed to one joining player
//! What an exploding thing is, read once per prefab from its trigger effects (see ReadBlast)
class MUD_BlastInfo
{
	float m_fKg;                  // charge weight x TNT equivalent; 0 = no charge found
	bool m_bHeat;                 // fires a penetrator (HEAT / HEDP)
	bool m_bExplosive = true;     // false = only fire / incendiary damage (burning fuel, tires): no crater
	bool m_bTntGuess;             // TntEquivalent not set: 1 assumed
	bool m_bCharge;               // placed charge (Warhead_ExplosiveCharge_* or a child): pooled; everything else digs at once
}

//! Ahead undo (TEST 2026-10-11): one ahead press the vehicle's wheels have not reached yet. Reached -> kept;
//! still not reached after m_fDue (the vehicle stopped, turned or backed off) -> its verts go back.
class MUD_PendPress
{
	Vehicle m_Veh;                  // weak
	vector m_vPos;
	vector m_vDir;                  // travel direction (flat) when pressed
	float m_fAt;
	float m_fDue;
	ref array<int> m_aGx = {};
	ref array<int> m_aGz = {};
}

//! Body scrape (TEST 2026-10-11): the underside of one vehicle, measured once by tracing up at its own
//! collider. m_aPts = vehicle-local (x, underside y, z) on a grid; wheel spots left out.
class MUD_Belly
{
	Vehicle m_Veh;                  // weak: a deleted vehicle reads null
	ref array<vector> m_aPts = {};
	float m_fFront;                 // local z of the most forward point, m_fBack the most rearward
	float m_fBack;
	float m_fHalfW;                 // largest |local x|
	ref array<int> m_aSpinSign = {};    // ditch escape, per wheel: +1/-1 = sign of WheelGetAngularSpeed driving forward, 0 = not learned
	float m_fEscapeAt = -100;       // ditch escape: m_fTime of the last dig step
	float m_fBrakeHs;               // ahead ruts: flat speed at m_fBrakeAt
	float m_fBrakeAt = -100;        // ahead ruts: m_fTime of the last BrakeCap update
	float m_fDecel;                 // ahead ruts: smoothed slow-down (m/s2, > 0 = slowing)
	int m_iEscapePending;           // ditch escape (Dig On Release): dig steps saved up while spinning
	float m_fStopAt = -100;         // ahead hide: m_fTime this vehicle stopped (-100 = moving)
	int m_iHx0 = -1;                // ahead hide: global fine-vertex box of the last moving frame's hide strips
	int m_iHx1;
	int m_iHz0;
	int m_iHz1;
	float m_fRampAt = -100;         // approach ramp: m_fTime of the last cut step
	vector m_vHideDir;              // ahead hide: travel direction of the last moving frame
	int m_iKx0;                     // ahead hide: box kept hidden since the vehicle stopped
	int m_iKx1;
	int m_iKz0;
	int m_iKz1;
	vector m_vKeepDir;              // ... the direction it was going
	float m_fKeepSince;             // ... m_fTime it stopped
	float m_fKeepUntil = -100;      // ... kept until (or until it drives on the same way)
	vector m_vEscapeDir;            // ditch escape: flat direction the wheels last pushed
	ref array<vector> m_aEscapeLips = {};   // ditch escape: lip points (+ contact Y) the wheels pushed at the last saved step
}

class MUD_JipItem
{
	int m_iPid;
	int m_iKey;
	float m_fSendAt;
	bool m_bCatchUp;              // send by distance: the player came near a patch changed while far
}

class MUD_WorldGroundEntity : GenericEntity
{
	static const int PCELLS = 32;       // fine cells per patch side
	// RUT FIELD: a field rut vertex shows the groove (soil) only this deep under the undug ground - v097's brick rule
	// (MUD_VoxelProbe.DISP_SOIL_CUT under the seeded top, which is LIFT over the ground). The reveal's own 0.01 m
	// (tuned for one-pass heightfield ruts) made the whole 1 m wide trough soil: ruts looked ~2x wider (user 2026-10-15)
	// DIRT HIGHER (user 2026-10-21 'move dirt very close to the grass, like 0.5 cm'; was 0.06): the first cut (pass 2)
	// shows broken dirt (ragged noise + blend), full dirt by pass 3. Pass 1 still only wears (no cut, nothing drawn).
	static const float FIELD_SOIL_D = 0.005;    // WEAR COUNT 2026-10-19 (v233 = before): pass 1 only wears (MUD_VoxelProbe.WEAR_PASSES 1), passes 2-3 cut 0.03 each = dirt on pass 3 as with 0.08 before (+ ragged noise up to RAG_D); was 0.08, 0.03
	// RAGGED EDGE (2026-10-18, user: dirt pops in with no transition grass -> dirt): the groove reveal threshold gets
	// world-fixed noise (same value every rebuild and on both sides of a patch seam), so a rut rim turns to dirt in
	// broken patches instead of one clean line. Each vertex needs RevealDepth + n * RAG_D deeper (n 0..1) and the
	// band is RAG_BLEND wider. Noise = soft blobs RAG_CELLS fine cells big + RAG_WHITE share of per-vertex speckle.
	// No extra mesh builds. false = v213 reveal.
	// PASS 2 DIRT (research 2026-10-21 Q8, v277 = before): with 0.035 / 0.03 dirt showed only where c > 0.0285 + 0.035 n,
	// so a 3 cm pass-2 cut showed ~no dirt. 0.012 / 0.01: dirt where c > 0.0207 + 0.012 n = ~80 % of a pass-2 rut
	static const bool RAGGED = true;
	static const float RAG_D = 0.012;      // was 0.035
	static const float RAG_BLEND = 0.01;   // was 0.03
	// WEAR SHOW (research 2026-10-21 Q9 option 1, v277 = before): a wear-only pass (pass 1, no cut) shows the groove dirt
	// layer as a ragged strip, look only: a worn vertex's reveal input goes WEAR_SHOW_D x wear past RevealDepth (no height
	// or collider change; the wear is J2 synced state, so every machine draws the same). The wear pass marks the patch
	// mesh dirty on render machines (PressWearMark). With RAG_D 0.012 + blend 0.04: dirt where n < ~0.78.
	// false = pass 1 draws nothing (v277)
	static const bool WEAR_SHOW = true;
	// SOLID STRIP (user 2026-10-21 'ruts very spotty until a few passes', v279 = before): was 0.025 = reveal input 0.035,
	// inside the ragged band (thr 0.01 + 0.012 noise, blend 0.04) -> ~1 vertex in 5 grass at random. 0.06 = 0.07 = full
	// dirt at any noise. Half a pass (one RUT_RAMP op of two) already counts full (WEAR_SHOW_K x wear, max 1).
	static const float WEAR_SHOW_D = 0.06;
	static const float WEAR_SHOW_K = 2;
	// MUD RING (user 2026-10-21 'some with a small mud ring', v278 = before): undug or raised ground (crater rim, the grass
	// round a hole, trench edges) within RING_REACH m of ground dug deeper than RING_DUG shows the groove dirt, full next
	// to the hole, ragged out to grass (RING_SHOW_D x fade past RevealDepth). Look only, from the synced heights alone
	// (no new net state): every machine draws the same. false = rims and edges grass.
	// CLUTTER ON THE SEAM (user 2026-10-21 'clutter round the seam hides it, looks way better', v279 = before): grass
	// clutter grows only where the DRAWN ground is >= 1 mm over the heightmap (mud-gotchas 2026-10-08 + the look lift
	// 2026-10-19). The reveal normally sinks the surface 3 x RevealGap under the dirt layer = no clutter; ring vertices
	// instead keep the surface and lift the dirt layer by the same amount (dirt over the surface, clutter through it).
	static const bool RING_DIRT = true;
	static const float RING_REACH = 0.85;     // m from the nearest dug vertex: dirt fades to none here
	static const int RING_REACH_V = 3;        // fine steps searched (= RING_REACH / 0.25 m, rounded down)
	static const float RING_DUG = 0.05;       // m: a vertex this far under its base counts as dug
	static const float RING_MIN_H = -0.0005;  // m: undug or raised vertices only (dug ones are the hole: no clutter there)
	static const float RING_SHOW_D = 0.03;    // core reveal input 0.04: solid dirt, dirt layer 1-5 cm over the surface
	static const int RING_EXT = 39;           // PVERTS + 2 x RING_REACH_V
	protected int m_aRingDug[1521];           // RING_EXT x RING_EXT scratch: dug (1 / 0), patch + RING_REACH_V round it. int, not bool: member bool arrays read back wrong (mud-gotchas, audit F4)
	protected int m_aRingRow[1521];           // nearest dug in the same row (steps, 99 = none)
	protected int m_aRingD2[1089];            // per patch vertex: squared steps to the nearest dug (99 = none)
	static const float RAG_CELLS = 3;
	static const float RAG_WHITE = 0.35;
	// RUT FIELD settle (review F1 2026-10-16): a patch whose only dirt is rut floor waits until it had no new floor
	// dirt for FLOOR_QUIET s (or FLOOR_MAX_WAIT s since the first) before mesh + collider rebuild - one build per
	// crossing instead of one per rut op (~4 / s, 2 patches each, log 21-35-43). Ruts are laid behind the wheels.
	// false = rebuild at once (v100 .. v108).
	// OFF 2026-10-18 (user: rear wheel floats then drops at speed; v212 = before): with the real cut AT the wheel
	// (MUD_VoxelProbe.RUT_BEHIND false) ops never go quiet while driving, so the collider waited up to 1.5 s
	// (WB log 21:43:33: 12 max-wait releases, 1383 held patch-frames in 10 s). Rebuild cost: judge on a live server.
	static const bool FLOOR_SETTLE = false;
	// RENDER GATE (perf quick win 2026-10-18, research/2026-10-18-perf-research-ida-web.md Q4): a client patch mesh
	// rebuild waits while the patch was not drawn last frame (IEntity.IsRendered, needs EntityFlags.SET_RENDERED, set at
	// spawn on rendering machines) and is over RENDER_NEAR m from the camera. Only patches that already have a mesh;
	// colliders never wait. Turning round: the old mesh shows a frame, then the queue builds it. false = off.
	static const bool RENDER_GATE = true;
	static const float RENDER_NEAR = 25;
	// LOOK COVERS (perf quick win 2026-10-18, research/2026-10-18-perf-assessment.md item 4): a server rut op whose
	// changed floor samples all lie at or under (CATCH) a client look sample, with no ridge there, changes nothing the
	// mesh draws (MeshFloor = min(real, look)): only the collider rebuilds, the mesh keeps. false = rebuild both.
	// WB 22:27 run: 'rut ops mesh kept 0' in every 10 s window (inference: each rut op also changes ridge / edge samples
	// the look never draws) - no gain, so off.
	static const bool LOOK_COVERS = false;
	static const float FLOOR_QUIET = 0.4;
	static const float FLOOR_MAX_WAIT = 1.5;
	// FLOOR SPREAD (2026-10-18, results/2026-10-18-h1-snap-probe.md run 4): a rut floor change on a patch still building
	// used to finish it in the op (FloorFinish -> FinishNow) = every 10-13 ms 'dirty' spike (25 / 25 client, 24 / 24
	// server). Now it gets one build step per frame (FloorSpreadTick). Ruts land 2+ m behind the wheels.
	// false = finish at once as before.
	static const bool FLOOR_SPREAD = true;
	static const bool LOOK_SPREAD = true;     // client look ruts finish lazy / building patches too (MarkLookDirty); false = off
	protected ref array<int> m_aFloorSpread = {};     // patch keys: building, a rut floor reached them
	protected int m_iFloorSpreadAdds;                 // log (10 s 'MUD world: budget' line)
	protected int m_iLookSpreadAdds;                         // LOOK_SPREAD: patches the look ruts sent to the floor spread, per 10 s
	protected int m_iFloorSpreadSteps;
	// LAZY RUT MESH (MUD_MudSheet.LAZY_RUT_MESH): s a patch's 'covered by the mud sheet' answer is kept (a cover change
	// in the sheet - MUD_MudSheet.CoverGen - asks again at once)
	static const float SHEET_CHECK = 0.25;
	// RUT FIELD patch memory (review Stage 4, 2026-10-16): a floor-only patch (m_bFloor, not m_bDeformed) holds nothing
	// the rut tiles don't, so FreeIdle frees it like an undeformed one once nothing is near (FieldDue keeps the ones in
	// range); FieldDue rebuilds it when a camera / player / vehicle comes near. false = kept for the match (v100 .. v111).
	static const bool FLOOR_FREE = true;
	static const int PVERTS = 33;
	static const int SKIN = 4;          // skin cells per patch side
	static const int FINE = 8;          // fine cells per skin cell side
	static const float RESCUE_DEPTH = 0.7;  // feet this far under our ground (and collider): lift back on top. Prone sat up
	                                        // to ~0.3 m under while fine (fall probe 2026-10-10); a real fall keeps going
	static const int MAT_SLOTS = 4;    // surface pieces per collider; + bullet slab + safety plate = 6 (MakeStatic max)
	static const int TLBR = 16;         // MUD_FinePatch.m_aCell bit: the skin cell's shared edge is tl-br
	static const int VOX = 32;          // MUD_FinePatch.m_aCell bit, probe 2026-10-13 (MUD_VoxelProbe overlay): a live
	                                    // voxel tile carries the cell - no patch mesh, no patch collider there
	static const int LOOK = 64;         // MUD_FinePatch.m_aCell bit, TEMP probe 2026-10-14 (MUD_VoxelProbe RUT_LOOK): a look rut
	                                    // draws there (voxel mesh) - no patch mesh, the patch COLLIDER stays (until VOX)
	static const int IDX_SETS = 16;     // cached index layouts, MUD_IdxSet (~120-200 KB each)
	static const int SLOT = 15;
	static const bool RUT_MUD = true;   // Build 3 Stage 4: rut cells get m_RutMudMaterial on the grid collider (false = as before)
	static const float RUT_MAT_D = 0.06;    // 2026-10-21: kept at 0.06 when FIELD_SOIL_D went to 0.005 (dirt look earlier, grip unchanged = pass 3). WEAR COUNT 2026-10-19: = FIELD_SOIL_D, mud grip where the dirt shows (was 0.08, 0.05). a 2 m skin cell is rut mud where a vertex's rut floor is this far under its base
	// MUD_FinePatch.m_iBuild: steps a patch made ahead still needs, one per frame (Fix B, 2026-10-09)
	static const int BUILD_INDEX = 3;       // index lists (a cache miss is ~2.5 ms)
	static const int BUILD_COLLIDER = 2;    // collider; the skin stays until it exists
	static const int BUILD_COVER = 1;       // mesh + skin cover in one frame (no z-fight, no gap)
	static const int MAX_WAIT = 2;          // frames a job may wait on the frame budget, then it runs if it fits m_iFrameCap
	// worst cost (ms) of a budgeted job, for the m_iFrameCap check (measured 2026-10-09 14:46-14:49 Exp 192624)
	static const int COST_STEP = 4;         // ahead step: index 3-5, collider ~2, cover ~2-4
	static const int COST_INDEX = 4;        // index-list cache miss
	static const int COST_MESH = 1;
	static const int COST_COLLIDER = 4;     // smooth 1-2 + collider 1-2
	static const int COST_FREE = 3;
	static const int WHEEL_BUILDS_PER_FRAME = 1;    // Fix C: 2 wheel-made patches in one frame = 22-31 ms (2026-10-09)

	[Attribute("{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat", UIWidgets.ResourcePickerThumbnail, "Surface material (terrain look, same as the skin). Must load or no mesh is built.", "emat", category: "Ground")]
	protected ResourceName m_Material;

	[Attribute("{6EBF7ADA9FD582F7}Assets/Structures/Military/Fortifications/DirtMounds/Data/DirtCover_01.emat", UIWidgets.ResourcePickerThumbnail, "Groove material: a layer just under the surface that shows where ruts / craters go deeper than Reveal Depth. Empty = off.", "emat", category: "Ground")]
	protected ResourceName m_RutMaterial;

	[Attribute("0.01", UIWidgets.EditBox, "Deeper than this (m) shows the groove material. 0.01 (user 2026-10-11): dirt shows from the first wheel pass (was 0.06: needed 2-3 passes)", category: "Ground")]
	protected float m_fRevealDepth;

	[Attribute("0.03", UIWidgets.EditBox, "Depth band (m) over which the surface sinks under the groove layer. 0.03: a one-pass rut edge (0.04 deep) is full dirt too (was 0.1)", category: "Ground")]
	protected float m_fRevealBlend;

	[Attribute("0.03", UIWidgets.EditBox, "Groove layer this far under the surface (m)", category: "Ground")]
	protected float m_fRevealGap;

	[Attribute("{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat", UIWidgets.ResourcePickerThumbnail, "Game material of the safety plate and the bullet slab fallback", "gamemat", category: "Ground")]
	protected ResourceName m_GameMaterial;

	[Attribute("asphalt,concrete", UIWidgets.EditBox, "Hard surfaces: a game material whose path contains one of these words (comma separated) gets no ruts and no shovel. Explosions still crater it.", category: "Ground")]
	protected string m_sHardWords;

	[Attribute("{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat", UIWidgets.ResourcePickerThumbnail, "Game material a hard cell turns into where an explosion broke it", "gamemat", category: "Ground")]
	protected ResourceName m_CraterMaterial;

	[Attribute("{2BCBC124175231A2}Common/Materials/Game/mud_rut.gamemat", UIWidgets.ResourcePickerThumbnail, "Build 3 Stage 4: game material of rut cells (a 2 m cell whose rut floor is RUT_MAT_D under the ground), collider only. Empty = off.", "gamemat", category: "Ground")]
	protected ResourceName m_RutMudMaterial;
	// Build 3 Stage 4 fix (2026-10-17): holds the rut mud gamemat loaded. Nothing else keeps a mod gamemat alive, and the
	// 15:15 log showed the wheels' contact material as a released BaseResourceObject (GetVehicleInfo null)
	protected ref Resource m_RutMudRes;

	[Attribute("0", UIWidgets.ComboBox, "Locked border: verts at kept terrain never move, digging fades in over Edge Fade. Dirt wall: dig to the edge, a wall fills the gap.", "", ParamEnumArray.FromEnum(MUD_EEdgeMode), category: "Edges")]
	protected MUD_EEdgeMode m_eEdgeMode;

	[Attribute("1", UIWidgets.EditBox, "Locked border: depth fades from 0 at kept terrain to full over this distance (m)", category: "Edges")]
	protected float m_fEdgeFade;

	[Attribute("-1", UIWidgets.EditBox, "Building lock: grow (+) or shrink (-) a building's box (m). Inside it the ground never moves.", category: "Edges")]
	protected float m_fBuildingMargin;

	[Attribute("16", UIWidgets.EditBox, "Make patches ahead within this radius (m) of a moving wheeled vehicle", category: "Patches")]
	protected float m_fAheadRadius;

	[Attribute("1", UIWidgets.EditBox, "A vehicle counts as moving above this speed (m/s)", category: "Patches")]
	protected float m_fAheadSpeed;

	[Attribute("1", UIWidgets.EditBox, "Build steps per frame for patches made ahead. A new patch takes 3-4 steps (setup ~2.9 ms, index lists ~2.5 ms on a cache miss, collider ~1.4 ms, mesh + skin cover ~2.8 ms; 2026-10-09). Stamps build theirs at once.", category: "Patches")]
	protected int m_iAheadPerFrame;

	[Attribute("1", UIWidgets.CheckBox, "Lazy patches (O1): a patch made ahead stops after setup + index lists and the skin keeps carrying it. Its first rut / crater / dig builds collider, mesh and skin cover at once. ~70 % of patches made ahead never got a rut (task 5 drive). Off = build every patch made ahead in full.", category: "Patches")]
	protected bool m_bLazy;

	[Attribute("300", UIWidgets.EditBox, "Client only: a server change on a patch farther than this (m) from the camera keeps only its heights (no collider, mesh or skin cover; the ruts / crater do not show yet). The patch builds when the camera comes within the radius. 0 = build every change at once.", category: "Patches")]
	protected float m_fClientBuildRadius;

	[Attribute("4", UIWidgets.EditBox, "Frame budget (ms): once the frame's ground work passed this, rebuilds, ahead steps and frees wait for a later frame (after 3 frames a job runs if it fits Frame Cap). Wheels always run.", category: "Patches")]
	protected int m_iFrameBudget;

	[Attribute("9", UIWidgets.EditBox, "Frame cap (ms): a job that waited its 3 frames still waits while it would push the frame's ground work past this. Wheels always run.", category: "Patches")]
	protected int m_iFrameCap;

	[Attribute("60", UIWidgets.EditBox, "An undeformed patch with no vehicle within the radius for this long (s) goes again", category: "Patches")]
	protected float m_fFreeAfter;

	[Attribute("30", UIWidgets.EditBox, "A deformed patch with no vehicle within the radius for this long (s) sleeps: its collider goes (~270 KB), the skin collider comes back under it, its mesh stays (ruts still show). A vehicle near wakes it.", category: "Patches")]
	protected float m_fSleepAfter;

	[Attribute("12", UIWidgets.EditBox, "A player (on foot or in a vehicle) within this radius (m) keeps deformed patches awake and wakes sleeping ones, so the ruts under their feet are solid. 0 = only vehicles count.", category: "Patches")]
	protected float m_fPlayerRadius;

	[Attribute("2", UIWidgets.EditBox, "Sleeping patches wake within the ahead radius or within this many seconds of travel (speed x seconds) of a vehicle, whichever is larger: a fast truck meets its ruts and craters awake", category: "Patches")]
	protected float m_fWakeLead;

	[Attribute("2000", UIWidgets.EditBox, "Warn once when this many patches are deformed (kept for the match; live ~340-400 KB, asleep less)", category: "Patches")]
	protected int m_iBudget;

	[Attribute("4", UIWidgets.EditBox, "Max Depth: nothing digs deeper than this below the undeformed surface (m). Depth boxes override it.", category: "Floor")]
	protected float m_fMaxDepth;

	[Attribute("0.5", UIWidgets.EditBox, "Safety plate this far under the deepest floor (m)", category: "Floor")]
	protected float m_fPlateGap;

	[Attribute("1.5", UIWidgets.EditBox, "Bullet slab this far under the collider (m), FireGeo only, facing down. 0 = off.", category: "Floor")]
	protected float m_fBulletSlab;

	[Attribute("0.1", UIWidgets.EditBox, "Min seconds between mesh rebuilds of a patch", category: "Rebuild")]
	protected float m_fMeshInterval;

	[Attribute("0.2", UIWidgets.EditBox, "Min seconds between collider rebuilds of a patch (and net send interval)", category: "Rebuild")]
	protected float m_fColliderInterval;

	[Attribute("2", UIWidgets.Slider, "Collider smoothing radius in fine steps (0 = off, 2 = 1.25 m)", "0 3 1", category: "Rebuild")]
	protected int m_iColliderSmooth;

	[Attribute("0.08", UIWidgets.EditBox, "How far a wheel sinks the ground per pass (m). 0.08 (user 2026-10-10: 0.05 too slow, 0.12 rough + worse MP stutter)", category: "Ruts")]
	protected float m_fRutStep;

	[Attribute("1", UIWidgets.EditBox, "(1 = release; was 3 for tests until 2026-10-11 17:55, ruts came in instantly): Rut Step and the berm step x this, so ruts reach full depth in fewer passes", category: "Ruts")]
	protected float m_fRutSpeed;

	[Attribute("0.35", UIWidgets.EditBox, "Deepest any rut gets below the undeformed surface (m), however heavy the vehicle", category: "Ruts")]
	protected float m_fRutMaxDepth;

	[Attribute("0.08", UIWidgets.EditBox, "Rut depth (m) = this + vehicle mass in tons x Rut Depth Per Ton, capped at Rut Max Depth", category: "Ruts")]
	protected float m_fRutBaseDepth;

	[Attribute("0.025", UIWidgets.EditBox, "Extra rut depth per ton of vehicle mass (m)", category: "Ruts")]
	protected float m_fRutDepthPerTon;

	[Attribute("0.5", UIWidgets.EditBox, "Rut radius around a wheel contact (m). Keep at least 2x the fine step (0.25 m).", category: "Ruts")]
	protected float m_fRutRadius;

	[Attribute("0.8", UIWidgets.EditBox, "Share of raised ground (crater rim, berm) a wheel plows away per pass, 0-1", category: "Ruts")]
	protected float m_fRimCut;

	[Attribute("0.9", UIWidgets.EditBox, "Outer radius of the berm pushed up beside a rut (m)", category: "Ruts")]
	protected float m_fBermRadius;

	[Attribute("0.08", UIWidgets.EditBox, "Max berm height above the undeformed surface (m)", category: "Ruts")]
	protected float m_fBermHeight;

	[Attribute("0", UIWidgets.EditBox, "Wheel hold (P1a, off 2026-10-11 17:55 - user: wheels floated over the ruts, as in the 2026-10-10 copy): a vertex a wheel touched within this many seconds keeps its old collider height; the new height goes in at the first collider rebuild after the wheel left. The ground never moves under a wheel, on any machine (MP stutter). 0 = off.", category: "Ruts")]
	protected float m_fWheelHold;

	[Attribute("0.6", UIWidgets.EditBox, "A vertex must be free of wheels this long (s) before the next touch counts as a new pass", category: "Ruts")]
	protected float m_fPassGap;

	[Attribute("0.5", UIWidgets.EditBox, "A wheel contact more than this (m) above or below our ground is on something else (bridge, rubble, a truck bed): no rut", category: "Ruts")]
	protected float m_fPressHeight;

	// Ahead ruts (TEST 2026-10-11): the server presses each wheel's rut a lead distance ahead in the direction of
	// travel (forward or reverse), so the collider is final before the wheel gets there - the ground never changes
	// under a wheel. The mesh hides those changes until the wheel is within Reveal Ahead.
	[Attribute("1", UIWidgets.CheckBox, "TEST: press ruts ahead of the wheels instead of under them (off = old behaviour)", category: "Ahead ruts")]
	protected bool m_bAheadRuts;

	[Attribute("2", UIWidgets.EditBox, "Lead (m) at any speed: the press lands at least this far ahead of the wheel", category: "Ahead ruts")]
	protected float m_fAheadMin;

	[Attribute("0.5", UIWidgets.EditBox, "Extra lead per m/s (s): lead = Min + speed x this. Must cover collider interval + net delay (~0.3-0.4 s)", category: "Ahead ruts")]
	protected float m_fAheadTime;

	[Attribute("8", UIWidgets.EditBox, "Lead never longer than this (m)", category: "Ahead ruts")]
	protected float m_fAheadMax;

	[Attribute("1.8", UIWidgets.EditBox, "Skip a press that lands within this (m, flat) of any wheel of the same vehicle - e.g. the rear wheel's point under the front wheel. Berm radius + collider blur + tyre.", category: "Ahead ruts")]
	protected float m_fAheadClear;

	[Attribute("0.6", UIWidgets.EditBox, "The mesh shows the rut this far (m) in front of the wheel; farther ahead it stays hidden", category: "Ahead ruts")]
	protected float m_fRevealAhead;

	[Attribute("0", UIWidgets.CheckBox, "Draw the ahead points (off by default since 2026-10-14): green sphere = pressed, red = skipped (too close to a wheel), yellow = reveal point, text = lead + speed", category: "Ahead ruts")]
	protected bool m_bAheadDraw;

	[Attribute("1", UIWidgets.CheckBox, "Only the frontmost and rearmost axle press ruts (6-wheel truck: the middle axle sits right next to the rear one). Every wheel still counts for Ahead Clear.", category: "Ahead ruts")]
	protected bool m_bEndAxles;

	[Attribute("1", UIWidgets.CheckBox, "Rut fill: the trailing axle (rear going forward, front in reverse) cannot press ahead - its path runs under the other wheels. It fills its own track up to one front pass (never deeper), at the point between the axles farthest from every wheel. Draw: blue sphere (grey = no clear point).", category: "Ahead ruts")]
	protected bool m_bRutFill;

	[Attribute("1.2", UIWidgets.EditBox, "Rut fill: the fill point must be at least this far (m) from every wheel contact, else it is skipped", category: "Ahead ruts")]
	protected float m_fFillClear;

	[Attribute("1", UIWidgets.CheckBox, "Rut fill only on the front track: fills only where the ground 1-2 m before and after the fill point is already rutted (a gap in the front rut). In a turn the rear wheels cut inside the front track - no second rut there (user 2026-10-11: it made one blob)", category: "Ahead ruts")]
	protected bool m_bFillTrackOnly;

	[Attribute("1", UIWidgets.CheckBox, "Ahead undo: an ahead press the wheels never reach (vehicle stopped, turned, backed off) is put back after its due time. Clients keep the ground hidden meanwhile.", category: "Ahead ruts")]
	protected bool m_bAheadUndo;

	[Attribute("1.5", UIWidgets.EditBox, "Ahead undo: extra seconds after the expected arrival before a press counts as never reached", category: "Ahead ruts")]
	protected float m_fUndoGrace;

	[Attribute("12", UIWidgets.EditBox, "Ahead undo: seconds a stopped vehicle's hidden strips stay hidden on clients (longer than any due time)", category: "Ahead ruts")]
	protected float m_fStopHide;

	// Body scrape (TEST 2026-10-11, step 1): the vehicle's underside, moved the same lead ahead, cuts any ground
	// that would touch it (rut crown, berm, crater rim, small bank). Done ahead, so the body never meets ground
	// that is still changing. Needs Ahead Ruts on.
	[Attribute("1", UIWidgets.CheckBox, "TEST: the underside, a lead ahead, scrapes off ground that would touch it", category: "Body scrape")]
	protected bool m_bScrape;

	[Attribute("0.05", UIWidgets.EditBox, "Ground is cut to this far (m) under the underside. Smaller = the belly brushes the ground (more stutter); negative = it rubs", category: "Body scrape")]
	protected float m_fScrapeMargin;

	[Attribute("0.05", UIWidgets.EditBox, "A scrape lowers the ground at most this much (m) per pass (a pass ends when no underside point was over it for Pass Gap). Ruts sink Rut Step per wheel pass.", category: "Body scrape")]
	protected float m_fScrapeStep;

	[Attribute("0.4", UIWidgets.EditBox, "A scrape never cuts deeper than this (m) below the undeformed ground: big banks stay (ditch escape is step 2)", category: "Body scrape")]
	protected float m_fScrapeMaxDepth;

	[Attribute("0.6", UIWidgets.EditBox, "Ground more than this (m) above the predicted underside is not scraped (a wall, or a bad tilt guess)", category: "Body scrape")]
	protected float m_fScrapeReach;

	[Attribute("0.35", UIWidgets.EditBox, "Radius (m) each underside point cuts", category: "Body scrape")]
	protected float m_fScrapeRadius;

	[Attribute("0.5", UIWidgets.EditBox, "Underside grid step (m) - measured once per vehicle", category: "Body scrape")]
	protected float m_fBellyStep;

	[Attribute("0", UIWidgets.CheckBox, "Draw the underside points (off by default since 2026-10-14) a lead ahead: orange = cutting ground, cyan = clear", category: "Body scrape")]
	protected bool m_bScrapeDraw;

	// Approach ramp (TEST 2026-10-11): a bumper hitting a crater lip stops the truck dead (engine collision). The
	// line from the leading tyre contact through the bottom of the bumper (approach angle; departure in reverse)
	// is carried on ahead of the bumper, and our ground above it is cut down, a step at a time, before the
	// bumper gets there: the lip becomes a ramp the bumper clears.
	[Attribute("1", UIWidgets.CheckBox, "TEST: cut ground above the approach-angle line ahead of the bumper (departure angle in reverse)", category: "Approach ramp")]
	protected bool m_bRamp;

	[Attribute("0.05", UIWidgets.EditBox, "Ground is cut to this far (m) under the line", category: "Approach ramp")]
	protected float m_fRampMargin;

	[Attribute("0.05", UIWidgets.EditBox, "Each step lowers the ground at most this much (m)", category: "Approach ramp")]
	protected float m_fRampStep;

	[Attribute("0.25", UIWidgets.EditBox, "Seconds between steps (0.05 m every 0.25 s = 0.2 m/s)", category: "Approach ramp")]
	protected float m_fRampInterval;

	[Attribute("0.5", UIWidgets.EditBox, "The ramp starts this far (m) in front of the bumper (never right under it)", category: "Approach ramp")]
	protected float m_fRampStart;

	[Attribute("3", UIWidgets.EditBox, "...and reaches this far (m) past the start (at most the ahead lead)", category: "Approach ramp")]
	protected float m_fRampLength;

	[Attribute("1", UIWidgets.EditBox, "Never deeper than this (m) below the undeformed ground", category: "Approach ramp")]
	protected float m_fRampMaxDepth;

	[Attribute("1", UIWidgets.EditBox, "Ground more than this (m) above the line is not cut (a real wall stays)", category: "Approach ramp")]
	protected float m_fRampReach;

	[Attribute("0", UIWidgets.CheckBox, "Draw the ramp line (off by default since 2026-10-14) (magenta) and the points it cuts", category: "Approach ramp")]
	protected bool m_bRampDraw;

	// Ditch escape (TEST 2026-10-11, step 2): a stuck vehicle (throttle on, wheels spinning, barely moving) digs
	// slowly at the ground in front of its body and at the lips its wheels push against. It is stopped, so this
	// change is NOT done ahead: expect some snaps for the driver while it digs.
	[Attribute("1", UIWidgets.CheckBox, "TEST: a stuck vehicle (throttle, wheels spinning, barely moving) digs its way out slowly", category: "Ditch escape")]
	protected bool m_bEscape;

	[Attribute("1", UIWidgets.EditBox, "Stuck = flat speed below this (m/s)...", category: "Ditch escape")]
	protected float m_fEscapeSpeed;

	[Attribute("1", UIWidgets.EditBox, "...and the fastest wheel's rim spins this much (m/s) faster than the vehicle moves", category: "Ditch escape")]
	protected float m_fEscapeSlip;

	[Attribute("0.5", UIWidgets.EditBox, "Seconds between dig steps while stuck", category: "Ditch escape")]
	protected float m_fEscapeInterval;

	[Attribute("0.03", UIWidgets.EditBox, "Each dig step lowers the ground at most this much (m)", category: "Ditch escape")]
	protected float m_fEscapeStep;

	[Attribute("1", UIWidgets.CheckBox, "Save the dig steps up while the wheels spin and dig them all when the driver lets off the throttle (rocking). The ground then never changes under a pushing wheel: server and driver agree on the next push. Off = dig while spinning (DS 2026-10-11 18:57: the driver twice saw itself drive out ~0.85 m and was snapped back)", category: "Ditch escape")]
	protected bool m_bEscapeOnRelease;

	[Attribute("8", UIWidgets.EditBox, "Dig On Release: at most this many steps are saved up per push", category: "Ditch escape")]
	protected int m_iEscapeMaxPending;

	[Attribute("0.3", UIWidgets.EditBox, "The underside digs this far (m) ahead in the spin direction", category: "Ditch escape")]
	protected float m_fEscapeLead;

	[Attribute("1", UIWidgets.EditBox, "Digging never goes deeper than this (m) below the undeformed ground", category: "Ditch escape")]
	protected float m_fEscapeMaxDepth;

	[Attribute("1", UIWidgets.EditBox, "Ground more than this (m) above the underside is not dug (a real wall or hill stays)", category: "Ditch escape")]
	protected float m_fEscapeReach;

	[Attribute("1.5", UIWidgets.EditBox, "Crater depth (m) for 1 kg TNT (user 2026-10-21 late, sheet parked: back to the big preset 1.5 - 81 mm mortar ~1.43 m deep, ~6 m bowl; half size 0.75 was for the sheet; tried middle 0.8, mix 1.1). Depth = this x (charge weight x TNT equivalent in kg) ^ TNT Power up to Max Depth, read from the exploding thing (ammo -> warhead, mine, charge, vehicle blast).", category: "Craters")]
	protected float m_fCraterDepthPerKg;

	[Attribute("0.6", UIWidgets.EditBox, "TNT Power: how fast depth grows with charge (old big preset; tried middle 0.45, mix 0.5). 0.33 = textbook cube root.", category: "Craters")]
	protected float m_fCraterTntPower;

	[Attribute("1.35", UIWidgets.EditBox, "Knee of the size curve (m): bigger charges grow only with the cube root past it (user 2026-10-21 late: big preset again; half size 0.7 = 81 mm mortar 0.99 kg -> 0.74 m; old big preset: 81 mm mortar 0.99 kg -> 1.43 m, Hydra M229 2.9 kg -> 2.04 m, TM-62M 7.5 kg -> 2.80 m).", category: "Craters")]
	protected float m_fCraterMaxDepth;

	[Attribute("0.5", UIWidgets.EditBox, "TNT (kg) for exploding ammo whose warhead has no readable charge (modded ammo)", category: "Craters")]
	protected float m_fCraterFallbackTnt;

	[Attribute("0.3", UIWidgets.EditBox, "No crater at or below this TNT (kg): smoke 0.015-0.1, 40 mm 0.04-0.08, AP mines 0.03-0.05, hand grenades 0.11-0.24 (too small to see, user 2026-10-10: save the cost). TNT 400 g and rockets still dig.", category: "Craters")]
	protected float m_fCraterMinTnt;

	[Attribute("3", UIWidgets.EditBox, "Deepest any single blast digs (m): AT mines, vehicle ammo racks and fuel blasts reach it.", category: "Craters")]
	protected float m_fCraterHugeDepth;

	[Attribute("2", UIWidgets.EditBox, "Charges below this TNT (kg) are shrunk to fit 2 x 2 patches (outer edge 3.95 m, cheap rebuild). At or above it the crater may span more patches (rare, one slower rebuild).", category: "Craters")]
	protected float m_fCraterBigTnt;

	[Attribute("0.6", UIWidgets.EditBox, "HEAT / HEDP (shaped charge, the warhead fires a penetrator) digs this share of the depth: the jet goes into the target, not the ground. A 1.0 m crater -> 0.6 m.", category: "Craters")]
	protected float m_fCraterHeatFactor;

	[Attribute("0.35", UIWidgets.EditBox, "Crater variation (0.15 until user 2026-10-17 'more variation'; 0.3 until user 2026-10-21 'the largest crater causes the issues, same size as v010'; 0 until user 2026-10-21 'more variation'): depth x 1 - 0 .. this, random per crater. ONLY SMALLER since 2026-10-21 (was +- this): the full size stays the largest. 0 = all the same.", category: "Craters")]
	protected float m_fCraterDepthJitter;

	[Attribute("0.2", UIWidgets.EditBox, "Crater variation (0.15 until user 2026-10-17, 0.35 until 2026-10-21, then 0: user saw several sizes; 0.2 since user 2026-10-21 'more variation'; heightfield + voxel craters roll 20-100 % of it each): outline stretched up to this share by angle (random oval / three lobes, random turn). Only widens, so walls get no steeper. Shrinks where the outer edge would pass 3.95 m. 0 = round.", category: "Craters")]
	protected float m_fCraterShapeJitter;

	[Attribute("0.08", UIWidgets.EditBox, "Rim height as a share of the crater depth (0.17 until user 2026-10-22 'reduce the ridges a lot', v289 = before)", category: "Craters")]
	protected float m_fCraterRimShare;

	[Attribute("0.4", UIWidgets.EditBox, "Rim width outside the crest as a share of the crater depth (at least 2 fine steps)", category: "Craters")]
	protected float m_fCraterRimWidthShare;

	[Attribute("0.2", UIWidgets.EditBox, "Share of craters with NO rim (user 2026-10-21: some with a dirt lip, some just dropping straight from the grass; 0.5 until the mud ring kind came, 0.35 until the high rim kind). RING_DIRT draws a flat dirt band round them (grass clutter through it, like a trench edge). Rolled per crater on the server (clients get the heights). Rim kind roll: below this = none, then Ring Chance = small mud ring, then High Chance = high rim, the rest = full rim (x 0.6 .. 1).", category: "Craters")]
	protected float m_fCraterNoRimChance;

	[Attribute("0.25", UIWidgets.EditBox, "Share of craters with a SMALL MUD RING (0.35 until 2026-10-21 late) (user 2026-10-21 'some with a small mud ring'): a low narrow lip (Ring Share x depth high, at least 6 cm, 2 fine steps wide), drawn as dirt by RING_DIRT. 0 = none.", category: "Craters")]
	protected float m_fCraterRingChance;

	[Attribute("0.09", UIWidgets.EditBox, "Mud ring height as a share of the crater depth (at least 6 cm)", category: "Craters")]
	protected float m_fCraterRingShare;

	[Attribute("0.1", UIWidgets.EditBox, "Share of craters with a HIGH RIM (user 2026-10-21 late 'some more with higher rims'; 0.25 until user 2026-10-22 'reduce the ridges'):rim height and width x 1.3 .. High Factor (the whole bowl still shrinks to fit 3.95 m). 0 = none.", category: "Craters")]
	protected float m_fCraterHighChance;

	[Attribute("1.4", UIWidgets.EditBox, "High rim: rim height and width x 1.3 .. this (1.9 until user 2026-10-22 'reduce the ridges')", category: "Craters")]
	protected float m_fCraterHighFactor;

	[Attribute("0.2", UIWidgets.EditBox, "Share of craters that are LARGE (user 2026-10-22 'they can even be a little bigger, add a variant that is also larger'): depth x 1.15 .. Large Factor (no shrink jitter), and like a big charge it may span 3 x 3 patches (outer edge up to 7.95 m, one slower rebuild). Rolled apart from the rim kind. 0 = none.", category: "Craters")]
	protected float m_fCraterLargeChance;

	[Attribute("1.25", UIWidgets.EditBox, "Large crater: depth x 1.15 .. this (never past Huge Depth; 1.4 until user 2026-10-22 'largest somewhat smaller')", category: "Craters")]
	protected float m_fCraterLargeFactor;

	[Attribute("1.67", UIWidgets.EditBox, "Repeated hits dig at most this many times the crater's own depth", category: "Craters")]
	protected float m_fCraterStackFactor;

	[Attribute("2.5", UIWidgets.EditBox, "Repeated hits never dig deeper than this (m)", category: "Craters")]
	protected float m_fCraterStackDepth;

	[Attribute("0.9", UIWidgets.EditBox, "Steepest crater wall before the collider blur (rise per metre, 0.9 = 42 deg). Crest radius and rim width grow to keep it. Model: Tools/crater_profile.py", category: "Craters")]
	protected float m_fCraterMaxSlope;

	[Attribute("0.5", UIWidgets.EditBox, "A hit on dug ground digs this share of its normal depth below what is there (up to the stack caps)", category: "Craters")]
	protected float m_fCraterDeepenGain;

	[Attribute("0.3", UIWidgets.EditBox, "Full crater for blasts up to this high above the ground (m)", category: "Craters")]
	protected float m_fCraterAirFull;

	[Attribute("1.2", UIWidgets.EditBox, "No crater for blasts this high above the ground or higher (m); in between depth and rim shrink linearly", category: "Craters")]
	protected float m_fCraterAirNone;

	[Attribute("0", UIWidgets.CheckBox, "Print stats every 10 s and check every new patch against the skin by trace", category: "Debug")]
	protected bool m_bDebug;

	[Attribute("0", UIWidgets.EditBox, "Print height hashes of all deformed patches every this many seconds (0 = off). Compare server and client logs with Tools/compare_hashes.py.", category: "Debug")]
	protected float m_fHashEvery;

	[Attribute("0", UIWidgets.EditBox, "Layered-SDF test: print the heights of changed deformed patches every this many seconds (0 = off, at most 8 patches each time) as 'MUD dump' lines for Tools/sdf_patch_proto.py --dump", category: "Debug")]
	protected float m_fDumpEvery;

	[Attribute("0", UIWidgets.CheckBox, "Layered-SDF test (phase 2, 2026-10-13): patch mesh and collider from the band surface-nets mesher (MUD_BandMesher.c) instead of the 33 x 33 grid. Off = the grid exactly as before. Prints a 'MUD band' stats line every 10 s.", category: "Ground")]
	protected bool m_bBandMesher;
	protected ref MUD_BandIdx m_RutMudBand;    // Build 3 Stage 4: the band collider's rut mud tris (made on first use)

	[Attribute("0.3", UIWidgets.EditBox, "Band mesher: skirt hanging this far (m) under the patch rim, toward the skin / kept terrain", category: "Ground")]
	protected float m_fBandSkirt;

	[Attribute("8", UIWidgets.EditBox, "JIP: full patch copies sent per frame (all joining players together)", category: "Patches")]
	protected int m_iJipPerFrame;

	[Attribute("500", UIWidgets.EditBox, "Send by distance (F4): live ground changes go only to players within this radius (m) of the patch; a farther player gets the patch's full heights once they come within it (checked each second, sent through the JIP queue). 0 = every change to every client.", category: "Patches")]
	protected float m_fSendRadius;

	protected MUD_CoarseSkinEntity m_Skin;
	protected ref map<int, ref MUD_FinePatch> m_mPatches = new map<int, ref MUD_FinePatch>();
	protected ref set<int> m_sNoHole = new set<int>();     // patch keys without a hole cell
	protected ref array<int> m_aDirty = {};                // patch keys with a pending mesh / collider
	protected ref MUD_FineBuffers m_Buf;
	protected ref MUD_WallBuf m_Wall = new MUD_WallBuf();     // STEEP WALLS: strips of the patch mesh being built
	protected ref array<float> m_aSmoothW = {};
	protected ref array<string> m_aHardWords = {};
	protected ref map<ResourceName, ref MUD_BlastInfo> m_mBlast = new map<ResourceName, ref MUD_BlastInfo>();     // exploding prefab -> what it is (cache)
	protected ref array<IEntity> m_aFound = {};
	protected ref TraceParam m_Trace = new TraceParam();
	protected RplComponent m_RplComponent;
	protected float m_fX0;
	protected float m_fZ0;
	protected float m_fCell;
	protected float m_fFine;
	static const int LIFT_T = 5;        // SPARSE SHEET A2: 3 m tiles per side checked under one 8 m patch (4 at most)
	protected int m_aLiftT[25];         // A2: 1 = sync tile there (LIFT_T x LIFT_T from m_iLiftTx0, m_iLiftTz0)
	protected int m_iLiftTx0;
	protected int m_iLiftTz0;
	protected float m_fLiftX0;          // A2: world corner of the patch LiftSetup was last called for
	protected float m_fLiftZ0;
	protected int m_aPressVox[36];      // 6 x 6 skin cells: the patch's 4 x 4 + one ring, 1 = voxel cell
	protected float m_aSkinLift[25];    // LOOK LIFT: lift (m) at the patch's 5 x 5 skin vertices (LookLiftSetup)
	// LOOK SLOPE (MUD_LookLift.SLOPE): most lift per vertex of the patch + LOOK_CAP_R round it (LookCapSetup)
	static const int LOOK_CAP_R = 5;     // fine vertices the slope reaches: SLOPE 1 x 1.25 m covers base 0.75 + GROW 0.3
	static const int LOOK_CAP_W = 43;    // PVERTS (33) + 2 x LOOK_CAP_R
	protected float m_aLookCap[1849];   // LOOK_CAP_W x LOOK_CAP_W
	protected bool m_bLookCap;          // any cap in m_aLookCap (false = skip it)
	protected int m_iLookCapMs;         // LookCapSetup script ms (5 s log)
	protected int m_iLookCapN;          // LookCapSetup calls with a cap (5 s log)
	// LOOK LIFT gap (b) switch (2026-10-23): true = a border vertex presses out only when every neighbour sharing it
	// draws. User WB test: more pop-in + worse seams - lazy neighbours never draw, so cut / worn borders stayed lifted
	// until the neighbour built. false = the old press (small crack into the skin) while that is checked.
	static const bool LOOK_BORDER_WAIT = false;
	protected int m_aLookNb[9];         // LOOK LIFT gap (b): 1 = that neighbour ((dz + 1) * 3 + dx + 1) draws (LookLiftSetup)
	protected int m_iLookHeal;          // LOOK LIFT gap (b): neighbours re-drawn because a neighbour came / went (log)
	protected float m_fPressSoil;       // groove reveal input of the vertex FarAt did last (1000 = no soil)
	// FAR PRESS (MUD_MudSheet.FAR_PRESS, plan plans/old/2026-10-21-far-press-plan.md): patch meshes pressed by the sync layer
	protected bool m_bFarOn;                                    // the mesh being built is pressed (FarSetup said yes)
	protected ref array<float> m_aFarD = {};                    // sync mud depth round the patch (MUD_MudSync.FarBox)
	protected int m_iFarGi0;                                    // ... its first world cell
	protected int m_iFarGk0;
	protected int m_iFarW;                                      // ... cells a row
	protected int m_iFarH;
	protected int m_aFarNb[9];                                  // 1 = neighbour patch drawn ((dz + 1) * 3 + dx + 1)
	protected ref map<int, float> m_mFarDue = new map<int, float>();   // patch key -> m_fTime its look rebuild may go
	protected ref set<int> m_sFarKeys = new set<int>();         // every patch key far press marked (toggle rebuild)
	protected int m_iFarMarks;                                  // 10 s meters
	protected int m_iFarSent;
	protected int m_iFarBuilt;
	protected int m_iFarEmpty;
	protected int m_iFarVerts;
	protected int m_iFarSoil;
	protected int m_iFarFaded;
	protected int m_iFarHole;
	protected int m_iFarHeal;
	protected int m_iNPX;
	protected int m_iNPZ;
	protected float m_fTime;
	protected float m_fSinceFree;
	protected bool m_bReady;
	protected bool m_bRender;
	protected bool m_bGroove;
	protected int m_iDeformed;
	protected bool m_bBudgetWarned;

	// debug stats, printed every 10 s
	protected float m_fSinceStats;
	protected int m_iStatFrames;                                // frames since the last Stats print (fps)
	protected int m_iRenderSkip;                               // RENDER_GATE: mesh picks skipped per 10 s (patch-frames)
	protected int m_iLookCovered;                              // LOOK_COVERS: rut ops whose patch meshes were kept
	protected vector m_vGateCam;                               // RENDER_GATE: camera this frame
	protected int m_iSettleQuiet;                               // FLOOR_SETTLE per 10 s: released after quiet,
	protected int m_iSettleMax;                                 // ... after the max wait,
	protected int m_iSettleHeld;                                // ... patch-frames held
	// LAZY RUT MESH (synced mud sheet Stage 3, user 2026-10-18): patches whose mesh is hidden under the sheet
	protected ref array<int> m_aSheetHid = {};
	protected int m_iSheetHeld;                                 // per 10 s: patch-frames a mesh rebuild was held
	protected int m_iSheetShown;                                // per 10 s: hidden patches shown again
	protected int m_iFieldMade;                                 // FLOOR_FREE per 10 s: floor patches FieldDue made,
	protected int m_iFloorFreed;                                // ... floor-only patches freed
	protected ref set<int> m_sFieldPatches = new set<int>();    // RUT FIELD: patch keys a rut floor change reached (FloorDirty)
	protected ref array<int> m_aFieldDue = {};                  // of those, in range at the last check (FieldDue)
	protected ref array<float> m_aFieldDueDist = {};
	protected float m_fFieldCheckAt;
	protected int m_iSeq;                                       // server: number of the next live message
	protected ref array<int> m_aNetKeys = {};                   // server: patch keys with unsent changes
	protected ref array<ref MUD_NetMsg> m_aHeld = {};           // client: live messages not applied yet
	protected static ref array<ref MUD_NetMsg> s_aEarly = {};   // client: per-player live messages that came before this entity existed
	protected ref set<int> m_sJipPending = new set<int>();      // client: keys whose full copy is still coming (task 10)
	protected ref map<int, int> m_mFullSeq = new map<int, int>();   // client: seq stamped on the full copy (task 10)
	protected float m_fSinceNet;
	protected float m_fSinceHash;
	protected int m_iHashBlock;
	protected int m_iNetMsgs;
	protected int m_iNetInts;
	protected int m_iNetGot;
	protected int m_iNetBad;
	protected int m_iJipHeld;
	protected int m_iJipReplayed;
	protected int m_iJipDropped;
	protected ref array<ref MUD_JipItem> m_aJip = {};   // server: full copies owed, queue order
	protected ref map<int, int> m_mJipAskCount = new map<int, int>();     // server: asks per player id
	protected ref map<int, float> m_mJipAskTime = new map<int, float>();  // server: last ask per player id
	protected ref map<int, ref map<int, bool>> m_mOwed = new map<int, ref map<int, bool>>();  // server, send by distance: per player id, patch keys changed while they were far (true = full copy queued)
	protected ref array<int> m_aSendPids = {};           // SendChanges: remote players this tick
	protected ref array<SCR_PlayerController> m_aSendPcs = {};
	protected ref array<vector> m_aSendPos = {};
	protected ref array<bool> m_aSendKnown = {};          // false: no controlled entity, every change goes
	protected float m_fOwedCheckAt;
	protected int m_iNetLive;                             // per-player live messages in 10 s
	protected int m_iNetOwed;                             // changes owed to a far player in 10 s
	protected int m_iNetCaught;                           // catch-up full copies sent / applied in 10 s
	protected bool m_bJipNeeded;        // client: RplLoad came, full copies still to ask for / receive
	protected bool m_bJipAsked;
	protected float m_fJipStart;
	protected float m_fJipLastAsk;      // client: last ask or last full copy; no news for JIP_RETRY s -> ask again
	protected int m_iJipAsks;
	protected int m_iJipTotal;
	protected int m_iJipSent;
	protected int m_iMadeAhead;
	protected int m_iMadeStamp;
	protected int m_iFreed;
	protected int m_iMeshBuilds;
	protected int m_iMeshMs;
	protected int m_iColBuilds;
	protected int m_iColMs;
	protected int m_iSmoothMs;
	protected int m_iSwapMs;
	// layered-SDF test (band mesher): per-patch build stats over 10 s, grid path counted too as the baseline
	protected ref MUD_BandMesher m_Band;
	protected int m_iBandMeshN;
	protected int m_iBandMeshMs;
	protected int m_iBandMeshWorst;
	protected int m_iBandColN;
	protected int m_iBandColMs;
	protected int m_iBandColWorst;
	protected int m_iBandCoreMs;          // of both: the mesher itself
	// cost split (rut field Stage 4 follow-up, 2026-10-15): where a band build's ms go, 10 s sums. ms ticks, so
	// small parts round down; 'rest' = total - parts
	protected int m_iCsMeshIn;          // mesh: input loop (m_aH, ahead hold, rut floor)
	protected int m_iCsMeshCore;        // mesh: m_Band.Build
	protected int m_iCsMeshUv;          // mesh: groove reveal + UV loop
	protected int m_iCsMeshRes;         // mesh: MeshObject.Create + Update* + SetObject
	protected int m_iCsColIn;           // collider: input loop (smooth, rut floor, wheel hold, plate)
	protected int m_iCsColCore;         // collider: m_Band.Build
	protected int m_iCsColGeom;         // collider: per-material + slab + plate CreateTriMesh
	protected int m_iCsColDrop;         // collider: DropBody
	protected int m_iCsColStatic;       // collider: MakeStatic
	// OPT PASS 2026-10-23 (plans/2026-10-23-optimization-pass.md #1a): the band mesh input pass split, 10 s sums (ms ticks,
	// sampled: sums over many passes are fair, single passes round)
	protected int m_iInRf;              // RutFieldOn + LookRuts.InBox + PatchBegin + BandFill
	protected int m_iInLift;            // LiftSetup (look lift: neighbours, skin lift, LookCapSetup)
	protected int m_iInCap;             // of it: LookCapSetup (look slope)
	protected int m_iInPart;            // PartialOk + kept input copy
	protected int m_iInLoop;            // BandMeshIn (per vertex: hold, rut floor, ridge, wear show, ring, LiftAt)
	protected int m_iInFill;            // FillBandOn
	protected int m_iInRfN;             // passes with the rut field / look ruts on
	protected int m_iInLiftN;           // passes with a lift
	protected int m_iInCapN;            // passes whose LookCapSetup made a cap (m_bLookCap)
	protected int m_iCsColGeoms;        // collider: geoms made (count)
	protected int m_iBandCells;           // sums / max per build
	protected int m_iBandCellsMax;
	protected int m_iBandTris;
	protected int m_iBandTrisMax;
	protected int m_iBandVerts;
	protected int m_iBandSkirtTris;
	protected int m_iBandDegen;
	protected int m_iBandMissing;
	protected int m_iBandFastV;         // verts / y quads made on the mesher's FAST paths
	protected int m_iBandFastQ;
	protected int m_iBandFallback;        // overflow / no triangle: grid built instead
	protected int m_iGridMeshN;
	protected int m_iGridMeshMs;
	protected int m_iGridMeshWorst;
	protected int m_iGridColN;
	protected int m_iGridColMs;
	protected int m_iGridColWorst;
	protected float m_fSinceBand;
	protected float m_fSinceDump;
	protected ref map<int, int> m_mDumpSig = new map<int, int>();
	protected int m_iWorstMs;
	protected string m_sWorstSplit;               // where the worst frame's ms went
	protected string m_sFrameStep;                // this frame's ahead build steps
	protected string m_sFrameRebuild;             // this frame's rebuild jobs with their ms
	protected int m_iLastDropMs;                  // last BuildCollider: old body + geoms freed
	protected int m_iLastStaticMs;                // last BuildCollider: CreateStaticEx
	protected int m_iRushed;                      // patches made ahead that a stamp finished at once
	protected int m_iReadyNow;                    // patches made ahead that stopped READY (lazy) in 10 s
	protected int m_iHeldVerts;                   // collider verts kept at their old height under a wheel in 10 s
	protected int m_iHeldBuilds;                  // collider builds that held some vert (and queued a re-build)
	protected ref set<int> m_sFar = new set<int>();       // client: deformed patches held as heights only (far from the camera)
	protected ref set<int> m_sFarCrater = new set<int>(); // CRATER VIEW: of m_sFar, the crater ones (built within the view distance)
	// CRATER VIEW (user 2026-10-23 live test: 'you can see the explosions but no crater far away'): a crater is sent to
	// players and built on clients out to the view distance, not only m_fSendRadius / m_fClientBuildRadius. Ruts keep those.
	static const bool CRATER_VIEW = true;
	protected ref array<int> m_aFarDue = {};              // of those, within m_fClientBuildRadius at the last check: Ahead builds them
	protected ref array<float> m_aFarDueDist = {};
	protected float m_fFarCheckAt;
	protected int m_iFarHeld;                             // messages kept as heights only in 10 s
	protected int m_iWheelBuilds;                 // patches wheels made or finished this frame
	protected int m_iWheelCapped;                 // wheel presses that waited on a missing / building patch (10 s)
	protected int m_iWaitAhead;                   // budget waits in 10 s (stats)
	protected int m_iWaitMesh;
	protected int m_iWaitCol;
	protected int m_iWaitFree;
	protected ref array<int> m_aFreeQueue = {};   // idle patch keys from the last 1 s scan, freed one per frame
	protected ref array<int> m_aPlayerIds = {};
	protected ref array<int> m_aSleepQueue = {};  // far deformed patch keys from the last 1 s scan, one sleeps per frame
	protected int m_iSleepSkip;
	protected int m_iWaitSleep;
	protected int m_iAsleep;                      // patches asleep now
	protected int m_iSlept;                       // in 10 s (stats)
	protected int m_iWoken;
	protected int m_iWokeNow;                     // of those: woken at once by a wheel / stamp
	protected int m_iFeetWoke;                    // of those: woken at once under a player's feet (FEET WAKE)
	static const float FEET_WAKE_R = 2;           // m from a player to an asleep patch that wakes at once
	static const int FEET_WAKES_PER_FRAME = 2;    // each is a full build (~4-7 ms)
	protected int m_iSleepKB;                     // KB the sleeps of the last 10 s gave back (skin rebuild included)
	protected int m_iWakeKB;                      // KB the wakes of the last 10 s took
	protected int m_iBuildKey = -1;               // patch made ahead and still building (one at a time)
	protected int m_iAheadSkip;                   // frames each job waited on the frame budget in a row
	protected int m_iMeshSkip;
	protected int m_iColSkip;
	protected int m_iFreeSkip;
	protected int m_iSkipHeight;
	protected int m_iAheadPressed;                  // ahead ruts: presses in 10 s
	protected int m_iAheadSkipped;                  // ahead ruts: points within Ahead Clear of a wheel, 10 s
	protected int m_iLeadSkipped;                   // ahead ruts: of those, on the leading axle (the one that should always press), 10 s
	protected int m_iFills;                         // rut fill: fill presses that changed ground, 10 s
	protected int m_iFillTried;                     // rut fill: fill presses (changed or not), 10 s
	protected int m_iFillSkipped;                   // rut fill: no point Fill Clear from every wheel, 10 s
	protected ref array<ref MUD_PendPress> m_aPend = {};   // ahead undo: presses not reached yet (server)
	protected ref MUD_PendPress m_Rec;                // ahead undo: Press records its changes here when set
	protected int m_iUndone;                        // ahead undo: presses put back, 10 s
	protected int m_iKept;                          // ahead undo: presses reached, 10 s
	protected int m_iHideX0;                        // ahead hide: box MarkHide touched since HideBoxReset
	protected int m_iHideX1;
	protected int m_iHideZ0;
	protected int m_iHideZ1;
	protected int m_iRampCuts;                      // approach ramp: points that cut ground, 10 s
	protected int m_iFillOffTrack;                  // rut fill: skipped, fill point not on the front track, 10 s
	protected float m_fLeadCap = 1000;              // ahead ruts: BrakeCap of the vehicle being handled (AheadLead never exceeds it)
	protected float m_fSpanMin;                     // ahead ruts: one vehicle's contacts along its travel dir: rearmost (m from origin)
	protected float m_fSpanMax;                     // ... and frontmost
	protected ref array<vector> m_aContacts = {};   // ahead ruts: one vehicle's wheel contacts this frame
	protected ref array<int> m_aRutLead = {};       // RUT_LEAD_AXLE (MUD_VoxelProbe.LeadAxle): per wheel, 1 = cuts the rut
	protected ref array<int> m_aWheelPress = {};    // ahead ruts, per wheel index: 1 = on the ground and presses (end axle)
	protected ref array<float> m_aWheelZ = {};      // ahead ruts, per wheel index: vehicle-local z of the contact
	static const float AXLE_GAP = 0.3;              // ahead ruts: contacts within this (m, local z) are one axle
	protected ref array<ref MUD_Belly> m_aBellies = {};   // body scrape: measured undersides
	protected ref map<Vehicle, MUD_Belly> m_mBelly = new map<Vehicle, MUD_Belly>();   // audit B1: Belly lookup (weak values)
	protected ref array<int> m_aCandKey = {};             // audit B8: Ahead candidates, reused every frame
	protected ref array<float> m_aCandDist = {};
	protected ref map<Vehicle, float> m_mParkScanAt = new map<Vehicle, float>();     // audit B2: parked vehicle's last Ahead scan
	static const float PARK_SCAN_S = 0.5;                 // audit B2: a parked vehicle (no pilot, body asleep) scans this often
	protected int m_iParkSkips;                           // audit B1 / B2 TEST_KIT meter: parked-vehicle skips in 10 s
	protected int m_iOpChars;                             // audit B4 meter: voxel op text chars sent in 10 s
	protected Vehicle m_BellyVeh;                   // body scrape: the vehicle BellyOnly lets through
	protected int m_iScrapeCuts;                    // body scrape: underside points that cut ground, 10 s
	protected int m_iEscapeSteps;                   // ditch escape: dig steps, 10 s
	protected int m_iEscapeCuts;                    // ditch escape: points that dug ground, 10 s
	static const float HIDE_HOLD = 0.1;             // ahead ruts: a vertex stays hidden this long (s) after its last mark (> one frame)
	// L2 look rut perf (DS 2026-10-18 11:29, research/2026-10-18-sheet-alternatives-and-perf.md sec 9): a patch with only client
	// look rut dirt (no collider dirt) rebuilds its mesh at most this often - at m_fMeshInterval 0.1 s look meshes were ~25 / s,
	// ~5.4 ms each, 90 % of the off-zone rebuild cost. Real dirt keeps m_fMeshInterval.
	static const float LOOK_MESH_EVERY = 0.25;
	// PAPER TEST 3 (2026-10-18, research/2026-10-18-mud-mesh-cost-research-findings.md item 2): a queued band collider
	// rebuild is skipped while no vertex moved COL_SMALL_M or more and the last real build is < COL_SMALL_MAX_S old; the
	// patch stays dirty, so it builds at the latest then. false = off (v204)
	static const bool COL_SKIP_SMALL = true;     // PERF IDEA 4 (user 2026-10-19, v232 = before): re-test with 3 cm wear passes; false 2026-10-18 (quick win 2: 0-4 skips per 10 s on the DS 16:16 = no gain)
	static const float COL_SMALL_M = 0.035;    // PERF IDEA 4: one wear pass (RUT_PASS_MAX 0.03) waits up to COL_SMALL_MAX_S; was 0.02
	static const float COL_SMALL_MAX_S = 1.0;
	// PERF IDEA 2 (user 2026-10-19 'move on to idea 2', v234 = before; plan 2026-10-19-rut-wear-perf-plan.md): the same for a
	// queued patch MESH rebuild (client: look ruts + real rut floor) - skipped while no vertex moved MESH_SMALL_M or more
	// against the last built band mesh and that build is < MESH_SMALL_MAX_S old; the patch stays dirty (built then at the
	// latest). A 3 cm wear pass then shows up to 0.5 s late; a first cut on a quiet patch builds at once. false = off.
	static const bool MESH_SKIP_SMALL = true;
	static const float MESH_SMALL_M = 0.035;
	static const float MESH_SMALL_MAX_S = 0.5;
	protected bool m_bMeshGate;         // true only around the rebuild queue's BuildMesh call
	protected bool m_bMeshSkipped;      // that call skipped (MESH_SKIP_SMALL)
	protected int m_iMeshSkipSmall;     // log: skips per 10 s ('MUD world: budget' line)
	// PERF IDEA 3 (user 2026-10-19, v235 = before): the band mesh / collider input reads the rut floor, ridge and look floor
	// of a patch in one fill call each (MUD_VoxelProbe.FieldPatchFill, MUD_LookRuts.PatchFill) and the press holder once,
	// not 3-4 calls per vertex. (Its TEMP self-check BAND_FILL_CHECK: archived 2026-10-21.)
	// PERF B7 (user 2026-10-19, v237 = before; plans/2026-10-18-perf-plan.md B7): with voxel ruts on (MUD_VoxelProbe
	// ENABLED + RUTS) no heightfield press happens (run 9: 'presses 0' on server and client), and the ahead hide only
	// holds m_aH back (BuildMeshBand) - the voxel rut floor goes in after it, never hidden. So AheadView skips the
	// per-wheel ArcHide and HoldHide then. ScrapeView keeps its own. false = old behaviour (A/B switch).
	static const bool AHEAD_HIDE_SKIP = true;
	// REBUILD 5 (user 2026-10-19, v240 = before): 5 cuts of the patch rebuild cost, each on its own switch (A/B on the DS);
	// the 'MUD rebuild 5' 10 s line counts each. Option 5's old mesher (BuildRef) + bench: archived 2026-10-22.
	// DS run 11 (22:15, user 2026-10-19): KEPT 4 + 5; 1, 2, 3 OFF (no measurable win - results run 11), code kept.
	static const bool RUT_PACE = false;               // 1: mesh dirt from the rut floor alone (no look dirt) rebuilds at
	static const float RUT_MESH_EVERY = 0.25;        //    most every RUT_MESH_EVERY s (was m_fMeshInterval)
	static const bool SAME_SKIP = false;              // 2: a queued band mesh whose input equals the shown mesh's is not rebuilt
	static const bool PARTIAL_IN = false;             // 3: the queued band mesh input pass reads only the changed box
	static const bool COL_MERGE = true;              // 4: the collider draws flat untouched 2 x 2 quad blocks as one quad
	static const float COL_MERGE_TOL = 0.01;         //    max off-plane (m) of a merged block's inner vertices
	// (option 5's TEMP bench MESHER_BENCH and option 3's PARTIAL_CHECK: archived 2026-10-21)
	// REBUILD 6 (2026-10-19, v243 = before; research/2026-10-19-rebuild-and-bugs-research.md, prompts/2026-10-19-local-
	// rebuild-followup.md): client A/B switches, NOT const - MUD_StutterRobot sets them at each robot run start (odd ON,
	// even OFF; 'MUD AB' line). COARSE_PLATE is const (on all session).
	static bool VIEW_TRIM = true;      // 2: AheadView skips the dead per-wheel loop; MarkHide one EnsurePress per patch
	static bool LOOK_PACE = true;      // 3: no height dirt since the last mesh -> LOOK_MESH_EVERY pace (+ C1 hold reset)
	// mud lab stage 2 B7 (user OK 2026-10-21, fix set 15): with sheet local on, DoVoxCells builds the patch colliders in the
	// go-live frame as before, but their meshes one per frame after it (the sheet draws over them). Default off.
	static bool VOX_MESH_LATER = false;
	// mud lab stage 3 B9 (fix set 20, sheet local): a band patch mesh without its groove section when every revealed vert
	// is under the sheet's full cover (heights and sink unchanged); a listed patch rebuilds when that cover is lost. Default off.
	static bool GROOVE_UNDER_OFF = false;
	// stage 3 B10 (fix set 21, sheet local): a queued band mesh rebuild is held when every changed vert is under the sheet's
	// full cover and its shown height (less its groove sink) is at least 2 cm under the new ground + LIFT; the kept input
	// and the changed box stay, the changed verts join the B9 watch list. Look only, colliders untouched. Default off.
	static bool MESH_UNDER_HOLD = false;
	protected ref array<int> m_aVoxLater = {};     // B7: patch keys whose go-live mesh waits (one per frame, VoxLaterStep)
	protected int m_iVoxLaterN;                    // B7 meters, per run of the list: meshes built, frames, ms, worst, re-queued
	protected int m_iVoxLaterF;
	protected int m_iVoxLaterMs;
	protected int m_iVoxLaterWorst;
	protected int m_iVoxLaterQ;
	static const bool COARSE_PLATE = true;     // 5: flat-floor patches get 2 plate tris per hole skin cell
	protected int m_aWhyBuild[16];     // 0 REBUILD_WHY (TEMP): queued mesh builds per dirt kind bits (1 look, 2 floor, 4 height, 8 sweep)
	protected int m_iWhySkips;         //   queued small skips
	protected int m_iWhyPace01;        //   queued builds picked at the m_fMeshInterval pace
	protected int m_iWhyAge;           //   MeshSmall false: only the age test failed
	protected int m_iWhyBig;           //   MeshSmall false: a vertex moved >= MESH_SMALL_M
	protected int m_iWhyOther;         //   MeshSmall false: no kept mesh / building
	protected int m_iPlateCoarse;      // 5: collider builds with the coarse plate
	protected int m_iPlateFull;        // 5: with the full plate
	protected int m_iPlateTris;        // 5: plate tris over those builds
	protected int m_iPaceHeldPF;     // 1: patch-frames the old pace would have built
	protected int m_iPaceBuilds;     // 1: builds (or same skips) of a patch that was held
	protected int m_iSameSkip;       // 2
	protected int m_iPartN;          // 3: partial input passes
	protected int m_iPartVerts;      // 3: vertices they read
	protected int m_iPartMs;         // 3: input ms of those builds (fills included)
	protected int m_iFullN;          // 3: full input passes (band mesh)
	protected int m_iFullMs;
	protected int m_iColMerged;      // 4: merged blocks over all band collider builds
	protected int m_iColTrisN;       // 4: their collider tris
	protected int m_iColMergeN;      // 4: band collider builds counted
	protected int m_iProfAheadScan;     // TEMP prof split: Ahead's candidate scan ms this frame (MUD_Prof.A_SCAN)
	protected ref array<float> m_aInFl = {};     // fill: floor (+ look) per patch vertex
	protected ref array<float> m_aInRg = {};     // fill: ridge per patch vertex
	protected bool m_bColGate;          // true only around the rebuild queue's BuildCollider call
	protected bool m_bQueueMesh;        // TEMP split delay meter: true only around the rebuild queue's BuildMesh call
	// stage 3 Part 0 B meter (TEST_KIT, sheet local, print only): band mesh builds under the sheet (10 s)
	protected bool m_bBMeter;           // this band mesh build is metered
	protected int m_iBRev;              // this build: revealed (dirt) verts
	protected int m_iBRevOut;           // this build: revealed verts not under the sheet's full cover
	protected float m_fBWx0;            // this build: patch corner (world x, z)
	protected float m_fBWz0;
	protected int m_iBGrT;
	protected int m_iBBuilds;
	protected int m_iBNone;
	protected int m_iBAllCov;
	protected int m_iBSome;
	protected int m_iBGrMs;
	protected int m_iBHold;
	protected int m_iBQueued;
	protected int m_iBHoldVerts;
	// stage 3 B9 / B10 (mud lab fix sets 20 / 21, sheet local)
	protected bool m_bGrooveNow;                  // B9: this build draws the groove section
	protected ref array<float> m_aBRevP = {};     // B9: this build's revealed verts (world x, z pairs)
	protected ref map<int, ref array<float>> m_mBWatch = new map<int, ref array<float>>();     // B9 / B10 watch list: patch key -> world x, z pairs that need the sheet's full cover
	protected ref array<int> m_aBWatchK = {};     // the listed keys (one re-checked per frame, round robin)
	protected int m_iBWatchI;
	protected int m_iBWatchP;                     // next point of that patch (BWATCH_PTS a frame)
	static const int BWATCH_PTS = 300;            // B9 / B10: watch points re-checked per frame
	static const int BWATCH_MAX = 4096;           // B10: a patch's watch list max floats (else the rebuild goes on and resets it)
	protected ref array<float> m_aBHoldP = {};    // B10: this queued build's changed verts (world x, z pairs)
	protected int m_iBHeld;                       // 10 s: queued rebuilds held
	protected int m_iBGrDrop;                     // 10 s: band mesh builds without the groove section
	protected int m_iBWatchRe;                    // 10 s: listed patches rebuilt (cover lost / switches or sheet local off)
	protected int m_iColSkipSmall;      // log: skips per 10 s ('MUD world: budget' line)
	protected float m_fSinceWheelLog;
	protected int m_iPresses;
	protected ref array<ref MUD_IdxSet> m_aIdxSets = {};   // newest IDX_SETS layouts, ring-replaced at m_iIdxNext
	protected int m_iIdxNext;
	protected int m_iIdxHits;
	protected int m_iIdxMisses;
	protected int m_iMakeMs;
	protected ref MUD_CellCache m_Cells = new MUD_CellCache();        // new patches: init, edges, smoothing, softness, spawn (not their builds)
	protected int m_iChkPts;
	protected int m_iChkHit;
	protected int m_iChkGood;
	protected float m_fChkWorst;
	protected int m_iMemStartKB;
	protected int m_iCraters;
	protected int m_iCraterSkips;
	protected int m_iCraterWorstMs;     // OnExplosion runs outside EOnFrame: its cost is not in the worst frame split
	protected bool m_bWarheadHook;      // MUD_WarheadComponent fired at least once: the ammo-only hook stands down
	protected bool m_bNearVehicle;      // VehicleAt query result
	protected vector m_vLastBlast;      // last blast taken (pos, tick, hook): the two hooks may both see one explosion
	protected int m_iLastBlastTick = -100000;
	protected bool m_bLastBlastWarhead;
	// charge pool (user 2026-10-22: one TNT block = a small crater, more blocks = a bigger one): blasts within POOL_R
	// and POOL_MS of each other add their kg into ONE crater (remote fuze chains go off in the same frame)
	static const float POOL_R = 1.5;
	static const int POOL_MS = 100;
	protected ref array<vector> m_aPoolPos = {};
	protected ref array<float> m_aPoolKg = {};
	protected ref array<int> m_aPoolHeat = {};      // 1 = all HEAT so far: the sum digs as HEAT (int: no bool arrays, mud-gotchas)
	protected ref array<int> m_aPoolAt = {};        // tick of the first blast
	protected ref array<int> m_aPoolN = {};
	protected vector m_aCoverPos[8];    // last 8 explosive blasts (ring) for GroundCovers
	protected int m_aCoverTick[8];
	protected int m_iCoverNext;
	protected int m_iCoverDropped;      // fragment / blast hits our ground stopped (stats)
	protected ref TraceParam m_CoverTrace = new TraceParam();
	protected IEntity m_CoverPrintVictim;  // debug print once per soldier and blast
	protected int m_iCoverPrintBlast;
	protected int m_iCoverPrintTick;
	protected int m_iCraterMs;
	protected int m_iDigs;
	// audit A1 (2026-10-22): server, heightfield ops waiting for their colliders before MUD_GroundFollow runs
	static const float FOLLOW_WAIT_S = 3;          // s: run anyway after this (held verts, a patch that never builds)
	protected ref array<int> m_aFollowG = {};      // 4 per job: gx0, gx1, gz0, gz1 (global fine vertices)
	protected ref array<int> m_aFollowKind = {};   // MUD_GroundFollow kind: 0 crater, 2 trench / shovel, 3 fill
	protected ref array<float> m_aFollowLift = {}; // fill: how far the ground rose (m)
	protected ref array<float> m_aFollowAt = {};   // m_fTime the job was queued
	protected int m_iMaxGeoms;
	protected bool m_bExplosionHooked;

	protected static MUD_WorldGroundEntity s_Inst;
	protected static bool s_bHidden;

	//------------------------------------------------------------------------------------------------
	void MUD_WorldGroundEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	void ~MUD_WorldGroundEntity()
	{
		if (m_bExplosionHooked)
			SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Remove(OnExplosion);
		if (s_Inst == this)
			s_Inst = null;
		s_aEarly.Clear();   // not into the next play session
		MUD_VoxelProbe.Clear();    // probe 2026-10-12
		MUD_LookRuts.Reset();     // client look ruts: no tiles / tracks into the next session
		MUD_VoxelHouse.Clear();    // TEMP probe 2026-10-13
		MUD_GroundFollow.Reset();    // audit A15: no tree watch CallLater / hidden-prop list into the next session
		MUD_HeliGround.Reset();      // 2026-10-24: helis + settle items
		// Entities are still alive here (probe 2026-10-09 13:12: 107 of 107). Same order as FreePatch: body, geoms,
		// entity. Deleting the entity first and then freeing its geoms corrupted the heap on play stop (12:56).
		int left;
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			if (!p)
				continue;
			p.m_Phys = null;
			if (p.m_Ent)
			{
				Physics ph = p.m_Ent.GetPhysics();
				if (ph)
					ph.Destroy();
				foreach (PhysicsGeom g : p.m_aGeoms)
				{
					if (g)
						g.Destroy();
				}
				delete p.m_Ent;
			}
			else
			{
				left += p.m_aGeoms.Count();   // entity already gone: its body may still hold them, leave them
			}
			p.m_aGeoms.Clear();
		}
		if (left > 0)
			Print(string.Format("MUD world: teardown left %1 geoms (their entities were already gone)", left), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Old body, then its geoms: Physics.Destroy() does not free the PhysicsGeom it was made from
	//! (measured 2026-10-09 Exp 192624: ~270 KB kept per collider rebuild until geom.Destroy())
	protected static void DropBody(MUD_FinePatch p)
	{
		if (p.m_Phys)
		{
			p.m_Phys.Destroy();
			p.m_Phys = null;
		}
		foreach (PhysicsGeom g : p.m_aGeoms)
		{
			if (g)
				g.Destroy();
		}
		p.m_aGeoms.Clear();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame().InPlayMode())
			ClearEventMask(EntityEvent.FRAME);
		else
			MUD_Release.Check(m_bDebug);     // RELEASE GATE (research 2026-10-21 Q7)
	}

	//------------------------------------------------------------------------------------------------
	static MUD_WorldGroundEntity GetInstance()
	{
		return s_Inst;
	}

	//------------------------------------------------------------------------------------------------
	bool IsReady()
	{
		return m_bReady;
	}

	//------------------------------------------------------------------------------------------------
	static const float CRATER_MAX_OUTER = 3.95;     // m: a fresh crater stays within 2 x 2 patches (8 m)
	static const float CRATER_HUGE_OUTER = 5.95;    // m: a big charge's / large crater's outer edge (7.95 = 3 x 3 patches until user 2026-10-22 'largest somewhat smaller')
	static const float JIP_RETRY = 5;     // s without a full copy before the client asks again
	static const int JIP_MAX_ASKS = 10;
	static const float JIP_ASK_RESET = 600; // s of no asks from a player before the server's count starts over

	//------------------------------------------------------------------------------------------------
	//! True on a client: the server owns the ground shape
	protected bool IsProxy()
	{
		return m_RplComponent != null && m_RplComponent.IsProxy();
	}

	//------------------------------------------------------------------------------------------------
	//! Runs every frame until the skin is built. False = not yet.
	protected bool Setup()
	{
		m_Skin = MUD_CoarseSkinEntity.GetInstance();
		if (!m_Skin || !m_Skin.IsReady())
			return false;

		int ncx;
		int ncz;
		m_Skin.GetGrid(m_fX0, m_fZ0, ncx, ncz);
		m_fCell = m_Skin.GetCell();
		m_fFine = m_fCell / FINE;
		m_iNPX = ncx / SKIN;        // Arland 2048 / 4 = 512
		m_iNPZ = ncz / SKIN;
		m_bRender = RplSession.Mode() != RplMode.Dedicated;
		m_bGroove = m_bRender && !m_RutMaterial.IsEmpty();
		m_RplComponent = RplComponent.Cast(FindComponent(RplComponent));
		m_Buf = new MUD_FineBuffers();
		if (!m_RutMudMaterial.IsEmpty())
		{
			m_RutMudRes = Resource.Load(m_RutMudMaterial);
			if (!m_RutMudRes || !m_RutMudRes.IsValid())
			{
				Print("MUD world: rut mud material does not load - rut mud off: " + m_RutMudMaterial, LogLevel.WARNING);
				m_RutMudMaterial = "";
			}
		}

		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				m_Buf.m_aGrooveUVs[v * 2] = vx / 32.0;
				m_Buf.m_aGrooveUVs[v * 2 + 1] = 1 - vz / 32.0;
			}
		}

		array<string> words = {};
		m_sHardWords.Split(",", words, true);
		foreach (string w : words)
		{
			string word = w.Trim();     // Trim returns the trimmed copy (shipped use: x = y.Trim())
			word.ToLower();
			if (!word.IsEmpty())
				m_aHardWords.Insert(word);
		}

		// collider blur weights = Pascal row 2r (r 2 -> 1 4 6 4 1), as MUD_GroundEntity.c:446-450
		int r = Math.ClampInt(m_iColliderSmooth, 0, 3);
		m_aSmoothW.Insert(1);
		for (int k = 1; k <= 2 * r; k++)
			m_aSmoothW.Insert(m_aSmoothW[k - 1] * (2 * r - k + 1) / k);

		// first world ground of a play session: drop hide state a previous session may have left
		s_bHidden = false;
		MUD_VehicleAccess.ResetTraceHide();
		if (m_iNPX * m_iNPZ > 262144)
			Print("MUD world: more than 262144 patches - JIP order keys overflow (Task 10)", LogLevel.ERROR);
		s_Inst = this;
		MUD_LookRuts.Reset();
		MUD_LookLift.GrowReset();     // look lift GROW: a new play starts flat
		m_iMemStartKB = System.MemoryAllocationKB();
		SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Insert(OnExplosion);
		m_bExplosionHooked = true;
		Print(string.Format("MUD world: ready, patch grid %1 x %2, fine %3 m, render %4, groove %5, edge mode %6, rpl %7",
			m_iNPX, m_iNPZ, m_fFine, m_bRender, m_bGroove, typename.EnumToString(MUD_EEdgeMode, m_eEdgeMode), m_RplComponent != null));
		MUD_TrenchSelfTest.Run();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bReady)
		{
			m_bReady = Setup();
			if (m_bReady)
			{
				MUD_PinLift.Run(this);    // pin cut-out: trees / rocks / props on holes get the skipped terrain y, before op replay
				// per-player messages from before this entity existed go first (they came first)
				for (int e = s_aEarly.Count() - 1; e >= 0; e--)
					m_aHeld.InsertAt(s_aEarly[e], 0);
				s_aEarly.Clear();
				ReplayHeld(-1, 0);   // messages that came before this machine was ready
			}
			return;
		}

		if (MUD_TestKit.TEST_KIT)
			MUD_LoadDrivers.Tick(timeSlice);    // load test (2026-10-23): server trucks; before t0 = not in 'MUD script'
		int t0 = System.GetTickCount();
		int swap0 = m_iSwapMs;
		m_fTime += timeSlice;
		m_sFrameStep = "";
		m_sFrameRebuild = "";
		m_iWheelBuilds = 0;
		GroundRescue();
		if (MUD_TestKit.TEST_KIT)
			MUD_JoltProbe.Tick(timeSlice, IsProxy(), this);    // test kit (2026-10-17, back from the archive): MP robot / snap probe
		MUD_TestKit.BenchTick(timeSlice);    // sheet cost A4 (2026-10-19): 'MUD bench natives' once, 30 s in, clients
		// MUD_Prof (TEMP 2026-10-18): each part's ms
		int tP = System.GetTickCount();
		if (!m_aVoxLater.IsEmpty())
			VoxLaterStep();     // stage 2 B7: one go-live patch mesh per frame
		MUD_VoxelProbe.Tick(m_fTime);    // probe 2026-10-12
		MUD_TunnelGuard.Tick();    // 2026-10-22: local character fell out / stuck in a wall -> back to a safe spot
		MUD_Prof.Add(MUD_Prof.VOX, System.GetTickCount(tP));
		tP = System.GetTickCount();
		MUD_KeepScan.Tick(m_fTime);    // keep mask + pins: test kit, or the Build plugin's scan request (2026-10-23)
		MUD_Prof.Add(MUD_Prof.KEEPSCAN, System.GetTickCount(tP));
		if (m_bSheetBox)
		{
			m_bSheetBox = false;
			MUD_MudSheet.FloorChanged(m_vSheetLo, m_vSheetHi, null, null);     // heightfield craters / digs since last frame
		}
		MUD_MudSheet.Tick(m_fTime);    // TEST 2026-10-14: visual mud sheet (adds its own MUD_Prof parts)
		if (!m_aBWatchK.IsEmpty())
			BWatchTick();     // stage 3 B9 / B10
		if (m_bRender && (MUD_MudSheet.ENABLED || MUD_TestKit.TEST_KIT))
			MUD_MudLab.Tick(m_fTime);    // mud lab 2026-10-20: surface mode switcher (client look only; TEST_KIT numpad 3 cycles)
		if (m_bRender)
			MUD_LookLift.Tick(m_fTime);    // look lift press-out (2026-10-23: own tick, was in the mud lab = TEST_KIT only)
		tP = System.GetTickCount();
		MUD_VoxelHouse.Tick();    // TEMP probe 2026-10-13
		MUD_Prof.Add(MUD_Prof.HOUSE, System.GetTickCount(tP));
		tP = System.GetTickCount();
		m_iProfWSub = 0;
		Wheels(timeSlice);
		if (!IsProxy() && m_bReady)
		{
			MUD_HeliGround.Tick(m_fTime);     // F2 / F3 2026-10-24: heli ruts, set-down marks, impact craters
			MUD_Settle.Tick(m_fTime);         // jitter probe + settle damping A/B (2026-10-24)
		}
		int tWh = System.GetTickCount(tP);
		MUD_Prof.Add(MUD_Prof.WHEELS, tWh);
		MUD_Prof.Add(MUD_Prof.W_OTHER, Math.Max(0, tWh - m_iProfWSub));
		tP = System.GetTickCount();
		if (m_bRender && m_bReady)
			MUD_LookRuts.Tick(m_fTime, m_fRutStep, m_fRutBaseDepth, m_fRutDepthPerTon, m_fRutMaxDepth);   // client look ruts
		MUD_Prof.Add(MUD_Prof.LOOK, System.GetTickCount(tP));
		tP = System.GetTickCount();
		int tAv = System.GetTickCount();
		AheadView();
		MUD_Prof.Add(MUD_Prof.A_VIEW, System.GetTickCount(tAv));
		tAv = System.GetTickCount();
		FloorSpreadTick();
		MUD_Prof.Add(MUD_Prof.A_SPREAD, System.GetTickCount(tAv));
		int tW = System.GetTickCount(t0);
		// after the wheels every job checks the frame budget (Fix B: one new patch + a rebuild in one
		// frame was 10-18 ms, measured 2026-10-09 14:12 Exp 192624)
		tAv = System.GetTickCount();
		m_iProfAheadScan = 0;
		Ahead(t0);
		int tAh = System.GetTickCount(tAv);
		MUD_Prof.Add(MUD_Prof.A_SCAN, m_iProfAheadScan);
		MUD_Prof.Add(MUD_Prof.A_BUILD, Math.Max(0, tAh - m_iProfAheadScan));
		int tA = System.GetTickCount(t0);
		MUD_Prof.Add(MUD_Prof.AHEAD, System.GetTickCount(tP));
		tP = System.GetTickCount();
		Rebuild(t0);
		if (!m_aPoolPos.IsEmpty())
			PoolTick();     // pooled blasts -> one crater each
		if (!m_aFollowKind.IsEmpty())
			FollowTick();     // audit A1: heightfield ops whose colliders are rebuilt
		int tR = System.GetTickCount(t0);
		MUD_Prof.Add(MUD_Prof.REBUILD, System.GetTickCount(tP));
		tP = System.GetTickCount();
		int freed0 = m_iFreed;
		FreeIdle(timeSlice, t0);
		if (IsNet())
		{
			SendChanges(timeSlice);
			CatchUp();
			SendJip();
		}
		else if (IsProxy())
		{
			AskJip();
		}
		int ms = System.GetTickCount(t0);
		MUD_Prof.Add(MUD_Prof.FREE_NET, System.GetTickCount(tP));
		MUD_Prof.EndFrame(timeSlice, ms);
		if (ms > m_iWorstMs)
		{
			m_iWorstMs = ms;
			m_sWorstSplit = string.Format("wheels %1, ahead %2 (%3), rebuild %4 (%5), free %6 (%7 patches), skin swaps in it %8",
				tW, tA - tW, m_sFrameStep, tR - tA, m_sFrameRebuild, ms - tR, m_iFreed - freed0, m_iSwapMs - swap0);
		}
		if (m_bDebug || MUD_TestKit.TEST_KIT)
			Stats(timeSlice);   // test kit (2026-10-17): PERF runs m_bDebug 0 and still needs the 10 s lines
		HashLines(timeSlice);
		DumpPatches(timeSlice);    // layered-SDF test
		BandStats(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! World XZ -> global fine vertex coords (float). False outside the patch grid.
	protected bool ToFine(vector wp, out float fx, out float fz)
	{
		fx = (wp[0] - m_fX0) / m_fFine;
		fz = (wp[2] - m_fZ0) / m_fFine;
		return fx >= 0 && fz >= 0 && fx <= m_iNPX * PCELLS && fz <= m_iNPZ * PCELLS;
	}

	//------------------------------------------------------------------------------------------------
	//! First patch whose vertex range holds global fine vertex (gx, gz), else the patch to its left /
	//! below (a vertex on a patch edge is in 2 or 4 patches). null if none exists. v = local index.
	protected MUD_FinePatch At(int gx, int gz, out int v)
	{
		int pxa = gx / PCELLS;
		int pza = gz / PCELLS;
		for (int pz = pza; pz >= pza - 1; pz--)
		{
			int lz = gz - pz * PCELLS;
			if (pz < 0 || pz >= m_iNPZ || lz < 0 || lz > PCELLS)
				continue;
			for (int px = pxa; px >= pxa - 1; px--)
			{
				int lx = gx - px * PCELLS;
				if (px < 0 || px >= m_iNPX || lx < 0 || lx > PCELLS)
					continue;
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (p)
				{
					v = lz * PVERTS + lx;
					return p;
				}
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetH(int gx, int gz)
	{
		int v;
		MUD_FinePatch p = At(gx, gz, v);
		if (!p)
			return 0;
		return p.m_aH[v];
	}

	//------------------------------------------------------------------------------------------------
	//! Write h into every patch holding vertex (gx, gz)
	protected void SetH(int gx, int gz, float h)
	{
		int pxa = gx / PCELLS;
		int pza = gz / PCELLS;
		for (int pz = pza; pz >= pza - 1; pz--)
		{
			int lz = gz - pz * PCELLS;
			if (pz < 0 || pz >= m_iNPZ || lz < 0 || lz > PCELLS)
				continue;
			for (int px = pxa; px >= pxa - 1; px--)
			{
				int lx = gx - px * PCELLS;
				if (px < 0 || px >= m_iNPX || lx < 0 || lx > PCELLS)
					continue;
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				p.m_aH[lz * PVERTS + lx] = h;
				if (!p.m_bDeformed && Math.AbsFloat(h) > 0.0005)
				{
					p.m_bDeformed = true;
					m_iDeformed++;
					if (m_iDeformed > m_iBudget && !m_bBudgetWarned)
					{
						m_bBudgetWarned = true;
						Print(string.Format("MUD world: %1 deformed patches, past the budget %2 (kept for the match; live ~340-400 KB, asleep less)", m_iDeformed, m_iBudget), LogLevel.WARNING);
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pass timer of global fine vertex (gx, gz) in every patch holding it (a new copy made later
	//! starts at -100, which only allows one extra pass on that vertex)
	protected void SetTouch(int gx, int gz, bool berm, float t)
	{
		int pxa = gx / PCELLS;
		int pza = gz / PCELLS;
		for (int pz = pza; pz >= pza - 1; pz--)
		{
			int lz = gz - pz * PCELLS;
			if (pz < 0 || pz >= m_iNPZ || lz < 0 || lz > PCELLS)
				continue;
			for (int px = pxa; px >= pxa - 1; px--)
			{
				int lx = gx - px * PCELLS;
				if (px < 0 || px >= m_iNPX || lx < 0 || lx > PCELLS)
					continue;
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				if (berm)
					p.EnsurePress().m_aBermTouch[lz * PVERTS + lx] = t;
				else
					p.EnsurePress().m_aRutTouch[lz * PVERTS + lx] = t;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server (or no Rpl): one wheel contact at world wp. A vertex sinks by m_fRutStep once per pass
	//! (no wheel on it for m_fPassGap). Hard ground takes no rut and no berm; next to it both shrink with
	//! the softness; the locked border scales them too. Copy of MUD_GroundEntity.c:2293 Press on global
	//! fine verts.
	protected void Press(vector wp, float depth)
	{
		float fx;
		float fz;
		if (!ToFine(wp, fx, fz))
			return;
		// contact over kept terrain (road, water edge): the real terrain carries it
		int ccx = Math.Floor(fx / FINE);
		int ccz = Math.Floor(fz / FINE);
		if (!m_Skin.IsHoleCell(ccx, ccz))
			return;

		float rr = m_fBermRadius / m_fFine;
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		int gx0 = Math.ClampInt(Math.Floor(fx - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(fx + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(fz - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(fz + rr), 0, gzmax);

		// height check BEFORE making patches: a wheel on a bridge deck must not make any
		int cgx = Math.ClampInt(Math.Round(fx), 0, gmax);
		int cgz = Math.ClampInt(Math.Round(fz), 0, gzmax);
		int cv;
		MUD_FinePatch cp = At(cgx, cgz, cv);
		float surf;
		if (cp)
		{
			surf = cp.m_aBase[cv] + cp.m_aH[cv];
		}
		else
		{
			bool ok;
			surf = BaseAt(fx, fz, ok);
			if (!ok)
				return;
		}
		if (Math.AbsFloat(wp[1] - surf) > m_fPressHeight)
		{
			m_iSkipHeight++;
			return;
		}

		// past the per-frame wheel build cap: this press waits (the wheel presses again next frame)
		if (!EnsureRange(gx0, gx1, gz0, gz1, true))
		{
			m_iWheelCapped++;
			return;
		}
		m_iPresses++;

		bool changed;
		float bermSpan = m_fBermRadius - m_fRutRadius;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float soft = VertexSoft(gx, gz) * p.m_aLock[v];
				if (soft <= 0)
					continue;
				float dx = (gx - fx) * m_fFine;
				float dz = (gz - fz) * m_fFine;
				float d = Math.Sqrt(dx * dx + dz * dz);
				float h = p.m_aH[v];

				if (d < m_fRutRadius)
				{
					bool newPass = m_fTime - p.RutTouch(v) > m_fPassGap;
					SetTouch(gx, gz, false, m_fTime);
					// the pass digs from the height it began at, deepest profile wins: a moving (ahead) press first
					// touches a vertex with its rim (half depth) - one-shot passes left ruts half deep, no berms
					// (2026-10-11: rut fill changed ground on ~90 % of its presses on straight runs)
					if (newPass)
						p.EnsurePress().m_aRutFrom[v] = h;
					float from = p.RutFrom(v);
					float cap = depth * soft;
					if (h <= -cap)
						continue;
					float k = d / m_fRutRadius;
					float fall = 1 - k * k * 0.5;
					// raised loose soil (crater rim, berm, spoil) gets plowed: most of it goes in one pass
					float top = from;
					if (from > 0)
						top = from - from * m_fRimCut * fall;
					float y = Math.Max(top - m_fRutStep * m_fRutSpeed * fall, Math.Max(-cap, p.m_aFloor[v]));
					if (y < h - 0.0005)
					{
						Pend(p, v, gx, gz, h);
						SetH(gx, gz, y);
						changed = true;
					}
				}
				else if (d < m_fBermRadius)
				{
					bool newBerm = m_fTime - p.BermTouch(v) > m_fPassGap;
					SetTouch(gx, gz, true, m_fTime);
					if (newBerm)
						p.EnsurePress().m_aBermFrom[v] = h;
					// ruts win over berms: never push up a vertex that is already below the surface
					float bermTop = m_fBermHeight * soft;
					float bfrom = p.BermFrom(v);
					if (h < -0.001 || bfrom < -0.001 || bfrom >= bermTop)
						continue;
					float t = 1 - (d - m_fRutRadius) / bermSpan;
					float y2 = Math.Min(bfrom + m_fRutStep * m_fRutSpeed * 0.5 * t, bermTop);
					if (y2 > h + 0.0005)
					{
						Pend(p, v, gx, gz, h);
						SetH(gx, gz, y2);
						changed = true;
					}
				}
			}
		}
		if (changed)
			MarkDirty(gx0, gx1, gz0, gz1);
	}

	//------------------------------------------------------------------------------------------------
	//! Server (or no Rpl): every wheel contact of every moving wheeled vehicle presses the ground.
	//! Parked vehicles do not press (a parked wheel makes no new pass anyway, MUD_GroundEntity.c:2323);
	//! they only keep the wheel hold under their wheels.
	//! MUD_Prof (TEMP 2026-10-18): a WHEELS sub-part; the sum is taken off WHEELS for 'w other'
	protected int m_iProfWSub;
	protected void ProfW(int part, int ms)
	{
		MUD_Prof.Add(part, ms);
		m_iProfWSub += ms;
	}

	//------------------------------------------------------------------------------------------------
	protected void Wheels(float timeSlice)
	{
		if (IsProxy())
		{
			TouchLocal();
			return;
		}
		m_fSinceWheelLog += timeSlice;
		bool wheelLog = m_bDebug && m_fSinceWheelLog >= 1;
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh)
				continue;
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!sim || !sim.IsValid())
				continue;
			Physics phys = veh.GetPhysics();
			if (!phys)
				continue;
			// audit B1: parked (no pilot, body asleep) - no escape to run; a stuck truck has a driver
			bool parked = !veh.GetPilot() && !phys.IsActive();
			if (parked)
				m_iParkSkips++;
			if (m_bEscape && !parked)
				Escape(veh, sim, phys);
			if (m_bAheadRuts && m_bAheadUndo)
				PendTick(veh, sim, phys);
			if (phys.GetVelocity().Length() < 0.2)
			{
				// stopped: no press, but its wheels keep the wheel hold like the driver's TouchLocal does.
				// Without this the server's hold ran out under a stopped truck and its collider dropped
				// into the rut while the driver's stayed up: 0.3 m apart, a correction every ~150 ms,
				// the truck bounced in place (probe v2 2026-10-11 02:01:11-13, PeerPlugin1).
				// awake, or someone drives it (an asleep truck with a driver would otherwise drop again)
				if (m_fWheelHold > 0 && (phys.IsActive() || veh.GetPilot()))
				{
					for (int t = 0; t < sim.WheelCount(); t++)
					{
						if (sim.WheelHasContact(t))
							TouchWheel(sim.WheelGetContactPosition(t));
					}
				}
				// all-voxel test: RUT_BEHIND's waiting segments still go out (4 s timeout) while it stands
				int tFs = System.GetTickCount();
				MUD_VoxelProbe.RutFlush(veh);
				ProfW(MUD_Prof.W_FLUSH, System.GetTickCount(tFs));
				continue;
			}
			// heavier vehicle, deeper rut (MUD_GroundEntity.c:2192-2197)
			float mass = phys.GetMass();
			float depth = Math.Min(m_fRutBaseDepth + mass * 0.001 * m_fRutDepthPerTon, m_fRutMaxDepth);
			int count = sim.WheelCount();
			if (m_bAheadRuts)
				Contacts(veh, sim);
			vector vel = phys.GetVelocity();
			vector velFlat = Vector(vel[0], 0, vel[2]);
			float hsW = velFlat.Length();
			float yaw = YawRate(veh, phys);
			vector dirW = Vector(0, 0, 1);
			if (hsW > 0.05)
				dirW = velFlat * (1 / hsW);
			vector org = veh.GetOrigin();
			m_fLeadCap = 1000;
			if (m_bAheadRuts)
			{
				AxleSpan(org, dirW);
				BrakeCap(veh, sim, hsW);
			}
			// all-voxel test: the 2 leading wheels along the travel direction (convert ahead)
			if (MUD_VoxelProbe.RutsOn())
				MUD_VoxelProbe.LeadAxle(veh, sim, vel, m_aRutLead);     // RUT_LEAD_AXLE: the leading axle cuts
			vector voxLead0 = vector.Zero;
			vector voxLead1 = vector.Zero;
			float voxAl0 = -1000;
			float voxAl1 = -1000;
			vector voxRear = vector.Zero;
			float voxAlMin = 1000;
			for (int w = 0; w < count; w++)
			{
				if (!sim.WheelHasContact(w))
					continue;
				vector cp = sim.WheelGetContactPosition(w);
				if (MUD_VoxelProbe.RutsOn())
				{
					// all-voxel test (2026-10-13): voxel ruts instead of the heightfield press / ahead / fill
					if (!m_bAheadRuts || m_aWheelPress[w] != 0)
					{
						int tRw = System.GetTickCount();
						if (m_aRutLead[w] != 0)
							MUD_VoxelProbe.RutWheel(veh, w, MUD_VoxelProbe.RutPoint(veh, sim, w), m_fRutStep, depth);     // RUT AT THE HUB
						else
							MUD_VoxelProbe.RutWheelOff(veh, w);
						ProfW(MUD_Prof.W_TRACK, System.GetTickCount(tRw));
					}
					float voxAl = vector.Dot(cp - org, dirW);
					if (voxAl > voxAl0)
					{
						voxAl1 = voxAl0;
						voxLead1 = voxLead0;
						voxAl0 = voxAl;
						voxLead0 = cp;
					}
					else if (voxAl > voxAl1)
					{
						voxAl1 = voxAl;
						voxLead1 = cp;
					}
					if (voxAl < voxAlMin)
					{
						voxAlMin = voxAl;
						voxRear = cp;
					}
					continue;
				}
				if (m_bAheadRuts)
				{
					if (m_aWheelPress[w] == 0)
						continue;   // a middle axle
					float along = vector.Dot(cp - org, dirW);
					vector ap;
					float lead;
					if (AheadPoint(cp, phys, yaw, hsW, ap, lead))
					{
						if (m_bAheadUndo)
						{
							m_Rec = new MUD_PendPress();
							m_Rec.m_Veh = veh;
							m_Rec.m_vPos = ap;
							m_Rec.m_vDir = dirW;
							m_Rec.m_fAt = m_fTime;
							m_Rec.m_fDue = m_fTime + Math.Min(lead / Math.Max(hsW, 0.3) + m_fUndoGrace, 10);
						}
						Press(ap, depth);
						if (m_Rec && !m_Rec.m_aGx.IsEmpty())
							m_aPend.Insert(m_Rec);
						m_Rec = null;
						m_iAheadPressed++;
					}
					else
					{
						m_iAheadSkipped++;
						if (along > m_fSpanMax - AXLE_GAP)
							m_iLeadSkipped++;
					}
					if (m_bRutFill && hsW > 0.05 && along < m_fSpanMin + AXLE_GAP)
					{
						vector fp;
						bool fpOk = FillPoint(cp, phys.GetVelocityAt(cp), yaw, hsW, org, dirW, fp);
						if (fpOk && m_bFillTrackOnly && !OnTrack(fp, dirW))
						{
							m_iFillOffTrack++;
						}
						else if (fpOk)
						{
							m_iFillTried++;
							if (Fill(fp, depth))
								m_iFills++;
						}
						else
						{
							m_iFillSkipped++;
						}
					}
				}
				else
				{
					Press(cp, depth);
				}
				if (wheelLog && w == 0)
				{
					GameMaterial touch = sim.WheelGetContactMaterial(w);
					ResourceName touchName;
					if (touch)
						touchName = touch.GetResourceName();
					Print(string.Format("MUD wheel: touches %1 | hole %2 | mass %3 kg, rut depth %4 m | at %5", touchName, MUD_Surface.IsHole(GetWorld(), cp[0], cp[2]), mass, depth, cp));
					m_fSinceWheelLog = 0;
				}
			}
			int tRa = System.GetTickCount();
			if (voxAl1 > -1000)
				MUD_VoxelProbe.RutAhead(veh, voxLead0, voxLead1, dirW, hsW);     // all-voxel test (2026-10-13)
			if (voxAlMin < 1000)
				MUD_VoxelProbe.RutTail(veh, voxRear, dirW);     // all-voxel test (2026-10-13)
			ProfW(MUD_Prof.W_AHEAD, System.GetTickCount(tRa));
			tRa = System.GetTickCount();
			MUD_VoxelProbe.RutFlush(veh);     // all-voxel test (2026-10-13)
			ProfW(MUD_Prof.W_FLUSH, System.GetTickCount(tRa));
			if (m_bAheadRuts && m_bScrape)
				Scrape(veh, sim, vel);
			if (m_bAheadRuts && m_bRamp)
				Ramp(veh, sim, dirW, hsW, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the wheels of the vehicle this player sits in mark the verts under them as touched (no
	//! press: the server digs). The wheel hold (BuildCollider) then keeps those verts at their old
	//! collider height when a server message lands while the wheel is on them.
	protected void TouchLocal()
	{
		if (m_fWheelHold <= 0)
			return;
		IEntity me = SCR_PlayerController.GetLocalControlledEntity();
		if (!me)
			return;
		Vehicle veh = Vehicle.Cast(CompartmentAccessComponent.GetVehicleIn(me));
		if (!veh)
			return;
		VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
		if (!sim || !sim.IsValid())
			return;
		int count = sim.WheelCount();
		for (int w = 0; w < count; w++)
		{
			if (sim.WheelHasContact(w))
				TouchWheel(sim.WheelGetContactPosition(w));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Wheel hold: mark the verts within Berm Radius of one wheel contact as touched now (no press)
	protected void TouchWheel(vector cp)
	{
		float fx;
		float fz;
		if (!ToFine(cp, fx, fz))
			return;
		float rr = m_fBermRadius / m_fFine;
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		int gx0 = Math.ClampInt(Math.Floor(fx - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(fx + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(fz - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(fz + rr), 0, gzmax);
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float dx = gx - fx;
				float dz = gz - fz;
				if (dx * dx + dz * dz <= rr * rr)
					SetTouch(gx, gz, false, m_fTime);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: m_aContacts = the wheel contacts of one vehicle this frame; m_aWheelPress[w] = 1 for a
	//! wheel on the ground on the frontmost or rearmost axle (every grounded wheel with End Axles off)
	protected void Contacts(Vehicle veh, VehicleWheeledSimulation sim)
	{
		m_aContacts.Clear();
		m_aWheelPress.Clear();
		m_aWheelZ.Clear();
		float zMin = 1000000;
		float zMax = -1000000;
		int count = sim.WheelCount();
		for (int w = 0; w < count; w++)
		{
			if (!sim.WheelHasContact(w))
			{
				m_aWheelPress.Insert(0);
				m_aWheelZ.Insert(0);
				continue;
			}
			vector cp = sim.WheelGetContactPosition(w);
			m_aContacts.Insert(cp);
			vector lc = veh.CoordToLocal(cp);
			m_aWheelPress.Insert(1);
			m_aWheelZ.Insert(lc[2]);
			zMin = Math.Min(zMin, lc[2]);
			zMax = Math.Max(zMax, lc[2]);
		}
		if (!m_bEndAxles)
			return;
		for (int i = 0; i < count; i++)
		{
			if (m_aWheelPress[i] == 0)
				continue;
			float z = m_aWheelZ[i];
			if (Math.AbsFloat(z - zMin) > AXLE_GAP && Math.AbsFloat(z - zMax) > AXLE_GAP)
				m_aWheelPress[i] = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: m_fSpanMin / m_fSpanMax = rearmost / frontmost contact in m_aContacts along travel dir
	//! (m from o). Wheels within AXLE_GAP of the max are the leading axle, of the min the trailing one.
	protected void AxleSpan(vector o, vector dir)
	{
		m_fSpanMin = 1000000;
		m_fSpanMax = -1000000;
		foreach (vector c : m_aContacts)
		{
			float a = vector.Dot(c - o, dir);
			m_fSpanMin = Math.Min(m_fSpanMin, a);
			m_fSpanMax = Math.Max(m_fSpanMax, a);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rut fill: the point on trailing wheel cp's path (PathAt) between the axles that is farthest from
	//! every contact in m_aContacts, sampled every 0.25 m up to 0.3 m short of the leading axle. False when
	//! even the best is closer than Fill Clear (axles too close together).
	protected bool FillPoint(vector cp, vector vp, float yaw, float hs, vector o, vector dir, out vector fp)
	{
		fp = cp;
		float best = -1;
		for (int i = 2; i < 48; i++)
		{
			float dist = i * 0.25;
			vector q = PathAt(cp, vp, yaw, dist / hs);
			if (vector.Dot(q - o, dir) > m_fSpanMax - 0.3)
				break;   // past the leading axle: its own ahead press covers that
			float md = 1000000;
			foreach (vector c : m_aContacts)
			{
				float dx = q[0] - c[0];
				float dz = q[2] - c[2];
				md = Math.Min(md, dx * dx + dz * dz);
			}
			if (md > best)
			{
				best = md;
				fp = q;
			}
		}
		return best >= m_fFillClear * m_fFillClear;
	}

	//------------------------------------------------------------------------------------------------
	//! Rut fill: true when at least 2 of the 4 points 1 and 2 m before and after wp (along dir) sit in a rut
	//! (m_aH < -0.01 at the nearest vertex), one of them ahead: wp is a gap in the front track, not fresh ground
	//! beside it. Ahead = toward the leading axle, only the front wheels rut there - the trailing wheel's own
	//! earlier fills (behind wp) can't keep an inner-turn track going on their own.
	protected bool OnTrack(vector wp, vector dir)
	{
		int hits = 0;
		int ahead = 0;
		for (int i = 0; i < 4; i++)
		{
			float off = 1;
			if (i >= 2)
				off = 2;
			if (i == 1 || i == 3)
				off = -off;
			vector q = wp + dir * off;
			float fx = 0;
			float fz = 0;
			if (!ToFine(q, fx, fz))
				continue;
			int gx = Math.Round(fx);
			int gz = Math.Round(fz);
			if (GetH(gx, gz) < -0.01)
			{
				hits++;
				if (off > 0)
					ahead++;
			}
		}
		return hits >= 2 && ahead >= 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead undo: Press is about to move vertex (gx, gz) of p (canonical At copy, index v) from h. Remember the
	//! height before the first pending press and add the vertex to m_Rec.
	protected void Pend(MUD_FinePatch p, int v, int gx, int gz, float h)
	{
		if (!m_Rec)
			return;
		if (p.PendAt(v) < 0)
			p.EnsurePress().m_aPendOrig[v] = h;
		p.EnsurePress().m_aPendAt[v] = m_fTime;
		m_Rec.m_aGx.Insert(gx);
		m_Rec.m_aGz.Insert(gz);
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead undo, server, once per vehicle per frame: its pending presses that a wheel has reached are kept;
	//! past due and not reached (and no wheel within Ahead Clear, unless it stopped) they go back. A vertex
	//! goes back only for its newest pending press, so the order never matters; a reached press clears it.
	protected void PendTick(Vehicle veh, VehicleWheeledSimulation sim, Physics phys)
	{
		if (m_aPend.IsEmpty())
			return;
		vector vel = phys.GetVelocity();
		vector flatV = Vector(vel[0], 0, vel[2]);
		float hs = flatV.Length();
		array<vector> wheels = {};
		for (int w = 0; w < sim.WheelCount(); w++)
		{
			if (sim.WheelHasContact(w))
				wheels.Insert(sim.WheelGetContactPosition(w));
		}
		float reach2 = (m_fRutRadius + 0.2) * (m_fRutRadius + 0.2);
		float clear2 = m_fAheadClear * m_fAheadClear;
		for (int i = m_aPend.Count() - 1; i >= 0; i--)
		{
			MUD_PendPress r = m_aPend[i];
			if (!r.m_Veh)
			{
				m_aPend.Remove(i);   // vehicle gone: keep what it made
				continue;
			}
			if (r.m_Veh != veh)
				continue;
			float near2 = 1000000;
			foreach (vector c : wheels)
			{
				float dx = c[0] - r.m_vPos[0];
				float dz = c[2] - r.m_vPos[2];
				near2 = Math.Min(near2, dx * dx + dz * dz);
			}
			if (near2 < reach2)
			{
				PendClear(r);
				m_aPend.Remove(i);
				m_iKept++;
				continue;
			}
			// going the other way now (stopped, then reversed - user 2026-10-11: the ruts ahead showed up when
			// he backed off): its wheels won't get there, put it back now
			bool away = hs >= 0.3 && vector.Dot(flatV, r.m_vDir) < 0;
			if (!away && (m_fTime < r.m_fDue || (hs >= 0.3 && near2 < clear2)))
				continue;
			PendUndo(r);
			m_aPend.Remove(i);
			m_iUndone++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead undo: r was reached - its verts are no longer pending
	protected void PendClear(MUD_PendPress r)
	{
		for (int i = 0; i < r.m_aGx.Count(); i++)
		{
			int v = 0;
			MUD_FinePatch p = At(r.m_aGx[i], r.m_aGz[i], v);
			if (p)
				p.ClearPend(v);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead undo: r was never reached - verts whose newest pending press is r go back to their height before
	//! the first pending press
	protected void PendUndo(MUD_PendPress r)
	{
		for (int i = 0; i < r.m_aGx.Count(); i++)
		{
			int gx = r.m_aGx[i];
			int gz = r.m_aGz[i];
			int v = 0;
			MUD_FinePatch p = At(gx, gz, v);
			if (!p || p.PendAt(v) < 0 || p.PendAt(v) > r.m_fAt)
				continue;   // reached since, or a newer pending press owns it
			p.ClearPend(v);
			SetH(gx, gz, p.PendOrig(v));
			MarkDirty(gx, gx, gz, gz);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Approach ramp: the leading end of veh's underside (bumper) and the line from the leading axle's tyre
	//! contact through the bumper bottom, carried on ahead. Server (draw = false): cut our ground above the
	//! line, one Ramp Step every Ramp Interval. Draw (rendering machines): the line + the points it would cut.
	protected void Ramp(Vehicle veh, VehicleWheeledSimulation sim, vector dir, float hs, bool draw)
	{
		if (hs < 0.05)
			return;
		MUD_Belly b = Belly(veh, sim);
		if (!b || b.m_aPts.IsEmpty())
			return;
		if (!draw)
		{
			if (m_fTime - b.m_fRampAt < m_fRampInterval)
				return;
			b.m_fRampAt = m_fTime;
		}
		// travel direction in vehicle space: which end leads
		vector o = veh.GetOrigin();
		vector ld = veh.CoordToLocal(o + dir) - veh.CoordToLocal(o);
		float sgn = 1;
		float zEnd = b.m_fFront;
		if (ld[2] < 0)
		{
			sgn = -1;
			zEnd = b.m_fBack;
		}
		// bumper bottom: lowest underside point within 0.4 m of that end
		float yb = 1000;
		foreach (vector lp : b.m_aPts)
		{
			if (Math.AbsFloat(lp[2] - zEnd) <= 0.4)
				yb = Math.Min(yb, lp[1]);
		}
		// leading axle contacts (vehicle space): the grounded wheels nearest that end
		float zc = -1000 * sgn;
		int count = sim.WheelCount();
		for (int w = 0; w < count; w++)
		{
			if (!sim.WheelHasContact(w))
				continue;
			vector lc = veh.CoordToLocal(sim.WheelGetContactPosition(w));
			if (lc[2] * sgn > zc * sgn)
				zc = lc[2];
		}
		float yc = 0;
		int n = 0;
		for (int w2 = 0; w2 < count; w2++)
		{
			if (!sim.WheelHasContact(w2))
				continue;
			vector lc2 = veh.CoordToLocal(sim.WheelGetContactPosition(w2));
			if (Math.AbsFloat(lc2[2] - zc) <= AXLE_GAP)
			{
				yc += lc2[1];
				n++;
			}
		}
		if (n == 0 || yb > 999 || (zEnd - zc) * sgn < 0.2)
			return;   // no grounded leading axle, no bumper, or the bumper is not ahead of it
		yc /= n;
		float k = (yb - yc) / (zEnd - zc);   // rise per local z
		float len = Math.Min(m_fRampLength, Math.Max(AheadLead(hs), m_fRampStart + 0.5));
		float clear2 = (m_fRutRadius + 0.3) * (m_fRutRadius + 0.3);
		int col = ARGB(255, 255, 0, 255);
		if (draw && m_bRampDraw)
		{
			float za = zEnd + sgn * m_fRampStart;
			float zb = zEnd + sgn * (m_fRampStart + len);
			for (float sx = -1; sx <= 1; sx += 2)
			{
				vector a = veh.CoordToParent(Vector(sx * b.m_fHalfW, yc + k * (zc - zc), zc));
				vector e = veh.CoordToParent(Vector(sx * b.m_fHalfW, yc + k * (zb - zc), zb));
				Shape.CreateLine(col, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, a, e);
				vector s0 = veh.CoordToParent(Vector(sx * b.m_fHalfW, yc + k * (za - zc), za));
				Shape.CreateSphere(col, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, s0, 0.08);
			}
		}
		for (float dz = m_fRampStart; dz <= m_fRampStart + len; dz += 0.4)
		{
			float z = zEnd + sgn * dz;
			float ly = yc + k * (z - zc) - m_fRampMargin;
			for (float x = -b.m_fHalfW; x <= b.m_fHalfW + 0.01; x += 0.4)
			{
				vector wp = veh.CoordToParent(Vector(x, ly, z));
				bool nearWheel = false;
				foreach (vector c : m_aContacts)
				{
					float cx = wp[0] - c[0];
					float cz = wp[2] - c[2];
					if (cx * cx + cz * cz < clear2)
					{
						nearWheel = true;
						break;
					}
				}
				if (nearWheel)
					continue;
				if (draw)
				{
					if (!m_bRampDraw)
						continue;
					float fx = 0;
					float fz = 0;
					float surf = 0;
					if (ToFine(wp, fx, fz) && GroundY(fx, fz, surf) && surf > wp[1] + 0.002 && surf - wp[1] <= m_fRampReach)
						Shape.CreateSphere(col, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, wp, 0.06);
					continue;
				}
				if (Cut(wp, wp[1], m_fRampStep, m_fRampMaxDepth, m_fRampReach, true))
					m_iRampCuts++;
			}
		}
		if (!draw)
			Spill(veh, b, dir);     // F1: the cut ground goes beside the bumper
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead hide: a stopped vehicle keeps the strips hidden that were hidden when it stopped (box from its
	//! last moving frame) for Stop Hide s, so the server can put back presses it never reached first
	protected void HoldHide(MUD_Belly b)
	{
		if (b.m_iKx0 < 0)
			return;
		float since = b.m_fKeepSince - HIDE_HOLD - 0.05;
		for (int pz = PatchLo(b.m_iKz0, m_iNPZ); pz <= PatchHi(b.m_iKz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(b.m_iKx0, m_iNPX); px <= PatchHi(b.m_iKx1, m_iNPX); px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				int ox = px * PCELLS;
				int oz = pz * PCELLS;
				int lx0 = Math.ClampInt(b.m_iKx0 - ox, 0, PCELLS);
				int lx1 = Math.ClampInt(b.m_iKx1 - ox, 0, PCELLS);
				int lz0 = Math.ClampInt(b.m_iKz0 - oz, 0, PCELLS);
				int lz1 = Math.ClampInt(b.m_iKz1 - oz, 0, PCELLS);
				for (int lz = lz0; lz <= lz1; lz++)
				{
					for (int lx = lx0; lx <= lx1; lx++)
					{
						int v = lz * PVERTS + lx;
						if (p.HideAt(v) >= since)
						{
							p.EnsurePress().m_aHideAt[v] = m_fTime;
							p.m_fHideUntil = m_fTime + HIDE_HOLD;     // REBUILD 5 option 3
						}
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rut fill, server: bring the ground at wp to the shape ONE front pass leaves (rut Rut Step x Rut Speed
	//! deep with Press's falloff, berm half that high) where it is missing. Never deeper, never touches the
	//! pass timers: a rut the front made already (or deepened over several passes) stays as it is.
	protected bool Fill(vector wp, float depth)
	{
		float fx;
		float fz;
		if (!ToFine(wp, fx, fz))
			return false;
		int ccx = Math.Floor(fx / FINE);
		int ccz = Math.Floor(fz / FINE);
		if (!m_Skin.IsHoleCell(ccx, ccz))
			return false;
		float rr = m_fBermRadius / m_fFine;
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		int gx0 = Math.ClampInt(Math.Floor(fx - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(fx + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(fz - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(fz + rr), 0, gzmax);
		if (!EnsureRange(gx0, gx1, gz0, gz1, true))
		{
			m_iWheelCapped++;
			return false;
		}
		bool changed;
		float bermSpan = m_fBermRadius - m_fRutRadius;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float soft = VertexSoft(gx, gz) * p.m_aLock[v];
				if (soft <= 0)
					continue;
				float dx = (gx - fx) * m_fFine;
				float dz = (gz - fz) * m_fFine;
				float d = Math.Sqrt(dx * dx + dz * dz);
				float h = p.m_aH[v];
				if (d < m_fRutRadius)
				{
					float k = d / m_fRutRadius;
					float fall = 1 - k * k * 0.5;
					float y = Math.Max(-m_fRutStep * m_fRutSpeed * fall, Math.Max(-depth * soft, p.m_aFloor[v]));
					if (y < h - 0.002)
					{
						SetH(gx, gz, y);
						changed = true;
					}
				}
				else if (d < m_fBermRadius)
				{
					if (h < -0.001)
						continue;   // ruts win over berms, as in Press
					float t = 1 - (d - m_fRutRadius) / bermSpan;
					float y2 = Math.Min(m_fRutStep * m_fRutSpeed * 0.5 * t, m_fBermHeight * soft);
					if (y2 > h + 0.002)
					{
						SetH(gx, gz, y2);
						changed = true;
					}
				}
			}
		}
		if (changed)
			MarkDirty(gx0, gx1, gz0, gz1);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: ap = where wheel contact cp will be after lead m of travel, on its own arc through the turn
	//! (reverse works the same). hs = the vehicle's flat speed, yaw = YawRate. False when the vehicle barely
	//! moves flat or ap lies within Ahead Clear of a contact in m_aContacts (pressing there would change the
	//! ground under that wheel). ap is set either way, for the debug draw.
	protected bool AheadPoint(vector cp, Physics phys, float yaw, float hs, out vector ap, out float lead)
	{
		ap = cp;
		lead = 0;
		if (hs < 0.05)
			return false;
		lead = AheadLead(hs);
		ap = PathAt(cp, phys.GetVelocityAt(cp), yaw, lead / hs);   // y follows the vertical speed: only Press's height check reads it
		float clear2 = m_fAheadClear * m_fAheadClear;
		foreach (vector c : m_aContacts)
		{
			float dx = ap[0] - c[0];
			float dz = ap[2] - c[2];
			if (dx * dx + dz * dz < clear2)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: the vehicle's yaw rate (rad/s, about world up, same sign as the cross product PathAt uses),
	//! from the velocity of two points 2 m apart - no guess about GetAngularVelocity's units
	protected float YawRate(Vehicle veh, Physics phys)
	{
		vector a = veh.GetOrigin();
		vector b = veh.CoordToParent(Vector(0, 0, 2));
		vector dv = phys.GetVelocityAt(b) - phys.GetVelocityAt(a);
		float rx = b[0] - a[0];
		float rz = b[2] - a[2];
		float r2 = rx * rx + rz * rz;
		if (r2 < 0.01)
			return 0;
		return (dv[0] * rz - dv[2] * rx) / r2;
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: where body point p (velocity vp) is after t s at the current velocity + yaw rate: a circle
	//! round the turn centre, a line when barely turning. Vertical: vp[1] x t.
	protected vector PathAt(vector p, vector vp, float yaw, float t)
	{
		if (Math.AbsFloat(yaw) < 0.01)
			return p + vp * t;
		// p - turn centre, from v = w x r with w = (0, yaw, 0)
		float rx = -vp[2] / yaw;
		float rz = vp[0] / yaw;
		float th = yaw * t;
		float c = Math.Cos(th);
		float sn = Math.Sin(th);
		float nx = rx * c + rz * sn;
		float nz = -rx * sn + rz * c;
		return Vector(p[0] - rx + nx, p[1] + vp[1] * t, p[2] - rz + nz);
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: hide the strip half m to each side of body point p's path from t0 to t1 s ahead, as 4
	//! straight pieces overlapping 0.25 m (the first one starts exactly at t0)
	protected void ArcHide(vector p, vector vp, float yaw, float t0, float t1, float half)
	{
		if (t1 <= t0)
			return;
		vector a = PathAt(p, vp, yaw, t0);
		for (int i = 1; i <= 4; i++)
		{
			vector b = PathAt(p, vp, yaw, t0 + (t1 - t0) * i / 4);
			vector seg = Vector(b[0] - a[0], 0, b[2] - a[2]);
			float len = seg.Length();
			float back = -0.25;
			if (i == 1)
				back = 0;   // don't hide behind the reveal point
			if (len > 0.01)
				MarkHide(a, seg * (1 / len), back, len + 0.25, half);
			a = b;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: debug - body point p's path from now to t s ahead as 4 arrows
	protected void DrawPath(vector p, vector vp, float yaw, float t, int col)
	{
		vector a = p;
		for (int i = 1; i <= 4; i++)
		{
			vector b = PathAt(p, vp, yaw, t * i / 4);
			float head = 0.05;
			if (i == 4)
				head = 0.15;
			Shape.CreateArrow(a, b, head, col, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
			a = b;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: how far ahead (m) a vehicle at flat speed hs presses
	protected float AheadLead(float hs)
	{
		return Math.Min(Math.Clamp(m_fAheadMin + hs * m_fAheadTime, m_fAheadMin, m_fAheadMax), m_fLeadCap);
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: m_fLeadCap = how far veh still rolls while it slows down (hs^2 / 2 x slow-down + Rut Radius),
	//! so a stopping vehicle doesn't leave ruts ahead where its wheels never get (user 2026-10-11 screenshot:
	//! ~2 m of rut in front of a stopped Ural). 1000 = not slowing. Slow-down smoothed over ~0.2 s; once per
	//! frame per vehicle (a listen server calls it from Wheels and AheadView).
	protected void BrakeCap(Vehicle veh, VehicleWheeledSimulation sim, float hs)
	{
		m_fLeadCap = 1000;
		MUD_Belly b = Belly(veh, sim);
		if (!b)
			return;
		if (b.m_fBrakeAt != m_fTime)
		{
			float dt = m_fTime - b.m_fBrakeAt;
			if (dt > 0 && dt < 0.5)
			{
				float k = Math.Min(dt / 0.2, 1);
				b.m_fDecel += ((b.m_fBrakeHs - hs) / dt - b.m_fDecel) * k;
			}
			else
			{
				b.m_fDecel = 0;
			}
			b.m_fBrakeHs = hs;
			b.m_fBrakeAt = m_fTime;
		}
		if (b.m_fDecel > 0.5)
			m_fLeadCap = hs * hs / (2 * b.m_fDecel) + m_fRutRadius;
	}

	//------------------------------------------------------------------------------------------------
	protected bool BellyOnly(notnull IEntity e)
	{
		return e == m_BellyVeh;
	}

	//------------------------------------------------------------------------------------------------
	//! Body scrape: veh's measured underside, measured now if not yet (needs a wheel on the ground, to leave
	//! the wheel spots out). null = cannot measure yet. Same traces on every machine, so the same map.
	protected MUD_Belly Belly(Vehicle veh, VehicleWheeledSimulation sim)
	{
		MUD_Belly found;
		if (m_mBelly.Find(veh, found) && found && found.m_Veh == veh)
			return found;
		// audit B1: a miss is rare (one per vehicle): drop dead undersides here and rebuild the lookup
		m_mBelly.Clear();
		for (int i = m_aBellies.Count() - 1; i >= 0; i--)
		{
			MUD_Belly b = m_aBellies[i];
			if (!b.m_Veh)
				m_aBellies.Remove(i);
			else
				m_mBelly.Insert(b.m_Veh, b);
		}
		if (m_mBelly.Find(veh, found) && found)
			return found;
		// wheel contacts in vehicle space: underside points near them are the wheels, not the body
		array<vector> wheels = {};
		float lowY = 1000;
		for (int w = 0; w < sim.WheelCount(); w++)
		{
			if (!sim.WheelHasContact(w))
				continue;
			vector lc = veh.CoordToLocal(sim.WheelGetContactPosition(w));
			wheels.Insert(lc);
			lowY = Math.Min(lowY, lc[1]);
		}
		if (wheels.IsEmpty())
			return null;

		vector mins;
		vector maxs;
		veh.GetBounds(mins, maxs);
		MUD_Belly nb = new MUD_Belly();
		nb.m_Veh = veh;
		nb.m_fFront = -1000;
		nb.m_fBack = 1000;
		float wheelR = m_fRutRadius + 0.2;
		TraceParam tp = new TraceParam();
		tp.Flags = TraceFlags.ENTS;
		tp.LayerMask = EPhysicsLayerPresets.Terrain;    // what our ground collides with
		tp.Include = veh;
		m_BellyVeh = veh;
		int traces;
		for (float z = mins[2] + m_fBellyStep * 0.5; z < maxs[2]; z += m_fBellyStep)
		{
			for (float x = mins[0] + m_fBellyStep * 0.5; x < maxs[0]; x += m_fBellyStep)
			{
				bool nearWheel = false;
				foreach (vector wl : wheels)
				{
					float dx = x - wl[0];
					float dz = z - wl[2];
					if (dx * dx + dz * dz < wheelR * wheelR)
					{
						nearWheel = true;
						break;
					}
				}
				if (nearWheel)
					continue;
				tp.Start = veh.CoordToParent(Vector(x, mins[1] - 0.5, z));
				tp.End = veh.CoordToParent(Vector(x, maxs[1], z));
				traces++;
				float frac = GetWorld().TraceMove(tp, BellyOnly);
				if (frac >= 1)
					continue;
				vector hit = tp.Start + (tp.End - tp.Start) * frac;
				vector lh = veh.CoordToLocal(hit);
				float ly = lh[1];
				if (ly <= lowY + 0.05)
					continue;   // as low as a tyre bottom: a wheel collider, not the body
				nb.m_aPts.Insert(Vector(x, ly, z));
				nb.m_fFront = Math.Max(nb.m_fFront, z);
				nb.m_fBack = Math.Min(nb.m_fBack, z);
				nb.m_fHalfW = Math.Max(nb.m_fHalfW, Math.AbsFloat(x));
			}
		}
		m_BellyVeh = null;
		m_aBellies.Insert(nb);
		m_mBelly.Insert(veh, nb);
		float lowest = 1000;
		foreach (vector bp : nb.m_aPts)
			lowest = Math.Min(lowest, bp[1] - lowY);
		Print(string.Format("MUD scrape: %1 underside measured - %2 points of %3 traces, step %4 m, lowest %5 m over the tyre bottoms, wheels %6, bounds %7 .. %8",
			veh.GetID(), nb.m_aPts.Count(), traces, m_fBellyStep, lowest, wheels.Count(), mins, maxs));
		return nb;
	}

	//------------------------------------------------------------------------------------------------
	//! Body scrape, server: every underside point of veh, moved lead m along its arc (PathAt), cuts the
	//! ground under it down to underside - Scrape Margin. m_aContacts must hold veh's wheel contacts.
	protected void Scrape(Vehicle veh, VehicleWheeledSimulation sim, vector vel)
	{
		vector flat = Vector(vel[0], 0, vel[2]);
		float hs = flat.Length();
		if (hs < 0.05)
			return;
		MUD_Belly b = Belly(veh, sim);
		if (!b)
			return;
		Physics phys = veh.GetPhysics();
		float yaw = YawRate(veh, phys);
		float t = AheadLead(hs) / hs;
		float clear2 = m_fAheadClear * m_fAheadClear;
		foreach (vector lp : b.m_aPts)
		{
			vector wp;
			if (!BellyAhead(veh, phys, lp, yaw, t, wp))
				continue;
			bool nearWheel = false;
			foreach (vector c : m_aContacts)
			{
				float dx = wp[0] - c[0];
				float dz = wp[2] - c[2];
				if (dx * dx + dz * dz < clear2)
				{
					nearWheel = true;
					break;
				}
			}
			if (!nearWheel && Cut(wp, wp[1] - m_fScrapeMargin, m_fScrapeStep, m_fScrapeMaxDepth, m_fScrapeReach, false))
				m_iScrapeCuts++;
		}
		Spill(veh, b, flat * (1 / hs));     // F1: the scraped ground goes beside the body
	}

	//------------------------------------------------------------------------------------------------
	//! Body scrape: underside point lp (vehicle space) after t s on its arc (PathAt). Its height follows the
	//! UNDEFORMED ground between here and there, not the vertical speed: every suspension bounce predicted a
	//! lower belly and cuts only go down, so the lowest bounce won (user 2026-10-11: deforms too much too fast).
	//! False where the undeformed ground is unknown.
	protected bool BellyAhead(Vehicle veh, Physics phys, vector lp, float yaw, float t, out vector ap)
	{
		vector now = veh.CoordToParent(lp);
		ap = PathAt(now, phys.GetVelocityAt(now), yaw, t);
		float fx0;
		float fz0;
		float fx1;
		float fz1;
		if (!ToFine(now, fx0, fz0) || !ToFine(ap, fx1, fz1))
			return false;
		bool ok0;
		bool ok1;
		float b0 = BaseAt(fx0, fz0, ok0);
		float b1 = BaseAt(fx1, fz1, ok1);
		if (!ok0 || !ok1)
			return false;
		ap[1] = now[1] + b1 - b0;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Body scrape: our ground's world Y at the fine vertex nearest (fx, fz) (the skin where no patch is).
	//! False off the map.
	protected bool GroundY(float fx, float fz, out float surf)
	{
		int cgx = Math.ClampInt(Math.Round(fx), 0, m_iNPX * PCELLS);
		int cgz = Math.ClampInt(Math.Round(fz), 0, m_iNPZ * PCELLS);
		int cv;
		MUD_FinePatch cp = At(cgx, cgz, cv);
		if (cp)
		{
			surf = cp.m_aBase[cv] + cp.m_aH[cv];
			return true;
		}
		bool ok;
		surf = BaseAt(fx, fz, ok);
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Body scrape: scrape pass timer of global fine vertex (gx, gz) in every patch holding it (as SetTouch)
	protected void SetScrapeTouch(int gx, int gz, float t)
	{
		int pxa = gx / PCELLS;
		int pza = gz / PCELLS;
		for (int pz = pza; pz >= pza - 1; pz--)
		{
			int lz = gz - pz * PCELLS;
			if (pz < 0 || pz >= m_iNPZ || lz < 0 || lz > PCELLS)
				continue;
			for (int px = pxa; px >= pxa - 1; px--)
			{
				int lx = gx - px * PCELLS;
				if (px < 0 || px >= m_iNPX || lx < 0 || lx > PCELLS)
					continue;
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (p)
					p.EnsurePress().m_aScrapeTouch[lz * PVERTS + lx] = t;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ditch escape, server: a vehicle with a driver, throttle on, wheels spinning and barely moving digs one
	//! Escape Step every Escape Interval: the ground Escape Lead in front of its underside, and the lip in front
	//! of each wheel that pushes against it. The spin direction tells forward from reverse; each wheel's sign is
	//! learned while the vehicle drives normally (WheelGetAngularSpeed sign: likely differs left / right).
	protected void Escape(Vehicle veh, VehicleWheeledSimulation sim, Physics phys)
	{
		vector vel = phys.GetVelocity();
		vector flat = Vector(vel[0], 0, vel[2]);
		float hs = flat.Length();
		vector fwd = veh.CoordToParent(Vector(0, 0, 1)) - veh.GetOrigin();
		fwd[1] = 0;
		float fl = fwd.Length();
		if (fl < 0.1)
			return;   // standing on its nose or tail
		fwd = fwd * (1 / fl);
		MUD_Belly b = Belly(veh, sim);
		if (!b)
			return;
		int count = sim.WheelCount();
		while (b.m_aSpinSign.Count() < count)
			b.m_aSpinSign.Insert(0);
		float along = vel[0] * fwd[0] + vel[2] * fwd[2];
		// one shared sign flipped every few frames (test 2026-10-11 18:57; inference: left / right wheels
		// report opposite signs), so each wheel learns its own while rolling normally
		if (hs > 2 && Math.AbsFloat(along) > 1.5)
		{
			for (int lw = 0; lw < count; lw++)
			{
				float av = sim.WheelGetAngularSpeed(lw) * sim.WheelGetRadius(lw);
				if (!sim.WheelHasContact(lw) || Math.AbsFloat(av) < 1)
					continue;
				int sign = 1;
				if ((av > 0) != (along > 0))
					sign = -1;
				if (b.m_aSpinSign[lw] == 0 && m_bDebug)     // audit B11: the first time only (it flips on every reverse)
					Print(string.Format("MUD escape: veh %1 wheel %2 spin sign learned %3", veh.GetID(), lw, sign));
				b.m_aSpinSign[lw] = sign;
			}
			return;
		}
		// fastest spinning grounded wheel, as forward rim speed (> 0 = trying to go forward)
		float spin = 0;
		for (int w = 0; w < count; w++)
		{
			if (!sim.WheelHasContact(w) || b.m_aSpinSign[w] == 0)
				continue;   // unlearned wheels don't count: drive normally a bit first
			float rim = sim.WheelGetAngularSpeed(w) * sim.WheelGetRadius(w) * b.m_aSpinSign[w];
			if (Math.AbsFloat(rim) > Math.AbsFloat(spin))
				spin = rim;
		}
		bool pushing = veh.GetPilot() != null && sim.EngineIsOn() && sim.GetThrottle() >= 0.3 && !sim.IsHandbrakeOn();
		if (b.m_iEscapePending > 0 && (!pushing || hs >= m_fEscapeSpeed))
		{
			// let off (or got moving): dig the saved steps now, while no wheel pushes
			int steps = b.m_iEscapePending;
			b.m_iEscapePending = 0;
			Dig(veh, sim, b, b.m_vEscapeDir, m_fEscapeStep * steps, hs, spin, steps);
			return;
		}
		if (hs >= m_fEscapeSpeed || Math.AbsFloat(spin) < hs + m_fEscapeSlip || !pushing)
			return;
		if (m_fTime - b.m_fEscapeAt < m_fEscapeInterval)
			return;
		b.m_fEscapeAt = m_fTime;
		vector dir = fwd;
		if (spin < 0)
			dir = fwd * -1;
		if (!m_bEscapeOnRelease)
		{
			Dig(veh, sim, b, dir, m_fEscapeStep, hs, spin, 1);
			return;
		}
		if (b.m_iEscapePending < m_iEscapeMaxPending)
			b.m_iEscapePending++;
		b.m_vEscapeDir = dir;
		Lips(sim, dir, b.m_aEscapeLips);   // now, while the wheels still push: on release they may roll back
		if (m_bDebug)
			Print(string.Format("MUD escape: veh %1 stuck (speed %2 m/s, forward spin %3 m/s) - %4 dig steps saved, dug when the throttle is let off",
				veh.GetID(), hs, spin, b.m_iEscapePending));
	}

	//------------------------------------------------------------------------------------------------
	//! Ditch escape: lips = a point 0.15 m along dir from each wheel whose contact normal faces back at dir
	//! (the wheel pushes against a lip); Y = that contact's height
	protected void Lips(VehicleWheeledSimulation sim, vector dir, array<vector> lips)
	{
		lips.Clear();
		int count = sim.WheelCount();
		for (int i = 0; i < count; i++)
		{
			if (!sim.WheelHasContact(i))
				continue;
			vector n = sim.WheelGetContactNormal(i);
			if (n[0] * dir[0] + n[2] * dir[2] > -0.2)
				continue;
			vector cp = sim.WheelGetContactPosition(i);
			lips.Insert(cp + dir * 0.15);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ditch escape, server: lower the ground in front of veh's underside (Escape Lead along dir) and the
	//! lips its wheels push against, by up to step m. steps = how many saved-up steps this is (log only).
	protected void Dig(Vehicle veh, VehicleWheeledSimulation sim, MUD_Belly b, vector dir, float step, float hs, float spin, int steps)
	{
		Contacts(veh, sim);
		m_iEscapeSteps += steps;
		int bodyCuts = 0;
		int wheelCuts = 0;
		// underside: the ground in front of it, down to underside - margin
		float keep = m_fRutRadius + 0.2;   // don't dig under a wheel: it would sink in place
		foreach (vector lp : b.m_aPts)
		{
			vector now = veh.CoordToParent(lp);
			vector ap = now + dir * m_fEscapeLead;
			bool nearWheel = false;
			foreach (vector c : m_aContacts)
			{
				float dx = ap[0] - c[0];
				float dz = ap[2] - c[2];
				if (dx * dx + dz * dz < keep * keep)
				{
					nearWheel = true;
					break;
				}
			}
			if (!nearWheel && Cut(ap, now[1] - m_fScrapeMargin, step, m_fEscapeMaxDepth, m_fEscapeReach, true))
				bodyCuts++;
		}
		// lips the wheels pushed against: saved while pushing (dig on release), else right now
		if (!m_bEscapeOnRelease)
			Lips(sim, dir, b.m_aEscapeLips);
		foreach (vector lip : b.m_aEscapeLips)
		{
			if (Cut(lip, lip[1] - step, step, m_fEscapeMaxDepth, m_fEscapeReach, true))
				wheelCuts++;
		}
		b.m_aEscapeLips.Clear();
		Spill(veh, b, dir);     // F1: the dug ground goes beside the vehicle, not back in its way
		m_iEscapeCuts += bodyCuts + wheelCuts;
		if (m_bDebug)
			Print(string.Format("MUD escape: veh %1 dug %2 step(s), %3 m (speed %4 m/s, forward spin %5 m/s) - body cuts %6, wheel cuts %7",
				veh.GetID(), steps, step, hs, spin, bodyCuts, wheelCuts));
	}

	//------------------------------------------------------------------------------------------------
	//! Body scrape, server: lower the ground within Scrape Radius of wp to world Y target (never deeper than
	//! Scrape Max Depth, the floor, or into hard / locked ground). False when nothing there was above target.
	//! step / maxDepth / reach per call so ditch escape can dig deeper and higher; timed = the caller limits
	//! the rate, no pass check.
	protected bool Cut(vector wp, float target, float step, float maxDepth, float reach, bool timed)
	{
		float fx;
		float fz;
		if (!ToFine(wp, fx, fz))
			return false;
		int ccx = Math.Floor(fx / FINE);
		int ccz = Math.Floor(fz / FINE);
		if (!m_Skin.IsHoleCell(ccx, ccz))
			return false;   // kept terrain (road, water edge)
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		// cheap test at the nearest vertex first: most points are clear
		float surf;
		if (!GroundY(fx, fz, surf) || surf <= target + 0.002 || surf - target > reach)
			return false;

		float rr = m_fScrapeRadius / m_fFine;
		int gx0 = Math.ClampInt(Math.Floor(fx - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(fx + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(fz - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(fz + rr), 0, gzmax);
		if (!EnsureRange(gx0, gx1, gz0, gz1, true))
		{
			m_iWheelCapped++;
			return false;
		}
		bool changed;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float ddx = gx - fx;
				float ddz = gz - fz;
				if (ddx * ddx + ddz * ddz > rr * rr)
					continue;
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float soft = VertexSoft(gx, gz) * p.m_aLock[v];
				if (soft <= 0)
					continue;
				float h = p.m_aH[v];
				float want = Math.Max(target - p.m_aBase[v], Math.Max(-maxDepth * soft, p.m_aFloor[v]));
				if (want >= h - 0.002)
					continue;
				if (!timed)
				{
					// one step per pass, like a rut: the belly staying over it keeps the pass going
					bool newPass = m_fTime - p.ScrapeTouch(v) > m_fPassGap;
					SetScrapeTouch(gx, gz, m_fTime);
					if (!newPass)
						continue;
				}
				float nh = Math.Max(want, h - step);
				m_fCutVol += h - nh;     // F1 spill: summed vertex drops (m); x fine cell area = volume
				SetH(gx, gz, nh);
				changed = true;
			}
		}
		if (changed)
			MarkDirty(gx0, gx1, gz0, gz1);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! F1 PUSHED DIRT (user 2026-10-24, server): the ground Cut took since the last Spill (m_fCutVol) goes back up as
	//! two low piles beside the vehicle's leading end (front driving forward, rear backing up), SPILL_OUT m out from its
	//! measured underside - a cut lip spills to the sides, not back into the way. SPILL_SHARE of it (the rest packs
	//! down). Each pile is a cone of SPILL_R radius, never higher than SPILL_TOP over the undug ground.
	static const bool SPILL = true;
	static const float SPILL_SHARE = 0.7;
	static const float SPILL_OUT = 1.0;     // pile centre this far out: its edge stays ~0.25 m clear of the body (wheels)
	static const float SPILL_R = 1.0;      // WB 2026-10-24 (0.75 / 0.5): a rod poked up beside a reversing truck
	static const float SPILL_TOP = 0.3;
	protected float m_fCutVol;
	protected int m_iSpills;          // 10 s log
	protected float m_fSpillVol;
	protected void Spill(Vehicle veh, MUD_Belly b, vector dir)
	{
		float vol = m_fCutVol * m_fFine * m_fFine * SPILL_SHARE;
		m_fCutVol = 0;
		if (!SPILL || vol <= 0.0005)
			return;
		vector fwd = veh.GetTransformAxis(2);
		float z = b.m_fFront;
		if (vector.Dot(fwd, dir) < 0)
			z = b.m_fBack;
		float x = b.m_fHalfW + SPILL_OUT;
		Pile(veh.CoordToParent(Vector(x, 0, z)), vol * 0.5);
		Pile(veh.CoordToParent(Vector(-x, 0, z)), vol * 0.5);
		m_iSpills++;
		m_fSpillVol += vol;
	}

	//------------------------------------------------------------------------------------------------
	//! F1: raise a cone of volume vol (m3) at world x, z of wp (y ignored) on soft, unlocked hole cells
	protected void Pile(vector wp, float vol)
	{
		PileR(wp, vol, SPILL_R, SPILL_TOP);
	}

	//------------------------------------------------------------------------------------------------
	//! Pile with its own radius r (m) and dome top (m over the undug ground) - heli impact mound / berms
	protected void PileR(vector wp, float vol, float r, float top)
	{
		float fx;
		float fz;
		if (!ToFine(wp, fx, fz))
			return;
		int ccx = Math.Floor(fx / FINE);
		int ccz = Math.Floor(fz / FINE);
		if (!m_Skin.IsHoleCell(ccx, ccz))
			return;   // kept terrain (road): the dirt is lost
		float peak = 3 * vol / (Math.PI * r * r);
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		float rr = r / m_fFine;
		int gx0 = Math.ClampInt(Math.Floor(fx - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(fx + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(fz - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(fz + rr), 0, gzmax);
		if (!EnsureRange(gx0, gx1, gz0, gz1, true))
			return;
		bool raised;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float ddx = gx - fx;
				float ddz = gz - fz;
				float d = Math.Sqrt(ddx * ddx + ddz * ddz) / rr;
				if (d >= 1)
					continue;
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float soft = VertexSoft(gx, gz) * p.m_aLock[v];
				if (soft <= 0)
					continue;
				float h = p.m_aH[v];
				// dome cap, not a flat cap (WB 2026-10-24: each spill added most at the centre vertex, so repeated small
				// piles stacked into a one-vertex rod): never above SPILL_TOP x (1 - d^2) over the undug ground
				float up = Math.Min(h + peak * (1 - d), top * soft * (1 - d * d));
				if (up <= h + 0.001)
					continue;
				SetH(gx, gz, up);
				raised = true;
			}
		}
		if (raised)
			MarkDirty(gx0, gx1, gz0, gz1);
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts, every rendering machine: hide the ground changes in front of each moving wheel (BuildMesh
	//! shows m_aShow there) and draw the debug points. Runs from each machine's own view of the vehicles, so
	//! the client hides what the server pressed ahead before the wheel gets there.
	protected void AheadView()
	{
		if (!m_bAheadRuts || !m_bRender)
			return;
		bool hideOn = !AHEAD_HIDE_SKIP || !MUD_VoxelProbe.ENABLED || !MUD_VoxelProbe.RUTS;     // PERF B7
		vector viewCam[4];
		GetWorld().GetCurrentCamera(viewCam);
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh)
				continue;
			// audit B2: a vehicle past the client build radius (+ 3 s of travel) shows nothing here - the look comes
			// from the server's ruts when the camera gets near
			Physics gph = veh.GetPhysics();
			float gs = 0;
			if (gph)
				gs = gph.GetVelocity().Length();
			if (vector.Distance(veh.GetOrigin(), viewCam[3]) > m_fClientBuildRadius + gs * 3)
			{
				m_iParkSkips++;
				continue;
			}
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!sim || !sim.IsValid())
				continue;
			Physics phys = veh.GetPhysics();
			if (!phys)
				continue;
			vector vel = phys.GetVelocity();
			vector flat = Vector(vel[0], 0, vel[2]);
			float hs = flat.Length();
			MUD_Belly hb = Belly(veh, sim);
			bool stopped = vel.Length() < 0.2 || hs < 0.05;
			// a stopped vehicle presses nothing: what was hidden when it stopped stays hidden Stop Hide s - also
			// once it backs off the other way (user 2026-10-11) - the server puts back the ahead presses its
			// wheels never reached. Driving on the same way ends it (the wheels get there, normal hide).
			if (hb && m_bAheadUndo)
			{
				if (stopped && hb.m_fStopAt < 0)
				{
					hb.m_fStopAt = m_fTime;
					hb.m_iKx0 = hb.m_iHx0;
					hb.m_iKx1 = hb.m_iHx1;
					hb.m_iKz0 = hb.m_iHz0;
					hb.m_iKz1 = hb.m_iHz1;
					hb.m_vKeepDir = hb.m_vHideDir;
					hb.m_fKeepSince = m_fTime;
					hb.m_fKeepUntil = m_fTime + m_fStopHide;
				}
				if (!stopped)
				{
					hb.m_fStopAt = -100;
					if (vector.Dot(flat * (1 / hs), hb.m_vKeepDir) > 0.5)
						hb.m_fKeepUntil = -100;
				}
				if (hideOn && m_fTime < hb.m_fKeepUntil)
				{
					int tHh = System.GetTickCount();
					HoldHide(hb);
					MUD_Prof.Add(MUD_Prof.A_HIDE, System.GetTickCount(tHh));
				}
			}
			if (stopped)
				continue;
			m_iHideX0 = -1;
			vector dir = flat * (1 / hs);
			// a vertex shows up to HIDE_HOLD + one mesh interval after it left the corridor: start the
			// corridor that much farther ahead so the rut still appears Reveal Ahead in front of the wheel
			float revealFrom = m_fRevealAhead + hs * (HIDE_HOLD + m_fMeshInterval);
			BrakeCap(veh, sim, hs);     // ScrapeView reads m_fLeadCap (AheadLead) and yaw
			float yaw = YawRate(veh, phys);
			int pressed = 0;
			int skipped = 0;
			float lead = 0;
			// REBUILD 6 VIEW_TRIM: the per-wheel loop only feeds the wheel hide and the debug draw (the press path calls
			// its own Contacts)
			bool wheelLoop = !VIEW_TRIM || hideOn || m_bAheadDraw;
			int wheelN = 0;
			if (wheelLoop)
			{
				Contacts(veh, sim);
				wheelN = m_aWheelPress.Count();
			}
			for (int w = 0; w < wheelN; w++)
			{
				if (m_aWheelPress[w] == 0)
					continue;   // in the air, or a middle axle
				vector cp = sim.WheelGetContactPosition(w);
				vector vp = phys.GetVelocityAt(cp);
				vector ap;
				bool ok = AheadPoint(cp, phys, yaw, hs, ap, lead);
				if (ok)
					pressed++;
				else
					skipped++;
				// to the far edge of the press (berm radius) + 0.5 m slack: client and server leads differ a little
				if (hideOn)
				{
					int tAc = System.GetTickCount();
					ArcHide(cp, vp, yaw, revealFrom / hs, (lead + m_fBermRadius + 0.5) / hs, m_fBermRadius);
					MUD_Prof.Add(MUD_Prof.A_HIDE, System.GetTickCount(tAc));
				}
				if (!m_bAheadDraw)
					continue;
				int col = ARGB(255, 0, 255, 0);
				if (!ok)
					col = ARGB(255, 255, 0, 0);
				DrawPath(cp, vp, yaw, lead / hs, col);
				Shape.CreateSphere(col, ShapeFlags.WIREFRAME | ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, ap, m_fRutRadius);
				Shape.CreateSphere(ARGB(255, 255, 220, 0), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, PathAt(cp, vp, yaw, m_fRevealAhead / hs), 0.12);
				if (!m_bRutFill)
					continue;
				vector vo = veh.GetOrigin();
				AxleSpan(vo, dir);
				if (vector.Dot(cp - vo, dir) >= m_fSpanMin + AXLE_GAP)
					continue;   // not the trailing axle
				vector fp;
				int fcol = ARGB(255, 0, 120, 255);
				if (!FillPoint(cp, vp, yaw, hs, vo, dir, fp))
					fcol = ARGB(255, 120, 120, 120);
				Shape.CreateArrow(cp, fp, 0.1, fcol, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
				Shape.CreateSphere(fcol, ShapeFlags.WIREFRAME | ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, fp, m_fRutRadius);
			}
			if (m_bScrape)
				ScrapeView(veh, sim, phys, yaw, dir, hs, revealFrom);
			if (m_bRamp && m_bAheadDraw)
				Ramp(veh, sim, dir, hs, true);
			if (hb && m_iHideX0 >= 0)
			{
				hb.m_iHx0 = m_iHideX0;
				hb.m_iHx1 = m_iHideX1;
				hb.m_iHz0 = m_iHideZ0;
				hb.m_iHz1 = m_iHideZ1;
				hb.m_vHideDir = dir;
			}
			if (m_bAheadDraw)
			{
				vector o = veh.GetOrigin();
				float yawDeg = yaw * Math.RAD2DEG;
				string txt = string.Format("lead %1 m | reveal %2 m | %3 m/s | turn %4 deg/s | pressed %5 skipped %6",
					lead.ToString(-1, 1), m_fRevealAhead.ToString(-1, 1), hs.ToString(-1, 1), yawDeg.ToString(-1, 0), pressed, skipped);
				DebugTextWorldSpace.Create(GetWorld(), txt, DebugTextFlags.ONCE | DebugTextFlags.CENTER | DebugTextFlags.FACE_CAMERA,
					o[0], o[1] + 3.5, o[2], 16, ARGB(255, 255, 255, 255), ARGB(160, 0, 0, 0));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Body scrape, every rendering machine: hide the strip in front of the body where the server scrapes,
	//! and draw the underside points a lead ahead (orange = this machine's ground is above the cut height)
	protected void ScrapeView(Vehicle veh, VehicleWheeledSimulation sim, Physics phys, float yaw, vector dir, float hs, float revealFrom)
	{
		MUD_Belly b = Belly(veh, sim);
		if (!b || b.m_aPts.IsEmpty())
			return;
		float lead = AheadLead(hs);
		float t = lead / hs;
		vector o = veh.GetOrigin();
		float front = -1000;
		float half = 0;
		int cutCol = ARGB(255, 255, 140, 0);
		int clearCol = ARGB(255, 0, 220, 255);
		foreach (vector lp : b.m_aPts)
		{
			vector wp = veh.CoordToParent(lp);
			float rx = wp[0] - o[0];
			float rz = wp[2] - o[2];
			front = Math.Max(front, rx * dir[0] + rz * dir[2]);
			half = Math.Max(half, Math.AbsFloat(rx * dir[2] - rz * dir[0]));
			if (!m_bScrapeDraw)
				continue;
			vector ap;
			if (!BellyAhead(veh, phys, lp, yaw, t, ap))
				continue;
			float fx;
			float fz;
			float surf;
			int col = clearCol;
			if (ToFine(ap, fx, fz) && GroundY(fx, fz, surf) && surf > ap[1] - m_fScrapeMargin + 0.002 && surf - (ap[1] - m_fScrapeMargin) <= m_fScrapeReach)
				col = cutCol;
			Shape.CreateSphere(col, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, ap, 0.06);
		}
		// the strip ahead of the body follows the arc of its front centre
		vector fp = o + dir * front;
		ArcHide(fp, phys.GetVelocityAt(fp), yaw, revealFrom / hs, (lead + m_fScrapeRadius + 0.5) / hs, half + m_fScrapeRadius);
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts: mark every patch copy of the verts in the strip d0..d1 m ahead of cp along dir, half
	//! m to each side, as hidden now
	protected void MarkHide(vector cp, vector dir, float d0, float d1, float half)
	{
		if (d1 <= d0)
			return;
		float cfx;
		float cfz;
		float bfx;
		float bfz;
		ToFine(cp, cfx, cfz);
		ToFine(cp + dir * d1, bfx, bfz);
		float rr = half / m_fFine;
		float f0 = d0 / m_fFine;
		float f1 = d1 / m_fFine;
		float dx = dir[0];
		float dz = dir[2];
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		int gx0 = Math.ClampInt(Math.Floor(Math.Min(cfx, bfx) - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(Math.Max(cfx, bfx) + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(Math.Min(cfz, bfz) - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(Math.Max(cfz, bfz) + rr), 0, gzmax);
		if (m_iHideX0 < 0)
		{
			m_iHideX0 = gx0;
			m_iHideX1 = gx1;
			m_iHideZ0 = gz0;
			m_iHideZ1 = gz1;
		}
		else
		{
			m_iHideX0 = Math.Min(m_iHideX0, gx0);
			m_iHideX1 = Math.Max(m_iHideX1, gx1);
			m_iHideZ0 = Math.Min(m_iHideZ0, gz0);
			m_iHideZ1 = Math.Max(m_iHideZ1, gz1);
		}
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				int ox = px * PCELLS;
				int oz = pz * PCELLS;
				int lx0 = Math.ClampInt(gx0 - ox, 0, PCELLS);
				int lx1 = Math.ClampInt(gx1 - ox, 0, PCELLS);
				int lz0 = Math.ClampInt(gz0 - oz, 0, PCELLS);
				int lz1 = Math.ClampInt(gz1 - oz, 0, PCELLS);
				MUD_PatchPress pr = null;     // REBUILD 6 VIEW_TRIM: one EnsurePress per patch (made at the first in-strip vertex)
				for (int lz = lz0; lz <= lz1; lz++)
				{
					float rz = oz + lz - cfz;
					for (int lx = lx0; lx <= lx1; lx++)
					{
						float rx = ox + lx - cfx;
						float along = rx * dx + rz * dz;
						if (along < f0 || along > f1)
							continue;
						if (Math.AbsFloat(rx * dz - rz * dx) > rr)
							continue;
						if (!VIEW_TRIM)
						{
							p.EnsurePress().m_aHideAt[lz * PVERTS + lx] = m_fTime;
							p.m_fHideUntil = m_fTime + HIDE_HOLD;     // REBUILD 5 option 3: no partial pass while hidden
							continue;
						}
						if (!pr)
							pr = p.EnsurePress();
						pr.m_aHideAt[lz * PVERTS + lx] = m_fTime;
					}
				}
				if (pr)
					p.m_fHideUntil = m_fTime + HIDE_HOLD;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lowest patch index whose vertex range reaches global vertex g (a vertex on an edge is in 2)
	protected int PatchLo(int g, int n)
	{
		if (g <= 0)
			return 0;
		return Math.ClampInt((g - 1) / PCELLS, 0, n - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected int PatchHi(int g, int n)
	{
		if (g <= 0)
			return 0;
		return Math.ClampInt(g / PCELLS, 0, n - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Audit A1, server: a heightfield op changed fine vertices gx0..gx1 x gz0..gz1 - things on that ground follow
	//! it (MUD_GroundFollow.ChangedHeight) once the colliders are rebuilt (FollowTick). A job of the same kind that
	//! overlaps and still waits grows instead (free dig = 2 pieces, repeated swings).
	protected void FollowQueue(int gx0, int gx1, int gz0, int gz1, int kind, float lift)
	{
		if (IsProxy())
			return;
		for (int j = 0; j < m_aFollowKind.Count(); j++)
		{
			int b = j * 4;
			if (m_aFollowKind[j] != kind || gx0 > m_aFollowG[b + 1] || gx1 < m_aFollowG[b] || gz0 > m_aFollowG[b + 3] || gz1 < m_aFollowG[b + 2])
				continue;
			m_aFollowG[b] = Math.Min(m_aFollowG[b], gx0);
			m_aFollowG[b + 1] = Math.Max(m_aFollowG[b + 1], gx1);
			m_aFollowG[b + 2] = Math.Min(m_aFollowG[b + 2], gz0);
			m_aFollowG[b + 3] = Math.Max(m_aFollowG[b + 3], gz1);
			m_aFollowLift[j] = Math.Max(m_aFollowLift[j], lift);
			return;
		}
		m_aFollowG.Insert(gx0);
		m_aFollowG.Insert(gx1);
		m_aFollowG.Insert(gz0);
		m_aFollowG.Insert(gz1);
		m_aFollowKind.Insert(kind);
		m_aFollowLift.Insert(lift);
		m_aFollowAt.Insert(m_fTime);
	}

	//------------------------------------------------------------------------------------------------
	//! Audit A1, server, each frame with jobs: a job whose awake patches (range + blur reach) have their new collider
	//! (built, not dirty) - or that waited FOLLOW_WAIT_S - runs MUD_GroundFollow over its box. Asleep patches do not
	//! hold it: the skin collider is there and a vehicle near wakes the patch.
	protected void FollowTick()
	{
		int r = m_aSmoothW.Count() / 2;
		for (int j = m_aFollowKind.Count() - 1; j >= 0; j--)
		{
			int b = j * 4;
			int gx0 = m_aFollowG[b];
			int gx1 = m_aFollowG[b + 1];
			int gz0 = m_aFollowG[b + 2];
			int gz1 = m_aFollowG[b + 3];
			bool ready = m_fTime - m_aFollowAt[j] >= FOLLOW_WAIT_S;
			if (!ready)
			{
				ready = true;
				for (int pz = PatchLo(gz0 - r, m_iNPZ); ready && pz <= PatchHi(gz1 + r, m_iNPZ); pz++)
				{
					for (int px = PatchLo(gx0 - r, m_iNPX); px <= PatchHi(gx1 + r, m_iNPX); px++)
					{
						MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
						if (p && !p.m_bAsleep && (p.m_bColDirty || p.m_iBuild > 0))
						{
							ready = false;
							break;
						}
					}
				}
			}
			if (!ready)
				continue;
			vector lo = Vector(m_fX0 + gx0 * m_fFine, 0, m_fZ0 + gz0 * m_fFine);
			vector hi = Vector(m_fX0 + gx1 * m_fFine, 0, m_fZ0 + gz1 * m_fFine);
			int kind = m_aFollowKind[j];
			float lift = m_aFollowLift[j];
			for (int k = 3; k >= 0; k--)
				m_aFollowG.RemoveOrdered(b + k);
			m_aFollowKind.RemoveOrdered(j);
			m_aFollowLift.RemoveOrdered(j);
			m_aFollowAt.RemoveOrdered(j);
			MUD_GroundFollow.ChangedHeight(lo, hi, kind, lift);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! m_Cells = the skin cells InitPatch(p) reads: the patch's 4 x 4 cells, one ring round them, and every
	//! cell within Edge Fade of it (BaseAt asks cells -1..+4, EdgeLock cells within m_fEdgeFade)
	protected void FillCellCache(MUD_FinePatch p)
	{
		MUD_CellCache cc = m_Cells;
		cc.m_iNX = 0;
		float r = Math.AbsFloat(m_fEdgeFade);
		float size = PCELLS * m_fFine;
		float x0 = p.m_iPx * size;
		float z0 = p.m_iPz * size;
		int xl = p.m_iPx * SKIN - 1;
		int xh = p.m_iPx * SKIN + SKIN;
		int zl = p.m_iPz * SKIN - 1;
		int zh = p.m_iPz * SKIN + SKIN;
		int f = Math.Floor((x0 - r) / m_fCell);
		if (f < xl)
			xl = f;
		f = Math.Floor((x0 + size + r) / m_fCell);
		if (f > xh)
			xh = f;
		f = Math.Floor((z0 - r) / m_fCell);
		if (f < zl)
			zl = f;
		f = Math.Floor((z0 + size + r) / m_fCell);
		if (f > zh)
			zh = f;
		int nx = xh - xl + 1;
		int nz = zh - zl + 1;
		if (nx * nz > 256)
			return;     // Edge Fade over ~10 m: InitPatch asks the skin directly
		cc.m_iX0 = xl;
		cc.m_iZ0 = zl;
		cc.m_bAnyKept = false;
		float h00;
		float h10;
		float h01;
		float h11;
		bool tlbr;
		ResourceName mat;
		for (int cz = zl; cz <= zh; cz++)
		{
			for (int cx = xl; cx <= xh; cx++)
			{
				int i = (cz - zl) * nx + cx - xl;
				if (!m_Skin.CellShape(cx, cz, h00, h10, h01, h11, tlbr, mat))
				{
					cc.m_aHole[i] = 0;    // kept (or off the map): same answer as IsHoleCell
					cc.m_bAnyKept = true;
					continue;
				}
				cc.m_aHole[i] = 1;
				cc.m_aH00[i] = h00;
				cc.m_aH10[i] = h10;
				cc.m_aH01[i] = h01;
				cc.m_aH11[i] = h11;
				cc.m_aTlbr[i] = 0;
				if (tlbr)
					cc.m_aTlbr[i] = 1;
			}
		}
		cc.m_iNX = nx;
		cc.m_iNZ = nz;
	}

	//------------------------------------------------------------------------------------------------
	//! EdgeLock on m_Cells (FillCellCache for this vertex's patch first)
	protected float EdgeLockCached(int gx, int gz)
	{
		MUD_CellCache cc = m_Cells;
		float x = gx * m_fFine;
		float z = gz * m_fFine;
		int cx0 = Math.Floor((x - m_fEdgeFade) / m_fCell);
		int cx1 = Math.Floor((x + m_fEdgeFade) / m_fCell);
		int cz0 = Math.Floor((z - m_fEdgeFade) / m_fCell);
		int cz1 = Math.Floor((z + m_fEdgeFade) / m_fCell);
		float best = 1e9;
		for (int cz = cz0; cz <= cz1; cz++)
		{
			for (int cx = cx0; cx <= cx1; cx++)
			{
				if (cc.m_aHole[(cz - cc.m_iZ0) * cc.m_iNX + cx - cc.m_iX0] != 0)
					continue;
				float dx = Math.Max(Math.Max(cx * m_fCell - x, 0), x - (cx + 1) * m_fCell);
				float dz = Math.Max(Math.Max(cz * m_fCell - z, 0), z - (cz + 1) * m_fCell);
				float d = Math.Sqrt(dx * dx + dz * dz);
				if (d < best)
					best = d;
			}
		}
		if (m_fEdgeFade <= 0)
		{
			if (best > 0)
				return 1;
			return 0;
		}
		float t = Math.Clamp(best / m_fEdgeFade, 0, 1);
		return t * t * (3 - 2 * t);
	}

	//------------------------------------------------------------------------------------------------
	//! Undeformed height at fine coords (fx, fz) from the skin triangles; ok false where no hole cell
	//! touches the point (kept terrain: such a vertex is never drawn and always locked)
	protected float BaseAt(float fx, float fz, out bool ok)
	{
		int cxa = Math.Floor(fx / FINE);
		int cza = Math.Floor(fz / FINE);
		float h00;
		float h10;
		float h01;
		float h11;
		bool tlbr;
		ResourceName mat;
		for (int dz = 0; dz >= -1; dz--)
		{
			for (int dx = 0; dx >= -1; dx--)
			{
				int cx = cxa + dx;
				int cz = cza + dz;
				float u = (fx - cx * FINE) / FINE;
				float w = (fz - cz * FINE) / FINE;
				if (u < 0 || u > 1 || w < 0 || w > 1)
					continue;
				if (!m_Skin.CellShape(cx, cz, h00, h10, h01, h11, tlbr, mat))
					continue;
				ok = true;
				return TriHeight(h00, h10, h01, h11, tlbr, u, w);
			}
		}
		ok = false;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Height inside one skin cell at fractions (u, w): the triangle the skin draws there. Skin
	//! triangles (MUD_CoarseSkin.c BuildGeometry): tl-br = (bl, tl, br) + (tl, tr, br); else
	//! (bl, tl, tr) + (tr, br, bl).
	protected float TriHeight(float h00, float h10, float h01, float h11, bool tlbr, float u, float w)
	{
		if (tlbr)
		{
			if (u + w <= 1)
				return h00 + u * (h10 - h00) + w * (h01 - h00);
			return h11 + (1 - u) * (h01 - h11) + (1 - w) * (h10 - h11);
		}
		if (w >= u)
			return h00 + w * (h01 - h00) + u * (h11 - h01);
		return h00 + u * (h10 - h00) + w * (h11 - h10);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsHardMat(ResourceName mat)
	{
		string path = mat;
		path.ToLower();
		foreach (string w : m_aHardWords)
		{
			if (path.Contains(w))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Hard (or kept terrain) at global skin cell (cx, cz): the patch's own state if it exists (a crater
	//! may have broken it, task 6), else the skin's game material
	protected bool CellHard(int cx, int cz)
	{
		if (cx < 0 || cz < 0 || cx >= m_iNPX * SKIN || cz >= m_iNPZ * SKIN)
			return true;
		MUD_FinePatch p = m_mPatches.Get((cz / SKIN) * m_iNPX + cx / SKIN);
		int c = (cz - (cz / SKIN) * SKIN) * SKIN + cx - (cx / SKIN) * SKIN;
		if (p)
			return p.m_aCell[c] < 0 || p.m_aCellHard[c] != 0;
		float h00;
		float h10;
		float h01;
		float h11;
		bool tlbr;
		ResourceName mat;
		if (!m_Skin.CellShape(cx, cz, h00, h10, h01, h11, tlbr, mat))
			return true;
		return IsHardMat(mat);
	}

	//------------------------------------------------------------------------------------------------
	//! Softness of the patch's 16 cells: hard or kept = 0, else the share of soft hole cells in its 3 x 3
	//! (kept terrain does not count, the edge lock fades there), so a rut runs out over ~2 m toward hard
	//! ground (as MUD_GroundEntity.c:1089 UpdateSoft, per 2 m skin cell instead of 1 m samples)
	protected void ComputeSoft(MUD_FinePatch p)
	{
		for (int lz = 0; lz < SKIN; lz++)
		{
			for (int lx = 0; lx < SKIN; lx++)
			{
				int c = lz * SKIN + lx;
				if (p.m_aCell[c] < 0 || p.m_aCellHard[c] != 0)
				{
					p.m_aCellSoft[c] = 0;
					continue;
				}
				int cx = p.m_iPx * SKIN + lx;
				int cz = p.m_iPz * SKIN + lz;
				float soft = 0;
				float all = 0;
				for (int nz = cz - 1; nz <= cz + 1; nz++)
				{
					for (int nx = cx - 1; nx <= cx + 1; nx++)
					{
						if (!m_Skin.IsHoleCell(nx, nz))
							continue;
						all++;
						if (!CellHard(nx, nz))
							soft++;
					}
				}
				p.m_aCellSoft[c] = soft / all;     // all >= 1: the cell itself
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Softness under global fine vertex (gx, gz): the LOWEST softness of the 1, 2 or 4 skin cells
	//! touching it, so a vertex on a hard cell's edge never moves. Taking only the cell at x / 8 let a
	//! rut press the shared edge of the hard cell to its -x / -z side, BreakHard then broke that cell and
	//! the rut crept over the asphalt cell by cell (DS run 2026-10-09 20:39, patches 192209 -> 191697 / 191696).
	//! Same answer in every patch sharing the vertex.
	protected float VertexSoft(int gx, int gz)
	{
		int cxHi = gx / FINE;
		int czHi = gz / FINE;
		int cxLo = cxHi;
		int czLo = czHi;
		if (cxHi * FINE == gx)
			cxLo = cxHi - 1;
		if (czHi * FINE == gz)
			czLo = czHi - 1;
		int ncx = m_iNPX * SKIN;
		int ncz = m_iNPZ * SKIN;
		float best = 1;
		for (int cz = czLo; cz <= czHi; cz++)
		{
			if (cz < 0 || cz >= ncz)
				continue;
			for (int cx = cxLo; cx <= cxHi; cx++)
			{
				if (cx < 0 || cx >= ncx)
					continue;
				MUD_FinePatch p = m_mPatches.Get((cz / SKIN) * m_iNPX + cx / SKIN);
				if (!p)
					return 0;
				float s = p.m_aCellSoft[(cz - (cz / SKIN) * SKIN) * SKIN + cx - (cx / SKIN) * SKIN];
				if (s < best)
					best = s;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Locked border (spec 4.3): 0 on a kept skin cell's edge, rising to 1 over m_fEdgeFade (smoothstep).
	//! Only kept cells count, so the same vertex gets the same lock in every patch.
	protected float EdgeLock(int gx, int gz)
	{
		if (m_eEdgeMode != MUD_EEdgeMode.LOCKED)
			return 1;
		float x = gx * m_fFine;
		float z = gz * m_fFine;
		int cx0 = Math.Floor((x - m_fEdgeFade) / m_fCell);
		int cx1 = Math.Floor((x + m_fEdgeFade) / m_fCell);
		int cz0 = Math.Floor((z - m_fEdgeFade) / m_fCell);
		int cz1 = Math.Floor((z + m_fEdgeFade) / m_fCell);
		float best = 1e9;
		for (int cz = cz0; cz <= cz1; cz++)
		{
			for (int cx = cx0; cx <= cx1; cx++)
			{
				if (m_Skin.IsHoleCell(cx, cz))
					continue;   // also false outside the map: the map edge locks too
				float dx = Math.Max(Math.Max(cx * m_fCell - x, 0), x - (cx + 1) * m_fCell);
				float dz = Math.Max(Math.Max(cz * m_fCell - z, 0), z - (cz + 1) * m_fCell);
				float d = Math.Sqrt(dx * dx + dz * dz);
				if (d < best)
					best = d;
			}
		}
		if (m_fEdgeFade <= 0)
		{
			if (best > 0)
				return 1;
			return 0;
		}
		float t = Math.Clamp(best / m_fEdgeFade, 0, 1);
		return t * t * (3 - 2 * t);
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddFound(IEntity e)
	{
		m_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Buildings (and MUD_CutBoxEntity boxes) on the patch: verts inside their box never move.
	//! Box test in the building's own frame (as MUD_GroundEntity.c:818-824). Returns locked verts.
	protected int BuildingLock(MUD_FinePatch p)
	{
		float wx0 = m_fX0 + p.m_iPx * PCELLS * m_fFine;
		float wz0 = m_fZ0 + p.m_iPz * PCELLS * m_fFine;
		float size = PCELLS * m_fFine;
		m_aFound.Clear();
		GetWorld().QueryEntitiesByAABB(Vector(wx0, -300, wz0), Vector(wx0 + size, 400, wz0 + size), AddFound);
		int locked;
		foreach (IEntity e : m_aFound)
		{
			vector bmin;
			vector bmax;
			MUD_CutBoxEntity box = MUD_CutBoxEntity.Cast(e);
			if (box)
			{
				box.GetBox(bmin, bmax);
			}
			else if (Building.Cast(e))
			{
				e.GetBounds(bmin, bmax);
				bmin[0] = bmin[0] - m_fBuildingMargin;
				bmin[2] = bmin[2] - m_fBuildingMargin;
				bmax[0] = bmax[0] + m_fBuildingMargin;
				bmax[2] = bmax[2] + m_fBuildingMargin;
			}
			else
			{
				continue;
			}
			for (int vz = 0; vz < PVERTS; vz++)
			{
				for (int vx = 0; vx < PVERTS; vx++)
				{
					int v = vz * PVERTS + vx;
					if (p.m_aLock[v] <= 0)
						continue;
					vector lp = e.CoordToLocal(Vector(wx0 + vx * m_fFine, p.m_aBase[v], wz0 + vz * m_fFine));
					if (lp[0] >= bmin[0] && lp[0] <= bmax[0] && lp[2] >= bmin[2] && lp[2] <= bmax[2])
					{
						p.m_aLock[v] = 0;
						locked++;
					}
				}
			}
		}
		return locked;
	}

	//------------------------------------------------------------------------------------------------
	//! Floor of every vertex: -Max Depth, or a depth box's Max Depth where one holds the vertex
	protected void ApplyFloor(MUD_FinePatch p)
	{
		float wx0 = m_fX0 + p.m_iPx * PCELLS * m_fFine;
		float wz0 = m_fZ0 + p.m_iPz * PCELLS * m_fFine;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				p.m_aFloor[v] = -MUD_DepthBoxEntity.DepthAt(Vector(wx0 + vx * m_fFine, p.m_aBase[v], wz0 + vz * m_fFine), m_fMaxDepth);
			}
		}
		// REBUILD 6 COARSE_PLATE: no depth box here = one floor value = the plate is the skin's 2 planes per cell
		p.m_bFlatFloor = true;
		for (int fv = 1; fv < PVERTS * PVERTS; fv++)
		{
			if (p.m_aFloor[fv] != p.m_aFloor[0])
			{
				p.m_bFlatFloor = false;
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Safety plate geom of p: base + floor - m_fPlateGap under every collider cell (m_aCellSlot >= 0: not kept, not
	//! VOX). REBUILD 6 COARSE_PLATE: a flat-floor patch gets 2 tris per skin cell on its 4 corners (same planes, same
	//! diagonal + winding as FillIndices) instead of the 8 x 8 x 2 fine tris. null = no plate cell.
	protected PhysicsGeom PlateGeom(MUD_FinePatch p, MUD_IdxSet ix)
	{
		if (!COARSE_PLATE || !p.m_bFlatFloor)
		{
			if (ix.m_Plate.m_iN <= 0)
				return null;
			m_iPlateFull++;
			m_iPlateTris += ix.m_Plate.m_iN / 3;
			return PhysicsGeom.CreateTriMesh(m_Buf.m_aPlate, ix.m_Plate.m_aIdx, 1089, ix.m_Plate.m_iN);
		}
		for (int pz = 0; pz <= SKIN; pz++)
		{
			for (int px = 0; px <= SKIN; px++)
			{
				int pv = (pz * FINE) * PVERTS + px * FINE;
				m_Buf.m_aPlateC[pz * (SKIN + 1) + px] = Vector(px * FINE * m_fFine, p.m_aBase[pv] + p.m_aFloor[pv] - m_fPlateGap, pz * FINE * m_fFine);
			}
		}
		int np = 0;
		for (int c = 0; c < SKIN * SKIN; c++)
		{
			int cell = p.m_aCell[c];
			if (cell < 0 || (cell & VOX) != 0)
				continue;     // kept terrain / voxel-carried: no plate (FillIndices m_aCellSlot -1)
			int cz = c / SKIN;
			int cx = c - cz * SKIN;
			int bl = cz * (SKIN + 1) + cx;
			int br = bl + 1;
			int tl = bl + SKIN + 1;
			int tr = tl + 1;
			if ((cell & TLBR) != 0)
			{
				m_Buf.m_aPlateCI[np] = bl;
				m_Buf.m_aPlateCI[np + 1] = tl;
				m_Buf.m_aPlateCI[np + 2] = br;
				m_Buf.m_aPlateCI[np + 3] = tl;
				m_Buf.m_aPlateCI[np + 4] = tr;
				m_Buf.m_aPlateCI[np + 5] = br;
			}
			else
			{
				m_Buf.m_aPlateCI[np] = bl;
				m_Buf.m_aPlateCI[np + 1] = tl;
				m_Buf.m_aPlateCI[np + 2] = tr;
				m_Buf.m_aPlateCI[np + 3] = tr;
				m_Buf.m_aPlateCI[np + 4] = br;
				m_Buf.m_aPlateCI[np + 5] = bl;
			}
			np += 6;
		}
		if (np <= 0)
			return null;
		m_iPlateCoarse++;
		m_iPlateTris += np / 3;
		return PhysicsGeom.CreateTriMesh(m_Buf.m_aPlateC, m_Buf.m_aPlateCI, 25, np);
	}

	//------------------------------------------------------------------------------------------------
	//! Base, lock, floor, cells, softness. Same inputs on every machine, so every machine builds the same.
	protected void InitPatch(MUD_FinePatch p)
	{
		int cx0 = p.m_iPx * SKIN;
		int cz0 = p.m_iPz * SKIN;
		p.m_bFloor = MUD_VoxelProbe.FieldInBox(p.m_iPx * PCELLS, p.m_iPx * PCELLS + PCELLS, p.m_iPz * PCELLS, p.m_iPz * PCELLS + PCELLS);
		float h00;
		float h10;
		float h01;
		float h11;
		bool tlbr;
		ResourceName mat;
		for (int lz = 0; lz < SKIN; lz++)
		{
			for (int lx = 0; lx < SKIN; lx++)
			{
				int c = lz * SKIN + lx;
				if (!m_Skin.CellShape(cx0 + lx, cz0 + lz, h00, h10, h01, h11, tlbr, mat))
				{
					p.m_aCell[c] = -1;
					p.m_aCellHard[c] = 1;
					continue;
				}
				int slot = p.m_aMats.Find(mat);
				if (slot < 0)
				{
					if (p.m_aMats.Count() < MAT_SLOTS)
						slot = p.m_aMats.Insert(mat);
					else
						slot = 0;   // more than 4 surfaces in one patch: the rest join slot 0
				}
				int cell = slot;
				if (tlbr)
					cell = cell | TLBR;
				if (m_Skin.IsVoxCell(cx0 + lx, cz0 + lz))
					cell = cell | VOX;
				if (MUD_VoxelProbe.IsLookCell(cx0 + lx, cz0 + lz))
					cell = cell | LOOK;
				p.m_aCell[c] = cell;
				p.m_aCellHard[c] = 0;
				if (IsHardMat(mat))
					p.m_aCellHard[c] = 1;
			}
		}

		int gx0 = p.m_iPx * PCELLS;
		int gz0 = p.m_iPz * PCELLS;
		FillCellCache(p);
		MUD_CellCache cc = m_Cells;
		// no kept cell near the patch (most of the map): EdgeLock is 1 at every vertex
		bool lockAll = m_eEdgeMode != MUD_EEdgeMode.LOCKED || !cc.m_bAnyKept;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				int gx = gx0 + vx;
				int gz = gz0 + vz;
				bool ok = false;
				float bh = 0;
				if (cc.m_iNX == 0)
				{
					bh = BaseAt(gx, gz, ok);
				}
				else
				{
					// BaseAt on the cache: same cells in the same order
					float fx = gx;
					float fz = gz;
					int cxa = Math.Floor(fx / FINE);
					int cza = Math.Floor(fz / FINE);
					for (int dz = 0; dz >= -1 && !ok; dz--)
					{
						for (int dx = 0; dx >= -1 && !ok; dx--)
						{
							int ccx = cxa + dx;
							int ccz = cza + dz;
							float u = (fx - ccx * FINE) / FINE;
							float w = (fz - ccz * FINE) / FINE;
							if (u < 0 || u > 1 || w < 0 || w > 1)
								continue;
							int ci = (ccz - cc.m_iZ0) * cc.m_iNX + ccx - cc.m_iX0;
							if (cc.m_aHole[ci] == 0)
								continue;
							ok = true;
							bh = TriHeight(cc.m_aH00[ci], cc.m_aH10[ci], cc.m_aH01[ci], cc.m_aH11[ci], cc.m_aTlbr[ci] != 0, u, w);
						}
					}
				}
				p.m_aBase[v] = bh;
				p.m_aH[v] = 0;
				p.m_aSmooth[v] = 0;
				p.m_aLock[v] = 0;
				if (!ok)
					continue;
				if (cc.m_iNX == 0)
					p.m_aLock[v] = EdgeLock(gx, gz);
				else if (lockAll)
					p.m_aLock[v] = 1;
				else
					p.m_aLock[v] = EdgeLockCached(gx, gz);
			}
		}
		// Stage 5 (F4): the rut floor uses the lock before buildings - BuildingLock depends on what this machine has
		// loaded / destroyed when the patch is made, the kept terrain border does not
		for (int ev = 0; ev < PVERTS * PVERTS; ev++)
		{
			p.m_aEdgeLock[ev] = p.m_aLock[ev];
		}
		BuildingLock(p);
		ApplyFloor(p);
	}

	//------------------------------------------------------------------------------------------------
	//! A new patch takes the heights of edge verts it shares with patches that already exist, so a
	//! patch made next to a deformed one starts without a step (Review Focus 1). Returns verts copied
	//! with a non-zero height.
	protected int CopyNeighbourEdges(MUD_FinePatch p)
	{
		int copied;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				if (vx != 0 && vx != PCELLS && vz != 0 && vz != PCELLS)
					continue;
				int gx = p.m_iPx * PCELLS + vx;
				int gz = p.m_iPz * PCELLS + vz;
				// p is not in the map yet: At() finds only the neighbours
				int ov;
				MUD_FinePatch o = At(gx, gz, ov);
				if (!o)
					continue;
				int v = vz * PVERTS + vx;
				p.m_aH[v] = o.m_aH[ov];
				p.m_aSmooth[v] = o.m_aSmooth[ov];
				if (Math.AbsFloat(o.m_aH[ov]) > 0.0005)
					copied++;
			}
		}
		return copied;
	}

	//------------------------------------------------------------------------------------------------
	//! Patch (px, pz) has at least one hole cell (patches without are cached in m_sNoHole)
	protected bool HasHole(int px, int pz)
	{
		if (px < 0 || pz < 0 || px >= m_iNPX || pz >= m_iNPZ)
			return false;
		int key = pz * m_iNPX + px;
		if (m_sNoHole.Contains(key))
			return false;
		for (int lz = 0; lz < SKIN; lz++)
		{
			for (int lx = 0; lx < SKIN; lx++)
			{
				if (m_Skin.IsHoleCell(px * SKIN + lx, pz * SKIN + lz))
					return true;
			}
		}
		m_sNoHole.Insert(key);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The patch at (px, pz) for a stamp or wheel: made and built now, the 4 x 4 skin cells covered in this
	//! same call; a patch still building ahead is finished now. null when the patch has no hole cell or is
	//! off the grid. wheel: at most WHEEL_BUILDS_PER_FRAME such builds per frame; past that a missing patch
	//! stays missing (null; the skin carries the wheel, Ahead makes the patch under the vehicle first) and
	//! one still building is returned as it is (its last steps use the heights written meanwhile).
	protected MUD_FinePatch EnsurePatch(int px, int pz, bool wheel = false)
	{
		if (px < 0 || pz < 0 || px >= m_iNPX || pz >= m_iNPZ)
			return null;
		MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
		bool capped = wheel && m_iWheelBuilds >= WHEEL_BUILDS_PER_FRAME;
		if (p)
		{
			// a sleeping patch wakes before anything presses on it
			if ((p.m_iBuild > 0 || p.m_bAsleep) && !capped)
			{
				if (p.m_bAsleep)
				{
					WakePatch(p);
					m_iWokeNow++;
				}
				else
				{
					m_iRushed++;
				}
				if (wheel)
					m_iWheelBuilds++;
				FinishNow(p);
			}
			return p;
		}
		if (capped || !HasHole(px, pz))
			return null;
		if (wheel)
			m_iWheelBuilds++;
		p = StartPatch(px, pz);
		if (!p)
			return null;
		m_iMadeStamp++;
		FinishNow(p);
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! Every build step left on p, in this call. Stops when the collider made no body: the skin stays
	//! over the patch and the next stamp on it tries again.
	protected void FinishNow(MUD_FinePatch p)
	{
		while (p.m_iBuild > 0)
		{
			if (!BuildStep(p))
				return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lazy patch (O1): made, index lists set, never dug. Heights equal the skin, so the skin keeps
	//! carrying it (no collider, no mesh, no cover) until the first stamp on it (EnsurePatch -> FinishNow).
	//! Ahead stops here. A deformed patch (edges copied from a dug neighbour) or a waking one goes on.
	protected bool IsLazyReady(MUD_FinePatch p)
	{
		return m_bLazy && p.m_iBuild == BUILD_COLLIDER && !p.m_bAsleep && !p.m_bDeformed && !p.m_bFloor;
	}

	//------------------------------------------------------------------------------------------------
	//! probe 2026-10-13 (MUD_VoxelProbe overlay): every cell of p is kept terrain or voxel-carried (no mesh, no
	//! collider of its own: no body is right)
	protected bool NoShownCell(MUD_FinePatch p)
	{
		for (int c = 0; c < SKIN * SKIN; c++)
		{
			if (p.m_aCell[c] >= 0 && (p.m_aCell[c] & VOX) == 0)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! probe 2026-10-13 (MUD_VoxelProbe overlay, any machine): the skin cells cx0..cx1, cz0..cz1 changed CELL_VOX
	//! (voxel bricks went live): each made patch there takes the new VOX bits. A live patch gets its collider now (in
	//! the frame the bricks show: else its old collider would hold the ground over a voxel crater; all cells voxel =
	//! just no body), its mesh in the rebuild queue; a building / sleeping one uses the bits in its next steps.
	//! Returns patches changed.
	static int VoxCellsChanged(int cx0, int cx1, int cz0, int cz1)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return 0;
		int changed = s_Inst.DoVoxCells(cx0, cx1, cz0, cz1);
		if (MUD_LookLift.ENABLED && s_Inst.m_bRender)
		{
			// LOOK LIFT (seam fix 2026-10-23): a voxel cell drops the lift of its corners, which neighbour
			// patches also draw (one skin cell of fine vertices each way): their meshes re-draw too
			s_Inst.MarkLookDirty((cx0 - 1) * FINE, (cx1 + 2) * FINE, (cz0 - 1) * FINE, (cz1 + 2) * FINE, -1, 0);
		}
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP probe 2026-10-14 (MUD_VoxelProbe RUT_LOOK, rendering machines): the skin cells cx0..cx1, cz0..cz1 may have
	//! become look cells (a look rut's voxel mesh draws there): each made patch there takes the LOOK bits. Only the
	//! patch MESH changes (now when live, else queued) - its collider keeps the undug ground under the wheels.
	//! Returns patches changed.
	static int LookCellsChanged(int cx0, int cx1, int cz0, int cz1)
	{
		if (!s_Inst || !s_Inst.m_bReady || !s_Inst.m_bRender)
			return 0;
		return s_Inst.DoLookCells(cx0, cx1, cz0, cz1);
	}

	//------------------------------------------------------------------------------------------------
	protected int DoLookCells(int cx0, int cx1, int cz0, int cz1)
	{
		int changed;
		int meshes;
		int t0 = System.GetTickCount();
		int px0 = Math.Max(0, Math.Floor(cx0 / SKIN));
		int px1 = Math.Min(m_iNPX - 1, Math.Floor(cx1 / SKIN));
		int pz0 = Math.Max(0, Math.Floor(cz0 / SKIN));
		int pz1 = Math.Min(m_iNPZ - 1, Math.Floor(cz1 / SKIN));
		for (int pz = pz0; pz <= pz1; pz++)
		{
			for (int px = px0; px <= px1; px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				bool diff = false;    // explicit: a loop-body local is not reset each pass
				for (int c = 0; c < SKIN * SKIN; c++)
				{
					if (p.m_aCell[c] < 0)
						continue;
					int lx = c - (c / SKIN) * SKIN;
					int lz = c / SKIN;
					int want = p.m_aCell[c] & ~LOOK;
					if (MUD_VoxelProbe.IsLookCell(px * SKIN + lx, pz * SKIN + lz))
						want = want | LOOK;
					if (want != p.m_aCell[c])
					{
						p.m_aCell[c] = want;
						diff = true;
					}
				}
				if (!diff)
					continue;
				changed++;
				if (p.m_iBuild == 0 && p.m_Ent)
				{
					// in the frame the look bricks show (as DoVoxCells), asleep ones too
					BuildMesh(p);
					p.m_fMeshAt = m_fTime;
					meshes++;
				}
				else
				{
					p.m_bMeshDirty = true;
					p.m_bMBoxAll = true;     // REBUILD 5 option 3: whole-patch change
					p.m_bRutMesh = false;
					if (!p.m_bQueued)
					{
						p.m_bQueued = true;
						p.m_fQueuedAt = m_fTime;
						m_aDirty.Insert(p.m_iKey);
					}
				}
			}
		}
		if (changed > 0)
			Print(string.Format("MUD world: look cells live - %1 patches hid their mesh there (%2 meshes now, colliders kept) in %3 ms", changed, meshes, System.GetTickCount(t0)), LogLevel.NORMAL);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	protected int DoVoxCells(int cx0, int cx1, int cz0, int cz1)
	{
		int changed;
		int now;
		int dropped;
		int meshes;
		int colMs;     // stage 2 B7 meter: collider ms, mesh ms, meshes put off
		int meshMs;
		int later;
		bool vLater = VOX_MESH_LATER && MUD_MudSheet.LocalOn();
		int t0 = System.GetTickCount();
		int px0 = Math.Max(0, Math.Floor(cx0 / SKIN));
		int px1 = Math.Min(m_iNPX - 1, Math.Floor(cx1 / SKIN));
		int pz0 = Math.Max(0, Math.Floor(cz0 / SKIN));
		int pz1 = Math.Min(m_iNPZ - 1, Math.Floor(cz1 / SKIN));
		for (int pz = pz0; pz <= pz1; pz++)
		{
			for (int px = px0; px <= px1; px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				bool diff = false;    // explicit: a loop-body local is not reset each pass
				for (int c = 0; c < SKIN * SKIN; c++)
				{
					if (p.m_aCell[c] < 0)
						continue;
					int lx = c - (c / SKIN) * SKIN;
					int lz = c / SKIN;
					int want = p.m_aCell[c] & ~VOX;
					if (m_Skin.IsVoxCell(px * SKIN + lx, pz * SKIN + lz))
						want = want | VOX;
					if (want != p.m_aCell[c])
					{
						p.m_aCell[c] = want;
						diff = true;
					}
				}
				if (!diff)
					continue;
				changed++;
				if (p.m_iBuild == 0 && !p.m_bAsleep && NoShownCell(p))
				{
					// every cell voxel-carried or kept: no body is right, no index lists or trimesh needed (bricks
					// 2026-10-13: the go-live frame must stay short)
					DropBody(p);
					p.m_bColValid = false;
					p.m_bColDirty = false;
					dropped++;
				}
				else if (p.m_iBuild == 0 && !p.m_bAsleep)
				{
					int tc = System.GetTickCount();
					BuildCollider(p);
					colMs += System.GetTickCount(tc);
					p.m_fColAt = m_fTime;
					now++;
				}
				else if (p.m_iBuild == BUILD_COVER)
					p.m_iBuild = BUILD_COLLIDER;    // its collider has the old cells: build it again before the cover
				if (vLater && m_bRender && p.m_iBuild == 0 && p.m_Ent)
				{
					// stage 2 B7 VOX_MESH_LATER: the sheet draws over it - its mesh comes one per frame after this one
					if (m_aVoxLater.Find(p.m_iKey) < 0)
						m_aVoxLater.Insert(p.m_iKey);
					later++;
				}
				else if (m_bRender && p.m_iBuild == 0 && p.m_Ent)
				{
					// mesh now, in the frame the bricks show, asleep ones too: the queue waits (one per frame, Mesh
					// Interval) and skips sleeping patches, so the old surface (ruts) lay over the crater
					// (user 2026-10-13 20:24)
					int tm = System.GetTickCount();
					BuildMesh(p);
					meshMs += System.GetTickCount(tm);
					p.m_fMeshAt = m_fTime;
					meshes++;
				}
				else if (m_bRender)
				{
					p.m_bMeshDirty = true;
					p.m_bMBoxAll = true;     // REBUILD 5 option 3: whole-patch change
					p.m_bRutMesh = false;
					p.m_bHMesh = true;        // REBUILD 6
					if (LOOK_PACE)
						p.m_fMeshHoldTo = -100;     // C1 HOLD_RESET
				}
				if (!p.m_bQueued)
				{
					p.m_bQueued = true;
					p.m_fQueuedAt = m_fTime;
					m_aDirty.Insert(p.m_iKey);
				}
			}
		}
		if (changed > 0)
			Print(string.Format("MUD world: voxel cells live - %1 patches took the voxel cells (%2 colliders rebuilt now, %3 dropped: all voxel, %4 meshes now) in %5 ms", changed, now, dropped, meshes, System.GetTickCount(t0))
				+ string.Format(" | colliders %1 ms, meshes %2 ms, meshes later %3 (B7 %4)", colMs, meshMs, later, vLater), LogLevel.NORMAL);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage 2 B7 VOX_MESH_LATER (EOnFrame, before the voxel tick): build the next go-live patch mesh DoVoxCells put off.
	//! A patch gone / building / without entity goes to the normal mesh queue instead. Prints once the list is empty.
	protected void VoxLaterStep()
	{
		int key = m_aVoxLater[0];
		m_aVoxLater.RemoveOrdered(0);
		m_iVoxLaterF++;
		MUD_FinePatch p = m_mPatches.Get(key);
		if (p && m_bRender && p.m_iBuild == 0 && p.m_Ent)
		{
			int t0 = System.GetTickCount();
			BuildMesh(p);
			p.m_fMeshAt = m_fTime;
			int ms = System.GetTickCount(t0);
			m_iVoxLaterN++;
			m_iVoxLaterMs += ms;
			if (ms > m_iVoxLaterWorst)
				m_iVoxLaterWorst = ms;
		}
		else if (p && m_bRender)
		{
			p.m_bMeshDirty = true;     // as DoVoxCells' queued branch
			p.m_bMBoxAll = true;
			p.m_bRutMesh = false;
			p.m_bHMesh = true;
			if (LOOK_PACE)
				p.m_fMeshHoldTo = -100;
			if (!p.m_bQueued)
			{
				p.m_bQueued = true;
				p.m_fQueuedAt = m_fTime;
				m_aDirty.Insert(p.m_iKey);
			}
			m_iVoxLaterQ++;
		}
		if (!m_aVoxLater.IsEmpty())
			return;
		Print(string.Format("MUD world: voxel cells meshes later - %1 built over %2 frames, %3 ms (worst %4 ms), %5 to the mesh queue",
			m_iVoxLaterN, m_iVoxLaterF, m_iVoxLaterMs, m_iVoxLaterWorst, m_iVoxLaterQ), LogLevel.NORMAL);
		m_iVoxLaterN = 0;
		m_iVoxLaterF = 0;
		m_iVoxLaterMs = 0;
		m_iVoxLaterWorst = 0;
		m_iVoxLaterQ = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! probe 2026-10-13 (MUD_VoxelProbe overlay, any machine): undug ground height at world (x, z) - the skin
	//! triangles, the heightmap on kept terrain. The same on every machine (no ruts). False = unknown.
	static bool UndugY(float x, float z, out float y)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		return s_Inst.UndugOnly(x, z, y);     // perf plan B1: was Heights (it also built the dug height, then dropped it)
	}

	//------------------------------------------------------------------------------------------------
	//! perf plan B1 (2026-10-18, audit-ops.md item 1): Heights' undug value alone - the same steps (ToFine, BaseAt,
	//! the surface fallback off the skin), so the same float; no dug blend, rut floor or ridge reads
	protected bool UndugOnly(float x, float z, out float undug)
	{
		float fx;
		float fz;
		if (!ToFine(Vector(x, 0, z), fx, fz))
			return false;
		bool ok;
		undug = BaseAt(fx, fz, ok);
		if (!ok)
		{
			undug = GetWorld().GetSurfaceY(x, z);
			return undug > -255;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! probe 2026-10-13 (MUD_VoxelProbe bricks, server): ground at world (x, z) now (ruts, berms, heightfield
	//! craters) and undug. The server seeds new voxel columns with dug - undug. False = unknown.
	static bool DugUndug(float x, float z, out float dug, out float undug)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		return s_Inst.Heights(x, z, dug, undug);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD (Stage 5, F4): rut floor f (world y) at global fine vertex gx, gz with the kept-terrain edge lock
	//! applied, as the patch mesh / collider do - for the bricks (Displace). f unchanged off the grid or not below
	//! the undug ground.
	static float FieldEdgeLocked(int gx, int gz, float f)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return f;
		float lock = s_Inst.LockAt(gx, gz);
		if (lock >= 1)
			return f;
		bool ok;
		float undug = s_Inst.BaseAt(gx, gz, ok);
		if (!ok || f >= undug)
			return f;
		return undug + (f - undug) * lock;
	}

	//------------------------------------------------------------------------------------------------
	//! One build step of a new patch (BUILD_*). The skin cells are covered only in the last step, after
	//! the collider exists, and in the same frame as the mesh: never a hole, never two surfaces showing.
	//! False: the collider made no body.
	protected bool BuildStep(MUD_FinePatch p)
	{
		if (p.m_iBuild == BUILD_INDEX)
		{
			int miss0 = m_iIdxMisses;
			IdxSet(p);
			p.m_iBuild = BUILD_COLLIDER;
			if (m_iIdxMisses != miss0)
			{
				m_sFrameStep += "index ";
				return true;
			}
			if (IsLazyReady(p))
				return true;    // lazy: the first stamp builds the rest (FinishNow calls again)
			// cached layout: costs nothing, the collider goes in this step
		}
		if (p.m_iBuild == BUILD_COLLIDER)
		{
			int kbc = System.MemoryAllocationKB();
			SmoothPatch(p);     // a dig next door may have reached it since setup
			BuildCollider(p);
			if (p.m_bAsleep)
				m_iWakeKB += System.MemoryAllocationKB() - kbc;
			if (!p.m_Phys && !NoShownCell(p))
			{
				p.m_iBuildFails++;
				Print(string.Format("MUD world: patch %1 %2 collider made no body (try %3), skin stays", p.m_iPx, p.m_iPz, p.m_iBuildFails), LogLevel.ERROR);
				return false;
			}
			p.m_iBuild = BUILD_COVER;
			m_sFrameStep += "collider ";
			return true;
		}
		if (p.m_bAsleep)
		{
			// waking: the mesh never went; the skin collider goes now that the patch collider exists
			if (m_bRender && p.m_bMeshDirty)
				BuildMesh(p);
			int kbw = System.MemoryAllocationKB();
			int tw = System.GetTickCount();
			m_Skin.SetSleep(p.m_iPx * SKIN, p.m_iPz * SKIN, SKIN, false);
			m_iSwapMs += System.GetTickCount(tw);
			m_iWakeKB += System.MemoryAllocationKB() - kbw;
			p.m_bAsleep = false;
			p.m_iBuild = 0;
			p.m_fNearAt = m_fTime;     // woken from afar (wake lead): not straight back to sleep
			m_iAsleep--;
			m_iWoken++;
			m_sFrameStep += "wake ";
			return true;
		}
		if (m_bRender)
			BuildMesh(p);
		else if (!p.m_MeshRes)
			BoundsMesh(p);
		p.m_iBuild = 0;
		if (p.m_HoldRes)
		{
			// CRATER HOLD: the mesh waits for the rest of the crater - the skin keeps drawing here until HoldRelease
			p.m_bHoldCover = true;
			HoldNote(p);
			m_sFrameStep += "cover-held ";
			return true;
		}
		int t0 = System.GetTickCount();
		m_Skin.SetCovered(p.m_iPx * SKIN, p.m_iPz * SKIN, SKIN, true);
		m_iSwapMs += System.GetTickCount(t0);
		m_sFrameStep += "cover ";
		if (m_bDebug)
			CheckNew(p);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! First build step of a new patch: base, lock, edges, softness and its entity. No mesh, no collider:
	//! the skin still carries the ground (m_iBuild = BUILD_INDEX). In m_mPatches from here on, so digs
	//! and seams see it. null when the patch has no hole cell, is off the grid or did not spawn.
	protected MUD_FinePatch StartPatch(int px, int pz)
	{
		if (!HasHole(px, pz))
			return null;
		int key = pz * m_iNPX + px;
		MUD_FinePatch p = new MUD_FinePatch();
		p.m_iPx = px;
		p.m_iPz = pz;
		p.m_iKey = key;
		p.m_fNearAt = m_fTime;
		int tm = System.GetTickCount();
		InitPatch(p);
		RutFieldOn(p);      // m_bFloor before the lazy stop: a patch over an old rut builds in full and draws it
		if (m_bRender && m_sFarKeys.Contains(key) && MUD_MudSheet.FarPressOn())
			p.m_bFloor = true;     // FAR PRESS: far press marked it (a worn-only track) - build in full, not lazy
		int copied = CopyNeighbourEdges(p);
		m_mPatches.Insert(key, p);
		if (copied > 0)
		{
			p.m_bDeformed = true;
			m_iDeformed++;
			p.m_iSx0 = 0;
			p.m_iSx1 = PCELLS;
			p.m_iSz0 = 0;
			p.m_iSz1 = PCELLS;
			SmoothPatch(p);
		}
		ComputeSoft(p);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = Vector(m_fX0 + px * PCELLS * m_fFine, 0, m_fZ0 + pz * PCELLS * m_fFine);
		p.m_Ent = GetGame().SpawnEntity(MUD_FinePatchEntity, GetWorld(), params);
		if (p.m_Ent && RENDER_GATE && m_bRender)
			p.m_Ent.SetFlags(EntityFlags.SET_RENDERED, false);     // RENDER_GATE: IsRendered works
		if (!p.m_Ent)
		{
			Print("MUD world: could not spawn patch entity", LogLevel.ERROR);
			m_mPatches.Remove(key);
			m_iMakeMs += System.GetTickCount(tm);
			return null;
		}
		if (s_bHidden)
		{
			p.m_Ent.ClearFlags(EntityFlags.TRACEABLE, false);
			p.m_bTraceHidden = true;
		}
		p.m_iBuild = BUILD_INDEX;
		m_iMakeMs += System.GetTickCount(tm);
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! Every patch whose vertex range meets [gx0..gx1] x [gz0..gz1] (edge verts included), made now
	//! (wheel: capped per frame, see EnsurePatch). False when a patch with hole cells is still missing or
	//! building: then a dig must wait, or it would lower a live patch's edge next to undug skin (a crack).
	//! CRATER BUILD ONCE: every hole patch in the range exists (StartPatch: heights, no mesh / collider); building or
	//! asleep ones stay so - the rush builds them once with the crater in. False = a patch did not start or its build
	//! failed again (the crater skips, as EnsureRange: a live neighbour's edge would sink next to the skin).
	protected bool StartRange(int gx0, int gx1, int gz0, int gz1)
	{
		bool all = true;
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
			{
				MUD_FinePatch p = StartOnly(px, pz);
				if (!p)
				{
					if (HasHole(px, pz))
						all = false;
					continue;
				}
				if (p.m_iBuildFails > 0)
				{
					FinishNow(p);     // a failed build tries again now, as EnsureRange did
					if (p.m_iBuild > 0)
						all = false;
				}
			}
		}
		return all;
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER BUILD ONCE: patch (px, pz) as it is (any build state), or started now. null = off the grid, no hole cell
	//! or did not spawn.
	protected MUD_FinePatch StartOnly(int px, int pz)
	{
		if (px < 0 || pz < 0 || px >= m_iNPX || pz >= m_iNPZ)
			return null;
		MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
		if (p)
			return p;
		if (!HasHole(px, pz))
			return null;
		p = StartPatch(px, pz);
		if (p)
			m_iMadeStamp++;
		return p;
	}

	//------------------------------------------------------------------------------------------------
	protected bool EnsureRange(int gx0, int gx1, int gz0, int gz1, bool wheel = false)
	{
		bool all = true;
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
			{
				MUD_FinePatch p = EnsurePatch(px, pz, wheel);
				if (p)
				{
					if (p.m_iBuild > 0 || p.m_bAsleep)
						all = false;
				}
				else if (HasHole(px, pz))
				{
					all = false;
				}
			}
		}
		return all;
	}

	//------------------------------------------------------------------------------------------------
	//! Uncover the skin cells (a patch still building never covered them; an asleep one, waking or not, did - SetCovered
	//! false clears its sleep flag too), then delete the patch
	protected void FreePatch(MUD_FinePatch p)
	{
		if (m_bRender)
		{
			FarNbGone(p);     // FAR PRESS: pressed neighbours close their border toward it
			LookNbGone(p);    // LOOK LIFT gap (b): the same for the look lift press-out
		}
		if (p.m_bAsleep)
			m_iAsleep--;
		if (p.m_iBuild == 0 || p.m_bAsleep)
		{
			int t0 = System.GetTickCount();
			m_Skin.SetCovered(p.m_iPx * SKIN, p.m_iPz * SKIN, SKIN, false);
			m_iSwapMs += System.GetTickCount(t0);
		}
		if (p.m_iKey == m_iBuildKey)
			m_iBuildKey = -1;
		DropBody(p);
		if (p.m_Ent)
			delete p.m_Ent;
		m_mPatches.Remove(p.m_iKey);
		m_iFreed++;
	}

	//------------------------------------------------------------------------------------------------
	//! Far deformed patch: the skin collider comes back under it first, then its own collider goes. Mesh,
	//! entity and heights stay (the ruts still show). Same frame, so the ground under it never goes.
	protected void SleepPatch(MUD_FinePatch p)
	{
		int kb0 = System.MemoryAllocationKB();
		int t0 = System.GetTickCount();
		m_Skin.SetSleep(p.m_iPx * SKIN, p.m_iPz * SKIN, SKIN, true);
		m_iSwapMs += System.GetTickCount(t0);
		DropBody(p);
		p.m_bColValid = false;
		p.m_bAsleep = true;
		p.m_Idx = null;     // probe 2026-10-13: the ring cache (or a rebuild) serves it when it wakes
		m_iAsleep++;
		m_iSlept++;
		m_iSleepKB += kb0 - System.MemoryAllocationKB();
	}

	//------------------------------------------------------------------------------------------------
	//! Start waking a sleeping patch: index lists (cache miss only) -> collider -> skin collider off
	//! (BuildStep). It stays m_bAsleep until that last step.
	protected void WakePatch(MUD_FinePatch p)
	{
		if (p.m_iBuild == 0)
			p.m_iBuild = BUILD_INDEX;
		p.m_iBuildFails = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A budgeted job may run: the frame's ground work is under m_iFrameBudget, or the job already
	//! waited MAX_WAIT frames in a row and its worst cost still fits under m_iFrameCap. Without the cap a
	//! starved free ran after ahead 4 + rebuild 5 ms: 12 ms frame (2026-10-09 14:47:59 Exp 192624).
	protected bool MayRun(int t0, int waited, int cost)
	{
		int spent = System.GetTickCount(t0);
		if (spent < m_iFrameBudget)
			return true;
		return waited >= MAX_WAIT && spent + cost <= m_iFrameCap;
	}

	//------------------------------------------------------------------------------------------------
	//! Patches ahead of moving wheeled vehicles, nearest first, built one step per frame
	//! (m_iAheadPerFrame steps): one patch at a time, the next starts when it is live. Every vehicle,
	//! moving or parked, keeps the patches around it from being freed.
	//! FLOOR SPREAD: one build step on the oldest building patch a rut floor reached (as Ahead's first step: always
	//! one, whatever the budget, so the rut floor turns solid within a few frames)
	protected void FloorSpreadTick()
	{
		while (!m_aFloorSpread.IsEmpty())
		{
			MUD_FinePatch p = m_mPatches.Get(m_aFloorSpread[0]);
			if (!p || p.m_iBuild == 0 || p.m_bAsleep || p.m_iBuildFails >= 3)
			{
				m_aFloorSpread.RemoveOrdered(0);     // built (Ahead or a stamp finished it), freed, asleep or failing
				continue;
			}
			BuildStep(p);
			m_iFloorSpreadSteps++;
			if (p.m_iBuild == 0)
				m_aFloorSpread.RemoveOrdered(0);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Ahead(int t0)
	{
		int tScan = System.GetTickCount();     // TEMP prof split (MUD_Prof.A_SCAN)
		array<int> candKey = m_aCandKey;       // audit B8: reused, cleared each frame (nothing keeps them)
		if (m_mParkScanAt.Count() > 2 * MUD_Wheels.GetAll().Count() + 8)
			m_mParkScanAt.Clear();     // deleted vehicles' keys
		array<float> candDist = m_aCandDist;
		candKey.Clear();
		candDist.Clear();
		float size = PCELLS * m_fFine;
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh)
				continue;
			Physics ph = veh.GetPhysics();
			if (!ph)
				continue;
			// audit B2: a parked vehicle (no pilot, body asleep) finds the same patches every frame - scan it every
			// PARK_SCAN_S (keeps near patches awake, wakes a sleeping one under it within that time)
			if (!veh.GetPilot() && !ph.IsActive())
			{
				float lastScan;
				if (m_mParkScanAt.Find(veh, lastScan) && m_fTime - lastScan < PARK_SCAN_S)
				{
					m_iParkSkips++;
					continue;
				}
				m_mParkScanAt.Set(veh, m_fTime);
			}
			float speed = ph.GetVelocity().Length();
			bool moving = speed > m_fAheadSpeed;
			// sleeping patches wake further out at speed (deep ones skip the wheel's height check, so
			// only this wakes them before the wheels arrive)
			float wakeR = Math.Max(m_fAheadRadius, speed * m_fWakeLead);
			vector o = veh.GetOrigin();
			int pxa = Math.Floor((o[0] - m_fX0 - wakeR) / size);
			int pxb = Math.Floor((o[0] - m_fX0 + wakeR) / size);
			int pza = Math.Floor((o[2] - m_fZ0 - wakeR) / size);
			int pzb = Math.Floor((o[2] - m_fZ0 + wakeR) / size);
			for (int pz = Math.Max(pza, 0); pz <= Math.Min(pzb, m_iNPZ - 1); pz++)
			{
				for (int px = Math.Max(pxa, 0); px <= Math.Min(pxb, m_iNPX - 1); px++)
				{
					// distance from the vehicle to the patch square
					float x0 = m_fX0 + px * size;
					float z0 = m_fZ0 + pz * size;
					float dx = Math.Max(Math.Max(x0 - o[0], 0), o[0] - x0 - size);
					float dz = Math.Max(Math.Max(z0 - o[2], 0), o[2] - z0 - size);
					float d = Math.Sqrt(dx * dx + dz * dz);
					if (d > wakeR)
						continue;
					int key = pz * m_iNPX + px;
					MUD_FinePatch p = m_mPatches.Get(key);
					if (p)
					{
						if (d > m_fAheadRadius)
						{
							// only the wake reaches this far
							if (p.m_bAsleep && key != m_iBuildKey && p.m_iBuildFails < 3 && !candKey.Contains(key))
							{
								candKey.Insert(key);
								candDist.Insert(d);
							}
							continue;
						}
						p.m_fNearAt = m_fTime;
						// a sleeping patch wakes for any vehicle near, moving or parked
						if (p.m_bAsleep && key != m_iBuildKey && p.m_iBuildFails < 3 && !candKey.Contains(key))
						{
							candKey.Insert(key);
							candDist.Insert(d);
						}
						continue;
					}
					if (moving && d <= m_fAheadRadius && !m_sNoHole.Contains(key) && !candKey.Contains(key))
					{
						candKey.Insert(key);
						candDist.Insert(d);
					}
				}
			}
		}

		NearPlayers(candKey, candDist);
		FarDue(candKey, candDist);
		FieldDue(candKey, candDist);
		if (m_bRender)
			FarPressTick();     // FAR PRESS: one paced look rebuild a frame
		m_iProfAheadScan = System.GetTickCount(tScan);

		if (m_iBuildKey < 0 && candKey.IsEmpty())
			return;
		if (!MayRun(t0, m_iAheadSkip, COST_STEP))
		{
			m_iAheadSkip++;
			m_iWaitAhead++;
			return;
		}
		m_iAheadSkip = 0;
		for (int n = 0; n < m_iAheadPerFrame; n++)
		{
			if (n > 0 && System.GetTickCount(t0) >= m_iFrameBudget)
				break;
			MUD_FinePatch b = null;
			if (m_iBuildKey >= 0)
				b = m_mPatches.Get(m_iBuildKey);
			if (b && b.m_iBuild > 0 && !IsLazyReady(b))
			{
				bool stepOk = BuildStep(b);
				// live now, lazy READY, or a collider that failed 3 times (ERROR each try): the skin stays
				// there and FreeIdle takes the patch later
				if (IsLazyReady(b))
					m_iReadyNow++;
				if (b.m_iBuild == 0 || IsLazyReady(b) || (!stepOk && b.m_iBuildFails >= 3))
					m_iBuildKey = -1;
				continue;
			}
			// none building (or a stamp finished it, or it was freed): start the next
			m_iBuildKey = -1;
			if (candKey.IsEmpty())
				break;
			int best = 0;
			for (int i = 1; i < candKey.Count(); i++)
			{
				if (candDist[i] < candDist[best])
					best = i;
			}
			int k = candKey[best];
			candKey.Remove(best);
			candDist.Remove(best);
			MUD_FinePatch sp = m_mPatches.Get(k);
			if (sp)
			{
				// asleep: its first wake step goes in this step
				if (sp.m_bAsleep)
				{
					WakePatch(sp);
					m_iBuildKey = k;
					BuildStep(sp);
					if (sp.m_iBuild == 0)
						m_iBuildKey = -1;
				}
				else if (sp.m_iBuild > 0 && !IsLazyReady(sp))
				{
					// client: a far patch held as heights only, the camera came near (FarDue)
					m_iBuildKey = k;
					BuildStep(sp);
					if (sp.m_iBuild == 0)
						m_iBuildKey = -1;
				}
				continue;
			}
			MUD_FinePatch np = StartPatch(k - (k / m_iNPX) * m_iNPX, k / m_iNPX);
			if (np)
			{
				m_iBuildKey = k;
				m_iMadeAhead++;
				if (np.m_bFloor && m_aFieldDue.Contains(k))
					m_iFieldMade++;
				m_sFrameStep += "setup ";
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Players within m_fPlayerRadius: their patches count as near (no sleep, no free) and sleeping ones
	//! there join the wake candidates. Players make no new patches (only vehicles and stamps do).
	protected void NearPlayers(array<int> candKey, array<float> candDist)
	{
		if (m_fPlayerRadius <= 0)
			return;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		float size = PCELLS * m_fFine;
		int feetWakes = 0;
		m_aPlayerIds.Clear();
		pm.GetPlayers(m_aPlayerIds);
		foreach (int id : m_aPlayerIds)
		{
			IEntity ent = pm.GetPlayerControlledEntity(id);
			if (!ent)
				continue;
			vector o = ent.GetOrigin();
			bool makeUnder = m_bRender && MUD_LookLift.MakeUnder(ent);     // LOOK LIFT PRESS-OUT: a patch to press in
			int pxa = Math.Floor((o[0] - m_fX0 - m_fPlayerRadius) / size);
			int pxb = Math.Floor((o[0] - m_fX0 + m_fPlayerRadius) / size);
			int pza = Math.Floor((o[2] - m_fZ0 - m_fPlayerRadius) / size);
			int pzb = Math.Floor((o[2] - m_fZ0 + m_fPlayerRadius) / size);
			for (int pz = Math.Max(pza, 0); pz <= Math.Min(pzb, m_iNPZ - 1); pz++)
			{
				for (int px = Math.Max(pxa, 0); px <= Math.Min(pxb, m_iNPX - 1); px++)
				{
					float x0 = m_fX0 + px * size;
					float z0 = m_fZ0 + pz * size;
					float dx = Math.Max(Math.Max(x0 - o[0], 0), o[0] - x0 - size);
					float dz = Math.Max(Math.Max(z0 - o[2], 0), o[2] - z0 - size);
					float d = Math.Sqrt(dx * dx + dz * dz);
					MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
					if (!p)
					{
						// LOOK LIFT PRESS-OUT (MUD_LookLift): players on foot make the patch under them while the test runs
						int mk = pz * m_iNPX + px;
						if (makeUnder && d <= MUD_LookLift.MAKE_R && !m_sNoHole.Contains(mk) && !candKey.Contains(mk))
						{
							candKey.Insert(mk);
							candDist.Insert(d);
						}
						continue;
					}
					if (d > m_fPlayerRadius)
						continue;
					p.m_fNearAt = m_fTime;
					// FEET WAKE (2026-10-24, trench builder): an asleep patch under a player's feet stands on the
					// coarse skin collider (undug ground): user walked ON TOP of instant-dug trenches whose patches
					// slept while the body stood far (manager open). Wake + finish it now, as a wheel does.
					if (p.m_bAsleep && d <= FEET_WAKE_R && feetWakes < FEET_WAKES_PER_FRAME && p.m_iBuildFails < 3)
					{
						feetWakes++;
						m_iFeetWoke++;
						EnsurePatch(px, pz);
						continue;
					}
					if (p.m_bAsleep && p.m_iKey != m_iBuildKey && p.m_iBuildFails < 3 && !candKey.Contains(p.m_iKey))
					{
						candKey.Insert(p.m_iKey);
						candDist.Insert(d);
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Once a second: undeformed patches with no vehicle near for m_fFreeAfter go (not the one Ahead
	//! is building; one it gave up on goes too)
	protected void FreeIdle(float timeSlice, int t0)
	{
		m_fSinceFree += timeSlice;
		if (m_fSinceFree >= 1)
		{
			m_fSinceFree = 0;
			m_aFreeQueue.Clear();
			m_aSleepQueue.Clear();
			foreach (int key, MUD_FinePatch p : m_mPatches)
			{
				if (key == m_iBuildKey)
					continue;
				if (FreeDue(p))
					m_aFreeQueue.Insert(key);
				else if (MaySleep(p))
					m_aSleepQueue.Insert(key);
			}
		}
		if (m_aFreeQueue.IsEmpty())
		{
			SleepIdle(t0);
			return;
		}
		if (!MayRun(t0, m_iFreeSkip, COST_FREE))
		{
			m_iFreeSkip++;
			m_iWaitFree++;
			return;
		}
		m_iFreeSkip = 0;
		// one per frame: 7-16 frees in one frame took 13-26 ms (each rebuilds a skin chunk; measured
		// 2026-10-09 14:03-14:05 Exp 192624). Re-checked here: a vehicle may have come back.
		while (!m_aFreeQueue.IsEmpty())
		{
			int last = m_aFreeQueue.Count() - 1;
			MUD_FinePatch q = m_mPatches.Get(m_aFreeQueue[last]);
			m_aFreeQueue.Remove(last);
			if (q && q.m_iKey != m_iBuildKey && FreeDue(q))
			{
				if (q.m_bFloor)
					m_iFloorFreed++;
				FreePatch(q);
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Undeformed, no vehicle / player near for m_fFreeAfter; a floor-only one (FLOOR_FREE) also out of FieldDue's range
	//! that long - its rut tiles stay, a rebuild reads them (StartPatch -> RutFieldOn)
	protected bool FreeDue(MUD_FinePatch p)
	{
		if (p.m_bDeformed || m_fTime - p.m_fNearAt <= m_fFreeAfter)
			return false;
		if (!p.m_bFloor)
			return true;
		return FLOOR_FREE && m_fTime - p.m_fFieldSeenAt > m_fFreeAfter;
	}

	//------------------------------------------------------------------------------------------------
	//! Deformed, live, no rebuild pending, not building, no vehicle near for m_fSleepAfter
	protected bool MaySleep(MUD_FinePatch p)
	{
		return (p.m_bDeformed || p.m_bFloor) && !p.m_bAsleep && p.m_iBuild == 0 && p.m_Phys && !p.m_bQueued
			&& p.m_iKey != m_iBuildKey && m_fTime - p.m_fNearAt > m_fSleepAfter
			&& !p.m_bRush && p.m_HoldRes == null && !p.m_bHoldCover;     // CRATER HOLD: not with a mesh / cover still to show
	}

	//------------------------------------------------------------------------------------------------
	//! One far deformed patch sleeps per frame, under the frame budget like a free (each rebuilds a
	//! skin chunk, ~1.6 ms), and only in a frame without a free. Re-checked here: a vehicle may have
	//! come back.
	protected void SleepIdle(int t0)
	{
		if (m_aSleepQueue.IsEmpty())
			return;
		if (!MayRun(t0, m_iSleepSkip, COST_FREE))
		{
			m_iSleepSkip++;
			m_iWaitSleep++;
			return;
		}
		m_iSleepSkip = 0;
		while (!m_aSleepQueue.IsEmpty())
		{
			int last = m_aSleepQueue.Count() - 1;
			MUD_FinePatch q = m_mPatches.Get(m_aSleepQueue[last]);
			m_aSleepQueue.Remove(last);
			if (q && MaySleep(q))
			{
				SleepPatch(q);
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD (MUD_VoxelProbe.RUT_FIELD): rut floor samples [gx0..gx1] x [gz0..gz1] (= global fine verts) changed.
	//! Only EXISTING patches there are finished / woken as a wheel press does (server too: it needs the collider), then
	//! rebuild mesh + collider; missing ones read the floor when they are built (StartPatch -> RutFieldOn, FieldDue
	//! makes them near a camera / player / vehicle) - no patch storm when a joiner replays the ops. Before review
	//! Stage 4 (v111) this made every missing patch (EnsureRange). The floor is never written into m_aH and never
	//! queued for the net (the op stream carries it; m_aH would go to every client and again in the patch JIP).
	//! Returns the patches marked.
	static int FloorDirty(int gx0, int gx1, int gz0, int gz1, bool keepMesh = false)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return 0;
		s_Inst.FloorFinish(gx0, gx1, gz0, gz1);
		if (s_Inst.m_bRender)
			MUD_LookLift.GrowBox(gx0, gx1, gz0, gz1);     // look lift GROW: deeper ruts lift the ground round them more
		if (keepMesh)
			s_Inst.m_iLookCovered++;
		return s_Inst.MarkFloorDirty(gx0, gx1, gz0, gz1, keepMesh);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: EnsureRange for the patches that exist (building -> finished, asleep -> woken; wheel cap), and every
	//! patch key in range remembered for FieldDue
	protected void FloorFinish(int gx0, int gx1, int gz0, int gz1)
	{
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
			{
				int key = pz * m_iNPX + px;
				m_sFieldPatches.Insert(key);
				MUD_FinePatch fp = m_mPatches.Get(key);
				if (!fp)
					continue;
				if (FLOOR_SPREAD && fp.m_iBuild > 0 && !fp.m_bAsleep)
				{
					if (!m_aFloorSpread.Contains(key))
					{
						m_aFloorSpread.Insert(key);
						m_iFloorSpreadAdds++;
					}
					continue;
				}
				EnsurePatch(px, pz, true);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD (FLOOR_FREE): once a second, the rut patches (m_sFieldPatches) within range of the camera (drawn
	//! only: m_fClientBuildRadius, 0 = any distance), a player (m_fPlayerRadius) or a wheeled vehicle at any speed
	//! (m_fAheadRadius; Ahead makes patches for moving ones only). Existing ones there are kept (m_fFieldSeenAt);
	//! every frame the nearest one still missing or building joins Ahead's candidates (camera ones after the rest).
	protected void FieldDue(array<int> candKey, array<float> candDist)
	{
		if (!MUD_VoxelProbe.RUT_FIELD || !FLOOR_FREE || m_sFieldPatches.IsEmpty())
			return;
		if (m_fTime >= m_fFieldCheckAt)
		{
			m_fFieldCheckAt = m_fTime + 1;
			array<vector> fromPos = {};
			array<float> fromR = {};
			array<float> fromAdd = {};
			if (m_bRender)
			{
				vector cam[4];
				GetWorld().GetCurrentCamera(cam);
				fromPos.Insert(cam[3]);
				fromR.Insert(m_fClientBuildRadius);
				fromAdd.Insert(m_fAheadRadius);
			}
			PlayerManager pm = GetGame().GetPlayerManager();
			if (pm && m_fPlayerRadius > 0)
			{
				m_aPlayerIds.Clear();
				pm.GetPlayers(m_aPlayerIds);
				foreach (int id : m_aPlayerIds)
				{
					IEntity ent = pm.GetPlayerControlledEntity(id);
					if (!ent)
						continue;
					fromPos.Insert(ent.GetOrigin());
					fromR.Insert(m_fPlayerRadius);
					fromAdd.Insert(0);
				}
			}
			foreach (Vehicle veh : MUD_Wheels.GetAll())
			{
				if (!veh)
					continue;
				fromPos.Insert(veh.GetOrigin());
				fromR.Insert(m_fAheadRadius);
				fromAdd.Insert(0);
			}
			m_aFieldDue.Clear();
			m_aFieldDueDist.Clear();
			foreach (int fk : m_sFieldPatches)
			{
				int fpx = fk - (fk / m_iNPX) * m_iNPX;
				int fpz = fk / m_iNPX;
				float best = -1;
				for (int s = 0; s < fromPos.Count(); s++)
				{
					float fd = PatchDist(fpx, fpz, fromPos[s]);
					if (fromR[s] > 0 && fd > fromR[s])
						continue;
					float rank = fd + fromAdd[s];
					if (best < 0 || rank < best)
						best = rank;
				}
				if (best < 0)
					continue;
				MUD_FinePatch fp = m_mPatches.Get(fk);
				if (fp)
					fp.m_fFieldSeenAt = m_fTime;
				m_aFieldDue.Insert(fk);
				m_aFieldDueDist.Insert(best);
			}
		}
		// the nearest one still to build (Ahead builds one at a time); live, asleep (its mesh shows; NearPlayers and
		// Ahead wake it), given up or without a hole cell: dropped until the next check
		// key + dist, not an index: Remove(i) moves the last entry into i (index out of bounds 2026-10-16)
		int pick = -1;
		float pickDist;
		for (int i = m_aFieldDue.Count() - 1; i >= 0; i--)
		{
			int k = m_aFieldDue[i];
			MUD_FinePatch p = m_mPatches.Get(k);
			bool drop;
			if (p)
				drop = p.m_iBuild == 0 || p.m_bAsleep || IsLazyReady(p) || p.m_iBuildFails >= 3;
			else
				drop = !HasHole(k - (k / m_iNPX) * m_iNPX, k / m_iNPX);
			if (drop)
			{
				m_aFieldDue.Remove(i);
				m_aFieldDueDist.Remove(i);
				continue;
			}
			if (k == m_iBuildKey || candKey.Contains(k))
				continue;
			if (pick < 0 || m_aFieldDueDist[i] < pickDist)
			{
				pick = k;
				pickDist = m_aFieldDueDist[i];
			}
		}
		if (pick >= 0)
		{
			candKey.Insert(pick);
			candDist.Insert(pickDist);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! REBUILD 5 option 3: a look sample dropped with no rebuild (MUD_LookRuts CATCH: up to 2 cm) still changes the
	//! mesh input there - grow the changed box of the patches holding [gx0..gx1] x [gz0..gz1], no dirt
	static void LookBox(int gx0, int gx1, int gz0, int gz1)
	{
		if (!s_Inst || !s_Inst.m_bReady || !s_Inst.m_bRender)
			return;
		for (int pz = s_Inst.PatchLo(gz0, s_Inst.m_iNPZ); pz <= s_Inst.PatchHi(gz1, s_Inst.m_iNPZ); pz++)
		{
			for (int px = s_Inst.PatchLo(gx0, s_Inst.m_iNPX); px <= s_Inst.PatchHi(gx1, s_Inst.m_iNPX); px++)
			{
				MUD_FinePatch p = s_Inst.m_mPatches.Get(pz * s_Inst.m_iNPX + px);
				if (p)
					s_Inst.MeshBoxG(p, gx0, gx1, gz0, gz1);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client look ruts (MUD_LookRuts): mesh-only dirt on the patches holding global fine vertices [gx0..gx1] x
	//! [gz0..gz1] - no collider, no net, no new patches; builds past the floor settle hold (Rebuild). Patches marked.
	//! since / speed: TEMP delay meter (m_fTime the cut's oldest segment's wheel passed, its vehicle speed); -1 = not timed
	static int LookDirty(int gx0, int gx1, int gz0, int gz1, float since = -1, float speed = 0)
	{
		if (!s_Inst || !s_Inst.m_bReady || !s_Inst.m_bRender)
			return 0;
		return s_Inst.MarkLookDirty(gx0, gx1, gz0, gz1, since, speed);
	}

	//------------------------------------------------------------------------------------------------
	protected int MarkLookDirty(int gx0, int gx1, int gz0, int gz1, float since, float speed)
	{
		int marked = 0;
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
				{
					MUD_LookRuts.NoteMark(MUD_LookRuts.M_MISSING);     // TEMP split delay meter: look floor not drawn here
					continue;
				}
				// LOOK SPREAD (user 2026-10-18 DS: holes right at / ahead of the wheels at speed, v224 = before): a lazy or
				// building patch draws nothing (the skin carries it), so a live neighbour's look rut ended in an open step
				// at their border (the skin has no skirt) until the server floor finished the patch. Finish it now, as
				// FloorFinish does for the server floor: one build step per frame (FloorSpreadTick).
				if (LOOK_SPREAD && FLOOR_SPREAD && p.m_iBuild > 0 && !p.m_bAsleep && !m_aFloorSpread.Contains(p.m_iKey))
				{
					m_aFloorSpread.Insert(p.m_iKey);
					m_iLookSpreadAdds++;
				}
				if (p.m_fLookCutAt < 0)
				{
					p.m_fLookCutAt = m_fTime;     // TEMP split delay meter
					if (p.m_iBuild > 0)
						MUD_LookRuts.NoteMark(MUD_LookRuts.M_BUILDING);
					else if (p.m_bAsleep)
						MUD_LookRuts.NoteMark(MUD_LookRuts.M_ASLEEP);
					else
						MUD_LookRuts.NoteMark(MUD_LookRuts.M_READY);
				}
				p.m_bMeshDirty = true;
				p.m_bLookDirty = true;
				if (since >= 0)
					p.m_bWhyLook = true;     // REBUILD_WHY
				else
					p.m_bWhySweep = true;
				MeshBoxG(p, gx0, gx1, gz0, gz1);     // REBUILD 5 option 3
				if (since >= 0 && (p.m_fLookSince < 0 || since < p.m_fLookSince))
				{
					p.m_fLookSince = since;     // TEMP delay meter
					p.m_fLookSpeed = speed;
				}
				if (!p.m_bQueued)
				{
					p.m_bQueued = true;
					p.m_fQueuedAt = m_fTime;
					m_aDirty.Insert(p.m_iKey);
				}
				marked++;
			}
		}
		return marked;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: MarkDirty without the blur reach (the floor is not blurred) and without the net rect
	protected int MarkFloorDirty(int gx0, int gx1, int gz0, int gz1, bool keepMesh = false)
	{
		int marked = 0;
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				p.m_bFloor = true;
				if (FLOOR_SETTLE)
				{
					// hold only a patch with no other dirt owed (MarkDirty clears the hold)
					if (!p.m_bFloorWait && !p.m_bMeshDirty && !p.m_bColDirty)
					{
						p.m_bFloorWait = true;
						p.m_fFloorFirstAt = m_fTime;
					}
					p.m_fFloorDirtAt = m_fTime;
				}
				if (m_bRender && !keepMesh)     // LOOK_COVERS: the look floor already shows it
				{
					if (!p.m_bMeshDirty)
						p.m_bRutMesh = true;     // REBUILD 5 option 1: only rut floor dirt so far
					p.m_bMeshDirty = true;
					p.m_bWhyFloor = true;     // REBUILD_WHY
				}
				MeshBoxG(p, gx0, gx1, gz0, gz1);     // REBUILD 5 option 3 (keepMesh too: the next build reads it)
				p.m_bColDirty = true;
				if (!p.m_bQueued)
				{
					p.m_bQueued = true;
					p.m_fQueuedAt = m_fTime;
					m_aDirty.Insert(p.m_iKey);
				}
				marked++;
			}
		}
		return marked;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: p's mesh / collider read the rut floor (a floor tile reaches it; FLOOR_FREE false: also keeps it from being freed)
	protected bool RutFieldOn(MUD_FinePatch p)
	{
		if (!MUD_VoxelProbe.RUT_FIELD)
			return false;
		int gx0 = p.m_iPx * PCELLS;
		int gz0 = p.m_iPz * PCELLS;
		if (!MUD_VoxelProbe.FieldInBox(gx0, gx0 + PCELLS, gz0, gz0 + PCELLS))
			return false;
		p.m_bFloor = true;
		return true;
	}

	// HEIGHTFIELD CRATERS (SURFACE_OPS false, user 2026-10-21): heightfield height changes reach the mud sheet as one
	// re-ground box a frame (as the v010 test copy: voxel ops already call MUD_MudSheet.FloorChanged themselves)
	protected bool m_bSheetBox;
	protected vector m_vSheetLo;
	protected vector m_vSheetHi;

	// CRATER RUSH (user DS 2026-10-22: craters showed late, "like it builds them"): a crater's patches build mesh +
	// collider in the next frame on every machine (server: Crater; client: the heights message carries RUSH_BIT),
	// not one patch per frame at the queue pace. Cost = a few patch builds in one frame per crater (measure on the DS).
	static const int RUSH_BIT = 16777216;     // 1 << 24 in a heights message rect (the rect itself uses bits 0-23)
	static const int RUSH_MAX = 12;           // build steps rushed per frame (the rest next frame; was 6 whole patches)
	static const int RUSH_MS = 8;             // and no more ms per frame after the first step (a barrage; was 12 with whole patches)
	// RUSH SPLIT (user 2026-10-22 'big craters freeze', DS profile results/2026-10-22-crater-freeze-profile.md: rush
	// frames 12-28 ms, a whole patch = index + collider + mesh ~10 ms in one go): one build step per pick (a new patch:
	// BuildStep; a live one: collider, then mesh at the next pick), nearest the camera first. Peak per frame ~ RUSH_MS +
	// one step instead of RUSH_MS + one whole patch. false = whole patches (FinishNow), as before.
	static const bool RUSH_SPLIT = true;
	static const int RUSH_COL = 1;            // MUD_FinePatch.m_iRushDone bits: live patch collider done
	static const int RUSH_MESH = 2;           // ... mesh done (or nothing to draw)
	// CRATER HOLD (user DS 2026-10-22 23:35 'when a crater explodes I can briefly see through the map'): RUSH_SPLIT builds
	// a crater's patches over 3-6 frames (rush log lines). A patch drawn WITH the crater next to one drawn without it (or
	// the skin, still at the old height) opens a crack at the seam as deep as the crater there; the band skirt (~0.6 m,
	// BuildMeshBand SKIRT DEPTH) is shorter, and under it the real terrain is holed = see-through. Now, on a drawing
	// machine, the new mesh of a rushed patch (and a new patch's skin cover) waits; when the rush list is empty all of
	// them show in one frame (SetObject + one skin chunk rebuild per chunk). Builds stay spread. false = show at once.
	// Cost: up to RUSH_HOLD_MAX_S the client's skin collider still stands at the old height over a new patch (server
	// never holds: m_bRender false).
	static const bool RUSH_HOLD = true;
	static const float RUSH_HOLD_MAX_S = 1.0;     // a barrage keeping the rush busy: show what is held after this anyway
	protected ref array<int> m_aHoldKeys = {};       // keys of patches with m_HoldRes / m_bHoldCover
	protected float m_fHeldSince;                // m_fTime of the oldest hold in m_aHoldKeys
	protected int m_iHeldShown;                  // stats: patches shown by HoldRelease
	protected int m_iHeldWorstMs;                // stats: worst HoldRelease ms
	// CRATER BUILD ONCE (user DS 2026-10-22 'big craters freeze'; DS log 23:02: Crater 15-85 ms, avg 46, in the blast
	// frame; client: 27 patches made by stamps in 10 s): EnsureRange / EnsurePatch built every missing patch in full at
	// the OLD heights (FinishNow), then the rush built it again with the crater. Now the crater only starts them
	// (StartPatch: heights, no mesh / collider) and the rush finishes each once with the crater in. false = old way.
	static const bool BUILD_ONCE = true;
	protected bool m_bRushMark;               // MarkDirty: the patches it dirties (own + collider reach) go to m_aRush
	protected ref array<int> m_aRush = {};

	// CRATER DECAL on the heightfield (user DS 2026-10-22: vanilla's blast decal lands on the hidden terrain at the old
	// height, never in our crater). Each machine that draws notes the blast (a client gets only heights, no crater
	// data); once dug ground is built there, the vanilla mortar decal goes onto the patch entities under it, projected
	// down. World.CreateDecal returns null even when the decal shows (mud-gotchas 2026-10-18): judge by eye.
	static const bool HF_DECAL = true;
	static const float DECAL_WAIT_S = 4;      // a note with no dug ground by then is dropped (no crater: min charge, vehicle)
	static const float DECAL_MAX_R = 8;       // m: decal radius cap
	static const float DECAL_SIZE_K = 1.0;    // decal size per m of dug radius (tune by eye)
	static const float DECAL_LIFE_S = 300;    // s (vanilla mortar: 60)
	protected ref array<vector> m_aDecalPos = {};
	protected ref array<float> m_aDecalAt = {};
	protected ref array<float> m_aDecalD0 = {};     // dug depth there at the note (an older crater)

	//------------------------------------------------------------------------------------------------
	//! Grow this frame's sheet re-ground box by fine vertices gx0..gx1, gz0..gz1 (flushed next EOnFrame)
	protected void SheetBox(int gx0, int gx1, int gz0, int gz1)
	{
		vector lo = Vector(m_fX0 + gx0 * m_fFine, 0, m_fZ0 + gz0 * m_fFine);
		vector hi = Vector(m_fX0 + gx1 * m_fFine, 0, m_fZ0 + gz1 * m_fFine);
		if (!m_bSheetBox)
		{
			m_bSheetBox = true;
			m_vSheetLo = lo;
			m_vSheetHi = hi;
			return;
		}
		m_vSheetLo = Vector(Math.Min(m_vSheetLo[0], lo[0]), 0, Math.Min(m_vSheetLo[2], lo[2]));
		m_vSheetHi = Vector(Math.Max(m_vSheetHi[0], hi[0]), 0, Math.Max(m_vSheetHi[2], hi[2]));
	}

	//------------------------------------------------------------------------------------------------
	//! Flag every patch holding a changed vertex (mesh + collider) and every patch the blur reaches
	//! from it (collider) - as MUD_GroundEntity.c:1892 MarkDirty, global fine vertex coords.
	//! Task 9 adds the net rect here.
	protected void MarkDirty(int gx0, int gx1, int gz0, int gz1)
	{
		if (m_bRender && MUD_MudSheet.ENABLED)
			SheetBox(gx0 - 1, gx1 + 1, gz0 - 1, gz1 + 1);
		int r = m_aSmoothW.Count() / 2;
		bool net = IsNet();
		// MUD RING: a hole up to RING_REACH_V steps past a patch's edge changes its dirt ring - that mesh builds too
		int rm = 0;
		if (RING_DIRT)
			rm = RING_REACH_V;
		int rl = Math.Max(r, rm);
		for (int pz = PatchLo(gz0 - rl, m_iNPZ); pz <= PatchHi(gz1 + rl, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0 - rl, m_iNPX); px <= PatchHi(gx1 + rl, m_iNPX); px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				int ox = px * PCELLS;
				int oz = pz * PCELLS;
				bool own = gx1 >= ox && gx0 <= ox + PCELLS && gz1 >= oz && gz0 <= oz + PCELLS;
				bool ownRing = gx1 + rm >= ox && gx0 - rm <= ox + PCELLS && gz1 + rm >= oz && gz0 - rm <= oz + PCELLS;
				bool inR = gx1 + r >= ox && gx0 - r <= ox + PCELLS && gz1 + r >= oz && gz0 - r <= oz + PCELLS;
				if (m_bRushMark && (own || inR))
				{
					if (!p.m_bRush)
					{
						p.m_bRush = true;     // CRATER RUSH
						m_aRush.Insert(p.m_iKey);
					}
					p.m_iRushDone = 0;     // RUSH SPLIT: a new crater on it - collider and mesh again
				}
				if (ownRing && m_bRender && rm > 0)
					MeshBoxG(p, gx0 - rm, gx1 + rm, gz0 - rm, gz1 + rm);     // MUD RING: the ring reach rebuilds too
				if (!inR)
				{
					// MUD RING only (past the blur reach): mesh, no collider
					if (ownRing && m_bRender)
					{
						p.m_bMeshDirty = true;
						p.m_bHMesh = true;
						if (!p.m_bQueued)
						{
							p.m_bQueued = true;
							p.m_fQueuedAt = m_fTime;
							m_aDirty.Insert(p.m_iKey);
						}
					}
					continue;
				}
				if (ownRing && m_bRender)
				{
					p.m_bMeshDirty = true;
					p.m_bRutMesh = false;     // REBUILD 5 option 1: height dirt - normal pace
					p.m_bHMesh = true;        // REBUILD 6: LOOK_PACE keeps today's pace, REBUILD_WHY kind 4
					if (LOOK_PACE)
						p.m_fMeshHoldTo = -100;     // C1 HOLD_RESET: a small-skip hold does not delay real height dirt
				}
				p.m_bColDirty = true;
				p.m_bFloorWait = false;     // real dirt: build at today's pace (FLOOR_SETTLE)
				if (!p.m_bQueued)
				{
					p.m_bQueued = true;
					p.m_fQueuedAt = m_fTime;
					m_aDirty.Insert(p.m_iKey);
				}

				int sx0 = Math.ClampInt(gx0 - r - ox, 0, PCELLS);
				int sx1 = Math.ClampInt(gx1 + r - ox, 0, PCELLS);
				int sz0 = Math.ClampInt(gz0 - r - oz, 0, PCELLS);
				int sz1 = Math.ClampInt(gz1 + r - oz, 0, PCELLS);
				MeshBoxL(p, sx0, sx1, sz0, sz1);     // REBUILD 5 option 3: m_aH + m_aSmooth (ridge flat test) of the reach
				if (p.m_iSx0 < 0)
				{
					p.m_iSx0 = sx0;
					p.m_iSx1 = sx1;
					p.m_iSz0 = sz0;
					p.m_iSz1 = sz1;
				}
				else
				{
					p.m_iSx0 = Math.Min(p.m_iSx0, sx0);
					p.m_iSx1 = Math.Max(p.m_iSx1, sx1);
					p.m_iSz0 = Math.Min(p.m_iSz0, sz0);
					p.m_iSz1 = Math.Max(p.m_iSz1, sz1);
				}

				if (!net || !own)
					continue;
				// changed verts inside this patch (the patch range is widened by the blur)
				int nx0 = Math.ClampInt(gx0 - ox, 0, PCELLS);
				int nx1 = Math.ClampInt(gx1 - ox, 0, PCELLS);
				int nz0 = Math.ClampInt(gz0 - oz, 0, PCELLS);
				int nz1 = Math.ClampInt(gz1 - oz, 0, PCELLS);
				if (p.m_iNx0 < 0)
				{
					p.m_iNx0 = nx0;
					p.m_iNx1 = nx1;
					p.m_iNz0 = nz0;
					p.m_iNz1 = nz1;
				}
				else
				{
					if (nx0 < p.m_iNx0)
						p.m_iNx0 = nx0;
					if (nx1 > p.m_iNx1)
						p.m_iNx1 = nx1;
					if (nz0 < p.m_iNz0)
						p.m_iNz0 = nz0;
					if (nz1 > p.m_iNz1)
						p.m_iNz1 = nz1;
				}
				if (m_bRushMark)
					p.m_bNetRush = true;     // CRATER RUSH: the client rushes it too
				if (!p.m_bNetQueued)
				{
					p.m_bNetQueued = true;
					m_aNetKeys.Insert(p.m_iKey);
				}
			}
		}
	}


	//------------------------------------------------------------------------------------------------
	//! LAZY RUT MESH (MUD_MudSheet.LAZY_RUT_MESH, synced mud sheet Stage 3, user 2026-10-18 'hide it under the sheet,
	//! colliders stay immediate'): does the mud sheet draw over all of patch p? Kept SHEET_CHECK s, or until the
	//! sheet's cover changes (MUD_MudSheet.CoverGen). Rendering machines only.
	protected bool SheetCovered(MUD_FinePatch p)
	{
		if (!MUD_MudSheet.LazyOn() || !m_bRender || !p.m_Ent)
			return false;
		int gen = MUD_MudSheet.CoverGen();
		if (gen != p.m_iSheetGen || m_fTime - p.m_fSheetAt >= SHEET_CHECK)
		{
			p.m_iSheetGen = gen;
			p.m_fSheetAt = m_fTime;
			float side = PCELLS * m_fFine;
			float x0 = m_fX0 + p.m_iPx * side;
			float z0 = m_fZ0 + p.m_iPz * side;
			p.m_bSheetCov = MUD_MudSheet.Covers(x0, z0, x0 + side, z0 + side);
		}
		return p.m_bSheetCov;
	}

	//------------------------------------------------------------------------------------------------
	//! LAZY RUT MESH: p (mesh dirty) under the sheet - its stale mesh (higher than the pressed sheet) is hidden and its
	//! rebuild held. True = held.
	//! RENDER_GATE: true = this patch's mesh rebuild waits (has a mesh, not drawn last frame, past RENDER_NEAR)
	protected bool RenderWait(MUD_FinePatch p)
	{
		if (!RENDER_GATE || !m_bRender || !p.m_Ent || p.m_fMeshAt < 0)
			return false;
		if (p.m_Ent.IsRendered())
			return false;
		float half = PCELLS * m_fFine * 0.5;
		vector c = p.m_Ent.GetOrigin() + Vector(half, 0, half);
		if (vector.DistanceSqXZ(c, m_vGateCam) < RENDER_NEAR * RENDER_NEAR)
			return false;
		m_iRenderSkip++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool SheetHolds(MUD_FinePatch p)
	{
		if (!SheetCovered(p))
			return false;
		if (!p.m_bSheetHid)
		{
			p.m_bSheetHid = true;
			p.m_Ent.ClearFlags(EntityFlags.VISIBLE, false);
			m_aSheetHid.Insert(p.m_iKey);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! LAZY RUT MESH, every frame before the rebuild pick: hidden patches the sheet no longer covers (mesh dropped
	//! past the camera radius, crater, zone edge) show again; their held mesh rebuild then runs at the normal pace.
	//! A patch freed meanwhile just leaves the list (a new one under the same key starts shown).
	protected void SheetShow()
	{
		for (int i = m_aSheetHid.Count() - 1; i >= 0; i--)
		{
			MUD_FinePatch p = m_mPatches.Get(m_aSheetHid[i]);
			if (p && p.m_bSheetHid && SheetCovered(p))
				continue;
			if (p && p.m_bSheetHid)
			{
				p.m_bSheetHid = false;
				if (p.m_Ent)
					p.m_Ent.SetFlags(EntityFlags.VISIBLE, false);
				m_iSheetShown++;
			}
			m_aSheetHid.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! At most one mesh and one collider per frame, the longest-waiting first (MUD_GroundEntity.c:2220),
	//! each only while the frame is under budget. A patch still building waits in the queue: its last
	//! build steps use the newest heights.
	protected void Rebuild(int t0)
	{
		MUD_FinePatch mp;
		MUD_FinePatch cp;
		int lookCand = 0;     // TEMP split delay meter: look patches due this frame (one mesh per frame)
		if (RENDER_GATE && m_bRender)
		{
			vector gcm[4];
			GetWorld().GetCurrentCamera(gcm);
			m_vGateCam = gcm[3];
		}
		SheetShow();
		if (!m_aRush.IsEmpty())
			RushTick();     // CRATER RUSH: before the queue (a rushed patch is no longer dirty there)
		if (!m_aHoldKeys.IsEmpty())
			HoldRelease();  // CRATER HOLD: the whole crater shows in one frame
		if (!m_aDecalPos.IsEmpty())
			DecalTick();    // CRATER DECAL: after the rush built the crater mesh
		for (int i = m_aDirty.Count() - 1; i >= 0; i--)
		{
			MUD_FinePatch p = m_mPatches.Get(m_aDirty[i]);
			if (!p || (!p.m_bMeshDirty && !p.m_bColDirty))
			{
				if (p)
				{
					p.m_bQueued = false;
					p.m_bFloorWait = false;
				}
				m_aDirty.Remove(i);
				continue;
			}
			// LAZY RUT MESH: under the mud sheet the mesh waits (hidden); the collider below goes on as before
			bool sheetHeld = m_bRender && p.m_bMeshDirty && SheetHolds(p);
			if (sheetHeld)
				m_iSheetHeld++;
			bool lk = p.m_bLookDirty && m_bRender && p.m_bMeshDirty;     // TEMP split delay meter: a look patch waiting
			if (lk && p.m_iBuild > 0)
				MUD_LookRuts.NoteWait(MUD_LookRuts.W_BUILDING);
			else if (lk && p.m_bAsleep)
				MUD_LookRuts.NoteWait(MUD_LookRuts.W_ASLEEP);
			else if (lk && sheetHeld)
				MUD_LookRuts.NoteWait(MUD_LookRuts.W_SHEET);
			if (p.m_iBuild > 0 || p.m_bAsleep)
				continue;   // building: its last steps use the newest heights; asleep: its wake does
			if (p.m_bFloorWait)
			{
				// FLOOR_SETTLE: held until quiet or the max wait, then released to the normal mesh / collider pace
				bool quiet = m_fTime - p.m_fFloorDirtAt >= FLOOR_QUIET;
				bool waitedOut = m_fTime - p.m_fFloorFirstAt >= FLOOR_MAX_WAIT;
				if (!quiet && !waitedOut)
				{
					// client look ruts: the MESH may build now (look floor shows), the collider keeps waiting
					if (p.m_bLookDirty && m_bRender && p.m_bMeshDirty && !sheetHeld && m_fTime - p.m_fMeshAt >= LOOK_MESH_EVERY && m_fTime >= p.m_fMeshHoldTo && (!mp || p.m_fMeshAt < mp.m_fMeshAt))
						mp = p;
					if (lk && !sheetHeld)
					{
						if (m_fTime - p.m_fMeshAt < LOOK_MESH_EVERY)
							MUD_LookRuts.NoteWait(MUD_LookRuts.W_EVERY);
						else
							lookCand++;
					}
					m_iSettleHeld++;
					continue;
				}
				p.m_bFloorWait = false;
				if (quiet)
					m_iSettleQuiet++;
				else
					m_iSettleMax++;
			}
			float meshEvery = MeshEvery(p);
			if (RUT_PACE && p.m_bRutMesh && !p.m_bLookDirty && RUT_MESH_EVERY > meshEvery)
			{
				// REBUILD 5 option 1: rut floor dirt alone - slower pace (debug: held = the old pace would build now)
				if (m_bRender && p.m_bMeshDirty && m_fTime - p.m_fMeshAt >= meshEvery && m_fTime - p.m_fMeshAt < RUT_MESH_EVERY)
				{
					m_iPaceHeldPF++;
					p.m_bPaceHeld = true;
				}
				meshEvery = RUT_MESH_EVERY;
			}
			if (m_bRender && p.m_bMeshDirty && !sheetHeld && m_fTime - p.m_fMeshAt >= meshEvery && m_fTime >= p.m_fMeshHoldTo && (!mp || p.m_fMeshAt < mp.m_fMeshAt) && !RenderWait(p))
				mp = p;     // PERF IDEA 2 fix: a skipped patch waits for m_fMeshHoldTo (one skip per window, not one per 0.25 s)
			if (lk && !sheetHeld)
			{
				if (m_fTime - p.m_fMeshAt < meshEvery)
					MUD_LookRuts.NoteWait(MUD_LookRuts.W_EVERY);
				else
					lookCand++;
			}
			if (p.m_bColDirty && m_fTime - p.m_fColAt >= m_fColliderInterval && (!cp || p.m_fColAt < cp.m_fColAt))
				cp = p;
		}
		// an index-list cache miss (~2.5 ms) is this frame's whole rebuild job; the build runs next frame
		// on the cached lists. Layouts vary a lot (misses 120 vs 85 new patches per 10 s, 14:28 Exp 192624).
		bool idxJob = false;
		bool mpLook = mp && mp.m_bLookDirty;     // TEMP split delay meter
		if (mpLook)
			MUD_LookRuts.NoteWait(MUD_LookRuts.W_LOST, lookCand - 1);
		else
			MUD_LookRuts.NoteWait(MUD_LookRuts.W_LOST, lookCand);
		if (mp)
		{
			int meshCost = COST_INDEX;
			if (FindIdx(mp))
				meshCost = COST_MESH;
			if (MayRun(t0, m_iMeshSkip, meshCost))
			{
				m_iMeshSkip = 0;
				int tm = System.GetTickCount();
				if (!FindIdx(mp))
				{
					if (mpLook)
						MUD_LookRuts.NoteWait(MUD_LookRuts.W_INDEX);
					IdxSet(mp);
					idxJob = true;
					MUD_Prof.Add(MUD_Prof.RB_INDEX, System.GetTickCount(tm));
					if (m_bDebug || MUD_TestKit.TEST_KIT)     // audit B9: only the 10 s worst-frame line reads it
						m_sFrameRebuild += string.Format("index %1 ", System.GetTickCount(tm));
				}
				else
				{
					bool lookMesh = mp.m_bLookDirty;
					float lookSince = mp.m_fLookSince;     // TEMP delay meter
					float lookSpeed = mp.m_fLookSpeed;
					m_bQueueMesh = true;     // TEMP split delay meter: this build came from the queue
					// REBUILD_WHY (TEMP): which dirt kinds this build shows, and the pace that picked it
					int whyKind = 0;
					if (mp.m_bWhyLook)
						whyKind = whyKind | 1;
					if (mp.m_bWhyFloor)
						whyKind = whyKind | 2;
					if (mp.m_bHMesh)
						whyKind = whyKind | 4;
					if (mp.m_bWhySweep)
						whyKind = whyKind | 8;
					bool whyPace01 = MeshEvery(mp) <= m_fMeshInterval + 0.001;
					m_bMeshGate = true;
					m_bMeshSkipped = false;
					BuildMesh(mp);
					m_bMeshGate = false;
					m_bQueueMesh = false;
					if (m_bMeshSkipped)
					{
						m_iWhySkips++;
					}
					else
					{
						m_aWhyBuild[whyKind] = m_aWhyBuild[whyKind] + 1;
						if (whyPace01)
							m_iWhyPace01++;
					}
					if (m_bMeshSkipped)
						lookMesh = false;     // PERF IDEA 2: nothing built - no look mesh / delay sample
					if (lookMesh)
						MUD_LookRuts.NoteMesh(System.GetTickCount(tm));     // look ruts 10 s line: client rebuild cost
					if (lookMesh && lookSince >= 0)
						MUD_LookRuts.NoteDelay(m_fTime - lookSince, lookSpeed);
					mp.m_fMeshAt = m_fTime;
					mp.m_fQueuedAt = m_fTime;     // test kit: waits from its last build
					if (m_bDebug || MUD_TestKit.TEST_KIT)     // audit B9: only the 10 s worst-frame line reads it
						m_sFrameRebuild += string.Format("mesh %1 ", System.GetTickCount(tm));
					MUD_Prof.Add(MUD_Prof.RB_MESH, System.GetTickCount(tm));
				}
			}
			else
			{
				if (mpLook)
					MUD_LookRuts.NoteWait(MUD_LookRuts.W_BUDGET);
				m_iMeshSkip++;
				m_iWaitMesh++;
			}
		}
		if (cp && !idxJob)
		{
			if (MayRun(t0, m_iColSkip, COST_COLLIDER))
			{
				m_iColSkip = 0;
				int tc = System.GetTickCount();
				if (!FindIdx(cp))
				{
					IdxSet(cp);
					if (m_bDebug || MUD_TestKit.TEST_KIT)     // audit B9: only the 10 s worst-frame line reads it
						m_sFrameRebuild += string.Format("index %1 ", System.GetTickCount(tc));
					MUD_Prof.Add(MUD_Prof.RB_INDEX, System.GetTickCount(tc));
				}
				else
				{
					BeforeCollider(cp);
					SmoothPatch(cp);
					int ts = System.GetTickCount(tc);
					m_bColGate = true;
					BuildCollider(cp);
					m_bColGate = false;
					MUD_Prof.Add(MUD_Prof.RB_SMOOTH, ts);
					MUD_Prof.Add(MUD_Prof.RB_COLLIDER, System.GetTickCount(tc) - ts);
					cp.m_fColAt = m_fTime;
					cp.m_fQueuedAt = m_fTime;
					if (m_bDebug || MUD_TestKit.TEST_KIT)     // audit B9: only the 10 s worst-frame line reads it
						m_sFrameRebuild += string.Format("smooth %1 collider %2 (old body %3, new body %4) ", ts, System.GetTickCount(tc) - ts, m_iLastDropMs, m_iLastStaticMs);
				}
			}
			else
			{
				m_iColSkip++;
				m_iWaitCol++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER RUSH: the crater's patches build mesh (render) and collider now, at most RUSH_MAX steps / RUSH_MS a frame,
	//! no queue pace. RUSH SPLIT: one step per pick, nearest the camera first; a patch stays in m_aRush until done.
	protected void RushTick()
	{
		int t0 = System.GetTickCount();
		int n = 0;
		int done = 0;
		while (!m_aRush.IsEmpty() && n < RUSH_MAX)
		{
			if (n > 0 && System.GetTickCount(t0) >= RUSH_MS)
				break;
			int at = RushPick();
			MUD_FinePatch p = m_mPatches.Get(m_aRush[at]);
			if (!p)
			{
				m_aRush.RemoveOrdered(at);
				continue;
			}
			if (p.m_iBuild > 0 || p.m_bAsleep)
			{
				// BUILD ONCE: a patch the crater only started (or found asleep) builds now, crater included - one build
				// step per pick (RUSH_SPLIT). Far-held (client, m_sFar) and failing patches keep their own build / wake.
				bool keep = BUILD_ONCE && p.m_iBuildFails == 0 && !m_sFar.Contains(p.m_iKey);
				if (keep && !p.m_bDeformed && !p.m_bAsleep)
					keep = false;     // lazy, untouched (blur reach only): the skin still carries it
				if (!keep)
				{
					RushDrop(at, p);
					continue;
				}
				if (p.m_bAsleep && p.m_iBuild == 0)
				{
					WakePatch(p);
					m_iWokeNow++;
				}
				if (RUSH_SPLIT)
				{
					BuildStep(p);
				}
				else
				{
					FinishNow(p);
				}
				p.m_fQueuedAt = m_fTime;
				n++;
				if (p.m_iBuild == 0 || p.m_iBuildFails > 0)
				{
					RushDrop(at, p);
					done++;
				}
				continue;
			}
			// live: collider first (bodies fall in), mesh at the next pick (RUSH_SPLIT); each once per crater mark
			if (!FindIdx(p))
				IdxSet(p);
			bool built = false;
			if ((p.m_iRushDone & RUSH_COL) == 0)
			{
				p.m_iRushDone = p.m_iRushDone | RUSH_COL;
				if (p.m_bColDirty)
				{
					BeforeCollider(p);
					SmoothPatch(p);
					m_bColGate = true;
					BuildCollider(p);
					m_bColGate = false;
					p.m_fColAt = m_fTime;
					p.m_fQueuedAt = m_fTime;
					built = true;
				}
			}
			if ((p.m_iRushDone & RUSH_MESH) == 0 && (!built || !RUSH_SPLIT))     // split: the mesh waits for the next pick
			{
				p.m_iRushDone = p.m_iRushDone | RUSH_MESH;
				if (m_bRender && p.m_bMeshDirty)
				{
					m_bMeshGate = true;     // as the queue: partial input / same-input skip allowed
					m_bMeshSkipped = false;
					BuildMesh(p);
					m_bMeshGate = false;
					p.m_fMeshAt = m_fTime;
					p.m_fQueuedAt = m_fTime;
					built = true;
				}
			}
			if (built)
				n++;
			if (p.m_iRushDone == RUSH_COL + RUSH_MESH)
			{
				RushDrop(at, p);
				done++;
			}
		}
		if (n > 0 && m_bDebug)
			Print(string.Format("MUD world: crater rush - %1 steps, %2 patches done in %3 ms, %4 left for next frame", n, done, System.GetTickCount(t0), m_aRush.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER RUSH: index in m_aRush of the next patch - nearest the camera on a drawing machine (the crater you look
	//! at first), else the oldest
	protected int RushPick()
	{
		if (!m_bRender || m_aRush.Count() < 2)
			return 0;
		vector cam[4];
		GetWorld().GetCurrentCamera(cam);
		int best = 0;
		float bestD = -1;
		for (int i = 0; i < m_aRush.Count(); i++)
		{
			int key = m_aRush[i];
			float d = PatchDist(key - (key / m_iNPX) * m_iNPX, key / m_iNPX, cam[3]);
			if (bestD < 0 || d < bestD)
			{
				bestD = d;
				best = i;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER RUSH: patch p (at index at) leaves m_aRush
	protected void RushDrop(int at, MUD_FinePatch p)
	{
		m_aRush.RemoveOrdered(at);
		p.m_bRush = false;
		p.m_iRushDone = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER HOLD: put mesh (made from res) on p now - or, while p is in the crater rush (or already holds one), keep
	//! it in p.m_HoldRes until HoldRelease. p.m_MeshRes stays the shown mesh (dropping it would drop the shown one).
	protected void ShowMesh(MUD_FinePatch p, Resource res, MeshObject mesh)
	{
		if (RUSH_HOLD && (p.m_bRush || p.m_HoldRes != null || p.m_bHoldCover))
		{
			p.m_HoldRes = res;     // a newer build replaces an older held one
			HoldNote(p);
			return;
		}
		p.m_Ent.SetObject(mesh, "");
		p.m_Ent.Update();       // fresh bounds, else culling keeps the spawn-time ones
		p.m_MeshRes = res;
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER HOLD: p has a held mesh or cover
	protected void HoldNote(MUD_FinePatch p)
	{
		if (m_aHoldKeys.Contains(p.m_iKey))
			return;
		if (m_aHoldKeys.IsEmpty())
			m_fHeldSince = m_fTime;
		m_aHoldKeys.Insert(p.m_iKey);
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER HOLD: once the rush list is empty (or after RUSH_HOLD_MAX_S), every held mesh shows and every held skin
	//! cover goes in, in this one frame
	protected void HoldRelease()
	{
		if (!m_aRush.IsEmpty() && m_fTime - m_fHeldSince < RUSH_HOLD_MAX_S)
			return;
		int t0 = System.GetTickCount();
		array<int> cxs = {};
		array<int> czs = {};
		int shown = 0;
		foreach (int key : m_aHoldKeys)
		{
			MUD_FinePatch p = m_mPatches.Get(key);
			if (!p || !p.m_Ent)
				continue;     // freed meanwhile
			if (p.m_HoldRes)
			{
				p.m_Ent.SetObject(p.m_HoldRes.GetResource().ToMeshObject(), "");
				p.m_Ent.Update();
				p.m_MeshRes = p.m_HoldRes;
				p.m_HoldRes = null;
				shown++;
			}
			if (p.m_bHoldCover)
			{
				p.m_bHoldCover = false;
				for (int lz = 0; lz < SKIN; lz++)
				{
					for (int lx = 0; lx < SKIN; lx++)
					{
						cxs.Insert(p.m_iPx * SKIN + lx);
						czs.Insert(p.m_iPz * SKIN + lz);
					}
				}
			}
		}
		int ts = System.GetTickCount();
		if (!cxs.IsEmpty())
			m_Skin.CoverCells(cxs, czs);
		m_iSwapMs += System.GetTickCount(ts);
		float heldS = m_fTime - m_fHeldSince;
		int n = m_aHoldKeys.Count();
		m_aHoldKeys.Clear();
		m_iHeldShown += shown;
		int ms = System.GetTickCount(t0);
		if (ms > m_iHeldWorstMs)
			m_iHeldWorstMs = ms;
		if (m_bDebug)
			Print(string.Format("MUD world: crater hold - %1 patches (%2 meshes, %3 skin cells covered) shown in %4 ms after %5 s", n, shown, cxs.Count(), ms, heldS));
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER DECAL, any machine that draws (Blast, before the client return): note an explosive blast off vehicles.
	//! Bullet impacts (bare warheads) and fires are left out, as in Blast.
	protected void DecalNote(IEntity pHitEntity, vector pos, IEntity damageSource, bool warhead)
	{
		if (!HF_DECAL || !m_bRender || m_aDecalPos.Count() >= 16)
			return;
		MUD_BlastInfo blast = BlastInfo(damageSource);
		if (warhead && (!blast || blast.m_fKg <= 0))
			return;
		if (blast && !blast.m_bExplosive)
			return;
		if (pHitEntity && Vehicle.Cast(pHitEntity.GetRootParent()))
			return;
		if (warhead && VehicleAt(pos))
			return;
		foreach (vector had : m_aDecalPos)
		{
			if (vector.DistanceSqXZ(had, pos) < 1)
				return;     // the same blast from the other hook
		}
		float d0 = 0;
		float dug;
		float undug;
		if (Heights(pos[0], pos[2], dug, undug))
			d0 = undug - dug;
		m_aDecalPos.Insert(pos);
		m_aDecalAt.Insert(m_fTime);
		m_aDecalD0.Insert(d0);
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER DECAL: a noted blast gets its decal once the patch under it is built with dug ground - deeper than at
	//! the note (its new crater arrived), or after 1 s any dug ground (no new crater, an older one there)
	protected void DecalTick()
	{
		for (int i = m_aDecalPos.Count() - 1; i >= 0; i--)
		{
			vector pos = m_aDecalPos[i];
			float age = m_fTime - m_aDecalAt[i];
			bool lay = false;
			float dug = 0;
			float undug = 0;
			if (DecalReady(pos) && Heights(pos[0], pos[2], dug, undug))
			{
				float d = undug - dug;
				lay = d > m_aDecalD0[i] + 0.05 || (age >= 1 && d > 0.1);
			}
			if (!lay && age < DECAL_WAIT_S)
				continue;
			m_aDecalPos.RemoveOrdered(i);
			m_aDecalAt.RemoveOrdered(i);
			m_aDecalD0.RemoveOrdered(i);
			if (lay)
				LayDecal(pos, undug, dug);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER DECAL: the patch at pos is live with its newest mesh (not building, asleep, dirty or rushed)
	protected bool DecalReady(vector pos)
	{
		if (!m_aRush.IsEmpty() || !m_aHoldKeys.IsEmpty())
			return false;     // review 2026-10-22: a crater over more patches than one frame's rush - lay it on all of them
		float fx;
		float fz;
		if (!ToFine(pos, fx, fz))
			return false;
		int v;
		MUD_FinePatch p = At(Math.Floor(fx), Math.Floor(fz), v);
		return p && p.m_Ent && p.m_iBuild == 0 && !p.m_bAsleep && !p.m_bMeshDirty && !p.m_bRush;
	}

	//------------------------------------------------------------------------------------------------
	//! CRATER DECAL: the dug radius round pos (4 directions, fine steps, DECAL_MAX_R at most), then the vanilla mortar
	//! decal onto every patch entity that radius reaches, projected down from 1 m over the undug ground
	protected void LayDecal(vector pos, float undug, float floorY)
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetWorld());
		if (!world)
			return;
		float rad = DECAL_MAX_R;
		int steps = Math.Floor(DECAL_MAX_R / m_fFine);
		for (int s = 1; s <= steps; s++)
		{
			float r = s * m_fFine;
			int deep = 0;
			for (int q = 0; q < 4; q++)
			{
				float sx = pos[0];
				float sz = pos[2];
				if (q == 0)
					sx += r;
				else if (q == 1)
					sx -= r;
				else if (q == 2)
					sz += r;
				else
					sz -= r;
				float sd;
				float su;
				if (Heights(sx, sz, sd, su) && su - sd > 0.05)
					deep++;
			}
			if (deep == 0)
			{
				rad = r;
				break;
			}
		}
		vector origin = Vector(pos[0], undug + 1, pos[2]);
		float farClip = undug + 1 - floorY + 0.5;
		float angle = Math.RandomFloat(0, 360);
		float size = Math.Max(rad * DECAL_SIZE_K, 1);
		float side = PCELLS * m_fFine;
		int px0 = Math.Floor((pos[0] - rad - m_fX0) / side);
		int px1 = Math.Floor((pos[0] + rad - m_fX0) / side);
		int pz0 = Math.Floor((pos[2] - rad - m_fZ0) / side);
		int pz1 = Math.Floor((pos[2] + rad - m_fZ0) / side);
		int ents = 0;
		for (int pz = pz0; pz <= pz1; pz++)
		{
			for (int px = px0; px <= px1; px++)
			{
				if (px < 0 || pz < 0 || px >= m_iNPX || pz >= m_iNPZ)
					continue;
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p || !p.m_Ent || !p.m_Ent.GetVObject() || p.m_iBuild > 0)
					continue;
				world.CreateDecal(p.m_Ent, origin, "0 -1 0", 0, farClip, angle, size, 1, MUD_VoxelProbe.DECAL_MAT, DECAL_LIFE_S, 0xFFFFFFFF);
				ents++;
			}
		}
		if (m_bDebug)
			Print(string.Format("MUD world: crater decal at %1, dug radius %2 m, depth %3 m, on %4 patch entities", pos, rad, undug - floorY, ents));
	}

	//------------------------------------------------------------------------------------------------
	//! Mesh queue pace of p (RUT_PACE aside). REBUILD 6 LOOK_PACE: no height dirt since the last mesh (look + rut floor
	//! only) -> LOOK_MESH_EVERY, also with collider dirt (it kept the 0.1 s pace near the wheels, research 1.2)
	protected float MeshEvery(MUD_FinePatch p)
	{
		if (LOOK_PACE && !p.m_bHMesh)
			return Math.Max(m_fMeshInterval, LOOK_MESH_EVERY);
		if (p.m_bLookDirty && !p.m_bColDirty)
			return Math.Max(m_fMeshInterval, LOOK_MESH_EVERY);     // L2: look-only dirt
		return m_fMeshInterval;
	}

	//------------------------------------------------------------------------------------------------
	//! REBUILD 6 10 s lines (in the 'MUD band' block): REBUILD_WHY kinds, coarse plate, layout cache; then reset
	protected void RebuildSixLog()
	{
		// kind bits 1 look, 2 floor, 4 height, 8 sweep: height wins, then look + floor, look, floor, sweep only, none
		int kLook = 0;
		int kFloor = 0;
		int kBoth = 0;
		int kHeight = 0;
		int kSweep = 0;
		int kOther = 0;
		int kN = 0;
		for (int k = 0; k < 16; k++)
		{
			int n = m_aWhyBuild[k];
			kN += n;
			if ((k & 4) != 0)
				kHeight += n;
			else if ((k & 3) == 3)
				kBoth += n;
			else if ((k & 1) != 0)
				kLook += n;
			else if ((k & 2) != 0)
				kFloor += n;
			else if ((k & 8) != 0)
				kSweep += n;
			else
				kOther += n;
			m_aWhyBuild[k] = 0;
		}
		int kSum = kLook + kFloor + kBoth + kHeight + kSweep + kOther;
		int lFull = 0;
		int lSlots = 0;
		int lHits = 0;
		int lBad = 0;
		int lChecks = 0;
		if (m_Band)     // null until the first band build (m_bBandMesher off: never)
		{
			lFull = m_Band.m_iLayoutFull;
			lSlots = m_Band.m_iLayoutSlots;
			lHits = m_Band.m_iLayoutHits;
			lBad = m_Band.m_iLayoutBad;
			lChecks = m_Band.m_iLayoutChecks;
			m_Band.m_iLayoutFull = 0;
			m_Band.m_iLayoutSlots = 0;
			m_Band.m_iLayoutHits = 0;
			m_Band.m_iLayoutBad = 0;
			m_Band.m_iLayoutChecks = 0;
		}
		bool hasCounts = kN + m_iWhySkips + m_iWhyAge + m_iWhyBig + m_iWhyOther + m_iPlateCoarse + m_iPlateFull + lFull + lSlots + lHits > 0;
		if (hasCounts && (m_bDebug || MUD_TestKit.TEST_KIT))     // audit C1
		{
			Print(string.Format("MUD rebuild why 10 s: builds %1 (look %2, floor %3, look+floor %4, height %5, sweep %6, other %7; sum %8 must = builds) | at 0.1 s pace %9",
				kN, kLook, kFloor, kBoth, kHeight, kSweep, kOther, kSum, m_iWhyPace01)
				+ string.Format(" | skips %1 | MeshSmall false: age %2, big %3, no mesh / building %4 | LOOK_PACE %5, VIEW_TRIM %6",
				m_iWhySkips, m_iWhyAge, m_iWhyBig, m_iWhyOther, LOOK_PACE, VIEW_TRIM));
			float plateN = Math.Max(m_iPlateCoarse + m_iPlateFull, 1.0);
			Print(string.Format("MUD rebuild 6 10 s: plate %1 - coarse plates %2 / full plates %3, plate tris per build %4 | layout split %5 - full %6, slots %7, hits %8, bad %9 (must be 0)",
				COARSE_PLATE, m_iPlateCoarse, m_iPlateFull, m_iPlateTris / plateN, MUD_BandMesher.s_bLayoutSplit, lFull, lSlots, lHits, lBad)
				+ string.Format(" (checks %1)", lChecks));
		}
		m_iWhySkips = 0;
		m_iWhyPace01 = 0;
		m_iWhyAge = 0;
		m_iWhyBig = 0;
		m_iWhyOther = 0;
		m_iPlateCoarse = 0;
		m_iPlateFull = 0;
		m_iPlateTris = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! REBUILD 6: the dirt kinds of p's last mesh are shown (called where m_bLookDirty is cleared after a build)
	protected void WhyClear(MUD_FinePatch p)
	{
		p.m_bHMesh = false;
		p.m_bWhyLook = false;
		p.m_bWhySweep = false;
		p.m_bWhyFloor = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void BeforeCollider(MUD_FinePatch p)
	{
		BreakHard(p);
	}

	//------------------------------------------------------------------------------------------------
	//! A hard cell (asphalt, concrete) whose ground moved more than 3 cm (a crater) turns into
	//! m_CraterMaterial and soft: from then on it takes ruts and the shovel (MUD_GroundEntity.c:1140).
	//! Same heights on every machine, so every machine breaks the same cells.
	protected void BreakHard(MUD_FinePatch p)
	{
		bool any;
		for (int c = 0; c < SKIN * SKIN; c++)
		{
			if (p.m_aCell[c] < 0 || p.m_aCellHard[c] == 0)
				continue;
			int lx0 = (c - (c / SKIN) * SKIN) * FINE;
			int lz0 = (c / SKIN) * FINE;
			bool broken;
			for (int vz = lz0; vz <= lz0 + FINE && !broken; vz++)
			{
				for (int vx = lx0; vx <= lx0 + FINE; vx++)
				{
					if (Math.AbsFloat(p.m_aH[vz * PVERTS + vx]) > 0.03)
					{
						broken = true;
						break;
					}
				}
			}
			if (!broken)
				continue;
			int slot = p.m_aMats.Find(m_CraterMaterial);
			if (slot < 0)
			{
				if (p.m_aMats.Count() < MAT_SLOTS)
					slot = p.m_aMats.Insert(m_CraterMaterial);
				else
					slot = 0;
			}
			int cell = slot;
			if ((p.m_aCell[c] & TLBR) != 0)
				cell = cell | TLBR;
			cell = cell | (p.m_aCell[c] & (VOX | LOOK));
			p.m_aCell[c] = cell;
			p.m_aCellHard[c] = 0;
			any = true;
		}
		if (!any)
			return;
		// this patch's softness and every neighbour's (their 3 x 3 reaches into this patch)
		for (int pz = p.m_iPz - 1; pz <= p.m_iPz + 1; pz++)
		{
			for (int px = p.m_iPx - 1; px <= p.m_iPx + 1; px++)
			{
				if (px < 0 || pz < 0 || px >= m_iNPX || pz >= m_iNPZ)
					continue;
				MUD_FinePatch n = m_mPatches.Get(pz * m_iNPX + px);
				if (n)
					ComputeSoft(n);
			}
		}
		if (m_bDebug)
			Print(string.Format("MUD world: hard ground broken in patch %1, surfaces %2", p.m_iKey, p.m_aMats.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: wp is over a hole cell of the world ground (dig helper, MUD_Dig.UpdateHelper)
	static bool IsOver(vector wp)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		return MUD_Surface.IsHole(s_Inst.GetWorld(), wp[0], wp[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: edge lock at global fine vertex gx, gz - the patch's m_aLock (kept edge + buildings), else EdgeLock
	protected float LockAt(int gx, int gz)
	{
		int px = Math.ClampInt(gx / PCELLS, 0, m_iNPX - 1);
		int pz = Math.ClampInt(gz / PCELLS, 0, m_iNPZ - 1);
		int vx = gx - px * PCELLS;
		int vz = gz - pz * PCELLS;
		MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
		if (p && vx >= 0 && vx < PVERTS && vz >= 0 && vz < PVERTS)
			return p.m_aEdgeLock[vz * PVERTS + vx];
		return EdgeLock(gx, gz);
	}

	//------------------------------------------------------------------------------------------------
	//! Ground heights at world (x, z): dug = what is there now (bilinear over the fine verts), undug =
	//! before any deformation (skin triangles). On kept terrain both are the heightmap. False off the
	//! patch grid.
	protected bool Heights(float x, float z, out float dug, out float undug)
	{
		float fx;
		float fz;
		if (!ToFine(Vector(x, 0, z), fx, fz))
			return false;
		bool ok;
		undug = BaseAt(fx, fz, ok);
		if (!ok)
		{
			undug = GetWorld().GetSurfaceY(x, z);
			dug = undug;
			return undug > -255;
		}
		int ix = Math.ClampInt(Math.Floor(fx), 0, m_iNPX * PCELLS - 1);
		int iz = Math.ClampInt(Math.Floor(fz), 0, m_iNPZ * PCELLS - 1);
		float u = Math.Clamp(fx - ix, 0, 1);
		float w = Math.Clamp(fz - iz, 0, 1);
		float h = GetH(ix, iz) * (1 - u) * (1 - w) + GetH(ix + 1, iz) * u * (1 - w) + GetH(ix, iz + 1) * (1 - u) * w + GetH(ix + 1, iz + 1) * u * w;
		dug = undug + h;
		// RUT FIELD: the rut floor under it (a crater converting a rutted column later seeds the rut, the floor stays
		// on top: the min of both)
		if (MUD_VoxelProbe.RUT_FIELD)
		{
			float dug0 = dug;
			float rf = MUD_VoxelProbe.FieldFloorAt(x, z, dug);
			if (rf < undug)
			{
				float rfL = LockAt(ix, iz) * (1 - u) * (1 - w) + LockAt(ix + 1, iz) * u * (1 - w) + LockAt(ix, iz + 1) * (1 - u) * w + LockAt(ix + 1, iz + 1) * u * w;
				rf = undug + (rf - undug) * rfL;     // edge lock, as the patch mesh / collider
			}
			if (rf < dug)
				dug = rf;
			// Build 3 Stage 3: the ridge, only ever up and only where the heightfield is at its undug height (as the
			// patches draw it), so the hole check expects it and MakeSeed carries it into new columns. FieldTopAt blends
			// each corner's floor, else its ridge, else dug0: between a rut corner and a ridge corner that is the drawn
			// slope, which the floor-only blend above puts too low. No ridge corner = FLOOR_NONE = no change
			if (MUD_VoxelProbe.RUT_RIDGES && Math.AbsFloat(h) < MUD_VoxelProbe.RUT_RIDGE_FLAT)
			{
				float rt = MUD_VoxelProbe.FieldTopAt(x, z, dug0);
				if (rt < MUD_VoxelProbe.FLOOR_NONE * 0.5)
				{
					float rgL = LockAt(ix, iz) * (1 - u) * (1 - w) + LockAt(ix + 1, iz) * u * (1 - w) + LockAt(ix, iz + 1) * (1 - u) * w + LockAt(ix + 1, iz + 1) * u * w;
					rt = undug + (rt - undug) * rgL;     // edge lock, as the patches
					if (rt > dug)
						dug = rt;
				}
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Disp hole check (MUD_VoxelProbe.DispHoles, RUT_FIELD): the patch at world (x, z) has a live collider that is
	//! up to date (built, awake, no collider rebuild owed) - else a trace onto it says nothing about the floor.
	static bool PatchColliderLive(float x, float z)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		float fx;
		float fz;
		if (!s_Inst.ToFine(Vector(x, 0, z), fx, fz))
			return false;
		int gx = Math.Floor(fx);
		int gz = Math.Floor(fz);
		int v;
		MUD_FinePatch p = s_Inst.At(gx, gz, v);
		return p && p.m_iBuild == 0 && !p.m_bAsleep && p.m_bColValid && !p.m_bColDirty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, F3 heli crash (MUD_HeliGround, 2026-10-24): an impact crater of kg TNT at wp - the blast path after its
	//! vehicle / hook checks (PoolDig: min charge, crater, net send next frame)
	static void ImpactCrater(vector wp, float kg)
	{
		if (!s_Inst || !s_Inst.m_bReady || s_Inst.IsProxy())
			return;
		s_Inst.PoolDig(wp, kg, false, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, F3 crash scrape (MUD_HeliGround, user sketch 2026-10-24): cut the ground under each world point of pts
	//! down to its y (step m per call, at most maxDepth under the undug ground, ground more than 2 m above stays), then
	//! pile the cut dirt as rough berms side m out either side of the path (dir = flat travel). Returns points that cut.
	static int CrashScrape(array<vector> pts, vector dir, float step, float maxDepth, float side)
	{
		if (!s_Inst || !s_Inst.m_bReady || s_Inst.IsProxy() || !pts || pts.IsEmpty())
			return 0;
		return s_Inst.DoCrashScrape(pts, dir, step, maxDepth, side);
	}

	//------------------------------------------------------------------------------------------------
	protected int DoCrashScrape(array<vector> pts, vector dir, float step, float maxDepth, float side)
	{
		m_fCutVol = 0;
		int cuts;
		foreach (vector p : pts)
		{
			if (Cut(p, p[1], step, maxDepth, 2, true))
				cuts++;
		}
		float vol = m_fCutVol * m_fFine * m_fFine * SPILL_SHARE;
		m_fCutVol = 0;
		if (cuts == 0 || vol <= 0.0005 || side <= 0)
			return cuts;     // side 0: no berms (heli downwash ring)
		vector right = Vector(dir[2], 0, -dir[0]);
		float each = vol / (2 * pts.Count());
		foreach (vector q : pts)
		{
			// rough edge (sketch: jagged berms both sides): each pile a random bit nearer / farther and bigger / smaller
			float jl = Math.RandomFloat(0.6, 1.4);
			float jr = Math.RandomFloat(0.6, 1.4);
			Pile(q + right * (side * jl), each * jl);
			Pile(q - right * (side * jr), each * jr);
		}
		m_iSpills++;
		m_fSpillVol += vol;
		return cuts;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, F3 heli IMPACT GOUGE (user 2026-10-24: 'the crater doesnt look like something massive hit, just an
	//! explosion'): instead of a round blast bowl, a furrow from start along dir (flat) len m long, halfW m half wide:
	//! depth under the undug ground = depth x (1 - 0.65 x along share) x (1 - across share^2) - deepest where it hit,
	//! shallowing where the wreck ploughed on. The dirt it moves: GOUGE_MOUND of it as a mound pushed ahead of the end,
	//! the rest as rough berms along both sides. Returns points that cut.
	static const float GOUGE_GRID = 0.4;
	static const float GOUGE_MOUND = 0.45;
	static int ImpactGouge(vector start, vector dir, float len, float halfW, float depth)
	{
		if (!s_Inst || !s_Inst.m_bReady || s_Inst.IsProxy())
			return 0;
		return s_Inst.DoImpactGouge(start, dir, len, halfW, depth);
	}

	//------------------------------------------------------------------------------------------------
	protected int DoImpactGouge(vector start, vector dir, float len, float halfW, float depth)
	{
		vector right = Vector(dir[2], 0, -dir[0]);
		m_fCutVol = 0;
		int cuts;
		int na = Math.Max(1, Math.Ceil(len / GOUGE_GRID));
		int nc = Math.Max(1, Math.Ceil(2 * halfW / GOUGE_GRID));
		for (int a = 0; a <= na; a++)
		{
			float fa = a;
			fa = fa / na;
			for (int c = 0; c <= nc; c++)
			{
				float fc = c;
				fc = fc / nc * 2 - 1;     // -1 .. 1 across
				float dd = depth * (1 - 0.65 * fa) * (1 - fc * fc);
				if (dd < 0.03)
					continue;
				vector wp = start + dir * (len * fa) + right * (halfW * fc);
				float undug;
				if (!UndugY(wp[0], wp[2], undug))
					continue;
				if (Cut(Vector(wp[0], undug - dd, wp[2]), undug - dd, dd + 0.1, depth + 0.1, 5, true))
					cuts++;
			}
		}
		float vol = m_fCutVol * m_fFine * m_fFine * SPILL_SHARE;
		m_fCutVol = 0;
		if (cuts == 0 || vol <= 0.001)
			return cuts;
		// mound pushed ahead of the furrow's end
		float moundR = Math.Max(1.2, halfW * 1.1);
		PileR(start + dir * (len + moundR * 0.6), vol * GOUGE_MOUND, moundR, Math.Min(1.2, depth * 0.9));
		// rough berms along both sides
		int nb = Math.Max(2, Math.Ceil(len / 1.0));
		float each = vol * (1 - GOUGE_MOUND) / (2 * nb);
		for (int b = 0; b < nb; b++)
		{
			float fb = b;
			fb = (fb + 0.5) / nb;
			vector bp = start + dir * (len * fb);
			float jl = Math.RandomFloat(0.6, 1.4);
			float jr = Math.RandomFloat(0.6, 1.4);
			float bt = Math.Min(0.6, depth * 0.5);
			PileR(bp + right * (halfW + 0.7 * jl), each * jl, 0.9, bt);
			PileR(bp - right * (halfW + 0.7 * jr), each * jr, 0.9, bt);
		}
		m_iSpills++;
		m_fSpillVol += vol;
		return cuts;
	}

	//------------------------------------------------------------------------------------------------
	//! Jitter probe (MUD_Settle, 2026-10-24): seconds since the patch collider at world (x, z) was last built.
	//! False = no live patch body there.
	static bool ColAge(float x, float z, out float age)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		float fx;
		float fz;
		if (!s_Inst.ToFine(Vector(x, 0, z), fx, fz))
			return false;
		int gx = Math.Floor(fx);
		int gz = Math.Floor(fz);
		int v;
		MUD_FinePatch p = s_Inst.At(gx, gz, v);
		if (!p || !p.m_Phys)
			return false;
		age = s_Inst.m_fTime - p.m_fColBuiltAt;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: ground height now at world (x, z) (trench ghosts, site spawn). False = unknown.
	static bool SurfaceY(float x, float z, out float y)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		float undug;
		return s_Inst.Heights(x, z, y, undug);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a soldier whose feet are at wp stands on this ground (dug or undug surface within
	//! the deepest trench + 0.25 m), not on a roof or bridge above it. Voxel cells (bricks): our heightfield there is
	//! hidden and the voxel floor can be TUNNEL_FLOOR under the undug ground (U1 2026-10-17), so the tolerance reaches that.
	static bool AtGround(vector wp)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		float dug;
		float undug;
		if (!s_Inst.Heights(wp[0], wp[2], dug, undug))
			return false;
		float tol = Math.Max(1.5, MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER + 0.25);
		// probe 2026-10-13 (bricks): voxel ground can be dug FLOOR deep (craters, tunnels) under the undug ground
		MUD_CoarseSkinEntity skin = s_Inst.m_Skin;
		if (skin)
		{
			float sx0;
			float sz0;
			int snx;
			int snz;
			skin.GetGrid(sx0, sz0, snx, snz);
			float sc = skin.GetCell();
			if (sc > 0)
			{
				int vcx = Math.Floor((wp[0] - sx0) / sc);
				int vcz = Math.Floor((wp[2] - sz0) / sc);
				if (skin.IsVoxCell(vcx, vcz))
					tol = Math.Max(tol, MUD_VoxelProbe.TUNNEL_FLOOR + 0.25);
			}
		}
		return Math.AbsFloat(wp[1] - dug) <= tol || Math.AbsFloat(wp[1] - undug) <= tol;
	}

	//------------------------------------------------------------------------------------------------
	//! probe 2026-10-12 (MUD_VoxelProbe): sink every fine vertex of the rect x0..x1, z0..z1 to depth under
	//! the undug ground (the floor caps it; locks ignored). Local only, no net. Re-sinking is cheap: only vertices
	//! that moved are marked dirty. False = a patch is missing / building, try again. shallow = smallest depth.
	static bool ProbeSink(float x0, float x1, float z0, float z1, float depth, out float shallow)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		return s_Inst.DoProbeSink(x0, x1, z0, z1, depth, shallow);
	}

	//------------------------------------------------------------------------------------------------
	protected bool DoProbeSink(float x0, float x1, float z0, float z1, float depth, out float shallow)
	{
		int gx0 = Math.ClampInt(Math.Ceil((x0 - m_fX0) / m_fFine - 0.001), 0, m_iNPX * PCELLS);
		int gx1 = Math.ClampInt(Math.Floor((x1 - m_fX0) / m_fFine + 0.001), 0, m_iNPX * PCELLS);
		int gz0 = Math.ClampInt(Math.Ceil((z0 - m_fZ0) / m_fFine - 0.001), 0, m_iNPZ * PCELLS);
		int gz1 = Math.ClampInt(Math.Floor((z1 - m_fZ0) / m_fFine + 0.001), 0, m_iNPZ * PCELLS);
		shallow = depth;
		if (gx0 > gx1 || gz0 > gz1)
			return true;    // nothing in it
		if (!EnsureRange(gx0, gx1, gz0, gz1))
			return false;
		int cx0 = gx1 + 1;
		int cx1 = gx0 - 1;
		int cz0 = gz1 + 1;
		int cz1 = gz0 - 1;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float y = Math.Max(-depth, p.m_aFloor[v]);
				if (-y < shallow)
					shallow = -y;
				if (y < p.m_aH[v])
				{
					SetH(gx, gz, y);
					cx0 = Math.Min(cx0, gx);
					cx1 = Math.Max(cx1, gx);
					cz0 = Math.Min(cz0, gz);
					cz1 = Math.Max(cz1, gz);
				}
			}
		}
		if (cx0 <= cx1)
			MarkDirty(cx0, cx1, cz0, cz1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! probe 2026-10-13 (MUD_VoxelProbe): make / finish / wake at most max patches of the rect x0..x1, z0..z1, so
	//! the sink later finds them ready (EnsureRange in the sink frame built them all at once: 323-594 ms frames, run
	//! 2026-10-13 00:38). built = patches built in this call. True = every patch there is ready.
	static bool ProbeWarm(float x0, float x1, float z0, float z1, int max, out int built)
	{
		built = 0;
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		return s_Inst.DoProbeWarm(x0, x1, z0, z1, max, built);
	}

	//------------------------------------------------------------------------------------------------
	//! probe 2026-10-13 (MUD_VoxelProbe): heightfield patch side (m) and the patch grid corner. 0 = not ready.
	static float ProbePatchGrid(out float x0, out float z0)
	{
		x0 = 0;
		z0 = 0;
		if (!s_Inst || !s_Inst.m_bReady)
			return 0;
		x0 = s_Inst.m_fX0;
		z0 = s_Inst.m_fZ0;
		return PCELLS * s_Inst.m_fFine;
	}

	//------------------------------------------------------------------------------------------------
	//! probe 2026-10-13 (MUD_VoxelProbe): ProbeSink, then the mesh + collider of every patch the rect covers past
	//! its edge are rebuilt in this call. The rebuild queue does one per frame, oldest first: a voxel crater showed
	//! piece by piece over 1-3 s (run 2026-10-13 00:43). False = a patch there is missing / building / asleep, nothing
	//! done (ProbeWarm it first). rebuilt = patches rebuilt now.
	static bool ProbeSinkNow(float x0, float x1, float z0, float z1, float depth, out float shallow, out int rebuilt)
	{
		rebuilt = 0;
		shallow = depth;
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		int built;
		if (!s_Inst.DoProbeWarm(x0, x1, z0, z1, 0, built))
			return false;
		if (!s_Inst.DoProbeSink(x0, x1, z0, z1, depth, shallow))
			return false;
		rebuilt = s_Inst.RebuildInside(x0, x1, z0, z1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Mesh + collider now for the dirty patches whose inside the rect meets (a touch on a shared edge only is left
	//! to the rebuild queue). Returns the patches rebuilt.
	protected int RebuildInside(float x0, float x1, float z0, float z1)
	{
		int gx0 = Math.ClampInt(Math.Ceil((x0 - m_fX0) / m_fFine - 0.001), 0, m_iNPX * PCELLS);
		int gx1 = Math.ClampInt(Math.Floor((x1 - m_fX0) / m_fFine + 0.001), 0, m_iNPX * PCELLS);
		int gz0 = Math.ClampInt(Math.Ceil((z0 - m_fZ0) / m_fFine - 0.001), 0, m_iNPZ * PCELLS);
		int gz1 = Math.ClampInt(Math.Floor((z1 - m_fZ0) / m_fFine + 0.001), 0, m_iNPZ * PCELLS);
		if (gx0 > gx1 || gz0 > gz1)
			return 0;
		int n;
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			if (pz * PCELLS >= gz1 || (pz + 1) * PCELLS <= gz0)
				continue;
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
			{
				if (px * PCELLS >= gx1 || (px + 1) * PCELLS <= gx0)
					continue;
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p || p.m_iBuild > 0 || p.m_bAsleep)
					continue;
				if (!p.m_bMeshDirty && !p.m_bColDirty)
					continue;
				if (m_bRender && p.m_bMeshDirty)
				{
					BuildMesh(p);
					p.m_fMeshAt = m_fTime;
				}
				if (p.m_bColDirty)
				{
					BeforeCollider(p);
					SmoothPatch(p);
					BuildCollider(p);
					p.m_fColAt = m_fTime;
				}
				n++;
			}
		}
		return n;
	}

	//------------------------------------------------------------------------------------------------
	protected bool DoProbeWarm(float x0, float x1, float z0, float z1, int max, out int built)
	{
		built = 0;
		int gx0 = Math.ClampInt(Math.Ceil((x0 - m_fX0) / m_fFine - 0.001), 0, m_iNPX * PCELLS);
		int gx1 = Math.ClampInt(Math.Floor((x1 - m_fX0) / m_fFine + 0.001), 0, m_iNPX * PCELLS);
		int gz0 = Math.ClampInt(Math.Ceil((z0 - m_fZ0) / m_fFine - 0.001), 0, m_iNPZ * PCELLS);
		int gz1 = Math.ClampInt(Math.Floor((z1 - m_fZ0) / m_fFine + 0.001), 0, m_iNPZ * PCELLS);
		if (gx0 > gx1 || gz0 > gz1)
			return true;
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (p && p.m_iBuild == 0 && !p.m_bAsleep)
					continue;
				if (!p && !HasHole(px, pz))
					continue;
				if (built >= max)
					return false;
				built++;
				p = EnsurePatch(px, pz);
				if (!p || p.m_iBuild > 0 || p.m_bAsleep)
					return false;    // collider made no body: try again next call
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server (clients from Plan 3 Task 9): sink every vertex of trench piece `piece` to layers x 0.25 m
	//! under the undug ground (spec section 5). Only lowers; locked / hard verts stay; the floor caps it.
	//! \return verts moved (0 is fine: already that deep), -1 = a patch is missing / building (swing not
	//! counted, try again), -2 = nothing in the piece can be dug (road, hard ground, locked)
	static int CarveTrench(MUD_TrenchLayout lay, int piece, int layers)
	{
		if (!s_Inst || !s_Inst.m_bReady || !lay || piece < 0 || piece >= lay.Count())
			return -1;
		if (MUD_VoxelProbe.TakesDig(lay))
		{
			// probe 2026-10-13: voxel bricks; REFUSED = not voxels (cap, seed size), the heightfield digs it
			int vr = MUD_VoxelProbe.Trench(lay, piece, layers);
			if (vr != MUD_VoxelProbe.REFUSED)
				return vr;
		}
		return s_Inst.DoCarveTrench(lay, piece, layers);
	}

	//------------------------------------------------------------------------------------------------
	//! Global fine vertex range of a trench piece's box grown by margin (m, the sloped wall). False if it
	//! is off the patch grid.
	protected bool TrenchRange(MUD_TrenchLayout lay, int piece, float margin, out int gx0, out int gx1, out int gz0, out int gz1)
	{
		vector lo;
		vector hi;
		lay.PieceBox(piece, lo, hi);
		lo = lo - Vector(margin, 0, margin);
		hi = hi + Vector(margin, 0, margin);
		gx0 = Math.ClampInt(Math.Ceil((lo[0] - m_fX0) / m_fFine), 0, m_iNPX * PCELLS);
		gx1 = Math.ClampInt(Math.Floor((hi[0] - m_fX0) / m_fFine), 0, m_iNPX * PCELLS);
		gz0 = Math.ClampInt(Math.Ceil((lo[2] - m_fZ0) / m_fFine), 0, m_iNPZ * PCELLS);
		gz1 = Math.ClampInt(Math.Floor((hi[2] - m_fZ0) / m_fFine), 0, m_iNPZ * PCELLS);
		return gx0 <= gx1 && gz0 <= gz1;
	}

	//------------------------------------------------------------------------------------------------
	protected int DoCarveTrench(MUD_TrenchLayout lay, int piece, int layers)
	{
		int gx0;
		int gx1;
		int gz0;
		int gz1;
		float depth = MUD_TrenchPlan.LAYER * layers;
		float run = depth * lay.Run();
		if (!TrenchRange(lay, piece, run, gx0, gx1, gz0, gz1))
			return -2;
		// a patch in range still missing or building: no swing, else a live edge sinks next to skin (a crack)
		if (!EnsureRange(gx0, gx1, gz0, gz1))
			return -1;

		// owned verts go to depth; verts within run outside the piece shape form the sloped wall (user
		// 2026-10-10 00:21: steep one-step walls stretched the top-down texture). Only lowers, so a deeper
		// neighbour piece keeps its depth.
		int inside;
		int diggable;
		int moved;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				float wx = m_fX0 + gx * m_fFine;
				float wz = m_fZ0 + gz * m_fFine;
				int owner = lay.PieceAt(wx, wz);
				bool isOwned = (owner == piece);
				float want = depth;
				// another piece of the same trench: left for that piece's own swings, no slope into it
				// (user 2026-10-10 01:02: the next piece sank halfway with this one)
				if (!isOwned && owner >= 0)
					continue;
				if (!isOwned)
				{
					float d = lay.OutsideDist(piece, wx, wz);
					if (d >= run)
						continue;
					want = depth - d / lay.Run();
				}
				else
				{
					inside++;
				}
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float lock = p.m_aLock[v];
				if (lock <= 0 || VertexSoft(gx, gz) <= 0)
					continue;
				if (isOwned)
					diggable++;
				float y = Math.Max(-want * lock, p.m_aFloor[v]);
				if (y < p.m_aH[v] - 0.0005)
				{
					SetH(gx, gz, y);
					moved++;
				}
			}
		}
		if (diggable == 0)
			return -2;
		if (moved > 0)
		{
			MarkDirty(gx0, gx1, gz0, gz1);
			FollowQueue(gx0, gx1, gz0, gz1, 2, 0);     // audit A1
		}
		m_iDigs++;
		if (m_bDebug)
			Print(string.Format("MUD trench: piece %1 verts %2 diggable %3 moved %4 (with slope) depth %5", piece, inside, diggable, moved, depth));
		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: wp moved onto the nearest fine grid vertex (XZ; free dig squares snap to it so they
	//! line up with each other). Unchanged when there is no world ground.
	static vector SnapFine(vector wp)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return wp;
		float fine = s_Inst.m_fFine;
		wp[0] = s_Inst.m_fX0 + Math.Round((wp[0] - s_Inst.m_fX0) / fine) * fine;
		wp[2] = s_Inst.m_fZ0 + Math.Round((wp[2] - s_Inst.m_fZ0) / fine) * fine;
		return wp;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one free dig swing (no site; user 2026-10-10 00:04): piece 0 of lay goes one layer below
	//! its shallowest diggable vertex, so a square next to a dug trench starts at the top like any other
	//! and repeated swings deepen it evenly. Max MAX_LAYERS (2.5 m).
	//! Returns verts moved, -1 = patch missing / building, -2 = nothing diggable, -3 = already full depth
	static int FreeDig(MUD_TrenchLayout lay)
	{
		if (!s_Inst || !s_Inst.m_bReady || s_Inst.IsProxy() || !lay || lay.Count() < 1)
			return -1;
		if (MUD_VoxelProbe.TakesDig(lay))
		{
			// probe 2026-10-13: voxel bricks; REFUSED = not voxels (cap, seed size), the heightfield digs it
			int vr = MUD_VoxelProbe.FreeDig(lay);
			if (vr != MUD_VoxelProbe.REFUSED)
				return vr;
		}
		int layers = s_Inst.FreeLayers(lay);
		if (layers < 0)
			return layers;
		// the 2 m square is 2 pieces of the layout: both to the same depth
		int moved;
		for (int i = 0; i < lay.Count(); i++)
		{
			int r = s_Inst.DoCarveTrench(lay, i, layers);
			if (r == -1)
				return -1;
			if (r > 0)
				moved += r;
		}
		if (s_Inst.m_bDebug)
			Print(string.Format("MUD free dig: at %1, width %2 m, layer %3 / %4, moved %5", s_Inst.FreeMiddle(lay), lay.Half() * 2, layers, MUD_TrenchPlan.MAX_LAYERS, moved));
		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one fill swing raises the square one 0.25 m layer (user 2026-10-10 00:45, was the whole
	//! hole at once). Every vertex of lay comes up to the new floor. Around it, a vertex no deeper than
	//! this hole's own sloped wall gets the wall of the shallower hole; a deeper one (a neighbour hole)
	//! gets a ~60 deg shoulder down from the new floor's edge, so the neighbour stays open. Only raises,
	//! never above the undug ground.
	//! Returns verts moved, -1 = patch missing / building, -2 = nothing dug in the square
	static int FreeFill(MUD_TrenchLayout lay)
	{
		if (!s_Inst || !s_Inst.m_bReady || s_Inst.IsProxy() || !lay || lay.Count() < 1)
			return -1;
		if (MUD_VoxelProbe.TakesDig(lay))
		{
			// probe 2026-10-13: voxel bricks; REFUSED = not voxels (cap, seed size), the heightfield fills it
			int vr = MUD_VoxelProbe.FreeFill(lay);
			if (vr != MUD_VoxelProbe.REFUSED)
				return vr;
		}
		return s_Inst.DoFreeFill(lay);
	}

	//------------------------------------------------------------------------------------------------
	protected int DoFreeFill(MUD_TrenchLayout lay)
	{
		float margin = MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER * lay.Run();
		int gx0 = 1000000000;
		int gx1 = -1;
		int gz0 = 1000000000;
		int gz1 = -1;
		for (int i = 0; i < lay.Count(); i++)
		{
			int ax0;
			int ax1;
			int az0;
			int az1;
			if (!TrenchRange(lay, i, margin, ax0, ax1, az0, az1))
				continue;
			if (ax0 < gx0)
				gx0 = ax0;
			if (ax1 > gx1)
				gx1 = ax1;
			if (az0 < gz0)
				gz0 = az0;
			if (az1 > gz1)
				gz1 = az1;
		}
		if (gx1 < gx0 || gz1 < gz0)
			return -2;
		if (!EnsureRange(gx0, gx1, gz0, gz1))
			return -1;

		// how deep this hole is: its deepest vertex (unscaled by the vertex's lock)
		float holeDepth;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				if (lay.PieceAt(m_fX0 + gx * m_fFine, m_fZ0 + gz * m_fFine) < 0)
					continue;
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p || p.m_aLock[v] <= 0)
					continue;
				holeDepth = Math.Max(holeDepth, -p.m_aH[v] / p.m_aLock[v]);
			}
		}
		if (holeDepth < 0.002)
			return -2;

		// one layer per swing (user 2026-10-10 00:45: filling it all at once traps whoever stands in it)
		int layersLeft = Math.Ceil(holeDepth / MUD_TrenchPlan.LAYER - 0.01) - 1;
		if (layersLeft < 0)
			layersLeft = 0;
		float newDepth = layersLeft * MUD_TrenchPlan.LAYER;

		int moved;
		for (int rz = gz0; rz <= gz1; rz++)
		{
			for (int rx = gx0; rx <= gx1; rx++)
			{
				int rv;
				MUD_FinePatch rp = At(rx, rz, rv);
				if (!rp)
					continue;
				float lock = rp.m_aLock[rv];
				float h = rp.m_aH[rv];
				if (lock <= 0 || h >= -0.0005)
					continue;
				float wx = m_fX0 + rx * m_fFine;
				float wz = m_fZ0 + rz * m_fFine;
				float y = -newDepth * lock;
				if (lay.PieceAt(wx, wz) < 0)
				{
					float d = margin;
					for (int k = 0; k < lay.Count(); k++)
					{
						d = Math.Min(d, lay.OutsideDist(k, wx, wz));
					}
					if (d >= margin)
						continue;
					// this hole's own wall: the wall of the shallower hole; deeper (a neighbour hole): a
					// shoulder down from the square's new floor edge
					float wall = Math.Max(0, holeDepth - d / lay.Run());
					if (-h / lock > wall + 0.01)
						y = -(newDepth + d / lay.Run()) * lock;
					else
						y = -Math.Max(0, newDepth - d / lay.Run()) * lock;
				}
				if (y > h + 0.0005)
				{
					SetH(rx, rz, y);
					moved++;
				}
			}
		}
		if (moved > 0)
		{
			MarkDirty(gx0, gx1, gz0, gz1);
			FollowQueue(gx0, gx1, gz0, gz1, 3, holeDepth - newDepth);     // audit A1: a fill lifts items back on top
		}
		m_iDigs++;
		if (m_bDebug)
			Print(string.Format("MUD fill: at %1, was %2 m deep, now %3 m, moved %4", FreeMiddle(lay), holeDepth, newDepth, moved));
		return moved;
	}

	//------------------------------------------------------------------------------------------------
	protected vector FreeMiddle(MUD_TrenchLayout lay)
	{
		return (lay.PieceEdge(0, 0, 0) + lay.PieceEdge(lay.Count() - 1, 1, 0)) * 0.5;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers the next free dig swing digs to: 1 + the layers the square's shallowest diggable,
	//! not-yet-floored vertex already has. -1 / -2 / -3 as FreeDig.
	protected int FreeLayers(MUD_TrenchLayout lay)
	{
		int fewest = MUD_TrenchPlan.MAX_LAYERS;
		int diggable;
		for (int piece = 0; piece < lay.Count(); piece++)
		{
			int gx0;
			int gx1;
			int gz0;
			int gz1;
			if (!TrenchRange(lay, piece, 0, gx0, gx1, gz0, gz1))
				continue;
			if (!EnsureRange(gx0, gx1, gz0, gz1))
				return -1;

			for (int gz = gz0; gz <= gz1; gz++)
			{
				for (int gx = gx0; gx <= gx1; gx++)
				{
					if (lay.PieceAt(m_fX0 + gx * m_fFine, m_fZ0 + gz * m_fFine) != piece)
						continue;
					int v;
					MUD_FinePatch p = At(gx, gz, v);
					if (!p)
						continue;
					float lock = p.m_aLock[v];
					if (lock <= 0 || VertexSoft(gx, gz) <= 0)
						continue;
					diggable++;
					float h = p.m_aH[v];
					// at the map floor / depth box cap: this vertex can go no deeper, it does not hold the square back
					if (h <= p.m_aFloor[v] + 0.002)
						continue;
					int has = Math.Floor(-h / (lock * MUD_TrenchPlan.LAYER) + 0.01);
					if (has < fewest)
						fewest = has;
				}
			}
		}
		if (diggable == 0)
			return -2;
		if (fewest >= MUD_TrenchPlan.MAX_LAYERS)
			return -3;
		// a bump (spoil, rut ridge) counts as 0 layers: the swing cuts it flat and sinks the rest one layer
		if (fewest < 0)
			fewest = 0;
		return fewest + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Debug: every piece's verts against its target (layers[piece] x 0.25 m; 0 = not dug). One line:
	//! 'MUD trench check <tag>: inside N, at depth D, deeper K, locked L, undug U, no patch M, WRONG W,
	//! outside dug O'. WRONG = a diggable vertex shallower than its target; outside dug = a vertex in no
	//! piece that is more than 1 cm below the deepest sloped wall any piece allows there (a crater / dig
	//! there, or a spill: should be 0 on fresh ground; may count a vertex twice where boxes overlap).
	static void CheckTrench(MUD_TrenchLayout lay, array<int> layers, string tag)
	{
		if (!s_Inst || !s_Inst.m_bReady || !lay)
			return;
		s_Inst.DoCheckTrench(lay, layers, tag);
	}

	//------------------------------------------------------------------------------------------------
	//! Deepest height (m, <= 0, before lock fade) the sloped walls of the dug pieces allow at (x, z)
	protected static float SlopeAllowed(MUD_TrenchLayout lay, array<int> layers, float x, float z)
	{
		float deepest = 0;
		for (int i = 0; i < lay.Count() && i < layers.Count(); i++)
		{
			float depth = MUD_TrenchPlan.LAYER * layers[i];
			float want = depth - lay.OutsideDist(i, x, z) / lay.Run();
			if (want > deepest)
				deepest = want;
		}
		return -deepest;
	}

	//------------------------------------------------------------------------------------------------
	protected void DoCheckTrench(MUD_TrenchLayout lay, array<int> layers, string tag)
	{
		int inside;
		int atDepth;
		int deeper;
		int locked;
		int undug;
		int noPatch;
		int wrong;
		int outside;
		for (int piece = 0; piece < lay.Count(); piece++)
		{
			int gx0;
			int gx1;
			int gz0;
			int gz1;
			if (!TrenchRange(lay, piece, MUD_TrenchPlan.LAYER * MUD_TrenchPlan.MAX_LAYERS * lay.Run(), gx0, gx1, gz0, gz1))
				continue;
			int want = 0;
			if (piece < layers.Count())
				want = layers[piece];
			for (int gz = gz0; gz <= gz1; gz++)
			{
				for (int gx = gx0; gx <= gx1; gx++)
				{
					int owner = lay.PieceAt(m_fX0 + gx * m_fFine, m_fZ0 + gz * m_fFine);
					int v;
					MUD_FinePatch p = At(gx, gz, v);
					if (owner < 0)
					{
						if (p && p.m_aH[v] < -0.01 && p.m_aH[v] < SlopeAllowed(lay, layers, m_fX0 + gx * m_fFine, m_fZ0 + gz * m_fFine) * p.m_aLock[v] - 0.01)
							outside++;
						continue;
					}
					if (owner != piece)
						continue;
					inside++;
					if (want <= 0)
					{
						undug++;
						continue;
					}
					if (!p)
					{
						noPatch++;
						continue;
					}
					float lock = p.m_aLock[v];
					if (lock <= 0 || VertexSoft(gx, gz) <= 0)
					{
						locked++;
						continue;
					}
					float target = Math.Max(-MUD_TrenchPlan.LAYER * want * lock, p.m_aFloor[v]);
					float h = p.m_aH[v];
					if (Math.AbsFloat(h - target) <= 0.002)
					{
						atDepth++;
					}
					else if (h < target)
					{
						deeper++;
					}
					else
					{
						wrong++;
						if (wrong <= 3)
							Print(string.Format("MUD trench check %1: WRONG piece %2 vertex %3 %4 h %5 target %6", tag, piece, gx, gz, h, target), LogLevel.WARNING);
					}
				}
			}
		}
		Print(string.Format("MUD trench check %1: inside %2, at depth %3, deeper %4, locked %5, undug %6, no patch %7, WRONG %8, outside dug %9",
			tag, inside, atDepth, deeper, locked, undug, noPatch, wrong, outside));
	}

	//------------------------------------------------------------------------------------------------
	//! Old hook: SCR_ExplosionAmmoEffect, only on 13 vanilla ammo (40 mm, rockets, mortars; pak 2026-10-10).
	//! Kept as a fallback until the warhead hook (every explosion) has been seen to fire.
	protected void OnExplosion(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)
	{
		if (m_bWarheadHook)
			return;
		Blast(pHitEntity, outMat[0], damageSource, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Every warhead that is spawned (MUD_WarheadComponent on vanilla Warhead_Base.et): ammo warheads,
	//! hand grenades, mines, charges, vehicle blasts, fires - vanilla and mods that inherit it.
	void OnWarheadBlast(IEntity warhead)
	{
		if (!warhead)
			return;
		m_bWarheadHook = true;
		// world position: a warhead spawned as a child (attached to its charge) has a LOCAL GetOrigin
		vector wm[4];
		warhead.GetWorldTransform(wm);
		Blast(null, wm[3], warhead, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void Blast(IEntity pHitEntity, vector pos, IEntity damageSource, bool warhead)
	{
		if (!m_bReady)
			return;
		DecalNote(pHitEntity, pos, damageSource, warhead);     // CRATER DECAL: every machine that draws
		// clients get craters from the server
		if (IsProxy())
			return;
		MUD_BlastInfo blast = BlastInfo(damageSource);
		// warhead with no charge: bullet impacts (Ammo_Bullet_Base spawns a bare Warhead_Base), smoke,
		// flares. Silent, it happens on every shot. Only the old ammo hook may use the fallback charge.
		if (warhead && (!blast || blast.m_fKg <= 0))
			return;
		// fire: counted, not printed (BlastInfo printed it once)
		if (blast && !blast.m_bExplosive)
		{
			m_iCraterSkips++;
			return;
		}
		// the same explosion seen by the other hook (first blast only, before the ammo hook stands down)
		int now = System.GetTickCount();
		if (warhead != m_bLastBlastWarhead && now - m_iLastBlastTick < 300 && vector.Distance(pos, m_vLastBlast) < 1.5)
			return;
		m_vLastBlast = pos;
		m_iLastBlastTick = now;
		m_bLastBlastWarhead = warhead;
		m_aCoverPos[m_iCoverNext] = pos;
		m_aCoverTick[m_iCoverNext] = now;
		m_iCoverNext = (m_iCoverNext + 1) % 8;
		vector outMat[3];
		outMat[0] = pos;
		float kg = m_fCraterFallbackTnt;
		bool heat;
		if (blast)
		{
			if (blast.m_fKg > 0)
				kg = blast.m_fKg;
			heat = blast.m_bHeat;
		}
		MUD_VoxelHouse.Blast(outMat[0], kg);    // TEMP probe 2026-10-13: voxel house (SP)
		MUD_HouseCopyProbe.Blast(outMat[0], kg);    // TEMP probe 2026-10-25: house copy holes (SP, TEST_KIT)
		// any blast at a vehicle (wreck too): no pit. Hits spend themselves on the vehicle; its own ammo rack /
		// fuel blast made only a small spot under it (user test 2026-10-10 22:31, also with depth x 0.3) -
		// whether vehicle blasts deform the ground is decided later. AT mines under a vehicle skip too.
		// The warhead hook has no hit entity: a vehicle within 0.5 m of the blast counts as hit.
		bool projectile = true;
		if (damageSource && !damageSource.FindComponent(ProjectileMoveComponent))
			projectile = false;
		bool onVehicle;
		if (pHitEntity)
			onVehicle = Vehicle.Cast(pHitEntity.GetRootParent()) != null;
		else if (warhead)
			onVehicle = VehicleAt(pos);
		if (onVehicle)
		{
			m_iCraterSkips++;
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (vehicle, projectile %1, %2 kg) at %3", projectile, kg, outMat[0]));
			return;
		}
		// shells, grenades, mines, rockets: the crater at once (user DS 2026-10-22: the pool's 100 ms showed as a late crater)
		if (!blast || !blast.m_bCharge)
		{
			PoolDig(outMat[0], kg, heat, 1);
			return;
		}
		// charges pooled: the min-charge test and the crater come in PoolTick, on the summed kg
		for (int q = 0; q < m_aPoolPos.Count(); q++)
		{
			if (now - m_aPoolAt[q] > POOL_MS || vector.Distance(m_aPoolPos[q], outMat[0]) > POOL_R)
				continue;
			m_aPoolKg[q] = m_aPoolKg[q] + kg;
			if (!heat)
				m_aPoolHeat[q] = 0;
			m_aPoolN[q] = m_aPoolN[q] + 1;
			return;
		}
		m_aPoolPos.Insert(outMat[0]);
		m_aPoolKg.Insert(kg);
		int heatI = 0;
		if (heat)
			heatI = 1;
		m_aPoolHeat.Insert(heatI);
		m_aPoolAt.Insert(now);
		m_aPoolN.Insert(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, each frame with pooled blasts: a pool older than POOL_MS digs ONE crater with its summed kg
	protected void PoolTick()
	{
		int now = System.GetTickCount();
		for (int q = m_aPoolPos.Count() - 1; q >= 0; q--)
		{
			if (now - m_aPoolAt[q] <= POOL_MS)
				continue;
			vector pos = m_aPoolPos[q];
			float kg = m_aPoolKg[q];
			bool heat = m_aPoolHeat[q] != 0;
			int n = m_aPoolN[q];
			m_aPoolPos.Remove(q);
			m_aPoolKg.Remove(q);
			m_aPoolHeat.Remove(q);
			m_aPoolAt.Remove(q);
			m_aPoolN.Remove(q);
			PoolDig(pos, kg, heat, n);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one crater for n blasts (n > 1 = pooled charges) with kg TNT in all, unless under the minimum charge
	protected void PoolDig(vector pos, float kg, bool heat, int n)
	{
		if (n > 1 && m_bDebug)
			Print(string.Format("MUD world: %1 blasts pooled at %2 - %3 kg TNT in one crater", n, pos, kg));
		if (kg <= m_fCraterMinTnt)
		{
			m_iCraterSkips++;
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (charge %1 kg <= min %2) at %3", kg, m_fCraterMinTnt, pos));
			return;
		}
		if (MUD_TestKit.TEST_KIT) MUD_CraterTrace.SrvHit(pos, kg);     // stage 2 B1 crater log
		int t0 = System.GetTickCount();
		Crater(pos, kg, heat);
		m_fSinceNet = m_fColliderInterval;     // send it next frame, not up to 0.2 s later (user DS 2026-10-22)
		int ms = System.GetTickCount(t0);
		m_iCraterMs += ms;
		if (ms > m_iCraterWorstMs)
			m_iCraterWorstMs = ms;
	}

	//------------------------------------------------------------------------------------------------
	//! Local player only, every frame: RescueSunk with this and last frame's position.
	protected vector m_vRescueLast;
	protected void GroundRescue()
	{
		ChimeraCharacter ch = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!ch || ch.IsInVehicle())
			return;
		vector o = ch.GetOrigin();
		vector last = m_vRescueLast;
		m_vRescueLast = o;
		// probe 2026-10-13 (MUD_VoxelProbe overlay): a voxel tile carries the ground here, our hidden heightfield
		// is not it (a walk down a voxel crater crossed its line)
		float rfx;
		float rfz;
		if (ToFine(o, rfx, rfz) && m_Skin.IsVoxCell(Math.Floor(rfx / FINE), Math.Floor(rfz / FINE)))
		{
			m_bRescueArmed = false;
			return;
		}
		float dug;
		float undug;
		if (Heights(o[0], o[2], dug, undug))
			RescueSunk(ch, o, last, dug);
	}

	//------------------------------------------------------------------------------------------------
	//! Local player sunk through our ground: lift back on top. The vanilla prone collider is a box 0.6 x
	//! 0.4 x 1.8 m (our Character_Base.et override: 0.6 long, which fixed it); on a crater wall the physics
	//! shoved it into our thin
	//! trimesh (fall probe 2026-10-10 23:28: prone 0.30 m under the collider, 1.09 m shove in one frame,
	//! then 3.6 m under the map). Terrain pushes anything below it back up, a trimesh does not. Owner
	//! side, as vanilla fast travel (SCR_Global.TeleportLocalPlayer). Only over a built patch body.
	protected int m_iRescueTick;
	protected bool m_bRescueArmed;
	protected int m_iRescues;
	protected void RescueSunk(ChimeraCharacter ch, vector o, vector last, float dug)
	{
		CharacterControllerComponent ctl = ch.GetCharacterController();
		float coll;
		if (!ctl || ctl.GetLifeState() != ECharacterLifeState.ALIVE || !ColliderAt(o, coll))
		{
			m_bRescueArmed = false;
			return;
		}
		float line = Math.Min(dug, coll) - RESCUE_DEPTH;
		// only on the frame the feet cross the line, from a short step: someone already down there
		// (cellar, bunker under our ground) or teleported / spawned there stays. Armed = last frame was
		// above the line AT ITS OWN SPOT (a shove into the wall moves the line up with it: 00:03:50 drop
		// went 38.36 -> 37.58 while the line rose 38.19 -> 38.71, so the old 'last above this line' test missed)
		bool armed = m_bRescueArmed;
		m_bRescueArmed = o[1] > line;
		if (m_bRescueArmed || !armed || vector.Distance(o, last) > 3)
			return;
		int now = System.GetTickCount();
		if (now - m_iRescueTick < 1000)
			return;
		m_iRescueTick = now;
		vector to = o;
		to[1] = Math.Max(dug, coll) + 0.1;
		if (!SCR_Global.TeleportLocalPlayer(to))
			return;
		m_iRescues++;
		Print(string.Format("MUD fall: rescued (%1) from %2 m under our ground at %3", m_iRescues, Math.Max(dug, coll) - o[1], o), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Collider height (blurred) at wp, bilinear over the 4 fine vertices around it. False if one of
	//! them has no patch with a physics body.
	protected bool ColliderAt(vector wp, out float h)
	{
		float fx;
		float fz;
		if (!ToFine(wp, fx, fz))
			return false;
		int gx = Math.Floor(fx);
		int gz = Math.Floor(fz);
		float tx = fx - gx;
		float tz = fz - gz;
		float c[4];
		for (int k = 0; k < 4; k++)
		{
			int v;
			MUD_FinePatch p = At(gx + (k & 1), gz + (k >> 1), v);
			if (!p || !p.m_Phys)
				return false;
			c[k] = p.m_aBase[v] + p.m_aSmooth[v];
		}
		h = Math.Lerp(Math.Lerp(c[0], c[1], tx), Math.Lerp(c[2], c[3], tx), tz);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: our ground lies between a recent blast and this hit (MUD_BlastCover.c). The engine skips our
	//! ground for blast cover (fragments 5-10 m through crater walls, probe 2026-10-10 23:02-23:13; FireGeo
	//! and STATIC tried, no change). The blast is the recent one (1 s) the hit direction points away from
	//! (fragment lines are instant and straight: hit dir matched blast -> hit to 0.01). Trace from 0.1 m over
	//! it, as the engine's line starts, to the hit; only our patch / skin entities count.
	//! minDist: closer hits always land (blast NoTrace part reaches 1-3 m through any wall, pak).
	static bool GroundCovers(IEntity victim, vector hit, vector dir, float minDist = 0.5)
	{
		if (!s_Inst || !s_Inst.m_bReady || dir == vector.Zero)
			return false;
		return s_Inst.DoGroundCovers(victim, hit, dir.Normalized(), minDist);
	}

	//------------------------------------------------------------------------------------------------
	protected bool DoGroundCovers(IEntity victim, vector hit, vector dir, float minDist)
	{
		int now = System.GetTickCount();
		for (int i = 0; i < 8; i++)
		{
			if (now - m_aCoverTick[i] > 1000)
				continue;
			vector d = hit - m_aCoverPos[i];
			float dist = d.Length();
			if (dist < minDist || dist > 30 || vector.Dot(d / dist, dir) < 0.99)
				continue;
			m_CoverTrace.Start = m_aCoverPos[i] + "0 0.1 0";
			m_CoverTrace.End = hit;
			m_CoverTrace.Flags = TraceFlags.ENTS;
			m_CoverTrace.LayerMask = EPhysicsLayerPresets.Projectile;
			m_CoverTrace.Exclude = victim;
			m_CoverTrace.TraceEnt = null;
			float frac = GetWorld().TraceMove(m_CoverTrace, null);
			if (frac >= 1)
				return false;
			IEntity e = m_CoverTrace.TraceEnt;
			if (!MUD_FinePatchEntity.Cast(e) && !MUD_CoarseChunkEntity.Cast(e) && !MUD_VoxelSubEntity.Cast(e))
				return false;
			// the line must enter our ground from the open side: a blast under the (blurred) collider would
			// otherwise "hide" everyone around it behind the surface it went off under
			if (vector.Dot(m_CoverTrace.TraceNorm, dir) >= 0)
				return false;
			m_iCoverDropped++;
			// one line per soldier and blast, not one per hit zone
			if (m_bDebug && (victim != m_CoverPrintVictim || i != m_iCoverPrintBlast || now - m_iCoverPrintTick > 1000))
			{
				m_CoverPrintVictim = victim;
				m_iCoverPrintBlast = i;
				m_iCoverPrintTick = now;
				Print(string.Format("MUD cover: hit on %1 stopped by our ground (blast %2 m away, line blocked at %3, normal %4)", victim, dist, frac, m_CoverTrace.TraceNorm));
			}
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True if a vehicle (or a part of one) touches 0.5 m around pos.
	protected bool VehicleAt(vector pos)
	{
		m_bNearVehicle = false;
		GetWorld().QueryEntitiesBySphere(pos, 0.5, VehicleHit);
		return m_bNearVehicle;
	}

	//------------------------------------------------------------------------------------------------
	protected bool VehicleHit(IEntity e)
	{
		if (!e || !Vehicle.Cast(e.GetRootParent()))
			return true;
		m_bNearVehicle = true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! What blew up, cached per prefab: the warhead (warhead hook) or the projectile (old ammo hook;
	//! shipped SCR_FireSupportRequestedTaskEntity.c:51-64 matches it against Ammo_* prefabs). null = unreadable (caller uses m_fCraterFallbackTnt).
	protected MUD_BlastInfo BlastInfo(IEntity src)
	{
		if (!src)
			return null;
		EntityPrefabData data = src.GetPrefabData();
		if (!data)
			return null;
		ResourceName prefab = data.GetPrefabName();
		MUD_BlastInfo info;
		if (m_mBlast.Find(prefab, info))
			return info;
		info = new MUD_BlastInfo();
		ReadBlast(prefab, info, 0);
		info.m_bCharge = NameInChain(prefab, "explosivecharge");    // Warhead_ExplosiveCharge_M112 / _TNT400g (Exp 1.9 pak)
		m_mBlast.Insert(prefab, info);
		if (m_bDebug)
			Print(string.Format("MUD world: blast %1 kg TNT (TntEquivalent guessed %2), HEAT %3, explosive %4 for %5",
				info.m_fKg, info.m_bTntGuess, info.m_bHeat, info.m_bExplosive, prefab));
		return info;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the prefab's trigger effects: an ExplosionDamageContainer gives the charge (ChargeWeight g
	//! x TntEquivalent / 1000, values inherited) and whether it is explosive at all; a penetrator
	//! SubmunitionEffect marks HEAT. No charge here but an ExplosionEffect = ammo or mine: follow its
	//! EffectPrefab (the warhead). Same layout for every vanilla and modded explosive (pak 2026-10-10:
	//! ammo / mines -> warhead, M112 / vehicle blasts / fires hold the container themselves).
	protected static void ReadBlast(ResourceName prefab, MUD_BlastInfo info, int level)
	{
		if (prefab.IsEmpty() || level > 2)
			return;
		Resource res = Resource.Load(prefab);     // held while its containers are read
		BaseContainerList effects = TriggerEffects(res);
		if (!effects)
			return;
		ResourceName next;
		for (int i = 0, n = effects.Count(); i < n; i++)
		{
			BaseContainer c = effects.Get(i);
			if (!c)
				continue;
			string cname = c.GetClassName();
			if (cname == "ExplosionEffect")
			{
				if (next.IsEmpty())
					c.Get("EffectPrefab", next);
				continue;
			}
			if (cname == "SubmunitionEffect")
			{
				ResourceName sub;
				c.Get("Prefab", sub);
				if (!info.m_bHeat && IsPenetrator(sub))
					info.m_bHeat = true;
				continue;
			}
			typename cls = cname.ToType();
			if (info.m_fKg > 0 || !cls || !cls.IsInherited(ExplosionDamageContainer))
				continue;
			// fire warheads set no ChargeWeight (Warhead_Fire_Base, pak 2026-10-10): decide here, or the
			// caller's fallback charge would dig a crater under every burning tire
			if (!HasBlast(c))
			{
				info.m_bExplosive = false;
				continue;
			}
			float grams;
			float factor;
			c.Get("ChargeWeight", grams);
			c.Get("TntEquivalent", factor);
			if (grams <= 0)
				continue;
			// O-832DU, TM-62M, ammo racks set none: the engine default is unknown, TNT itself assumed
			if (factor <= 0)
			{
				factor = 1;
				info.m_bTntGuess = true;
			}
			info.m_fKg = grams * factor / 1000;
			info.m_bExplosive = true;
		}
		if (info.m_fKg <= 0 && info.m_bExplosive)
			ReadBlast(next, info, level + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! True if the container deals explosive damage (fire containers only have FIRE / INCENDIARY). If no
	//! DamageType can be read at all, true: never lose craters to an unreadable list.
	protected static bool HasBlast(BaseContainer container)
	{
		BaseContainerList list = container.GetObjectArray("ExplosionEffects");
		if (!list)
			return true;
		bool readAny;
		for (int i = 0, n = list.Count(); i < n; i++)
		{
			BaseContainer x = list.Get(i);
			if (!x)
				continue;
			int type;
			if (!x.Get("DamageType", type))
				continue;
			readAny = true;
			float value;
			x.Get("DamageValue", value);
			if (type == EDamageType.EXPLOSIVE && value > 0)
				return true;
		}
		// unproven that an enum reads into an int: if not, fires (4-35 kg) would crater every 2 s. Say so loudly.
		if (!readAny && list.Count() > 0)
			Print("MUD world: could not read DamageType of an explosion - fires will dig craters (HasBlast)", LogLevel.WARNING);
		return !readAny;
	}

	//------------------------------------------------------------------------------------------------
	//! Full depth (m) on flat ground for a charge: DepthPerKg x kg ^ TntPower up to the knee
	//! (m_fCraterMaxDepth), past it the cube root (crater volume ~ charge), never over m_fCraterHugeDepth.
	protected float FullDepth(float kg, bool heat)
	{
		kg = Math.Max(kg, 0);
		float perKg = Math.Max(m_fCraterDepthPerKg, 0.01);
		float power = Math.Max(m_fCraterTntPower, 0.05);
		float knee = Math.Pow(m_fCraterMaxDepth / perKg, 1 / power);
		float d;
		if (kg <= knee)
			d = perKg * Math.Pow(kg, power);
		else
			d = m_fCraterMaxDepth * Math.Pow(kg / knee, 1 / 3.0);
		if (heat)
			d *= Math.Max(m_fCraterHeatFactor, 0);
		return Math.Min(d, m_fCraterHugeDepth);
	}

	//------------------------------------------------------------------------------------------------
	//! True if the prefab or one of its parents has "penetrator" in its name (vanilla
	//! Ammo_Penetrator_Base.et and every shipped child; modded ones that inherit it count too).
	protected static bool IsPenetrator(ResourceName prefab)
	{
		return NameInChain(prefab, "penetrator");
	}

	//------------------------------------------------------------------------------------------------
	//! True if the prefab or one of its parents has part (lower case) in its file name
	protected static bool NameInChain(ResourceName prefab, string part)
	{
		if (prefab.IsEmpty())
			return false;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return false;
		BaseResourceObject obj = res.GetResource();
		if (!obj)
			return false;
		BaseContainer c = obj.ToBaseContainer();
		while (c)
		{
			string name = c.GetResourceName();
			name.ToLower();
			if (name.Contains(part))
				return true;
			c = c.GetAncestor();
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! PROJECTILE_EFFECTS of the prefab's trigger (CollisionTriggerComponent on ammo, BaseTriggerComponent
	//! on warheads). Valid only while res lives.
	protected static BaseContainerList TriggerEffects(Resource res)
	{
		IEntityComponentSource trigger = SCR_BaseContainerTools.FindComponentSource(res, BaseTriggerComponent);
		if (!trigger)
			return null;
		return trigger.GetObjectArray("PROJECTILE_EFFECTS");
	}

	//------------------------------------------------------------------------------------------------
	//! Build 1 (C2 + C6, 2026-10-17): a voxel crater with the heightfield's variation (Depth Jitter, Shape Jitter) and
	//! stack cap (Stack Depth / Factor, Deepen Gain), rolled HERE on the server and carried in the op - clients replay.
	//! False = not taken as voxels (the heightfield digs it).
	protected bool VoxelCrater(vector wp, float full)
	{
		float depth = full * (1 - m_fCraterDepthJitter * Math.RandomFloat01());     // only smaller (2026-10-21)
		float cap = Math.Min(m_fCraterStackDepth, full * m_fCraterStackFactor);
		// own share per crater (user 2026-10-17 'more variation'): some near round, some strong oval / lobes
		float stretch = Math.Clamp(m_fCraterShapeJitter * Math.RandomFloat(0.2, 1), 0, 0.5);
		float lobeW = Math.RandomFloat01();
		float turn2 = Math.RandomFloat(0, Math.PI2);
		float turn3 = Math.RandomFloat(0, Math.PI2);
		return MUD_VoxelProbe.Blast(wp, depth, cap, m_fCraterDeepenGain, stretch, lobeW, turn2, turn3);
	}

	//------------------------------------------------------------------------------------------------
	//! Cosine bowl from -depth at the centre up to the rim crest (crest radius), cosine fall to 0 at
	//! the outer edge: no step anywhere, walls no steeper than m_fCraterMaxSlope before the collider
	//! blur (model + numbers: Tools/crater_profile.py). Depth from the warhead's TNT (kg); blasts up
	//! in the air shrink it, none at m_fCraterAirNone or higher. A hit on dug ground digs deeper,
	//! capped (M2). The rim only builds on undug ground (M1). Explosions crater hard ground too; the
	//! locked border and buildings scale it down.
	protected void Crater(vector wp, float tnt, bool heat)
	{
		float fx;
		float fz;
		if (!ToFine(wp, fx, fz))
		{
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (off the ground grid) at %1", wp));
			return;
		}
		// centre over kept terrain (road): no crater (the rim would cut a step into the field beside it)
		int ccx = Math.Floor(fx / FINE);
		int ccz = Math.Floor(fz / FINE);
		if (!m_Skin.IsHoleCell(ccx, ccz))
		{
			m_iCraterSkips++;
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (kept cell %1 %2) at %3", ccx, ccz, wp));
			// road crater (user 2026-10-18, option A): scorch decal + concrete chunks, no dig. Not for air bursts
			// (roof, tree, bridge): the same height rule as the voxel path, against the real terrain under the road
			float kAirNone = m_fCraterAirNone * Math.Max(1, Math.Pow(Math.Max(tnt, 0), 1 / 3.0));
			if (wp[1] - GetWorld().GetSurfaceY(wp[0], wp[2]) < kAirNone)
				MUD_RoadCraterComponent.Spawn(wp, FullDepth(tnt, heat), m_bDebug);
			return;
		}
		// probe 2026-10-13: in a voxel area our heightfield is sunk 4 m - the height checks below would call it an air burst
		if (MUD_VoxelProbe.InArea(wp))
		{
			// only the air check (above the undug ground): a hit deep in an earlier crater is still ground
			bool vok;
			float undug = BaseAt(fx, fz, vok);
			float vAirNone = m_fCraterAirNone * Math.Max(1, Math.Pow(Math.Max(tnt, 0), 1 / 3.0));
			if (vok && wp[1] - undug >= vAirNone)
			{
				m_iCraterSkips++;
				return;
			}
			VoxelCrater(wp, FullDepth(tnt, heat));
			return;
		}
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		int cgx = Math.ClampInt(Math.Round(fx), 0, gmax);
		int cgz = Math.ClampInt(Math.Round(fz), 0, gzmax);
		int cv;
		MUD_FinePatch cp = At(cgx, cgz, cv);
		float surf;
		float hc;           // height at the centre now, relative to the undeformed surface
		if (cp)
		{
			hc = cp.m_aH[cv];
			// asleep or still building: the skin collider (undug level) took the hit, not the dug ground
			if (cp.m_bAsleep || cp.m_iBuild > 0)
				surf = cp.m_aBase[cv];
			else
				surf = cp.m_aBase[cv] + hc;
		}
		else
		{
			bool ok;
			surf = BaseAt(fx, fz, ok);
			if (!ok)
			{
				if (m_bDebug)
					Print(string.Format("MUD world: crater skipped (no ground height) at %1", wp));
				return;
			}
		}
		// full-size depth of this charge on flat ground
		float full = FullDepth(tnt, heat);
		float slope = Math.Max(m_fCraterMaxSlope, 0.1);     // guard a 0 attribute (divide)
		// under the ground (cellar, deeper than the crater would reach), or up on a roof, a bridge, a
		// tree, in the air: not the ground
		float above = wp[1] - surf;
		// a bigger blast digs from higher up (vehicle ammo rack, fuel tank): heights grow with the cube root
		float airK = Math.Max(1, Math.Pow(Math.Max(tnt, 0), 1 / 3.0));
		float airFull = m_fCraterAirFull * airK;
		float airNone = m_fCraterAirNone * airK;
		if (above < -Math.Max(full, 0.5) || above >= airNone)
		{
			m_iCraterSkips++;
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (height: blast y %1, ground %2, patch %3) at %4", wp[1], surf, cp != null, wp));
			return;
		}
		float scale = 1;
		if (above > airFull)
			scale = (airNone - above) / (airNone - airFull);
		if (VoxelCrater(wp, full * scale))    // probe 2026-10-13: voxel craters (SP)
			return;

		// variation (server only; clients get the heights): only shallower (user 2026-10-21: the full size is the largest)
		float depth = full * scale * (1 - m_fCraterDepthJitter * Math.RandomFloat01());
		// LARGE (user 2026-10-22, v280 = before): a share digs past the full size and may pass the 2 x 2 patch limit
		bool large = Math.RandomFloat01() < m_fCraterLargeChance;
		if (large)
			depth = Math.Min(m_fCraterHugeDepth, full * scale * Math.RandomFloat(1.15, Math.Max(1.15, m_fCraterLargeFactor)));
		float rim = depth * m_fCraterRimShare;
		float rimWidth = Math.Max(2 * m_fFine, depth * m_fCraterRimWidthShare);
		// RIM KIND (user 2026-10-21, v278 = before; 3 HIGH v279 = before), one roll: 0 NO RIM = the bowl meets the grass at
		// the crest (outer = crest, the rim branch below never runs; RING_DIRT draws a flat dirt band); 1 MUD RING = a low
		// narrow lip, dirt by RING_DIRT; 3 HIGH = rim height + width x 1.3 .. High Factor; 2 full rim x 0.6 .. 1
		int rimKind = 2;
		float rimRoll = Math.RandomFloat01();
		if (rimRoll < m_fCraterNoRimChance)
		{
			rimKind = 0;
			rim = 0;
			rimWidth = 0;
		}
		else if (rimRoll < m_fCraterNoRimChance + m_fCraterRingChance)
		{
			rimKind = 1;
			rim = Math.Max(0.06, depth * m_fCraterRingShare);
			rimWidth = 2 * m_fFine;
		}
		else if (rimRoll < m_fCraterNoRimChance + m_fCraterRingChance + m_fCraterHighChance)
		{
			rimKind = 3;
			float high = Math.RandomFloat(1.3, Math.Max(1.3, m_fCraterHighFactor));
			rim *= high;
			rimWidth *= high;
		}
		else
		{
			rim *= Math.RandomFloat(0.6, 1);
		}
		if (hc < 0)
		{
			float cap = Math.Min(m_fCraterStackDepth, full * m_fCraterStackFactor);
			depth = Math.Max(depth, Math.Min(cap, -hc + m_fCraterDeepenGain * depth));
		}
		// 2 cm: below a rut step, not worth a rebuild and a net message
		if (depth < 0.02)
		{
			m_iCraterSkips++;
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (too small: scale %1) at %2", scale, wp));
			return;
		}
		// at least 3 fine steps: a smaller bowl is too coarse on the 0.25 m grid to keep the slope
		float crest = Math.Max(3 * m_fFine, (depth + rim) * Math.PI / (2 * slope));
		float outer = crest + Math.Max(rimWidth, rim * Math.PI / (2 * slope));
		// a fresh normal crater stays within 2 x 2 patches: scale the whole bowl (same slopes). M2 and big
		// charges (m_fCraterBigTnt) may pass it.
		bool big = tnt >= m_fCraterBigTnt || large;
		float maxOuter = CRATER_MAX_OUTER;
		if (big)
			maxOuter = CRATER_HUGE_OUTER;
		if (hc >= 0 && outer > maxOuter)
		{
			float k = maxOuter / outer;
			depth *= k;
			rim *= k;
			crest *= k;
			outer = maxOuter;
		}
		// outline: distance / f(angle), f = 1 .. 1 + stretch (oval share w, three lobes 1 - w). Only
		// widens (walls no steeper); big ones stretch less to stay under CRATER_MAX_OUTER.
		float stretch = Math.Clamp(Math.Min(m_fCraterShapeJitter, maxOuter / outer - 1), 0, 1);
		float lobeW = Math.RandomFloat01();
		float turn2 = Math.RandomFloat(0, Math.PI2);
		float turn3 = Math.RandomFloat(0, Math.PI2);

		float rr = outer * (1 + stretch) / m_fFine;
		int gx0 = Math.ClampInt(Math.Floor(fx - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(fx + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(fz - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(fz + rr), 0, gzmax);
		// a patch in range that did not build (collider made no body): no crater, else its live
		// neighbours' edges would sink next to the skin (a crack). BUILD ONCE: start them only, the rush builds them
		if ((BUILD_ONCE && !StartRange(gx0, gx1, gz0, gz1)) || (!BUILD_ONCE && !EnsureRange(gx0, gx1, gz0, gz1)))
		{
			m_iCraterSkips++;
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (range not ready) at %1", wp));
			return;
		}

		cp = At(cgx, cgz, cv);
		if (!cp)
		{
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (no patch at the centre) at %1", wp));
			return;
		}
		// fitted into the floor at the centre; the crest stays, so a shallow floor only flattens the walls
		depth = Math.Min(depth, -cp.m_aFloor[cv]);

		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float lock = p.m_aLock[v];
				if (lock <= 0)
					continue;
				float dx = (gx - fx) * m_fFine;
				float dz = (gz - fz) * m_fFine;
				float d = Math.Sqrt(dx * dx + dz * dz);
				if (d >= outer * (1 + stretch))
					continue;
				if (stretch > 0)
				{
					float ang = Math.Atan2(dz, dx);
					d /= 1 + stretch * (lobeW * (0.5 + 0.5 * Math.Cos(2 * (ang - turn2)))
						+ (1 - lobeW) * (0.5 + 0.5 * Math.Cos(3 * (ang - turn3))));
					if (d >= outer)
						continue;
				}
				float y;
				if (d < crest)
					y = -depth + (depth + rim) * (1 - Math.Cos(Math.PI * d / crest)) * 0.5;
				else if (outer > crest)    // no rim: outer = crest, guard the 0 / 0
					y = rim * (1 + Math.Cos(Math.PI * (d - crest) / (outer - crest))) * 0.5;
				else
					continue;
				y *= lock;
				float h = p.m_aH[v];
				if (y < 0)
				{
					y = Math.Max(y, p.m_aFloor[v]);
					if (y < h)
						SetH(gx, gz, y);
				}
				// M1: the rim never fills dug ground (it made a 2.3 m trap wall beside an older crater)
				else if (h >= 0 && y > h)
				{
					SetH(gx, gz, y);
				}
			}
		}
		m_bRushMark = true;     // CRATER RUSH: these patches build next frame, here and on the clients
		MarkDirty(gx0, gx1, gz0, gz1);
		m_bRushMark = false;
		FollowQueue(gx0, gx1, gz0, gz1, 0, 0);     // audit A1: wake / move what sits on it, set off armed mines
		m_iCraters++;
		if (m_bDebug)
			Print(string.Format("MUD world: crater at %1, TNT %2 kg, HEAT %9, scale %3, depth %4 m, crest %5 m, outer %6 m (+ stretch), floor at centre %7 m, centre now %8 m",
				wp, tnt, scale, depth, crest, outer, cp.m_aFloor[cv], cp.m_aH[cv], heat) + string.Format(", rim %1 m (kind %2: 0 none, 1 mud ring, 2 full, 3 high), large %3", rim, rimKind, large));
	}

	//------------------------------------------------------------------------------------------------
	protected void SmoothPatch(MUD_FinePatch p)
	{
		if (p.m_iSx0 < 0)
			return;
		MeshBoxL(p, p.m_iSx0, p.m_iSx1, p.m_iSz0, p.m_iSz1);     // REBUILD 5 option 3: m_aSmooth feeds the mesh ridge test
		UpdateSmooth(p, p.m_iSx0, p.m_iSx1, p.m_iSz0, p.m_iSz1);
		p.m_iSx0 = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-blur m_aH into m_aSmooth for patch-local verts [x0..x1] x [z0..z1]: separable binomial kernel,
	//! deformation only (MUD_GroundEntity.c:1503). Neighbour heights come through GetH, so both copies
	//! of an edge vertex get the same value. Hard and locked verts keep their own height.
	protected void UpdateSmooth(MUD_FinePatch p, int x0, int x1, int z0, int z1)
	{
		int t0 = System.GetTickCount();
		int r = m_aSmoothW.Count() / 2;
		int cols = x1 - x0 + 1;
		int wc = cols + 2 * r;
		int rows = z1 - z0 + 1 + 2 * r;
		int gx0 = p.m_iPx * PCELLS;
		int gz0 = p.m_iPz * PCELLS;

		for (int wz = 0; wz < rows; wz++)
		{
			int lz = z0 - r + wz;
			for (int wx = 0; wx < wc; wx++)
			{
				int lx = x0 - r + wx;
				float h;
				if (lx >= 0 && lx <= PCELLS && lz >= 0 && lz <= PCELLS)
					h = p.m_aH[lz * PVERTS + lx];
				else
					h = GetH(gx0 + lx, gz0 + lz);
				m_Buf.m_aBlur[wz * wc + wx] = h;
			}
		}

		// STEEP WALLS (MUD_TrenchWalls, user 2026-10-23: walked up a 90 deg wall): a vertex with a step over STEP in its
		// window keeps its own height, so a steep wall stays a wall in the collider (gentler ground blurs as before)
		bool anySteep = false;
		if (MUD_TrenchWalls.COLLIDER && r > 0)
		{
			for (int sz = 0; sz < rows; sz++)
			{
				for (int sx = 0; sx < wc; sx++)
				{
					int si = sz * wc + sx;
					float sh = m_Buf.m_aBlur[si];
					int st = 0;
					if (sx + 1 < wc && Math.AbsFloat(m_Buf.m_aBlur[si + 1] - sh) > MUD_TrenchWalls.STEP)
						st = 1;
					else if (sz + 1 < rows && Math.AbsFloat(m_Buf.m_aBlur[si + wc] - sh) > MUD_TrenchWalls.STEP)
						st = 1;
					m_Buf.m_aSteep[si] = st;
					if (st != 0)
						anySteep = true;
				}
			}
			if (anySteep)
			{
				for (int dz = 0; dz < rows; dz++)
				{
					for (int dx = 0; dx < cols; dx++)
					{
						int hit = 0;
						for (int dk = 0; dk <= 2 * r; dk++)
						{
							if (m_Buf.m_aSteep[dz * wc + dx + dk] != 0)
							{
								hit = 1;
								break;
							}
						}
						m_Buf.m_aSteepRows[dz * cols + dx] = hit;
					}
				}
			}
		}

		float wsum = 0;
		foreach (float wt : m_aSmoothW)
			wsum += wt;
		float inv = 1 / (wsum * wsum);

		for (int z = 0; z < rows; z++)
		{
			for (int x = 0; x < cols; x++)
			{
				float sum = 0;
				for (int kx = -r; kx <= r; kx++)
					sum += m_Buf.m_aBlur[z * wc + x + r + kx] * m_aSmoothW[kx + r];
				m_Buf.m_aBlurRows[z * cols + x] = sum;
			}
		}

		for (int z2 = z0; z2 <= z1; z2++)
		{
			for (int x2 = x0; x2 <= x1; x2++)
			{
				int v = z2 * PVERTS + x2;
				if (p.m_aLock[v] < 1 || VertexSoft(gx0 + x2, gz0 + z2) <= 0)
				{
					p.m_aSmooth[v] = p.m_aH[v];
					continue;
				}
				if (anySteep && SteepNear(z2 - z0, x2 - x0, cols, r))
				{
					p.m_aSmooth[v] = p.m_aH[v];     // STEEP WALLS
					continue;
				}
				float sum2 = 0;
				for (int kz = -r; kz <= r; kz++)
					sum2 += m_Buf.m_aBlurRows[(z2 - z0 + r + kz) * cols + x2 - x0] * m_aSmoothW[kz + r];
				p.m_aSmooth[v] = sum2 * inv;
			}
		}
		m_iSmoothMs += System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! STEEP WALLS (MUD_TrenchWalls.LOOK): fills m_Wall with a vertical strip in front of each steep step of p - the
	//! grid edge B0-B1 on the low side whose outward neighbours A0, A1 (one step towards side) both stand more than STEP
	//! higher (dig depth). Strips on the patch's far column / row belong to the next patch (no double strip).
	//! Returns the quad count.
	protected int WallQuads(MUD_FinePatch p, bool groove)
	{
		m_Wall.m_iV = 0;
		m_Wall.m_iI = 0;
		if (!MUD_TrenchWalls.LOOK)
			return 0;
		// no step over STEP fits in a patch whose dig depths span less (ruts, flat ground: most builds). INFERENCE: a
		// fully dug patch edge with its rim in the next patch is missed - trenches are at most 2 m wide, a patch 8 m.
		float hLo = 0;
		float hHi = 0;
		for (int hv = 0; hv < PVERTS * PVERTS; hv++)
		{
			float hh = p.m_aH[hv];
			if (hh < hLo)
				hLo = hh;
			else if (hh > hHi)
				hHi = hh;
		}
		if (hHi - hLo <= MUD_TrenchWalls.STEP)
			return 0;
		int gx0 = p.m_iPx * PCELLS;
		int gz0 = p.m_iPz * PCELLS;
		float gap = 0;
		if (groove)
			gap = m_fRevealGap;
		int quads;
		for (int axis = 0; axis < 2; axis++)     // 0: strip along z at column a (faces x), 1: along x at row a (faces z)
		{
			for (int side = -1; side <= 1; side += 2)     // where the high ground (A) is
			{
				for (int a = 0; a < PCELLS; a++)
				{
					for (int b = 0; b < PCELLS; b++)
					{
						int bx0 = a;
						int bz0 = b;
						int bx1 = a;
						int bz1 = b + 1;
						int sx = side;
						int sz = 0;
						if (axis == 1)
						{
							bx0 = b;
							bz0 = a;
							bx1 = b + 1;
							bz1 = a;
							sx = 0;
							sz = side;
						}
						int vb0 = bz0 * PVERTS + bx0;
						int vb1 = bz1 * PVERTS + bx1;
						float dB0 = p.m_aH[vb0];
						float dB1 = p.m_aH[vb1];
						float yA0;
						float yA1;
						if (!WallTop(p, gx0, gz0, bx0 + sx, bz0 + sz, dB0, yA0) || !WallTop(p, gx0, gz0, bx1 + sx, bz1 + sz, dB1, yA1))
							continue;
						// the cell between B and A must be ours and drawn (not kept terrain / voxel / look)
						int fx = bx0;
						int fz = bz0;
						if (sx < 0)
							fx--;
						if (sz < 0)
							fz--;
						if (fx >= 0 && fz >= 0 && fx < PCELLS && fz < PCELLS)
						{
							int cell = p.m_aCell[(fz / FINE) * SKIN + fx / FINE];
							if (cell < 0 || (cell & (VOX | LOOK)) != 0)
								continue;
						}
						if (quads >= MUD_TrenchWalls.MAX_QUADS)
						{
							MUD_TrenchWalls.s_iDropped++;
							continue;
						}
						float yB0 = p.m_aBase[vb0] + dB0 - gap - MUD_TrenchWalls.TUCK;
						float yB1 = p.m_aBase[vb1] + dB1 - gap - MUD_TrenchWalls.TUCK;
						WallQuad(Vector(bx0 * m_fFine, yB0, bz0 * m_fFine), Vector(bx1 * m_fFine, yB1, bz1 * m_fFine), yA0, yA1,
							(gz0 + bz0) * (1 - axis) + (gx0 + bx0) * axis, axis, side);
						quads++;
					}
				}
			}
		}
		return quads;
	}

	//------------------------------------------------------------------------------------------------
	//! STEEP WALLS: drawn height of the high vertex (patch-local ax, az; may sit in the next patch) when it stands more
	//! than STEP over the low vertex's dig depth dB. False = no wall here.
	protected bool WallTop(MUD_FinePatch p, int gx0, int gz0, int ax, int az, float dB, out float yA)
	{
		float dA;
		float baseA;
		if (ax >= 0 && az >= 0 && ax <= PCELLS && az <= PCELLS)
		{
			int va = az * PVERTS + ax;
			dA = p.m_aH[va];
			baseA = p.m_aBase[va];
		}
		else
		{
			int vo;
			MUD_FinePatch o = At(gx0 + ax, gz0 + az, vo);
			if (!o)
				return false;
			dA = o.m_aH[vo];
			baseA = o.m_aBase[vo];
		}
		if (dA - dB <= MUD_TrenchWalls.STEP)
			return false;
		yA = baseA + dA;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! STEEP WALLS: one strip, bottom b0 - b1 (patch-local), tops yA0 / yA1, facing away from side. g = global grid
	//! index of b0 along the strip: UVs run TILE m per tile along it and up it, one tile per strip (clamped, as the voxel quads).
	protected void WallQuad(vector b0, vector b1, float yA0, float yA1, int g, int axis, int side)
	{
		int n = m_Wall.m_iV;
		m_Wall.m_aV[n] = b0;
		m_Wall.m_aV[n + 1] = b1;
		m_Wall.m_aV[n + 2] = Vector(b0[0], yA0, b0[2]);
		m_Wall.m_aV[n + 3] = Vector(b1[0], yA1, b1[2]);
		float along = g * m_fFine;
		float ta = Math.Floor(along / MUD_TrenchWalls.TILE) * MUD_TrenchWalls.TILE;
		float u0 = Math.Clamp((along - ta) / MUD_TrenchWalls.TILE, 0, 1);
		float u1 = Math.Clamp((along + m_fFine - ta) / MUD_TrenchWalls.TILE, 0, 1);
		float ty = Math.Floor((b0[1] + yA0) * 0.5 / MUD_TrenchWalls.TILE) * MUD_TrenchWalls.TILE;
		m_Wall.m_aUV[n * 2] = u0;
		m_Wall.m_aUV[n * 2 + 1] = Math.Clamp(1 - (b0[1] - ty) / MUD_TrenchWalls.TILE, 0, 1);
		m_Wall.m_aUV[n * 2 + 2] = u1;
		m_Wall.m_aUV[n * 2 + 3] = Math.Clamp(1 - (b1[1] - ty) / MUD_TrenchWalls.TILE, 0, 1);
		m_Wall.m_aUV[n * 2 + 4] = u0;
		m_Wall.m_aUV[n * 2 + 5] = Math.Clamp(1 - (yA0 - ty) / MUD_TrenchWalls.TILE, 0, 1);
		m_Wall.m_aUV[n * 2 + 6] = u1;
		m_Wall.m_aUV[n * 2 + 7] = Math.Clamp(1 - (yA1 - ty) / MUD_TrenchWalls.TILE, 0, 1);

		// winding as FillIndices (bl, tl, br = normal up): (b0, top0, b1) faces cross(up, along) = +x (axis 0) / -z (axis 1)
		bool natural = (axis == 0 && side < 0) || (axis == 1 && side > 0);
		int i = m_Wall.m_iI;
		if (natural)
		{
			m_Wall.m_aI[i] = n;
			m_Wall.m_aI[i + 1] = n + 2;
			m_Wall.m_aI[i + 2] = n + 1;
			m_Wall.m_aI[i + 3] = n + 2;
			m_Wall.m_aI[i + 4] = n + 3;
			m_Wall.m_aI[i + 5] = n + 1;
		}
		else
		{
			m_Wall.m_aI[i] = n;
			m_Wall.m_aI[i + 1] = n + 1;
			m_Wall.m_aI[i + 2] = n + 2;
			m_Wall.m_aI[i + 3] = n + 2;
			m_Wall.m_aI[i + 4] = n + 1;
			m_Wall.m_aI[i + 5] = n + 3;
		}
		m_Wall.m_iV = n + 4;
		m_Wall.m_iI = i + 6;
	}

	//------------------------------------------------------------------------------------------------
	//! STEEP WALLS: MeshObject.Create for a patch mesh - surface (nv, ni), groove layer (gv, gi) when groove, the wall
	//! strips (m_Wall, groove material when groove) when there are any. Wall section = 1 without groove, 2 with.
	protected Resource PatchMeshCreate(MUD_FinePatch p, int nv, int ni, int gv, int gi, bool groove)
	{
		string surf = MUD_LookLift.SurfaceMat(m_Material, m_fZ0 + p.m_iPz * PCELLS * m_fFine);     // SNOW zone: white
		// WALL CRASH FIX 2026-10-23 (user: 75 / 90 deg trenches crash WB, 60 deg do not - 60 makes no strips): the wall
		// section shared an .emat with another section ({m_Material, m_Material} / {.., m_RutMaterial, m_RutMaterial}) =
		// the proven 2026-10-14 crash (mud-gotchas: same .emat on two sections, read at 0xffffffffffffffff). Own
		// material now (MUD_TrenchWalls.WALL_MAT); if it does not load the strips are left out, the patch still draws.
		if (m_Wall.m_iI > 0 && !MUD_TrenchWalls.WallMatOk())
		{
			m_Wall.m_iV = 0;
			m_Wall.m_iI = 0;
		}
		int wv = m_Wall.m_iV;
		int wi = m_Wall.m_iI;
		if (groove && wi > 0)
		{
			int nv3[] = {nv, gv, wv};
			int ni3[] = {ni, gi, wi};
			string m3[] = {surf, m_RutMaterial, MUD_TrenchWalls.WALL_MAT};
			return MeshObject.Create(3, nv3, ni3, m3, 0);
		}
		if (groove)
		{
			int nv2[] = {nv, gv};
			int ni2[] = {ni, gi};
			string m2[] = {surf, m_RutMaterial};
			return MeshObject.Create(2, nv2, ni2, m2, 0);
		}
		if (wi > 0)
		{
			int nvw[] = {nv, wv};
			int niw[] = {ni, wi};
			string mw[] = {surf, MUD_TrenchWalls.WALL_MAT};
			return MeshObject.Create(2, nvw, niw, mw, 0);
		}
		int nv1[] = {nv};
		int ni1[] = {ni};
		string m1[] = {surf};
		return MeshObject.Create(1, nv1, ni1, m1, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! STEEP WALLS: fills the wall section of a mesh made by PatchMeshCreate
	protected void PatchMeshWalls(MeshObject mesh, bool groove)
	{
		if (m_Wall.m_iI <= 0)
			return;
		int section = 1;
		if (groove)
			section = 2;
		mesh.UpdateIndices(section, m_Wall.m_aI);
		mesh.UpdateVerts(section, m_Wall.m_aV, m_Wall.m_aUV);
	}

	//------------------------------------------------------------------------------------------------
	//! STEEP WALLS: a steep step within r rows of output vertex (row, col) of UpdateSmooth (m_aSteepRows)
	protected bool SteepNear(int row, int col, int cols, int r)
	{
		for (int k = 0; k <= 2 * r; k++)
		{
			if (m_Buf.m_aSteepRows[(row + k) * cols + col] != 0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! m_aIndices (each fine cell split along its skin cell's diagonal, same winding as the skin),
	//! m_aShellIdx (reversed), m_aCellSlot (-1 = kept terrain or voxel cell) and m_aMeshIdx (those collapsed)
	protected void FillIndices(MUD_FinePatch p)
	{
		int n;
		for (int fz = 0; fz < PCELLS; fz++)
		{
			for (int fx = 0; fx < PCELLS; fx++)
			{
				int cell = p.m_aCell[(fz / FINE) * SKIN + fx / FINE];
				int bl = fz * PVERTS + fx;
				int br = bl + 1;
				int tl = bl + PVERTS;
				int tr = tl + 1;
				int fc = fz * PCELLS + fx;
				if (cell >= 0 && (cell & TLBR) != 0)
				{
					m_Buf.m_aIndices[n] = bl;
					m_Buf.m_aIndices[n + 1] = tl;
					m_Buf.m_aIndices[n + 2] = br;
					m_Buf.m_aIndices[n + 3] = tl;
					m_Buf.m_aIndices[n + 4] = tr;
					m_Buf.m_aIndices[n + 5] = br;
				}
				else
				{
					m_Buf.m_aIndices[n] = bl;
					m_Buf.m_aIndices[n + 1] = tl;
					m_Buf.m_aIndices[n + 2] = tr;
					m_Buf.m_aIndices[n + 3] = tr;
					m_Buf.m_aIndices[n + 4] = br;
					m_Buf.m_aIndices[n + 5] = bl;
				}
				// a LOOK cell (TEMP probe 2026-10-14) loses its mesh but keeps its collider
				if (cell < 0 || (cell & VOX) != 0)
					m_Buf.m_aCellSlot[fc] = -1;
				else
					m_Buf.m_aCellSlot[fc] = cell & SLOT;
				if (cell < 0 || (cell & (VOX | LOOK)) != 0)
				{
					for (int j = 0; j < 6; j++)
						m_Buf.m_aMeshIdx[n + j] = bl;
				}
				else
				{
					for (int j2 = 0; j2 < 6; j2++)
						m_Buf.m_aMeshIdx[n + j2] = m_Buf.m_aIndices[n + j2];
				}
				n += 6;
			}
		}
		for (int i = 0; i < 6144; i += 3)
		{
			m_Buf.m_aShellIdx[i] = m_Buf.m_aIndices[i];
			m_Buf.m_aShellIdx[i + 1] = m_Buf.m_aIndices[i + 2];
			m_Buf.m_aShellIdx[i + 2] = m_Buf.m_aIndices[i + 1];
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Cached index lists for p's layout, or null (no counters, no build)
	protected MUD_IdxSet FindIdx(MUD_FinePatch p)
	{
		if (p.m_Idx)
		{
			bool own = true;
			for (int oc = 0; oc < 16; oc++)
			{
				if (p.m_Idx.m_aCell[oc] != p.m_aCell[oc])
				{
					own = false;
					break;
				}
			}
			if (own)
				return p.m_Idx;
		}
		foreach (MUD_IdxSet s : m_aIdxSets)
		{
			bool same = true;
			for (int c = 0; c < 16; c++)
			{
				if (s.m_aCell[c] != p.m_aCell[c])
				{
					same = false;
					break;
				}
			}
			if (same)
				return s;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Index lists for p's layout: cached by its 16 cells, FillIndices + split only on a miss
	protected MUD_IdxSet IdxSet(MUD_FinePatch p)
	{
		if (p.m_Idx)
		{
			bool own = true;
			for (int oc = 0; oc < 16; oc++)
			{
				if (p.m_Idx.m_aCell[oc] != p.m_aCell[oc])
				{
					own = false;
					break;
				}
			}
			if (own)
			{
				m_iIdxHits++;
				return p.m_Idx;
			}
		}
		MUD_IdxSet found = FindIdx(p);
		if (found)
		{
			m_iIdxHits++;
			p.m_Idx = found;
			return found;
		}
		m_iIdxMisses++;

		FillIndices(p);
		MUD_IdxSet ix = new MUD_IdxSet();
		int maxSlot = -1;
		for (int c2 = 0; c2 < 16; c2++)
		{
			ix.m_aCell[c2] = p.m_aCell[c2];
			if (p.m_aCell[c2] < 0)
				continue;
			int cellSlot = p.m_aCell[c2];
			cellSlot = cellSlot & SLOT;
			if (cellSlot > maxSlot)
				maxSlot = cellSlot;
		}
		for (int i = 0; i < 6144; i++)
			ix.m_Mesh.m_aIdx[i] = m_Buf.m_aMeshIdx[i];
		ix.m_Mesh.m_iN = 6144;
		for (int k = 0; k <= maxSlot; k++)
		{
			MUD_IdxList sl = new MUD_IdxList();
			for (int fc = 0; fc < PCELLS * PCELLS; fc++)
			{
				if (m_Buf.m_aCellSlot[fc] != k)
					continue;
				for (int j = 0; j < 6; j++)
				{
					sl.m_aIdx[sl.m_iN] = m_Buf.m_aIndices[fc * 6 + j];
					sl.m_iN++;
				}
			}
			ix.m_aSlot.Insert(sl);
		}
		for (int sc = 0; sc < PCELLS * PCELLS; sc++)
		{
			if (m_Buf.m_aCellSlot[sc] < 0)
				continue;
			for (int sj = 0; sj < 6; sj++)
			{
				ix.m_Shell.m_aIdx[ix.m_Shell.m_iN] = m_Buf.m_aShellIdx[sc * 6 + sj];
				ix.m_Shell.m_iN++;
				ix.m_Plate.m_aIdx[ix.m_Plate.m_iN] = m_Buf.m_aIndices[sc * 6 + sj];
				ix.m_Plate.m_iN++;
			}
		}

		if (m_aIdxSets.Count() < IDX_SETS)
		{
			m_aIdxSets.Insert(ix);
		}
		else
		{
			m_aIdxSets.Set(m_iIdxNext, ix);
			m_iIdxNext = (m_iIdxNext + 1) % IDX_SETS;
		}
		p.m_Idx = ix;
		return ix;
	}

	//------------------------------------------------------------------------------------------------
	//! SPARSE SHEET A2 (MUD_MudSheet.SPARSE, 2026-10-18): set up the mesh lift for patch p - which 3 m tiles under it have
	//! sync mud data or are in the sheet's live window (there the sheet draws, the patch stays unlifted). False = no lift
	//! (DS, sheet off, zones only).
	protected bool LiftSetup(MUD_FinePatch p, bool rfOn)
	{
		m_bFarOn = false;
		if (!m_bRender)
			return false;
		if (MUD_LookLift.ENABLED)
		{
			LookLiftSetup(p);     // look lift: no sheet tiles read
			return true;
		}
		if (!MUD_MudSheet.PatchLiftOn() && !MUD_LookLift.ENABLED)
		{
			if (!MUD_MudSheet.FarPressOn())
				return false;
			return FarSetup(p);     // FAR PRESS: pressed by the sync layer where it has mud data
		}
		m_fLiftX0 = m_fX0 + p.m_iPx * PCELLS * m_fFine;
		m_fLiftZ0 = m_fZ0 + p.m_iPz * PCELLS * m_fFine;
		m_iLiftTx0 = Math.Floor(m_fLiftX0 / MUD_MudSheet.TILE_M);
		m_iLiftTz0 = Math.Floor(m_fLiftZ0 / MUD_MudSheet.TILE_M);
		if (MUD_MudSheet.LIFT_BY_DRAWN)
		{
			// one patch cell beyond the patch on the low side (LiftAt looks one cell each way): still 5 tiles
			m_iLiftTx0 = Math.Floor((m_fLiftX0 - m_fFine) / MUD_MudSheet.TILE_M);
			m_iLiftTz0 = Math.Floor((m_fLiftZ0 - m_fFine) / MUD_MudSheet.TILE_M);
		}
		for (int k = 0; k < LIFT_T * LIFT_T; k++)
		{
			m_aLiftT[k] = 0;
			int lkx = m_iLiftTx0 + k % LIFT_T;
			int lkz = m_iLiftTz0 + k / LIFT_T;
			if (MUD_MudSheet.LIFT_BY_DRAWN)
			{
				if (MUD_MudSheet.Unlifted(lkx, lkz))
					m_aLiftT[k] = 1;
			}
			else if (MUD_MudSync.HasTile(lkx, lkz) || MUD_MudSheet.LiveTile(lkx, lkz))
				m_aLiftT[k] = 1;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! LOOK LIFT (seam fix 2026-10-23): m_aSkinLift = lift of p's 5 x 5 skin vertices, by the skin's own rule
	//! (MUD_CoarseSkinEntity.LiftsVertex: all 4 cells round it hole cells, none voxel) - shared points of patch, skin
	//! and neighbour patches get the same lift. All 0 with MUD_LookLift 'lift off' (numpad .).
	protected void LookLiftSetup(MUD_FinePatch p)
	{
		// GAP (b) 2026-10-23: which neighbours draw. A border vertex presses out only when every patch sharing it draws -
		// next to an undrawn one the skin draws that edge lifted, so a pressed border was an open crack into it (as FAR
		// PRESS solved, FarSetup). A drawn neighbour built while p was not drawn left its border toward p unpressed:
		// it re-draws now (bit 8 - k = p as seen from q).
		int mask = 0;
		for (int nk = 0; nk < 9; nk++)
		{
			m_aLookNb[nk] = 0;
			if (nk == 4)
				continue;
			int nkz = nk / 3;
			int nkx = nk - nkz * 3;
			MUD_FinePatch q = FarNbDrawn(p.m_iPx + nkx - 1, p.m_iPz + nkz - 1);
			if (!q)
				continue;
			m_aLookNb[nk] = 1;
			mask = mask | (1 << nk);
			if (LOOK_BORDER_WAIT && q.m_iLookSides >= 0 && (q.m_iLookSides & (1 << (8 - nk))) == 0)
				LookRedraw(q);
		}
		p.m_iLookSides = mask;
		bool off = MUD_LookLift.LiftOff();
		for (int k = 0; k < (SKIN + 1) * (SKIN + 1); k++)
		{
			m_aSkinLift[k] = 0;
			if (off)
				continue;
			int kz = k / (SKIN + 1);
			int kx = k - kz * (SKIN + 1);
			if (!m_Skin || m_Skin.LiftsVertex(p.m_iPx * SKIN + kx, p.m_iPz * SKIN + kz))
				m_aSkinLift[k] = MUD_LookLift.VertexLift(p.m_iPx * SKIN + kx, p.m_iPz * SKIN + kz);     // base (numpad 0) + GROW
		}
		m_bLookCap = false;
		if (!off)
		{
			int capT = System.GetTickCount();     // OPT PASS: input split (band + grid mesh builds)
			LookCapSetup(p);
			m_iInCap += System.GetTickCount(capT);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! LOOK SLOPE (user shot 2026-10-23: deep presses in 0.75 m snow left sharp spikes - one lifted vertex between
	//! pressed ones stood the full lift high): m_aLookCap = most lift of every vertex of p and LOOK_CAP_R round it. Seeds
	//! from MUD_LookLift.SeedAt (0 = pressed out, a body cap, or none), then two chamfer passes: a vertex is at most
	//! SLOPE m per m higher than a neighbour (steps 1 and 1.414 cells). The window crosses the patch edge, so the two
	//! patches of a shared vertex get the same cap. Cost: LOOK_CAP_W^2 seed lookups per patch mesh build (UNMEASURED).
	protected void LookCapSetup(MUD_FinePatch p)
	{
		if (MUD_LookLift.SLOPE <= 0)
			return;
		// FPS FIX (MUD_LookLift.SLOPE_MIN_LIFT): only patches whose lift reaches over it (the snow zone) run this
		float top = 0;
		for (int sk = 0; sk < 25; sk++)
			top = Math.Max(top, m_aSkinLift[sk]);
		if (top <= MUD_LookLift.SLOPE_MIN_LIFT)
			return;
		int t0 = System.GetTickCount();
		int gx0 = p.m_iPx * PCELLS - LOOK_CAP_R;
		int gz0 = p.m_iPz * PCELLS - LOOK_CAP_R;
		int w = LOOK_CAP_W;
		bool any = false;
		for (int k = 0; k < w * w; k++)
		{
			int kz = k / w;
			int kx = k - kz * w;
			int gx = gx0 + kx;
			int gz = gz0 + kz;
			float seed = MUD_LookLift.SeedAt(gx, gz, m_fX0 + gx * m_fFine, m_fZ0 + gz * m_fFine);
			// cut ground draws no lift either (PressedOut, CUT_D): seed the slope from it too (user shot 2026-10-24: lone
			// full-lift spikes between scattered heli wash cuts in the 0.75 m snow - SeedAt knows presses and caps only)
			if (seed > 0 && GetH(gx, gz) < MUD_LookLift.CUT_D)
				seed = 0;
			m_aLookCap[k] = seed;
			if (seed < MUD_LookLift.NO_CAP)
				any = true;
		}
		m_bLookCap = any;
		if (!any)
			return;
		float s1 = MUD_LookLift.SLOPE * m_fFine;
		float s2 = s1 * 1.4142;
		// forward: from left, up-left, up, up-right
		for (int fz = 0; fz < w; fz++)
		{
			for (int fx = 0; fx < w; fx++)
			{
				int fi = fz * w + fx;
				float fv = m_aLookCap[fi];
				if (fx > 0)
					fv = Math.Min(fv, m_aLookCap[fi - 1] + s1);
				if (fz > 0)
				{
					fv = Math.Min(fv, m_aLookCap[fi - w] + s1);
					if (fx > 0)
						fv = Math.Min(fv, m_aLookCap[fi - w - 1] + s2);
					if (fx < w - 1)
						fv = Math.Min(fv, m_aLookCap[fi - w + 1] + s2);
				}
				m_aLookCap[fi] = fv;
			}
		}
		// backward: from right, down-right, down, down-left
		for (int bz = w - 1; bz >= 0; bz--)
		{
			for (int bx = w - 1; bx >= 0; bx--)
			{
				int bi = bz * w + bx;
				float bv = m_aLookCap[bi];
				if (bx < w - 1)
					bv = Math.Min(bv, m_aLookCap[bi + 1] + s1);
				if (bz < w - 1)
				{
					bv = Math.Min(bv, m_aLookCap[bi + w] + s1);
					if (bx < w - 1)
						bv = Math.Min(bv, m_aLookCap[bi + w + 1] + s2);
					if (bx > 0)
						bv = Math.Min(bv, m_aLookCap[bi + w - 1] + s2);
				}
				m_aLookCap[bi] = bv;
			}
		}
		m_iLookCapN++;
		m_iLookCapMs += System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! SPARSE SHEET A2: m to draw vertex v of the patch LiftSetup was last called for higher (0 over a sync tile;
	//! times the lock: 0 at kept terrain / buildings, the same fade the rut floor uses)
	protected float LiftAt(MUD_FinePatch p, int v, float d)
	{
		if (m_bFarOn)
			return -FarAt(p, v, d);      // FAR PRESS: drawn down, never up
		if (MUD_LookLift.ENABLED)
		{
			// LOOK LIFT PRESS-OUT (MUD_LookLift, 2026-10-23): no lift where cut, worn, or under a seen wheel / foot
			int pvz = v / PVERTS;
			int pvx = v - pvz * PVERTS;
			int pgx = p.m_iPx * PCELLS + pvx;
			int pgz = p.m_iPz * PCELLS + pvz;
			if (LookBorderShared(pvx, pvz) && MUD_LookLift.PressedOut(pgx, pgz, d, m_fX0 + pgx * m_fFine, m_fZ0 + pgz * m_fFine))
				return 0;
			// GAP (a) 2026-10-23 (plan 'Look lift for real' step 3): no 'building box = 0' rule any more. The skin knows no
			// buildings and lifts every hole cell (the keep scan keeps only the cell under a building's origin), so a patch
			// edge in a building box next to a skin-only cell was 0 on one side and lifted on the other = open crack. Now
			// the patch follows the skin there too. Kept terrain stays 0: every skin vertex touching a kept cell is 0 in
			// m_aSkinLift, so the bilinear below is 0 on and inside kept cells. The building lock still stops digging.
			// SEAM FIX 2026-10-23: bilinear over the skin vertices' lift (LookLiftSetup), not the 1 m lock fade - the skin
			// lifts per 2 m vertex, so a patch edge next to skin, a road or a voxel cell now meets it exactly
			int sx = pvx / FINE;
			int sz = pvz / FINE;
			if (sx > SKIN - 1)
				sx = SKIN - 1;
			if (sz > SKIN - 1)
				sz = SKIN - 1;
			float fx = pvx - sx * FINE;
			float fz = pvz - sz * FINE;
			fx = fx / FINE;
			fz = fz / FINE;
			int s0 = sz * (SKIN + 1) + sx;
			int s1 = s0 + SKIN + 1;
			float l0 = m_aSkinLift[s0] + (m_aSkinLift[s0 + 1] - m_aSkinLift[s0]) * fx;
			float l1 = m_aSkinLift[s1] + (m_aSkinLift[s1 + 1] - m_aSkinLift[s1]) * fx;
			float lt = l0 + (l1 - l0) * fz;
			if (m_bLookCap && LookBorderShared(pvx, pvz))
				lt = Math.Min(lt, m_aLookCap[(pvz + LOOK_CAP_R) * LOOK_CAP_W + pvx + LOOK_CAP_R]);     // LOOK SLOPE + BODY CAP
			return lt;
		}
		int lvz = v / PVERTS;     // int maths on its own line: '%' in a float expression does not compile
		int lvx = v - lvz * PVERTS;
		if (MUD_MudSheet.LIFT_BY_DRAWN)
		{
			// LIFT BY DRAWN: drop only when every tile within one patch cell is drawn - the slope to the lifted
			// neighbour then lies under the sheet, never outside its edge
			float lwx = m_fLiftX0 + lvx * m_fFine;
			float lwz = m_fLiftZ0 + lvz * m_fFine;
			int lax = Math.Floor((lwx - m_fFine) / MUD_MudSheet.TILE_M) - m_iLiftTx0;
			int lbx = Math.Floor((lwx + m_fFine) / MUD_MudSheet.TILE_M) - m_iLiftTx0;
			int laz = Math.Floor((lwz - m_fFine) / MUD_MudSheet.TILE_M) - m_iLiftTz0;
			int lbz = Math.Floor((lwz + m_fFine) / MUD_MudSheet.TILE_M) - m_iLiftTz0;
			if (lax < 0 || laz < 0 || lbx >= LIFT_T || lbz >= LIFT_T)
				return MUD_MudSheet.PatchLift() * p.m_aLock[v];
			if (m_aLiftT[laz * LIFT_T + lax] != 0 && m_aLiftT[laz * LIFT_T + lbx] != 0 && m_aLiftT[lbz * LIFT_T + lax] != 0 && m_aLiftT[lbz * LIFT_T + lbx] != 0)
				return 0;
			return MUD_MudSheet.PatchLift() * p.m_aLock[v];
		}
		int ltx = Math.Floor((m_fLiftX0 + lvx * m_fFine) / MUD_MudSheet.TILE_M);
		int ltz = Math.Floor((m_fLiftZ0 + lvz * m_fFine) / MUD_MudSheet.TILE_M);
		ltx -= m_iLiftTx0;
		ltz -= m_iLiftTz0;
		if (ltx < 0 || ltz < 0 || ltx >= LIFT_T || ltz >= LIFT_T)
			return 0;
		if (m_aLiftT[ltz * LIFT_T + ltx] != 0)
			return 0;
		return MUD_MudSheet.PatchLift() * p.m_aLock[v];
	}

	//------------------------------------------------------------------------------------------------
	//! SPARSE SHEET A2: a sync tile (3 m, world corner x, z) came or was reloaded - the patch meshes over it rebuild
	//! (mesh only, as client look ruts), so their lift goes off there
	static void SheetLiftDirty(float x, float z)
	{
		if (!s_Inst || !s_Inst.m_bReady || !s_Inst.m_bRender || !MUD_MudSheet.PatchLiftOn())
			return;
		float f = s_Inst.m_fFine;
		int gx0 = Math.Floor((x - s_Inst.m_fX0) / f) - 1;
		int gx1 = Math.Ceil((x + MUD_MudSheet.TILE_M - s_Inst.m_fX0) / f) + 1;
		int gz0 = Math.Floor((z - s_Inst.m_fZ0) / f) - 1;
		int gz1 = Math.Ceil((z + MUD_MudSheet.TILE_M - s_Inst.m_fZ0) / f) + 1;
		s_Inst.MarkLookDirty(gx0, gx1, gz0, gz1, -1, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! WEAR_SHOW: a wear-only pass counted wear at world x, z (MUD_VoxelProbe.WearPass) - the patch mesh there builds
	//! again (client look only, normal mesh pacing). No floor read, no op change. (Name from the archived pressed lift.)
	static void PressWearMark(float x, float z)
	{
		if (!s_Inst || !s_Inst.m_bReady || !s_Inst.m_bRender)
			return;
		float f = s_Inst.m_fFine;
		int gx = Math.Round((x - s_Inst.m_fX0) / f);
		int gz = Math.Round((z - s_Inst.m_fZ0) / f);
		MUD_LookLift.GrowBox(gx, gx, gz, gz);     // look lift GROW: pass 1 (wear) lifts round it a little
		if (!WEAR_SHOW)
			return;
		s_Inst.MarkLookDirty(gx - 1, gx + 1, gz - 1, gz + 1, -1, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Pressed lift / FAR PRESS: m_aPressVox = the voxel skin cells of p and one ring round it (PressNearHole reads it)
	protected void PressVoxSetup(MUD_FinePatch p)
	{
		for (int k = 0; k < 36; k++)
		{
			m_aPressVox[k] = 0;
			int kz = k / 6;
			int kx = k - kz * 6;
			int cx = p.m_iPx * SKIN + kx - 1;     // global skin cell
			int cz = p.m_iPz * SKIN + kz - 1;
			if (cx < 0 || cz < 0)
				continue;
			int qx = cx / SKIN;
			int qz = cz / SKIN;
			if (qx >= m_iNPX || qz >= m_iNPZ)
				continue;
			MUD_FinePatch q = m_mPatches.Get(qz * m_iNPX + qx);
			if (!q)
				continue;
			int cell = q.m_aCell[(cz - qz * SKIN) * SKIN + cx - qx * SKIN];
			if (cell >= 0 && (cell & VOX) != 0)
				m_aPressVox[k] = 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS: set up patch p's mesh press - the sync mud round it (one cell past half a patch cell each way), which
	//! neighbour patches draw (border vertices next to an undrawn one stay unpressed: no hole into the skin), the voxel
	//! cells. False = no sync mud pressed there (the mesh is built as without far press).
	protected bool FarSetup(MUD_FinePatch p)
	{
		int gxa = p.m_iPx * PCELLS;     // global fine vertex of the patch corner (FarAt uses the same ints: shared border
		int gza = p.m_iPz * PCELLS;     // vertices give the same press in both patches)
		float half = m_fFine * 0.5;
		float cell = MUD_MudSheet.CELL;
		m_iFarGi0 = Math.Ceil((m_fX0 + gxa * m_fFine - half) / cell) - 1;     // -1 / +1: float rounding at the edge
		m_iFarGk0 = Math.Ceil((m_fZ0 + gza * m_fFine - half) / cell) - 1;
		int gi1 = Math.Floor((m_fX0 + (gxa + PCELLS) * m_fFine + half) / cell) + 1;
		int gk1 = Math.Floor((m_fZ0 + (gza + PCELLS) * m_fFine + half) / cell) + 1;
		m_iFarW = gi1 - m_iFarGi0 + 1;
		m_iFarH = gk1 - m_iFarGk0 + 1;
		int n = m_iFarW * m_iFarH;
		if (m_aFarD.Count() < n)
			m_aFarD.Resize(n);
		if (MUD_MudSync.FarBox(m_iFarGi0, m_iFarGk0, m_iFarW, m_iFarH, m_aFarD) == 0)
		{
			m_iFarEmpty++;
			p.m_iFarSides = -1;
			return false;
		}
		m_fLiftX0 = m_fX0 + gxa * m_fFine;
		m_fLiftZ0 = m_fZ0 + gza * m_fFine;
		int mask = 0;
		for (int k = 0; k < 9; k++)
		{
			m_aFarNb[k] = 0;
			if (k == 4)
				continue;
			int kz = k / 3;
			int kx = k - kz * 3;
			MUD_FinePatch q = FarNbDrawn(p.m_iPx + kx - 1, p.m_iPz + kz - 1);
			if (!q)
				continue;
			m_aFarNb[k] = 1;
			mask = mask | (1 << k);
			// a drawn pressed neighbour built while p was not drawn left the shared border unpressed: build it again.
			// p sits at the mirrored offset from q: bit 8 - k ((1 - dz) * 3 + 1 - dx)
			if (q.m_iFarSides >= 0 && (q.m_iFarSides & (1 << (8 - k))) == 0)
			{
				FarDueAdd(q, true);
				m_iFarHeal++;
			}
		}
		p.m_iFarSides = mask;
		PressVoxSetup(p);
		m_bFarOn = true;
		m_iFarBuilt++;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS: patch px, pz if it draws a mesh (live, or asleep with its mesh kept), else null
	protected MUD_FinePatch FarNbDrawn(int px, int pz)
	{
		if (px < 0 || pz < 0 || px >= m_iNPX || pz >= m_iNPZ)
			return null;
		MUD_FinePatch q = m_mPatches.Get(pz * m_iNPX + px);
		if (!q || !q.m_Ent)
			return null;
		if (q.m_iBuild == 0 || q.m_bAsleep)
			return q;
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS: m to draw vertex v of the patch FarSetup was last called for lower = (THICK - least sync depth within
	//! half a patch cell) x FAR_PRESS_K x lock; sets m_fPressSoil (soil from FIELD_SOIL_D deep, as PressAt)
	protected float FarAt(MUD_FinePatch p, int v, float d)
	{
		m_fPressSoil = 1000;
		int lvz = v / PVERTS;     // int maths on its own line
		int lvx = v - lvz * PVERTS;
		float x = m_fX0 + (p.m_iPx * PCELLS + lvx) * m_fFine;
		float z = m_fZ0 + (p.m_iPz * PCELLS + lvz) * m_fFine;
		float half = m_fFine * 0.5;
		float cell = MUD_MudSheet.CELL;
		int ia = Math.Ceil((x - half) / cell) - m_iFarGi0;
		int ib = Math.Floor((x + half) / cell) - m_iFarGi0;
		int ka = Math.Ceil((z - half) / cell) - m_iFarGk0;
		int kb = Math.Floor((z + half) / cell) - m_iFarGk0;
		ia = Math.ClampInt(ia, 0, m_iFarW - 1);
		ib = Math.ClampInt(ib, 0, m_iFarW - 1);
		ka = Math.ClampInt(ka, 0, m_iFarH - 1);
		kb = Math.ClampInt(kb, 0, m_iFarH - 1);
		float thick = MUD_MudSheet.THICK;
		float dmin = thick;
		for (int k = ka; k <= kb; k++)
		{
			for (int i = ia; i <= ib; i++)
			{
				float dc = m_aFarD[k * m_iFarW + i];
				if (dc < dmin)
					dmin = dc;
			}
		}
		float press = (thick - dmin) * MUD_MudSheet.FAR_PRESS_K * p.m_aLock[v];
		if (press <= 0.001)
			return 0;
		// border vertex next to a patch that draws nothing: unpressed (the skin there has no skirt)
		int ndx = 0;
		if (lvx == 0)
			ndx = -1;
		else if (lvx == PVERTS - 1)
			ndx = 1;
		int ndz = 0;
		if (lvz == 0)
			ndz = -1;
		else if (lvz == PVERTS - 1)
			ndz = 1;
		if (ndx != 0 || ndz != 0)
		{
			bool open = (ndx != 0 && m_aFarNb[4 + ndx] == 0) || (ndz != 0 && m_aFarNb[(1 + ndz) * 3 + 1] == 0);
			if (ndx != 0 && ndz != 0 && m_aFarNb[(1 + ndz) * 3 + 1 + ndx] == 0)
				open = true;
			if (open)
			{
				m_iFarFaded++;
				return 0;
			}
		}
		if (PressNearHole(p, lvx, lvz))
		{
			m_iFarHole++;
			return 0;
		}
		m_iFarVerts++;
		float drawn = press + Math.Max(0, -d);
		m_fPressSoil = -drawn + FIELD_SOIL_D - m_fRevealDepth;
		if (drawn >= FIELD_SOIL_D)
			m_iFarSoil++;
		return press;
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS (MUD_MudSync StepSheet / LoadSnap): the sync mud of world cells ci0..ci1 x ck0..ck1 changed - the patches
	//! there get a paced look rebuild (missing ones: FieldDue makes them). Rendering machines, far press on.
	static void FarPressMark(int ci0, int ci1, int ck0, int ck1)
	{
		if (!MUD_MudSheet.FAR_PRESS || !s_Inst || !s_Inst.m_bReady || !s_Inst.m_bRender)
			return;
		s_Inst.FarMark(ci0, ci1, ck0, ck1);
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS: keys are kept while the numpad / switch has it off too (on again: FarPressToggle builds them)
	protected void FarMark(int ci0, int ci1, int ck0, int ck1)
	{
		bool on = MUD_MudSheet.FarPressOn();
		float cell = MUD_MudSheet.CELL;
		int gx0 = Math.Floor((ci0 * cell - m_fX0) / m_fFine) - 1;
		int gx1 = Math.Ceil((ci1 * cell - m_fX0) / m_fFine) + 1;
		int gz0 = Math.Floor((ck0 * cell - m_fZ0) / m_fFine) - 1;
		int gz1 = Math.Ceil((ck1 * cell - m_fZ0) / m_fFine) + 1;
		if (on)
			m_iFarMarks++;
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
			{
				int key = pz * m_iNPX + px;
				m_sFarKeys.Insert(key);
				if (!on)
					continue;
				m_sFieldPatches.Insert(key);     // FieldDue makes a missing one near a camera / player / vehicle
				MUD_FinePatch p = m_mPatches.Get(key);
				if (!p)
					continue;
				p.m_bFloor = true;     // out of the lazy stop (IsLazyReady), as MarkFloorDirty
				FarDueAdd(p, false);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS: p's look rebuild goes FAR_PRESS_EVERY s after its last one (now: at once); a waiting one keeps its time
	protected void FarDueAdd(MUD_FinePatch p, bool now)
	{
		if (m_mFarDue.Contains(p.m_iKey))
			return;
		float due = p.m_fFarAt + MUD_MudSheet.FAR_PRESS_EVERY;
		if (now || due < m_fTime)
			due = m_fTime;
		m_mFarDue.Set(p.m_iKey, due);
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS, every frame (rendering machines): one due patch a frame gets a look rebuild of its whole mesh
	protected void FarPressTick()
	{
		if (m_mFarDue.IsEmpty())
			return;
		int pick = -1;
		foreach (int key, float due : m_mFarDue)
		{
			if (due <= m_fTime)
			{
				pick = key;
				break;
			}
		}
		if (pick < 0)
			return;
		m_mFarDue.Remove(pick);
		MUD_FinePatch p = m_mPatches.Get(pick);
		if (!p)
			return;
		FarRebuild(p);
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS: mesh rebuild of p's whole mesh. As MarkLookDirty, but NOT a look rut rebuild (m_bLookDirty stays
	//! off): no look rut delay meters, no look-rut priority over the rut pace (RUT_PACE). Colliders untouched.
	protected void FarRebuild(MUD_FinePatch p)
	{
		// a lazy / building patch draws nothing - finish it (LOOK SPREAD, FloorSpreadTick: one build step per frame)
		if (FLOOR_SPREAD && p.m_iBuild > 0 && !p.m_bAsleep && !m_aFloorSpread.Contains(p.m_iKey))
			m_aFloorSpread.Insert(p.m_iKey);
		p.m_bMeshDirty = true;
		p.m_bWhySweep = true;     // REBUILD_WHY
		MeshBoxL(p, 0, PCELLS, 0, PCELLS);
		if (!p.m_bQueued)
		{
			p.m_bQueued = true;
			p.m_fQueuedAt = m_fTime;
			m_aDirty.Insert(p.m_iKey);
		}
		p.m_fFarAt = m_fTime;
		m_iFarSent++;
	}

	//------------------------------------------------------------------------------------------------
	//! LOOK LIFT gap (b): local vertex (pvx, pvz) of the patch LookLiftSetup was last called for may press out - inside,
	//! or on the border with every neighbour that shares the vertex drawn (m_aLookNb)
	protected bool LookBorderShared(int pvx, int pvz)
	{
		if (!LOOK_BORDER_WAIT)
			return true;
		int dx = 0;
		int dz = 0;
		if (pvx == 0)
			dx = -1;
		else if (pvx == PCELLS)
			dx = 1;
		if (pvz == 0)
			dz = -1;
		else if (pvz == PCELLS)
			dz = 1;
		if (dx == 0 && dz == 0)
			return true;
		if (dx != 0 && m_aLookNb[4 + dx] == 0)
			return false;
		if (dz != 0 && m_aLookNb[4 + dz * 3] == 0)
			return false;
		if (dx != 0 && dz != 0 && m_aLookNb[4 + dz * 3 + dx] == 0)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! LOOK LIFT gap (b): mesh-only re-draw of patch q alone (its whole vertex box), as MarkLookDirty does per patch
	protected void LookRedraw(MUD_FinePatch q)
	{
		int gx0 = q.m_iPx * PCELLS;
		int gz0 = q.m_iPz * PCELLS;
		q.m_bMeshDirty = true;
		q.m_bLookDirty = true;
		q.m_bWhySweep = true;     // REBUILD_WHY
		MeshBoxG(q, gx0, gx0 + PCELLS, gz0, gz0 + PCELLS);     // REBUILD 5 option 3
		if (!q.m_bQueued)
		{
			q.m_bQueued = true;
			q.m_fQueuedAt = m_fTime;
			m_aDirty.Insert(q.m_iKey);
		}
		m_iLookHeal++;
	}

	//------------------------------------------------------------------------------------------------
	//! LOOK LIFT gap (b): p goes (freed) - drawn neighbours that pressed their border toward it re-draw it lifted
	protected void LookNbGone(MUD_FinePatch p)
	{
		if (!MUD_LookLift.ENABLED || !LOOK_BORDER_WAIT)
			return;
		for (int k = 0; k < 9; k++)
		{
			if (k == 4)
				continue;
			int kz = k / 3;
			int kx = k - kz * 3;
			MUD_FinePatch q = FarNbDrawn(p.m_iPx + kx - 1, p.m_iPz + kz - 1);
			if (q && q.m_iLookSides >= 0 && (q.m_iLookSides & (1 << (8 - k))) != 0)
			{
				q.m_iLookSides = q.m_iLookSides - (1 << (8 - k));     // the bit is set (checked above)
				LookRedraw(q);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! LOOK LIFT buildings (2026-10-23): every patch that exists re-draws its mesh (the skin's no-lift vertices changed
	//! under it). Patches only, no skin, no net. Returns patches marked.
	static int LookRedrawAll()
	{
		if (!s_Inst || !s_Inst.m_bRender)
			return 0;
		int n;
		foreach (int key, MUD_FinePatch p : s_Inst.m_mPatches)
		{
			if (!p)
				continue;
			s_Inst.LookRedraw(p);
			n++;
		}
		return n;
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS: p goes (freed) - its drawn neighbours pressed toward it build again (their border would open into the skin)
	protected void FarNbGone(MUD_FinePatch p)
	{
		if (p.m_iFarSides < 0 && !m_sFarKeys.Contains(p.m_iKey))
			return;
		for (int k = 0; k < 9; k++)
		{
			if (k == 4)
				continue;
			int kz = k / 3;
			int kx = k - kz * 3;
			MUD_FinePatch q = FarNbDrawn(p.m_iPx + kx - 1, p.m_iPz + kz - 1);
			if (q && q.m_iFarSides >= 0 && (q.m_iFarSides & (1 << (8 - k))) != 0)
			{
				q.m_iFarSides = q.m_iFarSides - (1 << (8 - k));     // the bit is set (checked above)
				FarDueAdd(q, true);
				m_iFarHeal++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! FAR PRESS, TEST_KIT numpad /: flip it; every marked patch builds its mesh again (paced, one a frame)
	static void FarPressToggle()
	{
		MUD_MudSheet.s_bFarPress = !MUD_MudSheet.s_bFarPress;
		if (!s_Inst || !s_Inst.m_bReady || !s_Inst.m_bRender)
			return;
		bool on = MUD_MudSheet.FarPressOn();
		int n = 0;
		foreach (int key : s_Inst.m_sFarKeys)
		{
			if (on)
				s_Inst.m_sFieldPatches.Insert(key);     // tracks driven while it was off: FieldDue makes their patches
			MUD_FinePatch p = s_Inst.m_mPatches.Get(key);
			if (!p)
				continue;
			if (on)
				p.m_bFloor = true;
			s_Inst.FarDueAdd(p, true);
			n++;
		}
		Print(string.Format("MUD far press: on %1 (key) - %2 patches build again", MUD_MudSheet.s_bFarPress, n), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Pressed lift: true = vertex lvx, lvz of p touches a voxel skin cell or a dug vertex (m_aH < -0.02) - no lip over
	//! holes. Neighbours past the patch edge are read globally, so a shared border vertex gives the same in both patches.
	protected bool PressNearHole(MUD_FinePatch p, int lvx, int lvz)
	{
		int sxa = -1;
		if (lvx > 0)
			sxa = (lvx - 1) / FINE;
		int sxb = lvx / FINE;
		int sza = -1;
		if (lvz > 0)
			sza = (lvz - 1) / FINE;
		int szb = lvz / FINE;
		for (int sz = sza; sz <= szb; sz++)
		{
			for (int sx = sxa; sx <= sxb; sx++)
			{
				if (m_aPressVox[(sz + 1) * 6 + sx + 1] != 0)
					return true;
			}
		}
		int gx = p.m_iPx * PCELLS + lvx;
		int gz = p.m_iPz * PCELLS + lvz;
		for (int dz = -1; dz <= 1; dz++)
		{
			for (int dx = -1; dx <= 1; dx++)
			{
				int nx = lvx + dx;
				int nz = lvz + dz;
				float hh;
				if (nx >= 0 && nz >= 0 && nx < PVERTS && nz < PVERTS)
					hh = p.m_aH[nz * PVERTS + nx];
				else
					hh = GetH(gx + dx, gz + dz);
				if (hh < -0.02)
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! RAGGED EDGE: 0..1 hash of a world lattice point (fine vertex units)
	static float RagHash(int x, int z)
	{
		float s = Math.Sin(x * 12.9898 + z * 78.233) * 43758.5453;
		return s - Math.Floor(s);
	}

	//------------------------------------------------------------------------------------------------
	//! RAGGED EDGE: 0..1 noise at world fine vertex gx, gz (soft blobs + speckle)
	static float RagNoise(float gx, float gz)
	{
		float fx = gx / RAG_CELLS;
		float fz = gz / RAG_CELLS;
		int ix = Math.Floor(fx);
		int iz = Math.Floor(fz);
		float tx = fx - ix;
		float tz = fz - iz;
		tx = tx * tx * (3 - 2 * tx);
		tz = tz * tz * (3 - 2 * tz);
		float a = RagHash(ix, iz) + (RagHash(ix + 1, iz) - RagHash(ix, iz)) * tx;
		float c = RagHash(ix, iz + 1) + (RagHash(ix + 1, iz + 1) - RagHash(ix, iz + 1)) * tx;
		float soft = a + (c - a) * tz;
		int wx = Math.Round(gx) + 7919;
		int wz = Math.Round(gz) - 104729;
		return soft * (1 - RAG_WHITE) + RagHash(wx, wz) * RAG_WHITE;
	}

	//------------------------------------------------------------------------------------------------
	//! RAGGED EDGE: reveal 0..1 (before smoothstep) for depth dep (m, positive down) at world fine vertex gx, gz
	float RevealT(float dep, float gx, float gz)
	{
		float thr = m_fRevealDepth;
		if (dep <= thr)
			return 0;     // most verts: no noise maths
		float blend = Math.Max(m_fRevealBlend, 0.01);
		if (RAGGED)
		{
			thr += RagNoise(gx, gz) * RAG_D;
			blend += RAG_BLEND;
		}
		return Math.Clamp((dep - thr) / blend, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! WEAR SHOW: groove reveal input rfD of vertex v of p (global fine vertex gx, gz) - a worn sample goes at least
	//! WEAR_SHOW_D x wear past RevealDepth, edge-locked (none at kept terrain / roads). Look only.
	protected float WearShowD(MUD_FinePatch p, int v, int gx, int gz, float rfD)
	{
		float wear = MUD_VoxelProbe.WearG(gx, gz);
		if (wear <= 0)
			return rfD;
		float wd = -(m_fRevealDepth + WEAR_SHOW_D * Math.Min(1, wear * WEAR_SHOW_K)) * p.m_aEdgeLock[v];
		if (wd < rfD)
			return wd;
		return rfD;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD RING: m_aRingD2 of patch p = squared fine steps from each vertex to the nearest vertex dug deeper than
	//! RING_DUG within RING_REACH_V (also past the patch edge), 99 = none. Exact distance in two passes (row, then
	//! column). False = no dug vertex near the patch (most patches: one scan, nothing else).
	protected bool RingPrep(MUD_FinePatch p)
	{
		int r = RING_REACH_V;
		int gx0 = p.m_iPx * PCELLS;
		int gz0 = p.m_iPz * PCELLS;
		bool any;
		for (int ez = 0; ez < RING_EXT; ez++)
		{
			for (int ex = 0; ex < RING_EXT; ex++)
			{
				int x = ex - r;
				int z = ez - r;
				float h;
				if (x >= 0 && x <= PCELLS && z >= 0 && z <= PCELLS)
					h = p.m_aH[z * PVERTS + x];
				else
					h = GetH(gx0 + x, gz0 + z);
				m_aRingDug[ez * RING_EXT + ex] = 0;
				if (h < -RING_DUG)
				{
					m_aRingDug[ez * RING_EXT + ex] = 1;
					any = true;
				}
			}
		}
		if (!any)
			return false;
		for (int rz = 0; rz < RING_EXT; rz++)
		{
			for (int rx = r; rx < r + PVERTS; rx++)
			{
				int best = 99;
				for (int dx = -r; dx <= r; dx++)
				{
					int ax = Math.AbsInt(dx);
					if (ax < best && m_aRingDug[rz * RING_EXT + rx + dx] != 0)
						best = ax;
				}
				m_aRingRow[rz * RING_EXT + rx] = best;
			}
		}
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int b2 = 99;
				for (int dz = -r; dz <= r; dz++)
				{
					int row = m_aRingRow[(vz + r + dz) * RING_EXT + vx + r];
					if (row < 99)
					{
						int c2 = row * row + dz * dz;
						if (c2 < b2)
							b2 = c2;
					}
				}
				m_aRingD2[vz * PVERTS + vx] = b2;
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD RING: groove reveal input of ring vertex v (after RingPrep): dirt full next to the hole, fading out to
	//! RING_REACH, edge-locked (none at kept terrain / roads). rfD unchanged (and false) when v is not near a dug
	//! vertex or is not part of the dirt.
	protected bool RingDirtD(MUD_FinePatch p, int v, inout float rfD)
	{
		int d2 = m_aRingD2[v];
		if (d2 == 0 || d2 >= 99 || p.m_aEdgeLock[v] <= 0)
			return false;
		float fade = Math.Clamp((RING_REACH - Math.Sqrt(d2) * m_fFine) / (RING_REACH - m_fFine), 0, 1);
		if (fade <= 0)
			return false;
		float rd = -(m_fRevealDepth + RING_SHOW_D * fade) * p.m_aEdgeLock[v];
		if (rd < rfD)
			rfD = rd;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Static mesh (flags 0), a fresh Create on every change (MUD_GroundEntity.c:1560). Surface UVs are
	//! the skin chunk's (0..1 per 64 m), so the terrain-look material lines up with the skin around it.
	protected void BuildMesh(MUD_FinePatch p)
	{
		if (m_bBandMesher && BuildMeshBand(p))
			return;
		int t0 = System.GetTickCount();
		MUD_IdxSet ix = IdxSet(p);
		int chunkPatches = MUD_CoarseSkinEntity.CELLS / SKIN;     // 8 patches per skin chunk side
		int ux = (p.m_iPx - (p.m_iPx / chunkPatches) * chunkPatches) * PCELLS;
		int uz = (p.m_iPz - (p.m_iPz / chunkPatches) * chunkPatches) * PCELLS;
		float uvDiv = chunkPatches * PCELLS;
		bool hideOwed;      // ahead ruts: a hidden vertex still shows an old height
		bool rfOn = RutFieldOn(p);     // RUT FIELD: as BuildMeshBand (band overflow falls back here)
		int rfX0 = p.m_iPx * PCELLS;
		int rfZ0 = p.m_iPz * PCELLS;
		if (!rfOn)
			rfOn = MUD_LookRuts.InBox(rfX0, rfX0 + PCELLS, rfZ0, rfZ0 + PCELLS);     // client look ruts (mesh only)
		bool lift = LiftSetup(p, rfOn);     // SPARSE SHEET A2: drawn higher on untouched covered ground (mesh only)
		bool ringOn = RING_DIRT && m_bGroove && RingPrep(p);     // MUD RING: a hole near this patch
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				float d = p.m_aH[v];
				if (m_bAheadRuts && m_fTime - p.HideAt(v) < HIDE_HOLD)
				{
					if (Math.AbsFloat(d - p.Show(v)) > 0.0005)
						hideOwed = true;
					d = p.Show(v);
				}
				else
				{
					p.SetShow(v, d);
				}
				float rfD = d;     // groove reveal input: a field rut shows soil only from FIELD_SOIL_D deep (as v097)
				float hfD = d;     // RIDGE IN CRATER fix: the heightfield as drawn (m_aSmooth is stale here, see the ridge check)
				if (rfOn)
				{
					float rfF = MUD_LookRuts.MeshFloor(rfX0 + vx, rfZ0 + vz) - p.m_aBase[v];     // + client look ruts
					if (rfF < 0)
						rfF = rfF * p.m_aEdgeLock[v];     // edge lock as a press: 0 at kept terrain (road) -> no gap there
					if (rfF < d)
					{
						d = rfF;
						float rfS = rfF + FIELD_SOIL_D - m_fRevealDepth;
						if (rfS < rfD)
							rfD = rfS;
					}
					// Build 3 Stage 3: the bulldozed ridge on top, edge-locked (none at kept terrain / roads), only where
					// the heightfield is at its undug height (a heightfield crater over it hides it). Same in the 4 paths
					// RIDGE IN CRATER (user shot 2026-10-21, v277 = before): a mesh build runs BEFORE SmoothPatch (FlushBox,
					// queue: collider later), so m_aSmooth still said 'undug' right after a crater and the ridge stood at
					// ground height on the crater floor (walls, mesh only). The drawn height must be undug too.
					if (MUD_VoxelProbe.RUT_RIDGES && Math.AbsFloat(p.m_aSmooth[v]) < MUD_VoxelProbe.RUT_RIDGE_FLAT && Math.AbsFloat(hfD) < MUD_VoxelProbe.RUT_RIDGE_FLAT)
					{
						float rgF = MUD_VoxelProbe.FieldRidge(rfX0 + vx, rfZ0 + vz);
						if (rgF < MUD_VoxelProbe.FLOOR_NONE * 0.5)
						{
							float rgD = (rgF - p.m_aBase[v]) * p.m_aEdgeLock[v];
							if (rgD > d)
								d = rgD;
						}
					}
					if (WEAR_SHOW)
						rfD = WearShowD(p, v, rfX0 + vx, rfZ0 + vz, rfD);     // WEAR SHOW: pass 1 dirt strip (look only)
				}
				bool ring = false;     // MUD RING: dirt next to a hole, surface kept (clutter on the seam)
				if (ringOn && hfD > RING_MIN_H && d > RING_MIN_H)
					ring = RingDirtD(p, v, rfD);
				float h = p.m_aBase[v] + d;
				if (lift)
				{
					h += LiftAt(p, v, d);
					if (m_bFarOn && m_fPressSoil < rfD)
						rfD = m_fPressSoil;     // far press: soil shows where the drawn press is FIELD_SOIL_D deep
				}
				// groove reveal (MUD_GroundEntity.c:1440-1448)
				if (m_bGroove)
				{
					float t = RevealT(-rfD, rfX0 + vx, rfZ0 + vz);     // RAGGED EDGE
					float sk = t * t * (3 - 2 * t) * m_fRevealGap * 3;
					if (ring)
					{
						m_Buf.m_aUnder[v] = Vector(vx * m_fFine, h - m_fRevealGap + sk, vz * m_fFine);     // MUD RING: dirt up, surface stays
					}
					else
					{
						m_Buf.m_aUnder[v] = Vector(vx * m_fFine, h - m_fRevealGap, vz * m_fFine);
						h -= sk;
					}
				}
				m_Buf.m_aVerts[v] = Vector(vx * m_fFine, h, vz * m_fFine);
				m_Buf.m_aUVs[v * 2] = (ux + vx) / uvDiv;
				m_Buf.m_aUVs[v * 2 + 1] = 1 - (uz + vz) / uvDiv;
			}
		}

		Resource res;
		WallQuads(p, m_bGroove);     // STEEP WALLS
		if (m_bGroove)
		{
			res = PatchMeshCreate(p, 1089, 6144, 1089, 6144, true);
			if (!res || !res.GetResource())
			{
				Print(string.Format("MUD world: groove material '%1' did not load - groove layer off", m_RutMaterial), LogLevel.ERROR);
				m_bGroove = false;
				p.m_bMeshDirty = true;
				p.m_fMeshBuiltAt = -100;     // REBUILD 5: the groove layer goes off - no same skip against the old mesh
				return;
			}
		}
		else
		{
			res = PatchMeshCreate(p, 1089, 6144, 0, 0, false);
		}
		if (!res || !res.GetResource())
		{
			Print(string.Format("MUD world: MeshObject.Create failed - material '%1' did not load?", m_Material), LogLevel.ERROR);
			m_bRender = false;
			return;
		}

		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals + tangents from the triangle list
		mesh.UpdateIndices(0, ix.m_Mesh.m_aIdx);
		mesh.UpdateVerts(0, m_Buf.m_aVerts, m_Buf.m_aUVs);
		if (m_bGroove)
		{
			mesh.UpdateIndices(1, ix.m_Mesh.m_aIdx);
			mesh.UpdateVerts(1, m_Buf.m_aUnder, m_Buf.m_aGrooveUVs);
		}
		PatchMeshWalls(mesh, m_bGroove);     // STEEP WALLS
		ShowMesh(p, res, mesh);     // CRATER HOLD: or held until the crater's last patch is built
		if (m_iMeshBuilds < 3)    // VC flags readback probe (MUD_VoxelProbe.s_iVcLog), log only
			Print(string.Format("MUD vcflags: ground patch %1 (8 = NOSHADOW)", p.m_Ent.SetVComponentFlags(0)), LogLevel.NORMAL);
		p.m_bMeshDirty = hideOwed;     // ahead ruts: rebuild again (Mesh Interval) until every hidden change shows
		if (p.m_fLookCutAt >= 0)
			MUD_LookRuts.NoteBuilt(m_fTime - p.m_fLookCutAt, m_bQueueMesh);     // TEMP split delay meter
		p.m_fLookCutAt = -1;
		p.m_bLookDirty = false;
		WhyClear(p);     // REBUILD 6
		p.m_fLookSince = -1;     // TEMP delay meter
		m_iMeshBuilds++;
		p.m_fMeshBuiltAt = -100;     // PERF IDEA 2: m_aMeshY is not this mesh
		int gridMs = System.GetTickCount(t0);
		m_iMeshMs += gridMs;
		m_iGridMeshN++;
		m_iGridMeshMs += gridMs;
		if (gridMs > m_iGridMeshWorst)
			m_iGridMeshWorst = gridMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Dedicated server: a 2-triangle mesh spanning the patch so traces find it (MUD_BoundsMesh.c).
	//! Built once: 30 m below the lowest base covers floor, slab and plate (depth boxes deeper than ~25 m
	//! would need more), 5 m above the highest covers rims and berms.
	protected void BoundsMesh(MUD_FinePatch p)
	{
		float yLo = p.m_aBase[0];
		float yHi = yLo;
		for (int ib = 0; ib < 1089; ib++)
		{
			float hb = p.m_aBase[ib];
			if (hb < yLo)
				yLo = hb;
			if (hb > yHi)
				yHi = hb;
		}
		float side = PCELLS * m_fFine;
		p.m_MeshRes = MUD_BoundsMesh.Make(p.m_Ent, Vector(0, yLo - 30, 0), Vector(side, yHi + 5, side), m_Material);
		p.m_fMeshBuiltAt = -100;     // REBUILD 5: not a band mesh - no skip / partial pass against it
		if (!p.m_MeshRes)
			Print(string.Format("MUD world: bounds mesh failed - material '%1'", m_Material), LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 4: mask bits (cz * SKIN + cx) of every skin cell that touches fine vertex vx, vz
	protected int RutMudCells(int vx, int vz)
	{
		int x0 = vx - 1;
		if (x0 < 0)
			x0 = 0;
		x0 = x0 / FINE;
		int x1 = vx / FINE;
		if (x1 >= SKIN)
			x1 = SKIN - 1;
		int z0 = vz - 1;
		if (z0 < 0)
			z0 = 0;
		z0 = z0 / FINE;
		int z1 = vz / FINE;
		if (z1 >= SKIN)
			z1 = SKIN - 1;
		int mask;
		for (int cz = z0; cz <= z1; cz++)
		{
			for (int cx = x0; cx <= x1; cx++)
				mask = mask | (1 << (cz * SKIN + cx));
		}
		return mask;
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 4: rut mud is one more surface piece - false (and one debug line) when the patch already
	//! uses MAT_SLOTS surfaces (+ slab + plate = MakeStatic's 6)
	protected bool RutMudFits(MUD_FinePatch p, int used)
	{
		if (used < MAT_SLOTS)
			return true;
		if (m_bDebug && MUD_VoxelProbe.OP_PRINTS && p.m_iRutMud != -1)
			Print(string.Format("MUD rutmat: patch %1 full (%2 surfaces) - no rut mud", p.m_iKey, used));
		p.m_iRutMud = -1;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 4: debug line when a patch's rut mud cells change
	protected void RutMudLog(MUD_FinePatch p, int mudMask, int tris, string path)
	{
		if (mudMask == p.m_iRutMud || (mudMask == 0 && p.m_iRutMud <= 0))
			return;
		if (m_bDebug && MUD_VoxelProbe.OP_PRINTS)
		{
			int cells;
			for (int b = 0; b < 16; b++)
			{
				if ((mudMask & (1 << b)) != 0)
					cells++;
			}
			Print(string.Format("MUD rutmat: patch %1 mud cells %2 (mask %3) tris %4 (%5)", p.m_iKey, cells, mudMask, tris, path));
		}
		p.m_iRutMud = mudMask;
	}

	//------------------------------------------------------------------------------------------------
	//! Build 3 Stage 4: split one slot list into m_Buf.m_RutRest (cells outside mudMask) and m_Buf.m_RutMud
	//! (appended: one mud piece for every slot). FillIndices starts every fine cell's 6 indices with its bl vertex.
	protected void SplitRutMud(MUD_IdxList sl, int mudMask)
	{
		MUD_IdxList rest = m_Buf.m_RutRest;
		MUD_IdxList mud = m_Buf.m_RutMud;
		rest.m_iN = 0;
		for (int i = 0; i < sl.m_iN; i += 6)
		{
			int bl = sl.m_aIdx[i];
			int fz = bl / PVERTS;
			int fx = bl - fz * PVERTS;
			int sc = (fz / FINE) * SKIN + fx / FINE;
			MUD_IdxList dst = rest;
			if ((mudMask & (1 << sc)) != 0)
				dst = mud;
			for (int j = 0; j < 6; j++)
			{
				dst.m_aIdx[dst.m_iN] = sl.m_aIdx[i + j];
				dst.m_iN++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! One trimesh per surface (<= 4) from the smoothed heights, + bullet slab (FireGeo, facing down)
	//! + safety plate (base + floor - m_fPlateGap). Old body goes and the new one comes in this call
	//! (MUD_GroundEntity.c:1643).
	protected void BuildCollider(MUD_FinePatch p)
	{
		if (m_bBandMesher && BuildColliderBand(p))
			return;
		int t0 = System.GetTickCount();
		MUD_IdxSet ix = IdxSet(p);
		// wheel hold (P1a): a vert under a wheel keeps the height the live body has
		bool hold = m_fWheelHold > 0 && p.m_bColValid && p.m_Phys != null;
		bool rfOn = RutFieldOn(p);     // RUT FIELD: y = base + min(smooth, floor - base), the floor not blurred
		int rfX0 = p.m_iPx * PCELLS;
		int rfZ0 = p.m_iPz * PCELLS;
		bool hadBody = p.m_bColValid && p.m_Phys != null;   // TEMP probe 2026-10-11
		bool moved;
		int held;
		bool rutMud = rfOn && RUT_MUD && !m_RutMudMaterial.IsEmpty();
		int mudMask;     // Build 3 Stage 4: bit per skin cell (cz * SKIN + cx) that is rut mud
		bool coarsePlate = COARSE_PLATE && p.m_bFlatFloor;     // REBUILD 6 (PlateGeom)
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				float y = p.m_aBase[v] + p.m_aSmooth[v];
				if (rfOn)
				{
					float rfF = MUD_VoxelProbe.FieldFloor(rfX0 + vx, rfZ0 + vz);
					if (rfF < p.m_aBase[v])
						rfF = p.m_aBase[v] + (rfF - p.m_aBase[v]) * p.m_aEdgeLock[v];     // edge lock, as the mesh
					if (rfF < y)
						y = rfF;
					// Build 3 Stage 4: every skin cell touching this vertex is rut mud (FLOOR_NONE never passes)
					if (rutMud && rfF <= p.m_aBase[v] - RUT_MAT_D)
						mudMask = mudMask | RutMudCells(vx, vz);
					// Build 3 Stage 3: the ridge on top, as the mesh
					if (MUD_VoxelProbe.RUT_RIDGES && Math.AbsFloat(p.m_aSmooth[v]) < MUD_VoxelProbe.RUT_RIDGE_FLAT)
					{
						float rgF = MUD_VoxelProbe.FieldRidge(rfX0 + vx, rfZ0 + vz);
						if (rgF < MUD_VoxelProbe.FLOOR_NONE * 0.5)
						{
							rgF = p.m_aBase[v] + (rgF - p.m_aBase[v]) * p.m_aEdgeLock[v];
							if (rgF > y)
								y = rgF;
						}
					}
				}
				if (hold && y != p.m_aColH[v] && m_fTime - Math.Max(p.RutTouch(v), p.BermTouch(v)) < m_fWheelHold)
				{
					y = p.m_aColH[v];
					held++;
				}
				if (hadBody && Math.AbsFloat(y - p.m_aColH[v]) > 0.001)
					moved = true;
				p.m_aColH[v] = y;
				m_Buf.m_aVerts[v] = Vector(vx * m_fFine, y, vz * m_fFine);
				if (!coarsePlate)     // REBUILD 6: the coarse plate reads base + floor itself
					m_Buf.m_aPlate[v] = Vector(vx * m_fFine, p.m_aBase[v] + p.m_aFloor[v] - m_fPlateGap, vz * m_fFine);
				m_Buf.m_aShell[v] = m_Buf.m_aVerts[v] - vector.Up * m_fBulletSlab;
			}
		}

		array<PhysicsGeom> geoms = {};
		array<ResourceName> mats = {};
		array<int> layers = {};
		// Terrain preset + Foliage: vehicle get in/out sweep skips Foliage (MUD_GroundEntity.c:1668-1676)
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;
		// Build 3 Stage 4: rut mud = one more surface piece, so only while the patch uses < MAT_SLOTS surfaces
		if (mudMask != 0)
		{
			int used;
			for (int ku = 0; ku < p.m_aMats.Count() && ku < ix.m_aSlot.Count(); ku++)
			{
				if (ix.m_aSlot[ku].m_iN > 0)
					used++;
			}
			if (!RutMudFits(p, used))
				mudMask = 0;
		}
		m_Buf.m_RutMud.m_iN = 0;
		for (int k = 0; k < p.m_aMats.Count() && k < ix.m_aSlot.Count(); k++)
		{
			MUD_IdxList sl = ix.m_aSlot[k];
			if (sl.m_iN == 0)
				continue;
			if (mudMask != 0)
			{
				SplitRutMud(sl, mudMask);
				sl = m_Buf.m_RutRest;
				if (sl.m_iN == 0)
					continue;
			}
			PhysicsGeom geom = PhysicsGeom.CreateTriMesh(m_Buf.m_aVerts, sl.m_aIdx, 1089, sl.m_iN);
			if (!geom)
			{
				Print("MUD world: CreateTriMesh failed", LogLevel.ERROR);
				continue;
			}
			geoms.Insert(geom);
			mats.Insert(p.m_aMats[k]);
			layers.Insert(layer);
		}
		if (m_Buf.m_RutMud.m_iN > 0)
		{
			PhysicsGeom mudGeom = PhysicsGeom.CreateTriMesh(m_Buf.m_aVerts, m_Buf.m_RutMud.m_aIdx, 1089, m_Buf.m_RutMud.m_iN);
			if (mudGeom)
			{
				geoms.Insert(mudGeom);
				mats.Insert(m_RutMudMaterial);
				layers.Insert(layer);
			}
			else
			{
				Print("MUD world: CreateTriMesh failed (rut mud)", LogLevel.ERROR);
			}
		}
		RutMudLog(p, mudMask, m_Buf.m_RutMud.m_iN / 3, "grid");

		// slab and plate: every non-kept cell
		if (m_fBulletSlab > 0 && ix.m_Shell.m_iN > 0 && !geoms.IsEmpty())
		{
			PhysicsGeom shell = PhysicsGeom.CreateTriMesh(m_Buf.m_aShell, ix.m_Shell.m_aIdx, 1089, ix.m_Shell.m_iN);
			if (shell)
			{
				geoms.Insert(shell);
				mats.Insert(p.m_aMats[0]);
				layers.Insert(EPhysicsLayerPresets.FireGeo);
			}
		}
		if (ix.m_Plate.m_iN > 0 && !geoms.IsEmpty())
		{
			PhysicsGeom plate = PlateGeom(p, ix);     // REBUILD 6 COARSE_PLATE
			if (plate)
			{
				geoms.Insert(plate);
				mats.Insert(m_GameMaterial);
				layers.Insert(layer);
			}
		}

		int td = System.GetTickCount();
		DropBody(p);
		m_iLastDropMs = System.GetTickCount(td);
		m_iLastStaticMs = 0;
		if (!geoms.IsEmpty())
		{
			int tsx = System.GetTickCount();
			p.m_Phys = MakeStatic(p.m_Ent, geoms, mats, layers);
			m_iLastStaticMs = System.GetTickCount(tsx);
			if (geoms.Count() > m_iMaxGeoms)
				m_iMaxGeoms = geoms.Count();
			if (p.m_Phys)
			{
				p.m_aGeoms.Copy(geoms);
			}
			else
			{
				Print("MUD world: CreateStaticEx failed", LogLevel.ERROR);
				foreach (PhysicsGeom g : geoms)
					g.Destroy();
			}
		}
		p.m_bColDirty = false;
		p.m_bColValid = p.m_Phys != null;
		p.m_fColBuiltAt = m_fTime;
		if (moved)
			p.m_fColChangeAt = m_fTime;
		if (held > 0 && p.m_Phys)
		{
			// the held verts take their new height at a later rebuild (m_fColliderInterval on)
			m_iHeldVerts += held;
			m_iHeldBuilds++;
			p.m_bColDirty = true;
			if (!p.m_bQueued)
			{
				p.m_bQueued = true;
				p.m_fQueuedAt = m_fTime;
				m_aDirty.Insert(p.m_iKey);
			}
		}
		m_iColBuilds++;
		int gridMs = System.GetTickCount(t0);
		m_iColMs += gridMs;
		m_iGridColN++;
		m_iGridColMs += gridMs;
		if (gridMs > m_iGridColWorst)
			m_iGridColWorst = gridMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Layered-SDF test: m_Band.m_aSkin from p's skin cells (-1 = kept terrain or voxel, else collider slot)
	protected void FillBandOn(MUD_FinePatch p)
	{
		for (int c = 0; c < SKIN * SKIN; c++)
		{
			int cell = p.m_aCell[c];
			int slot = -1;
			if (cell >= 0 && (cell & VOX) == 0)
				slot = cell & SLOT;
			m_Band.m_aSkin[c] = slot;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Layered-SDF test: per-build counts of the mesher for the 'MUD band' stats line
	protected void BandCount()
	{
		int tris = m_Band.m_iI / 3;
		m_iBandCells += m_Band.m_iCells;
		if (m_Band.m_iCells > m_iBandCellsMax)
			m_iBandCellsMax = m_Band.m_iCells;
		m_iBandTris += tris;
		if (tris > m_iBandTrisMax)
			m_iBandTrisMax = tris;
		m_iBandVerts += m_Band.m_iV;
		m_iBandSkirtTris += m_Band.m_iSkirtI / 3;
		m_iBandDegen += m_Band.m_iDegen;
		m_iBandMissing += m_Band.m_iMissing;
		m_iBandFastV += m_Band.m_iFastV;
		m_iBandFastQ += m_Band.m_iFastQ;
	}

	//------------------------------------------------------------------------------------------------
	//! stage 3 Part 0 B meter (TEST_KIT, sheet local, print only): a revealed (dirt) vert of this band mesh, local q
	protected void BRevNote(vector q)
	{
		m_iBRev++;
		if (m_iBRevOut > 0 && !MUD_TestKit.TEST_KIT)
			return;     // B9: the groove stays - no more queries
		float x = m_fBWx0 + q[0];
		float z = m_fBWz0 + q[2];
		if (!MUD_MudSheet.FullCoverAt(x, z))
		{
			m_iBRevOut++;
		}
		else if (GROOVE_UNDER_OFF && m_iBRevOut == 0)
		{
			m_aBRevP.Insert(x);     // stage 3 B9: watched while the groove section is off
			m_aBRevP.Insert(z);
		}
	}

	//! stage 3 B9: draw the groove section this build? Off when every revealed vert is under the sheet's full cover
	protected void BGrooveDecide()
	{
		m_bGrooveNow = m_bGroove;
		if (GROOVE_UNDER_OFF && m_bBMeter && m_iBRev > 0 && m_iBRevOut == 0)
			m_bGrooveNow = false;
	}

	//! B9: a band mesh is built - list it (its revealed verts) when its groove section is off, else unlist it
	protected void BWatchSet(MUD_FinePatch p)
	{
		if (m_bGroove && !m_bGrooveNow)
		{
			m_iBGrDrop++;
			array<float> pts = new array<float>();
			pts.Copy(m_aBRevP);
			if (!m_mBWatch.Contains(p.m_iKey))
				m_aBWatchK.Insert(p.m_iKey);
			m_mBWatch.Set(p.m_iKey, pts);
		}
		else if (m_mBWatch.Contains(p.m_iKey))
		{
			BWatchDrop(p.m_iKey);
		}
	}

	protected void BWatchDrop(int key)
	{
		m_mBWatch.Remove(key);
		int ix = m_aBWatchK.Find(key);
		if (ix >= 0)
			m_aBWatchK.Remove(ix);
	}

	//! B9 / B10: re-check one listed patch per frame; a vert out of the sheet's full cover (tile dropped / hidden, crater,
	//! window moved) rebuilds that patch in full. Both switches off or sheet local off: every listed patch rebuilds now.
	protected void BWatchTick()
	{
		if ((!GROOVE_UNDER_OFF && !MESH_UNDER_HOLD) || !MUD_MudSheet.LocalOn())
		{
			for (int k = 0; k < m_aBWatchK.Count(); k++)
				BWatchRebuild(m_aBWatchK[k]);
			m_aBWatchK.Clear();
			m_mBWatch.Clear();
			return;
		}
		if (m_iBWatchI >= m_aBWatchK.Count())
			m_iBWatchI = 0;
		int key = m_aBWatchK[m_iBWatchI];
		array<float> pts = m_mBWatch.Get(key);
		bool lost = !pts;
		if (pts)
		{
			int end = m_iBWatchP + BWATCH_PTS * 2;
			if (end > pts.Count())
				end = pts.Count();
			for (int i = m_iBWatchP; i + 1 < end; i += 2)
			{
				if (!MUD_MudSheet.FullCoverAt(pts[i], pts[i + 1]))
				{
					lost = true;
					break;
				}
			}
			m_iBWatchP = end;
			if (!lost && end < pts.Count())
				return;     // this patch goes on next frame
		}
		m_iBWatchP = 0;
		if (!lost)
		{
			m_iBWatchI++;
			return;
		}
		BWatchRebuild(key);
		BWatchDrop(key);
	}

	//! B9 / B10: queue a full rebuild of patch key (no same / small skip, no B10 hold: m_fMeshBuiltAt -100)
	protected void BWatchRebuild(int key)
	{
		m_iBWatchRe++;
		MUD_FinePatch p = m_mPatches.Get(key);
		if (!p || !m_bRender)
			return;
		p.m_bMeshDirty = true;     // as VoxLaterStep's queued branch
		p.m_bMBoxAll = true;
		p.m_bRutMesh = false;
		p.m_bHMesh = true;
		p.m_fMeshBuiltAt = -100;
		if (LOOK_PACE)
			p.m_fMeshHoldTo = -100;
		if (!p.m_bQueued)
		{
			p.m_bQueued = true;
			p.m_fQueuedAt = m_fTime;
			m_aDirty.Insert(p.m_iKey);
		}
	}

	//! B meter: this band mesh build is done
	protected void BBuildNote()
	{
		m_bBMeter = false;
		m_iBBuilds++;
		if (m_iBRev == 0)
			m_iBNone++;
		else if (m_iBRevOut == 0)
			m_iBAllCov++;
		else
			m_iBSome++;
	}

	//! stage 3 Part 0 B10 meter (switch not built): would this queued rebuild be held? The shown mesh is the kept input,
	//! same skin classes, nothing hidden owed, and every changed vert is under the sheet's full cover with its shown
	//! height (less its groove sink) at least 2 cm under the new height + LIFT. Meter only: the build goes on.
	protected bool BHoldMeter(MUD_FinePatch p, bool hideOwed, int rfX0, int rfZ0)
	{
		m_iBQueued++;
		if (hideOwed || !p.m_aMeshY || !p.m_aMeshD || !p.m_aMeshSkin || !p.m_MeshRes || p.m_fMeshBuiltAt < -50)
			return false;
		for (int c = 0; c < SKIN * SKIN; c++)
		{
			if (m_Band.m_aSkin[c] != p.m_aMeshSkin[c])
				return false;
		}
		m_aBHoldP.Clear();
		float sink = m_fRevealGap * 3;
		float wx0 = m_fX0 + rfX0 * m_fFine;
		float wz0 = m_fZ0 + rfZ0 * m_fFine;
		int changed = 0;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				if (m_Band.m_aY[v] == p.m_aMeshY[v] && m_Band.m_aD[v] == p.m_aMeshD[v])
					continue;
				changed++;
				float hx = wx0 + vx * m_fFine;
				float hz = wz0 + vz * m_fFine;
				if (!MUD_MudSheet.FullCoverAt(hx, hz))
					return false;
				float s = 0;
				if (m_bGroove)
				{
					float t = RevealT(-p.m_aMeshD[v], rfX0 + vx, rfZ0 + vz);
					if (t > 1)
						t = 1;
					if (t > 0)
						s = t * t * (3 - 2 * t) * sink;
				}
				if (p.m_aMeshY[v] - s > m_Band.m_aY[v] + MUD_MudSheet.LIFT - 0.02)
					return false;
				if (MESH_UNDER_HOLD)
				{
					m_aBHoldP.Insert(hx);     // stage 3 B10: watched while held
					m_aBHoldP.Insert(hz);
				}
			}
		}
		if (changed == 0)
			return false;
		m_iBHold++;
		m_iBHoldVerts += changed;
		return true;
	}

	//! stage 3 B10: room for this hold's verts in the patch's watch list (full: the rebuild goes on and resets the list)
	protected bool BWatchRoom(MUD_FinePatch p)
	{
		array<float> pts = m_mBWatch.Get(p.m_iKey);
		return !pts || pts.Count() + m_aBHoldP.Count() <= BWATCH_MAX;
	}

	//! B10: a held build's changed verts join the patch's watch list
	protected void BWatchAdd(MUD_FinePatch p)
	{
		array<float> pts = m_mBWatch.Get(p.m_iKey);
		if (!pts)
		{
			pts = new array<float>();
			m_mBWatch.Set(p.m_iKey, pts);
			m_aBWatchK.Insert(p.m_iKey);
		}
		for (int i = 0; i < m_aBHoldP.Count(); i++)
			pts.Insert(m_aBHoldP[i]);
	}

	//! B meter 10 s line, then reset
	protected void BUnderLog()
	{
		if (!MUD_TestKit.TEST_KIT || (m_iBBuilds == 0 && m_iBQueued == 0))
			return;
		Print(string.Format("MUD world B under sheet 10 s: band mesh builds metered %1 - none revealed %2, revealed all under full cover %3, some revealed uncovered %4 | groove section upload %5 ms | B10 would hold %6 of %7 queued (changed verts %8)",
			m_iBBuilds, m_iBNone, m_iBAllCov, m_iBSome, m_iBGrMs, m_iBHold, m_iBQueued, m_iBHoldVerts)
			+ string.Format(" | B9 groove section off %1 builds, watch list %2 patches, rebuilt from it %3 (GROOVE_UNDER_OFF %4) | B10 held %5 (MESH_UNDER_HOLD %6)",
			m_iBGrDrop, m_aBWatchK.Count(), m_iBWatchRe, GROOVE_UNDER_OFF, m_iBHeld, MESH_UNDER_HOLD), LogLevel.NORMAL);
		m_iBGrDrop = 0;
		m_iBHeld = 0;
		m_iBWatchRe = 0;
		m_iBBuilds = 0;
		m_iBNone = 0;
		m_iBAllCov = 0;
		m_iBSome = 0;
		m_iBGrMs = 0;
		m_iBHold = 0;
		m_iBQueued = 0;
		m_iBHoldVerts = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Layered-SDF test (Band Mesher on): BuildMesh from the band surface-nets mesher. Same display rule
	//! (ahead ruts), groove reveal per vertex from the interpolated deformation, skirts in submesh 0 (a separate
	//! skirt submesh crashed WB, mud-gotchas 2026-10-10). False = overflow or no triangle: BuildMesh builds the grid.
	protected bool BuildMeshBand(MUD_FinePatch p)
	{
		int t0 = System.GetTickCount();
		if (!m_Band)
			m_Band = new MUD_BandMesher();
		m_Band.m_bMerge = false;     // REBUILD 5 option 4: the drawn mesh never merges (collider only)
		int csIn = System.GetTickCount();
		int inT = csIn;     // OPT PASS: input split
		// RUT FIELD: the rut floor goes into the mesher INPUTS (d = min(m_aH, floor - base)), never into m_aH;
		// the groove reveal then works for field ruts too. m_aShow keeps the m_aH value (no floor): ahead-rut hold.
		bool rfOn = RutFieldOn(p);
		int rfX0 = p.m_iPx * PCELLS;
		int rfZ0 = p.m_iPz * PCELLS;
		if (!rfOn)
			rfOn = MUD_LookRuts.InBox(rfX0, rfX0 + PCELLS, rfZ0, rfZ0 + PCELLS);     // client look ruts (mesh only)
		m_iInRf += System.GetTickCount(inT);
		inT = System.GetTickCount();
		bool bLift = LiftSetup(p, rfOn);     // SPARSE SHEET A2, as BuildMesh
		m_iInLift += System.GetTickCount(inT);
		inT = System.GetTickCount();
		if (bLift)
			m_iInLiftN++;
		if (bLift && m_bLookCap)
			m_iInCapN++;
		if (rfOn)
		{
			m_iInRfN++;
			MUD_LookRuts.PatchBegin(rfX0, rfZ0, PVERTS - 1);     // perf plan B2: tiles once per patch
			BandFill(rfX0, rfZ0, true);     // PERF IDEA 3: floor + look + ridge of every vertex, one call
		}
		m_iInRf += System.GetTickCount(inT);
		inT = System.GetTickCount();
		// REBUILD 5 option 3: the kept input of the shown mesh + only the box marked since (PartialOk); else every vertex
		bool part = PartialOk(p, rfOn, bLift);
		int bx0 = 0;
		int bx1 = PCELLS;
		int bz0 = 0;
		int bz1 = PCELLS;
		if (part)
		{
			for (int k = 0; k < PVERTS * PVERTS; k++)
			{
				m_Band.m_aY[k] = p.m_aMeshY[k];
				m_Band.m_aD[k] = p.m_aMeshD[k];
			}
			bx1 = -1;     // no box marked: nothing to read (the same skip then catches it)
			bz1 = -1;
			if (p.m_iMBx0 >= 0)
			{
				bx0 = p.m_iMBx0;
				bx1 = p.m_iMBx1;
				bz0 = p.m_iMBz0;
				bz1 = p.m_iMBz1;
			}
		}
		m_iInPart += System.GetTickCount(inT);
		inT = System.GetTickCount();
		bool hideOwed = BandMeshIn(p, rfOn, bLift, bx0, bx1, bz0, bz1);
		m_iInLoop += System.GetTickCount(inT);
		if (part)
		{
			m_iPartN++;
			m_iPartVerts += (bx1 - bx0 + 1) * (bz1 - bz0 + 1);
		}
		else
		{
			m_iFullN++;
		}
		inT = System.GetTickCount();
		FillBandOn(p);     // before the skips: MeshSame / KeepMeshY read m_aSkin
		m_iInFill += System.GetTickCount(inT);
		int inMs = System.GetTickCount(csIn);
		m_iCsMeshIn += inMs;
		if (part)
			m_iPartMs += inMs;
		else
			m_iFullMs += inMs;
		if (SAME_SKIP && m_bMeshGate && MeshSame(p))
		{
			// REBUILD 5 option 2: same input = same mesh (RevealT, UVs: from D and position only) - the shown one stays
			m_iSameSkip++;
			m_bMeshSkipped = true;
			p.m_bMeshDirty = hideOwed;
			p.m_fLookCutAt = -1;
			p.m_bLookDirty = false;
			WhyClear(p);     // REBUILD 6
			p.m_fLookSince = -1;
			MeshDone(p, hideOwed, rfOn);
			return true;
		}
		if (m_bMeshGate && MESH_SKIP_SMALL && MeshSmall(p))
		{
			m_iMeshSkipSmall++;
			m_bMeshSkipped = true;
			p.m_fMeshHoldTo = p.m_fMeshBuiltAt + MESH_SMALL_MAX_S;     // fix (run 8): the input pass is ~1.2 ms - retry once, at the window end
			return true;     // PERF IDEA 2: old mesh kept, m_bMeshDirty / m_bLookDirty stay - built on a later pass
		}
		if ((MUD_TestKit.TEST_KIT || MESH_UNDER_HOLD) && m_bQueueMesh && MUD_MudSheet.LocalOn())
		{
			// stage 3 Part 0 B10 'would hold' meter; B10 MESH_UNDER_HOLD: the shown mesh stays (the sheet draws over it).
			// No KeepMeshY / MeshDone: the kept input and the changed box stay for the next real build.
			if (BHoldMeter(p, hideOwed, rfX0, rfZ0) && MESH_UNDER_HOLD && BWatchRoom(p))
			{
				m_iBHeld++;
				m_bMeshSkipped = true;
				p.m_bMeshDirty = false;
				p.m_fLookCutAt = -1;
				p.m_bLookDirty = false;
				WhyClear(p);
				p.m_fLookSince = -1;
				BWatchAdd(p);
				return true;
			}
		}
		KeepMeshY(p);
		// SKIRT DEPTH (2026-10-18, user: see-through gaps while driving): a look rut drawn on one patch before its
		// neighbour opens a crack at the seam as deep as the rut (up to Rut Max Depth 0.35) + the groove sink
		// (Reveal Gap x 3); the 0.3 m skirt was too short for a heavy truck. Same tri count, so no extra cost.
		float skirt = Math.Max(m_fBandSkirt, m_fRutMaxDepth + m_fRevealGap * 3 + 0.1);
		int tc = System.GetTickCount();
		bool ok = BandCore(skirt);
		m_iBandCoreMs += System.GetTickCount(tc);
		m_iCsMeshCore += System.GetTickCount(tc);
		int csUv = System.GetTickCount();
		if (!ok)
		{
			if (m_Band.m_bOverflow)
				m_iBandFallback++;     // else: no on cell (all kept terrain) - the grid collapses it the same way
			return false;
		}

		int chunkPatches = MUD_CoarseSkinEntity.CELLS / SKIN;     // 8 patches per skin chunk side
		int ux = (p.m_iPx - (p.m_iPx / chunkPatches) * chunkPatches) * PCELLS;
		int uz = (p.m_iPz - (p.m_iPz / chunkPatches) * chunkPatches) * PCELLS;
		float uvDiv = chunkPatches * PCELLS;
		int nv = m_Band.m_iV;
		int nAll = m_Band.m_iVAll;
		// constants out of the loop (the loop runs ~1500x per build)
		float invFine = 1.0 / m_fFine;
		float invPatch = 1.0 / PCELLS;
		float invDiv = 1.0 / uvDiv;
		float blend = m_fRevealBlend;
		if (blend < 0.01)
			blend = 0.01;
		float invBlend = 1.0 / blend;
		float sink = m_fRevealGap * 3;
		m_bBMeter = (MUD_TestKit.TEST_KIT || GROOVE_UNDER_OFF) && m_bGroove && MUD_MudSheet.LocalOn();     // stage 3 Part 0 B meter, B9
		m_iBRev = 0;
		m_iBRevOut = 0;
		m_aBRevP.Clear();
		m_fBWx0 = m_fX0 + rfX0 * m_fFine;
		m_fBWz0 = m_fZ0 + rfZ0 * m_fFine;
		for (int i = 0; i < nAll; i++)
		{
			vector q = m_Band.m_Vb.m_aV[i];
			float qx = q[0] * invFine;
			float qz = q[2] * invFine;
			int i2 = i * 2;
			if (m_bGroove)
			{
				if (i < nv)
				{
					m_Band.m_Aux.m_aV[i] = Vector(q[0], q[1] - m_fRevealGap, q[2]);
					m_Band.m_Uv.m_aUnder[i2] = qx * invPatch;
					m_Band.m_Uv.m_aUnder[i2 + 1] = 1 - qz * invPatch;
				}
				// groove reveal (BuildMesh), from the deformation interpolated at the vertex; most verts have t = 0
				float t = RevealT(-m_Band.m_Info.m_aD[i], rfX0 + qx, rfZ0 + qz);     // RAGGED EDGE (was * invBlend)
				if (t > 0)
				{
					if (t > 1)
						t = 1;
					if (m_bBMeter)
						BRevNote(q);
					m_Band.m_Vb.m_aV[i] = Vector(q[0], q[1] - t * t * (3 - 2 * t) * sink, q[2]);
				}
			}
			m_Band.m_Uv.m_aTop[i2] = (ux + qx) * invDiv;
			m_Band.m_Uv.m_aTop[i2 + 1] = 1 - (uz + qz) * invDiv;
		}

		BGrooveDecide();     // stage 3 B9
		m_iCsMeshUv += System.GetTickCount(csUv);
		int csRes = System.GetTickCount();
		int ni = m_Band.m_iI;
		int niAll = ni + m_Band.m_iSkirtI;
		Resource res;
		WallQuads(p, m_bGrooveNow);     // STEEP WALLS
		if (m_bGrooveNow)
		{
			res = PatchMeshCreate(p, nAll, niAll, nv, ni, true);
			if (!res || !res.GetResource())
			{
				Print(string.Format("MUD world: groove material '%1' did not load - groove layer off", m_RutMaterial), LogLevel.ERROR);
				m_bGroove = false;
				p.m_bMeshDirty = true;
				p.m_fMeshBuiltAt = -100;     // REBUILD 5: the kept input (KeepMeshY) is not the shown mesh
				return true;
			}
		}
		else
		{
			res = PatchMeshCreate(p, nAll, niAll, 0, 0, false);
		}
		if (!res || !res.GetResource())
		{
			Print(string.Format("MUD world: MeshObject.Create failed - material '%1' did not load?", m_Material), LogLevel.ERROR);
			m_bRender = false;
			return true;
		}

		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals + tangents from the triangle list
		mesh.UpdateIndices(0, m_Band.m_Ib.m_aI);
		mesh.UpdateVerts(0, m_Band.m_Vb.m_aV, m_Band.m_Uv.m_aTop);
		if (m_bGrooveNow)
		{
			m_iBGrT = System.GetTickCount();
			mesh.UpdateIndices(1, m_Band.m_Ib.m_aI);       // its first ni: the surface without skirts
			mesh.UpdateVerts(1, m_Band.m_Aux.m_aV, m_Band.m_Uv.m_aUnder);
			if (m_bBMeter)
				m_iBGrMs += System.GetTickCount(m_iBGrT);     // stage 3 Part 0 B meter
		}
		PatchMeshWalls(mesh, m_bGrooveNow);     // STEEP WALLS
		ShowMesh(p, res, mesh);     // CRATER HOLD: or held until the crater's last patch is built
		m_iCsMeshRes += System.GetTickCount(csRes);
		p.m_bMeshDirty = hideOwed;     // ahead ruts: rebuild again (Mesh Interval) until every hidden change shows
		if (p.m_fLookCutAt >= 0)
			MUD_LookRuts.NoteBuilt(m_fTime - p.m_fLookCutAt, m_bQueueMesh);     // TEMP split delay meter
		p.m_fLookCutAt = -1;
		p.m_bLookDirty = false;
		WhyClear(p);     // REBUILD 6
		p.m_fLookSince = -1;     // TEMP delay meter
		p.m_fMeshBuiltAt = m_fTime;     // PERF IDEA 2: m_aMeshY (KeepMeshY above) is this mesh
		MeshDone(p, hideOwed, rfOn);     // REBUILD 5
		m_iMeshBuilds++;
		BWatchSet(p);     // stage 3 B9 (before BBuildNote clears m_bBMeter)
		if (m_bBMeter)
			BBuildNote();
		int ms = System.GetTickCount(t0);
		m_iMeshMs += ms;
		m_iBandMeshN++;
		m_iBandMeshMs += ms;
		if (ms > m_iBandMeshWorst)
			m_iBandMeshWorst = ms;
		BandCount();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! BuildMeshBand's input pass over patch-local vertex rows z0..z1, columns x0..x1 (full = 0..PCELLS, the same order
	//! as before REBUILD 5): m_Band.m_aY / m_aD there, after BandFill. True = a hidden change is owed (ahead ruts).
	protected bool BandMeshIn(MUD_FinePatch p, bool rfOn, bool bLift, int x0, int x1, int z0, int z1)
	{
		bool hideOwed = false;
		MUD_PatchPress pr = p.m_Press;     // PERF IDEA 3: HideAt / Show / SetShow inlined (null = their defaults)
		bool ringOn = RING_DIRT && m_bGroove && RingPrep(p);     // MUD RING, as BuildMesh
		for (int vz = z0; vz <= z1; vz++)
		{
			for (int vx = x0; vx <= x1; vx++)
			{
				int v = vz * PVERTS + vx;
				float d = p.m_aH[v];
				if (m_bAheadRuts && pr && m_fTime - pr.m_aHideAt[v] < HIDE_HOLD)     // no holder: HideAt -100 = never
				{
					if (Math.AbsFloat(d - pr.m_aShow[v]) > 0.0005)
						hideOwed = true;
					d = pr.m_aShow[v];
				}
				// = SetShow: a hidden vertex keeps its m_aShow, the rest show m_aH; the holder is made only for a non-0
				if (pr)
					pr.m_aShow[v] = d;
				else if (d != 0)
				{
					pr = p.EnsurePress();
					pr.m_aShow[v] = d;
				}
				float rfD = d;         // groove reveal input (m_aD): a field rut shows soil only from FIELD_SOIL_D deep
				float hfD = d;         // RIDGE IN CRATER fix, as BuildMesh
				if (rfOn)
				{
					float rfF = m_aInFl[v] - p.m_aBase[v];     // + client look ruts (BandFill)
					if (rfF < 0)
						rfF = rfF * p.m_aEdgeLock[v];     // edge lock as a press: 0 at kept terrain (road) -> no gap there
					if (rfF < d)
					{
						d = rfF;
						float rfS = rfF + FIELD_SOIL_D - m_fRevealDepth;
						if (rfS < rfD)
							rfD = rfS;
					}
					// Build 3 Stage 3: the ridge on top, as BuildMesh (RIDGE IN CRATER: the drawn height undug too)
					if (MUD_VoxelProbe.RUT_RIDGES && Math.AbsFloat(p.m_aSmooth[v]) < MUD_VoxelProbe.RUT_RIDGE_FLAT && Math.AbsFloat(hfD) < MUD_VoxelProbe.RUT_RIDGE_FLAT)
					{
						float rgF = m_aInRg[v];     // PERF IDEA 3 (BandFill)
						if (rgF < MUD_VoxelProbe.FLOOR_NONE * 0.5)
						{
							float rgD = (rgF - p.m_aBase[v]) * p.m_aEdgeLock[v];
							if (rgD > d)
								d = rgD;
						}
					}
					if (WEAR_SHOW)
						rfD = WearShowD(p, v, p.m_iPx * PCELLS + vx, p.m_iPz * PCELLS + vz, rfD);     // WEAR SHOW: as BuildMesh
				}
				bool ring = false;     // MUD RING: as BuildMesh
				if (ringOn && hfD > RING_MIN_H && d > RING_MIN_H)
					ring = RingDirtD(p, v, rfD);
				m_Band.m_aD[v] = rfD;
				m_Band.m_aY[v] = p.m_aBase[v] + d;
				if (bLift)
				{
					m_Band.m_aY[v] = m_Band.m_aY[v] + LiftAt(p, v, d);
					if (m_bFarOn && m_fPressSoil < rfD)
						m_Band.m_aD[v] = m_fPressSoil;     // far press: soil where the drawn press is FIELD_SOIL_D deep
				}
				if (ring)
				{
					// MUD RING: the band mesher sinks every revealed vertex (BandMesh); up by the same amount first, so the
					// surface stays (clutter) and the dirt layer ends over it, as BuildMesh
					float rt = RevealT(-m_Band.m_aD[v], p.m_iPx * PCELLS + vx, p.m_iPz * PCELLS + vz);
					m_Band.m_aY[v] = m_Band.m_aY[v] + rt * rt * (3 - 2 * rt) * m_fRevealGap * 3;
				}
			}
		}
		return hideOwed;
	}

	//------------------------------------------------------------------------------------------------
	//! REBUILD 5 option 3: true = this queued band mesh may read only p's changed box - the kept input (m_aMeshY / D)
	//! is the shown mesh's, every change since has a box, nothing is hidden or owed, the same rut floor / look switch
	protected bool PartialOk(MUD_FinePatch p, bool rfOn, bool bLift)
	{
		if (!PARTIAL_IN || !m_bMeshGate || bLift || p.m_bMBoxAll || p.m_bLastHideOwed || p.m_iBuild != 0)
			return false;
		if (!p.m_aMeshY || !p.m_aMeshD || !p.m_aMeshSkin || !p.m_MeshRes || p.m_fMeshBuiltAt < -50)
			return false;
		if (rfOn != p.m_bLastRfOn || m_fTime < p.m_fHideUntil)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! REBUILD 5 option 2: true = m_Band's input (Y, D, skin) equals the kept input of p's shown band mesh exactly
	protected bool MeshSame(MUD_FinePatch p)
	{
		if (!p.m_aMeshY || !p.m_aMeshD || !p.m_aMeshSkin || !p.m_MeshRes || p.m_iBuild != 0 || p.m_fMeshBuiltAt < -50)
			return false;
		for (int c = 0; c < SKIN * SKIN; c++)
		{
			if (m_Band.m_aSkin[c] != p.m_aMeshSkin[c])
				return false;
		}
		for (int v = 0; v < PVERTS * PVERTS; v++)
		{
			if (m_Band.m_aY[v] != p.m_aMeshY[v] || m_Band.m_aD[v] != p.m_aMeshD[v])
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! REBUILD 5: p's band mesh is now its kept input (built or same skip) - box, owed, rut pace reset
	protected void MeshDone(MUD_FinePatch p, bool owed, bool rfOn)
	{
		p.m_iMBx0 = -1;
		p.m_bMBoxAll = false;
		p.m_bLastHideOwed = owed;
		p.m_bLastRfOn = rfOn;
		p.m_bRutMesh = false;
		if (p.m_bPaceHeld)
			m_iPaceBuilds++;
		p.m_bPaceHeld = false;
	}

	//------------------------------------------------------------------------------------------------
	//! REBUILD 5 option 3: grow p's changed box by patch-local vertices x0..x1, z0..z1
	protected void MeshBoxL(MUD_FinePatch p, int x0, int x1, int z0, int z1)
	{
		if (p.m_iMBx0 < 0)
		{
			p.m_iMBx0 = x0;
			p.m_iMBx1 = x1;
			p.m_iMBz0 = z0;
			p.m_iMBz1 = z1;
			return;
		}
		p.m_iMBx0 = Math.Min(p.m_iMBx0, x0);
		p.m_iMBx1 = Math.Max(p.m_iMBx1, x1);
		p.m_iMBz0 = Math.Min(p.m_iMBz0, z0);
		p.m_iMBz1 = Math.Max(p.m_iMBz1, z1);
	}

	//------------------------------------------------------------------------------------------------
	//! REBUILD 5 option 3: MeshBoxL from global fine vertex coords (clamped to p)
	protected void MeshBoxG(MUD_FinePatch p, int gx0, int gx1, int gz0, int gz1)
	{
		int ox = p.m_iPx * PCELLS;
		int oz = p.m_iPz * PCELLS;
		MeshBoxL(p, Math.ClampInt(gx0 - ox, 0, PCELLS), Math.ClampInt(gx1 - ox, 0, PCELLS), Math.ClampInt(gz0 - oz, 0, PCELLS), Math.ClampInt(gz1 - oz, 0, PCELLS));
	}

	//------------------------------------------------------------------------------------------------
	//! REBUILD 5 option 5: the band mesher (the v240 BuildRef + Bench archived 2026-10-22, audit C5)
	protected bool BandCore(float skirt)
	{
		return m_Band.Build(m_fFine, skirt);
	}

	//------------------------------------------------------------------------------------------------
	//! PERF IDEA 3: m_aInFl / m_aInRg for the patch at global vertex gx0, gz0 (after FieldPatchBegin, and PatchBegin when
	//! look): the floor (look = + the client look floor, MeshFloorP) and ridge of every vertex.
	protected void BandFill(int gx0, int gz0, bool look)
	{
		if (m_aInFl.Count() != PVERTS * PVERTS)
		{
			m_aInFl.Resize(PVERTS * PVERTS);
			m_aInRg.Resize(PVERTS * PVERTS);
		}
		MUD_VoxelProbe.FieldPatchFill(gx0, gz0, PVERTS, m_aInFl, m_aInRg);
		if (look)
			MUD_LookRuts.PatchFill(gx0, gz0, PVERTS, m_aInFl);
	}

	//------------------------------------------------------------------------------------------------
	//! PERF IDEA 2: true = the band input (m_Band.m_aY) is within MESH_SMALL_M of p's last built band mesh everywhere
	//! and that build is < MESH_SMALL_MAX_S old (live patch only) - the rebuild can wait
	protected bool MeshSmall(MUD_FinePatch p)
	{
		if (!p.m_aMeshY || !p.m_MeshRes || p.m_iBuild != 0)
		{
			m_iWhyOther++;     // REBUILD_WHY
			return false;
		}
		bool aged = m_fTime - p.m_fMeshBuiltAt >= MESH_SMALL_MAX_S;
		for (int v = 0; v < PVERTS * PVERTS; v++)
		{
			if (Math.AbsFloat(m_Band.m_aY[v] - p.m_aMeshY[v]) >= MESH_SMALL_M)
			{
				m_iWhyBig++;     // REBUILD_WHY: a big change (whatever the age)
				return false;
			}
		}
		if (aged)
		{
			m_iWhyAge++;     // REBUILD_WHY: only the age test failed
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! PERF IDEA 2: m_Band.m_aY (this band mesh's input) into p.m_aMeshY; m_fMeshBuiltAt is set once the mesh is made
	protected void KeepMeshY(MUD_FinePatch p)
	{
		if (!MESH_SKIP_SMALL && !SAME_SKIP && !PARTIAL_IN)
			return;
		if (!p.m_aMeshY)
		{
			p.m_aMeshY = new array<float>();
			p.m_aMeshY.Resize(PVERTS * PVERTS);
		}
		if (!p.m_aMeshD)
		{
			p.m_aMeshD = new array<float>();     // REBUILD 5: + D and skin (same skip, partial pass)
			p.m_aMeshD.Resize(PVERTS * PVERTS);
		}
		if (!p.m_aMeshSkin)
		{
			p.m_aMeshSkin = new array<int>();
			p.m_aMeshSkin.Resize(SKIN * SKIN);
		}
		for (int v = 0; v < PVERTS * PVERTS; v++)
		{
			p.m_aMeshY[v] = m_Band.m_aY[v];
			p.m_aMeshD[v] = m_Band.m_aD[v];
		}
		for (int c = 0; c < SKIN * SKIN; c++)
			p.m_aMeshSkin[c] = m_Band.m_aSkin[c];
	}

	//------------------------------------------------------------------------------------------------
	//! Layered-SDF test (Band Mesher on): BuildCollider from the band mesher on the smoothed heights (same
	//! wheel hold, one trimesh per surface slot, slab = the same tris lowered + reversed). The safety plate stays
	//! the grid. False = overflow or no triangle: BuildCollider builds the grid (m_aColH not touched yet).
	protected bool BuildColliderBand(MUD_FinePatch p)
	{
		int t0 = System.GetTickCount();
		if (!m_Band)
			m_Band = new MUD_BandMesher();
		MUD_IdxSet ix = IdxSet(p);     // plate
		int csIn = System.GetTickCount();
		bool hold = m_fWheelHold > 0 && p.m_bColValid && p.m_Phys != null;
		bool rfOn = RutFieldOn(p);     // RUT FIELD: y = base + min(smooth, floor - base), the floor not blurred
		int rfX0 = p.m_iPx * PCELLS;
		int rfZ0 = p.m_iPz * PCELLS;
		bool hadBody = p.m_bColValid && p.m_Phys != null;
		bool moved = false;
		int held = 0;
		float colDiff = 0;      // PAPER TEST 3: biggest vertex move against the live body
		bool rutMud = rfOn && RUT_MUD && !m_RutMudMaterial.IsEmpty();
		int mudMask;     // Build 3 Stage 4: bit per skin cell (cz * SKIN + cx) that is rut mud
		bool coarsePlate = COARSE_PLATE && p.m_bFlatFloor;     // REBUILD 6 (PlateGeom)
		if (rfOn)
		{
			MUD_VoxelProbe.FieldPatchBegin(rfX0, rfZ0, PVERTS - 1);     // perf plan B2: tiles once per patch
			BandFill(rfX0, rfZ0, false);     // PERF IDEA 3: floor + ridge of every vertex, one call
		}
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				float y = p.m_aBase[v] + p.m_aSmooth[v];
				if (rfOn)
				{
					float rfF = m_aInFl[v];     // PERF IDEA 3 (BandFill)
					if (rfF < p.m_aBase[v])
						rfF = p.m_aBase[v] + (rfF - p.m_aBase[v]) * p.m_aEdgeLock[v];     // edge lock, as the mesh
					if (rfF < y)
						y = rfF;
					// Build 3 Stage 4: every skin cell touching this vertex is rut mud (FLOOR_NONE never passes)
					if (rutMud && rfF <= p.m_aBase[v] - RUT_MAT_D)
						mudMask = mudMask | RutMudCells(vx, vz);
					// Build 3 Stage 3: the ridge on top, as the mesh
					if (MUD_VoxelProbe.RUT_RIDGES && Math.AbsFloat(p.m_aSmooth[v]) < MUD_VoxelProbe.RUT_RIDGE_FLAT)
					{
						float rgF = m_aInRg[v];     // PERF IDEA 3 (BandFill)
						if (rgF < MUD_VoxelProbe.FLOOR_NONE * 0.5)
						{
							rgF = p.m_aBase[v] + (rgF - p.m_aBase[v]) * p.m_aEdgeLock[v];
							if (rgF > y)
								y = rgF;
						}
					}
				}
				if (hold && y != p.m_aColH[v] && m_fTime - Math.Max(p.RutTouch(v), p.BermTouch(v)) < m_fWheelHold)
				{
					y = p.m_aColH[v];
					held++;
				}
				if (hadBody && Math.AbsFloat(y - p.m_aColH[v]) > 0.001)
					moved = true;
				if (hadBody)
					colDiff = Math.Max(colDiff, Math.AbsFloat(y - p.m_aColH[v]));
				if (y != p.m_aBase[v] + p.m_aSmooth[v])
					m_Band.m_aTouch[v] = 1;     // REBUILD 5 option 4: rut / ridge / hold - never merged
				else
					m_Band.m_aTouch[v] = 0;
				m_Band.m_aY[v] = y;
				m_Band.m_aD[v] = 0;
				if (!coarsePlate)     // REBUILD 6: the coarse plate reads base + floor itself
					m_Buf.m_aPlate[v] = Vector(vx * m_fFine, p.m_aBase[v] + p.m_aFloor[v] - m_fPlateGap, vz * m_fFine);
			}
		}
		if (m_bColGate && COL_SKIP_SMALL && hadBody && p.m_iBuild == 0 && colDiff < COL_SMALL_M && m_fTime - p.m_fColBuiltAt < COL_SMALL_MAX_S)
		{
			m_iColSkipSmall++;
			return true;        // PAPER TEST 3: body kept, m_bColDirty stays - built on a later pass
		}
		FillBandOn(p);
		m_iCsColIn += System.GetTickCount(csIn);
		m_Band.m_bMerge = COL_MERGE;     // REBUILD 5 option 4
		m_Band.m_fMergeTol = COL_MERGE_TOL;
		int tc = System.GetTickCount();
		bool ok = BandCore(0);
		m_iBandCoreMs += System.GetTickCount(tc);
		m_iCsColCore += System.GetTickCount(tc);
		if (!ok)
		{
			if (m_Band.m_bOverflow)
				m_iBandFallback++;     // else: no on cell (all kept terrain) - the grid collapses it the same way
			return false;
		}
		m_iColMerged += m_Band.m_iMerged;     // REBUILD 5 option 4
		m_iColTrisN += m_Band.m_iI / 3;
		m_iColMergeN++;
		for (int vc = 0; vc < PVERTS * PVERTS; vc++)
			p.m_aColH[vc] = m_Band.m_aY[vc];

		int csGeom = System.GetTickCount();
		int nv = m_Band.m_iV;
		int ni = m_Band.m_iI;
		array<PhysicsGeom> geoms = {};
		array<ResourceName> mats = {};
		array<int> layers = {};
		// Terrain preset + Foliage: vehicle get in/out sweep skips Foliage (MUD_GroundEntity.c:1668-1676)
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;
		// Build 3 Stage 4: a tri whose centre lies in a mud skin cell goes to one extra mud piece
		if (mudMask != 0)
		{
			int slotUsed;     // bit per used slot
			for (int tu = 0; tu < ni / 3; tu++)
			{
				int su = m_Band.m_Slot.m_aS[tu];
				if (su >= 0 && su < p.m_aMats.Count())
					slotUsed = slotUsed | (1 << su);
			}
			int used;
			for (int ku = 0; ku < MAT_SLOTS; ku++)
			{
				if ((slotUsed & (1 << ku)) != 0)
					used++;
			}
			if (!RutMudFits(p, used))
				mudMask = 0;
		}
		if (mudMask != 0 && !m_RutMudBand)
			m_RutMudBand = new MUD_BandIdx();
		int nMud = 0;
		float cellW = m_fFine * FINE;
		for (int k = 0; k < p.m_aMats.Count(); k++)
		{
			int n = 0;
			for (int ti = 0; ti < ni; ti += 3)
			{
				if (m_Band.m_Slot.m_aS[ti / 3] != k)
					continue;
				int i0 = m_Band.m_Ib.m_aI[ti];
				int i1 = m_Band.m_Ib.m_aI[ti + 1];
				int i2 = m_Band.m_Ib.m_aI[ti + 2];
				if (mudMask != 0)
				{
					vector ctr = (m_Band.m_Vb.m_aV[i0] + m_Band.m_Vb.m_aV[i1] + m_Band.m_Vb.m_aV[i2]) * (1.0 / 3.0);
					int tcx = Math.Floor(ctr[0] / cellW);
					int tcz = Math.Floor(ctr[2] / cellW);
					if (tcx < 0)
						tcx = 0;
					if (tcx >= SKIN)
						tcx = SKIN - 1;
					if (tcz < 0)
						tcz = 0;
					if (tcz >= SKIN)
						tcz = SKIN - 1;
					if ((mudMask & (1 << (tcz * SKIN + tcx))) != 0)
					{
						m_RutMudBand.m_aI[nMud] = i0;
						m_RutMudBand.m_aI[nMud + 1] = i1;
						m_RutMudBand.m_aI[nMud + 2] = i2;
						nMud += 3;
						continue;
					}
				}
				m_Band.m_Scratch.m_aI[n] = i0;
				m_Band.m_Scratch.m_aI[n + 1] = i1;
				m_Band.m_Scratch.m_aI[n + 2] = i2;
				n += 3;
			}
			if (n == 0)
				continue;
			PhysicsGeom geom = PhysicsGeom.CreateTriMesh(m_Band.m_Vb.m_aV, m_Band.m_Scratch.m_aI, nv, n);
			if (!geom)
			{
				Print("MUD world: CreateTriMesh failed", LogLevel.ERROR);
				continue;
			}
			geoms.Insert(geom);
			mats.Insert(p.m_aMats[k]);
			layers.Insert(layer);
		}
		if (nMud > 0)
		{
			PhysicsGeom mudGeom = PhysicsGeom.CreateTriMesh(m_Band.m_Vb.m_aV, m_RutMudBand.m_aI, nv, nMud);
			if (mudGeom)
			{
				geoms.Insert(mudGeom);
				mats.Insert(m_RutMudMaterial);
				layers.Insert(layer);
			}
			else
			{
				Print("MUD world: CreateTriMesh failed (rut mud, band)", LogLevel.ERROR);
			}
		}
		RutMudLog(p, mudMask, nMud / 3, "band");

		// slab: every surface tri, lowered and reversed (facing down)
		if (m_fBulletSlab > 0 && !geoms.IsEmpty())
		{
			for (int vs = 0; vs < nv; vs++)
				m_Band.m_Aux.m_aV[vs] = m_Band.m_Vb.m_aV[vs] - vector.Up * m_fBulletSlab;
			for (int ts = 0; ts < ni; ts += 3)
			{
				m_Band.m_Scratch.m_aI[ts] = m_Band.m_Ib.m_aI[ts];
				m_Band.m_Scratch.m_aI[ts + 1] = m_Band.m_Ib.m_aI[ts + 2];
				m_Band.m_Scratch.m_aI[ts + 2] = m_Band.m_Ib.m_aI[ts + 1];
			}
			PhysicsGeom shell = PhysicsGeom.CreateTriMesh(m_Band.m_Aux.m_aV, m_Band.m_Scratch.m_aI, nv, ni);
			if (shell)
			{
				geoms.Insert(shell);
				mats.Insert(p.m_aMats[0]);
				layers.Insert(EPhysicsLayerPresets.FireGeo);
			}
		}
		if (ix.m_Plate.m_iN > 0 && !geoms.IsEmpty())
		{
			PhysicsGeom plate = PlateGeom(p, ix);     // REBUILD 6 COARSE_PLATE
			if (plate)
			{
				geoms.Insert(plate);
				mats.Insert(m_GameMaterial);
				layers.Insert(layer);
			}
		}

		m_iCsColGeom += System.GetTickCount(csGeom);
		m_iCsColGeoms += geoms.Count();
		// from here as BuildCollider
		int td = System.GetTickCount();
		DropBody(p);
		m_iLastDropMs = System.GetTickCount(td);
		m_iCsColDrop += m_iLastDropMs;
		m_iLastStaticMs = 0;
		if (!geoms.IsEmpty())
		{
			int tsx = System.GetTickCount();
			p.m_Phys = MakeStatic(p.m_Ent, geoms, mats, layers);
			m_iLastStaticMs = System.GetTickCount(tsx);
			m_iCsColStatic += m_iLastStaticMs;
			if (geoms.Count() > m_iMaxGeoms)
				m_iMaxGeoms = geoms.Count();
			if (p.m_Phys)
			{
				p.m_aGeoms.Copy(geoms);
			}
			else
			{
				Print("MUD world: CreateStaticEx failed", LogLevel.ERROR);
				foreach (PhysicsGeom g : geoms)
					g.Destroy();
			}
		}
		p.m_bColDirty = false;
		p.m_bColValid = p.m_Phys != null;
		p.m_fColBuiltAt = m_fTime;
		if (moved)
			p.m_fColChangeAt = m_fTime;
		if (held > 0 && p.m_Phys)
		{
			m_iHeldVerts += held;
			m_iHeldBuilds++;
			p.m_bColDirty = true;
			if (!p.m_bQueued)
			{
				p.m_bQueued = true;
				p.m_fQueuedAt = m_fTime;
				m_aDirty.Insert(p.m_iKey);
			}
		}
		m_iColBuilds++;
		int ms = System.GetTickCount(t0);
		m_iColMs += ms;
		m_iBandColN++;
		m_iBandColMs += ms;
		if (ms > m_iBandColWorst)
			m_iBandColWorst = ms;
		BandCount();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Layered-SDF test: every 10 s while the band mesher is on (or Debug), per-patch build cost of both paths
	protected void BandStats(float timeSlice)
	{
		if (!m_bBandMesher && !m_bDebug)
			return;
		m_fSinceBand += timeSlice;
		if (m_fSinceBand < 10)
			return;
		m_fSinceBand = 0;
		int builds = m_iBandMeshN + m_iBandColN;
		if ((m_bDebug || MUD_TestKit.TEST_KIT) && builds + m_iGridMeshN + m_iGridColN + m_iBandFallback > 0)     // audit C1: test meters only
		{
			float b = Math.Max(builds, 1);
			float meshMean = m_iBandMeshMs / Math.Max(m_iBandMeshN, 1.0);
			float colMean = m_iBandColMs / Math.Max(m_iBandColN, 1.0);
			float gMeshMean = m_iGridMeshMs / Math.Max(m_iGridMeshN, 1.0);
			float gColMean = m_iGridColMs / Math.Max(m_iGridColN, 1.0);
			Print(string.Format("MUD band: last 10 s - switch %1 | band mesh %2 builds, mean %3 ms, worst %4 | band collider %5, mean %6 ms, worst %7 | mesher itself %8 ms in all",
				m_bBandMesher, m_iBandMeshN, meshMean, m_iBandMeshWorst, m_iBandColN, colMean, m_iBandColWorst, m_iBandCoreMs));
			Print(string.Format("MUD band: per build - cells mean %1 max %2, tris mean %3 max %4 (grid 2048), verts mean %5 (grid 1089), skirt tris mean %6 | degen %7, missing %8, overflowed (grid built) %9",
				m_iBandCells / b, m_iBandCellsMax, m_iBandTris / b, m_iBandTrisMax, m_iBandVerts / b, m_iBandSkirtTris / Math.Max(m_iBandMeshN, 1.0), m_iBandDegen, m_iBandMissing, m_iBandFallback));
			Print(string.Format("MUD band: mesh split ms - input %1, mesher %2, reveal + uv %3, mesh object %4, rest %5 (of %6)",
				m_iCsMeshIn, m_iCsMeshCore, m_iCsMeshUv, m_iCsMeshRes, m_iBandMeshMs - m_iCsMeshIn - m_iCsMeshCore - m_iCsMeshUv - m_iCsMeshRes, m_iBandMeshMs));
			Print(string.Format("MUD band: input split ms - rut field %1 (%2 passes), lift setup %3 (%4 passes; slope cap %5 ms, %6 caps), partial check %7, vertex loop %8, skin slots %9",
				m_iInRf, m_iInRfN, m_iInLift, m_iInLiftN, m_iInCap, m_iInCapN, m_iInPart, m_iInLoop, m_iInFill)
				+ string.Format(" | rest %1 (of input %2)", m_iCsMeshIn - m_iInRf - m_iInLift - m_iInPart - m_iInLoop - m_iInFill, m_iCsMeshIn), LogLevel.NORMAL);
			Print(string.Format("MUD band: collider split ms - input %1, mesher %2, trimeshes %3 (%4 geoms), drop %5, static %6, rest %7 (of %8)",
				m_iCsColIn, m_iCsColCore, m_iCsColGeom, m_iCsColGeoms, m_iCsColDrop, m_iCsColStatic,
				m_iBandColMs - m_iCsColIn - m_iCsColCore - m_iCsColGeom - m_iCsColDrop - m_iCsColStatic, m_iBandColMs));
			Print(string.Format("MUD band: grid path - mesh %1 builds, mean %2 ms, worst %3 | collider %4, mean %5 ms, worst %6 | band fast verts mean %7, fast quads mean %8",
				m_iGridMeshN, gMeshMean, m_iGridMeshWorst, m_iGridColN, gColMean, m_iGridColWorst, m_iBandFastV / b, m_iBandFastQ / b));
			BUnderLog();     // stage 3 Part 0 B meter
			float mergeN = Math.Max(m_iColMergeN, 1.0);
			Print(string.Format("MUD rebuild 5: 1 rut pace %1 - held patch-frames %2, held builds %3 | 2 same skip %4 - skips %5 | 3 partial %6 - passes %7, verts mean %8, input ms %9",
				RUT_PACE, m_iPaceHeldPF, m_iPaceBuilds, SAME_SKIP, m_iSameSkip, PARTIAL_IN, m_iPartN, m_iPartVerts / Math.Max(m_iPartN, 1.0), m_iPartMs)
				+ string.Format(" | full passes %1, input ms %2", m_iFullN, m_iFullMs)
				+ string.Format(" | 4 merge %1 - col builds %2, merged blocks mean %3, col tris mean %4 (%5 saved)",
				COL_MERGE, m_iColMergeN, m_iColMerged / mergeN, m_iColTrisN / mergeN, m_iColMerged * 6 / mergeN));
		}
		RebuildSixLog();     // REBUILD 6: prints when it has counts, always resets
		m_iPaceHeldPF = 0;
		m_iPaceBuilds = 0;
		m_iSameSkip = 0;
		m_iPartN = 0;
		m_iPartVerts = 0;
		m_iPartMs = 0;
		m_iFullN = 0;
		m_iFullMs = 0;
		m_iColMerged = 0;
		m_iColTrisN = 0;
		m_iColMergeN = 0;
		m_iBandMeshN = 0;
		m_iBandMeshMs = 0;
		m_iBandMeshWorst = 0;
		m_iCsMeshIn = 0;
		m_iCsMeshCore = 0;
		m_iCsMeshUv = 0;
		m_iCsMeshRes = 0;
		m_iCsColIn = 0;
		m_iCsColCore = 0;
		m_iCsColGeom = 0;
		m_iCsColDrop = 0;
		m_iCsColStatic = 0;
		m_iInRf = 0;
		m_iInLift = 0;
		m_iInCap = 0;
		m_iInPart = 0;
		m_iInLoop = 0;
		m_iInFill = 0;
		m_iInRfN = 0;
		m_iInLiftN = 0;
		m_iInCapN = 0;
		m_iCsColGeoms = 0;
		m_iBandColN = 0;
		m_iBandColMs = 0;
		m_iBandColWorst = 0;
		m_iBandCoreMs = 0;
		m_iBandCells = 0;
		m_iBandCellsMax = 0;
		m_iBandTris = 0;
		m_iBandTrisMax = 0;
		m_iBandVerts = 0;
		m_iBandSkirtTris = 0;
		m_iBandDegen = 0;
		m_iBandMissing = 0;
		m_iBandFastV = 0;
		m_iBandFastQ = 0;
		m_iBandFallback = 0;
		m_iGridMeshN = 0;
		m_iGridMeshMs = 0;
		m_iGridMeshWorst = 0;
		m_iGridColN = 0;
		m_iGridColMs = 0;
		m_iGridColWorst = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Layered-SDF test, every m_fDumpEvery s on every machine: the heights of deformed patches that changed
	//! since their last dump (at most 8 per pass) as 'MUD dump' lines, mm ints: base, H, shown, smoothed.
	//! Read by Tools/sdf_patch_proto.py --dump.
	protected void DumpPatches(float timeSlice)
	{
		if (m_fDumpEvery <= 0)
			return;
		m_fSinceDump += timeSlice;
		if (m_fSinceDump < m_fDumpEvery)
			return;
		m_fSinceDump = 0;
		int dumped = 0;
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			if (dumped >= 8)
				break;
			if (!p.m_bDeformed)
				continue;
			int sig = 0;
			for (int sv = 0; sv < PVERTS * PVERTS; sv++)
			{
				int scol = sv - (sv / PVERTS) * PVERTS;
				sig += ToMm(p.m_aH[sv]) * (scol + 1) + ToMm(p.Show(sv));
			}
			int last = 0;
			if (m_mDumpSig.Find(key, last) && last == sig)
				continue;
			if (sig == 0 && !m_mDumpSig.Contains(key))
				continue;
			m_mDumpSig.Set(key, sig);
			dumped++;
			string head = string.Format("MUD dumppatch %1 %2 %3", key, p.m_iPx, p.m_iPz);
			for (int c = 0; c < 16; c++)
				head += " " + p.m_aCell[c].ToString();
			Print(string.Format("%1", head));   // Print(s) logs "string s = '...'" (2026-10-10)
			for (int kind = 0; kind < 4; kind++)
			{
				string tag = "dumpb";
				if (kind == 1)
					tag = "dumph";
				else if (kind == 2)
					tag = "dumps";
				else if (kind == 3)
					tag = "dumpw";
				for (int row = 0; row < PVERTS; row++)
				{
					string line = string.Format("MUD %1 %2 %3", tag, key, row);
					for (int col = 0; col < PVERTS; col++)
					{
						int v = row * PVERTS + col;
						float val = p.m_aBase[v];
						if (kind == 1)
							val = p.m_aH[v];
						else if (kind == 2)
							val = p.Show(v);
						else if (kind == 3)
							val = p.m_aSmooth[v];
						int mm = Math.Round(val * 1000);
						line += " " + mm.ToString();
					}
					Print(string.Format("%1", line));
				}
			}
			Print(string.Format("MUD dumpend %1", key));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Static body from 1-6 geoms, one literal array per count (copy of MUD_GroundEntity.c:1234)
	protected Physics MakeStatic(IEntity ent, array<PhysicsGeom> g, array<ResourceName> m, array<int> l)
	{
		switch (g.Count())
		{
			case 1:
			{
				autoptr PhysicsGeomDef d1[] = {PhysicsGeomDef("", g[0], m[0], l[0])};
				return Physics.CreateStaticEx(ent, d1);
			}
			case 2:
			{
				autoptr PhysicsGeomDef d2[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1])};
				return Physics.CreateStaticEx(ent, d2);
			}
			case 3:
			{
				autoptr PhysicsGeomDef d3[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2])};
				return Physics.CreateStaticEx(ent, d3);
			}
			case 4:
			{
				autoptr PhysicsGeomDef d4[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3])};
				return Physics.CreateStaticEx(ent, d4);
			}
			case 5:
			{
				autoptr PhysicsGeomDef d5[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3]), PhysicsGeomDef("", g[4], m[4], l[4])};
				return Physics.CreateStaticEx(ent, d5);
			}
			case 6:
			{
				autoptr PhysicsGeomDef d6[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3]), PhysicsGeomDef("", g[4], m[4], l[4]), PhysicsGeomDef("", g[5], m[5], l[5])};
				return Physics.CreateStaticEx(ent, d6);
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OursOnly(notnull IEntity e)
	{
		return MUD_Surface.IsOurs(e);
	}

	//------------------------------------------------------------------------------------------------
	//! Debug: 4 points inside a new patch's hole cells, traced (ENTS | WORLD); the hit must be this patch
	//! within 5 mm of the skin triangle height there
	protected void CheckNew(MUD_FinePatch p)
	{
		for (int i = 0; i < 4; i++)
		{
			int fx = Math.RandomInt(0, PCELLS);
			int fz = Math.RandomInt(0, PCELLS);
			int ccell = p.m_aCell[(fz / FINE) * SKIN + fx / FINE];
			if (ccell < 0 || (ccell & VOX) != 0)
				continue;
			float gfx = p.m_iPx * PCELLS + fx + 0.37;
			float gfz = p.m_iPz * PCELLS + fz + 0.61;
			bool ok;
			float want = BaseAt(gfx, gfz, ok);
			if (!ok)
				continue;
			m_iChkPts++;
			float wx = m_fX0 + gfx * m_fFine;
			float wz = m_fZ0 + gfz * m_fFine;
			m_Trace.Start = Vector(wx, want + 5, wz);
			m_Trace.End = Vector(wx, want - 5, wz);
			m_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
			m_Trace.TraceEnt = null;
			float frac = GetWorld().TraceMove(m_Trace, OursOnly);
			if (frac >= 1 || m_Trace.TraceEnt != p.m_Ent)
				continue;
			m_iChkHit++;
			float err = Math.AbsFloat(want + 5 - 10 * frac - want);
			if (err < 0.005)
				m_iChkGood++;
			if (err > m_fChkWorst)
				m_fChkWorst = err;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! TRACEABLE off (hide) or back on for every fine patch; new patches follow while hidden.
	//! Called by MUD_VehicleAccess.HideFromTraces (vehicle get in / out).
	static void SetTraceHidden(bool hide)
	{
		s_bHidden = hide;
		if (!s_Inst)
			return;
		foreach (int key, MUD_FinePatch p : s_Inst.m_mPatches)
		{
			if (!p.m_Ent)
				continue;
			if (hide)
			{
				p.m_bTraceHidden = (p.m_Ent.GetFlags() & EntityFlags.TRACEABLE) != 0;
				if (p.m_bTraceHidden)
					p.m_Ent.ClearFlags(EntityFlags.TRACEABLE, false);
			}
			else if (p.m_bTraceHidden)
			{
				p.m_Ent.SetFlags(EntityFlags.TRACEABLE, false);
				p.m_bTraceHidden = false;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server with a registered RplComponent in a networked session: changes go out. An unregistered
	//! item (prefab GUID missing from the resource DB) logs an error per Rpc, so it is skipped
	//! (MUD_GroundEntity.c:1930).
	protected bool IsNet()
	{
		return m_RplComponent != null && !IsProxy() && RplSession.Mode() != RplMode.None && m_RplComponent.Id().IsValid();
	}

	//------------------------------------------------------------------------------------------------
	protected int ToMm(float h)
	{
		return Math.ClampInt(Math.Round(h * 1000), -32768, 32767);
	}

	//------------------------------------------------------------------------------------------------
	//! Signed 16-bit half of a packed int: 0 = low, 1 = high (MUD_GroundEntity.c Unpack)
	protected int Unpack(int packed, int half)
	{
		int v = packed & 65535;
		if (half == 1)
			v = (packed >> 16) & 65535;
		if (v > 32767)
			v -= 65536;
		return v;
	}

	//------------------------------------------------------------------------------------------------
	//! Patch-local rect [x0..x1] x [z0..z1] of p in mm, two per int, low half first
	protected array<int> PackRect(MUD_FinePatch p, int x0, int x1, int z0, int z1)
	{
		// fresh array per message: Rpc may serialize later than this call
		array<int> pack = {};
		int pending;
		bool odd;
		for (int z = z0; z <= z1; z++)
		{
			for (int x = x0; x <= x1; x++)
			{
				int mm = ToMm(p.m_aH[z * PVERTS + x]) & 65535;
				if (!odd)
					pending = mm;
				else
					pack.Insert(pending | (mm << 16));
				odd = !odd;
			}
		}
		if (odd)
			pack.Insert(pending);
		return pack;
	}

	//------------------------------------------------------------------------------------------------
	protected array<int> PackFull(MUD_FinePatch p)
	{
		return PackRect(p, 0, PCELLS, 0, PCELLS);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, every m_fColliderInterval: one message per patch with unsent changes
	protected void SendChanges(float timeSlice)
	{
		m_fSinceNet += timeSlice;
		if (m_fSinceNet < m_fColliderInterval)
			return;
		m_fSinceNet = 0;
		bool byDist = m_fSendRadius > 0;
		if (byDist && !m_aNetKeys.IsEmpty())
			SendPlayers();
		foreach (int key : m_aNetKeys)
		{
			MUD_FinePatch p = m_mPatches.Get(key);
			if (!p)
				continue;
			p.m_bNetQueued = false;
			if (p.m_iNx0 < 0)
				continue;
			array<int> pack = PackRect(p, p.m_iNx0, p.m_iNx1, p.m_iNz0, p.m_iNz1);
			int rect = p.m_iNx0 | (p.m_iNx1 << 6) | (p.m_iNz0 << 12) | (p.m_iNz1 << 18);
			if (p.m_bNetRush)
			{
				rect = rect | RUSH_BIT;     // CRATER RUSH
				p.m_bNetRush = false;
			}
			if (byDist)
				SendNear(p, rect, pack);
			else
				Rpc(RpcDo_Heights, key, m_iSeq, rect, pack);
			m_iSeq++;
			m_iNetMsgs++;
			m_iNetInts += pack.Count();
			p.m_iNx0 = -1;
		}
		m_aNetKeys.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Server, send by distance: the remote players and where they are, once per send tick. The host's
	//! own controller is left out (the server has the heights).
	protected void SendPlayers()
	{
		m_aSendPids.Clear();
		m_aSendPcs.Clear();
		m_aSendPos.Clear();
		m_aSendKnown.Clear();
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		PlayerController local = GetGame().GetPlayerController();
		m_aPlayerIds.Clear();
		pm.GetPlayers(m_aPlayerIds);
		foreach (int pid : m_aPlayerIds)
		{
			SCR_PlayerController pc = SCR_PlayerController.Cast(pm.GetPlayerController(pid));
			if (!pc || pc == local)
				continue;
			IEntity ent = pm.GetPlayerControlledEntity(pid);
			m_aSendPids.Insert(pid);
			m_aSendPcs.Insert(pc);
			if (ent)
				m_aSendPos.Insert(ent.GetOrigin());
			else
				m_aSendPos.Insert(vector.Zero);
			m_aSendKnown.Insert(ent != null);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server, send by distance: one change (numbered m_iSeq) live to each near player; a far one owes
	//! it. A patch owed to a player stays owed (no live message) until its full copy is sent: that copy
	//! carries every change before it. Live and full copies both go through the player's controller, so
	//! they arrive in send order (INFERENCE: one entity, one reliable channel - as the JIP replay assumes).
	protected void SendNear(MUD_FinePatch p, int rect, array<int> pack)
	{
		for (int i = 0; i < m_aSendPids.Count(); i++)
		{
			int pid = m_aSendPids[i];
			map<int, bool> owed = m_mOwed.Get(pid);
			if (owed && owed.Contains(p.m_iKey))
				continue;
			if (!m_aSendKnown[i] || PatchDist(p.m_iPx, p.m_iPz, m_aSendPos[i]) <= SendR(rect))
			{
				m_aSendPcs[i].MUD_SendLive(p.m_iKey, m_iSeq, rect, pack);
				m_iNetLive++;
				continue;
			}
			if (!owed)
			{
				owed = new map<int, bool>();
				m_mOwed.Insert(pid, owed);
			}
			owed.Insert(p.m_iKey, false);
			m_iNetOwed++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server, once a second: owed patches a player has come within m_fSendRadius of (or any, while
	//! they control nothing) join the JIP queue as catch-up copies. Players who left are dropped.
	protected void CatchUp()
	{
		if (m_mOwed.IsEmpty() || m_fTime < m_fOwedCheckAt)
			return;
		m_fOwedCheckAt = m_fTime + 1;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		array<int> gone = {};
		array<int> due = {};
		foreach (int pid, map<int, bool> owed : m_mOwed)
		{
			if (!pm.GetPlayerController(pid) || !owed || owed.IsEmpty())
			{
				gone.Insert(pid);
				continue;
			}
			IEntity ent = pm.GetPlayerControlledEntity(pid);
			due.Clear();
			foreach (int key, bool queued : owed)
			{
				if (queued)
					continue;
				if (!ent || PatchDist(key - (key / m_iNPX) * m_iNPX, key / m_iNPX, ent.GetOrigin()) <= m_fSendRadius)
					due.Insert(key);
			}
			foreach (int k : due)
			{
				owed.Set(k, true);
				MUD_JipItem it = new MUD_JipItem();
				it.m_iPid = pid;
				it.m_iKey = k;
				it.m_fSendAt = m_fTime;
				it.m_bCatchUp = true;
				m_aJip.Insert(it);
			}
		}
		foreach (int g : gone)
			m_mOwed.Remove(g);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a live message sent to this player only (send by distance)
	void OnLivePc(int key, int seq, int rect, array<int> packed)
	{
		OnLive(key, seq, rect, packed);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a per-player live message that came before this entity was streamed in; replayed when
	//! the entity is ready
	static void HoldEarly(int key, int seq, int rect, array<int> packed)
	{
		if (s_aEarly.Count() >= 20000)
			return;     // a client that never builds this entity: do not grow without end
		MUD_NetMsg msg = new MUD_NetMsg();
		msg.m_iKey = key;
		msg.m_iSeq = seq;
		msg.m_iRect = rect;
		msg.m_aPacked = packed;
		s_aEarly.Insert(msg);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Heights(int key, int seq, int rect, array<int> packed)
	{
		if (!packed)
			return;
		OnLive(key, seq, rect, packed);
	}

	//------------------------------------------------------------------------------------------------
	//! probe 2026-10-13 (MUD_VoxelProbe ground test, MP): server, one voxel op (text) to every client
	static void VoxelBroadcast(string text)
	{
		if (s_Inst && !s_Inst.IsProxy())
			s_Inst.SendVoxelOp(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void SendVoxelOp(string text)
	{
		m_iOpChars += text.Length();     // audit B4 meter (10 s 'MUD patches' line)
		Rpc(RpcDo_VoxelOp, text);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_VoxelOp(string text)
	{
		MUD_VoxelProbe.Receive(text);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: apply a live message now, or hold it (not ready, or the patch waits for its full copy).
	//! A message older than the patch's full copy is dropped: the full copy already has it.
	protected void OnLive(int key, int seq, int rect, array<int> packed)
	{
		if (!m_bReady || m_sJipPending.Contains(key))
		{
			MUD_NetMsg msg = new MUD_NetMsg();
			msg.m_iKey = key;
			msg.m_iSeq = seq;
			msg.m_iRect = rect;
			msg.m_aPacked = packed;
			m_aHeld.Insert(msg);
			if (m_sJipPending.Contains(key))
				m_iJipHeld++;
			return;
		}
		if (m_mFullSeq.Contains(key) && seq < m_mFullSeq.Get(key))
		{
			m_iJipDropped++;
			return;
		}
		ApplyRect(key, rect, packed);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: held messages (arrival order = server send order) for one key (key >= 0) or all keys
	//! not waiting for a full copy (key < 0); for a key with a full copy, only seq >= its stamp
	protected void ReplayHeld(int key, int fromSeq)
	{
		for (int i = 0; i < m_aHeld.Count(); )
		{
			MUD_NetMsg msg = m_aHeld[i];
			bool mine = msg.m_iKey == key || (key < 0 && !m_sJipPending.Contains(msg.m_iKey));
			if (!mine)
			{
				i++;
				continue;
			}
			m_aHeld.RemoveOrdered(i);
			int stamp = fromSeq;
			if (key < 0 && m_mFullSeq.Contains(msg.m_iKey))
				stamp = m_mFullSeq.Get(msg.m_iKey);
			if (msg.m_iSeq < stamp)
			{
				m_iJipDropped++;
				continue;
			}
			ApplyRect(msg.m_iKey, msg.m_iRect & (RUSH_BIT - 1), msg.m_aPacked);     // a join replay is not rushed
			m_iJipReplayed++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: write a patch-local rect of heights (mm) into the patch (made now if missing) and every
	//! copy of its edge verts. Not the wheel cap: a capped EnsurePatch returns null and the message
	//! would be lost for good (reliable Rpc, never resent).
	protected void ApplyRect(int key, int rect, array<int> packed)
	{
		int x0 = rect & 63;
		int x1 = (rect >> 6) & 63;
		int z0 = (rect >> 12) & 63;
		int z1 = (rect >> 18) & 63;
		if (key < 0 || key >= m_iNPX * m_iNPZ || x1 > PCELLS || z1 > PCELLS || x0 > x1 || z0 > z1
			|| packed.Count() != ((x1 - x0 + 1) * (z1 - z0 + 1) + 1) / 2)
		{
			m_iNetBad++;
			return;
		}
		int px = key - (key / m_iNPX) * m_iNPX;
		int pz = key / m_iNPX;
		MUD_FinePatch p;
		bool crater = (rect & RUSH_BIT) != 0;
		if (FarFromCamera(px, pz, crater))
			p = HeightsOnly(px, pz, crater);
		else if (BUILD_ONCE && (rect & RUSH_BIT) != 0)
			p = StartOnly(px, pz);     // CRATER BUILD ONCE: the rush builds it next frame with these heights
		else
			p = EnsurePatch(px, pz);
		if (!p)
		{
			m_iNetBad++;
			return;
		}
		int gx0 = px * PCELLS;
		int gz0 = pz * PCELLS;
		int i;
		for (int z = z0; z <= z1; z++)
		{
			for (int x = x0; x <= x1; x++)
			{
				SetH(gx0 + x, gz0 + z, Unpack(packed[i / 2], i - (i / 2) * 2) * 0.001);
				i++;
			}
		}
		m_bRushMark = (rect & RUSH_BIT) != 0;     // CRATER RUSH: a crater's patches build next frame
		MarkDirty(gx0 + x0, gx0 + x1, gz0 + z0, gz0 + z1);
		m_bRushMark = false;
		m_iNetGot++;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: patch (px, pz) is farther than m_fClientBuildRadius from the camera
	protected bool FarFromCamera(int px, int pz, bool crater)
	{
		if (m_fClientBuildRadius <= 0 || !IsProxy())
			return false;
		vector cam[4];
		GetWorld().GetCurrentCamera(cam);
		return PatchDist(px, pz, cam[3]) > FarR(crater);
	}

	//------------------------------------------------------------------------------------------------
	//! Client build radius (m): CRATER VIEW stretches a crater's to this client's view distance
	protected float FarR(bool crater)
	{
		if (crater && CRATER_VIEW)
			return Math.Max(m_fClientBuildRadius, GetGame().GetViewDistance());
		return m_fClientBuildRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Server send radius (m) of one change: CRATER VIEW stretches a crater's to the server's view distance limit
	protected float SendR(int rect)
	{
		if (CRATER_VIEW && (rect & RUSH_BIT) != 0)
			return Math.Max(m_fSendRadius, GetGame().GetViewDistanceServerLimit());
		return m_fSendRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! XZ distance from pos to the square of patch (px, pz), 0 inside
	protected float PatchDist(int px, int pz, vector pos)
	{
		float size = PCELLS * m_fFine;
		float x0 = m_fX0 + px * size;
		float z0 = m_fZ0 + pz * size;
		float dx = Math.Max(Math.Max(x0 - pos[0], 0), pos[0] - x0 - size);
		float dz = Math.Max(Math.Max(z0 - pos[2], 0), pos[2] - z0 - size);
		return Math.Sqrt(dx * dx + dz * dz);
	}

	//------------------------------------------------------------------------------------------------
	//! Client, far patch: made (setup only) if missing, never built here. A patch already live or asleep
	//! is returned as it is (its rebuild / wake works as before). FarDue builds it once the camera nears.
	protected MUD_FinePatch HeightsOnly(int px, int pz, bool crater)
	{
		int key = pz * m_iNPX + px;
		MUD_FinePatch p = m_mPatches.Get(key);
		if (!p)
		{
			p = StartPatch(px, pz);
			if (!p)
				return null;
			m_iMadeStamp++;
		}
		if (p.m_iBuild > 0 && !p.m_bAsleep)
		{
			m_sFar.Insert(key);
			if (crater)
				m_sFarCrater.Insert(key);
			m_iFarHeld++;
		}
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: once a second, the held far patches now within m_fClientBuildRadius of the camera; every
	//! frame those still unbuilt join Ahead's candidates (after the vehicle ones: distance + radius).
	protected void FarDue(array<int> candKey, array<float> candDist)
	{
		if (m_sFar.IsEmpty())
			return;
		if (m_fTime >= m_fFarCheckAt)
		{
			m_fFarCheckAt = m_fTime + 1;
			vector cam[4];
			GetWorld().GetCurrentCamera(cam);
			m_aFarDue.Clear();
			m_aFarDueDist.Clear();
			foreach (int fk : m_sFar)
			{
				float fd = PatchDist(fk - (fk / m_iNPX) * m_iNPX, fk / m_iNPX, cam[3]);
				if (m_fClientBuildRadius <= 0 || fd <= FarR(m_sFarCrater.Contains(fk)))
				{
					m_aFarDue.Insert(fk);
					m_aFarDueDist.Insert(fd);
				}
			}
		}
		for (int i = m_aFarDue.Count() - 1; i >= 0; i--)
		{
			int k = m_aFarDue[i];
			MUD_FinePatch p = m_mPatches.Get(k);
			if (!p || p.m_iBuild == 0 || p.m_bAsleep || p.m_iBuildFails >= 3)
			{
				// built (or freed, or given up): no longer held
				m_aFarDue.Remove(i);
				m_aFarDueDist.Remove(i);
				m_sFar.RemoveItem(k);
				m_sFarCrater.RemoveItem(k);
				continue;
			}
			if (k != m_iBuildKey && !candKey.Contains(k))
			{
				candKey.Insert(k);
				candDist.Insert(m_fAheadRadius + m_aFarDueDist[i]);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Joining client gets only the keys of the deformed patches; the heights follow per player
	override bool RplSave(ScriptBitWriter writer)
	{
		array<int> keys = {};
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			if (p.m_bDeformed)
				keys.Insert(key);
		}
		writer.WriteInt(keys.Count());
		foreach (int k : keys)
			writer.WriteInt(k);
		Print(string.Format("MUD world JIP: RplSave %1 keys, %2 bytes", keys.Count(), 4 + 4 * keys.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Can come before Setup (MUD_GroundEntity.c m_aJipMm): only fills m_sJipPending
	override bool RplLoad(ScriptBitReader reader)
	{
		int n;
		if (!reader.ReadInt(n) || n < 0 || n > 4000000)
			return false;
		for (int i = 0; i < n; i++)
		{
			int k;
			if (!reader.ReadInt(k))
				return false;
			if (k >= 0)
				m_sJipPending.Insert(k);
		}
		m_iJipTotal = m_sJipPending.Count();
		m_bJipNeeded = true;
		Print(string.Format("MUD world JIP: RplLoad %1 keys, %2 to fetch", n, m_iJipTotal));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client, once ready and its player controller exists: ask the server for the full copies
	protected void AskJip()
	{
		if (!m_bJipNeeded)
			return;
		// an ask sent before this client owns its controller can be lost (10:25 run 2026-10-10: no ask
		// reached the server, 6 keys pending forever) - ask again while nothing arrives
		if (m_bJipAsked && (m_fTime - m_fJipLastAsk < JIP_RETRY || m_iJipAsks >= JIP_MAX_ASKS))
			return;
		if (m_sJipPending.IsEmpty())
		{
			m_bJipNeeded = false;
			Print("MUD world JIP: 0 patches to fetch");
			return;
		}
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;
		pc.MUD_RequestJip();
		if (!m_bJipAsked)
			m_fJipStart = m_fTime;
		m_bJipAsked = true;
		m_fJipLastAsk = m_fTime;
		m_iJipAsks++;
		Print(string.Format("MUD world JIP: asking for %1 patches (try %2 of %3, player %4)", m_sJipPending.Count(), m_iJipAsks, JIP_MAX_ASKS, pc.GetPlayerId()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: queue every deformed patch for player pid, nearest to `near` first
	void QueueJip(int pid, IEntity near)
	{
		Print(string.Format("MUD world JIP: ask from player %1 arrived (ready %2)", pid, m_bReady));
		if (IsProxy() || !m_bReady)
			return;
		// one queue per player: a repeated ask would queue every deformed patch again (review 2026-10-10)
		foreach (MUD_JipItem q : m_aJip)
		{
			if (q.m_iPid == pid && !q.m_bCatchUp)
				return;
		}
		// per-player cap (master list 'JIP ask cap'): each ask after the queue ran out is a full resend
		// (~4.4 MB at 2000 patches) that delays everyone else. A real client asks at most JIP_MAX_ASKS
		// times, JIP_RETRY s apart; a quiet spell of JIP_ASK_RESET s (a rejoin later) starts the count over.
		float lastAsk;
		if (m_mJipAskTime.Find(pid, lastAsk) && m_fTime - lastAsk > JIP_ASK_RESET)
			m_mJipAskCount.Set(pid, 0);
		int asks = m_mJipAskCount.Get(pid);
		if (asks > 0 && (asks >= JIP_MAX_ASKS || m_fTime - lastAsk < JIP_RETRY * 0.5))
		{
			Print(string.Format("MUD world JIP: ask from player %1 refused (%2 asks, last %3 s ago)", pid, asks, m_fTime - lastAsk), LogLevel.WARNING);
			return;
		}
		m_mJipAskCount.Set(pid, asks + 1);
		m_mJipAskTime.Set(pid, m_fTime);
		vector o = vector.Zero;
		if (near)
			o = near.GetOrigin();
		float size = PCELLS * m_fFine;
		array<int> order = {};
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			if (!p.m_bDeformed)
				continue;
			float dx = m_fX0 + (p.m_iPx + 0.5) * size - o[0];
			float dz = m_fZ0 + (p.m_iPz + 0.5) * size - o[2];
			int du = Math.ClampInt(Math.Sqrt(dx * dx + dz * dz) / size, 0, 4095);
			order.Insert((du << 18) | key);     // key < 262144 (Setup guard), du << 18 < 2^30
		}
		order.Sort();
		foreach (int e : order)
		{
			MUD_JipItem it = new MUD_JipItem();
			it.m_iPid = pid;
			it.m_iKey = e & 262143;
			it.m_fSendAt = m_fTime;
			m_aJip.Insert(it);
		}
		Print(string.Format("MUD world JIP: player %1 asked, %2 deformed patches queued", pid, order.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: up to m_iJipPerFrame full copies per frame, queue order
	protected void SendJip()
	{
		int sent;
		for (int i = 0; i < m_aJip.Count() && sent < m_iJipPerFrame; )
		{
			MUD_JipItem it = m_aJip[i];
			if (it.m_fSendAt > m_fTime)
			{
				i++;
				continue;
			}
			m_aJip.RemoveOrdered(i);
			SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(it.m_iPid));
			if (!pc)
				continue;      // player left: nothing owed
			MUD_FinePatch p = m_mPatches.Get(it.m_iKey);
			array<int> packed = {};   // patch gone: an empty copy still ends the client's wait for this key
			if (p)
				packed = PackFull(p);
			// every live message numbered < m_iSeq is inside this copy; later ones are applied on top
			pc.MUD_SendFull(it.m_iKey, m_iSeq, packed);
			sent++;
			if (it.m_bCatchUp)
				m_iNetCaught++;
			else
				m_iJipSent++;
			// owed to this player (send by distance): this copy carries it, live messages go again
			map<int, bool> owed = m_mOwed.Get(it.m_iPid);
			if (owed)
				owed.Remove(it.m_iKey);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a full patch copy stamped with seq; held live messages for it with seq >= stamp go on top.
	//! An empty copy = the server has no such patch any more: only the wait ends.
	void ApplyFull(int key, int seq, array<int> packed)
	{
		int at = m_sJipPending.Find(key);
		if (at < 0)
		{
			// send by distance: a catch-up copy (this player came near a patch changed while far). It
			// arrives in order with the live messages (same controller), so it is applied as it is.
			// Only with send by distance on: then live messages come the same way, in order. Off (broadcast
			// live) a repeated JIP copy may be older than live messages already applied: dropped as before.
			if (!m_bReady || m_fSendRadius <= 0 || (m_mFullSeq.Contains(key) && seq < m_mFullSeq.Get(key)))
			{
				m_iJipDropped++;
				return;
			}
			m_mFullSeq.Set(key, seq);
			if (!packed.IsEmpty())
				ApplyRect(key, 0 | (PCELLS << 6) | (0 << 12) | (PCELLS << 18), packed);
			m_iNetCaught++;
			return;
		}
		m_sJipPending.Remove(at);
		m_fJipLastAsk = m_fTime;
		m_mFullSeq.Set(key, seq);
		if (!packed.IsEmpty())
			ApplyRect(key, 0 | (PCELLS << 6) | (0 << 12) | (PCELLS << 18), packed);
		ReplayHeld(key, seq);
		if (m_sJipPending.IsEmpty() && m_bJipNeeded)
		{
			m_bJipNeeded = false;
			Print(string.Format("MUD world JIP: %1 patches in %2 s, held %3, replayed %4, dropped %5", m_iJipTotal, m_fTime - m_fJipStart, m_iJipHeld, m_iJipReplayed, m_iJipDropped));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Debug, every m_fHashEvery s on every machine: one line per patch with a non-zero height
	protected void HashLines(float timeSlice)
	{
		if (m_fHashEvery <= 0)
			return;
		m_fSinceHash += timeSlice;
		if (m_fSinceHash < m_fHashEvery)
			return;
		m_fSinceHash = 0;
		array<string> lines = {};
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			int s1 = 0;     // explicit: a loop-body local is not trusted to reset each pass
			int s2 = 0;
			bool any = false;
			for (int v = 0; v < PVERTS * PVERTS; v++)
			{
				int mm = ToMm(p.m_aH[v]);
				if (mm == 0)
					continue;
				any = true;
				s1 += mm;
				s2 += mm * ((v - (v / PVERTS) * PVERTS) + 1);   // weight = column + 1, so a moved height changes s2
			}
			if (any)
				lines.Insert(string.Format("MUD hash %1 %2 %3", key, s1, s2));
		}
		m_iHashBlock++;
		Print(string.Format("MUD hash block %1 patches %2", m_iHashBlock, lines.Count()));
		foreach (string l : lines)
			Print(string.Format("%1", l));   // Print(l) logs "string l = '...'" (2026-10-10)
		Print(string.Format("MUD hash end %1", m_iHashBlock));
	}

	//------------------------------------------------------------------------------------------------
	//! Debug: every vertex two patches share must have the same world height in both (pairs right and
	//! up of each patch), and a deformed vertex on a patch edge must not face the skin (a missing
	//! neighbour that has hole cells): either is a visible step
	protected void SeamCheck()
	{
		int pairs;
		int shared;
		int bad;
		float worst;
		int open;
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			for (int side = 0; side < 4; side++)
			{
				int nx = p.m_iPx;
				int nz = p.m_iPz;
				if (side == 0)
					nx++;
				else if (side == 1)
					nz++;
				else if (side == 2)
					nx--;
				else
					nz--;
				MUD_FinePatch o;
				if (nx >= 0 && nz >= 0 && nx < m_iNPX && nz < m_iNPZ)
					o = m_mPatches.Get(nz * m_iNPX + nx);
				for (int i = 0; i < PVERTS; i++)
				{
					// vertex on this side, local in p and in o
					int vx = PCELLS;
					int vz = i;
					int ox = 0;
					int oz = i;
					if (side == 1)
					{
						vx = i;
						vz = PCELLS;
						ox = i;
						oz = 0;
					}
					else if (side == 2)
					{
						vx = 0;
						ox = PCELLS;
					}
					else if (side == 3)
					{
						vx = i;
						vz = 0;
						ox = i;
						oz = PCELLS;
					}
					int v = vz * PVERTS + vx;
					// a neighbour still building or lazy READY shows the skin, as a missing one
					if ((!o || o.m_iBuild > 0) && p.m_iBuild == 0 && Math.AbsFloat(p.m_aH[v]) > 0.0005 && HasHole(nx, nz))
						open++;
					if (!o)
						continue;
					if (side >= 2)
						continue;   // pairs counted once, from the left / lower patch
					if (i == 0)
						pairs++;
					shared++;
					int w = oz * PVERTS + ox;
					float e = Math.AbsFloat(p.m_aBase[v] + p.m_aH[v] - o.m_aBase[w] - o.m_aH[w]);
					if (e > 0.001)
						bad++;
					if (e > worst)
						worst = e;
				}
			}
		}
		Print(string.Format("MUD world seams: pairs %1, shared verts %2, mismatched %3 (worst %4 m), deformed verts on a border to the coarse skin %5", pairs, shared, bad, worst, open));
	}

	//------------------------------------------------------------------------------------------------
	//! Test kit (back 2026-10-17; TEMP probe 2026-10-11, MUD_JoltProbe). False = not our ground (kept terrain: road, water edge).
	//! Within 0.5 m of wp: pend = how far (m) the newest heights differ from the live collider (held
	//! under a wheel, or not rebuilt yet), age = seconds since a rebuild last moved that collider (1000 =
	//! never), dirty = a patch there waits for smoothing or a rebuild.
	bool ProbeGround(vector wp, out float pend, out float age, out bool dirty)
	{
		pend = 0;
		age = 1000;
		dirty = false;
		float fx;
		float fz;
		if (!m_bReady || !m_Skin || !ToFine(wp, fx, fz))
			return false;
		int cx = Math.Floor(fx / FINE);
		int cz = Math.Floor(fz / FINE);
		if (!m_Skin.IsHoleCell(cx, cz))
			return false;
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		int gx0 = Math.ClampInt(Math.Round(fx) - 2, 0, gmax);
		int gx1 = Math.ClampInt(Math.Round(fx) + 2, 0, gmax);
		int gz0 = Math.ClampInt(Math.Round(fz) - 2, 0, gzmax);
		int gz1 = Math.ClampInt(Math.Round(fz) + 2, 0, gzmax);
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				if (p.m_bColDirty || p.m_iSx0 >= 0)
					dirty = true;
				if (!p.m_bColValid)
					continue;
				pend = Math.Max(pend, Math.AbsFloat(p.m_aBase[v] + p.m_aSmooth[v] - p.m_aColH[v]));
				age = Math.Min(age, m_fTime - p.m_fColChangeAt);
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every 10 s while m_bDebug (Stats; patch memory baseline 2026-10-15). live = built and awake; heights-only =
	//! client far patch held unbuilt (m_sFar); lazy = made ahead, index lists only, skin carries it (IsLazyReady,
	//! freed after m_fFreeAfter like any undeformed patch); building = the rest.
	//! live + asleep + heights-only + lazy + building = total.
	//! floor-only and deformed overlap the other four (floor-only = m_bFloor && !m_bDeformed).
	//! fps + moving vehicles (> 2 m/s now) added for the review stress run 2026-10-15.
	//! floor free (review Stage 4, 2026-10-16): FLOOR_FREE, rut patch keys known, in FieldDue range, made by FieldDue, freed.
	protected void PatchCounts(float fps)
	{
		int nMoving;
		foreach (Vehicle mv : MUD_Wheels.GetAll())
		{
			if (mv && mv.GetPhysics() && mv.GetPhysics().GetVelocity().Length() > 2)
				nMoving++;
		}
		int nLive;
		int nAsleep;
		int nBuilding;
		int nLazy;
		int nHeights;
		int nFloorOnly;
		int nDeformed;
		foreach (int ck, MUD_FinePatch cp : m_mPatches)
		{
			if (cp.m_bAsleep)
				nAsleep++;
			else if (cp.m_iBuild == 0)
				nLive++;
			else if (m_sFar.Contains(ck))
				nHeights++;
			else if (IsLazyReady(cp))
				nLazy++;
			else
				nBuilding++;
			if (cp.m_bDeformed)
				nDeformed++;
			else if (cp.m_bFloor)
				nFloorOnly++;
		}
		// test kit (2026-10-17): queue = m_aDirty; waiting = awake, not building, with dirt this machine builds (a DS
		// builds no meshes); oldest dirty = longest of those since it was queued or last built
		int nWaiting;
		float oldestDirty;
		foreach (int dk : m_aDirty)
		{
			MUD_FinePatch dp = m_mPatches.Get(dk);
			if (!dp || dp.m_bAsleep || dp.m_iBuild > 0)
				continue;
			if (!dp.m_bColDirty && !(m_bRender && dp.m_bMeshDirty))
				continue;
			nWaiting++;
			if (m_fTime - dp.m_fQueuedAt > oldestDirty)
				oldestDirty = m_fTime - dp.m_fQueuedAt;
		}
		Print(string.Format("MUD patches: live %1, asleep %2, floor-only %3, deformed %4, heights-only %5, lazy %6, building %7, total %8, mem %9 KB",
			nLive, nAsleep, nFloorOnly, nDeformed, nHeights, nLazy, nBuilding, m_mPatches.Count(), System.MemoryAllocationKB())
			+ string.Format(" | fps %1, vehicles moving %2 of %3, parked / far skips %4, voxel op chars sent %5", fps, nMoving, MUD_Wheels.GetAll().Count(), m_iParkSkips, m_iOpChars)
			+ string.Format(" | floor settle %1: released quiet %2, max wait %3, held patch-frames %4", FLOOR_SETTLE, m_iSettleQuiet, m_iSettleMax, m_iSettleHeld)
			+ string.Format(" | render gate %1: mesh waits (patch-frames) %2 | look covers %3: rut ops mesh kept %4", RENDER_GATE, m_iRenderSkip, LOOK_COVERS, m_iLookCovered)
			+ string.Format(" | floor free %1: rut patch keys %2, in range %3, made near %4, floor-only freed %5", FLOOR_FREE, m_sFieldPatches.Count(), m_aFieldDue.Count(), m_iFieldMade, m_iFloorFreed)
			+ string.Format(" | queue %1 (waiting %2), oldest dirty %3 s, ops %4", m_aDirty.Count(), nWaiting, oldestDirty, MUD_VoxelProbe.OpCount())
			+ string.Format(" | lazy rut mesh %1: hidden under the sheet %2, held patch-frames %3, shown again %4", MUD_MudSheet.LazyOn(), m_aSheetHid.Count(), m_iSheetHeld, m_iSheetShown) + " - cover " + MUD_MudSheet.CoverWhy());
		m_iParkSkips = 0;     // audit B1 / B2 / B4 meters: per 10 s
		m_iOpChars = 0;
		if (m_bRender && MUD_MudSheet.FAR_PRESS)
		{
			Print(string.Format("MUD far press 10 s: on %1, mesh cap %2, stored sheet meshes %3 | marks %4, rebuilds sent %5, waiting %6, marked patches %7",
				MUD_MudSheet.s_bFarPress, MUD_MudSheet.s_iMeshCap, MUD_MudSheet.MeshesStored(), m_iFarMarks, m_iFarSent, m_mFarDue.Count(), m_sFarKeys.Count())
				+ string.Format(" | meshes pressed %1, no mud %2, verts pressed %3, soil verts %4, border unpressed %5, hole guard %6, neighbour heals %7",
				m_iFarBuilt, m_iFarEmpty, m_iFarVerts, m_iFarSoil, m_iFarFaded, m_iFarHole, m_iFarHeal), LogLevel.NORMAL);
			m_iFarMarks = 0;
			m_iFarSent = 0;
			m_iFarBuilt = 0;
			m_iFarEmpty = 0;
			m_iFarVerts = 0;
			m_iFarSoil = 0;
			m_iFarFaded = 0;
			m_iFarHole = 0;
			m_iFarHeal = 0;
		}
		m_iSheetHeld = 0;
		m_iSheetShown = 0;
		m_iFieldMade = 0;
		m_iFloorFreed = 0;
		m_iRenderSkip = 0;
		m_iLookCovered = 0;
		m_iSettleQuiet = 0;
		m_iSettleMax = 0;
		m_iSettleHeld = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Every 10 s while something happened
	protected void Stats(float timeSlice)
	{
		m_fSinceStats += timeSlice;
		m_iStatFrames++;
		if (m_fSinceStats < 10)
			return;
		float fps = m_iStatFrames / m_fSinceStats;
		m_fSinceStats = 0;
		m_iStatFrames = 0;
		PatchCounts(fps);
		if (m_iMadeAhead + m_iMadeStamp + m_iFreed + m_iMeshBuilds + m_iColBuilds + m_iSlept + m_iWoken + m_iCraters + m_iCraterSkips + m_iDigs + m_iNetMsgs + m_iNetGot == 0)
			return;
		Print(string.Format("MUD world: last 10 s - made %1 ahead + %2 by stamps, freed %3, meshes %4 (%5 ms), colliders %6 (%7 ms), smoothing %8 ms, skin swaps %9 ms",
			m_iMadeAhead, m_iMadeStamp, m_iFreed, m_iMeshBuilds, m_iMeshMs, m_iColBuilds, m_iColMs, m_iSmoothMs, m_iSwapMs));
		Print(string.Format("MUD world: patches %1 (deformed %2), mem +%3 KB since ready, worst frame %4 ms, skips (height) %5 | check new: points %6, hit %7, within 5 mm %8, worst %9 m",
			m_mPatches.Count(), m_iDeformed, System.MemoryAllocationKB() - m_iMemStartKB, m_iWorstMs, m_iSkipHeight, m_iChkPts, m_iChkHit, m_iChkGood, m_fChkWorst));
		Print("MUD world: worst frame split (ms): " + m_sWorstSplit);
		if (m_bAheadRuts)
		{
			Print(string.Format("MUD ahead: last 10 s - pressed %1, skipped %2 (within %3 m of a wheel) | lead %4 m + %5 s x speed, max %6 m, reveal %7 m",
				m_iAheadPressed, m_iAheadSkipped, m_fAheadClear, m_fAheadMin, m_fAheadTime, m_fAheadMax, m_fRevealAhead));
			Print(string.Format("MUD ahead: last 10 s - leading axle skipped %1 | rut fill: changed ground %2 of %3 fill presses, no clear point %4 (fill clear %5 m)",
				m_iLeadSkipped, m_iFills, m_iFillTried, m_iFillSkipped, m_fFillClear));
			Print(string.Format("MUD ahead: last 10 s - undo: %1 presses reached, %2 put back (%3 pending) | fill skipped off the front track %4 | ramp cut %5 points",
				m_iKept, m_iUndone, m_aPend.Count(), m_iFillOffTrack, m_iRampCuts));
			m_iKept = 0;
			m_iUndone = 0;
			m_iFillOffTrack = 0;
			m_iRampCuts = 0;
			m_iAheadPressed = 0;
			m_iAheadSkipped = 0;
			m_iLeadSkipped = 0;
			m_iFills = 0;
			m_iFillTried = 0;
			m_iFillSkipped = 0;
			if (m_bScrape)
				Print(string.Format("MUD scrape: last 10 s - %1 underside points cut ground (margin %2 m, step %3 m per pass, max depth %4 m)", m_iScrapeCuts, m_fScrapeMargin, m_fScrapeStep, m_fScrapeMaxDepth));
			m_iScrapeCuts = 0;
			if (m_bEscape)
				Print(string.Format("MUD escape: last 10 s - %1 dig steps, %2 points dug (step %3 m every %4 s)", m_iEscapeSteps, m_iEscapeCuts, m_fEscapeStep, m_fEscapeInterval));
			m_iEscapeSteps = 0;
			m_iEscapeCuts = 0;
			if (m_iSpills > 0)
				Print(string.Format("MUD spill: last 10 s - %1 spills, %2 m3 piled beside vehicles (share %3)", m_iSpills, m_fSpillVol.ToString(-1, 3), SPILL_SHARE));
			m_iSpills = 0;
			m_fSpillVol = 0;
		}
		Print(string.Format("MUD world: presses %1, craters %2 (skipped %3, %4 ms, worst %5 ms), digs %6, most collider pieces %7 in 10 s", m_iPresses, m_iCraters, m_iCraterSkips, m_iCraterMs, m_iCraterWorstMs, m_iDigs, m_iMaxGeoms));
		Print(string.Format("MUD world: make %1 ms (new patches without their builds) | index sets: hits %2, misses %3, cached %4",
			m_iMakeMs, m_iIdxHits, m_iIdxMisses, m_aIdxSets.Count()));
		m_iCraters = 0;
		m_iCraterSkips = 0;
		m_iCraterMs = 0;
		m_iCraterWorstMs = 0;
		m_iDigs = 0;
		m_iMaxGeoms = 0;
		Print(string.Format("MUD world: budget %1 ms - waits: ahead %2, mesh %3, collider %4, free %5 | rushed %6 (made ahead, finished at once by a stamp) | wheel presses that waited (patch missing / building) %7",
			m_iFrameBudget, m_iWaitAhead, m_iWaitMesh, m_iWaitCol, m_iWaitFree, m_iRushed, m_iWheelCapped)
			+ string.Format(" | floor spread: %1 patches (look %4), %2 steps, %3 waiting", m_iFloorSpreadAdds, m_iFloorSpreadSteps, m_aFloorSpread.Count(), m_iLookSpreadAdds)
			+ string.Format(" | PAPER TEST 3 small collider skips %1 (COL_SMALL_M %2, max %3 s)", m_iColSkipSmall, COL_SMALL_M, COL_SMALL_MAX_S)
			+ string.Format(" | PERF IDEA 2 small mesh skips %1 (MESH_SMALL_M %2, max %3 s)", m_iMeshSkipSmall, MESH_SMALL_M, MESH_SMALL_MAX_S));
		m_iColSkipSmall = 0;
		m_iMeshSkipSmall = 0;
		m_iFloorSpreadAdds = 0;
		m_iLookSpreadAdds = 0;
		m_iFloorSpreadSteps = 0;
		int ready;
		foreach (int rk, MUD_FinePatch rp : m_mPatches)
		{
			if (IsLazyReady(rp))
				ready++;
		}
		Print(string.Format("MUD world lazy %1: stopped READY %2 in 10 s, held READY now %3 of %4 patches | client far (> %5 m): msgs kept as heights %6, patches held %7, due %8",
			m_bLazy, m_iReadyNow, ready, m_mPatches.Count(), m_fClientBuildRadius, m_iFarHeld, m_sFar.Count(), m_aFarDue.Count()));
		m_iReadyNow = 0;
		m_iFarHeld = 0;
		Print(string.Format("MUD world look slope (SLOPE %1): patch caps %2 in 10 s, %3 ms script", MUD_LookLift.SLOPE, m_iLookCapN, m_iLookCapMs));
		m_iLookCapN = 0;
		m_iLookCapMs = 0;
		Print(string.Format("MUD world wheel hold (%1 s): verts kept under a wheel %2 in %3 collider builds", m_fWheelHold, m_iHeldVerts, m_iHeldBuilds));
		m_iHeldVerts = 0;
		m_iHeldBuilds = 0;
		int live = m_mPatches.Count() - m_iAsleep;
		int slept = Math.ClampInt(m_iSlept, 1, 1000000);
		int woken = Math.ClampInt(m_iWoken, 1, 1000000);
		Print(string.Format("MUD world: sleep - live %1, asleep %2 | slept %3 (gave back %4 KB, %5 each) | woke %6 (%7 at once by a wheel / stamp; took %8 KB, %9 each)",
			live, m_iAsleep, m_iSlept, m_iSleepKB, m_iSleepKB / slept, m_iWoken, m_iWokeNow, m_iWakeKB, m_iWakeKB / woken));
		Print(string.Format("MUD world: sleep waits %1 | feet wakes %2 (asleep patch under a player, woken at once)", m_iWaitSleep, m_iFeetWoke));
		m_iSlept = 0;
		m_iWoken = 0;
		m_iWokeNow = 0;
		m_iFeetWoke = 0;
		m_iSleepKB = 0;
		m_iWakeKB = 0;
		m_iWaitSleep = 0;
		m_iWheelCapped = 0;
		m_iWaitAhead = 0;
		m_iWaitMesh = 0;
		m_iWaitCol = 0;
		m_iWaitFree = 0;
		m_iRushed = 0;
		m_iPresses = 0;
		m_iMakeMs = 0;
		m_iIdxHits = 0;
		m_iIdxMisses = 0;
		Print(string.Format("MUD world net: sent %1 msgs %2 ints, got %3 (bad %4), JIP held %5 replayed %6 dropped %7, seq %8", m_iNetMsgs, m_iNetInts, m_iNetGot, m_iNetBad, m_iJipHeld, m_iJipReplayed, m_iJipDropped, m_iSeq));
		int owedN;
		foreach (int op, map<int, bool> om : m_mOwed)
		{
			if (om)
				owedN += om.Count();
		}
		Print(string.Format("MUD world net by distance (%1 m): live per player %2, owed to far players %3 (owed now %4), catch-up copies %5", m_fSendRadius, m_iNetLive, m_iNetOwed, owedN, m_iNetCaught));
		m_iNetLive = 0;
		m_iNetOwed = 0;
		m_iNetCaught = 0;
		m_iNetMsgs = 0;
		m_iNetInts = 0;
		m_iNetGot = 0;
		Print(string.Format("MUD world JIP: sent %1 full copies in 10 s, %2 queued, %3 pending here", m_iJipSent, m_aJip.Count(), m_sJipPending.Count()));
		m_iJipSent = 0;
		if (m_bDebug)
			SeamCheck();    // scans every patch: m_bDebug only, not the test kit (a 10 s spike in PERF)
		m_iMadeAhead = 0;
		m_iMadeStamp = 0;
		m_iFreed = 0;
		m_iMeshBuilds = 0;
		m_iMeshMs = 0;
		m_iColBuilds = 0;
		m_iColMs = 0;
		m_iSmoothMs = 0;
		m_iSwapMs = 0;
		m_iWorstMs = 0;
		m_sWorstSplit = "";
	}
}
