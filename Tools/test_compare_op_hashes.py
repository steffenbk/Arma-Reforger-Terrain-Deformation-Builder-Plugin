import compare_op_hashes as co


SERVER = """12:00:00.000  SCRIPT       : MUD field hash seq 1 kind 0: 4711 (812 samples)
12:00:00.100  SCRIPT       : MUD disp hash seq 2 kind 4: 99 (10 written, 0 cleared)
12:00:00.200  SCRIPT       : MUD field hash seq 3 kind 1: -5 (64 samples)
12:00:00.300  SCRIPT       : MUD voxel probe: something else
"""


def test_read_hashes_both_types():
    h = co.read_hashes(SERVER.splitlines())
    assert h == {("field", 1): (0, 4711), ("disp", 2): (4, 99), ("field", 3): (1, -5)}


def test_compare_clean_and_partial():
    s = co.read_hashes(SERVER.splitlines())
    c = {("field", 1): (0, 4711), ("disp", 2): (4, 99)}
    r = co.compare(s, c)
    assert r == {"both": 2, "only_server": [("field", 3)], "only_client": [], "mismatched": []}


def test_compare_finds_mismatch():
    s = {("field", 1): (0, 1), ("field", 2): (2, 7)}
    c = {("field", 1): (0, 1), ("field", 2): (2, 8)}
    assert co.compare(s, c)["mismatched"] == [("field", 2)]


def test_main_exit_codes(tmp_path, capsys):
    s = tmp_path / "server.log"
    c = tmp_path / "client.log"
    e = tmp_path / "empty.log"
    s.write_text(SERVER, encoding="utf-8")
    c.write_text(SERVER.replace(": -5 (", ": -6 ("), encoding="utf-8")
    e.write_text("nothing\n", encoding="utf-8")
    assert co.main([str(s), str(s)]) == 0
    assert co.main([str(s), str(c)]) == 1
    assert "MISMATCH field seq 3" in capsys.readouterr().out
    assert co.main([str(s), str(e)]) == 2
