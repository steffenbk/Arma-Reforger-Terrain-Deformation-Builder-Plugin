# MudRunner mud rendering (article read 2026-10-14)

Source: "Mud and Water of Spintires:MudRunner", Pavel Zagrebelnyy (lead dev), gamedeveloper.com, 2017-11-16.
User's printout: C:\Users\Steffen\Videos\45.pdf (31 pages, images only; mud = pages 2-13, water = 13-27).
Read from the article, not measured by us.

## Layers, bottom to top
1. Mud heightfield - the main thing. A plain 2D vertex grid; per 16 m block a 128 x 128 texture (0.125 m):
   R = mud height RELATIVE to base terrain, GB = "mud offset" (the mud texture slides away from the wheels),
   A = "mud tracks" (blend between terrain shader and mud shader so the edge is invisible).
   The base terrain has a mask (25 x 25 per block, A channel) that marks where the mud REPLACES the terrain -
   so mud can go lower than the base terrain.
2. Road overlays (baked into terrain shader).
3. Mud chunks - rigid bodies, break into smaller ones when driven over (managed like trees/rocks).
4. Mud particles - flat bits floating on the mud, pushed by wheels, can stick to them; CPU, own thread, visual only.
5. Wheel tracks - a strip of trapezes following each wheel, drawn after the terrain with parallax (2 passes).
6. Terrain mud decals - from mud particles thrown by fast / spinning wheels; z-projected oriented boxes.

## How the mud is deformed (page 9-10)
Per wheel and chassis the CPU draws "primitives" into the mud texture (a render target):
- MUD HEIGHT "EXTRUDE": blend MIN - presses the height down under the wheel.
- MUD HEIGHT "PUSH": blend MAX - raises a ridge beside / in front of the wheel (a shape, not a volume).
- MUD TRACKS (A): where the tyre went.
- MUD OFFSET (GB): texture dragged along with the wheel - this is what makes it look fluid.
Inputs: wheel / chassis position, size, velocity, current sink depth. "Pure empirics, very vague connection to
real world physics." The mud pixel shader only blends a few diffuse textures by the normal.
Only blocks near the player's truck are active (physics + mud + water sim).

## What it means for us
- Our physical ground (FinePatch heightfield / voxel bricks over the holed terrain) already is MudRunner's
  layer 1: it replaces the terrain and can go below it. A sheet ON TOP can only add height (ridges), never dips.
- Our sheet test (MUD_MudSheet.c, 2026-10-14 20:46): volume-kept berms came out ~2-3 cm high - not visible.
  MudRunner does not keep volume: EXTRUDE (min) and PUSH (max) are just shapes, sized by sink depth and speed.
- Mud offset = per-vertex UV drag. We control per-vertex UVs (clamped 0..1 per mesh), so this is possible.
- Wheel tracks with parallax and z-projected decals: not possible for us (no custom shaders); the old
  MUD_RutTrailEntity strip is our nearest thing.
