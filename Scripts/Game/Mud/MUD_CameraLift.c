//! LOOK LIFT camera (user 2026-10-23: 'the camera can go under the layer in 3rd person'). The engine keeps the 3rd
//! person camera out of COLLIDERS; the look lift is drawn up to MUD_LookLift base + GROW over our ground collider, so
//! a low camera sat inside the drawn layer. After the vanilla OnUpdate this walks the camera's back line (pivot ->
//! back along last frame's view direction, m_fDistance long) and shortens m_fDistance to the last point that stays
//! CLEAR over the drawn ground (our ground + MUD_LookLift.DrawnLift). Shorter = at once, longer = eased (EASE m/s),
//! as a wall would. On foot and the vehicle 3rd person camera (v323). Client look only.
class MUD_CameraLift
{
	static const bool ON = true;
	static const float CLEAR = 0.15;        // m the camera stays over the drawn ground
	static const int STEPS = 8;             // samples along the back line
	static const float SKIP_M = 0.6;        // both ends this far over the drawn top: nothing to do (most frames)
	static const float EASE = 2;            // m/s the distance grows back
	static const float IN_HOLE = 0.3;       // m dug under the pivot = in a crater / trench: no clamp
	static const float BAND_BELOW = 0.05;   // m under the collider ground still counted as the lift band
	protected static float s_fNextLog;      // TEST_KIT 'OUR clamp' line: world time (ms) of the next one

	//------------------------------------------------------------------------------------------------
	//! Drawn ground y at world x, z (false off the grid)
	static bool DrawnY(float x, float z, out float y)
	{
		float dug;
		float undug;
		if (!MUD_WorldGroundEntity.DugUndug(x, z, dug, undug))
			return false;
		y = dug + MUD_LookLift.DrawnLift(x, z);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance (m, <= want) the camera may go back from pivot along back before it dips into the drawn layer
	static float Allowed(vector pivot, vector back, float want)
	{
		float top = MUD_LookLift.MaxBase() + MUD_LookLift.GROW_MAX + CLEAR + SKIP_M;
		vector farP = pivot + back * want;
		float yp;
		float yf;
		if (!DrawnY(pivot[0], pivot[2], yp) || !DrawnY(farP[0], farP[2], yf))
			return want;
		if (pivot[1] - yp > top && farP[1] - yf > top)
			return want;
		// standing in a dug hole (crater, trench): the camera looks back up a slope of real ground - leave it to the
		// engine (user 2026-10-23: still zoomed in when entering a crater)
		float pDug;
		float pUndug;
		if (MUD_WorldGroundEntity.DugUndug(pivot[0], pivot[2], pDug, pUndug) && pUndug - pDug > IN_HOLE)
			return want;
		for (int i = 1; i <= STEPS; i++)
		{
			float t = want * i / STEPS;
			vector pt = pivot + back * t;
			// only the LIFT BAND is ours: from the collider ground up to the drawn top. Below the collider (trench /
			// crater walls, tunnel roofs) the engine's own camera solver handles it (user 2026-10-23 live test: our
			// clamp zoomed in inside trenches, craters and tunnels).
			float dug;
			float undug;
			if (!MUD_WorldGroundEntity.DugUndug(pt[0], pt[2], dug, undug))
				continue;
			float lift = MUD_LookLift.DrawnLift(pt[0], pt[2]);
			if (lift < 0.01 || pt[1] < dug - BAND_BELOW)
				continue;
			if (pt[1] < dug + lift + CLEAR)
				return want * (i - 1) / STEPS;
		}
		return want;
	}

	//------------------------------------------------------------------------------------------------
	//! Camera distance for this frame: want cut to Allowed along last frame's view line, grown back at EASE.
	//! last = the camera's own memory (-1 = none yet), updated here.
	static float Clamp(IEntity character, vector pivot, float want, float dt, inout float last)
	{
		vector cam[4];
		character.GetWorld().GetCurrentCamera(cam);     // last frame's view: its direction is the back line
		float allowed = Allowed(pivot, -cam[2], want);
		if (MUD_TestKit.TEST_KIT && allowed < want - 0.2)
		{
			// WHO ZOOMS (user 2026-10-23: 'still camera zooming in trenches and craters'): a zoom with no line here is
			// the engine's own collision solver (real trench / crater walls), not this clamp
			float now = character.GetWorld().GetWorldTime();
			if (now >= s_fNextLog || now < s_fNextLog - 1000)     // second test: a new play starts the clock again
			{
				s_fNextLog = now + 1000;
				float dug;
				float undug;
				MUD_WorldGroundEntity.DugUndug(pivot[0], pivot[2], dug, undug);
				Print(string.Format("MUD cam: OUR clamp %1 -> %2 m | pivot %3 m over our ground, dug %4 m, drawn lift there %5 m",
					want, allowed, pivot[1] - dug, undug - dug, MUD_LookLift.DrawnLift(pivot[0], pivot[2])), LogLevel.NORMAL);
			}
		}
		if (last >= 0 && allowed > last)
			allowed = Math.Min(allowed, last + EASE * dt);
		last = allowed;
		return allowed;
	}
}

//------------------------------------------------------------------------------------------------
modded class CharacterCamera3rdPersonBase
{
	protected float m_fMudCamDist = -1;     // MUD_CameraLift: distance used last frame (-1 = none yet)

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);
		if (!MUD_CameraLift.ON || !MUD_LookLift.ENABLED || !m_OwnerCharacter || m_OwnerCharacter.IsInVehicle())
			return;
		float want = pOutResult.m_fDistance;
		if (want <= 0 || pOutResult.m_fPositionModelSpace < 0.5)
			return;
		vector pivot = m_OwnerCharacter.CoordToParent(pOutResult.m_CameraTM[3]);
		pOutResult.m_fDistance = MUD_CameraLift.Clamp(m_OwnerCharacter, pivot, want, pDt, m_fMudCamDist);
	}
}

//------------------------------------------------------------------------------------------------
//! Vehicle 3rd person (user shot 2026-10-23: peeking under the layer from the vehicle camera). Pivot as vanilla
//! builds it: character space, or world-aligned round the character with m_bNoParent. Runs after the other modded
//! OnUpdate (MUD_CameraSmooth) in whichever order the engine chains them - both only touch their own fields.
modded class CharacterCamera3rdPersonVehicle
{
	protected float m_fMudLiftDist = -1;    // MUD_CameraLift: distance used last frame (-1 = none yet)

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);
		if (!MUD_CameraLift.ON || !MUD_LookLift.ENABLED || !m_OwnerCharacter || !pOutResult.m_bAllowCollisionSolver)
			return;
		float want = pOutResult.m_fDistance;
		if (want <= 0)
			return;
		vector pivot;
		if (pOutResult.m_bNoParent)
			pivot = m_OwnerCharacter.GetOrigin() + pOutResult.m_CameraTM[3];
		else
			pivot = m_OwnerCharacter.CoordToParent(pOutResult.m_CameraTM[3]);
		pOutResult.m_fDistance = MUD_CameraLift.Clamp(m_OwnerCharacter, pivot, want, pDt, m_fMudLiftDist);
	}
}
