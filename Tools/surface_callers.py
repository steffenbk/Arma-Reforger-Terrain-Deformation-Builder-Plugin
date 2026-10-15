"""Offline scan of the Experimental pak (plan 3 task 1, carried item 1 + 2).

Table 1: every shipped script line that calls GetSurfaceY or SCR_TerrainHelper.GetTerrainY /
GetHeightAboveTerrain (all return -256-based heights on a hole cell), with the enclosing function.
Table 2: every prefab (.et) whose own Flags line has EntityFlags.RELATIVE_Y (0x80000) - a runtime
spawn of such a prefab onto a hole cell would load at its raw offset, like the pinned world roots.

Usage: python -I surface_callers.py <out.md>
"""
import re
import sys

RELATIVE_Y = 0x80000
CALL = re.compile(r'\b(GetSurfaceY|GetTerrainY|GetHeightAboveTerrain)\s*\(')
FUNC = re.compile(r'^\s*(?:(?:static|protected|private|override|proto|external|native|event|sealed)\s+)*'
                  r'[A-Za-z_][\w<>,\s]*?\s+([A-Za-z_]\w*)\s*\([^;]*$')
FLAGS = re.compile(r'^\s*Flags\s+(\d+)\s*$', re.M)


def strip_comment(line):
    i = line.find('//')
    return line if i < 0 else line[:i]


def find_calls(path, text):
    """Rows {'file', 'line', 'api', 'func', 'code'} for each call; the definition line itself is skipped."""
    rows = []
    func = ''
    for n, raw in enumerate(text.splitlines(), 1):
        code = strip_comment(raw)
        m = FUNC.match(code)
        if m and not code.strip().startswith(('if', 'else', 'while', 'for', 'switch', 'return', 'new', 'case')):
            func = m.group(1)
            if CALL.search(code) and m.group(1) in ('GetSurfaceY', 'GetTerrainY', 'GetHeightAboveTerrain'):
                continue    # the helper's own definition
        for c in CALL.finditer(code):
            rows.append({'file': path, 'line': n, 'api': c.group(1), 'func': func, 'code': raw.strip()})
    return rows


def has_relative_y(text):
    return any(int(v) & RELATIVE_Y for v in FLAGS.findall(text))


def main(out):
    sys.path.insert(0, r'C:/Users/Steffen/Documents/A_documents/Github/blender-mcp-BK/tools')
    import pakread
    p = pakread.Pak('experimental')
    calls = []
    for f in sorted(p.find(r'^scripts/.*\.c$')):
        calls += find_calls(f, p.read(f).decode('utf-8', 'replace'))
    prefabs = sorted(p.find(r'^Prefabs/.*\.et$'))
    rel = [f for f in prefabs if has_relative_y(p.read(f).decode('utf-8', 'replace'))]

    files = sorted({r['file'] for r in calls})
    by_api = {}
    for r in calls:
        by_api[r['api']] = by_api.get(r['api'], 0) + 1
    with open(out, 'w', encoding='utf-8') as fh:
        fh.write('# Shipped hole-height callers (pak experimental)\n\n')
        fh.write('Regex: `%s` on code with `//` comments stripped, all `scripts/**/*.c`.\n\n' % CALL.pattern)
        fh.write('Calls %d in %d files; by api: %s; sum %d (checksum = calls).\n\n'
                 % (len(calls), len(files), by_api, sum(by_api.values())))
        fh.write('| file | line | api | function | code |\n|---|---|---|---|---|\n')
        for r in calls:
            fh.write('| %s | %d | %s | %s | `%s` |\n' % (r['file'], r['line'], r['api'], r['func'],
                                                       r['code'].replace('|', '\\|')))
        fh.write('\n# Prefabs with RELATIVE_Y (0x80000) on a Flags line\n\n')
        fh.write('Prefabs scanned %d, with RELATIVE_Y %d.\n\n' % (len(prefabs), len(rel)))
        for f in rel:
            fh.write('- %s\n' % f)
    print('calls %d in %d files, prefabs %d, relY %d -> %s' % (len(calls), len(files), len(prefabs), len(rel), out))


if __name__ == '__main__':
    main(sys.argv[1])
