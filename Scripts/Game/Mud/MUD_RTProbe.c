//! P-N2 RENDER TARGET PROBE (TEST_KIT; research/2026-10-20-engine-native-deformation.md N2 + sec 6, built 2026-10-23,
//! checkpoint v297 = before). Question: can a mask painted by script (RTTextureWidget + CanvasWidget) drive a mud
//! material on our own mesh, and does a canvas that does not clear keep its old strokes?
//! Look only: local entity + widgets, nothing replicated, nothing in rut files, never on a DS. Called every frame from
//! MUD_MudLab.Tick (render machines). Keys (diag exes only):
//! - numpad 5: next material (off -> 12 materials -> off). Each material has "$rendertarget" in ONE slot
//!   (Assets/Mud/Probe/MUD_RTP_*.emat). The plane is placed 5 m in front of the camera when the probe starts.
//! - numpad 6: canvas CLEAR <-> KEEP. CLEAR (layout Clear 1): every stroke stays a draw command (redrawn each frame).
//!   KEEP (layout Clear 0): only the NEWEST stroke is a command - if the whole track stays visible, the target keeps
//!   its pixels (a true paint buffer). At each loop KEEP paints one black square over everything (does erase work?).
//! A white 'wheel track' grows across the 4 x 4 m plane, one segment every STEP_S, LOOP segments, then starts again.
//! G / H (2026-10-23, '3D look' retry - C used a hard 0/1 step at parallax 0.1, too weak to rule out):
//! - G paints a NORMAL map into NMOMap: neutral blue bg, each track edge drawn as a tilted-normal colour. INFERENCE:
//!   NMO = normal X/Y in R/G (128 = flat); if lighting does not change, report which colours the plane shows.
//! - H paints a soft groove (white = top, darker to the middle) into HeightMap at ParallaxScale 0.5.
//! I-L (2026-10-23, after G = 'two square tracks', H = nothing). INFERENCE: script meshes carry no tangents (IDA
//! 2026-10-14: Create allocates pos, UV0, indices, normals only), and normal maps + parallax need tangents. So:
//! - I: OUR script plane + vanilla Road_Dirt_01 BCR / NMO / H at parallax 0.03 (no RT) - control: any depth at all?
//! - J: the prebuilt MudTile.xob (Blender import, has tangents) + the same road material - control 2.
//! - K / L: MudTile.xob + the H soft groove / G normal paint. 'prebuilt mesh + masks', the dev's setup.
//! Log: 'MUD rt probe: ...' on every switch.
class MUD_RTProbe
{
	static const ResourceName LAYOUT_CLEAR = "{4D55445254504C00}UI/layouts/Mud/MUD_RTProbe_Clear.layout";
	static const ResourceName LAYOUT_KEEP = "{4D55445254504B00}UI/layouts/Mud/MUD_RTProbe_Keep.layout";
	static const float SIZE = 4;            // plane side, m
	static const int N = 8;                 // quads per side
	static const float LIFT = 0.03;         // over the ground
	static const float AHEAD = 5;           // plane middle this far in front of the camera
	static const float STEP_S = 0.25;
	static const int LOOP = 64;             // segments per pass
	static const float WIDTH = 0.06;        // stroke width, share of the canvas side (0.24 m on the plane)
	static const ResourceName XOB = "{A95F20716B75061A}Assets/Mud/MudTile.xob";

	protected static ref array<ResourceName> s_aMats;
	protected static ref array<string> s_aMatNames;
	protected static ref array<int> s_aBrush;   // 0 white line, 1 normal paint (G), 2 soft groove (H)
	protected static ref array<int> s_aXob;     // 1 = MudTile.xob instead of the script plane
	protected static int s_iMat = -1;       // -1 = off
	protected static bool s_bKeep;
	protected static float s_fTickLast = -1;
	protected static float s_fStepAt;
	protected static int s_iSeg;
	protected static vector s_vMid;
	protected static IEntity s_Ent;
	protected static ref Resource s_Res;    // the plane mesh goes with its last ref
	protected static Widget s_wRoot;
	protected static RTTextureWidget s_wRT;
	protected static CanvasWidget s_wCanvas;
	protected static ref array<ref CanvasWidgetCommand> s_aCmds;
	protected static ref TraceParam s_Trace;

	//------------------------------------------------------------------------------------------------
	protected static void Init()
	{
		s_aMats = {
			"{4D55445254503031}Assets/Mud/Probe/MUD_RTP_Colour.emat",
			"{4D55445254503032}Assets/Mud/Probe/MUD_RTP_Opacity.emat",
			"{4D55445254503033}Assets/Mud/Probe/MUD_RTP_Height.emat",
			"{4D55445254503034}Assets/Mud/Probe/MUD_RTP_MudBasicVfx.emat",
			"{4D55445254503035}Assets/Mud/Probe/MUD_RTP_MudMultiVfx.emat",
			"{4D55445254503036}Assets/Mud/Probe/MUD_RTP_MudMaskRt.emat",
			"{4D55445254503037}Assets/Mud/Probe/MUD_RTP_Normal.emat",
			"{4D55445254503038}Assets/Mud/Probe/MUD_RTP_HeightDeep.emat",
			"{4D55445254503039}Assets/Mud/Probe/MUD_RTP_RoadH.emat",
			"{4D55445254503039}Assets/Mud/Probe/MUD_RTP_RoadH.emat",
			"{4D55445254503038}Assets/Mud/Probe/MUD_RTP_HeightDeep.emat",
			"{4D55445254503037}Assets/Mud/Probe/MUD_RTP_Normal.emat"
		};
		s_aMatNames = {
			"A colour (MatPBRBasic BCRMap)",
			"B opacity (MatPBRBasic OpacityMap, AlphaTest 0.5)",
			"C height (MatPBRBasic HeightMap, ParallaxScale 0.1)",
			"D mud VFX (MatPBRBasic VFXMap)",
			"E mud VFX (MatPBRMulti VFXMap)",
			"F mud mask (MatPBRBasic MudMaskMap)",
			"G normal paint (MatPBRBasic NMOMap) - look low sun, both sides",
			"H soft groove (MatPBRBasic HeightMap, ParallaxScale 0.5)",
			"I CONTROL script plane + vanilla dirt road height (no RT)",
			"J CONTROL MudTile.xob + vanilla dirt road height (no RT)",
			"K MudTile.xob + H soft groove",
			"L MudTile.xob + G normal paint"
		};
		s_aBrush = {0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 2, 1};
		s_aXob = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1};
		s_aCmds = {};
	}

	//------------------------------------------------------------------------------------------------
	//! Every frame from MUD_MudLab.Tick (render machines)
	static void Tick(float time)
	{
		if (!MUD_TestKit.TEST_KIT)
			return;
		if (!s_aMats)
			Init();
		// a new play keeps the statics: the old world's entity is gone, drop everything
		if (time < s_fTickLast)
		{
			s_Ent = null;
			Stop();
			s_iMat = -1;
		}
		s_fTickLast = time;
#ifdef ENABLE_DIAG
		if (Debug.KeyState(KeyCode.KC_NUMPAD5))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD5);
			Next(time);
		}
		if (Debug.KeyState(KeyCode.KC_NUMPAD6))
		{
			Debug.ClearKey(KeyCode.KC_NUMPAD6);
			s_bKeep = !s_bKeep;
			if (s_iMat >= 0)
				Start(time);
			Show();
		}
#endif
		if (s_iMat < 0 || !s_wCanvas)
			return;
		if (time - s_fStepAt < STEP_S)
			return;
		s_fStepAt = time;
		Stroke();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Next(float time)
	{
		if (s_iMat < 0)
			PlaceAtCamera();
		s_iMat++;
		if (s_iMat >= s_aMats.Count())
		{
			s_iMat = -1;
			Stop();
			Show();
			return;
		}
		Start(time);
		Show();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show()
	{
		string mode = "CLEAR";
		if (s_bKeep)
			mode = "KEEP";
		string what = "OFF";
		if (s_iMat >= 0)
			what = s_aMatNames[s_iMat];
		string line = string.Format("MUD rt probe: %1 | canvas %2 | numpad 5 next material, numpad 6 clear / keep", what, mode);
		Print(line, LogLevel.NORMAL);
		SCR_HintManagerComponent.ShowCustomHint(what + "\ncanvas " + mode + " (numpad 5 / 6)", "Mud RT probe", 6, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PlaceAtCamera()
	{
		vector cam[4];
		GetGame().GetWorld().GetCurrentCamera(cam);
		vector fwd = cam[2];
		fwd[1] = 0;
		if (fwd.LengthSq() < 0.0001)
			fwd = vector.Forward;
		fwd.Normalize();
		s_vMid = cam[3] + fwd * AHEAD;
		s_vMid[1] = GroundY(s_vMid[0], s_vMid[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Top of whatever is there (terrain, our ground entities), no plane yet
	protected static float GroundY(float x, float z)
	{
		// trace from above the camera: GetSurfaceY is -256 on holed cells (first WB run 2026-10-23: plane at y -256)
		BaseWorld world = GetGame().GetWorld();
		vector cam[4];
		world.GetCurrentCamera(cam);
		vector camPos = cam[3];
		float top = camPos[1] + 30;
		if (!s_Trace)
			s_Trace = new TraceParam();
		s_Trace.Start = Vector(x, top, z);
		s_Trace.End = Vector(x, top - 200, z);
		s_Trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		s_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		s_Trace.Exclude = s_Ent;
		float f = world.TraceMove(s_Trace, NotMovers);
		if (f >= 1)
			return camPos[1] - 1.7;
		return top - 200 * f;
	}

	//------------------------------------------------------------------------------------------------
	//! Trace filter: skip vehicles and characters (the plane landed on a truck roof, WB 2026-10-23)
	protected static bool NotMovers(notnull IEntity e)
	{
		IEntity root = e.GetRootParent();
		return !Vehicle.Cast(root) && !ChimeraCharacter.Cast(root);
	}

	//------------------------------------------------------------------------------------------------
	//! (Re)build the plane with the current material and fresh widgets
	protected static void Start(float time)
	{
		Stop();
		if (RplSession.Mode() == RplMode.Dedicated)
			return;
		ResourceName mat = s_aMats[s_iMat];
		bool ok;
		if (s_aXob[s_iMat])
			ok = SpawnXob(mat);
		else
			ok = SpawnPlane(mat);
		if (!ok)
			return;

		ResourceName layout = LAYOUT_CLEAR;
		if (s_bKeep)
			layout = LAYOUT_KEEP;
		s_wRoot = GetGame().GetWorkspace().CreateWidgets(layout);
		if (!s_wRoot)
		{
			Print(string.Format("MUD rt probe: layout '%1' did not load", layout), LogLevel.ERROR);
			return;
		}
		s_wRT = RTTextureWidget.Cast(s_wRoot.FindAnyWidget("RT"));
		s_wCanvas = CanvasWidget.Cast(s_wRoot.FindAnyWidget("Canvas"));
		if (!s_wRT || !s_wCanvas)
		{
			Print("MUD rt probe: RT or Canvas widget missing in the layout", LogLevel.ERROR);
			return;
		}
		s_wRT.SetRenderTarget(s_Ent);
		s_aCmds.Clear();
		AddBackground(Units());
		s_wCanvas.SetDrawCommands(s_aCmds);
		s_iSeg = 0;
		s_fStepAt = time;
		vector units = s_wCanvas.GetSizeInUnits();
		Print(string.Format("MUD rt probe: plane at %1, canvas %2 x %3 units, %4 px per unit", s_vMid, units[0], units[1], s_wCanvas.PixelPerUnit()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SpawnPlane(ResourceName mat)
	{
		// MeshObject takes static arrays; both windings, so the plane shows whichever way is up. The back faces use
		// their OWN vertex copy (81..161): shared verts summed an up and a down face normal = zero normal (2026-10-23)
		vector verts[162];
		float uvs[324];
		int idx[768];
		float half = SIZE * 0.5;
		float nf = N;                           // float copies: int / int would floor
		float cell = SIZE / nf;
		for (int j = 0; j <= N; j++)
		{
			float fj = j;
			for (int i = 0; i <= N; i++)
			{
				float fi = i;
				int v = j * (N + 1) + i;
				float x = s_vMid[0] - half + fi * cell;
				float z = s_vMid[2] - half + fj * cell;
				verts[v] = Vector(x - s_vMid[0], LIFT, z - s_vMid[2]);   // flat: per-corner traces bent it over a truck (WB 2026-10-23)
				uvs[v * 2] = fi / nf;
				uvs[v * 2 + 1] = 1 - fj / nf;   // canvas y runs down: v = 0 at the far edge
				verts[v + 81] = verts[v];
				uvs[v * 2 + 162] = uvs[v * 2];
				uvs[v * 2 + 163] = uvs[v * 2 + 1];
			}
		}
		int k;
		for (int qj = 0; qj < N; qj++)
		{
			for (int qi = 0; qi < N; qi++)
			{
				int a = qj * (N + 1) + qi;      // (i, j)
				int b = a + N + 1;              // (i, j + 1)
				int c = a + 1;                  // (i + 1, j)
				int d = b + 1;                  // (i + 1, j + 1)
				idx[k] = a; idx[k + 1] = b; idx[k + 2] = c;
				idx[k + 3] = c; idx[k + 4] = b; idx[k + 5] = d;
				idx[k + 6] = a + 81; idx[k + 7] = c + 81; idx[k + 8] = b + 81;
				idx[k + 9] = c + 81; idx[k + 10] = d + 81; idx[k + 11] = b + 81;
				k += 12;
			}
		}
		int numVerts[] = {162};
		int numIndices[] = {768};
		string materials[] = {mat};
		s_Res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!s_Res || !s_Res.GetResource())
		{
			Print(string.Format("MUD rt probe: MeshObject.Create failed - material '%1' did not load?", mat), LogLevel.ERROR);
			return false;
		}
		MeshObject mesh = s_Res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, idx);
		mesh.UpdateVerts(0, verts, uvs);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = s_vMid;
		s_Ent = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		if (!s_Ent)
			return false;
		s_Ent.SetObject(mesh, "");
		s_Ent.Update();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The prebuilt MudTile.xob (imported mesh = engine tangents) with its one material remapped to mat
	protected static bool SpawnXob(ResourceName mat)
	{
		s_Res = Resource.Load(XOB);
		if (!s_Res || !s_Res.IsValid())
		{
			Print(string.Format("MUD rt probe: '%1' did not load", XOB), LogLevel.ERROR);
			return false;
		}
		VObject vo = s_Res.GetResource().ToVObject();
		if (!vo)
			return false;
		// remap source = the name the mesh holds now (KB render-targets: the authored name fails)
		string mats[16];
		int n = vo.GetMaterials(mats);
		string remap;
		for (int i = 0; i < n; i++)
		{
			remap += string.Format("$remap '%1' '%2';", mats[i], mat);
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = s_vMid + Vector(0, LIFT, 0);
		s_Ent = GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
		if (!s_Ent)
			return false;
		s_Ent.SetObject(vo, remap);
		s_Ent.Update();
		vector mins, maxs;
		s_Ent.GetBounds(mins, maxs);
		Print(string.Format("MUD rt probe: MudTile.xob %1 material(s), first '%2', bounds %3 .. %4", n, mats[0], mins, maxs), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stop()
	{
		if (s_wRT && s_Ent && !s_Ent.IsDeleted())
			s_wRT.RemoveRenderTarget(s_Ent);
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();
		s_wRoot = null;
		s_wRT = null;
		s_wCanvas = null;
		if (s_aCmds)
			s_aCmds.Clear();
		if (s_Ent && !s_Ent.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(s_Ent);
		s_Ent = null;
		s_Res = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Track point at pass share t (0..1), in canvas units
	protected static vector TrackAt(float t, vector units)
	{
		float x = 0.1 + 0.8 * t;
		float y = 0.5 + 0.25 * Math.Sin(t * Math.PI2 * 1.5);
		return Vector(x * units[0], y * units[1], 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Black background + red corner square (WB 2026-10-23: A was plain white - unbound target, or a white
	//! target background hiding the white track? Black + red tells them apart)
	protected static void AddBackground(vector units)
	{
		PolygonDrawCommand bg = new PolygonDrawCommand();
		bg.m_iColor = ARGB(255, 0, 0, 0);
		if (s_aBrush[s_iMat] == 1)
			bg.m_iColor = ARGB(255, 128, 128, 255);     // flat normal (INFERENCE: R/G = X/Y)
		else if (s_aBrush[s_iMat] == 2)
			bg.m_iColor = ARGB(255, 255, 255, 255);     // white = top with ParallaxMidlePlane 1 (INFERENCE)
		bg.m_Vertices = {0, 0, units[0], 0, units[0], units[1], 0, units[1]};
		s_aCmds.Insert(bg);
		float r = 0.15 * units[0];
		PolygonDrawCommand red = new PolygonDrawCommand();
		red.m_iColor = ARGB(255, 255, 0, 0);
		red.m_Vertices = {0, 0, r, 0, r, r, 0, r};
		s_aCmds.Insert(red);
	}

	//------------------------------------------------------------------------------------------------
	protected static vector Units()
	{
		vector units = s_wCanvas.GetSizeInUnits();
		if (units[0] <= 0 || units[1] <= 0)
			units = Vector(1024, 1024, 0);
		return units;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddLine(vector p0, vector p1, float w, int color)
	{
		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = color;
		line.m_fWidth = w;
		line.m_Vertices = {p0[0], p0[1], p1[0], p1[1]};
		s_aCmds.Insert(line);
	}

	//------------------------------------------------------------------------------------------------
	//! G: two edge strips with normals tilted toward the track middle, flat floor between
	protected static void AddNormalStroke(vector p0, vector p1, float w)
	{
		vector d = p1 - p0;
		d[2] = 0;
		if (d.LengthSq() < 0.0001)
			return;
		d.Normalize();
		vector side = Vector(-d[1], d[0], 0);           // canvas-space left of the stroke
		vector off = side * (w * 0.5);
		// each wall tilts toward the track middle: the left (+side) wall faces -side, the right wall +side; 128 +- 90
		int lr = 128 - Math.Round(side[0] * 90);
		int lg = 128 + Math.Round(side[1] * 90);        // canvas y runs down, normal map y up (INFERENCE)
		int rr = 128 + Math.Round(side[0] * 90);
		int rg = 128 - Math.Round(side[1] * 90);
		AddLine(p0 + off, p1 + off, w * 0.5, ARGB(255, lr, lg, 230));
		AddLine(p0 - off, p1 - off, w * 0.5, ARGB(255, rr, rg, 230));
	}

	//------------------------------------------------------------------------------------------------
	//! H: wide light to narrow dark = a soft round groove
	protected static void AddSoftStroke(vector p0, vector p1, float w)
	{
		for (int i = 0; i < 4; i++)
		{
			float fi = i;
			int c = 192 - i * 64;
			AddLine(p0, p1, w * 2 * (1 - fi * 0.22), ARGB(255, c, c, c));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CLEAR: background + every stroke so far. KEEP: background only with the first stroke of a pass (erase
	//! test at each loop), then only the newest stroke - the old track stays only if the target keeps pixels.
	protected static void Stroke()
	{
		vector units = Units();
		if (s_iSeg >= LOOP)
			s_iSeg = 0;
		if (s_iSeg == 0 || s_bKeep)
			s_aCmds.Clear();
		if (s_iSeg == 0)
			AddBackground(units);
		float seg = s_iSeg;
		float loop = LOOP;
		vector p0 = TrackAt(seg / loop, units);
		vector p1 = TrackAt((seg + 1) / loop, units);
		float w = WIDTH * units[0];
		if (s_aBrush[s_iMat] == 1)
			AddNormalStroke(p0, p1, w);
		else if (s_aBrush[s_iMat] == 2)
			AddSoftStroke(p0, p1, w);
		else
			AddLine(p0, p1, w, ARGB(255, 255, 255, 255));
		s_wCanvas.SetDrawCommands(s_aCmds);
		s_iSeg++;
	}
}
