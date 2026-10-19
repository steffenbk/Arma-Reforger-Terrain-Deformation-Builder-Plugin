"""mud_holes.py area (pin cut-out test area, 2026-10-17)."""
import pytest

import mud_holes as mh
from test_mud_holes import mask_text, pins_text


def test_box_origin_reads_mask_and_pins_corner():
    assert mh.box_origin('12:00 SCRIPT : MUD mask box 12.5 -3 cells 64 32 cell 2', mh.BOX) == (12.5, -3.0)
    assert mh.box_origin(pins_text(64, 32, []), mh.PBOX) == (0.0, 0.0)


def test_rect_cells_covers_every_touched_cell_and_clips():
    assert mh.rect_cells((3, 4, 9, 8), 0, 0, 2, 64, 32) == (1, 2, 5, 4)
    assert mh.rect_cells((-10, -10, 500, 500), 0, 0, 2, 64, 32) == (0, 0, 64, 32)
    with pytest.raises(ValueError):
        mh.rect_cells((10, 10, 10, 20), 0, 0, 2, 64, 32)


def test_merge_area_new_inside_old_outside():
    old = mh.parse_mask(mask_text(64, 32, [(z, 0, 63) for z in range(32)]))     # all kept
    new = mh.parse_mask(mask_text(64, 32, [(5, 10, 10)]))                       # one cell
    old_p = mh.parse_pins(pins_text(64, 32, [(1, 1), (6, 12)]))
    new_p = mh.parse_pins(pins_text(64, 32, [(7, 13), (20, 50)]))
    m, p = mh.merge_area(new, old, new_p, old_p, (8, 4, 16, 8))                 # x 8-15, z 4-7
    inside = [m['keep'][z * 64 + x] for z in range(4, 8) for x in range(8, 16)]
    assert sum(inside) == 1 and m['keep'][5 * 64 + 10] == 1
    assert sum(m['keep']) == 64 * 32 - 32 + 1
    assert p['cells'] == {1 * 64 + 1, 7 * 64 + 13}    # old pin inside dropped, new pin outside ignored


def test_merge_area_refuses_other_grid():
    a = mh.parse_mask(mask_text(64, 32, []))
    b = mh.parse_mask(mask_text(32, 32, []))
    p = mh.parse_pins(pins_text(64, 32, []))
    with pytest.raises(ValueError, match='size'):
        mh.merge_area(b, a, p, p, (0, 0, 1, 1))


def test_texts_round_trip():
    m = mh.parse_mask(mask_text(64, 32, [(0, 0, 3), (0, 10, 63), (31, 5, 5)]))
    back = mh.parse_mask(mh.mask_text_of(m, 0, 0))
    assert back['keep'] == m['keep']
    p = mh.parse_pins(pins_text(64, 32, [(3, 4), (0, 63)]))
    assert mh.parse_pins(mh.pins_text_of(p, 0, 0))['cells'] == p['cells']


def test_area_cli_writes_files_plan_reads(tmp_path, capsys):
    files = {}
    for name, text in (('new', mask_text(64, 32, [])), ('old', mask_text(64, 32, [(z, 0, 63) for z in range(32)])),
                       ('np', pins_text(64, 32, [(2, 2)])), ('op', pins_text(64, 32, []))):
        files[name] = tmp_path / (name + '.txt')
        files[name].write_text(text)
    om, op = tmp_path / 'm.txt', tmp_path / 'p.txt'
    assert mh.main(['area', str(files['new']), str(files['old']), '--pins', str(files['np']), '--old-pins',
                    str(files['op']), '--rect', '0,0,8,8', '--out-mask', str(om), '--out-pins', str(op)]) == 0
    assert 'keep old 16 -> new 0' in capsys.readouterr().out
    assert mh.main(['plan', str(om), '--pins', str(op), '--margin', '0']) == 0
    out = capsys.readouterr().out
    assert 'keep 2033 + holes 15' in out    # 2048 - 16 holed + pin (2, 2) kept
