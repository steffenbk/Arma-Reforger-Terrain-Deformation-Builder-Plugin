//! VEHICLE BONE PROBE (TEST_KIT, 2026-10-23, user: 'could we deform vehicles by bones - use a vanilla vehicle, quick
//! test'). Question: does a script SetBoneMatrix on a VANILLA vehicle's bones show, and does it survive the vehicle's
//! own animation (wheels / doors / suspension run every frame)? Our bone tiles have no animation, so they never had
//! this problem. Look only, local, nothing synced. Ticked from MUD_BoneProbe.Tick.
//! Key (diag exes only): numpad + cycles on the vehicle you sit in, else the nearest MUD_Wheels vehicle within 30 m
//! of the camera:
//!   OFF -> EVERY FRAME (bones pushed each frame) -> ONCE (pushed one time, then left alone: does it stick?) -> OFF.
//! Bones pushed: every bone whose name has door / hood / bonnet / trunk / body / hatch / mirror in it; if none, the
//! first 4 bones that are not wheels. Push = DENT_M sideways + down in model space (same bone-axes maths as the tiles).
//! Log: 'MUD veh bone: ...' - all bone names once, the picked bones, sets / fails every 5 s.
//! COLLIDER TEST (4th press, 2026-10-23, user: 'can we make a test with it, could be worth it'): does a script
//! SetBoneMatrix move the vehicle's COLLIDER too, or only the look? (IDA: colliders can follow animation as rigid
//! per-bone shapes, AnimatedColliderComponent - unknown for script bone sets.) Per test bone (v_body + door bones, max
//! COL_MAX): trace straight down onto the bone from COL_LIFT + 1.5 m above (sees only this vehicle), lift the bone
//! COL_LIFT m up (model up), wait COL_WAIT frames, trace again, put it back. Collider follows = the hit rises by
//! ~COL_LIFT. 'MUD veh col: <bone> rest hit h0, lifted hit h1, rise r -> FOLLOWS / LOOK ONLY / NO HIT'.
class MUD_VehBoneProbe
{
	static const float NEAR = 30;
	static const float DENT_M = 0.15;
	static const int MODE_OFF = 0;
	static const int MODE_FRAME = 1;
	static const int MODE_ONCE = 2;
	static const int MODE_COL = 3;
	static const float COL_LIFT = 2.5;
	static const int COL_WAIT = 10;
	static const int COL_MAX = 4;
	static const float NO_HIT = -100000;

	protected static int s_iMode;
	protected static IEntity s_Veh;
	protected static ref array<int> s_aIds;
	protected static ref array<string> s_aNames;
	protected static ref array<vector> s_aAx0;
	protected static ref array<vector> s_aAx1;
	protected static ref array<vector> s_aAx2;
	protected static float s_fNextStat;
	protected static int s_iSets;
	protected static int s_iFails;
	protected static int s_iColBone;        // index into s_aIds being tested
	protected static int s_iColFrame;
	protected static float s_fColRest;      // rest hit height (world y), NO_HIT = no hit
	protected static int s_iColFollow;
	protected static int s_iColLook;
	protected static ref TraceParam s_ColTrace;
	protected static vector s_vColAt;       // rest bone position (world) under test

	//------------------------------------------------------------------------------------------------
	static void Tick(float time)
	{
#ifdef ENABLE_DIAG
		if (Debug.KeyState(KeyCode.KC_ADD))
		{
			Debug.ClearKey(KeyCode.KC_ADD);
			s_iMode = (s_iMode + 1) % 4;
			if (s_iMode == MODE_FRAME)
			{
				if (!Pick())
					s_iMode = MODE_OFF;
			}
			else if (s_iMode == MODE_ONCE)
			{
				Push();
			}
			else if (s_iMode == MODE_COL)
			{
				ColStart();
			}
			else
			{
				Reset();
			}
			string txt = "OFF (bones back to rest)";
			if (s_iMode == MODE_FRAME)
				txt = "EVERY FRAME";
			else if (s_iMode == MODE_ONCE)
				txt = "ONCE (pushed one time, now left alone)";
			else if (s_iMode == MODE_COL)
				txt = "COLLIDER TEST (bones lifted one by one, watch the log)";
			int n;
			if (s_aIds)
				n = s_aIds.Count();
			Print(string.Format("MUD veh bone: mode %1 | %2 bones on %3", txt, n, s_Veh), LogLevel.NORMAL);
			SCR_HintManagerComponent.ShowCustomHint(string.Format("%1\n%2 bones (numpad +)", txt, n), "Vehicle bone probe", 6, true);
		}
#endif
		if (s_iMode == MODE_FRAME)
			Push();
		if (s_iMode == MODE_COL)
			ColStep();
		if (s_iMode != MODE_OFF && s_iMode != MODE_COL && time >= s_fNextStat)
		{
			s_fNextStat = time + 5;
			Print(string.Format("MUD veh bone: 5 s | mode %1, bone sets %2, set fails %3", s_iMode, s_iSets, s_iFails), LogLevel.NORMAL);
			s_iSets = 0;
			s_iFails = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Find the vehicle and its bones; false = nothing to test
	protected static bool Pick()
	{
		s_Veh = null;
		ChimeraCharacter ch = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (ch && ch.IsInVehicle())
		{
			CompartmentAccessComponent ca = ch.GetCompartmentAccessComponent();
			if (ca && ca.GetCompartment())
				s_Veh = ca.GetCompartment().GetVehicle();
		}
		if (!s_Veh)
		{
			vector cam[4];
			GetGame().GetWorld().GetCurrentCamera(cam);
			float best = NEAR * NEAR;
			foreach (Vehicle veh : MUD_Wheels.GetAll())
			{
				if (!veh)
					continue;
				float d2 = vector.DistanceSq(veh.GetOrigin(), cam[3]);
				if (d2 < best)
				{
					best = d2;
					s_Veh = veh;
				}
			}
		}
		if (!s_Veh)
		{
			Print("MUD veh bone: no vehicle (sit in one or stand within 30 m)", LogLevel.WARNING);
			return false;
		}
		Animation anim = s_Veh.GetAnimation();
		if (!anim)
		{
			Print(string.Format("MUD veh bone: %1 has no Animation (no skeleton) - nothing to bend", s_Veh), LogLevel.WARNING);
			return false;
		}
		array<string> names = {};
		anim.GetBoneNames(names);
		string all = "";
		foreach (string bn : names)
		{
			all += bn + " ";
		}
		Print(string.Format("MUD veh bone: %1 has %2 bones: %3", s_Veh, names.Count(), all), LogLevel.NORMAL);
		s_aIds = {};
		s_aNames = {};
		s_aAx0 = {};
		s_aAx1 = {};
		s_aAx2 = {};
		array<string> want = {"door", "hood", "bonnet", "trunk", "body", "hatch", "mirror"};
		foreach (string bn2 : names)
		{
			string low = bn2;
			low.ToLower();
			foreach (string w : want)
			{
				if (low.Contains(w))
				{
					AddBone(anim, bn2);
					break;
				}
			}
		}
		if (s_aIds.IsEmpty())
		{
			foreach (string bn3 : names)
			{
				string low3 = bn3;
				low3.ToLower();
				if (!low3.Contains("wheel") && s_aIds.Count() < 4)
					AddBone(anim, bn3);
			}
		}
		string picked = "";
		foreach (string pn : s_aNames)
		{
			picked += pn + " ";
		}
		Print(string.Format("MUD veh bone: pushing %1 bones: %2", s_aIds.Count(), picked), LogLevel.NORMAL);
		return !s_aIds.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddBone(Animation anim, string name)
	{
		int id = anim.GetBoneIndex(name);
		vector m[4];
		if (id < 0 || !anim.GetBoneMatrix(id, m))
			return;
		s_aIds.Insert(id);
		s_aNames.Insert(name);
		s_aAx0.Insert(m[0]);
		s_aAx1.Insert(m[1]);
		s_aAx2.Insert(m[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Push every picked bone DENT_M sideways + down (model space, written in the bone's rest axes as the tiles)
	protected static void Push()
	{
		Set(Vector(DENT_M, -DENT_M, 0));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Reset()
	{
		Set(vector.Zero);
		s_Veh = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Collider test: keep v_body + door bones of the picked ones (max COL_MAX), bones back to rest
	protected static void ColStart()
	{
		Set(vector.Zero);
		if (!s_Veh && !Pick())
			return;
		for (int b = s_aIds.Count() - 1; b >= 0; b--)
		{
			string low = s_aNames[b];
			low.ToLower();
			if (!low.Contains("body") && !low.Contains("door"))
				RemoveBone(b);
		}
		while (s_aIds.Count() > COL_MAX)
			RemoveBone(s_aIds.Count() - 1);
		s_iColBone = 0;
		s_iColFrame = 0;
		s_iColFollow = 0;
		s_iColLook = 0;
		string list = "";
		foreach (string n : s_aNames)
		{
			list += n + " ";
		}
		Print(string.Format("MUD veh col: start on %1, %2 bones: %3(lift %4 m, wait %5 frames)", s_Veh, s_aIds.Count(), list, COL_LIFT, COL_WAIT), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RemoveBone(int b)
	{
		s_aIds.Remove(b);
		s_aNames.Remove(b);
		s_aAx0.Remove(b);
		s_aAx1.Remove(b);
		s_aAx2.Remove(b);
	}

	//------------------------------------------------------------------------------------------------
	//! One step a frame: frame 0 rest trace + lift, frame COL_WAIT lifted trace + back to rest, then the next bone
	protected static void ColStep()
	{
		if (!s_Veh || s_Veh.IsDeleted() || !s_aIds || s_iColBone >= s_aIds.Count())
		{
			if (s_aIds)
				Print(string.Format("MUD veh col: done - %1 of %2 bones FOLLOW, %3 LOOK ONLY", s_iColFollow, s_aIds.Count(), s_iColLook), LogLevel.NORMAL);
			SCR_HintManagerComponent.ShowCustomHint(string.Format("collider test done: %1 follow, %2 look only", s_iColFollow, s_iColLook), "Vehicle bone probe", 6, true);
			s_iMode = MODE_OFF;
			Reset();
			return;
		}
		Animation anim = s_Veh.GetAnimation();
		if (!anim)
		{
			s_iColBone = s_aIds.Count();
			return;
		}
		Physics phys = s_Veh.GetPhysics();
		if (phys && !phys.IsActive())
			phys.SetActive(ActiveState.ACTIVE);
		if (s_iColFrame == 0)
		{
			s_fColRest = ColTrace(anim);
			SetOne(anim, s_iColBone, s_Veh.VectorToLocal(Vector(0, COL_LIFT, 0)));
		}
		else if (s_iColFrame >= COL_WAIT)
		{
			float lifted = ColTrace(anim);
			string verdict = "NO HIT (trace saw nothing either time)";
			float rise = 0;
			if (lifted > NO_HIT + 1 && s_fColRest > NO_HIT + 1)
			{
				rise = lifted - s_fColRest;
				if (rise > COL_LIFT * 0.5)
				{
					verdict = "FOLLOWS (collider moved with the bone)";
					s_iColFollow++;
				}
				else
				{
					verdict = "LOOK ONLY (collider stayed)";
					s_iColLook++;
				}
			}
			else if (s_fColRest > NO_HIT + 1)
			{
				verdict = "LOOK ONLY (lifted trace hit nothing)";
				s_iColLook++;
			}
			Print(string.Format("MUD veh col: %1 rest hit %2, lifted hit %3, rise %4 m -> %5", s_aNames[s_iColBone], s_fColRest, lifted, rise, verdict), LogLevel.NORMAL);
			SetOne(anim, s_iColBone, vector.Zero);
			s_iColBone++;
			s_iColFrame = -5;     // a few frames at rest before the next bone
			return;
		}
		s_iColFrame++;
	}

	//------------------------------------------------------------------------------------------------
	//! World y of the first hit straight down onto the REST position of the bone under test (only this vehicle);
	//! NO_HIT = no hit
	protected static float ColTrace(Animation anim)
	{
		// same spot both times: the rest position, taken at frame 0 (the vehicle stands still meanwhile)
		if (s_iColFrame == 0)
		{
			vector m[4];
			if (!anim.GetBoneMatrix(s_aIds[s_iColBone], m))
				return NO_HIT;
			s_vColAt = s_Veh.CoordToParent(m[3]);
		}
		vector wp = s_vColAt;
		if (!s_ColTrace)
			s_ColTrace = new TraceParam();
		s_ColTrace.Include = s_Veh;
		s_ColTrace.Flags = TraceFlags.ENTS;
		s_ColTrace.LayerMask = EPhysicsLayerPresets.Projectile;
		s_ColTrace.Start = wp + Vector(0, COL_LIFT + 1.5, 0);
		s_ColTrace.End = wp - Vector(0, 1, 0);
		float f = GetGame().GetWorld().TraceMove(s_ColTrace, null);
		if (f >= 1)
			return NO_HIT;
		return s_ColTrace.Start[1] - (COL_LIFT + 2.5) * f;
	}

	//------------------------------------------------------------------------------------------------
	//! One bone at off (model space, in the bone's rest axes as Set)
	protected static void SetOne(Animation anim, int b, vector off)
	{
		vector m[4];
		Math3D.MatrixIdentity4(m);
		m[3] = Vector(vector.Dot(off, s_aAx0[b]), vector.Dot(off, s_aAx1[b]), vector.Dot(off, s_aAx2[b]));
		if (!anim.SetBoneMatrix(s_Veh, s_aIds[b], m))
			Print(string.Format("MUD veh col: SetBoneMatrix failed on %1", s_aNames[b]), LogLevel.WARNING);
		s_Veh.Update();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Set(vector off)
	{
		if (!s_Veh || !s_aIds || s_Veh.IsDeleted())
			return;
		Animation anim = s_Veh.GetAnimation();
		if (!anim)
			return;
		for (int b = 0; b < s_aIds.Count(); b++)
		{
			vector a0 = s_aAx0[b];
			vector a1 = s_aAx1[b];
			vector a2 = s_aAx2[b];
			vector m[4];
			Math3D.MatrixIdentity4(m);
			m[3] = Vector(vector.Dot(off, a0), vector.Dot(off, a1), vector.Dot(off, a2));
			s_iSets++;
			if (!anim.SetBoneMatrix(s_Veh, s_aIds[b], m))
				s_iFails++;
		}
		s_Veh.Update();
	}
}
