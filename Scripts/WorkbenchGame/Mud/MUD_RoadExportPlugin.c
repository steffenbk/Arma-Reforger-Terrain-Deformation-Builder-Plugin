#ifdef WORKBENCH
//! TEMP probe (whole-map cut plan Task 7A, 2026-10-18): can a World Editor plugin read every road's shape, width and
//! material, so the game can draw OUR copy of a cratered road? Per RoadEntity source it writes its Width, Material,
//! the SplinePoints it holds (local Position -> world via the entity) and, when its parent is a ShapeEntity, the
//! parent's tessellated shape (GenerateTesselatedShape). Open MudArland in the World Editor, run 'MUD Export Roads'.
//! Lines in $profile:mud_road_export.txt:
//!   MUD rexp road <i> origin <x y z> width <w> mat <material> type <Type> spline <n> parent <class> tess <m>
//!   MUD rexp sp <i> <x> <y> <z> tg <in xyz out xyz | ->  (spline points, world; explicit tangents as world directions)
//! Bake: Tools/mud_roads.py bake <this file> -> Data/Mud/mud_roads.c
//!   MUD rexp tp <i> <x> <y> <z>        (the spline's tessellated points = the curve, world; the bake prefers these)
//!   MUD rexp done roads <n> with spline <a> with parent shape <b> no entity <c>
[WorkbenchPluginAttribute(
	name: "MUD Export Roads",
	description: "Probe: write every road's points, width and material to $profile:mud_road_export.txt",
	wbModules: { "WorldEditor" },
	awesomeFontCode: 0xF018)]
class MUD_RoadExportPlugin : WorkbenchPlugin
{
	static const string FILE = "$profile:mud_road_export.txt";
	static const int ROAD_TESS_LIMIT = 100000;  // tessellated points written for the first N roads (all: the bake uses them)

	//------------------------------------------------------------------------------------------------
	override void Run()
	{
		WorldEditor worldEditor = Workbench.GetModule(WorldEditor);
		if (!worldEditor)
			return;
		WorldEditorAPI api = worldEditor.GetApi();
		if (!api)
			return;
		FileHandle f = FileIO.OpenFile(FILE, FileMode.WRITE);
		if (!f)
		{
			Print("MUD rexp: cannot open " + FILE, LogLevel.ERROR);
			return;
		}
		int roads;
		int withSpline;
		int withParent;
		int noEnt;
		for (int i, count = api.GetEditorEntityCount(); i < count; i++)
		{
			IEntitySource src = api.GetEditorEntity(i);
			if (!src || src.GetClassName() != "RoadEntity")
				continue;
			IEntity e = api.SourceToEntity(src);
			if (!e)
			{
				noEnt++;
				continue;
			}
			ResourceName mat;
			src.Get("Material", mat);
			float width;
			src.Get("Width", width);
			string type;
			src.Get("Type", type);

			// the road's own spline points (local to the road entity). World via the entity's world matrix, not
			// CoordToParent (= parent space; the 2026-10-18 export was right because the generators sit at the world origin - inference from 9/9 hits)
			vector wm[4];
			e.GetTransform(wm);
			array<vector> sp = {};
			array<string> tgs = {};
			BaseContainerList pts = src.GetObjectArray("SplinePoints");
			if (pts)
			{
				for (int p = 0; p < pts.Count(); p++)
				{
					BaseContainer pc = pts.Get(p);
					vector lp;
					if (!pc || !pc.Get("Position", lp))
						continue;
					sp.Insert(wm[3] + wm[0] * lp[0] + wm[1] * lp[1] + wm[2] * lp[2]);
					// explicit tangents (RoadGeneratorEntity.UpdateRoadEntity: Data[0] SplinePointData), turned to world
					// directions; none = '-' (the engine's own auto tangents apply - unknown rule)
					string tg = "-";
					BaseContainerList pdata = pc.GetObjectArray("Data");
					if (pdata && pdata.Count() > 0 && pdata.Get(0))
					{
						vector ti;
						vector tOut;
						pdata.Get(0).Get("InTangent", ti);
						pdata.Get(0).Get("OutTangent", tOut);
						vector wi = wm[0] * ti[0] + wm[1] * ti[1] + wm[2] * ti[2];
						vector wo = wm[0] * tOut[0] + wm[1] * tOut[1] + wm[2] * tOut[2];
						tg = string.Format("%1 %2 %3 %4 %5 %6", wi[0], wi[1], wi[2], wo[0], wo[1], wo[2]);
					}
					tgs.Insert(tg);
				}
			}
			if (!sp.IsEmpty())
				withSpline++;

			// the spline the road follows. Hierarchy (RoadGeneratorEntity.c, Exp 1.9): SplineShapeEntity >
			// RoadGeneratorEntity > RoadEntity. The 2026-10-18 export found no shape via source parents on any road,
			// so try source parent / grandparent, then runtime parent / grandparent.
			string parentClass = "-";
			array<vector> tp = {};
			IEntitySource ps = src.GetParent();
			if (ps)
				parentClass = ps.GetClassName();
			ShapeEntity shape;
			if (ps)
			{
				shape = ShapeEntity.Cast(api.SourceToEntity(ps));
				IEntitySource gps = ps.GetParent();
				if (!shape && gps)
					shape = ShapeEntity.Cast(api.SourceToEntity(gps));
			}
			if (!shape && e.GetParent())
			{
				shape = ShapeEntity.Cast(e.GetParent());
				if (!shape)
					shape = ShapeEntity.Cast(e.GetParent().GetParent());
			}
			if (shape)
			{
				parentClass = parentClass + ">" + shape.ClassName();
				shape.GenerateTesselatedShape(tp);
				vector sm[4];
				shape.GetTransform(sm);
				for (int t = 0; t < tp.Count(); t++)
				{
					vector q = tp[t];
					tp[t] = sm[3] + sm[0] * q[0] + sm[1] * q[1] + sm[2] * q[2];
				}
				withParent++;
			}

			vector o = e.GetOrigin();
			f.WriteLine(string.Format("MUD rexp road %1 origin %2 %3 %4 width %5 mat %6 type %7 spline %8 parent %9",
				roads, o[0], o[1], o[2], width, mat, type, sp.Count(), parentClass) + string.Format(" tess %1", tp.Count()));
			foreach (int k, vector v : sp)
				f.WriteLine(string.Format("MUD rexp sp %1 %2 %3 %4 tg %5", roads, v[0], v[1], v[2], tgs[k]));
			if (roads < ROAD_TESS_LIMIT)
			{
				foreach (vector w : tp)
					f.WriteLine(string.Format("MUD rexp tp %1 %2 %3 %4", roads, w[0], w[1], w[2]));
			}
			roads++;
		}
		string done = string.Format("MUD rexp done roads %1 with spline %2 with parent shape %3 no entity %4", roads, withSpline, withParent, noEnt);
		f.WriteLine(done);
		f.Close();
		Print(done + " -> " + FILE);
	}
}
#endif // WORKBENCH
