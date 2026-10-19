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


## Part D, round 2 (v2 prompt D5-D8, 2026-10-19, same IDB, no code run)

### D5. GenericComponent.Deactivate on a ProcAnimComponent

| what | address | finding | label |
|---|---|---|---|
| Deactivate native | `sub_141321EA0` | if the component is already inactive (sign bit of `comp+76`) it returns; else it sets that bit and calls the component's vtable `+48` with the owner. | IDA |
| ProcAnimComponent vtable | `??_7ProcAnimComponent@gamecode@@6B@` = `0x141B390C0` | slot `+40` = `sub_1402A0500` (activate side), slot `+48` = `sub_1402A0560` (deactivate side). | IDA |
| deactivate side | `sub_1402A0560` | unhooks its signals (`sub_1402A0460`), then, if the OnFrameUpdate bit (`+156 & 1`) is set, calls `sub_1402A2980` = **ProcAnimSystem::Unregister**: removes the component from the flat per-frame list (swap with last, count - 1, slot = -1). | IDA |
| activate side | `sub_1402A0500` | if `+156 & 0x21` and the entity has a MeshObject -> `sub_1402A2780` = ProcAnimSystem::Register (back on the list). | IDA |
| the `+156` bits | ctor `sub_14029FD60` | bit 1 = the prefab property `OnFrameUpdate` (default true); bit 4 is ALWAYS set in the ctor and nothing on the Deactivate path clears it. | IDA |

Answer: yes, Deactivate stops the per-frame evaluation - not by clearing `+156 & 4`, but by taking the component off
ProcAnimSystem's list (the job walks only that list). `Activate(owner)` puts it back. So design C (live ProcAnim pool)
can park a piece with one Deactivate and wake it with one Activate. Other script path to OnFrameUpdate: none found -
it is a prefab property only (no script methods on ProcAnimComponent were found); `OnFrameUpdate 0` in the prefab
means the component is never registered at all. Confidence: high for the list removal; WB C2 still checks that the
piece stops moving.

### D6. Skinned entity with unchanged bones: per-frame work?

| what | address | finding | label |
|---|---|---|---|
| SetBone write | `sub_140A48F30` | an unchanged matrix returns early; a change sets MeshObject flag `0x100000` (VObject `+12`) and the entity dirty mark. | IDA (D1) |
| GetAnimation side | `sub_140A47C10` | creating the skeleton instance also sets `0x100000` - the flag marks "has a skeleton instance", not only "changed". | IDA |
| draw-side bone read | `MeshObject` vtable `0x141CA3CB0` slot 32 = `sub_140AAF5A0` | the MeshObject draw-prepare method calls `sub_140AA8D70` (current bone matrices, from the instance at `+48` via `sub_140A47850`) and reserves / copies `48 B x bones` of matrices per draw (`v198 += 48 * bones`). No test of a changed / dirty flag was found on this path. | IDA |
| reader of `0x100000` | byte-pattern search (test / bt on `[reg+0Ch]`) | no direct reader found - so no "skip if unchanged" gate was found. | IDA (negative, narrow search) |

Answer: the script side costs nothing when bones do not change (D1). The render side, as far as IDA shows, copies the
bone palette (48 B per bone) for every DRAWN skinned object every frame it is drawn - there is no "unchanged" skip.
A culled (not drawn) object does not reach this method. For a 121-bone tile that is ~5.8 KB of matrix copy per visible
tile per frame; 460-620 stored tiles in view = ~2.7-3.6 MB / frame of palette data (INFERENCE from the sizes).
Confidence: medium (the per-draw copy is clear; that no other cache exists is a negative from a narrow search).
Measure it: C1's 300 tiles left alone, then ms on the DS.

### D7. Bone limits

| what | address | finding | label |
|---|---|---|---|
| bone buffers in the MeshObject code | `sub_140A472A0` (animated bounds, flag `0x20`), `sub_140AAF5A0` (draw-prepare, twice) | each uses a fixed stack buffer of **384** bone matrices (`384 x 48 B`) to fetch the current pose. | IDA |
| animated bounds | `sub_140A472A0` | with request flag `0x20` and a skeleton, bounds are computed from the CURRENT bone matrices (min / max over bones); otherwise the static box or the `BoundingBox` override. Who passes `0x20` was not traced. | IDA |
| override mask | skeleton instance (`0x80` B) | the override bit mask is inline (`+24 .. +119` = 768 bits); matrices are heap arrays sized by the skeleton's bone count. | IDA |
| strings | find_regex "too many bones / max bones / bone limit" | no limit string or assert found. | IDA (negative) |

Answer: the hard number in the code is 384 bones per skeleton (stack buffers in the pose / bounds path); 121 bones
(11 x 11) fits. No per-draw (GPU) bone cap was found in this exe; a shader-side cap would be in the shaders, not here.
Import (Blender / FBX) limits are not in this exe. Confidence: medium (384 is a buffer size, not a stated limit).
Note: a skinned tile may get animated bounds automatically if the renderer asks with flag `0x20` - C1's cull test
(with and without `BoundingBox`) settles it.

### D8. SetObject on an entity that already has a skeleton instance

| what | address | finding | label |
|---|---|---|---|
| SetObject native | `sub_140E4E1E0` | parses the `$remap` string, then calls `sub_1409B5610(ent, vobject, remap)`. | IDA |
| set VObject | `sub_1409B5610` | if the new VObject's resource AND remap equal the current ones -> returns the old MeshObject (no-op). Otherwise clears `ent+80`, creates a NEW MeshObject through the VObject's factory (vtable `+248`), releases the old one (vtable `+64`, `+576`), marks the entity dirty (`0x10800000`) and updates it. | IDA |

Answer: dropped. A real SetObject builds a fresh MeshObject (its skeleton instance at `+48` starts empty), so all
SetBone overrides are lost; call `GetAnimation()` again and re-send the bones after a swap. Same object + same remap
= nothing happens (overrides kept). Main thread only (the same "locked entity" assert). Confidence: high for "new
object, old released"; medium that the new object's `+48` is empty (the instance is made only on demand by
`sub_140A47C10`).

### What this changes for sec 15
- 15.2 design C: parking = `Deactivate`, waking = `Activate` (D5) - parked pieces cost nothing per frame.
- 15.2 design B / 15.3 skinned tile: "unchanged bones cost nothing" holds for script only; each drawn skinned object
  copies its bone palette every frame (D6). Design A (static .xob variants) stays the only zero-per-frame option.
- 15.2 designs A + B on one piece: a variant swap (SetObject) wipes the bones (D8) - pick one per piece.
- 15.3: 121 bones fit under the 384 buffer (D7).
- v2 prompt B2 puts A5 (render-gated stored builds) in the same launch as A1 + A2: the 'sheet sync' and 'sh stored'
  parts are logged apart, but the total MUD ms mixes them - read the parts, not the total.

### D9. Who asks for animated bounds (flag `0x20`, `sub_140A472A0`)?

| what | address | finding | label |
|---|---|---|---|
| entity bounds | `GenericEntity` vtable `+136` = `sub_1409B0800` | asks the VObject for its box: VObject vtable `+608` with flags = the VObject's OWN flag word (`VObject+12`). | IDA |
| animated MeshObject class | `sub_140AA7B70` (factory) | a mesh whose resource has a skeleton (`+208`) (or two other resource conditions) is created as `MeshObjectAnimatedComponent` (vtable `0x141C9A920`), not a plain MeshObject. Its `+608` = `sub_140A47580` -> `+616` = `sub_140A472A0`, flags passed through. | IDA |
| bone bounds | `sub_140A472A0` | flag `0x20` set -> box from the current bones; else `0x20000` -> the `BoundingBox` override; else the static box. | IDA |
| who sets `0x20` on `VObject+12` | byte search `or dword [reg+0Ch], 20h` | ProcAnim job `sub_1402A1080` (cleared each eval, set again if its bones moved), door bone animation `sub_140332DA0`, Cinematic Tool `sub_1415B29A0`, two more (`sub_140B07C60`, `sub_1404F2A30`, not identified). Not SetBone / SetBoneMatrix (they set `0x100000`). | IDA |

Answer: the renderer does not ask for bone bounds; the flag lives on the mesh and only animation systems turn it on.
A tile or piece moved only by `SetBone` keeps the static (bind-pose) box -> it needs the MeshObject `BoundingBox`
override when bones move out of the bind box (or a bind pose that already spans the full depth range). A ProcAnim-driven
piece (design C) gets bone bounds automatically while it moves. Confidence: high for the call chain and the SetBone
side; medium that no other path sets `0x20` (byte search, one instruction form).

### Review of research sec 15.8 + v2 prompt (commit 54766a8)
- 15.8 numbers check: 121 x 48 B x 460-620 = 2.7-3.6 MB; 36 x 48 B x 460-620 = 0.8-1.1 MB; 121 x 48 B x 16-64 =
  0.09-0.37 MB; 4 x 48 B x 1000 = 0.19 MB. All match.
- B5 must be read on the DS CLIENT (the server draws nothing, so it has no palette copy); say so in the prompt.
- C1: with D9, the cull test is expected to FAIL without `BoundingBox` for bones moved 0.5 m out of the bind box;
  the run confirms it rather than decides it.
- Design B rut pieces: same - the "pressed" depth must stay inside the bind box or the piece needs `BoundingBox`.
