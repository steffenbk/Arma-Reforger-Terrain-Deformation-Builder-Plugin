# Prompt: wheel-to-rut track mark (new chat, 2026-10-18)

Copy everything below the line into a fresh Claude Code chat in this mod folder.

---

Mud ground mod (Arma Reforger, Enfusion, Enforce Script), this folder. Read CLAUDE.md, then
docs/superpowers/research/mud-gotchas.md (grep "decal", "CreateDecal", "64 local", "80 KB") and
docs/features/ruts/README.md before anything else. The mud sheet / mud zone is PARKED (MUD_MudSheet.ENABLED false,
2026-10-18) - do not touch it. We are improving the VOXEL RUTS now.

## The problem

The rut does not start at the tyres. Behind a driving truck there is a stretch of untouched ground, then the rut
begins. Why (by design, keep it):
- The felt rut (server, MUD_VoxelProbe RUT_BEHIND) is cut only once the REARMOST wheel is RUT_BEHIND_GAP past a
  segment, so the ground under the tyres never changes while driving (no sinking into your own rut, no snaps).
- The client look rut (MUD_LookRuts) is cut GAP 0.4 m behind the rearmost wheel contact and then needs a patch mesh
  rebuild. Measured DS 2026-10-18 16:16 / 16:27 ('MUD delay look 10 s' lines in PeerPlugin1 logs): wheel -> look cut
  avg 404-534 ms, max 1.5-3 s.
- So from each FRONT wheel back to the rear axle + 0.4 m + speed x ~0.5 s nothing is drawn.

## The idea (user)

Fill that gap with a mark that starts right at each wheel and runs back to where the voxel / look rut starts - so
the eye sees one continuous rut. Look only: no physics, no net, nothing that feeds hashes or the synced layer.
Open question from the user: keep spawning many short decals, or one long mark that follows the truck and grows /
shrinks with the situation (speed, stop, reverse, the rut catching up)?

## Proven facts (do not re-test unless you change the setup)

- World.CreateDecal onto OUR ground has never worked: ~1700 'MUD decal:' log lines (crater decal probe,
  MUD_VoxelProbe.CraterDecal, CRATER_DECAL), every one 'decals made 0' / 'decal 0'. Run 1: the trace hit the hidden
  GenericTerrainEntity at the old ground height and CreateDecal on it returned null (8/8). Run 2: CreateDecal
  straight onto our shown voxel brick entities (script MeshObject) returned null every time.
- On holed terrain cells a vanilla DecalEntity hangs under the map (mud-gotchas 2026-10-12); offroad ground is holed
  (TERRAIN RULE), so the real terrain is NOT under the wheels offroad - our script patch meshes are.
- CreateDecal works on vanilla entities in another of the user's mods: addons/Spray tagging can/Scripts/Game/
  SCR_SprayRaycastComponent.c (it queries an entity near a terrain hit because a terrain hit has no entity).
- Script meshes are cheap to make, expensive to fill in script: MeshObject.Create ~0 ms, UpdateIndices + UpdateVerts
  ~0.24 ms per 1089 verts; flags-0 meshes never redraw after UpdateVerts, so every change = a fresh mesh; member
  arrays fed to UpdateVerts crashed twice (use local static arrays). Enforce: max 64 locals in scope per method
  (params count), 80 KB locals per method, '%' / '&' in a float expression fails.
- VehicleWheelEffectComponent (vanilla, game code since 1.9) has wheel / body mud and dust setters; its skid-mark
  config was NOT checked. It probably draws onto the real terrain (holed offroad) - unverified.
- No custom shaders in mods (IDA 2026-10-16).

## What to find out first (before building)

1. Can a decal land on a script MeshObject entity at all? Find WHY CreateDecal returned null on our bricks (material
   flags? the entity needs a VObject / a mesh with a decal-receiving material? the projection box / angle / size?).
   Read the CreateDecal / Decal API (enfusion-mcp api_search, game scripts that call it) and run ONE small probe in
   Workbench on our patch entity. If null again after the obvious fixes, drop decals - do not loop.
2. If decals cannot work, the fallback is our own thin RIBBON MESH per wheel (MudRunner draws wheel tracks as a
   separate strip of trapezes following each wheel, drawn over the ground): a few quads from the wheel contact
   back to the rut start, lifted a few mm over the patch mesh, tread / wet-soil material, fresh MeshObject per
   update. Compare cost and look with option 1.
3. Grow / shrink: the mark's back end should end where the drawn rut begins (look rut cut point or the felt cut),
   its front at the wheel contact. Find the cheapest source of "where does the drawn rut start now" (MUD_LookRuts
   track points, RUT_BEHIND gap) without reading the synced rut floor on any op path (J2 rule in mud-gotchas).
4. Stop / reverse / leaving the ground (air, teleport: MUD_LookRuts JUMP) / getting out: what the mark does.

## Also in this chat: voxel rut quick wins (lessons from the parked mud sheet)

Lessons list: docs/features/ruts/README.md top. Most of them were built only into the mud sheet code (now off), so
the voxel ruts do not have them yet. Status 2026-10-18:
- Live already: delay meter ('MUD delay look 10 s' in MUD_LookRuts), FLOOR_SPREAD (MUD_WorldGround).
- Do first, small (one session with the decal probe, checkpoint before):
  1. MUD_VoxelProbe.OP_LOG -> false: the per-op 'MUD rut floor' Print costs the server ~2.2 ms per rut op
     (proven DS 14:11, results/2026-10-18-h1-snap-probe.md run 5). Check what else OP_LOG gates before flipping.
  2. MUD_WorldGround.COL_SKIP_SMALL -> false: paper test 3, only 0-4 skips per 10 s (DS 16:16) = no gain, extra code
     path in the collider build.
  3. Faster loops in the ground patch builders (MUD_WorldGround BuildMeshBand / BuildMesh, BuildColliderBand, and
     MUD_VoxelProbe DispEmit): the recipe that made the sheet tile build 1.10 -> 0.88 ms (-20 %): hand-inline small
     helpers, read per-vertex flags once into a local int array, skip untouched cells. Bench first: N old + N new
     builds of the same patch between two GetTickCount calls, same output count, behind a switch (old path kept).
     Mind the 64-locals limit (params count) and the 80 KB stack.
- Later, one at a time with the user: wheels-first order for the patch rebuild queue (today longest-waiting first;
  sheet proof: delay up to 9 s -> ~80 ms); a cheaper rut cut off the sheet (crude = no RelaxFloor / GrowRidges halved
  the op, but it changes the rut look and the server floor - J2 / hash rules apply); far LOD for patch meshes.
- Design rule (lesson 7): one system owns each visible surface. The sheet's patch mesh + sheet tile from two queues
  gave a height step and pops that build order + a 1 cm lift did not cure. The track mark must sit ON the ground
  mesh (decal or a strip a few mm above it), never replace or hide part of it.

## Rules

- Present options with costs before coding, one at a time; user picks. Checkpoint before code
  (mud_backups/checkpoint.ps1 save "name").
- Everything behind a static const switch (false = today exactly). Client / rendering machines only.
- Tests: Workbench for look; perf only on a dedicated server or the real game (never measure perf in Workbench).
  Group checks into one session per build and end with a numbered checklist. Read logs yourself.
- Run the adversarial-reviewer skill on the Enforce changes before calling them done.
- New ideas that are not this go to Parked ideas in docs/superpowers/backlog/2026-10-15-master-list.md.
- Plain short answers, no emojis, no git commits unless asked.
