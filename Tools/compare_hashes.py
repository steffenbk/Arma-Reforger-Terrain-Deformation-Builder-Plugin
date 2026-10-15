"""Compare the last complete 'MUD hash' block of a server log and a client log (Plan 3 Task 9/10).

Usage: python compare_hashes.py <server console.log> <client console.log>
Exit 0 = same heights on both, 1 = differences, 2 = a log has no complete block.
"""
import re
import sys

BLOCK = re.compile(r"MUD hash block (\d+) patches (\d+)\s*$")
ENTRY = re.compile(r"MUD hash (-?\d+) (-?\d+) (-?\d+)'?\s*$")  # '? = Print(l) wrapper "string l = '...'" (2026-10-10)
END = re.compile(r"MUD hash end (\d+)\s*$")


def last_block(lines):
    """{key: (s1, s2)} of the last block whose end line came and whose entry count matches, else None."""
    best = None
    cur = None
    want = 0
    num = None
    for line in lines:
        m = BLOCK.search(line)
        if m:
            num, want, cur = m.group(1), int(m.group(2)), {}
            continue
        if cur is None:
            continue
        m = END.search(line)
        if m:
            if m.group(1) == num and len(cur) == want:
                best = cur
            cur = None
            continue
        m = ENTRY.search(line)
        if m:
            cur[int(m.group(1))] = (int(m.group(2)), int(m.group(3)))
    return best


def compare(server, client):
    return {
        "missing": sorted(k for k in server if k not in client),
        "extra": sorted(k for k in client if k not in server),
        "mismatched": sorted(k for k in server if k in client and server[k] != client[k]),
    }


def main(argv):
    blocks = []
    for path in argv[:2]:
        with open(path, encoding="utf-8", errors="replace") as f:
            blocks.append(last_block(f.read().splitlines()))
    if blocks[0] is None or blocks[1] is None:
        print("no complete MUD hash block in %s" % [p for p, b in zip(argv, blocks) if b is None])
        return 2
    r = compare(blocks[0], blocks[1])
    print("server patches %d, client patches %d, missing %d, extra %d, mismatched %d"
          % (len(blocks[0]), len(blocks[1]), len(r["missing"]), len(r["extra"]), len(r["mismatched"])))
    for name in ("missing", "extra", "mismatched"):
        if r[name]:
            print("%s: %s" % (name, r[name][:20]))
    return 0 if not (r["missing"] or r["extra"] or r["mismatched"]) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
