"""Own copy of a vanilla world, so holed tiles touch ONLY our map (I1, 2026-10-10 Exp 192624).

A world finds its terrain by LOCATION: <dir of the .ent that owns the terrain entity>/<entity name>/ (proven
2026-10-10: MudArland.ent copy loaded worlds/MudArland/Terrain/*, vanilla GM_Arland stayed clean; vanilla
EmptyArland.ent shares worlds/Arland/Terrain with no path stored). So tiles placed over the vanilla folder hit
every mission on that map; a copied world gets its own folder.

Copies from the pak into <addon>/worlds/<name>/:
  - every file under the world's folder except OTHER .ent worlds (Arland: EmptyArland.ent), top-level files
    renamed <World>* -> <name>* (.smap .smd .topo _SMD.conf: the engine looks them up by world name)
  - <name>.ent (byte copy) + .meta with a new GUID
  - with --gm: GM_<name>.ent = the GM layer with its parent GUID (8 bytes, little-endian, found exactly once)
    patched to the copy, + .meta. EBIN stores the parent by GUID only, so Workbench must be RESTARTED after a
    copy (the addon resource DB must learn the new GUIDs) or the game loads no world (SCR_MapLocator null).
  - with --tiles: our holed .ttile files over the copy's terrain .Data
Then point the scenario's SubScene Parent at the printed line. A game update does not reach the copy: re-run
(to a fresh name or after moving the old copy away), then mud_holes write -> fit -> bake.

    python -I Tools/mud_world_copy.py --world worlds/Arland/Arland.ent --gm worlds/GameMaster/GM_Arland.ent
        --name MudArland --addon <addon> --bk <blender-mcp-BK> [--tiles <dir of holed .ttile>]
        [--install experimental]
"""
import argparse
import os
import secrets
import shutil
import struct
import sys
import zlib

META = ('MetaFileClass {\n Name "{%s}%s"\n Configurations {\n  ENTResourceClass PC {\n  }\n'
        '  ENTResourceClass XBOX_ONE : PC {\n  }\n  ENTResourceClass XBOX_SERIES : PC {\n  }\n'
        '  ENTResourceClass PS4 : PC {\n  }\n  ENTResourceClass PS5 : PC {\n  }\n'
        '  ENTResourceClass HEADLESS : PC {\n  }\n }\n}')


def rdb_guid(rdb, path):
    """GUID (16 upper hex) of `path` in a resourceDatabase.rdb: u32 len, path, NUL, 6 bytes, GUID u64 LE."""
    key = path.encode() + b'\0'
    i = rdb.find(struct.pack('<I', len(key)) + key)
    if i < 0:
        raise KeyError('%s not in resourceDatabase.rdb' % path)
    g = rdb[i + 4 + len(key) + 6:i + 4 + len(key) + 14]
    return g[::-1].hex().upper()


def patch_parent(gm, old_guid, new_guid):
    """GM layer bytes with the parent world GUID swapped. ValueError unless the old GUID occurs exactly once."""
    old = bytes.fromhex(old_guid)[::-1]
    n = gm.count(old)
    if n != 1:
        raise ValueError('parent GUID %s found %d times in the GM layer (want 1)' % (old_guid, n))
    i = gm.find(old)
    return gm[:i] + bytes.fromhex(new_guid)[::-1] + gm[i + 8:], i


def plan_copy(paths, world, name):
    """{pak path: path relative to worlds/<name>/} for the world's folder."""
    wdir, wfile = world.rsplit('/', 1)
    stem = wfile[:-len('.ent')]
    out = {}
    for p in paths:
        if not p.startswith(wdir + '/'):
            continue
        rel = p[len(wdir) + 1:]
        if '/' not in rel:
            if rel.endswith('.ent') and rel != wfile:
                continue  # another world in the same folder
            if rel.startswith(stem):
                rel = name + rel[len(stem):]
        out[p] = rel
    return out


def new_guid():
    return secrets.token_hex(8).upper()  # no CR: a CR inside a .meta GUID breaks the resource (2026-10-10)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--world', required=True)
    ap.add_argument('--gm')
    ap.add_argument('--name', required=True)
    ap.add_argument('--addon', required=True)
    ap.add_argument('--bk', required=True)
    ap.add_argument('--tiles')
    ap.add_argument('--install', default='auto')
    a = ap.parse_args(argv)
    if not a.world.endswith('.ent') or '/' not in a.world:
        ap.error('--world is a pak path like worlds/Arland/Arland.ent')

    sys.path.insert(0, os.path.join(a.bk, 'tools'))
    import pakread
    inst = pakread.resolve_install(a.install)
    d = pakread.INSTALLS.get(inst, inst)
    idx = {}
    for f in sorted(os.listdir(d)):
        if f.endswith('.pak'):  # every pak: GM worlds live in worlds.pak, which pakread.Pak skips
            for p, e in pakread.parse_pak(os.path.join(d, f)).items():
                idx.setdefault(p, (f,) + e)

    def read(p):
        f, off, cl, ol = idx[p]
        with open(os.path.join(d, f), 'rb') as fh:
            fh.seek(off)
            b = fh.read(cl)
        return zlib.decompress(b) if cl != ol else b

    for p in [a.world] + ([a.gm] if a.gm else []):
        if p not in idx:
            sys.exit('not in the %s paks: %s' % (d, p))
    with open(os.path.join(d, 'resourceDatabase.rdb'), 'rb') as fh:
        world_guid = rdb_guid(fh.read(), a.world)

    target = os.path.join(a.addon, 'worlds', a.name)
    if os.path.exists(target):
        sys.exit('%s exists - move it away first (nothing is overwritten)' % target)
    gm_bytes = None
    g1, g2 = new_guid(), new_guid()
    if a.gm:
        gm_bytes, at = patch_parent(read(a.gm), world_guid, g1)  # check before writing anything

    plan = plan_copy(idx, a.world, a.name)
    for p, rel in sorted(plan.items()):
        out = os.path.join(target, rel)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, 'wb') as fh:
            fh.write(read(p))
    base = 'worlds/%s/' % a.name
    with open(os.path.join(target, a.name + '.ent.meta'), 'w', newline='\n') as fh:
        fh.write(META % (g1, base + a.name + '.ent'))
    print('copied %d files from %s (GUID %s) -> %s, %s.ent GUID %s'
          % (len(plan), a.world, world_guid, target, a.name, g1))

    if a.gm:
        gm_name = 'GM_%s.ent' % a.name
        with open(os.path.join(target, gm_name), 'wb') as fh:
            fh.write(gm_bytes)
        with open(os.path.join(target, gm_name + '.meta'), 'w', newline='\n') as fh:
            fh.write(META % (g2, base + gm_name))
        print('%s: parent patched at byte %d, GUID %s' % (gm_name, at, g2))
        print('scenario .ent:  SubScene {\n Parent "{%s}%s%s"\n}' % (g2, base, gm_name))

    if a.tiles:
        terrs = [r for r in plan.values() if r.endswith('.terr')]
        if len(terrs) != 1:
            sys.exit('want exactly 1 .terr in the copy, found %s - copy tiles by hand' % terrs)
        data = os.path.join(target, os.path.dirname(terrs[0]), '.Data')
        n = 0
        for f in sorted(os.listdir(a.tiles)):
            if f.endswith('.ttile'):
                if not os.path.exists(os.path.join(data, f)):
                    sys.exit('%s has no vanilla twin in %s' % (f, data))
                shutil.copy2(os.path.join(a.tiles, f), os.path.join(data, f))
                n += 1
        print('holed tiles copied over the vanilla ones: %d -> %s' % (n, data))
    print('NEXT: restart Workbench fully (new GUIDs), then open the scenario.')


if __name__ == '__main__':
    main()
