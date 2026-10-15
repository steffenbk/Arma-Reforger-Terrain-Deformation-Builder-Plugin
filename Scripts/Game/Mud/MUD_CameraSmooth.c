//! Camera ease after a snap (TEST 2026-10-11). A server correction teleports the driver's vehicle; the vehicle
//! cameras sit in the character's space, so the view jumps with it. Here a jump the velocity does not explain
//! is kept as a world offset that pulls the camera back to where it was and fades out over TAU. The vehicle
//! still snaps - only the view eases. Position only (angle snaps are not eased).
class MUD_CamSmooth
{
	static const bool ON = true;
	static const float SNAP_MIN = 0.03;     // m: smaller jumps are normal frame noise (accel x dt^2 is mm)
	static const float SNAP_MAX = 3;        // m: bigger = a teleport / respawn, not eased
	static const float TAU = 0.25;          // s: offset falls to 37 % in this time, ~5 % after 3 x TAU
	static const float OFFSET_MAX = 1.5;    // m: never hold the view farther than this from the vehicle

	protected IEntity m_Veh;
	protected vector m_vLastPos;
	protected vector m_vLastVel;
	protected vector m_vOffset;             // world space
	protected bool m_bHas;

	//------------------------------------------------------------------------------------------------
	//! After the vanilla OnUpdate. tm = the camera result (character space, or world-aligned when noParent).
	void Apply(IEntity veh, IEntity character, float dt, bool noParent, inout vector tm[4])
	{
		if (!ON || !veh || !character || dt <= 0)
			return;
		veh = veh.GetRootParent();
		Physics phys = veh.GetPhysics();
		vector pos = veh.GetOrigin();
		vector vel;
		if (phys)
			vel = phys.GetVelocity();
		if (veh != m_Veh || !m_bHas)
		{
			m_Veh = veh;
			m_bHas = true;
			m_vOffset = vector.Zero;
		}
		else
		{
			// where the vehicle should be now if nothing snapped (average of the two velocities)
			vector expect = m_vLastPos + (m_vLastVel + vel) * (0.5 * dt);
			vector jump = pos - expect;
			float len = jump.Length();
			float speed = vel.Length();
			// physics moves in fixed 60 Hz steps (gotchas 2026-10-11 11:45): a render frame can hold one
			// step more or less than expected, so one frame of travel is not a snap
			if (len > SNAP_MIN + speed * dt && len < SNAP_MAX)
			{
				m_vOffset = m_vOffset - jump;
				Print(string.Format("MUD cam: snap %1 m eased (speed %2 m/s)", len.ToString(-1, 3), speed.ToString(-1, 1)));
			}
			else if (len >= SNAP_MAX)
			{
				m_vOffset = vector.Zero;
			}
		}
		m_vLastPos = pos;
		m_vLastVel = vel;

		float olen = m_vOffset.Length();
		if (olen > OFFSET_MAX)
			m_vOffset = m_vOffset * (OFFSET_MAX / olen);
		m_vOffset = m_vOffset * Math.Pow(2.71828, -dt / TAU);
		if (m_vOffset.LengthSq() < 0.000001)
			return;

		vector add = m_vOffset;
		if (!noParent)
		{
			vector charMat[4];
			character.GetWorldTransform(charMat);
			add = m_vOffset.InvMultiply3(charMat);
		}
		tm[3] = tm[3] + add;
	}
}

//------------------------------------------------------------------------------------------------
modded class CharacterCamera3rdPersonVehicle
{
	protected ref MUD_CamSmooth m_MudSmooth = new MUD_CamSmooth();

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);
		m_MudSmooth.Apply(m_OwnerVehicle, m_OwnerCharacter, pDt, pOutResult.m_bNoParent, pOutResult.m_CameraTM);
	}
}

//------------------------------------------------------------------------------------------------
modded class CharacterCamera1stPersonVehicle
{
	protected ref MUD_CamSmooth m_MudSmooth = new MUD_CamSmooth();

	//------------------------------------------------------------------------------------------------
	override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);
		m_MudSmooth.Apply(m_OwnerVehicle, m_OwnerCharacter, pDt, pOutResult.m_bNoParent, pOutResult.m_CameraTM);
	}
}
