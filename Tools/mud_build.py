"""Terrain pass of the World Editor plugin 'MUD Ground: Build / Unbuild' (Scripts/WorkbenchGame/Mud/MUD_GroundBuildPlugin.c).

The plugin starts this with Workbench.RunProcess while the World Editor has ANOTHER world open (the tiles must not
be in use), waits for it, then reads --report. Any map: the tile layout comes from the tiles (HGHT size) and
--tiles-x (WorldEditorAPI.GetTerrainTilesX). Docs: docs/features/build-plugin/README.md.

    python -I Tools/mud_build.py build   --tiles <world dir>/<terrain name>/.Data --backup <dir> --mask <f>
                                         --pins <f> --samples <f> --heights <out .c> --tiles-x <n> --cell <m>
                                         --report <f> [--margin 1]
    python -I Tools/mud_build.py unbuild --tiles <dir> --backup <dir> --report <f> [--heights <the .c Build made>]

build, in this order (each step stops the run on a fault; nothing is written before step 3):
  1 fit: the plugin's height samples ('MUD bake sample' lines) against the tiles' HGHT (mud_holes.fit). A failed fit
    stops before any tile is touched.
  2 snapshot: the FIRST copy of every tile goes to --backup and is never overwritten = the pre-Build state.
  3 holes: every tile = its CURRENT bytes with the pristine HOLE chunk + ours (mud_holes.reset_holes + merge_tile),
    so map edits made after the snapshot are kept. All tiles are made in memory first, then written.
  4 bake: the heights of every hole block -> --heights (MUDH, mud_holes.bake).
unbuild: every tile gets its pristine HOLE chunk back (heights and paint stay as they are now); --heights is moved
  into --backup as <name>.unbuilt.
Report: one 'MUD build: ...' line per step, written as each step ends (the plugin's progress bar counts them),
  last line 'MUD build exit <code>' (0 = ok). build writes 5 step lines, unbuild 1-2.
"""
import argparse
import os
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I drops the script dir from sys.path
import mud_holes as mh  # noqa: E402
import ttile_holes as tt  # noqa: E402

FIT_MAX = 0.002  # m, as mud_holes fit


def read_text(p):
    with open(p, encoding='utf-8', errors='replace') as f:
        return f.read()


def read_bytes(p):
    with open(p, 'rb') as f:
        return f.read()


def tile_layout(tiles_dir):
    """(vertices a tile side, tile count) from the first tile's HGHT (u16 per vertex, square)."""
    names = mh.tile_names(tiles_dir)
    if not names:
        raise ValueError('no Terrain_<n>.ttile in %s' % tiles_dir)
    hght = dict(tt.chunks(read_bytes(os.path.join(tiles_dir, names[min(names)])))).get(b'HGHT', b'')
    n = int(round((len(hght) / 2) ** 0.5))
    if n < 2 or 2 * n * n != len(hght):
        raise ValueError('HGHT of %s is %d bytes, not a square u16 grid' % (names[min(names)], len(hght)))
    return n, len(names)


def build(a, out):
    verts, count = tile_layout(a.tiles)
    mh.set_layout(verts, a.tiles_x)
    if a.tiles_x <= 0 or count % a.tiles_x:
        raise ValueError('%d tiles is not a whole number of rows of %d' % (count, a.tiles_x))
    tiles_z = count // a.tiles_x
    side = mh.TILE_BLOCKS * mh.BLOCK
    out('layout %d x %d tiles, %d vertices a side = %d hole blocks a side' % (a.tiles_x, tiles_z, verts, mh.TILE_BLOCKS))
    mask = mh.parse_mask(read_text(a.mask))
    pins = mh.parse_pins(read_text(a.pins))
    if abs(mask['cell'] - a.cell) > 1e-4:
        raise ValueError('mask cell %g m, terrain cell %g m' % (mask['cell'], a.cell))
    if (mask['nx'], mask['nz']) != (a.tiles_x * side, tiles_z * side):
        raise ValueError('mask grid %d x %d, the tiles hold %d x %d cells - scan of another world?'
                         % (mask['nx'], mask['nz'], a.tiles_x * side, tiles_z * side))

    # 1 fit (reads only)
    samples = mh.parse_samples(read_text(a.samples))
    if not samples:
        raise ValueError('no MUD bake sample lines in %s' % a.samples)
    fitd = mh.fit(samples, lambda t: read_bytes(os.path.join(a.tiles, 'Terrain_%d.ttile' % t)))
    out('fit samples %d orient %s a %.9f b %.6f max residual %.6f m' % (fitd['samples'], fitd['orient'], fitd['a'],
                                                                       fitd['b'], fitd['resid']))
    if fitd['resid'] >= FIT_MAX:
        raise ValueError('FIT FAIL (residual %.4f m >= %.3f) - no tile touched' % (fitd['resid'], FIT_MAX))

    # 2 snapshot
    copied, total = mh.snapshot(a.tiles, a.backup)
    out('snapshot %d tiles copied now, backup holds %d (first copy kept)' % (copied, total))
    if total != count:
        raise ValueError('backup holds %d tiles, the world %d - no tile touched' % (total, count))

    # 3 holes: all tiles in memory first
    s = mh.stats(mask, a.margin, pins, keep_full=True)
    per_tile = {}
    for k, rows in s['block_map'].items():
        per_tile.setdefault(mh.owner_tile(k[0], k[1]), {})[k] = rows
    names = mh.tile_names(a.tiles)
    lost = sorted(set(per_tile) - set(names))
    if lost:
        raise ValueError('hole blocks for tiles that do not exist: %s - no tile touched' % lost[:5])
    outs = {}
    ours = 0
    for t, name in sorted(names.items()):
        now = read_bytes(os.path.join(a.tiles, name))
        base = mh.reset_holes(now, read_bytes(os.path.join(a.backup, name)))
        mine = per_tile.get(t, {})
        ours += sum(bin(r).count('1') for rows in mine.values() for r in rows)
        outs[t] = (name, now, mh.merge_tile(base, mine, refuse_full=True))
    if ours != s['holes']:
        raise ValueError('hole cells %d in blocks, mask says %d - no tile touched' % (ours, s['holes']))
    written = 0
    for t, (name, now, data) in sorted(outs.items()):
        if data != now:
            with open(os.path.join(a.tiles, name), 'wb') as f:
                f.write(data)
            written += 1
    out('holes margin %d, pins %d (%d were holes): keep %d + holes %d = %d cells, %d blocks, %d tiles rewritten of %d'
        % (a.margin, len(pins['cells']), s['pins_new'], s['keep'], s['holes'], s['keep'] + s['holes'], s['blocks'],
           written, count))

    # 4 bake
    os.makedirs(os.path.dirname(os.path.abspath(a.heights)), exist_ok=True)
    b = mh.bake(a.tiles, fitd, a.heights)
    out('baked %(blocks)d blocks from %(tiles)d tiles, %(bytes)d bytes' % b + ' -> ' + a.heights)


def unbuild(a, out):
    rep = mh.restore(a.tiles, a.backup)
    if not rep:
        raise ValueError('backup %s holds no tiles - nothing to put back' % a.backup)
    before = sum(r[1] for r in rep)
    after = sum(max(r[2], 0) for r in rep)
    whole = sum(1 for r in rep if r[2] < 0)
    out('restore %d tiles: hole cells %d -> %d (pristine), %d tiles copied back whole' % (len(rep), before, after, whole))
    if a.heights and os.path.exists(a.heights):  # Build made it next to the world: moved out, not deleted
        keep = os.path.join(a.backup, os.path.basename(a.heights) + '.unbuilt')
        os.replace(a.heights, keep)
        out('heights file moved to %s' % keep)


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['build', 'unbuild'])
    ap.add_argument('--tiles', required=True)
    ap.add_argument('--backup', required=True)
    ap.add_argument('--report', required=True)
    ap.add_argument('--mask')
    ap.add_argument('--pins')
    ap.add_argument('--samples')
    ap.add_argument('--heights')
    ap.add_argument('--tiles-x', type=int)
    ap.add_argument('--cell', type=float)
    ap.add_argument('--margin', type=int, default=1)
    a = ap.parse_args(argv)
    if a.cmd == 'build' and None in (a.mask, a.pins, a.samples, a.heights, a.tiles_x, a.cell):
        ap.error('build needs --mask --pins --samples --heights --tiles-x --cell')
    os.makedirs(os.path.dirname(os.path.abspath(a.report)), exist_ok=True)
    lines = []
    open(a.report, 'w').close()  # the plugin polls this file for progress while we run

    def out(text):
        lines.append('MUD build: ' + text)
        print(lines[-1])
        with open(a.report, 'a', encoding='utf-8', newline='\n') as f:
            f.write(lines[-1] + '\n')

    code = 0
    try:
        (build if a.cmd == 'build' else unbuild)(a, out)
    except Exception as e:  # the plugin reads the report, not stderr
        out('ERROR %s: %s' % (type(e).__name__, e))
        for t in traceback.format_exc().splitlines()[-4:]:
            out('  ' + t)
        code = 1
    lines.append('MUD build exit %d' % code)
    with open(a.report, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    return code


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
