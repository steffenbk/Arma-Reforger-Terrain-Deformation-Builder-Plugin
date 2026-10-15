import surface_callers as sc

SRC = '''class A
{
	void Foo(vector p)
	{
		float y = GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
	}

	static float Bar(vector pos)
	{
		// GetSurfaceY in a comment does not count
		return SCR_TerrainHelper.GetTerrainY(pos);
	}
}
'''


def test_find_calls_reports_function_and_line():
    rows = sc.find_calls('scripts/Game/A.c', SRC)
    assert [(r['line'], r['api'], r['func']) for r in rows] == [
        (5, 'GetSurfaceY', 'Foo'),
        (11, 'GetTerrainY', 'Bar'),
    ]


def test_relative_y_flag_detection():
    assert sc.has_relative_y('SomeEntity {\n Flags 525315\n}\n')      # 0x80000 | 1027
    assert not sc.has_relative_y('SomeEntity {\n Flags 1027\n}\n')
    assert not sc.has_relative_y('SomeEntity {\n}\n')
