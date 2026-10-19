//! Build 2 Stage 5 (C7b, 2026-10-17): things on the ground follow a voxel op (crater 0, tunnel 1, trench 2, fill 3).
//! Called by MUD_VoxelProbe.GoLive on EVERY machine, once the op's new bricks (and bodies) are live:
//! - dynamic bodies over the op box (vehicles, loose physics props) are woken: they fall instead of hanging;
//! - mines and other items lying on the ground (InventoryItemComponent, no parent) move down with it, a fill lifts
//!   them back on top; an ARMED pressure mine under a crater goes off instead (server: TriggerManuallyServer, the
//!   vanilla replicated trigger; user 2026-10-17: crater = go off, trench / shovel / fill = move with the ground);
//! - static world props without an RplComponent (fences, crates, trees from the world file) whose whole footprint
//!   now lies > HIDE_GAP under them are hidden + collision off, and shown again (layers restored) once the ground is
//!   back (a fill). The state follows the ground NOW, not the op order: a joiner that replays a crater and its fill
//!   in one go-live ends like the server that saw them apart. Props WITH an RplComponent are left (streaming /
//!   GM-placed: not the same set on every machine) and counted in the log line.
//! Not followed: anything resting on a non-ground entity (house floor, table, bridge - down trace), anything with
//! its base > ON_GROUND over the undug ground (upstairs).
//! Ground height = MUD_VoxelProbe.TopAt: voxel data without the rut floor (J2: a joiner replays ops before the rut
//! snapshot), no ground traces (a DS's bricks may not be traceable, mud-gotchas 2026-10-09 20:38).
class MUD_GroundFollow
{
	static const float HIDE_GAP = 0.3;      // m: a static prop whose footprint dropped more than this is hidden
	static const float MOVE_GAP = 0.05;     // m: an item this far over (or under, fill) the new ground moves onto it
	static const float ON_GROUND = 1.0;     // m: base higher than this over the undug ground = never stood on it
	static const float REST_TRACE = 0.3;    // m: down trace from the base for a non-ground support
	static const float MAX_PROP = 20;       // m: bigger footprints are never hidden (power lines, walls of a site)
	static const float QUERY_Y = 60;        // m: the box query reaches this far above / below the undug ground
	static const float TRUNK_R = 0.5;       // m: a stump is tested on its trunk (origin +- this), not its world box
	static const int WHY_MAX = 40;
	static const int WATCH_MS = 1000;       // standing trees over a hole: rechecked this often for the stump model
	static const int WATCH_MAX = 500;       // most watched trees (oldest dropped)          // TEST_KIT: 'why' lines per op (floating stumps left, 2026-10-17)

	protected static ref MUD_GroundFollow s_Inst;
	protected ref array<IEntity> m_aFound = {};
	protected ref array<IEntity> m_aWatch = {};              // standing trees with a floating trunk (weak)
	protected bool m_bWatching;
	protected ref array<IEntity> m_aHid = {};                // props this hid (weak: a deleted one reads null)
	protected ref array<ref array<int>> m_aHidLayers = {};   // per hidden prop: body layer, then each geom's layer
	protected ref TraceParam m_Trace = new TraceParam();
	protected IEntity m_Self;                                // RestsOnOther: the entity tracing
	protected int m_iWoken;
	protected int m_iHidden;
	protected int m_iShown;
	protected int m_iRplLeft;
	protected int m_iPoint;
	protected int m_iMoved;
	protected int m_iFired;
	protected int m_iWhy;
	protected float m_fTop;                                  // Floating: highest new ground under the footprint
	protected vector m_vLo;                                  // TEST_KIT: op box, for the child 'why' lines
	protected vector m_vHi;

	//------------------------------------------------------------------------------------------------
	//! Any machine, GoLive: the op of kind (0-3) with x, z box lo / hi went live. lift: fill only, how much the
	//! ground rose (m) - an item up to that far under the new ground is lifted onto it.
	static void Changed(vector lo, vector hi, int kind, float lift)
	{
		if (!s_Inst)
			s_Inst = new MUD_GroundFollow();
		s_Inst.Run(lo, hi, kind, lift);
	}

	//------------------------------------------------------------------------------------------------
	//! A tree in its last destruction phase = the stump is left: its model is the last phase's m_sPhaseModel
	//! (t_picea_abies_3f.et: 2 phases, last = *_dst_base.xob). The model swap runs on every machine (vanilla
	//! GoToDamagePhase, streamed / JIP too). SCR_DestructibleEntity is an engine class: no mod hook (compile 2026-10-17).
	static bool IsStump(IEntity e)
	{
		if (!BaseTree.Cast(e))
			return false;
		SCR_DestructibleEntityClass prefabData = SCR_DestructibleEntityClass.Cast(e.GetPrefabData());
		VObject vo = e.GetVObject();
		if (!prefabData || !vo)
			return false;
		int n = prefabData.GetNumDestructionPhases();
		if (n < 2)
			return false;
		SCR_BaseDestructionPhase phase = SCR_BaseDestructionPhase.Cast(prefabData.GetDestructionPhase(n - 1));
		if (!phase)
			return false;
		ResourceName model;
		string remap;
		if (!phase.GetPhaseModelAndRemap(model, remap))
			return false;
		string name = FilePath.StripExtension(FilePath.StripPath(model));
		return name != "" && vo.GetResourceName().Contains(name);
	}

	//------------------------------------------------------------------------------------------------
	//! Watched standing trees (a blast destroys the tree after the op went live; a joiner streams the stump state
	//! later): a stump -> Prop (hides it if still over the hole); off the list when deleted or the hole is filled
	protected static void WatchTickS()
	{
		if (s_Inst)
			s_Inst.WatchTick();
	}

	//------------------------------------------------------------------------------------------------
	protected void WatchTick()
	{
		m_iHidden = 0;
		m_iWhy = 0;
		for (int i = m_aWatch.Count() - 1; i >= 0; i--)
		{
			IEntity e = m_aWatch[i];
			if (!e || e.IsDeleted())
			{
				m_aWatch.Remove(i);
				continue;
			}
			float base;
			bool floating;
			if (!Floating(e, e.GetOrigin(), base, floating) || !floating)
			{
				m_aWatch.Remove(i);
				continue;
			}
			if (!IsStump(e))
				continue;
			m_aWatch.Remove(i);
			int was = m_iHidden;
			Prop(e);
			if (m_iHidden > was)
				Print(string.Format("MUD ground follow: stump %1 at %2 is over a hole - hidden", PrefabOf(e), e.GetOrigin()), LogLevel.NORMAL);
		}
		if (m_aWatch.IsEmpty() && m_bWatching)
		{
			GetGame().GetCallqueue().Remove(WatchTickS);
			m_bWatching = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Run(vector lo, vector hi, int kind, float lift)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		int t0 = System.GetTickCount();
		float cy;
		if (!MUD_WorldGroundEntity.UndugY((lo[0] + hi[0]) * 0.5, (lo[2] + hi[2]) * 0.5, cy))
			return;
		m_iWoken = 0;
		m_iHidden = 0;
		m_iShown = 0;
		m_iRplLeft = 0;
		m_iPoint = 0;
		m_iMoved = 0;
		m_iFired = 0;
		m_iWhy = 0;
		m_vLo = lo;
		m_vHi = hi;
		ShowBack(lo, hi);
		vector qlo = Vector(lo[0], cy - QUERY_Y, lo[2]);
		vector qhi = Vector(hi[0], cy + QUERY_Y, hi[2]);
		m_aFound.Clear();
		world.QueryEntitiesByAABB(qlo, qhi, Add, null, EQueryEntitiesFlags.ALL);
		bool server = Replication.IsServer();
		foreach (IEntity e : m_aFound)
		{
			if (!e || e.IsDeleted())
				continue;
			Physics ph = e.GetPhysics();
			if (ph && ph.IsDynamic())
			{
				ph.SetActive(ActiveState.ACTIVE);
				m_iWoken++;
				continue;
			}
			if (Vehicle.Cast(e))
				continue;     // a proxy's body is not dynamic: the server's wake moves it
			if (e.FindComponent(InventoryItemComponent))
			{
				Item(e, kind, lift, server);
				continue;
			}
			Prop(e);
		}
		m_aFound.Clear();
		if (m_iWoken + m_iHidden + m_iShown + m_iRplLeft + m_iMoved + m_iFired > 0)
			Print(string.Format("MUD ground follow: op kind %1 box %2 .. %3 - %4 woken, %5 props hidden, %6 shown again, %7 floating props left (RplComponent), %8 point-bounds props",
				kind, lo, hi, m_iWoken, m_iHidden, m_iShown, m_iRplLeft, m_iPoint) + string.Format(", %1 items moved, %2 mines set off (%3 ms)", m_iMoved, m_iFired, System.GetTickCount(t0)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Query: root entities that are not our ground, terrain, decals, buildings (Stage 4 skipped, user 2026-10-17)
	//! or characters
	protected bool Add(IEntity e)
	{
		if (e && e.GetParent() && MUD_TestKit.TEST_KIT && e.GetVObject() && !ParticleEffectEntity.Cast(e) && !MUD_Surface.IsOurs(e))
			WhyChild(e);
		if (!e || e.GetParent() || MUD_Surface.IsOurs(e) || Building.Cast(e) || ChimeraCharacter.Cast(e))
			return true;
		if (GenericTerrainEntity.Cast(e) || DecalEntity.Cast(e))
			return true;
		// the blast's own effects (dirt throw, smoke) sit on the ground the crater drops: hiding them killed the
		// vanilla dirt-throw (WB 2026-10-17: '1 props hidden' on every crater)
		if (ParticleEffectEntity.Cast(e) || PrefabOf(e).Contains("Debris"))
			return true;     // + flying blast debris (Explosion_Debris_Dirt.et, WB 2026-10-17)
		m_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! An item on the ground (mine, dropped gear): an armed pressure mine under a crater goes off (server); else it
	//! moves onto the new ground when the ground dropped under it (any op) or rose over it by up to lift (fill)
	protected void Item(IEntity e, int kind, float lift, bool server)
	{
		vector o = e.GetOrigin();
		float y;
		if (!MUD_VoxelProbe.TopAt(o[0], o[2], y) || !NearGround(o) || RestsOnOther(e, o))
			return;
		bool down = o[1] - y > MOVE_GAP;
		bool up = kind == 3 && y - o[1] > MOVE_GAP && y - o[1] <= lift + MOVE_GAP;
		if (!down && !up)
			return;
		SCR_PressureTriggerComponent trig = SCR_PressureTriggerComponent.Cast(e.FindComponent(SCR_PressureTriggerComponent));
		if (kind == 0 && down && trig && trig.IsActivated())
		{
			// the server sets it off (vanilla RPC_DoTrigger broadcast); clients leave it where it is until it goes
			if (server)
			{
				trig.TriggerManuallyServer();
				m_iFired++;
				Print(string.Format("MUD ground follow: armed mine %1 at %2 under a crater (ground now %3) - set off", e.ClassName(), o, y), LogLevel.NORMAL);
			}
			return;
		}
		o[1] = y;
		e.SetOrigin(o);
		e.Update();
		m_iMoved++;
	}

	//------------------------------------------------------------------------------------------------
	//! A static world prop: hidden + collision off when the ground under its whole footprint (origin and the 4
	//! bottom corners of its world box) dropped more than HIDE_GAP under its base. Trees: only a destroyed one (the
	//! stump is left), tested on its trunk (user 2026-10-17: standing trees stay).
	protected void Prop(IEntity e)
	{
		if (!e.GetPhysics() && !e.GetVObject())
			return;     // nothing to see or touch (logic, sound, trigger entities)
		if (m_aHid.Find(e) >= 0)
			return;     // ShowBack decides it
		vector o = e.GetOrigin();
		float base;
		bool floating;
		if (BaseTree.Cast(e) && !IsStump(e))
		{
			// a standing tree stays (user 2026-10-17); watched in case the blast destroys it after this op
			if (m_aWatch.Find(e) < 0 && Floating(e, o, base, floating) && floating)
			{
				if (m_aWatch.Count() >= WATCH_MAX)
					m_aWatch.RemoveOrdered(0);
				m_aWatch.Insert(e);
				if (!m_bWatching)
				{
					GetGame().GetCallqueue().CallLater(WatchTickS, WATCH_MS, true);
					m_bWatching = true;
				}
			}
			return;
		}
		m_fTop = 0;
		if (!Floating(e, o, base, floating))
		{
			Why(e, base, "too big or a footprint corner on unconverted ground");
			return;
		}
		if (!floating)
		{
			if (base - m_fTop > 0.05)
				Why(e, base, "not floating (gap <= HIDE_GAP at the highest footprint point)");
			return;
		}
		if (!NearGround(Vector(o[0], base, o[2])))
		{
			Why(e, base, "base > ON_GROUND over the undug ground");
			return;
		}
		if (RestsOnOther(e, Vector(o[0], base, o[2])))
		{
			Why(e, base, "rests on " + m_Trace.TraceEnt.ClassName());
			return;
		}
		if (e.FindComponent(RplComponent))
		{
			Why(e, base, "has RplComponent");
			m_iRplLeft++;
			return;
		}
		int flags = e.GetFlags();
		if ((flags & EntityFlags.VISIBLE) == 0)
		{
			Why(e, base, "already hidden by someone else");
			return;
		}
		array<int> layers = {};
		Physics ph = e.GetPhysics();
		if (ph)
		{
			layers.Insert(ph.GetInteractionLayer());
			int n = ph.GetNumGeoms();
			for (int g = 0; g < n; g++)
				layers.Insert(ph.GetGeomInteractionLayer(g));
			ph.SetInteractionLayer(0);
			for (int g2 = 0; g2 < n; g2++)
				ph.SetGeomInteractionLayer(g2, 0);
		}
		e.ClearFlags(EntityFlags.VISIBLE, false);
		m_aHid.Insert(e);
		m_aHidLayers.Insert(layers);
		m_iHidden++;
		if (MUD_TestKit.TEST_KIT)
			Print(string.Format("MUD ground follow: hid %1 %2 at %3", e.ClassName(), PrefabOf(e), o), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Props this hid with their origin in the x, z box: shown again (layers back) when no longer floating.
	//! Deleted ones are dropped (backwards: RemoveOrdered keeps the two arrays paired).
	protected void ShowBack(vector lo, vector hi)
	{
		for (int i = m_aHid.Count() - 1; i >= 0; i--)
		{
			IEntity e = m_aHid[i];
			if (!e || e.IsDeleted())
			{
				m_aHid.RemoveOrdered(i);
				m_aHidLayers.RemoveOrdered(i);
				continue;
			}
			vector o = e.GetOrigin();
			if (o[0] < lo[0] || o[0] > hi[0] || o[2] < lo[2] || o[2] > hi[2])
				continue;
			float base;
			bool floating;
			if (Floating(e, o, base, floating) && floating)
				continue;
			array<int> layers = m_aHidLayers[i];
			Physics ph = e.GetPhysics();
			if (ph && layers.Count() > 0)
			{
				ph.SetInteractionLayer(layers[0]);
				int n = Math.Min(ph.GetNumGeoms(), layers.Count() - 1);
				for (int g = 0; g < n; g++)
					ph.SetGeomInteractionLayer(g, layers[g + 1]);
			}
			e.SetFlags(EntityFlags.VISIBLE, false);
			m_aHid.RemoveOrdered(i);
			m_aHidLayers.RemoveOrdered(i);
			m_iShown++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! floating = the ground under the whole footprint (origin + the 4 bottom corners of the world box; origin only
	//! when the box is a point, counted) is more than HIDE_GAP under the base. False = too big, or a footprint point
	//! is on ground no op converted (carried there).
	protected bool Floating(IEntity e, vector o, out float base, out bool floating)
	{
		floating = false;
		vector mins;
		vector maxs;
		bool point;
		base = o[1];
		if (BaseTree.Cast(e))
		{
			// the world box of a stump is roots + leftovers (3 m wide, 4 m under the origin, WB 2026-10-17): use the trunk
			mins = Vector(o[0] - TRUNK_R, o[1], o[2] - TRUNK_R);
			maxs = Vector(o[0] + TRUNK_R, o[1], o[2] + TRUNK_R);
		}
		else
		{
			e.GetWorldBounds(mins, maxs);
			if (maxs[0] - mins[0] > MAX_PROP || maxs[2] - mins[2] > MAX_PROP)
				return false;
			point = maxs[0] - mins[0] < 0.05 && maxs[2] - mins[2] < 0.05;
			if (point)
				m_iPoint++;
			else
				base = Math.Max(o[1], mins[1]);
		}
		float top = -100000;
		int pts = 5;
		if (point)
			pts = 1;
		for (int q = 0; q < pts; q++)
		{
			float x = o[0];
			float z = o[2];
			if (q > 0)
			{
				x = mins[0];
				z = mins[2];
				if (q == 2 || q == 4)
					x = maxs[0];
				if (q >= 3)
					z = maxs[2];
			}
			float y;
			if (!MUD_VoxelProbe.TopAt(x, z, y))
				return false;
			top = Math.Max(top, y);
		}
		floating = base - top > HIDE_GAP;
		m_fTop = top;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! TEST_KIT: why a prop over the op was left (floating stumps, 2026-10-17). Origin y, bounds bottom, base,
	//! highest new ground under the footprint, undug ground at the origin.
	protected void Why(IEntity e, float base, string why)
	{
		if (!MUD_TestKit.TEST_KIT || m_iWhy >= WHY_MAX || ParticleEffectEntity.Cast(e))
			return;
		vector o = e.GetOrigin();
		vector mins;
		vector maxs;
		e.GetWorldBounds(mins, maxs);
		if (!BaseTree.Cast(e) && maxs[0] - mins[0] < 0.3 && maxs[2] - mins[2] < 0.3)
			return;     // leaves, debris
		m_iWhy++;
		float undug = 0;
		MUD_WorldGroundEntity.UndugY(o[0], o[2], undug);
		Print(string.Format("MUD ground follow why: %1 %2 at %3 - %4 | origin y %5, bounds y %6 .. %7, base %8, top %9",
			e.ClassName(), PrefabOf(e), o, why, o[1], mins[1], maxs[1], base, m_fTop) + string.Format(", undug %1, size %2 x %3", undug, maxs[0] - mins[0], maxs[2] - mins[2]), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! TEST_KIT: a child entity (skipped by Add) with its origin in the op box: logged with its root
	protected void WhyChild(IEntity e)
	{
		vector o = e.GetOrigin();
		if (o[0] < m_vLo[0] || o[0] > m_vHi[0] || o[2] < m_vLo[2] || o[2] > m_vHi[2])
			return;
		IEntity root = e.GetRootParent();
		m_fTop = 0;
		MUD_VoxelProbe.TopAt(o[0], o[2], m_fTop);
		Why(e, o[1], "CHILD of " + root.ClassName() + " " + PrefabOf(root) + " (Add skips children)");
	}

	//------------------------------------------------------------------------------------------------
	protected static string PrefabOf(IEntity e)
	{
		EntityPrefabData pd = e.GetPrefabData();
		if (pd && pd.GetPrefabName() != "")
			return FilePath.StripPath(pd.GetPrefabName());
		return "(no prefab)";
	}

	//------------------------------------------------------------------------------------------------
	//! p (a base point) is no more than ON_GROUND over the undug ground: it stood on the ground, not upstairs
	protected bool NearGround(vector p)
	{
		float undug;
		if (!MUD_WorldGroundEntity.UndugY(p[0], p[2], undug))
			return false;
		return p[1] - undug <= ON_GROUND;
	}

	//------------------------------------------------------------------------------------------------
	//! A down trace from just over base p hits something that is not our ground (house floor, table, bridge)
	protected bool RestsOnOther(IEntity e, vector p)
	{
		BaseWorld world = e.GetWorld();
		m_Self = e;
		m_Trace.Start = p + Vector(0, 0.05, 0);
		m_Trace.End = p - Vector(0, REST_TRACE, 0);
		m_Trace.Flags = TraceFlags.ENTS;     // no WORLD: the hidden vanilla terrain stays at the old ground y (WB 2026-10-17: stumps 'rest on' it)
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.Exclude = e;
		m_Trace.TraceEnt = null;
		float frac = world.TraceMove(m_Trace, NotGround);
		m_Self = null;
		return frac < 1 && m_Trace.TraceEnt != null && NotGround(m_Trace.TraceEnt);
	}

	//------------------------------------------------------------------------------------------------
	protected bool NotGround(notnull IEntity e)
	{
		return e != m_Self && !MUD_Surface.IsOurs(e) && !GenericTerrainEntity.Cast(e);
	}
}

