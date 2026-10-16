# Voxel crater wall slopes - today's Carve vs a cosine bowl (2026-10-16, offline)

Tool: `python Tools/crater_slope_voxel.py` (uses Tools/dc_vertex_proto.py: the game's Carve formula from
MUD_VoxelProbe.c, seed, Polygonize + Quad, surface-nets mean vertex = CellPos). Grid widened to 26 m, crater centre
(13.1, 12.9), off the sample lines. Depths = FullDepth of each charge (Tools/crater_profile.py families).
Labels: MEASURED = this run (offline, the game formulas; not seen in game yet).

- "voxel today" = MUD_VoxelProbe.Carve: half ellipsoid (crest = 2.2 x depth) + funnel + sine rim (rimH 0.2 x depth).
- "cosine bowl 42" = the heightfield crater profile (MUD_WorldGround.c Crater, Tools/crater_profile.py) as a
  vertical-distance field: bowl -D .. +H over Rc = (D + H) x pi / (2 x 0.9), rim falls over W; max slope 0.9 = 42 deg.
- Soldier limit (IDA / pak 2026-10-10): MaxSlope 50 deg, "Move max slope angle" 60 deg.
- Walk-out: 72 radial lines from the centre, mesh height every 0.05 m. "Steep climb" = most height gained in one
  unbroken stretch steeper than 50 deg. "Easiest way" = the best of the 72 lines.

| Charge | Depth m | Shape | Outer r m | Tris | Steepest tri deg | Area > 50 deg % | Area > 60 deg % | Steep climb > 50 deg on the EASIEST way out m | ... median way m | Steepest 0.25 m stretch, easiest way deg |
|---|---|---|---|---|---|---|---|---|---|---|
| HEAT 0.51 kg | 0.6 | voxel today | 1.92 | 5566 | 35 | 0.0 | 0.0 | 0.00 | 0.00 | 26 |
| HEAT 0.51 kg | 0.6 | cosine bowl 42 | 1.73 | 5550 | 43 | 0.0 | 0.0 | 0.00 | 0.00 | 30 |
| O-832DU 0.40 kg | 0.86 | voxel today | 2.75 | 5622 | 43 | 0.0 | 0.0 | 0.00 | 0.00 | 36 |
| O-832DU 0.40 kg | 0.86 | cosine bowl 42 | 2.26 | 5574 | 47 | 0.0 | 0.0 | 0.00 | 0.00 | 37 |
| 81 mm 0.99 kg | 1.43 | voxel today | 4.58 | 5846 | 67 | 14.4 | 2.4 | 0.21 | 0.52 | 52 |
| 81 mm 0.99 kg | 1.43 | cosine bowl 42 | 3.49 | 5654 | 44 | 0.0 | 0.0 | 0.00 | 0.00 | 40 |
| Hydra 2.9 kg | 2.04 | voxel today | 6.53 | 6206 | 72 | 14.9 | 9.4 | 0.43 | 0.90 | 59 |
| Hydra 2.9 kg | 2.04 | cosine bowl 42 | 4.98 | 5898 | 44 | 0.0 | 0.0 | 0.00 | 0.00 | 41 |
| TM-62M 7.5 kg | 2.81 | voxel today | 8.99 | 6838 | 74 | 17.8 | 11.0 | 0.82 | 1.40 | 64 |
| TM-62M 7.5 kg | 2.81 | cosine bowl 42 | 6.86 | 6262 | 43 | 0.0 | 0.0 | 0.00 | 0.00 | 41 |

(Tris = the whole 26 m grid, not only the crater.) Run twice (scratchpad, then from Tools/): identical.

## What it shows
- Small charges (<= ~0.9 m deep) are fine today: no ground over 43 deg.
- From the 81 mm mortar up, today's crater has a steep ring at the crest: 14-18 % of the crater area over 50 deg,
  up to 74 deg. Even the EASIEST of 72 ways out climbs 0.21 m (mortar) / 0.43 m (Hydra) / 0.82 m (TM-62M) over
  50 deg. Whether a soldier gets out depends on the character step height (not read yet) - an in-game check.
  Cause (formula): the ellipsoid wall turns vertical at r = crest, and the rim bump starts at 0.8 x crest, so the
  lip is a near-vertical 0.3-0.7 m step that the 0.5 m samples only partly round off.
- The cosine bowl keeps every triangle at 43-47 deg max (the 42 deg design + sampling), and its footprint is
  ~25 % smaller in radius: a mortar converts ~25 columns instead of ~31 (columns per op, box + ring, averaged over
  4000 positions: scratch calc), so MAX_COLUMNS 6000 lasts ~240 mortar craters instead of ~195.
- The vertical-distance form needs no FUNNEL (it is a heightfield cut, so it cannot undercut), but like today's
  funnel it cuts everything above the bowl: a blast under a roof (tunnel) opens it to the sky. A blast with solid
  ground above the hit needs its own (sphere) shape either way.
