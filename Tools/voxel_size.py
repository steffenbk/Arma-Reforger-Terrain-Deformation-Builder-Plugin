"""Flip the voxel brick size in Scripts/Game/Mud/MUD_VoxelProbe.c.

Every line ending in '// SIZE <coarse> | <fine>' gets its '= value;' set to one side.
  python Tools/voxel_size.py coarse   -> 0.5 m cells, 8 per brick side (normal)
  python Tools/voxel_size.py fine     -> 0.25 m cells, 16 per brick side (probe)
  python Tools/voxel_size.py          -> show the current setting
MUD_VoxelProbe.Init (SizesOk) refuses a mixed file. Reload scripts in Workbench after.
"""
import os
import re
import sys

PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Scripts", "Game", "Mud", "MUD_VoxelProbe.c")
LINE = re.compile(r"^(\s*static const \w+ \w+ = )([^;]+)(;.*// SIZE (\S+) \| (\S+).*)$")


def main():
    want = sys.argv[1] if len(sys.argv) > 1 else None
    if want not in (None, "coarse", "fine"):
        sys.exit("usage: voxel_size.py [coarse|fine]")
    with open(PATH, encoding="utf-8", newline="") as f:
        lines = f.read().split("\n")
    found = 0
    sides = set()
    for n, line in enumerate(lines):
        m = LINE.match(line)
        if not m:
            continue
        found += 1
        cur, coarse, fine = m.group(2).strip(), m.group(4), m.group(5)
        sides.add("coarse" if cur == coarse else "fine" if cur == fine else "other")
        if want:
            lines[n] = m.group(1) + (coarse if want == "coarse" else fine) + m.group(3)
    if found != 13:
        sys.exit("expected 13 SIZE lines, found %d - nothing written" % found)
    if want:
        with open(PATH, "w", encoding="utf-8", newline="") as f:
            f.write("\n".join(lines))
        print("set %d lines to %s (was %s) - reload scripts in Workbench" % (found, want, "/".join(sorted(sides))))
    else:
        print("current: %s (%d lines)" % ("/".join(sorted(sides)), found))


if __name__ == "__main__":
    main()
