import compare_hashes as ch


SERVER = """12:00:00.000  SCRIPT       : MUD hash block 1 patches 2
12:00:00.001  SCRIPT       : MUD hash 100 5 7
12:00:00.002  SCRIPT       : MUD hash 200 -3 -9
12:00:00.003  SCRIPT       : MUD hash end 1
12:00:20.000  SCRIPT       : MUD hash block 2 patches 2
12:00:20.001  SCRIPT       : MUD hash 100 6 8
12:00:20.002  SCRIPT       : MUD hash 200 -3 -9
12:00:20.003  SCRIPT       : MUD hash end 2
12:00:40.000  SCRIPT       : MUD hash block 3 patches 2
12:00:40.001  SCRIPT       : MUD hash 100 6 8
"""


def test_last_complete_block_wins_and_cut_block_is_ignored():
    assert ch.last_block(SERVER.splitlines()) == {100: (6, 8), 200: (-3, -9)}


def test_block_with_wrong_count_is_not_complete():
    lines = ["MUD hash block 1 patches 3", "MUD hash 1 1 1", "MUD hash end 1"]
    assert ch.last_block(lines) is None


def test_compare_clean():
    a = {1: (1, 2), 2: (3, 4)}
    assert ch.compare(a, dict(a)) == {"missing": [], "extra": [], "mismatched": []}


def test_compare_finds_missing_extra_mismatched():
    srv = {1: (1, 2), 2: (3, 4), 3: (5, 6)}
    cli = {1: (1, 2), 2: (3, 5), 4: (7, 8)}
    assert ch.compare(srv, cli) == {"missing": [3], "extra": [4], "mismatched": [2]}


def test_main_exit_codes(tmp_path, capsys):
    s = tmp_path / "server.log"
    c = tmp_path / "client.log"
    s.write_text(SERVER, encoding="utf-8")
    c.write_text(SERVER, encoding="utf-8")
    assert ch.main([str(s), str(c)]) == 0
    assert "missing 0, extra 0, mismatched 0" in capsys.readouterr().out
    c.write_text(SERVER.replace("MUD hash 200 -3 -9\n12:00:20.003", "MUD hash 200 -3 -8\n12:00:20.003"), encoding="utf-8")
    assert ch.main([str(s), str(c)]) == 1
    assert "mismatched 1" in capsys.readouterr().out


def test_main_no_block(tmp_path, capsys):
    s = tmp_path / "s.log"
    s.write_text("nothing\n", encoding="utf-8")
    assert ch.main([str(s), str(s)]) == 2


def test_entries_printed_as_a_variable_are_read():
    # Print(l) logs "string l =    '...'" (Exp 192624, 2026-10-10 MP run): same data, must still count
    text = """12:00:00.000  SCRIPT       : MUD hash block 1 patches 2
12:00:00.001  SCRIPT       : string l =    'MUD hash 100 5 7'
12:00:00.002  SCRIPT       : string l =    'MUD hash 200 -3 -9'
12:00:00.003  SCRIPT       : MUD hash end 1
"""
    assert ch.last_block(text.splitlines()) == {100: (5, 7), 200: (-3, -9)}
