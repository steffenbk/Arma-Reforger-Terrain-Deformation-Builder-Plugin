//! Trench builder F4, round 2: the manager's Paint-style palette at the top of the screen. Row 1 = the tools
//! (trench, firing step, roof, tunnel, dugout, delete), rows 2-3 = the trench shapes (MUD_BuildShapes). Drawn on one
//! full-screen CanvasWidget (UI/layouts/Mud/MUD_BuildPalette.layout); the manager hit-tests the mouse itself, so no
//! widget events. The canvas is sized in native pixels (SetSizeInUnits = workspace size): 1 unit = 1 mouse pixel.
//! INFERENCE (check in WB): a script-sized canvas maps units to pixels 1:1 on both axes.

//------------------------------------------------------------------------------------------------
//! One line of the settings panel (round 2b, user 2026-10-24: "every parameter a clickable menu, not keys")
class MUD_BuildRow
{
	static const int BUTTON = 0;              // the whole line is a button
	static const int CHECK = 1;               // a check box: click toggles
	static const int STEP = 2;                // - / + boxes at the right
	static const int CYCLE = 3;               // click = next value

	int m_iId;
	int m_iKind;
	string m_sLabel;
	string m_sValue;
	bool m_bOn;

	//------------------------------------------------------------------------------------------------
	void MUD_BuildRow(int id, int kind, string label, string value = "", bool on = false)
	{
		m_iId = id;
		m_iKind = kind;
		m_sLabel = label;
		m_sValue = value;
		m_bOn = on;
	}
}

//------------------------------------------------------------------------------------------------
class MUD_BuildPalette
{
	static const ResourceName LAYOUT = "{4D554442504C5400}UI/layouts/Mud/MUD_BuildPalette.layout";
	static const float CELL = 48;             // reference px (DPI scaled)
	static const float GAP = 4;
	static const float TOP = 16;
	static const float LABEL = 13;            // text size, reference px
	static const int PER_ROW = 6;
	static const float ROW_H = 30;            // settings panel (top right), reference px
	static const float ROW_W = 260;
	static const float ROW_MARGIN = 16;
	static const int ZONE_ROW = 0;            // HitRow code = row id * 4 + zone
	static const int ZONE_MINUS = 1;
	static const int ZONE_PLUS = 2;

	protected Widget m_wRoot;
	protected CanvasWidget m_wCanvas;
	protected ref array<ref CanvasWidgetCommand> m_aCmds = {};
	protected ref array<ref array<ref array<vector>>> m_aIcons = {};   // shape icons, unit box, built once
	protected int m_iW;
	protected int m_iH;
	protected string m_sDrawn;

	//------------------------------------------------------------------------------------------------
	void ~MUD_BuildPalette()
	{
		Close();
	}

	//------------------------------------------------------------------------------------------------
	bool Open()
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws)
			return false;
		m_wRoot = ws.CreateWidgets(LAYOUT);
		if (!m_wRoot)
		{
			Print(string.Format("MUD build palette: layout '%1' did not load", LAYOUT), LogLevel.ERROR);
			return false;
		}
		m_wCanvas = CanvasWidget.Cast(m_wRoot.FindAnyWidget("Canvas"));
		if (!m_wCanvas)
		{
			Print("MUD build palette: no Canvas widget in the layout", LogLevel.ERROR);
			Close();
			return false;
		}
		m_wCanvas.SetDrawCommands(m_aCmds);
		m_aIcons.Clear();
		for (int s = 0; s < MUD_BuildShapes.SHAPES; s++)
		{
			array<ref array<vector>> lines = {};
			MUD_BuildShapes.IconLines(s, lines);
			UnitBox(lines);
			m_aIcons.Insert(lines);
		}
		m_sDrawn = "";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
		m_wRoot = null;
		m_wCanvas = null;
		m_aCmds.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Icon lines scaled into 0..1 (x right, y DOWN: world z up = screen up), aspect kept, centred
	protected static void UnitBox(array<ref array<vector>> lines)
	{
		float x0 = 1000000;
		float z0 = 1000000;
		float x1 = -1000000;
		float z1 = -1000000;
		foreach (array<vector> line : lines)
		{
			foreach (vector p : line)
			{
				x0 = Math.Min(x0, p[0]);
				z0 = Math.Min(z0, p[2]);
				x1 = Math.Max(x1, p[0]);
				z1 = Math.Max(z1, p[2]);
			}
		}
		float size = Math.Max(Math.Max(x1 - x0, z1 - z0), 0.001);
		float padX = (1 - (x1 - x0) / size) * 0.5;
		float padZ = (1 - (z1 - z0) / size) * 0.5;
		foreach (array<vector> l : lines)
		{
			for (int i = 0; i < l.Count(); i++)
			{
				vector q = l[i];
				l[i] = Vector(padX + (q[0] - x0) / size, padZ + (z1 - q[2]) / size, 0);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Items: 0..TOOLS-1 = tools, then TOOLS + shape
	static int Items()
	{
		return MUD_BuildManager.TOOLS + MUD_BuildShapes.SHAPES;
	}

	//------------------------------------------------------------------------------------------------
	//! Top-left corner of item i in native px
	protected void CellPos(int i, out float x, out float y)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		float cell = ws.DPIScale(CELL);
		float gap = ws.DPIScale(GAP);
		float row;
		float col;
		if (i < MUD_BuildManager.TOOLS)
		{
			row = 0;
			col = i;
		}
		else
		{
			int k = i - MUD_BuildManager.TOOLS;
			int r = 1 + k / PER_ROW;     // int division: the row
			int c = k % PER_ROW;
			row = r;
			col = c;
		}
		float total = PER_ROW * cell + (PER_ROW - 1) * gap;
		x = (m_iW - total) * 0.5 + col * (cell + gap);
		y = ws.DPIScale(TOP) + row * (cell + gap);
		if (row > 0)
			y += gap * 2;     // a little space between tools and shapes
	}

	//------------------------------------------------------------------------------------------------
	//! Item under the mouse (native px), -1 = none
	int HitTest(int mx, int my)
	{
		if (!m_wCanvas)
			return -1;
		float cell = GetGame().GetWorkspace().DPIScale(CELL);
		for (int i = 0; i < Items(); i++)
		{
			float x;
			float y;
			CellPos(i, x, y);
			if (mx >= x && mx < x + cell && my >= y && my < y + cell)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! True when (mx, my) is over the icons or the settings panel (clicks there never reach the ground)
	bool Over(int mx, int my, array<ref MUD_BuildRow> rows)
	{
		return OverIcons(mx, my) || OverRows(mx, my, rows);
	}

	//------------------------------------------------------------------------------------------------
	protected bool OverRows(int mx, int my, array<ref MUD_BuildRow> rows)
	{
		if (!m_wCanvas || !rows || rows.IsEmpty())
			return false;
		WorkspaceWidget ws = GetGame().GetWorkspace();
		float pad = ws.DPIScale(GAP) * 2;
		float x0 = RowsX();
		float y0 = ws.DPIScale(TOP);
		return mx >= x0 - pad && mx < x0 + ws.DPIScale(ROW_W) + pad && my >= y0 - pad && my < y0 + rows.Count() * ws.DPIScale(ROW_H) + pad;
	}

	//------------------------------------------------------------------------------------------------
	protected float RowsX()
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		return m_iW - ws.DPIScale(ROW_MARGIN) - ws.DPIScale(ROW_W);
	}

	//------------------------------------------------------------------------------------------------
	//! Settings row under the mouse: row id * 4 + zone (ZONE_*), -1 = none
	int HitRow(int mx, int my, array<ref MUD_BuildRow> rows)
	{
		if (!OverRows(mx, my, rows))
			return -1;
		WorkspaceWidget ws = GetGame().GetWorkspace();
		float rowH = ws.DPIScale(ROW_H);
		float x0 = RowsX();
		float y0 = ws.DPIScale(TOP);
		float fi = (my - y0) / rowH;
		int i = Math.Floor(fi);
		if (i < 0 || i >= rows.Count() || mx < x0 || mx >= x0 + ws.DPIScale(ROW_W))
			return -1;
		MUD_BuildRow row = rows[i];
		int zone = ZONE_ROW;
		if (row.m_iKind == MUD_BuildRow.STEP)
		{
			float right = x0 + ws.DPIScale(ROW_W);
			if (mx >= right - rowH)
				zone = ZONE_PLUS;
			else if (mx >= right - rowH * 2)
				zone = ZONE_MINUS;
		}
		return row.m_iId * 4 + zone;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OverIcons(int mx, int my)
	{
		if (!m_wCanvas)
			return false;
		float x0;
		float y0;
		float x1;
		float y1;
		CellPos(0, x0, y0);
		CellPos(Items() - 1, x1, y1);
		float cell = GetGame().GetWorkspace().DPIScale(CELL);
		float pad = GetGame().GetWorkspace().DPIScale(GAP) * 2;
		float right = x0 + PER_ROW * cell + (PER_ROW - 1) * GetGame().GetWorkspace().DPIScale(GAP);
		return mx >= x0 - pad && mx < right + pad && my >= y0 - pad && my < y1 + cell + pad;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuild the draw commands when anything shown changed
	void Draw(int tool, int shape, int hover, array<ref MUD_BuildRow> rows, int rowHover)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!m_wCanvas || !ws)
			return;
		int w = ws.GetWidth();
		int h = ws.GetHeight();
		string key = string.Format("%1 %2 %3 %4 %5 %6", tool, shape, hover, w, h, rowHover);
		foreach (MUD_BuildRow kr : rows)
		{
			key += string.Format("|%1 %2 %3", kr.m_sLabel, kr.m_sValue, kr.m_bOn);
		}
		if (key == m_sDrawn)
			return;
		m_sDrawn = key;
		if (w != m_iW || h != m_iH)
		{
			m_iW = w;
			m_iH = h;
			m_wCanvas.SetSizeInUnits(Vector(w, h, 0));
		}
		m_aCmds.Clear();
		float cell = ws.DPIScale(CELL);
		float gap = ws.DPIScale(GAP);
		float bx0;
		float by0;
		float bx1;
		float by1;
		CellPos(0, bx0, by0);
		CellPos(Items() - 1, bx1, by1);
		float right = bx0 + PER_ROW * cell + (PER_ROW - 1) * gap;
		Box(bx0 - gap * 2, by0 - gap * 2, right + gap * 2, by1 + cell + gap * 2, ARGB(170, 15, 15, 15));
		for (int i = 0; i < Items(); i++)
		{
			float x;
			float y;
			CellPos(i, x, y);
			bool shapesOn = tool == MUD_BuildManager.T_TRENCH || tool == MUD_BuildManager.T_TUNNEL;     // shapes draw trenches or tunnels
			bool picked = (i == tool) || (shapesOn && i == MUD_BuildManager.TOOLS + shape);
			int bg = ARGB(200, 55, 55, 55);
			if (picked)
				bg = ARGB(230, 60, 105, 160);
			else if (i == hover)
				bg = ARGB(220, 90, 90, 90);
			Box(x, y, x + cell, y + cell, bg);
			int ink = ARGB(255, 235, 235, 235);
			if (i >= MUD_BuildManager.TOOLS && !shapesOn)
				ink = ARGB(255, 150, 150, 150);     // dim while a tool that draws no shapes is on
			if (i < MUD_BuildManager.TOOLS)
				ToolIcon(i, x, y, cell, ink);
			else
				ShapeIcon(i - MUD_BuildManager.TOOLS, x, y, cell, ink);
		}
		if (hover >= 0)
		{
			float hx;
			float hy;
			CellPos(hover, hx, hy);
			Text(ItemName(hover), hx, hy + cell + gap, ARGB(255, 255, 255, 255));
		}
		DrawRows(rows, rowHover);
	}

	//------------------------------------------------------------------------------------------------
	//! The settings panel: one line per row, label left, value / box / - + right
	protected void DrawRows(array<ref MUD_BuildRow> rows, int rowHover)
	{
		if (!rows || rows.IsEmpty())
			return;
		WorkspaceWidget ws = GetGame().GetWorkspace();
		float rowH = ws.DPIScale(ROW_H);
		float rowW = ws.DPIScale(ROW_W);
		float gap = ws.DPIScale(GAP);
		float x0 = RowsX();
		float y0 = ws.DPIScale(TOP);
		Box(x0 - gap * 2, y0 - gap * 2, x0 + rowW + gap * 2, y0 + rows.Count() * rowH + gap * 2, ARGB(170, 15, 15, 15));
		int white = ARGB(255, 235, 235, 235);
		for (int i = 0; i < rows.Count(); i++)
		{
			MUD_BuildRow row = rows[i];
			float y = y0 + i * rowH;
			float textY = y + (rowH - ws.DPIScale(LABEL)) * 0.45;
			int hoverZone = -1;
			if (rowHover >= 0 && rowHover / 4 == row.m_iId)
				hoverZone = rowHover % 4;
			int bg = ARGB(200, 45, 45, 45);
			if (row.m_iKind != MUD_BuildRow.STEP && hoverZone == ZONE_ROW)
				bg = ARGB(220, 90, 90, 90);
			Box(x0, y + 1, x0 + rowW, y + rowH - 1, bg);
			float right = x0 + rowW;
			if (row.m_iKind == MUD_BuildRow.CHECK)
			{
				float b = rowH * 0.55;
				float bx = x0 + gap * 2;
				float by = y + (rowH - b) * 0.5;
				Box(bx, by, bx + b, by + b, ARGB(255, 20, 20, 20));
				if (row.m_bOn)
					Box(bx + b * 0.2, by + b * 0.2, bx + b * 0.8, by + b * 0.8, ARGB(255, 90, 200, 110));
				Text(row.m_sLabel, bx + b + gap * 2, textY, white);
				continue;
			}
			Text(row.m_sLabel, x0 + gap * 2, textY, white);
			if (row.m_iKind == MUD_BuildRow.STEP)
			{
				int minusBg = ARGB(255, 70, 70, 70);
				int plusBg = ARGB(255, 70, 70, 70);
				if (hoverZone == ZONE_MINUS)
					minusBg = ARGB(255, 110, 110, 110);
				if (hoverZone == ZONE_PLUS)
					plusBg = ARGB(255, 110, 110, 110);
				Box(right - rowH * 2 + 2, y + 3, right - rowH - 2, y + rowH - 3, minusBg);
				Box(right - rowH + 2, y + 3, right - 2, y + rowH - 3, plusBg);
				Text("-", right - rowH * 1.62, textY, white);
				Text("+", right - rowH * 0.65, textY, white);
				right = right - rowH * 2;
			}
			if (!row.m_sValue.IsEmpty())
				Text(row.m_sValue, right - rowW * 0.38, textY, ARGB(255, 255, 210, 120));
		}
	}

	//------------------------------------------------------------------------------------------------
	static string ItemName(int i)
	{
		if (i < 0)
			return "";
		if (i < MUD_BuildManager.TOOLS)
			return MUD_BuildManager.ToolName(i);
		return "Shape: " + MUD_BuildShapes.Name(i - MUD_BuildManager.TOOLS);
	}

	//------------------------------------------------------------------------------------------------
	protected void Box(float x0, float y0, float x1, float y1, int color)
	{
		PolygonDrawCommand box = new PolygonDrawCommand();
		box.m_iColor = color;
		box.m_Vertices = {x0, y0, x1, y0, x1, y1, x0, y1};
		m_aCmds.Insert(box);
	}

	//------------------------------------------------------------------------------------------------
	protected void Text(string text, float x, float y, int color)
	{
		TextDrawCommand t = new TextDrawCommand();
		t.m_iColor = color;
		t.m_sText = text;
		t.m_fSize = GetGame().GetWorkspace().DPIScale(LABEL);
		t.m_Position = Vector(x, y, 0);
		t.m_iFontPropertiesId = 0;
		m_aCmds.Insert(t);
	}

	//------------------------------------------------------------------------------------------------
	//! One polyline in the cell; pts in 0..1 cell space (y down)
	protected void Line(array<vector> pts, float x, float y, float cell, int color)
	{
		if (pts.Count() < 2)
			return;
		float pad = cell * 0.18;
		float inner = cell - pad * 2;
		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = color;
		line.m_fWidth = Math.Max(1.5, cell * 0.045);
		line.m_Vertices = new array<float>();
		foreach (vector p : pts)
		{
			line.m_Vertices.Insert(x + pad + p[0] * inner);
			line.m_Vertices.Insert(y + pad + p[1] * inner);
		}
		m_aCmds.Insert(line);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShapeIcon(int shape, float x, float y, float cell, int color)
	{
		if (shape < 0 || shape >= m_aIcons.Count())
			return;
		foreach (array<vector> line : m_aIcons[shape])
		{
			Line(line, x, y, cell, color);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A hand-drawn icon line: xy pairs in 0..1 cell space (y down)
	protected void Hand(array<float> xy, float x, float y, float cell, int color)
	{
		array<vector> pts = {};
		for (int i = 0; i + 1 < xy.Count(); i += 2)
		{
			pts.Insert(Vector(xy[i], xy[i + 1], 0));
		}
		Line(pts, x, y, cell, color);
	}

	//------------------------------------------------------------------------------------------------
	//! Tool icons: cross-sections / plans drawn by hand
	protected void ToolIcon(int tool, float x, float y, float cell, int color)
	{
		if (tool == MUD_BuildManager.T_TRENCH)
		{
			array<float> trench = {0.0, 0.3, 0.25, 0.3, 0.38, 0.8, 0.62, 0.8, 0.75, 0.3, 1.0, 0.3};
			Hand(trench, x, y, cell, color);
		}
		else if (tool == MUD_BuildManager.T_STEP)
		{
			array<float> step = {0.0, 0.3, 0.2, 0.3, 0.3, 0.8, 0.55, 0.8, 0.55, 0.55, 0.75, 0.55, 0.82, 0.3, 1.0, 0.3};
			Hand(step, x, y, cell, color);
		}
		else if (tool == MUD_BuildManager.T_COVER)
		{
			array<float> ground = {0.0, 0.3, 1.0, 0.3};
			array<float> cut = {0.2, 0.3, 0.3, 0.85, 0.7, 0.85, 0.8, 0.3};
			array<float> roof = {0.0, 0.15, 1.0, 0.15};
			Hand(ground, x, y, cell, color);
			Hand(cut, x, y, cell, color);
			Hand(roof, x, y, cell, color);
		}
		else if (tool == MUD_BuildManager.T_TUNNEL)
		{
			array<vector> arch = {};
			for (int k = 0; k <= 8; k++)
			{
				float a = Math.PI * k / 8;
				arch.Insert(Vector(0.5 - Math.Cos(a) * 0.35, 0.75 - Math.Sin(a) * 0.4, 0));
			}
			arch.Insert(Vector(0.15, 0.75, 0));
			Line(arch, x, y, cell, color);
			array<float> top = {0.0, 0.1, 1.0, 0.1};
			Hand(top, x, y, cell, color);
		}
		else if (tool == MUD_BuildManager.T_DUGOUT)
		{
			array<float> room = {0.35, 0.15, 0.1, 0.15, 0.1, 0.9, 0.9, 0.9, 0.9, 0.15, 0.65, 0.15};
			array<float> doorL = {0.35, 0.15, 0.35, 0.0};
			array<float> doorR = {0.65, 0.15, 0.65, 0.0};
			Hand(room, x, y, cell, color);
			Hand(doorL, x, y, cell, color);
			Hand(doorR, x, y, cell, color);
		}
		else
		{
			array<float> a1 = {0.1, 0.1, 0.9, 0.9};
			array<float> a2 = {0.9, 0.1, 0.1, 0.9};
			Hand(a1, x, y, cell, color);
			Hand(a2, x, y, cell, color);
		}
	}
}
