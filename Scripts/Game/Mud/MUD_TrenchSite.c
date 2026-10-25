[ComponentEditorProps(category: "Mud", description: "One confirmed trench (plan mode), dug piece by piece with a shovel. Spawned by the server; needs an RplComponent.")]
class MUD_TrenchSiteComponentClass : ScriptComponentClass
{
}

//! One confirmed trench plan (spec section 6). The server owns the progress (one character per piece:
//! layers dug as DIGITS[n], SKIP_CHAR = red / hard / locked, never dug); every machine rebuilds the layout
//! from the plan text and shows ghosts over unfinished pieces. Deletes itself when every piece is done.
class MUD_TrenchSiteComponent : ScriptComponent
{
	static const ResourceName PREFAB = "{4D55445452454E30}Prefabs/Mud/MudTrenchSite.et";
	static const int MAX_SITES = 20;                // server-wide
	static const int SITE_IDLE_S = 900;             // audit A5 2026-10-22: a site with no dig this long is deleted (dug pieces stay)
	static const int SKIPPED = 99;              // > any MUD_TrenchPlan.MAX_LAYERS: a skipped piece counts as done
	static const string SKIP_CHAR = "x";
	// one char per layer count: more than 9 layers since 2026-10-13 (2.5 m = 10)
	static const string DIGITS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	// TEMP debug (user 2026-10-10 01:02): F in plan mode digs the whole trench at once. false = real play.
	// 2026-10-11: false - Ctrl+F digs it at once instead (MUD_TrenchInstant), F = dig with the shovel.
	static const bool DEBUG_INSTANT = false;
	// Review F6 (2026-10-16): server check for a client's instant dig (Ctrl+F sends instant = true). true for testing;
	// FLIP TO false BEFORE RELEASE (master list Lane 2) - then a client can't dig a whole trench at once.
	static const bool ALLOW_INSTANT_DIG = true;

	[RplProp(onRplName: "OnPlanRpl")]
	protected string m_sPlan;           // MUD_TrenchPlan.Pack text

	[RplProp(onRplName: "OnProgressRpl")]
	protected string m_sProgress;       // one digit per piece

	protected ref MUD_TrenchLayout m_Layout;
	protected int m_iLayers;
	protected ref MUD_TrenchGhosts m_Ghosts = new MUD_TrenchGhosts();
	protected bool m_bDeleting;
	protected int m_iId;                // server: log name
	protected int m_iDugAt;             // server: tick of the placing or the last dig (A5 idle expiry)

	protected static ref array<MUD_TrenchSiteComponent> s_aAll = {};   // every site on this machine
	protected static int s_iNextId;

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
		// a joining client may get the plan before init without the onRpl call
		if (!m_sPlan.IsEmpty())
		{
			Rebuild();
			UpdateGhosts();
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		m_Ghosts.Clear();
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(DeleteSite);
			GetGame().GetCallqueue().Remove(IdleCheck);
		}
		s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: spawn a site for plan. null and why when refused.
	static MUD_TrenchSiteComponent Spawn(MUD_TrenchPlan plan, out string why)
	{
		s_aAll.RemoveItem(null);
		if (s_aAll.Count() >= MAX_SITES)
		{
			why = string.Format("%1 trench sites already (max %2; one goes after %3 min without a dig)", s_aAll.Count(), MAX_SITES, SITE_IDLE_S / 60);
			return null;
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(plan))
		{
			why = "bad plan: " + lay.m_sError;
			return null;
		}
		Resource res = Resource.Load(PREFAB);
		if (!res || !res.IsValid())
		{
			why = "site prefab will not load: " + PREFAB;
			return null;
		}

		vector origin = plan.m_aPoints[0];
		float y;
		if (!MUD_WorldGroundEntity.SurfaceY(origin[0], origin[2], y))
			y = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]);
		origin[1] = y;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = origin;
		IEntity ent = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!ent)
		{
			why = "spawn failed";
			return null;
		}
		MUD_TrenchSiteComponent site = MUD_TrenchSiteComponent.Cast(ent.FindComponent(MUD_TrenchSiteComponent));
		if (!site)
		{
			why = "prefab has no MUD_TrenchSiteComponent";
			RplComponent.DeleteRplEntity(ent, false);
			return null;
		}
		site.SetPlan(plan.Pack());
		return site;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, right after spawn: plan text in, red pieces (middle not over a hole cell) start SKIPPED
	protected void SetPlan(string packed)
	{
		s_iNextId++;
		m_iId = s_iNextId;
		m_sPlan = packed;
		Rebuild();
		m_sProgress = "";
		int red;
		if (m_Layout)
		{
			for (int i = 0; i < m_Layout.Count(); i++)
			{
				if (MUD_WorldGroundEntity.IsOver(m_Layout.PieceMiddle(i)))
				{
					m_sProgress += "0";
				}
				else
				{
					m_sProgress += SKIP_CHAR;
					red++;
				}
			}
		}
		Replication.BumpMe();
		int count;
		if (m_Layout)
			count = m_Layout.Count();
		Print(string.Format("MUD trench site: %1 placed %2, pieces %3, red %4, layers %5", m_iId, m_sPlan, count, red, m_iLayers));
		m_iDugAt = System.GetTickCount();
		GetGame().GetCallqueue().CallLater(IdleCheck, 60000, true);
		UpdateGhosts();
		CheckFinished();
	}

	//------------------------------------------------------------------------------------------------
	//! Server (audit A5): no dig for SITE_IDLE_S -> the site goes, so idle sites can't fill MAX_SITES for everyone
	protected void IdleCheck()
	{
		if (m_bDeleting || System.GetTickCount(m_iDugAt) < SITE_IDLE_S * 1000)
			return;
		m_bDeleting = true;
		GetGame().GetCallqueue().Remove(IdleCheck);
		Print(string.Format("MUD trench site: %1 idle %2 s without a dig, deleting", m_iId, System.GetTickCount(m_iDugAt) / 1000));
		DeleteSite();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlanRpl()
	{
		Rebuild();
		UpdateGhosts();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnProgressRpl()
	{
		UpdateGhosts();
	}

	//------------------------------------------------------------------------------------------------
	protected void Rebuild()
	{
		m_Layout = null;
		m_iLayers = 0;
		MUD_TrenchPlan plan = MUD_TrenchPlan.Unpack(m_sPlan);
		if (!plan)
		{
			Print("MUD trench site: plan does not unpack: " + m_sPlan, LogLevel.WARNING);
			return;
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(plan))
		{
			Print("MUD trench site: plan does not build: " + lay.m_sError, LogLevel.WARNING);
			return;
		}
		m_Layout = lay;
		m_iLayers = plan.m_iLayers;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers dug of piece i (SKIPPED for red ones and out of range)
	int Digit(int i)
	{
		if (i < 0 || i >= m_sProgress.Length())
			return SKIPPED;
		int d = DIGITS.IndexOf(m_sProgress.Substring(i, 1));
		if (d < 0)
			return SKIPPED;     // SKIP_CHAR (or anything unknown)
		return d;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetDigit(int i, int d)
	{
		string c = SKIP_CHAR;
		if (d >= 0 && d < DIGITS.Length())
			c = DIGITS.Substring(d, 1);
		int len = m_sProgress.Length();
		m_sProgress = m_sProgress.Substring(0, i) + c + m_sProgress.Substring(i + 1, len - i - 1);
	}

	//------------------------------------------------------------------------------------------------
	int Layers()
	{
		return m_iLayers;
	}

	//------------------------------------------------------------------------------------------------
	MUD_TrenchLayout GetLayout()
	{
		return m_Layout;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDeleting()
	{
		return m_bDeleting;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: the first live site with an unfinished piece under wp (world XZ)
	static bool FindPiece(vector wp, out MUD_TrenchSiteComponent site, out int piece)
	{
		foreach (MUD_TrenchSiteComponent s : s_aAll)
		{
			if (!s || s.IsDeleting() || !s.GetLayout())
				continue;
			int p = s.GetLayout().PieceAt(wp[0], wp[2]);
			if (p < 0 || s.Digit(p) >= s.Layers())
				continue;
			site = s;
			piece = p;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one swing on piece. False = not counted (patch building, done, not the server).
	bool DigPiece(int piece)
	{
		if (m_bDeleting || !m_Layout || !Replication.IsServer())
			return false;
		int dug = Digit(piece);
		if (dug >= m_iLayers)
			return false;

		int r = MUD_WorldGroundEntity.CarveTrench(m_Layout, piece, dug + 1);
		if (r == -1)
			return false;
		if (r == -2)
		{
			SetDigit(piece, SKIPPED);
			Print(string.Format("MUD trench dig: site %1 piece %2 skipped (hard / locked ground)", m_iId, piece));
		}
		else
		{
			SetDigit(piece, dug + 1);
			Print(string.Format("MUD trench dig: site %1 piece %2 layer %3 / %4", m_iId, piece, dug + 1, m_iLayers));
		}
		m_iDugAt = System.GetTickCount();
		Replication.BumpMe();
		UpdateGhosts();     // own RplProp changes do not call the onRpl handler here
		CheckFinished();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateGhosts()
	{
		if (!m_Layout || m_sProgress.Length() != m_Layout.Count())
		{
			m_Ghosts.Clear();
			return;
		}
		array<int> states = {};
		for (int i = 0; i < m_Layout.Count(); i++)
		{
			int dug = Digit(i);
			if (dug >= m_iLayers)
				states.Insert(MUD_TrenchGhosts.HIDDEN);
			else if (dug > 0)
				states.Insert(MUD_TrenchGhosts.DUG);
			else
				states.Insert(MUD_TrenchGhosts.GREEN);
		}
		m_Ghosts.Draw(m_Layout, states);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, DEBUG_INSTANT: dig every piece to full depth now, layer by layer across the trench like
	//! real swings. Returns the swings done (a piece over a building patch stays for the shovel).
	int DebugDigAll()
	{
		int swings;
		if (!m_Layout)
			return 0;
		for (int layer = 1; layer <= m_iLayers; layer++)
		{
			for (int i = 0; i < m_Layout.Count(); i++)
			{
				if (Digit(i) < layer && DigPiece(i))
					swings++;
			}
		}
		Print(string.Format("MUD trench site: %1 DEBUG instant dig, %2 swings", m_iId, swings));
		return swings;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every piece done or skipped -> check line, delete in 0.5 s
	protected void CheckFinished()
	{
		if (m_bDeleting || !m_Layout || !Replication.IsServer())
			return;
		for (int i = 0; i < m_Layout.Count(); i++)
		{
			if (Digit(i) < m_iLayers)
				return;
		}
		m_bDeleting = true;
		array<int> layers = {};
		for (int i2 = 0; i2 < m_Layout.Count(); i2++)
		{
			int d = Digit(i2);
			if (d == SKIPPED)
				d = 0;
			layers.Insert(d);
		}
		MUD_WorldGroundEntity.CheckTrench(m_Layout, layers, string.Format("site %1", m_iId));
		Print(string.Format("MUD trench site: %1 done, deleting", m_iId));
		GetGame().GetCallqueue().CallLater(DeleteSite, 500, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteSite()
	{
		IEntity owner = GetOwner();
		if (owner && !owner.IsDeleted())
			RplComponent.DeleteRplEntity(owner, false);
	}
}
