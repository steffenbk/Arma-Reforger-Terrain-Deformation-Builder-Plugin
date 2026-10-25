[ComponentEditorProps(category: "Mud", description: "Trench builder part (F4): one planned trench / step / roofed trench / tunnel / dugout of a build area. Spawned by the server; needs an RplComponent.")]
class MUD_BuildPartComponentClass : ScriptComponentClass
{
}

//! Trench builder F4 (spec sec 4-5): one saved part of a build area, its own small replicated entity (like
//! MUD_TrenchSiteComponent, so a 200 m network never travels as one big string). Replicated: kind + text (every
//! machine builds the same pieces from it) and one progress char per piece. Trench kinds dig like a trench site
//! (MUD_WorldGroundEntity.CarveTrench, a layer per swing); roofed kinds dig one shovel-tunnel capsule per swing
//! (MUD_VoxelProbe.ServerTunnel). Links (sec 2 step 7): a trench / tunnel reaching into a neighbour area gets a mirror
//! part there; each digs only the pieces inside its own area (the source also the ones between areas).
//! Stays after it is dug (the manager shows and edits it); goes with its area.
class MUD_BuildPartComponent : ScriptComponent
{
	static const ResourceName PREFAB = "{4D55444255494C40}Prefabs/Mud/MudBuildPart.et";
	static const int SKIPPED = 99;              // > any layer count: done / never dug
	static const string SKIP_CHAR = "x";        // hard / locked ground: never dug
	static const string OTHER_CHAR = "o";       // link: the other area's copy digs it
	static const string COVER_CHAR = "c";       // under a roofed stretch: left undug (it is the roof)
	static const int GHOST_MS = 1000;
	protected static float s_fGhostRange = 120;     // m from the camera; the manager raises it

	[RplProp(onRplName: "OnSpecRpl")]
	protected int m_iUid;
	[RplProp(onRplName: "OnSpecRpl")]
	protected int m_iArea;
	[RplProp(onRplName: "OnSpecRpl")]
	protected int m_iKind;
	[RplProp(onRplName: "OnSpecRpl")]
	protected int m_iSource;                // link mirror: uid of the part it copies; 0 = an own part
	[RplProp(onRplName: "OnSpecRpl")]
	protected string m_sText;
	[RplProp(onRplName: "OnProgressRpl")]
	protected string m_sProgress;           // one char per piece: MUD_TrenchSiteComponent.DIGITS / x / o / c

	protected ref MUD_BuildSpec m_Spec;
	protected ref MUD_TrenchGhosts m_Ghosts = new MUD_TrenchGhosts();
	protected ref MUD_BuildDraw m_Draw = new MUD_BuildDraw();
	protected bool m_bDrawn;
	protected bool m_bGhostDirty = true;
	protected bool m_bDeleting;
	protected vector m_vMid;
	protected float m_fRadius;

	protected static ref array<MUD_BuildPartComponent> s_aAll = {};
	protected static int s_iNextUid;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!GetGame().InPlayMode())
			return;
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		s_aAll.RemoveItem(null);
		s_aAll.Insert(this);
		if (!m_sText.IsEmpty())
			Rebuild();
		if (RplSession.Mode() != RplMode.Dedicated)
			GetGame().GetCallqueue().CallLater(GhostTick, GHOST_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		m_Ghosts.Clear();
		m_Draw.Clear();
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(GhostTick);
		s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSpecRpl()
	{
		Rebuild();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnProgressRpl()
	{
		m_bGhostDirty = true;
		GhostTick();
	}

	//------------------------------------------------------------------------------------------------
	protected void Rebuild()
	{
		m_Spec = MUD_BuildSpec.Make(m_iKind, m_sText);
		if (!m_Spec)
		{
			if (!m_sText.IsEmpty() && m_iKind != 0)
				Print(string.Format("MUD build part: %1 kind %2 does not build: %3", m_iUid, m_iKind, m_sText), LogLevel.WARNING);
			return;
		}
		m_Spec.Floors();
		m_Spec.Bounds(m_vMid, m_fRadius);
		m_bGhostDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The manager draws ghosts of parts this far from its camera (0 = back to the default)
	static void SetGhostRange(float range)
	{
		if (range <= 0)
			range = 120;
		if (range == s_fGhostRange)
			return;
		s_fGhostRange = range;
		foreach (MUD_BuildPartComponent p : s_aAll)
		{
			if (p)
				p.GhostTick();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: spawn a part of area. mirrorOf = the source uid for a link mirror (0 = own part); mirrored = the other
	//! areas that got a mirror of this part (their pieces are OTHER_CHAR here). null when it failed.
	static MUD_BuildPartComponent Spawn(MUD_BuildAreaComponent area, int kind, string text, int mirrorOf, array<int> mirrored, out string why)
	{
		Resource res = Resource.Load(PREFAB);
		if (!res || !res.IsValid())
		{
			why = "part prefab will not load: " + PREFAB;
			return null;
		}
		MUD_BuildSpec spec = MUD_BuildSpec.Make(kind, text);
		if (!spec)
		{
			why = "bad part";
			return null;
		}
		vector mid;
		float radius;
		spec.Bounds(mid, radius);
		mid[1] = MUD_BuildDraw.GroundY(mid[0], mid[2]);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = mid;
		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
		{
			why = "spawn failed";
			return null;
		}
		MUD_BuildPartComponent part = MUD_BuildPartComponent.Cast(ent.FindComponent(MUD_BuildPartComponent));
		if (!part)
		{
			why = "prefab has no MUD_BuildPartComponent";
			RplComponent.DeleteRplEntity(ent, false);
			return null;
		}
		part.SetUp(area, kind, text, mirrorOf, mirrored);
		return part;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, right after spawn: who digs each piece (sec 5 link rule), red trench pieces start skipped
	protected void SetUp(MUD_BuildAreaComponent area, int kind, string text, int mirrorOf, array<int> mirrored)
	{
		s_iNextUid++;
		m_iUid = s_iNextUid;
		m_iArea = area.Id();
		m_iKind = kind;
		m_iSource = mirrorOf;
		m_sText = text;
		Rebuild();
		m_sProgress = "";
		int red = 0;
		int other = 0;
		MUD_BuildQuad own = area.Quad();
		for (int i = 0; i < m_Spec.Count(); i++)
		{
			vector m = m_Spec.PieceXZ(i);
			string c = "0";
			bool inOwn = own && own.Inside(m[0], m[2], MUD_Build.INSIDE_TOL);
			if (!inOwn && (mirrorOf != 0 || InMirrored(m, mirrored, area.Id())))
			{
				c = OTHER_CHAR;
				other++;
			}
			else if (!m_Spec.IsCapsules() && !MUD_WorldGroundEntity.IsOver(m))
			{
				c = SKIP_CHAR;
				red++;
			}
			m_sProgress += c;
		}
		Replication.BumpMe();
		Print(string.Format("MUD build part: %1 %2 in area %3 (mirror of %4), pieces %5, red %6, other area %7: %8", m_iUid, MUD_Build.KindName(kind), m_iArea, mirrorOf, m_Spec.Count(), red, other, text));
		m_bGhostDirty = true;
		GhostTick();
	}

	//------------------------------------------------------------------------------------------------
	//! (x, z) lies in one of the mirrored areas (not area skip)
	protected static bool InMirrored(vector m, array<int> mirrored, int skip)
	{
		if (!mirrored)
			return false;
		foreach (int id : mirrored)
		{
			if (id == skip)
				continue;
			MUD_BuildAreaComponent a = MUD_BuildAreaComponent.Find(id);
			if (a && a.Quad() && a.Quad().Inside(m[0], m[2], MUD_Build.INSIDE_TOL))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	int Uid()
	{
		return m_iUid;
	}

	//------------------------------------------------------------------------------------------------
	int AreaId()
	{
		return m_iArea;
	}

	//------------------------------------------------------------------------------------------------
	int Kind()
	{
		return m_iKind;
	}

	//------------------------------------------------------------------------------------------------
	int Source()
	{
		return m_iSource;
	}

	//------------------------------------------------------------------------------------------------
	string Text()
	{
		return m_sText;
	}

	//------------------------------------------------------------------------------------------------
	MUD_BuildSpec Spec()
	{
		return m_Spec;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDeleting()
	{
		return m_bDeleting;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers dug of piece i (SKIPPED for x / o / c and out of range)
	int Digit(int i)
	{
		if (i < 0 || i >= m_sProgress.Length())
			return SKIPPED;
		int d = MUD_TrenchSiteComponent.DIGITS.IndexOf(m_sProgress.Substring(i, 1));
		if (d < 0)
			return SKIPPED;
		return d;
	}

	//------------------------------------------------------------------------------------------------
	string CharAt(int i)
	{
		if (i < 0 || i >= m_sProgress.Length())
			return SKIP_CHAR;
		return m_sProgress.Substring(i, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetChar(int i, string c)
	{
		int len = m_sProgress.Length();
		if (i < 0 || i >= len)
			return;
		m_sProgress = m_sProgress.Substring(0, i) + c + m_sProgress.Substring(i + 1, len - i - 1);
	}

	//------------------------------------------------------------------------------------------------
	int Layers()
	{
		if (!m_Spec)
			return 0;
		return m_Spec.Layers();
	}

	//------------------------------------------------------------------------------------------------
	bool PieceDone(int i)
	{
		return Digit(i) >= Layers();
	}

	//------------------------------------------------------------------------------------------------
	//! Any piece has a layer or capsule dug
	bool Started()
	{
		for (int i = 0; i < m_sProgress.Length(); i++)
		{
			int d = Digit(i);
			if (d > 0 && d != SKIPPED)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pieces this part digs (not x / o / c) and of them done
	void Counts(out int mine, out int done)
	{
		mine = 0;
		done = 0;
		for (int i = 0; i < m_sProgress.Length(); i++)
		{
			if (Digit(i) == SKIPPED)
				continue;
			mine++;
			if (PieceDone(i))
				done++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: the live part with this uid
	static MUD_BuildPartComponent Find(int uid)
	{
		foreach (MUD_BuildPartComponent p : s_aAll)
		{
			if (p && !p.IsDeleting() && p.Uid() == uid)
				return p;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: every live part of area
	static void ForArea(int area, array<MUD_BuildPartComponent> outParts)
	{
		outParts.Clear();
		foreach (MUD_BuildPartComponent p : s_aAll)
		{
			if (p && !p.IsDeleting() && p.AreaId() == area)
				outParts.Insert(p);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: the first live trench / step part with an unfinished piece under wp (world XZ)
	static bool FindTrenchPiece(vector wp, out MUD_BuildPartComponent part, out int piece)
	{
		foreach (MUD_BuildPartComponent p : s_aAll)
		{
			if (!p || p.IsDeleting() || !p.Spec() || !p.Spec().m_Layout)
				continue;
			int k = p.Spec().m_Layout.PieceAt(wp[0], wp[2]);
			if (k < 0 || p.PieceDone(k))
				continue;
			part = p;
			piece = k;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: the undug capsule a player with feet at feet, digging at dp (MUD_Dig.DigPoint), would dig: within
	//! DIG_PICK (flat) of dp, DIG_REACH of the chest, the feet not over it (no digging it from the surface), and next to
	//! an open one (CapOpen). The nearest such capsule.
	static bool FindCapsule(vector feet, vector dp, out MUD_BuildPartComponent part, out int piece)
	{
		float best = MUD_Build.DIG_PICK * MUD_Build.DIG_PICK;
		bool found = false;
		vector chest = feet + Vector(0, MUD_VoxelProbe.TUNNEL_MID, 0);
		foreach (MUD_BuildPartComponent p : s_aAll)
		{
			if (!p || p.IsDeleting() || !p.Spec() || !p.Spec().IsCapsules())
				continue;
			MUD_BuildSpec spec = p.Spec();
			if (vector.DistanceXZ(dp, p.Mid()) > p.Radius() + MUD_Build.DIG_PICK)
				continue;
			if (!spec.Floors())
				continue;
			for (int i = 0; i < spec.Count(); i++)
			{
				if (p.PieceDone(i))
					continue;
				vector mid = spec.CapMid(i);
				float dSq = vector.DistanceSqXZ(mid, dp);
				if (dSq >= best)
					continue;
				if (vector.Distance(mid, chest) > MUD_Build.DIG_REACH || feet[1] > mid[1] + MUD_Build.DIG_FEET_OVER)
					continue;
				if (!p.CapOpen(i))
					continue;
				best = dSq;
				part = p;
				piece = i;
				found = true;
			}
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	vector Mid()
	{
		return m_vMid;
	}

	//------------------------------------------------------------------------------------------------
	float Radius()
	{
		return m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Capsule i may be dug: an end of a path (or the door row of a dugout), or next to a dug / not-ours one
	bool CapOpen(int i)
	{
		if (!m_Spec)
			return false;
		int n = m_Spec.Count();
		if (m_iKind == MUD_Build.DUGOUT)
		{
			if (m_Spec.DoorRow(i))
				return true;
			for (int j = 0; j < n; j++)
			{
				if (m_Spec.Touch(i, j) && Digit(j) >= 1)
					return true;
			}
			return false;
		}
		if (i == 0 || i == n - 1)
			return true;
		return Digit(i - 1) >= 1 || Digit(i + 1) >= 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one swing on piece. False = not counted (patch building, done, not the server).
	bool DigPiece(int piece)
	{
		if (m_bDeleting || !m_Spec || !Replication.IsServer())
			return false;
		int dug = Digit(piece);
		if (dug >= Layers())
			return false;
		MUD_BuildAreaComponent area = MUD_BuildAreaComponent.Find(m_iArea);
		if (m_Spec.IsCapsules())
		{
			if (!m_Spec.Floors())
				return false;
			MUD_VoxelProbe.ServerTunnel(m_Spec.CapMid(piece));
			SetChar(piece, "1");
			Print(string.Format("MUD build dig: part %1 %2 capsule %3 / %4 at %5", m_iUid, MUD_Build.KindName(m_iKind), piece + 1, m_Spec.Count(), m_Spec.CapMid(piece)));
		}
		else
		{
			int r = MUD_WorldGroundEntity.CarveTrench(m_Spec.m_Layout, piece, dug + 1);
			if (r == -1)
				return false;
			if (r == -2)
			{
				SetChar(piece, SKIP_CHAR);
				Print(string.Format("MUD build dig: part %1 piece %2 skipped (hard / locked ground)", m_iUid, piece));
			}
			else
			{
				SetChar(piece, MUD_TrenchSiteComponent.DIGITS.Substring(dug + 1, 1));
				Print(string.Format("MUD build dig: part %1 %2 piece %3 layer %4 / %5", m_iUid, MUD_Build.KindName(m_iKind), piece, dug + 1, Layers()));
			}
		}
		if (area)
			area.Touch();
		Replication.BumpMe();
		m_bGhostDirty = true;
		GhostTick();        // own RplProp changes do not call the onRpl handler here
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, instant dig (test, ALLOW_INSTANT_DIG): every piece to full depth, layer by layer like real swings.
	//! Returns the swings done (a piece over a building patch stays for the shovel).
	int DigAll()
	{
		int swings = 0;
		if (!m_Spec)
			return 0;
		if (m_Spec.IsCapsules())
		{
			for (int c = 0; c < m_Spec.Count(); c++)
			{
				if (Digit(c) < 1 && DigPiece(c))
					swings++;
			}
			return swings;
		}
		for (int layer = 1; layer <= Layers(); layer++)
		{
			for (int i = 0; i < m_Spec.Count(); i++)
			{
				if (Digit(i) < layer && DigPiece(i))
					swings++;
			}
		}
		return swings;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, trench part under a COVER spec: its pieces between the cover's ends (less COVER_ENTRY each end) become
	//! COVER_CHAR (on) or undug again (off). On refuses (false, why) when one of them is dug already.
	bool Cover(MUD_BuildSpec cover, bool on, out string why)
	{
		if (!m_Spec || !m_Spec.m_Layout || !cover || cover.m_aPath.Count() < 2)
		{
			why = "no trench under the roof";
			return false;
		}
		MUD_TrenchLayout lay = m_Spec.m_Layout;
		float off;
		float s0 = lay.ChainPos(cover.m_aPath[0][0], cover.m_aPath[0][2], off);
		int last = cover.m_aPath.Count() - 1;
		float s1 = lay.ChainPos(cover.m_aPath[last][0], cover.m_aPath[last][2], off);
		float lo = Math.Min(s0, s1) + MUD_Build.COVER_ENTRY;
		float hi = Math.Max(s0, s1) - MUD_Build.COVER_ENTRY;
		array<int> under = {};
		for (int i = 0; i < lay.Count(); i++)
		{
			vector m = lay.PieceMiddle(i);
			float s = lay.ChainPos(m[0], m[2], off);
			if (s > lo && s < hi)
				under.Insert(i);
		}
		if (on)
		{
			foreach (int k : under)
			{
				int d = Digit(k);
				if (d > 0 && d != SKIPPED)
				{
					why = "that stretch is dug open already";
					return false;
				}
			}
			foreach (int k2 : under)
			{
				if (CharAt(k2) == "0")
					SetChar(k2, COVER_CHAR);
			}
		}
		else
		{
			foreach (int k3 : under)
			{
				if (CharAt(k3) == COVER_CHAR)
					SetChar(k3, "0");
			}
		}
		Replication.BumpMe();
		m_bGhostDirty = true;
		GhostTick();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server
	void DeletePart()
	{
		if (m_bDeleting)
			return;
		m_bDeleting = true;
		m_Ghosts.Clear();
		m_Draw.Clear();
		IEntity owner = GetOwner();
		if (owner && !owner.IsDeleted())
			RplComponent.DeleteRplEntity(owner, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ghosts while the camera is near; redrawn when the progress changed
	protected void GhostTick()
	{
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		bool want = m_Spec != null && !m_bDeleting;
		if (want)
			want = vector.DistanceXZ(MUD_BuildAreaComponent.CameraPos(), m_vMid) - m_fRadius < s_fGhostRange;
		if (!want)
		{
			if (m_bDrawn)
			{
				m_Ghosts.Clear();
				m_Draw.Clear();
			}
			m_bDrawn = false;
			return;
		}
		if (m_bDrawn && !m_bGhostDirty)
			return;
		m_bDrawn = true;
		m_bGhostDirty = false;
		if (!m_Spec.IsCapsules())
		{
			array<int> states = {};
			for (int i = 0; i < m_Spec.Count(); i++)
			{
				int dug = Digit(i);
				if (dug >= Layers())
					states.Insert(MUD_TrenchGhosts.HIDDEN);
				else if (dug > 0)
					states.Insert(MUD_TrenchGhosts.DUG);
				else
					states.Insert(MUD_TrenchGhosts.GREEN);
			}
			m_Ghosts.Draw(m_Spec.m_Layout, states);
			return;
		}
		// roofed parts lie under the ground: their capsules as squares on the surface above, dug ones faint
		float yaw = 0;
		if (m_iKind == MUD_Build.DUGOUT)
			yaw = m_Spec.m_fYaw;
		m_Draw.Begin();
		for (int c = 0; c < m_Spec.Count(); c++)
		{
			string ch = CharAt(c);
			if (ch == OTHER_CHAR)
				continue;
			int state = MUD_TrenchGhosts.GREEN;
			if (Digit(c) >= 1)
				state = MUD_TrenchGhosts.DUG;
			m_Draw.Square(m_Spec.PieceXZ(c), 0.35, yaw, state);
		}
		m_Draw.End();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: how much area holds now (caps, sec 6). Link mirrors do not count: their source's area caps bound them,
	//! and counting them would let a neighbour (an enemy, PvP) fill this area's caps with links.
	static void Totals(int area, out int parts, out int pieces, out int caps)
	{
		parts = 0;
		pieces = 0;
		caps = 0;
		foreach (MUD_BuildPartComponent p : s_aAll)
		{
			if (!p || p.IsDeleting() || p.AreaId() != area || !p.Spec() || p.Source() != 0)
				continue;
			parts++;
			if (p.Spec().IsCapsules())
				caps += p.Spec().Count();
			else
				pieces += p.Spec().Count();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server (manager Save): check and place one part in area for player. "" = placed, else why not.
	static string ServerSave(MUD_BuildAreaComponent area, int kind, string text)
	{
		MUD_BuildSpec spec = MUD_BuildSpec.Make(kind, text);
		if (!spec)
		{
			MUD_BuildSpec probe = new MUD_BuildSpec();
			probe.Parse(kind, text);
			return "bad " + MUD_Build.KindName(kind) + ": " + probe.m_sError;
		}
		MUD_BuildQuad own = area.Quad();
		if (!own)
			return "area not ready";

		// every point inside the area; trench lines and tunnels may reach out to LINK_REACH (links)
		bool linkable = kind == MUD_Build.TRENCH || kind == MUD_Build.TUNNEL;
		array<vector> pts = {};
		spec.Points(pts);
		array<int> mirrored = {};
		foreach (vector p : pts)
		{
			if (own.Inside(p[0], p[2], MUD_Build.INSIDE_TOL))
				continue;
			if (!linkable)
				return MUD_Build.KindName(kind) + " must lie inside the area";
			if (own.DistOutside(p[0], p[2]) > MUD_Build.LINK_REACH)
				return string.Format("a point is more than %1 m out of the area", MUD_Build.LINK_REACH);
			MUD_BuildAreaComponent other = MUD_BuildAreaComponent.AreaAt(p[0], p[2], 0, area.Id());
			if (other && mirrored.Find(other.Id()) < 0)
				mirrored.Insert(other.Id());
		}

		int parts;
		int pieces;
		int caps;
		Totals(area.Id(), parts, pieces, caps);
		if (parts >= MUD_Build.MAX_PARTS)
			return string.Format("the area has %1 parts (max %2)", parts, MUD_Build.MAX_PARTS);
		if (spec.IsCapsules() && caps + spec.Count() > MUD_Build.MAX_CAPS)
			return string.Format("too much tunnel / roof / dugout in the area (%1 + %2 capsules, max %3)", caps, spec.Count(), MUD_Build.MAX_CAPS);
		if (!spec.IsCapsules() && pieces + spec.Count() > MUD_Build.MAX_PIECES)
			return string.Format("too much trench in the area (%1 + %2 pieces, max %3)", pieces, spec.Count(), MUD_Build.MAX_PIECES);

		// a roofed stretch: its trench (a part of this area) keeps the pieces under the roof undug
		MUD_BuildPartComponent covered;
		if (kind == MUD_Build.COVER)
		{
			covered = Find(spec.m_iCoverOf);
			if (!covered || covered.AreaId() != area.Id() || covered.Kind() != MUD_Build.TRENCH)
				return "the roof must lie on a saved trench of this area";
			string coverWhy;
			if (!covered.Cover(spec, true, coverWhy))
				return "roof: " + coverWhy;
		}

		// mirrors only where that area has room (else the source digs those pieces itself)
		array<int> ok = {};
		foreach (int id : mirrored)
		{
			int mp;
			int mpc;
			int mc;
			Totals(id, mp, mpc, mc);
			bool room = mpc + spec.Count() <= MUD_Build.MAX_PIECES;
			if (spec.IsCapsules())
				room = mc + spec.Count() <= MUD_Build.MAX_CAPS;
			if (mp < MUD_Build.MAX_PARTS && room)
				ok.Insert(id);
			else
				Print(string.Format("MUD build part: link into area %1 not mirrored (its caps are full) - area %2 digs those pieces", id, area.Id()), LogLevel.WARNING);
		}

		string why;
		MUD_BuildPartComponent part = Spawn(area, kind, text, 0, ok, why);
		if (!part)
		{
			if (covered)
				covered.Cover(spec, false, why);
			return why;
		}
		foreach (int mid : ok)
		{
			MUD_BuildAreaComponent ma = MUD_BuildAreaComponent.Find(mid);
			if (ma && !Spawn(ma, kind, text, part.Uid(), null, why))
				Print(string.Format("MUD build part: mirror of %1 in area %2 failed: %3", part.Uid(), mid, why), LogLevel.WARNING);
		}
		area.Touch();
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server (manager delete): part uid of area goes, with its link group (source + mirrors) and the roofs on it.
	//! A deleted roof gives its trench stretch back to the shovel. Dug ground stays. "" = done, else why not.
	static string ServerDelete(MUD_BuildAreaComponent area, int uid)
	{
		MUD_BuildPartComponent part = Find(uid);
		if (!part || part.AreaId() != area.Id())
			return "no such part in this area";
		int root = uid;
		if (part.Source() != 0)
			root = part.Source();
		array<MUD_BuildPartComponent> group = {};
		foreach (MUD_BuildPartComponent p : s_aAll)
		{
			if (p && !p.IsDeleting() && (p.Uid() == root || p.Source() == root))
				group.Insert(p);
		}
		// roofs lying on a part of the group go too
		array<MUD_BuildPartComponent> roofs = {};
		foreach (MUD_BuildPartComponent r : s_aAll)
		{
			if (!r || r.IsDeleting() || r.Kind() != MUD_Build.COVER || !r.Spec())
				continue;
			foreach (MUD_BuildPartComponent g : group)
			{
				if (r.Spec().m_iCoverOf == g.Uid() && roofs.Find(r) < 0)
					roofs.Insert(r);
			}
		}
		foreach (MUD_BuildPartComponent g2 : group)
		{
			if (g2.Kind() == MUD_Build.COVER && roofs.Find(g2) < 0)
				roofs.Insert(g2);
		}
		string why;
		foreach (MUD_BuildPartComponent roof : roofs)
		{
			MUD_BuildPartComponent under = Find(roof.Spec().m_iCoverOf);
			if (under && group.Find(under) < 0)
				under.Cover(roof.Spec(), false, why);
			Print(string.Format("MUD build part: %1 (roof) deleted", roof.Uid()));
			roof.DeletePart();
		}
		foreach (MUD_BuildPartComponent g3 : group)
		{
			Print(string.Format("MUD build part: %1 %2 deleted", g3.Uid(), MUD_Build.KindName(g3.Kind())));
			g3.DeletePart();
		}
		area.Touch();
		return "";
	}
}
