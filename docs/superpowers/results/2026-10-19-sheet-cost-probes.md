# Sheet cost probes: IDA answers (Part D) + brief review (2026-10-19)

Answers to `prompts/2026-10-19-local-sheet-cost-followup.md` Part D, plus a review of Parts A-C against the code on
this machine. No code was run: no Workbench, no DS, no build. Parts B and C (DS session, WB probes) are NOT done yet;
their sections below hold only what IDA or the code already decides.

Binary: `G:\SteamLibrary\steamapps\common\Arma Reforger Experimental\ArmaReforgerSteamDiag.exe` (the 1.9 Experimental
Diag exe; source paths in the asserts say `continuous_branches_stable_1.9.0`). Addresses are from that IDB.
Labels: **IDA** = read from decompiled code, **IDA-string** = from an engine property / diag text, **CODE** = our
scripts, **INFERENCE** = not proven.

## Part D - IDA

### D1. IEntity.SetBone / SetBoneMatrix: what they need, and bounds

| what | address | finding | label |
|---|---|---|---|
| script binding list of IEntity | `sub_140E05050` | `SetBone` -> `sub_140E3BAE0`, `SetBoneMatrix` -> `sub_140E3BD40`, `GetBoneMatrix` -> `sub_140E3B3A0`, `GetAnimation` -> `sub_140E4C680` | IDA |
| SetBone native | `sub_140E3BAE0` | reads (ent, bone, angles, trans, scale); takes `ent + 80` (the VObject), checks it is type id `0x16720CC` (the MeshObject type - same id as in the MeshObject property list below), then reads the MeshObject's **skeleton instance at `+48`**. If the VObject is not a MeshObject, or the instance is null, or the bone id is not in the skeleton, it **returns silently** (no error, nothing moves). No anim component is looked up. | IDA |
| bone write | `sub_140A48F30` | builds a 3x4 matrix from angles / trans / scale and stores it as a per-bone OVERRIDE (array at instance `+120`, 48 B per bone, bit mask at `+24`). Same matrix as before -> returns 0, nothing marked. angles 0 + trans 0 + scale 1 CLEARS the override (bit off). Otherwise sets MeshObject flag `0x100000` and calls `sub_1409B0F90`. | IDA |
| entity dirty | `sub_1409B0F90` | sets `0x20000000` on the entity and its parents (an "update me" mark). Asserts "Attempt to modify locked entity from a different thread" -> main thread only. | IDA |
| who makes the skeleton instance | `sub_140A47C10` | creates the instance (`new 0x80`, copies the bind pose) ONLY if the mesh resource has a skeleton (`resource + 208`); otherwise returns null. Callers: `IEntity.GetAnimation()` native (`sub_140E4C680`), the ragdoll setup (`sub_140B067A0`, "Can't create ragdoll from other than animated MeshObject"), anim components. | IDA |
| bounds | `sub_140A47D00` (MeshObject property list) | the MeshObject component has a property **`BoundingBox`**: "User override of bounding volume, if object is animated and can exceed its static bounds". Nothing in the SetBone path touches bounds. | IDA-string + IDA |

Answer: SetBone needs (1) a MeshObject whose .xob has a skeleton (Export Skinning) and (2) the skeleton instance to
exist - on a bare GenericEntity call `ent.GetAnimation()` once after spawn (it creates the instance); no
ProcAnim / anim component is required. Bounds stay the static (bind-pose) box: a bone moved out of it will be culled
by the static box unless the prefab's MeshObject `BoundingBox` override is set big enough. Per call: native bridge +
12 float compares + a parent-chain walk; unchanged bones cost almost nothing.
Confidence: high for the path; medium for "bounds are static" (the property text + no bounds code in the path; not
seen on screen).
Still to see in WB (C1): that the GPU skinning actually shows the override on a bare entity, and the cull with and
without `BoundingBox`.

### D2. SignalsManagerComponent.SetSignalValue -> ProcAnim per-frame cost

| what | address | finding | label |
|---|---|---|---|
| SetSignalValue native | `sub_1401918A0` -> `sub_1401BDD30` | writes ONE float into the signal array (two storage ranges). **No changed flag, no event, no wake-up.** | IDA |
| ProcAnim per frame | `sub_1402A1330` (ProcAnimSystem update) | every frame it walks ALL registered ProcAnim components and queues them as `ProcAnimJob`s, 4 components per job (worker threads). | IDA |
| job | `sub_1402A28D0` -> `sub_1402A1080` | per component: runs if a component flag bit (`+156 & 4`) is set and the entity has a MeshObject (`sub_1412E0480`). Reads its signals, evaluates the compiled ops, writes bones. **No distance, visibility or "signal changed" test.** | IDA |
| registration | `sub_1402A2780` ("ProcAnimSystem::Register %s") | a flat list; `ProcAnimComponent` has the property `OnFrameUpdate` "Enable/disable on-frame updating of the component" (default true, `sub_1402A07D0`). | IDA + IDA-string |

Answer: a ProcAnim is evaluated EVERY frame while registered, whether or not a signal changed. Setting the signal is
one cheap native call; the cost is per piece per frame (on worker threads), so it grows with the number of pieces,
not with how often they change. 20 000 pieces with a ProcAnim each = 20 000 evaluations every frame -> ProcAnim only
on the few live / near pieces, plain static .xob for the rest (INFERENCE for the ms; the scaling is IDA).
Not found: a script call that turns a piece's per-frame update off and on (the `SetDisableUpdate` seen in IDA belongs
to a sound component, not ProcAnim).

### D3. Two entities with the same .xob: instanced draw?

UNRESOLVED. What IDA showed:
- `sub_140BFCF00` prints a diag block "Terrain objects: Instances: %d, Draw calls: %d, Ideal draw calls: %d" (Detail /
  Super / Fallback) -> the terrain-object path batches instances. Whether runtime-spawned entities go the same way is
  not shown. (IDA-string)
- The Render diag item "Shape instancing" (`sub_140147CD0`, used in `sub_140A0F350`) batches debug `Shape` draws only,
  not MeshObjects. (IDA)
Measured fact we already have: 350-750 small script-mesh entities cost ~0.2 ms render (sheet-alternatives sec 7-8).
Test instead of more IDA: in C3, spawn 100 then 1000 pieces of one .xob and read the engine draw-call stats (Diag
render stats) in WB - count only, function check.

### D4. SpawnEntityPrefab (MeshObject prefab) vs SpawnEntity (script class)

| what | address | finding | label |
|---|---|---|---|
| SpawnEntity native | `sub_14130AFA0` | thin: reads (world, typename, params) and calls the game's vtable `+184`. | IDA |
| SpawnEntityPrefab native | `sub_14130AAB0` | thin: reads (world, **Resource**, params), checks the Resource is a loaded resource object, calls vtable `+192`. Nothing is loaded inside the native when you pass a Resource you already hold. | IDA |

Answer (partial): no resource lookup in the native itself - keep the `Resource` from one `Resource.Load` and reuse it.
The extra per-spawn work (component creation / init for the prefab's components) sits behind the vtable calls and was
not traced. The pool + `SetTransform` + `Update` route avoids both, so C3's 100 spawns vs 100 pooled moves decides it.

## Brief review (Parts A-C) - things to fix before building

1. **Files live on the cloud branch only.** The research doc, offline maths doc, `Tools/mud_sheet_cost_exp.py` and the
   prompt are on `claude/upbeat-lovelace-q0w022` (based on 43c2ada). The local tree (HEAD ebe4527 + many uncommitted
   changes) does not have them. Do not switch branches with that tree; copy the 4 files in
   (`git checkout origin/claude/upbeat-lovelace-q0w022 -- <paths>`).
2. **"static const switch" vs "one launch with test-kit settings" conflict (A1/A2 + B1/B2).** Test-kit values are
   read once at start and per machine (`$profile:MUD_TestKit.txt` for WB, server, PeerPlugin1). A1/A2 change the
   synced maths, so the DS and every client MUST have the same value or the hashes split. Either way B1 (off) vs
   B2 (on) needs a restart = two launches. Ask the user before calling that "one session".
3. **A1 port details** (`MUD_MudSync.c`, CODE):
   - The op box `m_iJi0..` is set AFTER the op's last stamp (line ~672: box = union of the stamp boxes + 1), so the
     "within 1 of stamped cells" mask cannot be filled into a box array during Stamp. Collect the stamped cells (the
     ones that passed `TileOf(i, k, true)`) as an int list in Stamp, then build + dilate the mask when the box is known.
   - Offline (`slump_region`) a flow happens only between two MASK cells (`mask & Mn`); port exactly that (cells
     outside the mask neither give nor take), keep the `da * 0.25` cap, factor 0.6 replaces `MUD_MudSheet.SLUMP` only in
     the sync slump.
   - Offline ground = the felt field sampled per cell; the game uses `GroundY` on even cells + the mean of 4. Only the
     even cells the mask's cells need must be read; the "-60 % reads" is an inference.
   - JOB_QUANT, the hash and the sheet restore keep walking the whole op box (unchanged).
4. **A2** matches the game's 2-sweep order (offline: mask cells in row order, then reversed = the game's reversed
   k and i). Keep the game's rule that cells with no tile are skipped and neighbours with no tile read THICK / 0 drag.
5. **A5 WB check is already answered:** `IsRendered` + `SET_RENDERED` works on our own mesh entities - the patch
   RENDER_GATE (`MUD_WorldGround.RenderWait`) counted 2749-23933 waits per 10 s on the DS client
   (research/2026-10-18-audit-ground.md, PeerPlugin1 log 2026-10-18 22:31). Reuse RenderWait's pattern for stored
   sheet tiles; skip the WB print test.
6. **B6 cannot be done with the robot:** `MUD_StutterRobot.Tick(dt, mine, me)` drives only the local player's
   vehicle. 2-4 trucks = 2-4 peer clients with drivers. A cheap stand-in is `fake_rut_ops` in the test kit (tagged
   kind-4 ops from the server, all one vehicle id, one burst) - it measures per-op cost, not "per remote truck".
7. **C1 must call `ent.GetAnimation()` once** after spawn on the bare entity (D1), and set the MeshObject
   `BoundingBox` override on the second variant to test culling.
8. **C2 / 3.4 scaling:** ProcAnim costs every frame per piece (D2) - ProcAnim only on live pieces; the bulk of the
   pieces must be static .xob.
