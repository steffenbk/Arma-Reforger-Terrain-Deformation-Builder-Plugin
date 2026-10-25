//! Client look ruts (user 2026-10-17: ruts close to the wheel and at once, at least on the client; the server keeps its
//! batched RUT_BEHIND cut, op 4). Every RENDERING machine (clients, WB host - not a DS) cuts its own rut a short GAP
//! behind the rearmost wheel of each nearby moving vehicle into a LOCAL look floor. Patch MESHES draw
//! min(rut floor, look floor) (MeshFloor); colliders, hashes, ops, JIP and the net never see it - no physics change,
//! so no snaps. A look sample drops once the real floor reaches it (no rebuild) or after TTL (rebuild).
//! Cut rule = CarveFloor (step, RUT_SHARE, cap, fall 1 - k * k, whole cm), no relax / ridges. Plain RUT FIELD ground
//! only (no voxel column there: bricks draw their own floor). checkpoint v172 = before.
class MUD_LookTrack
{
	Vehicle m_Veh;
	ref array<vector> m_aLast = {};     // per wheel: last point a segment ended at
	ref array<int> m_aHas = {};
	ref array<vector> m_aPendA = {};    // segments waiting for the rearmost wheel to be GAP past them
	ref array<vector> m_aPendB = {};
	ref array<float> m_aPendT = {};     // TEMP delay meter: time the wheel reached the segment's end
	ref array<int> m_aPendW = {};       // PER_WHEEL: the wheel that drove each waiting segment
	ref array<vector> m_aCp = {};       // PER_WHEEL: per wheel, its contact point the last frame it touched
	int m_iId;                          // look samples it wrote carry this (no stacking by its own aligned wheels)
	vector m_vTail;
	vector m_vTailDir;
	float m_fCutAt;
	vector m_vPrev;                     // origin last frame: speed from the move (a proxy's physics velocity may read 0)
	float m_fPrevAt = -1;
	vector m_vVel;
}

//! 4 x 4 m of look floor (16 x 16 samples, the rut tile grid)
class MUD_LookTile
{
	float m_aY[256];
	float m_aAt[256];
	int m_aOwn[256];    // MUD_LookTrack.m_iId of the vehicle that wrote the sample
	int m_iN;           // live samples

	//------------------------------------------------------------------------------------------------
	void MUD_LookTile()
	{
		for (int i = 0; i < 256; i++)
			m_aY[i] = MUD_VoxelProbe.FLOOR_NONE;
	}
}

class MUD_LookRuts
{
	static const bool ENABLED = true;       // false = v172 exactly
	static const bool SHEET_SKIP = true;    // R4 perf 2026-10-18: no look cut where the mud sheet lies (its wheel stamps show the rut;
	                                        // research/2026-10-18-mud-zone-rut-perf.md). Under the sheet = MUD_MudZoneEntity.SheetCovers
	// H2 (plans/old/2026-10-18-hybrid-sheet-feel-plan.md): under the mud sheet the look cut goes on, but only the SHEET
	// draws it (MUD_MudSheet.LookChanged, SheetFloorAt): the rut shows at once behind the wheel and the server's cut
	// (same rule, crude under the sheet) lands at the same depth = no visible drop. Patch meshes there are hidden
	// (lazy rut mesh) so they are not marked. false = SHEET_SKIP as before (no look cut under the sheet).
	static const bool SHEET_LOOK = true;
	// stage 3 B8 (mud lab fix set 19, sheet local): no look cut for a segment whose two lead-shifted ends lie where sheet
	// local stamps the wheel itself (MUD_MudSheet.LocalStampsAt) - its band hides the look rut there and the sheet never
	// reads the look floor in local. Look only: server ops, ruts, wear and colliders are untouched.
	static bool LOOK_UNDER_SKIP = false;
	static const float SHEET_GAP = 0.1;     // m behind the rearmost wheel contact, segments under the sheet
	static const float GAP = 0.2;           // 0.4 until 2026-10-18 (user: ruts not instant). m behind the wheel contact a segment is cut (server: RUT_BEHIND_GAP 1.5)
	// PER_WHEEL (user 2026-10-18, results/2026-10-18-look-rut-delay-split.md: the rearmost-wheel wait put the look rut
	// ~7 m behind the front wheel): every wheel's segment is cut once THAT wheel is GAP past it, so the front wheel cuts
	// at once. A wheel running in a rut its own vehicle cut within TTL (aligned rear wheel) adds no extra step: the sample
	// keeps the deeper of the two cuts from the uncut ground. Off the rut (turning, other track width) it cuts as usual.
	// Visual only: rear wheels ride the server collider, so they may look a few cm high over the front's look rut until
	// the server rut arrives. false = wait for the rearmost wheel (v210).
	static const bool PER_WHEEL = true;
	// LEAD (user 2026-10-18: dirt pops in behind the tyre - 'if it happened under the wheels we could mask it'): each
	// wheel's segment is laid LEAD m AHEAD of its contact along the travel line and cut at once (no GAP wait), so by the
	// time the mesh is drawn (cut every EVERY + build ~0.1 s + LOOK_MESH_EVERY cap) the tyre stands on it and hides the
	// pop. LEAD = LEAD_MIN + speed * LEAD_S, at most LEAD_MAX. Trade-offs: a hard stop leaves up to LEAD of dirt ahead of
	// the tyre; over 1-2 cm the tyre may look high over the drawn rut until the server rut lands. false = GAP rule (v214).
	static const bool LEAD = true;
	static const float LEAD_MIN = 0.1;
	static const float LEAD_S = 0.4;         // 0.2 until 2026-10-18 (WB 21:59 meter: wheel -> drawn avg 0.2 s, max 0.4 s)
	static const float LEAD_MAX = 2;          // 1.5 until 2026-10-18
	static const float STEP = 0.25;         // segment length (server RUT_STEP 0.5)
	static const float JUMP = 2;            // farther than this since the last point (air, teleport): track restarts
	static const float EVERY = 0.05;        // 0.1 until 2026-10-18 (user: ruts not instant). s between look cuts per vehicle; one cut per sample per batch (as one op)
	static const float RANGE = 150;         // m from the camera
	static const float TTL = 8;             // s a look sample lives without the real floor reaching it
	static const float CATCH = 0.02;        // real floor this close or deeper = caught up: drop, no rebuild
	// MP 2026-10-17 (client log 23:41-23:46): 15-30 % of look samples ran to TTL - the server's repose relax lifts the
	// rut bottom, so its floor stays shallower than the plain look cut. A real floor that is there but shallower and
	// older than REPLACE s: drop the look sample and rebuild (the mesh shows the server's rut, a small step up)
	static const float REPLACE = 3;
	// L3 look rut perf (DS 2026-10-18, research/2026-10-18-sheet-alternatives-and-perf.md sec 9: 24-534 replaced samples
	// per 10 s, each a tile-wide mesh rebuild): a real floor at most this much shallower than the look sample counts as
	// caught up - dropped with no rebuild (the mesh keeps the look floor, <= 3 cm deeper, until the patch builds again)
	static const float REPLACE_MIN = 0.03;
	static const float SWEEP = 0.5;         // s between expiry passes
	static const int MAX_PEND = 200;        // waiting segments per vehicle; past this the oldest go
	static const int TS = 16;               // = MUD_VoxelProbe.FSUB
	static const int TKEY = 8192;           // tile key = bk * TKEY + bi (+ TKEY / 2 offset each)
	static const int KOFF = 1024;           // Cut's local sample keys: +-256 m round its first sample

	protected static ref map<int, ref MUD_LookTile> s_mTiles = new map<int, ref MUD_LookTile>();
	protected static ref array<MUD_LookTile> s_aPTiles = {};   // perf plan B2: PatchBegin's tiles (row-major, may hold null)
	protected static int s_iPBi0;     // perf plan B2: first tile column / row, tiles per row
	protected static int s_iPBk0;
	protected static int s_iPNx;
	protected static ref array<ref MUD_LookTrack> s_aTracks = {};
	protected static int s_iNextId;
	protected static float s_fSweepAt;
	protected static float s_fLogAt;
	// 10 s log
	protected static int s_iVeh;            // vehicle-frames with >= 1 wheel contact
	protected static int s_iNoContact;      // moving vehicle-frames in range with no contact (proxy without wheel sim?)
	protected static int s_iCuts;
	protected static int s_iWritten;
	protected static ref array<int> s_aLead = {};     // MUD_VoxelProbe.LeadAxle: per wheel, 1 = leading axle (cuts)
	protected static int s_iAligned;        // PER_WHEEL: samples an aligned wheel of the same vehicle left as they were
	protected static int s_iWorn;           // WEAR COUNT: samples skipped because the server only counts wear there
	protected static int s_iCaught;
	protected static int s_iExpired;
	protected static int s_iReplaced;
	protected static int s_iNearCaught;     // L3: real floor within REPLACE_MIN after REPLACE s - dropped, no rebuild
	protected static int s_iDirty;
	protected static int s_iMs;
	protected static int s_iMeshes;         // patch mesh builds a look cut asked for, and their ms
	protected static int s_iMeshMs;
	protected static int s_iSheetCut;       // H2: look samples drawn by the sheet / given back to it (10 s log)
	protected static int s_iSheetBack;
	protected static int s_iUnder;          // R4: segments not cut because the mud sheet lies there
	protected static int s_iB8Under;        // stage 3 Part 0 B8 meter: segments sheet local stamps itself (both ends)
	// TEMP delay meter (2026-10-18, user: ruts show a little after the wheel): s, per 10 s
	protected static float s_fDlCutSum;     // wheel at a segment's end -> its look cut
	protected static float s_fDlCutMax;
	protected static int s_iDlCutN;
	protected static float s_fDlMeshSum;    // wheel at the oldest segment -> its patch mesh drawn
	protected static float s_fDlMeshMax;
	protected static float s_fDlDistSum;    // that delay x the vehicle speed (m behind the wheel)
	// TEMP split delay meter (2026-10-18, user: ruts at the wheel). Mark = the patch's state when a look cut first marks
	// it after its last mesh build (MISSING: no fine patch there = the look floor is not drawn at all, every mark).
	// Wait = per frame, per look-dirty patch, why its mesh is not built this frame (patch-frames). Built = first mark ->
	// mesh built (queue = rebuild queue; other = a patch's own build steps / finish).
	static const int M_READY = 0;
	static const int M_BUILDING = 1;
	static const int M_ASLEEP = 2;
	static const int M_MISSING = 3;
	static const int W_BUILDING = 0;     // patch still in its build steps
	static const int W_ASLEEP = 1;
	static const int W_SHEET = 2;        // mud sheet holds the mesh
	static const int W_EVERY = 3;        // its last mesh is younger than LOOK_MESH_EVERY / Mesh Interval
	static const int W_LOST = 4;         // due, but another patch got this frame's one mesh
	static const int W_BUDGET = 5;       // picked, but the frame budget said no
	static const int W_INDEX = 6;        // picked, but this frame went to its index-list job
	protected static ref array<int> s_aMark = {0, 0, 0, 0};
	protected static ref array<int> s_aWait = {0, 0, 0, 0, 0, 0, 0};
	protected static float s_fBuiltQSum;     // first mark -> mesh built, queue builds
	protected static float s_fBuiltQMax;
	protected static int s_iBuiltQN;
	protected static float s_fBuiltOSum;     // same, other builds
	protected static int s_iBuiltON;
	protected static int s_iDlMeshN;

	//------------------------------------------------------------------------------------------------
	//! MUD_WorldGroundEntity frame, rendering machines only. step / cap rule as its Wheels (same prefab values).
	static void Tick(float now, float step, float baseDepth, float perTon, float maxDepth)
	{
		// decal / road hide / road look / grass-cut probes and track stamps: archived 2026-10-21 (Github/Terrain
		// deformation/Archived code/2026-10-21-cleanup)
		MUD_RutDebug.Tick();          // diag menu Mud > Ruts and wheels
		if (!ENABLED || !MUD_VoxelProbe.RUTS || !MUD_TestKit.LookRuts())     // no client look ruts over heightfield ruts (feel test 2026-10-19)
			return;
		int t0 = System.GetTickCount();
		float x0;
		float z0;
		if (!MUD_VoxelProbe.LookOrigin(x0, z0))
			return;
		vector cm[4];
		GetGame().GetWorld().GetCurrentCamera(cm);
		vector cam = cm[3];
		float range2 = RANGE * RANGE;
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh || vector.DistanceSqXZ(veh.GetOrigin(), cam) > range2)
				continue;
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!sim || !sim.IsValid())
				continue;
			MUD_LookTrack t = Track(veh);
			vector org = veh.GetOrigin();
			float dt = now - t.m_fPrevAt;
			if (t.m_fPrevAt >= 0 && dt > 0.001)
				t.m_vVel = (org - t.m_vPrev) * (1 / dt);
			t.m_vPrev = org;
			t.m_fPrevAt = now;
			vector velFlat = Vector(t.m_vVel[0], 0, t.m_vVel[2]);
			float hs = velFlat.Length();
			if (hs < 0.2)
				continue;
			vector dir = velFlat * (1 / hs);
			float mass = 0;
			Physics phys = veh.GetPhysics();
			if (phys)
				mass = phys.GetMass();
			float cap = Math.Min(baseDepth + mass * 0.001 * perTon, maxDepth);
			float alMin = 1000;
			int count = sim.WheelCount();
			while (t.m_aLast.Count() < count)
			{
				t.m_aLast.Insert(org);
				t.m_aHas.Insert(0);
				t.m_aCp.Insert(org);
			}
			MUD_VoxelProbe.LeadAxle(veh, sim, t.m_vVel, s_aLead, true);     // RUT_LEAD_AXLE: as the server's feel ruts
			for (int w = 0; w < count; w++)
			{
				if (!sim.WheelHasContact(w))
					continue;
				vector cp = MUD_VoxelProbe.RutPoint(veh, sim, w);     // RUT AT THE HUB, as the server
				t.m_aCp.Set(w, cp);
				float al = vector.Dot(cp - org, dir);
				if (al < alMin)
				{
					alMin = al;
					t.m_vTail = cp;
				}
				if (s_aLead[w] == 0)
				{
					t.m_aHas.Set(w, 0);     // trailing wheel: no look cut; restarts here when it leads
					continue;
				}
				if (t.m_aHas[w] == 0)
				{
					t.m_aLast.Set(w, cp);
					t.m_aHas.Set(w, 1);
					continue;
				}
				vector last = t.m_aLast[w];
				float dd = vector.DistanceSqXZ(cp, last);
				if (dd > JUMP * JUMP)
				{
					t.m_aLast.Set(w, cp);
					continue;
				}
				if (dd < STEP * STEP)
					continue;
				bool underSheet = MUD_MudZoneEntity.SheetCovers(cp[0], cp[2]);
				if (underSheet)
					s_iUnder++;
				if (SHEET_SKIP && !SHEET_LOOK && underSheet)
				{
					t.m_aLast.Set(w, cp);
					continue;
				}
				vector lead = vector.Zero;
				if (LEAD)
					lead = dir * Math.Min(LEAD_MIN + hs * LEAD_S, LEAD_MAX);
				if ((LOOK_UNDER_SKIP || MUD_TestKit.TEST_KIT) && MUD_MudSheet.LocalStampsAt(last + lead) && MUD_MudSheet.LocalStampsAt(cp + lead))
				{
					s_iB8Under++;     // stage 3 Part 0 B8 meter
					if (LOOK_UNDER_SKIP)
					{
						t.m_aLast.Set(w, cp);
						continue;     // stage 3 B8: the sheet stamps this wheel itself here
					}
				}
				t.m_aPendA.Insert(last + lead);
				t.m_aPendB.Insert(cp + lead);
				t.m_aPendT.Insert(now);
				t.m_aPendW.Insert(w);
				t.m_aLast.Set(w, cp);
			}
			if (alMin > 999)
			{
				s_iNoContact++;
				continue;
			}
			s_iVeh++;
			t.m_vTailDir = dir;
			while (t.m_aPendA.Count() > MAX_PEND)
			{
				t.m_aPendA.Remove(0);
				t.m_aPendB.Remove(0);
				if (!t.m_aPendT.IsEmpty())
					t.m_aPendT.Remove(0);
				if (!t.m_aPendW.IsEmpty())
					t.m_aPendW.Remove(0);
			}
			if (now >= t.m_fCutAt)
			{
				Cut(t, now, x0, z0, step, cap, hs);
				t.m_fCutAt = now + EVERY;
			}
		}
		if (now >= s_fSweepAt)
		{
			Sweep(now);
			s_fSweepAt = now + SWEEP;
		}
		s_iMs += System.GetTickCount(t0);
		if (now >= s_fLogAt)
		{
			bool meters = MUD_TestKit.TEST_KIT;     // audit C1: the 10 s meter lines are test-only
			if (meters && (s_iVeh > 0 || s_iNoContact > 0 || !s_mTiles.IsEmpty()))
				Print(string.Format("MUD look ruts: 10 s - vehicle-frames %1 (no contact %2), cuts %3, samples written %4, caught up %5, expired %6, dirty marks %7, tiles %8, %9 ms",
					s_iVeh, s_iNoContact, s_iCuts, s_iWritten, s_iCaught, s_iExpired, s_iDirty, s_mTiles.Count(), s_iMs)
					+ string.Format(" | look meshes %1 (%2 ms), replaced by a shallower server rut %3 (+ %5 within REPLACE_MIN, no rebuild), segments under the sheet %4 (SHEET_LOOK: cut for the sheet, else skipped)", s_iMeshes, s_iMeshMs, s_iReplaced, s_iUnder, s_iNearCaught)
					+ string.Format(" | sheet look samples %1, given back %2 | PER_WHEEL %3, aligned (no extra step) %4 | worn only (WEAR COUNT, no look cut) %5", s_iSheetCut, s_iSheetBack, PER_WHEEL, s_iAligned, s_iWorn)
					+ string.Format(" | B8 meter: segments sheet local stamps itself %1 (LOOK_UNDER_SKIP %2: skipped when on)", s_iB8Under, LOOK_UNDER_SKIP), LogLevel.NORMAL);
			float dlDist = s_fDlDistSum / Math.Max(1, s_iDlMeshN);
			if (meters && (s_iDlCutN > 0 || s_iDlMeshN > 0))
				Print(string.Format("MUD delay look 10 s (TEMP): wheel -> look cut avg %1 max %2 ms (%3 segments) | wheel -> drawn mesh avg %4 max %5 ms, ~%6 m behind the wheel (%7 meshes) | GAP %8 m, LOOK_MESH_EVERY %9 s",
				Math.Round(1000 * s_fDlCutSum / Math.Max(1, s_iDlCutN)), Math.Round(1000 * s_fDlCutMax), s_iDlCutN, Math.Round(1000 * s_fDlMeshSum / Math.Max(1, s_iDlMeshN)), Math.Round(1000 * s_fDlMeshMax),
				dlDist.ToString(-1, 2), s_iDlMeshN, GAP, MUD_WorldGroundEntity.LOOK_MESH_EVERY), LogLevel.NORMAL);
			if (meters && (s_iDlCutN > 0 || s_iBuiltQN + s_iBuiltON > 0 || s_aMark[3] > 0))
				Print(string.Format("MUD delay split 10 s (TEMP): mark ready %1 building %2 asleep %3 MISSING %4 | wait patch-frames building %5 asleep %6 sheet %7 every %8 lost %9",
				s_aMark[0], s_aMark[1], s_aMark[2], s_aMark[3], s_aWait[0], s_aWait[1], s_aWait[2], s_aWait[3], s_aWait[4])
				+ string.Format(" budget %1 index %2 | mark -> built: queue avg %3 max %4 ms (%5), other avg %6 ms (%7) | look build avg %8 ms",
				s_aWait[5], s_aWait[6], Math.Round(1000 * s_fBuiltQSum / Math.Max(1, s_iBuiltQN)), Math.Round(1000 * s_fBuiltQMax), s_iBuiltQN,
				Math.Round(1000 * s_fBuiltOSum / Math.Max(1, s_iBuiltON)), s_iBuiltON, Math.Round(s_iMeshMs / Math.Max(1, s_iMeshes))), LogLevel.NORMAL);
			for (int mi = 0; mi < s_aMark.Count(); mi++)
				s_aMark[mi] = 0;
			for (int wi = 0; wi < s_aWait.Count(); wi++)
				s_aWait[wi] = 0;
			s_fBuiltQSum = 0;
			s_fBuiltQMax = 0;
			s_iBuiltQN = 0;
			s_fBuiltOSum = 0;
			s_iBuiltON = 0;
			s_fDlCutSum = 0;
			s_fDlCutMax = 0;
			s_iDlCutN = 0;
			s_fDlMeshSum = 0;
			s_fDlMeshMax = 0;
			s_fDlDistSum = 0;
			s_iDlMeshN = 0;
			s_iUnder = 0;
			s_iB8Under = 0;
			s_iSheetCut = 0;
			s_iSheetBack = 0;
			s_iVeh = 0;
			s_iNoContact = 0;
			s_iCuts = 0;
			s_iWritten = 0;
			s_iAligned = 0;
			s_iWorn = 0;
			s_iCaught = 0;
			s_iExpired = 0;
			s_iReplaced = 0;
			s_iNearCaught = 0;
			s_iDirty = 0;
			s_iMs = 0;
			s_iMeshes = 0;
			s_iMeshMs = 0;
			s_fLogAt = now + 10;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static ref map<int, float> s_mCutBest = new map<int, float>();
	protected static ref map<int, float> s_mCutAtY = new map<int, float>();

	//! Segments the rearmost wheel is GAP past: one look cut (CarveFloor's rule, strongest fall per sample)
	protected static void Cut(MUD_LookTrack t, float now, float x0, float z0, float step, float cap, float speed)
	{
		array<float> keepT = {};     // TEMP delay meter
		array<int> keepW = {};
		float oldest = -1;
		float r = MUD_VoxelProbe.RUT_R;
		float fc = MUD_VoxelProbe.FCELL;
		map<int, float> best = s_mCutBest;     // audit B8: reused (cleared), the keep arrays go into the track
		map<int, float> atY = s_mCutAtY;
		best.Clear();
		atY.Clear();
		array<vector> keepA = {};
		array<vector> keepB = {};
		int used = 0;
		int i0 = 0;     // cut-local keys (as CarveFloor's RUT_KEY_OFF): global ones pass 2^31 on a big map
		int k0 = 0;
		bool hasK0 = false;
		for (int i = 0; i < t.m_aPendA.Count(); i++)
		{
			vector a = t.m_aPendA[i];
			vector b = t.m_aPendB[i];
			vector ref0 = t.m_vTail;
			int segW = -1;
			if (i < t.m_aPendW.Count())
				segW = t.m_aPendW[i];
			if (PER_WHEEL && segW >= 0 && segW < t.m_aCp.Count())
				ref0 = t.m_aCp[segW];     // this segment's own wheel, not the rearmost
			float back = -Math.Max(vector.Dot(a - ref0, t.m_vTailDir), vector.Dot(b - ref0, t.m_vTailDir));
			float segT = now;
			if (i < t.m_aPendT.Count())
				segT = t.m_aPendT[i];
			float gap = GAP;
			if (SHEET_LOOK && MUD_MudZoneEntity.SheetCovers(b[0], b[2]))
				gap = SHEET_GAP;
			if (LEAD)
				gap = -LEAD_MAX - 1;     // laid ahead: cut at once
			if (back <= gap)
			{
				keepA.Insert(a);
				keepB.Insert(b);
				keepT.Insert(segT);
				keepW.Insert(segW);
				continue;
			}
			used++;
			MUD_RutDebug.Seg(a, b, true);     // diag menu Mud > Ruts and wheels
			// TEMP delay meter: wheel at the segment's end -> this cut
			float lag = now - segT;
			s_fDlCutSum += lag;
			s_iDlCutN++;
			if (lag > s_fDlCutMax)
				s_fDlCutMax = lag;
			if (oldest < 0 || segT < oldest)
				oldest = segT;
			float abx = b[0] - a[0];
			float abz = b[2] - a[2];
			float len2 = abx * abx + abz * abz;
			if (len2 < 0.000001)
				continue;
			int fi0 = Math.Floor((Math.Min(a[0], b[0]) - r - x0) / fc);
			int fi1 = Math.Ceil((Math.Max(a[0], b[0]) + r - x0) / fc);
			int fk0 = Math.Floor((Math.Min(a[2], b[2]) - r - z0) / fc);
			int fk1 = Math.Ceil((Math.Max(a[2], b[2]) + r - z0) / fc);
			for (int fk = fk0; fk <= fk1; fk++)
			{
				for (int fi = fi0; fi <= fi1; fi++)
				{
					float px = x0 + fi * fc;
					float pz = z0 + fk * fc;
					float u = ((px - a[0]) * abx + (pz - a[2]) * abz) / len2;
					if (u < 0 || u >= 1)
						continue;
					float qx = a[0] + abx * u - px;
					float qz = a[2] + abz * u - pz;
					float dist = Math.Sqrt(qx * qx + qz * qz);
					if (dist >= r)
						continue;
					float k = dist / r;
					float fall = 1 - k * k;
					if (!hasK0)
					{
						i0 = fi;
						k0 = fk;
						hasK0 = true;
					}
					int li = fi - i0 + KOFF;
					int lk = fk - k0 + KOFF;
					if (li < 0 || li >= KOFF * 2 || lk < 0 || lk >= KOFF * 2)
						continue;
					int key = lk * KOFF * 2 + li;
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
		t.m_aPendA = keepA;
		t.m_aPendB = keepB;
		t.m_aPendT = keepT;
		t.m_aPendW = keepW;
		if (used == 0)
			return;
		s_iCuts++;
		int gx0 = 1000000;
		int gx1 = -1000000;
		int gz0 = 1000000;
		int gz1 = -1000000;
		array<float> shX = {};
		array<float> shZ = {};
		foreach (int ck, float cf : best)
		{
			int sfi = ck % (KOFF * 2) - KOFF + i0;
			int sfk = ck / (KOFF * 2) - KOFF + k0;
			float wheelY = atY.Get(ck);
			float fieldY;
			float undug;
			if (!MUD_VoxelProbe.LookBase(sfi, sfk, wheelY, fieldY, undug))
				continue;
			float surf = fieldY;
			float real = MUD_VoxelProbe.FieldFloor(sfi, sfk);
			if (real < surf)
				surf = real;
			if (MUD_VoxelProbe.WearOnly(sfi, sfk))
			{
				s_iWorn++;     // WEAR COUNT: the server only counts this pass here - no look cut either
				continue;
			}
			MUD_LookTile tile = s_mTiles.Get(TileKey(FloorDiv(sfi, TS), FloorDiv(sfk, TS)));
			int n = (sfk - FloorDiv(sfk, TS) * TS) * TS + sfi - FloorDiv(sfi, TS) * TS;
			// own sample (this vehicle, younger than TTL) = an aligned wheel in its own fresh rut: measure from the uncut
			// ground and keep the deeper cut, no extra step (PER_WHEEL)
			bool mine = PER_WHEEL && tile && tile.m_aY[n] < MUD_VoxelProbe.FLOOR_NONE * 0.5 && tile.m_aOwn[n] == t.m_iId && now - tile.m_aAt[n] < TTL;
			if (tile && tile.m_aY[n] < surf && !mine)
				surf = tile.m_aY[n];
			if (Math.AbsFloat(surf - wheelY) > MUD_VoxelProbe.RUT_ON)
				continue;
			float capY = undug - cap * cf;
			float cut = step * cf;
			float left = MUD_VoxelProbe.RUT_SHARE * (surf - capY);
			if (left > cut)
				cut = left;
			if (MUD_VoxelProbe.RUT_FIRST_D > 0 && undug - surf < MUD_VoxelProbe.RUT_FIRST_D * cf - MUD_VoxelProbe.RUT_FIRST_TOL)
				cut = MUD_VoxelProbe.RUT_FIRST_D * cf - (undug - surf);     // first pass reaches the dirt (a top-up: no RUT_RAMP here)
			else if (MUD_VoxelProbe.RUT_PASS_MAX > 0 && cut > MUD_VoxelProbe.RUT_PASS_MAX)
				cut = MUD_VoxelProbe.RUT_PASS_MAX;     // as CarveFloor: later passes deepen slowly
			float floorY = surf - cut;
			if (floorY < capY)
				floorY = capY;
			float deepY = undug - MUD_VoxelProbe.FLOOR;
			if (floorY < deepY)
				floorY = deepY;
			floorY = Math.Round(floorY * 100) / 100.0;
			if (floorY >= surf)
				continue;
			if (mine && floorY >= tile.m_aY[n])
			{
				s_iAligned++;
				continue;
			}
			if (!tile)
			{
				tile = new MUD_LookTile();
				s_mTiles.Set(TileKey(FloorDiv(sfi, TS), FloorDiv(sfk, TS)), tile);
			}
			if (tile.m_aY[n] > MUD_VoxelProbe.FLOOR_NONE * 0.5)
				tile.m_iN++;
			tile.m_aY[n] = floorY;
			tile.m_aAt[n] = now;
			tile.m_aOwn[n] = t.m_iId;
			s_iWritten++;
			float swx = x0 + sfi * fc;
			float swz = z0 + sfk * fc;
			if (SHEET_LOOK && MUD_MudZoneEntity.SheetCovers(swx, swz))
			{
				shX.Insert(swx);     // H2: the sheet draws it; the patch mesh there is hidden
				shZ.Insert(swz);
				continue;
			}
			if (sfi < gx0)
				gx0 = sfi;
			if (sfi > gx1)
				gx1 = sfi;
			if (sfk < gz0)
				gz0 = sfk;
			if (sfk > gz1)
				gz1 = sfk;
		}
		if (gx0 <= gx1)
			s_iDirty += MUD_WorldGroundEntity.LookDirty(gx0, gx1, gz0, gz1, oldest, speed);
		if (!shX.IsEmpty())
		{
			s_iSheetCut += shX.Count();
			MUD_MudSheet.LookChanged(shX, shZ);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drop look samples the real floor reached (mesh already right: no rebuild) or older than TTL (rebuild)
	protected static void Sweep(float now)
	{
		array<int> gone = {};
		float ox = 0;
		float oz = 0;
		bool hasO = MUD_VoxelProbe.LookOrigin(ox, oz);
		array<float> shX = {};
		array<float> shZ = {};
		foreach (int tk, MUD_LookTile tile : s_mTiles)
		{
			int bi = tk % TKEY - TKEY / 2;
			int bk = tk / TKEY - TKEY / 2;
			bool expired = false;
			bool caughtUp = false;
			for (int n = 0; n < 256; n++)
			{
				float y = tile.m_aY[n];
				if (y > MUD_VoxelProbe.FLOOR_NONE * 0.5)
					continue;
				int fi = bi * TS + n % TS;
				int fk = bk * TS + n / TS;
				float real = MUD_VoxelProbe.FieldFloor(fi, fk);
				if (real <= y + CATCH)
				{
					s_iCaught++;
					if (real > y)
						caughtUp = true;     // REBUILD 5: the drawn floor moves up to CATCH with no rebuild
				}
				else if (real <= y + REPLACE_MIN && now - tile.m_aAt[n] > REPLACE)
				{
					s_iNearCaught++;
					caughtUp = true;     // REBUILD 5: dropped with no rebuild too
				}
				else if (real < MUD_VoxelProbe.FLOOR_NONE * 0.5 && now - tile.m_aAt[n] > REPLACE)
				{
					s_iReplaced++;
					SweepRedraw(fi, fk, hasO, ox, oz, shX, shZ, expired);
				}
				else if (now - tile.m_aAt[n] > TTL)
				{
					s_iExpired++;
					SweepRedraw(fi, fk, hasO, ox, oz, shX, shZ, expired);
				}
				else
				{
					continue;
				}
				tile.m_aY[n] = MUD_VoxelProbe.FLOOR_NONE;
				tile.m_iN--;
			}
			if (expired)
				s_iDirty += MUD_WorldGroundEntity.LookDirty(bi * TS, bi * TS + TS - 1, bk * TS, bk * TS + TS - 1);
			else if (caughtUp)
				MUD_WorldGroundEntity.LookBox(bi * TS, bi * TS + TS - 1, bk * TS, bk * TS + TS - 1);     // partial pass reads it
			if (tile.m_iN <= 0)
				gone.Insert(tk);
		}
		foreach (int g : gone)
			s_mTiles.Remove(g);
		if (!shX.IsEmpty())
		{
			s_iSheetBack += shX.Count();
			MUD_MudSheet.LookChanged(shX, shZ);     // after the drop: the sheet reads the real floor again
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sweep: a dropped look sample that was drawn deeper than the real floor - the sheet re-reads it (H2), else the
	//! patch mesh rebuilds (expired)
	protected static void SweepRedraw(int fi, int fk, bool hasO, float ox, float oz, array<float> shX, array<float> shZ, inout bool expired)
	{
		if (SHEET_LOOK && hasO)
		{
			float wx = ox + fi * MUD_VoxelProbe.FCELL;
			float wz = oz + fk * MUD_VoxelProbe.FCELL;
			if (MUD_MudZoneEntity.SheetCovers(wx, wz))
			{
				shX.Insert(wx);
				shZ.Insert(wz);
				return;
			}
		}
		expired = true;
	}

	//------------------------------------------------------------------------------------------------
	//! H2 (MUD_MudSheet.HoleGround): look floor at world x, z, bilinear over the floor samples; a corner with no look
	//! sample counts as its real rut floor, else ownY. No look sample at any corner = FLOOR_NONE (no change).
	static float SheetFloorAt(float x, float z, float ownY)
	{
		if (!ENABLED || !SHEET_LOOK || s_mTiles.IsEmpty())
			return MUD_VoxelProbe.FLOOR_NONE;
		float ox;
		float oz;
		if (!MUD_VoxelProbe.LookOrigin(ox, oz))
			return MUD_VoxelProbe.FLOOR_NONE;
		float fx = (x - ox) / MUD_VoxelProbe.FCELL;
		float fz = (z - oz) / MUD_VoxelProbe.FCELL;
		int ix = Math.Floor(fx);
		int iz = Math.Floor(fz);
		float tx = fx - ix;
		float tz = fz - iz;
		bool any = false;
		float f00 = SheetCorner(ix, iz, ownY, any);
		float f10 = SheetCorner(ix + 1, iz, ownY, any);
		float f01 = SheetCorner(ix, iz + 1, ownY, any);
		float f11 = SheetCorner(ix + 1, iz + 1, ownY, any);
		if (!any)
			return MUD_VoxelProbe.FLOOR_NONE;
		return (f00 * (1 - tx) + f10 * tx) * (1 - tz) + (f01 * (1 - tx) + f11 * tx) * tz;
	}

	protected static float SheetCorner(int fi, int fk, float ownY, inout bool any)
	{
		float real = MUD_VoxelProbe.FieldFloor(fi, fk);
		float isNone = MUD_VoxelProbe.FLOOR_NONE * 0.5;
		float y = ownY;
		if (real < isNone)
			y = real;
		MUD_LookTile tile = s_mTiles.Get(TileKey(FloorDiv(fi, TS), FloorDiv(fk, TS)));
		if (!tile)
			return y;
		float ly = tile.m_aY[(fk - FloorDiv(fk, TS) * TS) * TS + fi - FloorDiv(fi, TS) * TS];
		if (ly > isNone)
			return y;
		any = true;
		if (ly < y)
			return ly;
		return y;
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_WorldGroundEntity.Rebuild: a look-dirty patch mesh was built in ms
	static void NoteMesh(int ms)
	{
		s_iMeshes++;
		s_iMeshMs += ms;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP split delay meter (MUD_WorldGroundEntity.MarkLookDirty): a patch's state at its first look mark
	static void NoteMark(int state)
	{
		s_aMark[state] = s_aMark[state] + 1;
	}

	//! TEMP split delay meter (MUD_WorldGroundEntity.Rebuild): n look patches waited this frame for reason r
	static void NoteWait(int r, int n = 1)
	{
		if (n > 0)
			s_aWait[r] = s_aWait[r] + n;
	}

	//! TEMP split delay meter (MUD_WorldGroundEntity mesh builders): a look-marked patch mesh built wait s after its
	//! first mark; queue = from the rebuild queue
	static void NoteBuilt(float wait, bool queue)
	{
		if (queue)
		{
			s_fBuiltQSum += wait;
			s_iBuiltQN++;
			if (wait > s_fBuiltQMax)
				s_fBuiltQMax = wait;
			return;
		}
		s_fBuiltOSum += wait;
		s_iBuiltON++;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP delay meter: a look-dirty patch mesh was drawn delay s after its oldest segment's wheel passed
	static void NoteDelay(float delay, float speed)
	{
		s_fDlMeshSum += delay;
		s_iDlMeshN++;
		if (delay > s_fDlMeshMax)
			s_fDlMeshMax = delay;
		s_fDlDistSum += delay * speed;
	}

	//------------------------------------------------------------------------------------------------
	//! Patch mesh floor at global fine vertex gx, gz: min(real rut floor, look floor). FLOOR_NONE = none
	static float MeshFloor(int gx, int gz)
	{
		float real = MUD_VoxelProbe.FieldFloor(gx, gz);
		if (!ENABLED || s_mTiles.IsEmpty())
			return real;
		MUD_LookTile tile = s_mTiles.Get(TileKey(FloorDiv(gx, TS), FloorDiv(gz, TS)));
		if (!tile)
			return real;
		float y = tile.m_aY[(gz - FloorDiv(gz, TS) * TS) * TS + gx - FloorDiv(gx, TS) * TS];
		if (y < real)
			return y;
		return real;
	}

	//------------------------------------------------------------------------------------------------
	//! perf plan B2 (2026-10-18): MeshFloor for one patch build without a map Get per vertex - caches the look tiles
	//! (and the rut tiles, MUD_VoxelProbe.FieldPatchBegin) reaching [gx0..gx0+span] x [gz0..gz0+span]. Same samples,
	//! same floats as MeshFloor. Call right before one build loop; valid until the next tile change.
	static void PatchBegin(int gx0, int gz0, int span)
	{
		MUD_VoxelProbe.FieldPatchBegin(gx0, gz0, span);
		s_aPTiles.Clear();
		if (!ENABLED || s_mTiles.IsEmpty())
			return;     // no tiles cached = the real floor, as MeshFloor
		s_iPBi0 = FloorDiv(gx0, TS);
		s_iPBk0 = FloorDiv(gz0, TS);
		s_iPNx = FloorDiv(gx0 + span, TS) - s_iPBi0 + 1;
		int nz = FloorDiv(gz0 + span, TS) - s_iPBk0 + 1;
		for (int k = 0; k < nz; k++)
		{
			for (int i = 0; i < s_iPNx; i++)
				s_aPTiles.Insert(s_mTiles.Get(TileKey(s_iPBi0 + i, s_iPBk0 + k)));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! PERF IDEA 3 (2026-10-19): MeshFloorP of the n x n vertices from gx0, gz0 (inside the PatchBegin box) in one call:
	//! fl holds FieldPatchFill's real floor; a look sample under it wins (= MeshFloorP's min). No call per vertex.
	static void PatchFill(int gx0, int gz0, int n, array<float> fl)
	{
		if (s_aPTiles.IsEmpty())
			return;
		for (int vz = 0; vz < n; vz++)
		{
			int oz = gz0 + vz - s_iPBk0 * TS;
			int tk = oz / TS;
			int row = (oz - tk * TS) * TS;
			int tRow = tk * s_iPNx;
			for (int vx = 0; vx < n; vx++)
			{
				int ox = gx0 + vx - s_iPBi0 * TS;
				int ti = ox / TS;
				MUD_LookTile tile = s_aPTiles[tRow + ti];
				if (!tile)
					continue;
				float y = tile.m_aY[row + ox - ti * TS];
				if (y < fl[vz * n + vx])
					fl[vz * n + vx] = y;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! perf plan B2: MeshFloor at gx, gz inside the PatchBegin box
	static float MeshFloorP(int gx, int gz)
	{
		float real = MUD_VoxelProbe.FieldFloorP(gx, gz);
		if (s_aPTiles.IsEmpty())
			return real;
		int ox = gx - s_iPBi0 * TS;
		int oz = gz - s_iPBk0 * TS;
		int ti = ox / TS;
		int tk = oz / TS;
		MUD_LookTile tile = s_aPTiles[tk * s_iPNx + ti];
		if (!tile)
			return real;
		float y = tile.m_aY[(oz - tk * TS) * TS + ox - ti * TS];
		if (y < real)
			return y;
		return real;
	}

	//------------------------------------------------------------------------------------------------
	//! LOOK_COVERS: the drawn floor at global fine vertex gx, gz does not change with the real floor now there - a
	//! look sample at or under it (CATCH), a real floor (not none), no ridge
	static bool Covers(int gx, int gz)
	{
		if (!ENABLED || s_mTiles.IsEmpty())
			return false;
		float real = MUD_VoxelProbe.FieldFloor(gx, gz);
		if (real > MUD_VoxelProbe.FLOOR_NONE * 0.5 || MUD_VoxelProbe.FieldRidge(gx, gz) < MUD_VoxelProbe.FLOOR_NONE * 0.5)
			return false;
		MUD_LookTile tile = s_mTiles.Get(TileKey(FloorDiv(gx, TS), FloorDiv(gz, TS)));
		if (!tile)
			return false;
		float y = tile.m_aY[(gz - FloorDiv(gz, TS) * TS) * TS + gx - FloorDiv(gx, TS) * TS];
		if (y > MUD_VoxelProbe.FLOOR_NONE * 0.5)
			return false;
		return y <= real + CATCH;
	}

	//------------------------------------------------------------------------------------------------
	//! A look tile reaches global fine vertices [gx0..gx1] x [gz0..gz1] (the patch mesh must read the floor)
	static bool InBox(int gx0, int gx1, int gz0, int gz1)
	{
		if (!ENABLED || s_mTiles.IsEmpty())
			return false;
		for (int bk = FloorDiv(gz0, TS); bk <= FloorDiv(gz1, TS); bk++)
		{
			for (int bi = FloorDiv(gx0, TS); bi <= FloorDiv(gx1, TS); bi++)
			{
				if (s_mTiles.Contains(TileKey(bi, bk)))
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! New play session: statics outlive it
	static void Reset()
	{
		s_mTiles.Clear();
		s_aTracks.Clear();
		s_fSweepAt = 0;
		s_fLogAt = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static MUD_LookTrack Track(Vehicle veh)
	{
		for (int i = s_aTracks.Count() - 1; i >= 0; i--)
		{
			MUD_LookTrack t = s_aTracks[i];
			if (!t.m_Veh)
			{
				s_aTracks.Remove(i);
				continue;
			}
			if (t.m_Veh == veh)
				return t;
		}
		MUD_LookTrack fresh = new MUD_LookTrack();
		fresh.m_Veh = veh;
		s_iNextId++;
		fresh.m_iId = s_iNextId;
		s_aTracks.Insert(fresh);
		return fresh;
	}

	//------------------------------------------------------------------------------------------------
	protected static int TileKey(int bi, int bk)
	{
		return (bk + TKEY / 2) * TKEY + bi + TKEY / 2;
	}

	protected static int FloorDiv(int a, int b)
	{
		if (a >= 0)
			return a / b;
		return -((-a + b - 1) / b);
	}
}
