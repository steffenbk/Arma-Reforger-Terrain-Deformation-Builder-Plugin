[ComponentEditorProps(category: "Mud", description: "Trench builder build area (F4): 4 staked corners, owner, edit lock. Spawned by the server; needs an RplComponent.")]
class MUD_BuildAreaComponentClass : ScriptComponentClass
{
}

//! Trench builder F4 (spec docs/superpowers/specs/2026-10-24-trench-builder-design.md sec 2): one staked build area.
//! The server owns it: corners, the staking player, the open-to-all flag (after OWNER_HOLD_S), the edit lock (one
//! manager at a time). Its parts are MUD_BuildPartComponent entities that name its id. Every machine draws its
//! outline when the camera is near. Goes (with its parts; dug ground stays) after AREA_IDLE_S without a dig or edit.
class MUD_BuildAreaComponent : ScriptComponent
{
	static const ResourceName PREFAB = "{4D55444255494C30}Prefabs/Mud/MudBuildArea.et";
	static const int TICK_MS = 10000;           // server: rights / lock / idle check
	static const int DRAW_MS = 1000;            // client: outline range check
	static const float DRAW_RANGE = 400;        // m from the camera to the area edge: outline drawn

	[RplProp(onRplName: "OnQuadRpl")]
	protected int m_iId;
	[RplProp(onRplName: "OnQuadRpl")]
	protected string m_sQuad;               // MUD_BuildQuad.Pack
	[RplProp()]
	protected int m_iOwner;                 // player id of the staker
	[RplProp()]
	protected string m_sOwnerName;
	[RplProp()]
	protected bool m_bOpen;                 // true after OWNER_HOLD_S: everyone may edit
	[RplProp()]
	protected int m_iEditor;                // player id with the manager open, 0 = none

	protected ref MUD_BuildQuad m_Quad;
	protected ref MUD_BuildDraw m_Outline = new MUD_BuildDraw();
	protected float m_fOutlineW;            // width drawn now, 0 = not drawn
	protected bool m_bDeleting;
	protected int m_iStakeAt;               // server ticks
	protected int m_iTouchAt;
	protected int m_iLockAt;

	protected static ref array<MUD_BuildAreaComponent> s_aAll = {};
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
		// a joining client may get the props before init without the onRpl call
		if (!m_sQuad.IsEmpty())
			m_Quad = MUD_BuildQuad.Unpack(m_sQuad);
		if (RplSession.Mode() != RplMode.Dedicated)
			GetGame().GetCallqueue().CallLater(DrawTick, DRAW_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		m_Outline.Clear();
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(DrawTick);
			GetGame().GetCallqueue().Remove(Tick);
		}
		s_aAll.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnQuadRpl()
	{
		m_Quad = MUD_BuildQuad.Unpack(m_sQuad);
		m_fOutlineW = 0;
		DrawTick();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stake an area for player. null and why when refused.
	static MUD_BuildAreaComponent Spawn(MUD_BuildQuad q, int player, out string why)
	{
		s_aAll.RemoveItem(null);
		if (!q || !q.Valid(why))
			return null;
		int live = 0;
		foreach (MUD_BuildAreaComponent a : s_aAll)
		{
			if (!a || a.IsDeleting())
				continue;
			live++;
			if (a.Quad() && a.Quad().Overlaps(q))
			{
				why = string.Format("it overlaps build area %1", a.Id());
				return null;
			}
		}
		if (live >= MUD_Build.MAX_AREAS)
		{
			why = string.Format("%1 build areas already (max %2)", live, MUD_Build.MAX_AREAS);
			return null;
		}
		Resource res = Resource.Load(PREFAB);
		if (!res || !res.IsValid())
		{
			why = "area prefab will not load: " + PREFAB;
			return null;
		}
		vector origin = q.Centre();
		origin[1] = MUD_BuildDraw.GroundY(origin[0], origin[2]);
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
		MUD_BuildAreaComponent area = MUD_BuildAreaComponent.Cast(ent.FindComponent(MUD_BuildAreaComponent));
		if (!area)
		{
			why = "prefab has no MUD_BuildAreaComponent";
			RplComponent.DeleteRplEntity(ent, false);
			return null;
		}
		area.SetUp(q, player);
		return area;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetUp(MUD_BuildQuad q, int player)
	{
		s_iNextId++;
		m_iId = s_iNextId;
		m_sQuad = q.Pack();
		m_Quad = MUD_BuildQuad.Unpack(m_sQuad);     // the packed (cm) corners: the same floats as every client
		m_iOwner = player;
		m_sOwnerName = GetGame().GetPlayerManager().GetPlayerName(player);
		m_bOpen = false;
		m_iEditor = 0;
		m_iStakeAt = System.GetTickCount();
		m_iTouchAt = m_iStakeAt;
		Replication.BumpMe();
		Print(string.Format("MUD build area: %1 staked by player %2 (%3), corners %4", m_iId, player, m_sOwnerName, m_sQuad));
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		DrawTick();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: owner hold ends, a dead lock frees, an idle area goes
	protected void Tick()
	{
		if (m_bDeleting)
			return;
		bool bump = false;
		if (!m_bOpen && System.GetTickCount(m_iStakeAt) >= MUD_Build.OWNER_HOLD_S * 1000)
		{
			m_bOpen = true;
			bump = true;
			Print(string.Format("MUD build area: %1 open to everyone now (%2 s after the stake)", m_iId, MUD_Build.OWNER_HOLD_S));
		}
		if (m_iEditor != 0)
		{
			bool gone = !GetGame().GetPlayerManager().GetPlayerController(m_iEditor);
			if (gone || System.GetTickCount(m_iLockAt) >= MUD_Build.LOCK_IDLE_S * 1000)
			{
				Print(string.Format("MUD build area: %1 edit lock of player %2 freed (left %3)", m_iId, m_iEditor, gone));
				m_iEditor = 0;
				bump = true;
			}
		}
		if (bump)
			Replication.BumpMe();
		if (m_iEditor == 0 && System.GetTickCount(m_iTouchAt) >= MUD_Build.AREA_IDLE_S * 1000)
		{
			Print(string.Format("MUD build area: %1 idle %2 s (no dig, no edit), deleting with its plan", m_iId, System.GetTickCount(m_iTouchAt) / 1000));
			DeleteArea();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the area and every part of it go (dug ground stays)
	void DeleteArea()
	{
		if (m_bDeleting)
			return;
		m_bDeleting = true;
		GetGame().GetCallqueue().Remove(Tick);
		array<MUD_BuildPartComponent> parts = {};
		MUD_BuildPartComponent.ForArea(m_iId, parts);
		foreach (MUD_BuildPartComponent p : parts)
		{
			p.DeletePart();
		}
		IEntity owner = GetOwner();
		if (owner && !owner.IsDeleted())
			RplComponent.DeleteRplEntity(owner, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: outline on the ground when the camera is near; wider when the manager looks from high up
	protected void DrawTick()
	{
		if (!m_Quad || m_bDeleting)
		{
			m_Outline.Clear();
			m_fOutlineW = 0;
			return;
		}
		vector cam = CameraPos();
		vector c = m_Quad.Centre();
		bool want = vector.DistanceXZ(cam, c) - m_Quad.Radius() < DRAW_RANGE;
		if (!want)
		{
			if (m_fOutlineW > 0)
				m_Outline.Clear();
			m_fOutlineW = 0;
			return;
		}
		float w = Math.Max(0.15, MUD_BuildManager.ViewHeight() * 0.006);
		if (m_fOutlineW > 0 && Math.AbsFloat(w - m_fOutlineW) < m_fOutlineW * 0.3)
			return;
		m_fOutlineW = w;
		m_Outline.Begin();
		m_Outline.Ribbon(m_Quad.m_aC, w, MUD_TrenchGhosts.DUG, true, 0.06);
		m_Outline.End();
	}

	//------------------------------------------------------------------------------------------------
	static vector CameraPos()
	{
		CameraManager cams = GetGame().GetCameraManager();
		if (!cams || !cams.CurrentCamera())
			return vector.Zero;
		vector m[4];
		cams.CurrentCamera().GetWorldTransform(m);
		return m[3];
	}

	//------------------------------------------------------------------------------------------------
	//! Server: may player edit now? why = the reason when not
	bool MayEdit(int player, out string why)
	{
		if (m_bDeleting)
		{
			why = "the area is being removed";
			return false;
		}
		if (player != m_iOwner && !m_bOpen)
		{
			int left = MUD_Build.OWNER_HOLD_S - System.GetTickCount(m_iStakeAt) / 1000;
			why = string.Format("only %1 can edit it for %2 more min", m_sOwnerName, Math.Ceil(left / 60.0));
			return false;
		}
		if (m_iEditor != 0 && m_iEditor != player)
		{
			why = string.Format("%1 has its manager open", GetGame().GetPlayerManager().GetPlayerName(m_iEditor));
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the edit lock for player (MayEdit first)
	bool TryLock(int player, out string why)
	{
		if (!MayEdit(player, why))
			return false;
		m_iEditor = player;
		m_iLockAt = System.GetTickCount();
		Touch();
		Replication.BumpMe();
		Print(string.Format("MUD build area: %1 manager opened by player %2", m_iId, player));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server
	void Unlock(int player)
	{
		if (m_iEditor != player)
			return;
		m_iEditor = 0;
		Replication.BumpMe();
		Print(string.Format("MUD build area: %1 manager closed by player %2", m_iId, player));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the lock holder is still there
	void KeepAlive(int player)
	{
		if (m_iEditor == player)
			m_iLockAt = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a dig or an edit: the idle clock starts again
	void Touch()
	{
		m_iTouchAt = System.GetTickCount();
	}

	//------------------------------------------------------------------------------------------------
	bool HoldsLock(int player)
	{
		return m_iEditor != 0 && m_iEditor == player;
	}

	//------------------------------------------------------------------------------------------------
	int Id()
	{
		return m_iId;
	}

	//------------------------------------------------------------------------------------------------
	MUD_BuildQuad Quad()
	{
		return m_Quad;
	}

	//------------------------------------------------------------------------------------------------
	int Owner()
	{
		return m_iOwner;
	}

	//------------------------------------------------------------------------------------------------
	string OwnerName()
	{
		return m_sOwnerName;
	}

	//------------------------------------------------------------------------------------------------
	bool IsOpen()
	{
		return m_bOpen;
	}

	//------------------------------------------------------------------------------------------------
	bool IsDeleting()
	{
		return m_bDeleting;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: the live area with this id
	static MUD_BuildAreaComponent Find(int id)
	{
		foreach (MUD_BuildAreaComponent a : s_aAll)
		{
			if (a && !a.IsDeleting() && a.Id() == id)
				return a;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: the first live area holding (x, z) (edges grown by grow m); skip = an area id to leave out
	static MUD_BuildAreaComponent AreaAt(float x, float z, float grow = 0, int skip = 0)
	{
		foreach (MUD_BuildAreaComponent a : s_aAll)
		{
			if (!a || a.IsDeleting() || !a.Quad() || a.Id() == skip)
				continue;
			if (a.Quad().Inside(x, z, grow))
				return a;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: every live area except skip
	static void Others(int skip, array<MUD_BuildAreaComponent> outAreas)
	{
		outAreas.Clear();
		foreach (MUD_BuildAreaComponent a : s_aAll)
		{
			if (a && !a.IsDeleting() && a.Quad() && a.Id() != skip)
				outAreas.Insert(a);
		}
	}
}
