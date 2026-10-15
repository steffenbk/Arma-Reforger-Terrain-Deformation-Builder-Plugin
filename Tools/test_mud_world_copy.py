import struct

import pytest

import mud_world_copy as wc


def rdb_entry(path, guid):
    key = path.encode() + b'\0'
    return struct.pack('<I', len(key)) + key + b'\x06\0\0\0\0\0' + bytes.fromhex(guid)[::-1]


def test_rdb_guid_reads_little_endian_guid_after_path():
    rdb = b'FORM....' + rdb_entry('worlds/Arland', '1111111111111111') \
        + rdb_entry('worlds/Arland/Arland.ent', 'A9806AF617972E97') + rdb_entry('worlds/Arland/Arland.smap', '22')
    assert wc.rdb_guid(rdb, 'worlds/Arland/Arland.ent') == 'A9806AF617972E97'


def test_rdb_guid_does_not_match_a_longer_path_suffix():
    rdb = rdb_entry('worlds/Arland/EmptyArland.ent', 'BBBBBBBBBBBBBBBB')
    with pytest.raises(KeyError):
        wc.rdb_guid(rdb, 'Arland.ent')


def test_patch_parent_swaps_the_one_guid():
    gm = b'EBIN' + b'x' * 10 + bytes.fromhex('A9806AF617972E97')[::-1] + b'tail'
    out, at = wc.patch_parent(gm, 'A9806AF617972E97', 'EB55F161D3DB412A')
    assert at == 14 and len(out) == len(gm)
    assert out[14:22] == bytes.fromhex('EB55F161D3DB412A')[::-1] and out.endswith(b'tail')


@pytest.mark.parametrize('copies', [0, 2])
def test_patch_parent_refuses_missing_or_ambiguous_guid(copies):
    gm = b'EBIN' + bytes.fromhex('A9806AF617972E97')[::-1] * copies
    with pytest.raises(ValueError):
        wc.patch_parent(gm, 'A9806AF617972E97', 'EB55F161D3DB412A')


def test_plan_copy_renames_world_files_and_skips_other_worlds():
    paths = ['worlds/Arland/Arland.ent', 'worlds/Arland/EmptyArland.ent', 'worlds/Arland/Arland_SMD.conf',
             'worlds/Arland/Arland.smap', 'worlds/Arland/.Shore/x.bin', 'worlds/Arland/Terrain/Terrain.terr',
             'worlds/Arland/Terrain/.Data/Terrain_0.ttile', 'worlds/ArlandX/a.ent', 'worlds/Eden/Eden.ent']
    assert wc.plan_copy(paths, 'worlds/Arland/Arland.ent', 'MudArland') == {
        'worlds/Arland/Arland.ent': 'MudArland.ent',
        'worlds/Arland/Arland_SMD.conf': 'MudArland_SMD.conf',
        'worlds/Arland/Arland.smap': 'MudArland.smap',
        'worlds/Arland/.Shore/x.bin': '.Shore/x.bin',
        'worlds/Arland/Terrain/Terrain.terr': 'Terrain/Terrain.terr',
        'worlds/Arland/Terrain/.Data/Terrain_0.ttile': 'Terrain/.Data/Terrain_0.ttile',
    }
