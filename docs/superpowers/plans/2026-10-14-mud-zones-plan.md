# Mud zones: pre-built mud at map load (user pick B, 2026-10-14)

Goal: mud only where a placed MUD_MudZoneEntity box says so, all of it built at load, no mud appearing round
the vehicle out of nothing. Ruts stay visual (sheet), voxel ruts stay off (MUD_VoxelProbe.RUTS false).

## Design (reuses the trail system)
- MUD_MudZoneEntity: placed in the World Editor, size X / Z (m, default 64), axis-aligned, editor box drawn.
- Every 3 m world tile whose middle is in a zone is a mud tile. At load each one is traced (even cells, 11 x 11
  traces, rest filled), given the THICK layer, meshed and kept as a stored tile. One tile per frame
  (64 m zone = ~440 tiles, ~7 s), tiles nearest the player first.
- The 12 m live window still follows the vehicle and does the editing (stamps, slump, drag). Tiles entering it
  come out of the store; leaving zone tiles go back into the store (always, not only touched ones). Window tiles
  outside every zone are hidden, and give no drag.
- A tile fades into the ground only on sides where the next tile is not a zone tile (so fades never change and
  stored tiles never need re-meshing). No trail limit: the zones bound the tile count.

## Cost fixes from the 23:01 timing (slump ~3 ms/frame, upload ~1.7 ms/frame)
- Slump runs with the upload (10 Hz), not every frame.
- Upload rebuilds only window tiles whose cells changed (stamp rect, slump box, traces).

## Not in this step
- MP: the sheet stays per client; drag stays SP only. JIP trail sync later.
- Craters / digs inside a zone: stored tiles keep their old ground until the window re-traces them.
- Cost of drawing many zone tiles: measure with one 64 m zone first.

## Test
Place one zone (64 x 64) on the test area, play: mud is there before any vehicle comes, ruts press in, leave
and come back - ruts still there, no mud outside the box. Log: 'MUD mud zones:' build line + sheet ms split.
