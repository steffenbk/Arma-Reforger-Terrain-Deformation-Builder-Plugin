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
	static const int PVERTS = 33;
	static const int SKIN = 4;          // skin cells per patch side
	static const int FINE = 8;          // fine cells per skin cell side
	static const float RESCUE_DEPTH = 0.7;  // feet this far under our ground (and collider): lift back on top. Prone sat up
	                                        // to ~0.3 m under while fine (fall probe 2026-10-10); a real fall keeps going
	static const int MAT_SLOTS = 4;    // surface pieces per collider; + bullet slab + safety plate = 6 (MakeStatic max)
	static const int TLBR = 16;         // MUD_FinePatch.m_aCell bit: the skin cell's shared edge is tl-br
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

	[Attribute("0.06", UIWidgets.EditBox, "Deeper than this (m) shows the groove material", category: "Ground")]
	protected float m_fRevealDepth;

	[Attribute("0.1", UIWidgets.EditBox, "Depth band (m) over which the surface sinks under the groove layer", category: "Ground")]
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

	[Attribute("3", UIWidgets.EditBox, "TEST (user 2026-10-11, set 1 for release): Rut Step and the berm step x this, so ruts reach full depth in fewer passes", category: "Ruts")]
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

	[Attribute("0.15", UIWidgets.EditBox, "Wheel hold (P1a): a vertex a wheel touched within this many seconds keeps its old collider height; the new height goes in at the first collider rebuild after the wheel left. The ground never moves under a wheel, on any machine (MP stutter). 0 = off.", category: "Ruts")]
	protected float m_fWheelHold;

	[Attribute("0.6", UIWidgets.EditBox, "A vertex must be free of wheels this long (s) before the next touch counts as a new pass", category: "Ruts")]
	protected float m_fPassGap;

	[Attribute("0.5", UIWidgets.EditBox, "A wheel contact more than this (m) above or below our ground is on something else (bridge, rubble, a truck bed): no rut", category: "Ruts")]
	protected float m_fPressHeight;

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

	[Attribute("1", UIWidgets.CheckBox, "Print stats every 10 s and check every new patch against the skin by trace", category: "Debug")]
	protected bool m_bDebug;

	[Attribute("0", UIWidgets.EditBox, "Print height hashes of all deformed patches every this many seconds (0 = off). Compare server and client logs with Tools/compare_hashes.py.", category: "Debug")]
	protected float m_fHashEvery;

	[Attribute("8", UIWidgets.EditBox, "JIP: full patch copies sent per frame (all joining players together)", category: "Patches")]
	protected int m_iJipPerFrame;

	[Attribute("500", UIWidgets.EditBox, "Send by distance (F4): live ground changes go only to players within this radius (m) of the patch; a farther player gets the patch's full heights once they come within it (checked each second, sent through the JIP queue). 0 = every change to every client.", category: "Patches")]
	protected float m_fSendRadius;

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
		MUD_JoltProbe.Tick(timeSlice, IsProxy(), this);    // TEMP probe 2026-10-11
		Wheels(timeSlice);
		int tW = System.GetTickCount(t0);
		// after the wheels every job checks the frame budget (Fix B: one new patch + a rebuild in one
		// frame was 10-18 ms, measured 2026-10-09 14:12 Exp 192624)
		Ahead(t0);
		int tA = System.GetTickCount(t0);
		Rebuild(t0);
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
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP probe 2026-10-11 (MUD_JoltProbe). False = not our ground (kept terrain: road, water edge).
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
					float cap = depth * soft;
					if (!newPass || h <= -cap)
						continue;
					float k = d / m_fRutRadius;
					float fall = 1 - k * k * 0.5;
					// raised loose soil (crater rim, berm, spoil) gets plowed: most of it goes in one pass
					float top = h;
					if (h > 0)
						top = h - h * m_fRimCut * fall;
					float y = Math.Max(top - m_fRutStep * m_fRutSpeed * fall, Math.Max(-cap, p.m_aFloor[v]));
					if (y < h)
					{
						SetH(gx, gz, y);
						changed = true;
					}
				}
				else if (d < m_fBermRadius)
				{
					bool newBerm = m_fTime - p.m_aBermTouch[v] > m_fPassGap;
					SetTouch(gx, gz, true, m_fTime);
					// ruts win over berms: never push up a vertex that is already below the surface
					float bermTop = m_fBermHeight * soft;
					if (!newBerm || h < -0.001 || h >= bermTop)
						continue;
					float t = 1 - (d - m_fRutRadius) / bermSpan;
					float y2 = Math.Min(h + m_fRutStep * m_fRutSpeed * 0.5 * t, bermTop);
					if (y2 > h)
					{
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
				continue;
			}
			// heavier vehicle, deeper rut (MUD_GroundEntity.c:2192-2197)
			float mass = phys.GetMass();
			float depth = Math.Min(m_fRutBaseDepth + mass * 0.001 * m_fRutDepthPerTon, m_fRutMaxDepth);
			int count = sim.WheelCount();
			for (int w = 0; w < count; w++)
			{
				if (!sim.WheelHasContact(w))
					continue;
				vector cp = sim.WheelGetContactPosition(w);
				Press(cp, depth);
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
		return m_bLazy && p.m_iBuild == BUILD_COLLIDER && !p.m_bAsleep && !p.m_bDeformed;
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
			if (!p.m_Phys)
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
				if (!p.m_bDeformed && m_fTime - p.m_fNearAt > m_fFreeAfter)
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
			if (q && !q.m_bDeformed && m_fTime - q.m_fNearAt > m_fFreeAfter)
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
		return p.m_bDeformed && !p.m_bAsleep && p.m_iBuild == 0 && p.m_Phys && !p.m_bQueued
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
	//! 1.5 m), not on a roof or bridge above it
	static bool AtGround(vector wp)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		float dug;
		float undug;
		if (!s_Inst.Heights(wp[0], wp[2], dug, undug))
			return false;
		return Math.AbsFloat(wp[1] - dug) <= 1.5 || Math.AbsFloat(wp[1] - undug) <= 1.5;
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
			MarkDirty(gx0, gx1, gz0, gz1);
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
	//! and repeated swings deepen it evenly. Max MAX_LAYERS (1.5 m).
	//! Returns verts moved, -1 = patch missing / building, -2 = nothing diggable, -3 = already full depth
	static int FreeDig(MUD_TrenchLayout lay)
	{
		if (!s_Inst || !s_Inst.m_bReady || s_Inst.IsProxy() || !lay || lay.Count() < 1)
			return -1;
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
			MarkDirty(gx0, gx1, gz0, gz1);
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
			if (!MUD_FinePatchEntity.Cast(e) && !MUD_CoarseChunkEntity.Cast(e))
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
	//! m_aShellIdx (reversed), m_aCellSlot (-1 = kept terrain) and m_aMeshIdx (kept cells collapsed)
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
				if (cell < 0)
				{
					m_Buf.m_aCellSlot[fc] = -1;
					for (int j = 0; j < 6; j++)
						m_Buf.m_aMeshIdx[n + j] = bl;
				}
				else
				{
					m_Buf.m_aCellSlot[fc] = cell & SLOT;
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
		MUD_IdxSet found = FindIdx(p);
		if (found)
		{
			m_iIdxHits++;
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
		return ix;
	}

	//------------------------------------------------------------------------------------------------
	//! Static mesh (flags 0), a fresh Create on every change (MUD_GroundEntity.c:1560). Surface UVs are
	//! the skin chunk's (0..1 per 64 m), so the terrain-look material lines up with the skin around it.
	protected void BuildMesh(MUD_FinePatch p)
	{
		int t0 = System.GetTickCount();
		MUD_IdxSet ix = IdxSet(p);
		int chunkPatches = MUD_CoarseSkinEntity.CELLS / SKIN;     // 8 patches per skin chunk side
		int ux = (p.m_iPx - (p.m_iPx / chunkPatches) * chunkPatches) * PCELLS;
		int uz = (p.m_iPz - (p.m_iPz / chunkPatches) * chunkPatches) * PCELLS;
		float uvDiv = chunkPatches * PCELLS;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				float d = p.m_aH[v];
				float h = p.m_aBase[v] + d;
				// groove reveal (MUD_GroundEntity.c:1440-1448)
				if (m_bGroove)
				{
					m_Buf.m_aUnder[v] = Vector(vx * m_fFine, h - m_fRevealGap, vz * m_fFine);
					float t = Math.Clamp((-d - m_fRevealDepth) / Math.Max(m_fRevealBlend, 0.01), 0, 1);
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
		p.m_bMeshDirty = false;
		m_iMeshBuilds++;
		m_iMeshMs += System.GetTickCount(t0);
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
		int t0 = System.GetTickCount();
		MUD_IdxSet ix = IdxSet(p);
		// wheel hold (P1a): a vert under a wheel keeps the height the live body has
		bool hold = m_fWheelHold > 0 && p.m_bColValid && p.m_Phys != null;
		bool hadBody = p.m_bColValid && p.m_Phys != null;   // TEMP probe 2026-10-11
		bool moved;
		int held;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				float y = p.m_aBase[v] + p.m_aSmooth[v];
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
		m_iColMs += System.GetTickCount(t0);
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
			if (p.m_aCell[(fz / FINE) * SKIN + fx / FINE] < 0)
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
