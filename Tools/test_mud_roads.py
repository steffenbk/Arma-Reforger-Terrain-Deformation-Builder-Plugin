import pytest

import mud_roads as mr


def export_text(roads, done=None):
    """roads: list of (mat, type, width, pts); origin = first point."""
    lines = []
    for i, (mat, ty, w, pts) in enumerate(roads):
        o = pts[0]
        lines.append('MUD rexp road %d origin %g %g %g width %g mat %s type %d spline %d parent - tess 0'
                     % (i, o[0], o[1], o[2], w, mat, ty, len(pts)))
        lines += ['MUD rexp sp %d %g %g %g' % ((i,) + p) for p in pts]
    if done is not False:
        lines.append('MUD rexp done roads %d with spline %d with parent shape 0 no entity 0'
                     % (len(roads) if done is None else done, len(roads)))
    return '\n'.join(lines) + '\n'


ASPHALT = '{4B94AEDC2E5F9ABB}Assets/Structures/Infrastructure/Roads/Data/Road_Asphalt_E_02.emat'
FLOWER = '{6D9276D2FE1EFFF0}Assets/Decals/Soil/FlowerBed/Data/FlowerBed_01.emat'
RUNWAY = '{8B155CEDB41C2C64}Assets/Decals/Markings/Runway/Data/RunwayConcrete_01_40m.emat'
SKID = '{74479FC1FA27C8F2}Assets/Decals/Markings/Runway/Data/RunwaySkidMarks_01.emat'
LINE = [(0.0, 1.0, 0.0), (10.0, 1.0, 0.0)]
NL = chr(10)


def test_parse_reads_roads_and_points():
    r = mr.parse_export(export_text([(ASPHALT, 2, 8, LINE)]))
    assert len(r) == 1 and r[0]['pts'][0] == LINE[0] and r[0]['pts'][-1] == LINE[-1] and r[0]['width'] == 8 and r[0]['mat'] == ASPHALT


def test_parse_empty_material():
    # real export: 'mat  type 2' -> split drops the empty token
    r = mr.parse_export(export_text([('', 2, 4.5, LINE)]))
    assert r[0]['mat'] == '' and r[0]['type'] == 2


def test_parse_refuses_missing_done():
    with pytest.raises(ValueError, match='no done'):
        mr.parse_export(export_text([(ASPHALT, 2, 8, LINE)], done=False))


def test_parse_refuses_wrong_done_count():
    with pytest.raises(ValueError, match='done line'):
        mr.parse_export(export_text([(ASPHALT, 2, 8, LINE)], done=2))


def test_parse_refuses_point_count_mismatch():
    t = export_text([(ASPHALT, 2, 8, LINE)]).replace('spline 2', 'spline 3')
    with pytest.raises(ValueError, match='points'):
        mr.parse_export(t)


def test_parse_refuses_one_point_road():
    with pytest.raises(ValueError, match='fewer than 2'):
        mr.parse_export(export_text([(ASPHALT, 2, 8, LINE[:1])]))


def test_armor_by_material_not_type():
    assert mr.is_armor(ASPHALT)
    assert mr.is_armor(RUNWAY)
    assert mr.is_armor('')
    assert not mr.is_armor(FLOWER)
    assert not mr.is_armor(SKID)


def test_bake_round_trip_and_armor_count():
    roads = mr.parse_export(export_text([(ASPHALT, 255, 8, LINE), (FLOWER, 255, 2, LINE), ('', 2, 4.5, LINE)]))
    text, armor = mr.bake_text(roads)
    lines = text.splitlines()
    assert lines[0] == 'MUD roads 1 3'
    assert lines[-1] == 'MUD roads done 3 armor 2 curve 3'
    rl = [l for l in lines if l.startswith('r ')]
    assert [l.split()[2] for l in rl] == ['1', '0', '1']
    assert rl[2].split()[-1] == '-'
    assert sum(1 for l in lines if l.startswith('p ')) == 3 * 6     # 10 m line every 2 m = 6 points


def test_check_hits_inside_and_outside():
    roads = mr.parse_export(export_text([(ASPHALT, 2, 8, LINE)]))
    rows = mr.check_hits(roads, '5,1,3.9|0,1,0\n5,1,4.1|0,1,0\n')
    assert [r[5] for r in rows] == [True, False]


def test_check_hits_unknown_origin_refused():
    roads = mr.parse_export(export_text([(ASPHALT, 2, 8, LINE)]))
    with pytest.raises(ValueError, match='0 roads'):
        mr.check_hits(roads, '5,1,0|50,1,0\n')


def test_tess_points_replace_spline_points():
    t = export_text([(ASPHALT, 2, 8, LINE)])
    t = t.replace('MUD rexp done', 'MUD rexp tp 0 0 1 0' + NL + 'MUD rexp tp 0 5 1 1' + NL + 'MUD rexp tp 0 10 1 0' + NL + 'MUD rexp done')
    r = mr.parse_export(t)
    assert r[0]['curve'] and r[0]['pts'] == [(0, 1, 0), (5, 1, 1), (10, 1, 0)]
    assert mr.bake_text(r)[0].splitlines()[-1].endswith('curve 1')


def test_spline_line_with_tangents_parses():
    t = export_text([(ASPHALT, 2, 8, LINE)]).replace('MUD rexp sp 0 10 1 0', 'MUD rexp sp 0 10 1 0 tg 1 0 0 1 0 0')
    assert mr.parse_export(t)[0]['pts'][-1] == (10, 1, 0)


def test_hermite_straight_line_stays_straight():
    c = mr.hermite(LINE, [None, None])
    assert len(c) == 6 and all(abs(p[2]) < 1e-9 for p in c)
    assert [round(p[0], 6) for p in c] == [0, 2, 4, 6, 8, 10]


def test_hermite_bends_through_points():
    pts = [(0.0, 0, 0.0), (10.0, 0, 10.0), (20.0, 0, 0.0)]
    c = mr.hermite(pts, [None] * 3)
    assert (10.0, 0, 10.0) in [tuple(round(v, 9) for v in p) for p in c]
    # the curve rounds the corner: a sample between p0 and p1 sits off the straight chord x == z
    mid = c[4]
    assert abs(mid[0] - mid[2]) > 0.1


def test_explicit_tangent_used():
    pts = [(0.0, 0, 0.0), (10.0, 0, 0.0)]
    c = mr.hermite(pts, [((0, 0, 10.0), (0, 0, 10.0)), None])
    assert max(p[2] for p in c) > 0.5     # pushed sideways by the +z tangent at the start
