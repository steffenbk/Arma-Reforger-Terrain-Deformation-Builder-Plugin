# Prompt for the local AI: engine-native deformation probes + IDA questions (2026-10-20)

Copy everything below the line. This runs separately from mud lab stage 1 (its own files, its own launch).

---

Run the engine-native probes from `docs/superpowers/research/2026-10-20-engine-native-deformation.md` (sec 3) and its
IDA questions (sec 5), on branch `claude/upbeat-lovelace-q0w022`. These are probes, not features. Each one answers
yes / no with a screenshot. Run one WB launch, function and look only, no ms. Stop and report after it.

## Rules
- Read `CLAUDE.md`, the research doc, and `mud-gotchas.md` first.
- **Each probe gets its own file** under `Scripts/Game/Mud/` named `MUD_NativeProbe_<id>.c`, behind
  `MUD_TestKit.TEST_KIT`, and is started by a key. All numpad keys are taken, so use Left Ctrl + numpad 3 only if mud
  lab stage 1 is not built yet; otherwise Left Alt + numpad 3 cycles the probes. Tell me which one you used.
- **Nothing in rut files**, and nothing on a DS. Free everything when the probe stops.
- **Checkpoint first:** `mud_backups/checkpoint.ps1 save "before native probes"`.
- **Git:** `git add --ignore-removal .`, commit, push to this branch. No PR.

## IDA first (it can save probes)
Answer the research doc's sec 5 questions 1-5. Above all, which material classes read `$rendertarget` /
`$renderview`, and in which slot. If no material takes it in a normal / height / opacity slot, skip those parts of
P-N2 and all of P-N3.

## Probes, in this order
1. **P-N1, wheel attach.** Attach a small box to each wheel of a Ural, two ways:
   - (a) `AddChild` to a non-spinning node: list the vehicle's bone names and pick the hub or suspension;
   - (b) the vehicle root, with the offset from `WheelGetPosition` every frame.
   Check: does it follow without spinning or lag while driving and on bumps? Then scale the box with speed through a
   ProcAnim signal, or tell me why not.
2. **P-N2, render target.**
   - Make a script MeshObject plane, 4 x 4 m, with an emat that uses `$rendertarget` in the colour slot. An
     `RTTextureWidget` with a `CanvasWidget` child draws one LineDrawCommand that moves every second. Call
     `SetRenderTarget(planeEnt)`.
   - Then try the normal and height slots, if IDA says they can take it.
   - Print the `RenderTargetWidgetBlendMode` / `RenderTargetWidgetFormat` values.
   - Try `SetClearColor(false, ...)`: do old pixels stay?
   - Try an orthographic camera (`SetOrthoLinear`) with `SetRenderView`, looking up from 1 m below the ground at a
     parked truck: does the texture show the wheels?
3. **P-N4, physics mat.**
   - Spawn 10 x 10 small dynamic boxes (0.25 m) over 3 x 3 m of flat holed grass, 5 cm above the ground. Turn gravity
     off (`EnableGravity(false)`) and set high linear damping. Give each box a 6DOF joint to a static anchor that
     allows only vertical travel, -0.15 .. +0.08 m.
   - Move a kinematic sphere (r 0.5 m) through them on a fixed path. Do they press and STAY? Do they jitter? Do they
     sleep afterwards?
   - Then put the sphere on a driven Ural wheel, on a collision layer that only the boxes collide with. Does the truck
     feel anything? It must not.
4. **P-N5, ragdoll mat.**
   - Read a ragdoll definition from the user's local addon "Euphoria ragdoll" and describe its format.
   - If a grid-shaped definition looks possible, make a 4 x 4-bone skinned plane. If you cannot export one, write the
     Blender script and stop.
   - `CreateRagdoll`, gravity off on each bone body, push it with the proxy from P-N4. Do the bones follow without any
     SetBone call?
5. **P-N3, shells.** Only if P-N2 found an opacity / alpha slot that takes `$rendertarget`. Make 6 planes 2.5 cm apart,
   with alpha-test cutoffs 1/6 .. 6/6, and one painted line. Does a groove show from the side?
6. **P-N6, granular.** 300 spheres (0.12 m, gravity on, high friction) in a 3 x 2 m box; the proxy ploughs through.
   Look only: do ridges and a bow wave form?

## Report back
- The IDA answers.
- Per probe: yes / no, a screenshot, the log lines, and anything odd.
- The files you added.

Do not build anything beyond the probes. I pick what goes into the mud lab.
