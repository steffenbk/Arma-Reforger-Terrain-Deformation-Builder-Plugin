//! OLD RUT ZONE (TEMP A/B test, user 2026-10-19, v229 = before): one box on the map where wheels press the old
//! heightfield ruts (MUD_WorldGroundEntity.Press, the pre-voxel way of v001-v009) instead of voxel ruts.
//! Everywhere else: voxel ruts as usual. Craters / trenches / tunnels stay voxel in both places.
//! Server: MUD_WorldGroundEntity.Wheels checks Inside per wheel. Clients: MUD_LookRuts skips wheels inside.
//! Marker: red wire box + 8 m corner poles, drawn every frame on rendering machines (MUD_LookRuts.Tick).
//! Inside = world x / z only (any height). ENABLED false = voxel ruts everywhere, no marker.
class MUD_OldRutZone
{
	static const bool ENABLED = true;
	// box round the 2nd test Ural (1083.8, 2515.0, layer moved by the user 2026-10-19); the 1st Ural (1045.5, 2589.3) is outside
	static const float X0 = 1048;
	static const float X1 = 1118;
	static const float Z0 = 2480;
	static const float Z1 = 2550;
	static const float POLE = 8;      // corner pole height, m

	protected static float s_fY = -10000;     // ground y for the marker, read once

	//------------------------------------------------------------------------------------------------
	static bool Inside(vector p)
	{
		return ENABLED && p[0] >= X0 && p[0] <= X1 && p[2] >= Z0 && p[2] <= Z1;
	}

	//------------------------------------------------------------------------------------------------
	//! Rendering machines, every frame
	static void Draw()
	{
		if (!ENABLED)
			return;
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		if (s_fY < -9000)
		{
			float gy;
			if (!Y((X0 + X1) * 0.5, (Z0 + Z1) * 0.5, gy))
				return;     // ground not ready yet
			s_fY = gy;
		}
		int col = ARGB(255, 255, 40, 40);
		Shape.Create(ShapeType.BBOX, col, ShapeFlags.WIREFRAME | ShapeFlags.ONCE, Vector(X0, s_fY - 1, Z0), Vector(X1, s_fY + 2, Z1));
		Pole(world, X0, Z0, col);
		Pole(world, X1, Z0, col);
		Pole(world, X0, Z1, col);
		Pole(world, X1, Z1, col);
	}

	//------------------------------------------------------------------------------------------------
	//! undug ground y (GetSurfaceY is -256 on holed ground)
	protected static bool Y(float x, float z, out float y)
	{
		return MUD_WorldGroundEntity.UndugY(x, z, y);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Pole(BaseWorld world, float x, float z, int col)
	{
		float y;
		if (!Y(x, z, y))
			return;
		Shape.Create(ShapeType.BBOX, col, ShapeFlags.ONCE, Vector(x - 0.15, y, z - 0.15), Vector(x + 0.15, y + POLE, z + 0.15));
	}
}
