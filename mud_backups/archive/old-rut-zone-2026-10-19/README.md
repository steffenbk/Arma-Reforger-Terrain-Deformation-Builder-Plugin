# Old rut zone A/B (archived 2026-10-19)

TEMP test: one red box (x 1048-1118, z 2480-2550) where wheels pressed the old heightfield ruts, voxel ruts elsewhere.
Result: results/2026-10-19-mesh-vs-voxel-ruts-ds.md run 4 - heightfield ruts cost more (server 6.31 vs 4.48 ms/frame).
User 2026-10-19: keep voxel ruts. Checkpoint v230 = the build with the box in.

Call sites removed (to put it back):
- MUD_WorldGround.c Wheels, per wheel after `vector cp = sim.WheelGetContactPosition(w);`:
  `bool oldZone = MUD_OldRutZone.Inside(cp);` / `if (oldZone && MUD_VoxelProbe.RutsOn()) MUD_VoxelProbe.RutWheelOff(veh, w);`
  and the voxel branch test `if (MUD_VoxelProbe.RutsOn() && !oldZone)`.
- MUD_LookRuts.c Tick: `MUD_OldRutZone.Draw();` after MUD_TrackStamps.Tick; in the wheel loop after
  `vector cp = MUD_VoxelProbe.RutPoint(...)`: `if (MUD_OldRutZone.Inside(cp)) { t.m_aLast.Set(w, cp); continue; }`.
Kept: MUD_LookRuts.Tick returns when MUD_VoxelProbe.RUTS is false (no look ruts over heightfield ruts).
