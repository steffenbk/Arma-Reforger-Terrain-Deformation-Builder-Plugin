"""Compare per-op ground hashes between a server log and a client log (Build 0 test kit, 2026-10-16).

Reads every 'MUD field hash seq N kind K: H ...' line (voxel field, test kit) and every 'MUD disp hash seq N kind K:
H ...' line (rut floor, MUD_VoxelProbe DISP hash) and compares them op by op (same seq on both machines).
Synced mud sheet Stage 2 (2026-10-18): also every 'MUD sheet hash seq N: H ...' line (MUD_MudSync, the sheet's sync
layer: kind 4 rut ops; since Stage 4 also crater / dig ops kinds 0-3). A DS draws no sheet, so for sheet hashes pass two CLIENT logs (client A, client B) or a
WB host log and a client log; 'server' / 'client' below then just mean the first / second log.

Usage: python compare_op_hashes.py <server console.log> <client console.log>
Exit 0 = every op both logs have is equal, 1 = differences, 2 = no hash lines in a log.
Ops only one log has (a client still draining, a log cut short) are listed, not counted as differences.
"""
import re
import sys

LINE = re.compile(r"MUD (field|disp) hash seq (\d+) kind (\d+): (-?\d+)")
SHEET = re.compile(r"MUD sheet hash seq (\d+): (-?\d+)")


def read_hashes(lines):
    """{(type, seq): (kind, hash)} - the last line wins if one op logged twice (a re-apply)."""
    out = {}
    for line in lines:
        m = LINE.search(line)
        if m:
            out[(m.group(1), int(m.group(2)))] = (int(m.group(3)), int(m.group(4)))
            continue
        m = SHEET.search(line)
        if m:
            out[("sheet", int(m.group(1)))] = (4, int(m.group(2)))
    return out


def compare(server, client):
    both = [k for k in server if k in client]
    return {
        "both": len(both),
        "only_server": sorted(k for k in server if k not in client),
        "only_client": sorted(k for k in client if k not in server),
        "mismatched": sorted(k for k in both if server[k] != client[k]),
    }


def main(argv):
    hashes = []
    for path in argv[:2]:
        with open(path, encoding="utf-8", errors="replace") as f:
            hashes.append(read_hashes(f.read().splitlines()))
    if not hashes[0] or not hashes[1]:
        print("no 'MUD field/disp/sheet hash seq' lines in %s" % [p for p, h in zip(argv, hashes) if not h])
        return 2
    r = compare(hashes[0], hashes[1])
    print("ops in both %d, mismatched %d, only server %d, only client %d"
          % (r["both"], len(r["mismatched"]), len(r["only_server"]), len(r["only_client"])))
    for k in r["mismatched"][:20]:
        print("MISMATCH %s seq %d: server kind %d hash %d, client kind %d hash %d"
              % (k[0], k[1], hashes[0][k][0], hashes[0][k][1], hashes[1][k][0], hashes[1][k][1]))
    for name in ("only_server", "only_client"):
        if r[name]:
            print("%s (first 20): %s" % (name, ["%s %d" % k for k in r[name][:20]]))
    return 1 if r["mismatched"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
