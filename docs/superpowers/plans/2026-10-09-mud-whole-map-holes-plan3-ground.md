# Mud Whole-Map Holes - Deformable Ground + MP (Plan 3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wheel ruts first, then craters, trenches, depth boxes and dirt walls, on the holed Arland, synced in MP + JIP: 8 m fine patches swap in over the coarse skin where the ground moves.

**Architecture:** Task 1 measures the risks Plan 2 carried over (shipped scripts reading `GetSurfaceY` = -256 on hole cells, runtime spawns, LOD cracks) and Task 2 fixes only what Task 1 saw fail. Task 3 makes the coarse skin keep its per-chunk heights and cells and lets cells be covered and uncovered. Task 4 adds `MUD_WorldGroundEntity`: sparse 8 m fine patches (33 x 33 verts, 0.25 m) that start exactly on the skin triangles and cover those skin cells. Task 5 adds ruts (RUTS FIRST, tested in play before anything else deforms), Tasks 6-8 craters + floor + depth boxes + plate + bullet slab, trenches, dirt walls. Tasks 9-10 add the server-authoritative sync (live per-patch rect messages, JIP key list + batches nearest first). Task 11 bakes the skin heights at hole-write time. Task 12 (skirts) runs only if Task 1 saw cracks.

**Tech Stack:** Enforce Script (Arma Reforger Experimental 192624 Workbench), Python 3 stdlib + pytest, `blender-mcp-BK/tools/pakread.py`.

**Spec:** `docs/superpowers/specs/2026-10-08-mud-whole-map-holes-design.md` (sections 4.3, 5.2, 5.3, 5.4, 6, 7 steps 4 rest + 5, 8). Ledger this plan stands on: `.superpowers/sdd/2026-10-08-mud-whole-map-holes-plan2-tool-skin/progress.md` ("Carried to Plan 3"). Skin results: `docs/superpowers/results/2026-10-09-plan2-task3-skin.md`. KB: `~/.claude/arma-knowledge/patterns/Scripting_And_Core/runtime-procedural-mesh.md`, `~/.claude/arma-knowledge/patterns/Terrain_And_Environment/world-environment.md` (terrain holes).

**Scope:** spec build order step 4 minus what Plan 2 did (fine patches, coarse swap, Edge Mode, depth boxes, floor, hard surfaces, bullet slab) plus step 5 (MP / JIP), plus the carried items of the Plan 2 ledger.

**Deviations from the spec (decided here, each costs one task if wrong):**
- Spec 5 says "one placed entity (refactor of `MUD_GroundEntity`)". This plan adds a new `MUD_WorldGroundEntity` (+ `MUD_FinePatch.c`) next to the coarse skin instead of refactoring the 2365-line `MUD_GroundEntity`, which stays for the NEw test map. Algorithms (press, crater, dig, smoothing, mesh, collider, net packing) are copied from it with line references. The old entity is archived after Task 10 passes (Task 10 last step).
- Spec 5.2 "cells within ~30 m of a vehicle": here patches are made ahead only around MOVING wheeled vehicles, radius attribute default **16 m**, at most **2 per frame**. Every stamp (rut, crater, dig, server message) still creates the patches it touches in the same call, as the spec says, so the radius only decides how early.
- Spec 5.2 "today's patch code reused": surface game materials of a fine patch come from the skin's per-2 m-cell gamemat (one trace per hole cell, Plan 2), not from a new 1 m surface sampling.
- Spec 4.3 Locked border "0.3 m skirt": only built if Task 1 sees cracks (Task 12). Plan 2 never looked (ledger item 3).
- Spec 5.3 "safety plate 0.5 m under the deepest floor": per fine patch (base + floor - 0.5 m), not a separate map-wide plate. The skin never deforms, so nothing can get under it except through a fine patch.
- Spec 6 "16 bits +-32 m": heights are mm deltas from the undeformed base, clamped to -32768..32767.

Paths below: `ADDON` = `C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\addons\Testing plan vech msh`, `BK` = `C:\Users\Steffen\Documents\A_documents\Github\blender-mcp-BK`, `DATA` = `ADDON\worlds\Arland\Terrain\.Data`, `TILEBAK` = `C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\mud_backups\tiles`, `ARCHIVE` = `C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\mud_backups\archive\scripts`. Workbench log folder: newest `C:\Users\Steffen\Documents\My Games\ArmaReforgerWorkbench\logs\logs_<date>_<time>\` (`console.log`; `$logs:` files land in the same folder). LEDGER = `ADDON\.superpowers\sdd\2026-10-09-mud-whole-map-holes-plan3-ground\progress.md` (create in Task 1 step 1; one line per finding, ruling or number, with time).

## Global Constraints

- Target: Arma Reforger **Experimental 192624** only (Workbench "Experimental, port 5776"). Game files from the Experimental pak (`pakread.Pak('experimental')`), never the stale loose `addons/data` tree or `E:\Arma reforger data\extracted_files\`.
- Arland: terrain box `<0,-163,0> .. <4096,148.375,4096>`, **2 m** cells, 2048 x 2048 cells, 64 x 64 skin chunks of 64 m (32 x 32 cells, 33 x 33 verts), 1697 chunks with holes, 1340627 hole cells (Plan 2 results).
- Fine patch: **8 m** = 4 x 4 skin cells = 32 x 32 fine cells of **0.25 m**, 33 x 33 verts; patch grid 512 x 512; key = `pz * 512 + px` (`m_iNPX`, not a literal, in code).
- Hole test in script: `GetSurfaceY(x, z) < -255` (-256 in a hole). Heightmap height in or near holes: `TraceFlags.WORLD` trace with a terrain-only filter, retrace from 1 cm under any non-terrain hit (WORLD traces hit `RoadEntity`, Plan 2). Never `GetSurfaceY` for a height.
- Our ground (skin chunks, fine patches) by trace: `TraceFlags.ENTS | TraceFlags.WORLD` with a filter, then check `TraceEnt` (ENTS alone missed 1060 of 2000 skin cells, Plan 2).
- Bodies: one trimesh per game material, `Physics.CreateStaticEx` with 1-6 geoms (literal array per count), layer `EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage`; old body `Destroy()` and new one made in the same script call.
- Meshes: `MeshObject.Create(..., 0)` (static), rebuilt on every change; the material must load or Create returns null; UVs inside 0..1; `UpdateIndices` before `UpdateVerts`; call `ent.Update()` after `SetObject` (else culling uses stale bounds).
- Script class members max 256 KB: big buffers in helper classes. Fresh static `bool` arrays read true: use `int`. Never index with a float (`Math.Min`/`Max` return float: use `Math.ClampInt` or int vars). No `%` in float expressions. `string.Format` takes at most 9 arguments.
- Skin material `{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat`, dirt gamemat `{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat`, groove material `{6EBF7ADA9FD582F7}Assets/Structures/Military/Fortifications/DirtMounds/Data/DirtCover_01.emat`, skin height +0.001 m.
- **User compiles** every changed `.c` (open in Script Editor, Ctrl+S / F7); `wb_reload` does not compile. `wb_play` needs the user to focus the game window. Ask, then wait for "compiled" / "in play".
- **Close the world in Workbench before any `.layer` or `.ttile` write.** Tile backups to `TILEBAK`. `.ttile` writes only through `Tools/mud_holes.py`.
- **Measure, don't guess:** every cause is confirmed by a log probe in play before a fix is written; a task whose fix has no failing probe line is skipped and the ledger says why.
- **Adversarial review:** before a task with new or big Enforce Script changes is called done, run `engineering-skills:adversarial-reviewer` on the changed files, fix or rule on every finding in the LEDGER.
- No git (user rule). Results in `ADDON\docs\superpowers\results\<run date>-plan3-task<N>-<topic>.md` with time and game build; whole per-item tables plus a checksum line (buckets sum to the total, and the sum is written out).
- TEMP code (probes, test attributes) is marked `TEMP` in a comment and removed by the step named in its task.
- Old `MUD_GroundEntity` keeps working on the NEw test map until Task 10's archive step; nothing here changes its behaviour except the static hooks named in Task 4 (`HideFromTraces` calls the world ground, new `ResetTraceHide`); Task 7 only adds a fallback call after `MUD_GroundEntity.DigAt` in `MUD_Dig.c`.

## Review Focus

1. **Seam between two patches when one is created next to an already deformed one.** A rut that ends on a patch border, then a new patch made later beside it, must not leave a step: the new patch takes the shared edge heights from its existing neighbour. Expected: debug line `MUD world seams: ... mismatched 0` after driving along and across patch borders. Pinned: Task 5 step 7 (`SeamCheck`, plus the drive pattern).
2. **Wheel on something above our ground (bridge deck, rubble, a truck bed) over hole cells.** Expected: no rut under it; the press is skipped by the 0.5 m height check and counted. Pinned: Task 5 step 8 (`skips (height)` rises while the wheel is on the raised object, no patch deformed under it).
3. **JIP full copy and a live message for the same patch arriving in either order.** Expected: the client ends with the server's heights in both orders (full copy then later live; live first then the older full copy, which is replayed under the stored live rect). Pinned: Task 10 step 6 (forced order test with `m_bJipTestDelayFull`) and step 7 (`Tools/compare_hashes.py` line `missing 0, extra 0, mismatched 0`; hashes are per patch).
4. **A long drive makes many deformed patches (memory).** Expected: patches stay ~35 KB each, a warning past `m_iBudget`, no hitch above a few ms per frame. Pinned: Task 5 step 9 (stats line `deformed`, `mem`, `worst frame` after a 2 km off-road drive) and the budget warning test with `m_iBudget 5`.
5. **RplSave too big for a joiner after a long match (many deformed keys).** Expected: the joiner gets every key or the size limit is known before players hit it. Pinned: Task 10 step 8 (TEMP fake keys 1000 / 10000 / 100000, join works or fails, bytes logged).

---

## File map

| File | Task | Responsibility |
|---|---|---|
| `Scripts/Game/Mud/MUD_RiskProbe.c` (create, TEMP) | 1, deleted in 2 | in-play risk probe: AI spawn, house kill, grenade, sunk watcher |
| `Tools/surface_callers.py`, `Tools/test_surface_callers.py` (create) | 1 | offline table of shipped `GetSurfaceY` / `GetTerrainY` callers + RELATIVE_Y prefab count |
| `Scripts/Game/Mud/MUD_Surface.c` (create) | 2 | `IsHole`, `HeightmapY`, `Y` (our ground, else heightmap) |
| `Scripts/Game/Mud/MUD_HoleFixes.c` (create, only fixes Task 1 saw fail) | 2 | modded shipped classes (terrain helper / debris snap, AI group spawn) |
| `Scripts/Game/Mud/MUD_CoarseSkin.c` (modify) | 3, 11, 12 | per-chunk records, cover API, baked heights, skirts |
| `Scripts/Game/Mud/MUD_FinePatch.c` (create) | 4 | patch entity class, per-patch data, shared build buffers |
| `Scripts/Game/Mud/MUD_WorldGround.c` (create) | 4-10 | the world ground entity: patches, stamps, builds, sync |
| `Scripts/Game/Mud/MUD_Wheels.c` (create) | 4 | registry of wheeled vehicles |
| `Scripts/Game/Mud/MUD_DepthBox.c` (create) | 6 | depth box entity |
| `Scripts/Game/Mud/MUD_WorldGroundNet.c` (create) | 10 | modded `SCR_PlayerController`: JIP request / batches |
| `Scripts/Game/Mud/MUD_VehicleAccess.c`, `MUD_BuildingDestructionFix.c`, `MUD_GroundEntity.c` (modify, small) | 4 | world ground in get-in hiding and ruin query |
| `Scripts/Game/Mud/MUD_Dig.c` (modify) | 7 | shovel on the world ground |
| `Prefabs/Mud/MudWorldGround.et` (+ `.meta`) (create) | 9 | world ground + RplComponent |
| `Arlnd_Layers/default.layer` (modify, world closed) | 1, 4, 9 | probe, world ground entity, prefab instance |
| `Tools/mud_holes.py`, `Tools/test_mud_holes.py` (modify) | 11 | `fit` and `bake` subcommands |
| `Tools/compare_hashes.py`, `Tools/test_compare_hashes.py` (create) | 9 | compare the last `MUD hash` block of a server and a client log |
| `Data/Mud/mud_fit.json`, `Data/Mud/mud_heights.bin` (written by the tool) | 11 | HGHT fit, baked skin vertex heights |

---

### Task 1: Measure the carried risks

**Files:**
- Create: `ADDON/Scripts/Game/Mud/MUD_RiskProbe.c` (TEMP, deleted in Task 2 step 7)
- Create: `ADDON/Tools/surface_callers.py`, `ADDON/Tools/test_surface_callers.py`
- Modify: `ADDON/Arlnd_Layers/default.layer` (append the probe entity; world closed)
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task1-risks.md`
- Create: LEDGER

**Interfaces:**
- Consumes: shipped `SCR_TerrainHelper.GetTerrainY(vector pos, BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null)` (pak `scripts/Game/Helpers/SCR_TerrainHelper.c:12`), `SCR_DamageManagerComponent.Kill(notnull Instigator instigator)` (`scripts/GameCode/Components/SCR_DamageManagerComponent.c:536`; `SCR_DestructibleBuildingComponent` derives from it), `SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect()` invoker `(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)`, prefabs `{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et`, `{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et`, `{E8F00BF730225B00}Prefabs/Weapons/Grenades/Grenade_M67.et` (GUIDs grepped from the pak entity catalogs 2026-10-09).
- Produces: results file rows R1-R10 with PASS / FAIL; Task 2 reads them to pick fixes F1-F3. Task 12 reads R7.

- [ ] **Step 1: Ledger**

Create LEDGER with:

```markdown
# Plan 3 ledger (mud whole-map holes, deformable ground + MP)
Plan: docs/superpowers/plans/2026-10-09-mud-whole-map-holes-plan3-ground.md
Build: Experimental 192624. One line per finding / ruling / number, with time.
```

- [ ] **Step 2: Write the failing test for the offline caller scan**

Create `ADDON/Tools/test_surface_callers.py`:

```python
import surface_callers as sc

SRC = '''class A
{
	void Foo(vector p)
	{
		float y = GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
	}

	static float Bar(vector pos)
	{
		// GetSurfaceY in a comment does not count
		return SCR_TerrainHelper.GetTerrainY(pos);
	}
}
'''


def test_find_calls_reports_function_and_line():
    rows = sc.find_calls('scripts/Game/A.c', SRC)
    assert [(r['line'], r['api'], r['func']) for r in rows] == [
        (5, 'GetSurfaceY', 'Foo'),
        (11, 'GetTerrainY', 'Bar'),
    ]


def test_relative_y_flag_detection():
    assert sc.has_relative_y('SomeEntity {\n Flags 525315\n}\n')      # 0x80000 | 1027
    assert not sc.has_relative_y('SomeEntity {\n Flags 1027\n}\n')
    assert not sc.has_relative_y('SomeEntity {\n}\n')
```

- [ ] **Step 3: Run it to see it fail**

Run: `cd ADDON/Tools && python -m pytest test_surface_callers.py -q`
Expected: FAIL, `ModuleNotFoundError: No module named 'surface_callers'`.

- [ ] **Step 4: Write `surface_callers.py`**

Create `ADDON/Tools/surface_callers.py`:

```python
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
```

- [ ] **Step 5: Run the tests, then the scan**

Run: `cd ADDON/Tools && python -m pytest test_surface_callers.py -q`
Expected: `2 passed`.

Run: `cd ADDON/Tools && python -I surface_callers.py ../docs/superpowers/results/<run date>-plan3-task1-callers.md`
Expected: one line `calls N in M files, prefabs P, relY R -> ...`. The Plan 2 ledger said "72 scripts / 134 calls" with another regex; this file's numbers replace it (write both into the LEDGER, with the regex). R (RELATIVE_Y prefabs) is expected 0; if not 0, every listed prefab is a runtime-spawn risk and goes into row R9.

- [ ] **Step 6: Write the in-play probe**

Create `ADDON/Scripts/Game/Mud/MUD_RiskProbe.c`:

```c
[EntityEditorProps(category: "Mud", description: "TEMP plan 3 task 1: measures the hole risks carried from plan 2, prints 'MUD risk' lines. Deleted in task 2.", color: "255 0 255 255")]
class MUD_RiskProbeEntityClass : GenericEntityClass
{
}

//! TEMP (plan 3 task 1). Near itself it finds a hole point (3 x 3 hole cells) and a kept control point
//! (3 x 3 kept cells above sea level), then: an AI fire team on both, a rifleman 3 m off the hole point,
//! the house nearest m_vHouse killed, three M67 at the player's feet (the user throws one onto a hole
//! cell). Every root entity that appears near those points (or near an explosion) is checked at
//! +3 / +10 / +30 s: "sunk" = more than 1 m under the heightmap (WORLD trace, terrain only).
class MUD_RiskProbeEntity : GenericEntity
{
	[Attribute("{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et", UIWidgets.ResourcePickerThumbnail, "AI group spawned on both points", "et")]
	protected ResourceName m_Group;

	[Attribute("{26A9756790131354}Prefabs/Characters/Factions/BLUFOR/US_Army/Character_US_Rifleman.et", UIWidgets.ResourcePickerThumbnail, "Soldier spawned 3 m off the hole point", "et")]
	protected ResourceName m_Soldier;

	[Attribute("{E8F00BF730225B00}Prefabs/Weapons/Grenades/Grenade_M67.et", UIWidgets.ResourcePickerThumbnail, "Grenade dropped at the player's feet", "et")]
	protected ResourceName m_Grenade;

	[Attribute("1260.776 0 2209.114", UIWidgets.Coords, "Kill the SCR_DestructibleBuildingEntity nearest this XZ (within 30 m)")]
	protected vector m_vHouse;

	[Attribute("300", UIWidgets.EditBox, "Search radius (m) around this entity for the two points")]
	protected float m_fSearch;

	[Attribute("40", UIWidgets.EditBox, "Watch box half size (m)")]
	protected float m_fWatch;

	protected float m_fTime;
	protected int m_iStage;
	protected vector m_vHole;
	protected vector m_vKept;
	protected ref array<vector> m_aWatchAt = {};
	protected ref array<float> m_aWatchT0 = {};
	protected ref array<int> m_aWatchDone = {};
	protected ref array<string> m_aWatchName = {};
	protected ref set<IEntity> m_sBaseline = new set<IEntity>();
	protected ref array<IEntity> m_aFound = {};
	protected ref TraceParam m_Trace = new TraceParam();

	//------------------------------------------------------------------------------------------------
	void MUD_RiskProbeEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	void ~MUD_RiskProbeEntity()
	{
		SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Remove(OnExplosion);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!GetGame().InPlayMode())
			return;
		m_fTime += timeSlice;

		if (m_iStage == 0 && m_fTime >= 5)
		{
			m_iStage = 1;
			if (!FindPoints())
			{
				Print("MUD risk: FAILED - no hole point or no control point within the search radius", LogLevel.ERROR);
				m_iStage = 9;
				return;
			}
			Baseline(m_vHole);
			Baseline(m_vKept);
			Baseline(m_vHouse);
			SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Insert(OnExplosion);
		}
		else if (m_iStage == 1 && m_fTime >= 6)
		{
			m_iStage = 2;
			RunTests();
		}

		for (int i = 0; i < m_aWatchAt.Count(); i++)
		{
			int done = m_aWatchDone[i];
			if (done >= 3 || m_fTime - m_aWatchT0[i] < CheckAt(done))
				continue;
			m_aWatchDone[i] = done + 1;
			CheckWatch(i, CheckAt(done));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds after a watch starts for its checks 0, 1, 2
	protected float CheckAt(int n)
	{
		if (n == 0)
			return 3;
		if (n == 1)
			return 10;
		return 30;
	}

	//------------------------------------------------------------------------------------------------
	//! Heightmap height by WORLD trace (also inside holes), terrain only, retraced under roads
	protected float HeightmapY(float x, float z)
	{
		m_Trace.Start = Vector(x, 400, z);
		m_Trace.End = Vector(x, -300, z);
		m_Trace.Flags = TraceFlags.WORLD;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.TraceEnt = null;
		float frac = GetWorld().TraceMove(m_Trace, TerrainOnly);
		float y = 400 - 700 * frac;
		for (int r = 0; r < 4 && frac < 1 && m_Trace.TraceEnt && !GenericTerrainEntity.Cast(m_Trace.TraceEnt); r++)
		{
			m_Trace.Start = Vector(x, y - 0.01, z);
			m_Trace.TraceEnt = null;
			frac = GetWorld().TraceMove(m_Trace, TerrainOnly);
			y = m_Trace.Start[1] + (-300 - m_Trace.Start[1]) * frac;
		}
		return y;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TerrainOnly(notnull IEntity e)
	{
		return GenericTerrainEntity.Cast(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! 3 x 3 cells around cell (cx, cz) all holes (want 1) or all kept above sea level (want 0)
	protected bool Block(int cx, int cz, bool hole)
	{
		BaseWorld world = GetWorld();
		for (int dz = -1; dz <= 1; dz++)
		{
			for (int dx = -1; dx <= 1; dx++)
			{
				float s = world.GetSurfaceY((cx + dx) * 2 + 1, (cz + dz) * 2 + 1);
				if (hole && s > -255)
					return false;
				if (!hole && s < 1)
					return false;
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Nearest hole block and kept block, ring by ring of 2 m cells around this entity
	protected bool FindPoints()
	{
		vector o = GetOrigin();
		int ocx = Math.Floor(o[0] / 2);
		int ocz = Math.Floor(o[2] / 2);
		int rings = m_fSearch / 2;
		bool haveHole;
		bool haveKept;
		for (int r = 0; r <= rings && !(haveHole && haveKept); r++)
		{
			for (int dz = -r; dz <= r; dz++)
			{
				for (int dx = -r; dx <= r; dx++)
				{
					if (Math.AbsInt(dx) != r && Math.AbsInt(dz) != r)
						continue;   // ring only
					int cx = ocx + dx;
					int cz = ocz + dz;
					if (!haveHole && Block(cx, cz, true))
					{
						haveHole = true;
						m_vHole = Vector(cx * 2 + 1, 0, cz * 2 + 1);
						m_vHole[1] = HeightmapY(m_vHole[0], m_vHole[2]);
					}
					if (!haveKept && Block(cx, cz, false))
					{
						haveKept = true;
						m_vKept = Vector(cx * 2 + 1, 0, cz * 2 + 1);
						m_vKept[1] = GetWorld().GetSurfaceY(m_vKept[0], m_vKept[2]);
					}
				}
			}
		}
		if (!haveHole || !haveKept)
			return false;

		float helper = SCR_TerrainHelper.GetTerrainY(m_vHole, GetWorld());
		Print(string.Format("MUD risk: hole point %1 GetSurfaceY %2 heightmap %3 | SCR_TerrainHelper.GetTerrainY %4 | control %5 GetSurfaceY %6",
			m_vHole, GetWorld().GetSurfaceY(m_vHole[0], m_vHole[2]), m_vHole[1], helper, m_vKept, m_vKept[1]));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddFound(IEntity e)
	{
		if (!e.GetParent())
			m_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Query(vector at)
	{
		m_aFound.Clear();
		vector half = Vector(m_fWatch, 200, m_fWatch);
		GetWorld().QueryEntitiesByAABB(at - half, at + half, AddFound);
	}

	//------------------------------------------------------------------------------------------------
	protected void Baseline(vector at)
	{
		Query(at);
		foreach (IEntity e : m_aFound)
			m_sBaseline.Insert(e);
	}

	//------------------------------------------------------------------------------------------------
	protected void Watch(vector at, string name)
	{
		m_aWatchAt.Insert(at);
		m_aWatchT0.Insert(m_fTime);
		m_aWatchDone.Insert(0);
		m_aWatchName.Insert(name);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Spawn(ResourceName res, vector pos)
	{
		Resource r = Resource.Load(res);
		if (!r || !r.IsValid())
		{
			Print("MUD risk: cannot load " + res, LogLevel.ERROR);
			return null;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		return GetGame().SpawnEntityPrefab(r, GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTests()
	{
		// R1 / R2: AI group on the hole point and on the control point, placed at the ground height
		IEntity g1 = Spawn(m_Group, m_vHole + Vector(0, 0.2, 0));
		IEntity g2 = Spawn(m_Group, m_vKept + Vector(0, 0.2, 0));
		Print(string.Format("MUD risk: group on hole %1, group on control %2", g1 != null, g2 != null));
		Watch(m_vHole, "hole point");
		Watch(m_vKept, "control point");

		// R3: a runtime soldier 3 m off the hole point
		vector sp = m_vHole + Vector(3, 0, 0);
		sp[1] = HeightmapY(sp[0], sp[2]) + 0.2;
		IEntity s = Spawn(m_Soldier, sp);
		Print(string.Format("MUD risk: soldier spawned %1 at %2", s != null, sp));

		// R4: destroyed house (debris, interior sink)
		vector hp = m_vHouse;
		hp[1] = HeightmapY(hp[0], hp[2]);
		m_aFound.Clear();
		GetWorld().QueryEntitiesBySphere(hp, 30, AddFound);
		SCR_DestructibleBuildingEntity house;
		float best = 1e9;
		foreach (IEntity e : m_aFound)
		{
			SCR_DestructibleBuildingEntity b = SCR_DestructibleBuildingEntity.Cast(e);
			if (!b)
				continue;
			float d = vector.DistanceXZ(b.GetOrigin(), hp);
			if (d < best)
			{
				best = d;
				house = b;
			}
		}
		if (house)
		{
			vector ho = house.GetOrigin();
			int holes;
			for (int dz = -1; dz <= 1; dz++)
			{
				for (int dx = -1; dx <= 1; dx++)
				{
					if (GetWorld().GetSurfaceY(ho[0] + dx * 2, ho[2] + dz * 2) < -255)
						holes++;
				}
			}
			SCR_DamageManagerComponent dm = SCR_DamageManagerComponent.Cast(house.FindComponent(SCR_DamageManagerComponent));
			if (dm)
				dm.Kill(Instigator.CreateInstigator(this));
			Print(string.Format("MUD risk: house %1 at %2 killed %3, holes in its 3 x 3 cells %4 / 9", house.GetPrefabData().GetPrefabName(), ho, dm != null, holes));
			Watch(ho, "house");
		}
		else
		{
			Print("MUD risk: no SCR_DestructibleBuildingEntity within 30 m of m_vHouse", LogLevel.WARNING);
		}

		// R5: grenades for the user
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
		{
			Print("MUD risk: no player - grenades not dropped", LogLevel.WARNING);
			return;
		}
		for (int k = 0; k < 3; k++)
			Spawn(m_Grenade, player.GetOrigin() + Vector(0.6 + k * 0.3, 0.3, 0.6));
		Print("MUD risk: 3 x M67 at the player's feet - throw one onto a hole cell");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnExplosion(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)
	{
		vector p = outMat[0];
		string hit = "none";
		if (pHitEntity)
			hit = pHitEntity.ClassName();
		Print(string.Format("MUD risk explosion at %1: hole %2, GetSurfaceY %3, heightmap %4, hit %5",
			p, GetWorld().GetSurfaceY(p[0], p[2]) < -255, GetWorld().GetSurfaceY(p[0], p[2]), HeightmapY(p[0], p[2]), hit));
		Baseline(p);
		Watch(p, "explosion");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckWatch(int i, float after)
	{
		Query(m_aWatchAt[i]);
		int fresh;
		int sunk;
		array<string> lines = {};
		foreach (IEntity e : m_aFound)
		{
			if (m_sBaseline.Contains(e))
				continue;
			fresh++;
			vector o = e.GetOrigin();
			float dy = o[1] - HeightmapY(o[0], o[2]);
			if (dy >= -1)
				continue;
			sunk++;
			if (lines.Count() < 10)
				lines.Insert(string.Format("MUD risk sunk:   %1 at %2 dy %3 hole %4", e.ClassName(), o, dy, GetWorld().GetSurfaceY(o[0], o[2]) < -255));
		}
		Print(string.Format("MUD risk watch '%1' +%2 s: new roots %3, more than 1 m under the heightmap %4", m_aWatchName[i], after, fresh, sunk));
		foreach (string l : lines)
			Print(l);
	}
}
```

- [ ] **Step 7: Place the probe (world closed)**

Ask the user to close the Arland world in Workbench and say so. Then append to `ADDON/Arlnd_Layers/default.layer`:

```
MUD_RiskProbeEntity {
 coords 1250 40 2190
}
```

Ask the user to compile `MUD_RiskProbe.c` (Ctrl+S / F7) and reopen the world.

- [ ] **Step 8: Play and read the probe**

Ask the user to start play (`wb_play`) and focus the game window. In play the user: (a) waits 40 s; (b) throws one M67 onto open ground (any field cell is a hole); (c) walks to the M923A1 at 1246 / 2175 and gets in and out once, from the ground beside it; (d) uses the Workbench debug free camera to look at a road edge from about 300 m up (any road crossing fields within 1 km), saves a screenshot to `ADDON\docs\superpowers\results\<run date>-plan3-task1-300m.png`, and says whether a gap / light line along the road edge is visible.

Expected log lines (newest `console.log`):

```
MUD risk: hole point <x y z> GetSurfaceY -256 heightmap <h> | SCR_TerrainHelper.GetTerrainY <v> | control <x y z> GetSurfaceY <h2>
MUD risk: group on hole 1, group on control 1
MUD risk: soldier spawned 1 at <...>
MUD risk: house <prefab> at <...> killed 1, holes in its 3 x 3 cells <n> / 9
MUD risk: 3 x M67 at the player's feet - throw one onto a hole cell
MUD risk watch 'hole point' +3 s: new roots <n>, more than 1 m under the heightmap <s>
... (+10, +30 s; also 'control point', 'house', 'explosion')
MUD risk explosion at <...>: hole 1, GetSurfaceY -256, heightmap <h>, hit <class>
```

Also count in the same `console.log`: lines containing `TERRAIN (E)` and `index buffer` (carried item 6), and lines `MUD vehicle access`.

- [ ] **Step 9: Results file**

Write `ADDON/docs/superpowers/results/<run date>-plan3-task1-risks.md` with the log folder name, time, build, and this table (one row each, every number from the log; sunk = the +30 s line unless an earlier line is larger, then both):

| Row | Risk | Evidence line(s) | Number | PASS / FAIL |
|---|---|---|---|---|
| R1 | AI group on a hole point | watch 'hole point' | new / sunk | FAIL if any `Character` sunk |
| R2 | AI group on a kept point (control) | watch 'control point' | new / sunk | control must be 0 sunk, else the probe is wrong: stop |
| R3 | runtime soldier on a hole cell | same watch, sunk line class `SCR_ChimeraCharacter` at sp | | |
| R4 | destroyed house: debris / sink | watch 'house' | new / sunk + classes | FAIL if any sunk |
| R5 | explosion on a hole cell | explosion line + watch 'explosion' | sunk | FAIL if any sunk |
| R6 | M923 get in / out from the skin | user + `MUD vehicle access` lines | works yes/no | FAIL if "SeatObstructed" or no get in |
| R7 | cracks at hole edges from 300 m | user + screenshot | seen yes/no | FAIL = seen |
| R8 | `TERRAIN (E)` index buffer warnings | console.log count | n | info only |
| R9 | shipped callers + RELATIVE_Y prefabs | callers file | calls / files / relY prefabs | FAIL if relY prefabs > 0 |
| R10 | `SCR_TerrainHelper.GetTerrainY` on a hole point | first risk line | value | -256 = the helper returns the hole floor |

Add the checksum line: "rows 10; PASS p + FAIL f + info i = 10". Ledger items 4 (90663 road hits) and 6 (+85 MB spread build) get one line each: "not pursued: spread build is not the default (Plan 2 ruling)". Append every FAIL row to the LEDGER.

- [ ] **Step 10: Task gate**

Task 1 is done when the results file has all 10 rows with numbers and R2 is 0 sunk. No adversarial review (probe code is TEMP and deleted in Task 2).

---

### Task 2: Fix what Task 1 saw fail

**Files:**
- Create: `ADDON/Scripts/Game/Mud/MUD_Surface.c` (always: later tasks use it)
- Create: `ADDON/Scripts/Game/Mud/MUD_HoleFixes.c` (only the fixes whose rows FAILED)
- Delete: `ADDON/Scripts/Game/Mud/MUD_RiskProbe.c` and its layer entry (step 7, world closed)
- Modify: the Task 1 results file (re-run rows)

**Interfaces:**
- Consumes: Task 1 rows R1, R4, R5, R10.
- Produces: `MUD_Surface.Get()` with `static bool IsHole(notnull BaseWorld world, float x, float z)`, `float HeightmapY(notnull BaseWorld world, float x, float z)`, `float Y(notnull BaseWorld world, float x, float z)`, `static bool IsOurs(IEntity e)` (Task 4 adds the fine patch class to `IsOurs`). Used by Tasks 4-8.

Decision table (from the Task 1 results; write the chosen line into the LEDGER before coding):

| Fix | Apply when | What |
|---|---|---|
| F1 | R4 or R5 FAIL with sunk debris / effect entities, and R10 = -256 | `SCR_TerrainHelper.GetTerrainY` returns our ground on a hole cell |
| F2 | R1 FAIL (group characters sunk on the hole point) | `SCR_AIGroup.SpawnGroupMember` lifts members over holes |
| - | R6 FAIL | handled in Task 4 step 9 (vehicle access + hiding), not here |
| - | R7 FAIL | Task 12 |
| - | any sunk class not explained by F1 / F2 (e.g. a sunk class whose shipped code calls `GetSurfaceY` directly) | stop: ledger line with the class, its shipped caller line from the Task 1 callers table, and ask the user before adding a fix |

- [ ] **Step 1: `MUD_Surface.c`**

Create `ADDON/Scripts/Game/Mud/MUD_Surface.c`:

```c
//! Heights on the holed map (mud plan 3). GetSurfaceY is -256 on a hole cell; nothing here returns that.
class MUD_Surface
{
	protected static ref MUD_Surface s_Inst;
	protected ref TraceParam m_Trace = new TraceParam();

	//------------------------------------------------------------------------------------------------
	static MUD_Surface Get()
	{
		if (!s_Inst)
			s_Inst = new MUD_Surface();
		return s_Inst;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsHole(notnull BaseWorld world, float x, float z)
	{
		return world.GetSurfaceY(x, z) < -255;
	}

	//------------------------------------------------------------------------------------------------
	//! Our ground entities: skin chunks (and fine patches, Task 4)
	static bool IsOurs(IEntity e)
	{
		return MUD_CoarseChunkEntity.Cast(e) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Heightmap height: WORLD trace (hits the heightmap inside holes too), terrain only, retraced from
	//! 1 cm under a road (the filter does not stop WORLD road geometry, plan 2). GetSurfaceY if nothing hit.
	float HeightmapY(notnull BaseWorld world, float x, float z)
	{
		m_Trace.Start = Vector(x, 400, z);
		m_Trace.End = Vector(x, -300, z);
		m_Trace.Flags = TraceFlags.WORLD;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.TraceEnt = null;
		float frac = world.TraceMove(m_Trace, TerrainOnly);
		if (frac >= 1)
			return world.GetSurfaceY(x, z);
		float y = 400 - 700 * frac;
		for (int r = 0; r < 4 && m_Trace.TraceEnt && !GenericTerrainEntity.Cast(m_Trace.TraceEnt); r++)
		{
			m_Trace.Start = Vector(x, y - 0.01, z);
			m_Trace.TraceEnt = null;
			frac = world.TraceMove(m_Trace, TerrainOnly);
			if (frac >= 1)
				break;
			y = m_Trace.Start[1] + (-300 - m_Trace.Start[1]) * frac;
		}
		return y;
	}

	//------------------------------------------------------------------------------------------------
	//! Top of our ground at x, z (skin chunk or fine patch, by ENTS | WORLD trace); the heightmap height
	//! where the first hit is something else (kept terrain, a road, a WORLD static)
	float Y(notnull BaseWorld world, float x, float z)
	{
		m_Trace.Start = Vector(x, 400, z);
		m_Trace.End = Vector(x, -300, z);
		m_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
		m_Trace.TraceEnt = null;
		float frac = world.TraceMove(m_Trace, OurGround);
		if (frac < 1 && IsOurs(m_Trace.TraceEnt))
			return 400 - 700 * frac;
		return HeightmapY(world, x, z);
	}

	//------------------------------------------------------------------------------------------------
	protected bool OurGround(notnull IEntity e)
	{
		return IsOurs(e);
	}

	//------------------------------------------------------------------------------------------------
	protected bool TerrainOnly(notnull IEntity e)
	{
		return GenericTerrainEntity.Cast(e) != null;
	}
}
```

- [ ] **Step 2: F1 (only if the table picked it)**

Create (or extend) `ADDON/Scripts/Game/Mud/MUD_HoleFixes.c`:

```c
//! Shipped code that reads the terrain height through SCR_TerrainHelper gets -256 on a hole cell
//! (GetSurfaceY fallback, pak scripts/Game/Helpers/SCR_TerrainHelper.c:12). Plan 3 task 1 row R4 / R5.
modded class SCR_TerrainHelper
{
	//------------------------------------------------------------------------------------------------
	override static float GetTerrainY(vector pos, BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null)
	{
		BaseWorld w = world;
		if (!w)
			w = GetGame().GetWorld();
		if (!w || !MUD_Surface.IsHole(w, pos[0], pos[2]))
			return super.GetTerrainY(pos, world, noUnderwater, trace);

		float y = MUD_Surface.Get().Y(w, pos[0], pos[2]);
		if (trace)
		{
			// the caller's own trace may stop on something above our ground (a roof); it falls back to
			// GetSurfaceY = -256 when it hits nothing
			float t = super.GetTerrainY(pos, world, false, trace);
			if (t > y)
				y = t;
		}
		if (noUnderwater && y < 0)
			y = 0;
		return y;
	}
}
```

Ask the user to compile. If the compiler refuses `override static` or `super` in a static method, delete that class and use this instead, which covers the building debris path (`SCR_TimedEffect.SnapToTerrain`, pak `scripts/Game/Destruction/Building/SCR_DestructibleBuildingComponent.c:336`):

```c
//! Plan 3 F1 (fallback): building debris snaps to SCR_TerrainHelper.GetTerrainY = -256 on a hole cell
modded class SCR_TimedEffect
{
	//------------------------------------------------------------------------------------------------
	override void SnapToTerrain(inout vector origin, IEntity owner)
	{
		super.SnapToTerrain(origin, owner);
		if (origin[1] > -255 || !owner)
			return;
		origin[1] = MUD_Surface.Get().Y(owner.GetWorld(), origin[0], origin[2]);
	}
}
```

Record which variant compiled in the LEDGER. With the fallback, any R5 (explosion) sunk class needs its own line in the decision table's last row.

- [ ] **Step 3: F2 (only if the table picked it)**

Append to `ADDON/Scripts/Game/Mud/MUD_HoleFixes.c`:

```c
//! AI group members snap to GetSurfaceY = -256 on a hole cell, out of reach of the navmesh snap's
//! 10 m box (pak scripts/Game/Entities/SCR_AIGroup.c:1658). Plan 3 task 1 row R1.
modded class SCR_AIGroup
{
	//------------------------------------------------------------------------------------------------
	override protected bool SpawnGroupMember(bool snapToTerrain, int index, ResourceName res, bool editMode, bool isLast)
	{
		if (!snapToTerrain || editMode)
			return super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);

		// same member position as the shipped code
		AIFormationComponent fc = AIFormationComponent.Cast(FindComponent(AIFormationComponent));
		AIFormationDefinition def;
		if (fc)
			def = fc.GetFormation();
		vector pos;
		if (def)
			pos = CoordToParent(def.GetOffsetPosition(index));
		else
			pos = CoordToParent(Vector(index, 0, 0));

		BaseWorld world = GetWorld();
		if (!MUD_Surface.IsHole(world, pos[0], pos[2]))
			return super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);

		// lift the group to our ground for this one member and spawn without the snap; the shipped navmesh
		// snap then corrects the height within its 10 m box
		vector o = GetOrigin();
		SetOrigin(Vector(o[0], MUD_Surface.Get().Y(world, pos[0], pos[2]), o[2]));
		bool ok = super.SpawnGroupMember(false, index, res, editMode, isLast);
		SetOrigin(o);
		return ok;
	}
}
```

- [ ] **Step 4: Compile**

Ask the user to compile `MUD_Surface.c` and `MUD_HoleFixes.c`. Expected: no script errors in the console.

- [ ] **Step 5: Re-run the probe**

Same play protocol as Task 1 step 8 (a)-(b) only. Expected: every row that was FAIL and has a fix now shows `more than 1 m under the heightmap 0` on all its watch lines; R2 still 0. Add a column "after Task 2" to the results table with the new numbers, and a new checksum line.

- [ ] **Step 6: Adversarial review**

Run `engineering-skills:adversarial-reviewer` on `MUD_Surface.c` and `MUD_HoleFixes.c` (modded shipped classes = risky). Rule on every finding in the LEDGER.

- [ ] **Step 7: Remove the probe (world closed)**

Ask the user to close the Arland world. Remove the `MUD_RiskProbeEntity { ... }` block from `ADDON/Arlnd_Layers/default.layer`, delete `ADDON/Scripts/Game/Mud/MUD_RiskProbe.c`. Ask the user to reopen the world and compile (the deleted class must leave no error). LEDGER line: "probe removed".

---

### Task 3: Skin keeps its chunks; cells can be covered

**Files:**
- Modify: `ADDON/Scripts/Game/Mud/MUD_CoarseSkin.c` (current: 659 lines; `BuildChunk` at :327-523 is replaced)
- Modify: `ADDON/Scripts/Game/Mud/MUD_Surface.c` (nothing yet; `IsOurs` unchanged)
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task3-cover.md`

**Interfaces:**
- Consumes: Plan 2 skin (`HeightAt`, `TerrainOnly`, `MakeStatic`, `Check`, `SkinOnly`, buffers).
- Produces (Tasks 4-12 rely on these exact names):
  - `class MUD_SkinChunk` (`m_aH[1089]` heights incl. offset, `m_aCell[1024]` packed, `m_aMats`, `m_vOrigin`, `m_Ent`, `m_MeshRes`, `m_Phys`, `m_iCovered`)
  - `static MUD_CoarseSkinEntity MUD_CoarseSkinEntity.GetInstance()`
  - `bool IsReady()`, `float GetCell()`, `void GetGrid(out float x0, out float z0, out int ncx, out int ncz)`
  - `bool IsHoleCell(int cx, int cz)` (global cell indices from the terrain corner; false outside the map)
  - `bool CellShape(int cx, int cz, out float h00, out float h10, out float h01, out float h11, out bool tlbr, out ResourceName mat)` - h00 = vertex (cx, cz), h10 = (cx+1, cz), h01 = (cx, cz+1), h11 = (cx+1, cz+1); false when not a hole
  - `int SetCovered(int cx0, int cz0, int n, bool covered)` - n x n cells, rebuilds every touched chunk at once, returns cells changed
  - `static void SetTraceHidden(bool hide)` - TRACEABLE off / on for every chunk (Task 4 step 9 uses it only if R6 FAILED)
  - constants `CELL_SLOT = 15`, `CELL_TLBR = 16`, `CELL_COVER = 32`

- [ ] **Step 1: Record class**

In `MUD_CoarseSkin.c`, after the `MUD_CoarseSkinBuffers` class (ends at :23), insert:

```c
//! One skin chunk with holes, kept after the build for the fine patches (plan 3): ~8.4 KB each.
class MUD_SkinChunk
{
	float m_aH[1089];           // vertex heights incl. m_fOffset, 33 x 33, bl = z * 33 + x
	int m_aCell[1024];          // -1 = not a hole, else slot | CELL_TLBR | CELL_COVER
	ref array<ResourceName> m_aMats = {};
	vector m_vOrigin;
	IEntity m_Ent;
	ref Resource m_MeshRes;
	Physics m_Phys;
	int m_iCovered;
}
```

- [ ] **Step 2: Members, Start, destructor**

In `MUD_CoarseSkinEntity`:

1. After `static const int MAT_SLOTS = 6;` add:

```c
	static const int CELL_SLOT = 15;    // m_aCell bits 0-3: collider slot (index into m_aMats)
	static const int CELL_TLBR = 16;    // bit 4: shared edge tl-br (else bl-tr)
	static const int CELL_COVER = 32;   // bit 5: a fine patch covers the cell (plan 3)
```

2. After the `m_iCheckCells` attribute add:

```c
	// TEMP plan 3 task 3: removed in task 3 step 7
	[Attribute("0 0 0", UIWidgets.Coords, "TEMP: 6 s after the build, cover the 4 x 4 cells at this XZ, trace, uncover, trace (0 0 0 = off)", category: "Check")]
	protected vector m_vTestCover;
	protected int m_iCoverStage;
```

3. Replace the members `protected ref array<ref Resource> m_aMeshRes = {};` and `protected ref array<ResourceName> m_aChunkMats = {};` with:

```c
	protected ref array<ref MUD_SkinChunk> m_aRec = {};   // per chunk (z * m_iChunksX + x), null = no holes
	protected int m_iChunksX;
	protected static MUD_CoarseSkinEntity s_Inst;
```

4. In the destructor, before the `foreach`, add:

```c
		if (s_Inst == this)
			s_Inst = null;
```

5. In `Start()`, after `int nz = Math.Ceil(m_iNCZ / 32.0);` add:

```c
		m_iChunksX = nx;
		m_aRec.Clear();
		m_aRec.Resize(nx * nz);     // all null
		s_Inst = this;
```

- [ ] **Step 3: Replace `BuildChunk` by sample + geometry**

Replace the whole `BuildChunk(vector o)` method (`MUD_CoarseSkin.c:327-523`) with:

```c
	//------------------------------------------------------------------------------------------------
	protected void BuildChunk(vector o)
	{
		MUD_SkinChunk rec = SampleChunk(o);
		if (!rec)
			return;
		int ix = Math.Round((o[0] - m_fX0) / m_fChunk);
		int iz = Math.Round((o[2] - m_fZ0) / m_fChunk);
		m_aRec[iz * m_iChunksX + ix] = rec;
		BuildGeometry(rec);
	}

	//------------------------------------------------------------------------------------------------
	//! Heights, hole cells, diagonals and game materials of the chunk at world XZ o; null = no holes.
	//! Same traces in the same order as plan 2 (the miss fallback m_fLastGood depends on the order).
	protected MUD_SkinChunk SampleChunk(vector o)
	{
		BaseWorld world = GetWorld();
		int t0 = System.GetTickCount();
		MUD_SkinChunk rec = new MUD_SkinChunk();
		rec.m_vOrigin = o;

		int holes = 0;
		// a map whose cell count is not a multiple of 32 has a last chunk reaching past the terrain, where
		// GetSurfaceY is -256 too (no terrain answers): those cells are never holes
		int ox = Math.Round((o[0] - m_fX0) / m_fCell);
		int oz = Math.Round((o[2] - m_fZ0) / m_fCell);
		for (int hz = 0; hz < CELLS; hz++)
		{
			for (int hx = 0; hx < CELLS; hx++)
			{
				int hc = hz * CELLS + hx;
				rec.m_aCell[hc] = -1;
				if (ox + hx >= m_iNCX || oz + hz >= m_iNCZ)
					continue;
				if (world.GetSurfaceY(o[0] + (hx + 0.5) * m_fCell, o[2] + (hz + 0.5) * m_fCell) < -255)
				{
					rec.m_aCell[hc] = 0;
					holes++;
				}
			}
		}
		if (holes == 0)
		{
			m_iEmpty++;
			m_iHeightMs += System.GetTickCount(t0);
			return null;
		}

		for (int vz = 0; vz < VERTS; vz++)
		{
			for (int vx = 0; vx < VERTS; vx++)
				rec.m_aH[vz * VERTS + vx] = HeightAt(o[0] + vx * m_fCell, o[2] + vz * m_fCell) + m_fOffset;
		}

		// per hole cell: diagonal whose midpoint is nearest the traced centre, and the centre's gamemat
		for (int z = 0; z < CELLS; z++)
		{
			for (int x = 0; x < CELLS; x++)
			{
				int c = z * CELLS + x;
				if (rec.m_aCell[c] < 0)
					continue;

				float hm = HeightAt(o[0] + (x + 0.5) * m_fCell, o[2] + (z + 0.5) * m_fCell) + m_fOffset;
				ResourceName mat;
				if (m_bLastHit)
				{
					GameMaterial gm = m_Trace.SurfaceProps;
					if (gm)
						mat = gm.GetResourceName();
				}
				if (mat.IsEmpty())
					mat = m_GameMaterial;
				int slot = rec.m_aMats.Find(mat);
				if (slot < 0)
				{
					if (rec.m_aMats.Count() < MAT_SLOTS)
					{
						slot = rec.m_aMats.Count();
						rec.m_aMats.Insert(mat);
					}
					else
					{
						slot = 0;   // more than 6 surfaces in one chunk: the rest join slot 0
						m_iJoined++;
					}
				}

				int bl = z * VERTS + x;
				float dA = Math.AbsFloat((rec.m_aH[bl] + rec.m_aH[bl + VERTS + 1]) * 0.5 - hm);
				float dB = Math.AbsFloat((rec.m_aH[bl + 1] + rec.m_aH[bl + VERTS]) * 0.5 - hm);
				int cell = slot;
				if (dB < dA - 0.001)
					cell = cell | CELL_TLBR;    // shared edge tl-br (99.93 % of non-flat Arland cells, 2026-10-08)
				rec.m_aCell[c] = cell;
			}
		}
		m_iHoleCells += holes;
		m_iHeightMs += System.GetTickCount(t0);
		return rec;
	}

	//------------------------------------------------------------------------------------------------
	//! Mesh and collider of one chunk: its hole cells that no fine patch covers. Spawns the chunk entity
	//! on first use; on a rebuild the old body is destroyed and the new one made in this same call.
	protected void BuildGeometry(MUD_SkinChunk rec)
	{
		if (!rec.m_Ent)
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = rec.m_vOrigin;
			rec.m_Ent = GetGame().SpawnEntity(MUD_CoarseChunkEntity, GetWorld(), params);
			if (!rec.m_Ent)
			{
				Print("MUD coarse: could not spawn chunk entity", LogLevel.ERROR);
				return;
			}
			m_aChunks.Insert(rec.m_Ent);
		}

		for (int vz = 0; vz < VERTS; vz++)
		{
			for (int vx = 0; vx < VERTS; vx++)
			{
				int v = vz * VERTS + vx;
				m_Buf.m_aVerts[v] = Vector(vx * m_fCell, rec.m_aH[v], vz * m_fCell);
			}
		}

		int n;
		for (int z = 0; z < CELLS; z++)
		{
			for (int x = 0; x < CELLS; x++)
			{
				int c = z * CELLS + x;
				int bl = z * VERTS + x;
				int br = bl + 1;
				int tl = bl + VERTS;
				int tr = tl + 1;
				int cell = rec.m_aCell[c];
				if (cell < 0 || (cell & CELL_COVER) != 0)
				{
					// kept terrain or a fine patch here: zero-area cell keeps the index count fixed
					for (int j = 0; j < 6; j++)
					{
						m_Buf.m_aIndices[n] = bl;
						n++;
					}
					m_Buf.m_aCellSlot[c] = -1;
					continue;
				}
				m_Buf.m_aCellSlot[c] = cell & CELL_SLOT;
				if ((cell & CELL_TLBR) != 0)
				{
					m_Buf.m_aIndices[n] = bl; n++;
					m_Buf.m_aIndices[n] = tl; n++;
					m_Buf.m_aIndices[n] = br; n++;
					m_Buf.m_aIndices[n] = tl; n++;
					m_Buf.m_aIndices[n] = tr; n++;
					m_Buf.m_aIndices[n] = br; n++;
				}
				else
				{
					// shared edge bl-tr (also flat cells: both diagonals are the same there)
					m_Buf.m_aIndices[n] = bl; n++;
					m_Buf.m_aIndices[n] = tl; n++;
					m_Buf.m_aIndices[n] = tr; n++;
					m_Buf.m_aIndices[n] = tr; n++;
					m_Buf.m_aIndices[n] = br; n++;
					m_Buf.m_aIndices[n] = bl; n++;
				}
			}
		}

		if (m_bRender)
		{
			int t1 = System.GetTickCount();
			int numVerts[] = {1089};
			int numIndices[] = {6144};
			string materials[] = {m_Material};
			Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
			if (res && res.GetResource())
			{
				MeshObject mesh = res.GetResource().ToMeshObject();
				// indices first: UpdateVerts rebuilds normals + tangents from the triangle list
				mesh.UpdateIndices(0, m_Buf.m_aIndices);
				mesh.UpdateVerts(0, m_Buf.m_aVerts, m_Buf.m_aUVs);
				rec.m_Ent.SetObject(mesh, "");
				rec.m_MeshRes = res;
			}
			else
			{
				Print(string.Format("MUD coarse: MeshObject.Create failed - material '%1' did not load?", m_Material), LogLevel.ERROR);
				m_bRender = false;
			}
			m_iMeshMs += System.GetTickCount(t1);
		}

		int t2 = System.GetTickCount();
		array<PhysicsGeom> geoms = {};
		array<ResourceName> mats = {};
		array<int> layers = {};
		// same layers as the ground patches: Terrain preset + Foliage (vehicle get in/out sweep skips it)
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;
		for (int k = 0; k < rec.m_aMats.Count(); k++)
		{
			int sn = 0;
			for (int sc = 0; sc < CELLS * CELLS; sc++)
			{
				if (m_Buf.m_aCellSlot[sc] != k)
					continue;
				for (int sj = 0; sj < 6; sj++)
				{
					m_Buf.m_aSplitIdx[sn] = m_Buf.m_aIndices[sc * 6 + sj];
					sn++;
				}
			}
			if (sn == 0)
				continue;
			PhysicsGeom geom = PhysicsGeom.CreateTriMesh(m_Buf.m_aVerts, m_Buf.m_aSplitIdx, 1089, sn);
			if (!geom)
			{
				Print("MUD coarse: CreateTriMesh failed", LogLevel.ERROR);
				continue;
			}
			geoms.Insert(geom);
			mats.Insert(rec.m_aMats[k]);
			layers.Insert(layer);
		}
		if (rec.m_Phys)
		{
			rec.m_Phys.Destroy();
			rec.m_Phys = null;
		}
		if (!geoms.IsEmpty())
		{
			rec.m_Phys = MakeStatic(rec.m_Ent, geoms, mats, layers);
			if (!rec.m_Phys)
				Print("MUD coarse: CreateStaticEx failed", LogLevel.ERROR);
		}
		// the chunk got its mesh after it was spawned: Update() refreshes its bounds in the scene, else the renderer
		// keeps the empty spawn-time bounds and culls the chunk when its corner is off screen (user 2026-10-09 00:17;
		// shipped SCR_DestructionBaseHandler does SetObject then Update)
		rec.m_Ent.Update();
		m_iColMs += System.GetTickCount(t2);
	}

	//------------------------------------------------------------------------------------------------
	static MUD_CoarseSkinEntity GetInstance()
	{
		return s_Inst;
	}

	//------------------------------------------------------------------------------------------------
	bool IsReady()
	{
		return m_bDone;
	}

	//------------------------------------------------------------------------------------------------
	float GetCell()
	{
		return m_fCell;
	}

	//------------------------------------------------------------------------------------------------
	void GetGrid(out float x0, out float z0, out int ncx, out int ncz)
	{
		x0 = m_fX0;
		z0 = m_fZ0;
		ncx = m_iNCX;
		ncz = m_iNCZ;
	}

	//------------------------------------------------------------------------------------------------
	//! Record holding global cell (cx, cz) and the cell's index in it; null outside the map or no holes
	protected MUD_SkinChunk RecOf(int cx, int cz, out int c)
	{
		if (cx < 0 || cz < 0 || cx >= m_iNCX || cz >= m_iNCZ)
			return null;
		int ix = cx / CELLS;
		int iz = cz / CELLS;
		c = (cz - iz * CELLS) * CELLS + cx - ix * CELLS;
		return m_aRec[iz * m_iChunksX + ix];
	}

	//------------------------------------------------------------------------------------------------
	bool IsHoleCell(int cx, int cz)
	{
		int c;
		MUD_SkinChunk rec = RecOf(cx, cz, c);
		return rec && rec.m_aCell[c] >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Corner heights (incl. offset), diagonal and game material of hole cell (cx, cz); false if not a hole
	bool CellShape(int cx, int cz, out float h00, out float h10, out float h01, out float h11, out bool tlbr, out ResourceName mat)
	{
		int c;
		MUD_SkinChunk rec = RecOf(cx, cz, c);
		if (!rec || rec.m_aCell[c] < 0)
			return false;
		int lx = c % CELLS;
		int lz = c / CELLS;
		int bl = lz * VERTS + lx;
		h00 = rec.m_aH[bl];
		h10 = rec.m_aH[bl + 1];
		h01 = rec.m_aH[bl + VERTS];
		h11 = rec.m_aH[bl + VERTS + 1];
		tlbr = (rec.m_aCell[c] & CELL_TLBR) != 0;
		mat = rec.m_aMats[rec.m_aCell[c] & CELL_SLOT];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Cover (a fine patch took over) or uncover n x n hole cells from (cx0, cz0); rebuilds every chunk
	//! touched, now. Returns the number of cells that changed.
	int SetCovered(int cx0, int cz0, int n, bool covered)
	{
		array<MUD_SkinChunk> touched = {};
		int changed;
		for (int cz = cz0; cz < cz0 + n; cz++)
		{
			for (int cx = cx0; cx < cx0 + n; cx++)
			{
				int c;
				MUD_SkinChunk rec = RecOf(cx, cz, c);
				if (!rec || rec.m_aCell[c] < 0)
					continue;
				bool was = (rec.m_aCell[c] & CELL_COVER) != 0;
				if (was == covered)
					continue;
				if (covered)
				{
					rec.m_aCell[c] = rec.m_aCell[c] | CELL_COVER;
					rec.m_iCovered++;
				}
				else
				{
					rec.m_aCell[c] = rec.m_aCell[c] & ~CELL_COVER;
					rec.m_iCovered--;
				}
				changed++;
				if (!touched.Contains(rec))
					touched.Insert(rec);
			}
		}
		foreach (MUD_SkinChunk t : touched)
			BuildGeometry(t);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! TRACEABLE off (hide) or back on for every chunk (vehicle get in/out, only if plan 3 task 1 R6 failed)
	static void SetTraceHidden(bool hide)
	{
		if (!s_Inst)
			return;
		foreach (IEntity e : s_Inst.m_aChunks)
		{
			if (!e)
				continue;
			if (hide)
				e.ClearFlags(EntityFlags.TRACEABLE, false);
			else
				e.SetFlags(EntityFlags.TRACEABLE, false);
		}
	}
```

Notes for the implementer: `Finish()` and `Check()` stay as they are. Remove any remaining use of `m_aChunkMats` / `m_aMeshRes` (there are none outside the old `BuildChunk`). `CELLS` / `VERTS` are the existing class constants.

- [ ] **Step 4: TEMP cover test**

Add to `MUD_CoarseSkinEntity`:

```c
	//------------------------------------------------------------------------------------------------
	//! TEMP plan 3 task 3: skin hits at the centres of n x n cells from (cx, cz), by ENTS | WORLD trace
	protected int TraceCells(int cx, int cz, int n, out int holes)
	{
		TraceParam p = new TraceParam();
		int hits;
		holes = 0;
		for (int z = cz; z < cz + n; z++)
		{
			for (int x = cx; x < cx + n; x++)
			{
				if (!IsHoleCell(x, z))
					continue;
				holes++;
				float wx = m_fX0 + (x + 0.5) * m_fCell;
				float wz = m_fZ0 + (z + 0.5) * m_fCell;
				p.Start = Vector(wx, m_fTop, wz);
				p.End = Vector(wx, m_fBottom, wz);
				p.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
				p.LayerMask = EPhysicsLayerPresets.Projectile;
				p.TraceEnt = null;
				float frac = GetWorld().TraceMove(p, SkinOnly);
				if (frac < 1 && MUD_CoarseChunkEntity.Cast(p.TraceEnt))
					hits++;
			}
		}
		return hits;
	}

	//------------------------------------------------------------------------------------------------
	//! TEMP plan 3 task 3: stage 1 cover, stage 2 trace + uncover, stage 3 trace (one stage per second)
	protected void CoverTest()
	{
		int cx = Math.Floor((m_vTestCover[0] - m_fX0) / m_fCell);
		int cz = Math.Floor((m_vTestCover[2] - m_fZ0) / m_fCell);
		cx = cx - cx % 4;   // a patch's cells: 4-aligned
		cz = cz - cz % 4;
		int holes;
		m_iCoverStage++;
		if (m_iCoverStage == 1)
		{
			int t0 = System.GetTickCount();
			int changed = SetCovered(cx, cz, 4, true);
			Print(string.Format("MUD coarse cover test: cells %1 %2, covered %3, rebuild %4 ms", cx, cz, changed, System.GetTickCount(t0)));
		}
		else if (m_iCoverStage == 2)
		{
			int hitCovered = TraceCells(cx, cz, 4, holes);
			int t1 = System.GetTickCount();
			int back = SetCovered(cx, cz, 4, false);
			Print(string.Format("MUD coarse cover test: hole cells %1, skin hits while covered %2 (want 0), uncovered %3, rebuild %4 ms", holes, hitCovered, back, System.GetTickCount(t1)));
		}
		else if (m_iCoverStage == 3)
		{
			int hitBack = TraceCells(cx, cz, 4, holes);
			Print(string.Format("MUD coarse cover test: skin hits after uncover %1 (want %2)", hitBack, holes));
		}
	}
```

In `EOnFrame`, inside the `if (m_bDone)` block, before its `return;`, add:

```c
			// TEMP plan 3 task 3
			if (m_vTestCover != vector.Zero && m_iCoverStage < 3 && m_fSinceDone >= 6 + m_iCoverStage)
				CoverTest();
```

- [ ] **Step 5: Compile and set the test point**

Ask the user to compile `MUD_CoarseSkin.c`, then (world open, through the entity's properties in the World Editor, saved by Workbench) set `m_vTestCover` on the `MUD_CoarseSkinEntity` to `1250 0 2190` (a field cell next to the M923; Task 1's hole point is also fine). If the user prefers a file edit, close the world first and add `m_vTestCover 1250 0 2190` to the entity block in `default.layer`.

- [ ] **Step 6: Play and compare with Plan 2**

Ask the user to start play and wait 12 s. Expected lines (Plan 2 values from `docs/superpowers/results/2026-10-09-plan2-task3-skin.md` in brackets; they must be equal except time and memory):

```
MUD coarse: built 1697 chunks with holes + 2399 without = 4096 | hole cells 1340627, tris 2681254 | via init      [same]
MUD coarse: work <ms> (heights ..., meshes ..., colliders ...) ...                                                [~5230 ms]
MUD coarse: trace misses 0, cells joined to slot 0 (> 6 surfaces) <j>, mem +<KB> KB                             [+263778 KB; expect about +14 MB more]
MUD coarse check (build): 2000 hole cells, skin hit 2000, within 5 cm 2000, worst <= 0.0005 m
MUD coarse cover test: cells <cx> <cz>, covered <k>, rebuild <ms> ms
MUD coarse cover test: hole cells <k>, skin hits while covered 0 (want 0), uncovered <k>, rebuild <ms> ms
MUD coarse cover test: skin hits after uncover <k> (want <k>)
```

If "skin hits while covered" is not 0, the trace saw the old body: log a probe line in the next frame (stage timing already one second apart) and stop; do not continue to Task 4 with a skin that cannot cover. Write the results file: the Plan 2 vs now table (every line above, both values), the memory difference, the two rebuild times (they are the per-patch skin swap cost Task 4 pays).

- [ ] **Step 7: Remove the TEMP test, review**

Remove the TEMP attribute, `m_iCoverStage`, `TraceCells`, `CoverTest` and the EOnFrame lines (and `m_vTestCover` from the layer if written there, world closed). Ask the user to compile. Run `engineering-skills:adversarial-reviewer` on `MUD_CoarseSkin.c`; rule on every finding in the LEDGER.

---

### Task 4: Fine patches over the skin (no deformation yet)

**Files:**
- Create: `ADDON/Scripts/Game/Mud/MUD_FinePatch.c`
- Create: `ADDON/Scripts/Game/Mud/MUD_Wheels.c`
- Create: `ADDON/Scripts/Game/Mud/MUD_WorldGround.c`
- Modify: `ADDON/Scripts/Game/Mud/MUD_Surface.c` (`IsOurs`)
- Modify: `ADDON/Scripts/Game/Mud/MUD_BuildingDestructionFix.c` (whole file is 14 lines)
- Modify: `ADDON/Scripts/Game/Mud/MUD_GroundEntity.c:339-377` (`HideFromTraces`) and add `ResetTraceHide`
- Modify: `ADDON/Scripts/Game/Mud/MUD_VehicleAccess.c:80-89` (`OnMud`)
- Modify: `ADDON/Arlnd_Layers/default.layer` (world closed)
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task4-patches.md`

**Interfaces:**
- Consumes: Task 3 skin API (`GetInstance`, `IsReady`, `GetCell`, `GetGrid`, `IsHoleCell`, `CellShape`, `SetCovered`, `SetTraceHidden`); Task 2 `MUD_Surface.IsHole`, `MUD_Surface.IsOurs`; shipped `Vehicle.EOnInit(IEntity owner)` (pak `scripts/GameCode/Vehicle/Vehicle.c:196`, public `override void`), `VehicleWheeledSimulation`, `Physics.GetVelocity()`, `Physics.GetMass()`.
- Produces (Tasks 5-12 rely on these exact names):
  - `class MUD_FinePatchEntity : GenericEntity`, `class MUD_FinePatch` (members listed in step 1), `class MUD_FineBuffers`
  - `enum MUD_EEdgeMode { LOCKED, DIRT_WALL }`
  - `class MUD_Wheels` with `static array<Vehicle> GetAll()`
  - `MUD_WorldGroundEntity`: constants `PCELLS 32`, `PVERTS 33`, `SKIN 4`, `FINE 8`, `MAT_SLOTS 4`, `TLBR 16`, `SLOT 15`; `static MUD_WorldGroundEntity GetInstance()`, `static void SetTraceHidden(bool hide)`, `bool IsReady()`, `protected bool IsProxy()`, `protected bool HasHole(int px, int pz)`, `protected MUD_FinePatch EnsurePatch(int px, int pz, bool byStamp)`, `protected float BaseAt(float fx, float fz, out bool ok)`, `protected void InitPatch(MUD_FinePatch p)`, `protected void ApplyFloor(MUD_FinePatch p)`, `protected void BeforeCollider(MUD_FinePatch p)`, `protected void SmoothPatch(MUD_FinePatch p)`, `protected void FillIndices(MUD_FinePatch p)`, `protected Physics MakeStatic(IEntity ent, array<PhysicsGeom> g, array<ResourceName> m, array<int> l)`, `protected bool IsHardMat(ResourceName mat)`, members `m_mPatches`, `m_Skin`, `m_Buf`, `m_fTime`, `m_fFine`, `m_fCell`, `m_fX0`, `m_fZ0`, `m_iNPX`, `m_iNPZ`, `m_bRender`, `m_bDebug`, `m_iSkipHeight`, `m_RplComponent`, `protected void EnsureRange(int gx0, int gx1, int gz0, int gz1)`, `protected MUD_FinePatch At(int gx, int gz, out int v)`, `protected float GetH(int gx, int gz)`, `protected void SetH(int gx, int gz, float h)`, `protected float VertexSoft(int gx, int gz)`, `protected bool CellHard(int cx, int cz)`, `protected void ComputeSoft(MUD_FinePatch p)`, `protected void MarkDirty(int gx0, int gx1, int gz0, int gz1)`, `protected int PatchLo(int g, int n)`, `protected int PatchHi(int g, int n)`, `protected bool ToFine(vector wp, out float fx, out float fz)`, `protected void BuildMesh(MUD_FinePatch p)`, `protected void BuildCollider(MUD_FinePatch p)`, `protected void Stats(float timeSlice)`; global fine vertex coords `gx, gz` = 0 .. `m_iNPX * PCELLS`, world X = `m_fX0 + gx * m_fFine`.

- [ ] **Step 1: `MUD_FinePatch.c`**

Create `ADDON/Scripts/Game/Mud/MUD_FinePatch.c`:

```c
//! Mud plan 3: entity of one 8 m fine patch (and of its dirt wall, task 8). Own class so
//! MUD_BuildingDestructionFix keeps it out of a ruin's sink-and-delete query and MUD_Surface.IsOurs knows it.
class MUD_FinePatchEntityClass : GenericEntityClass
{
}

class MUD_FinePatchEntity : GenericEntity
{
}

//! Edge Mode (spec 4.3): what the ground does where a hole meets kept terrain (road, water)
enum MUD_EEdgeMode
{
	LOCKED,         // edge verts never move, digging fades in over Edge Fade
	DIRT_WALL       // dig to the edge, a wall fills the gap (task 8)
}

//! One 8 m piece of deformable ground over 4 x 4 skin cells: 33 x 33 verts of 0.25 m, ~35 KB.
//! World Y of vertex v = m_aBase[v] + m_aH[v]. A vertex on a patch edge is stored in every patch
//! holding it; MUD_WorldGroundEntity.SetH writes all copies, so they never differ.
class MUD_FinePatch
{
	float m_aBase[1089];        // undeformed world Y: the skin triangles (incl. the skin's +1 mm)
	float m_aH[1089];           // deformation (m): < 0 dug, > 0 berm / rim / spoil
	float m_aSmooth[1089];      // m_aH blurred for the collider
	float m_aFloor[1089];       // deepest m_aH allowed (negative): Max Depth or a depth box
	float m_aLock[1089];        // 0 = never moves (kept terrain border, building), 1 = free
	float m_aRutTouch[1089];    // last m_fTime a wheel was on it
	float m_aBermTouch[1089];
	int m_aCell[16];            // per skin cell (4 x 4): -1 = kept terrain, else slot | TLBR
	float m_aCellSoft[16];      // 0 = hard (no ruts, no shovel) .. 1 = soft
	int m_aCellHard[16];        // 1 = hard game material (int: a fresh bool array reads true)
	ref array<ResourceName> m_aMats = {};   // collider surfaces, at most MUD_WorldGroundEntity.MAT_SLOTS
	int m_iPx;
	int m_iPz;
	int m_iKey;                 // m_iPz * m_iNPX + m_iPx
	IEntity m_Ent;
	ref Resource m_MeshRes;
	Physics m_Phys;
	bool m_bMeshDirty;
	bool m_bColDirty;
	bool m_bQueued;             // key is in MUD_WorldGroundEntity.m_aDirty
	bool m_bDeformed;           // some vertex moved: kept for the match, sent to joiners
	bool m_bTraceHidden;        // TRACEABLE cleared by MUD_WorldGroundEntity.SetTraceHidden, to restore
	float m_fMeshAt = -100;
	float m_fColAt = -100;
	float m_fNearAt;            // m_fTime a vehicle was last near
	// patch-local vertex rect whose m_aSmooth is stale (-1 = clean)
	int m_iSx0 = -1;
	int m_iSx1;
	int m_iSz0;
	int m_iSz1;
	// server: patch-local vertex rect not yet sent to clients (-1 = nothing), task 9
	int m_iNx0 = -1;
	int m_iNx1;
	int m_iNz0;
	int m_iNz1;
	bool m_bNetQueued;
}

//! Scratch buffers shared by all patches (script class members max 256 KB: ~146 KB here)
class MUD_FineBuffers
{
	vector m_aVerts[1089];
	vector m_aUnder[1089];      // groove layer, m_fRevealGap under the surface
	vector m_aShell[1089];      // bullet slab: collider verts lowered by m_fBulletSlab, facing down
	vector m_aPlate[1089];      // safety plate: base + floor - m_fPlateGap
	float m_aUVs[2178];         // surface: same UVs as the skin chunk under it (0..1 per 64 m chunk)
	float m_aGrooveUVs[2178];   // groove layer: 0..1 per patch
	int m_aIndices[6144];       // this patch's triangles, diagonal per skin cell
	int m_aMeshIdx[6144];       // m_aIndices with kept-terrain cells collapsed to a point
	int m_aShellIdx[6144];      // m_aIndices with the winding reversed
	int m_aSplitIdx[6144];      // one collider piece
	int m_aCellSlot[1024];      // collider slot per fine cell, -1 = kept terrain
	float m_aBlur[1521];        // smoothing window, up to 39 x 39
	float m_aBlurRows[1287];    // smoothing X pass, up to 33 x 39
}
```

- [ ] **Step 2: `MUD_Wheels.c`**

Create `ADDON/Scripts/Game/Mud/MUD_Wheels.c`:

```c
//! Mud plan 3: every wheeled vehicle in play, so the world ground never has to search the map for them
class MUD_Wheels
{
	protected static ref array<Vehicle> s_aAll = {};   // weak: a deleted vehicle reads null

	//------------------------------------------------------------------------------------------------
	static void Register(Vehicle v)
	{
		if (!v || s_aAll.Contains(v) || !v.FindComponent(VehicleWheeledSimulation))
			return;
		Prune();
		s_aAll.Insert(v);
	}

	//------------------------------------------------------------------------------------------------
	//! Live wheeled vehicles (statics outlive a play session: dead entries are pruned here)
	static array<Vehicle> GetAll()
	{
		Prune();
		return s_aAll;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Prune()
	{
		for (int i = s_aAll.Count() - 1; i >= 0; i--)
		{
			if (!s_aAll[i])
				s_aAll.Remove(i);
		}
	}
}

//------------------------------------------------------------------------------------------------
modded class Vehicle
{
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (GetGame().InPlayMode())
			MUD_Wheels.Register(this);
	}
}
```

- [ ] **Step 3: `MUD_WorldGround.c`**

Create `ADDON/Scripts/Game/Mud/MUD_WorldGround.c`:

```c
[EntityEditorProps(category: "Mud", description: "Mud plan 3: deformable ground on the whole holed map - 8 m fine patches swap in over the coarse skin where vehicles drive and the ground moves. Needs MUD_CoarseSkinEntity in the world. One per world.", color: "120 80 40 255")]
class MUD_WorldGroundEntityClass : GenericEntityClass
{
}

//! Plan 3. The coarse skin (MUD_CoarseSkinEntity) fills every hole cell with the exact 2 m heightmap
//! triangles. This entity swaps 8 m fine patches (MUD_FinePatch, 0.25 m verts) in over 4 x 4 skin
//! cells where a moving wheeled vehicle comes near or a stamp (rut, crater, shovel, server message)
//! lands, and covers those skin cells in the same call. A fresh patch lies exactly on the skin
//! triangles: its fine cells take the diagonal of the skin cell they are in. Undeformed patches with
//! no vehicle near for m_fFreeAfter go again; deformed ones stay for the match.
//! Algorithms copied from MUD_GroundEntity, line refs at each method.
class MUD_WorldGroundEntity : GenericEntity
{
	static const int PCELLS = 32;       // fine cells per patch side
	static const int PVERTS = 33;
	static const int SKIN = 4;          // skin cells per patch side
	static const int FINE = 8;          // fine cells per skin cell side
	static const int MAT_SLOTS = 4;     // surface pieces per collider; + bullet slab + safety plate = 6 (MakeStatic max)
	static const int TLBR = 16;         // MUD_FinePatch.m_aCell bit: the skin cell's shared edge is tl-br
	static const int SLOT = 15;

	[Attribute("{D6FF1C7CCAA00596}Common/Materials/default_MatTerrainObj1.emat", UIWidgets.ResourcePickerThumbnail, "Surface material (terrain look, same as the skin). Must load or no mesh is built.", "emat", category: "Ground")]
	protected ResourceName m_Material;

	[Attribute("{6EBF7ADA9FD582F7}Assets/Structures/Military/Fortifications/DirtMounds/Data/DirtCover_01.emat", UIWidgets.ResourcePickerThumbnail, "Groove material: a layer just under the surface that shows where ruts / craters go deeper than Reveal Depth. Empty = off.", "emat", category: "Ground")]
	protected ResourceName m_RutMaterial;

	[Attribute("0.06", UIWidgets.EditBox, "Deeper than this (m) shows the groove material", category: "Ground")]
	protected float m_fRevealDepth;

	[Attribute("0.1", UIWidgets.EditBox, "Depth band (m) over which the surface sinks under the groove layer", category: "Ground")]
	protected float m_fRevealBlend;

	[Attribute("0.03", UIWidgets.EditBox, "Groove layer this far under the surface (m)", category: "Ground")]
	protected float m_fRevealGap;

	[Attribute("{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat", UIWidgets.ResourcePickerThumbnail, "Game material of the safety plate and the bullet slab fallback", "gamemat", category: "Ground")]
	protected ResourceName m_GameMaterial;

	[Attribute("asphalt,concrete", UIWidgets.EditBox, "Hard surfaces: a game material whose path contains one of these words (comma separated) gets no ruts and no shovel. Explosions still crater it.", category: "Ground")]
	protected string m_sHardWords;

	[Attribute("{C3691F2D8FE0234F}Common/Materials/Game/dirt.gamemat", UIWidgets.ResourcePickerThumbnail, "Game material a hard cell turns into where an explosion broke it", "gamemat", category: "Ground")]
	protected ResourceName m_CraterMaterial;

	[Attribute("0", UIWidgets.ComboBox, "Locked border: verts at kept terrain never move, digging fades in over Edge Fade. Dirt wall: dig to the edge, a wall fills the gap.", "", ParamEnumArray.FromEnum(MUD_EEdgeMode), category: "Edges")]
	protected MUD_EEdgeMode m_eEdgeMode;

	[Attribute("1", UIWidgets.EditBox, "Locked border: depth fades from 0 at kept terrain to full over this distance (m)", category: "Edges")]
	protected float m_fEdgeFade;

	[Attribute("-1", UIWidgets.EditBox, "Building lock: grow (+) or shrink (-) a building's box (m). Inside it the ground never moves.", category: "Edges")]
	protected float m_fBuildingMargin;

	[Attribute("16", UIWidgets.EditBox, "Make patches ahead within this radius (m) of a moving wheeled vehicle", category: "Patches")]
	protected float m_fAheadRadius;

	[Attribute("1", UIWidgets.EditBox, "A vehicle counts as moving above this speed (m/s)", category: "Patches")]
	protected float m_fAheadSpeed;

	[Attribute("2", UIWidgets.EditBox, "At most this many patches made ahead per frame (stamps make theirs at once)", category: "Patches")]
	protected int m_iAheadPerFrame;

	[Attribute("60", UIWidgets.EditBox, "An undeformed patch with no vehicle within the radius for this long (s) goes again", category: "Patches")]
	protected float m_fFreeAfter;

	[Attribute("2000", UIWidgets.EditBox, "Warn once when this many patches are deformed (~35 KB each, kept for the match)", category: "Patches")]
	protected int m_iBudget;

	[Attribute("4", UIWidgets.EditBox, "Max Depth: nothing digs deeper than this below the undeformed surface (m). Depth boxes override it.", category: "Floor")]
	protected float m_fMaxDepth;

	[Attribute("0.5", UIWidgets.EditBox, "Safety plate this far under the deepest floor (m)", category: "Floor")]
	protected float m_fPlateGap;

	[Attribute("1.5", UIWidgets.EditBox, "Bullet slab this far under the collider (m), FireGeo only, facing down. 0 = off.", category: "Floor")]
	protected float m_fBulletSlab;

	[Attribute("0.1", UIWidgets.EditBox, "Min seconds between mesh rebuilds of a patch", category: "Rebuild")]
	protected float m_fMeshInterval;

	[Attribute("0.2", UIWidgets.EditBox, "Min seconds between collider rebuilds of a patch (and net send interval)", category: "Rebuild")]
	protected float m_fColliderInterval;

	[Attribute("2", UIWidgets.Slider, "Collider smoothing radius in fine steps (0 = off, 2 = 1.25 m)", "0 3 1", category: "Rebuild")]
	protected int m_iColliderSmooth;

	[Attribute("1", UIWidgets.CheckBox, "Print stats every 10 s and check every new patch against the skin by trace", category: "Debug")]
	protected bool m_bDebug;

	protected MUD_CoarseSkinEntity m_Skin;
	protected ref map<int, ref MUD_FinePatch> m_mPatches = new map<int, ref MUD_FinePatch>();
	protected ref set<int> m_sNoHole = new set<int>();     // patch keys without a hole cell
	protected ref array<int> m_aDirty = {};                // patch keys with a pending mesh / collider
	protected ref MUD_FineBuffers m_Buf;
	protected ref array<float> m_aSmoothW = {};
	protected ref array<string> m_aHardWords = {};
	protected ref array<IEntity> m_aFound = {};
	protected ref TraceParam m_Trace = new TraceParam();
	protected RplComponent m_RplComponent;
	protected float m_fX0;
	protected float m_fZ0;
	protected float m_fCell;
	protected float m_fFine;
	protected int m_iNPX;
	protected int m_iNPZ;
	protected float m_fTime;
	protected float m_fSinceFree;
	protected bool m_bReady;
	protected bool m_bRender;
	protected bool m_bGroove;
	protected int m_iDeformed;
	protected bool m_bBudgetWarned;

	// debug stats, printed every 10 s
	protected float m_fSinceStats;
	protected int m_iMadeAhead;
	protected int m_iMadeStamp;
	protected int m_iFreed;
	protected int m_iMeshBuilds;
	protected int m_iMeshMs;
	protected int m_iColBuilds;
	protected int m_iColMs;
	protected int m_iSmoothMs;
	protected int m_iSwapMs;
	protected int m_iWorstMs;
	protected int m_iSkipHeight;
	protected int m_iChkPts;
	protected int m_iChkHit;
	protected int m_iChkGood;
	protected float m_fChkWorst;
	protected int m_iMemStartKB;

	protected static MUD_WorldGroundEntity s_Inst;
	protected static bool s_bHidden;

	//------------------------------------------------------------------------------------------------
	void MUD_WorldGroundEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	void ~MUD_WorldGroundEntity()
	{
		if (s_Inst == this)
			s_Inst = null;
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			if (p && p.m_Ent)
				delete p.m_Ent;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame().InPlayMode())
			ClearEventMask(EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	static MUD_WorldGroundEntity GetInstance()
	{
		return s_Inst;
	}

	//------------------------------------------------------------------------------------------------
	bool IsReady()
	{
		return m_bReady;
	}

	//------------------------------------------------------------------------------------------------
	//! True on a client: the server owns the ground shape
	protected bool IsProxy()
	{
		return m_RplComponent != null && m_RplComponent.IsProxy();
	}

	//------------------------------------------------------------------------------------------------
	//! Runs every frame until the skin is built. False = not yet.
	protected bool Setup()
	{
		m_Skin = MUD_CoarseSkinEntity.GetInstance();
		if (!m_Skin || !m_Skin.IsReady())
			return false;

		int ncx;
		int ncz;
		m_Skin.GetGrid(m_fX0, m_fZ0, ncx, ncz);
		m_fCell = m_Skin.GetCell();
		m_fFine = m_fCell / FINE;
		m_iNPX = ncx / SKIN;        // Arland 2048 / 4 = 512
		m_iNPZ = ncz / SKIN;
		m_bRender = RplSession.Mode() != RplMode.Dedicated;
		m_bGroove = m_bRender && !m_RutMaterial.IsEmpty();
		m_RplComponent = RplComponent.Cast(FindComponent(RplComponent));
		m_Buf = new MUD_FineBuffers();

		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				m_Buf.m_aGrooveUVs[v * 2] = vx / 32.0;
				m_Buf.m_aGrooveUVs[v * 2 + 1] = 1 - vz / 32.0;
			}
		}

		array<string> words = {};
		m_sHardWords.Split(",", words, true);
		foreach (string w : words)
		{
			w.Trim();
			w.ToLower();
			if (!w.IsEmpty())
				m_aHardWords.Insert(w);
		}

		// collider blur weights = Pascal row 2r (r 2 -> 1 4 6 4 1), as MUD_GroundEntity.c:446-450
		int r = Math.ClampInt(m_iColliderSmooth, 0, 3);
		m_aSmoothW.Insert(1);
		for (int k = 1; k <= 2 * r; k++)
			m_aSmoothW.Insert(m_aSmoothW[k - 1] * (2 * r - k + 1) / k);

		// first world ground of a play session: drop hide state a previous session may have left
		s_bHidden = false;
		MUD_GroundEntity.ResetTraceHide();
		s_Inst = this;
		m_iMemStartKB = System.MemoryAllocationKB();
		Print(string.Format("MUD world: ready, patch grid %1 x %2, fine %3 m, render %4, groove %5, edge mode %6, rpl %7",
			m_iNPX, m_iNPZ, m_fFine, m_bRender, m_bGroove, typename.EnumToString(MUD_EEdgeMode, m_eEdgeMode), m_RplComponent != null));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bReady)
		{
			m_bReady = Setup();
			return;
		}

		int t0 = System.GetTickCount();
		m_fTime += timeSlice;
		Ahead();
		Rebuild();
		FreeIdle(timeSlice);
		int ms = System.GetTickCount(t0);
		if (ms > m_iWorstMs)
			m_iWorstMs = ms;
		if (m_bDebug)
			Stats(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! World XZ -> global fine vertex coords (float). False outside the patch grid.
	protected bool ToFine(vector wp, out float fx, out float fz)
	{
		fx = (wp[0] - m_fX0) / m_fFine;
		fz = (wp[2] - m_fZ0) / m_fFine;
		return fx >= 0 && fz >= 0 && fx <= m_iNPX * PCELLS && fz <= m_iNPZ * PCELLS;
	}

	//------------------------------------------------------------------------------------------------
	//! First patch whose vertex range holds global fine vertex (gx, gz), else the patch to its left /
	//! below (a vertex on a patch edge is in 2 or 4 patches). null if none exists. v = local index.
	protected MUD_FinePatch At(int gx, int gz, out int v)
	{
		int pxa = gx / PCELLS;
		int pza = gz / PCELLS;
		for (int pz = pza; pz >= pza - 1; pz--)
		{
			int lz = gz - pz * PCELLS;
			if (pz < 0 || pz >= m_iNPZ || lz < 0 || lz > PCELLS)
				continue;
			for (int px = pxa; px >= pxa - 1; px--)
			{
				int lx = gx - px * PCELLS;
				if (px < 0 || px >= m_iNPX || lx < 0 || lx > PCELLS)
					continue;
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (p)
				{
					v = lz * PVERTS + lx;
					return p;
				}
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetH(int gx, int gz)
	{
		int v;
		MUD_FinePatch p = At(gx, gz, v);
		if (!p)
			return 0;
		return p.m_aH[v];
	}

	//------------------------------------------------------------------------------------------------
	//! Write h into every patch holding vertex (gx, gz)
	protected void SetH(int gx, int gz, float h)
	{
		int pxa = gx / PCELLS;
		int pza = gz / PCELLS;
		for (int pz = pza; pz >= pza - 1; pz--)
		{
			int lz = gz - pz * PCELLS;
			if (pz < 0 || pz >= m_iNPZ || lz < 0 || lz > PCELLS)
				continue;
			for (int px = pxa; px >= pxa - 1; px--)
			{
				int lx = gx - px * PCELLS;
				if (px < 0 || px >= m_iNPX || lx < 0 || lx > PCELLS)
					continue;
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				p.m_aH[lz * PVERTS + lx] = h;
				if (!p.m_bDeformed && Math.AbsFloat(h) > 0.0005)
				{
					p.m_bDeformed = true;
					m_iDeformed++;
					if (m_iDeformed > m_iBudget && !m_bBudgetWarned)
					{
						m_bBudgetWarned = true;
						Print(string.Format("MUD world: %1 deformed patches, past the budget %2 (~35 KB each, kept for the match)", m_iDeformed, m_iBudget), LogLevel.WARNING);
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lowest patch index whose vertex range reaches global vertex g (a vertex on an edge is in 2)
	protected int PatchLo(int g, int n)
	{
		if (g <= 0)
			return 0;
		return Math.ClampInt((g - 1) / PCELLS, 0, n - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected int PatchHi(int g, int n)
	{
		if (g <= 0)
			return 0;
		return Math.ClampInt(g / PCELLS, 0, n - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Undeformed height at fine coords (fx, fz) from the skin triangles; ok false where no hole cell
	//! touches the point (kept terrain: such a vertex is never drawn and always locked)
	protected float BaseAt(float fx, float fz, out bool ok)
	{
		int cxa = Math.Floor(fx / FINE);
		int cza = Math.Floor(fz / FINE);
		float h00;
		float h10;
		float h01;
		float h11;
		bool tlbr;
		ResourceName mat;
		for (int dz = 0; dz >= -1; dz--)
		{
			for (int dx = 0; dx >= -1; dx--)
			{
				int cx = cxa + dx;
				int cz = cza + dz;
				float u = (fx - cx * FINE) / FINE;
				float w = (fz - cz * FINE) / FINE;
				if (u < 0 || u > 1 || w < 0 || w > 1)
					continue;
				if (!m_Skin.CellShape(cx, cz, h00, h10, h01, h11, tlbr, mat))
					continue;
				ok = true;
				return TriHeight(h00, h10, h01, h11, tlbr, u, w);
			}
		}
		ok = false;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Height inside one skin cell at fractions (u, w): the triangle the skin draws there. Skin
	//! triangles (MUD_CoarseSkin.c BuildGeometry): tl-br = (bl, tl, br) + (tl, tr, br); else
	//! (bl, tl, tr) + (tr, br, bl).
	protected float TriHeight(float h00, float h10, float h01, float h11, bool tlbr, float u, float w)
	{
		if (tlbr)
		{
			if (u + w <= 1)
				return h00 + u * (h10 - h00) + w * (h01 - h00);
			return h11 + (1 - u) * (h01 - h11) + (1 - w) * (h10 - h11);
		}
		if (w >= u)
			return h00 + w * (h01 - h00) + u * (h11 - h01);
		return h00 + u * (h10 - h00) + w * (h11 - h10);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsHardMat(ResourceName mat)
	{
		string path = mat;
		path.ToLower();
		foreach (string w : m_aHardWords)
		{
			if (path.Contains(w))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Hard (or kept terrain) at global skin cell (cx, cz): the patch's own state if it exists (a crater
	//! may have broken it, task 6), else the skin's game material
	protected bool CellHard(int cx, int cz)
	{
		if (cx < 0 || cz < 0 || cx >= m_iNPX * SKIN || cz >= m_iNPZ * SKIN)
			return true;
		MUD_FinePatch p = m_mPatches.Get((cz / SKIN) * m_iNPX + cx / SKIN);
		int c = (cz - (cz / SKIN) * SKIN) * SKIN + cx - (cx / SKIN) * SKIN;
		if (p)
			return p.m_aCell[c] < 0 || p.m_aCellHard[c] != 0;
		float h00;
		float h10;
		float h01;
		float h11;
		bool tlbr;
		ResourceName mat;
		if (!m_Skin.CellShape(cx, cz, h00, h10, h01, h11, tlbr, mat))
			return true;
		return IsHardMat(mat);
	}

	//------------------------------------------------------------------------------------------------
	//! Softness of the patch's 16 cells: hard or kept = 0, else the share of soft hole cells in its 3 x 3
	//! (kept terrain does not count, the edge lock fades there), so a rut runs out over ~2 m toward hard
	//! ground (as MUD_GroundEntity.c:1089 UpdateSoft, per 2 m skin cell instead of 1 m samples)
	protected void ComputeSoft(MUD_FinePatch p)
	{
		for (int lz = 0; lz < SKIN; lz++)
		{
			for (int lx = 0; lx < SKIN; lx++)
			{
				int c = lz * SKIN + lx;
				if (p.m_aCell[c] < 0 || p.m_aCellHard[c] != 0)
				{
					p.m_aCellSoft[c] = 0;
					continue;
				}
				int cx = p.m_iPx * SKIN + lx;
				int cz = p.m_iPz * SKIN + lz;
				float soft = 0;
				float all = 0;
				for (int nz = cz - 1; nz <= cz + 1; nz++)
				{
					for (int nx = cx - 1; nx <= cx + 1; nx++)
					{
						if (!m_Skin.IsHoleCell(nx, nz))
							continue;
						all++;
						if (!CellHard(nx, nz))
							soft++;
					}
				}
				p.m_aCellSoft[c] = soft / all;     // all >= 1: the cell itself
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Softness under global fine vertex (gx, gz): the skin cell holding it (x / 8, clamped), read from
	//! that cell's patch - the same answer in every patch sharing the vertex
	protected float VertexSoft(int gx, int gz)
	{
		int cx = Math.ClampInt(gx / FINE, 0, m_iNPX * SKIN - 1);
		int cz = Math.ClampInt(gz / FINE, 0, m_iNPZ * SKIN - 1);
		MUD_FinePatch p = m_mPatches.Get((cz / SKIN) * m_iNPX + cx / SKIN);
		if (!p)
			return 0;
		return p.m_aCellSoft[(cz - (cz / SKIN) * SKIN) * SKIN + cx - (cx / SKIN) * SKIN];
	}

	//------------------------------------------------------------------------------------------------
	//! Locked border (spec 4.3): 0 on a kept skin cell's edge, rising to 1 over m_fEdgeFade (smoothstep).
	//! Only kept cells count, so the same vertex gets the same lock in every patch.
	protected float EdgeLock(int gx, int gz)
	{
		if (m_eEdgeMode != MUD_EEdgeMode.LOCKED)
			return 1;
		float x = gx * m_fFine;
		float z = gz * m_fFine;
		int cx0 = Math.Floor((x - m_fEdgeFade) / m_fCell);
		int cx1 = Math.Floor((x + m_fEdgeFade) / m_fCell);
		int cz0 = Math.Floor((z - m_fEdgeFade) / m_fCell);
		int cz1 = Math.Floor((z + m_fEdgeFade) / m_fCell);
		float best = 1e9;
		for (int cz = cz0; cz <= cz1; cz++)
		{
			for (int cx = cx0; cx <= cx1; cx++)
			{
				if (m_Skin.IsHoleCell(cx, cz))
					continue;   // also false outside the map: the map edge locks too
				float dx = Math.Max(Math.Max(cx * m_fCell - x, 0), x - (cx + 1) * m_fCell);
				float dz = Math.Max(Math.Max(cz * m_fCell - z, 0), z - (cz + 1) * m_fCell);
				float d = Math.Sqrt(dx * dx + dz * dz);
				if (d < best)
					best = d;
			}
		}
		if (m_fEdgeFade <= 0)
		{
			if (best > 0)
				return 1;
			return 0;
		}
		float t = Math.Clamp(best / m_fEdgeFade, 0, 1);
		return t * t * (3 - 2 * t);
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddFound(IEntity e)
	{
		m_aFound.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Buildings (and MUD_CutBoxEntity boxes) on the patch: verts inside their box never move.
	//! Box test in the building's own frame (as MUD_GroundEntity.c:818-824). Returns locked verts.
	protected int BuildingLock(MUD_FinePatch p)
	{
		float wx0 = m_fX0 + p.m_iPx * PCELLS * m_fFine;
		float wz0 = m_fZ0 + p.m_iPz * PCELLS * m_fFine;
		float size = PCELLS * m_fFine;
		m_aFound.Clear();
		GetWorld().QueryEntitiesByAABB(Vector(wx0, -300, wz0), Vector(wx0 + size, 400, wz0 + size), AddFound);
		int locked;
		foreach (IEntity e : m_aFound)
		{
			vector bmin;
			vector bmax;
			MUD_CutBoxEntity box = MUD_CutBoxEntity.Cast(e);
			if (box)
			{
				box.GetBox(bmin, bmax);
			}
			else if (Building.Cast(e))
			{
				e.GetBounds(bmin, bmax);
				bmin[0] = bmin[0] - m_fBuildingMargin;
				bmin[2] = bmin[2] - m_fBuildingMargin;
				bmax[0] = bmax[0] + m_fBuildingMargin;
				bmax[2] = bmax[2] + m_fBuildingMargin;
			}
			else
			{
				continue;
			}
			for (int vz = 0; vz < PVERTS; vz++)
			{
				for (int vx = 0; vx < PVERTS; vx++)
				{
					int v = vz * PVERTS + vx;
					if (p.m_aLock[v] <= 0)
						continue;
					vector lp = e.CoordToLocal(Vector(wx0 + vx * m_fFine, p.m_aBase[v], wz0 + vz * m_fFine));
					if (lp[0] >= bmin[0] && lp[0] <= bmax[0] && lp[2] >= bmin[2] && lp[2] <= bmax[2])
					{
						p.m_aLock[v] = 0;
						locked++;
					}
				}
			}
		}
		return locked;
	}

	//------------------------------------------------------------------------------------------------
	//! Floor of every vertex: -Max Depth (task 6 adds depth boxes here)
	protected void ApplyFloor(MUD_FinePatch p)
	{
		for (int v = 0; v < 1089; v++)
			p.m_aFloor[v] = -m_fMaxDepth;
	}

	//------------------------------------------------------------------------------------------------
	//! Base, lock, floor, cells, softness. Same inputs on every machine, so every machine builds the same.
	protected void InitPatch(MUD_FinePatch p)
	{
		int cx0 = p.m_iPx * SKIN;
		int cz0 = p.m_iPz * SKIN;
		float h00;
		float h10;
		float h01;
		float h11;
		bool tlbr;
		ResourceName mat;
		for (int lz = 0; lz < SKIN; lz++)
		{
			for (int lx = 0; lx < SKIN; lx++)
			{
				int c = lz * SKIN + lx;
				if (!m_Skin.CellShape(cx0 + lx, cz0 + lz, h00, h10, h01, h11, tlbr, mat))
				{
					p.m_aCell[c] = -1;
					p.m_aCellHard[c] = 1;
					continue;
				}
				int slot = p.m_aMats.Find(mat);
				if (slot < 0)
				{
					if (p.m_aMats.Count() < MAT_SLOTS)
						slot = p.m_aMats.Insert(mat);
					else
						slot = 0;   // more than 4 surfaces in one patch: the rest join slot 0
				}
				int cell = slot;
				if (tlbr)
					cell = cell | TLBR;
				p.m_aCell[c] = cell;
				p.m_aCellHard[c] = 0;
				if (IsHardMat(mat))
					p.m_aCellHard[c] = 1;
			}
		}

		int gx0 = p.m_iPx * PCELLS;
		int gz0 = p.m_iPz * PCELLS;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				bool ok;
				p.m_aBase[v] = BaseAt(gx0 + vx, gz0 + vz, ok);
				p.m_aH[v] = 0;
				p.m_aSmooth[v] = 0;
				p.m_aRutTouch[v] = -100;
				p.m_aBermTouch[v] = -100;
				if (ok)
					p.m_aLock[v] = EdgeLock(gx0 + vx, gz0 + vz);
				else
					p.m_aLock[v] = 0;
			}
		}
		BuildingLock(p);
		ApplyFloor(p);
	}

	//------------------------------------------------------------------------------------------------
	//! A new patch takes the heights of edge verts it shares with patches that already exist, so a
	//! patch made next to a deformed one starts without a step (Review Focus 1). Returns verts copied
	//! with a non-zero height.
	protected int CopyNeighbourEdges(MUD_FinePatch p)
	{
		int copied;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				if (vx != 0 && vx != PCELLS && vz != 0 && vz != PCELLS)
					continue;
				int gx = p.m_iPx * PCELLS + vx;
				int gz = p.m_iPz * PCELLS + vz;
				// p is not in the map yet: At() finds only the neighbours
				int ov;
				MUD_FinePatch o = At(gx, gz, ov);
				if (!o)
					continue;
				int v = vz * PVERTS + vx;
				p.m_aH[v] = o.m_aH[ov];
				p.m_aSmooth[v] = o.m_aSmooth[ov];
				if (Math.AbsFloat(o.m_aH[ov]) > 0.0005)
					copied++;
			}
		}
		return copied;
	}

	//------------------------------------------------------------------------------------------------
	//! Patch (px, pz) has at least one hole cell (patches without are cached in m_sNoHole)
	protected bool HasHole(int px, int pz)
	{
		if (px < 0 || pz < 0 || px >= m_iNPX || pz >= m_iNPZ)
			return false;
		int key = pz * m_iNPX + px;
		if (m_sNoHole.Contains(key))
			return false;
		for (int lz = 0; lz < SKIN; lz++)
		{
			for (int lx = 0; lx < SKIN; lx++)
			{
				if (m_Skin.IsHoleCell(px * SKIN + lx, pz * SKIN + lz))
					return true;
			}
		}
		m_sNoHole.Insert(key);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The patch at (px, pz), made now if it has a hole cell: mesh + collider built and the 4 x 4 skin
	//! cells covered in this same call. null when the patch has no hole cell or is off the grid.
	protected MUD_FinePatch EnsurePatch(int px, int pz, bool byStamp)
	{
		if (px < 0 || pz < 0 || px >= m_iNPX || pz >= m_iNPZ)
			return null;
		int key = pz * m_iNPX + px;
		MUD_FinePatch p = m_mPatches.Get(key);
		if (p)
			return p;
		if (!HasHole(px, pz))
			return null;

		p = new MUD_FinePatch();
		p.m_iPx = px;
		p.m_iPz = pz;
		p.m_iKey = key;
		p.m_fNearAt = m_fTime;
		InitPatch(p);
		int copied = CopyNeighbourEdges(p);
		m_mPatches.Insert(key, p);
		if (copied > 0)
		{
			p.m_bDeformed = true;
			m_iDeformed++;
			p.m_iSx0 = 0;
			p.m_iSx1 = PCELLS;
			p.m_iSz0 = 0;
			p.m_iSz1 = PCELLS;
			SmoothPatch(p);
		}
		ComputeSoft(p);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = Vector(m_fX0 + px * PCELLS * m_fFine, 0, m_fZ0 + pz * PCELLS * m_fFine);
		p.m_Ent = GetGame().SpawnEntity(MUD_FinePatchEntity, GetWorld(), params);
		if (!p.m_Ent)
		{
			Print("MUD world: could not spawn patch entity", LogLevel.ERROR);
			m_mPatches.Remove(key);
			return null;
		}
		if (s_bHidden)
		{
			p.m_Ent.ClearFlags(EntityFlags.TRACEABLE, false);
			p.m_bTraceHidden = true;
		}
		if (m_bRender)
			BuildMesh(p);
		BuildCollider(p);
		int t0 = System.GetTickCount();
		m_Skin.SetCovered(px * SKIN, pz * SKIN, SKIN, true);
		m_iSwapMs += System.GetTickCount(t0);
		if (byStamp)
			m_iMadeStamp++;
		else
			m_iMadeAhead++;
		if (m_bDebug)
			CheckNew(p);
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! Every patch whose vertex range meets [gx0..gx1] x [gz0..gz1] (edge verts included), made now
	protected void EnsureRange(int gx0, int gx1, int gz0, int gz1)
	{
		for (int pz = PatchLo(gz0, m_iNPZ); pz <= PatchHi(gz1, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0, m_iNPX); px <= PatchHi(gx1, m_iNPX); px++)
				EnsurePatch(px, pz, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Uncover the skin cells, then delete the patch, in one call
	protected void FreePatch(MUD_FinePatch p)
	{
		int t0 = System.GetTickCount();
		m_Skin.SetCovered(p.m_iPx * SKIN, p.m_iPz * SKIN, SKIN, false);
		m_iSwapMs += System.GetTickCount(t0);
		if (p.m_Ent)
			delete p.m_Ent;
		m_mPatches.Remove(p.m_iKey);
		m_iFreed++;
	}

	//------------------------------------------------------------------------------------------------
	//! Patches ahead of moving wheeled vehicles (nearest first, m_iAheadPerFrame per frame); every
	//! vehicle, moving or parked, keeps the patches around it from being freed
	protected void Ahead()
	{
		array<int> candKey = {};
		array<float> candDist = {};
		float size = PCELLS * m_fFine;
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh)
				continue;
			Physics ph = veh.GetPhysics();
			if (!ph)
				continue;
			bool moving = ph.GetVelocity().Length() > m_fAheadSpeed;
			vector o = veh.GetOrigin();
			int pxa = Math.Floor((o[0] - m_fX0 - m_fAheadRadius) / size);
			int pxb = Math.Floor((o[0] - m_fX0 + m_fAheadRadius) / size);
			int pza = Math.Floor((o[2] - m_fZ0 - m_fAheadRadius) / size);
			int pzb = Math.Floor((o[2] - m_fZ0 + m_fAheadRadius) / size);
			for (int pz = Math.Max(pza, 0); pz <= Math.Min(pzb, m_iNPZ - 1); pz++)
			{
				for (int px = Math.Max(pxa, 0); px <= Math.Min(pxb, m_iNPX - 1); px++)
				{
					// distance from the vehicle to the patch square
					float x0 = m_fX0 + px * size;
					float z0 = m_fZ0 + pz * size;
					float dx = Math.Max(Math.Max(x0 - o[0], 0), o[0] - x0 - size);
					float dz = Math.Max(Math.Max(z0 - o[2], 0), o[2] - z0 - size);
					float d = Math.Sqrt(dx * dx + dz * dz);
					if (d > m_fAheadRadius)
						continue;
					int key = pz * m_iNPX + px;
					MUD_FinePatch p = m_mPatches.Get(key);
					if (p)
					{
						p.m_fNearAt = m_fTime;
						continue;
					}
					if (moving && !m_sNoHole.Contains(key) && !candKey.Contains(key))
					{
						candKey.Insert(key);
						candDist.Insert(d);
					}
				}
			}
		}

		for (int n = 0; n < m_iAheadPerFrame && !candKey.IsEmpty(); n++)
		{
			int best = 0;
			for (int i = 1; i < candKey.Count(); i++)
			{
				if (candDist[i] < candDist[best])
					best = i;
			}
			int k = candKey[best];
			candKey.Remove(best);
			candDist.Remove(best);
			EnsurePatch(k - (k / m_iNPX) * m_iNPX, k / m_iNPX, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Once a second: undeformed patches with no vehicle near for m_fFreeAfter go
	protected void FreeIdle(float timeSlice)
	{
		m_fSinceFree += timeSlice;
		if (m_fSinceFree < 1)
			return;
		m_fSinceFree = 0;
		array<MUD_FinePatch> idle = {};
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			if (!p.m_bDeformed && m_fTime - p.m_fNearAt > m_fFreeAfter)
				idle.Insert(p);
		}
		foreach (MUD_FinePatch q : idle)
			FreePatch(q);
	}

	//------------------------------------------------------------------------------------------------
	//! Flag every patch holding a changed vertex (mesh + collider) and every patch the blur reaches
	//! from it (collider) - as MUD_GroundEntity.c:1892 MarkDirty, global fine vertex coords.
	//! Task 9 adds the net rect here.
	protected void MarkDirty(int gx0, int gx1, int gz0, int gz1)
	{
		int r = m_aSmoothW.Count() / 2;
		for (int pz = PatchLo(gz0 - r, m_iNPZ); pz <= PatchHi(gz1 + r, m_iNPZ); pz++)
		{
			for (int px = PatchLo(gx0 - r, m_iNPX); px <= PatchHi(gx1 + r, m_iNPX); px++)
			{
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				int ox = px * PCELLS;
				int oz = pz * PCELLS;
				bool own = gx1 >= ox && gx0 <= ox + PCELLS && gz1 >= oz && gz0 <= oz + PCELLS;
				if (own && m_bRender)
					p.m_bMeshDirty = true;
				p.m_bColDirty = true;
				if (!p.m_bQueued)
				{
					p.m_bQueued = true;
					m_aDirty.Insert(p.m_iKey);
				}

				int sx0 = Math.ClampInt(gx0 - r - ox, 0, PCELLS);
				int sx1 = Math.ClampInt(gx1 + r - ox, 0, PCELLS);
				int sz0 = Math.ClampInt(gz0 - r - oz, 0, PCELLS);
				int sz1 = Math.ClampInt(gz1 + r - oz, 0, PCELLS);
				if (p.m_iSx0 < 0)
				{
					p.m_iSx0 = sx0;
					p.m_iSx1 = sx1;
					p.m_iSz0 = sz0;
					p.m_iSz1 = sz1;
				}
				else
				{
					p.m_iSx0 = Math.Min(p.m_iSx0, sx0);
					p.m_iSx1 = Math.Max(p.m_iSx1, sx1);
					p.m_iSz0 = Math.Min(p.m_iSz0, sz0);
					p.m_iSz1 = Math.Max(p.m_iSz1, sz1);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! At most one mesh and one collider per frame, the longest-waiting first (MUD_GroundEntity.c:2220)
	protected void Rebuild()
	{
		MUD_FinePatch mp;
		MUD_FinePatch cp;
		for (int i = m_aDirty.Count() - 1; i >= 0; i--)
		{
			MUD_FinePatch p = m_mPatches.Get(m_aDirty[i]);
			if (!p || (!p.m_bMeshDirty && !p.m_bColDirty))
			{
				if (p)
					p.m_bQueued = false;
				m_aDirty.Remove(i);
				continue;
			}
			if (m_bRender && p.m_bMeshDirty && m_fTime - p.m_fMeshAt >= m_fMeshInterval && (!mp || p.m_fMeshAt < mp.m_fMeshAt))
				mp = p;
			if (p.m_bColDirty && m_fTime - p.m_fColAt >= m_fColliderInterval && (!cp || p.m_fColAt < cp.m_fColAt))
				cp = p;
		}
		if (mp)
		{
			BuildMesh(mp);
			mp.m_fMeshAt = m_fTime;
		}
		if (cp)
		{
			BeforeCollider(cp);
			SmoothPatch(cp);
			BuildCollider(cp);
			cp.m_fColAt = m_fTime;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Hook before a collider rebuild (task 6: broken hard cells)
	protected void BeforeCollider(MUD_FinePatch p)
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void SmoothPatch(MUD_FinePatch p)
	{
		if (p.m_iSx0 < 0)
			return;
		UpdateSmooth(p, p.m_iSx0, p.m_iSx1, p.m_iSz0, p.m_iSz1);
		p.m_iSx0 = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-blur m_aH into m_aSmooth for patch-local verts [x0..x1] x [z0..z1]: separable binomial kernel,
	//! deformation only (MUD_GroundEntity.c:1503). Neighbour heights come through GetH, so both copies
	//! of an edge vertex get the same value. Hard and locked verts keep their own height.
	protected void UpdateSmooth(MUD_FinePatch p, int x0, int x1, int z0, int z1)
	{
		int t0 = System.GetTickCount();
		int r = m_aSmoothW.Count() / 2;
		int cols = x1 - x0 + 1;
		int wc = cols + 2 * r;
		int rows = z1 - z0 + 1 + 2 * r;
		int gx0 = p.m_iPx * PCELLS;
		int gz0 = p.m_iPz * PCELLS;

		for (int wz = 0; wz < rows; wz++)
		{
			int lz = z0 - r + wz;
			for (int wx = 0; wx < wc; wx++)
			{
				int lx = x0 - r + wx;
				float h;
				if (lx >= 0 && lx <= PCELLS && lz >= 0 && lz <= PCELLS)
					h = p.m_aH[lz * PVERTS + lx];
				else
					h = GetH(gx0 + lx, gz0 + lz);
				m_Buf.m_aBlur[wz * wc + wx] = h;
			}
		}

		float wsum = 0;
		foreach (float wt : m_aSmoothW)
			wsum += wt;
		float inv = 1 / (wsum * wsum);

		for (int z = 0; z < rows; z++)
		{
			for (int x = 0; x < cols; x++)
			{
				float sum = 0;
				for (int kx = -r; kx <= r; kx++)
					sum += m_Buf.m_aBlur[z * wc + x + r + kx] * m_aSmoothW[kx + r];
				m_Buf.m_aBlurRows[z * cols + x] = sum;
			}
		}

		for (int z2 = z0; z2 <= z1; z2++)
		{
			for (int x2 = x0; x2 <= x1; x2++)
			{
				int v = z2 * PVERTS + x2;
				if (p.m_aLock[v] < 1 || VertexSoft(gx0 + x2, gz0 + z2) <= 0)
				{
					p.m_aSmooth[v] = p.m_aH[v];
					continue;
				}
				float sum2 = 0;
				for (int kz = -r; kz <= r; kz++)
					sum2 += m_Buf.m_aBlurRows[(z2 - z0 + r + kz) * cols + x2 - x0] * m_aSmoothW[kz + r];
				p.m_aSmooth[v] = sum2 * inv;
			}
		}
		m_iSmoothMs += System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! m_aIndices (each fine cell split along its skin cell's diagonal, same winding as the skin),
	//! m_aShellIdx (reversed), m_aCellSlot (-1 = kept terrain) and m_aMeshIdx (kept cells collapsed)
	protected void FillIndices(MUD_FinePatch p)
	{
		int n;
		for (int fz = 0; fz < PCELLS; fz++)
		{
			for (int fx = 0; fx < PCELLS; fx++)
			{
				int cell = p.m_aCell[(fz / FINE) * SKIN + fx / FINE];
				int bl = fz * PVERTS + fx;
				int br = bl + 1;
				int tl = bl + PVERTS;
				int tr = tl + 1;
				int fc = fz * PCELLS + fx;
				if (cell >= 0 && (cell & TLBR) != 0)
				{
					m_Buf.m_aIndices[n] = bl;
					m_Buf.m_aIndices[n + 1] = tl;
					m_Buf.m_aIndices[n + 2] = br;
					m_Buf.m_aIndices[n + 3] = tl;
					m_Buf.m_aIndices[n + 4] = tr;
					m_Buf.m_aIndices[n + 5] = br;
				}
				else
				{
					m_Buf.m_aIndices[n] = bl;
					m_Buf.m_aIndices[n + 1] = tl;
					m_Buf.m_aIndices[n + 2] = tr;
					m_Buf.m_aIndices[n + 3] = tr;
					m_Buf.m_aIndices[n + 4] = br;
					m_Buf.m_aIndices[n + 5] = bl;
				}
				if (cell < 0)
				{
					m_Buf.m_aCellSlot[fc] = -1;
					for (int j = 0; j < 6; j++)
						m_Buf.m_aMeshIdx[n + j] = bl;
				}
				else
				{
					m_Buf.m_aCellSlot[fc] = cell & SLOT;
					for (int j2 = 0; j2 < 6; j2++)
						m_Buf.m_aMeshIdx[n + j2] = m_Buf.m_aIndices[n + j2];
				}
				n += 6;
			}
		}
		for (int i = 0; i < 6144; i += 3)
		{
			m_Buf.m_aShellIdx[i] = m_Buf.m_aIndices[i];
			m_Buf.m_aShellIdx[i + 1] = m_Buf.m_aIndices[i + 2];
			m_Buf.m_aShellIdx[i + 2] = m_Buf.m_aIndices[i + 1];
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Static mesh (flags 0), a fresh Create on every change (MUD_GroundEntity.c:1560). Surface UVs are
	//! the skin chunk's (0..1 per 64 m), so the terrain-look material lines up with the skin around it.
	protected void BuildMesh(MUD_FinePatch p)
	{
		int t0 = System.GetTickCount();
		FillIndices(p);
		int chunkPatches = MUD_CoarseSkinEntity.CELLS / SKIN;     // 8 patches per skin chunk side
		int ux = (p.m_iPx - (p.m_iPx / chunkPatches) * chunkPatches) * PCELLS;
		int uz = (p.m_iPz - (p.m_iPz / chunkPatches) * chunkPatches) * PCELLS;
		float uvDiv = chunkPatches * PCELLS;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				float d = p.m_aH[v];
				float h = p.m_aBase[v] + d;
				// groove reveal (MUD_GroundEntity.c:1440-1448)
				if (m_bGroove)
				{
					m_Buf.m_aUnder[v] = Vector(vx * m_fFine, h - m_fRevealGap, vz * m_fFine);
					float t = Math.Clamp((-d - m_fRevealDepth) / Math.Max(m_fRevealBlend, 0.01), 0, 1);
					h -= t * t * (3 - 2 * t) * m_fRevealGap * 3;
				}
				m_Buf.m_aVerts[v] = Vector(vx * m_fFine, h, vz * m_fFine);
				m_Buf.m_aUVs[v * 2] = (ux + vx) / uvDiv;
				m_Buf.m_aUVs[v * 2 + 1] = 1 - (uz + vz) / uvDiv;
			}
		}

		Resource res;
		if (m_bGroove)
		{
			int numVerts2[] = {1089, 1089};
			int numIndices2[] = {6144, 6144};
			string materials2[] = {m_Material, m_RutMaterial};
			res = MeshObject.Create(2, numVerts2, numIndices2, materials2, 0);
			if (!res || !res.GetResource())
			{
				Print(string.Format("MUD world: groove material '%1' did not load - groove layer off", m_RutMaterial), LogLevel.ERROR);
				m_bGroove = false;
				p.m_bMeshDirty = true;
				return;
			}
		}
		else
		{
			int numVerts[] = {1089};
			int numIndices[] = {6144};
			string materials[] = {m_Material};
			res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		}
		if (!res || !res.GetResource())
		{
			Print(string.Format("MUD world: MeshObject.Create failed - material '%1' did not load?", m_Material), LogLevel.ERROR);
			m_bRender = false;
			return;
		}

		MeshObject mesh = res.GetResource().ToMeshObject();
		// indices first: UpdateVerts rebuilds normals + tangents from the triangle list
		mesh.UpdateIndices(0, m_Buf.m_aMeshIdx);
		mesh.UpdateVerts(0, m_Buf.m_aVerts, m_Buf.m_aUVs);
		if (m_bGroove)
		{
			mesh.UpdateIndices(1, m_Buf.m_aMeshIdx);
			mesh.UpdateVerts(1, m_Buf.m_aUnder, m_Buf.m_aGrooveUVs);
		}
		p.m_Ent.SetObject(mesh, "");
		p.m_Ent.Update();       // fresh bounds, else culling keeps the spawn-time ones
		p.m_MeshRes = res;
		p.m_bMeshDirty = false;
		m_iMeshBuilds++;
		m_iMeshMs += System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! One trimesh per surface (<= 4) from the smoothed heights, + bullet slab (FireGeo, facing down)
	//! + safety plate (base + floor - m_fPlateGap). Old body goes and the new one comes in this call
	//! (MUD_GroundEntity.c:1643).
	protected void BuildCollider(MUD_FinePatch p)
	{
		int t0 = System.GetTickCount();
		FillIndices(p);
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				m_Buf.m_aVerts[v] = Vector(vx * m_fFine, p.m_aBase[v] + p.m_aSmooth[v], vz * m_fFine);
				m_Buf.m_aPlate[v] = Vector(vx * m_fFine, p.m_aBase[v] + p.m_aFloor[v] - m_fPlateGap, vz * m_fFine);
				m_Buf.m_aShell[v] = m_Buf.m_aVerts[v] - vector.Up * m_fBulletSlab;
			}
		}

		array<PhysicsGeom> geoms = {};
		array<ResourceName> mats = {};
		array<int> layers = {};
		// Terrain preset + Foliage: vehicle get in/out sweep skips Foliage (MUD_GroundEntity.c:1668-1676)
		int layer = EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage;
		for (int k = 0; k < p.m_aMats.Count(); k++)
		{
			int n = 0;
			for (int c = 0; c < PCELLS * PCELLS; c++)
			{
				if (m_Buf.m_aCellSlot[c] != k)
					continue;
				for (int j = 0; j < 6; j++)
				{
					m_Buf.m_aSplitIdx[n] = m_Buf.m_aIndices[c * 6 + j];
					n++;
				}
			}
			if (n == 0)
				continue;
			PhysicsGeom geom = PhysicsGeom.CreateTriMesh(m_Buf.m_aVerts, m_Buf.m_aSplitIdx, 1089, n);
			if (!geom)
			{
				Print("MUD world: CreateTriMesh failed", LogLevel.ERROR);
				continue;
			}
			geoms.Insert(geom);
			mats.Insert(p.m_aMats[k]);
			layers.Insert(layer);
		}

		// slab and plate: every non-kept cell
		int sn = 0;
		for (int sc = 0; sc < PCELLS * PCELLS; sc++)
		{
			if (m_Buf.m_aCellSlot[sc] < 0)
				continue;
			for (int sj = 0; sj < 6; sj++)
			{
				m_Buf.m_aSplitIdx[sn] = m_Buf.m_aShellIdx[sc * 6 + sj];
				sn++;
			}
		}
		if (m_fBulletSlab > 0 && sn > 0 && !geoms.IsEmpty())
		{
			PhysicsGeom shell = PhysicsGeom.CreateTriMesh(m_Buf.m_aShell, m_Buf.m_aSplitIdx, 1089, sn);
			if (shell)
			{
				geoms.Insert(shell);
				mats.Insert(p.m_aMats[0]);
				layers.Insert(EPhysicsLayerPresets.FireGeo);
			}
		}
		int pn = 0;
		for (int pc = 0; pc < PCELLS * PCELLS; pc++)
		{
			if (m_Buf.m_aCellSlot[pc] < 0)
				continue;
			for (int pj = 0; pj < 6; pj++)
			{
				m_Buf.m_aSplitIdx[pn] = m_Buf.m_aIndices[pc * 6 + pj];
				pn++;
			}
		}
		if (pn > 0 && !geoms.IsEmpty())
		{
			PhysicsGeom plate = PhysicsGeom.CreateTriMesh(m_Buf.m_aPlate, m_Buf.m_aSplitIdx, 1089, pn);
			if (plate)
			{
				geoms.Insert(plate);
				mats.Insert(m_GameMaterial);
				layers.Insert(layer);
			}
		}

		if (p.m_Phys)
		{
			p.m_Phys.Destroy();
			p.m_Phys = null;
		}
		if (!geoms.IsEmpty())
		{
			p.m_Phys = MakeStatic(p.m_Ent, geoms, mats, layers);
			if (!p.m_Phys)
				Print("MUD world: CreateStaticEx failed", LogLevel.ERROR);
		}
		p.m_bColDirty = false;
		m_iColBuilds++;
		m_iColMs += System.GetTickCount(t0);
	}

	//------------------------------------------------------------------------------------------------
	//! Static body from 1-6 geoms, one literal array per count (copy of MUD_GroundEntity.c:1234)
	protected Physics MakeStatic(IEntity ent, array<PhysicsGeom> g, array<ResourceName> m, array<int> l)
	{
		switch (g.Count())
		{
			case 1:
			{
				autoptr PhysicsGeomDef d1[] = {PhysicsGeomDef("", g[0], m[0], l[0])};
				return Physics.CreateStaticEx(ent, d1);
			}
			case 2:
			{
				autoptr PhysicsGeomDef d2[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1])};
				return Physics.CreateStaticEx(ent, d2);
			}
			case 3:
			{
				autoptr PhysicsGeomDef d3[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2])};
				return Physics.CreateStaticEx(ent, d3);
			}
			case 4:
			{
				autoptr PhysicsGeomDef d4[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3])};
				return Physics.CreateStaticEx(ent, d4);
			}
			case 5:
			{
				autoptr PhysicsGeomDef d5[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3]), PhysicsGeomDef("", g[4], m[4], l[4])};
				return Physics.CreateStaticEx(ent, d5);
			}
			case 6:
			{
				autoptr PhysicsGeomDef d6[] = {PhysicsGeomDef("", g[0], m[0], l[0]), PhysicsGeomDef("", g[1], m[1], l[1]), PhysicsGeomDef("", g[2], m[2], l[2]), PhysicsGeomDef("", g[3], m[3], l[3]), PhysicsGeomDef("", g[4], m[4], l[4]), PhysicsGeomDef("", g[5], m[5], l[5])};
				return Physics.CreateStaticEx(ent, d6);
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OursOnly(notnull IEntity e)
	{
		return MUD_Surface.IsOurs(e);
	}

	//------------------------------------------------------------------------------------------------
	//! Debug: 4 points inside a new patch's hole cells, traced (ENTS | WORLD); the hit must be this patch
	//! within 5 mm of the skin triangle height there
	protected void CheckNew(MUD_FinePatch p)
	{
		for (int i = 0; i < 4; i++)
		{
			int fx = Math.RandomInt(0, PCELLS);
			int fz = Math.RandomInt(0, PCELLS);
			if (p.m_aCell[(fz / FINE) * SKIN + fx / FINE] < 0)
				continue;
			float gfx = p.m_iPx * PCELLS + fx + 0.37;
			float gfz = p.m_iPz * PCELLS + fz + 0.61;
			bool ok;
			float want = BaseAt(gfx, gfz, ok);
			if (!ok)
				continue;
			m_iChkPts++;
			float wx = m_fX0 + gfx * m_fFine;
			float wz = m_fZ0 + gfz * m_fFine;
			m_Trace.Start = Vector(wx, want + 5, wz);
			m_Trace.End = Vector(wx, want - 5, wz);
			m_Trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
			m_Trace.LayerMask = EPhysicsLayerPresets.Projectile;
			m_Trace.TraceEnt = null;
			float frac = GetWorld().TraceMove(m_Trace, OursOnly);
			if (frac >= 1 || m_Trace.TraceEnt != p.m_Ent)
				continue;
			m_iChkHit++;
			float err = Math.AbsFloat(want + 5 - 10 * frac - want);
			if (err < 0.005)
				m_iChkGood++;
			if (err > m_fChkWorst)
				m_fChkWorst = err;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! TRACEABLE off (hide) or back on for every fine patch; new patches follow while hidden.
	//! Called by MUD_GroundEntity.HideFromTraces (vehicle get in / out).
	static void SetTraceHidden(bool hide)
	{
		s_bHidden = hide;
		if (!s_Inst)
			return;
		foreach (int key, MUD_FinePatch p : s_Inst.m_mPatches)
		{
			if (!p.m_Ent)
				continue;
			if (hide)
			{
				p.m_bTraceHidden = (p.m_Ent.GetFlags() & EntityFlags.TRACEABLE) != 0;
				if (p.m_bTraceHidden)
					p.m_Ent.ClearFlags(EntityFlags.TRACEABLE, false);
			}
			else if (p.m_bTraceHidden)
			{
				p.m_Ent.SetFlags(EntityFlags.TRACEABLE, false);
				p.m_bTraceHidden = false;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every 10 s while something happened
	protected void Stats(float timeSlice)
	{
		m_fSinceStats += timeSlice;
		if (m_fSinceStats < 10)
			return;
		m_fSinceStats = 0;
		if (m_iMadeAhead + m_iMadeStamp + m_iFreed + m_iMeshBuilds + m_iColBuilds == 0)
			return;
		Print(string.Format("MUD world: last 10 s - made %1 ahead + %2 by stamps, freed %3, meshes %4 (%5 ms), colliders %6 (%7 ms), smoothing %8 ms, skin swaps %9 ms",
			m_iMadeAhead, m_iMadeStamp, m_iFreed, m_iMeshBuilds, m_iMeshMs, m_iColBuilds, m_iColMs, m_iSmoothMs, m_iSwapMs));
		Print(string.Format("MUD world: patches %1 (deformed %2), mem +%3 KB since ready, worst frame %4 ms, skips (height) %5 | check new: points %6, hit %7, within 5 mm %8, worst %9 m",
			m_mPatches.Count(), m_iDeformed, System.MemoryAllocationKB() - m_iMemStartKB, m_iWorstMs, m_iSkipHeight, m_iChkPts, m_iChkHit, m_iChkGood, m_fChkWorst));
		m_iMadeAhead = 0;
		m_iMadeStamp = 0;
		m_iFreed = 0;
		m_iMeshBuilds = 0;
		m_iMeshMs = 0;
		m_iColBuilds = 0;
		m_iColMs = 0;
		m_iSmoothMs = 0;
		m_iSwapMs = 0;
		m_iWorstMs = 0;
	}
}
```

Notes for the implementer: `p.m_aMats.Insert` returns the new index (as `MUD_GroundEntity.c:1083` uses it). `Math.Floor` assigned to `int` truncates an integral float, as `MUD_GroundEntity.c:1876`. `Ahead` passes `Math.Max(pza, 0)` only as a loop bound compared with ints (not as an index). Do not add a `?:` anywhere (Enforce has none).

- [ ] **Step 4: Hooks in existing files**

1. `MUD_Surface.c`, `IsOurs` becomes:

```c
	static bool IsOurs(IEntity e)
	{
		return MUD_CoarseChunkEntity.Cast(e) != null || MUD_FinePatchEntity.Cast(e) != null;
	}
```

2. `MUD_BuildingDestructionFix.c` line `if (MUD_GroundEntity.IsMudPatch(entity) || MUD_CoarseChunkEntity.Cast(entity))` becomes:

```c
		if (MUD_GroundEntity.IsMudPatch(entity) || MUD_CoarseChunkEntity.Cast(entity) || MUD_FinePatchEntity.Cast(entity))
```

3. `MUD_GroundEntity.c`: in `HideFromTraces`, after the closing brace of the `foreach (MUD_GroundEntity g : s_aAll)` loop (end of the method), add:

```c
		// plan 3: the world ground's fine patches lie under parked vehicles too
		MUD_WorldGroundEntity.SetTraceHidden(hide);
```

   If Task 1 row R6 FAILED, also add right below it (and write in the LEDGER that R6 picked it):

```c
		MUD_CoarseSkinEntity.SetTraceHidden(hide);
```

   After the method add:

```c
	//------------------------------------------------------------------------------------------------
	//! New play session on a map without MUD_GroundEntity (MUD_WorldGroundEntity.Setup): statics outlive
	//! the session, its call queue (MUD_VehicleAccess.Tick) does not
	static void ResetTraceHide()
	{
		s_iTraceHideDepth = 0;
		MUD_VehicleAccess.Reset();
	}
```

4. `MUD_VehicleAccess.c`, replace `OnMud` (`:80-89`) with:

```c
	static bool OnMud(IEntity vehicleOwner, IEntity user)
	{
		if (vehicleOwner)
		{
			IEntity root = vehicleOwner.GetRootParent();
			if (root && (MUD_GroundEntity.IsOnAnyGround(root.GetOrigin()) || OnWorldGround(root)))
				return true;
		}
		return user && (MUD_GroundEntity.IsOnAnyGround(user.GetOrigin()) || OnWorldGround(user));
	}

	//------------------------------------------------------------------------------------------------
	//! Plan 3: over a hole cell of the holed map (skin or fine patch under it)
	static bool OnWorldGround(IEntity e)
	{
		if (!MUD_WorldGroundEntity.GetInstance())
			return false;
		vector o = e.GetOrigin();
		return MUD_Surface.IsHole(e.GetWorld(), o[0], o[2]);
	}
```

- [ ] **Step 5: Compile**

Ask the user to compile `MUD_FinePatch.c`, `MUD_Wheels.c`, `MUD_WorldGround.c`, `MUD_Surface.c`, `MUD_BuildingDestructionFix.c`, `MUD_GroundEntity.c`, `MUD_VehicleAccess.c`. Expected: no script errors. A compile error in `modded class Vehicle` (e.g. EOnInit not overridable): stop, LEDGER line with the error, and register vehicles from `MUD_WorldGroundEntity.Ahead` by a 5 s `QueryEntitiesBySphere` around the player instead (ask the user first).

- [ ] **Step 6: Place the entity (world closed)**

Ask the user to close the Arland world. Append to `ADDON/Arlnd_Layers/default.layer`:

```
MUD_WorldGroundEntity {
 coords 1425 38.6 2190
}
```

Ask the user to reopen the world.

- [ ] **Step 7: Play: patches ahead, swap, free, get in / out**

Ask the user to start play and focus the game window, then: (a) get in the M923 (1246 / 2175) and drive off-road across the fields for about 60 s at 20-40 km/h, crossing at least 10 patch borders (8 m apart); (b) stop on a field and get out and in again; (c) stand still for 70 s; (d) look along the driven line: any crack, light line, flicker or step where the fine patches meet the skin or each other (yes / no, screenshot to `ADDON\docs\superpowers\results\<run date>-plan3-task4-seams.png`).

Expected log lines:

```
MUD world: ready, patch grid 512 x 512, fine 0.25 m, render 1, groove 1, edge mode LOCKED, rpl 0
MUD world: last 10 s - made <a> ahead + 0 by stamps, freed <f>, meshes <m> (<ms> ms), colliders <c> (<ms> ms), smoothing 0 ms, skin swaps <ms> ms
MUD world: patches <n> (deformed 0), mem +<KB> KB since ready, worst frame <ms> ms, skips (height) 0 | check new: points <k>, hit <k>, within 5 mm <k>, worst <= 0.005 m
MUD vehicle access: mud hidden from traces while getting in/out
MUD vehicle access: done after <ms> ms, mud traceable again
```

Gate: `hit` = `points` and `within 5 mm` = `points` on every stats line (a fresh patch lies on the skin), freed > 0 within 70 s of stopping for patches beyond 16 m of the parked truck, get in / out works on the patch, no visible seam (d). A `hit` below `points` means the trace still sees the old skin body or the patch body was not built: add a probe line to `CheckNew` printing `m_Trace.TraceEnt` class and the skin `IsHoleCell` of that point, re-run, and rule in the LEDGER before any fix.

- [ ] **Step 8: Results file**

Write `ADDON/docs/superpowers/results/<run date>-plan3-task4-patches.md`: every `MUD world:` line from the play (whole lines), a table per stats line (made ahead, freed, meshes, ms per mesh = ms / meshes, colliders, ms per collider, skin swaps ms, patches, worst frame), the sum of `made` minus the sum of `freed` = last `patches` (checksum, write the sum out), the get in / out result and the seam answer + screenshot name.

- [ ] **Step 9: Adversarial review**

Run `engineering-skills:adversarial-reviewer` on `MUD_FinePatch.c`, `MUD_Wheels.c`, `MUD_WorldGround.c` and the four hooks. Rule on every finding in the LEDGER.

---

### Task 5: Ruts (RUTS FIRST), tested in play

**Files:**
- Modify: `ADDON/Scripts/Game/Mud/MUD_WorldGround.c`
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task5-ruts.md`

**Interfaces:**
- Consumes: Task 4 (`EnsureRange`, `At`, `SetH`, `VertexSoft`, `MarkDirty`, `ToFine`, `m_Skin.IsHoleCell`, `m_Skin.CellShape`, `MUD_Wheels.GetAll`, `IsProxy`, `m_iSkipHeight`).
- Produces: `protected void Press(vector wp, float depth)`, `protected void Wheels(float timeSlice)`, `protected void SetTouch(int gx, int gz, bool berm, float t)`, `protected void SeamCheck()`; members `m_fRutStep`, `m_fRutMaxDepth`, `m_fRutBaseDepth`, `m_fRutDepthPerTon`, `m_fRutRadius`, `m_fRimCut`, `m_fBermRadius`, `m_fBermHeight`, `m_fPassGap`, `m_fPressHeight`. Task 6 and 7 stamps follow the same shape as `Press` (global fine range, `EnsureRange`, height check, per vertex `SetH`, one `MarkDirty`).

- [ ] **Step 1: Rut attributes**

In `MUD_WorldGround.c`, after the `m_iColliderSmooth` attribute, add (values and texts from `MUD_GroundEntity.c:165-192`):

```c
	[Attribute("0.05", UIWidgets.EditBox, "How far a wheel sinks the ground per pass (m)", category: "Ruts")]
	protected float m_fRutStep;

	[Attribute("0.35", UIWidgets.EditBox, "Deepest any rut gets below the undeformed surface (m), however heavy the vehicle", category: "Ruts")]
	protected float m_fRutMaxDepth;

	[Attribute("0.08", UIWidgets.EditBox, "Rut depth (m) = this + vehicle mass in tons x Rut Depth Per Ton, capped at Rut Max Depth", category: "Ruts")]
	protected float m_fRutBaseDepth;

	[Attribute("0.025", UIWidgets.EditBox, "Extra rut depth per ton of vehicle mass (m)", category: "Ruts")]
	protected float m_fRutDepthPerTon;

	[Attribute("0.5", UIWidgets.EditBox, "Rut radius around a wheel contact (m). Keep at least 2x the fine step (0.25 m).", category: "Ruts")]
	protected float m_fRutRadius;

	[Attribute("0.8", UIWidgets.EditBox, "Share of raised ground (crater rim, berm) a wheel plows away per pass, 0-1", category: "Ruts")]
	protected float m_fRimCut;

	[Attribute("0.9", UIWidgets.EditBox, "Outer radius of the berm pushed up beside a rut (m)", category: "Ruts")]
	protected float m_fBermRadius;

	[Attribute("0.08", UIWidgets.EditBox, "Max berm height above the undeformed surface (m)", category: "Ruts")]
	protected float m_fBermHeight;

	[Attribute("0.6", UIWidgets.EditBox, "A vertex must be free of wheels this long (s) before the next touch counts as a new pass", category: "Ruts")]
	protected float m_fPassGap;

	[Attribute("0.5", UIWidgets.EditBox, "A wheel contact more than this (m) above or below our ground is on something else (bridge, rubble, a truck bed): no rut", category: "Ruts")]
	protected float m_fPressHeight;
```

And with the other members:

```c
	protected float m_fSinceWheelLog;
	protected int m_iPresses;
```

- [ ] **Step 2: `SetTouch` and `Press`**

Add to `MUD_WorldGroundEntity` (after `SetH`):

```c
	//------------------------------------------------------------------------------------------------
	//! Pass timer of global fine vertex (gx, gz) in every patch holding it (a new copy made later
	//! starts at -100, which only allows one extra pass on that vertex)
	protected void SetTouch(int gx, int gz, bool berm, float t)
	{
		int pxa = gx / PCELLS;
		int pza = gz / PCELLS;
		for (int pz = pza; pz >= pza - 1; pz--)
		{
			int lz = gz - pz * PCELLS;
			if (pz < 0 || pz >= m_iNPZ || lz < 0 || lz > PCELLS)
				continue;
			for (int px = pxa; px >= pxa - 1; px--)
			{
				int lx = gx - px * PCELLS;
				if (px < 0 || px >= m_iNPX || lx < 0 || lx > PCELLS)
					continue;
				MUD_FinePatch p = m_mPatches.Get(pz * m_iNPX + px);
				if (!p)
					continue;
				if (berm)
					p.m_aBermTouch[lz * PVERTS + lx] = t;
				else
					p.m_aRutTouch[lz * PVERTS + lx] = t;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server (or no Rpl): one wheel contact at world wp. A vertex sinks by m_fRutStep once per pass
	//! (no wheel on it for m_fPassGap). Hard ground takes no rut and no berm; next to it both shrink with
	//! the softness; the locked border scales them too. Copy of MUD_GroundEntity.c:2293 Press on global
	//! fine verts.
	protected void Press(vector wp, float depth)
	{
		float fx;
		float fz;
		if (!ToFine(wp, fx, fz))
			return;
		// contact over kept terrain (road, water edge): the real terrain carries it
		int ccx = Math.Floor(fx / FINE);
		int ccz = Math.Floor(fz / FINE);
		if (!m_Skin.IsHoleCell(ccx, ccz))
			return;

		float rr = m_fBermRadius / m_fFine;
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		int gx0 = Math.ClampInt(Math.Floor(fx - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(fx + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(fz - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(fz + rr), 0, gzmax);

		// height check BEFORE making patches: a wheel on a bridge deck must not make any
		int cgx = Math.ClampInt(Math.Round(fx), 0, gmax);
		int cgz = Math.ClampInt(Math.Round(fz), 0, gzmax);
		int cv;
		MUD_FinePatch cp = At(cgx, cgz, cv);
		float surf;
		if (cp)
		{
			surf = cp.m_aBase[cv] + cp.m_aH[cv];
		}
		else
		{
			bool ok;
			surf = BaseAt(fx, fz, ok);
			if (!ok)
				return;
		}
		if (Math.AbsFloat(wp[1] - surf) > m_fPressHeight)
		{
			m_iSkipHeight++;
			return;
		}

		EnsureRange(gx0, gx1, gz0, gz1);
		m_iPresses++;

		bool changed;
		float bermSpan = m_fBermRadius - m_fRutRadius;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float soft = VertexSoft(gx, gz) * p.m_aLock[v];
				if (soft <= 0)
					continue;
				float dx = (gx - fx) * m_fFine;
				float dz = (gz - fz) * m_fFine;
				float d = Math.Sqrt(dx * dx + dz * dz);
				float h = p.m_aH[v];

				if (d < m_fRutRadius)
				{
					bool newPass = m_fTime - p.m_aRutTouch[v] > m_fPassGap;
					SetTouch(gx, gz, false, m_fTime);
					float cap = depth * soft;
					if (!newPass || h <= -cap)
						continue;
					float k = d / m_fRutRadius;
					float fall = 1 - k * k * 0.5;
					// raised loose soil (crater rim, berm, spoil) gets plowed: most of it goes in one pass
					float top = h;
					if (h > 0)
						top = h - h * m_fRimCut * fall;
					float y = Math.Max(top - m_fRutStep * fall, Math.Max(-cap, p.m_aFloor[v]));
					if (y < h)
					{
						SetH(gx, gz, y);
						changed = true;
					}
				}
				else if (d < m_fBermRadius)
				{
					bool newBerm = m_fTime - p.m_aBermTouch[v] > m_fPassGap;
					SetTouch(gx, gz, true, m_fTime);
					// ruts win over berms: never push up a vertex that is already below the surface
					float bermTop = m_fBermHeight * soft;
					if (!newBerm || h < -0.001 || h >= bermTop)
						continue;
					float t = 1 - (d - m_fRutRadius) / bermSpan;
					float y2 = Math.Min(h + m_fRutStep * 0.5 * t, bermTop);
					if (y2 > h)
					{
						SetH(gx, gz, y2);
						changed = true;
					}
				}
			}
		}
		if (changed)
			MarkDirty(gx0, gx1, gz0, gz1);
	}
```

- [ ] **Step 3: `Wheels` and the frame order**

Add:

```c
	//------------------------------------------------------------------------------------------------
	//! Server (or no Rpl): every wheel contact of every moving wheeled vehicle presses the ground.
	//! Parked vehicles are skipped (a parked wheel makes no new pass anyway, MUD_GroundEntity.c:2323).
	protected void Wheels(float timeSlice)
	{
		if (IsProxy())
			return;
		m_fSinceWheelLog += timeSlice;
		bool wheelLog = m_bDebug && m_fSinceWheelLog >= 1;
		foreach (Vehicle veh : MUD_Wheels.GetAll())
		{
			if (!veh)
				continue;
			VehicleWheeledSimulation sim = VehicleWheeledSimulation.Cast(veh.FindComponent(VehicleWheeledSimulation));
			if (!sim || !sim.IsValid())
				continue;
			Physics phys = veh.GetPhysics();
			if (!phys || phys.GetVelocity().Length() < 0.2)
				continue;
			// heavier vehicle, deeper rut (MUD_GroundEntity.c:2192-2197)
			float mass = phys.GetMass();
			float depth = Math.Min(m_fRutBaseDepth + mass * 0.001 * m_fRutDepthPerTon, m_fRutMaxDepth);
			int count = sim.WheelCount();
			for (int w = 0; w < count; w++)
			{
				if (!sim.WheelHasContact(w))
					continue;
				vector cp = sim.WheelGetContactPosition(w);
				Press(cp, depth);
				if (wheelLog && w == 0)
				{
					GameMaterial touch = sim.WheelGetContactMaterial(w);
					ResourceName touchName;
					if (touch)
						touchName = touch.GetResourceName();
					Print(string.Format("MUD wheel: touches %1 | hole %2 | mass %3 kg, rut depth %4 m | at %5", touchName, MUD_Surface.IsHole(GetWorld(), cp[0], cp[2]), mass, depth, cp));
					m_fSinceWheelLog = 0;
				}
			}
		}
	}
```

Replace the body of `EOnFrame` after `m_fTime += timeSlice;` so the order is: wheels (stamps make their patches at once), then ahead, then builds:

```c
		m_fTime += timeSlice;
		Wheels(timeSlice);
		Ahead();
		Rebuild();
		FreeIdle(timeSlice);
```

- [ ] **Step 4: `SeamCheck` (Review Focus 1)**

Add, and call `SeamCheck();` at the end of `Stats` right after the second `Print` (inside the 10 s branch):

```c
	//------------------------------------------------------------------------------------------------
	//! Debug: every vertex two patches share must have the same world height in both (pairs right and
	//! up of each patch), and a deformed vertex on a patch edge must not face the skin (a missing
	//! neighbour that has hole cells): either is a visible step
	protected void SeamCheck()
	{
		int pairs;
		int shared;
		int bad;
		float worst;
		int open;
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			for (int side = 0; side < 4; side++)
			{
				int nx = p.m_iPx;
				int nz = p.m_iPz;
				if (side == 0)
					nx++;
				else if (side == 1)
					nz++;
				else if (side == 2)
					nx--;
				else
					nz--;
				MUD_FinePatch o;
				if (nx >= 0 && nz >= 0 && nx < m_iNPX && nz < m_iNPZ)
					o = m_mPatches.Get(nz * m_iNPX + nx);
				for (int i = 0; i < PVERTS; i++)
				{
					// vertex on this side, local in p and in o
					int vx = PCELLS;
					int vz = i;
					int ox = 0;
					int oz = i;
					if (side == 1)
					{
						vx = i;
						vz = PCELLS;
						ox = i;
						oz = 0;
					}
					else if (side == 2)
					{
						vx = 0;
						ox = PCELLS;
					}
					else if (side == 3)
					{
						vx = i;
						vz = 0;
						ox = i;
						oz = PCELLS;
					}
					int v = vz * PVERTS + vx;
					if (!o)
					{
						if (Math.AbsFloat(p.m_aH[v]) > 0.0005 && HasHole(nx, nz))
							open++;
						continue;
					}
					if (side >= 2)
						continue;   // pairs counted once, from the left / lower patch
					if (i == 0)
						pairs++;
					shared++;
					int w = oz * PVERTS + ox;
					float e = Math.AbsFloat(p.m_aBase[v] + p.m_aH[v] - o.m_aBase[w] - o.m_aH[w]);
					if (e > 0.001)
						bad++;
					if (e > worst)
						worst = e;
				}
			}
		}
		Print(string.Format("MUD world seams: pairs %1, shared verts %2, mismatched %3 (worst %4 m), deformed verts on a border to the coarse skin %5", pairs, shared, bad, worst, open));
	}
```

Also add to the second `Stats` line nothing; add one more line after it (9 argument limit):

```c
		Print(string.Format("MUD world: presses %1 in 10 s", m_iPresses));
		m_iPresses = 0;
```

- [ ] **Step 5: Compile**

Ask the user to compile `MUD_WorldGround.c`. Expected: no errors.

- [ ] **Step 6: Play: ruts (drive pattern)**

Ask the user to start play, focus the game window and drive the M923 off-road on a field next to the start (stay on fields, no road): (a) 50 m straight; (b) S-curves for 100 m so the wheels cross patch borders (8 m grid) at an angle; (c) turn round and drive back in the same ruts (second pass); (d) reverse 20 m. Then stop, get out and look at the ruts: depth roughly the truck's `rut depth` from the wheel line, berms beside them, ruts continuous over patch borders, groove material visible where deep.

Expected log lines:

```
MUD wheel: touches <dirt / grass gamemat> | hole 1 | mass <~9000-11000> kg, rut depth <0.30-0.35> m | at <pos>
MUD world: presses <n> in 10 s
MUD world: last 10 s - made <a> ahead + <s> by stamps, ...
MUD world: patches <n> (deformed <d>), ..., skips (height) 0 | check new: ...
MUD world seams: pairs <p>, shared verts <33 p>, mismatched 0 (worst <= 0.001 m), deformed verts on a border to the coarse skin 0
```

Gate: `deformed` > 0 after (a); ruts visible (user says yes, screenshot `<run date>-plan3-task5-ruts.png`); the truck still drives normally in its own ruts (no bounce, no sinking through). If a wheel line prints `hole 0` while the user is on a field, write it in the LEDGER with the position and ask the user to look at that spot (a kept cell inside a field means `mud_holes.py` kept it on purpose; no fix in this task).

- [ ] **Step 7: Seams (Review Focus 1)**

Still in play: drive a rut that ends exactly on the far side of a patch border, park 30 m away for 70 s (the undeformed patch beyond the rut end is freed, the deformed one stays), then drive back over that border into the freed area. Expected: every `MUD world seams` line `mismatched 0` and `deformed verts on a border to the coarse skin 0`; visually no step where the old rut meets the new patch. Any non-zero: probe first - add a TEMP line in `SeamCheck` printing the first bad pair (keys, local vertex, both heights), re-run, record in the LEDGER, then fix (the expected cause is `CopyNeighbourEdges` order; check it before touching anything else).

- [ ] **Step 8: Wheel on a raised object (Review Focus 2)**

Find a drivable prefab that lifts a truck off the ground (a ramp, a bridge piece, a flatbed) with enfusion `asset_search` (query `ramp`, then `bridge`) and give the user its path. Ask the user to place it in the World Editor on a field near the start (world closed for the layer write is done by the World Editor itself; the user saves), start play and drive onto it and stop with all wheels on it, then drive off. Expected: `skips (height)` rises while the wheels are on it (> 0 on that stats line), and the ground under the object shows no rut (user looks under / beside it). Then ask the user to delete the object again (or keep it, their call; LEDGER either way).

- [ ] **Step 9: Long drive and memory (Review Focus 4)**

Ask the user to drive ~2 km off-road in one go (fields and meadows, about 3 minutes at 40 km/h). Record every stats line. Expected: `deformed` grows with the drive, `mem` grows ~35-40 KB per deformed patch (write KB / deformed per line in the results), `worst frame` <= 10 ms. Then set `m_iBudget` to 5 on the entity in the World Editor, play, drive 30 m: expected one warning `MUD world: <n> deformed patches, past the budget 5 (~35 KB each, kept for the match)`; set it back to 2000.

Gate: `worst frame` <= 10 ms on every line. If above: probe before any change - split the line's ms into meshes / colliders / smoothing / skin swaps from the same stats line, write the split into the LEDGER, and ask the user whether to lower `m_iAheadPerFrame` to 1 or raise `m_fColliderInterval` (each is one attribute, no code).

- [ ] **Step 10: Results file**

Write `ADDON/docs/superpowers/results/<run date>-plan3-task5-ruts.md`: all `MUD wheel`, `MUD world`, `MUD world seams` lines (whole), a table per stats line (presses, made ahead / by stamps, freed, meshes + ms, colliders + ms, smoothing ms, skin swaps ms, patches, deformed, mem KB, KB per deformed, worst frame, skips), checksum line: sum of made minus sum of freed = last `patches` (sum written out), the raised-object result, the budget warning line, screenshots.

- [ ] **Step 11: Adversarial review**

Run `engineering-skills:adversarial-reviewer` on `MUD_WorldGround.c` (focus: `Press`, `SetTouch`, `SeamCheck`, the frame order). Rule on every finding in the LEDGER.

---

### Task 6: Craters, depth boxes, hard ground, floor and bullet slab

**Files:**
- Create: `ADDON/Scripts/Game/Mud/MUD_DepthBox.c`
- Modify: `ADDON/Scripts/Game/Mud/MUD_WorldGround.c`
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task6-craters.md`

**Interfaces:**
- Consumes: Task 4 (`EnsureRange`, `At`, `SetH`, `MarkDirty`, `ToFine`, `BaseAt`, `ApplyFloor`, `BeforeCollider`, `ComputeSoft`, `IsProxy`, `MAT_SLOTS`, `TLBR`, `SLOT`), Task 5 (`m_iPresses` stats line). Shipped: `SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect()` (used by `MUD_GroundEntity.c:461`), callback `(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)`, hit position `outMat[0]` (`MUD_GroundEntity.c:1756`).
- Produces: `class MUD_DepthBoxEntity : GenericEntity` with `static float DepthAt(vector wp, float def)`; `MUD_WorldGroundEntity.Crater(vector wp)`, `BreakHard(MUD_FinePatch p)`; members `m_fCraterRadius`, `m_fCraterDepth`, `m_fCraterRimHeight`, `m_iCraters`, `m_iCraterSkips`, `m_iMaxGeoms`.

- [ ] **Step 1: `MUD_DepthBox.c`**

Create `ADDON/Scripts/Game/Mud/MUD_DepthBox.c`:

```c
[EntityEditorProps(category: "Mud", description: "Mud plan 3: inside this box (XZ footprint, rotation counts) the world ground digs down to Max Depth instead of the ground's own Max Depth. Overlapping boxes: the deepest wins. Read when a fine patch is made.", color: "80 160 255 255")]
class MUD_DepthBoxEntityClass : GenericEntityClass
{
}

class MUD_DepthBoxEntity : GenericEntity
{
	[Attribute("8 4 8", UIWidgets.Coords, "Box size (m): X width, Y height (editor view only), Z length. Centred on the entity.")]
	protected vector m_vSize;

	[Attribute("8", UIWidgets.EditBox, "Max Depth inside the box (m below the undeformed surface)")]
	protected float m_fMaxDepth;

	protected static ref array<MUD_DepthBoxEntity> s_aAll = {};   // weak: a deleted box reads null

	//------------------------------------------------------------------------------------------------
	void MUD_DepthBoxEntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!GetGame().InPlayMode())
			return;
		for (int i = s_aAll.Count() - 1; i >= 0; i--)
		{
			if (!s_aAll[i])
				s_aAll.Remove(i);
		}
		s_aAll.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Max Depth at world wp: the deepest box holding wp, else def
	static float DepthAt(vector wp, float def)
	{
		float best = -1;
		foreach (MUD_DepthBoxEntity b : s_aAll)
		{
			if (!b)
				continue;
			vector lp = b.CoordToLocal(wp);
			if (Math.AbsFloat(lp[0]) > b.m_vSize[0] * 0.5 || Math.AbsFloat(lp[2]) > b.m_vSize[2] * 0.5)
				continue;
			if (b.m_fMaxDepth > best)
				best = b.m_fMaxDepth;
		}
		if (best < 0)
			return def;
		return best;
	}

#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override int _WB_GetAfterWorldUpdateSpecs(IEntitySource src)
	{
		return EEntityFrameUpdateSpecs.CALL_WHEN_ENTITY_VISIBLE;
	}

	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{
		vector mins = m_vSize * -0.5;
		vector maxs = m_vSize * 0.5;
		Shape box = Shape.Create(ShapeType.BBOX, ARGB(255, 80, 160, 255), ShapeFlags.WIREFRAME | ShapeFlags.ONCE, mins, maxs);
		vector mat[4];
		GetWorldTransform(mat);
		box.SetMatrix(mat);
		super._WB_AfterWorldUpdate(timeSlice);
	}
#endif
}
```

(Same editor drawing as `MUD_CutBoxEntity.c`.)

- [ ] **Step 2: Floor from depth boxes**

In `MUD_WorldGround.c` replace `ApplyFloor` with:

```c
	//------------------------------------------------------------------------------------------------
	//! Floor of every vertex: -Max Depth, or a depth box's Max Depth where one holds the vertex
	protected void ApplyFloor(MUD_FinePatch p)
	{
		float wx0 = m_fX0 + p.m_iPx * PCELLS * m_fFine;
		float wz0 = m_fZ0 + p.m_iPz * PCELLS * m_fFine;
		for (int vz = 0; vz < PVERTS; vz++)
		{
			for (int vx = 0; vx < PVERTS; vx++)
			{
				int v = vz * PVERTS + vx;
				p.m_aFloor[v] = -MUD_DepthBoxEntity.DepthAt(Vector(wx0 + vx * m_fFine, p.m_aBase[v], wz0 + vz * m_fFine), m_fMaxDepth);
			}
		}
	}
```

- [ ] **Step 3: Crater attributes, hook, `Crater`**

Attributes (after the rut attributes; values from `MUD_GroundEntity.c:194-201`):

```c
	[Attribute("2.5", UIWidgets.EditBox, "Crater radius (m)", category: "Craters")]
	protected float m_fCraterRadius;

	[Attribute("2", UIWidgets.EditBox, "Crater depth at the centre (m)", category: "Craters")]
	protected float m_fCraterDepth;

	[Attribute("0.3", UIWidgets.EditBox, "Crater rim height (m), rim reaches out to 1.5x radius", category: "Craters")]
	protected float m_fCraterRimHeight;
```

Members:

```c
	protected int m_iCraters;
	protected int m_iCraterSkips;
	protected int m_iMaxGeoms;
	protected bool m_bExplosionHooked;
```

At the end of `Setup()` before `return true;`:

```c
		SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Insert(OnExplosion);
		m_bExplosionHooked = true;
```

At the start of the destructor:

```c
		if (m_bExplosionHooked)
			SCR_ExplosionAmmoEffect.GetOnExplosionAmmoEffect().Remove(OnExplosion);
```

Methods:

```c
	//------------------------------------------------------------------------------------------------
	protected void OnExplosion(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)
	{
		// clients get craters from the server
		if (!m_bReady || IsProxy())
			return;
		Crater(outMat[0]);
	}

	//------------------------------------------------------------------------------------------------
	//! Bowl m_fCraterDepth deep at the centre (fitted into the floor there), raised rim out to 1.5x
	//! radius. Explosions crater hard ground too; the locked border and buildings scale it down.
	//! Copy of MUD_GroundEntity.c:1769 Crater on global fine verts.
	protected void Crater(vector wp)
	{
		float fx;
		float fz;
		if (!ToFine(wp, fx, fz))
			return;
		// centre over kept terrain (road): no crater (the rim would cut a step into the field beside it)
		if (!m_Skin.IsHoleCell(Math.Floor(fx / FINE), Math.Floor(fz / FINE)))
		{
			m_iCraterSkips++;
			return;
		}
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		int cgx = Math.ClampInt(Math.Round(fx), 0, gmax);
		int cgz = Math.ClampInt(Math.Round(fz), 0, gzmax);
		int cv;
		MUD_FinePatch cp = At(cgx, cgz, cv);
		float surf;
		if (cp)
		{
			surf = cp.m_aBase[cv] + cp.m_aH[cv];
		}
		else
		{
			bool ok;
			surf = BaseAt(fx, fz, ok);
			if (!ok)
				return;
		}
		// on a roof, a bridge, in the air: not the ground
		if (Math.AbsFloat(wp[1] - surf) > m_fCraterRadius)
		{
			m_iCraterSkips++;
			return;
		}

		float outer = m_fCraterRadius * 1.5;
		float rr = outer / m_fFine;
		int gx0 = Math.ClampInt(Math.Floor(fx - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(fx + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(fz - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(fz + rr), 0, gzmax);
		EnsureRange(gx0, gx1, gz0, gz1);

		cp = At(cgx, cgz, cv);
		if (!cp)
			return;
		float depth = Math.Min(m_fCraterDepth, -cp.m_aFloor[cv]);

		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float lock = p.m_aLock[v];
				if (lock <= 0)
					continue;
				float dx = (gx - fx) * m_fFine;
				float dz = (gz - fz) * m_fFine;
				float d = Math.Sqrt(dx * dx + dz * dz);
				float h = p.m_aH[v];
				if (d < m_fCraterRadius)
				{
					float k = d / m_fCraterRadius;
					float y = Math.Max(-depth * (1 - k * k) * lock, p.m_aFloor[v]);
					if (y < h)
						SetH(gx, gz, y);
				}
				else if (d < outer)
				{
					float t = 1 - (d - m_fCraterRadius) / (outer - m_fCraterRadius);
					float rim = m_fCraterRimHeight * t * t * lock;
					if (rim > h)
						SetH(gx, gz, rim);
				}
			}
		}
		MarkDirty(gx0, gx1, gz0, gz1);
		m_iCraters++;
		if (m_bDebug)
			Print(string.Format("MUD world: crater at %1, depth %2 m, floor at centre %3 m, centre now %4 m", wp, depth, cp.m_aFloor[cv], cp.m_aH[cv]));
	}
```

- [ ] **Step 4: Broken hard ground**

Replace the empty `BeforeCollider` with:

```c
	//------------------------------------------------------------------------------------------------
	protected void BeforeCollider(MUD_FinePatch p)
	{
		BreakHard(p);
	}

	//------------------------------------------------------------------------------------------------
	//! A hard cell (asphalt, concrete) whose ground moved more than 3 cm (a crater) turns into
	//! m_CraterMaterial and soft: from then on it takes ruts and the shovel (MUD_GroundEntity.c:1140).
	//! Same heights on every machine, so every machine breaks the same cells.
	protected void BreakHard(MUD_FinePatch p)
	{
		bool any;
		for (int c = 0; c < SKIN * SKIN; c++)
		{
			if (p.m_aCell[c] < 0 || p.m_aCellHard[c] == 0)
				continue;
			int lx0 = (c - (c / SKIN) * SKIN) * FINE;
			int lz0 = (c / SKIN) * FINE;
			bool broken;
			for (int vz = lz0; vz <= lz0 + FINE && !broken; vz++)
			{
				for (int vx = lx0; vx <= lx0 + FINE; vx++)
				{
					if (Math.AbsFloat(p.m_aH[vz * PVERTS + vx]) > 0.03)
					{
						broken = true;
						break;
					}
				}
			}
			if (!broken)
				continue;
			int slot = p.m_aMats.Find(m_CraterMaterial);
			if (slot < 0)
			{
				if (p.m_aMats.Count() < MAT_SLOTS)
					slot = p.m_aMats.Insert(m_CraterMaterial);
				else
					slot = 0;
			}
			int cell = slot;
			if ((p.m_aCell[c] & TLBR) != 0)
				cell = cell | TLBR;
			p.m_aCell[c] = cell;
			p.m_aCellHard[c] = 0;
			any = true;
		}
		if (!any)
			return;
		// this patch's softness and every neighbour's (their 3 x 3 reaches into this patch)
		for (int pz = p.m_iPz - 1; pz <= p.m_iPz + 1; pz++)
		{
			for (int px = p.m_iPx - 1; px <= p.m_iPx + 1; px++)
			{
				if (px < 0 || pz < 0 || px >= m_iNPX || pz >= m_iNPZ)
					continue;
				MUD_FinePatch n = m_mPatches.Get(pz * m_iNPX + px);
				if (n)
					ComputeSoft(n);
			}
		}
		if (m_bDebug)
			Print(string.Format("MUD world: hard ground broken in patch %1, surfaces %2", p.m_iKey, p.m_aMats.Count()));
	}
```

- [ ] **Step 5: Stats line with craters and collider pieces**

In `BuildCollider`, right after `if (!geoms.IsEmpty())` block's `MakeStatic` call, add:

```c
		if (geoms.Count() > m_iMaxGeoms)
			m_iMaxGeoms = geoms.Count();
```

In `Stats`, replace the Task 5 presses line and its reset with:

```c
		Print(string.Format("MUD world: presses %1, craters %2 (skipped %3), most collider pieces %4 in 10 s", m_iPresses, m_iCraters, m_iCraterSkips, m_iMaxGeoms));
		m_iPresses = 0;
		m_iCraters = 0;
		m_iCraterSkips = 0;
		m_iMaxGeoms = 0;
```

and in the `if (m_iMadeAhead + ... == 0) return;` early-out add `+ m_iCraters + m_iCraterSkips` so a crater alone prints the lines.

- [ ] **Step 6: Compile**

Ask the user to compile `MUD_DepthBox.c` and `MUD_WorldGround.c`. Expected: no errors.

- [ ] **Step 7: Play: craters on a field, at a road, by a house, on asphalt**

Ask the user to play as a soldier with grenades (or an RPG / mortar the user picks) and throw one each: (a) the middle of a field; (b) 1 m off a road edge on the field side; (c) 1 m from the house at 1260 / 2209; (d) on the road itself. Then walk into crater (a) to its bottom and back out; drive the M923 through crater (a).

Expected:

```
MUD world: crater at <pos a>, depth 2 m, floor at centre -4 m, centre now -2 m
MUD world: crater at <pos b>, depth 2 m, ..., centre now <shallower than -2 near the road: lock < 1>
MUD world: crater at <pos c>, ..., centre now <0 or shallow: building lock>
MUD world: presses <n>, craters <k> (skipped 1), most collider pieces <2..6> in 10 s
MUD world seams: pairs <p>, shared verts <s>, mismatched 0 (worst <= 0.001 m), deformed verts on a border to the coarse skin 0
```

Gate: (a) is a 2 m bowl with a rim, walkable and drivable in and out; (b) fades to 0 at the road edge (user says no step at the road); (c) no ground moved inside the house's box (no visible hole under the walls); (d) skipped (kept cell). A crater on hard ground that is a hole cell (an asphalt yard, if Task 1 / the user finds one): `MUD world: hard ground broken in patch <key>` and the truck leaves ruts in it afterwards.

- [ ] **Step 8: Depth box and bullet slab**

1. Ask the user to place a `MUD_DepthBoxEntity` (World Editor, create entity by class) on a field away from the start, size `8 4 8`, Max Depth `0.5`, save, play, and throw a grenade into its middle. Expected crater line: `depth 0.5 m, floor at centre -0.5 m, centre now -0.5 m`. Then change Max Depth to `8` and the ground's own `m_fMaxDepth` stays 4: the crater line shows `depth 2 m, floor at centre -8 m`.
2. Bullet slab: the user stands in crater (a) and fires a full magazine into the crater wall and rim at low angles, then walks around the rim looking for bullet holes / tracers exiting on the outside under the ground. Expected: bullets stop in the ground (impact decals on the wall, nothing comes out on the far side). If a bullet passes: probe first - the user notes where; add a TEMP debug trace (`TraceFlags.ENTS | TraceFlags.WORLD`, `EPhysicsLayerPresets.Projectile`) along that line printing every hit entity class, write the result into the LEDGER, then decide (slab thickness attribute or slab off) with the user.

- [ ] **Step 9: Results file**

Write `ADDON/docs/superpowers/results/<run date>-plan3-task6-craters.md`: every crater / hard / seams / stats line, a table (case a-d + depth box 0.5 / 8: expected depth, logged depth, centre now, user's visual), checksum line: craters + skipped on the stats lines = grenades thrown (sum written out), bullet test result, screenshots.

- [ ] **Step 10: Adversarial review**

Run `engineering-skills:adversarial-reviewer` on `MUD_DepthBox.c` and `MUD_WorldGround.c` (focus: `Crater`, `BreakHard`, explosion hook life time, `ApplyFloor`). Rule on every finding in the LEDGER.

---

### Task 7: Trenches (shovel on the world ground)

> SUPERSEDED 2026-10-10 by docs/superpowers/plans/2026-10-09-mud-trench-plan-mode.md (trench plan mode, free dig/fill). Results: docs/superpowers/results/2026-10-09-trench-plan-mode.md.

**Files:**
- Modify: `ADDON/Scripts/Game/Mud/MUD_WorldGround.c`
- Modify: `ADDON/Scripts/Game/Mud/MUD_Dig.c:55` and `:243`
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task7-trenches.md`

**Interfaces:**
- Consumes: Task 4-6 helpers (`ToFine`, `At`, `BaseAt`, `EnsureRange`, `SetH`, `VertexSoft`, `MarkDirty`, `IsProxy`, `m_Skin.IsHoleCell`), shipped-in-addon `MUD_Dig.DigPoint(notnull IEntity c)`, `MUD_GroundEntity.DigAt(vector wp)`, `MUD_GroundEntity.IsOnAnyGround(vector wp)`.
- Produces: `static bool MUD_WorldGroundEntity.IsOver(vector wp)`, `static bool MUD_WorldGroundEntity.DigAt(vector wp)`, `protected bool Dig(vector wp)`; members `m_fDigRadius`, `m_fDigStep`, `m_fDigMaxDepth`, `m_fSpoilRadius`, `m_fSpoilHeight`, `m_iDigs`.

- [ ] **Step 1: Dig attributes**

After the crater attributes (values from `MUD_GroundEntity.c:209-222`):

```c
	[Attribute("0.45", UIWidgets.EditBox, "Shovel: radius of the hole one swing digs (m)", category: "Digging")]
	protected float m_fDigRadius;

	[Attribute("0.08", UIWidgets.EditBox, "Shovel: how deep one swing digs at the centre (m)", category: "Digging")]
	protected float m_fDigStep;

	[Attribute("1.5", UIWidgets.EditBox, "Shovel: deepest a trench goes below the undeformed surface (m); the floor (Max Depth, depth box) stops it first if shallower", category: "Digging")]
	protected float m_fDigMaxDepth;

	[Attribute("1.1", UIWidgets.EditBox, "Shovel: outer radius of the dug-out soil pile around the hole (m)", category: "Digging")]
	protected float m_fSpoilRadius;

	[Attribute("0.3", UIWidgets.EditBox, "Shovel: max height of the soil pile (m)", category: "Digging")]
	protected float m_fSpoilHeight;
```

Member: `protected int m_iDigs;`

- [ ] **Step 2: `IsOver`, `DigAt`, `Dig`**

```c
	//------------------------------------------------------------------------------------------------
	//! Any machine: wp is over a hole cell of the world ground (dig helper, MUD_Dig.UpdateHelper)
	static bool IsOver(vector wp)
	{
		if (!s_Inst || !s_Inst.m_bReady)
			return false;
		return MUD_Surface.IsHole(s_Inst.GetWorld(), wp[0], wp[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one shovel swing at world wp. False if not on the world ground or refused (hard, locked).
	static bool DigAt(vector wp)
	{
		if (!s_Inst || !s_Inst.m_bReady || s_Inst.IsProxy())
			return false;
		return s_Inst.Dig(wp);
	}

	//------------------------------------------------------------------------------------------------
	//! One swing: the hole sinks m_fDigStep at the centre, the soil goes on a low pile around it; a pile
	//! never lands in a dug vertex. Copy of MUD_GroundEntity.c:1817 Dig on global fine verts.
	protected bool Dig(vector wp)
	{
		float fx;
		float fz;
		if (!ToFine(wp, fx, fz))
			return false;
		if (!m_Skin.IsHoleCell(Math.Floor(fx / FINE), Math.Floor(fz / FINE)))
			return false;
		int gmax = m_iNPX * PCELLS;
		int gzmax = m_iNPZ * PCELLS;
		int cgx = Math.ClampInt(Math.Round(fx), 0, gmax);
		int cgz = Math.ClampInt(Math.Round(fz), 0, gzmax);
		int cv;
		MUD_FinePatch cp = At(cgx, cgz, cv);
		float surf;
		if (cp)
		{
			surf = cp.m_aBase[cv] + cp.m_aH[cv];
		}
		else
		{
			bool ok;
			surf = BaseAt(fx, fz, ok);
			if (!ok)
				return false;
		}
		// soldier not on this ground (on a roof above it, ...): ignore
		if (Math.AbsFloat(wp[1] - surf) > 1.5)
			return false;

		float rr = m_fSpoilRadius / m_fFine;
		int gx0 = Math.ClampInt(Math.Floor(fx - rr), 0, gmax);
		int gx1 = Math.ClampInt(Math.Ceil(fx + rr), 0, gmax);
		int gz0 = Math.ClampInt(Math.Floor(fz - rr), 0, gzmax);
		int gz1 = Math.ClampInt(Math.Ceil(fz + rr), 0, gzmax);
		EnsureRange(gx0, gx1, gz0, gz1);
		cp = At(cgx, cgz, cv);
		// no shovel on asphalt / concrete or a locked spot (a crater turns hard ground to dirt first)
		if (!cp || VertexSoft(cgx, cgz) <= 0 || cp.m_aLock[cv] <= 0)
			return false;

		float spoilSpan = m_fSpoilRadius - m_fDigRadius;
		for (int gz = gz0; gz <= gz1; gz++)
		{
			for (int gx = gx0; gx <= gx1; gx++)
			{
				int v;
				MUD_FinePatch p = At(gx, gz, v);
				if (!p)
					continue;
				float lock = p.m_aLock[v];
				if (lock <= 0 || VertexSoft(gx, gz) <= 0)
					continue;
				float dx = (gx - fx) * m_fFine;
				float dz = (gz - fz) * m_fFine;
				float d = Math.Sqrt(dx * dx + dz * dz);
				float h = p.m_aH[v];
				if (d < m_fDigRadius)
				{
					float k = d / m_fDigRadius;
					float floor = Math.Max(-m_fDigMaxDepth, p.m_aFloor[v]);
					// a pile in the way goes first, the way a wheel plows a berm
					float top = Math.Min(h, 0);
					float y = Math.Max(top - m_fDigStep * (1 - k * k * 0.5) * lock, floor);
					if (y < h)
						SetH(gx, gz, y);
				}
				else if (d < m_fSpoilRadius && spoilSpan > 0)
				{
					if (h < -0.001 || h >= m_fSpoilHeight)
						continue;
					float t = 1 - (d - m_fDigRadius) / spoilSpan;
					SetH(gx, gz, Math.Min(h + m_fDigStep * 0.6 * t * lock, m_fSpoilHeight));
				}
			}
		}
		MarkDirty(gx0, gx1, gz0, gz1);
		m_iDigs++;
		if (m_bDebug)
			Print(string.Format("MUD world: dig at %1, centre now %2 m", wp, cp.m_aH[cv]));
		return true;
	}
```

In `Stats`, the craters line becomes:

```c
		Print(string.Format("MUD world: presses %1, craters %2 (skipped %3), digs %4, most collider pieces %5 in 10 s", m_iPresses, m_iCraters, m_iCraterSkips, m_iDigs, m_iMaxGeoms));
```

with `m_iDigs = 0;` added to the resets and `+ m_iDigs` to the early-out sum.

- [ ] **Step 3: Hook the shovel**

`MUD_Dig.c:55`:

```c
		if (!MUD_GroundEntity.IsOnAnyGround(wp) && !MUD_WorldGroundEntity.IsOver(wp))
```

`MUD_Dig.c:243`:

```c
		vector dp = MUD_Dig.DigPoint(c);
		if (!MUD_GroundEntity.DigAt(dp) && !MUD_WorldGroundEntity.DigAt(dp))
			Print(string.Format("MUD dig: nothing dug at %1 (not mud, hard ground or locked)", dp));
```

- [ ] **Step 4: Compile**

Ask the user to compile `MUD_WorldGround.c` and `MUD_Dig.c`. Expected: no errors.

- [ ] **Step 5: Play: trench across a patch border, at a road, on a road**

Ask the user to play as a soldier with an entrenching tool (the shovel used in the Plan 1 tests) and: (a) on a field, dig a 6 m trench in a straight line (about 15 swings per spot, step forward 0.5 m) crossing at least one 8 m patch border; (b) dig 0.5 m from a road edge; (c) try to dig on the road; (d) jump into the trench (a) and walk along it; drive the M923 across it.

Expected:

```
MUD world: dig at <pos>, centre now <down to -1.5> m
MUD world: presses <n>, craters 0 (skipped 0), digs <k>, most collider pieces <n> in 10 s
MUD world seams: pairs <p>, shared verts <s>, mismatched 0 (worst <= 0.001 m), deformed verts on a border to the coarse skin 0
```

Gate: trench (a) reaches 1.5 m and does not go deeper, continuous over the patch border; (b) is shallow at the road edge (lock fade) with no step; (c) no Dig action shows on the road (the helper needs a hole cell; a `MUD dig: nothing dug` line there means the helper and the server disagree: LEDGER); (d) the soldier stands in it and climbs out; the truck crosses it.

- [ ] **Step 6: Results file and adversarial review**

Write `ADDON/docs/superpowers/results/<run date>-plan3-task7-trenches.md` (all dig / seams / stats lines, swing count per spot, deepest centre value, checksum: dig lines + `nothing dug` lines = swings the user made, sum written out). Run `engineering-skills:adversarial-reviewer` on `MUD_WorldGround.c` (`Dig`, `IsOver`, `DigAt`) and `MUD_Dig.c`. Rule on every finding in the LEDGER.

---

### Task 8: Edge Mode "Dirt wall"

> DROPPED 2026-10-10 01:34 (user, in play: walls dark and see-through). Code reverted; Edge Mode stays LOCKED.

**Files:**
- Modify: `ADDON/Scripts/Game/Mud/MUD_FinePatch.c`
- Modify: `ADDON/Scripts/Game/Mud/MUD_WorldGround.c`
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task8-walls.md`

**Interfaces:**
- Consumes: Task 4 (`m_eEdgeMode`, `MUD_EEdgeMode.DIRT_WALL`, `EdgeLock` returns 1 in this mode, `BuildMesh`, `BuildCollider`, `MakeStatic`, `FreePatch`, `SetTraceHidden`, `m_Skin.IsHoleCell`), Task 7 (`Dig`) and Task 6 (`Crater`) to make edges move.
- Produces: `class MUD_WallBuffers` (`SEGS 512`), `MUD_FinePatch.m_Wall`, `m_WallRes`, `m_WallPhys`, `bool m_bWallTrace`; member `m_WallBuf`; `MUD_WorldGroundEntity.FillWall(MUD_FinePatch p, bool smooth, out float outTall)` returning the segment count, `BuildWallMesh(MUD_FinePatch p)`, `BuildWallCollider(MUD_FinePatch p)`, `DropWall(MUD_FinePatch p)`.

Where a hole cell meets a kept cell (road, water edge, map edge) the fine patch's edge verts move in this mode (no lock), so the real terrain edge stands above a dug surface (or under a berm). The wall is a vertical strip on every fine edge on that boundary, from the undeformed height (the terrain there) to the deformed height, drawn on both sides with the groove material, with its own collider. Each boundary edge belongs to the patch of its hole cell, so no wall is built twice.

- [ ] **Step 1: Patch members and buffers**

In `MUD_FinePatch.c`, add to `MUD_FinePatch` after `m_Phys`:

```c
	IEntity m_Wall;             // dirt wall (Edge Mode "Dirt wall"), own entity; null = no wall
	ref Resource m_WallRes;
	Physics m_WallPhys;
	bool m_bWallTrace;          // m_Wall TRACEABLE cleared by SetTraceHidden, to restore
```

At the end of the file:

```c
//! Dirt wall scratch: fixed 512 segments (16 hole cells x 4 sides x 8 fine edges at most), unused ones
//! collapsed onto the first vertex (MeshObject takes fixed-size arrays). ~70 KB.
class MUD_WallBuffers
{
	static const int SEGS = 512;
	vector m_aVerts[2048];      // 4 per segment: a bottom, a top, b top, b bottom
	float m_aUVs[4096];
	int m_aIdx[6144];           // both sides, 12 per segment
	int m_aColIdx[3072];        // one side, 6 per segment
}
```

- [ ] **Step 2: Wall build in `MUD_WorldGround.c`**

Member: `protected ref MUD_WallBuffers m_WallBuf;` and in `Setup()` after `m_Buf = new MUD_FineBuffers();`:

```c
		if (m_eEdgeMode == MUD_EEdgeMode.DIRT_WALL)
			m_WallBuf = new MUD_WallBuffers();
```

Methods:

```c
	//------------------------------------------------------------------------------------------------
	//! Wall segments of patch p into m_WallBuf (top = base + h, or + smoothed h for the collider).
	//! Returns the number of segments; tallest goes to outTall.
	protected int FillWall(MUD_FinePatch p, bool smooth, out float outTall)
	{
		int n;
		outTall = 0;
		for (int c = 0; c < SKIN * SKIN; c++)
		{
			if (p.m_aCell[c] < 0)
				continue;
			int lx = c - (c / SKIN) * SKIN;
			int lz = c / SKIN;
			int cx = p.m_iPx * SKIN + lx;
			int cz = p.m_iPz * SKIN + lz;
			for (int side = 0; side < 4; side++)
			{
				// side 0 left (x = lx*8), 1 right, 2 down (z = lz*8), 3 up
				int nx = cx;
				int nz = cz;
				if (side == 0)
					nx--;
				else if (side == 1)
					nx++;
				else if (side == 2)
					nz--;
				else
					nz++;
				if (m_Skin.IsHoleCell(nx, nz))
					continue;
				for (int i = 0; i < FINE; i++)
				{
					int ax = lx * FINE;
					int az = lz * FINE + i;
					int bx = ax;
					int bz = az + 1;
					if (side == 1)
					{
						ax += FINE;
						bx = ax;
					}
					else if (side >= 2)
					{
						ax = lx * FINE + i;
						bx = ax + 1;
						az = lz * FINE;
						if (side == 3)
							az += FINE;
						bz = az;
					}
					int a = az * PVERTS + ax;
					int b = bz * PVERTS + bx;
					float ha = p.m_aH[a];
					float hb = p.m_aH[b];
					if (smooth)
					{
						ha = p.m_aSmooth[a];
						hb = p.m_aSmooth[b];
					}
					float tall = Math.Max(Math.AbsFloat(ha), Math.AbsFloat(hb));
					if (tall <= 0.001 || n >= MUD_WallBuffers.SEGS)
						continue;
					if (tall > outTall)
						outTall = tall;
					int w = n * 4;
					m_WallBuf.m_aVerts[w] = Vector(ax * m_fFine, p.m_aBase[a], az * m_fFine);
					m_WallBuf.m_aVerts[w + 1] = Vector(ax * m_fFine, p.m_aBase[a] + ha, az * m_fFine);
					m_WallBuf.m_aVerts[w + 2] = Vector(bx * m_fFine, p.m_aBase[b] + hb, bz * m_fFine);
					m_WallBuf.m_aVerts[w + 3] = Vector(bx * m_fFine, p.m_aBase[b], bz * m_fFine);
					float va = Math.Min(Math.AbsFloat(ha) / 4, 1);
					float vb = Math.Min(Math.AbsFloat(hb) / 4, 1);
					m_WallBuf.m_aUVs[w * 2] = 0;
					m_WallBuf.m_aUVs[w * 2 + 1] = 0;
					m_WallBuf.m_aUVs[w * 2 + 2] = 0;
					m_WallBuf.m_aUVs[w * 2 + 3] = va;
					m_WallBuf.m_aUVs[w * 2 + 4] = 1;
					m_WallBuf.m_aUVs[w * 2 + 5] = vb;
					m_WallBuf.m_aUVs[w * 2 + 6] = 1;
					m_WallBuf.m_aUVs[w * 2 + 7] = 0;
					int k = n * 12;
					m_WallBuf.m_aIdx[k] = w;
					m_WallBuf.m_aIdx[k + 1] = w + 1;
					m_WallBuf.m_aIdx[k + 2] = w + 2;
					m_WallBuf.m_aIdx[k + 3] = w;
					m_WallBuf.m_aIdx[k + 4] = w + 2;
					m_WallBuf.m_aIdx[k + 5] = w + 3;
					m_WallBuf.m_aIdx[k + 6] = w;
					m_WallBuf.m_aIdx[k + 7] = w + 2;
					m_WallBuf.m_aIdx[k + 8] = w + 1;
					m_WallBuf.m_aIdx[k + 9] = w;
					m_WallBuf.m_aIdx[k + 10] = w + 3;
					m_WallBuf.m_aIdx[k + 11] = w + 2;
					for (int j = 0; j < 6; j++)
						m_WallBuf.m_aColIdx[n * 6 + j] = m_WallBuf.m_aIdx[k + j];
					n++;
				}
			}
		}
		if (n == 0)
			return 0;
		// unused segments: collapsed onto vertex 0 (zero area, inside the bounds)
		for (int f = n * 4; f < MUD_WallBuffers.SEGS * 4; f++)
		{
			m_WallBuf.m_aVerts[f] = m_WallBuf.m_aVerts[0];
			m_WallBuf.m_aUVs[f * 2] = 0;
			m_WallBuf.m_aUVs[f * 2 + 1] = 0;
		}
		for (int fi = n * 12; fi < MUD_WallBuffers.SEGS * 12; fi++)
			m_WallBuf.m_aIdx[fi] = 0;
		return n;
	}

	//------------------------------------------------------------------------------------------------
	protected bool EnsureWallEnt(MUD_FinePatch p)
	{
		if (p.m_Wall)
			return true;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = p.m_Ent.GetOrigin();
		p.m_Wall = GetGame().SpawnEntity(MUD_FinePatchEntity, GetWorld(), params);
		if (!p.m_Wall)
			return false;
		if (s_bHidden)
		{
			p.m_Wall.ClearFlags(EntityFlags.TRACEABLE, false);
			p.m_bWallTrace = true;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void DropWall(MUD_FinePatch p)
	{
		if (p.m_WallPhys)
		{
			p.m_WallPhys.Destroy();
			p.m_WallPhys = null;
		}
		if (p.m_Wall)
			delete p.m_Wall;
		p.m_WallRes = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildWallMesh(MUD_FinePatch p)
	{
		float tall;
		int n = FillWall(p, false, tall);
		if (n == 0)
		{
			DropWall(p);    // flat border again: wall and its collider go
			return;
		}
		if (!EnsureWallEnt(p))
			return;
		ResourceName mat = m_RutMaterial;
		if (mat.IsEmpty())
			mat = m_Material;
		int numVerts[] = {2048};
		int numIndices[] = {6144};
		string materials[] = {mat};
		Resource res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
		if (!res || !res.GetResource())
		{
			Print(string.Format("MUD world: wall mesh failed - material '%1' did not load?", mat), LogLevel.ERROR);
			return;
		}
		MeshObject mesh = res.GetResource().ToMeshObject();
		mesh.UpdateIndices(0, m_WallBuf.m_aIdx);
		mesh.UpdateVerts(0, m_WallBuf.m_aVerts, m_WallBuf.m_aUVs);
		p.m_Wall.SetObject(mesh, "");
		p.m_Wall.Update();
		p.m_WallRes = res;
		m_iWallBuilds++;
		if (tall > m_fWallTallest)
			m_fWallTallest = tall;
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildWallCollider(MUD_FinePatch p)
	{
		float tall;
		int n = FillWall(p, true, tall);
		if (p.m_WallPhys)
		{
			p.m_WallPhys.Destroy();
			p.m_WallPhys = null;
		}
		if (n == 0)
		{
			DropWall(p);
			return;
		}
		if (!EnsureWallEnt(p))
			return;
		PhysicsGeom geom = PhysicsGeom.CreateTriMesh(m_WallBuf.m_aVerts, m_WallBuf.m_aColIdx, 2048, n * 6);
		if (!geom)
			return;
		array<PhysicsGeom> g = {geom};
		array<ResourceName> m = {m_GameMaterial};
		array<int> l = {EPhysicsLayerPresets.Terrain | EPhysicsLayerDefs.Foliage};
		p.m_WallPhys = MakeStatic(p.m_Wall, g, m, l);
	}
```

Members: `protected int m_iWallBuilds;` and `protected float m_fWallTallest;`.

Hook calls:
- end of `BuildMesh` (after `m_iMeshMs += ...`): `if (m_WallBuf) BuildWallMesh(p);`
- end of `BuildCollider` (after `m_iColMs += ...`): `if (m_WallBuf) BuildWallCollider(p);`
- `FreePatch` and the destructor loop, before `delete p.m_Ent`: `DropWall(p);`
- `SetTraceHidden`, inside the loop after the patch entity branch:

```c
			if (p.m_Wall)
			{
				if (hide)
				{
					p.m_bWallTrace = (p.m_Wall.GetFlags() & EntityFlags.TRACEABLE) != 0;
					if (p.m_bWallTrace)
						p.m_Wall.ClearFlags(EntityFlags.TRACEABLE, false);
				}
				else if (p.m_bWallTrace)
				{
					p.m_Wall.SetFlags(EntityFlags.TRACEABLE, false);
					p.m_bWallTrace = false;
				}
			}
```

(`MUD_Surface.IsOurs` and the ruin fix already know the wall: it is a `MUD_FinePatchEntity`.)

Stats: append to the presses / craters line a fourth Print after it:

```c
		if (m_WallBuf)
			Print(string.Format("MUD world: walls built %1, tallest %2 m in 10 s", m_iWallBuilds, m_fWallTallest));
		m_iWallBuilds = 0;
		m_fWallTallest = 0;
```

- [ ] **Step 3: Compile**

Ask the user to compile `MUD_FinePatch.c` and `MUD_WorldGround.c`. Expected: no errors.

- [ ] **Step 4: Play in Dirt wall mode**

Ask the user to set `Edge Mode` to `Dirt wall` on the `MUD_WorldGroundEntity` in the World Editor (save), play and: (a) dig a trench up to and along a road edge (10 swings per spot); (b) throw a grenade 1 m off the road edge; (c) stand in the trench at the road edge and try to walk / crawl under the road; (d) look at the edge from the trench and from the road, also from 50 m.

Expected:

```
MUD world: ready, ..., edge mode DIRT_WALL, rpl <0|1>
MUD world: dig at <pos>, centre now <...> m
MUD world: walls built <n>, tallest <~1.5> m in 10 s
MUD world seams: ..., mismatched 0 ..., deformed verts on a border to the coarse skin 0
```

Gate: the trench reaches the road edge at full depth; a brown wall closes the gap from the trench to the road surface (no see-through, no sky under the road) from both sides; (c) the soldier cannot get under the road. Any see-through spot: screenshot + position, probe first (TEMP print of `FillWall` segment count and `IsHoleCell` of the kept neighbour for that patch), LEDGER, then fix.

- [ ] **Step 5: Back to the default and results**

Ask the user which Edge Mode stays the default (spec 4.3 leaves it to the map maker; `LOCKED` unless the user says otherwise) and set it. Write `ADDON/docs/superpowers/results/<run date>-plan3-task8-walls.md` (lines, screenshots, user's answers to a-d, checksum: walls built across stats lines = sum written out vs number of patches that had a wall at the end, counted by a TEMP debug print removed in this step).

- [ ] **Step 6: Adversarial review**

Run `engineering-skills:adversarial-reviewer` on `MUD_FinePatch.c` and `MUD_WorldGround.c` (focus: `FillWall` side / index math, filler, wall entity life time, trace hiding). Rule on every finding in the LEDGER.

---

### Task 9: Prefab and live sync (server-authoritative)

> Also sync MUD_TrenchSiteComponent: clients carve each piece to its replicated layer count (CarveTrench on proxies). Free dig / fill heights need the same sync. No per-player site cap (user 01:20). DEBUG_INSTANT must be false for release.

**Files:**
- Create: `ADDON/Prefabs/Mud/MudWorldGround.et`, `ADDON/Prefabs/Mud/MudWorldGround.et.meta`
- Modify: `ADDON/Arlnd_Layers/default.layer` (world closed)
- Modify: `ADDON/Scripts/Game/Mud/MUD_WorldGround.c`
- Create: `ADDON/Tools/compare_hashes.py`, `ADDON/Tools/test_compare_hashes.py`
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task9-sync.md`

**Interfaces:**
- Consumes: Task 4-8 (`MarkDirty`, `SetH`, `EnsurePatch`, `IsProxy`, `m_RplComponent`, `MUD_FinePatch.m_iNx0..m_iNz1`, `m_bNetQueued`, `m_fColliderInterval`).
- Produces (Task 10 uses them): `protected bool IsNet()`, `protected int ToMm(float h)`, `protected int Unpack(int packed, int half)`, `protected array<int> PackFull(MUD_FinePatch p)`, `protected void ApplyRect(int key, int rect, array<int> packed)`, `protected void OnLive(int key, int seq, int rect, array<int> packed)`, `protected void ReplayHeld(int key, int fromSeq)`, `class MUD_NetMsg { int m_iKey; int m_iSeq; int m_iRect; ref array<int> m_aPacked; }`; members `m_iSeq` (server: next message number), `m_aNetKeys`, `m_aHeld`, `ref set<int> m_sJipPending`, `ref map<int, int> m_mFullSeq`, counters `m_iNetMsgs`, `m_iNetInts`, `m_iNetGot`, `m_iNetBad`, `m_iJipHeld`, `m_iJipReplayed`, `m_iJipDropped`. Rect int = `x0 | x1 << 6 | z0 << 12 | z1 << 18`, patch-local vertex coords 0..32. Heights: mm, `ToMm` clamp -32768..32767, two per int (low half first), as `MUD_GroundEntity.c:1997-2057` but 16 bits.
- Hash lines (both machines): `MUD hash block <n> patches <k>`, then `k` lines `MUD hash <key> <s1> <s2>`, then `MUD hash end <n>`; `s1 = sum of mm`, `s2 = sum of mm * ((v % 31) + 1)` over the patch's 1089 verts, only patches with a non-zero mm.

- [ ] **Step 1: `compare_hashes.py` failing tests**

Create `ADDON/Tools/test_compare_hashes.py`:

```python
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
```

- [ ] **Step 2: Run, expect failure**

Run (from `ADDON\Tools`): `python -m pytest test_compare_hashes.py -q`
Expected: FAIL, `ModuleNotFoundError: No module named 'compare_hashes'`.

- [ ] **Step 3: `compare_hashes.py`**

Create `ADDON/Tools/compare_hashes.py`:

```python
"""Compare the last complete 'MUD hash' block of a server log and a client log (Plan 3 Task 9/10).

Usage: python compare_hashes.py <server console.log> <client console.log>
Exit 0 = same heights on both, 1 = differences, 2 = a log has no complete block.
"""
import re
import sys

BLOCK = re.compile(r"MUD hash block (\d+) patches (\d+)\s*$")
ENTRY = re.compile(r"MUD hash (-?\d+) (-?\d+) (-?\d+)\s*$")
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
```

- [ ] **Step 4: Run, expect pass**

Run (from `ADDON\Tools`): `python -m pytest test_compare_hashes.py -q`
Expected: `6 passed`. Then the whole tools suite: `python -m pytest -q` - expected `45 passed` from `test_mud_holes.py` + the Task 1 `test_surface_callers.py` count from the LEDGER + 6.

- [ ] **Step 5: Net code in `MUD_WorldGround.c`**

Above the entity class:

```c
//! One live height message held on a client (not ready yet, or its patch waits for a JIP full copy)
class MUD_NetMsg
{
	int m_iKey;
	int m_iSeq;
	int m_iRect;
	ref array<int> m_aPacked;
}
```

Attribute (category Debug):

```c
	[Attribute("0", UIWidgets.EditBox, "Print height hashes of all deformed patches every this many seconds (0 = off). Compare server and client logs with Tools/compare_hashes.py.", category: "Debug")]
	protected float m_fHashEvery;
```

Members:

```c
	protected int m_iSeq;                                       // server: number of the next live message
	protected ref array<int> m_aNetKeys = {};                   // server: patch keys with unsent changes
	protected ref array<ref MUD_NetMsg> m_aHeld = {};           // client: live messages not applied yet
	protected ref set<int> m_sJipPending = new set<int>();      // client: keys whose full copy is still coming
	protected ref map<int, int> m_mFullSeq = new map<int, int>();   // client: seq stamped on the full copy
	protected float m_fSinceNet;
	protected float m_fSinceHash;
	protected int m_iHashBlock;
	protected int m_iNetMsgs;
	protected int m_iNetInts;
	protected int m_iNetGot;
	protected int m_iNetBad;
	protected int m_iJipHeld;
	protected int m_iJipReplayed;
	protected int m_iJipDropped;
```

Methods:

```c
	//------------------------------------------------------------------------------------------------
	//! Server with a registered RplComponent in a networked session: changes go out. An unregistered
	//! item (prefab GUID missing from the resource DB) logs an error per Rpc, so it is skipped
	//! (MUD_GroundEntity.c:1916-1917).
	protected bool IsNet()
	{
		return m_RplComponent != null && !IsProxy() && RplSession.Mode() != RplMode.None && m_RplComponent.Id().IsValid();
	}

	//------------------------------------------------------------------------------------------------
	protected int ToMm(float h)
	{
		return Math.ClampInt(Math.Round(h * 1000), -32768, 32767);
	}

	//------------------------------------------------------------------------------------------------
	//! Signed 16-bit half of a packed int: 0 = low, 1 = high (MUD_GroundEntity.c:1985)
	protected int Unpack(int packed, int half)
	{
		int v = packed & 65535;
		if (half == 1)
			v = (packed >> 16) & 65535;
		if (v > 32767)
			v -= 65536;
		return v;
	}

	//------------------------------------------------------------------------------------------------
	//! Patch-local rect [x0..x1] x [z0..z1] of p in mm, two per int, low half first
	protected array<int> PackRect(MUD_FinePatch p, int x0, int x1, int z0, int z1)
	{
		// fresh array per message: Rpc may serialize later than this call
		array<int> pack = {};
		int pending;
		bool odd;
		for (int z = z0; z <= z1; z++)
		{
			for (int x = x0; x <= x1; x++)
			{
				int mm = ToMm(p.m_aH[z * PVERTS + x]) & 65535;
				if (!odd)
					pending = mm;
				else
					pack.Insert(pending | (mm << 16));
				odd = !odd;
			}
		}
		if (odd)
			pack.Insert(pending);
		return pack;
	}

	//------------------------------------------------------------------------------------------------
	protected array<int> PackFull(MUD_FinePatch p)
	{
		return PackRect(p, 0, PCELLS, 0, PCELLS);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, every m_fColliderInterval: one message per patch with unsent changes
	protected void SendChanges(float timeSlice)
	{
		m_fSinceNet += timeSlice;
		if (m_fSinceNet < m_fColliderInterval)
			return;
		m_fSinceNet = 0;
		foreach (int key : m_aNetKeys)
		{
			MUD_FinePatch p = m_mPatches.Get(key);
			if (!p)
				continue;
			p.m_bNetQueued = false;
			if (p.m_iNx0 < 0)
				continue;
			array<int> pack = PackRect(p, p.m_iNx0, p.m_iNx1, p.m_iNz0, p.m_iNz1);
			int rect = p.m_iNx0 | (p.m_iNx1 << 6) | (p.m_iNz0 << 12) | (p.m_iNz1 << 18);
			Rpc(RpcDo_Heights, key, m_iSeq, rect, pack);
			m_iSeq++;
			m_iNetMsgs++;
			m_iNetInts += pack.Count();
			p.m_iNx0 = -1;
		}
		m_aNetKeys.Clear();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Heights(int key, int seq, int rect, array<int> packed)
	{
		if (!packed)
			return;
		OnLive(key, seq, rect, packed);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: apply a live message now, or hold it (not ready, or the patch waits for its full copy).
	//! A message older than the patch's full copy is dropped: the full copy already has it.
	protected void OnLive(int key, int seq, int rect, array<int> packed)
	{
		if (!m_bReady || m_sJipPending.Contains(key))
		{
			MUD_NetMsg msg = new MUD_NetMsg();
			msg.m_iKey = key;
			msg.m_iSeq = seq;
			msg.m_iRect = rect;
			msg.m_aPacked = packed;
			m_aHeld.Insert(msg);
			if (m_sJipPending.Contains(key))
				m_iJipHeld++;
			return;
		}
		if (m_mFullSeq.Contains(key) && seq < m_mFullSeq.Get(key))
		{
			m_iJipDropped++;
			return;
		}
		ApplyRect(key, rect, packed);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: held messages (arrival order = server send order) for one key (key >= 0) or all keys
	//! not waiting for a full copy (key < 0); for a key with a full copy, only seq >= its stamp
	protected void ReplayHeld(int key, int fromSeq)
	{
		for (int i = 0; i < m_aHeld.Count(); )
		{
			MUD_NetMsg msg = m_aHeld[i];
			bool mine = msg.m_iKey == key || (key < 0 && !m_sJipPending.Contains(msg.m_iKey));
			if (!mine)
			{
				i++;
				continue;
			}
			m_aHeld.RemoveOrdered(i);
			int stamp = fromSeq;
			if (key < 0 && m_mFullSeq.Contains(msg.m_iKey))
				stamp = m_mFullSeq.Get(msg.m_iKey);
			if (msg.m_iSeq < stamp)
			{
				m_iJipDropped++;
				continue;
			}
			ApplyRect(msg.m_iKey, msg.m_iRect, msg.m_aPacked);
			m_iJipReplayed++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: write a patch-local rect of heights (mm) into the patch (made now if missing) and every
	//! copy of its edge verts
	protected void ApplyRect(int key, int rect, array<int> packed)
	{
		int x0 = rect & 63;
		int x1 = (rect >> 6) & 63;
		int z0 = (rect >> 12) & 63;
		int z1 = (rect >> 18) & 63;
		if (key < 0 || key >= m_iNPX * m_iNPZ || x1 > PCELLS || z1 > PCELLS || x0 > x1 || z0 > z1
			|| packed.Count() != ((x1 - x0 + 1) * (z1 - z0 + 1) + 1) / 2)
		{
			m_iNetBad++;
			return;
		}
		int px = key - (key / m_iNPX) * m_iNPX;
		int pz = key / m_iNPX;
		MUD_FinePatch p = EnsurePatch(px, pz, true);
		if (!p)
		{
			m_iNetBad++;
			return;
		}
		int gx0 = px * PCELLS;
		int gz0 = pz * PCELLS;
		int i;
		for (int z = z0; z <= z1; z++)
		{
			for (int x = x0; x <= x1; x++)
			{
				SetH(gx0 + x, gz0 + z, Unpack(packed[i / 2], i - (i / 2) * 2) * 0.001);
				i++;
			}
		}
		MarkDirty(gx0 + x0, gx0 + x1, gz0 + z0, gz0 + z1);
		m_iNetGot++;
	}

	//------------------------------------------------------------------------------------------------
	//! Debug, every m_fHashEvery s on every machine: one line per patch with a non-zero height
	protected void HashLines(float timeSlice)
	{
		if (m_fHashEvery <= 0)
			return;
		m_fSinceHash += timeSlice;
		if (m_fSinceHash < m_fHashEvery)
			return;
		m_fSinceHash = 0;
		array<string> lines = {};
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			int s1;
			int s2;
			bool any;
			for (int v = 0; v < 1089; v++)
			{
				int mm = ToMm(p.m_aH[v]);
				if (mm == 0)
					continue;
				any = true;
				s1 += mm;
				s2 += mm * ((v - (v / 31) * 31) + 1);
			}
			if (any)
				lines.Insert(string.Format("MUD hash %1 %2 %3", key, s1, s2));
		}
		m_iHashBlock++;
		Print(string.Format("MUD hash block %1 patches %2", m_iHashBlock, lines.Count()));
		foreach (string l : lines)
			Print(l);
		Print(string.Format("MUD hash end %1", m_iHashBlock));
	}
```

In `MarkDirty`, compute once before the loops:

```c
		bool net = IsNet();
```

and at the end of the inner loop body (after the smoothing-rect merge):

```c
				if (!net || !own)
					continue;
				// changed verts inside this patch (the patch range is widened by the blur)
				int nx0 = Math.ClampInt(gx0 - ox, 0, PCELLS);
				int nx1 = Math.ClampInt(gx1 - ox, 0, PCELLS);
				int nz0 = Math.ClampInt(gz0 - oz, 0, PCELLS);
				int nz1 = Math.ClampInt(gz1 - oz, 0, PCELLS);
				if (p.m_iNx0 < 0)
				{
					p.m_iNx0 = nx0;
					p.m_iNx1 = nx1;
					p.m_iNz0 = nz0;
					p.m_iNz1 = nz1;
				}
				else
				{
					p.m_iNx0 = Math.ClampInt(Math.Min(p.m_iNx0, nx0), 0, PCELLS);
					p.m_iNx1 = Math.ClampInt(Math.Max(p.m_iNx1, nx1), 0, PCELLS);
					p.m_iNz0 = Math.ClampInt(Math.Min(p.m_iNz0, nz0), 0, PCELLS);
					p.m_iNz1 = Math.ClampInt(Math.Max(p.m_iNz1, nz1), 0, PCELLS);
				}
				if (!p.m_bNetQueued)
				{
					p.m_bNetQueued = true;
					m_aNetKeys.Insert(p.m_iKey);
				}
```

In `EOnFrame` (Setup branch), right after `m_bReady = Setup();`:

```c
			if (m_bReady)
				ReplayHeld(-1, 0);   // messages that came before this machine was ready
```

and the frame body becomes:

```c
		m_fTime += timeSlice;
		Wheels(timeSlice);
		Ahead();
		Rebuild();
		FreeIdle(timeSlice);
		if (IsNet())
			SendChanges(timeSlice);
```

with `HashLines(timeSlice);` after `Stats(timeSlice);` (outside the `if (m_bDebug)`, it has its own switch).

In `Stats`, one more line (and its resets):

```c
		Print(string.Format("MUD world net: sent %1 msgs %2 ints, got %3 (bad %4), JIP held %5 replayed %6 dropped %7, seq %8", m_iNetMsgs, m_iNetInts, m_iNetGot, m_iNetBad, m_iJipHeld, m_iJipReplayed, m_iJipDropped, m_iSeq));
		m_iNetMsgs = 0;
		m_iNetInts = 0;
		m_iNetGot = 0;
```

and `+ m_iNetMsgs + m_iNetGot` in the early-out sum. In `Setup()` add a guard before `s_Inst = this;`:

```c
		if (m_iNPX * m_iNPZ > 262144)
			Print("MUD world: more than 262144 patches - JIP order keys overflow (Task 10)", LogLevel.ERROR);
```

- [ ] **Step 6: Compile**

Ask the user to compile `MUD_WorldGround.c`. Expected: no errors.

- [ ] **Step 7: Prefab (Workbench restart)**

Generate two IDs: `python -c "import secrets; print(secrets.token_hex(8).upper(), secrets.token_hex(8).upper())"` (A = entity ID, B = component GUID) and one more for the resource GUID C. Create `ADDON/Prefabs/Mud/MudWorldGround.et`:

```
MUD_WorldGroundEntity {
 ID "<A>"
 components {
  RplComponent "{<B>}" {
   Streamable Disabled
  }
 }
}
```

and `ADDON/Prefabs/Mud/MudWorldGround.et.meta`:

```
MetaFileClass {
 Name "{<C>}Prefabs/Mud/MudWorldGround.et"
 Configurations {
  EntityTemplateResourceClass PC {
  }
  XBOX_ONE : PC {
  }
  XBOX_SERIES : PC {
  }
  PS4 : PC {
  }
  PS5 : PC {
  }
  HEADLESS : PC {
  }
 }
}
```

(Same shape as `Prefabs/Mud/MudGround.et` and its meta.) Ask the user to close the world, then replace the Task 4 layer entry in `ADDON/Arlnd_Layers/default.layer` with:

```
MUD_WorldGroundEntity : "{<C>}Prefabs/Mud/MudWorldGround.et" {
 coords 1425 38.6 2190
}
```

Ask the user to restart Workbench (the resource database picks up a new prefab only then) and reopen the world. Expected at play: `MUD world: ready, ..., rpl 1` and no `RplComponent` / unregistered errors in the log.

- [ ] **Step 8: MP live test (no JIP yet)**

Ask the user to start a local dedicated server with this addon on Arland plus one game client, both in from the start (the user's usual MP setup; write the exact method and the three log folders into the LEDGER), set `m_fHashEvery` to 20 on the entity first. On the client: drive the M923 on a field 60 s (ruts), throw two grenades on the field, dig 10 swings. Then stop everything and wait 45 s.

Expected:
- server log: `MUD world net: sent <n> msgs <ints> ints, got 0 (bad 0), ..., seq <growing>`
- client log: `MUD world net: sent 0 msgs 0 ints, got <n> (bad 0), JIP held 0 replayed 0 dropped 0, seq 0`
- client: ruts / craters / trench visible as on the server's patches (the dedicated server draws nothing; the client sees what the server computed).
- `python Tools/compare_hashes.py <server console.log> <client console.log>` prints `missing 0, extra 0, mismatched 0`, exit 0.

Gate: compare clean and `bad 0`. A mismatch: probe first - TEMP print on both machines of the first mismatched key's 1089 mm (only that key), diff them, write the first differing vertex into the LEDGER, then fix.

- [ ] **Step 9: Results file and adversarial review**

Write `ADDON/docs/superpowers/results/<run date>-plan3-task9-sync.md`: every `MUD world net`, `MUD world:` stats line of both machines, the compare output, a table per stats line (msgs, ints, ints per msg), checksum: server `sent` msgs summed = client `got` summed (both sums written out). Run `engineering-skills:adversarial-reviewer` on `MUD_WorldGround.c` (focus: `MarkDirty` net rect, `SendChanges`, `OnLive`, `ReplayHeld`, `ApplyRect` bounds) and `compare_hashes.py`. Rule on every finding in the LEDGER.

---

### Task 10: JIP (key list + full copies nearest first)

> JIP: a joining client carves every live site's pieces to their current layer counts.

**Files:**
- Modify: `ADDON/Scripts/Game/Mud/MUD_WorldGround.c`
- Create: `ADDON/Scripts/Game/Mud/MUD_WorldGroundNet.c`
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task10-jip.md`

**Interfaces:**
- Consumes: Task 9 (`IsNet`, `PackFull`, `Unpack`, `OnLive`, `ReplayHeld`, `m_sJipPending`, `m_mFullSeq`, `m_iSeq`, counters), `SCR_PlayerController` RPC pattern of `MUD_Dig.c:216-244`.
- Produces: `MUD_WorldGroundEntity.QueueJip(int pid, IEntity near)`, `ApplyFull(int key, int seq, array<int> packed)`, `class MUD_JipItem`; `SCR_PlayerController.MUD_RequestJip()`, `MUD_SendFull(int key, int seq, array<int> packed)`.

- [ ] **Step 1: `MUD_WorldGroundNet.c`**

Create `ADDON/Scripts/Game/Mud/MUD_WorldGroundNet.c`:

```c
//! Mud plan 3 JIP: a joining client asks the server for the deformed patches (keys came in
//! MUD_WorldGroundEntity.RplLoad); the server sends each patch's full heights to that player only
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Owner client
	void MUD_RequestJip()
	{
		if (Replication.IsServer())
			MUD_RpcAsk_Jip();
		else
			Rpc(MUD_RpcAsk_Jip);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MUD_RpcAsk_Jip()
	{
		MUD_WorldGroundEntity g = MUD_WorldGroundEntity.GetInstance();
		if (g)
			g.QueueJip(GetPlayerId(), GetControlledEntity());
	}

	//------------------------------------------------------------------------------------------------
	//! Server
	void MUD_SendFull(int key, int seq, array<int> packed)
	{
		Rpc(MUD_RpcDo_Full, key, seq, packed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MUD_RpcDo_Full(int key, int seq, array<int> packed)
	{
		MUD_WorldGroundEntity g = MUD_WorldGroundEntity.GetInstance();
		if (g && packed)
			g.ApplyFull(key, seq, packed);
	}
}
```

- [ ] **Step 2: RplSave / RplLoad, queue, full copy**

In `MUD_WorldGround.c`, above the entity class:

```c
//! Server: one full patch copy owed to one joining player
class MUD_JipItem
{
	int m_iPid;
	int m_iKey;
	float m_fSendAt;
	int m_iSeq;                 // with m_aPacked: snapshot taken at queue time (delay test)
	ref array<int> m_aPacked;
}
```

Attributes (category Debug, marked TEMP, removed in step 9):

```c
	[Attribute("8", UIWidgets.EditBox, "JIP: full patch copies sent per frame (all joining players together)", category: "Patches")]
	protected int m_iJipPerFrame;

	// TEMP (Task 10 step 9): JIP order test - full copies snapshotted at request time, sent 3 s later
	[Attribute("0", UIWidgets.CheckBox, "TEMP: delay JIP full copies 3 s (order test)", category: "Debug")]
	protected bool m_bJipTestDelayFull;

	// TEMP (Task 10 step 9): RplSave size test - this many fake (negative) keys appended
	[Attribute("0", UIWidgets.EditBox, "TEMP: fake JIP keys appended to RplSave (size test)", category: "Debug")]
	protected int m_iJipFakeKeys;
```

Members:

```c
	protected ref array<ref MUD_JipItem> m_aJip = {};
	protected bool m_bJipNeeded;        // client: RplLoad came, full copies still to ask for / receive
	protected bool m_bJipAsked;
	protected float m_fJipStart;
	protected int m_iJipTotal;
	protected int m_iJipSent;
```

Methods:

```c
	//------------------------------------------------------------------------------------------------
	//! Joining client gets only the keys of the deformed patches; the heights follow per player
	override bool RplSave(ScriptBitWriter writer)
	{
		array<int> keys = {};
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			if (p.m_bDeformed)
				keys.Insert(key);
		}
		for (int f = 0; f < m_iJipFakeKeys; f++)
			keys.Insert(-1 - f);   // TEMP size test, ignored by RplLoad
		writer.WriteInt(keys.Count());
		foreach (int k : keys)
			writer.WriteInt(k);
		Print(string.Format("MUD world JIP: RplSave %1 keys (%2 fake), %3 bytes", keys.Count(), m_iJipFakeKeys, 4 + 4 * keys.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		int n;
		if (!reader.ReadInt(n) || n < 0 || n > 4000000)
			return false;
		int fake;
		for (int i = 0; i < n; i++)
		{
			int k;
			if (!reader.ReadInt(k))
				return false;
			if (k < 0)
				fake++;
			else
				m_sJipPending.Insert(k);
		}
		m_iJipTotal = m_sJipPending.Count();
		m_bJipNeeded = true;
		Print(string.Format("MUD world JIP: RplLoad %1 keys (%2 fake), %3 to fetch", n, fake, m_iJipTotal));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client, once ready and its player controller exists: ask the server for the full copies
	protected void AskJip()
	{
		if (!m_bJipNeeded || m_bJipAsked)
			return;
		if (m_sJipPending.IsEmpty())
		{
			m_bJipNeeded = false;
			Print("MUD world JIP: 0 patches to fetch");
			return;
		}
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;
		pc.MUD_RequestJip();
		m_bJipAsked = true;
		m_fJipStart = m_fTime;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: queue every deformed patch for player pid, nearest to `near` first
	void QueueJip(int pid, IEntity near)
	{
		if (IsProxy() || !m_bReady)
			return;
		vector o = vector.Zero;
		if (near)
			o = near.GetOrigin();
		float size = PCELLS * m_fFine;
		array<int> order = {};
		foreach (int key, MUD_FinePatch p : m_mPatches)
		{
			if (!p.m_bDeformed)
				continue;
			float dx = m_fX0 + (p.m_iPx + 0.5) * size - o[0];
			float dz = m_fZ0 + (p.m_iPz + 0.5) * size - o[2];
			int du = Math.ClampInt(Math.Sqrt(dx * dx + dz * dz) / size, 0, 4095);
			order.Insert((du << 18) | key);     // key < 262144 (Setup guard)
		}
		order.Sort();
		foreach (int e : order)
		{
			MUD_JipItem it = new MUD_JipItem();
			it.m_iPid = pid;
			it.m_iKey = e & 262143;
			it.m_fSendAt = m_fTime;
			if (m_bJipTestDelayFull)
			{
				it.m_fSendAt = m_fTime + 3;
				it.m_iSeq = m_iSeq;
				it.m_aPacked = PackFull(m_mPatches.Get(it.m_iKey));
			}
			m_aJip.Insert(it);
		}
		Print(string.Format("MUD world JIP: player %1 asked, %2 deformed patches queued (delay test %3)", pid, order.Count(), m_bJipTestDelayFull));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: up to m_iJipPerFrame full copies per frame, queue order
	protected void SendJip()
	{
		int sent;
		for (int i = 0; i < m_aJip.Count() && sent < m_iJipPerFrame; )
		{
			MUD_JipItem it = m_aJip[i];
			if (it.m_fSendAt > m_fTime)
			{
				i++;
				continue;
			}
			m_aJip.RemoveOrdered(i);
			SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(it.m_iPid));
			MUD_FinePatch p = m_mPatches.Get(it.m_iKey);
			if (!pc || !p)
				continue;      // player left / patch gone: nothing owed
			int seq = m_iSeq;
			array<int> packed = it.m_aPacked;
			if (packed)
				seq = it.m_iSeq;
			else
				packed = PackFull(p);
			// every live message numbered < seq is inside this copy; later ones are applied on top
			pc.MUD_SendFull(it.m_iKey, seq, packed);
			sent++;
			m_iJipSent++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a full patch copy stamped with seq; held live messages for it with seq >= stamp go on top
	void ApplyFull(int key, int seq, array<int> packed)
	{
		if (!m_sJipPending.Contains(key))
		{
			m_iJipDropped++;
			return;
		}
		m_sJipPending.Remove(m_sJipPending.Find(key));
		m_mFullSeq.Set(key, seq);
		ApplyRect(key, 0 | (PCELLS << 6) | (0 << 12) | (PCELLS << 18), packed);
		ReplayHeld(key, seq);
		if (m_sJipPending.IsEmpty() && m_bJipNeeded)
		{
			m_bJipNeeded = false;
			Print(string.Format("MUD world JIP: %1 patches in %2 s, held %3, replayed %4, dropped %5", m_iJipTotal, m_fTime - m_fJipStart, m_iJipHeld, m_iJipReplayed, m_iJipDropped));
		}
	}
```

Frame body (after `FreeIdle(timeSlice);`):

```c
		if (IsNet())
		{
			SendChanges(timeSlice);
			SendJip();
		}
		else if (IsProxy())
		{
			AskJip();
		}
```

`m_iJipSent` joins the net stats line as a second line (9-argument limit):

```c
		Print(string.Format("MUD world JIP: sent %1 full copies in 10 s, %2 queued, %3 pending here", m_iJipSent, m_aJip.Count(), m_sJipPending.Count()));
		m_iJipSent = 0;
```

- [ ] **Step 3: Compile**

Ask the user to compile `MUD_WorldGroundNet.c` and `MUD_WorldGround.c`. Expected: no errors.

- [ ] **Step 4: JIP, plain**

Same MP setup as Task 9 step 8 (dedicated server + client 1 from the start, `m_fHashEvery` 20). Client 1 makes ruts, two craters and a trench (as Task 9 step 8). Then client 2 joins. Expected:

- server: `MUD world JIP: RplSave <k> keys (0 fake), <4+4k> bytes`, `MUD world JIP: player <id> asked, <k> deformed patches queued (delay test 0)`, `MUD world JIP: sent <k> full copies ...`
- client 2: `MUD world JIP: RplLoad <k> keys (0 fake), <k> to fetch`, then `MUD world JIP: <k> patches in <t> s, held 0, replayed 0, dropped 0`
- client 2 sees the same ruts / craters / trench.
- after 45 s quiet: `compare_hashes.py` server vs client 2 and server vs client 1 both `missing 0, extra 0, mismatched 0`.

- [ ] **Step 5: JIP while the ground moves**

Client 2 leaves. Client 1 drives in circles over the deformed area (keeps sending live messages) while client 2 joins again. Expected on client 2: the done line, `held` and `replayed` may be > 0, `dropped` >= 0; after client 1 stops and 45 s pass, both compares clean.

- [ ] **Step 6: Forced order: live before the older full copy (Review Focus 3)**

Set `m_bJipTestDelayFull` on the entity (World Editor, save), restart the server. Client 1 drives over the deformed patches continuously; client 2 joins during it. Expected on client 2: `held` > 0 (live messages arrived first and were held), `replayed` > 0, done line present.

- [ ] **Step 7: Compare after the order test (Review Focus 3)**

Client 1 stops; wait 45 s; `python Tools/compare_hashes.py <server log> <client 2 log>`. Expected: `missing 0, extra 0, mismatched 0`. A mismatch: probe first (TEMP per-key mm dump as Task 9 step 8, plus a TEMP print in `ReplayHeld` of key / seq / stamp / applied-or-dropped), LEDGER, then fix.

- [ ] **Step 8: RplSave size (Review Focus 5)**

With `m_bJipTestDelayFull` off, set `m_iJipFakeKeys` to 1000, restart the server, client 2 joins: record the RplSave bytes line and whether the join works (client 2 in game, done line printed, RplLoad line shows `1000 fake`). Repeat with 10000 and 100000. Write a table: fake keys, bytes, join ok / failed, any error line from the server or client log (whole line). If a size fails: write the largest working size into the results and LEDGER as the known limit, and compute how many real deformed patches it allows (bytes / 4); a fix (keys split over several RPCs) becomes a follow-up only if that number is below 20000 (≈ the patches a full match would deform, Task 5 step 9 rate) - ask the user.

- [ ] **Step 9: Remove TEMP**

Remove the two TEMP attributes (`m_bJipTestDelayFull`, `m_iJipFakeKeys`), the fake-key loop in `RplSave` (and the `fake` count in its Print), and the delay branch in `QueueJip` (keep `it.m_aPacked` null always; `SendJip` keeps the `if (packed)` branch - harmless - or drop it too and use `PackFull` always; do the latter). Grep `TEMP` in `MUD_WorldGround.c`: expected no hits. Ask the user to compile. Re-run step 4 once: same expected lines.

- [ ] **Step 10: Results file, adversarial review, old entity question**

Write `ADDON/docs/superpowers/results/<run date>-plan3-task10-jip.md`: every JIP / net / hash compare line, the size table, checksum: server `sent ... full copies` summed per join = client 2 `<k> patches` of that join (both written out). Run `engineering-skills:adversarial-reviewer` on `MUD_WorldGround.c` and `MUD_WorldGroundNet.c` (focus: RplLoad guards, ordering / seq rule, player leaving mid-queue, ready-before-RplLoad and RplLoad-before-ready). Rule on every finding in the LEDGER.

Then ask the user: archive the old `MUD_GroundEntity` now? If yes, do not archive in this plan: grep every file that references `MUD_GroundEntity` (expected at least `MUD_Dig.c`, `MUD_VehicleAccess.c`, `MUD_BuildingDestructionFix.c`, `MUD_GroundEntity.c` itself, `MUD_RutTrailEntity.c` - grep, do not trust this list) and write them into the LEDGER as the scope of a follow-up plan.

---

### Task 11: Baked heights (skin loads them instead of tracing)

**Files:**
- Modify: `ADDON/Tools/mud_holes.py` (`fit`, `bake`), `ADDON/Tools/test_mud_holes.py`
- Modify: `ADDON/Scripts/Game/Mud/MUD_CoarseSkin.c`
- Create: `ADDON/Data/Mud/mud_heights.bin` (written by the tool), `ADDON/Data/Mud/mud_fit.json`
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task11-baked.md`

**Interfaces:**
- Consumes: `mud_holes.owner_tile(bx, bz)`, `ttile_holes.chunks`, `ttile_holes.decode`, the `DATA` tiles Plan 2 wrote (HGHT = 129 x 129 x 16-bit, 33282 bytes, as `fake_ttile` in `test_mud_holes.py`), Task 3 `SampleChunk`, `m_aRec`, `m_iChunksX`, Task 2 `MUD_Surface.Get().HeightmapY(world, x, z)`.
- Produces: `mud_holes.py` functions `hght_index(lx, lz, o)`, `hght_values(tile_bytes, signed)`, `vertex_local(bx, bz, v)`, `parse_samples(text)`, `linfit(us, hs)`, `fit(samples, load_tile)`, `bake(data_dir, fitd, out_path)`; CLI `fit <console.log> --data <DATA> --out <fit.json>` and `bake --data <DATA> --fit <fit.json> --out <bin>`. Bin format: bytes `MUDH`, int32 LE version 1, int32 block count, int32 32, then per block sorted by (bz, bx): int32 bx, int32 bz, 1089 float32 heights (no offset; vertex v = vz * 33 + vx). Skin attribute `m_sBaked`.

Spec: the baked file removes the 1089 vertex traces per holed chunk at load (the per-cell centre trace stays: it gives the diagonal and the game material). Heights come from the tiles' HGHT; the scale, offset and index order are fitted from 2000 traced skin samples, not guessed.

- [ ] **Step 1: TEMP sample lines in the skin**

In `MUD_CoarseSkin.c`, at the end of `Finish(string how)` (after the `Check("build")` call):

```c
		// TEMP plan 3 task 11: removed in task 11 step 9 - traced vertex heights for mud_holes.py fit
		int printed;
		for (int tries = 0; tries < 200000 && printed < 2000; tries++)
		{
			int ri = Math.RandomInt(0, m_aRec.Count());
			MUD_SkinChunk sr = m_aRec[ri];
			if (!sr)
				continue;
			int sv = Math.RandomInt(0, 1089);
			int smm = Math.Round((sr.m_aH[sv] - m_fOffset) * 1000);
			Print(string.Format("MUD bake sample %1 %2 %3 %4", ri - (ri / m_iChunksX) * m_iChunksX, ri / m_iChunksX, sv, smm));
			printed++;
		}
```

Ask the user to compile `MUD_CoarseSkin.c` and start play once (no driving needed). Expected: 2000 `MUD bake sample` lines after the `MUD coarse:` lines. Note the console.log path in the LEDGER.

- [ ] **Step 2: Failing tests for `fit` and `bake`**

Append to `ADDON/Tools/test_mud_holes.py`:

```python
# ---------------------------------------------------------------- fit / bake (plan 3 task 11)

def hght_tile(values, hole=None):
    """FORM/TERR like fake_ttile, HGHT = given 129*129 u16 values."""
    parts = [(b'VERS', b'\x01' * 9), (b'HGHT', struct.pack('<16641H', *values)), (b'BERR', b'\0' * 576)]
    if hole is not None:
        parts.append((b'HOLE', tt.encode(hole)))
    parts += [(b'LRS2', b'abc'), (b'TMAT', b'\0' * 4)]
    return tt.build(parts)


def pattern(i):
    x, z = i % 129, i // 129
    return (x * 131 + z * 977 + x * z) % 60000 + 1000


def test_hght_index_orients():
    assert mh.hght_index(0, 0, (0, 0, 0, 0)) == 0
    assert mh.hght_index(5, 7, (0, 0, 0, 0)) == 7 * 129 + 5
    assert mh.hght_index(5, 7, (1, 0, 0, 0)) == 5 * 129 + 7
    assert mh.hght_index(5, 7, (0, 1, 0, 0)) == 7 * 129 + 123
    assert mh.hght_index(5, 7, (0, 0, 1, 0)) == 121 * 129 + 5
    assert len(mh.ORIENTS) == 16


def test_vertex_local():
    assert mh.vertex_local(5, 6, 0) == (32, 64)
    assert mh.vertex_local(7, 0, 32 * 33 + 32) == (128, 32)


def test_parse_samples_with_prefix():
    text = "12:00:00.0 SCRIPT : MUD bake sample 3 4 100 -12345\nnoise\nMUD bake sample 0 0 0 7\n"
    assert mh.parse_samples(text) == [(3, 4, 100, -12345), (0, 0, 0, 7)]


def make_samples(o, a, b, tile_vals, n=300):
    out = []
    for k in range(n):
        ix, iz, v = k % 4, (k // 4) % 4, (k * 37) % 1089
        lx, lz = mh.vertex_local(ix, iz, v)
        out.append((ix, iz, v, round((a * tile_vals[mh.hght_index(lx, lz, o)] + b) * 1000)))
    return out


def test_fit_finds_orientation_and_scale():
    vals = [pattern(i) for i in range(16641)]
    o = (1, 0, 1, 0)
    samples = make_samples(o, 0.0047, -163.0, vals)
    r = mh.fit(samples, lambda t: hght_tile(vals))
    assert tuple(r['orient']) == o
    assert abs(r['a'] - 0.0047) < 1e-7 and abs(r['b'] + 163.0) < 1e-3
    assert r['resid'] < 0.001 and r['samples'] == 300


def test_fit_reports_bad_residual_on_noise():
    vals = [pattern(i) for i in range(16641)]
    samples = [(s[0], s[1], s[2], s[3] + (k % 7) * 500) for k, s in enumerate(make_samples((0, 0, 0, 0), 0.0047, -163.0, vals))]
    assert mh.fit(samples, lambda t: hght_tile(vals))['resid'] > 0.002


def test_bake_writes_hole_blocks_only(tmp_path):
    vals = [pattern(i) for i in range(16641)]
    rows = [0] * 32
    rows[3] = 1
    (tmp_path / 'Terrain_0.ttile').write_bytes(hght_tile(vals, {(1, 2): rows, (2, 2): [0] * 32}))
    (tmp_path / 'notes.txt').write_text('x')
    out = tmp_path / 'h.bin'
    fitd = {'orient': [0, 0, 0, 0], 'a': 0.5, 'b': -10.0}
    r = mh.bake(str(tmp_path), fitd, str(out))
    assert r == {'blocks': 1, 'tiles': 1, 'bytes': 16 + 8 + 4 * 1089}
    data = out.read_bytes()
    assert data[:4] == b'MUDH'
    assert struct.unpack('<iii', data[4:16]) == (1, 1, 32)
    assert struct.unpack('<ii', data[16:24]) == (1, 2)
    h = struct.unpack('<1089f', data[24:])
    assert h[0] == pytest.approx(0.5 * vals[mh.hght_index(32, 64, (0, 0, 0, 0))] - 10.0)
    assert h[1088] == pytest.approx(0.5 * vals[mh.hght_index(64, 96, (0, 0, 0, 0))] - 10.0)
    assert len(data) == r['bytes']


def test_bake_rejects_block_in_wrong_tile(tmp_path):
    vals = [pattern(i) for i in range(16641)]
    (tmp_path / 'Terrain_1.ttile').write_bytes(hght_tile(vals, {(0, 0): [1] * 32}))
    with pytest.raises(ValueError):
        mh.bake(str(tmp_path), {'orient': [0, 0, 0, 0], 'a': 1.0, 'b': 0.0}, str(tmp_path / 'h.bin'))


def test_main_fit_and_bake(tmp_path, capsys):
    vals = [pattern(i) for i in range(16641)]
    data = tmp_path / 'data'
    data.mkdir()
    (data / 'Terrain_0.ttile').write_bytes(hght_tile(vals, {(0, 0): [1] * 32}))
    log = tmp_path / 'console.log'
    log.write_text('\n'.join('MUD bake sample %d %d %d %d' % s for s in make_samples((0, 0, 0, 0), 0.0047, -163.0, vals)), encoding='utf-8')
    fitp = tmp_path / 'fit.json'
    assert mh.main(['fit', str(log), '--data', str(data), '--out', str(fitp)]) == 0
    assert 'FIT OK' in capsys.readouterr().out
    assert mh.main(['bake', '--data', str(data), '--fit', str(fitp), '--out', str(tmp_path / 'h.bin')]) == 0
    assert 'baked 1 blocks from 1 tiles' in capsys.readouterr().out
```

(`make_samples` uses blocks 0..3, all owned by tile 0, so one fake tile answers every `load_tile`.)

- [ ] **Step 3: Run, expect failure**

Run (from `ADDON\Tools`): `python -m pytest test_mud_holes.py -q`
Expected: the 8 new tests FAIL (`AttributeError: module 'mud_holes' has no attribute 'hght_index'` etc.), the 45 old ones pass.

- [ ] **Step 4: `fit` and `bake` in `mud_holes.py`**

Add `import json` and `import struct` to the imports. After `owner_tile` add:

```python
TILE_VERTS = 129
# (swap x/z, flip x, flip z, signed 16-bit): how a tile's HGHT is laid out, picked by fit()
ORIENTS = [(s, fx, fz, sg) for sg in (0, 1) for s in (0, 1) for fx in (0, 1) for fz in (0, 1)]
SAMPLE = re.compile(r'MUD bake sample (\d+) (\d+) (\d+) (-?\d+)\s*$')
TILE_FILE = re.compile(r'Terrain_(\d+)\.ttile$')


def hght_index(lx, lz, o):
    """Index in a tile's 129 x 129 HGHT of tile-local vertex (lx, lz) under orientation o."""
    x, z = (lz, lx) if o[0] else (lx, lz)
    if o[1]:
        x = TILE_VERTS - 1 - x
    if o[2]:
        z = TILE_VERTS - 1 - z
    return z * TILE_VERTS + x


def hght_values(tile_bytes, signed):
    d = dict(tt.chunks(tile_bytes)).get(b'HGHT', b'')
    if len(d) != 2 * TILE_VERTS * TILE_VERTS:
        raise ValueError('HGHT is %d bytes, expected %d' % (len(d), 2 * TILE_VERTS * TILE_VERTS))
    return struct.unpack('<%d%s' % (TILE_VERTS * TILE_VERTS, 'h' if signed else 'H'), d)


def vertex_local(bx, bz, v):
    """Tile-local vertex of skin chunk / hole block (bx, bz), chunk vertex v (33 x 33, x fastest)."""
    return (bx % 4) * BLOCK + v % (BLOCK + 1), (bz % 4) * BLOCK + v // (BLOCK + 1)


def parse_samples(text):
    """'MUD bake sample <ix> <iz> <v> <mm>' lines (MUD_CoarseSkin.c TEMP) -> [(ix, iz, v, mm)]."""
    out = []
    for line in text.splitlines():
        m = SAMPLE.search(line)
        if m:
            out.append(tuple(int(g) for g in m.groups()))
    return out


def linfit(us, hs):
    """Least squares h = a * u + b -> (a, b, max residual), None if u is constant."""
    n = len(us)
    mu = sum(us) / n
    mh_ = sum(hs) / n
    sxx = sum((u - mu) ** 2 for u in us)
    if sxx == 0:
        return None
    a = sum((u - mu) * (h - mh_) for u, h in zip(us, hs)) / sxx
    b = mh_ - a * mu
    return a, b, max(abs(a * u + b - h) for u, h in zip(us, hs))


def fit(samples, load_tile):
    """Best orientation + scale over all ORIENTS: {'orient', 'a', 'b', 'resid', 'samples'}."""
    cache = {}
    best = None
    for o in ORIENTS:
        us, hs = [], []
        for ix, iz, v, mm in samples:
            t = owner_tile(ix, iz)
            if (t, o[3]) not in cache:
                cache[(t, o[3])] = hght_values(load_tile(t), o[3])
            lx, lz = vertex_local(ix, iz, v)
            us.append(cache[(t, o[3])][hght_index(lx, lz, o)])
            hs.append(mm / 1000.0)
        r = linfit(us, hs)
        if r and (best is None or r[2] < best['resid']):
            best = {'orient': list(o), 'a': r[0], 'b': r[1], 'resid': r[2], 'samples': len(samples)}
    return best


def bake(data_dir, fitd, out_path):
    """Heights of every hole block in data_dir's tiles -> MUDH file. ValueError on a block in the wrong tile."""
    o, a, b = fitd['orient'], fitd['a'], fitd['b']
    blocks = []
    tiles = 0
    for name in sorted(os.listdir(data_dir)):
        m = TILE_FILE.search(name)
        if not m:
            continue
        t = int(m.group(1))
        with open(os.path.join(data_dir, name), 'rb') as f:
            data = f.read()
        parts = dict(tt.chunks(data))
        if b'HOLE' not in parts:
            continue
        tiles += 1
        vals = hght_values(data, o[3])
        for (bx, bz), rows in tt.decode(parts[b'HOLE']).items():
            if not any(rows):
                continue
            if owner_tile(bx, bz) != t:
                raise ValueError('block %d %d in Terrain_%d, owner is %d' % (bx, bz, t, owner_tile(bx, bz)))
            hs = [a * vals[hght_index(*vertex_local(bx, bz, v), o)] + b for v in range((BLOCK + 1) ** 2)]
            blocks.append((bz, bx, hs))
    blocks.sort(key=lambda e: (e[0], e[1]))
    with open(out_path, 'wb') as f:
        f.write(b'MUDH' + struct.pack('<iii', 1, len(blocks), BLOCK))
        for bz, bx, hs in blocks:
            f.write(struct.pack('<ii', bx, bz) + struct.pack('<%df' % len(hs), *hs))
    return {'blocks': len(blocks), 'tiles': tiles, 'bytes': 16 + len(blocks) * (8 + 4 * (BLOCK + 1) ** 2)}
```

In `main`: choices become `['plan', 'write', 'verify', 'fit', 'bake']`, `files` becomes `nargs='*'`, add `ap.add_argument('--data')` and `ap.add_argument('--fit')`, and right after `read = ...`:

```python
    if a.cmd == 'fit':
        if not (a.files and a.data):
            ap.error('fit needs <console.log> and --data')
        samples = parse_samples(read(a.files[0]))
        if not samples:
            print('no MUD bake sample lines')
            return 1
        r = fit(samples, lambda t: open(os.path.join(a.data, 'Terrain_%d.ttile' % t), 'rb').read())
        ok = r['resid'] < 0.002
        print('fit samples %d orient %s a %.9f b %.6f max residual %.6f m %s'
              % (r['samples'], r['orient'], r['a'], r['b'], r['resid'], 'FIT OK' if ok else 'FIT FAIL'))
        if a.out:
            with open(a.out, 'w') as f:
                json.dump(r, f)
        return 0 if ok else 1

    if a.cmd == 'bake':
        if not (a.data and a.fit and a.out):
            ap.error('bake needs --data, --fit and --out')
        with open(a.fit) as f:
            r = bake(a.data, json.load(f), a.out)
        print('baked %(blocks)d blocks from %(tiles)d tiles, %(bytes)d bytes' % r)
        return 0
```

Add the two commands to the module docstring usage block:

```
    python -I Tools/mud_holes.py fit    <console.log with MUD bake sample lines> --data <DATA> --out <fit.json>
    python -I Tools/mud_holes.py bake   --data <DATA> --fit <fit.json> --out <addon>/Data/Mud/mud_heights.bin
```

- [ ] **Step 5: Run, expect pass**

Run (from `ADDON\Tools`): `python -m pytest -q`
Expected: all pass (45 + 8 = 53 in `test_mud_holes.py`, plus the other suites' counts from the LEDGER).

- [ ] **Step 6: Fit and bake for real**

Run (from `ADDON`):

```
python -I Tools/mud_holes.py fit "<console.log from step 1>" --data "worlds/Arland/Terrain/.Data" --out Data/Mud/mud_fit.json
```

Expected: `fit samples 2000 orient [...] a <...> b <...> max residual <0.002 m FIT OK`, exit 0. `FIT FAIL`: stop - write the line into the LEDGER, then probe: print the 5 worst samples (ix, iz, v, mm, fitted) with a one-off `python -c` using `fit`'s best orientation, and check whether they share a tile (per-tile scale) or a vertex on a tile edge (index order); ask the user before changing the format.

```
python -I Tools/mud_holes.py bake --data "worlds/Arland/Terrain/.Data" --fit Data/Mud/mud_fit.json --out Data/Mud/mud_heights.bin
```

Expected: `baked 1697 blocks from <132 or fewer> tiles, <16 + 1697 * 4364 = 7405724> bytes` (1697 = chunks with holes, Plan 2 results; a different block count is a finding for the LEDGER before step 7).

- [ ] **Step 7: Skin loads the file**

In `MUD_CoarseSkin.c`:

Attribute (category Build, next to `m_bBuildAtInit`):

```c
	[Attribute("", UIWidgets.EditBox, "Baked heights file from Tools/mud_holes.py bake (e.g. $Testingplanvechmsh:Data/Mud/mud_heights.bin). Empty = trace every vertex.", category: "Build")]
	protected string m_sBaked;
```

Members:

```c
	protected ref map<int, ref array<float>> m_mBaked;     // chunk index -> 1089 heights, freed after the build
	protected int m_iBakedChunks;
	protected int m_iTracedChunks;
```

Method, called at the end of `Start()` (after `m_iChunksX` is set):

```c
	//------------------------------------------------------------------------------------------------
	//! Baked vertex heights (MUDH file): magic 'MUDH' read as int 1212437837, version 1, count, 32,
	//! then per block bx, bz, 1089 floats
	protected void LoadBaked()
	{
		if (m_sBaked.IsEmpty())
			return;
		int t0 = System.GetTickCount();
		FileHandle f = FileIO.OpenFile(m_sBaked, FileMode.READ);
		if (!f)
		{
			Print(string.Format("MUD coarse: baked heights '%1' did not open - tracing", m_sBaked), LogLevel.ERROR);
			return;
		}
		int magic;
		int ver;
		int n;
		int cells;
		f.Read(magic, 4);
		f.Read(ver, 4);
		f.Read(n, 4);
		f.Read(cells, 4);
		if (magic != 1212437837 || ver != 1 || cells != CELLS || n < 0 || n > 4096 * 4096 / (CELLS * CELLS))
		{
			Print(string.Format("MUD coarse: baked heights '%1' bad header (magic %2 version %3 count %4 cells %5) - tracing", m_sBaked, magic, ver, n, cells), LogLevel.ERROR);
			f.Close();
			return;
		}
		m_mBaked = new map<int, ref array<float>>();
		for (int i = 0; i < n; i++)
		{
			int bx;
			int bz;
			f.Read(bx, 4);
			f.Read(bz, 4);
			array<float> h = {};
			h.Resize(VERTS * VERTS);
			f.ReadArray(h, 4, VERTS * VERTS);
			m_mBaked.Insert(bz * m_iChunksX + bx, h);
		}
		bool whole = f.GetPos() == f.GetLength();
		f.Close();
		if (!whole)
		{
			Print(string.Format("MUD coarse: baked heights '%1' length does not match %2 blocks - tracing", m_sBaked, n), LogLevel.ERROR);
			m_mBaked = null;
			return;
		}
		Print(string.Format("MUD coarse: baked heights %1 blocks from '%2' (%3 ms)", n, m_sBaked, System.GetTickCount(t0)));
	}
```

In `SampleChunk`, replace the vertex loop (`for (int vz = 0; vz < VERTS; vz++) { ... HeightAt(...) + m_fOffset; }`) with:

```c
		array<float> baked;
		if (m_mBaked)
			baked = m_mBaked.Get(Math.Round((o[2] - m_fZ0) / m_fChunk) * m_iChunksX + Math.Round((o[0] - m_fX0) / m_fChunk));
		if (baked)
			m_iBakedChunks++;
		else
			m_iTracedChunks++;
		for (int vz = 0; vz < VERTS; vz++)
		{
			for (int vx = 0; vx < VERTS; vx++)
			{
				if (baked)
					rec.m_aH[vz * VERTS + vx] = baked[vz * VERTS + vx] + m_fOffset;
				else
					rec.m_aH[vz * VERTS + vx] = HeightAt(o[0] + vx * m_fCell, o[2] + vz * m_fCell) + m_fOffset;
			}
		}
```

(The index expression mixes `Math.Round` floats: assign to an `int` first - `int bi = Math.Round(...) * m_iChunksX + Math.Round(...);` then `m_mBaked.Get(bi)`; never pass a float as the key.)

In `Finish`, after the `MUD coarse:` lines:

```c
		if (m_sBaked != "")
			Print(string.Format("MUD coarse: chunks from baked heights %1, traced (no baked block) %2", m_iBakedChunks, m_iTracedChunks));
		m_mBaked = null;    // ~7.4 MB, not needed after the build
```

- [ ] **Step 8: Check independent of the sampler, check off by default**

In `Check(string when)`, the line `float hW = HeightAt(x, z) + m_fOffset;` becomes:

```c
			float hW = MUD_Surface.Get().HeightmapY(GetWorld(), x, z) + m_fOffset;
```

(its own trace: no shared `m_fLastGood`, no shared counters with the build). The `m_iCheckCells` attribute default `"2000"` becomes `"0"` (set it to 2000 on the placed entity for this step's test only).

- [ ] **Step 9: Remove TEMP, compile, play, gate**

Remove the TEMP sample block from step 1 (grep `TEMP plan 3 task 11`: expected no hits). Ask the user to compile `MUD_CoarseSkin.c`, set `m_sBaked` on the placed skin entity to `$Testingplanvechmsh:Data/Mud/mud_heights.bin` and `m_iCheckCells` to 2000, and play. If the open fails, try `Data/Mud/mud_heights.bin` (no prefix) and `{GUID}`-less `$Testingplanvechmsh:` variants and record which opened in the LEDGER.

Expected:

```
MUD coarse: baked heights 1697 blocks from '$Testingplanvechmsh:Data/Mud/mud_heights.bin' (<ms> ms)
MUD coarse: ... heights <ms> ms ...
MUD coarse: chunks from baked heights 1697, traced (no baked block) 0
MUD coarse check (build): <n> hole cells, skin hit 2000, within 5 cm 2000, worst <...> m
```

Gate: `heights` ms lower than the Plan 2 value in `docs/superpowers/results/2026-10-09-plan2-task3-skin.md` (write both numbers), traced 0, check 2000 / 2000 within 5 cm. Then set `m_iCheckCells` back to 0 on the entity. A check below 2000: probe first - print the 5 worst cells (x, z, baked + offset, `HeightmapY`), LEDGER, then decide with the user.

- [ ] **Step 10: Results file and adversarial review**

Write `ADDON/docs/superpowers/results/<run date>-plan3-task11-baked.md`: the fit line, bake line, all `MUD coarse` lines with and without the file (heights ms before / after), the opened path form, checksum: baked chunks + traced chunks = 1697 (sum written out). Run `engineering-skills:adversarial-reviewer` on `MUD_CoarseSkin.c` (focus: `LoadBaked` bounds and failure paths, the chunk index, `Check` change) and `mud_holes.py` (`fit`, `bake`). Rule on every finding in the LEDGER.

---

### Task 12: Skirts at hole edges (ONLY if Task 1 row R7 FAILED)

If R7 PASSED: write `Task 12 skipped: R7 PASS (<results file>)` into the LEDGER and stop here.

**Files:**
- Modify: `ADDON/Scripts/Game/Mud/MUD_CoarseSkin.c`
- Create: `ADDON/docs/superpowers/results/<run date>-plan3-task12-skirts.md`

**Interfaces:**
- Consumes: Task 3 `BuildGeometry(MUD_SkinChunk rec)` render branch (`MeshObject.Create(1, numVerts, numIndices, materials, 0)` with `{1089}`, `{6144}`), `IsHoleCell`, `m_aRec`, `m_iChunksX`.
- Produces: `class MUD_SkirtBuffers` with buckets of 64 / 256 / 1024 segments, `int FillSkirt(MUD_SkinChunk rec)`, attribute `m_fSkirt`, counter `m_iSkirtDropped`.

Spec 4.3: a 0.3 m strip hanging down from every skin edge that meets kept terrain, both sides, terrain-look material, so a terrain LOD that drops below the skin edge at distance shows the strip instead of a crack. Render only (no collider).

- [ ] **Step 1: Probe first (measure the cause)**

Before code: ask the user to fly / stand at the R7 crack spot from Task 1, at 300 m, and screenshot it; then walk to 20 m from the same edge and screenshot. Write into the LEDGER whether the crack is a gap where the real terrain's edge sinks below the skin edge (sky / far ground seen through: skirt fixes it) or a light line on the skin surface itself (shading: skirt does not fix it; stop, ask the user). Only the first case continues.

- [ ] **Step 2: Buffers and attribute**

After `MUD_SkinChunk` in `MUD_CoarseSkin.c`:

```c
//! Skirt scratch: MeshObject takes fixed-size arrays, so three bucket sizes; unused segments collapse
//! onto the first vertex. ~170 KB.
class MUD_SkirtBuffers
{
	vector m_aV64[256];
	float m_aUV64[512];
	int m_aI64[768];
	vector m_aV256[1024];
	float m_aUV256[2048];
	int m_aI256[3072];
	vector m_aV1024[4096];
	float m_aUV1024[8192];
	int m_aI1024[12288];
	// one segment list, filled first, then copied into the bucket that fits
	vector m_aSeg[4096];
}
```

Attribute (category Build): `[Attribute("0.3", UIWidgets.EditBox, "Skirt depth under hole edges (m), hides terrain LOD cracks at distance. 0 = off.", category: "Build")] protected float m_fSkirt;` Members: `protected ref MUD_SkirtBuffers m_Skirt;` (made in `Start()` when `m_bRender && m_fSkirt > 0`), `protected int m_iSkirtDropped;`, `protected int m_iSkirtSegs;`.

- [ ] **Step 3: `FillSkirt`**

```c
	//------------------------------------------------------------------------------------------------
	//! Segments on every hole-cell side that faces a kept cell: 4 verts each (top a, bottom a, bottom b,
	//! top b) into m_Skirt.m_aSeg. Returns the count; more than 1024 are dropped and counted.
	protected int FillSkirt(MUD_SkinChunk rec)
	{
		int ox = Math.Round((rec.m_vOrigin[0] - m_fX0) / m_fCell);
		int oz = Math.Round((rec.m_vOrigin[2] - m_fZ0) / m_fCell);
		int n;
		for (int z = 0; z < CELLS; z++)
		{
			for (int x = 0; x < CELLS; x++)
			{
				if (rec.m_aCell[z * CELLS + x] < 0)
					continue;
				for (int side = 0; side < 4; side++)
				{
					int nx = ox + x;
					int nz = oz + z;
					int ax = x;
					int az = z;
					int bx = x;
					int bz = z + 1;
					if (side == 0)
					{
						nx--;
					}
					else if (side == 1)
					{
						nx++;
						ax = x + 1;
						bx = x + 1;
					}
					else if (side == 2)
					{
						nz--;
						bx = x + 1;
						bz = z;
					}
					else
					{
						nz++;
						az = z + 1;
						bx = x + 1;
						bz = z + 1;
					}
					if (IsHoleCell(nx, nz))
						continue;
					if (n >= 1024)
					{
						m_iSkirtDropped++;
						continue;
					}
					float ha = rec.m_aH[az * VERTS + ax];
					float hb = rec.m_aH[bz * VERTS + bx];
					m_Skirt.m_aSeg[n * 4] = Vector(ax * m_fCell, ha, az * m_fCell);
					m_Skirt.m_aSeg[n * 4 + 1] = Vector(ax * m_fCell, ha - m_fSkirt, az * m_fCell);
					m_Skirt.m_aSeg[n * 4 + 2] = Vector(bx * m_fCell, hb - m_fSkirt, bz * m_fCell);
					m_Skirt.m_aSeg[n * 4 + 3] = Vector(bx * m_fCell, hb, bz * m_fCell);
					n++;
				}
			}
		}
		m_iSkirtSegs += n;
		return n;
	}

```

A bucket is a fixed-size static array, so the copy is written three times, one per bucket, as `FillBucket64(int n)`, `FillBucket256(int n)`, `FillBucket1024(int n)`, each the same body with its own arrays and cap. The body for the 64 bucket (copy it for 256 / 1024 with `m_aV256` / `m_aUV256` / `m_aI256`, cap 256, and `m_aV1024` / `m_aUV1024` / `m_aI1024`, cap 1024):

```c
	//------------------------------------------------------------------------------------------------
	protected void FillBucket64(int n)
	{
		for (int s = 0; s < 64; s++)
		{
			for (int k = 0; k < 4; k++)
			{
				int w = s * 4 + k;
				if (s < n)
					m_Skirt.m_aV64[w] = m_Skirt.m_aSeg[w];
				else
					m_Skirt.m_aV64[w] = m_Skirt.m_aSeg[0];
				float u = 0;
				if (k >= 2)
					u = 1;
				float v = 0;
				if (k == 1 || k == 2)
					v = 1;
				m_Skirt.m_aUV64[w * 2] = u;
				m_Skirt.m_aUV64[w * 2 + 1] = v;
			}
			int i = s * 12;
			int b = s * 4;
			if (s >= n)
				b = 0;
			m_Skirt.m_aI64[i] = b;
			m_Skirt.m_aI64[i + 1] = b + 1;
			m_Skirt.m_aI64[i + 2] = b + 2;
			m_Skirt.m_aI64[i + 3] = b;
			m_Skirt.m_aI64[i + 4] = b + 2;
			m_Skirt.m_aI64[i + 5] = b + 3;
			m_Skirt.m_aI64[i + 6] = b;
			m_Skirt.m_aI64[i + 7] = b + 2;
			m_Skirt.m_aI64[i + 8] = b + 1;
			m_Skirt.m_aI64[i + 9] = b;
			m_Skirt.m_aI64[i + 10] = b + 3;
			m_Skirt.m_aI64[i + 11] = b + 2;
			if (s >= n)
			{
				for (int j = 0; j < 12; j++)
					m_Skirt.m_aI64[i + j] = 0;
			}
		}
	}
```

`FillBucket256` and `FillBucket1024` are this method with `64` replaced by `256` / `1024` in the loop bound and in the array names (`m_aV256`, `m_aUV256`, `m_aI256`; `m_aV1024`, `m_aUV1024`, `m_aI1024`); nothing else changes.

- [ ] **Step 4: Second submesh in `BuildGeometry`**

Replace the render branch's mesh creation (`numVerts` / `numIndices` / `materials` / `MeshObject.Create(1, ...)` / `UpdateIndices` / `UpdateVerts`) with:

```c
			int sn = 0;
			if (m_Skirt)
				sn = FillSkirt(rec);
			Resource res;
			if (sn == 0)
			{
				int numVerts[] = {1089};
				int numIndices[] = {6144};
				string materials[] = {m_Material};
				res = MeshObject.Create(1, numVerts, numIndices, materials, 0);
			}
			else
			{
				int cap = 1024;
				if (sn <= 64)
					cap = 64;
				else if (sn <= 256)
					cap = 256;
				int numVerts2[] = {1089, cap * 4};
				int numIndices2[] = {6144, cap * 12};
				string materials2[] = {m_Material, m_Material};
				res = MeshObject.Create(2, numVerts2, numIndices2, materials2, 0);
			}
			if (res && res.GetResource())
			{
				MeshObject mesh = res.GetResource().ToMeshObject();
				// indices first: UpdateVerts rebuilds normals + tangents from the triangle list
				mesh.UpdateIndices(0, m_Buf.m_aIndices);
				mesh.UpdateVerts(0, m_Buf.m_aVerts, m_Buf.m_aUVs);
				if (sn > 0 && sn <= 64)
				{
					FillBucket64(sn);
					mesh.UpdateIndices(1, m_Skirt.m_aI64);
					mesh.UpdateVerts(1, m_Skirt.m_aV64, m_Skirt.m_aUV64);
				}
				else if (sn > 64 && sn <= 256)
				{
					FillBucket256(sn);
					mesh.UpdateIndices(1, m_Skirt.m_aI256);
					mesh.UpdateVerts(1, m_Skirt.m_aV256, m_Skirt.m_aUV256);
				}
				else if (sn > 256)
				{
					FillBucket1024(sn);
					mesh.UpdateIndices(1, m_Skirt.m_aI1024);
					mesh.UpdateVerts(1, m_Skirt.m_aV1024, m_Skirt.m_aUV1024);
				}
				rec.m_Ent.SetObject(mesh, "");
				rec.m_MeshRes = res;
			}
```

(the `else` error branch and `m_iMeshMs` stay as they are). In `Finish` add: `Print(string.Format("MUD coarse: skirts %1 segments, dropped %2 (more than 1024 in a chunk)", m_iSkirtSegs, m_iSkirtDropped));`.

- [ ] **Step 5: Compile, play, gate**

Ask the user to compile `MUD_CoarseSkin.c` and play. Expected: `MUD coarse: skirts <n> segments, dropped 0`; the build ms in the `MUD coarse:` line within +20 % of the Task 11 value; at the R7 spot from 300 m: no crack (screenshot); at 20 m: no visible strip above the ground (the skirt is under the kept terrain edge). `dropped` > 0: record the chunk count in the LEDGER and ask the user whether a 2048 bucket is worth +170 KB.

- [ ] **Step 6: Results file and adversarial review**

Write `ADDON/docs/superpowers/results/<run date>-plan3-task12-skirts.md` (probe screenshots, lines, build ms before / after, checksum: skirt segments counted by a one-off TEMP per-chunk print summed = the `skirts` total, TEMP print removed after). Run `engineering-skills:adversarial-reviewer` on `MUD_CoarseSkin.c` (focus: side / vertex index math, bucket choice, filler, the 2-submesh Create). Rule on every finding in the LEDGER.

---

## Done

Plan 3 is done when Tasks 1-11 have results files and LEDGER rulings (Task 12 a result or a skip line), every adversarial review is ruled on, the tools suites pass, and the user has answered the Task 10 archive question.
