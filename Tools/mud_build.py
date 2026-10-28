"""Terrain pass of the World Editor plugin 'MUD Ground: Build / Unbuild' (Scripts/WorkbenchGame/Mud/MUD_GroundBuildPlugin.c).

The plugin starts this with Workbench.RunProcess while the World Editor has ANOTHER world open (the tiles must not
be in use), waits for it, then reads --report. Any map: the tile layout comes from the tiles (HGHT size) and
--tiles-x (WorldEditorAPI.GetTerrainTilesX). Docs: docs/features/build-plugin/README.md.

    python -I Tools/mud_build.py build   --tiles <world dir>/<terrain name>/.Data --backup <dir> --mask <f>
                                         --pins <f> --samples <f> --heights <out .c> --tiles-x <n> --cell <m>
                                         --report <f> [--margin 1]
    python -I Tools/mud_build.py build   --vanilla <terrain entity name> --game-data <game addons/data dir | auto>
                                         --addon <this mod's root dir> (+ the build args above, no --tiles)
    python -I Tools/mud_build.py unbuild --tiles <dir> --backup <dir> --report <f> [--heights <the .c Build made>]

Two modes:
  own terrain: the world's own loose tiles are edited in place.
  vanilla (user 2026-10-28: no world copy, the terrain tools must stay locked): the world uses a terrain from the
    game's paks (Arland, Everon, Kolguyev). The tiles go into the mod at the SAME path (<addon>/<pak .Data path>),
    which overrides the pak tiles by path (gate 1 2026-10-08) and keeps the World Editor terrain tools locked
    (user-tested 2026-10-28 with one unchanged override tile). The pak .Data folder is found by name
    (<terrain>/.Data/<terrain>_0.ttile) and picked by the height fit, so a same-named terrain of another map is
    never used. Only tiles this run CREATED are listed in <backup>/override.txt; unbuild moves exactly those out
    of the mod (into <backup>/override_removed) = the vanilla tile from the pak is used again.

build, in this order (each step stops the run on a fault; no tile is written before step 3 - vanilla mode first
writes unchanged pak copies, which change nothing in game):
  1 fit: the plugin's height samples ('MUD bake sample' lines) against the tiles' HGHT (mud_holes.fit). A failed fit
    stops before any tile is touched.
  2 snapshot: the FIRST copy of every tile goes to --backup and is never overwritten = the pre-Build state.
  3 holes: every tile = its CURRENT bytes with the pristine HOLE chunk + ours (mud_holes.reset_holes + merge_tile),
    so map edits made after the snapshot are kept. All tiles are made in memory first, then written.
  4 bake: the heights of every hole block -> --heights (MUDH, mud_holes.bake).
unbuild: every tile gets its pristine HOLE chunk back (heights and paint stay as they are now); --heights is moved
  into --backup as <name>.unbuilt; vanilla mode then moves the created override tiles out of the mod.
Report: one 'MUD build: ...' line per step, written as each step ends (the plugin's progress bar counts them),
  last line 'MUD build exit <code>' (0 = ok). build writes 5 step lines (vanilla 7), unbuild 1-3.
  Vanilla build writes 'MUD build: override tiles <dir>' - the plugin keeps that dir for unbuild.
"""
import argparse
import os
import re
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I drops the script dir from sys.path
import mud_holes as mh  # noqa: E402
import pak_files as pf  # noqa: E402
import ttile_holes as tt  # noqa: E402

FIT_MAX = 0.002  # m, as mud_holes fit
OVERRIDE = 'override.txt'            # in --backup: 'tiles <dir>', 'source <pak dir>', 'created <file>' lines
REMOVED = 'override_removed'         # in --backup: created override tiles unbuild moved out of the mod


def read_text(p):
    with open(p, encoding='utf-8', errors='replace') as f:
        return f.read()


def read_bytes(p):
    with open(p, 'rb') as f:
        return f.read()


def hght_verts(data, what):
    """Vertices a tile side from a tile's HGHT (u16 per vertex, square)."""
    hght = dict(tt.chunks(data)).get(b'HGHT', b'')
    n = int(round((len(hght) / 2) ** 0.5))
    if n < 2 or 2 * n * n != len(hght):
        raise ValueError('HGHT of %s is %d bytes, not a square u16 grid' % (what, len(hght)))
    return n


def tile_layout(tiles_dir):
    """(vertices a tile side, tile count) from the first tile's HGHT."""
    names = mh.tile_names(tiles_dir)
    if not names:
        raise ValueError('no <terrain>_<n>.ttile in %s' % tiles_dir)
    first = names[min(names)]
    return hght_verts(read_bytes(os.path.join(tiles_dir, first)), first), len(names)


def read_override(backup):
    """{'tiles': dir, 'source': pak dir, 'created': [file names]} from <backup>/override.txt ({} when missing)."""
    p = os.path.join(backup, OVERRIDE)
    if not os.path.exists(p):
        return {}
    o = {'created': []}
    for line in read_text(p).splitlines():
        k, _, v = line.strip().partition(' ')
        if k == 'created':
            o['created'].append(v)
        elif k:
            o[k] = v
    return o


def write_override(backup, o):
    os.makedirs(backup, exist_ok=True)
    lines = ['tiles ' + o['tiles'], 'source ' + o['source']] + ['created ' + n for n in o['created']]
    with open(os.path.join(backup, OVERRIDE), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


GPROJ_RE = re.compile(r"gproj: '(.+)/ArmaReforger\.gproj' guid: '\w+' \(packed\)")


def game_data_from_log(addon):
    """The game's addons/data folder of the RUNNING Workbench (Stable or Experimental), from its newest console.log
    line "gproj: '<dir>/ArmaReforger.gproj' guid: '...' (packed)" (seen 2026-10-28; '$ArmaReforger:' is packed, so
    Workbench.GetAbsolutePath finds no folder for it). Logs: <My Games>/ArmaReforgerWorkbench/logs (= the addon's
    ../../logs, else under the user's Documents)."""
    roots = [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(addon))), 'logs'),
             os.path.join(os.path.expanduser('~'), 'Documents', 'My Games', 'ArmaReforgerWorkbench', 'logs')]
    logs = []
    for r in roots:
        if os.path.isdir(r):
            logs += [os.path.join(r, d, 'console.log') for d in os.listdir(r)]
    logs = sorted((p for p in set(logs) if os.path.isfile(p)), key=os.path.getmtime, reverse=True)
    for p in logs:
        m = GPROJ_RE.search(read_text(p))
        if m:
            return m.group(1)
    raise ValueError("cannot find the game's data folder: no ArmaReforger.gproj line in the Workbench logs (%s)"
                     % '; '.join(roots))


def locate(a, samples, out):
    """The pak terrain of this world: {'pak', 'dir' (pak .Data path), 'tiles' {n: pak path}, 'verts', 'fit'}.
    Every pak folder <x>/<name>/.Data/<name>_<n>.ttile is a candidate; the one whose heights fit the samples wins."""
    if a.game_data == 'auto':
        a.game_data = game_data_from_log(a.addon)
    pak = pf.PakFiles(a.game_data)
    name = re.escape(a.vanilla)
    tile_re = re.compile(r'^(.+/%s/\.Data)/%s_(\d+)\.ttile$' % (name, name), re.I)
    dirs = {}
    for p in pak.find(tile_re.pattern):
        m = tile_re.match(p)
        dirs.setdefault(m.group(1), {})[int(m.group(2))] = p
    if not dirs:
        raise ValueError("no '%s' terrain tiles in the paks of %s - not a vanilla terrain?" % (a.vanilla, a.game_data))
    best, tried = None, []
    for d, tiles in sorted(dirs.items()):
        if a.tiles_x <= 0 or len(tiles) % a.tiles_x or sorted(tiles) != list(range(len(tiles))):
            tried.append('%s (%d tiles)' % (d, len(tiles)))
            continue
        try:
            verts = hght_verts(pak.read(tiles[0]), tiles[0])
            mh.set_layout(verts, a.tiles_x)
            fitd = mh.fit(samples, lambda t, tiles=tiles: pak.read(tiles[t]))
        except (KeyError, ValueError, IndexError) as e:   # samples outside this terrain's tiles
            tried.append('%s (%s)' % (d, type(e).__name__))
            continue
        tried.append('%s (residual %.4f m)' % (d, fitd['resid']))
        if fitd['resid'] < FIT_MAX and (best is None or fitd['resid'] < best['fit']['resid']):
            best = {'pak': pak, 'dir': d, 'tiles': tiles, 'verts': verts, 'fit': fitd}
    if best is None:
        raise ValueError('no pak terrain fits the height samples - no tile touched. Tried: ' + '; '.join(tried))
    mh.set_layout(best['verts'], a.tiles_x)
    out('vanilla terrain %s: %d tiles, fit residual %.6f m (game data %s; candidates: %s)'
        % (best['dir'], len(best['tiles']), best['fit']['resid'], a.game_data, '; '.join(tried)))
    return best


def materialize(a, src, out):
    """Unchanged pak copies of every tile the mod does not hold yet -> <addon>/<pak dir>. Returns that dir."""
    dest = os.path.join(a.addon, *src['dir'].split('/'))
    os.makedirs(dest, exist_ok=True)
    o = read_override(a.backup) or {'created': []}
    o['tiles'], o['source'] = dest.replace('\\', '/'), src['dir']
    made = 0
    for t, p in sorted(src['tiles'].items()):
        fname = p.rsplit('/', 1)[1]
        target = os.path.join(dest, fname)
        if os.path.exists(target):
            continue
        with open(target, 'wb') as f:
            f.write(src['pak'].read(p))
        if fname not in o['created']:
            o['created'].append(fname)
        made += 1
    write_override(a.backup, o)   # before any hole: unbuild can always find what we added
    out('override tiles %s (%d created now, %d listed as ours, %d tiles)' % (o['tiles'], made, len(o['created']),
                                                                           len(src['tiles'])))
    return dest


def build(a, out):
    samples = mh.parse_samples(read_text(a.samples))
    if not samples:
        raise ValueError('no MUD bake sample lines in %s' % a.samples)
    if a.vanilla:
        src = locate(a, samples, out)
        verts, count = src['verts'], len(src['tiles'])
    else:
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
    if a.vanilla:
        a.tiles = materialize(a, src, out)

    # 1 fit (reads only)
    names = mh.tile_names(a.tiles)
    fitd = mh.fit(samples, lambda t: read_bytes(os.path.join(a.tiles, names[t])))
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
    o = read_override(a.backup)
    if o:
        a.tiles = o['tiles']   # vanilla mode: the plugin may only know the terrain name
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
    if o:
        gone = os.path.join(a.backup, REMOVED)
        os.makedirs(gone, exist_ok=True)
        moved, differ = 0, []
        for fname in o['created']:
            p = os.path.join(a.tiles, fname)
            if not os.path.exists(p):
                continue
            bak = os.path.join(a.backup, fname)
            if os.path.exists(bak) and read_bytes(bak) != read_bytes(p):
                differ.append(fname)   # still moved: the pak tile under it is the vanilla state
            os.replace(p, os.path.join(gone, fname))
            moved += 1
        o['created'] = []
        write_override(a.backup, o)
        out('override tiles moved out of the mod: %d (to %s), not equal to the pre-build copy: %d %s'
            % (moved, gone, len(differ), differ[:5]))


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['build', 'unbuild'])
    ap.add_argument('--tiles')
    ap.add_argument('--backup', required=True)
    ap.add_argument('--report', required=True)
    ap.add_argument('--mask')
    ap.add_argument('--pins')
    ap.add_argument('--samples')
    ap.add_argument('--heights')
    ap.add_argument('--tiles-x', type=int)
    ap.add_argument('--cell', type=float)
    ap.add_argument('--margin', type=int, default=1)
    ap.add_argument('--vanilla', help='terrain entity name: build into override tiles of that pak terrain')
    ap.add_argument('--game-data', help="the game's addons/data folder (the .pak files); 'auto' = from the Workbench log")
    ap.add_argument('--addon', help="this mod's root folder (override tiles go under it)")
    a = ap.parse_args(argv)
    if a.cmd == 'build' and None in (a.mask, a.pins, a.samples, a.heights, a.tiles_x, a.cell):
        ap.error('build needs --mask --pins --samples --heights --tiles-x --cell')
    if a.cmd == 'build' and a.vanilla and None in (a.game_data, a.addon):
        ap.error('build --vanilla needs --game-data --addon')
    if not a.vanilla and not a.tiles and not (a.cmd == 'unbuild' and read_override(a.backup)):
        ap.error('--tiles is needed (or --vanilla for build)')
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
