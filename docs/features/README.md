# Features - what we are working on

One folder per feature. Each README says: status, the files, the docs, what is open.
Status words: ACTIVE (working on it now), SHIPPED (works, user-OK), PARKED (on hold, switched off or left alone),
OFF (code stays, switched off).

The work order still lives in [../superpowers/backlog/2026-10-15-master-list.md](../superpowers/backlog/2026-10-15-master-list.md).
Proven engine facts: [../superpowers/research/mud-gotchas.md](../superpowers/research/mud-gotchas.md).

| Feature | Status | Folder |
|---|---|---|
| Whole-map heightfield ground | SHIPPED | [heightfield-ground](heightfield-ground/README.md) |
| Voxel ground (bricks) | SHIPPED | [voxel-ground](voxel-ground/README.md) |
| Ruts (voxel ruts, v035) | SHIPPED | [ruts](ruts/README.md) |
| Craters | SHIPPED | [craters](craters/README.md) |
| Trenches, shovel, tunnels | SHIPPED | [trenches-tunnels](trenches-tunnels/README.md) |
| MP sync, ahead system, JIP | SHIPPED (JIP lazy queues open) | [mp-sync](mp-sync/README.md) |
| Mud look layer (client-only look on the felt rut) | PARKED 2026-10-18 (code removed, archived) | [mud-look](mud-look/README.md) |
| Mud sheet + ONE GRID (mud zones) | PARKED AGAIN 2026-10-21 (ENABLED false, FAR_PRESS false, v275 = last state with synced sheet + far press; 2026-10-18 park v206; lessons for voxel ruts in its README) | [mud-sheet](mud-sheet/README.md) |
| AI navmesh refresh | OFF (dead end) | [ai-navmesh](ai-navmesh/README.md) |

Checkpoints: `mud_backups/checkpoint.ps1` (save / list / restore), stored in
`C:\Users\Steffen\Documents\A_documents\Github\Terrain deformation\Versions backup`.
