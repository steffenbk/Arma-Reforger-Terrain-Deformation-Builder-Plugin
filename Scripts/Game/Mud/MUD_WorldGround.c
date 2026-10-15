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
//! Algorithms copied from MUD_GroundEntity, line refs at each method.
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
	static const float FIELD_SOIL_D = 0.03;
	static const int PVERTS = 33;
	static const int SKIN = 4;          // skin cells per patch side
	static const int FINE = 8;          // fine cells per skin cell side
	static const float RESCUE_DEPTH = 0.7;  // feet this far under our ground (and collider): lift back on top. Prone sat up
	                                        // to ~0.3 m under while fine (fall probe 2026-10-10); a real fall keeps going
	static const int MAT_SLOTS = 4;    // surface pieces per collider; + bullet slab + safety plate = 6 (MakeStatic max)
	static const int TLBR = 16;         // MUD_FinePatch.m_aCell bit: the skin cell's shared edge is tl-br
	static const int VOX = 32;          // MUD_FinePatch.m_aCell bit, TEMP probe 2026-10-13 (MUD_VoxelProbe overlay): a live
	                                    // voxel tile carries the cell - no patch mesh, no patch collider there
	static const int LOOK = 64;         // MUD_FinePatch.m_aCell bit, TEMP probe 2026-10-14 (MUD_VoxelProbe RUT_LOOK): a look rut
	                                    // draws there (voxel mesh) - no patch mesh, the patch COLLIDER stays (until VOX)
	static const int IDX_SETS = 16;     // cached index layouts, MUD_IdxSet (~120-200 KB each)
	static const int SLOT = 15;
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

	[Attribute("1.5", UIWidgets.EditBox, "Crater depth (m) for 1 kg TNT. Depth = this x (charge weight x TNT equivalent in kg) ^ TNT Power up to Max Depth, read from the exploding thing (ammo -> warhead, mine, charge, vehicle blast).", category: "Craters")]
	protected float m_fCraterDepthPerKg;

	[Attribute("0.6", UIWidgets.EditBox, "TNT Power: how fast depth grows with charge. 0.6 = the line through 40 mm 0.3 m and mortar 1.5 m (rockets ~1 m). 0.33 = textbook cube root (40 mm much deeper).", category: "Craters")]
	protected float m_fCraterTntPower;

	[Attribute("1.35", UIWidgets.EditBox, "Knee of the size curve (m): bigger charges grow only with the cube root past it (81 mm mortar 0.99 kg -> 1.43 m, Hydra M229 2.9 kg -> 2.0 m, TM-62M 7.5 kg -> 2.8 m).", category: "Craters")]
	protected float m_fCraterMaxDepth;

	[Attribute("0.5", UIWidgets.EditBox, "TNT (kg) for exploding ammo whose warhead has no readable charge (modded ammo)", category: "Craters")]
	protected float m_fCraterFallbackTnt;

	[Attribute("0.3", UIWidgets.EditBox, "No crater at or below this TNT (kg): smoke 0.015-0.1, 40 mm 0.04-0.08, AP mines 0.03-0.05, hand grenades 0.11-0.24 (too small to see, user 2026-10-10: save the cost). TNT 400 g and rockets still dig.", category: "Craters")]
	protected float m_fCraterMinTnt;

	[Attribute("3", UIWidgets.EditBox, "Deepest any single blast digs (m): AT mines, vehicle ammo racks and fuel blasts reach it.", category: "Craters")]
	protected float m_fCraterHugeDepth;

	[Attribute("2", UIWidgets.EditBox, "Charges below this TNT (kg) are shrunk to fit 2 x 2 patches (outer edge 3.95 m, cheap rebuild). At or above it the crater may span more patches (rare, one slower rebuild).", category: "Craters")]
	protected float m_fCraterBigTnt;

	[Attribute("0.6", UIWidgets.EditBox, "HEAT / HEDP (shaped charge, the warhead fires a penetrator) digs this share of the depth: the jet goes into the target, not the ground. PG-7VM 1.0 m -> 0.6 m.", category: "Craters")]
	protected float m_fCraterHeatFactor;

	[Attribute("0.15", UIWidgets.EditBox, "Crater variation: depth x 1 +- this, random per crater (never over Max Depth). 0 = all the same.", category: "Craters")]
	protected float m_fCraterDepthJitter;

	[Attribute("0.15", UIWidgets.EditBox, "Crater variation: outline stretched up to this share by angle (random oval / three lobes, random turn). Only widens, so walls get no steeper. Shrinks where the outer edge would pass 3.95 m. 0 = round.", category: "Craters")]
	protected float m_fCraterShapeJitter;

	[Attribute("0.17", UIWidgets.EditBox, "Rim height as a share of the crater depth", category: "Craters")]
	protected float m_fCraterRimShare;

	[Attribute("0.4", UIWidgets.EditBox, "Rim width outside the crest as a share of the crater depth (at least 2 fine steps)", category: "Craters")]
	protected float m_fCraterRimWidthShare;

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

	[Attribute("0.3", UIWidgets.EditBox, "Band mesher: skirt hanging this far (m) under the patch rim, toward the skin / kept terrain", category: "Ground")]
	protected float m_fBandSkirt;

	[Attribute("8", UIWidgets.EditBox, "JIP: full patch copies sent per frame (all joining players together)", category: "Patches")]
	protected int m_iJipPerFrame;

	[Attribute("500", UIWidgets.EditBox, "Send by distance (F4): live ground changes go only to players within this radius (m) of the patch; a farther player gets the patch's full heights once they come within it (checked each second, sent through the JIP queue). 0 = every change to every client.", category: "Patches")]
	protected float m_fSendRadius;

	[Attribute("0", UIWidgets.CheckBox, "OFF: on our holed map the rebuild WIPES the navmesh tile (2026-10-12). Server: rebuild the AI navmesh over craters, trenches and shovel holes once they stop changing (ruts are skipped)", category: "AI navmesh")]
	protected bool m_bNavRefresh;

	[Attribute("8", UIWidgets.EditBox, "Rebuild an area once nothing changed in it for this long (s) - the digger is done", category: "AI navmesh")]
	protected float m_fNavQuiet;

	[Attribute("30", UIWidgets.EditBox, "Rebuild an area at the latest this long (s) after its first change, even if it keeps changing", category: "AI navmesh")]
	protected float m_fNavMaxWait;

	[Attribute("2", UIWidgets.EditBox, "Margin (m) added around the changed ground", category: "AI navmesh")]
	protected float m_fNavMargin;

	[Attribute("60", UIWidgets.EditBox, "Changes merge into one area up to this width (m); beyond it a new area starts", category: "AI navmesh")]
	protected float m_fNavMaxSide;

	protected ref MUD_NavRefresh m_Nav = new MUD_NavRefresh();
	protected MUD_CoarseSkinEntity m_Skin;
	protected ref map<int, ref MUD_FinePatch> m_mPatches = new map<int, ref MUD_FinePatch>();
	protected ref set<int> m_sNoHole = new set<int>();     // patch keys without a hole cell
	protected ref array<int> m_aDirty = {};                // patch keys with a pending mesh / collider
	protected ref MUD_FineBuffers m_Buf;
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
	protected ref array<int> m_aWheelPress = {};    // ahead ruts, per wheel index: 1 = on the ground and presses (end axle)
	protected ref array<float> m_aWheelZ = {};      // ahead ruts, per wheel index: vehicle-local z of the contact
	static const float AXLE_GAP = 0.3;              // ahead ruts: contacts within this (m, local z) are one axle
	protected ref array<ref MUD_Belly> m_aBellies = {};   // body scrape: measured undersides
	protected Vehicle m_BellyVeh;                   // body scrape: the vehicle BellyOnly lets through
	protected int m_iScrapeCuts;                    // body scrape: underside points that cut ground, 10 s
	protected int m_iEscapeSteps;                   // ditch escape: dig steps, 10 s
	protected int m_iEscapeCuts;                    // ditch escape: points that dug ground, 10 s
	static const float HIDE_HOLD = 0.1;             // ahead ruts: a vertex stays hidden this long (s) after its last mark (> one frame)
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
		MUD_VoxelProbe.Clear();    // TEMP probe 2026-10-12
		MUD_VoxelHouse.Clear();    // TEMP probe 2026-10-13
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
	static const float CRATER_HUGE_OUTER = 7.95;    // m: a big charge's crater stays within 3 x 3 patches
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
		MUD_GroundEntity.ResetTraceHide();
		if (m_iNPX * m_iNPZ > 262144)
			Print("MUD world: more than 262144 patches - JIP order keys overflow (Task 10)", LogLevel.ERROR);
		s_Inst = this;
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
				// per-player messages from before this entity existed go first (they came first)
				for (int e = s_aEarly.Count() - 1; e >= 0; e--)
					m_aHeld.InsertAt(s_aEarly[e], 0);
				s_aEarly.Clear();
				ReplayHeld(-1, 0);   // messages that came before this machine was ready
			}
			return;
		}

		int t0 = System.GetTickCount();
		int swap0 = m_iSwapMs;
		m_fTime += timeSlice;
		m_sFrameStep = "";
		m_sFrameRebuild = "";
		m_iWheelBuilds = 0;
		GroundRescue();
		MUD_VoxelProbe.Tick(m_fTime);    // TEMP probe 2026-10-12
		MUD_MudSheet.Tick(m_fTime);    // TEST 2026-10-14: visual mud sheet
		MUD_VoxelHouse.Tick();    // TEMP probe 2026-10-13
		Wheels(timeSlice);
		AheadView();
		int tW = System.GetTickCount(t0);
		// after the wheels every job checks the frame budget (Fix B: one new patch + a rebuild in one
		// frame was 10-18 ms, measured 2026-10-09 14:12 Exp 192624)
		Ahead(t0);
		int tA = System.GetTickCount(t0);
		Rebuild(t0);
		if (m_bNavRefresh && !IsProxy())
			m_Nav.Tick(m_fTime, m_fNavQuiet, m_fNavMaxWait, m_bDebug);
		int tR = System.GetTickCount(t0);
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
		if (ms > m_iWorstMs)
		{
			m_iWorstMs = ms;
			m_sWorstSplit = string.Format("wheels %1, ahead %2 (%3), rebuild %4 (%5), free %6 (%7 patches), skin swaps in it %8",
				tW, tA - tW, m_sFrameStep, tR - tA, m_sFrameRebuild, ms - tR, m_iFreed - freed0, m_iSwapMs - swap0);
		}
		if (m_bDebug)
			Stats(timeSlice);
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
					p.m_aBermTouch[lz * PVERTS + lx] = t;
				else
					p.m_aRutTouch[lz * PVERTS + lx] = t;
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
					bool newPass = m_fTime - p.m_aRutTouch[v] > m_fPassGap;
					SetTouch(gx, gz, false, m_fTime);
					// the pass digs from the height it began at, deepest profile wins: a moving (ahead) press first
					// touches a vertex with its rim (half depth) - one-shot passes left ruts half deep, no berms
					// (2026-10-11: rut fill changed ground on ~90 % of its presses on straight runs)
					if (newPass)
						p.m_aRutFrom[v] = h;
					float from = p.m_aRutFrom[v];
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
					bool newBerm = m_fTime - p.m_aBermTouch[v] > m_fPassGap;
					SetTouch(gx, gz, true, m_fTime);
					if (newBerm)
						p.m_aBermFrom[v] = h;
					// ruts win over berms: never push up a vertex that is already below the surface
					float bermTop = m_fBermHeight * soft;
					float bfrom = p.m_aBermFrom[v];
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
			if (m_bEscape)
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
				// TEMP all-voxel test: RUT_BEHIND's waiting segments still go out (4 s timeout) while it stands
				MUD_VoxelProbe.RutFlush(veh);
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
			// TEMP all-voxel test: the 2 leading wheels along the travel direction (convert ahead)
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
					// TEMP all-voxel test (2026-10-13): voxel ruts instead of the heightfield press / ahead / fill
					if (!m_bAheadRuts || m_aWheelPress[w] != 0)
						MUD_VoxelProbe.RutWheel(veh, w, cp, m_fRutStep, depth);
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
			if (voxAl1 > -1000)
				MUD_VoxelProbe.RutAhead(veh, voxLead0, voxLead1, dirW, hsW);     // TEMP all-voxel test (2026-10-13)
			if (voxAlMin < 1000)
				MUD_VoxelProbe.RutTail(veh, voxRear, dirW);     // TEMP all-voxel test (2026-10-13)
			MUD_VoxelProbe.RutFlush(veh);     // TEMP all-voxel test (2026-10-13)
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
		if (p.m_aPendAt[v] < 0)
			p.m_aPendOrig[v] = h;
		p.m_aPendAt[v] = m_fTime;
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
				p.m_aPendAt[v] = -1;
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
			if (!p || p.m_aPendAt[v] < 0 || p.m_aPendAt[v] > r.m_fAt)
				continue;   // reached since, or a newer pending press owns it
			p.m_aPendAt[v] = -1;
			SetH(gx, gz, p.m_aPendOrig[v]);
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
						if (p.m_aHideAt[v] >= since)
							p.m_aHideAt[v] = m_fTime;
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
		for (int i = m_aBellies.Count() - 1; i >= 0; i--)
		{
			MUD_Belly b = m_aBellies[i];
			if (!b.m_Veh)
				m_aBellies.Remove(i);
			else if (b.m_Veh == veh)
				return b;
		}
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
					p.m_aScrapeTouch[lz * PVERTS + lx] = t;
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
				if (b.m_aSpinSign[lw] != sign)
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
					bool newPass = m_fTime - p.m_aScrapeTouch[v] > m_fPassGap;
					SetScrapeTouch(gx, gz, m_fTime);
					if (!newPass)
						continue;
				}
				SetH(gx, gz, Math.Max(want, h - step));
				changed = true;
			}
		}
		if (changed)
			MarkDirty(gx0, gx1, gz0, gz1);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Ahead ruts, every rendering machine: hide the ground changes in front of each moving wheel (BuildMesh
	//! shows m_aShow there) and draw the debug points. Runs from each machine's own view of the vehicles, so
	//! the client hides what the server pressed ahead before the wheel gets there.
	protected void AheadView()
	{
		if (!m_bAheadRuts || !m_bRender)
			return;
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
				if (m_fTime < hb.m_fKeepUntil)
					HoldHide(hb);
			}
			if (stopped)
				continue;
			m_iHideX0 = -1;
			vector dir = flat * (1 / hs);
			// a vertex shows up to HIDE_HOLD + one mesh interval after it left the corridor: start the
			// corridor that much farther ahead so the rut still appears Reveal Ahead in front of the wheel
			float revealFrom = m_fRevealAhead + hs * (HIDE_HOLD + m_fMeshInterval);
			Contacts(veh, sim);
			BrakeCap(veh, sim, hs);
			float yaw = YawRate(veh, phys);
			int pressed = 0;
			int skipped = 0;
			float lead = 0;
			for (int w = 0; w < m_aWheelPress.Count(); w++)
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
				ArcHide(cp, vp, yaw, revealFrom / hs, (lead + m_fBermRadius + 0.5) / hs, m_fBermRadius);
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
						p.m_aHideAt[lz * PVERTS + lx] = m_fTime;
					}
				}
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
				p.m_aRutTouch[v] = -100;
				p.m_aBermTouch[v] = -100;
				p.m_aShow[v] = 0;
				p.m_aHideAt[v] = -100;
				p.m_aScrapeTouch[v] = -100;
				p.m_aRutFrom[v] = 0;
				p.m_aBermFrom[v] = 0;
				p.m_aPendOrig[v] = 0;
				p.m_aPendAt[v] = -1;
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
		if (MUD_VoxelProbe.GroundMode())
			return false;   // TEMP probe 2026-10-13: voxel ground test - no fine patch anywhere (ruts, trenches, shovel off)
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
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe overlay): every cell of p is kept terrain or voxel-carried (no mesh, no
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
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe overlay, any machine): the skin cells cx0..cx1, cz0..cz1 changed CELL_VOX
	//! (voxel bricks went live): each made patch there takes the new VOX bits. A live patch gets its collider now (in
	//! the frame the bricks show: else its old collider would hold the ground over a voxel crater; all cells voxel =
	//! just no body), its mesh in the rebuild queue; a building / sleeping one uses the bits in its next steps.
	//! Returns patches changed.
	static int VoxCellsChanged(int cx0, int cx1, int cz0, int cz1)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return 0;
		return s_Inst.DoVoxCells(cx0, cx1, cz0, cz1);
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
					if (!p.m_bQueued)
					{
						p.m_bQueued = true;
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
					BuildCollider(p);
					p.m_fColAt = m_fTime;
					now++;
				}
				else if (p.m_iBuild == BUILD_COVER)
					p.m_iBuild = BUILD_COLLIDER;    // its collider has the old cells: build it again before the cover
				if (m_bRender && p.m_iBuild == 0 && p.m_Ent)
				{
					// mesh now, in the frame the bricks show, asleep ones too: the queue waits (one per frame, Mesh
					// Interval) and skips sleeping patches, so the old surface (ruts) lay over the crater
					// (user 2026-10-13 20:24)
					BuildMesh(p);
					p.m_fMeshAt = m_fTime;
					meshes++;
				}
				else if (m_bRender)
				{
					p.m_bMeshDirty = true;
				}
				if (!p.m_bQueued)
				{
					p.m_bQueued = true;
					m_aDirty.Insert(p.m_iKey);
				}
			}
		}
		if (changed > 0)
			Print(string.Format("MUD world: voxel cells live - %1 patches took the voxel cells (%2 colliders rebuilt now, %3 dropped: all voxel, %4 meshes now) in %5 ms", changed, now, dropped, meshes, System.GetTickCount(t0)), LogLevel.NORMAL);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe overlay, any machine): undug ground height at world (x, z) - the skin
	//! triangles, the heightmap on kept terrain. The same on every machine (no ruts). False = unknown.
	static bool UndugY(float x, float z, out float y)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		float dug;
		return s_Inst.Heights(x, z, dug, y);
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe bricks, server): ground at world (x, z) now (ruts, berms, heightfield
	//! craters) and undug. The server seeds new voxel columns with dug - undug. False = unknown.
	static bool DugUndug(float x, float z, out float dug, out float undug)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		return s_Inst.Heights(x, z, dug, undug);
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
		int t0 = System.GetTickCount();
		m_Skin.SetCovered(p.m_iPx * SKIN, p.m_iPz * SKIN, SKIN, true);
		m_iSwapMs += System.GetTickCount(t0);
		p.m_iBuild = 0;
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
	//! Uncover the skin cells (a patch still building never covered them), then delete the patch
	protected void FreePatch(MUD_FinePatch p)
	{
		if (p.m_iBuild == 0)
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
		p.m_Idx = null;     // TEMP probe 2026-10-13: the ring cache (or a rebuild) serves it when it wakes
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
	protected void Ahead(int t0)
	{
		array<int> candKey = {};
		array<float> candDist = {};
		float size = PCELLS * m_fFine;
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh)
				continue;
			Physics ph = veh.GetPhysics();
			if (!ph)
				continue;
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
		m_aPlayerIds.Clear();
		pm.GetPlayers(m_aPlayerIds);
		foreach (int id : m_aPlayerIds)
		{
			IEntity ent = pm.GetPlayerControlledEntity(id);
			if (!ent)
				continue;
			vector o = ent.GetOrigin();
			int pxa = Math.Floor((o[0] - m_fX0 - m_fPlayerRadius) / size);
			int pxb = Math.Floor((o[0] - m_fX0 + m_fPlayerRadius) / size);
			int pza = Math.Floor((o[2] - m_fZ0 - m_fPlayerRadius) / size);
			int pzb = Math.Floor((o[2] - m_fZ0 + m_fPlayerRadius) / size);
			for (int pz = Math.Max(pza, 0); pz <= Math.Min(pzb, m_iNPZ - 1); pz++)
			{
				for (int px = Math.Max(pxa, 0); px <= Math.Min(pxb, m_iNPX - 1); px++)
				{
					MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
					if (!p)
						continue;
					float x0 = m_fX0 + px * size;
					float z0 = m_fZ0 + pz * size;
					float dx = Math.Max(Math.Max(x0 - o[0], 0), o[0] - x0 - size);
					float dz = Math.Max(Math.Max(z0 - o[2], 0), o[2] - z0 - size);
					float d = Math.Sqrt(dx * dx + dz * dz);
					if (d > m_fPlayerRadius)
						continue;
					p.m_fNearAt = m_fTime;
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
				if (!p.m_bDeformed && !p.m_bFloor && m_fTime - p.m_fNearAt > m_fFreeAfter)
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
			if (q && !q.m_bDeformed && !q.m_bFloor && m_fTime - q.m_fNearAt > m_fFreeAfter)
			{
				FreePatch(q);
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Deformed, live, no rebuild pending, not building, no vehicle near for m_fSleepAfter
	protected bool MaySleep(MUD_FinePatch p)
	{
		return (p.m_bDeformed || p.m_bFloor) && !p.m_bAsleep && p.m_iBuild == 0 && p.m_Phys && !p.m_bQueued
			&& p.m_iKey != m_iBuildKey && m_fTime - p.m_fNearAt > m_fSleepAfter;
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
	//! The patches there are made / woken as a wheel press does (server too: it needs the collider), then rebuild
	//! mesh + collider. The floor is never written into m_aH and never queued for the net (the op stream carries
	//! it; m_aH would go to every client and again in the patch JIP). Returns the patches marked.
	static int FloorDirty(int gx0, int gx1, int gz0, int gz1)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return 0;
		s_Inst.EnsureRange(gx0, gx1, gz0, gz1, true);
		return s_Inst.MarkFloorDirty(gx0, gx1, gz0, gz1);
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: MarkDirty without the blur reach (the floor is not blurred) and without the net rect
	protected int MarkFloorDirty(int gx0, int gx1, int gz0, int gz1)
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
				if (m_bRender)
					p.m_bMeshDirty = true;
				p.m_bColDirty = true;
				if (!p.m_bQueued)
				{
					p.m_bQueued = true;
					m_aDirty.Insert(p.m_iKey);
				}
				marked++;
			}
		}
		return marked;
	}

	//------------------------------------------------------------------------------------------------
	//! RUT FIELD: p's mesh / collider read the rut floor (a floor tile reaches it; also keeps it from being freed)
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

	//------------------------------------------------------------------------------------------------
	//! Flag every patch holding a changed vertex (mesh + collider) and every patch the blur reaches
	//! from it (collider) - as MUD_GroundEntity.c:1892 MarkDirty, global fine vertex coords.
	//! Task 9 adds the net rect here.
	protected void MarkDirty(int gx0, int gx1, int gz0, int gz1)
	{
		int r = m_aSmoothW.Count() / 2;
		bool net = IsNet();
		for (int pz = PatchLo(gz0 - r, m_iNPZ); pz <= PatchHi(gz1 + r, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0 - r, m_iNPX); px <= PatchHi(gx1 + r, m_iNPX); px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				int ox = px * PCELLS;
				int oz = pz * PCELLS;
				bool own = gx1 >= ox && gx0 <= ox + PCELLS && gz1 >= oz && gz0 <= oz + PCELLS;
				if (own && m_bRender)
					p.m_bMeshDirty = true;
				p.m_bColDirty = true;
				if (!p.m_bQueued)
				{
					p.m_bQueued = true;
					m_aDirty.Insert(p.m_iKey);
				}

				int sx0 = Math.ClampInt(gx0 - r - ox, 0, PCELLS);
				int sx1 = Math.ClampInt(gx1 + r - ox, 0, PCELLS);
				int sz0 = Math.ClampInt(gz0 - r - oz, 0, PCELLS);
				int sz1 = Math.ClampInt(gz1 + r - oz, 0, PCELLS);
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
				if (!p.m_bNetQueued)
				{
					p.m_bNetQueued = true;
					m_aNetKeys.Insert(p.m_iKey);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: global fine vertex box changed by a crater / trench / shovel -> AI navmesh rebuild later
	//! (MUD_NavRefresh). Y range: undug ground at the middle +-8 m (stacked craters go 2.5 m down).
	protected void NavNote(int gx0, int gx1, int gz0, int gz1)
	{
		if (!m_bNavRefresh || IsProxy())
			return;
		float x0 = m_fX0 + gx0 * m_fFine - m_fNavMargin;
		float x1 = m_fX0 + gx1 * m_fFine + m_fNavMargin;
		float z0 = m_fZ0 + gz0 * m_fFine - m_fNavMargin;
		float z1 = m_fZ0 + gz1 * m_fFine + m_fNavMargin;
		float dug;
		float undug;
		if (!Heights((x0 + x1) * 0.5, (z0 + z1) * 0.5, dug, undug))
			return;
		m_Nav.Note(Vector(x0, undug - 8, z0), Vector(x1, undug + 8, z1), m_fTime, m_fNavMaxSide);
	}

	//------------------------------------------------------------------------------------------------
	//! At most one mesh and one collider per frame, the longest-waiting first (MUD_GroundEntity.c:2220),
	//! each only while the frame is under budget. A patch still building waits in the queue: its last
	//! build steps use the newest heights.
	protected void Rebuild(int t0)
	{
		MUD_FinePatch mp;
		MUD_FinePatch cp;
		for (int i = m_aDirty.Count() - 1; i >= 0; i--)
		{
			MUD_FinePatch p = m_mPatches.Get(m_aDirty[i]);
			if (!p || (!p.m_bMeshDirty && !p.m_bColDirty))
			{
				if (p)
					p.m_bQueued = false;
				m_aDirty.Remove(i);
				continue;
			}
			if (p.m_iBuild > 0 || p.m_bAsleep)
				continue;   // building: its last steps use the newest heights; asleep: its wake does
			if (m_bRender && p.m_bMeshDirty && m_fTime - p.m_fMeshAt >= m_fMeshInterval && (!mp || p.m_fMeshAt < mp.m_fMeshAt))
				mp = p;
			if (p.m_bColDirty && m_fTime - p.m_fColAt >= m_fColliderInterval && (!cp || p.m_fColAt < cp.m_fColAt))
				cp = p;
		}
		// an index-list cache miss (~2.5 ms) is this frame's whole rebuild job; the build runs next frame
		// on the cached lists. Layouts vary a lot (misses 120 vs 85 new patches per 10 s, 14:28 Exp 192624).
		bool idxJob = false;
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
					IdxSet(mp);
					idxJob = true;
					m_sFrameRebuild += string.Format("index %1 ", System.GetTickCount(tm));
				}
				else
				{
					BuildMesh(mp);
					mp.m_fMeshAt = m_fTime;
					m_sFrameRebuild += string.Format("mesh %1 ", System.GetTickCount(tm));
				}
			}
			else
			{
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
					m_sFrameRebuild += string.Format("index %1 ", System.GetTickCount(tc));
				}
				else
				{
					BeforeCollider(cp);
					SmoothPatch(cp);
					int ts = System.GetTickCount(tc);
					BuildCollider(cp);
					cp.m_fColAt = m_fTime;
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
			return p.m_aLock[vz * PVERTS + vx];
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
			float rf = MUD_VoxelProbe.FieldFloorAt(x, z, dug);
			if (rf < undug)
			{
				float rfL = LockAt(ix, iz) * (1 - u) * (1 - w) + LockAt(ix + 1, iz) * u * (1 - w) + LockAt(ix, iz + 1) * (1 - u) * w + LockAt(ix + 1, iz + 1) * u * w;
				rf = undug + (rf - undug) * rfL;     // edge lock, as the patch mesh / collider
			}
			if (rf < dug)
				dug = rf;
		}
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
	//! hidden and the voxel floor can be FLOOR under the undug ground, so the tolerance reaches that (2026-10-13).
	static bool AtGround(vector wp)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		float dug;
		float undug;
		if (!s_Inst.Heights(wp[0], wp[2], dug, undug))
			return false;
		float tol = Math.Max(1.5, MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER + 0.25);
		// TEMP probe 2026-10-13 (bricks): voxel ground can be dug FLOOR deep (craters, tunnels) under the undug ground
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
					tol = Math.Max(tol, MUD_VoxelProbe.FLOOR + 0.25);
			}
		}
		return Math.AbsFloat(wp[1] - dug) <= tol || Math.AbsFloat(wp[1] - undug) <= tol;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP probe 2026-10-12 (MUD_VoxelProbe): sink every fine vertex of the rect x0..x1, z0..z1 to depth under
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
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe): make / finish / wake at most max patches of the rect x0..x1, z0..z1, so
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
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe): heightfield patch side (m) and the patch grid corner. 0 = not ready.
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
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe): ProbeSink, then the mesh + collider of every patch the rect covers past
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
			// TEMP probe 2026-10-13: voxel bricks; REFUSED = not voxels (cap, seed size), the heightfield digs it
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
		float run = depth * MUD_TrenchPlan.SLOPE_RUN;
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
					want = depth - d / MUD_TrenchPlan.SLOPE_RUN;
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
			NavNote(gx0, gx1, gz0, gz1);
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
			// TEMP probe 2026-10-13: voxel bricks; REFUSED = not voxels (cap, seed size), the heightfield digs it
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
			// TEMP probe 2026-10-13: voxel bricks; REFUSED = not voxels (cap, seed size), the heightfield fills it
			int vr = MUD_VoxelProbe.FreeFill(lay);
			if (vr != MUD_VoxelProbe.REFUSED)
				return vr;
		}
		return s_Inst.DoFreeFill(lay);
	}

	//------------------------------------------------------------------------------------------------
	protected int DoFreeFill(MUD_TrenchLayout lay)
	{
		float margin = MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.LAYER * MUD_TrenchPlan.SLOPE_RUN;
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
					float wall = Math.Max(0, holeDepth - d / MUD_TrenchPlan.SLOPE_RUN);
					if (-h / lock > wall + 0.01)
						y = -(newDepth + d / MUD_TrenchPlan.SLOPE_RUN) * lock;
					else
						y = -Math.Max(0, newDepth - d / MUD_TrenchPlan.SLOPE_RUN) * lock;
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
			NavNote(gx0, gx1, gz0, gz1);
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
			float want = depth - lay.OutsideDist(i, x, z) / MUD_TrenchPlan.SLOPE_RUN;
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
			if (!TrenchRange(lay, piece, MUD_TrenchPlan.LAYER * MUD_TrenchPlan.MAX_LAYERS * MUD_TrenchPlan.SLOPE_RUN, gx0, gx1, gz0, gz1))
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
		Blast(null, warhead.GetOrigin(), warhead, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void Blast(IEntity pHitEntity, vector pos, IEntity damageSource, bool warhead)
	{
		// clients get craters from the server
		if (!m_bReady || IsProxy())
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
		if (kg <= m_fCraterMinTnt)
		{
			m_iCraterSkips++;
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (charge %1 kg <= min %2) at %3", kg, m_fCraterMinTnt, outMat[0]));
			return;
		}
		int t0 = System.GetTickCount();
		Crater(outMat[0], kg, heat);
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
		// TEMP probe 2026-10-13 (MUD_VoxelProbe overlay): a voxel tile carries the ground here, our hidden heightfield
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
			if (name.Contains("penetrator"))
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
			return;
		// centre over kept terrain (road): no crater (the rim would cut a step into the field beside it)
		int ccx = Math.Floor(fx / FINE);
		int ccz = Math.Floor(fz / FINE);
		if (!m_Skin.IsHoleCell(ccx, ccz))
		{
			m_iCraterSkips++;
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (kept cell %1 %2) at %3", ccx, ccz, wp));
			return;
		}
		// TEMP probe 2026-10-13: in a voxel area our heightfield is sunk 4 m - the height checks below would call it an air burst
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
			MUD_VoxelProbe.Blast(wp, FullDepth(tnt, heat));
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
				return;
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
		if (MUD_VoxelProbe.Blast(wp, full * scale))    // TEMP probe 2026-10-13: voxel craters (SP)
			return;

		// variation (server only; clients get the heights): deeper or shallower, also past the cap
		float depth = full * scale * (1 + m_fCraterDepthJitter * Math.RandomFloat(-1, 1));
		float rim = depth * m_fCraterRimShare;
		float rimWidth = Math.Max(2 * m_fFine, depth * m_fCraterRimWidthShare);
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
		bool big = tnt >= m_fCraterBigTnt;
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
		// neighbours' edges would sink next to the skin (a crack)
		if (!EnsureRange(gx0, gx1, gz0, gz1))
		{
			m_iCraterSkips++;
			if (m_bDebug)
				Print(string.Format("MUD world: crater skipped (range not ready) at %1", wp));
			return;
		}

		cp = At(cgx, cgz, cv);
		if (!cp)
			return;
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
				else
					y = rim * (1 + Math.Cos(Math.PI * (d - crest) / (outer - crest))) * 0.5;
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
		MarkDirty(gx0, gx1, gz0, gz1);
		NavNote(gx0, gx1, gz0, gz1);
		m_iCraters++;
		if (m_bDebug)
			Print(string.Format("MUD world: crater at %1, TNT %2 kg, HEAT %9, scale %3, depth %4 m, crest %5 m, outer %6 m (+ stretch), floor at centre %7 m, centre now %8 m",
				wp, tnt, scale, depth, crest, outer, cp.m_aFloor[cv], cp.m_aH[cv], heat));
	}

	//------------------------------------------------------------------------------------------------
	protected void SmoothPatch(MUD_FinePatch p)
	{
		if (p.m_iSx0 < 0)
			return;
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
				float sum2 = 0;
				for (int kz = -r; kz <= r; kz++)
					sum2 += m_Buf.m_aBlurRows[(z2 - z0 + r + kz) * cols + x2 - x0] * m_aSmoothW[kz + r];
				p.m_aSmooth[v] = sum2 * inv;
			}
		}
		m_iSmoothMs += System.GetTickCount(t0);
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
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				float d = p.m_aH[v];
				if (m_bAheadRuts && m_fTime - p.m_aHideAt[v] < HIDE_HOLD)
				{
					if (Math.AbsFloat(d - p.m_aShow[v]) > 0.0005)
						hideOwed = true;
					d = p.m_aShow[v];
				}
				else
				{
					p.m_aShow[v] = d;
				}
				float rfD = d;     // groove reveal input: a field rut shows soil only from FIELD_SOIL_D deep (as v097)
				if (rfOn)
				{
					float rfF = MUD_VoxelProbe.FieldFloor(rfX0 + vx, rfZ0 + vz) - p.m_aBase[v];
					if (rfF < 0)
						rfF = rfF * p.m_aLock[v];     // edge lock as a press: 0 at kept terrain (road) -> no gap there
					if (rfF < d)
					{
						d = rfF;
						float rfS = rfF + FIELD_SOIL_D - m_fRevealDepth;
						if (rfS < rfD)
							rfD = rfS;
					}
				}
				float h = p.m_aBase[v] + d;
				// groove reveal (MUD_GroundEntity.c:1440-1448)
				if (m_bGroove)
				{
					m_Buf.m_aUnder[v] = Vector(vx * m_fFine, h - m_fRevealGap, vz * m_fFine);
					float t = Math.Clamp((-rfD - m_fRevealDepth) / Math.Max(m_fRevealBlend, 0.01), 0, 1);
					h -= t * t * (3 - 2 * t) * m_fRevealGap * 3;
				}
				m_Buf.m_aVerts[v] = Vector(vx * m_fFine, h, vz * m_fFine);
				m_Buf.m_aUVs[v * 2] = (ux + vx) / uvDiv;
				m_Buf.m_aUVs[v * 2 + 1] = 1 - (uz + vz) / uvDiv;
			}
		}

		Resource res;
		if (m_bGroove)
		{
			int numVerts2[] = {1089, 1089};
			int numIndices2[] = {6144, 6144};
			string materials2[] = {m_Material, m_RutMaterial};
			res = MeshObject.Create(2, numVerts2, numIndices2, materials2, 0);
			if (!res || !res.GetResource())
			{
				Print(string.Format("MUD world: groove material '%1' did not load - groove layer off", m_RutMaterial), LogLevel.ERROR);
				m_bGroove = false;
				p.m_bMeshDirty = true;
				return;
			}
		}
		else
		{
			int numVerts[] = {1089};
			int numIndices[] = {6144};
			string materials[] = {m_Material};
			res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
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
		p.m_Ent.SetObject(mesh, "");
		p.m_Ent.Update();       // fresh bounds, else culling keeps the spawn-time ones
		p.m_MeshRes = res;
		p.m_bMeshDirty = hideOwed;     // ahead ruts: rebuild again (Mesh Interval) until every hidden change shows
		m_iMeshBuilds++;
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
		if (!p.m_MeshRes)
			Print(string.Format("MUD world: bounds mesh failed - material '%1'", m_Material), LogLevel.ERROR);
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
						rfF = p.m_aBase[v] + (rfF - p.m_aBase[v]) * p.m_aLock[v];     // edge lock, as the mesh
					if (rfF < y)
						y = rfF;
				}
				if (hold && y != p.m_aColH[v] && m_fTime - Math.Max(p.m_aRutTouch[v], p.m_aBermTouch[v]) < m_fWheelHold)
				{
					y = p.m_aColH[v];
					held++;
				}
				if (hadBody && Math.AbsFloat(y - p.m_aColH[v]) > 0.001)
					moved = true;
				p.m_aColH[v] = y;
				m_Buf.m_aVerts[v] = Vector(vx * m_fFine, y, vz * m_fFine);
				m_Buf.m_aPlate[v] = Vector(vx * m_fFine, p.m_aBase[v] + p.m_aFloor[v] - m_fPlateGap, vz * m_fFine);
				m_Buf.m_aShell[v] = m_Buf.m_aVerts[v] - vector.Up * m_fBulletSlab;
			}
		}

		array<PhysicsGeom> geoms = {};
		array<ResourceName> mats = {};
		array<int> layers = {};
		// Terrain preset + Foliage: vehicle get in/out sweep skips Foliage (MUD_GroundEntity.c:1668-1676)
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;
		for (int k = 0; k < p.m_aMats.Count() && k < ix.m_aSlot.Count(); k++)
		{
			MUD_IdxList sl = ix.m_aSlot[k];
			if (sl.m_iN == 0)
				continue;
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
			PhysicsGeom plate = PhysicsGeom.CreateTriMesh(m_Buf.m_aPlate, ix.m_Plate.m_aIdx, 1089, ix.m_Plate.m_iN);
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
	//! Layered-SDF test (Band Mesher on): BuildMesh from the band surface-nets mesher. Same display rule
	//! (ahead ruts), groove reveal per vertex from the interpolated deformation, skirts in submesh 0 (a separate
	//! skirt submesh crashed WB, mud-gotchas 2026-10-10). False = overflow or no triangle: BuildMesh builds the grid.
	protected bool BuildMeshBand(MUD_FinePatch p)
	{
		int t0 = System.GetTickCount();
		if (!m_Band)
			m_Band = new MUD_BandMesher();
		bool hideOwed = false;
		int csIn = System.GetTickCount();
		// RUT FIELD: the rut floor goes into the mesher INPUTS (d = min(m_aH, floor - base)), never into m_aH;
		// the groove reveal then works for field ruts too. m_aShow keeps the m_aH value (no floor): ahead-rut hold.
		bool rfOn = RutFieldOn(p);
		int rfX0 = p.m_iPx * PCELLS;
		int rfZ0 = p.m_iPz * PCELLS;
		for (int v = 0; v < PVERTS * PVERTS; v++)
		{
			float d = p.m_aH[v];
			if (m_bAheadRuts && m_fTime - p.m_aHideAt[v] < HIDE_HOLD)
			{
				if (Math.AbsFloat(d - p.m_aShow[v]) > 0.0005)
					hideOwed = true;
				d = p.m_aShow[v];
			}
			p.m_aShow[v] = d;      // = BuildMesh: a hidden vertex keeps its m_aShow, the rest show m_aH
			float rfD = d;         // groove reveal input (m_aD): a field rut shows soil only from FIELD_SOIL_D deep
			if (rfOn)
			{
				int rfVx = v % PVERTS;
				int rfVz = v / PVERTS;
				float rfF = MUD_VoxelProbe.FieldFloor(rfX0 + rfVx, rfZ0 + rfVz) - p.m_aBase[v];
				if (rfF < 0)
					rfF = rfF * p.m_aLock[v];     // edge lock as a press: 0 at kept terrain (road) -> no gap there
				if (rfF < d)
				{
					d = rfF;
					float rfS = rfF + FIELD_SOIL_D - m_fRevealDepth;
					if (rfS < rfD)
						rfD = rfS;
				}
			}
			m_Band.m_aD[v] = rfD;
			m_Band.m_aY[v] = p.m_aBase[v] + d;
		}
		FillBandOn(p);
		m_iCsMeshIn += System.GetTickCount(csIn);
		int tc = System.GetTickCount();
		bool ok = m_Band.Build(m_fFine, m_fBandSkirt);
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
				float t = (-m_Band.m_Info.m_aD[i] - m_fRevealDepth) * invBlend;
				if (t > 0)
				{
					if (t > 1)
						t = 1;
					m_Band.m_Vb.m_aV[i] = Vector(q[0], q[1] - t * t * (3 - 2 * t) * sink, q[2]);
				}
			}
			m_Band.m_Uv.m_aTop[i2] = (ux + qx) * invDiv;
			m_Band.m_Uv.m_aTop[i2 + 1] = 1 - (uz + qz) * invDiv;
		}

		m_iCsMeshUv += System.GetTickCount(csUv);
		int csRes = System.GetTickCount();
		int ni = m_Band.m_iI;
		int niAll = ni + m_Band.m_iSkirtI;
		Resource res;
		if (m_bGroove)
		{
			int numVerts2[] = {nAll, nv};
			int numIndices2[] = {niAll, ni};
			string materials2[] = {m_Material, m_RutMaterial};
			res = MeshObject.Create(2, numVerts2, numIndices2, materials2, 0);
			if (!res || !res.GetResource())
			{
				Print(string.Format("MUD world: groove material '%1' did not load - groove layer off", m_RutMaterial), LogLevel.ERROR);
				m_bGroove = false;
				p.m_bMeshDirty = true;
				return true;
			}
		}
		else
		{
			int numVerts[] = {nAll};
			int numIndices[] = {niAll};
			string materials[] = {m_Material};
			res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
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
		if (m_bGroove)
		{
			mesh.UpdateIndices(1, m_Band.m_Ib.m_aI);       // its first ni: the surface without skirts
			mesh.UpdateVerts(1, m_Band.m_Aux.m_aV, m_Band.m_Uv.m_aUnder);
		}
		p.m_Ent.SetObject(mesh, "");
		p.m_Ent.Update();       // fresh bounds, else culling keeps the spawn-time ones
		m_iCsMeshRes += System.GetTickCount(csRes);
		p.m_MeshRes = res;
		p.m_bMeshDirty = hideOwed;     // ahead ruts: rebuild again (Mesh Interval) until every hidden change shows
		m_iMeshBuilds++;
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
						rfF = p.m_aBase[v] + (rfF - p.m_aBase[v]) * p.m_aLock[v];     // edge lock, as the mesh
					if (rfF < y)
						y = rfF;
				}
				if (hold && y != p.m_aColH[v] && m_fTime - Math.Max(p.m_aRutTouch[v], p.m_aBermTouch[v]) < m_fWheelHold)
				{
					y = p.m_aColH[v];
					held++;
				}
				if (hadBody && Math.AbsFloat(y - p.m_aColH[v]) > 0.001)
					moved = true;
				m_Band.m_aY[v] = y;
				m_Band.m_aD[v] = 0;
				m_Buf.m_aPlate[v] = Vector(vx * m_fFine, p.m_aBase[v] + p.m_aFloor[v] - m_fPlateGap, vz * m_fFine);
			}
		}
		FillBandOn(p);
		m_iCsColIn += System.GetTickCount(csIn);
		int tc = System.GetTickCount();
		bool ok = m_Band.Build(m_fFine, 0);
		m_iBandCoreMs += System.GetTickCount(tc);
		m_iCsColCore += System.GetTickCount(tc);
		if (!ok)
		{
			if (m_Band.m_bOverflow)
				m_iBandFallback++;     // else: no on cell (all kept terrain) - the grid collapses it the same way
			return false;
		}
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
		for (int k = 0; k < p.m_aMats.Count(); k++)
		{
			int n = 0;
			for (int ti = 0; ti < ni; ti += 3)
			{
				if (m_Band.m_Slot.m_aS[ti / 3] != k)
					continue;
				m_Band.m_Scratch.m_aI[n] = m_Band.m_Ib.m_aI[ti];
				m_Band.m_Scratch.m_aI[n + 1] = m_Band.m_Ib.m_aI[ti + 1];
				m_Band.m_Scratch.m_aI[n + 2] = m_Band.m_Ib.m_aI[ti + 2];
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
			PhysicsGeom plate = PhysicsGeom.CreateTriMesh(m_Buf.m_aPlate, ix.m_Plate.m_aIdx, 1089, ix.m_Plate.m_iN);
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
		if (builds + m_iGridMeshN + m_iGridColN + m_iBandFallback > 0)
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
			Print(string.Format("MUD band: collider split ms - input %1, mesher %2, trimeshes %3 (%4 geoms), drop %5, static %6, rest %7 (of %8)",
				m_iCsColIn, m_iCsColCore, m_iCsColGeom, m_iCsColGeoms, m_iCsColDrop, m_iCsColStatic,
				m_iBandColMs - m_iCsColIn - m_iCsColCore - m_iCsColGeom - m_iCsColDrop - m_iCsColStatic, m_iBandColMs));
			Print(string.Format("MUD band: grid path - mesh %1 builds, mean %2 ms, worst %3 | collider %4, mean %5 ms, worst %6 | band fast verts mean %7, fast quads mean %8",
				m_iGridMeshN, gMeshMean, m_iGridMeshWorst, m_iGridColN, gColMean, m_iGridColWorst, m_iBandFastV / b, m_iBandFastQ / b));
		}
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
				sig += ToMm(p.m_aH[sv]) * (scol + 1) + ToMm(p.m_aShow[sv]);
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
							val = p.m_aShow[v];
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
	//! Called by MUD_GroundEntity.HideFromTraces (vehicle get in / out).
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
			if (!m_aSendKnown[i] || PatchDist(p.m_iPx, p.m_iPz, m_aSendPos[i]) <= m_fSendRadius)
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
	//! TEMP probe 2026-10-13 (MUD_VoxelProbe ground test, MP): server, one voxel op (text) to every client
	static void VoxelBroadcast(string text)
	{
		if (s_Inst && !s_Inst.IsProxy())
			s_Inst.SendVoxelOp(text);
	}

	//------------------------------------------------------------------------------------------------
	protected void SendVoxelOp(string text)
	{
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
			ApplyRect(msg.m_iKey, msg.m_iRect, msg.m_aPacked);
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
		if (FarFromCamera(px, pz))
			p = HeightsOnly(px, pz);
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
		MarkDirty(gx0 + x0, gx0 + x1, gz0 + z0, gz0 + z1);
		m_iNetGot++;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: patch (px, pz) is farther than m_fClientBuildRadius from the camera
	protected bool FarFromCamera(int px, int pz)
	{
		if (m_fClientBuildRadius <= 0 || !IsProxy())
			return false;
		vector cam[4];
		GetWorld().GetCurrentCamera(cam);
		return PatchDist(px, pz, cam[3]) > m_fClientBuildRadius;
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
	protected MUD_FinePatch HeightsOnly(int px, int pz)
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
				if (m_fClientBuildRadius <= 0 || fd <= m_fClientBuildRadius)
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
	//! Every 10 s while something happened
	protected void Stats(float timeSlice)
	{
		m_fSinceStats += timeSlice;
		if (m_fSinceStats < 10)
			return;
		m_fSinceStats = 0;
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
			m_iFrameBudget, m_iWaitAhead, m_iWaitMesh, m_iWaitCol, m_iWaitFree, m_iRushed, m_iWheelCapped));
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
		Print(string.Format("MUD world wheel hold (%1 s): verts kept under a wheel %2 in %3 collider builds", m_fWheelHold, m_iHeldVerts, m_iHeldBuilds));
		m_iHeldVerts = 0;
		m_iHeldBuilds = 0;
		int live = m_mPatches.Count() - m_iAsleep;
		int slept = Math.ClampInt(m_iSlept, 1, 1000000);
		int woken = Math.ClampInt(m_iWoken, 1, 1000000);
		Print(string.Format("MUD world: sleep - live %1, asleep %2 | slept %3 (gave back %4 KB, %5 each) | woke %6 (%7 at once by a wheel / stamp; took %8 KB, %9 each)",
			live, m_iAsleep, m_iSlept, m_iSleepKB, m_iSleepKB / slept, m_iWoken, m_iWokeNow, m_iWakeKB, m_iWakeKB / woken));
		Print(string.Format("MUD world: sleep waits %1", m_iWaitSleep));
		m_iSlept = 0;
		m_iWoken = 0;
		m_iWokeNow = 0;
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
		SeamCheck();
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
