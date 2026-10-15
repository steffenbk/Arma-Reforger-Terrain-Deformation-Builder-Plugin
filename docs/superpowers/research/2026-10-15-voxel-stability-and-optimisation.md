# Voxels: ground that holds itself up + optimisation list (2026-10-15)

User: "how can we optimise and better our voxels? is it possible to add physics based on connecting, so we don't have
so much overhang? sometimes when I tunnel I can make a really thin bridge or connection that looks unrealistic."
Labels: CODE (master 34d1afe - v097 not on GitHub), MEASURED, PROVEN, INFERENCE (not tested). Nothing built.

## 1. Short answer
- **Yes, and it does not need real physics.** Soil does not need simulating as falling bodies; it needs a few RULES run
  on the voxel field after each dig: remove what is cut off, collapse roofs / bridges that are too thin, drop the
  material as rubble. That is what most voxel games do (cellular "support" rules, not rigid bodies).
- **Why it happens today (CODE):** craters already avoid undercuts (FUNNEL, MUD_VoxelProbe.c:261 / Carve comment ~2480:
  "a full ellipsoid undercut walls into bridges"). Tunnels do not check anything: CarveTunnel (~2722) cuts a capsule
  wherever it is, so a tunnel near the surface, next to another tunnel or under a crater leaves whatever is left -
  0.1 m roofs and bridges included. At 0.5 m samples anything thinner than ~1 m is a "paper" sheet in surface nets.
- **MP-safe without new net data:** ops already apply identically on every machine (same field, same order - that is
  how voxel MP + JIP work today). A deterministic rule pass inside the op apply gives the same collapse everywhere.
- **Recommended:** (A) a dig-time guard for tunnels (cheap, server only) + (B) a collapse pass after ops that touch
  existing air pockets. Plus the optimisation list in section 4 (biggest: the rut field, then brick memory).

## 2. The rules (all on the 0.5 m sample grid, inside the op's box + a margin)
| # | Rule | Fixes | Cost (INFERENCE) |
|---|---|---|---|
| R1 | **Thin roof:** in each sample column, for every air pocket with solid above it, roof thickness t = solid samples up to the next air / the surface. t < ROOF_MIN (1.0 m = 2 samples) -> the roof falls: remove those solid samples (a sinkhole opens) and add the same height of rubble on the pocket floor | thin tunnel roofs, tunnels dug too shallow, crater blown over a tunnel | per column, tiny |
| R2 | **Thin bridge / wall:** a solid sample with air under it (an overhang sample) that has fewer than N solid neighbours in its 3 x 3 x 3 ring (e.g. < 9 of 26) is "thin" -> removed, as rubble below. Run 1-2 passes. Only samples with air BELOW are touched, so normal ground, crater rims and trench lips (soil under them) never change | the thin bridges / connections from tunnelling, spikes, paper walls between two tunnels | per sample in the box, small |
| R3 | **Cut-off chunk:** flood fill the solid samples of the box + margin from anchors (box border samples that are solid, samples near FLOOR, hard columns). Solid not reached = floating -> removed, its volume dropped as rubble straight down | blocks left hanging by two tunnels / a crater | flood fill over the box, a few ms for a crater box (INFERENCE: 20 x 20 x 16 samples) |
| R4 | (later) **Span rule:** roof needs t >= span x k (wider tunnels need thicker roofs, like mine arching); span = air width at that height | realistic big caverns | more work |

Rubble: removed solid height goes onto the floor of the same column under the pocket (simple column "fall", the
MudRunner / cellular way). Keeps volume roughly, fills the tunnel where it fell in - reads as a cave-in.
Only run when the op's box has an air pocket under solid (any column with more than one surface crossing) - a crater
on plain ground (no tunnels) skips it at once.
Feedback (client only): dust particle + sound at each collapse, e.g. the vanilla ground-hit dust (pick a prefab).

## 3. Prevention at dig time (tunnels) - do this first
- Server, before issuing a tunnel op (ServerTunnel / DoTunnel): measure what the capsule would leave:
  - roof: undug / current surface over the capsule top < ROOF_MIN -> either refuse ("too shallow") or cut it as an
    open trench to the sky (no paper roof). User picks.
  - sides: air (another tunnel / crater) within WALL_MIN (0.75 m) of the capsule side -> cut through (merge the two
    openings) instead of leaving a sheet.
- Cheap (a few samples), server only, no new op kind; stops most bridges before they exist. R1-R3 then cover what
  other ops cause (a crater over a tunnel, two diggers).

## 4. Optimising the voxels (ranked)
| # | What | Gain | Evidence |
|---|---|---|---|
| 1 | **Rut field** (plans/2026-10-15-rut-field-plan.md) - ruts stop converting columns | most bricks / columns today come from ruts; frees the 6000-column budget for craters / tunnels | MEASURED freeze-test "After" table (columns / bricks per rut window) |
| 2 | **Drop the unused look array per brick:** MUD_VoxelBrick.m_aV[512] (RUT_LOOK only, off) is a static array in EVERY brick = 2 KB, as big as the real field m_aD | ~half the sample memory per brick | CODE MUD_VoxelProbe.c:97-98 (`float m_aV[SIZE_D]`, m_bHasV) - make it a lazily created holder object |
| 3 | **Go-live frame of a big crater** 50-100 ms (voxel README) | the stutter on big blasts | LOG; split it first (brick swaps vs skin / patch rebuilds in VoxCellsChanged, which rebuilds patch meshes "that frame") then pre-build the patch side hidden as the bricks are |
| 4 | **JIP:** lazy per-brick queues (papers Step D) + RLE seeds | faster big-match joins | READ (van Ruschen, Lengyel); not built |
| 5 | **Sharper crater rims / trench lips:** DC vertex for craters + trenches only, with a collider filter for folded tris | look | MEASURED dc-proto.md: -41 % / -48 % rms |
| 6 | **Wheel feel near voxel edges:** smooth profiles where wheels run (one centre ray, raw triangle normal) | fewer bumps | IDA 2026-10-15 item 5 |

## 5. Suggested order (user decides)
1. Finish the rut field test (already planned) - it is also optimisation #1.
2. Tunnel dig-time guard (section 3) - small, fixes the bridges you see most.
3. Brick memory (#2) - small, safe.
4. Collapse pass R1 + R2 (+ rubble), then R3, behind a switch; MP check with DISP_HASH-style hash lines per op.
5. Go-live profiling (#3), then DC rims (#5).
