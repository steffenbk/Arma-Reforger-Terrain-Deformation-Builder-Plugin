"""Terrain pass of the World Editor plugin 'MUD Ground: Build / Unbuild' (Scripts/WorkbenchGame/Mud/MUD_GroundBuildPlugin.c).

The plugin starts this with Workbench.RunProcess while the World Editor has ANOTHER world open (the tiles must not
be in use), waits for it, then reads --report. Any map: the tile layout comes from the tiles (HGHT size) and
--tiles-x (WorldEditorAPI.GetTerrainTilesX). Docs: docs/features/build-plugin/README.md.

    python -I Tools/mud_build.py build   --tiles <world dir>/<terrain name>/.Data --backup <dir> --mask <f>
                                         --pins <f> --samples <f> --heights <out .c> --tiles-x <n> --cell <m>
                                         --report <f> [--margin 1]
    python -I Tools/mud_build.py build   --vanilla <terrain entity name> --game-data <game addons/data dir | auto>
                                         --addon <this mod's root dir> (+ the build args above, no --tiles)
    python -I Tools/mud_build.py build   --vanilla <terrain entity name> --copy --game-data auto --addon <dir>
                                         (+ the build args above, no --tiles)
    python -I Tools/mud_build.py unbuild --tiles <dir> --backup <dir> --report <f> [--heights <the .c Build made>]
    python -I Tools/mud_build.py preview --mask <f> --pins <f> --report <f> [--boxes <f>] [--margin 1]
                                         [--out <dir, default: the mask's folder>]
  build and preview take --boxes <boxes.txt> (MUD_GroundBuildPlugin writes it from the open world; a missing file =
  no boxes): keep boxes (MUD_CutBoxEntity) stay real terrain, cut boxes (MUD_MudBoxEntity) become holes; order
  margin -> cut boxes -> keep boxes -> pins -> one cell per all-hole block (mud_holes.compose).
  preview reads / writes no tile: it composes as build would (NOT the map's own holes - those need the tiles) and
  writes preview.txt (MUD preview row runs by code) + preview.png (1 pixel a cell, north up).

Three modes:
  copy (2026-10-29, research/2026-10-29-map-copy-without-touching-original.md): the world is a SubScene of a world
    whose terrain sits in a DEPENDENCY mod (our packed MudArland copy). Same as vanilla mode, but the tiles are found
    in the dependency mods the project's .gproj lists (read from the Workbench log), and a folder the game's paks
    also hold is refused. The holed copies land at <addon>/<dependency .Data path>, a path only that copy reads, so
    the vanilla map and the dependency itself stay untouched (U4 2026-10-29: an override tile there wins per tile).
    Writes one more report line ('copy mode sources').
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
     Then (LOWER_HIDDEN) every vertex whose 4 cells are all our holes goes LOWER_DROP (1 m) down in the written tile (the retail
     terrain occlusion reads these hidden heights; mud_holes.lower_tile), recorded in <backup>/lowered.txt. A re-Build
     first puts those heights back (unlower_tile), so the lowering never stacks.
  4 bake: the heights of every hole block -> --heights (MUDH, mud_holes.bake), from the tiles BEFORE the lowering.
  5 groups: --groups (written by the plugin cut step from the open world) -> the hierarchy children whose top ancestor or own spot
    is a hole -> <key>_mud_groups.c beside --heights (MUD_PinLift lifts them by the ground at the top ancestor).
  6 roads: --road-export (plugin scan step, MUD_RoadExportPlugin) -> <key>_mud_roads.c beside --heights (mud_roads.bake_text).
unbuild: every tile gets its pristine HOLE chunk back and its lowered heights back (other heights and paint stay as
  they are now); --heights and the
  roads + groups files are moved into --backup as <name>.unbuilt; vanilla mode then moves the created override tiles out of the mod.
Report: one 'MUD build: ...' line per step, written as each step ends (the plugin's progress bar counts them),
  last line 'MUD build exit <code>' (0 = ok). build writes 7 step lines (vanilla 9), unbuild 1-4.
  Vanilla build writes 'MUD build: override tiles <dir>' - the plugin keeps that dir for unbuild.
"""
import argparse
import os
import re
import shutil
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I drops the script dir from sys.path
import mud_holes as mh  # noqa: E402
import mud_roads as mr  # noqa: E402
import pak_files as pf  # noqa: E402
import ttile_holes as tt  # noqa: E402

TOOLS_VERSION = 12  # same number in mud_holes.py + mud_roads.py and MUD_GroundBuildPlugin.TOOLS_VERSION (release_check
                   # tests it); bump all of them with any change to these Tools files - the plugin warns on a stale copy
FIT_MAX = 0.002  # m, as mud_holes fit
OVERRIDE = 'override.txt'            # in --backup: 'tiles <dir>', 'source <pak dir>', 'created <file>' lines
REMOVED = 'override_removed'         # in --backup: created override tiles unbuild moved out of the mod
# One real cell at the corner of every all-hole 64 m block (mud_holes.keep_one_per_full_block, for grass clutter,
# user 2026-10-10). Was OFF 2026-10-31 (lone squares stay real terrain a blast can't cut, 611 on Arlanda); the
# rebuild with it off left grass / clutter missing in many areas (user WB 2026-10-31) -> ON again.
KEEP_CORNER = True
# Lower the hidden heights under our holes (mud_holes.lower_tile; TRENCH SKY GAP, retail look check OK 2026-11-14).
LOWER_HIDDEN = True


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
GPROJ_ANY_RE = re.compile(r"gproj: '(.+)/[^/']+\.gproj' guid: '(\w+)'( \(packed\))?")
DEPS_RE = re.compile(r'Dependencies\s*\{([^}]*)\}')


def log_roots(addon):
    return [os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(addon))), 'logs'),
            os.path.join(os.path.expanduser('~'), 'Documents', 'My Games', 'ArmaReforgerWorkbench', 'logs')]


def newest_logs(addon):
    """Workbench console.log files, newest first. Logs: <My Games>/ArmaReforgerWorkbench/logs (= the addon's
    ../../logs, else under the user's Documents)."""
    logs = []
    for r in log_roots(addon):
        if os.path.isdir(r):
            logs += [os.path.join(r, d, 'console.log') for d in os.listdir(r)]
    return sorted((p for p in set(logs) if os.path.isfile(p)), key=os.path.getmtime, reverse=True)


def game_data_from_log(addon):
    """The game's addons/data folder of the RUNNING Workbench (Stable or Experimental), from its newest console.log
    line "gproj: '<dir>/ArmaReforger.gproj' guid: '...' (packed)" (seen 2026-10-28; '$ArmaReforger:' is packed, so
    Workbench.GetAbsolutePath finds no folder for it)."""
    for p in newest_logs(addon):
        m = GPROJ_RE.search(read_text(p))
        if m:
            return m.group(1)
    raise ValueError("cannot find the game's data folder: no ArmaReforger.gproj line in the Workbench logs (%s)"
                     % '; '.join(log_roots(addon)))


def same_dir(a, b):
    return os.path.normcase(os.path.abspath(a)) == os.path.normcase(os.path.abspath(b))


def dependency_dirs(addon, game_data):
    """[(guid, folder, packed)] of the mods this addon's .gproj lists under Dependencies, as the newest Workbench log
    that loaded this addon resolved them ("gproj: '<dir>/x.gproj' guid: 'G' [(packed)]", seen 2026-10-29). The
    game's own addons (the folders next to its addons/data) are left out. Direct dependencies only."""
    gp = sorted(f for f in os.listdir(addon) if f.lower().endswith('.gproj'))
    if not gp:
        raise ValueError('no .gproj in %s' % addon)
    m = DEPS_RE.search(read_text(os.path.join(addon, gp[0])))
    want = set(g.upper() for g in re.findall(r'[0-9A-Fa-f]{16}', m.group(1))) if m else set()
    game_root = os.path.dirname(os.path.abspath(game_data))
    for p in newest_logs(addon):
        rows = [(g.group(2).upper(), g.group(1), bool(g.group(3))) for g in GPROJ_ANY_RE.finditer(read_text(p))]
        if not any(same_dir(d, addon) for _, d, _ in rows):
            continue   # a log of a Workbench that did not load this project
        out, seen = [], set()
        for guid, d, packed in rows:
            key = (guid, os.path.normcase(os.path.abspath(d)))
            if guid in want and key not in seen and not same_dir(os.path.dirname(os.path.abspath(d)), game_root):
                seen.add(key)
                out.append((guid, d, packed))
        return out
    raise ValueError('no Workbench log lists the project %s - open it in Workbench first' % addon)


class LooseFiles:
    """The .ttile files of a loose (unpacked) mod folder, with PakFiles' find / read (paths relative to the root)."""

    def __init__(self, root, sub=None):
        self.dir = root
        self.index = {}
        for d, _, files in os.walk(os.path.join(root, sub) if sub else root):
            for f in files:
                if f.lower().endswith('.ttile'):
                    full = os.path.join(d, f)
                    self.index[os.path.relpath(full, root).replace('\\', '/')] = full

    def find(self, pattern):
        r = re.compile(pattern, re.I)
        return sorted(p for p in self.index if r.search(p))

    def read(self, path):
        return read_bytes(self.index[path])


PARENT_RE = re.compile(r'SubScene\s*\{\s*Parent\s+"(?:\{[0-9A-Fa-f]+\})?([^"]+)"')


def own_copy(a):
    """The world's parent world when it is in THIS project and holds the terrain's tiles (core mod: the mud maps sit
    on our map copy, e.g. worlds/MudArland/GM_MudArland.ent; user 2026-10-31 'do the same as the non-dev plugin'):
    its folder (relative to the addon) holding <name>/.Data, else None. The world file sits next to --heights
    (<world dir>/<key>_mud_heights.c, <key>.ent)."""
    if not a.heights or not a.heights.endswith('_mud_heights.c'):
        return None
    world = a.heights[:-len('_mud_heights.c')] + '.ent'
    if not os.path.exists(world):
        return None
    m = PARENT_RE.search(read_text(world))
    if not m:
        return None
    parent_dir = os.path.dirname(m.group(1).replace('\\', '/'))
    data = os.path.join(a.addon, *parent_dir.split('/'), a.vanilla, '.Data')
    if not os.path.exists(os.path.join(data, '%s_0.ttile' % a.vanilla)):
        return None
    return parent_dir + '/' + a.vanilla


def copy_sources(a, out):
    """Copy mode readers: one per dependency mod (its .pak files when packed, else its loose files). When the world's
    parent is a map copy in this project (own_copy), only that copy: its tiles are cut in place."""
    own = own_copy(a)
    if own:
        out('copy mode sources: this project %s (own map copy %s, cut in place)' % (a.addon, own))
        return [('this project %s (own copy)' % a.addon, LooseFiles(a.addon, own))]
    deps = dependency_dirs(a.addon, a.game_data)
    if not deps:
        raise ValueError('copy mode: %s lists no dependency mod (other than the game) in its .gproj' % a.addon)
    by_guid = {}
    for guid, d, _ in deps:
        by_guid.setdefault(guid, []).append(d)
    twice = {g: ds for g, ds in by_guid.items() if len(ds) > 1}
    if twice:   # e.g. a loose dev folder + its Workshop download: which one Workbench loads is not known here
        raise ValueError('copy mode: a dependency is in more than one folder, move all but one out of the addons '
                         'folder - no tile touched: %s' % '; '.join('%s: %s' % (g, ', '.join(ds)) for g, ds in twice.items()))
    srcs = []
    for guid, d, packed in deps:
        has_pak = os.path.isdir(d) and any(f.lower().endswith('.pak') for f in os.listdir(d))
        reader = pf.PakFiles(d) if has_pak else LooseFiles(d)
        srcs.append(('%s %s (%s)' % (guid, d, 'packed' if has_pak else 'loose'), reader))
    out('copy mode sources: ' + '; '.join(s[0] for s in srcs))
    return srcs


def locate(a, samples, out):
    """The packed terrain of this world: {'pak' (reader), 'dir' (.Data path), 'tiles' {n: path}, 'verts', 'fit'}.
    Every folder [<x>/]<name>/.Data/<name>_<n>.ttile is a candidate; the one whose heights fit the samples wins.
    Vanilla mode searches the game's paks, then the project's dependency mods (a map from a mod, seen 2026-11-01 on
    Belleau Wood: override tiles at its pak path work the same way). Copy mode searches only the dependency mods and
    refuses a folder the game's paks also hold (an override there would hole every world on that vanilla map, user
    test 2026-10-28)."""
    if a.game_data == 'auto':
        a.game_data = game_data_from_log(a.addon)
    game = pf.PakFiles(a.game_data)
    if not a.copy and own_copy(a):
        a.copy = True   # DEV entry on a world that sits on our map copy: cut the copy (user 2026-10-31), never vanilla
    if not a.copy and a.copy_only:   # every world on that game map would be holed (user tests 2026-10-28, 2026-10-31)
        raise ValueError("the world sits on a game map, not on a map copy in this project - cutting would hole every "
                         "world on that map, no tile touched. Put the world on a copy (SubScene of worlds/MudArland/"
                         "GM_MudArland.ent, worlds/MudCain/GM_MudCain.ent or worlds/MudEden/GM_MudEden.ent)")
    srcs = copy_sources(a, out) if a.copy else [('game data ' + a.game_data, game)]
    best, tried = fit_sources(a, samples, srcs, game)
    kind = 'copy' if a.copy else 'vanilla'
    dep_fault = ''
    if best is None and not a.copy:
        try:
            deps = copy_sources(a, out)
        except (OSError, ValueError) as e:
            dep_fault = ' (dependency mods not searched: %s)' % e
        else:
            srcs += deps
            best, more = fit_sources(a, samples, deps, game)
            tried += more
            kind = 'dependency mod'
    if not tried:
        seen = sorted(set(p.rsplit('/', 1)[0] for _, r in srcs for p in r.find(r'\.ttile$')))
        fixed = tile_dir_of(getattr(a, 'tile_dir', None))
        what = ("no tiles in the set tile folder '%s'" % fixed if fixed
                else "no '%s' terrain tiles (<x>/%s/.Data/%s_<n>.ttile)" % (a.vanilla, a.vanilla, a.vanilla))
        raise ValueError("%s in %s%s. Terrain tile folders there: %s - set one of them as 'Tile folder' in step 5 (Cut terrain)"
                         % (what, '; '.join(s[0] for s in srcs), dep_fault, ', '.join(seen[:20]) or 'none'))
    tried += [dep_fault.strip(' ()')] if dep_fault else []
    if best is None:
        raise ValueError('no packed terrain fits the height samples - no tile touched. Tried: ' + '; '.join(tried))
    mh.set_layout(best['verts'], a.tiles_x)
    out('%s terrain %s: %d tiles, fit residual %.6f m (game data %s; candidates: %s)'
        % (kind, best['dir'], len(best['tiles']), best['fit']['resid'], a.game_data, '; '.join(tried)))
    return best


def tile_dir_of(text):
    """--tile-dir as typed by a map maker -> '<path>/.Data' inside the game / mod, or '' (find it by name).
    Takes 'worlds/X/Terrain/.Data', the terrain folder without '/.Data', a tile file path, backslashes and a
    '$Mod:' or '{GUID}' prefix."""
    d = (text or '').strip().strip('"').replace('\\', '/')
    d = re.sub(r'^(\$[^:/]+:|\{[0-9A-Fa-f]{16}\})', '', d).strip('/')
    if d.lower().endswith('.ttile'):
        d = d.rsplit('/', 1)[0] if '/' in d else ''
    if d and d.split('/')[-1].lower() != '.data':
        d += '/.Data'
    return d


def fit_sources(a, samples, srcs, game):
    """The terrain in these sources that fits the samples best (None if none does) and a line per folder tried."""
    name = re.escape(a.vanilla)
    # the world may sit at the mod root (Congo Bushwar: Congo/.Data/Congo_0.ttile, user report 2026-11-02)
    tile_re = re.compile(r'^((?:.+/)?%s/\.Data)/%s_(\d+)\.ttile$' % (name, name), re.I)
    fixed = tile_dir_of(getattr(a, 'tile_dir', None))
    if fixed:   # the map maker's folder (plugin cut step 'Tile folder', user 2026-11-02): only that one, any tile name in it
        tile_re = re.compile(r'^(%s)/[^/]+_(\d+)\.ttile$' % re.escape(fixed), re.I)
    dirs = {}
    for label, reader in srcs:
        for p in reader.find(tile_re.pattern):
            m = tile_re.match(p)
            dirs.setdefault((m.group(1), label), (reader, {}))[1][int(m.group(2))] = p
    best, tried = None, []
    for (d, label), (pak, tiles) in sorted(dirs.items(), key=lambda kv: kv[0]):
        if a.copy and game.find('^' + re.escape(d) + '/'):
            tried.append('%s in %s (the game holds this path - refused)' % (d, label))
            continue
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
        tried.append('%s in %s (residual %.4f m)' % (d, label, fitd['resid']))
        if fitd['resid'] < FIT_MAX and (best is None or fitd['resid'] < best['fit']['resid']):
            best = {'pak': pak, 'dir': d, 'tiles': tiles, 'verts': verts, 'fit': fitd}
    return best, tried


def other_owner(backup, dest):
    """Build folder of ANOTHER world whose override tiles are in dest, else None. Two worlds on one map share the
    pak path (2026-10-31 review): the 2nd build would snapshot the 1st's holed tiles as pristine and list none as
    its own, and unbuilding the 1st would take the tiles the 2nd still uses."""
    if not os.path.isdir(dest):
        return None
    own = os.path.dirname(os.path.abspath(backup))
    root = os.path.dirname(own)
    for d in sorted(os.listdir(root)):
        folder = os.path.join(root, d)
        if os.path.normcase(folder) == os.path.normcase(own):
            continue
        o = read_override(os.path.join(folder, os.path.basename(backup)))
        if not o.get('tiles') or not os.path.isdir(o['tiles']):
            continue
        try:
            same = os.path.samefile(o['tiles'], dest)
        except OSError:
            same = False
        if same and any(os.path.exists(os.path.join(dest, n)) for n in o['created']):
            return folder
    return None


def materialize(a, src, out):
    """Unchanged pak copies of every tile the mod does not hold yet -> <addon>/<pak dir>. Returns that dir."""
    dest = os.path.join(a.addon, *src['dir'].split('/'))
    owner = other_owner(a.backup, dest)
    if owner:
        raise ValueError('the override tiles in %s belong to another built world (%s) - Unbuild that world first '
                         '(open it, MUD Ground: Unbuild), no tile touched' % (dest, owner))
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
    grids, box_note = load_boxes(a, mask)     # a broken boxes file stops here, before any tile step
    if a.vanilla:
        a.tiles = materialize(a, src, out)

    s = mh.stats(mask, a.margin, pins, keep_full=KEEP_CORNER, boxes=grids)   # reads only (up here for the E4 check)
    # review 2 E4 (user 2026-11-20: refuse only): tiles that already hold most of our holes but have no backup copy =
    # the profile backup was lost (wiped profile, other PC). Snapshotting them would make Unbuild unable to undo the
    # holes. Before the fit: on lowered tiles (LOWER_HIDDEN) the fit fails first with a FIT FAIL that hides the cause.
    overlap, planned = mh.holed_already(a.tiles, a.backup, s['block_map'])
    if planned >= mh.HOLED_MIN and overlap * 2 >= planned:
        fix = ('delete the holed override tiles in %s (the clean ones are in the game pak), then Cut again' % a.tiles
               if a.vanilla else 'put the clean tiles back from your own backup / version control, then Cut again')
        raise ValueError('the tiles already hold %d of the %d hole cells this cut makes, but the backup %s has no clean copy '
                         'of them (Workbench profile wiped or another PC?) - %s. No tile touched'
                         % (overlap, planned, a.backup, fix))

    # 1 fit (reads only)
    names = mh.tile_names(a.tiles)
    fitd = mh.fit(samples, lambda t: read_bytes(os.path.join(a.tiles, names[t])))
    lw_prev = mh.read_lowered(a.backup)
    if lw_prev and fitd['resid'] >= FIT_MAX:   # a re-Build: the samples may hold the real heights, the tiles the lowered
        real_fit = mh.fit(samples, lambda t: mh.unlower_tile(read_bytes(os.path.join(a.tiles, names[t])),
                                                             read_bytes(os.path.join(a.backup, names[t])), lw_prev))
        if real_fit['resid'] < fitd['resid']:
            fitd = real_fit
    out('fit samples %d orient %s a %.9f b %.6f max residual %.6f m%s' % (fitd['samples'], fitd['orient'], fitd['a'],
                                                                         fitd['b'], fitd['resid'],
                                                                         ' (chunk centres only)' if fitd.get('centre') else ''))
    if fitd['resid'] >= FIT_MAX:
        raise ValueError('FIT FAIL (residual %.4f m >= %.3f) - no tile touched' % (fitd['resid'], FIT_MAX))

    # 2 snapshot
    copied, total = mh.snapshot(a.tiles, a.backup)
    out('snapshot %d tiles copied now, backup holds %d (first copy kept)' % (copied, total))
    if total != count:
        raise ValueError('backup holds %d tiles, the world %d - no tile touched' % (total, count))

    # 3 holes: all tiles in memory first
    map_holes = {}
    for name in names.values():
        map_holes.update(mh.tile_holes(read_bytes(os.path.join(a.backup, name))))
    spared, map_whole = mh.spare_map_holes(s, mask['nx'], map_holes)
    if map_holes:
        out("the map's own holes: %d cells in %d blocks stay holes (%d blocks whole), %d blocks kept another cell"
            % (sum(bin(r).count('1') for rows in map_holes.values() for r in rows), len(map_holes), len(map_whole), spared))
    per_tile = {}
    for k, rows in s['block_map'].items():
        per_tile.setdefault(mh.owner_tile(k[0], k[1]), {})[k] = rows
    lost = sorted(set(per_tile) - set(names))
    if lost:
        raise ValueError('hole blocks for tiles that do not exist: %s - no tile touched' % lost[:5])
    lw_old = mh.read_lowered(a.backup)       # the last Build's lowering: undone first, so it never stacks
    lw = mh.lower_params(fitd) if LOWER_HIDDEN else None
    hrows = mh.hole_rows(s['block_map']) if lw else {}
    outs = {}
    real = {}                                 # name -> holed bytes before the lowering (bake reads these)
    ours = 0
    lowered = 0
    for t, name in sorted(names.items()):
        now = read_bytes(os.path.join(a.tiles, name))
        pristine = read_bytes(os.path.join(a.backup, name))
        base = mh.reset_holes(now, pristine)
        if lw_old:
            base = mh.unlower_tile(base, pristine, lw_old)
        mine = per_tile.get(t, {})
        ours += sum(bin(r).count('1') for rows in mine.values() for r in rows)
        real[name] = mh.merge_tile(base, mine, map_whole, refuse_full=KEEP_CORNER)
        data = real[name]
        if lw and mine:   # an all-hole vertex has 2+ of its cells in this tile, so a tile without our blocks has none
            data, n = mh.lower_tile(data, t, hrows, lw)
            lowered += n
        outs[t] = (name, now, data)
    if ours != s['holes']:
        raise ValueError('hole cells %d in blocks, mask says %d - no tile touched' % (ours, s['holes']))
    if lw:
        mh.write_lowered(a.backup, lw)        # before any tile: Unbuild / a re-Build can always undo it
    written = 0
    for t, (name, now, data) in sorted(outs.items()):
        if data != now:
            with open(os.path.join(a.tiles, name), 'wb') as f:
                f.write(data)
            written += 1
    if not lw and lw_old:
        os.remove(os.path.join(a.backup, mh.LOWER_FILE))
    low = (', hidden heights under holes %d vertices %.0f m lower' % (lowered, lw['drop'])) if lw else ''
    out('holes margin %d, pins %d (%d were holes): keep %d + holes %d = %d cells, %d blocks, %d tiles rewritten of %d%s'
        % (a.margin, len(pins['cells']), s['pins_new'], s['keep'], s['holes'], s['keep'] + s['holes'], s['blocks'],
           written, count, low) + box_note)

    # 4 bake
    os.makedirs(os.path.dirname(os.path.abspath(a.heights)), exist_ok=True)
    cells, why = scan_cells(a.mask, mask)
    b = mh.bake(a.tiles, fitd, a.heights, cells, tiles=real)
    note = ', cell codes baked (%d game materials)' % b['mats'] if cells else ', cell codes NOT baked (%s) - the load traces every hole cell' % why
    out('baked %(blocks)d blocks from %(tiles)d tiles, %(bytes)d bytes' % b + note + ' -> ' + a.heights)

    # 5 groups (2026-11-17, Kupiansk): hierarchy children whose top ancestor or own spot is a hole -> <key>_mud_groups.c
    x0, z0 = mh.box_origin(read_text(a.mask), mh.BOX)
    holes = dict(map_holes)
    for k, rows in s['block_map'].items():
        old = holes.get(k)
        holes[k] = [r | o for r, o in zip(rows, old)] if old else rows
    write_groups(a, out, (x0, z0, mask['cell']), holes)

    # 6 roads (road armor plan Task 2): the plugin scan step's road export -> <world key>_mud_roads.c next to the heights
    roads_out = roads_path(a.heights)
    if not a.heights.endswith('_mud_heights.c') or not a.road_export or not os.path.exists(a.road_export):
        out('roads NOT baked: no road export %s - run step 4 (Scan map) again for road armor' % a.road_export)
        if a.heights.endswith('_mud_heights.c'):
            remove_stale(roads_out, out)   # review 2 E3: an older cut's roads file would armor old roads
        return
    try:   # review 2 E2: bad road input must not fail a cut whose tiles are already written
        roads = mr.parse_export(read_text(a.road_export))
        x0, z0 = mh.box_origin(read_text(a.mask), mh.BOX)
        grid = (x0, z0, mask['nx'], mask['nz'], mask['cell'])
        text, armor = mr.bake_text(roads, grid)
    except Exception as e:  # noqa: BLE001 - any parse / bake fault is reported, the cut goes on
        out('roads NOT baked: %s: %s' % (type(e).__name__, e))
        remove_stale(roads_out, out)
        return
    with open(roads_out, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    out('roads %d (%d armor), %d points, %d armored cells baked -> %s'
        % (len(roads), armor, sum(len(r['pts']) for r in roads), mr.count_cells(text), roads_out))


def load_boxes(a, mask):
    """--boxes -> (box_grids or None, report text). No --boxes or no file = no boxes; a broken file raises."""
    if not a.boxes or not os.path.exists(a.boxes):
        return None, ''
    boxes = mh.parse_boxes(read_text(a.boxes))
    if not boxes:
        return None, ', boxes 0'
    x0, z0 = mh.box_origin(read_text(a.mask), mh.BOX)
    g = mh.box_grids(boxes, x0, z0, mask['cell'], mask['nx'], mask['nz'])
    return g, ', boxes keep %d cut %d' % (g['n_keep'], g['n_cut'])


def preview(a, out):
    """Compose as build would and write preview.txt + preview.png into --out (default the mask's folder)."""
    mask_text = read_text(a.mask)
    mask = mh.parse_mask(mask_text)
    pins = mh.parse_pins(read_text(a.pins))
    if (pins['nx'], pins['nz']) != (mask['nx'], mask['nz']):
        raise ValueError('size: pins %d x %d, mask %d x %d' % (pins['nx'], pins['nz'], mask['nx'], mask['nz']))
    grids, box_note = load_boxes(a, mask)
    nx, nz = mask['nx'], mask['nz']
    c = mh.compose(mask, a.margin, pins, grids, keep_full=KEEP_CORNER, codes=True)
    x0, z0 = mh.box_origin(mask_text, mh.BOX)
    text, t = mh.preview_text(c['codes'], nx, nz, x0, z0, mask['cell'])
    keep = sum(c['keep_grid'])
    if (t['keep'], t['holes']) != (keep, nx * nz - keep):
        raise ValueError('checksum: preview keep %d holes %d, composed keep %d' % (t['keep'], t['holes'], keep))
    d = a.out or os.path.dirname(os.path.abspath(a.mask))
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'preview.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    with open(os.path.join(d, 'preview.png'), 'wb') as f:
        f.write(mh.preview_png(c['codes'], nx, nz))
    n = {k: c['codes'].count(k) for k in range(1, 7)}
    out('preview margin %d%s: scan %d, margin %d, pins %d, keep box %d, grass corner %d; cut box %d (holes)'
        % (a.margin, box_note or ', boxes 0', n[1], n[2], n[3], n[4], n[6], n[5]))
    out("preview keep %d + holes %d = %d = %d x %d cells ok (the map's own holes are not in the preview)"
        % (t['keep'], t['holes'], t['keep'] + t['holes'], nx, nz))
    out('preview files %s %s' % (os.path.join(d, 'preview.txt').replace('\\', '/'),
                                 os.path.join(d, 'preview.png').replace('\\', '/')))


def scan_cells(mask_path, mask):
    """LOAD BAKE: step 3's cell file beside the mask (MUD_KeepScan CELLS_NAME / MATS_NAME) -> (cells, '') or
    (None, why). A missing or broken file only means the load traces as before, so it never stops the build."""
    d = os.path.dirname(os.path.abspath(mask_path))
    cpath, mpath = os.path.join(d, 'cells.c'), os.path.join(d, 'cell_mats.txt')
    if not os.path.exists(cpath) or not os.path.exists(mpath):
        return None, 'no %s from the scan - run step 4 (Scan map) again' % cpath
    try:
        cells = mh.parse_cells(read_bytes(cpath), read_text(mpath))
    except ValueError as e:
        return None, str(e)
    if (cells['nx'], cells['nz']) != (mask['nx'], mask['nz']):
        return None, 'cell file grid %d x %d, mask %d x %d' % (cells['nx'], cells['nz'], mask['nx'], mask['nz'])
    return cells, ''


def roads_path(heights):
    """<world dir>/<key>_mud_roads.c beside <key>_mud_heights.c"""
    return heights[:-len('_mud_heights.c')] + '_mud_roads.c'


def groups_path(heights):
    """<world dir>/<key>_mud_groups.c beside <key>_mud_heights.c (MUD_PinLift.LoadGroups)"""
    return heights[:-len('_mud_heights.c')] + '_mud_groups.c'


def parse_groups(text):
    """groups.txt of the plugin (MUD_GroundBuildPlugin.WriteGroups) -> [(cx, cz, tx, tz)]. Raises on a cut-off file."""
    rows, done = [], None
    for line in text.splitlines():
        w = line.split()
        if len(w) == 6 and w[:2] == ['MUD', 'group']:
            rows.append(tuple(float(v) for v in w[2:]))
        elif len(w) == 4 and w[:3] == ['MUD', 'groups', 'done']:
            done = int(w[3])
    if done != len(rows):
        raise ValueError('groups file broken: done %s, lines %d' % (done, len(rows)))
    return rows


def is_hole(holes, grid, x, z):
    """holes = {(bx, bz): 32 row bitmasks} (mask grid blocks); grid = (x0, z0, cell). The engine's cell: floor."""
    x0, z0, cell = grid
    ix, iz = int((x - x0) // cell), int((z - z0) // cell)
    if ix < 0 or iz < 0:
        return False
    rows = holes.get((ix // mh.BLOCK, iz // mh.BLOCK))
    return bool(rows) and bool(rows[iz % mh.BLOCK] >> (ix % mh.BLOCK) & 1)


def groups_text(rows, grid, holes):
    """Only the lines PinLift acts on: top ancestor on a hole (engine added nothing to the tree) or the child on a
    hole (PinLift must not lift it by its own spot). -> (text, kept)"""
    keep = [r for r in rows if is_hole(holes, grid, r[2], r[3]) or is_hole(holes, grid, r[0], r[1])]
    out = ['MUD groups 1']
    out += ['g %.3f %.3f %.3f %.3f' % r for r in keep]
    out.append('MUD groups done %d' % len(keep))
    return '\n'.join(out) + '\n', len(keep)


def remove_stale(path, out):
    """An older cut's output file next to the heights: removed so the game does not read old data."""
    if path and os.path.exists(path):
        os.remove(path)
        out('old %s removed (it belonged to an older cut)' % os.path.basename(path))


def write_groups(a, out, grid, holes):
    """Step 5. No --groups (an older plugin) or no file: report it and go on - PinLift then lifts by own spot."""
    if not a.heights.endswith('_mud_heights.c'):
        out('groups NOT written: heights file %s is not a <key>_mud_heights.c' % a.heights)
        return
    dst = groups_path(a.heights)
    if not a.groups or not os.path.exists(a.groups):
        out('groups NOT written: no groups file %s from the plugin (grouped objects lift by their own spot)' % a.groups)
        if os.path.exists(dst):
            os.remove(dst)     # a stale list of an older cut would lift by old holes
        return
    try:   # review 2 E2: bad group input must not fail a cut whose tiles are already written
        rows = parse_groups(read_text(a.groups))
        text, kept = groups_text(rows, grid, holes)
    except Exception as e:  # noqa: BLE001
        out('groups NOT written: %s: %s (grouped objects lift by their own spot)' % (type(e).__name__, e))
        remove_stale(dst, out)
        return
    with open(dst, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    out('groups %d of %d hierarchy children touch a hole -> %s' % (kept, len(rows), dst))


def unbuild(a, out):
    o = read_override(a.backup)
    if o:
        a.tiles = o['tiles']   # vanilla mode: the plugin may only know the terrain name
    lw = mh.read_lowered(a.backup)
    rep = mh.restore(a.tiles, a.backup, lw)
    if not rep:
        raise ValueError('backup %s holds no tiles - nothing to put back' % a.backup)
    if lw:
        os.remove(os.path.join(a.backup, mh.LOWER_FILE))
    before = sum(r[1] for r in rep)
    after = sum(max(r[2], 0) for r in rep)
    whole = sum(1 for r in rep if r[2] < 0)
    out('restore %d tiles: hole cells %d -> %d (pristine), %d tiles copied back whole' % (len(rep), before, after, whole))
    if a.heights and os.path.exists(a.heights):  # Build made it next to the world: moved out, not deleted
        keep = os.path.join(a.backup, os.path.basename(a.heights) + '.unbuilt')
        shutil.move(a.heights, keep)
        out('heights file moved to %s' % keep)
    roads = roads_path(a.heights) if a.heights and a.heights.endswith('_mud_heights.c') else None
    if roads and os.path.exists(roads):
        keep = os.path.join(a.backup, os.path.basename(roads) + '.unbuilt')
        shutil.move(roads, keep)
        out('roads file moved to %s' % keep)
    groups = groups_path(a.heights) if a.heights and a.heights.endswith('_mud_heights.c') else None
    if groups and os.path.exists(groups):
        keep = os.path.join(a.backup, os.path.basename(groups) + '.unbuilt')
        shutil.move(groups, keep)
        out('groups file moved to %s' % keep)
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
            shutil.move(p, os.path.join(gone, fname))
            moved += 1
        o['created'] = []
        write_override(a.backup, o)
        out('override tiles moved out of the mod: %d (to %s), not equal to the pre-build copy: %d %s'
            % (moved, gone, len(differ), differ[:5]))


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('cmd', choices=['build', 'unbuild', 'preview'])
    ap.add_argument('--tiles')
    ap.add_argument('--backup')
    ap.add_argument('--report', required=True)
    ap.add_argument('--mask')
    ap.add_argument('--pins')
    ap.add_argument('--samples')
    ap.add_argument('--heights')
    ap.add_argument('--road-export', help="the plugin scan step's MUD_RoadExportPlugin export; build bakes it into <key>_mud_roads.c")
    ap.add_argument('--tiles-x', type=int)
    ap.add_argument('--cell', type=float)
    ap.add_argument('--margin', type=int, default=1)
    ap.add_argument('--boxes', help='boxes.txt from the plugin (keep / cut boxes); a missing file = no boxes')
    ap.add_argument('--groups', help="groups.txt from the plugin (hierarchy children + top ancestor spots); build "
                                     "writes the ones touching a hole into <key>_mud_groups.c")
    ap.add_argument('--out', help='preview: folder for preview.txt + preview.png (default: the mask folder)')
    ap.add_argument('--vanilla', help='terrain entity name: build into override tiles of that pak terrain')
    ap.add_argument('--tile-dir', help="vanilla mode: the terrain's .Data folder inside the game / a mod, when "
                                       "finding it by the terrain name fails (still checked by the height fit)")
    ap.add_argument('--game-data',help="the game's addons/data folder (the .pak files); 'auto' = from the Workbench log")
    ap.add_argument('--addon', help="this mod's root folder (override tiles go under it)")
    ap.add_argument('--copy', action='store_true',
                    help="with --vanilla: find the terrain in this mod's dependency mods, not in the game's paks")
    ap.add_argument('--copy-only', action='store_true',
                    help="with --vanilla: refuse unless the world sits on a map copy in this project (DEV entries)")
    a = ap.parse_args(argv)
    if a.cmd == 'preview' and None in (a.mask, a.pins):
        ap.error('preview needs --mask --pins')
    if a.cmd != 'preview' and a.backup is None:
        ap.error('--backup is needed')
    if a.cmd == 'build' and None in (a.mask, a.pins, a.samples, a.heights, a.tiles_x, a.cell):
        ap.error('build needs --mask --pins --samples --heights --tiles-x --cell')
    if a.cmd == 'build' and a.vanilla and None in (a.game_data, a.addon):
        ap.error('build --vanilla needs --game-data --addon')
    if a.cmd != 'preview' and not a.vanilla and not a.tiles and not (a.cmd == 'unbuild' and read_override(a.backup)):
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
        {'build': build, 'unbuild': unbuild, 'preview': preview}[a.cmd](a, out)
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
