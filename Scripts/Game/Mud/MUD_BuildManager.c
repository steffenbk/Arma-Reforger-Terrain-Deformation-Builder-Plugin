//! Trench builder F4 (spec sec 3): the manager. A script camera looks straight down over a build area. Round 2
//! (user 2026-10-24: "locked to the position and very floaty - use the mouse cursor, Paint-like shapes"): the mouse
//! is a free cursor (context flag CursorVisible), the camera only moves on WASD / screen edge / middle-mouse grab /
//! wheel zoom toward the cursor, at a fixed height over the area (no bobbing over hills). A palette at the top
//! (MUD_BuildPalette) picks the tool or a trench shape (MUD_BuildShapes): click = point, drag = shape / pencil.
//! Tools draw parts on the real ground; nothing is sent until F (Save). The player's body stays where it stood and
//! is NOT safe (user 2026-10-24): the manager closes when it is hurt, killed, moved or put in a vehicle. Local
//! player only; the server holds the area's edit lock meanwhile.
//! Keys: Configs/System/chimeraInputCommon.conf, MUD_BuildManagerContext.

//------------------------------------------------------------------------------------------------
class MUD_BuildCameraClass : SCR_CameraBaseClass
{
}

//! The manager's top-down camera (no input of its own: MUD_BuildManager moves it every frame)
class MUD_BuildCamera : SCR_CameraBase
{
}

//------------------------------------------------------------------------------------------------
//! One unsaved change: a new part (kind + text) or a delete (m_iDelete = the saved part's uid)
class MUD_BuildPending
{
	int m_iKind;
	string m_sText;
	int m_iDelete;
	int m_iGroup;                               // parts added by one action (a shape's lines, dugout + entry) undo together
	ref MUD_BuildSpec m_Spec;
	ref MUD_TrenchGhosts m_Ghost;
}

//------------------------------------------------------------------------------------------------
class MUD_BuildManager
{
	static const string CONTEXT = "MUD_BuildManagerContext";
	static const int T_TRENCH = 0;
	static const int T_STEP = 1;
	static const int T_COVER = 2;
	static const int T_TUNNEL = 3;
	static const int T_DUGOUT = 4;
	static const int T_DELETE = 5;
	static const int TOOLS = 6;
	static const float H_MIN = 8;               // m over the ground under the camera
	static const float H_MAX = 220;
	static const float H_START = 45;
	static const float ZOOM_STEP = 1.15;        // per wheel notch
	static const float PAN_KEYS = 0.8;          // m per s per m of height (WASD, screen edge)
	static const float FOV_V = 60;              // deg, SetVerticalFOV: the cursor ray is worked out from it
	static const float EDGE_PX = 4;             // reference px: the mouse this near the screen edge pans
	static const float CLICK_PX = 6;            // reference px: a press + release moved less than this is a click
	static const float ROT_STEP = 15;           // deg per R (box shapes)
	static const float CAM_CLEAR = 3;           // m the camera stays over the ground under it at least
	static const float ATTACH_END_R = 3;        // m: auto-attach puts a trench point this near a trench END on that end
	static const float ATTACH_LINE_R = 2;       // m: ... else this near a trench's centre line on the line (junction)
	// settings panel rows (MUD_BuildRow ids)
	static const int B_WIDTH = 1;
	static const int B_DEPTH = 2;
	static const int B_WALL = 3;
	static const int B_ATTACH = 4;
	static const int B_SQUARE = 5;
	static const int B_TURN = 6;
	static const int B_BEND = 7;
	static const int B_FINISH = 8;
	static const int B_SIDE = 9;
	static const int B_SIZE = 10;
	static const int B_UNDO = 11;
	static const int B_SAVE = 12;
	static const int B_DIGALL = 13;
	static const int B_CLOSE = 14;
	static const int B_STEPW = 15;
	static const int B_WHOLE = 16;
	static const int B_AUTOSTEP = 17;
	static const int B_PLAT = 18;
	static const int B_FLIP = 19;
	static const float STAMP_REACH = 3;        // m past a trench's top edge that a foxhole / MG nest / ramp still attaches
	static const float STAIR_L = 0.75;         // m tread of an entrance ramp (one 0.25 m layer up each)
	static const float LINK_L = 1.5;           // m of ground between a trench wall and an attached foxhole / nest
	static const float NEST_R = 1.75;          // m MG nest ring (centre line)
	static const float FOXHOLE_L = 2;          // m foxhole (2 men side by side)
	static const float STEP_MIN = 1.5;         // m: shortest firing step stretch
	static const float TUNNEL_ATTACH_R = 1.5;  // m past a trench's top edge that a tunnel end still attaches
	static const float TUNNEL_LEAD = 2;        // m of straight tunnel square to the wall at an attached mouth
	static const float PICK_R = 3;              // m: a part this near the cursor is picked
	static const float BODY_MOVE = 2;           // m the body may be moved before the manager closes
	static const float TUNNEL_D_MIN = 1;
	static const float TUNNEL_D_MAX = 6;
	static const float TUNNEL_D_START = 2;

	protected static float s_fViewH;            // camera height while open, 0 = closed (outline width)

	protected int m_iArea;
	protected MUD_BuildCamera m_Cam;
	protected CameraBase m_PrevCam;
	protected vector m_vCam;                    // camera xz (y 0)
	protected float m_fH = H_START;             // m over m_fBaseY
	protected float m_fBaseY;                   // highest ground of the area (corners, middle, body): camera floor
	protected vector m_vAim;                    // cursor on the ground, snapped
	protected vector m_vAimRaw;                 // cursor on the ground, not snapped
	protected int m_iMouseX;                    // native px
	protected int m_iMouseY;
	protected ref MUD_BuildPalette m_Palette = new MUD_BuildPalette();
	protected int m_iHover = -1;                // palette item under the mouse
	protected int m_iShape = MUD_BuildShapes.POINTS;
	protected float m_fShapeAngle;              // deg, box shapes' frame (R)
	protected bool m_bDown;                     // LMB held, pressed over the ground
	protected bool m_bIgnoreHeld;               // LMB held but used up (palette press, drag cancelled): wait for release
	protected bool m_bDragging;                 // the held LMB moved past CLICK_PX
	protected int m_iDownX;
	protected int m_iDownY;
	protected vector m_vDown;                   // ground point of the press (trench: SnapTrench)
	protected ref array<vector> m_aPencil = {}; // PENCIL: points drawn so far
	protected bool m_bGrab;                     // middle mouse held: the ground under the cursor sticks to it
	protected bool m_bAttach = true;            // panel check box: trench ends snap onto other trenches (clean joints)
	protected bool m_bSquare;                   // panel check box: as Shift held (square boxes, 15 deg lines)
	protected bool m_bBendArm;                  // panel "Bend": the next ground click picks the segment
	protected ref array<ref MUD_BuildRow> m_aRows = {};
	protected int m_iRowHover = -1;             // HitRow code under the mouse
	protected ref MUD_BuildDraw m_PendDraw = new MUD_BuildDraw();   // top edges of unsaved trenches (redrawn on change)
	protected bool m_bPendDirty;
	protected bool m_bMouseMoved;               // edge pan waits for a real mouse position (it may read 0, 0 at open)
	protected int m_iOpenX;
	protected int m_iOpenY;
	protected int m_iGroupNext;                 // undo groups: one shape / one dugout = one RMB
	protected int m_iGroupOpen;                 // > 0: AddPending puts new parts in this group
	protected vector m_vGrab;
	protected vector m_vDrawnAim;
	protected vector m_vBody;
	protected float m_fHealth;
	protected bool m_bMove;
	protected bool m_bView;
	protected bool m_bWeapon;
	protected int m_iTool;
	protected ref MUD_TrenchPlan m_Draft = new MUD_TrenchPlan();
	protected ref MUD_TrenchGhosts m_DraftGhost = new MUD_TrenchGhosts();
	protected ref array<vector> m_aPts = {};    // TUNNEL path / DUGOUT door
	protected int m_iPick;                      // COVER: uid of the trench picked for the start (0 = none)
	protected float m_fCoverS0;
	protected int m_iStepSide = 1;             // side of the stretch being drawn (from the start click)
	protected int m_iStepWidth = 0;            // WidthOf index: 0.5 / 1.0 m platform
	protected bool m_bStepWhole;               // panel: one click = a step the whole trench long
	protected MUD_TrenchLayout m_StepLay;      // stretch start set on this trench (weak: gone with its part)
	protected float m_fStepS0;
	protected int m_iStepLayers = 5;           // platform depth under the ground: 1.25 m = chest over the edge
	protected int m_iAutoStep;                 // trench tool: firing step 0 off, 1 left, 2 right, 3 both sides
	protected bool m_bFlip;                    // zigzag / fire bays / arc: the other side (R while dragging)
	protected int m_iLastGroup;                // undo group of the last shape / stamp placed (R turns it while newest)
	protected bool m_bLastStamp;
	protected int m_iLastShape;
	protected vector m_vLastS;
	protected vector m_vLastE;
	protected float m_fLastAngle;
	protected bool m_bLastSquare;
	protected bool m_bLastFlip;
	protected vector m_vLastFacing;
	protected int m_iDugSize = 1;
	protected float m_fTunnelD = TUNNEL_D_START;
	protected int m_iBending = -1;
	protected float m_fBendFrom;
	protected ref array<ref MUD_BuildPending> m_aPending = {};
	protected ref MUD_BuildDraw m_Draw = new MUD_BuildDraw();
	protected bool m_bDirty = true;
	protected bool m_bFinished;
	protected bool m_bListening;
	protected bool m_bCloseAsked;
	protected string m_sNote;
	protected string m_sShownHint;
	protected string m_sWhyClosed;
	protected float m_fRangeSet;

	//------------------------------------------------------------------------------------------------
	static float ViewHeight()
	{
		return s_fViewH;
	}

	//------------------------------------------------------------------------------------------------
	void ~MUD_BuildManager()
	{
		RemoveListeners();
	}

	//------------------------------------------------------------------------------------------------
	int AreaId()
	{
		return m_iArea;
	}

	//------------------------------------------------------------------------------------------------
	string WhyClosed()
	{
		return m_sWhyClosed;
	}

	//------------------------------------------------------------------------------------------------
	//! Camera over area, body controls off, keys on. False = could not open.
	bool Start(int areaId)
	{
		MUD_BuildAreaComponent area = MUD_BuildAreaComponent.Find(areaId);
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		InputManager input = GetGame().GetInputManager();
		CameraManager cams = GetGame().GetCameraManager();
		if (!area || !area.Quad() || !player || !input || !cams)
			return false;
		m_iArea = areaId;
		m_vBody = player.GetOrigin();
		m_vCam = Vector(m_vBody[0], 0, m_vBody[2]);
		if (!area.Quad().Inside(m_vCam[0], m_vCam[2], MUD_Build.LINK_REACH))
			m_vCam = area.Quad().Centre();
		m_fBaseY = m_vBody[1];
		vector centre = area.Quad().Centre();
		m_fBaseY = Math.Max(m_fBaseY, MUD_BuildDraw.GroundY(centre[0], centre[2]));
		foreach (vector corner : area.Quad().m_aC)
		{
			m_fBaseY = Math.Max(m_fBaseY, MUD_BuildDraw.GroundY(corner[0], corner[2]));
		}
		DamageManagerComponent dm = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
		if (dm)
			m_fHealth = dm.GetHealth();

		vector mat[4];
		CamMatrix(mat);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = mat[3];
		m_Cam = MUD_BuildCamera.Cast(GetGame().SpawnEntity(MUD_BuildCamera, GetGame().GetWorld(), params));
		if (!m_Cam)
		{
			Print("MUD build manager: camera spawn failed", LogLevel.ERROR);
			return false;
		}
		m_Cam.SetVerticalFOV(60);
		m_Cam.SetNearPlane(0.3);
		m_Cam.SetFarPlane(2500);
		m_PrevCam = cams.CurrentCamera();
		cams.SetCamera(m_Cam);

		CharacterControllerComponent cc = player.GetCharacterController();
		if (cc)
		{
			m_bMove = cc.GetDisableMovementControls();
			m_bView = cc.GetDisableViewControls();
			m_bWeapon = cc.GetDisableWeaponControls();
			cc.SetDisableMovementControls(true);
			cc.SetDisableViewControls(true);
			cc.SetDisableWeaponControls(true);
		}

		WidgetManager.GetMousePos(m_iOpenX, m_iOpenY);
		if (!m_Palette.Open())
			Print("MUD build manager: no palette - Q / E still pick the tools", LogLevel.WARNING);
		input.AddActionListener("MUD_BuildRotate", EActionTrigger.DOWN, OnRotate);
		input.AddActionListener("MUD_TrenchUndo", EActionTrigger.DOWN, OnUndo);
		input.AddActionListener("MUD_TrenchWidth", EActionTrigger.DOWN, OnWidth);
		input.AddActionListener("MUD_TrenchWall", EActionTrigger.DOWN, OnWall);
		input.AddActionListener("MUD_TrenchBend", EActionTrigger.DOWN, OnBend);
		input.AddActionListener("MUD_TrenchConfirm", EActionTrigger.DOWN, OnSave);
		input.AddActionListener("MUD_BuildToolNext", EActionTrigger.DOWN, OnToolNext);
		input.AddActionListener("MUD_BuildToolPrev", EActionTrigger.DOWN, OnToolPrev);
		input.AddActionListener("MUD_BuildFinish", EActionTrigger.DOWN, OnFinish);
		input.AddActionListener("MUD_BuildClose", EActionTrigger.DOWN, OnClose);
		input.AddActionListener("MUD_BuildDeeper", EActionTrigger.DOWN, OnDeeper);
		input.AddActionListener("MUD_BuildShallower", EActionTrigger.DOWN, OnShallower);
		m_bListening = true;
		Print(string.Format("MUD build manager: open on area %1 at %2", m_iArea, m_vCam));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		RemoveListeners();
		m_Palette.Close();
		m_Draw.Clear();
		m_PendDraw.Clear();
		m_DraftGhost.Clear();
		m_aPending.Clear();
		CameraManager cams = GetGame().GetCameraManager();
		if (cams && m_PrevCam && cams.CurrentCamera() == m_Cam)
			cams.SetCamera(m_PrevCam);
		if (m_Cam)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Cam);
		m_Cam = null;
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (player && player.GetCharacterController())
		{
			CharacterControllerComponent cc = player.GetCharacterController();
			cc.SetDisableMovementControls(m_bMove);
			cc.SetDisableViewControls(m_bView);
			cc.SetDisableWeaponControls(m_bWeapon);
		}
		s_fViewH = 0;
		MUD_BuildPartComponent.SetGhostRange(0);
		SCR_HintManagerComponent.ClearLatestHint();
		Print(string.Format("MUD build manager: closed area %1 (%2)", m_iArea, m_sWhyClosed));
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveListeners()
	{
		if (!m_bListening)
			return;
		m_bListening = false;
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;
		input.RemoveActionListener("MUD_BuildRotate", EActionTrigger.DOWN, OnRotate);
		input.RemoveActionListener("MUD_TrenchUndo", EActionTrigger.DOWN, OnUndo);
		input.RemoveActionListener("MUD_TrenchWidth", EActionTrigger.DOWN, OnWidth);
		input.RemoveActionListener("MUD_TrenchWall", EActionTrigger.DOWN, OnWall);
		input.RemoveActionListener("MUD_TrenchBend", EActionTrigger.DOWN, OnBend);
		input.RemoveActionListener("MUD_TrenchConfirm", EActionTrigger.DOWN, OnSave);
		input.RemoveActionListener("MUD_BuildToolNext", EActionTrigger.DOWN, OnToolNext);
		input.RemoveActionListener("MUD_BuildToolPrev", EActionTrigger.DOWN, OnToolPrev);
		input.RemoveActionListener("MUD_BuildFinish", EActionTrigger.DOWN, OnFinish);
		input.RemoveActionListener("MUD_BuildClose", EActionTrigger.DOWN, OnClose);
		input.RemoveActionListener("MUD_BuildDeeper", EActionTrigger.DOWN, OnDeeper);
		input.RemoveActionListener("MUD_BuildShallower", EActionTrigger.DOWN, OnShallower);
	}

	//------------------------------------------------------------------------------------------------
	//! Camera height: m_fH over the area's highest ground, never closer than CAM_CLEAR to the ground under it
	protected float CamY()
	{
		return Math.Max(m_fBaseY + m_fH, MUD_BuildDraw.GroundY(m_vCam[0], m_vCam[2]) + CAM_CLEAR);
	}

	//------------------------------------------------------------------------------------------------
	//! Straight down from CamY; screen up = north (+z), screen right = east (+x)
	protected void CamMatrix(out vector mat[4])
	{
		mat[0] = vector.Right;
		mat[1] = vector.Forward;
		mat[2] = vector.Up * -1;
		mat[3] = Vector(m_vCam[0], CamY(), m_vCam[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Flat offset from the camera's xz to where the mouse ray meets height y (no lag: worked out from FOV_V,
	//! not from the last drawn frame)
	protected vector RayOffset(int mx, int my, float camY, float y)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		float w = ws.GetWidth();
		float h = ws.GetHeight();
		if (h < 1)
			return vector.Zero;
		float t = Math.Tan(FOV_V * 0.5 * Math.DEG2RAD) * Math.Max(0.5, camY - y);
		float u = (mx - w * 0.5) / (h * 0.5);
		float v = (my - h * 0.5) / (h * 0.5);
		return Vector(u * t, 0, -v * t);
	}

	//------------------------------------------------------------------------------------------------
	//! Ground under the mouse (3 rounds on the real ground; top-down rays settle fast)
	protected vector CursorGround(int mx, int my)
	{
		float camY = CamY();
		float y = MUD_BuildDraw.GroundY(m_vCam[0], m_vCam[2]);
		vector p = m_vCam;
		for (int i = 0; i < 3; i++)
		{
			p = m_vCam + RayOffset(mx, my, camY, y);
			y = MUD_BuildDraw.GroundY(p[0], p[2]);
		}
		p[1] = y;
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! Move the camera so the mouse ray meets ground point g again (grab pan, zoom toward the cursor)
	protected void KeepUnderMouse(vector g)
	{
		vector off = RayOffset(m_iMouseX, m_iMouseY, CamY(), g[1]);
		m_vCam[0] = g[0] - off[0];
		m_vCam[2] = g[2] - off[2];
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame. False = close (Backspace, area gone, body hurt / moved / dead, camera gone).
	bool Update(float timeSlice)
	{
		if (m_bFinished)
			return false;
		MUD_BuildAreaComponent area = MUD_BuildAreaComponent.Find(m_iArea);
		if (!area || !area.Quad())
			return Close("the area is gone");
		ChimeraCharacter player = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!player || player.IsInVehicle())
			return Close("no body / in a vehicle");
		if (vector.DistanceXZ(player.GetOrigin(), m_vBody) > BODY_MOVE)
			return Close("your body was moved");
		DamageManagerComponent dm = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
		if (dm && (dm.IsDestroyed() || dm.GetHealth() < m_fHealth - 0.01))
			return Close("your body was hit");
		if (!m_Cam)
			return Close("camera gone");

		InputManager input = GetGame().GetInputManager();
		WorkspaceWidget ws = GetGame().GetWorkspace();
		input.ActivateContext(CONTEXT);
		WidgetManager.GetMousePos(m_iMouseX, m_iMouseY);
		BuildRows();
		m_iHover = m_Palette.HitTest(m_iMouseX, m_iMouseY);
		m_iRowHover = m_Palette.HitRow(m_iMouseX, m_iMouseY, m_aRows);
		bool overPalette = m_Palette.Over(m_iMouseX, m_iMouseY, m_aRows);

		// zoom toward the cursor: the ground point under it stays under it
		float wheel = input.GetActionValue("MUD_TrenchDepth");
		if (wheel != 0 && !overPalette)
		{
			vector before = CursorGround(m_iMouseX, m_iMouseY);
			if (wheel > 0)
				m_fH = m_fH / ZOOM_STEP;
			else
				m_fH = m_fH * ZOOM_STEP;
			m_fH = Math.Clamp(m_fH, H_MIN, H_MAX);
			KeepUnderMouse(before);
		}
		s_fViewH = m_fH;
		float range = Math.Max(150, m_fH * 1.5 + 100);
		if (Math.AbsFloat(range - m_fRangeSet) > 20)
		{
			m_fRangeSet = range;
			MUD_BuildPartComponent.SetGhostRange(range);
		}

		// middle mouse grab: the ground follows the cursor exactly (no mouse-delta pan: that was the floaty part)
		bool grab = input.GetActionValue("MUD_BuildGrab") > 0.5;
		if (grab && !m_bGrab)
			m_vGrab = CursorGround(m_iMouseX, m_iMouseY);
		m_bGrab = grab;
		if (m_bGrab)
		{
			KeepUnderMouse(m_vGrab);
		}
		else
		{
			float dx = input.GetActionValue("MUD_BuildMoveR");
			float dz = input.GetActionValue("MUD_BuildMoveF");
			float edge = ws.DPIScale(EDGE_PX);
			if (m_iMouseX != m_iOpenX || m_iMouseY != m_iOpenY)
				m_bMouseMoved = true;
			bool inside = m_iMouseX >= 0 && m_iMouseY >= 0 && m_iMouseX < ws.GetWidth() && m_iMouseY < ws.GetHeight();
			if (!m_bMouseMoved || !inside)
				edge = -1;     // no edge pan
			if (m_iMouseX <= edge)
				dx = -1;
			else if (m_iMouseX >= ws.GetWidth() - 1 - edge)
				dx = 1;
			if (m_iMouseY <= edge)
				dz = 1;
			else if (m_iMouseY >= ws.GetHeight() - 1 - edge)
				dz = -1;
			m_vCam[0] = m_vCam[0] + dx * PAN_KEYS * m_fH * timeSlice;
			m_vCam[2] = m_vCam[2] + dz * PAN_KEYS * m_fH * timeSlice;
		}
		ClampCam(area.Quad());

		vector mat[4];
		CamMatrix(mat);
		m_Cam.SetWorldTransform(mat);

		m_vAimRaw = CursorGround(m_iMouseX, m_iMouseY);
		m_vAim = MUD_WorldGroundEntity.SnapFine(m_vAimRaw);
		m_vAim[1] = MUD_BuildDraw.GroundY(m_vAim[0], m_vAim[2]);
		MouseButton(input, overPalette);
		if (vector.DistanceXZ(m_vAim, m_vDrawnAim) > Math.Max(0.05, m_fH * 0.003))
		{
			m_vDrawnAim = m_vAim;
			if (m_iBending >= 0)
				MoveBend();
			m_bDirty = true;
		}
		if (m_bDirty)
		{
			m_bDirty = false;
			Redraw();
		}
		if (m_bPendDirty)
		{
			m_bPendDirty = false;
			DrawPendingEdges();
		}
		m_Palette.Draw(m_iTool, m_iShape, m_iHover, m_aRows, m_iRowHover);
		UpdateHint(area);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! LMB, polled (a click filter cannot see a drag): press over the palette picks, a short press on the ground
	//! is the tool's click, a drag draws the trench shape / pencil line
	protected void MouseButton(InputManager input, bool overPalette)
	{
		bool held = input.GetActionValue("MUD_BuildDraw") > 0.5;
		bool wasHeld = m_bDown || m_bIgnoreHeld;
		if (held && !wasHeld)
		{
			if (overPalette)
			{
				m_bIgnoreHeld = true;
				if (m_iRowHover >= 0)
					OnRow(m_iRowHover / 4, m_iRowHover % 4);
				else
					PalettePick(m_iHover);
				return;
			}
			m_bDown = true;
			m_bDragging = false;
			m_iDownX = m_iMouseX;
			m_iDownY = m_iMouseY;
			m_vDown = m_vAim;
			if (m_iTool == T_TRENCH)
				m_vDown = SnapTrench(m_vAim);
			m_aPencil.Clear();
			m_aPencil.Insert(m_vAimRaw);
			return;
		}
		if (held)
		{
			if (!m_bDown)
				return;
			float move = Math.Sqrt((m_iMouseX - m_iDownX) * (m_iMouseX - m_iDownX) + (m_iMouseY - m_iDownY) * (m_iMouseY - m_iDownY));
			if (!m_bDragging && move > GetGame().GetWorkspace().DPIScale(CLICK_PX) && DragTool())
			{
				m_bDragging = true;
				m_bDirty = true;
			}
			if (m_bDragging && m_iShape == MUD_BuildShapes.PENCIL && vector.DistanceXZ(m_aPencil[m_aPencil.Count() - 1], m_vAimRaw) >= MUD_BuildShapes.PENCIL_STEP)
			{
				m_aPencil.Insert(m_vAimRaw);
				m_bDirty = true;
			}
			return;
		}
		// released
		bool wasDown = m_bDown;
		bool dragged = m_bDragging;
		m_bDown = false;
		m_bIgnoreHeld = false;
		m_bDragging = false;
		if (!wasDown)
			return;
		if (dragged)
			FinishDrag();
		else if (DragTool() && MUD_BuildShapes.IsStamp(m_iShape))
			PlaceStamp(m_iShape, m_vAim, StampFacing(), false);
		else if (DragTool())
			SetNote(string.Format("%1: hold LMB and drag (Points draws by clicking)", MUD_BuildShapes.Name(m_iShape)));
		else
			OnPoint();
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! The settings panel for the tool on now (every setting the keys change, as clicks; keys still work)
	protected void BuildRows()
	{
		m_aRows.Clear();
		if (m_iTool == T_TRENCH)
		{
			m_aRows.Insert(new MUD_BuildRow(B_WIDTH, MUD_BuildRow.CYCLE, "Width", string.Format("%1 m", m_Draft.Width())));
			m_aRows.Insert(new MUD_BuildRow(B_DEPTH, MUD_BuildRow.STEP, "Depth", string.Format("%1 m", m_Draft.m_iLayers * MUD_TrenchPlan.LAYER)));
			m_aRows.Insert(new MUD_BuildRow(B_WALL, MUD_BuildRow.CYCLE, "Walls", string.Format("%1 deg", MUD_TrenchPlan.WallDegOf(m_Draft.m_iWall))));
			m_aRows.Insert(new MUD_BuildRow(B_ATTACH, MUD_BuildRow.CHECK, "Auto-attach to trenches", "", m_bAttach));
			array<string> sides = {"off", "left side", "right side", "both sides"};
			m_aRows.Insert(new MUD_BuildRow(B_AUTOSTEP, MUD_BuildRow.CYCLE, "Firing step (platform)", sides[m_iAutoStep]));
			if (m_iAutoStep != 0 || m_iShape == MUD_BuildShapes.MG_NEST)
				m_aRows.Insert(new MUD_BuildRow(B_PLAT, MUD_BuildRow.STEP, "Platform depth", string.Format("%1 m", m_iStepLayers * MUD_TrenchPlan.LAYER)));
			if (MUD_BuildShapes.IsStamp(m_iShape))
			{
				m_aRows.Insert(new MUD_BuildRow(B_FLIP, MUD_BuildRow.BUTTON, "Turn / other side (R)"));
			}
			else if (MUD_BuildShapes.CanFlip(m_iShape))
			{
				m_aRows.Insert(new MUD_BuildRow(B_FLIP, MUD_BuildRow.BUTTON, "Flip side (R)"));
			}
			else if (MUD_BuildShapes.IsBox(m_iShape))
			{
				m_aRows.Insert(new MUD_BuildRow(B_SQUARE, MUD_BuildRow.CHECK, "Square (or hold Shift)", "", m_bSquare));
				m_aRows.Insert(new MUD_BuildRow(B_TURN, MUD_BuildRow.BUTTON, "Turn 15 deg", string.Format("%1 deg", m_fShapeAngle)));
			}
			else if (m_iShape == MUD_BuildShapes.POINTS)
			{
				string bend = "";
				if (m_bBendArm)
					bend = "click a segment";
				else if (m_iBending >= 0)
					bend = "click to keep";
				m_aRows.Insert(new MUD_BuildRow(B_BEND, MUD_BuildRow.BUTTON, "Bend a segment", bend));
				m_aRows.Insert(new MUD_BuildRow(B_FINISH, MUD_BuildRow.BUTTON, "Finish line", string.Format("%1 points", m_Draft.m_aPoints.Count())));
			}
			else if (m_iShape != MUD_BuildShapes.PENCIL)
			{
				m_aRows.Insert(new MUD_BuildRow(B_SQUARE, MUD_BuildRow.CHECK, "15 deg steps (or hold Shift)", "", m_bSquare));
			}
		}
		else if (m_iTool == T_STEP)
		{
			string up = "";
			MUD_TrenchLayout hl;
			float hs;
			int hside;
			if (StepHover(hl, hs, hside))
				up = string.Format(", %1 m up", (hl.Layers() - m_iStepLayers) * MUD_TrenchPlan.LAYER);
			m_aRows.Insert(new MUD_BuildRow(B_DEPTH, MUD_BuildRow.STEP, "Platform depth (1.25 = chest over the edge)", string.Format("%1 m%2", m_iStepLayers * MUD_TrenchPlan.LAYER, up)));
			m_aRows.Insert(new MUD_BuildRow(B_STEPW, MUD_BuildRow.CYCLE, "Platform width", string.Format("%1 m", MUD_TrenchPlan.WidthOf(m_iStepWidth))));
			m_aRows.Insert(new MUD_BuildRow(B_WHOLE, MUD_BuildRow.CHECK, "Whole trench (one click)", "", m_bStepWhole));
		}
		else if (m_iTool == T_TUNNEL)
		{
			m_aRows.Insert(new MUD_BuildRow(B_DEPTH, MUD_BuildRow.STEP, "Depth (free ends)", string.Format("%1 m", m_fTunnelD)));
			m_aRows.Insert(new MUD_BuildRow(B_ATTACH, MUD_BuildRow.CHECK, "Auto-attach to trench walls", "", m_bAttach));
			if (MUD_BuildShapes.IsBox(m_iShape))
			{
				m_aRows.Insert(new MUD_BuildRow(B_SQUARE, MUD_BuildRow.CHECK, "Square (or hold Shift)", "", m_bSquare));
				m_aRows.Insert(new MUD_BuildRow(B_TURN, MUD_BuildRow.BUTTON, "Turn 15 deg", string.Format("%1 deg", m_fShapeAngle)));
			}
			else if (m_iShape == MUD_BuildShapes.POINTS)
			{
				m_aRows.Insert(new MUD_BuildRow(B_FINISH, MUD_BuildRow.BUTTON, "Finish tunnel", string.Format("%1 points", m_aPts.Count())));
			}
		}
		else if (m_iTool == T_DUGOUT)
		{
			m_aRows.Insert(new MUD_BuildRow(B_SIZE, MUD_BuildRow.CYCLE, "Room", string.Format("%1 x %2 m", MUD_Build.DugWidth(m_iDugSize), MUD_Build.DugLength(m_iDugSize))));
		}
		m_aRows.Insert(new MUD_BuildRow(B_UNDO, MUD_BuildRow.BUTTON, "Undo"));
		m_aRows.Insert(new MUD_BuildRow(B_SAVE, MUD_BuildRow.BUTTON, "Save", string.Format("%1 changes", m_aPending.Count())));
		if (MUD_TrenchSiteComponent.ALLOW_INSTANT_DIG)
			m_aRows.Insert(new MUD_BuildRow(B_DIGALL, MUD_BuildRow.BUTTON, "Save + dig all (test)"));
		string close = "";
		if (m_bCloseAsked)
			close = "again = drop changes";
		m_aRows.Insert(new MUD_BuildRow(B_CLOSE, MUD_BuildRow.BUTTON, "Close", close));
	}

	//------------------------------------------------------------------------------------------------
	//! A click on the settings panel: the same functions the keys call
	protected void OnRow(int id, int zone)
	{
		if (id == B_WIDTH || id == B_SIDE || id == B_SIZE)
			OnWidth();
		else if (id == B_DEPTH && zone == MUD_BuildPalette.ZONE_PLUS)
			Depth(1);
		else if (id == B_DEPTH && zone == MUD_BuildPalette.ZONE_MINUS)
			Depth(-1);
		else if (id == B_WALL)
			OnWall();
		else if (id == B_AUTOSTEP)
			m_iAutoStep = (m_iAutoStep + 1) % 4;
		else if (id == B_PLAT && zone == MUD_BuildPalette.ZONE_PLUS)
			m_iStepLayers = Math.ClampInt(m_iStepLayers + 1, MUD_TrenchPlan.MIN_LAYERS, MUD_TrenchPlan.MAX_LAYERS - 1);
		else if (id == B_PLAT && zone == MUD_BuildPalette.ZONE_MINUS)
			m_iStepLayers = Math.ClampInt(m_iStepLayers - 1, MUD_TrenchPlan.MIN_LAYERS, MUD_TrenchPlan.MAX_LAYERS - 1);
		else if (id == B_FLIP)
			OnRotate();
		else if (id == B_STEPW)
			m_iStepWidth = 1 - m_iStepWidth;
		else if (id == B_WHOLE)
			m_bStepWhole = !m_bStepWhole;
		else if (id == B_ATTACH)
			m_bAttach = !m_bAttach;
		else if (id == B_SQUARE)
			m_bSquare = !m_bSquare;
		else if (id == B_TURN)
			OnRotate();
		else if (id == B_BEND)
			BendButton();
		else if (id == B_FINISH)
			OnFinish();
		else if (id == B_UNDO)
			OnUndo();
		else if (id == B_SAVE)
			Save(false);
		else if (id == B_DIGALL)
			Save(true);
		else if (id == B_CLOSE)
			OnClose();
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Panel "Bend": while bending = keep; else the next ground click picks the segment
	protected void BendButton()
	{
		if (m_iBending >= 0)
		{
			m_iBending = -1;
			return;
		}
		m_bBendArm = !m_bBendArm;
		if (m_bBendArm)
			SetNote("Click the middle of a draft segment, move the mouse to the side, click to keep");
	}

	//------------------------------------------------------------------------------------------------
	//! The trench tool with a drag shape (everything but Points)
	protected bool DragTool()
	{
		return (m_iTool == T_TRENCH || m_iTool == T_TUNNEL) && m_iShape != MUD_BuildShapes.POINTS;
	}

	//------------------------------------------------------------------------------------------------
	protected void PalettePick(int item)
	{
		if (item < 0)
			return;
		if (item < TOOLS)
		{
			m_iTool = item;
		}
		else
		{
			if (m_iTool != T_TUNNEL || MUD_BuildShapes.IsStamp(item - TOOLS))
				m_iTool = T_TRENCH;     // stamps are trench parts     // shapes draw trenches, or tunnels while the tunnel tool is on
			m_iShape = item - TOOLS;
		}
		ResetTool();
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	//! The strokes the current drag would make (end = the cursor)
	protected void DragStrokes(array<ref MUD_BuildStroke> strokes)
	{
		if (m_iShape == MUD_BuildShapes.PENCIL)
		{
			array<vector> drawn = {};
			drawn.Copy(m_aPencil);
			if (m_iTool == T_TRENCH)
				drawn.Insert(SnapTrench(m_vAimRaw));
			else
				drawn.Insert(m_vAimRaw);
			drawn[0] = m_vDown;
			MUD_BuildShapes.Pencil(drawn, strokes);
			return;
		}
		StrokesFor(m_iShape, m_vDown, DragEnd(), m_fShapeAngle, DragSquare(), m_bFlip, strokes);
	}

	//------------------------------------------------------------------------------------------------
	protected vector DragEnd()
	{
		vector end = m_vAim;
		if (m_iTool == T_TRENCH)
			end = SnapTrench(m_vAim);
		if (vector.DistanceXZ(end, m_vDown) < MUD_TrenchPlan.MIN_SEGMENT)
			end = MUD_WorldGroundEntity.SnapFine(m_vAim);     // attach pulled it back onto the start
		return end;
	}

	//------------------------------------------------------------------------------------------------
	protected bool DragSquare()
	{
		return m_bSquare || GetGame().GetInputManager().GetActionValue("MUD_BuildShift") > 0.5;
	}

	//------------------------------------------------------------------------------------------------
	//! A shape's strokes. Box shapes started ON a trench turn with it (round 2d, user: "a T from the side points
	//! down"): their frame = the trench's direction there, plus the R angle.
	protected void StrokesFor(int shape, vector s, vector e, float angle, bool square, bool flip, array<ref MUD_BuildStroke> strokes)
	{
		float a = angle;
		if (MUD_BuildShapes.IsBox(shape))
			a = angle + BaseAngle(s);
		MUD_BuildShapes.Make(shape, s, e, a, square, strokes, flip);
	}

	//------------------------------------------------------------------------------------------------
	//! Direction (deg, MakeBox frame: from +x toward +z) of the trench line s lies on; 0 when on none / attach off
	protected float BaseAngle(vector s)
	{
		if (!m_bAttach)
			return 0;
		array<MUD_TrenchLayout> lays = {};
		TrenchLayouts(lays);
		foreach (MUD_TrenchLayout lay : lays)
		{
			float off;
			float at = lay.ChainPos(s[0], s[2], off);
			if (off > 0.1)
				continue;
			vector n = ChainNormal(lay, at);
			vector t = Vector(n[2], 0, -n[0]);     // Left(t) = n
			return Math.Atan2(t[2], t[0]) * Math.RAD2DEG;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Release after a drag: a stamp faces the drag; anything else becomes its unsaved parts (all or none)
	protected void FinishDrag()
	{
		if (MUD_BuildShapes.IsStamp(m_iShape))
		{
			vector f = m_vAim - m_vDown;
			f[1] = 0;
			if (f.Length() < 0.1)
				f = vector.Forward;
			PlaceStamp(m_iShape, m_vDown, f.Normalized(), false);
			return;
		}
		if (m_iShape == MUD_BuildShapes.PENCIL)
		{
			array<ref MUD_BuildStroke> strokes = {};
			DragStrokes(strokes);
			if (PlaceStrokes(strokes))
				m_iLastGroup = 0;     // a pencil line has nothing to turn
			return;
		}
		PlaceShape(m_iShape, m_vDown, DragEnd(), m_fShapeAngle, DragSquare(), m_bFlip);
	}

	//------------------------------------------------------------------------------------------------
	//! A drag shape as unsaved parts; remembered so R can turn / flip it while it is the last change
	protected bool PlaceShape(int shape, vector s, vector e, float angle, bool square, bool flip)
	{
		array<ref MUD_BuildStroke> strokes = {};
		StrokesFor(shape, s, e, angle, square, flip, strokes);
		if (!PlaceStrokes(strokes))
			return false;
		m_bLastStamp = false;
		m_iLastShape = shape;
		m_vLastS = s;
		m_vLastE = e;
		m_fLastAngle = angle;
		m_bLastSquare = square;
		m_bLastFlip = flip;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Strokes -> unsaved trench parts (+ auto firing steps) or tunnels, one undo group; m_iLastGroup = it
	protected bool PlaceStrokes(array<ref MUD_BuildStroke> strokes)
	{
		if (m_iTool == T_TUNNEL)
		{
			int tBefore = m_aPending.Count();
			FinishTunnelDrag(strokes);
			if (m_aPending.Count() == tBefore)
				return false;
			m_iLastGroup = m_aPending[m_aPending.Count() - 1].m_iGroup;
			return true;
		}
		array<string> texts = {};
		string why;
		if (!MUD_BuildShapes.ToPlans(strokes, m_Draft.m_iWidth, m_Draft.m_iLayers, m_Draft.m_iWall, texts, why))
		{
			SetNote(string.Format("%1: %2", MUD_BuildShapes.Name(m_iShape), why));
			return false;
		}
		int before = m_aPending.Count();
		int pieces = 0;
		m_iGroupOpen = NewGroup();
		foreach (string text : texts)
		{
			if (!AddPending(MUD_Build.TRENCH, text))
			{
				m_iGroupOpen = 0;
				PopFrom(before);
				return false;
			}
			pieces += m_aPending[m_aPending.Count() - 1].m_Spec.Count();
		}
		int steps = AutoSteps(texts);
		m_iLastGroup = m_iGroupOpen;
		m_iGroupOpen = 0;
		string turn = "";
		if (MUD_BuildShapes.IsBox(m_iShape))
			turn = ", R turns it";
		else if (MUD_BuildShapes.CanFlip(m_iShape))
			turn = ", R flips it";
		SetNote(string.Format("%1 added: %2 lines, %3 pieces, %4 firing steps - Save keeps it%5", MUD_BuildShapes.Name(m_iShape), texts.Count(), pieces, steps, turn));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void PopFrom(int before)
	{
		for (int i = m_aPending.Count() - 1; i >= before; i--)
		{
			PopPending(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Firing step setting on the trench tool (round 2d, user: "did not understand how to build the raised standing
	//! place"): every new trench line gets its platform on the chosen side(s) of its drawing direction, the whole
	//! length, in the open group. Count of step parts added.
	protected int AutoSteps(array<string> trenchTexts)
	{
		if (m_iAutoStep == 0)
			return 0;
		int added = 0;
		foreach (string text : trenchTexts)
		{
			MUD_TrenchLayout lay = new MUD_TrenchLayout();
			if (!lay.Build(MUD_TrenchPlan.Unpack(text)) || m_iStepLayers >= lay.Layers())
				continue;
			for (int k = 0; k < 2; k++)
			{
				int side = 1 - 2 * k;     // k 0: left (+1), k 1: right (-1)
				if ((k == 0 && m_iAutoStep == 2) || (k == 1 && m_iAutoStep == 1))
					continue;
				array<string> steps = {};
				string why;
				if (!StepTexts(lay, 0, lay.ChainLength(), side, steps, why))
					continue;
				foreach (string st : steps)
				{
					if (AddPending(MUD_Build.STEP, st))
						added++;
				}
			}
		}
		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! R: while dragging turns a box shape / flips a zigzag-bays-arc; else turns / flips the shape or stamp just
	//! placed (while it is still the last change); else turns the next shape's frame
	protected void OnRotate(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		m_bDirty = true;
		if (m_bDragging)
		{
			if (MUD_BuildShapes.CanFlip(m_iShape))
				m_bFlip = !m_bFlip;
			else
				TurnAngle();
			return;
		}
		if (m_iLastGroup != 0 && !m_aPending.IsEmpty() && m_aPending[m_aPending.Count() - 1].m_iGroup == m_iLastGroup)
		{
			while (!m_aPending.IsEmpty() && m_aPending[m_aPending.Count() - 1].m_iGroup == m_iLastGroup)
			{
				PopPending(m_aPending.Count() - 1);
			}
			if (m_bLastStamp)
			{
				float a = Math.Atan2(m_vLastFacing[0], m_vLastFacing[2]) + ROT_STEP * Math.DEG2RAD;
				PlaceStamp(m_iLastShape, m_vLastS, Vector(Math.Sin(a), 0, Math.Cos(a)), !m_bLastFlip);
				return;
			}
			float angle = m_fLastAngle;
			bool flip = m_bLastFlip;
			if (MUD_BuildShapes.CanFlip(m_iLastShape))
				flip = !flip;
			else
				angle = angle + ROT_STEP;
			if (!PlaceShape(m_iLastShape, m_vLastS, m_vLastE, angle, m_bLastSquare, flip))
				SetNote("That turn does not fit - RMB undoes the shape");
			return;
		}
		TurnAngle();
	}

	//------------------------------------------------------------------------------------------------
	protected void TurnAngle()
	{
		m_fShapeAngle = m_fShapeAngle + ROT_STEP;
		if (m_fShapeAngle >= 360)
			m_fShapeAngle = m_fShapeAngle - 360;
	}

	//------------------------------------------------------------------------------------------------
	//! Stamps (round 2d): the trench nearest p within its top width + STAMP_REACH, where along it
	protected bool StampAttach(vector p, out MUD_TrenchLayout lay, out float s)
	{
		if (!m_bAttach)
			return false;
		array<MUD_TrenchLayout> lays = {};
		TrenchLayouts(lays);
		float best = 1000000;
		foreach (MUD_TrenchLayout l : lays)
		{
			float off;
			float at = l.ChainPos(p[0], p[2], off);
			float reach = l.Half() + l.Layers() * MUD_TrenchPlan.LAYER * l.Run() + STAMP_REACH;
			if (off < reach && off < best)
			{
				best = off;
				lay = l;
				s = at;
			}
		}
		return lay != null;
	}

	//------------------------------------------------------------------------------------------------
	//! One straight 2-point plan a -> b
	protected static bool OnePlan(vector a, vector b, int width, int layers, int wall, array<string> texts, out string why)
	{
		MUD_BuildStroke st = new MUD_BuildStroke();
		st.Add(a);
		st.Add(b);
		array<ref MUD_BuildStroke> strokes = {};
		strokes.Insert(st);
		array<string> one = {};
		if (!MUD_BuildShapes.ToPlans(strokes, width, layers, wall, one, why))
			return false;
		foreach (string t : one)
		{
			texts.Insert(t);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The parts of a foxhole / MG nest / entrance ramp at p. Beside a trench (auto-attach): square to it on the
	//! side p is on (flipSide = the other side), joined to it by a short link; else free, facing 'facing'.
	//! kinds[i] / texts[i] = the parts. False (why) when it does not fit.
	protected bool StampParts(int shape, vector p, vector facing, bool flipSide, array<int> kinds, array<string> texts, out string why)
	{
		kinds.Clear();
		texts.Clear();
		MUD_TrenchLayout lay;
		float s;
		bool att = StampAttach(p, lay, s);
		vector a = p;
		vector f = facing;
		float half = 0;
		if (att)
		{
			a = lay.ChainPoint(s);
			vector nrm = ChainNormal(lay, s);
			float side = 1;
			if (vector.Dot(p - a, nrm) < 0)
				side = -1;
			if (flipSide)
				side = -side;
			f = nrm * side;
			half = lay.Half();
		}
		else if (shape == MUD_BuildShapes.RAMP)
		{
			why = "click beside a trench - the ramp climbs out of it";
			return false;
		}
		array<string> got = {};
		int width = m_Draft.m_iWidth;
		int layers = m_Draft.m_iLayers;
		if (shape == MUD_BuildShapes.RAMP)
		{
			// stairs out of the trench: STAIR_L m treads, each one layer (0.25 m) shallower, up to MIN_LAYERS
			int from = lay.Layers();
			for (int k = 1; from - k >= MUD_TrenchPlan.MIN_LAYERS; k++)
			{
				float d0 = half + (k - 1) * STAIR_L;
				if (k == 1)
					d0 = 0;
				if (!OnePlan(a + f * d0, a + f * (half + k * STAIR_L), width, from - k, 2, got, why))
					return false;
			}
			foreach (string rt : got)
			{
				kinds.Insert(MUD_Build.TRENCH);
				texts.Insert(rt);
			}
			return !texts.IsEmpty();
		}
		vector side2 = MUD_BuildShapes.Left(f);
		if (shape == MUD_BuildShapes.FOXHOLE)
		{
			width = Math.Max(1, width);
			vector c = p;
			if (att)
				c = a + f * (half + LINK_L + MUD_TrenchPlan.WidthOf(width) * 0.5);
			if (!OnePlan(c - side2 * (FOXHOLE_L * 0.5), c + side2 * (FOXHOLE_L * 0.5), width, layers, MUD_TrenchPlan.DEFAULT_WALL, got, why))
				return false;
			if (att && !OnePlan(a, c, 1, layers, MUD_TrenchPlan.DEFAULT_WALL, got, why))
				return false;
		}
		else
		{
			// MG nest: a ring round an undug middle (the gun table), a platform round its front half outside
			vector m = p;
			if (att)
				m = a + f * (half + LINK_L + NEST_R);
			MUD_BuildStroke ring = new MUD_BuildStroke();
			MUD_BuildShapes.Ellipse(m, f * -1, MUD_BuildShapes.Left(f * -1), NEST_R, NEST_R, 0, Math.PI * 2, 6, ring);
			array<ref MUD_BuildStroke> rs = {};
			rs.Insert(ring);
			array<string> ringTexts = {};
			if (!MUD_BuildShapes.ToPlans(rs, width, layers, MUD_TrenchPlan.DEFAULT_WALL, ringTexts, why))
				return false;
			foreach (string r : ringTexts)
			{
				got.Insert(r);
			}
			if (att && !OnePlan(a, m - f * NEST_R, width, layers, MUD_TrenchPlan.DEFAULT_WALL, got, why))
				return false;
			if (m_iStepLayers < layers && ringTexts.Count() == 1)
			{
				MUD_TrenchLayout rl = new MUD_TrenchLayout();
				array<string> steps = {};
				string swhy;
				if (rl.Build(MUD_TrenchPlan.Unpack(ringTexts[0])) && StepTexts(rl, rl.ChainLength() * 0.25, rl.ChainLength() * 0.75, -1, steps, swhy))
				{
					foreach (string st : steps)
					{
						kinds.Insert(MUD_Build.STEP);
						texts.Insert(st);
					}
				}
			}
		}
		foreach (string gt : got)
		{
			kinds.Insert(MUD_Build.TRENCH);
			texts.Insert(gt);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A stamp as unsaved parts (one undo group), remembered so R can turn / flip it
	protected bool PlaceStamp(int shape, vector p, vector facing, bool flipSide)
	{
		array<int> kinds = {};
		array<string> texts = {};
		string why;
		if (!StampParts(shape, p, facing, flipSide, kinds, texts, why))
		{
			SetNote(string.Format("%1: %2", MUD_BuildShapes.Name(shape), why));
			return false;
		}
		int before = m_aPending.Count();
		m_iGroupOpen = NewGroup();
		for (int i = 0; i < texts.Count(); i++)
		{
			if (!AddPending(kinds[i], texts[i]))
			{
				m_iGroupOpen = 0;
				PopFrom(before);
				return false;
			}
		}
		m_iLastGroup = m_iGroupOpen;
		m_iGroupOpen = 0;
		m_bLastStamp = true;
		m_iLastShape = shape;
		m_vLastS = p;
		m_vLastFacing = facing;
		m_bLastFlip = flipSide;
		SetNote(string.Format("%1 added (%2 parts) - R turns / flips it, Save keeps it", MUD_BuildShapes.Name(shape), texts.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Facing of a stamp placed by a click away from trenches (R turns it): 0 deg = north
	protected vector StampFacing()
	{
		float a = m_fShapeAngle * Math.DEG2RAD;
		return Vector(Math.Sin(a), 0, Math.Cos(a));
	}

	//------------------------------------------------------------------------------------------------
	//! What the stamp under the cursor (or being dragged) would make
	protected void DrawStamp()
	{
		vector p = m_vAim;
		vector f = StampFacing();
		if (m_bDragging)
		{
			p = m_vDown;
			f = m_vAim - m_vDown;
			f[1] = 0;
			if (f.Length() < 0.1)
				f = vector.Forward;
			f = f.Normalized();
		}
		array<int> kinds = {};
		array<string> texts = {};
		string why;
		if (!StampParts(m_iShape, p, f, false, kinds, texts, why))
			return;
		float edge = Math.Max(0.12, m_fH * 0.003);
		for (int i = 0; i < texts.Count(); i++)
		{
			MUD_TrenchLayout lay = new MUD_TrenchLayout();
			if (!lay.Build(MUD_TrenchPlan.Unpack(texts[i])))
				continue;
			m_Draw.Ribbon(LinePoints(lay), lay.Half() * 2, MUD_TrenchGhosts.GREEN, false, 0.08);
			if (kinds[i] == MUD_Build.TRENCH)
				DrawEdges(m_Draw, lay, edge);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool Close(string why)
	{
		m_sWhyClosed = why;
		m_bFinished = true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The camera stays over the area + LINK_REACH (its corner box)
	protected void ClampCam(MUD_BuildQuad q)
	{
		float x0 = 1000000000;
		float z0 = 1000000000;
		float x1 = -1000000000;
		float z1 = -1000000000;
		foreach (vector c : q.m_aC)
		{
			x0 = Math.Min(x0, c[0]);
			z0 = Math.Min(z0, c[2]);
			x1 = Math.Max(x1, c[0]);
			z1 = Math.Max(z1, c[2]);
		}
		float pad = MUD_Build.LINK_REACH;
		m_vCam[0] = Math.Clamp(m_vCam[0], x0 - pad, x1 + pad);
		m_vCam[2] = Math.Clamp(m_vCam[2], z0 - pad, z1 + pad);
	}

	//================================================================================================
	// parts near the cursor

	//------------------------------------------------------------------------------------------------
	//! m from (x, z) to spec's footprint (trench: its centre line less the half width; capsules: nearest one)
	protected static float DistTo(MUD_BuildSpec spec, float x, float z)
	{
		if (!spec)
			return 1000000000;
		if (spec.m_Layout)
		{
			float off;
			spec.m_Layout.ChainPos(x, z, off);
			return Math.Max(0, off - spec.m_Layout.Half());
		}
		if (spec.InRoom(x, z))
			return 0;
		float best = 1000000000;
		foreach (vector c : spec.m_aCaps)
		{
			best = Math.Min(best, Math.Sqrt((c[0] - x) * (c[0] - x) + (c[2] - z) * (c[2] - z)) - MUD_VoxelProbe.TUNNEL_R);
		}
		return Math.Max(0, best);
	}

	//------------------------------------------------------------------------------------------------
	//! Saved part of this area nearest the cursor within PICK_R; kind 0 = any kind
	protected MUD_BuildPartComponent PickSaved(int kind)
	{
		array<MUD_BuildPartComponent> parts = {};
		MUD_BuildPartComponent.ForArea(m_iArea, parts);
		MUD_BuildPartComponent best;
		float bestD = PICK_R;
		foreach (MUD_BuildPartComponent p : parts)
		{
			if (kind != 0 && p.Kind() != kind)
				continue;
			float d = DistTo(p.Spec(), m_vAim[0], m_vAim[2]);
			if (d < bestD)
			{
				bestD = d;
				best = p;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Pending new part nearest the cursor within PICK_R (index, -1 = none)
	protected int PickPending()
	{
		int best = -1;
		float bestD = PICK_R;
		for (int i = 0; i < m_aPending.Count(); i++)
		{
			MUD_BuildPending pend = m_aPending[i];
			if (pend.m_iDelete != 0)
				continue;
			float d = DistTo(pend.m_Spec, m_vAim[0], m_vAim[2]);
			if (d < bestD)
			{
				bestD = d;
				best = i;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Trench layouts of this area: saved trench / step parts, pending ones, the draft so far
	protected void Layouts(array<MUD_TrenchLayout> lays, array<int> layers)
	{
		lays.Clear();
		layers.Clear();
		array<MUD_BuildPartComponent> parts = {};
		MUD_BuildPartComponent.ForArea(m_iArea, parts);
		foreach (MUD_BuildPartComponent p : parts)
		{
			if (p.Spec() && p.Spec().m_Layout)
			{
				lays.Insert(p.Spec().m_Layout);
				layers.Insert(p.Spec().m_Layout.Layers());
			}
		}
		foreach (MUD_BuildPending pend : m_aPending)
		{
			if (pend.m_iDelete == 0 && pend.m_Spec && pend.m_Spec.m_Layout)
			{
				lays.Insert(pend.m_Spec.m_Layout);
				layers.Insert(pend.m_Spec.m_Layout.Layers());
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Auto-attach (panel check box): a trench point near a trench END goes on that end (the lines run on as one),
	//! else near a centre line onto the line (a junction); off or nothing near = the grid point
	protected vector SnapTrench(vector p)
	{
		if (!m_bAttach)
			return MUD_WorldGroundEntity.SnapFine(p);
		array<MUD_TrenchLayout> lays = {};
		array<int> layers = {};
		Layouts(lays, layers);
		float bestEnd = ATTACH_END_R;
		float bestLine = ATTACH_LINE_R;
		vector onEnd;
		vector onLine;
		bool hitEnd = false;
		bool hitLine = false;
		foreach (MUD_TrenchLayout lay : lays)
		{
			float total = lay.ChainLength();
			vector e0 = lay.ChainPoint(0);
			vector e1 = lay.ChainPoint(total);
			float d0 = vector.DistanceXZ(p, e0);
			float d1 = vector.DistanceXZ(p, e1);
			if (d0 < bestEnd)
			{
				bestEnd = d0;
				onEnd = e0;
				hitEnd = true;
			}
			if (d1 < bestEnd)
			{
				bestEnd = d1;
				onEnd = e1;
				hitEnd = true;
			}
			float off;
			float s = lay.ChainPos(p[0], p[2], off);
			if (off < bestLine)
			{
				bestLine = off;
				onLine = lay.ChainPoint(s);
				hitLine = true;
			}
		}
		if (hitEnd)
			return onEnd;
		if (hitLine)
			return onLine;
		return MUD_WorldGroundEntity.SnapFine(p);
	}

	//------------------------------------------------------------------------------------------------
	//! Floor depth (m under the undug ground) of whatever lies at p: a trench, a dugout room, else the tunnel depth
	protected float EndDepth(vector p)
	{
		array<MUD_TrenchLayout> lays = {};
		array<int> layers = {};
		Layouts(lays, layers);
		for (int i = 0; i < lays.Count(); i++)
		{
			if (lays[i].PieceAt(p[0], p[2]) >= 0)
				return layers[i] * MUD_TrenchPlan.LAYER;
		}
		array<MUD_BuildPartComponent> parts = {};
		MUD_BuildPartComponent.ForArea(m_iArea, parts);
		foreach (MUD_BuildPartComponent part : parts)
		{
			if (part.Spec() && part.Spec().InRoom(p[0], p[2]))
				return part.Spec().m_fDepth;
		}
		foreach (MUD_BuildPending pend : m_aPending)
		{
			if (pend.m_Spec && pend.m_Spec.InRoom(p[0], p[2]))
				return pend.m_Spec.m_fDepth;
		}
		return m_fTunnelD;
	}

	//------------------------------------------------------------------------------------------------
	//! to, pulled back to most m (flat) from from
	protected static vector ClampFrom(vector from, vector to, float most)
	{
		vector d = to - from;
		d[1] = 0;
		float len = d.Length();
		if (len <= most)
			return to;
		vector p = from + d * (most / len);
		p[1] = to[1];
		return p;
	}

	//================================================================================================
	// pending changes

	//------------------------------------------------------------------------------------------------
	//! A new part to save; false (note says why) when it does not parse
	protected bool AddPending(int kind, string text)
	{
		MUD_BuildSpec spec = new MUD_BuildSpec();
		if (!spec.Parse(kind, text))
		{
			SetNote(MUD_Build.KindName(kind) + ": " + spec.m_sError);
			return false;
		}
		MUD_BuildPending pend = new MUD_BuildPending();
		pend.m_iKind = kind;
		pend.m_sText = text;
		pend.m_Spec = spec;
		pend.m_iGroup = m_iGroupOpen;
		if (pend.m_iGroup == 0)
			pend.m_iGroup = NewGroup();
		if (spec.m_Layout)
		{
			pend.m_Ghost = new MUD_TrenchGhosts();
			array<int> states = {};
			GroundStates(spec.m_Layout, states);
			pend.m_Ghost.Draw(spec.m_Layout, states);
		}
		m_aPending.Insert(pend);
		m_bPendDirty = true;
		m_bCloseAsked = false;
		SetNote(string.Format("%1 added (%2 pieces) - F saves", MUD_Build.KindName(kind), spec.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int NewGroup()
	{
		m_iGroupNext++;
		return m_iGroupNext;
	}

	//------------------------------------------------------------------------------------------------
	//! Green where the piece middle is over a hole cell, red elsewhere (road, ...: not dug)
	protected static int GroundStates(MUD_TrenchLayout lay, array<int> states)
	{
		int red = 0;
		states.Clear();
		for (int i = 0; i < lay.Count(); i++)
		{
			if (MUD_WorldGroundEntity.IsOver(lay.PieceMiddle(i)))
			{
				states.Insert(MUD_TrenchGhosts.GREEN);
			}
			else
			{
				states.Insert(MUD_TrenchGhosts.RED);
				red++;
			}
		}
		return red;
	}

	//------------------------------------------------------------------------------------------------
	protected void PopPending(int index)
	{
		if (index < 0 || index >= m_aPending.Count())
			return;
		MUD_BuildPending pend = m_aPending[index];
		if (pend.m_Ghost)
			pend.m_Ghost.Clear();
		m_aPending.RemoveOrdered(index);
		m_bPendDirty = true;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected int PendingDeleteOf(int uid)
	{
		for (int i = 0; i < m_aPending.Count(); i++)
		{
			if (m_aPending[i].m_iDelete == uid)
				return i;
		}
		return -1;
	}

	//================================================================================================
	// tools

	//------------------------------------------------------------------------------------------------
	protected void ResetTool()
	{
		if (m_bDown)
			m_bIgnoreHeld = true;
		m_bDown = false;
		m_bDragging = false;
		m_aPencil.Clear();
		m_Draft.m_aPoints.Clear();
		m_Draft.m_aBends.Clear();
		m_iBending = -1;
		m_aPts.Clear();
		m_iPick = 0;
		m_StepLay = null;
		m_DraftGhost.Clear();
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnToolNext(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		m_iTool = (m_iTool + 1) % TOOLS;
		ResetTool();
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnToolPrev(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		m_iTool = (m_iTool + TOOLS - 1) % TOOLS;
		ResetTool();
		SetNote("");
	}

	//------------------------------------------------------------------------------------------------
	//! LMB: the tool's click at the cursor
	protected void OnPoint(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_bBendArm && m_iTool == T_TRENCH)
		{
			m_bBendArm = false;
			OnBend();     // picks the draft segment at this click
			m_bDirty = true;
			return;
		}
		if (m_iTool == T_TRENCH)
			TrenchPoint();
		else if (m_iTool == T_STEP)
			StepPick();
		else if (m_iTool == T_COVER)
			CoverPoint();
		else if (m_iTool == T_TUNNEL)
			TunnelPoint();
		else if (m_iTool == T_DUGOUT)
			DugoutPoint();
		else
			DeletePick();
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void TrenchPoint()
	{
		if (m_iBending >= 0)
		{
			m_iBending = -1;
			return;
		}
		int n = m_Draft.m_aPoints.Count();
		vector p = SnapTrench(m_vAim);
		if (n > 0)
		{
			vector last = m_Draft.m_aPoints[n - 1];
			if (vector.DistanceXZ(p, last) < MUD_TrenchPlan.MIN_SEGMENT)
				p = MUD_WorldGroundEntity.SnapFine(m_vAim);     // attach pulled it back onto the end it starts from
			p = ClampFrom(last, p, MUD_TrenchPlan.MAX_SEGMENT);
			if (vector.DistanceSqXZ(p, last) < MUD_TrenchPlan.MIN_SEGMENT * MUD_TrenchPlan.MIN_SEGMENT)
			{
				SetNote("Too close to the last point");
				return;
			}
		}
		m_Draft.AddPoint(p);
		SetNote("");
		// a full line goes to the list and the next one starts where it ended (one long trench, many lines)
		if (m_Draft.m_aPoints.Count() >= MUD_TrenchPlan.MAX_POINTS)
			FinishTrench(true);
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishTrench(bool goOn)
	{
		int n = m_Draft.m_aPoints.Count();
		if (n < 2)
		{
			SetNote("A trench needs 2 points");
			return;
		}
		vector last = m_Draft.m_aPoints[n - 1];
		m_iGroupOpen = NewGroup();
		string text = m_Draft.Pack();
		if (!AddPending(MUD_Build.TRENCH, text))
		{
			m_iGroupOpen = 0;
			return;
		}
		array<string> texts = {};
		texts.Insert(text);
		AutoSteps(texts);
		m_iGroupOpen = 0;
		m_Draft.m_aPoints.Clear();
		m_Draft.m_aBends.Clear();
		m_iBending = -1;
		if (goOn)
		{
			m_Draft.AddPoint(last);
			SetNote(string.Format("Line full (%1 points): goes on as a new line", MUD_TrenchPlan.MAX_POINTS));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Firing step, round 2c (user 2026-10-24: "the step is good but not intuitive"): hover ANY trench (saved or
	//! not); the platform goes on the side the cursor is on. Click start + click end along the trench, or with
	//! "Whole trench" one click. Panel: platform depth, platform width.
	protected void StepPick()
	{
		MUD_TrenchLayout lay;
		float s;
		int side;
		if (m_StepLay && !m_bStepWhole)
		{
			float off;
			float s1 = m_StepLay.ChainPos(m_vAim[0], m_vAim[2], off);
			float lo = Math.Min(m_fStepS0, s1);
			float hi = Math.Max(m_fStepS0, s1);
			if (hi - lo < STEP_MIN)
			{
				SetNote(string.Format("A step needs %1 m or more - click further along the trench", STEP_MIN));
				return;
			}
			AddStep(m_StepLay, lo, hi, m_iStepSide);
			m_StepLay = null;
			return;
		}
		if (!StepHover(lay, s, side))
		{
			SetNote("Aim at a trench, on the side the platform should go");
			return;
		}
		if (m_iStepLayers >= lay.Layers())
		{
			SetNote(string.Format("The platform must be higher than the trench floor (%1 m deep): Platform depth -", lay.Layers() * MUD_TrenchPlan.LAYER));
			return;
		}
		if (m_bStepWhole)
		{
			AddStep(lay, 0, lay.ChainLength(), side);
			return;
		}
		m_StepLay = lay;
		m_fStepS0 = s;
		m_iStepSide = side;
		SetNote("Start set - click where the platform ends (same trench), RMB clears");
	}

	//------------------------------------------------------------------------------------------------
	//! Trench nearest the cursor (within its top width + 2 m): where along it and which side the cursor is on
	protected bool StepHover(out MUD_TrenchLayout lay, out float s, out int side)
	{
		array<MUD_TrenchLayout> lays = {};
		TrenchLayouts(lays);
		float best = 1000000;
		foreach (MUD_TrenchLayout l : lays)
		{
			float off;
			float at = l.ChainPos(m_vAim[0], m_vAim[2], off);
			float reach = l.Half() + l.Layers() * MUD_TrenchPlan.LAYER * l.Run() + 2;
			if (off < reach && off < best)
			{
				best = off;
				lay = l;
				s = at;
			}
		}
		if (!lay)
			return false;
		vector nrm = ChainNormal(lay, s);
		side = 1;
		if (vector.Dot(m_vAim - lay.ChainPoint(s), nrm) < 0)
			side = -1;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Flat left normal of a layout's centre line at s
	protected static vector ChainNormal(MUD_TrenchLayout lay, float s)
	{
		float total = lay.ChainLength();
		vector a = lay.ChainPoint(Math.Max(0, s - 0.25));
		vector b = lay.ChainPoint(Math.Min(total, s + 0.25));
		vector dir = Vector(b[0] - a[0], 0, b[2] - a[2]);
		float len = dir.Length();
		if (len < 0.0001)
			return vector.Right;
		return MUD_BuildShapes.Left(dir * (1 / len));
	}

	//------------------------------------------------------------------------------------------------
	//! Step plan texts for lay's stretch lo..hi on side: the platform's centre line runs beside the floor
	protected bool StepTexts(MUD_TrenchLayout lay, float lo, float hi, int side, array<string> texts, out string why)
	{
		float d = lay.Half() + MUD_TrenchPlan.WidthOf(m_iStepWidth) * 0.5;
		int n = Math.Max(2, Math.Ceil((hi - lo) / 1.0) + 1);
		MUD_BuildStroke st = new MUD_BuildStroke();
		for (int i = 0; i < n; i++)
		{
			float s = lo + (hi - lo) * i / (n - 1);
			st.Add(lay.ChainPoint(s) + ChainNormal(lay, s) * (d * side));
		}
		array<ref MUD_BuildStroke> strokes = {};
		strokes.Insert(st);
		return MUD_BuildShapes.ToPlans(strokes, m_iStepWidth, m_iStepLayers, MUD_TrenchPlan.DEFAULT_WALL, texts, why);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddStep(MUD_TrenchLayout lay, float lo, float hi, int side)
	{
		if (m_iStepLayers >= lay.Layers())
		{
			SetNote("The platform must be higher than the trench floor");
			return;
		}
		array<string> texts = {};
		string why;
		if (!StepTexts(lay, lo, hi, side, texts, why))
		{
			SetNote("Step: " + why);
			return;
		}
		int before = m_aPending.Count();
		m_iGroupOpen = NewGroup();
		foreach (string text : texts)
		{
			if (!AddPending(MUD_Build.STEP, text))
			{
				m_iGroupOpen = 0;
				for (int i = m_aPending.Count() - 1; i >= before; i--)
				{
					PopPending(i);
				}
				return;
			}
		}
		m_iGroupOpen = 0;
		SetNote(string.Format("Firing step added: %1 m long, %2 m up from the floor - Save keeps it", Math.Round(hi - lo), (lay.Layers() - m_iStepLayers) * MUD_TrenchPlan.LAYER));
	}

	//------------------------------------------------------------------------------------------------
	//! What a click would make: the stretch so far (start set), the whole trench, or a 3 m piece at the cursor
	protected void DrawStepPreview()
	{
		MUD_TrenchLayout lay;
		float s;
		int side;
		float lo;
		float hi;
		if (m_StepLay && !m_bStepWhole)
		{
			float off;
			lay = m_StepLay;
			side = m_iStepSide;
			s = lay.ChainPos(m_vAim[0], m_vAim[2], off);
			lo = Math.Min(m_fStepS0, s);
			hi = Math.Max(m_fStepS0, s);
		}
		else
		{
			if (!StepHover(lay, s, side))
				return;
			lo = Math.Max(0, s - 1.5);
			hi = Math.Min(lay.ChainLength(), s + 1.5);
			if (m_bStepWhole)
			{
				lo = 0;
				hi = lay.ChainLength();
			}
		}
		if (hi - lo < 0.5)
			return;
		array<string> texts = {};
		string why;
		int state = MUD_TrenchGhosts.GREEN;
		if (m_iStepLayers >= lay.Layers())
			state = MUD_TrenchGhosts.RED;
		if (!StepTexts(lay, lo, hi, side, texts, why))
			return;
		float edge = Math.Max(0.12, m_fH * 0.003);
		foreach (string text : texts)
		{
			MUD_TrenchLayout step = new MUD_TrenchLayout();
			if (!step.Build(MUD_TrenchPlan.Unpack(text)))
				continue;
			m_Draw.Ribbon(LinePoints(step), step.Half() * 2, state, false, 0.1);
		}
		DrawEdges(m_Draw, lay, edge);
	}

	//------------------------------------------------------------------------------------------------
	//! The trench's points moved out sideways by its half width + the step's half width (mitred at corners), the
	//! same bends; null when that breaks the plan limits
	static MUD_TrenchPlan MakeStep(MUD_TrenchPlan src, int side, int layers)
	{
		MUD_TrenchPlan step = new MUD_TrenchPlan();
		step.m_iWidth = 0;
		step.m_iLayers = Math.ClampInt(layers, MUD_TrenchPlan.MIN_LAYERS, MUD_TrenchPlan.MAX_LAYERS);
		float d = src.Width() * 0.5 + MUD_TrenchPlan.WidthOf(0) * 0.5;
		int n = src.m_aPoints.Count();
		for (int i = 0; i < n; i++)
		{
			vector nrm = vector.Zero;
			vector n1 = vector.Zero;
			if (i > 0)
			{
				n1 = SegNormal(src.m_aPoints[i - 1], src.m_aPoints[i]);
				nrm = nrm + n1;
			}
			if (i < n - 1)
			{
				vector n2 = SegNormal(src.m_aPoints[i], src.m_aPoints[i + 1]);
				nrm = nrm + n2;
				if (i == 0)
					n1 = n2;
			}
			float len = nrm.Length();
			if (len < 0.0001)
				return null;
			nrm = nrm * (1 / len);
			float scale = 1 / Math.Max(0.35, vector.Dot(nrm, n1));
			vector p = src.m_aPoints[i] + nrm * (d * scale * side);
			step.AddPoint(Vector(p[0], 0, p[2]), src.m_aBends[i]);
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (!lay.Build(MUD_TrenchPlan.Unpack(step.Pack())))
			return null;
		return step;
	}

	//------------------------------------------------------------------------------------------------
	//! Flat left normal of a -> b (MUD_TrenchSeg.m_vFwd)
	protected static vector SegNormal(vector a, vector b)
	{
		vector r = Vector(b[0] - a[0], 0, b[2] - a[2]);
		float len = r.Length();
		if (len < 0.0001)
			return vector.Zero;
		r = r * (1 / len);
		return Vector(-r[2], 0, r[0]);
	}

	//------------------------------------------------------------------------------------------------
	//! Roofed trench: first click = start on a saved trench, second = end on the same trench
	protected void CoverPoint()
	{
		if (m_iPick == 0)
		{
			MUD_BuildPartComponent t = PickSaved(MUD_Build.TRENCH);
			if (!t || !t.Spec() || !t.Spec().m_Layout)
			{
				SetNote("Aim at a SAVED, undug trench stretch (F saves new ones first)");
				return;
			}
			float off;
			m_fCoverS0 = t.Spec().m_Layout.ChainPos(m_vAim[0], m_vAim[2], off);
			m_iPick = t.Uid();
			SetNote("Roof start set - LMB the end on the same trench");
			return;
		}
		MUD_BuildPartComponent tr = MUD_BuildPartComponent.Find(m_iPick);
		if (!tr || !tr.Spec() || !tr.Spec().m_Layout)
		{
			m_iPick = 0;
			SetNote("That trench is gone");
			return;
		}
		MUD_TrenchLayout lay = tr.Spec().m_Layout;
		float off2;
		float s1 = lay.ChainPos(m_vAim[0], m_vAim[2], off2);
		float lo = Math.Min(m_fCoverS0, s1);
		float hi = Math.Max(m_fCoverS0, s1);
		if (hi - lo < 2 * MUD_Build.COVER_ENTRY + 1)
		{
			SetNote(string.Format("A roof needs %1 m or more of trench", 2 * MUD_Build.COVER_ENTRY + 1));
			return;
		}
		if (hi - lo > MUD_Build.COVER_MAX)
		{
			SetNote(string.Format("A roof is %1 m at most", MUD_Build.COVER_MAX));
			return;
		}
		int segs = Math.Ceil((hi - lo) / 3.0);
		string text = string.Format("%1,%2", tr.Uid(), MUD_Build.Cm(lay.Layers() * MUD_TrenchPlan.LAYER));
		for (int k = 0; k <= segs; k++)
		{
			vector p = lay.ChainPoint(lo + (hi - lo) * k / segs);
			text += string.Format(";%1,%2", MUD_Build.Cm(p[0]), MUD_Build.Cm(p[2]));
		}
		m_iPick = 0;
		AddPending(MUD_Build.COVER, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Trench layouts (not steps) of this area, saved and unsaved: what steps and tunnels attach to
	protected void TrenchLayouts(array<MUD_TrenchLayout> lays)
	{
		lays.Clear();
		array<MUD_BuildPartComponent> parts = {};
		MUD_BuildPartComponent.ForArea(m_iArea, parts);
		foreach (MUD_BuildPartComponent p : parts)
		{
			if (p.Kind() == MUD_Build.TRENCH && p.Spec() && p.Spec().m_Layout)
				lays.Insert(p.Spec().m_Layout);
		}
		foreach (MUD_BuildPending pend : m_aPending)
		{
			if (pend.m_iDelete == 0 && pend.m_iKind == MUD_Build.TRENCH && pend.m_Spec && pend.m_Spec.m_Layout)
				lays.Insert(pend.m_Spec.m_Layout);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tunnel end auto-attach (round 2c, user: "tunnels and trenches should connect smoothly"): an end within the
	//! trench's top width + TUNNEL_ATTACH_R goes onto the trench WALL at floor level (wall), with a lead point
	//! TUNNEL_LEAD out, square to the wall on the side the tunnel runs to (next = the path's next point).
	protected bool TunnelAttach(vector p, vector next, out vector wall, out vector lead, out float depth)
	{
		if (!m_bAttach)
			return false;
		array<MUD_TrenchLayout> lays = {};
		TrenchLayouts(lays);
		MUD_TrenchLayout best;
		float bestOff = 1000000;
		float bestS;
		foreach (MUD_TrenchLayout lay : lays)
		{
			float off;
			float s = lay.ChainPos(p[0], p[2], off);
			float reach = lay.Half() + lay.Layers() * MUD_TrenchPlan.LAYER * lay.Run() + TUNNEL_ATTACH_R;
			if (off < reach && off < bestOff)
			{
				bestOff = off;
				best = lay;
				bestS = s;
			}
		}
		if (!best)
			return false;
		vector c = best.ChainPoint(bestS);
		vector nrm = ChainNormal(best, bestS);
		float side = 1;
		if (vector.Dot(next - c, nrm) < 0)
			side = -1;
		wall = c + nrm * (side * best.Half());
		lead = c + nrm * (side * (best.Half() + TUNNEL_LEAD));
		depth = best.Layers() * MUD_TrenchPlan.LAYER;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A tunnel path through pts with its end depths: ends near a trench get a wall mouth + square lead-in;
	//! points closer than 1 m merged, long gaps split. False (why) when it breaks the path limits.
	protected bool TunnelPath(array<vector> pts, array<vector> path, out float dA, out float dB, out string why)
	{
		path.Clear();
		int n = pts.Count();
		if (n < 2)
		{
			why = "a tunnel needs 2 points";
			return false;
		}
		array<vector> raw = {};
		vector wall;
		vector lead;
		float d;
		dA = EndDepth(pts[0]);
		dB = EndDepth(pts[n - 1]);
		if (TunnelAttach(pts[0], pts[1], wall, lead, d))
		{
			raw.Insert(wall);
			raw.Insert(lead);
			dA = d;
		}
		else
		{
			raw.Insert(pts[0]);
		}
		for (int i = 1; i < n - 1; i++)
		{
			raw.Insert(pts[i]);
		}
		if (TunnelAttach(pts[n - 1], pts[n - 2], wall, lead, d))
		{
			raw.Insert(lead);
			raw.Insert(wall);
			dB = d;
		}
		else
		{
			raw.Insert(pts[n - 1]);
		}
		foreach (vector rq : raw)
		{
			vector q = rq;
			q[1] = 0;
			int k = path.Count();
			if (k > 0 && vector.DistanceXZ(path[k - 1], q) < 1)
			{
				if (k >= 2)
					path[k - 1] = q;     // keep the true end (a wall mouth) over a too-near lead point
				continue;
			}
			if (k > 0)
			{
				float gap = vector.DistanceXZ(path[k - 1], q);
				int parts = Math.Ceil(gap / (MUD_Build.PATH_MAX_GAP - 0.5));
				for (int j = 1; j < parts; j++)
				{
					path.Insert(path[k - 1] + (q - path[k - 1]) * (j * 1.0 / parts));     // j / parts alone is int division
				}
			}
			path.Insert(q);
		}
		if (path.Count() < 2)
		{
			why = "too short";
			return false;
		}
		if (path.Count() > MUD_Build.PATH_MAX_POINTS)
		{
			why = string.Format("%1 points (at most %2) - a simpler or shorter shape", path.Count(), MUD_Build.PATH_MAX_POINTS);
			return false;
		}
		if (MUD_Build.PolyLength(path) > MUD_Build.TUNNEL_MAX)
		{
			why = string.Format("%1 m long (at most %2 m)", Math.Round(MUD_Build.PolyLength(path)), MUD_Build.TUNNEL_MAX);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The tunnel as it will be dug: 1.6 m band (capsule width), dark where it has a roof, ORANGE where the ground
	//! over it is too thin (the ramp out of a trench is an open cut there), orange squares on wall mouths
	protected void TunnelPreview(array<vector> path, float dA, float dB, bool ok)
	{
		float total = MUD_Build.PolyLength(path);
		if (total < 0.1)
			return;
		if (!ok)
		{
			m_Draw.Ribbon(path, 2 * MUD_VoxelProbe.TUNNEL_R, MUD_TrenchGhosts.RED, false, 0.1);
			return;
		}
		int n = Math.Max(2, Math.Ceil(total) + 1);
		array<float> ss = {};
		array<float> undug = {};
		array<vector> pts = {};
		vector dir;
		for (int i = 0; i < n; i++)
		{
			float s = total * i / (n - 1);
			vector p = MUD_Build.PolyPoint(path, s, dir);
			float y;
			if (!MUD_Build.UndugAt(p[0], p[2], y))
				y = MUD_BuildDraw.GroundY(p[0], p[2]);
			ss.Insert(s);
			undug.Insert(y);
			pts.Insert(p);
		}
		array<float> floors = {};
		MUD_Build.Profile(ss, undug, dA, dB, total, floors);
		array<vector> run = {};
		bool runOpen = false;
		for (int k = 0; k < n; k++)
		{
			bool open = undug[k] - (floors[k] + MUD_Build.CapHeight()) < MUD_Build.ROOF * 0.5;
			if (run.Count() > 0 && open != runOpen)
			{
				run.Insert(pts[k]);
				TunnelRun(run, runOpen);
				run.Clear();
			}
			runOpen = open;
			run.Insert(pts[k]);
		}
		TunnelRun(run, runOpen);
	}

	//------------------------------------------------------------------------------------------------
	protected void TunnelRun(array<vector> run, bool open)
	{
		if (run.Count() < 2)
			return;
		int state = MUD_TrenchGhosts.DUG;
		if (open)
			state = MUD_BuildDraw.MARK;
		m_Draw.Ribbon(run, 2 * MUD_VoxelProbe.TUNNEL_R, state, false, 0.1);
	}

	//------------------------------------------------------------------------------------------------
	//! Points tool: a click; a click that attaches to a trench (not the first) ends the tunnel there
	protected void TunnelPoint()
	{
		vector p = MUD_WorldGroundEntity.SnapFine(m_vAim);
		int n = m_aPts.Count();
		if (n >= MUD_Build.PATH_MAX_POINTS)
		{
			SetNote(string.Format("At most %1 points - Finish tunnel", MUD_Build.PATH_MAX_POINTS));
			return;
		}
		if (n > 0)
		{
			p = ClampFrom(m_aPts[n - 1], p, MUD_Build.PATH_MAX_GAP);
			if (vector.DistanceXZ(p, m_aPts[n - 1]) < 1)
			{
				SetNote("Too close to the last point");
				return;
			}
		}
		m_aPts.Insert(p);
		vector wall;
		vector lead;
		float d;
		if (n > 0 && TunnelAttach(p, m_aPts[n - 1], wall, lead, d))
		{
			FinishTunnel();
			return;
		}
		SetNote(string.Format("Tunnel point %1 - click on a trench to end it there, or Finish tunnel", n + 1));
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishTunnel()
	{
		if (AddTunnel(m_aPts))
			m_aPts.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! pts -> one unsaved tunnel part (attached ends, depths); false = note says why
	protected bool AddTunnel(array<vector> pts)
	{
		array<vector> path = {};
		float dA;
		float dB;
		string why;
		if (!TunnelPath(pts, path, dA, dB, why))
		{
			SetNote("Tunnel: " + why);
			return false;
		}
		return AddPending(MUD_Build.TUNNEL, PathText(path, dA, dB));
	}

	//------------------------------------------------------------------------------------------------
	//! A shape dragged with the tunnel tool: each stroke becomes one tunnel (arcs as 2 points per bent segment)
	protected void FinishTunnelDrag(array<ref MUD_BuildStroke> strokes)
	{
		int before = m_aPending.Count();
		m_iGroupOpen = NewGroup();
		foreach (MUD_BuildStroke st : strokes)
		{
			array<vector> line = {};
			st.Polyline(line, 2);
			if (!AddTunnel(line))
			{
				m_iGroupOpen = 0;
				for (int i = m_aPending.Count() - 1; i >= before; i--)
				{
					PopPending(i);
				}
				return;
			}
		}
		m_iGroupOpen = 0;
		SetNote(string.Format("%1 tunnel added - Save keeps it", MUD_BuildShapes.Name(m_iShape)));
	}

	//------------------------------------------------------------------------------------------------
	protected static string PathText(array<vector> pts, float dA, float dB)
	{
		int a = Math.ClampInt(MUD_Build.Cm(dA), MUD_Build.MIN_DEPTH_CM, MUD_Build.MAX_DEPTH_CM);
		int b = Math.ClampInt(MUD_Build.Cm(dB), MUD_Build.MIN_DEPTH_CM, MUD_Build.MAX_DEPTH_CM);
		string text = string.Format("%1,%2", a, b);
		foreach (vector p : pts)
		{
			text += string.Format(";%1,%2", MUD_Build.Cm(p[0]), MUD_Build.Cm(p[2]));
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Dugout: first click = the door (in a trench), second = the room middle. Adds the entry tunnel and the room.
	protected void DugoutPoint()
	{
		if (m_aPts.Count() == 0)
		{
			m_aPts.Insert(MUD_WorldGroundEntity.SnapFine(m_vAim));
			SetNote(string.Format("Door set (floor %1 m deep) - LMB the room middle", EndDepth(m_aPts[0])));
			return;
		}
		vector door = m_aPts[0];
		vector mid;
		vector fwd;
		float doorD;
		string why = DugoutPlace(door, m_vAim, mid, fwd, doorD);
		if (!why.IsEmpty())
		{
			SetNote(why);
			return;
		}
		float l = MUD_Build.DugLength(m_iDugSize);
		float yaw = Math.Atan2(fwd[0], fwd[2]) * Math.RAD2DEG;
		vector inside = mid - fwd * (l * 0.5 - 0.4);
		array<vector> path = {};
		path.Insert(door);
		float run = vector.DistanceXZ(door, inside);
		int legs = Math.Ceil(run / 10.0);
		for (int k = 1; k <= legs; k++)
		{
			path.Insert(door + (inside - door) * (k * 1.0 / legs));     // k / legs alone is int division
		}
		int yawDeg = Math.Round(yaw);     // an int: a float would print with decimals and the server's int parse refuses it
		m_iGroupOpen = NewGroup();     // entry tunnel + room: one RMB undoes both
		string dug = string.Format("%1,%2,%3,%4,%5,%6", MUD_Build.Cm(mid[0]), MUD_Build.Cm(mid[2]), yawDeg, MUD_Build.Cm(MUD_Build.DugWidth(m_iDugSize)), MUD_Build.Cm(l), MUD_Build.Cm(MUD_Build.DUGOUT_D));
		bool added = AddPending(MUD_Build.TUNNEL, PathText(path, doorD, MUD_Build.DUGOUT_D));
		if (added && !AddPending(MUD_Build.DUGOUT, dug))
		{
			PopPending(m_aPending.Count() - 1);
			added = false;
		}
		m_iGroupOpen = 0;
		if (!added)
			return;
		m_aPts.Clear();
		SetNote("Dugout + entry tunnel added - F saves");
	}

	//------------------------------------------------------------------------------------------------
	//! Where the room goes for door + cursor: pushed out so the entry slope fits. "" = fine, else why not.
	protected string DugoutPlace(vector door, vector aim, out vector mid, out vector fwd, out float doorD)
	{
		vector d = Vector(aim[0] - door[0], 0, aim[2] - door[2]);
		float len = d.Length();
		if (len < 0.5)
			return "Move the cursor away from the door (that way the room goes)";
		fwd = d * (1 / len);
		doorD = EndDepth(door);
		float l = MUD_Build.DugLength(m_iDugSize);
		float minRun = Math.Max(1, (MUD_Build.DUGOUT_D - doorD) / MUD_Build.SLOPE + 0.5);
		float run = Math.Max(len - l * 0.5, minRun);
		mid = door + fwd * (run + l * 0.5);
		mid[1] = 0;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Delete: a pending new part goes at once; a saved one is marked (F deletes it) or unmarked
	protected void DeletePick()
	{
		int pend = PickPending();
		if (pend >= 0)
		{
			PopPending(pend);
			SetNote("Unsaved part removed");
			return;
		}
		MUD_BuildPartComponent part = PickSaved(0);
		if (!part)
		{
			SetNote("Aim at a part to delete");
			return;
		}
		int marked = PendingDeleteOf(part.Uid());
		if (marked >= 0)
		{
			PopPending(marked);
			SetNote("Delete unmarked");
			return;
		}
		MUD_BuildPending del = new MUD_BuildPending();
		del.m_iDelete = part.Uid();
		del.m_iGroup = NewGroup();
		del.m_iKind = part.Kind();
		del.m_Spec = part.Spec();
		m_aPending.Insert(del);
		string extra = "";
		if (part.Started())
			extra = " (dug ground stays)";
		if (part.Source() != 0)
			extra += " - a link: its other side goes too";
		SetNote(string.Format("%1 marked for delete%2 - F saves", MUD_Build.KindName(part.Kind()), extra));
	}

	//------------------------------------------------------------------------------------------------
	//! RMB: cancel a drag, else undo the last click of the tool, else the last unsaved change (a whole shape)
	protected void OnUndo(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		m_bDirty = true;
		if (m_bDown)
		{
			m_bDown = false;
			m_bIgnoreHeld = true;     // LMB is still held: no new drag from here
			m_bDragging = false;
			m_aPencil.Clear();
			SetNote("Drag cancelled");
			return;
		}
		if (m_iTool == T_TRENCH)
		{
			if (m_iBending >= 0)
			{
				m_Draft.m_aBends[m_iBending] = m_fBendFrom;
				m_iBending = -1;
				return;
			}
			int n = m_Draft.m_aPoints.Count();
			if (n > 0)
			{
				m_Draft.m_aPoints.Remove(n - 1);
				m_Draft.m_aBends.Remove(n - 1);
				if (n >= 2)
					m_Draft.m_aBends[n - 2] = 0;
				return;
			}
		}
		if ((m_iTool == T_TUNNEL || m_iTool == T_DUGOUT) && m_aPts.Count() > 0)
		{
			m_aPts.Remove(m_aPts.Count() - 1);
			return;
		}
		if (m_iTool == T_STEP && m_StepLay)
		{
			m_StepLay = null;
			SetNote("Step start cleared");
			return;
		}
		if (m_iTool == T_COVER && m_iPick != 0)
		{
			m_iPick = 0;
			SetNote("Roof start cleared");
			return;
		}
		if (m_aPending.Count() > 0)
		{
			int group = m_aPending[m_aPending.Count() - 1].m_iGroup;
			int undone = 0;
			while (m_aPending.Count() > 0 && m_aPending[m_aPending.Count() - 1].m_iGroup == group)
			{
				PopPending(m_aPending.Count() - 1);
				undone++;
			}
			SetNote(string.Format("Last unsaved change undone (%1 parts)", undone));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! X: trench width / step side / dugout size
	protected void OnWidth(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iTool == T_TRENCH)
			m_Draft.m_iWidth = (m_Draft.m_iWidth + 1) % MUD_TrenchPlan.WIDTHS;
		else if (m_iTool == T_STEP)
			m_iStepWidth = 1 - m_iStepWidth;
		else if (m_iTool == T_DUGOUT)
			m_iDugSize = (m_iDugSize + 1) % MUD_Build.DUG_SIZES;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! C: trench wall angle
	protected void OnWall(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iTool == T_TRENCH)
			m_Draft.m_iWall = (m_Draft.m_iWall + 1) % MUD_TrenchPlan.WALLS;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Up: deeper (trench / step layers, tunnel end depth)
	protected void OnDeeper(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		Depth(1);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnShallower(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		Depth(-1);
	}

	//------------------------------------------------------------------------------------------------
	protected void Depth(int delta)
	{
		if (m_iTool == T_TRENCH)
			m_Draft.m_iLayers = Math.ClampInt(m_Draft.m_iLayers + delta, MUD_TrenchPlan.MIN_LAYERS, MUD_TrenchPlan.MAX_LAYERS);
		else if (m_iTool == T_STEP)
			m_iStepLayers = Math.ClampInt(m_iStepLayers + delta, MUD_TrenchPlan.MIN_LAYERS, MUD_TrenchPlan.MAX_LAYERS - 1);
		else if (m_iTool == T_TUNNEL || m_iTool == T_DUGOUT)
			m_fTunnelD = Math.Clamp(m_fTunnelD + delta * 0.25, TUNNEL_D_MIN, TUNNEL_D_MAX);
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! V: bend the draft segment nearest the cursor (as the trench planner)
	protected void OnBend(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iTool != T_TRENCH)
			return;
		if (m_iBending >= 0)
		{
			m_iBending = -1;
			m_bDirty = true;
			return;
		}
		int best = -1;
		float bestSq = 9;
		for (int i = 0; i < m_Draft.m_aPoints.Count() - 1; i++)
		{
			vector a = m_Draft.m_aPoints[i];
			vector b = m_Draft.m_aPoints[i + 1];
			if (vector.DistanceSqXZ(a, b) < MUD_TrenchPlan.MIN_BEND_CHORD * MUD_TrenchPlan.MIN_BEND_CHORD)
				continue;
			float dSq = vector.DistanceSqXZ((a + b) * 0.5, m_vAim);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = i;
			}
		}
		if (best < 0)
		{
			SetNote(string.Format("V: put the cursor near the middle of a draft segment of %1 m or more", MUD_TrenchPlan.MIN_BEND_CHORD));
			return;
		}
		m_iBending = best;
		m_fBendFrom = m_Draft.m_aBends[best];
		SetNote("Bending: move the cursor to the side, LMB / V keeps, RMB puts it back");
	}

	//------------------------------------------------------------------------------------------------
	//! MUD_TrenchPlanner.MoveBend on the draft
	protected void MoveBend()
	{
		if (m_iBending < 0 || m_iBending >= m_Draft.m_aPoints.Count() - 1)
			return;
		vector a = m_Draft.m_aPoints[m_iBending];
		vector b = m_Draft.m_aPoints[m_iBending + 1];
		a[1] = 0;
		b[1] = 0;
		vector chord = b - a;
		float len = chord.Length();
		if (len < 0.001)
			return;
		vector right = chord * (1 / len);
		vector fwd = Vector(-right[2], 0, right[0]);
		vector rel = m_vAim - (a + b) * 0.5;
		rel[1] = 0;
		float maxBend = len * 0.5 * MUD_TrenchPlan.MAX_BEND;
		float bend = Math.Clamp(vector.Dot(rel, fwd), -maxBend, maxBend);
		if (Math.AbsFloat(bend) < MUD_TrenchPlanner.BEND_SNAP)
			bend = 0;
		m_Draft.m_aBends[m_iBending] = bend;
	}

	//------------------------------------------------------------------------------------------------
	//! Space: finish the trench line / tunnel being drawn
	protected void OnFinish(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_iTool == T_TRENCH)
			FinishTrench(false);
		else if (m_iTool == T_TUNNEL)
			FinishTunnel();
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! F: send every unsaved change (a line / tunnel being drawn is finished first); Ctrl+F also digs it all at once
	protected void OnSave(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		Save(GetGame().GetInputManager().GetActionValue("MUD_TrenchInstant") > 0.5);
	}

	//------------------------------------------------------------------------------------------------
	//! Send every unsaved change; instant = also dig it all at once (test, the server checks ALLOW_INSTANT_DIG)
	protected void Save(bool instant)
	{
		if (m_iTool == T_TRENCH && m_Draft.m_aPoints.Count() >= 2)
			FinishTrench(false);
		if (m_iTool == T_TUNNEL && m_aPts.Count() >= 2)
			FinishTunnel();
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;
		int sent = 0;
		foreach (MUD_BuildPending pend : m_aPending)
		{
			if (pend.m_iDelete != 0)
				pc.MUD_AskDeletePart(m_iArea, pend.m_iDelete);
			else
				pc.MUD_AskSavePart(m_iArea, pend.m_iKind, pend.m_sText);
			sent++;
		}
		if (instant)
			pc.MUD_AskInstantArea(m_iArea);
		Print(string.Format("MUD build manager: saved %1 changes to area %2, instant %3", sent, m_iArea, instant));
		for (int i = m_aPending.Count() - 1; i >= 0; i--)
		{
			PopPending(i);
		}
		m_bCloseAsked = false;
		if (sent == 0 && !instant)
			SetNote("Nothing to save");
		else
			SetNote(string.Format("Sent %1 changes - they show when the server placed them", sent));
	}

	//------------------------------------------------------------------------------------------------
	//! Backspace: close (twice with unsaved changes)
	protected void OnClose(float value = 0, EActionTrigger reason = 0, string actionName = string.Empty)
	{
		if (m_aPending.Count() > 0 && !m_bCloseAsked)
		{
			m_bCloseAsked = true;
			SetNote(string.Format("%1 unsaved changes: Backspace again drops them, F saves", m_aPending.Count()));
			return;
		}
		Close("Backspace");
	}

	//------------------------------------------------------------------------------------------------
	protected void SetNote(string note)
	{
		m_sNote = note;
		m_bDirty = true;
	}

	//================================================================================================
	// drawing

	//------------------------------------------------------------------------------------------------
	protected void Redraw()
	{
		float w = Math.Max(0.6, m_fH * 0.008);
		m_Draw.Begin();
		m_Draw.Square(m_vAim, Math.Max(0.3, m_fH * 0.008), 45, MUD_TrenchGhosts.RED, 0.1);
		if (m_iTool == T_TRENCH && m_bAttach)
		{
			vector snap = SnapTrench(m_vAim);
			if (vector.DistanceXZ(snap, MUD_WorldGroundEntity.SnapFine(m_vAim)) > 0.05)
				m_Draw.Square(snap, Math.Max(0.5, m_fH * 0.012), 0, MUD_BuildDraw.MARK, 0.12);     // it will attach here
		}
		DrawTool(w);
		foreach (MUD_BuildPending pend : m_aPending)
		{
			if (!pend.m_Spec)
				continue;
			int state = MUD_TrenchGhosts.GREEN;
			if (pend.m_iDelete != 0)
				state = MUD_TrenchGhosts.RED;
			if (pend.m_Spec.m_Layout)
			{
				if (pend.m_iDelete != 0)
				{
					array<vector> line = LinePoints(pend.m_Spec.m_Layout);
					m_Draw.Ribbon(line, w, state, false, 0.12);
				}
				continue;
			}
			float yaw = 0;
			if (pend.m_Spec.m_iKind == MUD_Build.DUGOUT)
				yaw = pend.m_Spec.m_fYaw;
			foreach (vector c : pend.m_Spec.m_aCaps)
			{
				m_Draw.Square(c, 0.35, yaw, state, 0.08);
			}
		}
		m_Draw.End();
	}

	//------------------------------------------------------------------------------------------------
	//! Centre line of a layout every ~2 m (delete marks)
	protected static array<vector> LinePoints(MUD_TrenchLayout lay)
	{
		array<vector> pts = {};
		float total = lay.ChainLength();
		int n = Math.Ceil(total / 2.0) + 1;
		for (int i = 0; i < n; i++)
		{
			pts.Insert(lay.ChainPoint(total * i / (n - 1)));
		}
		return pts;
	}

	//------------------------------------------------------------------------------------------------
	//! What the tool would make with the cursor where it is
	protected void DrawTool(float w)
	{
		if (m_iTool == T_TRENCH && MUD_BuildShapes.IsStamp(m_iShape))
		{
			m_DraftGhost.Clear();
			DrawStamp();
			return;
		}
		if (m_iTool == T_TRENCH)
		{
			if (m_bDragging)
			{
				m_DraftGhost.Clear();
				DrawDrag();
				return;
			}
			if (DragTool() && MUD_BuildShapes.IsBox(m_iShape))
			{
				// the box frame's x axis at the cursor: shows what R turned
				float a = m_fShapeAngle * Math.DEG2RAD;
				array<vector> tick = {};
				tick.Insert(m_vAim);
				tick.Insert(m_vAim + Vector(Math.Cos(a), 0, Math.Sin(a)) * Math.Max(2, m_fH * 0.06));
				m_Draw.Ribbon(tick, w * 0.5, MUD_TrenchGhosts.DUG, false, 0.1);
			}
			DrawTrenchDraft();
			return;
		}
		m_DraftGhost.Clear();
		if (m_bDragging)
		{
			DrawDrag();
			return;
		}
		if (m_iTool == T_STEP)
		{
			DrawStepPreview();
			return;
		}
		if (m_iTool == T_TUNNEL && m_aPts.Count() > 0)
		{
			array<vector> line = {};
			foreach (vector p : m_aPts)
			{
				line.Insert(p);
			}
			line.Insert(ClampFrom(m_aPts[m_aPts.Count() - 1], m_vAim, MUD_Build.PATH_MAX_GAP));
			array<vector> path = {};
			float pA;
			float pB;
			string pwhy;
			bool pok = TunnelPath(line, path, pA, pB, pwhy);
			if (!pok)
				path.Copy(line);
			TunnelPreview(path, pA, pB, pok);
		}
		else if (m_iTool == T_DUGOUT && m_aPts.Count() > 0)
		{
			vector mid;
			vector fwd;
			float doorD;
			if (!DugoutPlace(m_aPts[0], m_vAim, mid, fwd, doorD).IsEmpty())
				return;
			float l = MUD_Build.DugLength(m_iDugSize) * 0.5;
			float hw = MUD_Build.DugWidth(m_iDugSize) * 0.5;
			vector right = Vector(fwd[2], 0, -fwd[0]);
			array<vector> room = {};
			room.Insert(mid + fwd * l + right * hw);
			room.Insert(mid + fwd * l - right * hw);
			room.Insert(mid - fwd * l - right * hw);
			room.Insert(mid - fwd * l + right * hw);
			m_Draw.Ribbon(room, w * 0.5, MUD_TrenchGhosts.GREEN, true, 0.1);
			array<vector> entry = {};
			entry.Insert(m_aPts[0]);
			entry.Insert(mid - fwd * l);
			m_Draw.Ribbon(entry, w, MUD_TrenchGhosts.DUG, false, 0.1);
		}
		else if (m_iTool == T_COVER && m_iPick != 0)
		{
			MUD_BuildPartComponent t = MUD_BuildPartComponent.Find(m_iPick);
			if (!t || !t.Spec() || !t.Spec().m_Layout)
				return;
			MUD_TrenchLayout lay = t.Spec().m_Layout;
			float off;
			float s1 = lay.ChainPos(m_vAim[0], m_vAim[2], off);
			array<vector> roof = {};
			int n = 12;
			for (int k = 0; k <= n; k++)
			{
				roof.Insert(lay.ChainPoint(m_fCoverS0 + (s1 - m_fCoverS0) * k / n));
			}
			m_Draw.Ribbon(roof, Math.Max(w, lay.Half() * 2 + 0.4), MUD_TrenchGhosts.DUG, false, 0.12);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A drag in progress, drawn from the SAME plans the release will add (round 2b, user: "preview not accurate"):
	//! the floor as a green band, the top of the walls (floor half + depth x wall run) as bright edge lines.
	//! Shapes the plan limits refuse show their raw centre lines in red.
	protected void DrawDrag()
	{
		array<ref MUD_BuildStroke> strokes = {};
		DragStrokes(strokes);
		if (m_iTool == T_TUNNEL)
		{
			foreach (MUD_BuildStroke ts : strokes)
			{
				array<vector> tl = {};
				ts.Polyline(tl, 2);
				array<vector> tpath = {};
				float tA;
				float tB;
				string twhy;
				bool tok = TunnelPath(tl, tpath, tA, tB, twhy);
				if (!tok)
					tpath.Copy(tl);
				TunnelPreview(tpath, tA, tB, tok);
			}
			return;
		}
		array<string> texts = {};
		string why;
		float edge = Math.Max(0.12, m_fH * 0.003);
		if (!MUD_BuildShapes.ToPlans(strokes, m_Draft.m_iWidth, m_Draft.m_iLayers, m_Draft.m_iWall, texts, why))
		{
			foreach (MUD_BuildStroke st : strokes)
			{
				array<vector> raw = {};
				st.Polyline(raw, 6);
				if (raw.Count() >= 2)
					m_Draw.Ribbon(raw, edge * 2, MUD_TrenchGhosts.RED, false, 0.1);
			}
			return;
		}
		foreach (string text : texts)
		{
			MUD_TrenchLayout lay = new MUD_TrenchLayout();
			if (!lay.Build(MUD_TrenchPlan.Unpack(text)))
				continue;
			m_Draw.Ribbon(LinePoints(lay), lay.Half() * 2, MUD_TrenchGhosts.GREEN, false, 0.08);
			DrawEdges(m_Draw, lay, edge);
			if (m_iAutoStep == 0 || m_iStepLayers >= lay.Layers())
				continue;
			for (int k = 0; k < 2; k++)
			{
				if ((k == 0 && m_iAutoStep == 2) || (k == 1 && m_iAutoStep == 1))
					continue;
				array<string> steps = {};
				string swhy;
				if (!StepTexts(lay, 0, lay.ChainLength(), 1 - 2 * k, steps, swhy))
					continue;
				foreach (string stepText : steps)
				{
					MUD_TrenchLayout sl = new MUD_TrenchLayout();
					if (sl.Build(MUD_TrenchPlan.Unpack(stepText)))
						m_Draw.Ribbon(LinePoints(sl), sl.Half() * 2, MUD_BuildDraw.MARK, false, 0.09);     // platform: orange band
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The two lines where the trench's walls meet the ground (bright, so the real width reads from above)
	protected static void DrawEdges(MUD_BuildDraw draw, MUD_TrenchLayout lay, float width)
	{
		float top = lay.Half() + lay.Layers() * MUD_TrenchPlan.LAYER * lay.Run();
		float total = lay.ChainLength();
		int n = Math.Max(2, Math.Ceil(total / 0.5) + 1);
		array<vector> left = {};
		array<vector> right = {};
		for (int i = 0; i < n; i++)
		{
			float s = total * i / (n - 1);
			vector a = lay.ChainPoint(Math.Max(0, s - 0.25));
			vector b = lay.ChainPoint(Math.Min(total, s + 0.25));
			vector dir = Vector(b[0] - a[0], 0, b[2] - a[2]);
			float len = dir.Length();
			if (len < 0.0001)
				continue;
			vector side = MUD_BuildShapes.Left(dir * (1 / len)) * top;
			vector p = lay.ChainPoint(s);
			left.Insert(p + side);
			right.Insert(p - side);
		}
		draw.Ribbon(left, width, MUD_BuildDraw.MARK, false, 0.1);
		draw.Ribbon(right, width, MUD_BuildDraw.MARK, false, 0.1);
	}

	//------------------------------------------------------------------------------------------------
	//! Edge lines of every unsaved trench (own draw: rebuilt only when the list changes, not on every mouse move)
	protected void DrawPendingEdges()
	{
		m_PendDraw.Begin();
		float edge = Math.Max(0.12, m_fH * 0.003);
		foreach (MUD_BuildPending pend : m_aPending)
		{
			if (pend.m_iDelete == 0 && pend.m_Spec && pend.m_Spec.m_Layout)
				DrawEdges(m_PendDraw, pend.m_Spec.m_Layout, edge);
		}
		m_PendDraw.End();
	}

	//------------------------------------------------------------------------------------------------
	protected void DrawTrenchDraft()
	{
		int n = m_Draft.m_aPoints.Count();
		if (n == 0)
		{
			m_DraftGhost.Clear();
			return;
		}
		MUD_TrenchPlan live = MUD_TrenchPlan.Unpack(m_Draft.Pack());
		if (!live && n >= 2)
		{
			m_DraftGhost.Clear();
			return;
		}
		if (!live)
		{
			live = new MUD_TrenchPlan();
			live.m_iWidth = m_Draft.m_iWidth;
			live.m_iWall = m_Draft.m_iWall;
			live.m_iLayers = m_Draft.m_iLayers;
			live.AddPoint(m_Draft.m_aPoints[0]);
		}
		if (m_iBending < 0 && n < MUD_TrenchPlan.MAX_POINTS)
		{
			vector p = ClampFrom(m_Draft.m_aPoints[n - 1], SnapTrench(m_vAim), MUD_TrenchPlan.MAX_SEGMENT);
			if (vector.DistanceXZ(p, m_Draft.m_aPoints[n - 1]) >= MUD_TrenchPlan.MIN_SEGMENT)
				live.AddPoint(p);
		}
		MUD_TrenchLayout lay = new MUD_TrenchLayout();
		if (live.m_aPoints.Count() < 2 || !lay.Build(MUD_TrenchPlan.Unpack(live.Pack())))
		{
			m_DraftGhost.Clear();
			return;
		}
		array<int> states = {};
		GroundStates(lay, states);
		m_DraftGhost.Draw(lay, states);
		DrawEdges(m_Draw, lay, Math.Max(0.12, m_fH * 0.003));
	}

	//------------------------------------------------------------------------------------------------
	static string ToolName(int tool)
	{
		array<string> names = {"Trench", "Firing step", "Roof", "Tunnel", "Dugout", "Delete"};
		if (tool < 0 || tool >= names.Count())
			return "?";
		return names[tool];
	}

	//------------------------------------------------------------------------------------------------
	protected string ToolLine()
	{
		string s;
		for (int i = 0; i < TOOLS; i++)
		{
			if (i > 0)
				s += " | ";
			if (i == m_iTool)
				s += "[" + ToolName(i) + "]";
			else
				s += ToolName(i);
		}
		return s;
	}

	//------------------------------------------------------------------------------------------------
	protected string ToolKeys()
	{
		if (DragTool())
		{
			string how = "Drag: start -> end";
			if (m_iShape == MUD_BuildShapes.PENCIL)
				how = "Hold LMB and draw";
			else if (MUD_BuildShapes.IsBox(m_iShape))
				how = string.Format("Drag: corner -> corner (Shift: square)\nR: turn the frame (%1 deg)", m_fShapeAngle);
			else
				how = "Drag: start -> end (Shift: 15 deg steps)";
			return string.Format("Shape: %1\n%2\nStart / end near a trench = joins it\nRMB: cancel drag / undo last shape\nUp / Down: depth %3 m\nX: width %4 m\nC: walls %5 deg",
				MUD_BuildShapes.Name(m_iShape), how, m_Draft.m_iLayers * MUD_TrenchPlan.LAYER, m_Draft.Width(), MUD_TrenchPlan.WallDegOf(m_Draft.m_iWall));
		}
		if (m_iTool == T_TRENCH)
		{
			string bend = "V: bend the segment at the cursor";
			if (m_iBending >= 0)
				bend = "Bending: LMB / V keep, RMB put back";
			return string.Format("LMB: point (%1 / %2; near a trench = joins it)\nSpace: finish the line\nRMB: undo point\nUp / Down: depth %3 m\nX: width %4 m\nC: walls %5 deg\n%6",
				m_Draft.m_aPoints.Count(), MUD_TrenchPlan.MAX_POINTS, m_Draft.m_iLayers * MUD_TrenchPlan.LAYER, m_Draft.Width(), MUD_TrenchPlan.WallDegOf(m_Draft.m_iWall), bend);
		}
		if (m_iTool == T_STEP)
			return "Hover a trench on the side the platform goes\nClick start + click end along it\n(or Whole trench: one click)\nRMB: clear the start\nUp / Down: platform depth   X: width";
		if (m_iTool == T_COVER)
			return string.Format("LMB: roof start on a saved trench, LMB again: end\n%1 m at each end stays open as the way in\nRMB: clear the start", MUD_Build.COVER_ENTRY);
		if (m_iTool == T_TUNNEL)
			return string.Format("LMB: tunnel point (%1 / %2)\nSpace: finish\nEnds take the depth of the trench / dugout there,\nelse Up / Down: %3 m\nRMB: undo point", m_aPts.Count(), MUD_Build.PATH_MAX_POINTS, m_fTunnelD);
		if (m_iTool == T_DUGOUT)
			return string.Format("LMB: door (in a trench), LMB: room middle\nX: room %1 x %2 m\nFloor %3 m deep, entry slope added\nRMB: undo", MUD_Build.DugWidth(m_iDugSize), MUD_Build.DugLength(m_iDugSize), MUD_Build.DUGOUT_D);
		return "LMB: unsaved part = remove, saved part = mark / unmark for delete\n(dug ground stays)";
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHint(MUD_BuildAreaComponent area)
	{
		string rights = "everyone may edit";
		if (!area.IsOpen())
			rights = area.OwnerName() + " only (for now)";
		array<MUD_BuildPartComponent> parts = {};
		MUD_BuildPartComponent.ForArea(m_iArea, parts);
		string head = string.Format("Build area %1 - %2 - %3 saved parts\nPalette (top) or Q / E: tool   %4", m_iArea, rights, parts.Count(), ToolLine());
		string foot = string.Format("WASD / screen edge / hold middle mouse: move\nWheel: zoom at the cursor (%1 m up)\nF: save %2 changes (Ctrl+F: and dig all at once)\nBackspace: close\nYour body stays where it is and can be shot", Math.Round(m_fH), m_aPending.Count());
		string text = head + "\n\n" + ToolKeys() + "\n\n" + foot;
		if (!m_sNote.IsEmpty())
			text += "\n\n" + m_sNote;
		if (text == m_sShownHint)
			return;
		m_sShownHint = text;
		SCR_HintManagerComponent.ShowCustomHint(text, "Trench builder", 3600, true);
	}
}
